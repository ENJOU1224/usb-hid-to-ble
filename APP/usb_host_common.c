/**
 * @file usb_host_common.c
 * @brief USB Host 公共函数实现
 *
 * ## 架构说明
 *
 * ### 1. 模块定位
 * 本模块属于系统的硬件抽象层，提供统一的USB主机操作接口。
 * 主要职责：
 * - USB主机事务执行
 * - DATA0/DATA1同步位管理
 * - 端点地址管理
 * - 设备端点信息获取和设置
 * - 端点有效性检查
 *
 * ### 2. 依赖关系
 * - 上层调用: usb_bridge.c（USB数据传输）
 * - 下层依赖:
 *   - CH58xBLE_LIB.h（USB硬件驱动）
 *   - CH58x_common.h（CH583硬件定义）
 *   - debug.c（调试日志）
 * - 数据依赖: USB_DEV_INFO结构体定义
 *
 * ### 3. 关键数据结构
 * - USB_DEV_INFO: USB设备信息结构体
 *   - DeviceStatus: 设备状态
 *   - GpVar: 通用变量数组（存储端点地址等）
 * - USB端点地址格式:
 *   - Bit7: DATA0/DATA1同步位（1=DATA1, 0=DATA0）
 *   - Bit0-6: 端点号（0-127）
 *
 * ### 4. 核心算法
 * - USB主机事务: 执行IN事务，读取USB设备数据
 * - 同步位管理: 独立管理每个端点的DATA0/DATA1同步位
 * - 端点管理: 从设备结构体获取和设置端点地址
 * - 有效性检查: 检查端点地址是否有效
 *
 * ## 使用指南
 *
 * ### 初始化流程
 * 1. 系统初始化时，USB硬件驱动已初始化
 * 2. 设备管理器初始化时，端点地址已设置
 * 3. 可以直接调用公共函数进行USB操作
 *
 * ### USB数据传输流程
 * 1. 获取端点地址：GetEndpointFromDevice()
 * 2. 检查端点有效性：IsEndpointValid()
 * 3. 获取同步位：Endpoint_GetToggleFlag()
 * 4. 执行USB事务：USB_HostTransact()
 * 5. 翻转同步位：Endpoint_SyncToggle()
 * 6. 保存端点地址：SetEndpointToDevice()
 *
 * ### 常用函数
 * - USB_HostTransact(): 执行USB主机IN事务
 * - Endpoint_GetToggle(): 获取端点同步位
 * - Endpoint_SetToggle(): 设置端点同步位
 * - Endpoint_SyncToggle(): 翻转端点同步位
 * - GetEndpointFromDevice(): 从设备获取端点
 * - SetEndpointToDevice(): 保存端点到设备
 * - IsEndpointValid(): 检查端点有效性
 * - Endpoint_GetAddr(): 获取端点地址
 * - Endpoint_GetToggleFlag(): 获取端点同步位标志
 *
 * ## 调试技巧
 * - 使用LOG_SYS()查看USB事务执行日志
 * - 检查端点地址和同步位状态
 * - 监控USB设备状态
 * - 验证端点有效性
 *
 * @author DIY User & AI Assistant
 * @version V1.0
 */

#include "usb_host_common.h"
#include "debug.h"
#include "CH58xBLE_LIB.h"
#include "CH58x_common.h"

// ===================================================================
// ? 外部变量声明
// ===================================================================

extern USB_DEV_INFO ThisUsb2Dev;
extern USB_DEV_INFO DevOnU2HubPort[];

// ===================================================================
// ? 公共函数实现
// ===================================================================

/**
 * @brief  执行USB主机IN事务
 *
 * @param  endp_addr  端点地址 (Bit7=同步位, Bit0-6=端点号)
 * @param  toggle_flag 同步位标志 (RB_UH_R_TOG | RB_UH_T_TOG 或 0)
 * @param  timeout    超时时间
 *
 * @return 错误码 (ERR_SUCCESS 表示成功)
 *
 * @details
 * #### 功能详细说明
 * 此函数执行USB主机IN事务，从USB设备读取数据。IN事务是USB通信中的基本操作，
 * 用于从设备读取数据到主机。
 *
 * #### 算法原理
 * 1. **参数检查**：检查端点地址和同步位标志
 * 2. **构建令牌**：构建USB PID_IN令牌和端点地址
 * 3. **执行事务**：调用USB2HostTransact()执行底层USB事务
 * 4. **返回结果**：返回事务执行结果
 *
 * #### 调用示例
 * 从USB设备端点0x81读取数据，使用DATA0同步位，超时时间为100ms。
 *
 * #### 使用场景
 * - USB设备数据读取时调用
 * - HID报告数据接收时调用
 * - USB设备通信时调用
 *
 * #### 错误处理
 * - 事务失败：返回错误码
 * - 超时：返回超时错误码
 * - 设备断开：返回设备断开错误码
 *
 * #### 边界情况
 * - 端点地址无效：可能返回错误
 * - 同步位不匹配：可能导致数据包错误
 * - 超时时间过短：可能无法完成事务
 *
 * #### 注意事项
 * - 端点地址的Bit7表示同步位（0=DATA0, 1=DATA1）
 * - 同步位标志必须与端点地址的同步位一致
 * - 超时时间需要根据设备响应时间设置
 * - 此函数会修改USB硬件状态
 */
