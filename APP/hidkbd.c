/*********************************************************************
 * File Name          : hidkbd.c
 * Author             : DIY User & AI Assistant
 * Description        : NiZ 蓝牙键盘主应用逻辑
 *                      - 集成 HID 键盘/鼠标复合设备
 *                      - 集成电池电量采集 (ADC) 与上报
 *                      - 状态指示灯控制
 *********************************************************************/

#include "CONFIG.h"
#include "devinfoservice.h"
#include "battservice.h"
#include "hidkbdservice.h"
#include "hiddev.h"
#include "hidkbd.h"
#include "debug.h"

// ===================================================================
// 蓝牙参数配置 (BLE Configuration)
// ===================================================================
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
// 硬件引脚配置 (Hardware Config)
// ===================================================================

// 电池检测 (ADC -> PA4)
#define BATT_ADC_PIN         GPIO_Pin_4
#define BATT_ADC_CHANNEL     0            // ADC Channel 0

// ===================================================================
// 蓝牙广播数据
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
// 全局变量
// ===================================================================

static uint8_t  hidEmuTaskId    = INVALID_TASK_ID;       // TMOS 任务ID
static uint16_t hidEmuConnHandle = GAP_CONNHANDLE_INIT;  // 连接句柄

static uint8_t is_ble_sleeping  = FALSE;  // 是否处于软休眠（蓝牙关闭）状态
static uint8_t is_sys_led_startup = TRUE; // 是否还在上电 SYS 灯长亮阶段

uint8_t is_usb_idle = FALSE;  // USB 30秒空闲标志

// 电池相关
static uint8_t      last_batt_percent   = 0;  // 上次上报的电量
static signed short ADC_RoughCalib_Value = 0; // ADC 校准偏移值

// 放电曲线查找表 (mV -> %), 线性插值
typedef struct { uint16_t mv; uint8_t pct; } BattMap;
static const BattMap batt_table[] = {
    {4200, 100},
    {4100,  95},
    {4070,  90},
    {4000,  80},
    {3910,  70},
    {3840,  60},
    {3750,  50},
    {3660,  40},
    {3600,  30},
    {3510,  20},
    {3450,  15},
    {3380,  10},
    {3340,   8},
    {3300,   6},
    {3260,   4},
    {3220,   2},
    {3180,   1},
    {3100,   0},
    {0,      0}  // 结束符（边界保护）
};
#define BATT_TABLE_COUNT  (sizeof(batt_table) / sizeof(BattMap))

// ===================================================================
// 内部函数声明
// ===================================================================
static void    HidEmu_ProcessTMOSMsg(tmos_event_hdr_t *pMsg);
static void    HidEmu_MeasureBattery(void);
static void    HidEmu_StateCB(gapRole_States_t newState, gapRoleEvent_t *pEvent);
static uint8_t HidEmu_RptCB(uint8_t id, uint8_t type, uint16_t uuid, uint8_t oper, uint16_t *pLen, uint8_t *pData);

// 回调结构体
static hidDevCB_t hidEmuHidCBs = {
    HidEmu_RptCB,
    NULL,  // EvtCB 未使用
    NULL,
    HidEmu_StateCB
};

