/*********************************************************************
 * File Name          : ota_protocol.c
 * Author             : DIY User & AI Assistant
 * Version            : V1.0
 * Description        : OTA协议实现
 *                      实现OTA通信协议和数据结构
 *********************************************************************/

#include "ota_protocol.h"
#include <string.h>

// ===================================================================
// ? OTA协议实现
// ===================================================================

/**
 * @brief  解析OTA控制数据
 * @param  data    数据指针
 * @param  length  数据长度
 * @param  control 控制数据结构体指针
 * @return 0=成功, 1=失败
 */
uint8_t OtaProtocol_ParseControlData(uint8_t *data, uint16_t length, OtaControlData_t *control)
{
    if (data == NULL || control == NULL || length < sizeof(OtaControlData_t)) {
        return 1;
    }

    memcpy(control, data, sizeof(OtaControlData_t));
    return 0;
}

/**
 * @brief  构建OTA状态数据
 * @param  status  状态数据结构体指针
 * @param  buffer  目标缓冲区
 * @param  size    缓冲区大小
 * @return 实际数据长度
 */
uint16_t OtaProtocol_BuildStatusData(OtaStatusData_t *status, uint8_t *buffer, uint16_t size)
{
    if (status == NULL || buffer == NULL || size < sizeof(OtaStatusData_t)) {
        return 0;
    }

    memcpy(buffer, status, sizeof(OtaStatusData_t));
    return sizeof(OtaStatusData_t);
}

/**
 * @brief  构建OTA数据包
 * @param  packet_id  数据包ID
 * @param  data       数据指针
 * @param  length     数据长度
 * @param  buffer     目标缓冲区
 * @param  size       缓冲区大小
 * @return 实际数据长度
 */
uint16_t OtaProtocol_BuildDataPacket(uint16_t packet_id, uint8_t *data, uint16_t length, uint8_t *buffer, uint16_t size)
{
    if (data == NULL || buffer == NULL || length > 244 || size < sizeof(OtaDataPacket_t)) {
        return 0;
    }

    OtaDataPacket_t *packet = (OtaDataPacket_t *)buffer;
    packet->packet_id = packet_id;
    packet->length = length;
    memcpy(packet->data, data, length);

    return sizeof(OtaDataPacket_t);
}

/**
 * @brief  解析OTA数据包
 * @param  data       数据指针
 * @param  length     数据长度
 * @param  packet_id  数据包ID指针
 * @param  packet_data 数据包数据指针
 * @param  packet_len 数据包长度指针
 * @return 0=成功, 1=失败
 */
uint8_t OtaProtocol_ParseDataPacket(uint8_t *data, uint16_t length, uint16_t *packet_id, uint8_t **packet_data, uint16_t *packet_len)
{
    if (data == NULL || packet_id == NULL || packet_data == NULL || packet_len == NULL || length < sizeof(OtaDataPacket_t)) {
        return 1;
    }

    OtaDataPacket_t *packet = (OtaDataPacket_t *)data;
    *packet_id = packet->packet_id;
    *packet_len = packet->length;
    *packet_data = packet->data;

    return 0;
}