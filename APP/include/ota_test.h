/**
 * @file ota_test.h
 * @brief OTA测试头文件
 *
 * ## 架构说明
 *
 * ### 1. 模块定位
 * 本模块属于系统的OTA测试层，提供OTA功能的测试接口。
 * 主要职责：
 * - OTA功能测试：在不烧录的情况下测试OTA功能
 * - CRC32测试：验证CRC32计算的正确性
 * - 协议测试：验证协议解析和构建的正确性
 * - 状态机测试：验证OTA状态机的正确性
 * - 数据结构测试：验证数据结构的正确性
 *
 * ### 2. 依赖关系
 * - 上层调用: 主测试程序或调试接口
 * - 下层依赖: ota_manager.c（OTA管理器）、ota_protocol.c（OTA协议）
 * - 数据依赖: ota_protocol.h（OTA协议定义）
 *
 * ### 3. 关键数据结构
 * - 测试函数接口：提供完整的OTA功能测试
 * - 测试用例：覆盖OTA协议的各个功能模块
 *
 * ### 4. 核心算法
 * - 测试框架：提供统一的测试接口
 * - 测试用例设计：覆盖正常流程和异常流程
 * - 结果验证：验证测试结果的正确性
 *
 * ## 使用指南
 *
 * ### OTA测试流程
 * 1. 调用OtaTest_RunAll()运行所有测试
 * 2. 系统会依次执行各个测试用例
 * 3. 输出测试结果和错误信息
 * 4. 根据测试结果进行调试和修复
 *
 * ### 测试用例说明
 * - OtaTest_CRC32(): 测试CRC32计算功能
 * - OtaTest_Protocol(): 测试协议解析和构建功能
 * - OtaTest_StateMachine(): 测试状态机功能
 * - OtaTest_DataStructures(): 测试数据结构功能
 *
 * ### 测试场景
 * - 正常流程测试：验证OTA更新的正常流程
 * - 异常流程测试：验证错误处理和恢复机制
 * - 边界条件测试：验证边界条件的处理
 * - 性能测试：验证OTA更新的性能指标
 *
 * ### 常用函数
 * - OtaTest_RunAll(): 运行所有OTA测试
 * - OtaTest_CRC32(): 测试CRC32计算
 * - OtaTest_Protocol(): 测试协议解析和构建
 * - OtaTest_StateMachine(): 测试状态机
 * - OtaTest_DataStructures(): 测试数据结构
 *
 * ## 调试技巧
 * - 使用OtaTest_RunAll()进行完整测试
 * - 单独运行特定测试用例进行针对性调试
 * - 结合日志输出分析测试结果
 * - 根据测试结果优化代码实现
 *
 * @author DIY User & AI Assistant
 * @version V1.0
 */

#ifndef __OTA_TEST_H__
#define __OTA_TEST_H__

// ===================================================================
// ? 测试函数声明
// ===================================================================

/**
 * @brief  运行所有OTA测试
 */
void OtaTest_RunAll(void);

/**
 * @brief  测试CRC32计算
 */
void OtaTest_CRC32(void);

/**
 * @brief  测试协议解析和构建
 */
void OtaTest_Protocol(void);

/**
 * @brief  测试状态机
 */
void OtaTest_StateMachine(void);

/**
 * @brief  测试数据结构
 */
void OtaTest_DataStructures(void);

#endif /* __OTA_TEST_H__ */