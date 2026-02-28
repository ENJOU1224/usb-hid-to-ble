/**
 * @file error_recovery.c
 * @brief 错误恢复机制实现
 *
 * ## 架构说明
 *
 * ### 1. 模块定位
 * 本模块属于系统的错误处理层，负责处理各种系统错误并自动恢复。
 * 主要职责：
 * - USB设备断开重连
 * - BLE连接自动重连
 * - 看门狗超时安全恢复
 * - 错误统计和日志记录
 * - 系统状态管理
 *
 * ### 2. 依赖关系
 * - 上层调用: usb_bridge.c（主桥接逻辑）
 * - 下层依赖:
 *   - usb_host_common.c（USB主机通用函数）
 *   - user_config.c（配置管理）
 *   - debug.c（调试日志）
 *   - CH58xBLE_LIB.h（BLE协议栈）
 * - 数据依赖: ErrorStats_t、ReconnectState_t结构体定义
 *
 * ### 3. 关键数据结构
 * - ErrorStats_t: 错误统计结构体
 *   - usb_connect_count: USB连接次数
 *   - usb_disconnect_count: USB断开次数
 *   - usb_enum_fail_count: USB枚举失败次数
 *   - usb_comm_fail_count: USB通信失败次数
 *   - ble_connect_count: BLE连接次数
 *   - ble_disconnect_count: BLE断开次数
 *   - ble_auth_fail_count: BLE认证失败次数
 *   - ble_comm_fail_count: BLE通信失败次数
 *   - watchdog_timeout_count: 看门狗超时次数
 *   - reset_count: 系统复位次数
 *   - usb_reconnect_retry: USB重连重试次数
 *   - ble_reconnect_retry: BLE重连重试次数
 *
 * - ReconnectState_t: 重连状态结构体
 *   - usb_reconnect_enabled: USB重连使能标志
 *   - usb_reconnect_state: USB重连状态机状态
 *   - usb_reconnect_delay: USB重连延迟计数器
 *   - usb_reconnect_max_retry: USB最大重试次数
 *   - usb_reconnect_retry: USB当前重试次数
 *   - ble_reconnect_enabled: BLE重连使能标志
 *   - ble_reconnect_state: BLE重连状态机状态
 *   - ble_reconnect_delay: BLE重连延迟计数器
 *   - ble_reconnect_max_retry: BLE最大重试次数
 *   - ble_reconnect_retry: BLE当前重试次数
 *   - watchdog_enabled: 看门狗使能标志
 *   - watchdog_timeout: 看门狗超时计数器
 *   - watchdog_safe_mode: 看门狗安全模式标志
 *
 * ### 4. 核心算法
 * - 错误检测: 通过状态寄存器和回调函数检测错误
 * - 错误分类: 将错误分为USB、BLE、系统三类
 * - 错误恢复: 根据错误类型执行相应的恢复策略
 * - 状态机管理: 使用状态机管理重连过程
 * - 统计记录: 记录所有错误事件和恢复尝试
 *
 * ## 使用指南
 *
 * ### 初始化流程
 * 1. 调用ErrorRecovery_Init()初始化错误恢复系统
 * 2. 系统会初始化错误统计、重连状态和看门狗
 * 3. 设置系统状态为正常（0）
 *
 * ### 错误处理流程
 * 1. 检测到错误时，调用相应的错误记录函数
 * 2. 错误恢复系统自动启动重连或恢复操作
 * 3. 记录错误统计信息
 * 4. 定期打印错误统计报告
 *
 * ### 常用函数
 * - ErrorRecovery_Init(): 初始化错误恢复系统
 * - ErrorRecovery_Poll(): 错误恢复轮询（在主循环中调用）
 * - USBReconnect_Start(): 启动USB重连
 * - BLEReconnect_Start(): 启动BLE重连
 * - Watchdog_SafeRecovery(): 执行看门狗安全恢复
 * - ErrorStats_Print(): 打印错误统计
 *
 * ## 调试技巧
 * - 使用LOG_SYS()查看错误恢复日志
 * - 定期调用ErrorStats_Print()查看错误统计
 * - 监控重连状态和重试次数
 * - 检查看门狗超时情况
 *
 * @author DIY User & AI Assistant
 * @version V1.0
 */

#include "usb_host_common.h"
#include "user_config.h"
#include "debug.h"
#include "CH58xBLE_LIB.h"
#include "CH58x_common.h"

// ===================================================================
// ? 错误统计结构体
// ===================================================================

typedef struct {
    // USB错误统计
    uint32_t usb_connect_count;      // USB连接次数
    uint32_t usb_disconnect_count;   // USB断开次数
    uint32_t usb_enum_fail_count;    // USB枚举失败次数
    uint32_t usb_comm_fail_count;    // USB通信失败次数

    // BLE错误统计
    uint32_t ble_connect_count;      // BLE连接次数
    uint32_t ble_disconnect_count;   // BLE断开次数
    uint32_t ble_auth_fail_count;    // BLE认证失败次数
    uint32_t ble_comm_fail_count;    // BLE通信失败次数

    // 系统错误统计
    uint32_t watchdog_timeout_count; // 看门狗超时次数
    uint32_t reset_count;            // 系统复位次数

    // 重试统计
    uint32_t usb_reconnect_retry;    // USB重连重试次数
    uint32_t ble_reconnect_retry;    // BLE重连重试次数
} ErrorStats_t;

