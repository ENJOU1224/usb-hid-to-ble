/**
 * @file usb_bridge.c
 * @brief USB Host 转 Bluetooth 桥接逻辑
 *
 * ## 架构说明
 *
 * ### 1. 模块定位
 * 本模块属于系统的应用层，是整个USB HID to BLE适配器的核心桥接模块。
 * 主要职责：
 * - USB设备发现和枚举
 * - USB数据接收和处理
 * - BLE数据发送
 * - 多设备管理协调
 * - 错误处理和恢复
 *
 * ### 2. 依赖关系
 * - 上层调用: HID服务层、BLE协议栈
 * - 下层依赖:
 *   - usb_device_manager.c: 设备管理
 *   - error_recovery.c: 错误恢复
 *   - user_config.c: 配置管理
 *   - usb_host_common.c: USB主机通用函数
 *   - debug.c: 调试日志
 * - 数据依赖: UsbDevice_t结构体、UserConfig_t结构体
 *
 * ### 3. 关键数据结构
 * - UsbDevice_t: USB设备信息结构体
 * - UserConfig_t: 用户配置结构体
 * - RxBuffer/TxBuffer: USB数据缓冲区
 *
 * ### 4. 核心算法
 * - USB主机事务处理: 使用USB_HostTransact()执行IN事务
 * - DATA0/DATA1同步: 通过同步位管理USB数据传输
 * - 键盘数据解析: 支持标准6键和NKRO键盘格式转换
 * - 设备发现: 通过U2SearchTypeDevice()查找设备
 *
 * ## 使用指南
 *
 * ### 初始化流程
 * 1. 调用USB_Bridge_Init()进行系统初始化
 * 2. 配置用户参数（通过UserConfig_t结构体）
 * 3. 调用USB_Bridge_Poll()开始主循环处理
 *
 * ### 常用函数
 * - USB_Bridge_Init(): 系统初始化
 * - USB_Bridge_Poll(): 主循环处理
 * - USB_Bridge_DiscoverDevices(): 设备发现
 * - USB_Bridge_ProcessDevice(): 设备数据处理
 *
 * ## 调试技巧
 * - 使用LOG_SYS()查看系统日志
 * - 检查错误恢复机制是否正常工作
 * - 监控设备连接状态
 *
 * @author DIY User & AI Assistant
 * @version V18.0 (Refactored & Optimized)
 */

#include "CH58xBLE_LIB.H"
#include "CH58x_common.h"
#include "debug.h"
#include "usb_host_common.h"
#include "user_config.h"
#include "error_recovery.h"
#include "usb_device_manager.h"

// ===================================================================
// ? 用户配置 (User Configuration)
// ===================================================================

// NiZ 键盘配置
#define NIZ_KEY_OFFSET    4       // NiZ键盘偏移量
#define NIZ_MOUSE_ENDP    0x84    // 强制指定端点地址 (Interface 2)
// 内存优化配置
#define MAX_PACKET_SIZE   16      // USB HID最大包大小 (优化为16字节，足够支持NKRO键盘)
#define MAX_REPORT_SIZE   8       // HID报告最大大小

// ===================================================================
// ? 全局变量和缓冲区
// ===================================================================

// --- USB 缓冲区 (4字节对齐) ---
__attribute__((aligned(4))) uint8_t RxBuffer[MAX_PACKET_SIZE]; 
__attribute__((aligned(4))) uint8_t TxBuffer[MAX_PACKET_SIZE]; 

// --- 状态标志 ---
volatile uint8_t Bridge_NewDevFlag = 0; // 新设备连接事件标志

// --- 键盘状态 ---
static uint8_t  last_kbd_report[MAX_REPORT_SIZE] = {0}; // 上次报告(去抖动)
static uint8_t  kbd_send_pending = 0;     // 键盘发送等待标志

// --- 鼠标状态 ---
static uint8_t  last_mouse_report[MAX_REPORT_SIZE] = {0}; // 上次报告(去抖动)

// [优化] NiZ 鼠标专用同步记录变量
// 格式说明:Bit7=同步位(0=DATA0, 1=DATA1), Bit0-6=端点地址
// 初始值为 0x04 (端点4, 使用DATA0)
static uint8_t  Var_NizMouse_Record = (NIZ_MOUSE_ENDP & 0x7F); 

// ===================================================================
// ? 外部函数声明
// ===================================================================
extern uint8_t HidEmu_SendUSBReport(uint8_t *pData);
extern uint8_t HidEmu_SendMouseReport(uint8_t *pData);
extern uint8_t InitRootU2Device(void);
extern uint8_t AnalyzeRootU2Hub(void);
extern uint8_t EnumAllU2HubPort(void);
extern uint16_t U2SearchTypeDevice(uint8_t type);
extern void SelectU2HubPort(uint8_t hub_port);


