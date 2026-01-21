/*********************************************************************
 * File Name          : hidkbd.c
 * Description        : NiZ 蓝牙键盘服务逻辑 (纯净版)
 *********************************************************************/

#include "CONFIG.h"
#include "devinfoservice.h"
#include "battservice.h"
#include "hidkbdservice.h"
#include "hiddev.h"
#include "hidkbd.h"

// ===================================================================
// ? 配置区
// ===================================================================
// 蓝牙搜索时显示的名字 (最长不要超过20个字符)
static const uint8_t attDeviceName[GAP_DEVICE_NAME_LEN] = "ENJOU BLE Adapter";

// 广播包里的名字 (建议简短)
static uint8_t advertData[] = {
    0x02, GAP_ADTYPE_FLAGS, GAP_ADTYPE_FLAGS_LIMITED | GAP_ADTYPE_FLAGS_BREDR_NOT_SUPPORTED,
    0x03, GAP_ADTYPE_APPEARANCE, LO_UINT16(GAP_APPEARE_HID_KEYBOARD), HI_UINT16(GAP_APPEARE_HID_KEYBOARD),
    0x0A, GAP_ADTYPE_LOCAL_NAME_COMPLETE, 
    'E', 'N', 'J', 'O', 'U', ' ', 'B', 'L', 'E' // "ENJOU BLE"
};
// ===================================================================

#define HID_KEYBOARD_IN_RPT_LEN              8
#define HID_LED_OUT_RPT_LEN                  1

// 常量定义 (保持默认即可)
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

// 扫描响应数据
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

// 回调函数注册
static hidDevCB_t hidEmuHidCBs = {
    hidEmuRptCB,
    hidEmuEvtCB,
    NULL,
    hidEmuStateCB
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

    // 3. 电池服务 (默认假数据)
    {
        uint8_t critical = DEFAULT_BATT_CRITICAL_LEVEL;
        Batt_SetParameter(BATT_PARAM_CRITICAL_LEVEL, sizeof(uint8_t), &critical);
    }

    // 4. HID 服务初始化
    Hid_AddService();
    HidDev_Register(&hidEmuCfg, &hidEmuHidCBs);

    // 启动设备
    tmos_set_event(hidEmuTaskId, START_DEVICE_EVT);
}

// ===================================================================
// 任务处理循环 (已移除演示代码)
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

    // 连接参数更新 (保持默认)
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

    // PHY 更新 (保持默认)
    if(events & START_PHY_UPDATE_EVT)
    {
        PRINT("Send Phy Update %x...\n", GAPRole_UpdatePHY(hidEmuConnHandle, 0, 
                    GAP_PHY_BIT_LE_2M, GAP_PHY_BIT_LE_2M, GAP_PHY_OPTIONS_NOPRE));
        return (events ^ START_PHY_UPDATE_EVT);
    }
    
    return 0;
}

static void hidEmu_ProcessTMOSMsg(tmos_event_hdr_t *pMsg)
{
    switch(pMsg->event)
    {
        default: break;
    }
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
            PRINT("BLE Init OK\n");
        }
        break;

        case GAPROLE_ADVERTISING:
            if(pEvent->gap.opcode == GAP_MAKE_DISCOVERABLE_DONE_EVENT)
                PRINT("Advertising...\n");
            break;

        case GAPROLE_CONNECTED:
            if(pEvent->gap.opcode == GAP_LINK_ESTABLISHED_EVENT)
            {
                gapEstLinkReqEvent_t *event = (gapEstLinkReqEvent_t *)pEvent;
                hidEmuConnHandle = event->connectionHandle;
                tmos_start_task(hidEmuTaskId, START_PARAM_UPDATE_EVT, START_PARAM_UPDATE_EVT_DELAY);
                PRINT("BLE Connected!\n");
            }
            break;

        case GAPROLE_CONNECTED_ADV:
             PRINT("Connected Advertising..\n");
             break;

        case GAPROLE_WAITING:
            if(pEvent->gap.opcode == GAP_LINK_TERMINATED_EVENT)
                PRINT("Disconnected. Reason:%x\n", pEvent->linkTerminate.reason);
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

// 接收 LED 状态 (如大小写灯) - 暂时预留
static uint8_t hidEmuRcvReport(uint8_t len, uint8_t *pData)
{
    if(len == HID_LED_OUT_RPT_LEN) {
        // pData[0] 的 Bit0=NumLock, Bit1=CapsLock, Bit2=ScrollLock
        // 如果你需要控制键盘灯，可以在这里写代码
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
        // ? 已删除：不要在这里启动演示用的定时器
        PRINT("Notification Enabled. Ready.\n");
    }
    return status;
}

static void hidEmuEvtCB(uint8_t evt) { return; }

// ===================================================================
// 核心接口：发送 USB 透传数据
// ===================================================================
// 参数: pData (8字节标准报文)
// 返回: SUCCESS(0) 或 错误码
uint8_t HidEmu_SendUSBReport(uint8_t *pData)
{
    // 调用底层发送函数
    // 参数1: Report ID (键盘为1)
    // 参数2: 类型 (Input)
    // 参数3: 长度 (8字节)
    return HidDev_Report(HID_RPT_ID_KEY_IN, HID_REPORT_TYPE_INPUT, 8, pData);
}