// ===================================================================
// ? 重连状态结构体
// ===================================================================

typedef struct {
    // USB重连状态
    uint8_t  usb_reconnect_enabled;  // USB重连使能
    uint8_t  usb_reconnect_state;    // USB重连状态 (0=空闲, 1=等待重连, 2=重连中)
    uint16_t usb_reconnect_delay;    // USB重连延迟计数器
    uint8_t  usb_reconnect_max_retry;// USB最大重试次数
    uint8_t  usb_reconnect_retry;    // USB当前重试次数

    // BLE重连状态
    uint8_t  ble_reconnect_enabled;  // BLE重连使能
    uint8_t  ble_reconnect_state;    // BLE重连状态 (0=空闲, 1=等待重连, 2=重连中)
    uint16_t ble_reconnect_delay;    // BLE重连延迟计数器
    uint8_t  ble_reconnect_max_retry;// BLE最大重试次数
    uint8_t  ble_reconnect_retry;    // BLE当前重试次数

    // 看门狗状态
    uint8_t  watchdog_enabled;       // 看门狗使能
    uint16_t watchdog_timeout;       // 看门狗超时计数器
    uint8_t  watchdog_safe_mode;     // 看门狗安全模式标志
} ReconnectState_t;

// ===================================================================
// ? 全局变量
// ===================================================================

static ErrorStats_t g_error_stats = {0};
static ReconnectState_t g_reconnect_state = {0};
static uint8_t g_system_state = 0;  // 系统状态 (0=正常, 1=错误, 2=恢复中)

// ===================================================================
// ? 外部函数声明
// ===================================================================

extern void USB_Bridge_Init(void);
extern void USB_Bridge_Poll(void);
extern void HidEmu_DisconnectBLE(void);
extern uint8_t HidEmu_ConnectBLE(void);
extern void mDelaymS(uint16_t delay);

// ===================================================================
// ? 错误统计函数
// ===================================================================

/**
 * @brief  初始化错误统计
 *
 * @details
 * #### 功能详细说明
 * 此函数初始化错误统计系统，将所有错误计数器清零。
 * 错误统计用于记录系统运行过程中的各种错误事件，便于分析和调试。
 *
 * #### 算法原理
 * 1. **内存清零**：使用memset将g_error_stats结构体全部清零
 * 2. **日志记录**：输出初始化信息
 * 3. **状态更新**：所有错误计数器归零
 *
 * #### 调用示例
 * 系统启动时调用，初始化错误统计系统。
 *
 * #### 使用场景
 * - 系统启动时调用
 * - 系统复位后调用
 * - 需要重置错误统计时调用
 *
 * #### 错误处理
 * - 本函数不会失败，总是成功执行
 * - 如果内存不足，memset会自动处理
 *
 * #### 边界情况
 * - 结构体为空：正常处理，所有字段清零
 * - 多次调用：每次调用都会重置统计信息
 *
 * #### 注意事项
 * - 此函数应该在系统启动时调用一次
 * - 调用此函数后，所有历史错误统计信息都会丢失
 * - 错误统计信息在系统运行过程中持续更新
 */
void ErrorStats_Init(void)
{
    memset(&g_error_stats, 0, sizeof(ErrorStats_t));
    LOG_SYS("Error statistics initialized\n");
}

/**
 * @brief  记录USB连接事件
 *
 * @details
 * #### 功能详细说明
 * 此函数记录USB设备连接成功的事件。每次USB设备成功连接时调用此函数，
 * 会增加USB连接计数器，并输出连接日志。
 *
 * #### 算法原理
 * 1. **计数器增加**：将usb_connect_count计数器加1
 * 2. **日志记录**：输出USB连接信息，包括总连接次数
 *
 * #### 调用示例
 * USB设备成功连接后调用此函数。
 *
 * #### 使用场景
 * - USB设备枚举成功后调用
 * - USB设备重新连接成功后调用
 * - 系统检测到USB设备连接时调用
 *
 * #### 错误处理
 * - 本函数不会失败，总是成功执行
 * - 计数器溢出：使用uint32_t，最大值为4294967295
 *
 * #### 边界情况
 * - 多次连接：每次连接都会增加计数器
 * - 系统复位：计数器会重置
 *
 * #### 注意事项
 * - 此函数只记录内存中的计数，不直接操作USB硬件
 * - 计数器在系统运行过程中持续增加
 * - 调用ErrorStats_Init()会重置计数器
 */
