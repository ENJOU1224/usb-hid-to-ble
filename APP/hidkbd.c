/*********************************************************************
 * File Name          : hidkbd.c
 * Author             : DIY User & AI Assistant
 * Version            : V2.0 (Refactored)
 * Description        : NiZ 蓝牙键盘主应用逻辑
 *                      - 集成 HID 键盘/鼠标复合设备
 *                      - 集成 电池电量采集 (ADC) 与上报
 *                      - 状态指示灯控制
 *********************************************************************/

#include "CONFIG.h"
#include "devinfoservice.h"
#include "battservice.h"
#include "hidkbdservice.h"
#include "hiddev.h"
#include "hidkbd.h"
#include "debug.h" 


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

// ===================================================================
// ?? 硬件引脚配置 (Hardware Config)
// ===================================================================

// --- 电池检测 (ADC -> PA4) ---
#define BATT_ADC_PIN         GPIO_Pin_4       
#define BATT_ADC_CHANNEL     0                // ADC Channel 0
#define BATT_READ_INTERVAL   (1600 * 60)      // 检测周期: 60秒 (1600 ticks = 1s)
#define BATT_BOOT_DELAY      (1600 * 2)       // 启动后首次检测延迟: 2秒

// ===================================================================
// ? 蓝牙参数配置 (BLE Configuration)
// ===================================================================

// 设备名称
static const uint8_t attDeviceName[] = "ENJOU BLE Adapter";

// 广播数据 (Flags + Appearance + Local Name)
static uint8_t advertData[] = {
    0x02, GAP_ADTYPE_FLAGS, GAP_ADTYPE_FLAGS_LIMITED | GAP_ADTYPE_FLAGS_BREDR_NOT_SUPPORTED,
    0x03, GAP_ADTYPE_APPEARANCE, LO_UINT16(GAP_APPEARE_HID_KEYBOARD), HI_UINT16(GAP_APPEARE_HID_KEYBOARD),
    0x0A, GAP_ADTYPE_LOCAL_NAME_COMPLETE, 
    'E', 'N', 'J', 'O', 'U', ' ', 'B', 'L', 'E'
};

// 扫描响应数据 (Conn Interval + UUIDs + TX Power)
static uint8_t scanRspData[] = {
    0x05, GAP_ADTYPE_SLAVE_CONN_INTERVAL_RANGE,
    LO_UINT16(DEFAULT_DESIRED_MIN_CONN_INTERVAL), HI_UINT16(DEFAULT_DESIRED_MIN_CONN_INTERVAL),
    LO_UINT16(DEFAULT_DESIRED_MAX_CONN_INTERVAL), HI_UINT16(DEFAULT_DESIRED_MAX_CONN_INTERVAL),
    0x05, GAP_ADTYPE_16BIT_MORE,
    LO_UINT16(HID_SERV_UUID), HI_UINT16(HID_SERV_UUID),
    LO_UINT16(BATT_SERV_UUID), HI_UINT16(BATT_SERV_UUID),
    0x02, GAP_ADTYPE_POWER_LEVEL, 0
};

// HID 配置参数
static hidDevCfg_t hidEmuCfg = {
    DEFAULT_HID_IDLE_TIMEOUT,
    HID_FEATURE_FLAGS
};

// ===================================================================
// ? 全局变量 (Global Variables)
// ===================================================================

static uint8_t  hidEmuTaskId = INVALID_TASK_ID;     // TMOS 任务ID
static uint16_t hidEmuConnHandle = GAP_CONNHANDLE_INIT; // 连接句柄

// --- 电池相关 ---
static uint8_t      last_batt_percent = 0;          // 上次上报的电量
static signed short ADC_RoughCalib_Value = 0;       // ADC 校准偏移值

// --- 放电曲线表 (mV -> %) ---
// 线性插值查表法，更符合锂电池放电特性
typedef struct { uint16_t mv; uint8_t pct; } BattMap;
static const BattMap batt_table[] = {
    /* 电压(mV)  百分比   剩余时间(约)   Log对应状态分析 */
    {4200,      100},  // 64.5h  充电器拔出瞬间
    {4100,       95},  // 61.3h  初期虚电消除 (Log约 3h)
    {4070,       90},  // 58.0h  (Log约 6.5h)
    {4000,       80},  // 51.6h  (Log约 13h)
    {3910,       70},  // 45.1h  (Log约 19.5h)
    {3840,       60},  // 38.7h  (Log约 26h)
    {3750,       50},  // 32.2h  *** 绝对中点 *** (电压比普通设备高，因为负载轻)
    {3660,       40},  // 25.8h  (Log约 39h)
    {3600,       30},  // 19.3h  开始进入平台末期
    {3510,       20},  // 12.9h  
    {3450,       15},  // 9.7h   
    {3380,       10},  // 6.5h   低压警告线，电压开始加速下降
    {3340,        8},  // 5.2h
    {3300,        6},  // 3.9h   (Log中 1-31 06:00 左右)
    {3260,        4},  // 2.6h
    {3220,        2},  // 1.3h   最后时刻，准备保存数据
    {3180,        1},  // 0.6h   (Log中 3180mV 后只剩半小时)
    {3100,        0},  // 0.0h   UVLO 关机保护
    
    {0,           0}   // 结束符，用于算法边界保护
};

