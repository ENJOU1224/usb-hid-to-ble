/*********************************************************************
 * File Name          : usb_bridge.c
 * Author             : DIY User & AI Assistant
 * Version            : V17.0 (Mouse Continuous Move Fix)
 * Description        : USB Host 转 Bluetooth 核心桥接逻辑
 *                      - 修复键盘模拟鼠标长按不移动的问题
 *                      - 包含详细注释
 *********************************************************************/

#include "CH58xBLE_LIB.H"
#include "CH58x_common.h"
#include "debug.h"

// ===================================================================
// ? 用户配置区 (User Configuration)
// ===================================================================

// NiZ 键盘键位偏移量
// NiZ 的 NKRO 协议通常从第 2 字节开始位图，计算出的键码通常偏小，需要补偿
#define NIZ_KEY_OFFSET    4   

// NiZ 键盘的鼠标数据端点 (Interface 2)
// 只有强制读取这个端点，才能获取 Fn+WASD 的数据
#define NIZ_MOUSE_ENDP    0x84 

// 键盘长按保活阈值 (单位: 轮询次数)
// 防止蓝牙连接活跃度低导致系统认为按键松开
#define KBD_KEEP_ALIVE_THRESHOLD  2000

// ===================================================================

// --- 外部函数引用 (External References) ---
// 发送键盘报文 (8字节)
extern uint8_t HidEmu_SendUSBReport(uint8_t *pData);
// 发送鼠标报文 (4字节)
extern uint8_t HidEmu_SendMouseReport(uint8_t *pData);

// 官方 USB2 Host 驱动函数
extern uint8_t InitRootU2Device(void);
extern uint8_t AnalyzeRootU2Hub(void);
extern uint8_t EnumAllU2HubPort(void);
extern uint16_t U2SearchTypeDevice(uint8_t type);
extern void SelectU2HubPort(uint8_t hub_port);
extern uint8_t U2SETorOFFNumLock(PUINT8 buf);

// --- 内存缓冲区 (必须4字节对齐) ---
__attribute__((aligned(4))) uint8_t RxBuffer[MAX_PACKET_SIZE]; 
__attribute__((aligned(4))) uint8_t TxBuffer[MAX_PACKET_SIZE]; 

// --- 全局状态变量 ---
volatile uint8_t Bridge_FoundNewDev = 0; // 新设备插入标志
uint8_t last_report[8] = {0};            // 键盘上一次发送的数据(用于去重)
uint8_t last_mouse_report[4] = {0};      // 鼠标上一次发送的数据
static uint8_t send_pending = 0;         // 键盘流控标志

// 键盘保活计数器
static uint32_t keep_alive_timer = 0;

// ===================================================================
// ?? 辅助函数区
// ===================================================================

/**
 * @brief  判断键盘是否处于全松开状态
 * @return 1=全松开, 0=有键按下
 */
uint8_t Is_All_Released(uint8_t *buf) {
    // 遍历8个字节，只要有一个非0，就说明有动作
    for(int i=0; i<8; i++) {
        if(buf[i] != 0) return 0; 
    }
    return 1;
}

/**
 * @brief  调试打印：显示原始鼠标数据
 */
void Show_Raw_Mouse(uint8_t *buf, uint8_t len)
{
    #if (defined(DEBUG_MOUSE) && DEBUG_MOUSE == 1)
    PRINT("RAW MOUSE [%d]: ", len);
    for(int i=0; i<len; i++) PRINT("%02X ", buf[i]);
    PRINT("\n");
    #endif
}

/**
 * @brief  调试打印：解析并显示键盘按键名称
 */
#if (defined(DEBUG_KEY) && DEBUG_KEY == 1)
static const char* key_names[] = {
    [0x00] = NULL, [0x04]="a", [0x05]="b", [0x06]="c", [0x07]="d", [0x08]="e",
    [0x09]="f", [0x0A]="g", [0x0B]="h", [0x0C]="i", [0x0D]="j", [0x0E]="k",
    [0x0F]="l", [0x10]="m", [0x11]="n", [0x12]="o", [0x13]="p", [0x14]="q",
    [0x15]="r", [0x16]="s", [0x17]="t", [0x18]="u", [0x19]="v", [0x1A]="w",
    [0x1B]="x", [0x1C]="y", [0x1D]="z", [0x1E]="1", [0x1F]="2", [0x20]="3",
    [0x21]="4", [0x22]="5", [0x23]="6", [0x24]="7", [0x25]="8", [0x26]="9",
    [0x27]="0", [0x28]="Enter", [0x29]="Esc", [0x2A]="Backspace", [0x2B]="Tab",
    [0x2C]="Space", [0x2D]="-", [0x2E]="=", [0x2F]="[", [0x30]="]", [0x31]="\\",
    [0x32]=";", [0x33]="'", [0x34]="`", [0x35]=",", [0x36]=".", [0x37]="/",
    [0x39]="CapsLock", [0x3A]="F1", [0x3B]="F2", [0x3C]="F3", [0x3D]="F4",
    [0x3E]="F5", [0x3F]="F6", [0x40]="F7", [0x41]="F8", [0x42]="F9", [0x43]="F10",
    [0x44]="F11", [0x45]="F12", [0x4F]="Right", [0x50]="Left", [0x51]="Down", [0x52]="Up",
};