void ErrorStats_USBConnect(void)
{
    g_error_stats.usb_connect_count++;
    LOG_SYS("USB connected (total: %u)\n", g_error_stats.usb_connect_count);
}

/**
 * @brief  记录USB断开事件
 */
void ErrorStats_USBDisconnect(void)
{
    g_error_stats.usb_disconnect_count++;
    LOG_SYS("USB disconnected (total: %u)\n", g_error_stats.usb_disconnect_count);
}

/**
 * @brief  记录USB枚举失败
 */
void ErrorStats_USBEnumFail(void)
{
    g_error_stats.usb_enum_fail_count++;
    LOG_SYS("USB enumeration failed (total: %u)\n", g_error_stats.usb_enum_fail_count);
}

/**
 * @brief  记录USB通信失败
 */
void ErrorStats_USBCommFail(void)
{
    g_error_stats.usb_comm_fail_count++;
    LOG_SYS("USB communication failed (total: %u)\n", g_error_stats.usb_comm_fail_count);
}

/**
 * @brief  记录BLE连接事件
 */
void ErrorStats_BLEConnect(void)
{
    g_error_stats.ble_connect_count++;
    LOG_SYS("BLE connected (total: %u)\n", g_error_stats.ble_connect_count);
}

/**
 * @brief  记录BLE断开事件
 */
void ErrorStats_BLEDisconnect(void)
{
    g_error_stats.ble_disconnect_count++;
    LOG_SYS("BLE disconnected (total: %u)\n", g_error_stats.ble_disconnect_count);
}

/**
 * @brief  记录BLE认证失败
 */
void ErrorStats_BLEAuthFail(void)
{
    g_error_stats.ble_auth_fail_count++;
    LOG_SYS("BLE authentication failed (total: %u)\n", g_error_stats.ble_auth_fail_count);
}

/**
 * @brief  记录BLE通信失败
 */
void ErrorStats_BLECommFail(void)
{
    g_error_stats.ble_comm_fail_count++;
    LOG_SYS("BLE communication failed (total: %u)\n", g_error_stats.ble_comm_fail_count);
}

/**
 * @brief  记录看门狗超时
 */
void ErrorStats_WatchdogTimeout(void)
{
    g_error_stats.watchdog_timeout_count++;
    LOG_SYS("Watchdog timeout (total: %u)\n", g_error_stats.watchdog_timeout_count);
}

/**
 * @brief  记录系统复位
 */
void ErrorStats_SystemReset(void)
{
    g_error_stats.reset_count++;
    LOG_SYS("System reset (total: %u)\n", g_error_stats.reset_count);
}

/**
 * @brief  打印错误统计
 */
void ErrorStats_Print(void)
{
    LOG_SYS("=== Error Statistics ===\n");
    LOG_SYS("USB: Connect=%u, Disconnect=%u, EnumFail=%u, CommFail=%u\n",
            g_error_stats.usb_connect_count,
            g_error_stats.usb_disconnect_count,
            g_error_stats.usb_enum_fail_count,
            g_error_stats.usb_comm_fail_count);
    LOG_SYS("BLE: Connect=%u, Disconnect=%u, AuthFail=%u, CommFail=%u\n",
            g_error_stats.ble_connect_count,
            g_error_stats.ble_disconnect_count,
            g_error_stats.ble_auth_fail_count,
            g_error_stats.ble_comm_fail_count);
    LOG_SYS("System: Watchdog=%u, Reset=%u\n",
            g_error_stats.watchdog_timeout_count,
            g_error_stats.reset_count);
    LOG_SYS("Retry: USB=%u, BLE=%u\n",
            g_error_stats.usb_reconnect_retry,
            g_error_stats.ble_reconnect_retry);
}

// ===================================================================
// ? USB重连函数
// ===================================================================

/**
 * @brief  初始化USB重连状态
 *
 * @details
 * #### 功能详细说明
 * 此函数初始化USB重连状态机，设置重连参数和初始状态。
 * USB重连机制用于在USB设备断开后自动重新连接。
 *
 * #### 算法原理
 * 1. **使能重连**：将usb_reconnect_enabled设置为1（使能）
 * 2. **设置初始状态**：将usb_reconnect_state设置为0（空闲状态）
 * 3. **清零延迟计数器**：将usb_reconnect_delay设置为0
 * 4. **设置最大重试次数**：将usb_reconnect_max_retry设置为3
 * 5. **清零当前重试次数**：将usb_reconnect_retry设置为0
 * 6. **日志记录**：输出初始化信息
 *
 * #### 调用示例
 * 系统启动时调用，初始化USB重连机制。
 *
 * #### 使用场景
 * - 系统启动时调用
 * - 系统复位后调用
 * - 需要重新配置USB重连参数时调用
 *
 * #### 错误处理
 * - 本函数不会失败，总是成功执行
 * - 参数设置错误：不会影响函数执行
 *
 * #### 边界情况
 * - 多次调用：每次调用都会重置重连状态
 * - 参数冲突：使用默认值覆盖
 *
 * #### 注意事项
 * - 此函数应该在系统启动时调用一次
 * - 调用此函数后，USB重连状态会被重置
 * - 重连参数可以根据实际需求调整
 * - 最大重试次数不宜设置过大，避免无限重试
 */