// ===================================================================
// 初始化
// ===================================================================
void HidEmu_Init()
{
    hidEmuTaskId = TMOS_ProcessEventRegister(HidEmu_ProcessEvent);

    // 1. 初始化 LED 引脚 (推挽输出)
    GPIOA_ModeCfg(SYS_LED_PIN | BLE_LED_PIN, GPIO_ModeOut_PP_5mA);

    // 2. 初始化 USER 按键 (上拉输入)
    GPIOB_ModeCfg(USER_KEY_PIN, GPIO_ModeIN_PU);

    // 3. 上电时 SYS 长亮
    is_sys_led_startup = TRUE;
    SYS_LED_ON();
    BLE_LED_OFF();
    tmos_start_task(hidEmuTaskId, HID_SYS_LED_OFF_EVT, TIME_SYS_LED_STARTUP);

    // 4. 启动按键轮询
    tmos_start_task(hidEmuTaskId, HID_USER_KEY_POLL_EVT, TIME_KEY_POLL);

    // 5. GAP 广播与通用配置
    {
        uint8_t initial_advertising_enable = TRUE;
        GAPRole_SetParameter(GAPROLE_ADVERT_ENABLED, sizeof(uint8_t), &initial_advertising_enable);
        GAPRole_SetParameter(GAPROLE_ADVERT_DATA, sizeof(advertData), advertData);
        GAPRole_SetParameter(GAPROLE_SCAN_RSP_DATA, sizeof(scanRspData), scanRspData);
    }
    GGS_SetParameter(GGS_DEVICE_NAME_ATT, sizeof(attDeviceName), (void *)attDeviceName);

    // 6. 安全与配对配置 (Bond Manager)
    {
        uint32_t passkey = DEFAULT_PASSCODE;
        uint8_t  pairMode = DEFAULT_PAIRING_MODE;
        uint8_t  mitm     = DEFAULT_MITM_MODE;
        uint8_t  ioCap    = DEFAULT_IO_CAPABILITIES;
        uint8_t  bonding  = DEFAULT_BONDING_MODE;

        GAPBondMgr_SetParameter(GAPBOND_PERI_DEFAULT_PASSCODE, sizeof(uint32_t), &passkey);
        GAPBondMgr_SetParameter(GAPBOND_PERI_PAIRING_MODE,     sizeof(uint8_t),  &pairMode);
        GAPBondMgr_SetParameter(GAPBOND_PERI_MITM_PROTECTION,  sizeof(uint8_t),  &mitm);
        GAPBondMgr_SetParameter(GAPBOND_PERI_IO_CAPABILITIES,  sizeof(uint8_t),  &ioCap);
        GAPBondMgr_SetParameter(GAPBOND_PERI_BONDING_ENABLED,  sizeof(uint8_t),  &bonding);
    }

    // 7. 注册 GATT 服务
    {
        uint8_t critical = DEFAULT_BATT_CRITICAL_LEVEL;
        Batt_SetParameter(BATT_PARAM_CRITICAL_LEVEL, sizeof(uint8_t), &critical);
        Hid_AddService();
        HidDev_Register(&hidEmuCfg, &hidEmuHidCBs);
    }

    // 8. ADC 硬件初始化 (电量检测)
    {
        GPIOA_ModeCfg(BATT_ADC_PIN, GPIO_ModeIN_Floating);
        ADC_ExtSingleChSampInit(SampleFreq_3_2, ADC_PGA_1_2);
        ADC_RoughCalib_Value = ADC_DataCalib_Rough();
        LOG_BATT("ADC Init. Offset: %d\n", ADC_RoughCalib_Value);

        tmos_start_task(hidEmuTaskId, START_BATT_READ_EVT, TIME_BATT_BOOT_DELAY);
    }

    // 9. 启动设备主事件
    tmos_set_event(hidEmuTaskId, START_DEVICE_EVT);

    // 10. 启动 USB 桥接的 TMOS 轮询任务 (初始全速)
    tmos_start_task(hidEmuTaskId, HID_USB_POLL_EVT, TIME_USB_POLL_ACTIVE);

    // 11. 启动 5 分钟不活动软休眠倒计时
    tmos_start_task(hidEmuTaskId, HID_SLEEP_TIMEOUT_EVT, TIME_SLEEP_TIMEOUT);

    // 12. 启动 30 秒 USB 空闲降频倒计时
    tmos_start_task(hidEmuTaskId, HID_USB_IDLE_EVT, TIME_USB_IDLE);
}

