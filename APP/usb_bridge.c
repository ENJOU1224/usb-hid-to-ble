/*********************************************************************
 * File Name          : usb_bridge.c
 * Author             : DIY User & AI Assistant
 * Version            : V8.0 (Final Engineering)
 * Description        : USB Host 转 Bluetooth 核心桥接逻辑
 *                      包含 NiZ 键盘特殊适配、NKRO 解析、蓝牙流控
 *********************************************************************/

#include "CH58xBLE_LIB.H"
#include "CH58x_common.h"

// ===================================================================
// ? 用户配置区 (User Configuration)
// ===================================================================

// NiZ 键盘键位偏移量 (根据之前的测试，NiZ 需要偏移 4)
#define NIZ_KEY_OFFSET    4   

// ===================================================================

// --- 外部函数引用 (External References) ---
// 发送报文给蓝牙，返回 SUCCESS(0) 或 失败代码
extern uint8_t HidEmu_SendUSBReport(uint8_t *pData);

// 官方 USB2 Host 驱动函数
extern uint8_t InitRootU2Device(void);
extern uint8_t AnalyzeRootU2Hub(void);
extern uint8_t EnumAllU2HubPort(void);
extern uint16_t U2SearchTypeDevice(uint8_t type);
extern void SelectU2HubPort(uint8_t hub_port);
extern uint8_t U2SETorOFFNumLock(PUINT8 buf);

// --- 内存缓冲区 (Memory Buffers) ---
// 必须 4 字节对齐，这是 DMA 的硬性要求
__attribute__((aligned(4))) uint8_t RxBuffer[MAX_PACKET_SIZE]; 
__attribute__((aligned(4))) uint8_t TxBuffer[MAX_PACKET_SIZE]; 

// --- 全局状态变量 (Global States) ---
volatile uint8_t Bridge_FoundNewDev = 0; // 标记是否有新设备插入
uint8_t last_report[8] = {0};            // 缓存：记录上一次成功发给电脑的数据
static uint8_t send_pending = 0;         // 标志位：1=蓝牙忙，数据待重发

// --- 宏逻辑处理 (Macro Logic) ---
// 如果开启了按键调试，则定义打印函数；否则定义为空宏(不占空间)
#ifdef DEBUG
    void Show_Current_Keys(uint8_t *report);
    #define DBG_KEYS(r) Show_Current_Keys(r)
    #define LOG_SYS(...) PRINT(__VA_ARGS__)
#else
    #define DBG_KEYS(r) do{}while(0) // 这是一个C语言技巧，表示“什么都不做”，会被编译器优化掉
    #define LOG_SYS(...) do{}while(0)
#endif



// ===================================================================
// ?? 核心功能函数实现
// ===================================================================

/**
 * @brief  初始化 USB 桥接模块
 * @note   务必开启 PA9 供电，否则静电容键盘无法工作
 */
void USB_Bridge_Init(void)
{
    // 1. 硬件供电开启 (针对你的评估板)
    GPIOA_SetBits(GPIO_Pin_9);
    GPIOA_ModeCfg(GPIO_Pin_9, GPIO_ModeOut_PP_5mA);
    
    // 2. 绑定 USB2 底层缓冲区
    pU2HOST_RX_RAM_Addr = RxBuffer;
    pU2HOST_TX_RAM_Addr = TxBuffer;
    
    // 3. 初始化 USB 控制器
    USB2_HostInit();
    
    Bridge_FoundNewDev = 0;
    send_pending = 0;
    
    LOG_SYS("USB2 Bridge Start. PA9 Power ON.\n");
    #ifdef DEBUG
        LOG_SYS("Debug Mode: ON (Keys will be printed)\n");
    #endif
}

/**
 * @brief  将 USB 接收到的原始数据 (可能为NKRO长包) 解析为标准 8字节 HID 报文
 * @param  in_buf:  USB 原始输入缓冲区
 * @param  len:     数据长度
 * @param  out_buf: 解析后的 8字节标准缓冲区
 */
void Parse_Input_Data(uint8_t* in_buf, uint8_t len, uint8_t* out_buf)
{
    // 先清空目标
    memset(out_buf, 0, 8);
    
    // 情况 A: 标准 6键无冲包 (长度刚好为 8)
    // 大多数情况下或切换到 6KRO 模式时触发
    if (len == 8) {
        memcpy(out_buf, in_buf, 8);
        return;
    }
    
    // 情况 B: 全键无冲 NKRO 包 (长度 > 8)
    // 此时数据是位图(Bitmap)格式，需要转换
    if (len > 8) {
        out_buf[0] = in_buf[0]; // 第0字节通常是修饰键(Ctrl/Shift)，直接复制
        int key_count = 0;
        
        // NiZ 的位图通常从第 2 字节开始 (Byte0=Mod, Byte1=Res)
        for (int i = 2; i < len; i++) {
            if (in_buf[i] != 0) { // 优化：如果该字节全0，直接跳过
                for (int bit = 0; bit < 8; bit++) {
                    // 检查每一位是否为 1
                    if ((in_buf[i] >> bit) & 0x01) {
                        // 计算标准 HID 键码
                        // 公式：(当前字节偏移 - 2) * 8 + 当前位 + 固定偏移量
                        uint8_t keycode = (i - 2) * 8 + bit + NIZ_KEY_OFFSET;
                        
                        // 简单的合法性过滤
                        if (keycode > 3 && keycode < 255) {
                            if (key_count < 6) { // 标准 HID 最多存 6 个键
                                out_buf[2 + key_count] = keycode;
                                key_count++;
                            }
                        }
                    }
                }
            }
        }
    }
}

