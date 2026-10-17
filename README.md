# STM32-OTA-Bootloader

基于 STM32F103C8T6 的 IAP 型 OTA 远程升级 Bootloader：XMODEM 串口升级、W25Q64 外部 Flash 固件暂存、M24C02 升级标志契约，并预留 4G（Air780E / OneNET）远程升级链路。

> STM32F103C8T6 OTA bootloader: XMODEM over UART, W25Q64 external-flash staging, M24C02 OTA flag, and a designed 4G (Air780E / OneNET) remote-upgrade path.

## 功能特性

**已实现（含真机验证）**

- ✅ Flash 双区划分与链接器硬约束：Bootloader 固定 20KB（分散加载文件限制，超限直接报 `L6406E` 拒绝链接，当前占用 17992B / 20480B）
- ✅ 中断向量表校验 + 安全跳转：跳转前校验 A 区 MSP/PC 合法性，非法则**拒绝跳转**，杜绝升级失败变砖
- ✅ XMODEM-CRC16 串口固件接收：128 字节有效数据/包，`C` 握手（100ms 周期，仅等待首包期间发送，避免传输中途插入导致重传错位）
- ✅ USART1 DMA + IDLE 空闲中断的环形帧队列接收，不丢帧、不定长
- ✅ W25Q64（SPI）固件暂存：1KB 块落盘、逐块**写后回读校验**
- ✅ M24C02（软件 I2C）保存升级契约：OTA 标志 / 固件长度 / 版本号，掉电不丢
- ✅ 上电自动升级：检测到有效 OTA 标志 → 擦 A 区 → W25Q64 搬运 → 跳转，全程无人工
- ✅ 串口命令行 + 救援机制：2 秒窗口按 `w` 进入命令行；**A 区为空/损坏时自动驻留命令行**，可远程重刷

**设计完成、持续开发中**

- 🚧 4G Air780E + OneNET 远程下载链路：采用「App 主导」方案——联网下载全部放在 A 区应用，App 只按契约写 W25Q64 和 EEPROM，**Bootloader 零改动**；仓库含完整框架设计文档与 Air780E 初始化/心跳参考代码
- 🚧 A 区示例 APP 工程（IROM 偏移 0x08005000）与固件断点续传

## 硬件清单

| 模块 | 型号 | 接口 |
|---|---|---|
| 主控 | STM32F103C8T6（64KB Flash / 1KB 每页） | — |
| SPI Flash | W25Q64（固件暂存区） | SPI1 |
| EEPROM | M24C02 / AT24C02 兼容（升级标志） | 软件 I2C |
| 4G 模组（规划） | 合宙 Air780E + OneNET 云 | USART 透传 |
| 调试串口 | USB-TTL | USART1，921600 8N1 |

## Flash 分区

| 分区 | 地址范围 | 大小 | 用途 |
|---|---|---|---|
| B 区 Bootloader | `0x08000000 – 0x08004FFF` | 20KB（页 0–19） | 引导、命令行、XMODEM、固件搬运 |
| A 区 Application | `0x08005000 – 0x0800FFFF` | 44KB（页 20–63） | 用户应用程序 |

> ⚠️ **20KB 红线**：bootloader 必须整体待在 B 区内，由链接器 scatter 文件强制约束——代码放不下会直接报 `L6406E / L6407E`，正确做法是精简代码而不是放大限制。

## 启动与升级流程

```
                        ┌───────── 上电复位 ─────────┐
                        ▼
                 bootloder_Brance()
          2 秒内串口收到 'w' ？──是──► 命令行菜单 [1]~[7]
                        │否
                        ▼
          M24C02 中 OTA_FLAG == 0x03020100 ？
             │否                     │是
             ▼                       ▼
       LOAD_A() 跳转 A 区      BootLoader_UpdateAFromW25Q64()
       （A 区向量表非法时        校验向量表 → 擦除 A 区 →
        打印错误并返回，          W25Q64 逐块搬运（写后回读）→
        自动驻留救援命令行）       LOAD_A() 跳转新固件
```

**4G 远程升级路径（App 主导，设计中）：**

