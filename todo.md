# USB HID to BLE Adapter - 开发计划

## 代码质量改进建议

### 1. 消除代码重复
- **问题**：`usb_bridge.c` 中 USB 主机代码存在重复逻辑
- **建议**：
  - 提取公共函数，如 `USB_HostTransact()`、`Endpoint_Sync()` 等
  - 创建统一的 USB 主机操作接口
  - 减少代码冗余，提高可维护性

### 2. 配置参数集中化
- **问题**：硬编码值分散在多个文件中
- **建议**：
  - 创建统一的配置结构体
  - 将配置存储在 SNV Flash 中
  - 集中管理的配置项：
    - 电池电压阈值
    - 连接参数（间隔、延迟、超时）
    - 设备名称
    - 调试日志级别
    - LED 指示灯行为

### 3. 增强错误恢复机制
- **问题**：USB 通信失败时缺乏重试机制
- **建议**：
  - 实现 USB 设备断开重连逻辑
  - 添加 BLE 连接丢失自动重连
  - 看门狗超时后的安全恢复流程
  - 错误统计和日志记录

### 4. 内存使用优化
- **问题**：BLE 栈占用 6KB RAM，剩余空间有限
- **建议**：
  - 优化缓冲区大小（`MAX_PACKET_SIZE`）
  - 使用更紧凑的数据结构
  - 考虑使用 `__attribute__((packed))` 减少结构体填充
  - 分析内存使用情况，识别优化机会

## 功能增强建议（前三点）

### 1. 多设备支持
- **目标**：支持同时连接多个 USB 设备
- **实现方案**：
  ```c
  typedef struct {
      uint8_t dev_addr;
      uint8_t dev_type;  // KEYBOARD, MOUSE, GAMEPAD
      uint8_t endpoint;
      uint8_t sync_toggle;  // DATA0/DATA1 同步
      uint8_t report_buffer[8];
  } UsbDevice_t;

  // 设备管理数组
  static UsbDevice_t usb_devices[MAX_DEVICES];
  ```
- **功能**：
  - 同时连接多个 USB 键盘/鼠标
  - 设备类型识别和分类
  - 独立的 DATA0/DATA1 同步管理
  - 设备热插拔支持

### 2. 配置存储（SNV Flash）
- **目标**：利用 CH583 的 SNV（Secure Non-Volatile）存储
- **存储内容**：
  - 设备配对信息（Bonding 数据）
  - 用户自定义配置
    - 设备名称
    - 连接参数
    - 电池校准数据
    - 调试设置
- **实现方案**：
  ```c
  typedef struct {
      uint8_t device_name[32];
      uint16_t min_conn_interval;
      uint16_t max_conn_interval;
      uint16_t slave_latency;
      uint16_t conn_timeout;
      uint8_t batt_calibration[10];
      uint8_t debug_level;
  } UserConfig_t;

  // SNV 存储函数
  void Config_SaveToSNV(UserConfig_t *config);
  void Config_LoadFromSNV(UserConfig_t *config);
  ```
- **优势**：
  - 配置持久化，断电不丢失
  - 用户可自定义设备行为
  - 便于现场调试和校准

### 3. 固件更新（OTA）
- **目标**：通过 BLE 进行无线固件更新
- **实现方案**：
  - 使用 CH583 的 Bootloader 功能
  - 实现固件版本检查和回滚机制
  - 添加固件完整性验证（CRC32）
  - 支持断点续传
- **流程**：
  1. 连接 BLE 设备
  2. 检查固件版本
  3. 下载新固件到临时区域
  4. 验证固件完整性
  5. 切换到新固件
  6. 验证启动成功
- **安全考虑**：
  - 固件签名验证
  - 防止降级攻击
  - 失败回滚机制

## 开发优先级

| 优先级 | 任务 | 预计工作量 | 备注 |
|--------|------|------------|------|
| 高 | 配置参数集中化 | 2-3天 | 提高可配置性 |
| 高 | 多设备支持 | 3-5天 | 核心功能增强 |
| 中 | 配置存储（SNV） | 2-3天 | 数据持久化 |
| 中 | 错误恢复机制 | 1-2天 | 提高稳定性 |
| 低 | 内存优化 | 1-2天 | 性能优化 |
| 低 | 固件更新（OTA） | 5-7天 | 长期目标 |

## 注意事项

1. **编译环境**：所有代码修改需在 Windows 下的沁恒微 IDE 中编译通过
2. **向后兼容**：保持与现有硬件的兼容性
3. **测试验证**：每个功能修改后需进行充分测试
4. **文档更新**：同步更新 README.md 和相关文档

## 下一步行动

1. 评估配置参数集中化的工作量
2. 设计多设备支持的数据结构
3. 研究 CH583 SNV 存储的 API
4. 制定详细的实现计划

---
*创建时间：2026-02-28*
*最后更新：2026-02-28*