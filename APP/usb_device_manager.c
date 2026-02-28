/**
 * @file usb_device_manager.c
 * @brief USB设备管理器实现
 *
 * ## 架构说明
 *
 * ### 1. 模块定位
 * 本模块属于系统的功能模块层，负责管理多个USB设备的连接状态和数据传输。
 * 主要职责：
 * - USB设备的添加和移除
 * - 设备类型识别和分类
 * - 设备状态维护
 * - DATA0/DATA1同步位管理
 * - 设备数据缓冲区管理
 *
 * ### 2. 依赖关系
 * - 上层调用: usb_bridge.c（主桥接逻辑）
 * - 下层依赖: debug.c（调试日志）
 * - 数据依赖: UsbDevice_t结构体定义
 *
 * ### 3. 关键数据结构
 * - UsbDevice_t: USB设备信息结构体
 *   - dev_addr: 设备地址
 *   - dev_type: 设备类型（键盘、鼠标、游戏手柄）
 *   - endpoint: 端点地址
 *   - sync_toggle: DATA0/DATA1同步位
 *   - report_buffer: 报告缓冲区
 *   - is_connected: 连接状态
 *   - is_valid: 有效性标志
 *
 * ### 4. 核心算法
 * - 设备发现: 通过U2SearchTypeDevice()查找设备
 * - 设备添加: 将设备信息保存到设备数组
 * - 设备移除: 从设备数组中移除设备
 * - 同步管理: 独立管理每个设备的DATA0/DATA1同步位
 *
 * ## 使用指南
 *
 * ### 初始化流程
 * 1. 调用UsbDeviceManager_Init()初始化设备管理器
 * 2. 设备管理器会清空所有设备信息
 * 3. 准备接收新设备连接
 *
 * ### 设备管理流程
 * 1. 设备发现: 通过USB_Bridge_DiscoverDevices()发现新设备
 * 2. 设备添加: 调用UsbDeviceManager_AddDevice()添加设备
 * 3. 设备处理: 通过UsbDeviceManager_GetDevice()获取设备信息
 * 4. 设备移除: 设备断开时自动移除
 *
 * ### 常用函数
 * - UsbDeviceManager_Init(): 初始化设备管理器
 * - UsbDeviceManager_AddDevice(): 添加新设备
 * - UsbDeviceManager_RemoveDevice(): 移除设备
 * - UsbDeviceManager_GetDevice(): 获取设备信息
 * - UsbDeviceManager_FindDeviceByType(): 按类型查找设备
 *
 * ## 调试技巧
 * - 使用LOG_SYS()查看设备连接日志
 * - 检查设备数组状态
 * - 监控设备连接数量
 *
 * @author DIY User & AI Assistant
 * @version V1.0
 */

#include "usb_device_manager.h"
#include "debug.h"
#include <string.h>

// ===================================================================
// ? 全局变量
// ===================================================================

static UsbDevice_t g_usb_devices[MAX_USB_DEVICES];
static uint8_t g_active_device_count = 0;

// ===================================================================
// ? 设备管理器初始化
// ===================================================================

/**
 * @brief  初始化设备管理器
 *
 * @details
 * #### 功能详细说明
 * 此函数是设备管理器的初始化入口，负责清空所有设备信息，将设备管理器重置为初始状态。
 * 初始化完成后，设备管理器准备接收新的USB设备连接。
 *
 * #### 算法原理
 * 1. **内存清零**：使用memset将设备数组g_usb_devices全部清零
 * 2. **计数器重置**：将活动设备计数器g_active_device_count设置为0
 * 3. **状态初始化**：遍历所有设备槽位，将is_valid和is_connected标志设置为0
 * 4. **日志记录**：输出初始化信息，显示最大支持设备数量
 *
 * #### 调用示例
 * ```c
 * // 在系统启动时调用
 * UsbDeviceManager_Init();
 * // 设备管理器现在可以接收新设备连接
 * ```
 *
 * #### 使用场景
 * - 系统启动时调用
 * - 设备管理器需要重置时调用
 * - 系统复位后重新初始化时调用
 *
 * #### 错误处理
 * - 本函数不会失败，总是成功执行
 * - 如果内存不足，memset会自动处理
 *
 * #### 边界情况
 * - 设备数组为空：正常处理，所有设备都初始化为无效状态
 * - 多次调用：每次调用都会重置设备管理器状态
 *
 * #### 注意事项
 * - 此函数应该在系统启动时调用一次
 * - 调用此函数后，所有已连接的设备信息都会丢失
 * - 此函数不会影响实际的USB硬件连接状态
 */
