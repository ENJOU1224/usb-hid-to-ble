/*********************************************************************
 * File Name          : usb_bridge.c
 * Author             : DIY User & AI Assistant
 * Version            : V18.0 (Refactored & Optimized)
 * Description        : USB Host 转 Bluetooth 核心桥接逻辑
 *                      - 统一了标准鼠标与非标准(NiZ)鼠标的处理逻辑
 *                      - 优化了 DATA0/DATA1 同步位的管理方式
 *********************************************************************/

#include "CH58xBLE_LIB.H"
#include "CH58x_common.h"
#include "debug.h"

// ===================================================================
// ? 用户配置区 (User Configuration)
// ===================================================================

// NiZ 键盘特殊参数
#define NIZ_KEY_OFFSET    4       // 键码偏移补偿
#define NIZ_MOUSE_ENDP    0x84    // 强制指定的鼠标端点 (Interface 2)

// ===================================================================
// ? 全局变量与缓冲区
// ===================================================================

// --- USB 缓冲区 (必须 4 字节对齐) ---
__attribute__((aligned(4))) uint8_t RxBuffer[MAX_PACKET_SIZE]; 
__attribute__((aligned(4))) uint8_t TxBuffer[MAX_PACKET_SIZE]; 

// --- 状态标志 ---
volatile uint8_t Bridge_NewDevFlag = 0; // 新设备插入事件标志

// --- 键盘状态 ---
static uint8_t  last_kbd_report[8] = {0}; // 键盘上次数据(去重用)
static uint8_t  kbd_send_pending = 0;     // 键盘流控重发标志

// --- 鼠标状态 ---
static uint8_t  last_mouse_report[4] = {0}; // 鼠标上次数据(去重用)

// [优化] NiZ 鼠标专用同步记录变量
// 格式说明：Bit7=同步位(0=DATA0, 1=DATA1), Bit0-6=端点号
// 初始化为 0x04 (即端点4, 期望DATA0)
static uint8_t  Var_NizMouse_Record = (NIZ_MOUSE_ENDP & 0x7F); 

// ===================================================================
// ? 外部函数引用
// ===================================================================
extern uint8_t HidEmu_SendUSBReport(uint8_t *pData);
extern uint8_t HidEmu_SendMouseReport(uint8_t *pData);
extern uint8_t InitRootU2Device(void);
extern uint8_t AnalyzeRootU2Hub(void);
extern uint8_t EnumAllU2HubPort(void);
extern uint16_t U2SearchTypeDevice(uint8_t type);
extern void SelectU2HubPort(uint8_t hub_port);


// ===================================================================
// ?? 辅助函数：数据解析与调试
// ===================================================================



/**
 * @brief  键盘数据解析 (兼容标准6键与NKRO)
 * @param  in_buf   USB接收到的原始数据
 * @param  len      数据长度
 * @param  out_buf  输出的标准8字节 HID 报文
 */