// ===================================================================
// ? 键盘数据解析函数
// ===================================================================



/**
 * @brief  键盘数据解析 (支持标准6键和NKRO)
 * @param  in_buf   USB收到的原始数据
 * @param  len      数据长度
 * @param  out_buf  输出的标准8字节 HID 报告
 */
void Parse_Keyboard_Data(uint8_t* in_buf, uint8_t len, uint8_t* out_buf) {
    memset(out_buf, 0, 8);
    
    // 情况1: 标准 8 字节 Boot Keyboard 格式
    if (len == 8) {
        memcpy(out_buf, in_buf, 8);
        return;
    }

    // 情况2: NiZ 或 NKRO 长格式 (位图转换为标准格式)
    if (len > 8) {
        out_buf[0] = in_buf[0]; // 功能键区 (Ctrl/Shift/Alt/Win)
        int key_slot = 0;

        // 解析位图数据 (从第2字节开始)
        for (int i = 2; i < len; i++) {
            if (in_buf[i] != 0) {
                for (int bit = 0; bit < 8; bit++) {
                    if ((in_buf[i] >> bit) & 0x01) {
                        // 计算键码并添加偏移量
                        uint8_t keycode = (i - 2) * 8 + bit + NIZ_KEY_OFFSET;

                        // 填充到 6 个按键位置
                        if (keycode > 3 && keycode < 255 && key_slot < 6) {
                            out_buf[2 + key_slot] = keycode;
                            key_slot++;
                        }
                    }
                }
            }
        }
    }
}


// ===================================================================
// ? 设备发现和管理函数
// ===================================================================

/**
 * @brief  发现并添加新设备到设备管理器
 */
void USB_Bridge_DiscoverDevices(void)
{
    uint16_t search_res;
    uint8_t dev_addr, dev_type, endpoint;

    // 查找键盘设备
    search_res = U2SearchTypeDevice(DEV_TYPE_KEYBOARD);
    if (search_res != 0xFFFF) {
        dev_addr = (uint8_t)(search_res >> 8); // HUB端口号
        uint8_t interface = (uint8_t)search_res; // 接口号

        // 获取端点地址
        endpoint = GetEndpointFromDevice(interface, 0); // 0 = 键盘设备

        if (IsEndpointValid(endpoint)) {
            // 检查设备是否已存在
            uint8_t dev_index = UsbDeviceManager_FindDeviceByType(DEV_TYPE_KEYBOARD);
            if (dev_index == 0xFF) {
                // 设备不存在，添加新设备
                dev_index = UsbDeviceManager_AddDevice(dev_addr, DEV_TYPE_KEYBOARD, endpoint);
                if (dev_index != 0xFF) {
                    LOG_SYS("Keyboard device added: index=%u\n", dev_index);
                }
            }
        }
    }

    // 查找鼠标设备
    search_res = U2SearchTypeDevice(DEV_TYPE_MOUSE);
    if (search_res != 0xFFFF) {
        dev_addr = (uint8_t)(search_res); // HUB端口号
        uint8_t interface = 1; // 鼠标设备通常使用接口1

        // 获取端点地址
        endpoint = GetEndpointFromDevice(interface, 1); // 1 = 鼠标设备

        if (IsEndpointValid(endpoint)) {
            // 检查设备是否已存在
            uint8_t dev_index = UsbDeviceManager_FindDeviceByType(DEV_TYPE_MOUSE);
            if (dev_index == 0xFF) {
                // 设备不存在，添加新设备
                dev_index = UsbDeviceManager_AddDevice(dev_addr, DEV_TYPE_MOUSE, endpoint);
                if (dev_index != 0xFF) {
                    LOG_SYS("Mouse device added: index=%u\n", dev_index);
                }
            }
        }
    }
}

/**
 * @brief  处理单个设备的数据读取
 * @param  dev_index  设备索引
 * @return 1=成功读取数据, 0=失败或无数据
 */
