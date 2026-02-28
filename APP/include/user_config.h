/**
 * @file user_config.h
 * @brief 用户配置结构体头文件
 *
 * ## 架构说明
 *
 * ### 1. 模块定位
 * 本模块属于系统的配置管理层，提供统一的用户配置结构体定义和管理。
 * 主要职责：
 * - 配置结构体定义：定义统一的配置数据结构
 * - 配置参数管理：集中管理所有配置参数
 * - 配置存储管理：支持SNV Flash存储和加载
 * - 配置验证机制：提供校验和验证功能
 * - 配置恢复机制：支持恢复默认配置
 *
 * ### 2. 依赖关系
 * - 上层调用: user_config.c（配置管理实现）、usb_bridge_config.c（配置接口）
 * - 下层依赖: SNV Flash存储（配置持久化）
 * - 数据依赖: 无
 *
 * ### 3. 关键数据结构
 * - UserConfig_t: 用户配置主结构体
 * - BatteryConfig_t: 电池配置结构体
 * - ConnectionConfig_t: 连接参数结构体
 * - DeviceConfig_t: 设备配置结构体
 * - DebugConfig_t: 调试配置结构体
 * - LEDConfig_t: LED配置结构体
 *
 * ### 4. 核心算法
 * - 配置版本管理：使用版本号控制配置兼容性
 * - 校验和计算：使用简单校验和验证配置完整性
 * - SNV Flash存储：使用CH583的SNV Flash存储配置
 * - 内存优化：使用位域和紧凑结构体减少内存占用
 *
 * ## 使用指南
 *
 * ### 配置管理流程
 * 1. 初始化配置：UserConfig_Init()
 * 2. 加载配置：UserConfig_LoadFromSNV()
 * 3. 修改配置：直接修改结构体字段
 * 4. 保存配置：UserConfig_SaveToSNV()
 * 5. 验证配置：UserConfig_VerifyChecksum()
 * 6. 恢复默认：UserConfig_RestoreDefaults()
 *
 * ### 配置结构说明
 * - 配置头信息：版本号、校验和
 * - 电池配置：低电量阈值、临界电量阈值、满电阈值、校准数据
 * - 连接参数：最小间隔、最大间隔、从机延迟、超时时间
 * - 设备配置：设备名称、名称长度
 * - 调试配置：调试级别、BLE日志、USB日志
 * - LED配置：模式、闪烁间隔、亮度
 *
 * ### SNV Flash存储
 * - 存储地址：0x77E00
 * - 存储大小：256字节
 * - 存储格式：配置结构体直接存储
 * - 读写接口：tmos_snv_read/tmos_snv_write
 *
 * ### 常用函数
 * - UserConfig_Init(): 初始化配置结构体
 * - UserConfig_LoadFromSNV(): 从SNV Flash加载配置
 * - UserConfig_SaveToSNV(): 保存配置到SNV Flash
 * - UserConfig_VerifyChecksum(): 验证配置校验和
 * - UserConfig_CalculateChecksum(): 计算配置校验和
 * - UserConfig_RestoreDefaults(): 恢复默认配置
 *
 * ## 调试技巧
 * - 使用UserConfig_VerifyChecksum()检查配置完整性
 * - 使用UserConfig_LoadFromSNV()加载保存的配置
 * - 使用UserConfig_RestoreDefaults()恢复默认配置
 * - 监控配置版本号确保兼容性
 *
 * @author DIY User & AI Assistant
 * @version V1.0
 */

#ifndef __USER_CONFIG_H__
#define __USER_CONFIG_H__

#include <stdint.h>

// ===================================================================
// ? 配置版本定义
// ===================================================================

#define USER_CONFIG_VERSION     0x01    // 配置版本号
#define USER_CONFIG_SIZE        256     // 配置结构体大小（字节）
#define USER_CONFIG_SNV_ADDR    0x77E00 // SNV Flash存储地址

// ===================================================================
// ? 电池配置结构体
// ===================================================================

#define BATTERY_CALIBRATION_SIZE 10  // 电池校准数据大小

typedef struct {
    uint16_t low_voltage;       // 低电量电压阈值 (mV)
    uint16_t critical_voltage;  // 临界电量电压阈值 (mV)
    uint16_t full_voltage;      // 满电电压阈值 (mV)
    uint8_t  calibration[BATTERY_CALIBRATION_SIZE];   // 电池校准数据
} BatteryConfig_t;

// ===================================================================
// ? 连接参数结构体
// ===================================================================

typedef struct {
    uint16_t min_interval;      // 最小连接间隔 (1.25ms单位)
    uint16_t max_interval;      // 最大连接间隔 (1.25ms单位)
    uint16_t slave_latency;     // 从机延迟
    uint16_t timeout;           // 连接超时 (10ms单位)
} ConnectionConfig_t;

