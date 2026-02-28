/*********************************************************************
 * File Name          : ota_test.c
 * Author             : DIY User & AI Assistant
 * Version            : V1.0
 * Description        : OTA测试文件
 *                      在不烧录的情况下测试OTA功能
 *********************************************************************/

#include "ota_manager.h"
#include "ota_protocol.h"
#include "ota_service.h"
#include "crc32.h"
#include "debug.h"
#include <string.h>

// ===================================================================
// ? 测试函数声明
// ===================================================================

void OtaTest_RunAll(void);
void OtaTest_CRC32(void);
void OtaTest_Protocol(void);
void OtaTest_StateMachine(void);
void OtaTest_DataStructures(void);

// ===================================================================
// ? 测试函数实现
// ===================================================================

/**
 * @brief  运行所有OTA测试
 */
void OtaTest_RunAll(void)
{
    LOG_SYS("=== OTA测试开始 ===\n");

    OtaTest_CRC32();
    OtaTest_Protocol();
    OtaTest_StateMachine();
    OtaTest_DataStructures();

    LOG_SYS("=== OTA测试完成 ===\n");
}

/**
 * @brief  测试CRC32计算
 */
void OtaTest_CRC32(void)
{
    LOG_SYS("测试CRC32计算...\n");

    // 测试数据
    uint8_t test_data[] = "Hello, World!";
    uint32_t expected_crc = 0x4A17B156; // "Hello, World!" 的CRC32值

    uint32_t calculated_crc = CRC32_Calculate(test_data, strlen((char *)test_data));

    if (calculated_crc == expected_crc) {
        LOG_SYS("CRC32测试通过: 0x%08lX\n", calculated_crc);
    } else {
        LOG_SYS("CRC32测试失败: 计算值=0x%08lX, 期望值=0x%08lX\n",
               calculated_crc, expected_crc);
    }
}

/**
 * @brief  测试协议解析和构建
 */
void OtaTest_Protocol(void)
{
    LOG_SYS("测试协议解析和构建...\n");

    // 测试控制数据解析
    uint8_t control_data[] = {
        0x01, // 命令: OTA_CMD_START
        0x00, 0x10, 0x00, 0x00, // 总大小: 4096字节
        0x12, 0x34, 0x56, 0x78, // CRC32: 0x12345678
        0x01, 0x00, // 版本: 1.0
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 // 保留字段
    };

    OtaControlData_t control;
    if (OtaProtocol_ParseControlData(control_data, sizeof(control_data), &control) == 0) {
        LOG_SYS("控制数据解析成功: 命令=0x%02X, 大小=%lu, CRC32=0x%08lX, 版本=0x%04X\n",
               control.command, control.total_size, control.crc32, control.version);
    } else {
        LOG_SYS("控制数据解析失败\n");
    }

    // 测试状态数据构建
    OtaStatusData_t status = {
        .state = 2, // OTA_STATE_DOWNLOADING
        .total_size = 4096,
        .received_size = 1024,
        .progress = 25,
        .error_code = 0
    };

    uint8_t status_buffer[256];
    uint16_t status_length = OtaProtocol_BuildStatusData(&status, status_buffer, sizeof(status_buffer));

    if (status_length > 0) {
        LOG_SYS("状态数据构建成功: 长度=%d\n", status_length);
    } else {
        LOG_SYS("状态数据构建失败\n");
    }

    // 测试数据包构建
    uint8_t packet_data[] = "Test packet data";
    uint8_t packet_buffer[256];
    uint16_t packet_length = OtaProtocol_BuildDataPacket(1, packet_data, strlen((char *)packet_data),
                                                         packet_buffer, sizeof(packet_buffer));

    if (packet_length > 0) {
        LOG_SYS("数据包构建成功: 长度=%d\n", packet_length);
    } else {
        LOG_SYS("数据包构建失败\n");
    }
}

/**
 * @brief  测试状态机
 */
void OtaTest_StateMachine(void)
{
    LOG_SYS("测试状态机...\n");

    // 初始化OTA管理器
    OtaManager_Init();

    // 测试状态查询
    uint8_t state = OtaManager_GetState();
    LOG_SYS("初始状态: %d (期望: 0)\n", state);

    // 测试开始OTA
    uint8_t result = OtaManager_StartUpdate(4096, 0x12345678);
    LOG_SYS("开始OTA结果: %d (期望: 0)\n", result);

    // 测试状态查询
    state = OtaManager_GetState();
    LOG_SYS("开始后状态: %d (期望: 1)\n", state);

    // 测试是否正在OTA
    uint8_t is_updating = OtaManager_IsUpdating();
    LOG_SYS("是否正在OTA: %d (期望: 1)\n", is_updating);

    // 测试取消OTA
    OtaManager_CancelUpdate();
    state = OtaManager_GetState();
    LOG_SYS("取消后状态: %d (期望: 0)\n", state);

    is_updating = OtaManager_IsUpdating();
    LOG_SYS("取消后是否正在OTA: %d (期望: 0)\n", is_updating);
}

/**
 * @brief  测试数据结构
 */
void OtaTest_DataStructures(void)
{
    LOG_SYS("测试数据结构...\n");

    // 测试结构体大小
    LOG_SYS("FirmwareHeader_t大小: %d 字节\n", sizeof(FirmwareHeader_t));
    LOG_SYS("OtaControlData_t大小: %d 字节\n", sizeof(OtaControlData_t));
    LOG_SYS("OtaStatusData_t大小: %d 字节\n", sizeof(OtaStatusData_t));
    LOG_SYS("OtaDataPacket_t大小: %d 字节\n", sizeof(OtaDataPacket_t));

    // 测试结构体对齐
    FirmwareHeader_t header;
    LOG_SYS("FirmwareHeader_t对齐测试:\n");
    LOG_SYS("  magic偏移: %d\n", (uint32_t)&header.magic - (uint32_t)&header);
    LOG_SYS("  version偏移: %d\n", (uint32_t)&header.version - (uint32_t)&header);
    LOG_SYS("  build偏移: %d\n", (uint32_t)&header.build - (uint32_t)&header);
    LOG_SYS("  size偏移: %d\n", (uint32_t)&header.size - (uint32_t)&header);
    LOG_SYS("  crc32偏移: %d\n", (uint32_t)&header.crc32 - (uint32_t)&header);
    LOG_SYS("  timestamp偏移: %d\n", (uint32_t)&header.timestamp - (uint32_t)&header);

    // 测试版本检查
    uint16_t current_version = OtaManager_GetCurrentVersion();
    LOG_SYS("当前固件版本: 0x%04X\n", current_version);

    uint8_t can_update = OtaManager_CheckNewFirmware(0x0200); // 版本 2.0
    LOG_SYS("版本2.0是否可用: %d (期望: 1)\n", can_update);

    can_update = OtaManager_CheckNewFirmware(0x0050); // 版本 0.5
    LOG_SYS("版本0.5是否可用: %d (期望: 0)\n", can_update);
}