void UsbDeviceManager_Init(void)
{
    memset(g_usb_devices, 0, sizeof(g_usb_devices));
    g_active_device_count = 0;

    // 初始化所有设备为无效状态
    for (uint8_t i = 0; i < MAX_USB_DEVICES; i++) {
        g_usb_devices[i].is_valid = 0;
        g_usb_devices[i].is_connected = 0;
    }

    LOG_SYS("USB Device Manager initialized (max %u devices)\n", MAX_USB_DEVICES);
}

// ===================================================================
// ? 设备管理函数
// ===================================================================

/**
 * @brief  添加设备到管理器
 *
 * @param  dev_addr  设备地址 - USB设备的地址（HUB端口号）
 * @param  dev_type  设备类型 - 设备类型枚举值（键盘、鼠标等）
 * @param  endpoint  端点地址 - USB端点地址（包含同步位）
 *
 * @return 设备索引 (0-3), 0xFF表示失败
 *
 * @details
 * #### 功能详细说明
 * 此函数将新发现的USB设备添加到设备管理器中。设备管理器维护一个设备数组，
 * 每个设备占用一个槽位。函数会查找第一个空闲槽位，初始化设备信息，并返回设备索引。
 *
 * #### 算法原理
 * 1. **槽位查找**：遍历设备数组，查找is_valid标志为0的空闲槽位
 * 2. **设备初始化**：在找到的槽位中设置设备信息：
 *    - dev_addr: 设备地址（HUB端口号）
 *    - dev_type: 设备类型（键盘、鼠标、游戏手柄等）
 *    - endpoint: 端点地址（包含DATA0/DATA1同步位）
 *    - sync_toggle: 同步位初始化为0（DATA0）
 *    - is_connected: 连接状态设置为1（已连接）
 *    - is_valid: 有效性标志设置为1（有效）
 * 3. **缓冲区清零**：清空设备的报告缓冲区
 * 4. **计数器更新**：活动设备计数器加1
 * 5. **日志记录**：输出设备添加信息
 * 6. **返回索引**：返回设备在数组中的索引位置
 *
 * #### 调用示例
 * 发现键盘设备后添加到管理器，设备地址为0x01，设备类型为键盘，端点地址为0x81。
 * 如果设备添加成功，返回设备索引；如果设备管理器已满，返回0xFF。
 *
 * #### 使用场景
 * - USB设备发现后调用
 * - 设备枚举完成后调用
 * - 热插拔检测到新设备时调用
 *
 * #### 错误处理
 * - 设备管理器已满：返回0xFF，表示无法添加新设备
 * - 设备地址无效：仍然添加，但后续通信可能失败
 * - 端点地址无效：仍然添加，但数据传输可能失败
 *
 * #### 边界情况
 * - 设备管理器已满：返回0xFF，需要等待设备断开释放槽位
 * - 设备地址重复：允许添加，但可能导致地址冲突
 * - 端点地址重复：允许添加，但可能导致端点冲突
 *
 * #### 注意事项
 * - 设备索引是设备在数组中的位置，不是设备地址
 * - 设备添加后，需要通过设备索引访问设备信息
 * - 设备断开时，需要调用UsbDeviceManager_RemoveDevice()释放槽位
 * - 设备类型必须是有效的枚举值
 */
