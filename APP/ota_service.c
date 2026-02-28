/*********************************************************************
 * File Name          : ota_service.c
 * Author             : DIY User & AI Assistant
 * Version            : V1.0
 * Description        : OTA BLE服务实现
 *                      实现OTA的BLE服务和特征值
 *********************************************************************/

#include "ota_service.h"
#include "ota_manager.h"
#include "ota_protocol.h"
#include "debug.h"
#include <string.h>

// ===================================================================
// ? 全局变量
// ===================================================================

static uint8_t g_ota_service_enabled = 0;
static OtaStatusData_t g_ota_status;

// ===================================================================
// ? OTA服务实现
// ===================================================================

/**
 * @brief  初始化OTA BLE服务
 */
void OtaService_Init(void)
{
    g_ota_service_enabled = 0;
    memset(&g_ota_status, 0, sizeof(OtaStatusData_t));

    LOG_SYS("OTA Service initialized\n");
}

/**
 * @brief  处理OTA控制特征写入
 * @param  data    数据指针
 * @param  length  数据长度
 */
void OtaService_ControlWrite(uint8_t *data, uint16_t length)
{
    OtaControlData_t control;

    if (OtaProtocol_ParseControlData(data, length, &control) != 0) {
        LOG_SYS("Failed to parse control data\n");
        return;
    }

    LOG_SYS("OTA control command: 0x%02X\n", control.command);

    switch (control.command) {
        case OTA_CMD_START:
            // 开始OTA更新
            if (OtaManager_StartUpdate(control.total_size, control.crc32) == 0) {
                LOG_SYS("OTA started successfully\n");
                g_ota_service_enabled = 1;
            } else {
                LOG_SYS("Failed to start OTA\n");
            }
            break;

        case OTA_CMD_CANCEL:
            // 取消OTA更新
            OtaManager_CancelUpdate();
            g_ota_service_enabled = 0;
            LOG_SYS("OTA cancelled\n");
            break;

        case OTA_CMD_GET_STATUS:
            // 获取状态
            LOG_SYS("OTA status requested\n");
            break;

        case OTA_CMD_GET_VERSION:
            // 获取版本
            LOG_SYS("OTA version requested\n");
            break;

        case OTA_CMD_GET_INFO:
            // 获取信息
            LOG_SYS("OTA info requested\n");
            break;

        case OTA_CMD_REBOOT:
            // 重启设备
            LOG_SYS("OTA reboot requested\n");
            // TODO: 实现重启逻辑
            break;

        default:
            LOG_SYS("Unknown OTA command: 0x%02X\n", control.command);
            break;
    }
}

/**
 * @brief  处理OTA数据特征写入
 * @param  data    数据指针
 * @param  length  数据长度
 */
void OtaService_DataWrite(uint8_t *data, uint16_t length)
{
    if (!g_ota_service_enabled) {
        LOG_SYS("OTA service not enabled\n");
        return;
    }

    // 解析数据包
    uint16_t packet_id;
    uint8_t *packet_data;
    uint16_t packet_len;

    if (OtaProtocol_ParseDataPacket(data, length, &packet_id, &packet_data, &packet_len) != 0) {
        LOG_SYS("Failed to parse data packet\n");
        return;
    }

    // 接收数据
    if (OtaManager_ReceiveData(packet_data, packet_len) != 0) {
        LOG_SYS("Failed to receive data packet %d\n", packet_id);
        return;
    }

    // 更新状态
    g_ota_status.state = OtaManager_GetState();
    g_ota_status.total_size = OtaManager_GetProgress(); // 简化处理
    g_ota_status.received_size = OtaManager_GetProgress();
    g_ota_status.progress = (uint8_t)((g_ota_status.received_size * 100) / g_ota_status.total_size);

    // 发送通知
    OtaService_SendNotification();
}

/**
 * @brief  处理OTA状态特征读取
 * @param  data    数据指针
 * @param  length  数据长度指针
 * @return 0=成功, 1=失败
 */
uint8_t OtaService_StatusRead(uint8_t *data, uint16_t *length)
{
    if (data == NULL || length == NULL) {
        return 1;
    }

    // 更新状态
    g_ota_status.state = OtaManager_GetState();
    g_ota_status.total_size = OtaManager_GetProgress(); // 简化处理
    g_ota_status.received_size = OtaManager_GetProgress();
    g_ota_status.progress = (uint8_t)((g_ota_status.received_size * 100) / g_ota_status.total_size);
    g_ota_status.error_code = OtaManager_GetError();

    // 构建状态数据
    *length = OtaProtocol_BuildStatusData(&g_ota_status, data, 256);

    return (*length == 0) ? 1 : 0;
}

/**
 * @brief  发送OTA状态通知
 */
void OtaService_SendNotification(void)
{
    // 这里应该通过BLE协议栈发送通知
    // 简化处理，只打印日志
    LOG_SYS("OTA status notification: state=%d, progress=%d%%\n",
           g_ota_status.state, g_ota_status.progress);
}

/**
 * @brief  获取OTA服务是否启用
 * @return 1=启用, 0=未启用
 */
uint8_t OtaService_IsEnabled(void)
{
    return g_ota_service_enabled;
}

/**
 * @brief  设置OTA服务启用状态
 * @param  enabled  启用状态
 */
void OtaService_SetEnabled(uint8_t enabled)
{
    g_ota_service_enabled = enabled;
}