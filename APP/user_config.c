/**
 * @file user_config.c
 * @brief 用户配置实现文件
 *
 * ## 架构说明
 *
 * ### 1. 模块定位
 * 本模块属于系统的配置管理层，负责管理所有用户配置参数。
 * 主要职责：
 * - 配置结构体初始化
 * - 配置参数验证
 * - SNV Flash存储管理
 * - 配置版本管理
 * - 配置恢复和备份
 *
 * ### 2. 依赖关系
 * - 上层调用: usb_bridge.c（配置读取）、usb_bridge_config.c（配置接口）
 * - 下层依赖:
 *   - CH58xBLE_LIB.h（SNV Flash读写函数）
 *   - debug.c（调试日志）
 * - 数据依赖: UserConfig_t结构体定义
 *
 * ### 3. 关键数据结构
 * - UserConfig_t: 用户配置主结构体
 *   - version: 配置版本号
 *   - checksum: 配置校验和
 *   - battery: 电池配置（低电压、临界电压、满电电压、校准数据）
 *   - connection: 连接参数（最小间隔、最大间隔、从机延迟、超时）
 *   - device: 设备配置（名称、名称长度）
 *   - debug: 调试配置（级别、BLE日志、USB日志）
 *   - led: LED配置（模式、闪烁间隔、亮度）
 *   - reserved: 保留字节（确保结构体大小为256字节）
 *
 * ### 4. 核心算法
 * - 校验和算法: 使用字节和取反作为校验和
 * - 版本管理: 通过版本号确保配置兼容性
 * - SNV存储: 使用CH583的SNV Flash存储配置
 * - 配置验证: 版本检查 + 校验和验证
 * - 默认配置: 提供完整的默认配置值
 *
 * ## 使用指南
 *
 * ### 初始化流程
 * 1. 调用UserConfig_Init()初始化配置结构体
 * 2. 调用UserConfig_LoadFromSNV()从Flash加载配置
 * 3. 如果加载失败，调用UserConfig_RestoreDefaults()恢复默认配置
 *
 * ### 配置管理流程
 * 1. 修改配置参数
 * 2. 调用UserConfig_SaveToSNV()保存到Flash
 * 3. 系统重启后自动加载配置
 *
 * ### 常用函数
 * - UserConfig_Init(): 初始化配置结构体
 * - UserConfig_LoadFromSNV(): 从Flash加载配置
 * - UserConfig_SaveToSNV(): 保存配置到Flash
 * - UserConfig_VerifyChecksum(): 验证配置完整性
 * - UserConfig_RestoreDefaults(): 恢复默认配置
 * - UserConfig_GetDeviceName(): 获取设备名称
 * - UserConfig_SetDeviceName(): 设置设备名称
 *
 * ## 调试技巧
 * - 使用LOG_SYS()查看配置加载和保存日志
 * - 检查配置版本号和校验和
 * - 监控SNV Flash读写操作
 * - 验证配置参数的合理性
 *
 * @author DIY User & AI Assistant
 * @version V1.0
 */

#include "user_config.h"
#include "CH58xBLE_LIB.h"
#include "CH58x_common.h"
#include "debug.h"

// ===================================================================
// ? 外部函数声明
// ===================================================================

extern uint8_t tmos_snv_read(uint8_t id, uint8_t len, uint8_t *pBuf);
extern uint8_t tmos_snv_write(uint8_t id, uint8_t len, uint8_t *pBuf);

// ===================================================================
// ? 配置函数实现
// ===================================================================

/**
 * @brief  初始化配置结构体
 *
 * @param  config  配置结构体指针 - 指向UserConfig_t结构体的指针
 *
 * @details
 * #### 功能详细说明
 * 此函数初始化配置结构体，设置默认值并计算校验和。
 * 配置结构体用于存储所有用户配置参数，包括电池、连接、设备、调试和LED配置。
 *
 * #### 算法原理
 * 1. **参数检查**：检查配置指针是否为NULL
 * 2. **内存清零**：使用memset将配置结构体全部清零
 * 3. **设置版本号**：将版本号设置为USER_CONFIG_VERSION
 * 4. **设置默认配置**：调用UserConfig_RestoreDefaults()设置默认值
 * 5. **计算校验和**：调用UserConfig_CalculateChecksum()计算初始校验和
 *
 * #### 调用示例
 * 创建配置结构体并初始化。
 *
 * #### 使用场景
 * - 系统启动时调用
 * - 需要创建新配置时调用
 * - 配置重置时调用
 *
 * #### 错误处理
 * - 配置指针为NULL：函数直接返回，不执行操作
 * - 内存不足：memset会自动处理
 *
 * #### 边界情况
 * - 配置指针为NULL：安全返回
 * - 多次调用：每次调用都会重置配置
 *
 * #### 注意事项
 * - 此函数会覆盖配置结构体的所有内容
 * - 调用此函数后，配置结构体包含默认配置
 * - 需要调用UserConfig_SaveToSNV()保存到Flash
 * - 配置版本号必须与USER_CONFIG_VERSION一致
 */
