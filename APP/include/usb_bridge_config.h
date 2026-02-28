/**
 * @file usb_bridge_config.h
 * @brief USB Bridge 配置管理头文件
 *
 * ## 架构说明
 *
 * ### 1. 模块定位
 * 本模块属于系统的配置接口层，提供USB桥接器的配置管理功能。
 * 主要职责：
 * - 配置系统初始化：初始化配置管理系统
 * - 配置应用：将配置参数应用到系统各个模块
 * - 配置获取：获取当前配置结构体
 * - 配置保存：将配置保存到SNV Flash
 * - 配置导入导出：支持配置数据的导入和导出
 * - 配置恢复：恢复默认配置
 * - 配置测试：测试配置存储功能
 *
 * ### 2. 依赖关系
 * - 上层调用: usb_bridge.c（主桥接逻辑）
 * - 下层依赖: user_config.c（配置管理实现）
 * - 数据依赖: UserConfig_t结构体定义
 *
 * ### 3. 关键数据结构
 * - UserConfig_t: 用户配置主结构体
 * - 配置管理接口：提供统一的配置操作接口
 *
 * ### 4. 核心算法
 * - 配置初始化：从SNV Flash加载配置或使用默认配置
 * - 配置应用：将配置参数应用到系统各个模块
 * - 配置验证：版本检查和校验和验证
 * - 配置备份：导出和导入配置数据
 *
 * ## 使用指南
 *
 * ### 配置管理流程
 * 1. 初始化配置系统：USB_Bridge_ConfigInit()
 * 2. 系统自动从Flash加载配置或使用默认配置
 * 3. 配置自动应用到系统各个模块
 * 4. 修改配置参数（通过配置接口）
 * 5. 调用USB_Bridge_SaveConfig()保存到Flash
 * 6. 系统重启后自动加载配置
 *
 * ### 配置管理函数
 * - USB_Bridge_ConfigInit(): 初始化配置系统
 * - USB_Bridge_ApplyConfig(): 应用配置到系统
 * - USB_Bridge_GetConfig(): 获取配置结构体指针
 * - USB_Bridge_SaveConfig(): 保存配置到SNV Flash
 * - USB_Bridge_ExportConfig(): 导出配置到缓冲区
 * - USB_Bridge_ImportConfig(): 从缓冲区导入配置
 * - USB_Bridge_RestoreDefaults(): 恢复默认配置
 * - USB_Bridge_TestConfigStorage(): 测试配置存储功能
 *
 * ### 配置应用场景
 * - 系统启动时调用USB_Bridge_ConfigInit()
 * - 需要修改配置时调用USB_Bridge_GetConfig()
 * - 配置修改完成后调用USB_Bridge_SaveConfig()
 * - 需要恢复默认配置时调用USB_Bridge_RestoreDefaults()
 * - 需要测试配置功能时调用USB_Bridge_TestConfigStorage()
 *
 * ### 配置导入导出
 * - 导出配置：USB_Bridge_ExportConfig()将配置复制到缓冲区
 * - 导入配置：USB_Bridge_ImportConfig()从缓冲区加载配置
 * - 验证配置：自动检查版本和校验和
 * - 应用配置：导入成功后自动应用配置
 *
 * ## 调试技巧
 * - 使用USB_Bridge_GetConfig()查看当前配置
 * - 使用USB_Bridge_TestConfigStorage()测试配置功能
 * - 使用USB_Bridge_RestoreDefaults()恢复默认配置
 * - 监控配置导入导出过程确保数据完整性
 *
 * @author DIY User & AI Assistant
 * @version V1.0
 */

#ifndef __USB_BRIDGE_CONFIG_H__
#define __USB_BRIDGE_CONFIG_H__

#include "user_config.h"

// ===================================================================
// ? 配置管理函数声明
// ===================================================================

/**
 * @brief  初始化配置系统
 */
void USB_Bridge_ConfigInit(void);

/**
 * @brief  应用配置到系统
 */
void USB_Bridge_ApplyConfig(void);

/**
 * @brief  获取配置结构体指针
 * @return 配置结构体指针
 */
UserConfig_t* USB_Bridge_GetConfig(void);

/**
 * @brief  保存当前配置到SNV Flash
 * @return 0=成功, 1=失败
 */
uint8_t USB_Bridge_SaveConfig(void);

/**
 * @brief  导出配置到缓冲区
 * @param  buffer  目标缓冲区
 * @param  size    缓冲区大小
 * @return 0=成功, 1=失败
 */
uint8_t USB_Bridge_ExportConfig(unsigned char *buffer, unsigned short size);

/**
 * @brief  从缓冲区导入配置
 * @param  buffer  源缓冲区
 * @param  size    缓冲区大小
 * @return 0=成功, 1=失败
 */
uint8_t USB_Bridge_ImportConfig(const unsigned char *buffer, unsigned short size);

/**
 * @brief  恢复默认配置
 * @return 0=成功, 1=失败
 */
uint8_t USB_Bridge_RestoreDefaults(void);

/**
 * @brief  测试配置存储功能
 * @return 0=成功, 1=失败
 */
uint8_t USB_Bridge_TestConfigStorage(void);

#endif /* __USB_BRIDGE_CONFIG_H__ */