// ===================================================================
// ? 设备配置结构体
// ===================================================================

#define MAX_DEVICE_NAME_SIZE 32  // 设备名称最大长度

typedef struct {
    uint8_t  name[MAX_DEVICE_NAME_SIZE];          // 设备名称
    uint8_t  name_length;       // 设备名称长度
} DeviceConfig_t;

// ===================================================================
// ? 调试配置结构体
// ===================================================================

// 使用位域优化内存使用，将3个字节合并为1个字节
typedef struct {
    uint8_t  level : 3;         // 调试级别 (0=关闭, 1=错误, 2=警告, 3=信息, 4=详细)
    uint8_t  ble_log : 1;       // BLE日志开关
    uint8_t  usb_log : 1;       // USB日志开关
    uint8_t  reserved : 3;      // 保留位
} DebugConfig_t;

// ===================================================================
// ? LED配置结构体
// ===================================================================

// 优化内存使用：使用uint8_t表示间隔（单位10ms），节省1字节
typedef struct {
    uint8_t  mode;              // LED模式 (0=关闭, 1=常亮, 2=闪烁, 3=呼吸)
    uint8_t  blink_interval;    // 闪烁间隔 (单位10ms, 0-2550ms)
    uint8_t  brightness;        // 亮度 (0-100%)
} LEDConfig_t;

// ===================================================================
// ? 用户配置主结构体
// ===================================================================

typedef struct {
    // 配置头信息
    uint8_t  version;           // 配置版本
    uint8_t  checksum;          // 校验和

    // 配置数据
    BatteryConfig_t    battery;     // 电池配置
    ConnectionConfig_t connection;  // 连接参数
    DeviceConfig_t     device;      // 设备配置
    DebugConfig_t      debug;       // 调试配置
    LEDConfig_t        led;         // LED配置

    // 保留字节（确保结构体大小为256字节）
    // 计算：256 - 2(头) - 16(电池) - 8(连接) - 33(设备) - 1(调试) - 3(LED) = 193字节
    uint8_t  reserved[193];
} UserConfig_t;

// ===================================================================
// ? 函数声明
// ===================================================================

/**
 * @brief  初始化配置结构体
 * @param  config  配置结构体指针
 */
void UserConfig_Init(UserConfig_t *config);

/**
 * @brief  从SNV Flash加载配置
 * @param  config  配置结构体指针
 * @return 0=成功, 1=失败
 */
uint8_t UserConfig_LoadFromSNV(UserConfig_t *config);

/**
 * @brief  保存配置到SNV Flash
 * @param  config  配置结构体指针
 * @return 0=成功, 1=失败
 */
uint8_t UserConfig_SaveToSNV(UserConfig_t *config);

/**
 * @brief  验证配置校验和
 * @param  config  配置结构体指针
 * @return 0=校验通过, 1=校验失败
 */
uint8_t UserConfig_VerifyChecksum(UserConfig_t *config);

/**
 * @brief  计算配置校验和
 * @param  config  配置结构体指针
 * @return 校验和值
 */
uint8_t UserConfig_CalculateChecksum(UserConfig_t *config);

/**
 * @brief  恢复默认配置
 * @param  config  配置结构体指针
 */
void UserConfig_RestoreDefaults(UserConfig_t *config);


// ===================================================================
// ? 默认配置值定义
// ===================================================================

// 电池默认配置
#define DEFAULT_BATT_LOW_VOLTAGE      3300    // 3.3V
#define DEFAULT_BATT_CRITICAL_VOLTAGE 3000    // 3.0V
#define DEFAULT_BATT_FULL_VOLTAGE     4200    // 4.2V

// 连接参数默认配置
#define DEFAULT_CONN_MIN_INTERVAL     6       // 7.5ms (6 * 1.25ms)
#define DEFAULT_CONN_MAX_INTERVAL     32      // 40ms (32 * 1.25ms)
#define DEFAULT_CONN_SLAVE_LATENCY    0       // 无延迟
#define DEFAULT_CONN_TIMEOUT          500     // 5s (500 * 10ms)

// 设备名称默认配置
#define DEFAULT_DEVICE_NAME           "ENJOU BLE Adapter"
#define DEFAULT_DEVICE_NAME_LENGTH    18

// 调试配置默认配置
#define DEFAULT_DEBUG_LEVEL           2       // 调试级别：警告
#define DEFAULT_DEBUG_BLE_LOG         1       // BLE日志：开启
#define DEFAULT_DEBUG_USB_LOG         1       // USB日志：开启

// LED配置默认配置
#define DEFAULT_LED_MODE              2       // 闪烁模式
#define DEFAULT_LED_BLINK_INTERVAL    50      // 500ms (50 * 10ms)
#define DEFAULT_LED_BRIGHTNESS        50      // 50%亮度

#endif /* __USER_CONFIG_H__ */