uint8_t USB_Bridge_ProcessDevice(uint8_t dev_index)
{
    UsbDevice_t* p_device = UsbDeviceManager_GetDevice(dev_index);
    if (p_device == NULL) {
        return 0;
    }

    if (!p_device->is_connected || !p_device->is_valid) {
        return 0;
    }

    uint8_t s, len;
    uint8_t endp_addr = p_device->endpoint;

    // 选择设备端口
    SelectU2HubPort(p_device->dev_addr);

    // 执行 IN 事务
    // 端点地址的 Bit7 会自动指示是否需要 DATA0 或 DATA1
    s = USB_HostTransact(endp_addr, Endpoint_GetToggleFlag(endp_addr), 0);

    if (s == ERR_SUCCESS) {
        // 成功，翻转同步位 (Bit7)
        endp_addr = Endpoint_SyncToggle(endp_addr);

        // 更新设备管理器中的端点地址
        p_device->endpoint = endp_addr;

        len = R8_USB2_RX_LEN;
        if (len > 0) {
            if (p_device->dev_type == DEV_TYPE_KEYBOARD) {
                // 处理键盘数据
                uint8_t temp_report[MAX_REPORT_SIZE] = {0};
                Parse_Keyboard_Data(RxBuffer, len, temp_report);
                DBG_KEYS(temp_report);

                // 更新设备报告缓冲区
                UsbDeviceManager_UpdateReport(dev_index, temp_report, 8);

                // 发送BLE报告
                if (HidEmu_SendUSBReport(temp_report) != SUCCESS) {
                    // 发送失败，记录错误
                    ErrorStats_USBCommFail();
                }
            }
            else if (p_device->dev_type == DEV_TYPE_MOUSE) {
                // 处理鼠标数据
                uint8_t mouse_data[MAX_REPORT_SIZE] = {0};

                // --- 协议适配 ---
                if (len == 5) {
                    // NiZ 格式: [ID, Btn, X, Y, Wheel] -> 偏移1字节
                    memcpy(mouse_data, RxBuffer + 1, 4);
                } else if (len >= 7) {
                    // 标准格式
                    mouse_data[0] = RxBuffer[1]; // Btn
                    mouse_data[1] = RxBuffer[2]; // X
                    mouse_data[2] = RxBuffer[4]; // Y
                    mouse_data[3] = RxBuffer[6]; // Wheel
                } else if (len == 3) {
                    // 标准鼠标格式: [Btn, X, Y]
                    memcpy(mouse_data, RxBuffer, 3);
                } else if (len == 4) {
                    // 某些带 ID 的 4 字节格式
                    if (RxBuffer[0] <= 5) memcpy(mouse_data, RxBuffer + 1, 3);
                    else memcpy(mouse_data, RxBuffer, 4);
                }

                DBG_MOUSE(mouse_data);

                // 更新设备报告缓冲区
                UsbDeviceManager_UpdateReport(dev_index, mouse_data, 4);

                // 发送BLE报告（鼠标不需要重试机制，确保实时性）
                HidEmu_SendMouseReport(mouse_data);
            }

            return 1;
        }
    }

    return 0;
}

/**
 * @brief  检查设备连接状态
 * @param  dev_index  设备索引
 * @return 1=设备断开, 0=设备正常
 */
uint8_t USB_Bridge_CheckDeviceDisconnect(uint8_t dev_index)
{
    UsbDevice_t* p_device = UsbDeviceManager_GetDevice(dev_index);
    if (p_device == NULL) {
        return 0;
    }

    // 检查设备状态
    if (ThisUsb2Dev.DeviceStatus == ROOT_DEV_DISCONNECTED) {
        return 1;
    }

    return 0;
}

/**
 * @brief  移除断开的设备
 */
void USB_Bridge_RemoveDisconnectedDevices(void)
{
    for (uint8_t i = 0; i < MAX_USB_DEVICES; i++) {
        if (UsbDeviceManager_IsValid(i)) {
            if (USB_Bridge_CheckDeviceDisconnect(i)) {
                UsbDeviceManager_RemoveDevice(i);
            }
        }
    }
}

// ===================================================================
// ? �����߼�
// ===================================================================

void USB_Bridge_Init(void) {
    // 1. Ӳ�� IO ��ʼ�� (���� USB ����)
    GPIOA_SetBits(GPIO_Pin_9);
    GPIOA_ModeCfg(GPIO_Pin_9, GPIO_ModeOut_PP_5mA);
    
    // 2. �� USB RAM
    pU2HOST_RX_RAM_Addr = RxBuffer;
    pU2HOST_TX_RAM_Addr = TxBuffer;
    
    // 3. ��ʼ��Э��ջ
    USB2_HostInit();
    
    Bridge_NewDevFlag = 0;
    kbd_send_pending = 0;
    
    // ��ʼ�� NiZ ��¼���� (��� DATA1 ��־��ֻ�����˵��)
    Var_NizMouse_Record = (NIZ_MOUSE_ENDP & 0x7F);

    // 4. ��ʼ��ϵͳ��ͳ
    ErrorStats_Init();

    // 5. ��ʼ�� USB ���
    USBReconnect_Init();

    // 6. ��ʼ�� BLE ���
    BLEReconnect_Init();

    // 7. ��ʼ�� Configuration
    USB_Bridge_ConfigInit();

    // 8. ��ʼ�� USB 豸���
    UsbDeviceManager_Init();

    LOG_SYS("USB Init OK. Bridge Ready.\n");
}

