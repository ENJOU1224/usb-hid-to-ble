/*********************************************************************
 * File Name          : hidkbd.c
 * Description        : NiZ 蓝牙键盘服务逻辑 (纯净版 + 电池管理)
 *********************************************************************/

#include "CONFIG.h"
#include "devinfoservice.h"
#include "battservice.h"
#include "hidkbdservice.h"
#include "hiddev.h"
#include "hidkbd.h"
#include "debug.h" // 确保包含 PRINT 定义

// ===================================================================
// ? 电量管理配置 (新增)
// ===================================================================
#define BATT_ADC_PIN         GPIO_Pin_4       // PA4
#define BATT_ADC_CHANNEL     0                // PA4 对应 ADC 通道 0

// 检测周期 (单位: 0.625ms)
// 1600 = 1秒。建议 60秒检测一次
#define BATT_READ_PERIOD     (1600 * 3)      

// ===================================================================

// ===================================================================
// ? 蓝牙配置区
// ===================================================================
static const uint8_t attDeviceName[GAP_DEVICE_NAME_LEN] = "ENJOU BLE Adapter";

static uint8_t advertData[] = {
    0x02, GAP_ADTYPE_FLAGS, GAP_ADTYPE_FLAGS_LIMITED | GAP_ADTYPE_FLAGS_BREDR_NOT_SUPPORTED,
    0x03, GAP_ADTYPE_APPEARANCE, LO_UINT16(GAP_APPEARE_HID_KEYBOARD), HI_UINT16(GAP_APPEARE_HID_KEYBOARD),
    0x0A, GAP_ADTYPE_LOCAL_NAME_COMPLETE, 
    'E', 'N', 'J', 'O', 'U', ' ', 'B', 'L', 'E'
};

#define HID_KEYBOARD_IN_RPT_LEN              8
#define HID_LED_OUT_RPT_LEN                  1

// 常量定义
#define START_PARAM_UPDATE_EVT_DELAY         12800
#define DEFAULT_HID_IDLE_TIMEOUT             60000
#define DEFAULT_DESIRED_MIN_CONN_INTERVAL    8
#define DEFAULT_DESIRED_MAX_CONN_INTERVAL    8
#define DEFAULT_DESIRED_SLAVE_LATENCY        0
#define DEFAULT_DESIRED_CONN_TIMEOUT         500
#define DEFAULT_PASSCODE                     0
#define DEFAULT_PAIRING_MODE                 GAPBOND_PAIRING_MODE_WAIT_FOR_REQ
#define DEFAULT_MITM_MODE                    FALSE
#define DEFAULT_BONDING_MODE                 TRUE
#define DEFAULT_IO_CAPABILITIES              GAPBOND_IO_CAP_NO_INPUT_NO_OUTPUT
#define DEFAULT_BATT_CRITICAL_LEVEL          6

// 全局变量
static uint8_t hidEmuTaskId = INVALID_TASK_ID;
static uint16_t hidEmuConnHandle = GAP_CONNHANDLE_INIT;

// ? 新增电量相关变量
static uint8_t last_batt_percent = 0; 
signed short ADC_RoughCalib_Value = 0; // ADC校准值

static uint8_t scanRspData[] = {
    0x05, GAP_ADTYPE_SLAVE_CONN_INTERVAL_RANGE,
    LO_UINT16(DEFAULT_DESIRED_MIN_CONN_INTERVAL), HI_UINT16(DEFAULT_DESIRED_MIN_CONN_INTERVAL),
    LO_UINT16(DEFAULT_DESIRED_MAX_CONN_INTERVAL), HI_UINT16(DEFAULT_DESIRED_MAX_CONN_INTERVAL),
    0x05, GAP_ADTYPE_16BIT_MORE,
    LO_UINT16(HID_SERV_UUID), HI_UINT16(HID_SERV_UUID),
    LO_UINT16(BATT_SERV_UUID), HI_UINT16(BATT_SERV_UUID),
    0x02, GAP_ADTYPE_POWER_LEVEL, 0
};

static hidDevCfg_t hidEmuCfg = {
    DEFAULT_HID_IDLE_TIMEOUT,
    HID_FEATURE_FLAGS
};

