#ifndef __DEBUG_H__
#define __DEBUG_H__

// 引入公共头文件，确保 GPIO_Pin_x, PRINT 等定义有效
#include "CH58x_common.h" 

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// 1. 功能开关配置 (Feature Switches)
// 建议：将这些宏移动到项目属性(Pre-processor)或独立的 config.h 中
// ============================================================

// #define DEBUG_SYS     // 启用系统级日志 (Init, Loop, Watchdog)
// #define DEBUG_USB     // 启用 USB 枚举与通讯日志
// #define DEBUG_BLE     // 启用蓝牙状态日志
// #define DEBUG_BATT    // 启用电池电压日志
// #define DEBUG_KEY     // 启用键盘按键矩阵日志
// #define DEBUG_MOUSE   // 启用鼠标坐标日志
// #define ENABLE_LED    // 启用 LED 指示灯 (关闭可省电)

// ============================================================
// 2. 串口硬件主开关 (Master UART Switch)
// 只要开启了任意一个日志功能，就自动定义 DEBUG_ENABLED
// ============================================================
#if defined(DEBUG_SYS) || defined(DEBUG_USB) || defined(DEBUG_BLE)  || \
    defined(DEBUG_BATT)|| defined(DEBUG_KEY) || defined(DEBUG_MOUSE)
    
    #ifndef DEBUG_ENABLED
        #define DEBUG_ENABLED  1  // 用于 main.c 判断是否初始化 UART1
    #endif

#endif

// ============================================================
// 3. 日志宏封装 (Log Macros)
// ============================================================

// -> 系统日志
#ifdef DEBUG_SYS
    #define LOG_SYS(...)  PRINT(__VA_ARGS__)
#else
    #define LOG_SYS(...)  do{}while(0) // 优化为空指令，编译器会直接删除
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
    // 声明外部辅助函数 (实现在 usb_bridge.c)
    void Show_Current_Keys(uint8_t *report);
    #define DBG_KEYS(r)   Show_Current_Keys(r)
#else
    #define DBG_KEYS(r)   do{}while(0)
#endif

// -> 鼠标专用调试
#ifdef DEBUG_MOUSE
    // 声明外部辅助函数
    void Show_Raw_Mouse(uint8_t *buf, uint8_t len);
    void Show_Current_Mouse(uint8_t *report);
    #define DBG_MOUSE(r)  Show_Current_Mouse(r)
#else
    #define DBG_MOUSE(r)  do{}while(0)
#endif

// ============================================================
// 4. LED 硬件抽象层 (LED HAL)
// ============================================================
// 统一在这里修改引脚，main.c 和 hidkbd.c 无需改动
#define LED1_PIN   GPIO_Pin_4  // 系统电源 (System Power)
#define LED2_PIN   GPIO_Pin_7  // 蓝牙连接 (BLE Link)

// 硬件初始化宏 (放在 main.c 的 GPIO 初始化部分)
#ifdef ENABLE_LED

    // 控制宏 (低电平点亮)
    #define LED_HW_INIT()  do{ \
    GPIOB_ModeCfg(LED1_PIN | LED2_PIN, GPIO_ModeOut_PP_5mA); \
    GPIOB_SetBits(LED1_PIN | LED2_PIN); /* 默认熄灭(高电平) */ \
    }while(0)
    #define LED1_ON()      GPIOB_ResetBits(LED1_PIN)
    #define LED1_OFF()     GPIOB_SetBits(LED1_PIN)
    #define LED2_ON()      GPIOB_ResetBits(LED2_PIN)
    #define LED2_OFF()     GPIOB_SetBits(LED2_PIN)

#else

    // 如果禁用 LED，所有操作替换为空，连 GPIO 都不配置，彻底省电
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