// ===================================================================
// TMOS 事件处理循环
// ===================================================================
uint16_t HidEmu_ProcessEvent(uint8_t task_id, uint16_t events)
{
    // [SYS] 上电倒计时结束，熄灭 SYS 灯
    if (events & HID_SYS_LED_OFF_EVT) {
        is_sys_led_startup = FALSE;
        SYS_LED_OFF();
        return (events ^ HID_SYS_LED_OFF_EVT);
    }

    // [SYS] 低电量闪烁翻转
    if (events & HID_SYS_LED_BLINK_EVT) {
        SYS_LED_TOGGLE();
        tmos_start_task(hidEmuTaskId, HID_SYS_LED_BLINK_EVT, TIME_SYS_LED_BLINK);
        return (events ^ HID_SYS_LED_BLINK_EVT);
    }

    // [BLE] 连接后定时熄灭 BLE 灯
    if (events & HID_BLE_LED_OFF_EVT) {
        BLE_LED_OFF();
        return (events ^ HID_BLE_LED_OFF_EVT);
    }

    // [BLE] 广播/断连时闪烁翻转
    if (events & HID_BLE_LED_BLINK_EVT) {
        BLE_LED_TOGGLE();
        tmos_start_task(hidEmuTaskId, HID_BLE_LED_BLINK_EVT, TIME_BLE_LED_BLINK);
        return (events ^ HID_BLE_LED_BLINK_EVT);
    }

    // [USER 按键] 按键轮询，按下时断开连接并进入软休眠
    if (events & HID_USER_KEY_POLL_EVT) {
        static uint8_t key_last_state = 1;
        uint8_t key_current = READ_USER_KEY();

        // 检测下降沿（按下）
        if (key_last_state == 1 && key_current == 0) {
            uint8_t gap_state;
            GAPRole_GetParameter(GAPROLE_STATE, &gap_state);
            if (gap_state == GAPROLE_CONNECTED) {
                is_ble_sleeping = TRUE;

                uint8_t adv_en = FALSE;
                GAPRole_SetParameter(GAPROLE_ADVERT_ENABLED, sizeof(uint8_t), &adv_en);

                SYS_LED_OFF();
                BLE_LED_OFF();
                tmos_stop_task(hidEmuTaskId, HID_BLE_LED_BLINK_EVT);

                GAPRole_TerminateLink(hidEmuConnHandle);
            }
        }
        key_last_state = key_current;

        tmos_start_task(hidEmuTaskId, HID_USER_KEY_POLL_EVT, TIME_KEY_POLL);
        return (events ^ HID_USER_KEY_POLL_EVT);
    }

    // 系统消息处理
    if (events & SYS_EVENT_MSG) {
        uint8_t *pMsg;
        if ((pMsg = tmos_msg_receive(hidEmuTaskId)) != NULL) {
            HidEmu_ProcessTMOSMsg((tmos_event_hdr_t *)pMsg);
            tmos_msg_deallocate(pMsg);
        }
        return (events ^ SYS_EVENT_MSG);
    }

    // 设备启动事件
    if (events & START_DEVICE_EVT) {
        return (events ^ START_DEVICE_EVT);
    }

    // 电池周期检测
    if (events & START_BATT_READ_EVT) {
        HidEmu_MeasureBattery();
        tmos_start_task(hidEmuTaskId, START_BATT_READ_EVT, TIME_BATT_READ_INTERVAL);
        return (events ^ START_BATT_READ_EVT);
    }

    // 连接参数更新
    if (events & START_PARAM_UPDATE_EVT) {
        GAPRole_PeripheralConnParamUpdateReq(hidEmuConnHandle,
                                             DEFAULT_DESIRED_MIN_CONN_INTERVAL,
                                             DEFAULT_DESIRED_MAX_CONN_INTERVAL,
                                             DEFAULT_DESIRED_SLAVE_LATENCY,
                                             DEFAULT_DESIRED_CONN_TIMEOUT,
                                             hidEmuTaskId);
        return (events ^ START_PARAM_UPDATE_EVT);
    }

    // PHY 更新 (2M Rate)
    if (events & START_PHY_UPDATE_EVT) {
        LOG_SYS("PHY Update to 2M\n");
        GAPRole_UpdatePHY(hidEmuConnHandle, 0,
                    GAP_PHY_BIT_LE_2M, GAP_PHY_BIT_LE_2M, GAP_PHY_OPTIONS_NOPRE);
        return (events ^ START_PHY_UPDATE_EVT);
    }

    // 软休眠超时：5分钟无按键，关闭蓝牙，保持 USB 监听
    if (events & HID_SLEEP_TIMEOUT_EVT) {
        is_ble_sleeping = TRUE;

        uint8_t adv_en = FALSE;
        GAPRole_SetParameter(GAPROLE_ADVERT_ENABLED, sizeof(uint8_t), &adv_en);

        SYS_LED_OFF();
        BLE_LED_OFF();
        tmos_stop_task(hidEmuTaskId, HID_BLE_LED_BLINK_EVT);

        uint8_t gap_state;
        GAPRole_GetParameter(GAPROLE_STATE, &gap_state);
        if (gap_state == GAPROLE_CONNECTED) {
            GAPRole_TerminateLink(hidEmuConnHandle);
        }

        return (events ^ HID_SLEEP_TIMEOUT_EVT);
    }

    // USB 动态轮询（三级电源管理）
    if (events & HID_USB_POLL_EVT) {
        extern void USB_Bridge_Poll(void);
        USB_Bridge_Poll();

        if (is_ble_sleeping) {
            // 第三级：蓝牙已关闭，深度降频 500ms
            tmos_start_task(hidEmuTaskId, HID_USB_POLL_EVT, TIME_USB_POLL_SLEEP);
        } else if (is_usb_idle) {
            // 第二级：蓝牙保持，USB 降频 50ms
            tmos_start_task(hidEmuTaskId, HID_USB_POLL_EVT, TIME_USB_POLL_IDLE);
        } else {
            // 第一级：正在输入，全速 ~1.25ms
            tmos_start_task(hidEmuTaskId, HID_USB_POLL_EVT, TIME_USB_POLL_ACTIVE);
        }

        return (events ^ HID_USB_POLL_EVT);
    }

    // USB 空闲降频超时（30秒无输入）
    if (events & HID_USB_IDLE_EVT) {
        is_usb_idle = TRUE;
        return (events ^ HID_USB_IDLE_EVT);
    }

    return 0;
}

