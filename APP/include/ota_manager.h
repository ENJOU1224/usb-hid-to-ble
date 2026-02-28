/**
 * @file ota_manager.h
 * @brief OTA固件更新管理器头文件
 *
 * ## 架构说明
 *
 * ### 1. 模块定位
 * 本模块属于系统的固件更新层，提供OTA（Over-The-Air）固件更新管理功能。
 * 主要职责：
 * - OTA状态管理
 * - 固件头信息定义
 * - OTA事件和错误码定义
 * - OTA管理器函数声明
 *
 * ### 2. 依赖关系
 * - 上层调用: ota_service.c（OTA服务接口）
 * - 下层依赖: ota_manager.c（OTA管理器实现）
 * - 数据依赖: 无
 *
 * ### 3. 关键数据结构
 * - FirmwareHeader_t: 固件头信息结构体
 *   - magic: 魔法数（用于验证固件头）
 *   - version: 固件版本号
 *   - build: 构建号
 *   - size: 固件大小
 *   - crc32: CRC32校验和
 *   - timestamp: 时间戳
 *   - reserved: 保留字段
 *
 * - OtaState_t: OTA状态枚举
 *   - OTA_STATE_IDLE: 空闲状态
 *   - OTA_STATE_CHECKING: 检查版本
 *   - OTA_STATE_DOWNLOADING: 下载固件
 *   - OTA_STATE_VERIFYING: 验证固件
 *   - OTA_STATE_UPDATING: 更新固件
 *   - OTA_STATE_COMPLETE: 更新完成
 *   - OTA_STATE_ERROR: 错误状态
 *
 * - OtaStateInfo_t: OTA状态信息结构体
 *   - state: OTA状态
 *   - total_size: 总大小
 *   - received_size: 已接收大小
 *   - crc32: 当前CRC32
 *   - buffer: 数据缓冲区
 *   - buffer_pos: 缓冲区位置
 *
 * - OtaEvent_t: OTA事件枚举
 *   - OTA_EVENT_START: 开始OTA
 *   - OTA_EVENT_DATA: 数据包
 *   - OTA_EVENT_COMPLETE: 完成传输
 *   - OTA_EVENT_CANCEL: 取消OTA
 *   - OTA_EVENT_ERROR: 错误事件
 *
 * - OtaError_t: OTA错误码枚举
 *   - OTA_ERROR_NONE: 无错误
 *   - OTA_ERROR_INVALID_PARAM: 无效参数
 *   - OTA_ERROR_INVALID_HEADER: 无效固件头
 *   - OTA_ERROR_CRC_FAILED: CRC校验失败
 *   - OTA_ERROR_FLASH_WRITE: Flash写入失败
 *   - OTA_ERROR_VERSION_MISMATCH: 版本不匹配
 *   - OTA_ERROR_SIZE_MISMATCH: 大小不匹配
 *   - OTA_ERROR_TIMEOUT: 超时
 *   - OTA_ERROR_UNKNOWN: 未知错误
 *
 * ### 4. 核心算法
 * - OTA状态机: 管理OTA更新的各个阶段
 * - 固件验证: 魔法数、版本、大小、CRC32验证
 * - 进度计算: 实时计算更新进度
 * - 错误处理: 各种错误情况的处理和恢复
 *
 * ## 使用指南
 *
 * ### OTA更新流程
 * 1. 调用OtaManager_StartUpdate()开始更新
 * 2. 调用OtaManager_ReceiveData()接收固件数据
 * 3. 调用OtaManager_CompleteUpdate()完成更新
 * 4. 系统重启后，Bootloader会加载新固件
 *
 * ### 常用函数
 * - OtaManager_Init(): 初始化OTA管理器
 * - OtaManager_StartUpdate(): 开始OTA更新
 * - OtaManager_ReceiveData(): 接收固件数据
 * - OtaManager_CompleteUpdate(): 完成OTA更新
 * - OtaManager_CancelUpdate(): 取消OTA更新
 * - OtaManager_GetState(): 获取OTA状态
 * - OtaManager_GetProgress(): 获取更新进度
 * - OtaManager_IsUpdating(): 检查是否正在更新
 *
 * ## 调试技巧
 * - 使用OTA状态枚举跟踪更新进度
 * - 检查OTA错误码定位问题
 * - 验证固件头信息的正确性
 * - 监控OTA事件处理
 *
 * @author DIY User & AI Assistant
 * @version V1.0
 */