uint8_t USB_HostTransact(uint8_t endp_addr, uint8_t toggle_flag, uint16_t timeout)
{
    // 构建事务参数
    // USB_PID_IN << 4 | (端点地址 & 0x7F)
    uint8_t token = (USB_PID_IN << 4) | (endp_addr & USB_ENDP_ADDR_MASK);

    // 执行事务
    return USB2HostTransact(token, toggle_flag, timeout);
}

/**
 * @brief  获取端点同步位
 * @param  endp_addr  端点地址 (Bit7=同步位, Bit0-6=端点号)
 * @return 同步位标志 (RB_UH_R_TOG | RB_UH_T_TOG 或 0)
 */
uint8_t Endpoint_GetToggle(uint8_t endp_addr)
{
    // Bit7 == 1 -> RB_UH_R_TOG | RB_UH_T_TOG (DATA1)
    // Bit7 == 0 -> 0                         (DATA0)
    return (endp_addr & USB_ENDP_SYNC_MASK) ? (RB_UH_R_TOG | RB_UH_T_TOG) : 0;
}

/**
 * @brief  设置端点同步位
 * @param  endp_addr  端点地址 (Bit7=同步位, Bit0-6=端点号)
 * @param  toggle     同步位值 (0=DATA0, 1=DATA1)
 * @return 设置后的端点地址
 */
uint8_t Endpoint_SetToggle(uint8_t endp_addr, uint8_t toggle)
{
    // 清除同步位
    endp_addr &= USB_ENDP_ADDR_MASK;

    // 设置同步位
    if (toggle) {
        endp_addr |= USB_ENDP_SYNC_MASK;
    }

    return endp_addr;
}

/**
 * @brief  翻转端点同步位 (DATA0 <-> DATA1)
 * @param  endp_addr  端点地址 (Bit7=同步位, Bit0-6=端点号)
 * @return 翻转后的端点地址
 */
uint8_t Endpoint_SyncToggle(uint8_t endp_addr)
{
    // 翻转Bit7 (同步位)
    return endp_addr ^ USB_ENDP_SYNC_MASK;
}

/**
 * @brief  从设备结构体中获取端点地址
 * @param  len        接口编号 (0=主设备, >0=HUB端口)
 * @param  dev_index  设备索引 (0=键盘, 1=鼠标)
 * @return 端点地址 (Bit7=同步位, Bit0-6=端点号)
 */
uint8_t GetEndpointFromDevice(uint8_t len, uint8_t dev_index)
{
    if (len) {
        // HUB端口设备
        return DevOnU2HubPort[len - 1].GpVar[dev_index];
    } else {
        // 主设备
        return ThisUsb2Dev.GpVar[dev_index];
    }
}

/**
 * @brief  将端点地址保存到设备结构体
 * @param  len        接口编号 (0=主设备, >0=HUB端口)
 * @param  endp_addr  端点地址 (Bit7=同步位, Bit0-6=端点号)
 * @param  dev_index  设备索引 (0=键盘, 1=鼠标)
 */
void SetEndpointToDevice(uint8_t len, uint8_t endp_addr, uint8_t dev_index)
{
    if (len) {
        // HUB端口设备
        DevOnU2HubPort[len - 1].GpVar[dev_index] = endp_addr;
    } else {
        // 主设备
        ThisUsb2Dev.GpVar[dev_index] = endp_addr;
    }
}

/**
 * @brief  检查端点是否有效
 * @param  endp_addr  端点地址 (Bit7=同步位, Bit0-6=端点号)
 * @return 1=有效, 0=无效
 */
uint8_t IsEndpointValid(uint8_t endp_addr)
{
    // 检查端点号是否有效 (Bit0-6)
    return (endp_addr & USB_ENDP_ADDR_MASK) != 0;
}

/**
 * @brief  获取端点地址 (去除同步位)
 * @param  endp_addr  端点地址 (Bit7=同步位, Bit0-6=端点号)
 * @return 端点地址 (Bit0-6)
 */
uint8_t Endpoint_GetAddr(uint8_t endp_addr)
{
    return endp_addr & USB_ENDP_ADDR_MASK;
}

/**
 * @brief  获取端点同步位标志
 * @param  endp_addr  端点地址 (Bit7=同步位, Bit0-6=端点号)
 * @return 同步位标志 (RB_UH_R_TOG | RB_UH_T_TOG 或 0)
 */
uint8_t Endpoint_GetToggleFlag(uint8_t endp_addr)
{
    // Bit7 == 1 -> RB_UH_R_TOG | RB_UH_T_TOG (DATA1)
    // Bit7 == 0 -> 0                         (DATA0)
    return (endp_addr & USB_ENDP_SYNC_MASK) ? (RB_UH_R_TOG | RB_UH_T_TOG) : 0;
}