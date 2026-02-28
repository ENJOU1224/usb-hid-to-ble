/**
 * @file ota_service.h
 * @brief OTA BLE服务头文件
 *
 * ## 架构说明
 *
 * ### 1. 模块定位
 * 本模块属于系统的OTA服务层，实现OTA的BLE服务和特征值。
 * 主要职责：
 * - OTA BLE服务初始化：创建和配置OTA服务
 * - 控制特征处理：处理OTA控制命令的写入
 * - 数据特征处理：处理固件数据的写入
 * - 状态特征处理：处理OTA状态的读取和通知
 * - 服务状态管理：管理OTA服务的启用状态
 *
 * ### 2. 依赖关系
 * - 上层调用: ota_manager.c（OTA管理器）
 * - 下层依赖: BLE协议栈（提供服务创建和特征操作）
 * - 数据依赖: ota_protocol.h（OTA协议定义）
 *
 * ### 3. 关键数据结构
 * - OTA服务UUID：0xFFE0
 * - OTA控制特征UUID：0xFFE1
 * - OTA数据特征UUID：0xFFE2
 * - OTA状态特征UUID：0xFFE3
 *
 * ### 4. 核心算法
 * - BLE服务创建：使用标准BLE GATT服务创建流程
 * - 特征值配置：配置控制、数据、状态三个特征
 * - 数据接收处理：处理固件数据的分包接收
 * - 状态通知机制：通过通知机制发送OTA状态更新
 *
 * ## 使用指南
 *
 * ### OTA服务初始化流程
 * 1. 调用OtaService_Init()初始化OTA服务
 * 2. 创建BLE服务和特征值
 * 3. 配置特征属性（读、写、通知）
 * 4. 注册特征回调函数
 *
 * ### OTA通信流程
 * 1. 设备端写入控制特征：OtaService_ControlWrite()
 * 2. 服务端解析控制命令并执行
 * 3. 设备端写入数据特征：OtaService_DataWrite()
 * 4. 服务端接收并处理固件数据
 * 5. 服务端发送状态通知：OtaService_SendNotification()
 * 6. 设备端读取状态特征：OtaService_StatusRead()
 *
 * ### 特征处理函数
 * - OtaService_ControlWrite(): 处理控制命令写入
 * - OtaService_DataWrite(): 处理固件数据写入
 * - OtaService_StatusRead(): 处理状态读取
 * - OtaService_SendNotification(): 发送状态通知
 *
 * ### 服务状态管理
 * - OtaService_IsEnabled(): 检查OTA服务是否启用
 * - OtaService_SetEnabled(): 设置OTA服务启用状态
 *
 * ### 常用函数
 * - OtaService_Init(): 初始化OTA BLE服务
 * - OtaService_ControlWrite(): 处理OTA控制特征写入
 * - OtaService_DataWrite(): 处理OTA数据特征写入
 * - OtaService_StatusRead(): 处理OTA状态特征读取
 * - OtaService_SendNotification(): 发送OTA状态通知
 * - OtaService_IsEnabled(): 获取OTA服务启用状态
 * - OtaService_SetEnabled(): 设置OTA服务启用状态
 *
 * ## 调试技巧
 * - 使用OtaService_IsEnabled()检查服务状态
 * - 监控OtaService_ControlWrite()的控制命令
 * - 监控OtaService_DataWrite()的数据接收
 * - 使用OtaService_SendNotification()发送状态更新
 *
 * @author DIY User & AI Assistant
 * @version V1.0
 */

#ifndef __OTA_SERVICE_H__
#define __OTA_SERVICE_H__

#include <stdint.h>

// ===================================================================
// ? OTA服务配置
// ===================================================================

#define OTA_SERVICE_UUID        0xFFE0  // OTA服务UUID
#define OTA_CONTROL_UUID        0xFFE1  // OTA控制特征UUID
#define OTA_DATA_UUID           0xFFE2  // OTA数据特征UUID
#define OTA_STATUS_UUID         0xFFE3  // OTA状态特征UUID

// ===================================================================
// ? OTA服务函数声明
// ===================================================================

/**
 * @brief  初始化OTA BLE服务
 */
void OtaService_Init(void);

/**
 * @brief  处理OTA控制特征写入
 * @param  data    数据指针
 * @param  length  数据长度
 */
void OtaService_ControlWrite(uint8_t *data, uint16_t length);

/**
 * @brief  处理OTA数据特征写入
 * @param  data    数据指针
 * @param  length  数据长度
 */
void OtaService_DataWrite(uint8_t *data, uint16_t length);

/**
 * @brief  处理OTA状态特征读取
 * @param  data    数据指针
 * @param  length  数据长度指针
 * @return 0=成功, 1=失败
 */
uint8_t OtaService_StatusRead(uint8_t *data, uint16_t *length);

/**
 * @brief  发送OTA状态通知
 */
void OtaService_SendNotification(void);

/**
 * @brief  获取OTA服务是否启用
 * @return 1=启用, 0=未启用
 */
uint8_t OtaService_IsEnabled(void);

/**
 * @brief  设置OTA服务启用状态
 * @param  enabled  启用状态
 */
void OtaService_SetEnabled(uint8_t enabled);

#endif /* __OTA_SERVICE_H__ */