void Show_Current_Keys(uint8_t *report) {
    uint8_t mods = report[0];
    uint8_t has_print = 0;
    PRINT("KEYS: ");
    if (mods & 0x01) { PRINT("L-Ctrl ");  has_print=1; }
    if (mods & 0x02) { PRINT("L-Shift "); has_print=1; }
    if (mods & 0x04) { PRINT("L-Alt ");   has_print=1; }
    if (mods & 0x08) { PRINT("L-Win ");   has_print=1; }
    if (mods & 0x10) { PRINT("R-Ctrl ");  has_print=1; }
    if (mods & 0x20) { PRINT("R-Shift "); has_print=1; }
    if (mods & 0x40) { PRINT("R-Alt ");   has_print=1; }
    if (mods & 0x80) { PRINT("R-Win ");   has_print=1; }
    for (int i = 2; i < 8; i++) {
        uint8_t code = report[i];
        if (code != 0 && code < sizeof(key_names)/sizeof(char*)) {
            if(key_names[code]) { 
                if(has_print) PRINT("+ ");
                PRINT("%s ", key_names[code]); 
            } else {
                if(has_print) PRINT("+ ");
                PRINT("[%02X] ", code);
            }
            has_print = 1;
        }
    }
    if (!has_print) PRINT("(All Released)");
    PRINT("\n");
}
#endif

/**
 * @brief  调试打印：显示鼠标坐标和按键
 */
#ifdef DEBUG_MOUSE
void Show_Current_Mouse(uint8_t *report) {
    PRINT("MOUSE: ");
    if (report[0] & 0x01) PRINT("[L] ");
    if (report[0] & 0x02) PRINT("[R] ");
    if (report[0] & 0x04) PRINT("[M] ");
    int8_t x=(int8_t)report[1], y=(int8_t)report[2], w=(int8_t)report[3];
    if (x!=0 || y!=0 || w!=0) PRINT("X:%d Y:%d W:%d", x, y, w);
    PRINT("\n");
}
#endif

/**
 * @brief  键盘数据解析 (兼容标准6键与NKRO)
 */