void USBReconnect_Init(void)
{
    g_reconnect_state.usb_reconnect_enabled = 1;  // 默认使能重连
    g_reconnect_state.usb_reconnect_state = 0;    // 空闲状态
    g_reconnect_state.usb_reconnect_delay = 0;
    g_reconnect_state.usb_reconnect_max_retry = 3; // 最大重试3次
    g_reconnect_state.usb_reconnect_retry = 0;

    LOG_SYS("USB reconnect initialized\n");
}

/**
 * @brief  检测USB设备断开
 *
 * @return 1=断开, 0=正常
 *
 * @details
 * #### 功能详细说明
 * 此函数检测USB设备是否已经断开连接。通过检查USB设备状态寄存器，
 * 判断设备是否处于断开状态。
 *
 * #### 算法原理
 * 1. **状态检查**：读取USB设备状态寄存器ThisUsb2Dev.DeviceStatus
 * 2. **状态比较**：检查状态是否为ROOT_DEV_DISCONNECTED
 * 3. **返回结果**：如果断开返回1，否则返回0
 *
 * #### 调用示例
 * 定期调用此函数检测USB设备连接状态。
 *
 * #### 使用场景
 * - USB设备状态监控时调用
 * - USB重连机制中调用
 * - 系统状态检查时调用
 *
 * #### 错误处理
 * - 状态寄存器读取失败：返回0（正常）
 * - 硬件故障：返回0（正常）
 *
 * #### 边界情况
 * - 设备未初始化：返回0（正常）
 * - 设备正在连接：返回0（正常）
 * - 设备已断开：返回1（断开）
 *
 * #### 注意事项
 * - 此函数依赖具体的USB硬件状态寄存器
 * - 需要根据实际硬件平台调整实现
 * - 状态检查可能受USB协议栈影响
 * - 建议在主循环中定期调用
 */
uint8_t USBReconnect_CheckDisconnect(void)
{
    // 检查USB设备状态
    // 这里需要根据具体的USB硬件状态寄存器来判断
    // 简化实现：检查设备状态是否为断开状态
    if (ThisUsb2Dev.DeviceStatus == ROOT_DEV_DISCONNECTED) {
        return 1;
    }
    return 0;
}

/**
 * @brief  启动USB重连
 *
 * @details
 * #### 功能详细说明
 * 此函数启动USB重连过程。当检测到USB设备断开时，调用此函数开始重连流程。
 * 重连过程包括等待延迟和重试机制。
 *
 * #### 算法原理
 * 1. **检查使能**：检查USB重连是否使能
 * 2. **检查状态**：检查当前是否处于空闲状态
 * 3. **设置状态**：将重连状态设置为等待重连（1）
 * 4. **设置延迟**：设置1秒延迟后开始重连
 * 5. **清零重试次数**：将当前重试次数清零
 * 6. **日志记录**：输出重连开始信息
 * 7. **错误统计**：记录USB断开事件
 *
 * #### 调用示例
 * 检测到USB设备断开后调用此函数。
 *
 * #### 使用场景
 * - USB设备断开时调用
 * - USB重连机制启动时调用
 * - 系统错误恢复时调用
 *
 * #### 错误处理
 * - 重连未使能：函数直接返回，不执行操作
 * - 状态非空闲：函数直接返回，避免重复启动
 *
 * #### 边界情况
 * - 多次调用：只有在空闲状态下才会启动重连
 * - 重连已启动：不会重复启动
 *
 * #### 注意事项
 * - 此函数只设置重连状态，不立即执行重连
 * - 实际重连操作在USBReconnect_Execute()中执行
 * - 重连延迟时间可以根据实际需求调整
 * - 调用此函数会记录USB断开事件
 */
void USBReconnect_Start(void)
{
    if (!g_reconnect_state.usb_reconnect_enabled) {
        return;
    }

    if (g_reconnect_state.usb_reconnect_state == 0) {
        g_reconnect_state.usb_reconnect_state = 1; // 等待重连
        g_reconnect_state.usb_reconnect_delay = 1000; // 1秒后开始重连
        g_reconnect_state.usb_reconnect_retry = 0;

        LOG_SYS("USB reconnect started\n");
        ErrorStats_USBDisconnect();
    }
}

