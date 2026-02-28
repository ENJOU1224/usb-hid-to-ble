/**
 * @file debug.h
 * @brief 调试和日志系统头文件
 *
 * ## 架构说明
 *
 * ### 1. 模块定位
 * 本模块属于系统的调试和日志层，提供统一的调试信息输出和硬件指示控制功能。
 * 主要职责：
 * - 调试开关管理：控制不同模块的日志输出
 * - 日志宏定义：提供统一的日志输出接口
 * - LED指示控制：提供硬件LED状态指示
 * - 键盘/鼠标调试：提供专用调试函数接口
 *
 * ### 2. 依赖关系
 * - 上层调用: 所有需要调试输出的模块（usb_bridge.c、error_recovery.c等）
 * - 下层依赖: CH58x_common.h（提供PRINT宏和GPIO操作）
 * - 数据依赖: 无
 *
 * ### 3. 关键数据结构
 * - 调试开关宏：DEBUG_SYS、DEBUG_USB、DEBUG_BLE等
 * - 日志宏：LOG_SYS()、LOG_USB()、LOG_BLE()等
 * - LED引脚定义：LED1_PIN、LED2_PIN
 * - 专用调试宏：DBG_KEYS()、DBG_MOUSE()
 *
 * ### 4. 核心算法
 * - 条件编译：通过预编译开关控制日志输出
 * - 空指令优化：未启用的日志宏被优化为空指令
 * - 硬件抽象：统一的LED控制接口
 *
 * ## 使用指南
 *
 * ### 调试开关配置
 * 1. 在 config.h 或编译选项中定义调试开关
 * 2. 系统会自动定义 DEBUG_ENABLED
 * 3. 根据需要启用特定模块的日志
 *
 * ### 日志输出流程
 * 1. 调用 LOG_SYS()、LOG_USB() 等宏输出日志
 * 2. 如果对应开关已定义，日志会输出到串口
 * 3. 如果开关未定义，日志被优化为空指令
 *
 * ### LED指示控制
 * 1. 在 main.c 中调用 LED_HW_INIT() 初始化LED
 * 2. 使用 LED1_ON()、LED2_ON() 控制LED状态
 * 3. LED默认熄灭，低电平点亮
 *
 * ### 常用函数
 * - LOG_SYS(): 系统日志输出
 * - LOG_USB(): USB相关日志输出
 * - LOG_BLE(): 蓝牙相关日志输出
 * - LED_HW_INIT(): LED硬件初始化
 *
 * ## 调试技巧
 * - 使用 DEBUG_SYS 查看系统初始化和主循环状态
 * - 使用 DEBUG_USB 查看USB设备枚举和数据传输
 * - 使用 DEBUG_BLE 查看蓝牙连接状态
 * - 使用 DEBUG_KEY 查看键盘按键检测
 * - 使用 DEBUG_MOUSE 查看鼠标移动检测
 *
 * @author DIY User & AI Assistant
 * @version V1.0
 */

#ifndef __DEBUG_H__
#define __DEBUG_H__

// 包含公共头文件，确保 GPIO_Pin_x, PRINT 等宏定义有效
#include "CH58x_common.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// 1. 功能开关定义 (Feature Switches)
// 说明：这些开关在预编译时定义，可以在 config.h 中统一配置
// ============================================================

// #define DEBUG_SYS     // 系统日志开关 (Init, Loop, Watchdog)
// #define DEBUG_USB     // USB 设备枚举和通讯日志开关
// #define DEBUG_BLE     // 蓝牙连接状态日志开关
// #define DEBUG_BATT    // 电池电压监控日志开关
// #define DEBUG_KEY     // 键盘按键检测日志开关
// #define DEBUG_MOUSE   // 鼠标移动检测日志开关
// #define ENABLE_LED    // LED 指示灯开关 (关闭可省电)

// ============================================================
// 2. 主硬件开关定义 (Master UART Switch)
// 只要下面任何一个日志功能开启，就自动定义 DEBUG_ENABLED
// ============================================================
#if defined(DEBUG_SYS) || defined(DEBUG_USB) || defined(DEBUG_BLE)  || \
    defined(DEBUG_BATT)|| defined(DEBUG_KEY) || defined(DEBUG_MOUSE)

    #ifndef DEBUG_ENABLED
        #define DEBUG_ENABLED  1  // 在 main.c 中判断是否初始化 UART1
    #endif

#endif

// ============================================================
// 3. 日志宏定义 (Log Macros)
// ============================================================

// -> 系统日志
#ifdef DEBUG_SYS
    #define LOG_SYS(...)  PRINT(__VA_ARGS__)
#else
    #define LOG_SYS(...)  do{}while(0) // 优化为空指令，编译时直接删除
#endif

// -> USB 日志
#ifdef DEBUG_USB
    #define LOG_USB(...)  PRINT(__VA_ARGS__)
#else
    #define LOG_USB(...)  do{}while(0)
#endif

// -> 蓝牙日志
#ifdef DEBUG_BLE
    #define LOG_BLE(...)  PRINT(__VA_ARGS__)
#else
    #define LOG_BLE(...)  do{}while(0)
#endif

// -> 电池日志
#ifdef DEBUG_BATT
    #define LOG_BATT(...) PRINT(__VA_ARGS__)
#else
    #define LOG_BATT(...) do{}while(0)
#endif

// -> 键盘专用调试
#ifdef DEBUG_KEY
    // 声明外部函数（实际在 usb_bridge.c）
    void Show_Current_Keys(uint8_t *report);
    #define DBG_KEYS(r)   Show_Current_Keys(r)
#else
    #define DBG_KEYS(r)   do{}while(0)
#endif

// -> 鼠标专用调试
#ifdef DEBUG_MOUSE
    // 声明外部函数
    void Show_Raw_Mouse(uint8_t *buf, uint8_t len);
    void Show_Current_Mouse(uint8_t *report);
    #define DBG_MOUSE(r)  Show_Current_Mouse(r)
#else
    #define DBG_MOUSE(r)  do{}while(0)
#endif

// ============================================================
// 4. LED 硬件抽象层 (LED HAL)
// ============================================================
// 统一管理LED引脚定义，main.c 和 hidkbd.c 中的定义保持一致
#define LED1_PIN   GPIO_Pin_4  // 系统电源指示 (System Power)
#define LED2_PIN   GPIO_Pin_7  // 蓝牙连接指示 (BLE Link)

// 硬件初始化宏 (在 main.c 的 GPIO 初始化函数中调用)
#ifdef ENABLE_LED

    // 控制函数 (低电平点亮)
    #define LED_HW_INIT()  do{ \
    GPIOB_ModeCfg(LED1_PIN | LED2_PIN, GPIO_ModeOut_PP_5mA); \
    GPIOB_SetBits(LED1_PIN | LED2_PIN); /* 默认熄灭(高电平) */ \
    }while(0)
    #define LED1_ON()      GPIOB_ResetBits(LED1_PIN)
    #define LED1_OFF()     GPIOB_SetBits(LED1_PIN)
    #define LED2_ON()      GPIOB_ResetBits(LED2_PIN)
    #define LED2_OFF()     GPIOB_SetBits(LED2_PIN)

#else

    // 如果LED功能关闭，所有宏替换为空，节省GPIO初始化开销
    #define LED_HW_INIT()  do{}while(0)
    #define LED1_ON()      do{}while(0)
    #define LED1_OFF()     do{}while(0)
    #define LED2_ON()      do{}while(0)
    #define LED2_OFF()     do{}while(0)

#endif

#ifdef __cplusplus
}
#endif

#endif // __DEBUG_H__