/**
 * @brief USB桥接主循环处理函数
 *
 * @details
 * #### 功能详细说明
 * 这是USB桥接模块的主循环函数，负责处理所有USB设备的数据传输和管理。
 * 该函数应该在系统主循环中定期调用，以确保USB设备的正常工作。
 *
 * #### 算法原理
 * 1. **键盘重发处理**：
 *    - 检查kbd_send_pending标志，如果为1表示上次键盘发送失败
 *    - 尝试重新发送上次的键盘报告
 *    - 如果成功，清除标志；如果仍然失败，返回等待下次重试
 *
 * 2. **USB硬件中断处理**：
 *    - 检测USB设备连接/断开事件
 *    - 处理USB枚举完成事件
 *    - 更新设备状态标志
 *
 * 3. **新设备连接处理**：
 *    - 当检测到新设备连接时，初始化USB设备
 *    - 配置设备参数，恢复同步位状态
 *    - 记录设备连接日志
 *
 * 4. **设备轮询处理**：
 *    - 遍历所有已连接的USB设备
 *    - 对每个设备执行数据读取和处理
 *    - 将处理后的数据发送到BLE
 *
 * #### 调用示例
 * ```c
 * // 在主循环中调用
 * while(1) {
 *     USB_Bridge_Poll();
 *     // 其他任务处理
 *     Delay_ms(1);
 * }
 * ```
 *
 * #### 使用场景
 * - 系统主循环中定期调用
 * - USB设备数据传输处理
 * - 设备状态监控
 *
 * #### 错误处理
 * - 键盘发送失败：设置kbd_send_pending标志，下次重试
 * - USB设备断开：自动移除设备，更新设备管理器
 * - 数据读取失败：跳过当前设备，继续处理下一个设备
 *
 * #### 边界情况
 * - 无设备连接：直接返回，不执行设备处理
 * - 设备忙：跳过当前设备，继续处理下一个设备
 * - 内存不足：设备管理器会返回错误，跳过设备添加
 *
 * #### 注意事项
 * - 必须定期调用，建议调用间隔不超过10ms
 * - 不要在中断上下文中调用
 * - 确保系统时钟正常工作
 */
void USB_Bridge_Poll(void) {
    uint8_t s, len, endp_addr;
    uint16_t search_res;
    
    // --------------------------------------------------------
    // [步骤 0] 键盘重发处理
    // 如果上次发送失败，设备忙，则重试发送，确保数据不丢失
    // --------------------------------------------------------
    if (kbd_send_pending) {
        if (HidEmu_SendUSBReport(last_kbd_report) == SUCCESS) {
            kbd_send_pending = 0; // 发送成功，清除标志
            LOG_BLE("KBD Resend OK\n");
        } else {
            return; // 仍然忙，发送，继续执行
        }
    }

    // --------------------------------------------------------
    // [步骤 1] 硬件中断处理，设备枚举
    // --------------------------------------------------------
    if(R8_USB2_INT_FG & RB_UIF_DETECT) {
        R8_USB2_INT_FG = RB_UIF_DETECT; // 清除中断
        s = AnalyzeRootU2Hub();
        if(s == ERR_USB_CONNECT) Bridge_NewDevFlag = 1;
        else if (s == ERR_USB_DISCON) Bridge_NewDevFlag = 0;
    }
    // 防止其他意外中断干扰
    else if (R8_USB2_INT_FG) {
        R8_USB2_INT_FG = 0xFF;
    }

    // 新设备连接处理
    if(Bridge_NewDevFlag) {
        Bridge_NewDevFlag = 0;
        mDelaymS(200); // 等待设备电源稳定
        s = InitRootU2Device();
        if(s == ERR_SUCCESS){
            LOG_SYS("Device Enum OK\n");
            // 新设备需要重新配置，特别是 NiZ 鼠标同步位
            // 恢复为 0x04 (Bit7=0 表示下次传输 DATA0)
            Var_NizMouse_Record = (NIZ_MOUSE_ENDP & 0x7F);
        }
    }

    // 维护 HUB 状态 (如果是 HUB)
    EnumAllU2HubPort(); 

    // =================================================================
    // [步骤 2] 设备发现
    // =================================================================
    // 发现新设备
    USB_Bridge_DiscoverDevices();

    // =================================================================
    // [步骤 3] 设备清理
    // =================================================================
    // 清理断开的设备
    USB_Bridge_RemoveDisconnectedDevices();

    // =================================================================
    // [步骤 4] 设备轮询
    // =================================================================
    // 轮询所有设备
    for (uint8_t i = 0; i < MAX_USB_DEVICES; i++) {
        if (UsbDeviceManager_IsValid(i)) {
            USB_Bridge_ProcessDevice(i);
        }
    }

    // =================================================================
    // [步骤 4] 错误恢复
    // =================================================================
    // 错误恢复轮询 (获取系统时间)
    // 错误恢复统计和 USB/BLE 重连
    ErrorRecovery_Poll(1); // 1ms tick
}