// 本地函数声明
static void    hidEmu_ProcessTMOSMsg(tmos_event_hdr_t *pMsg);
static uint8_t hidEmuRcvReport(uint8_t len, uint8_t *pData);
static uint8_t hidEmuRptCB(uint8_t id, uint8_t type, uint16_t uuid, uint8_t oper, uint16_t *pLen, uint8_t *pData);
static void    hidEmuEvtCB(uint8_t evt);
static void    hidEmuStateCB(gapRole_States_t newState, gapRoleEvent_t *pEvent);
static void    HidEmu_MeasureBattery(void); // ? 新增声明

// 回调函数注册
static hidDevCB_t hidEmuHidCBs = {
    hidEmuRptCB,
    hidEmuEvtCB,
    NULL,
    hidEmuStateCB
};

// ===================================================================
// ? 锂电池放电曲线表 (新增)
// ===================================================================
typedef struct {
    uint16_t mv;
    uint8_t  pct;
} BattMap;

static const BattMap batt_table[] = {
    {4150, 100}, {4100, 95}, {4050, 90}, {4000, 85},
    {3950, 80},  {3900, 75}, {3850, 70}, {3800, 65},
    {3750, 60},  {3720, 55}, {3690, 50}, {3660, 45},
    {3630, 40},  {3600, 35}, {3570, 30}, {3540, 25},
    {3510, 20},  {3480, 15}, {3450, 10}, {3400, 5},
    {0, 0}
};

// ===================================================================
// 核心初始化
// ===================================================================
void HidEmu_Init()
{
    hidEmuTaskId = TMOS_ProcessEventRegister(HidEmu_ProcessEvent);

    // 1. GAP 角色配置
    {
        uint8_t initial_advertising_enable = TRUE;
        GAPRole_SetParameter(GAPROLE_ADVERT_ENABLED, sizeof(uint8_t), &initial_advertising_enable);
        GAPRole_SetParameter(GAPROLE_ADVERT_DATA, sizeof(advertData), advertData);
        GAPRole_SetParameter(GAPROLE_SCAN_RSP_DATA, sizeof(scanRspData), scanRspData);
    }
    GGS_SetParameter(GGS_DEVICE_NAME_ATT, sizeof(attDeviceName), (void *)attDeviceName);

    // 2. 配对与安全配置
    {
        uint32_t passkey = DEFAULT_PASSCODE;
        uint8_t  pairMode = DEFAULT_PAIRING_MODE;
        uint8_t  mitm = DEFAULT_MITM_MODE;
        uint8_t  ioCap = DEFAULT_IO_CAPABILITIES;
        uint8_t  bonding = DEFAULT_BONDING_MODE;
        GAPBondMgr_SetParameter(GAPBOND_PERI_DEFAULT_PASSCODE, sizeof(uint32_t), &passkey);
        GAPBondMgr_SetParameter(GAPBOND_PERI_PAIRING_MODE, sizeof(uint8_t), &pairMode);
        GAPBondMgr_SetParameter(GAPBOND_PERI_MITM_PROTECTION, sizeof(uint8_t), &mitm);
        GAPBondMgr_SetParameter(GAPBOND_PERI_IO_CAPABILITIES, sizeof(uint8_t), &ioCap);
        GAPBondMgr_SetParameter(GAPBOND_PERI_BONDING_ENABLED, sizeof(uint8_t), &bonding);
    }

    // 3. 电池服务
    {
        uint8_t critical = DEFAULT_BATT_CRITICAL_LEVEL;
        Batt_SetParameter(BATT_PARAM_CRITICAL_LEVEL, sizeof(uint8_t), &critical);
    }

    // 4. HID 服务初始化
    Hid_AddService();
    HidDev_Register(&hidEmuCfg, &hidEmuHidCBs);

    // --------------------------------------------------------
    // ? 5. ADC 硬件初始化 (融合新增)
    // --------------------------------------------------------
    // 浮空输入
    GPIOA_ModeCfg(BATT_ADC_PIN, GPIO_ModeIN_Floating);
    // 初始化 ADC
    ADC_ExtSingleChSampInit(SampleFreq_3_2, ADC_PGA_0);
    // 获取校准值 (根据你的SDK示例)
    ADC_RoughCalib_Value = ADC_DataCalib_Rough();
    LOG_BATT("ADC Calib Offset: %d\n", ADC_RoughCalib_Value);
    
    // 启动第一次电量检测 (延时2秒)
    tmos_start_task(hidEmuTaskId, START_BATT_READ_EVT, 1600 * 2);
    // --------------------------------------------------------

    // 启动设备
    tmos_set_event(hidEmuTaskId, START_DEVICE_EVT);
}