uint8_t UsbDeviceManager_AddDevice(uint8_t dev_addr, uint8_t dev_type, uint8_t endpoint)
{
    // 查找空闲槽位
    for (uint8_t i = 0; i < MAX_USB_DEVICES; i++) {
        if (!g_usb_devices[i].is_valid) {
            // 找到空闲槽位，初始化设备
            g_usb_devices[i].dev_addr = dev_addr;
            g_usb_devices[i].dev_type = dev_type;
            g_usb_devices[i].endpoint = endpoint;
            g_usb_devices[i].sync_toggle = 0;  // 默认DATA0
            g_usb_devices[i].is_connected = 1;
            g_usb_devices[i].is_valid = 1;

            // 清空报告缓冲区
            memset(g_usb_devices[i].report_buffer, 0, sizeof(g_usb_devices[i].report_buffer));

            g_active_device_count++;

            LOG_SYS("Device added: index=%u, addr=%u, type=%u, endpoint=0x%02X\n",
                    i, dev_addr, dev_type, endpoint);

            return i;
        }
    }

    LOG_SYS("Device manager full (max %u devices)\n", MAX_USB_DEVICES);
    return 0xFF;  // 设备管理器已满
}

/**
 * @brief  移除设备
 *
 * @param  dev_index  设备索引 - 要移除的设备在数组中的位置（0-3）
 *
 * @details
 * #### 功能详细说明
 * 此函数从设备管理器中移除指定的USB设备。移除操作包括：
 * 1. 清空设备的所有信息
 * 2. 将设备标记为无效和未连接
 * 3. 减少活动设备计数
 * 4. 输出移除日志
 *
 * #### 算法原理
 * 1. **参数检查**：检查设备索引是否在有效范围内（0-MAX_USB_DEVICES-1）
 * 2. **有效性检查**：检查设备是否有效（is_valid标志为1）
 * 3. **日志记录**：输出设备移除信息，包括设备索引和地址
 * 4. **内存清零**：使用memset清空设备结构体的所有字段
 * 5. **状态更新**：将is_valid和is_connected标志设置为0
 * 6. **计数器更新**：活动设备计数器减1（确保不小于0）
 *
 * #### 调用示例
 * 移除设备索引为2的设备，然后检查设备是否已移除。
 *
 * #### 使用场景
 * - USB设备断开时调用
 * - 设备枚举失败时调用
 * - 系统需要清理无效设备时调用
 * - 设备管理器重置时调用
 *
 * #### 错误处理
 * - 设备索引无效：函数直接返回，不执行任何操作
 * - 设备已无效：函数直接返回，不重复移除
 * - 计数器下溢：确保计数器不会减到负数
 *
 * #### 边界情况
 * - 索引超出范围：函数安全返回，不执行操作
 * - 设备已移除：函数安全返回，不重复操作
 * - 计数器为0：不减少计数器，避免下溢
 * - 多个设备同时断开：需要分别调用此函数
 *
 * #### 注意事项
 * - 移除设备后，设备索引可能被新设备复用
 * - 移除设备不会影响实际的USB硬件连接
 * - 移除设备后，设备报告缓冲区中的数据也会被清空
 * - 此函数不会触发USB硬件层面的断开操作
 */
void UsbDeviceManager_RemoveDevice(uint8_t dev_index)
{
    if (dev_index >= MAX_USB_DEVICES) {
        return;
    }

    if (g_usb_devices[dev_index].is_valid) {
        LOG_SYS("Device removed: index=%u, addr=%u\n",
                dev_index, g_usb_devices[dev_index].dev_addr);

        // 清空设备信息
        memset(&g_usb_devices[dev_index], 0, sizeof(UsbDevice_t));
        g_usb_devices[dev_index].is_valid = 0;
        g_usb_devices[dev_index].is_connected = 0;

        if (g_active_device_count > 0) {
            g_active_device_count--;
        }
    }
}

