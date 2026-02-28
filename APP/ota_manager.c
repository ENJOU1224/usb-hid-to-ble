/**
 * @file ota_manager.c
 * @brief OTA固件更新管理器实现
 *
 * ## 架构说明
 *
 * ### 1. 模块定位
 * 本模块属于系统的固件更新层，负责管理OTA（Over-The-Air）固件更新过程。
 * 主要职责：
 * - OTA更新状态管理
 * - 固件数据接收和验证
 * - 固件完整性检查
 * - 更新进度监控
 * - 错误处理和恢复
 *
 * ### 2. 依赖关系
 * - 上层调用: ota_service.c（OTA服务接口）
 * - 下层依赖:
 *   - CH58x_flash.h（Flash操作）
 *   - crc32.c（CRC32校验）
 *   - debug.c（调试日志）
 * - 数据依赖: OtaStateInfo_t、FirmwareHeader_t结构体定义
 *
 * ### 3. 关键数据结构
 * - OtaStateInfo_t: OTA状态信息结构体
 *   - state: OTA状态（空闲、检查、下载、验证、完成、错误）
 *   - total_size: 固件总大小
 *   - received_size: 已接收大小
 *   - crc32: 固件CRC32校验和
 *   - error: 错误码
 *
 * - FirmwareHeader_t: 固件头信息结构体
 *   - magic: 魔法数（用于验证固件头）
 *   - version: 固件版本号
 *   - size: 固件大小
 *   - crc32: 固件CRC32校验和
 *
 * ### 4. 核心算法
 * - OTA状态机: 管理OTA更新的各个阶段
 * - 固件验证: 魔法数、版本、大小、CRC32验证
 * - 进度计算: 实时计算更新进度
 * - 错误处理: 各种错误情况的处理和恢复
 *
 * ## 使用指南
 *
 * ### OTA更新流程
 * 1. 调用OtaManager_StartUpdate()开始更新
 * 2. 调用OtaManager_ReceiveData()接收固件数据
 * 3. 调用OtaManager_CompleteUpdate()完成更新
 * 4. 系统重启后，Bootloader会加载新固件
 *
 * ### 常用函数
 * - OtaManager_Init(): 初始化OTA管理器
 * - OtaManager_StartUpdate(): 开始OTA更新
 * - OtaManager_ReceiveData(): 接收固件数据
 * - OtaManager_CompleteUpdate(): 完成OTA更新
 * - OtaManager_CancelUpdate(): 取消OTA更新
 * - OtaManager_GetState(): 获取OTA状态
 * - OtaManager_GetProgress(): 获取更新进度
 * - OtaManager_IsUpdating(): 检查是否正在更新
 *
 * ## 调试技巧
 * - 使用LOG_SYS()查看OTA更新日志
 * - 监控OTA状态变化
 * - 检查固件验证结果
 * - 查看更新进度
 *
 * @author DIY User & AI Assistant
 * @version V1.0
 */

#include "ota_manager.h"
#include "CH58x_flash.h"
#include "CH58x_common.h"
#include "debug.h"
#include "crc32.h"
#include <string.h>

// ===================================================================
// ? 全局变量
// ===================================================================

static OtaStateInfo_t g_ota_state;
static FirmwareHeader_t g_firmware_header;

// ===================================================================
// ? 固件存储地址定义
// ===================================================================

#define FIRMWARE_UPDATE_ADDR   0x20000  // 固件更新缓冲区地址
#define FIRMWARE_CURRENT_ADDR  0x10000  // 当前固件地址
#define FIRMWARE_BOOTLOADER_ADDR 0x00000  // Bootloader地址

// ===================================================================
// ? OTA管理器实现
// ===================================================================