// ===================================================================
// 任务处理循环
// ===================================================================
uint16_t HidEmu_ProcessEvent(uint8_t task_id, uint16_t events)
{
    if(events & SYS_EVENT_MSG)
    {
        uint8_t *pMsg;
        if((pMsg = tmos_msg_receive(hidEmuTaskId)) != NULL)
        {
            hidEmu_ProcessTMOSMsg((tmos_event_hdr_t *)pMsg);
            tmos_msg_deallocate(pMsg);
        }
        return (events ^ SYS_EVENT_MSG);
    }

    if(events & START_DEVICE_EVT)
    {
        return (events ^ START_DEVICE_EVT);
    }

    // ? 电量检测任务处理 (新增)
    if(events & START_BATT_READ_EVT)
    {
        HidEmu_MeasureBattery();
        // 循环执行
        tmos_start_task(hidEmuTaskId, START_BATT_READ_EVT, BATT_READ_PERIOD);
        return (events ^ START_BATT_READ_EVT);
    }

    if(events & START_PARAM_UPDATE_EVT)
    {
        GAPRole_PeripheralConnParamUpdateReq(hidEmuConnHandle,
                                             DEFAULT_DESIRED_MIN_CONN_INTERVAL,
                                             DEFAULT_DESIRED_MAX_CONN_INTERVAL,
                                             DEFAULT_DESIRED_SLAVE_LATENCY,
                                             DEFAULT_DESIRED_CONN_TIMEOUT,
                                             hidEmuTaskId);
        return (events ^ START_PARAM_UPDATE_EVT);
    }

    if(events & START_PHY_UPDATE_EVT)
    {
        LOG_SYS("Send Phy Update %x...\n", GAPRole_UpdatePHY(hidEmuConnHandle, 0, 
                    GAP_PHY_BIT_LE_2M, GAP_PHY_BIT_LE_2M, GAP_PHY_OPTIONS_NOPRE));
        return (events ^ START_PHY_UPDATE_EVT);
    }
    
    return 0;
}

static void hidEmu_ProcessTMOSMsg(tmos_event_hdr_t *pMsg)
{
    switch(pMsg->event) { default: break; }
}

// ===================================================================
// ? 核心电量计算函数 (新增)
// ===================================================================
static void HidEmu_MeasureBattery(void)
{
    uint32_t adc_sum = 0;
    uint16_t adc_avg = 0;
    uint16_t voltage_mv = 0;
    uint8_t  percent = 0;
    signed short raw_val;

    // 1. 切换到通道 0 (PA4)
    ADC_ChannelCfg(BATT_ADC_CHANNEL);

    // 2. 连续采样 20 次取平均值 (软件滤波)
    for (int i = 0; i < 20; i++) {
        // 读取并加上校准值
        raw_val = ADC_ExcutSingleConver() + ADC_RoughCalib_Value;
        if (raw_val < 0) raw_val = 0;
        adc_sum += raw_val;
    }
    adc_avg = adc_sum / 20;

    // 3. 转换为电压 (mV)
    // 假设 VCC=3.3V, 1/2分压。公式: (ADC / 4095) * 3300 * 2
    // 简化: (adc * 6600) / 4095
    voltage_mv = (adc_avg * 6600) / 4095;

    // 4. 查表计算百分比
    percent = 0;
    for (int i = 0; i < sizeof(batt_table)/sizeof(BattMap); i++) {
        if (voltage_mv >= batt_table[i].mv) {
            percent = batt_table[i].pct;
            break;
        }
    }

    // 5. 更新蓝牙服务
    if (percent != last_batt_percent) {
        last_batt_percent = percent;
        Batt_SetParameter(BATT_PARAM_LEVEL, sizeof(uint8_t), &percent);
    }

    // 6. 调试打印 (使用 LOG_BATT)
    LOG_BATT("BATT: ADC:%d V:%dmV Pct:%d%%\n", adc_avg, voltage_mv, percent);
}

