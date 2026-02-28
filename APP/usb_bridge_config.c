/**
 * @file usb_bridge_config.c
 * @brief USB Bridge 配置管理实现
 *
 * ## 架构说明
 *
 * ### 1. 模块定位
 * 本模块属于系统的配置接口层，提供USB桥接器的配置管理功能。
 * 主要职责：
 * - 配置系统初始化
 * - 配置参数应用
 * - 配置保存和加载
 * - 配置导入和导出
 * - 配置恢复和测试
 *
 * ### 2. 依赖关系
 * - 上层调用: usb_bridge.c（配置初始化和应用）
 * - 下层依赖:
 *   - user_config.c（配置管理）
 *   - usb_host_common.c（USB主机通用函数）
 *   - debug.c（调试日志）
 * - 数据依赖: UserConfig_t结构体定义
 *
 * ### 3. 关键数据结构
 * - UserConfig_t: 用户配置主结构体（在user_config.h中定义）
 * - g_user_config: 全局配置实例，存储当前系统配置
 *
 * ### 4. 核心算法
 * - 配置初始化: 从Flash加载配置或使用默认配置
 * - 配置应用: 将配置参数应用到系统各个模块
 * - 配置验证: 版本检查和校验和验证
 * - 配置备份: 导出和导入配置数据
 *
 * ## 使用指南
 *
 * ### 初始化流程
 * 1. 调用USB_Bridge_ConfigInit()初始化配置系统
 * 2. 系统会自动从Flash加载配置或使用默认配置
 * 3. 配置会自动应用到系统各个模块
 *
 * ### 配置管理流程
 * 1. 修改配置参数（通过配置接口）
 * 2. 调用USB_Bridge_SaveConfig()保存到Flash
 * 3. 系统重启后自动加载配置
 *
 * ### 常用函数
 * - USB_Bridge_ConfigInit(): 初始化配置系统
 * - USB_Bridge_ApplyConfig(): 应用配置到系统
 * - USB_Bridge_GetConfig(): 获取配置结构体指针
 * - USB_Bridge_SaveConfig(): 保存配置到Flash
 * - USB_Bridge_ExportConfig(): 导出配置到缓冲区
 * - USB_Bridge_ImportConfig(): 从缓冲区导入配置
 * - USB_Bridge_RestoreDefaults(): 恢复默认配置
 * - USB_Bridge_TestConfigStorage(): 测试配置存储功能
 *
 * ## 调试技巧
 * - 使用LOG_SYS()查看配置操作日志
 * - 检查配置版本号和校验和
 * - 监控配置保存和加载操作
 * - 使用测试函数验证配置存储功能
 *
 * @author DIY User & AI Assistant
 * @version V1.0
 */

#include "usb_host_common.h"
#include "user_config.h"
#include "debug.h"

// ===================================================================
// ? 配置实例
// ===================================================================

static UserConfig_t g_user_config;

// ===================================================================
// ? 配置管理函数
// ===================================================================

/**
 * @brief  初始化配置系统
 *
 * @details
 * #### 功能详细说明
 * 此函数初始化USB桥接器的配置系统。它会初始化配置结构体，
 * 尝试从SNV Flash加载配置，如果加载失败则使用默认配置，
 * 最后将配置应用到系统各个模块。
 *
 * #### 算法原理
 * 1. **初始化配置结构体**：调用UserConfig_Init()初始化全局配置实例
 * 2. **加载配置**：调用UserConfig_LoadFromSNV()从Flash加载配置
 * 3. **加载失败处理**：如果加载失败，输出日志并使用默认配置
 * 4. **应用配置**：调用USB_Bridge_ApplyConfig()将配置应用到系统
 *
 * #### 调用示例
 * 系统启动时调用，初始化配置系统。
 *
 * #### 使用场景
 * - 系统启动时调用
 * - 系统复位后调用
 * - 需要重新初始化配置时调用
 *
 * #### 错误处理
 * - Flash加载失败：使用默认配置
 * - 配置验证失败：使用默认配置
 * - 配置版本不匹配：使用默认配置
 *
 * #### 边界情况
 * - 首次启动：Flash中无配置，使用默认配置
 * - Flash损坏：使用默认配置
 * - 配置损坏：使用默认配置
 *
 * #### 注意事项
 * - 此函数应该在系统启动时调用一次
 * - 调用此函数后，配置系统准备就绪
 * - 配置会自动应用到系统各个模块
 * - 配置保存需要调用USB_Bridge_SaveConfig()
 */
