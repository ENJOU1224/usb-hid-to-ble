# USB HID to BLE Adapter (ENJOU Wireless Adapter)

一个基于 CH583 RISC-V 微控制器的 USB HID 到 BLE 适配器，可以将 USB 键盘和鼠标转换为蓝牙低功耗 (BLE) HID 设备。

## 项目概述

本项目实现了一个 USB Host 到 Bluetooth Low Energy (BLE) 的桥接器，将 USB HID（人机接口设备）键盘和鼠标输入转换为 BLE HID 设备功能。简单来说，它允许 USB 键盘或鼠标通过蓝牙无线使用。

### 主要功能

- **USB Host 功能**：
  - USB 2.0 Host 控制器支持
  - HID 设备枚举（键盘和鼠标）
  - 标准键盘和 NKRO（N键无冲）键盘数据解析
  - 鼠标数据解析（标准、NiZ 等多种格式）
  - USB 端点 DATA0/DATA1 同步处理

- **BLE 外围设备角色**：
  - GAP（通用访问配置文件）外围设备角色
  - GATT（通用属性配置文件）服务：
    - HID 服务（键盘和鼠标报告）
    - 电池服务（基于 ADC 电压测量）
    - 设备信息服务
    - 扫描参数服务
  - 安全配对的绑定管理器
  - 广播和连接管理

- **电池管理**：
  - 基于 ADC 的电池电压测量
  - 使用查找表计算电池电量百分比
  - 通过 BLE 电池服务报告电池电量
  - 临界电池电量监控

- **电源管理**：
  - DCDC 转换器支持低功耗运行
  - 多种睡眠模式（空闲、暂停、睡眠、关机）
  - 看门狗定时器确保系统安全

- **调试和日志**：
  - 可配置的 UART 调试输出
  - 独立的日志分类（系统、USB、BLE、电池、按键、鼠标）
  - LED 指示系统状态和 BLE 连接

## 硬件规格

- **微控制器**：CH582F（CH583 系列）
- **架构**：RISC-V (RV32IMAC)
- **内存**：
  - Flash：448KB
  - RAM：32KB
  - BLE 栈内存：6KB（可配置）
- **时钟**：60MHz 系统时钟（PLL）
- **USB**：USB 2.0 全速（12 Mbps）
- **BLE**：蓝牙低功耗 5.0

## 项目结构

```
usb-hid-to-ble/
├── APP/                    # 应用层
│   ├── hidkbd_main.c       # 主入口点和系统初始化
│   ├── usb_bridge.c        # USB 到 BLE 桥接逻辑
│   ├── hidkbd.c            # BLE HID 键盘/鼠标应用逻辑
│   ├── debug.c             # 调试日志工具
│   └── include/            # 应用头文件
├── HAL/                    # 硬件抽象层
│   ├── MCU.c               # 微控制器配置
│   ├── LED.c               # LED 控制
│   ├── KEY.c               # 按键/按钮处理
│   ├── SLEEP.c             # 睡眠/电源管理
│   ├── RTC.c               # 实时时钟
│   └── include/            # HAL 头文件
├── Profile/                # BLE 配置文件服务
│   ├── hidkbdservice.c     # HID 键盘/鼠标服务
│   ├── battservice.c       # 电池服务
│   ├── devinfoservice.c    # 设备信息服务
│   ├── scanparamservice.c  # 扫描参数服务
│   ├── hiddev.c            # HID 设备实现
│   └── include/            # 配置文件头文件
├── SRC/                    # 系统源文件
│   ├── StdPeriphDriver/    # CH583 外设驱动
│   ├── Startup/            # 启动代码 (startup_CH583.S)
│   ├── RVMSIS/             # RISC-V 核心支持
│   └── Ld/                 # 链接脚本 (Link.ld)
├── LIB/                    # 预编译库
│   ├── libCH58xBLE.a       # BLE 栈库
│   ├── CH58xBLE_LIB.h      # BLE 库头文件
│   ├── CH58xBLE_ROM.h      # 基于 ROM 的 BLE 函数
│   └── *.hex               # ROM 固件文件
└── HID_Keyboard.wvproj     # WCH IDE 项目配置
```

## 构建要求

- **开发环境**：WCH IDE（WCH-Link 调试器）
- **工具链**：RISC-V GCC 工具链
- **目标芯片**：CH582F（CH583 系列）
- **操作系统**：NoneOS（无操作系统）

## 构建配置

项目配置为 WCH IDE，具有以下设置：

- **目标**：CH582F (RISC-V, NoneOS)
- **优化**：大小优化
- **调试**：禁用（发布版本）
- **定义符号**：`DCDC_ENABLE=1`, `ENABLE_WATCHDOG=1`, `DEBUG=1`, `DEBUG_BATT=1`
- **库**：`ISP583`, `CH58xBLE`
- **链接脚本**：`Link.ld`（内存布局：FLASH 448KB @ 0x00000000, RAM 32KB @ 0x20000000）

## 硬件要求

- CH583 开发板
- USB Host 端口（用于连接 USB 键盘/鼠标）
- 电池测量电路（ADC 输入在 PA4）
- LED 指示灯（PB4 和 PB7）
- UART 调试输出（PA9 TX）

## 主要文件说明

1. **`hidkbd_main.c`** - 主应用程序入口点，系统初始化
2. **`usb_bridge.c`** - 核心 USB 到 BLE 转换逻辑
3. **`hidkbd.c`** - BLE HID 应用逻辑和电池管理
4. **`hidkbdservice.c`** - HID 服务实现，包含键盘/鼠标报告
5. **`battservice.c`** - 电池服务，包含电压测量
6. **`startup_CH583.S`** - RISC-V 启动代码和中断向量表
7. **`Link.ld`** - 内存布局和段定义
8. **`libCH58xBLE.a`** - 预编译的 BLE 栈库

## 使用说明

1. 将项目导入 WCH IDE
2. 连接 CH583 开发板
3. 编译并下载固件到开发板
4. 连接 USB 键盘/鼠标到开发板的 USB Host 端口
5. 通过蓝牙连接到适配器（设备名称：ENJOU BLE Adapter）

## 调试功能

项目包含可配置的调试输出，可以通过 UART 查看系统状态：

- `DEBUG_SYS` - 系统初始化、主循环、看门狗日志
- `DEBUG_USB` - USB 枚举和通信日志
- `DEBUG_BLE` - BLE 状态和连接日志
- `DEBUG_BATT` - 电池电压和电量日志
- `DEBUG_KEY` - 键盘按键事件日志
- `DEBUG_MOUSE` - 鼠标移动事件日志
- `ENABLE_LED` - 启用 LED 指示灯

## 项目状态

这是一个功能完整的嵌入式固件项目，成功实现了 USB 到 BLE 适配器。代码结构清晰，注释完善，具有良好的硬件抽象层分离。

## 许可证

本项目基于 WCH 官方库和开源组件开发，具体许可证请参考相关库文件。

## 贡献

欢迎提交问题和拉取请求。

## 联系方式

如有问题或建议，请通过 GitHub Issues 联系。

---

**ENJOU Wireless Adapter** - 将您的 USB 设备无线化！