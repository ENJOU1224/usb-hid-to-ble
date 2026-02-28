/**
 * @file usb_device_manager.h
 * @brief USB设备管理器头文件
 *
 * ## 架构说明
 *
 * ### 1. 模块定位
 * 本模块属于系统的USB设备管理层，提供多设备管理和状态维护功能。
 * 主要职责：
 * - 多设备管理：支持同时管理多个USB设备
 * - 设备类型识别：识别和分类USB设备类型
 * - 独立同步管理：为每个设备维护独立的DATA0/DATA1同步位
 * - 设备状态维护：管理设备连接状态和有效性
 * - 报告缓冲区管理：管理设备的HID报告数据
 *
 * ### 2. 依赖关系
 * - 上层调用: usb_bridge.c（USB桥接逻辑）
 * - 下层依赖: 无
 * - 数据依赖: usb_host_common.h（端点管理）
 *
 * ### 3. 关键数据结构
 * - UsbDevice_t: USB设备结构体
 * - UsbDeviceType_t: 设备类型枚举
 * - 设备管理器数组：管理多个设备实例
 *
 * ### 4. 核心算法
 * - 设备发现：通过设备地址和端点识别设备
 * - 设备分类：根据HID报告特征分类设备类型
 * - 同步位管理：为每个设备维护独立的同步状态
 * - 设备数组管理：使用数组管理多个设备实例
 *
 * ## 使用指南
 *
 * ### 设备管理流程
 * 1. 初始化设备管理器：UsbDeviceManager_Init()
 * 2. 发现新设备：UsbDeviceManager_AddDevice()
 * 3. 更新设备状态：UsbDeviceManager_UpdateSyncToggle()
 * 4. 获取设备信息：UsbDeviceManager_GetDevice()
 * 5. 移除设备：UsbDeviceManager_RemoveDevice()
 *
 * ### 设备类型识别
 * - DEV_TYPE_KEYBOARD: 键盘设备
 * - DEV_TYPE_MOUSE: 鼠标设备
 * - DEV_TYPE_GAMEPAD: 游戏手柄设备
 * - DEV_TYPE_HID_OTHER: 其他HID设备
 *
 * ### 设备结构体说明
 * - dev_addr: 设备地址（HUB端口或设备地址）
 * - dev_type: 设备类型
 * - endpoint: 端点地址（包含同步位）
 * - sync_toggle: DATA0/DATA1同步位
 * - report_buffer: HID报告缓冲区
 * - is_connected: 连接状态
 * - is_valid: 有效性标志
 *
 * ### 多设备支持
 * - 最大设备数：MAX_USB_DEVICES（4个）
 * - 设备索引：0-3
 * - 设备查找：通过类型查找设备
 * - 设备计数：获取活动设备数量
 *
 * ### 常用函数
 * - UsbDeviceManager_Init(): 初始化设备管理器
 * - UsbDeviceManager_AddDevice(): 添加设备到管理器
 * - UsbDeviceManager_RemoveDevice(): 移除设备
 * - UsbDeviceManager_GetDevice(): 获取设备信息
 * - UsbDeviceManager_FindDeviceByType(): 查找指定类型的设备
 * - UsbDeviceManager_UpdateSyncToggle(): 更新设备同步位
 * - UsbDeviceManager_GetSyncToggle(): 获取设备同步位
 * - UsbDeviceManager_UpdateReport(): 更新设备报告缓冲区
 * - UsbDeviceManager_GetReport(): 获取设备报告缓冲区
 * - UsbDeviceManager_IsValid(): 检查设备是否有效
 * - UsbDeviceManager_GetActiveCount(): 获取活动设备数量
 * - UsbDeviceManager_PrintStatus(): 打印设备管理器状态
 *
 * ## 调试技巧
 * - 使用UsbDeviceManager_PrintStatus()查看设备状态
 * - 使用UsbDeviceManager_GetActiveCount()获取设备数量
 * - 使用UsbDeviceManager_FindDeviceByType()查找特定设备
 * - 监控设备同步位状态确保数据同步正确
 *
 * @author DIY User & AI Assistant
 * @version V1.0
 */

#ifndef __USB_DEVICE_MANAGER_H__
#define __USB_DEVICE_MANAGER_H__

#include <stdint.h>