void Parse_Input_Data(uint8_t* in_buf, uint8_t len, uint8_t* out_buf)
{
    memset(out_buf, 0, 8);
    // 1. 标准 8 字节包：直接复制
    if (len == 8) {
        memcpy(out_buf, in_buf, 8);
        return;
    }
    // 2. NKRO 长包：位图转字节
    if (len > 8) {
        out_buf[0] = in_buf[0]; // 修饰键
        int key_count = 0;
        for (int i = 2; i < len; i++) {
            if (in_buf[i] != 0) {
                for (int bit = 0; bit < 8; bit++) {
                    if ((in_buf[i] >> bit) & 0x01) {
                        uint8_t keycode = (i - 2) * 8 + bit + NIZ_KEY_OFFSET;
                        if (keycode > 3 && keycode < 255) {
                            if (key_count < 6) {
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

// ===================================================================
// ? 核心初始化与轮询
// ===================================================================

void USB_Bridge_Init(void)
{
    // 开启 PA9 为 USB 供电 (根据板子硬件连接)
    GPIOA_SetBits(GPIO_Pin_9);
    GPIOA_ModeCfg(GPIO_Pin_9, GPIO_ModeOut_PP_5mA);
    
    // 绑定 USB 数据缓冲区
    pU2HOST_RX_RAM_Addr = RxBuffer;
    pU2HOST_TX_RAM_Addr = TxBuffer;
    
    // 初始化 USB Host
    USB2_HostInit();
    
    Bridge_FoundNewDev = 0;
    send_pending = 0;
    LOG_SYS("USB2 Init OK. Keep-Alive Mode ON.\n");
}

void USB_Bridge_Poll(void)
{
    uint8_t s, i, len, endp;
    uint16_t loc;
    
    // --------------------------------------------------------
    // 1. 键盘数据流控重发
    //    如果上一包键盘数据没发出去，优先重试，防止丢键
    // --------------------------------------------------------
    if (send_pending) {
        if (HidEmu_SendUSBReport(last_report) == SUCCESS) {
            send_pending = 0;
            LOG_BLE("Resend!\n");
        } else {
            return; // 蓝牙忙，退出等待下次
        }
    }

    // --------------------------------------------------------
    // 2. 硬件检测插拔事件
    // --------------------------------------------------------
    if(R8_USB2_INT_FG & RB_UIF_DETECT) {
        R8_USB2_INT_FG = RB_UIF_DETECT; // 清除标志
        s = AnalyzeRootU2Hub();
        if(s == ERR_USB_CONNECT) Bridge_FoundNewDev = 1;
        else if (s == ERR_USB_DISCON) Bridge_FoundNewDev = 0;
    }
    else if (R8_USB2_INT_FG) { 
        R8_USB2_INT_FG = 0xFF; // 清除杂项中断防止卡死
    }

    // --------------------------------------------------------
    // 3. 新设备枚举
    // --------------------------------------------------------
    if(Bridge_FoundNewDev) {
        Bridge_FoundNewDev = 0;
        mDelaymS(200); 
        s = InitRootU2Device();
        if(s == ERR_SUCCESS) LOG_SYS("Enum OK\n");
    }

    EnumAllU2HubPort(); // 维护 HUB 状态

    // =================================================================
    // ? 任务 A: 读取键盘 (需去重 + 保活)
    // =================================================================
    loc = U2SearchTypeDevice(DEV_TYPE_KEYBOARD);
    if(loc != 0xFFFF)
    {
        i = (uint8_t)(loc >> 8);
        len = (uint8_t)loc;
        SelectU2HubPort(len); 
        endp = len ? DevOnU2HubPort[len - 1].GpVar[0] : ThisUsb2Dev.GpVar[0];

        if(endp & USB_ENDP_ADDR_MASK)
        {
            s = USB2HostTransact(USB_PID_IN << 4 | (endp & 0x7F), 
                                 (endp & 0x80) ? (RB_UH_R_TOG | RB_UH_T_TOG) : 0, 0);

            if(s == ERR_SUCCESS)
            {
                endp ^= 0x80;
                if(len) DevOnU2HubPort[len - 1].GpVar[0] = endp;
                else    ThisUsb2Dev.GpVar[0] = endp;

                len = R8_USB2_RX_LEN;
                if(len > 0) 
                {
                    uint8_t current_report[8] = {0};
                    Parse_Input_Data(RxBuffer, len, current_report);
                    
                    // 立即发送
                    memcpy(last_report, current_report, 8);
                    DBG_KEYS(last_report);
                        
                    if (HidEmu_SendUSBReport(last_report) != SUCCESS) send_pending = 1;
                }
            }
        }
    }

    // =================================================================
    // ?? 任务 B: 读取鼠标 (修复版：有动作就发，不拦截)
    // =================================================================
    loc = U2SearchTypeDevice(DEV_TYPE_MOUSE);
    uint8_t target_endp = 0;
    
    // 1. 寻找端点
    if (loc != 0xFFFF) {
        len = (uint8_t)loc; 
        SelectU2HubPort(len);
        target_endp = len ? DevOnU2HubPort[len - 1].GpVar[0] : ThisUsb2Dev.GpVar[0];
    } else {
        // 针对 NiZ 的特殊处理 (强制读取 0x84)
        if (ThisUsb2Dev.DeviceStatus >= ROOT_DEV_SUCCESS) {
            target_endp = NIZ_MOUSE_ENDP; 
            static uint8_t niz_toggle = 0;
            if(niz_toggle) target_endp |= 0x80;
            niz_toggle = !niz_toggle;
        }
    }

    if (target_endp != 0)
    {
        s = USB2HostTransact(USB_PID_IN << 4 | (target_endp & 0x7F), 
                             (target_endp & 0x80) ? (RB_UH_R_TOG | RB_UH_T_TOG) : 0, 0);

        if(s == ERR_SUCCESS)
        {
            // 更新同步位 (仅标准设备)
            if (loc != 0xFFFF) {
                target_endp ^= 0x80;
                if(len) DevOnU2HubPort[len - 1].GpVar[0] = target_endp;
                else    ThisUsb2Dev.GpVar[0] = target_endp;
            }

            len = R8_USB2_RX_LEN;
            if(len >= 3) 
            {
                uint8_t current_mouse[4] = {0}; 
                // --- 协议智能适配 ---
                if (len == 5) {
                    memcpy(current_mouse, RxBuffer + 1, 4); // NiZ: 跳过ID [ID, Btn, X, Y, W]
                } else if (len >= 7) {
                    current_mouse[0] = RxBuffer[1]; // Btn
                    current_mouse[1] = RxBuffer[2]; // X
                    current_mouse[2] = RxBuffer[4]; // Y
                    current_mouse[3] = RxBuffer[6]; // Wheel
                } else if (len == 3) {
                    memcpy(current_mouse, RxBuffer, 3); // Std [Btn, X, Y]
                } else if (len == 4) {
                    if (RxBuffer[0] <= 5) memcpy(current_mouse, RxBuffer + 1, 3);
                    else memcpy(current_mouse, RxBuffer, 4);
                }
                Show_Raw_Mouse(current_mouse,len);
                memcpy(last_mouse_report, current_mouse, 4);
                DBG_MOUSE(current_mouse);
                    
                // 发送给蓝牙
                // 注意：鼠标数据流很大，如果蓝牙忙，这里选择丢弃当前包，
                // 而不是像键盘那样阻塞等待，以保证光标流畅度。
                HidEmu_SendMouseReport(current_mouse);
            }
        }
    }
}