/**
 * @brief  获取设备信息
 *
 * @param  dev_index  设备索引 - 要获取信息的设备在数组中的位置（0-3）
 *
 * @return 设备结构体指针 - 指向设备信息结构体的指针，失败时返回NULL
 *
 * @details
 * #### 功能详细说明
 * 此函数根据设备索引获取设备的详细信息。返回的指针指向设备管理器内部的设备结构体，
 * 可以直接访问设备的所有字段，但不建议直接修改结构体内容。
 *
 * #### 算法原理
 * 1. **参数检查**：检查设备索引是否在有效范围内（0-MAX_USB_DEVICES-1）
 * 2. **有效性检查**：检查设备是否有效（is_valid标志为1）
 * 3. **返回指针**：返回设备结构体的指针
 * 4. **错误处理**：如果索引无效或设备无效，返回NULL
 *
 * #### 调用示例
 * 获取设备索引为1的设备信息，如果设备有效，可以访问设备地址、类型、端点地址等信息。
 *
 * #### 使用场景
 * - 需要访问设备详细信息时调用
 * - 需要检查设备状态时调用
 * - 需要读取设备报告数据时调用
 * - 设备数据处理函数中调用
 *
 * #### 错误处理
 * - 设备索引无效：返回NULL
 * - 设备无效：返回NULL
 * - 设备未连接：仍然返回指针，但is_connected标志为0
 *
 * #### 边界情况
 * - 索引超出范围：返回NULL
 * - 设备已移除：返回NULL
 * - 设备正在添加：可能返回NULL或部分初始化的结构体
 *
 * #### 注意事项
 * - 返回的指针指向设备管理器内部数据，不要释放或修改
 * - 设备断开后，设备信息可能被清空，需要重新获取
 * - 多线程环境下需要加锁保护
 * - 不要保存返回的指针，因为设备可能被移除
 */
UsbDevice_t* UsbDeviceManager_GetDevice(uint8_t dev_index)
{
    if (dev_index >= MAX_USB_DEVICES) {
        return NULL;
    }

    if (!g_usb_devices[dev_index].is_valid) {
        return NULL;
    }

    return &g_usb_devices[dev_index];
}

/**
 * @brief  查找指定类型的设备
 *
 * @param  dev_type  设备类型 - 要查找的设备类型（键盘、鼠标、游戏手柄等）
 *
 * @return 设备索引 (0-3), 0xFF表示未找到
 *
 * @details
 * #### 功能详细说明
 * 此函数在设备管理器中查找指定类型的设备。如果找到匹配的设备，返回其索引；
 * 如果未找到或设备类型不匹配，返回0xFF。
 *
 * #### 算法原理
 * 1. **遍历设备数组**：从索引0开始遍历所有设备槽位
 * 2. **有效性检查**：检查设备是否有效（is_valid标志为1）
 * 3. **类型匹配**：检查设备类型是否与目标类型匹配
 * 4. **返回索引**：找到匹配设备后立即返回其索引
 * 5. **未找到处理**：遍历完成后未找到，返回0xFF
 *
 * #### 调用示例
 * 查找键盘设备，如果找到，获取设备信息并打印；如果未找到，输出提示信息。
 *
 * #### 使用场景
 * - 需要按类型访问设备时调用
 * - 设备发现后检查是否已存在同类设备时调用
 * - 需要发送特定类型设备的数据时调用
 * - 设备状态查询时调用
 *
 * #### 错误处理
 * - 设备类型无效：返回0xFF
 * - 未找到设备：返回0xFF
 * - 设备类型不匹配：继续查找下一个设备
 *
 * #### 边界情况
 * - 多个同类型设备：只返回第一个找到的设备索引
 * - 设备类型为未知：返回0xFF
 * - 设备管理器为空：返回0xFF
 *
 * #### 注意事项
 * - 此函数只返回第一个匹配的设备索引
 * - 如果需要处理多个同类型设备，需要多次调用或使用其他方法
 * - 设备类型必须是有效的枚举值
 * - 返回的索引可以用于后续的设备操作
 */
uint8_t UsbDeviceManager_FindDeviceByType(uint8_t dev_type)
{
    for (uint8_t i = 0; i < MAX_USB_DEVICES; i++) {
        if (g_usb_devices[i].is_valid && g_usb_devices[i].dev_type == dev_type) {
            return i;
        }
    }

    return 0xFF;  // 未找到
}