void UserConfig_Init(UserConfig_t *config)
{
    if (config == NULL) return;

    // 清零配置结构体
    memset(config, 0, sizeof(UserConfig_t));

    // 设置版本号
    config->version = USER_CONFIG_VERSION;

    // 设置默认配置
    UserConfig_RestoreDefaults(config);

    // 计算初始校验和
    config->checksum = UserConfig_CalculateChecksum(config);
}

/**
 * @brief  从SNV Flash加载配置
 *
 * @param  config  配置结构体指针 - 指向UserConfig_t结构体的指针
 *
 * @return 0=成功, 1=失败
 *
 * @details
 * #### 功能详细说明
 * 此函数从SNV Flash存储器中加载用户配置。如果加载失败或配置无效，
 * 会自动恢复为默认配置。
 *
 * #### 算法原理
 * 1. **参数检查**：检查配置指针是否为NULL
 * 2. **读取Flash**：调用tmos_snv_read()从SNV Flash读取配置
 * 3. **读取失败处理**：如果读取失败，恢复默认配置并返回1
 * 4. **版本验证**：检查配置版本号是否匹配
 * 5. **校验和验证**：检查配置校验和是否有效
 * 6. **验证失败处理**：如果验证失败，恢复默认配置并返回1
 * 7. **成功返回**：配置加载成功，返回0
 *
 * #### 调用示例
 * 从Flash加载配置，如果失败则使用默认配置。
 *
 * #### 使用场景
 * - 系统启动时调用
 * - 需要读取保存的配置时调用
 * - 配置恢复时调用
 *
 * #### 错误处理
 * - 配置指针为NULL：返回1（失败）
 * - Flash读取失败：恢复默认配置，返回1
 * - 版本不匹配：恢复默认配置，返回1
 * - 校验和无效：恢复默认配置，返回1
 *
 * #### 边界情况
 * - 首次启动：Flash中无配置，恢复默认配置
 * - Flash损坏：恢复默认配置
 * - 版本升级：版本不匹配时恢复默认配置
 *
 * #### 注意事项
 * - 此函数会修改配置结构体的内容
 * - 如果Flash中无有效配置，会使用默认配置
 * - 配置版本号必须与USER_CONFIG_VERSION一致
 * - 调用此函数后，配置结构体包含有效配置
 */
uint8_t UserConfig_LoadFromSNV(UserConfig_t *config)
{
    if (config == NULL) return 1;

    // 从SNV Flash读取配置
    uint8_t result = tmos_snv_read(USER_CONFIG_SNV_ADDR, sizeof(UserConfig_t), (uint8_t *)config);

    if (result != 0) {
        LOG_SYS("Failed to load config from SNV, using defaults\n");
        UserConfig_RestoreDefaults(config);
        return 1;
    }

    // 验证配置版本
    if (config->version != USER_CONFIG_VERSION) {
        LOG_SYS("Config version mismatch, using defaults\n");
        UserConfig_RestoreDefaults(config);
        return 1;
    }

    // 验证校验和
    if (UserConfig_VerifyChecksum(config) != 0) {
        LOG_SYS("Config checksum invalid, using defaults\n");
        UserConfig_RestoreDefaults(config);
        return 1;
    }

    LOG_SYS("Config loaded from SNV successfully\n");
    return 0;
}

/**
 * @brief  保存配置到SNV Flash
 * @param  config  配置结构体指针
 * @return 0=成功, 1=失败
 */
uint8_t UserConfig_SaveToSNV(UserConfig_t *config)
{
    if (config == NULL) return 1;

    // 更新校验和
    config->checksum = UserConfig_CalculateChecksum(config);

    // 写入SNV Flash
    uint8_t result = tmos_snv_write(USER_CONFIG_SNV_ADDR, sizeof(UserConfig_t), (uint8_t *)config);

    if (result != 0) {
        LOG_SYS("Failed to save config to SNV\n");
        return 1;
    }

    LOG_SYS("Config saved to SNV successfully\n");
    return 0;
}