// ===================================================================
// ? 设备管理器配置
// ===================================================================

#define MAX_USB_DEVICES 4       // 最大支持设备数量
#define MAX_REPORT_SIZE 8       // HID报告最大大小

// ===================================================================
// ? 设备类型定义
// ===================================================================

typedef enum {
    DEV_TYPE_UNKNOWN = 0,    // 未知设备
    DEV_TYPE_KEYBOARD = 1,   // 键盘设备
    DEV_TYPE_MOUSE = 2,      // 鼠标设备
    DEV_TYPE_GAMEPAD = 3,    // 游戏手柄设备
    DEV_TYPE_HID_OTHER = 4   // 其他HID设备
} UsbDeviceType_t;

// ===================================================================
// ? USB设备结构体定义
// ===================================================================

// 使用packed结构体减少内存填充，节省8字节（4个设备 × 2字节填充）
typedef struct __attribute__((packed)) {
    uint8_t dev_addr;           // 设备地址 (HUB端口或设备地址)
    uint8_t dev_type;           // 设备类型 (KEYBOARD, MOUSE, GAMEPAD等)
    uint8_t endpoint;           // 端点地址 (包含同步位)
    uint8_t sync_toggle;        // DATA0/DATA1同步位 (0=DATA0, 1=DATA1)
    uint8_t report_buffer[MAX_REPORT_SIZE];   // 报告缓冲区
    uint8_t is_connected;       // 连接状态 (1=已连接, 0=未连接)
    uint8_t is_valid;           // 有效性标志 (1=有效, 0=无效)
} UsbDevice_t;

// ===================================================================
// ? 函数声明
// ===================================================================

/**
 * @brief  初始化设备管理器
 */
void UsbDeviceManager_Init(void);

/**
 * @brief  添加设备到管理器
 * @param  dev_addr  设备地址
 * @param  dev_type  设备类型
 * @param  endpoint  端点地址
 * @return 设备索引 (0-3), 0xFF表示失败
 */
uint8_t UsbDeviceManager_AddDevice(uint8_t dev_addr, uint8_t dev_type, uint8_t endpoint);

/**
 * @brief  移除设备
 * @param  dev_index  设备索引
 */
void UsbDeviceManager_RemoveDevice(uint8_t dev_index);

/**
 * @brief  获取设备信息
 * @param  dev_index  设备索引
 * @return 设备结构体指针
 */
UsbDevice_t* UsbDeviceManager_GetDevice(uint8_t dev_index);

/**
 * @brief  查找指定类型的设备
 * @param  dev_type  设备类型
 * @return 设备索引 (0-3), 0xFF表示未找到
 */
uint8_t UsbDeviceManager_FindDeviceByType(uint8_t dev_type);

/**
 * @brief  更新设备同步位
 * @param  dev_index  设备索引
 * @param  toggle  同步位 (0=DATA0, 1=DATA1)
 */
void UsbDeviceManager_UpdateSyncToggle(uint8_t dev_index, uint8_t toggle);

/**
 * @brief  获取设备同步位
 * @param  dev_index  设备索引
 * @return 同步位 (0=DATA0, 1=DATA1)
 */
uint8_t UsbDeviceManager_GetSyncToggle(uint8_t dev_index);

/**
 * @brief  更新设备报告缓冲区
 * @param  dev_index  设备索引
 * @param  report  报告数据
 * @param  len  报告长度
 */
void UsbDeviceManager_UpdateReport(uint8_t dev_index, uint8_t* report, uint8_t len);

/**
 * @brief  获取设备报告缓冲区
 * @param  dev_index  设备索引
 * @return 报告缓冲区指针
 */
uint8_t* UsbDeviceManager_GetReport(uint8_t dev_index);

/**
 * @brief  检查设备是否有效
 * @param  dev_index  设备索引
 * @return 1=有效, 0=无效
 */
uint8_t UsbDeviceManager_IsValid(uint8_t dev_index);

/**
 * @brief  获取活动设备数量
 * @return 活动设备数量
 */
uint8_t UsbDeviceManager_GetActiveCount(void);

/**
 * @brief  打印设备管理器状态
 */
void UsbDeviceManager_PrintStatus(void);

#endif /* __USB_DEVICE_MANAGER_H__ */