void Parse_Keyboard_Data(uint8_t* in_buf, uint8_t len, uint8_t* out_buf) {
    memset(out_buf, 0, 8);
    
    // 情况1: 标准 8 字节 Boot Keyboard 报文
    if (len == 8) {
        memcpy(out_buf, in_buf, 8);
        return;
    }
    
    // 情况2: NiZ 等 NKRO 变长报文 (位图转标准键码)
    if (len > 8) {
        out_buf[0] = in_buf[0]; // 复制修饰键 (Ctrl/Shift/Alt/Win)
        int key_slot = 0;
        
        // 遍历位图数据 (从第2字节开始)
        for (int i = 2; i < len; i++) {
            if (in_buf[i] != 0) {
                for (int bit = 0; bit < 8; bit++) {
                    if ((in_buf[i] >> bit) & 0x01) {
                        // 计算键码并加上偏移量
                        uint8_t keycode = (i - 2) * 8 + bit + NIZ_KEY_OFFSET;
                        
                        // 填充到 6 个按键槽位中
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
// ? 核心逻辑
// ===================================================================

void USB_Bridge_Init(void) {
    // 1. 硬件 IO 初始化 (开启 USB 供电)
    GPIOA_SetBits(GPIO_Pin_9);
    GPIOA_ModeCfg(GPIO_Pin_9, GPIO_ModeOut_PP_5mA);
    
    // 2. 绑定 USB RAM
    pU2HOST_RX_RAM_Addr = RxBuffer;
    pU2HOST_TX_RAM_Addr = TxBuffer;
    
    // 3. 初始化协议栈
    USB2_HostInit();
    
    Bridge_NewDevFlag = 0;
    kbd_send_pending = 0;
    
    // 初始化 NiZ 记录变量 (清除 DATA1 标志，只保留端点号)
    Var_NizMouse_Record = (NIZ_MOUSE_ENDP & 0x7F);
    
    LOG_SYS("USB Init OK. Bridge Ready.\n");
}

void USB_Bridge_Poll(void) {
    uint8_t s, len, endp_addr;
    uint16_t search_res;
    
    // --------------------------------------------------------
    // [任务 0] 键盘流控处理
    // 如果上次发送失败（蓝牙忙），优先重试，保证按键不丢失
    // --------------------------------------------------------
    if (kbd_send_pending) {
        if (HidEmu_SendUSBReport(last_kbd_report) == SUCCESS) {
            kbd_send_pending = 0; // 发送成功，清除标志
            LOG_BLE("KBD Resend OK\n");
        } else {
            return; // 依然忙，暂停本轮处理
        }
    }

    // --------------------------------------------------------
    // [任务 1] 硬件插拔检测与设备枚举
    // --------------------------------------------------------
    if(R8_USB2_INT_FG & RB_UIF_DETECT) {
        R8_USB2_INT_FG = RB_UIF_DETECT; // 清中断
        s = AnalyzeRootU2Hub();
        if(s == ERR_USB_CONNECT) Bridge_NewDevFlag = 1;
        else if (s == ERR_USB_DISCON) Bridge_NewDevFlag = 0;
    }
    // 防止其他杂项中断卡死
    else if (R8_USB2_INT_FG) { 
        R8_USB2_INT_FG = 0xFF; 
    }

    // 处理新设备插入
    if(Bridge_NewDevFlag) {
        Bridge_NewDevFlag = 0;
        mDelaymS(200); // 等待设备电源稳定
        s = InitRootU2Device();
        if(s == ERR_SUCCESS){
            LOG_SYS("Device Enum OK\n");
            // 【重要】设备重新插入后，必须重置 NiZ 鼠标的同步位
            // 恢复为 0x04 (Bit7=0 表示下次期望 DATA0)
            Var_NizMouse_Record = (NIZ_MOUSE_ENDP & 0x7F);
        } 
    }

    // 周期性维护 HUB 状态 (如果有 HUB)
    EnumAllU2HubPort(); 

    // =================================================================
    // [任务 2] 读取键盘数据
    // =================================================================
    search_res = U2SearchTypeDevice(DEV_TYPE_KEYBOARD);
    if(search_res != 0xFFFF)
    {
        uint8_t dev_addr = (uint8_t)(search_res >> 8); // HUB端口号
        len = (uint8_t)search_res;                     // 接口号
        SelectU2HubPort(len); 
        
        // 获取官方库维护的端点记录
        endp_addr = len ? DevOnU2HubPort[len - 1].GpVar[0] : ThisUsb2Dev.GpVar[0];

        // 只有端点有效才通讯
        if(endp_addr & USB_ENDP_ADDR_MASK)
        {
            // 执行 IN 事务
            // 根据端点变量的 Bit7 自动决定是发 DATA0 还是 DATA1
            s = USB2HostTransact(USB_PID_IN << 4 | (endp_addr & 0x7F), 
                                 (endp_addr & 0x80) ? (RB_UH_R_TOG | RB_UH_T_TOG) : 0, 0);

            if(s == ERR_SUCCESS)
            {
                // 成功后翻转同步位 (Bit7)
                endp_addr ^= 0x80;
                // 写回官方库结构体
                if(len) DevOnU2HubPort[len - 1].GpVar[0] = endp_addr;
                else    ThisUsb2Dev.GpVar[0] = endp_addr;

                len = R8_USB2_RX_LEN;
                if(len > 0) 
                {
                    uint8_t temp_report[8] = {0};
                    Parse_Keyboard_Data(RxBuffer, len, temp_report);
                    DBG_KEYS(temp_report);
                    
                    memcpy(last_kbd_report, temp_report, 8);
                    
                    // 发送给蓝牙
                    if (HidEmu_SendUSBReport(last_kbd_report) != SUCCESS) {
                        kbd_send_pending = 1; // 标记待重发
                    }
                }
            }
        }
    }

    // =================================================================
    // [任务 3] 读取鼠标数据 (统一逻辑优化版)
    // =================================================================
    
    // 定义一个指针，指向“我们要操作的那个端点记录变量”
    // 无论是官方库管理的标准鼠标，还是我们自己管理的 NiZ 鼠标，都通过这个指针操作
    uint8_t *p_mouse_toggle_record = NULL; 
    uint8_t current_record_val = 0;

    // A. 尝试寻找标准鼠标
    search_res = U2SearchTypeDevice(DEV_TYPE_MOUSE);
    if (search_res != 0xFFFF) {
        uint8_t hub_port = (uint8_t)search_res; 
        SelectU2HubPort(hub_port); // 物理选中端口
        
        // 指向官方库结构体中的变量
        if (hub_port) p_mouse_toggle_record = &DevOnU2HubPort[hub_port - 1].GpVar[0];
        else          p_mouse_toggle_record = &ThisUsb2Dev.GpVar[0];
    } 
    // B. 如果没找到标准鼠标，且设备已枚举，尝试强制读取 NiZ 接口
    else if (ThisUsb2Dev.DeviceStatus >= ROOT_DEV_SUCCESS) {
        SelectU2HubPort(0); // 【关键】确保操作对象是根端口设备
        // 指向我们自己定义的全局变量
        p_mouse_toggle_record = &Var_NizMouse_Record;
    }

    // 如果确定了目标，开始传输
    if (p_mouse_toggle_record != NULL)
    {
        current_record_val = *p_mouse_toggle_record;
        
        // 如果端点号为0，说明设备未正确初始化，跳过
        if ((current_record_val & 0x7F) != 0) {
            
            // 计算 DATA0/1 标志
            // Bit7 == 1 -> RB_UH_R_TOG | RB_UH_T_TOG (发送DATA1)
            // Bit7 == 0 -> 0                         (发送DATA0)
            uint8_t token_pid = (current_record_val & 0x80) ? (RB_UH_R_TOG | RB_UH_T_TOG) : 0;
            
            // 发起传输
            s = USB2HostTransact(USB_PID_IN << 4 | (current_record_val & 0x7F), token_pid, 0);

            if(s == ERR_SUCCESS)
            {
                // 1. 更新同步位：直接翻转内存中的 Bit7
                // 这样下一次循环读取时，就会自动切换到相反的 DATA 状态
                *p_mouse_toggle_record ^= 0x80;

                // 2. 解析数据
                len = R8_USB2_RX_LEN;
                if(len >= 3) 
                {
                    uint8_t mouse_data[4] = {0}; 
                    
                    // --- 协议适配区 ---
                    if (len == 5) {
                        // NiZ 格式: [ID, Btn, X, Y, Wheel] -> 偏移1字节
                        memcpy(mouse_data, RxBuffer + 1, 4); 
                    } else if (len >= 7) {
                        // 复杂鼠标格式
                        mouse_data[0] = RxBuffer[1]; // Btn
                        mouse_data[1] = RxBuffer[2]; // X
                        mouse_data[2] = RxBuffer[4]; // Y
                        mouse_data[3] = RxBuffer[6]; // Wheel
                    } else if (len == 3) {
                        // 标准基础格式: [Btn, X, Y]
                        memcpy(mouse_data, RxBuffer, 3); 
                    } else if (len == 4) {
                        // 某些带 ID 的 4 字节格式
                        if (RxBuffer[0] <= 5) memcpy(mouse_data, RxBuffer + 1, 3);
                        else memcpy(mouse_data, RxBuffer, 4);
                    }

                    // --- 发送处理 ---
                    DBG_MOUSE(mouse_data);
                    
                    // 鼠标数据量大，不需要重发机制，蓝牙忙则丢弃以保证实时性
                    HidEmu_SendMouseReport(mouse_data);
                }
            }
        }
    }
}