/**
 * @brief  主轮询任务 (放在 main 的 while(1) 中)
 * @note   包含核心流控逻辑和枚举逻辑
 */
void USB_Bridge_Poll(void)
{
    uint8_t s, i, len, endp;
    uint16_t loc;
    
    // -------------------------------------------------------------
    // [阶段 1] 蓝牙流控 (Flow Control) - 最关键的防丢包机制
    // -------------------------------------------------------------
    // 如果上一次的数据还在等待发送 (send_pending=1)，
    // 必须停止读取 USB，死磕蓝牙发送，直到成功为止。
    if (send_pending) {
        if (HidEmu_SendUSBReport(last_report) == SUCCESS) {
            send_pending = 0; // 发送成功，解除阻塞
            // LOG_SYS("Resend OK\n");
        } else {
            return; // 蓝牙还是忙，退出函数，下一轮循环继续重试
        }
    }

    // -------------------------------------------------------------
    // [阶段 2] 硬件状态机维护 (Hot-Plug Detection)
    // -------------------------------------------------------------
    // 检查 USB 中断标志
    if(R8_USB2_INT_FG & RB_UIF_DETECT) {
        R8_USB2_INT_FG = RB_UIF_DETECT; // 清除中断位
        s = AnalyzeRootU2Hub();         // 分析是插入还是拔出
        if(s == ERR_USB_CONNECT) {
            Bridge_FoundNewDev = 1;
            LOG_SYS("Event: Device Plugged In\n");
        }
        else if (s == ERR_USB_DISCON) {
            Bridge_FoundNewDev = 0;
            LOG_SYS("Event: Device Removed\n");
        }
    }
    else if (R8_USB2_INT_FG) { 
        // 清除其他不关心的干扰中断，防止卡死
        R8_USB2_INT_FG = 0xFF; 
    }

    // -------------------------------------------------------------
    // [阶段 3] 新设备初始化 (Enumeration)
    // -------------------------------------------------------------
    if(Bridge_FoundNewDev) {
        Bridge_FoundNewDev = 0;
        mDelaymS(200); // 等待电压稳定
        s = InitRootU2Device(); // 执行标准 USB 枚举
        if(s == ERR_SUCCESS) LOG_SYS("Enum Success. Keyboard Ready.\n");
        else LOG_SYS("Enum Failed: %02X\n", s);
    }

    // 维护 HUB 下挂设备（虽然你还没用到 HUB）
    EnumAllU2HubPort();

    // -------------------------------------------------------------
    // [阶段 4] 键盘数据读取 (Data Polling)
    // -------------------------------------------------------------
    // 搜索键盘类型的设备接口
    loc = U2SearchTypeDevice(DEV_TYPE_KEYBOARD);
    if(loc != 0xFFFF)
    {
        // 解析端口号和地址
        i = (uint8_t)(loc >> 8);
        len = (uint8_t)loc;
        SelectU2HubPort(len); 
        // 获取该设备的中断端点
        endp = len ? DevOnU2HubPort[len - 1].GpVar[0] : ThisUsb2Dev.GpVar[0];

        // 端点有效，发起 IN 请求
        if(endp & USB_ENDP_ADDR_MASK)
        {
            s = USB2HostTransact(USB_PID_IN << 4 | (endp & 0x7F), 
                                 (endp & 0x80) ? (RB_UH_R_TOG | RB_UH_T_TOG) : 0, 0);

            if(s == ERR_SUCCESS)
            {
                // 成功读取到数据，翻转同步位
                endp ^= 0x80;
                if(len) DevOnU2HubPort[len - 1].GpVar[0] = endp;
                else    ThisUsb2Dev.GpVar[0] = endp;

                // 获取接收到的数据长度
                len = R8_USB2_RX_LEN;
                
                if(len > 0) 
                {
                    uint8_t current_report[8] = {0};
                    
                    // A. 数据标准化解析 (NKRO -> Std)
                    Parse_Input_Data(RxBuffer, len, current_report);

                    // B. 变化检测 (直通逻辑，Change Report)
                    if (memcmp(last_report, current_report, 8) != 0)
                    {
                        // 数据变了，更新缓存
                        memcpy(last_report, current_report, 8);
                        
                        // 打印调试信息 (通过宏控制)
                        DBG_KEYS(last_report);
                        
                        // C. 发送给蓝牙
                        if (HidEmu_SendUSBReport(last_report) != SUCCESS) {
                            // 如果蓝牙缓冲区满了，标记 Pending
                            // 下一轮循环会在 [阶段 1] 强制重发
                            send_pending = 1; 
                        }
                    }
                }
            }
        }
    }
}