#ifndef __OTA_MANAGER_H__
#define __OTA_MANAGER_H__

#include <stdint.h>

// ===================================================================
// ? OTA配置定义
// ===================================================================

/**
 * @brief OTA魔法数
 * @details 用于验证固件头的魔法数，对应ASCII码"CH58"
 * @note 必须与固件头中的magic字段匹配
 */
#define OTA_MAGIC_NUMBER        0x43483538  // "CH58"的ASCII码

/**
 * @brief OTA协议版本号
 * @details OTA协议的版本号，用于协议兼容性检查
 * @note 版本号格式：高8位为主版本，低8位为次版本
 */
#define OTA_VERSION             0x0100      // OTA协议版本 1.0

/**
 * @brief 最大数据包大小
 * @details BLE传输中单个数据包的最大字节数
 * @note 根据BLE MTU大小调整，通常为256字节
 */
#define OTA_MAX_PACKET_SIZE     256         // 最大数据包大小

/**
 * @brief OTA缓冲区大小
 * @details 用于存储固件数据的缓冲区大小
 * @note 根据内存大小调整，建议为最大数据包大小的2倍
 */
#define OTA_BUFFER_SIZE         512         // OTA缓冲区大小

// ===================================================================
// ? 固件头信息结构体
// ===================================================================

/**
 * @brief 固件头信息结构体
 * @details 包含固件的基本信息，用于OTA更新时验证固件的完整性和兼容性
 * @note 使用packed属性确保结构体紧密排列，无填充字节
 */
typedef struct __attribute__((packed)) {
    /**
     * @brief 魔法数
     * @details 用于验证固件头的魔法数，必须为0x43483538（"CH58"的ASCII码）
     * @note 如果魔法数不匹配，表示固件头无效
     */
    uint32_t magic;           // 魔数：0x43483538 (CH58)

    /**
     * @brief 固件版本号
     * @details 固件的主版本号和次版本号
     * @note 高8位为主版本，低8位为次版本
     */
    uint16_t version;         // 固件版本号

    /**
     * @brief 构建号
     * @details 固件的构建号，用于区分同一版本的不同构建
     * @note 每次构建时递增
     */
    uint16_t build;           // 构建号

    /**
     * @brief 固件大小
     * @details 固件的总字节数（不包括固件头）
     * @note 必须与实际固件大小匹配
     */
    uint32_t size;            // 固件大小

    /**
     * @brief CRC32校验和
     * @details 固件数据的CRC32校验和
     * @note 用于验证固件数据的完整性
     */
    uint32_t crc32;           // CRC32校验和

    /**
     * @brief 时间戳
     * @details 固件编译的时间戳
     * @note 用于记录固件生成时间
     */
    uint32_t timestamp;       // 时间戳

    /**
     * @brief 保留字段
     * @details 保留给未来使用的字段
     * @note 必须初始化为0
     */
    uint8_t  reserved[16];    // 保留字段
} FirmwareHeader_t;

// ===================================================================
// ? OTA状态定义
// ===================================================================

/**
 * @brief OTA状态枚举
 * @details 定义OTA更新过程中的各个状态
 */
typedef enum {
    /**
     * @brief 空闲状态
     * @details OTA更新未开始或已完成
     */
    OTA_STATE_IDLE = 0,

    /**
     * @brief 检查版本
     * @details 检查固件版本兼容性
     */
    OTA_STATE_CHECKING,

    /**
     * @brief 下载固件
     * @details 正在接收固件数据
     */
    OTA_STATE_DOWNLOADING,

    /**
     * @brief 验证固件
     * @details 验证固件完整性和兼容性
     */
    OTA_STATE_VERIFYING,

    /**
     * @brief 更新固件
     * @details 将固件写入Flash
     */
    OTA_STATE_UPDATING,

    /**
     * @brief 更新完成
     * @details OTA更新成功完成
     */
    OTA_STATE_COMPLETE,

    /**
     * @brief 错误状态
     * @details OTA更新过程中发生错误
     */
    OTA_STATE_ERROR
} OtaState_t;

// ===================================================================
// ? OTA状态结构体
// ===================================================================

/**
 * @brief OTA状态信息结构体
 * @details 包含OTA更新过程中的所有状态信息
 */