```
OneNET/服务器 ──MQTT/透传──► 4G Air780E ──UART──► A 区 APP
                                                  │ ① 分片固件按 256B/页写入 W25Q64 块 0
                                                  │ ② 整包 CRC 校验
                                                  │ ③ 写 M24C02：OTA_FLAG + 固件长度 + 新版本号
                                                  │ ④ NVIC_SystemReset()
                                                  ▼
                                     复位后 Bootloader 自动搬运（见上图右分支）
```

网络/串口 → SRAM 小缓冲 → 攒满一页(256B)写 W25Q64 → 置 EEPROM 标志 → 复位搬运，升级失败旧程序不受影响。

## Bootloader 命令行菜单

上电 2 秒内向 USART1 发送字符 `w` 进入：

| 菜单项 | 功能 |
|---|---|
| `[1]` | 擦除 A 分区 |
| `[2]` | UART IAP：XMODEM 直下 A 分区 |
| `[3]` / `[4]` | 设置 / 查看 OTA 版本号 |
| `[5]` | XMODEM 下载固件 → W25Q64 暂存（1KB 块落盘） |
| `[6]` | W25Q64 固件 → A 分区搬运并运行 |
| `[7]` | 复位重启 |

版本号格式（26 字节，写入时校验）：`VER-1.0.0-2023/02/20-12:00`

## 快速上手（XMODEM 升级演示）

1. USB-TTL 连接 USART1（PA9-TX / PA10-RX / 共地），串口助手 921600 8N1；
2. 上电 2 秒内发送 `w`，进入命令行菜单；
3. 选择 `[2]`（直下 A 区）或 `[5]`（W25Q64 暂存），用串口工具以 **XMODEM-CRC** 协议发送 `.bin` 固件；
4. 选 `[6]` 搬运运行，或直接复位观察自动升级；
5. 远程升级链路的架构设计与模块划分见 `OTA远程升级_4G模块框架设计.md`。

## 目录结构

```
├── Core/                          # CubeMX 生成代码 + main（引导主流程）
├── Drivers/                       # STM32 HAL 库
├── MDK-ARM/  EWARM/               # Keil / IAR 工程文件
├── hardware/
│   ├── boot.c / boot.h            # 核心：菜单、XMODEM、A区搬运、安全跳转
│   ├── fmc.c / fmc.h              # 内部 Flash 擦写
│   ├── w25q64.c / w25q64.h        # SPI Flash 固件暂存驱动
│   ├── m24c02.c / m24c02.h        # EEPROM 契约存储驱动
│   ├── my_uart.c / my_uart.h      # DMA + IDLE 环形帧队列
│   ├── my_spi.c / my_iic.c        # SPI / 软件 I2C
│   └── DWT.c  delay.c             # 微秒延时、周期计数
├── log/                           # 调试日志
├── OTA远程升级_4G模块框架设计.md    # 4G 链路分层架构与模块划分
├── OTA_功能5和6_设计与实现思路.md   # W25Q64 暂存/回搬功能设计
├── 4G_Air780_初始化与心跳代码.md    # Air780E 初始化与心跳参考
└── OTA_Bootloader_问题分析与修改说明.txt
```

## 编译与移植

- **Bootloader**：Keil MDK-ARM（或 IAR EWARM）直接打开对应目录工程；STM32CubeMX 配置文件为 `bootloder.ioc`。
- **APP 工程**：IROM 起始地址设为 `0x08005000`、大小 44KB，并将 `VECT_TAB_OFFSET` 配置为 `0x5000`；在应用内触发升级时按契约写 W25Q64 与 M24C02 后调用 `NVIC_SystemReset()`。
- 自定义跳转 `LOAD_A()`：设置 MSP → 取复位向量 → 跳转，跳转前将 B 区用到的外设寄存器复位。

## Roadmap

- [ ] A 区示例 APP：接入 Air780E AT 驱动 + OneNET MQTT，实现版本比对与分片下载
- [ ] 固件整包 CRC 校验与断点续传
- [ ] 救援路径：4G 模组预配置透传，服务器直接对接 bootloader 命令行（XMODEM 远程灌固件）

## 说明

本项目为个人学习与研究项目，欢迎交流指正（Issue / PR）。