// ===================================================================
// ? 内部函数声明
// ===================================================================
static void    HidEmu_ProcessTMOSMsg(tmos_event_hdr_t *pMsg);
static void    HidEmu_MeasureBattery(void);
static void    HidEmu_StateCB(gapRole_States_t newState, gapRoleEvent_t *pEvent);
static uint8_t HidEmu_RptCB(uint8_t id, uint8_t type, uint16_t uuid, uint8_t oper, uint16_t *pLen, uint8_t *pData);

// 注册回调结构体
static hidDevCB_t hidEmuHidCBs = {
    HidEmu_RptCB,
    NULL, // EvtCB 未使用
    NULL,
    HidEmu_StateCB
};

// ===================================================================
// ? 初始化函数 (Initialization)
// ===================================================================
void HidEmu_Init()
{
    hidEmuTaskId = TMOS_ProcessEventRegister(HidEmu_ProcessEvent);

    // 1. GAP 广播与通用配置
    {
        uint8_t initial_advertising_enable = TRUE;
        GAPRole_SetParameter(GAPROLE_ADVERT_ENABLED, sizeof(uint8_t), &initial_advertising_enable);
        GAPRole_SetParameter(GAPROLE_ADVERT_DATA, sizeof(advertData), advertData);
        GAPRole_SetParameter(GAPROLE_SCAN_RSP_DATA, sizeof(scanRspData), scanRspData);
    }
    GGS_SetParameter(GGS_DEVICE_NAME_ATT, sizeof(attDeviceName), (void *)attDeviceName);

    // 2. 安全与配对配置 (Bond Manager)
    {
        uint32_t passkey = DEFAULT_PASSCODE;
        uint8_t  pairMode = DEFAULT_PAIRING_MODE;
        uint8_t  mitm     = DEFAULT_MITM_MODE;
        uint8_t  ioCap    = DEFAULT_IO_CAPABILITIES;
        uint8_t  bonding  = DEFAULT_BONDING_MODE;
        
        GAPBondMgr_SetParameter(GAPBOND_PERI_DEFAULT_PASSCODE, sizeof(uint32_t), &passkey);
        GAPBondMgr_SetParameter(GAPBOND_PERI_PAIRING_MODE, sizeof(uint8_t), &pairMode);
        GAPBondMgr_SetParameter(GAPBOND_PERI_MITM_PROTECTION, sizeof(uint8_t), &mitm);
        GAPBondMgr_SetParameter(GAPBOND_PERI_IO_CAPABILITIES, sizeof(uint8_t), &ioCap);
        GAPBondMgr_SetParameter(GAPBOND_PERI_BONDING_ENABLED, sizeof(uint8_t), &bonding);
    }

    // 3. 注册 GATT 服务
    {
        uint8_t critical = DEFAULT_BATT_CRITICAL_LEVEL;
        Batt_SetParameter(BATT_PARAM_CRITICAL_LEVEL, sizeof(uint8_t), &critical); // 电池服务
        Hid_AddService(); // HID 服务
        HidDev_Register(&hidEmuCfg, &hidEmuHidCBs); // 注册 HID 回调
    }

    // 4. ADC 硬件初始化 (用于电量检测)
    {
        GPIOA_ModeCfg(BATT_ADC_PIN, GPIO_ModeIN_Floating);
        ADC_ExtSingleChSampInit(SampleFreq_3_2, ADC_PGA_1_2); // PGA=1/2, 满量程约 2.1V 或 6.3V(取决于VRef)
        ADC_RoughCalib_Value = ADC_DataCalib_Rough();
        LOG_BATT("ADC Init. Offset: %d\n", ADC_RoughCalib_Value);
        
        // 延时启动首次检测
        tmos_start_task(hidEmuTaskId, START_BATT_READ_EVT, BATT_BOOT_DELAY);
    }

    // 5. 启动设备主事件
    tmos_set_event(hidEmuTaskId, START_DEVICE_EVT);
}