/**
 * @brief  执行USB重连
 *
 * @return 1=重连成功, 0=重连失败或进行中
 *
 * @details
 * #### 功能详细说明
 * 此函数执行USB重连操作。当重连状态机处于重连中状态时，执行实际的重连逻辑。
 * 包括重试次数检查、USB重新初始化、设备连接检查等。
 *
 * #### 算法原理
 * 1. **状态检查**：检查当前是否处于重连中状态（2）
 * 2. **重试次数检查**：检查是否超过最大重试次数
 * 3. **执行重连**：重新初始化USB桥接器
 * 4. **等待连接**：延迟500ms等待设备连接
 * 5. **连接检查**：检查设备状态是否成功
 * 6. **结果处理**：根据连接结果更新状态和计数器
 *
 * #### 调用示例
 * 在USB重连状态机中调用，执行实际的重连操作。
 *
 * #### 使用场景
 * - USB重连状态机中调用
 * - USB设备断开后重连时调用
 * - 系统错误恢复时调用
 *
 * #### 错误处理
 * - 状态非重连中：直接返回0
 * - 超过最大重试次数：停止重连并返回0
 * - 重连失败：增加重试次数，继续等待重连
 *
 * #### 边界情况
 * - 首次重连：重试次数为1
 * - 最后一次重连：达到最大重试次数后停止
 * - 重连成功：回到空闲状态
 *
 * #### 注意事项
 * - 此函数会修改重连状态和重试次数
 * - 重连失败后会设置延迟时间
 * - 重连成功会记录连接事件
 * - 重连失败会记录重试次数
 */
uint8_t USBReconnect_Execute(void)
{
    uint8_t result = 0;

    // 检查是否需要重连
    if (g_reconnect_state.usb_reconnect_state != 2) {
        return 0;
    }

    // 检查重试次数
    if (g_reconnect_state.usb_reconnect_retry >= g_reconnect_state.usb_reconnect_max_retry) {
        LOG_SYS("USB reconnect failed after %u retries\n", g_reconnect_state.usb_reconnect_retry);
        g_reconnect_state.usb_reconnect_state = 0; // 停止重连
        return 0;
    }

    // 执行重连
    LOG_SYS("USB reconnect attempt %u/%u\n",
            g_reconnect_state.usb_reconnect_retry + 1,
            g_reconnect_state.usb_reconnect_max_retry);

    // 重新初始化USB
    USB_Bridge_Init();

    // 等待设备连接
    mDelaymS(500);

    // 检查设备是否连接成功
    if (ThisUsb2Dev.DeviceStatus >= ROOT_DEV_SUCCESS) {
        LOG_SYS("USB reconnect successful\n");
        ErrorStats_USBConnect();
        result = 1;
        g_reconnect_state.usb_reconnect_state = 0; // 重连成功，回到空闲状态
    } else {
        LOG_SYS("USB reconnect failed\n");
        g_reconnect_state.usb_reconnect_retry++;
        g_error_stats.usb_reconnect_retry++;
        g_reconnect_state.usb_reconnect_state = 1; // 继续等待重连
        g_reconnect_state.usb_reconnect_delay = 2000; // 2秒后再次尝试
    }

    return result;
}

/**
 * @brief  USB重连轮询函数
 *
 * @param  tick_ms  系统滴答计数（毫秒）
 *
 * @details
 * #### 功能详细说明
 * 此函数是USB重连状态机的轮询函数，需要在主循环中定期调用。
 * 它处理USB设备断开检测、重连状态转换和重连执行。
 *
 * #### 算法原理
 * 1. **断开检测**：检查USB设备是否断开
 * 2. **启动重连**：如果断开，启动重连过程
 * 3. **状态机处理**：根据当前状态执行相应操作
 *    - 状态1（等待重连）：减少延迟计数器，延迟结束后进入状态2
 *    - 状态2（重连中）：执行重连操作
 *    - 状态0（空闲）：不执行操作
 *
 * #### 调用示例
 * 在主循环中定期调用，例如每1ms调用一次。
 *
 * #### 使用场景
 * - 主循环中定期调用
 * - USB设备状态监控
 * - USB重连状态机管理
 *
 * #### 错误处理
 * - 无错误处理，函数总是成功执行
 * - 状态机错误：默认情况不执行操作
 *
 * #### 边界情况
 * - 延迟时间小于tick_ms：立即进入重连状态
 * - 延迟时间大于tick_ms：继续等待
 * - 重连失败：状态机继续运行
 *
 * #### 注意事项
 * - 必须在主循环中定期调用
 * - tick_ms应该反映实际的时间间隔
 * - 函数会修改重连状态和延迟计数器
 * - 建议调用间隔不超过10ms
 */
void USBReconnect_Poll(uint16_t tick_ms)
{
    // 检查USB设备状态
    if (USBReconnect_CheckDisconnect()) {
        USBReconnect_Start();
    }

    // 处理重连状态机
    switch (g_reconnect_state.usb_reconnect_state) {
        case 1: // 等待重连
            if (g_reconnect_state.usb_reconnect_delay > tick_ms) {
                g_reconnect_state.usb_reconnect_delay -= tick_ms;
            } else {
                g_reconnect_state.usb_reconnect_state = 2; // 开始重连
            }
            break;

        case 2: // 重连中
            USBReconnect_Execute();
            break;

        default: // 空闲状态
            break;
    }
}

/**
 * @brief  设置USB重连使能
 * @param  enable  1=使能, 0=禁用
 */