// ===================================================================
// 电池测量
// ===================================================================
static void HidEmu_MeasureBattery(void)
{
    uint32_t adc_sum = 0;
    uint16_t adc_avg = 0;
    int32_t  voltage_mv = 0;
    uint8_t  percent = 0;
    signed short raw_val;

    // 1. 切换通道并采集（软件滤波：20次平均）
    ADC_ChannelCfg(BATT_ADC_CHANNEL);
    for (int i = 0; i < 20; i++) {
        raw_val = ADC_ExcutSingleConver() + ADC_RoughCalib_Value;
        if (raw_val < 0) raw_val = 0;
        adc_sum += raw_val;
    }
    adc_avg = adc_sum / 20;

    // 2. 电压换算（根据硬件分压电阻）
    int32_t temp_calc = (int32_t)adc_avg * 2100;
    voltage_mv = (temp_calc + 512) / 1024 - 2100;
    if (voltage_mv < 0) voltage_mv = 0;

    // 3. 查表 + 线性插值获得百分比
    if (voltage_mv >= (int32_t)batt_table[0].mv) {
        percent = 100;
    } else if (voltage_mv <= (int32_t)batt_table[BATT_TABLE_COUNT - 2].mv) {
        percent = 0;
    } else {
        for (int i = 0; i < (int)(BATT_TABLE_COUNT - 1); i++) {
            if (voltage_mv >= (int32_t)batt_table[i + 1].mv) {
                uint16_t high_mv  = batt_table[i].mv;
                uint16_t low_mv   = batt_table[i + 1].mv;
                uint8_t  high_pct = batt_table[i].pct;
                uint8_t  low_pct  = batt_table[i + 1].pct;
                // 线性插值
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

    // 5. 低电量警告
    if (percent <= BATT_LOW_THRESHOLD) {
        tmos_start_task(hidEmuTaskId, HID_SYS_LED_BLINK_EVT, TIME_SYS_LED_BLINK);
    } else {
        tmos_stop_task(hidEmuTaskId, HID_SYS_LED_BLINK_EVT);
        if (!is_sys_led_startup) {
            SYS_LED_OFF();
        }
    }

    LOG_BATT("ADC:%d  V:%dmV  Pct:%d%%\n", adc_avg, (int)voltage_mv, percent);
}

// ===================================================================
// 蓝牙状态回调
// ===================================================================
static void HidEmu_StateCB(gapRole_States_t newState, gapRoleEvent_t *pEvent)
{
    switch (newState & GAPROLE_STATE_ADV_MASK)
    {
        case GAPROLE_STARTED:
        {
            uint8_t ownAddr[6];
            GAPRole_GetParameter(GAPROLE_BD_ADDR, ownAddr);
            GAP_ConfigDeviceAddr(ADDRTYPE_STATIC, ownAddr);
            LOG_BLE("BLE Stack Initialized\n");
        }
        break;

        case GAPROLE_ADVERTISING:
            if (pEvent->gap.opcode == GAP_MAKE_DISCOVERABLE_DONE_EVENT) {
                LOG_BLE("Advertising...\n");
            }
            tmos_stop_task(hidEmuTaskId, HID_BLE_LED_OFF_EVT);
            tmos_start_task(hidEmuTaskId, HID_BLE_LED_BLINK_EVT, TIME_BLE_LED_BLINK);
            break;

        case GAPROLE_CONNECTED:
            if (pEvent->gap.opcode == GAP_LINK_ESTABLISHED_EVENT)
            {
                gapEstLinkReqEvent_t *event = (gapEstLinkReqEvent_t *)pEvent;
                hidEmuConnHandle = event->connectionHandle;

                tmos_start_task(hidEmuTaskId, START_PARAM_UPDATE_EVT, TIME_PARAM_UPDATE_DELAY);
                LOG_BLE("Connected! Handle: %d\n", hidEmuConnHandle);

                // 停止闪烁，点亮 BLE 灯，10秒后熄灭
                tmos_stop_task(hidEmuTaskId, HID_BLE_LED_BLINK_EVT);
                BLE_LED_ON();
                tmos_start_task(hidEmuTaskId, HID_BLE_LED_OFF_EVT, TIME_BLE_LED_CONNECTED);

                // 强制刷新电量
                last_batt_percent = 0;
                tmos_start_task(hidEmuTaskId, START_BATT_READ_EVT, TIME_BATT_AFTER_CONNECT);
            }
            break;

        case GAPROLE_WAITING:  // 连接断开/空闲
            if (pEvent->gap.opcode == GAP_LINK_TERMINATED_EVENT) {
                LOG_BLE("Disconnected. Reason: 0x%02x\n", pEvent->linkTerminate.reason);
            }
            tmos_stop_task(hidEmuTaskId, HID_BLE_LED_OFF_EVT);

            // 处于休眠状态时，不重新开启广播
            if (!is_ble_sleeping) {
                uint8_t adv_enable = TRUE;
                GAPRole_SetParameter(GAPROLE_ADVERT_ENABLED, sizeof(uint8_t), &adv_enable);
                tmos_start_task(hidEmuTaskId, HID_BLE_LED_BLINK_EVT, TIME_BLE_LED_BLINK);
            }
            break;

        default: break;
    }
}

// ===================================================================
// HID 报文回调
// ===================================================================
static uint8_t HidEmu_RptCB(uint8_t id, uint8_t type, uint16_t uuid,
                            uint8_t oper, uint16_t *pLen, uint8_t *pData)
{
    uint8_t status = SUCCESS;

    if (oper == HID_DEV_OPER_WRITE)
    {
        if (uuid == REPORT_UUID && type == HID_REPORT_TYPE_OUTPUT)
        {
            // 可在此处理 LED 输出报文 (pData[0] 为 LED 位掩码)
            status = SUCCESS;
        }
        if (status == SUCCESS)
            status = Hid_SetParameter(id, type, uuid, *pLen, pData);
    }
    else if (oper == HID_DEV_OPER_READ)
    {
        status = Hid_GetParameter(id, type, uuid, pLen, pData);
    }
    else if (oper == HID_DEV_OPER_ENABLE)
    {
        LOG_SYS("HID Notification Enabled\n");
    }

    return status;
}

static void HidEmu_ProcessTMOSMsg(tmos_event_hdr_t *pMsg)
{
    // 预留底层消息处理接口
    return;
}

// ===================================================================
// 对外接口 (Public APIs)
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

/**
 * @brief 重置所有空闲/休眠倒计时（有按键输入时调用）
 * @return TRUE 表示刚从睡眠唤醒，本次按键数据应丢弃
 */
uint8_t HidEmu_ResetIdleTimer(void)
{
    uint8_t just_wake = FALSE;

    // 1. 从软休眠中唤醒
    if (is_ble_sleeping) {
        is_ble_sleeping = FALSE;
        just_wake = TRUE;

        uint8_t adv_enable = TRUE;
        GAPRole_SetParameter(GAPROLE_ADVERT_ENABLED, sizeof(uint8_t), &adv_enable);

        SYS_LED_ON();
        tmos_start_task(hidEmuTaskId, HID_SYS_LED_OFF_EVT, TIME_SYS_LED_WAKE_FLASH);
    }

    // 2. 刷新软休眠倒计时
    tmos_start_task(hidEmuTaskId, HID_SLEEP_TIMEOUT_EVT, TIME_SLEEP_TIMEOUT);

    // 3. 刷新 USB 空闲降频倒计时，清除空闲标志
    is_usb_idle = FALSE;
    tmos_start_task(hidEmuTaskId, HID_USB_IDLE_EVT, TIME_USB_IDLE);

    // 4. 立刻拉满 USB 轮询速度
    tmos_start_task(hidEmuTaskId, HID_USB_POLL_EVT, TIME_USB_POLL_ACTIVE);

    return just_wake;
}