// ===================================================================
// ?? 调试辅助函数 (Debug Helpers)
// ===================================================================

#ifdef DEBUG
// HID 键码到按键名称的映射表（标准USB HID键盘键码）
static const char* key_names[] = {
    [0x00] = NULL,         // 无键
    [0x04] = "a",
    [0x05] = "b",
    [0x06] = "c",
    [0x07] = "d",
    [0x08] = "e",
    [0x09] = "f",
    [0x0A] = "g",
    [0x0B] = "h",
    [0x0C] = "i",
    [0x0D] = "j",
    [0x0E] = "k",
    [0x0F] = "l",
    [0x10] = "m",
    [0x11] = "n",
    [0x12] = "o",
    [0x13] = "p",
    [0x14] = "q",
    [0x15] = "r",
    [0x16] = "s",
    [0x17] = "t",
    [0x18] = "u",
    [0x19] = "v",
    [0x1A] = "w",
    [0x1B] = "x",
    [0x1C] = "y",
    [0x1D] = "z",
    [0x1E] = "1",
    [0x1F] = "2",
    [0x20] = "3",
    [0x21] = "4",
    [0x22] = "5",
    [0x23] = "6",
    [0x24] = "7",
    [0x25] = "8",
    [0x26] = "9",
    [0x27] = "0",
    [0x28] = "Enter",
    [0x29] = "Esc",
    [0x2A] = "Backspace",
    [0x2B] = "Tab",
    [0x2C] = "Space",
    [0x2D] = "-",
    [0x2E] = "=",
    [0x2F] = "[",
    [0x30] = "]",
    [0x31] = "\\",
    [0x32] = ";",
    [0x33] = "'",
    [0x34] = "`",
    [0x35] = ",",
    [0x36] = ".",
    [0x37] = "/",
    [0x39] = "CapsLock",
    [0x3A] = "F1",
    [0x3B] = "F2",
    [0x3C] = "F3",
    [0x3D] = "F4",
    [0x3E] = "F5",
    [0x3F] = "F6",
    [0x40] = "F7",
    [0x41] = "F8",
    [0x42] = "F9",
    [0x43] = "F10",
    [0x44] = "F11",
    [0x45] = "F12",
    [0x46] = "PrintScreen",
    [0x47] = "ScrollLock",
    [0x48] = "Pause",
    [0x49] = "Insert",
    [0x4A] = "Home",
    [0x4B] = "PageUp",
    [0x4C] = "Delete",
    [0x4D] = "End",
    [0x4E] = "PageDown",
    [0x4F] = "Right",
    [0x50] = "Left",
    [0x51] = "Down",
    [0x52] = "Up",
    [0x53] = "NumLock",
    [0x54] = "KP /",
    [0x55] = "KP *",
    [0x56] = "KP -",
    [0x57] = "KP +",
    [0x58] = "KP Enter",
    [0x59] = "KP 1",
    [0x5A] = "KP 2",
    [0x5B] = "KP 3",
    [0x5C] = "KP 4",
    [0x5D] = "KP 5",
    [0x5E] = "KP 6",
    [0x5F] = "KP 7",
    [0x60] = "KP 8",
    [0x61] = "KP 9",
    [0x62] = "KP 0",
    [0x63] = "KP .",
    [0x64] = "\\",
    [0x65] = "Menu",
    // 更多键码可以在这里添加...
};

// 辅助函数：获取键码对应的名称
static const char* get_key_name(uint8_t key_code)
{
    if (key_code >= sizeof(key_names) / sizeof(key_names[0])) {
        return NULL;
    }
    return key_names[key_code];
}

void Show_Current_Keys(uint8_t *report)
{
    uint8_t mods = report[0];
    uint8_t has_print = 0;

    PRINT("KEYS: ");

    // 打印修饰键
    if (mods & 0x01) { PRINT("L-Ctrl ");  has_print=1; }
    if (mods & 0x02) { PRINT("L-Shift "); has_print=1; }
    if (mods & 0x04) { PRINT("L-Alt ");   has_print=1; }
    if (mods & 0x08) { PRINT("L-Win ");   has_print=1; }
    if (mods & 0x10) { PRINT("R-Ctrl ");  has_print=1; }
    if (mods & 0x20) { PRINT("R-Shift "); has_print=1; }
    if (mods & 0x40) { PRINT("R-Alt ");   has_print=1; }
    if (mods & 0x80) { PRINT("R-Win ");   has_print=1; }

    // 打印普通键码
    for (int i = 2; i < 8; i++) {
        uint8_t key_code = report[i];
        if (key_code != 0) {
            const char* key_name = get_key_name(key_code);
            if (key_name) {
                if(has_print) PRINT("+ ");
                PRINT("%s ", key_name);
            } else {
                if(has_print) PRINT("+ ");
                // 如果没有映射到名称，显示十六进制
                PRINT("[0x%02X] ", key_code);
            }
            has_print = 1;
        }
    }

    if (!has_print) PRINT("(All Released)");
    PRINT("\n");
}
#endif