/**
 * @brief  更新设备同步位
 *
 * @param  dev_index  设备索引 - 要更新同步位的设备在数组中的位置（0-3）
 * @param  toggle  同步位 - DATA0/DATA1同步位（0=DATA0, 1=DATA1）
 *
 * @details
 * #### 功能详细说明
 * 此函数更新指定设备的DATA0/DATA1同步位。USB通信中，数据包需要交替使用DATA0和DATA1，
 * 以确保接收方正确识别数据包。此函数用于维护每个设备的同步状态。
 *
 * #### 算法原理
 * 1. **参数检查**：检查设备索引是否在有效范围内（0-MAX_USB_DEVICES-1）
 * 2. **有效性检查**：检查设备是否有效（is_valid标志为1）
 * 3. **更新同步位**：将设备的sync_toggle字段设置为指定值
 * 4. **边界处理**：如果设备无效或索引无效，函数安全返回
 *
 * #### 调用示例
 * 更新设备索引为0的设备同步位为DATA1，然后获取更新后的同步位。
 *
 * #### 使用场景
 * - USB数据传输完成后调用
 * - 设备通信错误恢复时调用
 * - 设备重新连接时调用
 * - 同步位需要手动调整时调用
 *
 * #### 错误处理
 * - 设备索引无效：函数安全返回，不执行操作
 * - 设备无效：函数安全返回，不执行操作
 * - 同步位值无效：仍然设置，但可能导致通信错误
 *
 * #### 边界情况
 * - 索引超出范围：函数安全返回
 * - 设备已移除：函数安全返回
 * - 同步位值超出范围：仍然设置，但可能导致通信错误
 *
 * #### 注意事项
 * - 同步位必须在0和1之间交替
 * - 错误的同步位会导致USB通信失败
 * - 设备断开后，同步位状态会丢失
 * - 此函数只更新内存中的同步位，不直接影响USB硬件
 */
void UsbDeviceManager_UpdateSyncToggle(uint8_t dev_index, uint8_t toggle)
{
    if (dev_index >= MAX_USB_DEVICES) {
        return;
    }

    if (g_usb_devices[dev_index].is_valid) {
        g_usb_devices[dev_index].sync_toggle = toggle;
    }
}

/**
 * @brief  获取设备同步位
 *
 * @param  dev_index  设备索引 - 要获取同步位的设备在数组中的位置（0-3）
 *
 * @return 同步位 (0=DATA0, 1=DATA1) - 设备的当前DATA0/DATA1同步位
 *
 * @details
 * #### 功能详细说明
 * 此函数获取指定设备的DATA0/DATA1同步位状态。同步位用于确保USB数据传输的正确性，
 * 数据包需要交替使用DATA0和DATA1，接收方通过同步位识别数据包。
 *
 * #### 算法原理
 * 1. **参数检查**：检查设备索引是否在有效范围内（0-MAX_USB_DEVICES-1）
 * 2. **返回同步位**：返回设备的sync_toggle字段值
 * 3. **边界处理**：如果索引无效，返回默认值0（DATA0）
 *
 * #### 调用示例
 * 获取设备索引为0的设备同步位，根据同步位决定下一个数据包的类型。
 *
 * #### 使用场景
 * - USB数据传输前调用，确定数据包类型
 * - 设备通信状态检查时调用
 * - 同步位翻转逻辑中调用
 * - 调试和故障排除时调用
 *
 * #### 错误处理
 * - 设备索引无效：返回默认值0（DATA0）
 * - 设备无效：返回默认值0（DATA0）
 * - 设备未连接：仍然返回当前同步位值
 *
 * #### 边界情况
 * - 索引超出范围：返回默认值0
 * - 设备已移除：返回默认值0
 * - 设备正在初始化：返回默认值0
 *
 * #### 注意事项
 * - 同步位必须与USB端点地址的Bit7保持一致
 * - 错误的同步位会导致USB通信失败
 * - 设备断开后，同步位状态会丢失
 * - 此函数只读取内存中的同步位，不直接查询USB硬件
 */
