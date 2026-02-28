/**
 * @file ota_protocol.h
 * @brief OTA协议定义头文件
 *
 * ## 架构说明
 *
 * ### 1. 模块定位
 * 本模块属于系统的OTA协议层，定义OTA通信协议和数据结构。
 * 主要职责：
 * - OTA协议版本管理：定义协议版本和UUID
 * - OTA控制命令定义：定义OTA控制命令枚举
 * - OTA数据结构定义：定义控制数据、状态数据、数据包结构
 * - OTA协议函数声明：提供协议解析和构建函数接口
 *
 * ### 2. 依赖关系
 * - 上层调用: ota_manager.c（OTA管理器）、ota_service.c（OTA服务）
 * - 下层依赖: 无
 * - 数据依赖: 无
 *
 * ### 3. 关键数据结构
 * - OtaCommand_t: OTA控制命令枚举
 * - OtaControlData_t: OTA控制数据结构
 * - OtaStatusData_t: OTA状态数据结构
 * - OtaDataPacket_t: OTA数据包结构
 *
 * ### 4. 核心算法
 * - 协议版本管理：使用16位版本号（高8位主版本，低8位次版本）
 * - BLE服务UUID：定义OTA服务和特征UUID
 * - 数据包格式：定义OTA数据包的ID、长度和数据内容
 * - 协议解析：解析控制命令和状态数据
 *
 * ## 使用指南
 *
 * ### OTA协议流程
 * 1. 设备端发送OTA控制命令（开始、取消、获取状态等）
 * 2. 服务端解析控制命令并执行相应操作
 * 3. 服务端发送状态数据通知设备端
 * 4. 设备端发送固件数据包
 * 5. 服务端接收并验证数据包
 *
 * ### 控制命令定义
 * - OTA_CMD_START: 开始OTA更新
 * - OTA_CMD_CANCEL: 取消OTA更新
 * - OTA_CMD_GET_STATUS: 获取OTA状态
 * - OTA_CMD_GET_VERSION: 获取版本信息
 * - OTA_CMD_GET_INFO: 获取设备信息
 * - OTA_CMD_REBOOT: 重启设备
 *
 * ### 数据结构说明
 * - OtaControlData_t: 包含命令、总大小、CRC32、版本等信息
 * - OtaStatusData_t: 包含状态、总大小、已接收大小、进度等信息
 * - OtaDataPacket_t: 包含数据包ID、长度和数据内容（244字节）
 *
 * ### 常用函数
 * - OtaProtocol_ParseControlData(): 解析OTA控制数据
 * - OtaProtocol_BuildStatusData(): 构建OTA状态数据
 * - OtaProtocol_BuildDataPacket(): 构建OTA数据包
 * - OtaProtocol_ParseDataPacket(): 解析OTA数据包
 *
 * ## 调试技巧
 * - 使用OtaProtocol_ParseControlData()验证控制命令格式
 * - 使用OtaProtocol_BuildStatusData()构建状态响应
 * - 使用OtaProtocol_BuildDataPacket()构建数据包
 * - 使用OtaProtocol_ParseDataPacket()解析接收到的数据包
 *
 * @author DIY User & AI Assistant
 * @version V1.0
 */

#ifndef __OTA_PROTOCOL_H__
#define __OTA_PROTOCOL_H__

#include <stdint.h>

// ===================================================================
// ? OTA协议定义
// ===================================================================

#define OTA_PROTOCOL_VERSION    0x0100  // OTA协议版本 1.0
#define OTA_SERVICE_UUID        0xFFE0  // OTA服务UUID
#define OTA_CONTROL_UUID        0xFFE1  // OTA控制特征UUID
#define OTA_DATA_UUID           0xFFE2  // OTA数据特征UUID
#define OTA_STATUS_UUID         0xFFE3  // OTA状态特征UUID

// ===================================================================
// ? OTA控制命令定义
// ===================================================================

typedef enum {
    OTA_CMD_START = 0x01,     // 开始OTA
    OTA_CMD_CANCEL = 0x02,    // 取消OTA
    OTA_CMD_GET_STATUS = 0x03, // 获取状态
    OTA_CMD_GET_VERSION = 0x04, // 获取版本
    OTA_CMD_GET_INFO = 0x05,    // 获取信息
    OTA_CMD_REBOOT = 0x06       // 重启设备
} OtaCommand_t;

// ===================================================================
// ? OTA控制数据结构
// ===================================================================

typedef struct __attribute__((packed)) {
    uint8_t  command;         // 命令类型
    uint32_t total_size;      // 固件总大小
    uint32_t crc32;           // CRC32校验和
    uint16_t version;         // 固件版本号
    uint8_t  reserved[8];     // 保留字段
} OtaControlData_t;

// ===================================================================
// ? OTA状态数据结构
// ===================================================================

typedef struct __attribute__((packed)) {
    uint8_t  state;           // OTA状态
    uint32_t total_size;      // 总大小
    uint32_t received_size;   // 已接收大小
    uint8_t  progress;        // 进度百分比
    uint8_t  error_code;      // 错误码
    uint8_t  reserved[6];     // 保留字段
} OtaStatusData_t;

// ===================================================================
// ? OTA数据包结构
// ===================================================================

typedef struct __attribute__((packed)) {
    uint16_t packet_id;       // 数据包ID
    uint16_t length;          // 数据长度
    uint8_t  data[244];       // 数据内容（256-12=244字节）
} OtaDataPacket_t;

// ===================================================================
// ? OTA协议函数声明
// ===================================================================

/**
 * @brief  解析OTA控制数据
 * @param  data    数据指针
 * @param  length  数据长度
 * @param  control 控制数据结构体指针
 * @return 0=成功, 1=失败
 */
uint8_t OtaProtocol_ParseControlData(uint8_t *data, uint16_t length, OtaControlData_t *control);

/**
 * @brief  构建OTA状态数据
 * @param  status  状态数据结构体指针
 * @param  buffer  目标缓冲区
 * @param  size    缓冲区大小
 * @return 实际数据长度
 */
uint16_t OtaProtocol_BuildStatusData(OtaStatusData_t *status, uint8_t *buffer, uint16_t size);

/**
 * @brief  构建OTA数据包
 * @param  packet_id  数据包ID
 * @param  data       数据指针
 * @param  length     数据长度
 * @param  buffer     目标缓冲区
 * @param  size       缓冲区大小
 * @return 实际数据长度
 */
uint16_t OtaProtocol_BuildDataPacket(uint16_t packet_id, uint8_t *data, uint16_t length, uint8_t *buffer, uint16_t size);

/**
 * @brief  解析OTA数据包
 * @param  data       数据指针
 * @param  length     数据长度
 * @param  packet_id  数据包ID指针
 * @param  packet_data 数据包数据指针
 * @param  packet_len 数据包长度指针
 * @return 0=成功, 1=失败
 */
uint8_t OtaProtocol_ParseDataPacket(uint8_t *data, uint16_t length, uint16_t *packet_id, uint8_t **packet_data, uint16_t *packet_len);

#endif /* __OTA_PROTOCOL_H__ */