// ===================================================================
// ? TMOS 事件处理循环 (Main Loop)
// ===================================================================
uint16_t HidEmu_ProcessEvent(uint8_t task_id, uint16_t events)
{
    // --- 系统消息处理 (System Messages) ---
    if(events & SYS_EVENT_MSG)
    {
        uint8_t *pMsg;
        if((pMsg = tmos_msg_receive(hidEmuTaskId)) != NULL)
        {
            HidEmu_ProcessTMOSMsg((tmos_event_hdr_t *)pMsg);
            tmos_msg_deallocate(pMsg);
        }
        return (events ^ SYS_EVENT_MSG);
    }

    // --- 设备启动事件 ---
    if(events & START_DEVICE_EVT)
    {
        return (events ^ START_DEVICE_EVT);
    }

    // --- 电池周期检测事件 ---
    if(events & START_BATT_READ_EVT)
    {
        HidEmu_MeasureBattery();
        tmos_start_task(hidEmuTaskId, START_BATT_READ_EVT, BATT_READ_INTERVAL);
        return (events ^ START_BATT_READ_EVT);
    }

    // --- 连接参数更新事件 ---
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

    // --- PHY 更新事件 (2M Rate) ---
    if(events & START_PHY_UPDATE_EVT)
    {
        LOG_SYS("PHY Update to 2M\n");
        GAPRole_UpdatePHY(hidEmuConnHandle, 0, 
                    GAP_PHY_BIT_LE_2M, GAP_PHY_BIT_LE_2M, GAP_PHY_OPTIONS_NOPRE);
        return (events ^ START_PHY_UPDATE_EVT);
    }
    
    return 0;
}

// ===================================================================
// ? 电池测量逻辑 (Battery Measurement)
// ===================================================================
static void HidEmu_MeasureBattery(void)
{
    uint32_t adc_sum = 0;
    uint16_t adc_avg = 0;
    int32_t  voltage_mv = 0;
    uint8_t  percent = 0;
    signed short raw_val;

    // 1. 切换通道并采集
    ADC_ChannelCfg(BATT_ADC_CHANNEL);
    
    // 软件滤波: 20次采样取平均
    for (int i = 0; i < 20; i++) {
        raw_val = ADC_ExcutSingleConver() + ADC_RoughCalib_Value;
        if (raw_val < 0) raw_val = 0;
        adc_sum += raw_val;
    }
    adc_avg = adc_sum / 20;

    // 2. 电压换算 (根据硬件特定的分压电阻计算)
    // 原始公式: (adc_avg * 100 * 21 + 512) / 1024 - 2100
    // 注: 这里使用了 int32_t 防止乘法溢出
    int32_t temp_calc = (int32_t)adc_avg * 2100; // 优化: 100 * 21
    voltage_mv = (temp_calc + 512) / 1024 - 2100;
    
    // 保护: 避免显示负电压
    if (voltage_mv < 0) voltage_mv = 0;

    // 3. 查表获取百分比
    for (int i = 0; i < sizeof(batt_table)/sizeof(BattMap) - 1; i++) {
        if (voltage_mv >= batt_table[0].mv){
            percent = 100;
            break;
        } 
        if (voltage_mv <= batt_table[sizeof(batt_table)/sizeof(BattMap) - 2].mv) {
            percent = 0;
            break;
        }
        // 3. 查表插值
    for (int i = 0; i < sizeof(batt_table)/sizeof(BattMap) - 1; i++) {
        // 因为已经处理了 >= table[0]，这里主要找区间
        if (voltage_mv >= batt_table[i+1].mv) {
            uint16_t high_mv = batt_table[i].mv;
            uint16_t low_mv  = batt_table[i+1].mv;
            uint8_t  high_pct = batt_table[i].pct;
            uint8_t  low_pct  = batt_table[i+1].pct;

            // 线性插值公式
            percent = low_pct + (uint32_t)(voltage_mv - low_mv) * (high_pct - low_pct) / (high_mv - low_mv);
            break;
        }
    }
    }

    // 4. 若电量变化，推送到蓝牙服务
    if (percent != last_batt_percent) {
        last_batt_percent = percent;
        Batt_SetParameter(BATT_PARAM_LEVEL, sizeof(uint8_t), &percent);
    }

    LOG_BATT("ADC:%d  V:%dmV  Pct:%d%%\n", adc_avg, (int)voltage_mv, percent);
}