void USB_Bridge_ConfigInit(void)
{
    // 初始化配置结构体
    UserConfig_Init(&g_user_config);

    // 尝试从SNV Flash加载配置
    if (UserConfig_LoadFromSNV(&g_user_config) != 0) {
        LOG_SYS("Failed to load config from SNV, using defaults\n");
    }

    // 应用配置到系统
    USB_Bridge_ApplyConfig();
}

/**
 * @brief  应用配置到系统
 */
void USB_Bridge_ApplyConfig(void)
{
    // 应用调试配置
    if (g_user_config.debug.ble_log) {
        // 启用BLE日志
        // 这里可以根据需要设置日志级别
    }

    if (g_user_config.debug.usb_log) {
        // 启用USB日志
        // 这里可以根据需要设置日志级别
    }

    LOG_SYS("Configuration applied\n");
}

/**
 * @brief  获取配置结构体指针
 * @return 配置结构体指针
 */
UserConfig_t* USB_Bridge_GetConfig(void)
{
    return &g_user_config;
}

/**
 * @brief  保存当前配置到SNV Flash
 * @return 0=成功, 1=失败
 */
uint8_t USB_Bridge_SaveConfig(void)
{
    return UserConfig_SaveToSNV(&g_user_config);
}

/**
 * @brief  导出配置到缓冲区
 * @param  buffer  目标缓冲区
 * @param  size    缓冲区大小
 * @return 0=成功, 1=失败
 */
uint8_t USB_Bridge_ExportConfig(uint8_t *buffer, uint16_t size)
{
    if (buffer == NULL || size < sizeof(UserConfig_t)) {
        return 1;
    }

    memcpy(buffer, &g_user_config, sizeof(UserConfig_t));
    return 0;
}

/**
 * @brief  从缓冲区导入配置
 * @param  buffer  源缓冲区
 * @param  size    缓冲区大小
 * @return 0=成功, 1=失败
 */
uint8_t USB_Bridge_ImportConfig(const uint8_t *buffer, uint16_t size)
{
    if (buffer == NULL || size < sizeof(UserConfig_t)) {
        return 1;
    }

    memcpy(&g_user_config, buffer, sizeof(UserConfig_t));

    // 验证导入的配置
    if (g_user_config.version != USER_CONFIG_VERSION) {
        LOG_SYS("Imported config version mismatch\n");
        return 1;
    }

    if (UserConfig_VerifyChecksum(&g_user_config) != 0) {
        LOG_SYS("Imported config checksum invalid\n");
        return 1;
    }

    // 应用导入的配置
    USB_Bridge_ApplyConfig();

    LOG_SYS("Config imported successfully\n");
    return 0;
}

/**
 * @brief  恢复默认配置
 * @return 0=成功, 1=失败
 */
uint8_t USB_Bridge_RestoreDefaults(void)
{
    UserConfig_RestoreDefaults(&g_user_config);
    USB_Bridge_ApplyConfig();

    LOG_SYS("Default configuration restored\n");
    return 0;
}

/**
 * @brief  测试配置存储功能
 * @return 0=成功, 1=失败
 */
uint8_t USB_Bridge_TestConfigStorage(void)
{
    UserConfig_t test_config;
    uint8_t result;

    LOG_SYS("Testing configuration storage...\n");

    // 1. 初始化测试配置
    UserConfig_Init(&test_config);

    // 2. 修改一些配置值
    test_config.debug.level = 3;
    test_config.debug.ble_log = 1;
    test_config.debug.usb_log = 1;
    test_config.led.mode = 2;
    test_config.led.blink_interval = 30; // 300ms

    // 3. 保存到SNV Flash
    result = UserConfig_SaveToSNV(&test_config);
    if (result != 0) {
        LOG_SYS("Failed to save test config to SNV\n");
        return 1;
    }

    // 4. 从SNV Flash加载
    UserConfig_t loaded_config;
    result = UserConfig_LoadFromSNV(&loaded_config);
    if (result != 0) {
        LOG_SYS("Failed to load test config from SNV\n");
        return 1;
    }

    // 5. 验证加载的配置
    if (loaded_config.debug.level != test_config.debug.level ||
        loaded_config.debug.ble_log != test_config.debug.ble_log ||
        loaded_config.debug.usb_log != test_config.debug.usb_log ||
        loaded_config.led.mode != test_config.led.mode ||
        loaded_config.led.blink_interval != test_config.led.blink_interval) {
        LOG_SYS("Loaded config does not match saved config\n");
        return 1;
    }

    LOG_SYS("Configuration storage test passed\n");
    return 0;
}