void USBReconnect_SetEnable(uint8_t enable)
{
    g_reconnect_state.usb_reconnect_enabled = enable;
    LOG_SYS("USB reconnect %s\n", enable ? "enabled" : "disabled");
}

// ===================================================================
// ? BLE重连函数
// ===================================================================

/**
 * @brief  初始化BLE重连状态
 *
 * @details
 * #### 功能详细说明
 * 此函数初始化BLE重连状态机，设置重连参数和初始状态。
 * BLE重连机制用于在BLE连接断开后自动重新连接。
 *
 * #### 算法原理
 * 1. **使能重连**：将ble_reconnect_enabled设置为1（使能）
 * 2. **设置初始状态**：将ble_reconnect_state设置为0（空闲状态）
 * 3. **清零延迟计数器**：将ble_reconnect_delay设置为0
 * 4. **设置最大重试次数**：将ble_reconnect_max_retry设置为5
 * 5. **清零当前重试次数**：将ble_reconnect_retry设置为0
 * 6. **日志记录**：输出初始化信息
 *
 * #### 调用示例
 * 系统启动时调用，初始化BLE重连机制。
 *
 * #### 使用场景
 * - 系统启动时调用
 * - 系统复位后调用
 * - 需要重新配置BLE重连参数时调用
 *
 * #### 错误处理
 * - 本函数不会失败，总是成功执行
 * - 参数设置错误：不会影响函数执行
 *
 * #### 边界情况
 * - 多次调用：每次调用都会重置重连状态
 * - 参数冲突：使用默认值覆盖
 *
 * #### 注意事项
 * - 此函数应该在系统启动时调用一次
 * - 调用此函数后，BLE重连状态会被重置
 * - 重连参数可以根据实际需求调整
 * - 最大重试次数不宜设置过大，避免无限重试
 */
void BLEReconnect_Init(void)
{
    g_reconnect_state.ble_reconnect_enabled = 1;  // 默认使能重连
    g_reconnect_state.ble_reconnect_state = 0;    // 空闲状态
    g_reconnect_state.ble_reconnect_delay = 0;
    g_reconnect_state.ble_reconnect_max_retry = 5; // 最大重试5次
    g_reconnect_state.ble_reconnect_retry = 0;

    LOG_SYS("BLE reconnect initialized\n");
}

/**
 * @brief  检测BLE连接断开
 * @return 1=断开, 0=正常
 */
uint8_t BLEReconnect_CheckDisconnect(void)
{
    // 检查BLE连接状态
    // 这里需要根据具体的BLE状态函数来判断
    // 简化实现：检查连接句柄是否有效
    // 实际实现需要根据具体的BLE库函数

    return 0; // 简化实现
}

/**
 * @brief  启动BLE重连
 */
void BLEReconnect_Start(void)
{
    if (!g_reconnect_state.ble_reconnect_enabled) {
        return;
    }

    if (g_reconnect_state.ble_reconnect_state == 0) {
        g_reconnect_state.ble_reconnect_state = 1; // 等待重连
        g_reconnect_state.ble_reconnect_delay = 1000; // 1秒后开始重连
        g_reconnect_state.ble_reconnect_retry = 0;

        LOG_SYS("BLE reconnect started\n");
        ErrorStats_BLEDisconnect();
    }
}

/**
 * @brief  执行BLE重连
 * @return 1=重连成功, 0=重连失败或进行中
 */
uint8_t BLEReconnect_Execute(void)
{
    uint8_t result = 0;

    // 检查是否需要重连
    if (g_reconnect_state.ble_reconnect_state != 2) {
        return 0;
    }

    // 检查重试次数
    if (g_reconnect_state.ble_reconnect_retry >= g_reconnect_state.ble_reconnect_max_retry) {
        LOG_SYS("BLE reconnect failed after %u retries\n", g_reconnect_state.ble_reconnect_retry);
        g_reconnect_state.ble_reconnect_state = 0; // 停止重连
        return 0;
    }

    // 执行重连
    LOG_SYS("BLE reconnect attempt %u/%u\n",
            g_reconnect_state.ble_reconnect_retry + 1,
            g_reconnect_state.ble_reconnect_max_retry);

    // 断开当前连接
    HidEmu_DisconnectBLE();

    // 等待一段时间
    mDelaymS(100);

    // 重新连接
    if (HidEmu_ConnectBLE() == SUCCESS) {
        LOG_SYS("BLE reconnect successful\n");
        ErrorStats_BLEConnect();
        result = 1;
        g_reconnect_state.ble_reconnect_state = 0; // 重连成功，回到空闲状态
    } else {
        LOG_SYS("BLE reconnect failed\n");
        g_reconnect_state.ble_reconnect_retry++;
        g_error_stats.ble_reconnect_retry++;
        g_reconnect_state.ble_reconnect_state = 1; // 继续等待重连
        g_reconnect_state.ble_reconnect_delay = 3000; // 3秒后再次尝试
    }

    return result;
}