typedef struct {
    /**
     * @brief OTA状态
     * @details 当前OTA更新的状态（空闲、检查、下载、验证等）
     */
    uint8_t  state;           // OTA状态

    /**
     * @brief 总大小
     * @details 固件的总字节数
     */
    uint32_t total_size;      // 总大小

    /**
     * @brief 已接收大小
     * @details 已经接收到的固件字节数
     */
    uint32_t received_size;   // 已接收大小

    /**
     * @brief 当前CRC32
     * @details 当前已接收数据的CRC32校验和
     */
    uint32_t crc32;           // 当前CRC32

    /**
     * @brief 数据缓冲区
     * @details 用于存储固件数据的缓冲区
     */
    uint8_t  buffer[OTA_BUFFER_SIZE];  // 数据缓冲区

    /**
     * @brief 缓冲区位置
     * @details 缓冲区中当前数据的位置
     */
    uint16_t buffer_pos;      // 缓冲区位置
} OtaStateInfo_t;

// ===================================================================
// ? OTA事件定义
// ===================================================================

/**
 * @brief OTA事件枚举
 * @details 定义OTA更新过程中可能发生的事件
 */
typedef enum {
    /**
     * @brief 开始OTA
     * @details OTA更新开始事件
     */
    OTA_EVENT_START = 0,

    /**
     * @brief 数据包
     * @details 接收到固件数据包事件
     */
    OTA_EVENT_DATA,

    /**
     * @brief 完成传输
     * @details 固件传输完成事件
     */
    OTA_EVENT_COMPLETE,

    /**
     * @brief 取消OTA
     * @details OTA更新取消事件
     */
    OTA_EVENT_CANCEL,

    /**
     * @brief 错误事件
     * @details OTA更新过程中发生错误事件
     */
    OTA_EVENT_ERROR
} OtaEvent_t;

// ===================================================================
// ? OTA错误码定义
// ===================================================================

/**
 * @brief OTA错误码枚举
 * @details 定义OTA更新过程中可能发生的错误类型
 */
typedef enum {
    /**
     * @brief 无错误
     * @details OTA更新正常进行或成功完成
     */
    OTA_ERROR_NONE = 0,

    /**
     * @brief 无效参数
     * @details 传入的参数无效或超出范围
     */
    OTA_ERROR_INVALID_PARAM,

    /**
     * @brief 无效固件头
     * @details 固件头信息无效或魔法数不匹配
     */
    OTA_ERROR_INVALID_HEADER,

    /**
     * @brief CRC校验失败
     * @details 固件数据的CRC32校验失败
     */
    OTA_ERROR_CRC_FAILED,

    /**
     * @brief Flash写入失败
     * @details 固件写入Flash时发生错误
     */
    OTA_ERROR_FLASH_WRITE,

    /**
     * @brief 版本不匹配
     * @details 固件版本与当前系统不兼容
     */
    OTA_ERROR_VERSION_MISMATCH,

    /**
     * @brief 大小不匹配
     * @details 固件大小与声明的大小不一致
     */
    OTA_ERROR_SIZE_MISMATCH,

    /**
     * @brief 超时
     * @details OTA更新操作超时
     */
    OTA_ERROR_TIMEOUT,

    /**
     * @brief 未知错误
     * @details 发生未知错误
     */
    OTA_ERROR_UNKNOWN
} OtaError_t;

// ===================================================================
// ? 函数声明
// ===================================================================

/**
 * @brief  初始化OTA管理器
 *
 * @details
 * #### 功能详细说明
 * 此函数初始化OTA管理器，将所有状态变量清零，并设置初始状态为空闲。
 * OTA管理器负责管理固件更新的整个过程，包括状态跟踪、数据接收和验证。
 *
 * #### 调用示例
 * 系统启动时调用，初始化OTA管理器。
 *
 * #### 使用场景
 * - 系统启动时调用
 * - 系统复位后调用
 * - 需要重新初始化OTA管理器时调用
 */
void OtaManager_Init(void);

/**
 * @brief  开始OTA更新
 *
 * @param  total_size  固件总大小 - 要更新的固件总字节数
 * @param  crc32       固件CRC32校验和 - 固件数据的CRC32校验和
 *
 * @return 0=成功, 1=失败
 *
 * @details
 * #### 功能详细说明
 * 此函数开始OTA更新过程，设置OTA状态机为检查状态，并记录固件总大小和CRC32校验和。
 *
 * #### 调用示例
 * 开始更新固件，固件大小为65536字节，CRC32为0x12345678。
 *
 * #### 使用场景
 * - 接收到OTA更新请求时调用
 * - 开始固件下载前调用
 */
uint8_t OtaManager_StartUpdate(uint32_t total_size, uint32_t crc32);