uint8_t UsbDeviceManager_GetSyncToggle(uint8_t dev_index)
{
    if (dev_index >= MAX_USB_DEVICES) {
        return 0;
    }

    return g_usb_devices[dev_index].sync_toggle;
}

/**
 * @brief  更新设备报告缓冲区
 *
 * @param  dev_index  设备索引 - 要更新报告的设备在数组中的位置（0-3）
 * @param  report  报告数据 - 指向HID报告数据的指针
 * @param  len  报告长度 - 报告数据的长度（最大8字节）
 *
 * @details
 * #### 功能详细说明
 * 此函数将HID报告数据复制到设备的报告缓冲区中。报告缓冲区用于存储设备的最新状态，
 * 例如键盘按键状态、鼠标移动数据等。报告数据会被复制到设备结构体的report_buffer字段。
 *
 * #### 算法原理
 * 1. **参数检查**：检查设备索引和报告长度是否在有效范围内
 * 2. **有效性检查**：检查设备是否有效（is_valid标志为1）
 * 3. **数据复制**：使用memcpy将报告数据复制到设备的报告缓冲区
 * 4. **边界处理**：如果参数无效或设备无效，函数安全返回
 *
 * #### 调用示例
 * 键盘报告数据为8字节标准HID报告，更新设备索引为0的设备报告缓冲区，然后获取报告数据。
 *
 * #### 使用场景
 * - USB数据接收后调用
 * - HID报告解析完成后调用
 * - 设备状态更新时调用
 * - 需要保存设备最新状态时调用
 *
 * #### 错误处理
 * - 设备索引无效：函数安全返回，不执行操作
 * - 报告长度超过8字节：函数安全返回，不执行操作
 * - 设备无效：函数安全返回，不执行操作
 * - 报告数据指针为NULL：可能导致内存访问错误
 *
 * #### 边界情况
 * - 报告长度为0：不复制数据，但设备标记为有效
 * - 报告长度小于8：只复制指定长度的数据
 * - 报告长度等于8：复制全部8字节数据
 * - 设备报告缓冲区已满：新数据覆盖旧数据
 *
 * #### 注意事项
 * - 报告长度不能超过8字节（MAX_REPORT_SIZE）
 * - 报告数据指针不能为空
 * - 此函数只更新内存中的报告缓冲区，不直接影响USB硬件
 * - 设备断开后，报告缓冲区中的数据会丢失
 */
void UsbDeviceManager_UpdateReport(uint8_t dev_index, uint8_t* report, uint8_t len)
{
    if (dev_index >= MAX_USB_DEVICES || len > 8) {
        return;
    }

    if (g_usb_devices[dev_index].is_valid) {
        memcpy(g_usb_devices[dev_index].report_buffer, report, len);
    }
}

/**
 * @brief  获取设备报告缓冲区
 *
 * @param  dev_index  设备索引 - 要获取报告的设备在数组中的位置（0-3）
 *
 * @return 报告缓冲区指针 - 指向设备报告缓冲区的指针，失败时返回NULL
 *
 * @details
 * #### 功能详细说明
 * 此函数获取设备的HID报告缓冲区指针。报告缓冲区存储设备的最新状态数据，
 * 例如键盘按键状态、鼠标移动数据等。返回的指针可以直接访问报告数据。
 *
 * #### 算法原理
 * 1. **参数检查**：检查设备索引是否在有效范围内（0-MAX_USB_DEVICES-1）
 * 2. **有效性检查**：检查设备是否有效（is_valid标志为1）
 * 3. **返回指针**：返回设备报告缓冲区的指针
 * 4. **错误处理**：如果索引无效或设备无效，返回NULL
 *
 * #### 调用示例
 * 获取设备索引为0的设备报告缓冲区，如果设备有效，可以访问报告数据并解析键盘报告。
 *
 * #### 使用场景
 * - 需要读取设备最新状态时调用
 * - BLE数据发送前获取报告数据时调用
 * - 设备状态监控时调用
 * - 调试和故障排除时调用
 *
 * #### 错误处理
 * - 设备索引无效：返回NULL
 * - 设备无效：返回NULL
 * - 设备未连接：仍然返回指针，但数据可能过时
 *
 * #### 边界情况
 * - 索引超出范围：返回NULL
 * - 设备已移除：返回NULL
 * - 设备正在初始化：返回NULL
 * - 报告缓冲区为空：返回指针，但数据可能为0
 *
 * #### 注意事项
 * - 返回的指针指向设备管理器内部数据，不要释放
 * - 设备断开后，报告缓冲区中的数据会丢失
 * - 多线程环境下需要加锁保护
 * - 不要保存返回的指针，因为设备可能被移除
 * - 报告数据长度固定为8字节（MAX_REPORT_SIZE）
 */
