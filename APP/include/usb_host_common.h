/**
 * @file usb_host_common.h
 * @brief USB Host 公共函数头文件
 *
 * ## 架构说明
 *
 * ### 1. 模块定位
 * 本模块属于系统的USB主机协议层，提供统一的USB主机操作接口。
 * 主要职责：
 * - USB主机事务执行：提供标准化的USB IN事务接口
 * - 端点同步管理：管理DATA0/DATA1同步位
 * - 端点地址管理：端点地址的编码和解码
 * - 设备端点管理：从设备结构体中获取和保存端点信息
 *
 * ### 2. 依赖关系
 * - 上层调用: usb_bridge.c（USB桥接逻辑）
 * - 下层依赖: CH58xBLE_LIB.H（CH583硬件库）、CH58x_common.h（公共定义）
 * - 数据依赖: usb_device_manager.h（设备管理器）
 *
 * ### 3. 关键数据结构
 * - 端点地址格式：Bit7=同步位, Bit0-6=端点号
 * - 同步位标志：RB_UH_R_TOG | RB_UH_T_TOG 或 0
 * - 设备索引：0=键盘, 1=鼠标
 *
 * ### 4. 核心算法
 * - USB主机事务执行：使用CH583硬件寄存器执行USB IN事务
 * - DATA0/DATA1同步：通过Bit7位管理端点同步状态
 * - 端点地址编码：将同步位和端点号组合成单字节地址
 * - 设备端点映射：通过设备索引和接口编号获取端点地址
 *
 * ## 使用指南
 *
 * ### USB主机事务执行流程
 * 1. 从设备结构体获取端点地址：GetEndpointFromDevice()
 * 2. 检查端点有效性：IsEndpointValid()
 * 3. 获取同步位标志：Endpoint_GetToggleFlag()
 * 4. 执行USB事务：USB_HostTransact()
 * 5. 更新同步位：Endpoint_SyncToggle()
 * 6. 保存端点到设备：SetEndpointToDevice()
 *
 * ### 端点地址管理
 * - 端点地址格式：Bit7=同步位, Bit0-6=端点号
 * - 同步位0表示DATA0，1表示DATA1
 * - 翻转同步位：Endpoint_SyncToggle()
 * - 获取端点号：Endpoint_GetAddr()
 *
 * ### 设备端点映射
 * - 键盘设备：dev_index=0
 * - 鼠标设备：dev_index=1
 * - 接口编号：len=0表示主设备，>0表示HUB端口
 *
 * ### 常用函数
 * - USB_HostTransact(): 执行USB主机IN事务
 * - Endpoint_GetToggle(): 获取端点同步位
 * - Endpoint_SetToggle(): 设置端点同步位
 * - Endpoint_SyncToggle(): 翻转端点同步位
 * - GetEndpointFromDevice(): 从设备获取端点地址
 * - SetEndpointToDevice(): 保存端点到设备
 * - IsEndpointValid(): 检查端点有效性
 * - Endpoint_GetAddr(): 获取端点地址（去除同步位）
 * - Endpoint_GetToggleFlag(): 获取端点同步位标志
 *
 * ## 调试技巧
 * - 使用Endpoint_GetToggle()检查当前同步位状态
 * - 使用IsEndpointValid()验证端点地址有效性
 * - 使用Endpoint_GetAddr()获取实际的端点号
 * - 监控USB_HostTransact()的返回值判断事务状态
 *
 * @author DIY User & AI Assistant
 * @version V1.0
 */

#ifndef __USB_HOST_COMMON_H__
#define __USB_HOST_COMMON_H__

#include "CH58xBLE_LIB.H"
#include "CH58x_common.h"

// ===================================================================
// ? 公共函数声明
// ===================================================================

/**
 * @brief  执行USB主机IN事务
 * @param  endp_addr  端点地址 (Bit7=同步位, Bit0-6=端点号)
 * @param  toggle_flag 同步位标志 (RB_UH_R_TOG | RB_UH_T_TOG 或 0)
 * @param  timeout    超时时间
 * @return 错误码 (ERR_SUCCESS 表示成功)
 */
uint8_t USB_HostTransact(uint8_t endp_addr, uint8_t toggle_flag, uint16_t timeout);

/**
 * @brief  获取端点同步位
 * @param  endp_addr  端点地址 (Bit7=同步位, Bit0-6=端点号)
 * @return 同步位标志 (RB_UH_R_TOG | RB_UH_T_TOG 或 0)
 */
uint8_t Endpoint_GetToggle(uint8_t endp_addr);

/**
 * @brief  设置端点同步位
 * @param  endp_addr  端点地址 (Bit7=同步位, Bit0-6=端点号)
 * @param  toggle     同步位值 (0=DATA0, 1=DATA1)
 * @return 设置后的端点地址
 */
uint8_t Endpoint_SetToggle(uint8_t endp_addr, uint8_t toggle);

/**
 * @brief  翻转端点同步位 (DATA0 <-> DATA1)
 * @param  endp_addr  端点地址 (Bit7=同步位, Bit0-6=端点号)
 * @return 翻转后的端点地址
 */
uint8_t Endpoint_SyncToggle(uint8_t endp_addr);

/**
 * @brief  从设备结构体中获取端点地址
 * @param  len        接口编号 (0=主设备, >0=HUB端口)
 * @param  dev_index  设备索引 (0=键盘, 1=鼠标)
 * @return 端点地址 (Bit7=同步位, Bit0-6=端点号)
 */
uint8_t GetEndpointFromDevice(uint8_t len, uint8_t dev_index);

/**
 * @brief  将端点地址保存到设备结构体
 * @param  len        接口编号 (0=主设备, >0=HUB端口)
 * @param  endp_addr  端点地址 (Bit7=同步位, Bit0-6=端点号)
 * @param  dev_index  设备索引 (0=键盘, 1=鼠标)
 */
void SetEndpointToDevice(uint8_t len, uint8_t endp_addr, uint8_t dev_index);

/**
 * @brief  检查端点是否有效
 * @param  endp_addr  端点地址 (Bit7=同步位, Bit0-6=端点号)
 * @return 1=有效, 0=无效
 */
uint8_t IsEndpointValid(uint8_t endp_addr);

/**
 * @brief  获取端点地址 (去除同步位)
 * @param  endp_addr  端点地址 (Bit7=同步位, Bit0-6=端点号)
 * @return 端点地址 (Bit0-6)
 */
uint8_t Endpoint_GetAddr(uint8_t endp_addr);

/**
 * @brief  获取端点同步位标志
 * @param  endp_addr  端点地址 (Bit7=同步位, Bit0-6=端点号)
 * @return 同步位标志 (RB_UH_R_TOG | RB_UH_T_TOG 或 0)
 */
uint8_t Endpoint_GetToggleFlag(uint8_t endp_addr);

// ===================================================================
// ? 常量定义
// ===================================================================

// 设备类型定义
#define DEV_TYPE_KEYBOARD   0
#define DEV_TYPE_MOUSE      1

// 端点地址掩码
#define USB_ENDP_ADDR_MASK  0x7F
#define USB_ENDP_SYNC_MASK  0x80

#endif /* __USB_HOST_COMMON_H__ */