/**
 * @brief  验证配置校验和
 * @param  config  配置结构体指针
 * @return 0=校验通过, 1=校验失败
 */
uint8_t UserConfig_VerifyChecksum(UserConfig_t *config)
{
    if (config == NULL) return 1;

    uint8_t calculated_checksum = UserConfig_CalculateChecksum(config);
    return (calculated_checksum == config->checksum) ? 0 : 1;
}

/**
 * @brief  计算配置校验和
 * @param  config  配置结构体指针
 * @return 校验和
 */
uint8_t UserConfig_CalculateChecksum(UserConfig_t *config)
{
    if (config == NULL) return 0;

    uint8_t checksum = 0;
    uint8_t *data = (uint8_t *)config;

    // 计算除校验和字节外的所有字节的和
    for (uint16_t i = 0; i < sizeof(UserConfig_t) - 1; i++) {
        checksum += data[i];
    }

    // 取反作为校验和
    return ~checksum;
}

/**
 * @brief  恢复默认配置
 * @param  config  配置结构体指针
 */
void UserConfig_RestoreDefaults(UserConfig_t *config)
{
    if (config == NULL) return;

    // 电池默认配置
    config->battery.low_voltage = DEFAULT_BATT_LOW_VOLTAGE;
    config->battery.critical_voltage = DEFAULT_BATT_CRITICAL_VOLTAGE;
    config->battery.full_voltage = DEFAULT_BATT_FULL_VOLTAGE;
    memset(config->battery.calibration, 0, sizeof(config->battery.calibration));

    // 连接参数默认配置
    config->connection.min_interval = DEFAULT_CONN_MIN_INTERVAL;
    config->connection.max_interval = DEFAULT_CONN_MAX_INTERVAL;
    config->connection.slave_latency = DEFAULT_CONN_SLAVE_LATENCY;
    config->connection.timeout = DEFAULT_CONN_TIMEOUT;

    // 设备名称默认配置
    memcpy(config->device.name, DEFAULT_DEVICE_NAME, DEFAULT_DEVICE_NAME_LENGTH);
    config->device.name_length = DEFAULT_DEVICE_NAME_LENGTH;

    // 调试配置默认配置
    config->debug.level = DEFAULT_DEBUG_LEVEL;
    config->debug.ble_log = DEFAULT_DEBUG_BLE_LOG;
    config->debug.usb_log = DEFAULT_DEBUG_USB_LOG;

    // LED配置默认配置
    config->led.mode = DEFAULT_LED_MODE;
    config->led.blink_interval = DEFAULT_LED_BLINK_INTERVAL; // 单位10ms
    config->led.brightness = DEFAULT_LED_BRIGHTNESS;

    // 设置版本号
    config->version = USER_CONFIG_VERSION;

    // 计算校验和
    config->checksum = UserConfig_CalculateChecksum(config);

    LOG_SYS("Default configuration restored\n");
}

// ===================================================================
// ? 配置访问函数
// ===================================================================

/**
 * @brief  获取设备名称
 * @param  config  配置结构体指针
 * @return 设备名称字符串
 */
const char* UserConfig_GetDeviceName(UserConfig_t *config)
{
    if (config == NULL) return DEFAULT_DEVICE_NAME;
    return (const char*)config->device.name;
}

/**
 * @brief  获取设备名称长度
 * @param  config  配置结构体指针
 * @return 设备名称长度
 */
uint8_t UserConfig_GetDeviceNameLength(UserConfig_t *config)
{
    if (config == NULL) return DEFAULT_DEVICE_NAME_LENGTH;
    return config->device.name_length;
}

/**
 * @brief  设置设备名称
 * @param  config  配置结构体指针
 * @param  name    设备名称字符串
 * @param  length  设备名称长度
 */
void UserConfig_SetDeviceName(UserConfig_t *config, const char *name, uint8_t length)
{
    if (config == NULL || name == NULL) return;

    // 限制最大长度
    if (length > 31) length = 31;

    memcpy(config->device.name, name, length);
    config->device.name_length = length;

    // 更新校验和
    config->checksum = UserConfig_CalculateChecksum(config);
}