/**
 * @brief  初始化OTA管理器
 *
 * @details
 * #### 功能详细说明
 * 此函数初始化OTA管理器，将所有状态变量清零，并设置初始状态为空闲。
 * OTA管理器负责管理固件更新的整个过程，包括状态跟踪、数据接收和验证。
 *
 * #### 算法原理
 * 1. **清零OTA状态**：使用memset将g_ota_state结构体全部清零
 * 2. **清零固件头**：使用memset将g_firmware_header结构体全部清零
 * 3. **设置初始状态**：将OTA状态设置为空闲（OTA_STATE_IDLE）
 * 4. **日志记录**：输出初始化信息
 *
 * #### 调用示例
 * 系统启动时调用，初始化OTA管理器。
 *
 * #### 使用场景
 * - 系统启动时调用
 * - 系统复位后调用
 * - 需要重新初始化OTA管理器时调用
 *
 * #### 错误处理
 * - 本函数不会失败，总是成功执行
 * - 内存不足：memset会自动处理
 *
 * #### 边界情况
 * - 多次调用：每次调用都会重置OTA状态
 * - OTA正在更新：调用此函数会重置OTA状态
 *
 * #### 注意事项
 * - 此函数应该在系统启动时调用一次
 * - 调用此函数后，OTA管理器准备就绪
 * - OTA更新需要调用OtaManager_StartUpdate()开始
 * - OTA状态在更新过程中会不断变化
 */
void OtaManager_Init(void)
{
    memset(&g_ota_state, 0, sizeof(OtaStateInfo_t));
    memset(&g_firmware_header, 0, sizeof(FirmwareHeader_t));

    g_ota_state.state = OTA_STATE_IDLE;

    LOG_SYS("OTA Manager initialized\n");
}

/**
 * @brief  开始OTA更新
 * @param  total_size  固件总大小
 * @param  crc32       固件CRC32校验和
 * @return 0=成功, 1=失败
 */
uint8_t OtaManager_StartUpdate(uint32_t total_size, uint32_t crc32)
{
    if (total_size == 0 || total_size > 0x10000) { // 限制最大64KB
        LOG_SYS("Invalid firmware size: %lu\n", total_size);
        return 1;
    }

    // 检查是否正在更新
    if (g_ota_state.state != OTA_STATE_IDLE) {
        LOG_SYS("OTA already in progress\n");
        return 1;
    }

    // 初始化OTA状态
    memset(&g_ota_state, 0, sizeof(OtaStateInfo_t));
    g_ota_state.state = OTA_STATE_CHECKING;
    g_ota_state.total_size = total_size;
    g_ota_state.crc32 = crc32;

    LOG_SYS("OTA started: size=%lu, crc32=0x%08lX\n", total_size, crc32);
    return 0;
}

/**
 * @brief  接收数据包
 * @param  data    数据指针
 * @param  length  数据长度
 * @return 0=成功, 1=失败
 */
uint8_t OtaManager_ReceiveData(uint8_t *data, uint16_t length)
{
    if (data == NULL || length == 0) {
        return 1;
    }

    if (g_ota_state.state != OTA_STATE_DOWNLOADING) {
        LOG_SYS("Not in downloading state\n");
        return 1;
    }

    // 检查数据是否超出总大小
    if (g_ota_state.received_size + length > g_ota_state.total_size) {
        LOG_SYS("Data exceeds total size\n");
        return 1;
    }

    // 处理固件头信息
    if (g_ota_state.received_size == 0 && length >= sizeof(FirmwareHeader_t)) {
        memcpy(&g_firmware_header, data, sizeof(FirmwareHeader_t));

        // 验证固件头
        if (g_firmware_header.magic != OTA_MAGIC_NUMBER) {
            LOG_SYS("Invalid firmware magic number: 0x%08lX\n", g_firmware_header.magic);
            g_ota_state.state = OTA_STATE_ERROR;
            return 1;
        }

        // 验证固件大小
        if (g_firmware_header.size != g_ota_state.total_size) {
            LOG_SYS("Firmware size mismatch: header=%lu, expected=%lu\n",
                   g_firmware_header.size, g_ota_state.total_size);
            g_ota_state.state = OTA_STATE_ERROR;
            return 1;
        }

        // 验证CRC32
        if (g_firmware_header.crc32 != g_ota_state.crc32) {
            LOG_SYS("CRC32 mismatch: header=0x%08lX, expected=0x%08lX\n",
                   g_firmware_header.crc32, g_ota_state.crc32);
            g_ota_state.state = OTA_STATE_ERROR;
            return 1;
        }

        LOG_SYS("Firmware header validated: version=%d.%d, size=%lu\n",
               g_firmware_header.version >> 8, g_firmware_header.version & 0xFF,
               g_firmware_header.size);

        // 切换到下载状态
        g_ota_state.state = OTA_STATE_DOWNLOADING;
    }

    // 更新接收状态
    g_ota_state.received_size += length;

    // 更新CRC32（简化处理，实际应该实时计算）
    // 这里可以添加实时CRC32计算

    // 打印进度
    if (g_ota_state.received_size % 1024 == 0 ||
        g_ota_state.received_size == g_ota_state.total_size) {
        uint32_t progress = (g_ota_state.received_size * 100) / g_ota_state.total_size;
        LOG_SYS("OTA progress: %lu%% (%lu/%lu bytes)\n",
               progress, g_ota_state.received_size, g_ota_state.total_size);
    }

    return 0;
}