/**
 * @brief  BLE重连轮询函数
 * @param  tick_ms  系统滴答计数（毫秒）
 */
void BLEReconnect_Poll(uint16_t tick_ms)
{
    // 检查BLE连接状态
    if (BLEReconnect_CheckDisconnect()) {
        BLEReconnect_Start();
    }

    // 处理重连状态机
    switch (g_reconnect_state.ble_reconnect_state) {
        case 1: // 等待重连
            if (g_reconnect_state.ble_reconnect_delay > tick_ms) {
                g_reconnect_state.ble_reconnect_delay -= tick_ms;
            } else {
                g_reconnect_state.ble_reconnect_state = 2; // 开始重连
            }
            break;

        case 2: // 重连中
            BLEReconnect_Execute();
            break;

        default: // 空闲状态
            break;
    }
}

/**
 * @brief  设置BLE重连使能
 * @param  enable  1=使能, 0=禁用
 */
void BLEReconnect_SetEnable(uint8_t enable)
{
    g_reconnect_state.ble_reconnect_enabled = enable;
    LOG_SYS("BLE reconnect %s\n", enable ? "enabled" : "disabled");
}

// ===================================================================
// ? 看门狗安全恢复函数
// ===================================================================

/**
 * @brief  初始化看门狗
 *
 * @details
 * #### 功能详细说明
 * 此函数初始化看门狗安全恢复机制。看门狗用于检测系统是否卡死，
 * 如果系统长时间无响应，会触发安全恢复操作。
 *
 * #### 算法原理
 * 1. **使能看门狗**：将watchdog_enabled设置为1（使能）
 * 2. **清零超时计数器**：将watchdog_timeout设置为0
 * 3. **清除安全模式**：将watchdog_safe_mode设置为0
 * 4. **硬件配置**：配置看门狗硬件（需要根据具体硬件实现）
 * 5. **日志记录**：输出初始化信息
 *
 * #### 调用示例
 * 系统启动时调用，初始化看门狗机制。
 *
 * #### 使用场景
 * - 系统启动时调用
 * - 系统复位后调用
 * - 需要重新配置看门狗参数时调用
 *
 * #### 错误处理
 * - 本函数不会失败，总是成功执行
 * - 硬件配置失败：不影响函数执行
 *
 * #### 边界情况
 * - 多次调用：每次调用都会重置看门狗状态
 * - 硬件不支持：函数仍然执行，但看门狗功能可能无效
 *
 * #### 注意事项
 * - 此函数应该在系统启动时调用一次
 * - 看门狗硬件配置需要根据具体硬件平台实现
 * - 看门狗超时时间需要根据系统复杂度设置
 * - 看门狗会定期调用Watchdog_Feed()喂狗
 */
void Watchdog_Init(void)
{
    g_reconnect_state.watchdog_enabled = 1;
    g_reconnect_state.watchdog_timeout = 0;
    g_reconnect_state.watchdog_safe_mode = 0;

    // 配置看门狗超时时间（根据实际硬件配置）
    // 这里需要根据具体的CH583看门狗配置函数来实现

    LOG_SYS("Watchdog initialized\n");
}

/**
 * @brief  喂狗函数
 */
void Watchdog_Feed(void)
{
    if (g_reconnect_state.watchdog_enabled) {
        g_reconnect_state.watchdog_timeout = 0;
        // 实际喂狗操作需要根据具体的硬件看门狗函数
        // 例如：WDT_Feed();
    }
}

/**
 * @brief  检查看门狗超时
 * @return 1=超时, 0=正常
 */
uint8_t Watchdog_CheckTimeout(void)
{
    if (!g_reconnect_state.watchdog_enabled) {
        return 0;
    }

    // 检查看门狗状态寄存器
    // 这里需要根据具体的硬件看门狗状态函数来实现

    return 0; // 简化实现
}

/**
 * @brief  执行看门狗安全恢复
 */
void Watchdog_SafeRecovery(void)
{
    LOG_SYS("Watchdog timeout detected, performing safe recovery\n");
    ErrorStats_WatchdogTimeout();

    // 设置安全模式标志
    g_reconnect_state.watchdog_safe_mode = 1;

    // 1. 停止所有操作
    // 2. 重新初始化系统
    // 3. 恢复配置
    // 4. 重新连接设备

    // 重新初始化USB
    USB_Bridge_Init();

    // 重新初始化BLE
    // 这里需要根据具体的BLE初始化函数来实现

    // 清除安全模式标志
    g_reconnect_state.watchdog_safe_mode = 0;

    LOG_SYS("Safe recovery completed\n");
}

/**
 * @brief  看门狗轮询函数
 * @param  tick_ms  系统滴答计数（毫秒）
 */