/**
 * @brief  接收数据包
 *
 * @param  data    数据指针 - 指向固件数据的指针
 * @param  length  数据长度 - 数据的字节数
 *
 * @return 0=成功, 1=失败
 *
 * @details
 * #### 功能详细说明
 * 此函数接收固件数据包，验证数据完整性，并更新OTA状态。
 *
 * #### 调用示例
 * 接收256字节的固件数据包。
 *
 * #### 使用场景
 * - BLE接收到固件数据包时调用
 * - OTA下载过程中调用
 */
uint8_t OtaManager_ReceiveData(uint8_t *data, uint16_t length);

/**
 * @brief  完成OTA更新
 *
 * @return 0=成功, 1=失败
 *
 * @details
 * #### 功能详细说明
 * 此函数完成OTA更新过程，验证固件完整性，并切换到完成状态。
 *
 * #### 调用示例
 * 固件接收完成后调用。
 *
 * #### 使用场景
 * - 固件数据接收完成时调用
 * - OTA下载完成后调用
 */
uint8_t OtaManager_CompleteUpdate(void);

/**
 * @brief  取消OTA更新
 *
 * @details
 * #### 功能详细说明
 * 此函数取消正在进行的OTA更新，重置OTA状态机。
 *
 * #### 调用示例
 * 用户取消OTA更新时调用。
 *
 * #### 使用场景
 * - 用户取消更新时调用
 * - 网络断开时调用
 * - 系统错误时调用
 */
void OtaManager_CancelUpdate(void);

/**
 * @brief  获取OTA状态
 *
 * @return OTA状态 - 当前OTA更新的状态
 *
 * @details
 * #### 功能详细说明
 * 此函数获取当前OTA更新的状态。
 *
 * #### 调用示例
 * 检查OTA更新是否正在进行。
 *
 * #### 使用场景
 * - 监控OTA更新进度时调用
 * - 系统状态查询时调用
 */
uint8_t OtaManager_GetState(void);

/**
 * @brief  获取OTA进度
 *
 * @return 已接收字节数 - 已经接收到的固件字节数
 *
 * @details
 * #### 功能详细说明
 * 此函数获取当前OTA更新的进度。
 *
 * #### 调用示例
 * 获取已接收的固件字节数。
 *
 * #### 使用场景
 * - 显示更新进度时调用
 * - 监控下载速度时调用
 */
uint32_t OtaManager_GetProgress(void);

/**
 * @brief  获取OTA错误码
 *
 * @return 错误码 - OTA更新过程中的错误类型
 *
 * @details
 * #### 功能详细说明
 * 此函数获取OTA更新过程中的错误码。
 *
 * #### 调用示例
 * 检查OTA更新是否发生错误。
 *
 * #### 使用场景
 * - 错误处理时调用
 * - 日志记录时调用
 */
OtaError_t OtaManager_GetError(void);

/**
 * @brief  检查当前是否正在OTA
 *
 * @return 1=正在OTA, 0=未OTA
 *
 * @details
 * #### 功能详细说明
 * 此函数检查当前是否正在进行OTA更新。
 *
 * #### 调用示例
 * 检查OTA更新是否正在进行。
 *
 * #### 使用场景
 * - 系统状态检查时调用
 * - 防止重复OTA更新时调用
 */
uint8_t OtaManager_IsUpdating(void);

/**
 * @brief  获取当前固件版本
 *
 * @return 固件版本号 - 当前固件的版本号
 *
 * @details
 * #### 功能详细说明
 * 此函数获取当前运行的固件版本号。
 *
 * #### 调用示例
 * 获取当前固件版本，用于与新固件版本比较。
 *
 * #### 使用场景
 * - 版本检查时调用
 * - OTA更新前调用
 */
uint16_t OtaManager_GetCurrentVersion(void);

/**
 * @brief  检查新固件是否可用
 *
 * @param  new_version  新固件版本号 - 新固件的版本号
 *
 * @return 1=可用, 0=不可用
 *
 * @details
 * #### 功能详细说明
 * 此函数检查新固件是否可用，通过比较版本号判断。
 *
 * #### 调用示例
 * 检查版本号为0x0200的新固件是否可用。
 *
 * #### 使用场景
 * - OTA更新检查时调用
 * - 版本升级判断时调用
 */
uint8_t OtaManager_CheckNewFirmware(uint16_t new_version);

#endif /* __OTA_MANAGER_H__ */