/**
 * @file error_recovery.h
 * @brief 错误恢复机制头文件
 *
 * ## 架构说明
 *
 * ### 1. 模块定位
 * 本模块属于系统的错误恢复层，提供全面的错误检测和恢复机制。
 * 主要职责：
 * - USB设备断开重连：检测USB设备断开并自动重连
 * - BLE连接自动重连：检测BLE连接断开并自动重连
 * - 看门狗超时安全恢复：检测系统死锁并自动恢复
 * - 错误统计和日志记录：记录各种错误事件并统计分析
 *
 * ### 2. 依赖关系
 * - 上层调用: usb_bridge.c（主桥接逻辑）
 * - 下层依赖: CH583硬件库（看门狗、USB、BLE）
 * - 数据依赖: 无
 *
 * ### 3. 关键数据结构
 * - 错误统计结构：记录各种错误事件的计数
 * - 重连状态机：管理USB和BLE重连状态
 * - 看门狗状态：管理看门狗超时检测
 * - 系统状态：记录系统正常/错误/恢复中状态
 *
 * ### 4. 核心算法
 * - 错误检测：通过状态查询和事件触发检测错误
 * - 重连策略：使用状态机管理重连过程
 * - 看门狗机制：定期喂狗，超时自动恢复
 * - 错误统计：记录错误事件并提供统计分析
 *
 * ## 使用指南
 *
 * ### 错误恢复流程
 * 1. 初始化错误恢复系统：ErrorRecovery_Init()
 * 2. 定期调用轮询函数：ErrorRecovery_Poll()
 * 3. 系统自动检测错误并执行恢复
 * 4. 记录错误事件到统计系统
 *
 * ### USB重连机制
 * 1. 检测USB设备断开：USBReconnect_CheckDisconnect()
 * 2. 启动USB重连：USBReconnect_Start()
 * 3. 执行USB重连：USBReconnect_Execute()
 * 4. 轮询重连状态：USBReconnect_Poll()
 *
 * ### BLE重连机制
 * 1. 检测BLE连接断开：BLEReconnect_CheckDisconnect()
 * 2. 启动BLE重连：BLEReconnect_Start()
 * 3. 执行BLE重连：BLEReconnect_Execute()
 * 4. 轮询重连状态：BLEReconnect_Poll()
 *
 * ### 看门狗机制
 * 1. 初始化看门狗：Watchdog_Init()
 * 2. 定期喂狗：Watchdog_Feed()
 * 3. 检查看门狗超时：Watchdog_CheckTimeout()
 * 4. 执行安全恢复：Watchdog_SafeRecovery()
 *
 * ### 错误统计功能
 * - ErrorStats_Init(): 初始化错误统计
 * - ErrorStats_USBConnect(): 记录USB连接事件
 * - ErrorStats_USBDisconnect(): 记录USB断开事件
 * - ErrorStats_USBEnumFail(): 记录USB枚举失败
 * - ErrorStats_USBCommFail(): 记录USB通信失败
 * - ErrorStats_BLEConnect(): 记录BLE连接事件
 * - ErrorStats_BLEDisconnect(): 记录BLE断开事件
 * - ErrorStats_BLEAuthFail(): 记录BLE认证失败
 * - ErrorStats_BLECommFail(): 记录BLE通信失败
 * - ErrorStats_WatchdogTimeout(): 记录看门狗超时
 * - ErrorStats_SystemReset(): 记录系统复位
 * - ErrorStats_Print(): 打印错误统计
 *
 * ### 常用函数
 * - ErrorRecovery_Init(): 初始化错误恢复系统
 * - ErrorRecovery_Poll(): 错误恢复轮询函数
 * - ErrorRecovery_GetSystemState(): 获取系统状态
 * - ErrorRecovery_SetSystemState(): 设置系统状态
 *
 * ## 调试技巧
 * - 使用ErrorStats_Print()查看错误统计
 * - 监控ErrorRecovery_GetSystemState()了解系统状态
 * - 使用Watchdog_Feed()定期喂狗防止复位
 * - 监控重连状态机了解重连进度
 *
 * @author DIY User & AI Assistant
 * @version V1.0
 */

#ifndef __ERROR_RECOVERY_H__
#define __ERROR_RECOVERY_H__

#include <stdint.h>

// ===================================================================
// ? 错误统计函数声明
// ===================================================================

/**
 * @brief  初始化错误统计
 */
