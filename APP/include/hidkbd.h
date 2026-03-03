/*********************************************************************
 * File Name          : hidkbd.h
 * Author             : DIY User & AI Assistant
 * Description        : HID 键盘应用层头文件
 *                      - TMOS 任务事件定义
 *                      - 定时器时长集中配置
 *********************************************************************/

#ifndef HIDKBD_H
#define HIDKBD_H

#ifdef __cplusplus
extern "C" {
#endif

// ===================================================================
// TMOS 任务事件位定义 (Task Events)
// ===================================================================
#define START_DEVICE_EVT          0x0001  // 设备启动事件
#define START_PARAM_UPDATE_EVT    0x0004  // 连接参数更新请求
#define START_PHY_UPDATE_EVT      0x0008  // PHY 速率更新请求
#define START_BATT_READ_EVT       0x0010  // 触发一次电量检测
#define HID_SYS_LED_OFF_EVT       0x0100  // SYS 灯定时熄灭
#define HID_SYS_LED_BLINK_EVT     0x0200  // SYS 灯低电量闪烁
#define HID_BLE_LED_OFF_EVT       0x0400  // BLE 灯定时熄灭
#define HID_BLE_LED_BLINK_EVT     0x0800  // BLE 灯广播闪烁
#define HID_USER_KEY_POLL_EVT     0x1000  // 用户按键轮询
#define HID_SLEEP_TIMEOUT_EVT     0x2000  // 软休眠超时事件
#define HID_USB_POLL_EVT          0x4000  // USB 数据轮询
#define HID_USB_IDLE_EVT          0x8000  // USB 空闲降频超时

// ===================================================================
// 定时器时长配置 (Timing Configuration)
// 1 TMOS tick = 0.625ms, 即 1600 ticks = 1 秒
// ===================================================================
#define TICKS_PER_SEC             1600UL  // 每秒对应的 tick 数

// --- LED 时长 ---
#define TIME_SYS_LED_STARTUP      (TICKS_PER_SEC * 10)  // 上电 SYS 灯点亮: 10秒
#define TIME_BLE_LED_CONNECTED    (TICKS_PER_SEC * 10)  // 连接后 BLE 灯点亮: 10秒
#define TIME_SYS_LED_WAKE_FLASH   (TICKS_PER_SEC * 1)   // 唤醒时 SYS 灯闪亮: 1秒
#define TIME_BLE_LED_BLINK        800UL                  // BLE 广播闪烁周期: 0.5秒
#define TIME_SYS_LED_BLINK        (TICKS_PER_SEC * 1)   // 低电量 SYS 闪烁周期: 1秒

// --- 按键与轮询 ---
#define TIME_KEY_POLL             160UL   // 按键轮询间隔: 100ms
#define TIME_USB_POLL_ACTIVE      2UL     // USB 全速轮询: ~1.25ms
#define TIME_USB_POLL_IDLE        80UL    // USB 降速轮询: 50ms
#define TIME_USB_POLL_SLEEP       800UL   // USB 休眠轮询: 500ms

// --- 电源管理 ---
#define TIME_SLEEP_TIMEOUT        (TICKS_PER_SEC * 60 * 10)  // 软休眠超时: 10分钟
#define TIME_USB_IDLE             (TICKS_PER_SEC * 30)       // USB 空闲降频: 30秒

// --- 电量检测 ---
#define TIME_BATT_BOOT_DELAY      (TICKS_PER_SEC * 2)   // 上电首次检测延迟: 2秒
#define TIME_BATT_READ_INTERVAL   (TICKS_PER_SEC * 60)  // 周期检测间隔: 60秒
#define TIME_BATT_AFTER_CONNECT   800UL                  // 连接后刷新电量延时: 0.5秒

// --- 连接参数 ---
#define TIME_PARAM_UPDATE_DELAY   12800UL  // 连接参数更新延迟

// ===================================================================
// 业务阈值配置
// ===================================================================
#define BATT_LOW_THRESHOLD        15  // 低电量警告阈值 (%)

// ===================================================================
// 对外接口声明 (Public API)
// ===================================================================
extern void     HidEmu_Init(void);
extern uint16_t HidEmu_ProcessEvent(uint8_t task_id, uint16_t events);
extern uint8_t  HidEmu_ResetIdleTimer(void);

#ifdef __cplusplus
}
#endif

#endif /* HIDKBD_H */