// ===================================================================
// 蓝牙状态回调
// ===================================================================
static void hidEmuStateCB(gapRole_States_t newState, gapRoleEvent_t *pEvent)
{
    switch(newState & GAPROLE_STATE_ADV_MASK)
    {
        case GAPROLE_STARTED:
        {
            uint8_t ownAddr[6];
            GAPRole_GetParameter(GAPROLE_BD_ADDR, ownAddr);
            GAP_ConfigDeviceAddr(ADDRTYPE_STATIC, ownAddr);
            LOG_BLE("BLE Init OK\n");
        }
        break;

        case GAPROLE_ADVERTISING:
            if(pEvent->gap.opcode == GAP_MAKE_DISCOVERABLE_DONE_EVENT)
                LOG_BLE("Advertising...\n");
            break;

        case GAPROLE_CONNECTED:
            if(pEvent->gap.opcode == GAP_LINK_ESTABLISHED_EVENT)
            {
                gapEstLinkReqEvent_t *event = (gapEstLinkReqEvent_t *)pEvent;
                hidEmuConnHandle = event->connectionHandle;
                tmos_start_task(hidEmuTaskId, START_PARAM_UPDATE_EVT, START_PARAM_UPDATE_EVT_DELAY);
                LOG_BLE("BLE Connected!\n");
                
                // ?【新增这行】强制重置上次电量记录
                // 这样下一次测量时，代码会认为电量“变了”，从而强制发包给电脑
                last_batt_percent = 0; 
                
                // 立即触发一次测量
                tmos_start_task(hidEmuTaskId, START_BATT_READ_EVT, 500); // 稍微延时500ms等连接稳定
            }
            break;

        case GAPROLE_CONNECTED_ADV:
             LOG_BLE("Connected Advertising..\n");
             break;

        case GAPROLE_WAITING:
            if(pEvent->gap.opcode == GAP_LINK_TERMINATED_EVENT)
                LOG_BLE("Disconnected. Reason:%x\n", pEvent->linkTerminate.reason);
                
            // 断开后自动重新广播
            {
                uint8_t initial_advertising_enable = TRUE;
                GAPRole_SetParameter(GAPROLE_ADVERT_ENABLED, sizeof(uint8_t), &initial_advertising_enable);
            }
            break;

        default: break;
    }
}

// ===================================================================
// HID 报文处理
// ===================================================================
static uint8_t hidEmuRcvReport(uint8_t len, uint8_t *pData)
{
    if(len == HID_LED_OUT_RPT_LEN) {
        return SUCCESS;
    }
    return ATT_ERR_INVALID_VALUE_SIZE;
}

static uint8_t hidEmuRptCB(uint8_t id, uint8_t type, uint16_t uuid,
                           uint8_t oper, uint16_t *pLen, uint8_t *pData)
{
    uint8_t status = SUCCESS;

    if(oper == HID_DEV_OPER_WRITE)
    {
        if(uuid == REPORT_UUID && type == HID_REPORT_TYPE_OUTPUT)
             status = hidEmuRcvReport(*pLen, pData);

        if(status == SUCCESS)
            status = Hid_SetParameter(id, type, uuid, *pLen, pData);
    }
    else if(oper == HID_DEV_OPER_READ)
    {
        status = Hid_GetParameter(id, type, uuid, pLen, pData);
    }
    else if(oper == HID_DEV_OPER_ENABLE)
    {
        LOG_SYS("Notification Enabled. Ready.\n");
    }
    return status;
}

static void hidEmuEvtCB(uint8_t evt) { return; }

// ===================================================================
// 核心接口：发送 USB 透传数据
// ===================================================================
uint8_t HidEmu_SendUSBReport(uint8_t *pData)
{
    return HidDev_Report(HID_RPT_ID_KEY_IN, HID_REPORT_TYPE_INPUT, 8, pData);
}