uint8_t* UsbDeviceManager_GetReport(uint8_t dev_index)
{
    if (dev_index >= MAX_USB_DEVICES) {
        return NULL;
    }

    if (!g_usb_devices[dev_index].is_valid) {
        return NULL;
    }

    return g_usb_devices[dev_index].report_buffer;
}

/**
 * @brief  检查设备是否有效
 *
 * @param  dev_index  设备索引 - 要检查的设备在数组中的位置（0-3）
 *
 * @return 1=有效, 0=无效 - 设备有效性标志
 *
 * @details
 * #### 功能详细说明
 * 此函数检查指定设备是否有效。有效性标志表示设备是否已正确初始化并准备好使用。
 * 有效设备可以进行数据传输，无效设备不能进行任何操作。
 *
 * #### 算法原理
 * 1. **参数检查**：检查设备索引是否在有效范围内（0-MAX_USB_DEVICES-1）
 * 2. **返回有效性标志**：返回设备的is_valid字段值
 * 3. **边界处理**：如果索引无效，返回0（无效）
 *
 * #### 调用示例
 * 检查设备索引为0的设备是否有效，如果有效，获取设备信息并打印；如果无效，输出提示信息。
 *
 * #### 使用场景
 * - 设备操作前检查设备状态时调用
 * - 设备轮询时过滤无效设备时调用
 * - 设备管理时检查设备存在性时调用
 * - 错误处理时判断设备状态时调用
 *
 * #### 错误处理
 * - 设备索引无效：返回0（无效）
 * - 设备未初始化：返回0（无效）
 * - 设备已移除：返回0（无效）
 *
 * #### 边界情况
 * - 索引超出范围：返回0（无效）
 * - 设备正在添加：可能返回0（无效）
 * - 设备正在移除：可能返回0（无效）
 *
 * #### 注意事项
 * - 有效性标志不表示设备是否连接，只表示设备是否已初始化
 * - 设备连接状态由is_connected字段表示
 * - 有效设备可能未连接（is_connected=0）
 * - 此函数只检查内存中的标志，不直接查询USB硬件
 */
uint8_t UsbDeviceManager_IsValid(uint8_t dev_index)
{
    if (dev_index >= MAX_USB_DEVICES) {
        return 0;
    }

    return g_usb_devices[dev_index].is_valid;
}

/**
 * @brief  获取活动设备数量
 *
 * @return 活动设备数量 - 当前已连接并有效的设备数量（0-4）
 *
 * @details
 * #### 功能详细说明
 * 此函数返回当前设备管理器中活动设备的数量。活动设备是指已初始化并标记为有效的设备。
 * 此函数用于监控设备连接状态和系统负载。
 *
 * #### 算法原理
 * 1. **返回计数器**：直接返回全局变量g_active_device_count的值
 * 2. **无参数检查**：函数简单直接，不需要参数检查
 * 3. **实时更新**：计数器在设备添加和移除时自动更新
 *
 * #### 调用示例
 * 获取当前活动设备数量，根据设备数量调整系统行为，检查是否达到最大设备数。
 *
 * #### 使用场景
 * - 系统状态监控时调用
 * - 设备连接状态检查时调用
 * - 负载均衡决策时调用
 * - 调试和故障排除时调用
 *
 * #### 错误处理
 * - 本函数不会失败，总是返回有效值
 * - 返回值范围：0-MAX_USB_DEVICES
 *
 * #### 边界情况
 * - 无设备连接：返回0
 * - 所有槽位已满：返回MAX_USB_DEVICES
 * - 设备正在添加：返回添加前的计数
 * - 设备正在移除：返回移除前的计数
 *
 * #### 注意事项
 * - 返回值是设备数量，不是设备索引
 * - 计数器在设备添加和移除时自动更新
 * - 此函数只返回内存中的计数，不直接查询USB硬件
 * - 多线程环境下需要加锁保护
 */