void Watchdog_Poll(uint16_t tick_ms)
{
    if (!g_reconnect_state.watchdog_enabled) {
        return;
    }

    // 更新超时计数器
    g_reconnect_state.watchdog_timeout += tick_ms;

    // 检查是否超时（假设超时时间为10秒）
    if (g_reconnect_state.watchdog_timeout > 10000) {
        Watchdog_SafeRecovery();
        g_reconnect_state.watchdog_timeout = 0;
    }

    // 正常情况下，定期喂狗
    if (g_reconnect_state.watchdog_timeout % 1000 == 0) {
        Watchdog_Feed();
    }
}

// ===================================================================
// ? 系统状态管理函数
// ===================================================================

/**
 * @brief  获取系统状态
 * @return 系统状态 (0=正常, 1=错误, 2=恢复中)
 */
uint8_t ErrorRecovery_GetSystemState(void)
{
    return g_system_state;
}

/**
 * @brief  设置系统状态
 * @param  state  系统状态 (0=正常, 1=错误, 2=恢复中)
 */
void ErrorRecovery_SetSystemState(uint8_t state)
{
    g_system_state = state;
    LOG_SYS("System state changed to: %u\n", state);
}

/**
 * @brief  初始化错误恢复系统
 *
 * @details
 * #### 功能详细说明
 * 此函数初始化整个错误恢复系统，包括错误统计、重连状态和看门狗。
 * 这是错误恢复系统的入口函数，应该在系统启动时调用。
 *
 * #### 算法原理
 * 1. **初始化错误统计**：调用ErrorStats_Init()初始化错误统计
 * 2. **初始化USB重连**：调用USBReconnect_Init()初始化USB重连
 * 3. **初始化BLE重连**：调用BLEReconnect_Init()初始化BLE重连
 * 4. **初始化看门狗**：调用Watchdog_Init()初始化看门狗
 * 5. **设置系统状态**：将系统状态设置为正常（0）
 * 6. **日志记录**：输出初始化信息
 *
 * #### 调用示例
 * 系统启动时调用，初始化整个错误恢复系统。
 *
 * #### 使用场景
 * - 系统启动时调用
 * - 系统复位后调用
 * - 需要重新初始化错误恢复系统时调用
 *
 * #### 错误处理
 * - 本函数不会失败，总是成功执行
 * - 子系统初始化失败：不影响其他子系统初始化
 *
 * #### 边界情况
 * - 多次调用：每次调用都会重置所有子系统
 * - 部分初始化失败：其他子系统仍然正常初始化
 *
 * #### 注意事项
 * - 此函数应该在系统启动时调用一次
 * - 调用此函数后，错误恢复系统准备就绪
 * - 需要定期调用ErrorRecovery_Poll()进行错误恢复轮询
 * - 系统状态初始化为正常（0）
 */
void ErrorRecovery_Init(void)
{
    // 初始化错误统计
    ErrorStats_Init();

    // 初始化重连状态
    USBReconnect_Init();
    BLEReconnect_Init();

    // 初始化看门狗
    Watchdog_Init();

    // 设置系统状态为正常
    g_system_state = 0;

    LOG_SYS("Error recovery system initialized\n");
}

/**
 * @brief  错误恢复轮询函数
 *
 * @param  tick_ms  系统滴答计数（毫秒）
 *
 * @details
 * #### 功能详细说明
 * 此函数是错误恢复系统的主轮询函数，需要在主循环中定期调用。
 * 它协调执行USB重连、BLE重连、看门狗监控和错误统计打印。
 *
 * #### 算法原理
 * 1. **USB重连轮询**：调用USBReconnect_Poll()处理USB重连状态机
 * 2. **BLE重连轮询**：调用BLEReconnect_Poll()处理BLE重连状态机
 * 3. **看门狗轮询**：调用Watchdog_Poll()处理看门狗超时检测
 * 4. **错误统计打印**：每10秒打印一次错误统计信息
 *
 * #### 调用示例
 * 在主循环中定期调用，例如每1ms调用一次。
 *
 * #### 使用场景
 * - 主循环中定期调用
 * - 系统错误恢复管理
 * - 设备状态监控
 *
 * #### 错误处理
 * - 无错误处理，函数总是成功执行
 * - 子系统错误：不影响其他子系统执行
 *
 * #### 边界情况
 * - 时间间隔不准确：可能影响重连延迟和看门狗超时
 * - 统计打印间隔：每10秒打印一次
 *
 * #### 注意事项
 * - 必须在主循环中定期调用
 * - tick_ms应该反映实际的时间间隔
 * - 建议调用间隔不超过10ms
 * - 错误统计打印会占用一定时间
 */
void ErrorRecovery_Poll(uint16_t tick_ms)
{
    // 执行重连轮询
    USBReconnect_Poll(tick_ms);
    BLEReconnect_Poll(tick_ms);

    // 执行看门狗轮询
    Watchdog_Poll(tick_ms);

    // 定期打印错误统计（每10秒）
    static uint16_t stats_counter = 0;
    stats_counter += tick_ms;
    if (stats_counter >= 10000) {
        ErrorStats_Print();
        stats_counter = 0;
    }
}