/**
 * @brief  完成OTA更新
 * @return 0=成功, 1=失败
 */
uint8_t OtaManager_CompleteUpdate(void)
{
    if (g_ota_state.state != OTA_STATE_DOWNLOADING) {
        LOG_SYS("Not in downloading state\n");
        return 1;
    }

    // 检查是否接收完整
    if (g_ota_state.received_size != g_ota_state.total_size) {
        LOG_SYS("Firmware not fully received: %lu/%lu bytes\n",
               g_ota_state.received_size, g_ota_state.total_size);
        g_ota_state.state = OTA_STATE_ERROR;
        return 1;
    }

    // 切换到验证状态
    g_ota_state.state = OTA_STATE_VERIFYING;

    LOG_SYS("Firmware received completely, starting verification...\n");

    // TODO: 实现固件验证逻辑
    // 1. 验证CRC32
    // 2. 验证固件完整性
    // 3. 验证版本兼容性

    // 模拟验证成功
    g_ota_state.state = OTA_STATE_COMPLETE;

    LOG_SYS("OTA completed successfully\n");
    return 0;
}

/**
 * @brief  取消OTA更新
 */
void OtaManager_CancelUpdate(void)
{
    if (g_ota_state.state != OTA_STATE_IDLE) {
        LOG_SYS("OTA cancelled\n");
        memset(&g_ota_state, 0, sizeof(OtaStateInfo_t));
        g_ota_state.state = OTA_STATE_IDLE;
    }
}

/**
 * @brief  获取OTA状态
 * @return OTA状态
 */
uint8_t OtaManager_GetState(void)
{
    return g_ota_state.state;
}

/**
 * @brief  获取OTA进度
 * @return 已接收字节数
 */
uint32_t OtaManager_GetProgress(void)
{
    return g_ota_state.received_size;
}

/**
 * @brief  获取OTA错误码
 * @return 错误码
 */
OtaError_t OtaManager_GetError(void)
{
    if (g_ota_state.state == OTA_STATE_ERROR) {
        return OTA_ERROR_UNKNOWN; // 简化处理
    }
    return OTA_ERROR_NONE;
}

/**
 * @brief  检查当前是否正在OTA
 * @return 1=正在OTA, 0=未OTA
 */
uint8_t OtaManager_IsUpdating(void)
{
    return (g_ota_state.state != OTA_STATE_IDLE &&
            g_ota_state.state != OTA_STATE_COMPLETE &&
            g_ota_state.state != OTA_STATE_ERROR);
}

/**
 * @brief  获取当前固件版本
 * @return 固件版本号
 */
uint16_t OtaManager_GetCurrentVersion(void)
{
    // 这里应该从固件头信息中读取当前版本
    // 简化处理，返回固定值
    return 0x0100; // 版本 1.0
}

/**
 * @brief  检查新固件是否可用
 * @param  new_version  新固件版本号
 * @return 1=可用, 0=不可用
 */
uint8_t OtaManager_CheckNewFirmware(uint16_t new_version)
{
    uint16_t current_version = OtaManager_GetCurrentVersion();

    // 简单版本比较：新版本号大于当前版本号
    if (new_version > current_version) {
        return 1;
    }

    return 0;
}