uint8_t UsbDeviceManager_GetActiveCount(void)
{
    return g_active_device_count;
}

/**
 * @brief  打印设备管理器状态
 *
 * @details
 * #### 功能详细说明
 * 此函数打印设备管理器的当前状态，包括活动设备数量和每个有效设备的详细信息。
 * 输出信息用于调试和系统监控，帮助开发者了解设备连接状态。
 *
 * #### 算法原理
 * 1. **打印标题**：输出设备管理器状态标题
 * 2. **打印统计信息**：输出活动设备数量和最大设备数量
 * 3. **遍历设备数组**：从索引0开始遍历所有设备槽位
 * 4. **检查有效性**：只打印有效的设备信息
 * 5. **设备类型转换**：将设备类型枚举值转换为字符串
 * 6. **打印设备详情**：输出设备索引、地址、类型、端点和同步位
 *
 * #### 调用示例
 * ```c
 * // 打印设备管理器状态
 * UsbDeviceManager_PrintStatus();
 *
 * // 输出示例：
 * // === USB Device Manager Status ===
 * // Active devices: 2/4
 * //   Device 0: addr=1, type=KEYBOARD, endpoint=0x81, sync=DATA0
 * //   Device 1: addr=2, type=MOUSE, endpoint=0x82, sync=DATA1
 * ```
 *
 * #### 使用场景
 * - 系统启动后检查设备状态时调用
 * - 设备连接或断开后调用
 * - 调试和故障排除时调用
 * - 系统监控时定期调用
 *
 * #### 错误处理
 * - 本函数不会失败，总是成功执行
 * - 如果无有效设备，只打印统计信息
 *
 * #### 边界情况
 * - 无设备连接：只打印统计信息（0/4）
 * - 所有槽位已满：打印所有4个设备信息
 * - 设备类型未知：显示"UNKNOWN"
 * - 同步位为0：显示"DATA0"
 * - 同步位为1：显示"DATA1"
 *
 * #### 注意事项
 * - 此函数使用LOG_SYS输出信息，需要确保调试系统已初始化
 * - 输出信息可能较长，建议在调试模式下使用
 * - 此函数只读取内存中的设备信息，不直接查询USB硬件
 * - 多线程环境下需要加锁保护
 */
void UsbDeviceManager_PrintStatus(void)
{
    LOG_SYS("=== USB Device Manager Status ===\n");
    LOG_SYS("Active devices: %u/%u\n", g_active_device_count, MAX_USB_DEVICES);

    for (uint8_t i = 0; i < MAX_USB_DEVICES; i++) {
        if (g_usb_devices[i].is_valid) {
            const char* type_str = "UNKNOWN";
            switch (g_usb_devices[i].dev_type) {
                case DEV_TYPE_KEYBOARD: type_str = "KEYBOARD"; break;
                case DEV_TYPE_MOUSE: type_str = "MOUSE"; break;
                case DEV_TYPE_GAMEPAD: type_str = "GAMEPAD"; break;
                case DEV_TYPE_HID_OTHER: type_str = "HID_OTHER"; break;
            }

            LOG_SYS("  Device %u: addr=%u, type=%s, endpoint=0x%02X, sync=%s\n",
                    i, g_usb_devices[i].dev_addr, type_str,
                    g_usb_devices[i].endpoint,
                    g_usb_devices[i].sync_toggle ? "DATA1" : "DATA0");
        }
    }
}