// ===================================================================
// ? 蓝牙状态回调 (State Callback)
// ===================================================================
static void HidEmu_StateCB(gapRole_States_t newState, gapRoleEvent_t *pEvent)
{
    switch(newState & GAPROLE_STATE_ADV_MASK)
    {
        case GAPROLE_STARTED:
        {
            uint8_t ownAddr[6];
            GAPRole_GetParameter(GAPROLE_BD_ADDR, ownAddr);
            GAP_ConfigDeviceAddr(ADDRTYPE_STATIC, ownAddr); // 使用静态地址
            LOG_BLE("BLE Stack Initialized\n");
        }
        break;

        case GAPROLE_ADVERTISING:
            if(pEvent->gap.opcode == GAP_MAKE_DISCOVERABLE_DONE_EVENT) {
                LOG_BLE("Advertising...\n");
            }
            LED2_OFF(); // 广播时灭灯 (省电)
            break;

        case GAPROLE_CONNECTED:
            if(pEvent->gap.opcode == GAP_LINK_ESTABLISHED_EVENT)
            {
                gapEstLinkReqEvent_t *event = (gapEstLinkReqEvent_t *)pEvent;
                hidEmuConnHandle = event->connectionHandle;
                
                // 启动参数更新延时
                tmos_start_task(hidEmuTaskId, START_PARAM_UPDATE_EVT, START_PARAM_UPDATE_EVT_DELAY);
                
                LOG_BLE("Connected! Handle: %d\n", hidEmuConnHandle);
                
                LED2_ON(); // 连接成功亮灯

                // 强制刷新电量: 重置记录并立即触发测量
                last_batt_percent = 0; 
                tmos_start_task(hidEmuTaskId, START_BATT_READ_EVT, 800); 
            }
            break;

        case GAPROLE_WAITING: // 连接断开/空闲
            if(pEvent->gap.opcode == GAP_LINK_TERMINATED_EVENT) {
                LOG_BLE("Disconnected. Reason: 0x%02x\n", pEvent->linkTerminate.reason);
            }
            
            LED2_OFF(); // 断开灭灯

            // 自动重启广播
            {
                uint8_t adv_enable = TRUE;
                GAPRole_SetParameter(GAPROLE_ADVERT_ENABLED, sizeof(uint8_t), &adv_enable);
            }
            break;

        default: break;
    }
}

// ===================================================================
// ?? HID 报文回调 (Report Callback)
// ===================================================================
static uint8_t HidEmu_RptCB(uint8_t id, uint8_t type, uint16_t uuid,
                            uint8_t oper, uint16_t *pLen, uint8_t *pData)
{
    uint8_t status = SUCCESS;

    // 主机写入数据 (例如点亮键盘 NumLock 灯)
    if(oper == HID_DEV_OPER_WRITE)
    {
        if(uuid == REPORT_UUID && type == HID_REPORT_TYPE_OUTPUT)
        {
            // 可以在这里处理 LED 输出报文 (pData[0] 是 LED 位掩码)
            // 目前仅返回成功，实际逻辑可扩展
            status = SUCCESS;
        }

        if(status == SUCCESS)
            status = Hid_SetParameter(id, type, uuid, *pLen, pData);
    }
    // 主机读取数据
    else if(oper == HID_DEV_OPER_READ)
    {
        status = Hid_GetParameter(id, type, uuid, pLen, pData);
    }
    // 通知开启
    else if(oper == HID_DEV_OPER_ENABLE)
    {
        LOG_SYS("HID Notification Enabled\n");
    }
    
    return status;
}

static void HidEmu_ProcessTMOSMsg(tmos_event_hdr_t *pMsg) {
    // 预留底层消息处理接口
    return;
}

// ===================================================================
// ? 外部调用接口 (Public APIs)
// ===================================================================

/**
 * @brief 发送标准键盘报文 (8字节)
 * @param pData [Mods, Res, Key1, Key2, Key3, Key4, Key5, Key6]
 */
uint8_t HidEmu_SendUSBReport(uint8_t *pData)
{
    return HidDev_Report(HID_RPT_ID_KEY_IN, HID_REPORT_TYPE_INPUT, 8, pData);
}

/**
 * @brief 发送鼠标报文 (4字节)
 * @param pData [Buttons, X, Y, Wheel]
 */
uint8_t HidEmu_SendMouseReport(uint8_t *pData)
{
    return HidDev_Report(HID_RPT_ID_MOUSE_IN, HID_REPORT_TYPE_INPUT, 4, pData);
}