void ErrorStats_Init(void);

/**
 * @brief  记录USB连接事件
 */
void ErrorStats_USBConnect(void);

/**
 * @brief  记录USB断开事件
 */
void ErrorStats_USBDisconnect(void);

/**
 * @brief  记录USB枚举失败
 */
void ErrorStats_USBEnumFail(void);

/**
 * @brief  记录USB通信失败
 */
void ErrorStats_USBCommFail(void);

/**
 * @brief  记录BLE连接事件
 */
void ErrorStats_BLEConnect(void);

/**
 * @brief  记录BLE断开事件
 */
void ErrorStats_BLEDisconnect(void);

/**
 * @brief  记录BLE认证失败
 */
void ErrorStats_BLEAuthFail(void);

/**
 * @brief  记录BLE通信失败
 */
void ErrorStats_BLECommFail(void);

/**
 * @brief  记录看门狗超时
 */
void ErrorStats_WatchdogTimeout(void);

/**
 * @brief  记录系统复位
 */
void ErrorStats_SystemReset(void);

/**
 * @brief  打印错误统计
 */
void ErrorStats_Print(void);

// ===================================================================
// ? USB重连函数声明
// ===================================================================

/**
 * @brief  初始化USB重连状态
 */
void USBReconnect_Init(void);

/**
 * @brief  检测USB设备断开
 * @return 1=断开, 0=正常
 */
uint8_t USBReconnect_CheckDisconnect(void);

/**
 * @brief  启动USB重连
 */
void USBReconnect_Start(void);

/**
 * @brief  执行USB重连
 * @return 1=重连成功, 0=重连失败或进行中
 */
uint8_t USBReconnect_Execute(void);

/**
 * @brief  USB重连轮询函数
 * @param  tick_ms  系统滴答计数（毫秒）
 */
void USBReconnect_Poll(uint16_t tick_ms);

/**
 * @brief  设置USB重连使能
 * @param  enable  1=使能, 0=禁用
 */
void USBReconnect_SetEnable(uint8_t enable);

// ===================================================================
// ? BLE重连函数声明
// ===================================================================

/**
 * @brief  初始化BLE重连状态
 */
void BLEReconnect_Init(void);

/**
 * @brief  检测BLE连接断开
 * @return 1=断开, 0=正常
 */
uint8_t BLEReconnect_CheckDisconnect(void);

/**
 * @brief  启动BLE重连
 */
void BLEReconnect_Start(void);

/**
 * @brief  执行BLE重连
 * @return 1=重连成功, 0=重连失败或进行中
 */
uint8_t BLEReconnect_Execute(void);

/**
 * @brief  BLE重连轮询函数
 * @param  tick_ms  系统滴答计数（毫秒）
 */
void BLEReconnect_Poll(uint16_t tick_ms);

/**
 * @brief  设置BLE重连使能
 * @param  enable  1=使能, 0=禁用
 */
void BLEReconnect_SetEnable(uint8_t enable);

// ===================================================================
// ? 看门狗函数声明
// ===================================================================

/**
 * @brief  初始化看门狗
 */
void Watchdog_Init(void);

/**
 * @brief  喂狗函数
 */
void Watchdog_Feed(void);

/**
 * @brief  检查看门狗超时
 * @return 1=超时, 0=正常
 */
uint8_t Watchdog_CheckTimeout(void);

/**
 * @brief  执行看门狗安全恢复
 */
void Watchdog_SafeRecovery(void);

/**
 * @brief  看门狗轮询函数
 * @param  tick_ms  系统滴答计数（毫秒）
 */
void Watchdog_Poll(uint16_t tick_ms);

// ===================================================================
// ? 系统状态管理函数声明
// ===================================================================

/**
 * @brief  获取系统状态
 * @return 系统状态 (0=正常, 1=错误, 2=恢复中)
 */
uint8_t ErrorRecovery_GetSystemState(void);

/**
 * @brief  设置系统状态
 * @param  state  系统状态 (0=正常, 1=错误, 2=恢复中)
 */
void ErrorRecovery_SetSystemState(uint8_t state);

/**
 * @brief  初始化错误恢复系统
 */
void ErrorRecovery_Init(void);

/**
 * @brief  错误恢复轮询函数
 * @param  tick_ms  系统滴答计数（毫秒）
 */
void ErrorRecovery_Poll(uint16_t tick_ms);

#endif /* __ERROR_RECOVERY_H__ */