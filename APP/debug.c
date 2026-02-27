#include "CH58x_common.h" // 确保能找到 PRINT 定义
#ifdef DEBUG_KEY
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

#ifdef DEBUG_MOUSE
/**
 * @brief  调试打印：显示原始鼠标数据
 */
void Show_Raw_Mouse(uint8_t *buf, uint8_t len) {
    #if (defined(DEBUG_MOUSE) && DEBUG_MOUSE == 1)
    PRINT("RAW [%d]: ", len);
    for(int i=0; i<len; i++) PRINT("%02X ", buf[i]);
    PRINT("\n");
    #endif
}

/**
 * @brief  调试打印：显示鼠标坐标和按键
 */
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
