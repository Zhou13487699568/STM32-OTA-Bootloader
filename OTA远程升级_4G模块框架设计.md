# 远程 OTA（4G 模块）框架设计

> **前提说明**：4G 部分的代码目前**还没有写**。本文是基于你**已有工程**（bootloader 的接口约定、已占用的硬件资源、20KB Flash 限制）做的**框架设计**，不是对现有代码的分析。
> 文中带「实测」标记的数据都来自你工程里已经编译验证过的产物。

---

## 一、总体思路：一句话

**把"远程"这件事全部放进 A 区 App 做，bootloader 只当"搬运工"。**

```
云端服务器 ──(MQTT / HTTPS)──► 4G 模组 ──(AT / UART)──► MCU 的 A 区 App
                                                              │
                            ① 收到固件分片                     │
                            ② 直接写进 W25Q64 第 0 块           │
                            ③ 全部收完做整体 CRC 校验           │
                            ④ 写 M24C02(EEPROM)：               │
                                 OTA_FLAG = 0x03020100           │
                                 Firelen[0] = 固件字节数          │
                                 OTA_ver = 新版本号               │
                            ⑤ NVIC_SystemReset() 复位            │
                                                              ▼
                                                    ┌──────────────────┐
                                                    │   bootloader     │
                                                    │ 读 EEPROM 标志    │
                                                    │ 有 → 验向量表     │
                                                    │     → 擦 A 区     │
                                                    │     → W25Q64→A 区 │
                                                    │     → 跳转       │
                                                    │ 无 → 直接跳 A 区  │
                                                    └──────────────────┘
```

这套流程**恰好就是本项目已经实现好的**（菜单功能 5 的自动版 + 功能 6 的搬运逻辑），
App 只需按同一份"契约"写 EEPROM 和 W25Q64，bootloader **一行都不用改**。

---

## 二、为什么 4G 必须放在 A 区 App，而不是 bootloader

这不是偏好问题，是被三条硬约束逼出来的：

| 约束 | 实测数据 | 结论 |
|---|---|---|
| **Flash 预算** | bootloader 现在是 **17992 B / 20480 B**（88% 满，余 2488 B） | 4G AT 驱动 + MQTT + JSON 至少 15~30KB，**物理上装不下** |
| **可靠性要求** | bootloader 必须"永远能起来、永远能跳 A 区" | 一旦在里面引入网络状态机（阻塞等待、超时、看门狗），网络异常就可能把板子彻底变砖 |
| **架构职责** | bootloader 已有完整契约（`OTA_Info` + W25Q64 块 0 + `OTA_FLAG`） | 搬运职责已经闭环，网络职责天然属于"联网的那个程序" |

一句话：**bootloader 要"小而不可失败"，App 要"大而可失败"。**

---

## 三、两种可选架构（建议先用第一种，保留第二种做救援）

### 方案 A：App 主导（推荐，主路径）

4G 驱动、MQTT、固件下载**全部在 App 里**，App 把固件写进 W25Q64、置标志、复位，
bootloader 负责搬运。**这是本项目已有的能力，直接对接即可。**

- ✅ bootloader 不用改，Flash 预算压力为 0
- ✅ 下载失败只影响 W25Q64，A 区里的旧程序照跑
- ✅ 可以做断点续传、进度上报、版本比对
- ❌ 前提是 **App 本身还活着**（A 区程序正常）

### 方案 B：模组透传，复用 bootloader 的 XMODEM（救援路径）

用"上电自动连接 + 透传"的 4G 模组（或模组内置 MQTT/HTTP，把数据流透明转发到串口），
服务器通过网络**直接和 bootloader 的命令行对话**，把固件用现有的 XMODEM 协议灌进 A 区。

- ✅ **A 区彻底跑不起来时也能救回来**（这是唯一的远程救援手段）
- ✅ bootloader 完全不用动（现在这份就行）
- ❗ 需要模组"预配置好、上电自动连、纯透传"
- ❗ 需要服务器实现现有的文本菜单协议（按 `2` + XMODEM-CRC）

> **为什么方案 B 真的可行**（这是从你现有代码推出来的，很关键）：
> `bootloder_Brance()` 在 2 秒内没按 `w` 且 `OTA_FLAG != SET` 时会调 `LOAD_A(A区)`；
> 而 `LOAD_A()` 在发现 A 区向量表非法时会**打印错误并返回**，接着就落到
> `u0_printf("Enter bootloader command shell")` + `BootLoader_Info()`。
> 也就是说：**A 区是空的/坏的时，bootloader 会自动停在命令行**，
> 只要 4G 链路是透明的，服务器就能远程执行"按 2 + XMODEM 下载"把 A 区救回来。

**建议：先做方案 A（主路径），把方案 B 作为二期（救援）。**

---

## 四、模块怎么分：按"变化频率 + 可测性 + 失败影响面"三层九模块

### 4.1 分层图

```
┌──────────────────────────────────────────────────────────────────────┐
│ 第 5 层  app/          应用层：主循环 / 时间片 / 状态机调度            │
│   app_main.c           网络状态机 + 心跳 + OTA 触发 + LED/日志         │
│   app_config.h         服务器地址、主题、设备ID、版本号（易变）        │
├──────────────────────────────────────────────────────────────────────┤
│ 第 4 层  service/      业务层：只关心"要干什么"，不关心"怎么发出去"    │
│   ota/ota_client.c     查版本→下载→写W25Q64→CRC→置标志→复位（编排）   │
│   ota/ota_storage.c    W25Q64 块0 与 M24C02 OTA_Info 的统一封装        │
│   ota/ota_contract.h   ★与 bootloader 共用的"契约"头文件               │
│   mqtt/mqtt_client.c   CONNECT/SUBSCRIBE/PUBLISH/PINGREQ/QoS1 重发     │
├──────────────────────────────────────────────────────────────────────┤
│ 第 3 层  bsp/net/      通信层：把"网络"抽象成两条函数                  │
│   net_at.c             AT 通道：命令队列、响应匹配、URC 分发、超时重试 │
│   net_4g_<型号>.c       模组驱动：上电时序/握手/SIM/注网/PDP/连接/重连 │
│   net_4g_port.h        ★模组差异抽象层（换模组只改这里）               │
├──────────────────────────────────────────────────────────────────────┤
│ 第 2 层  bsp/          驱动层：芯片外设（大部分可直接复用现有代码）     │
│   uart/uart2_dma_idle.c  DMA+IDLE 不定长接收（从 my_uart.c 移植）      │
│   storage/w25q64.c       直接复用 bootloader 里的驱动                  │
│   storage/m24c02.c       直接复用 bootloader 里的驱动                  │
├──────────────────────────────────────────────────────────────────────┤
│ 第 1 层  util/         工具层：纯函数，可在 PC 上单元测试              │
│   crc.c  json.c  log.c  ringbuffer.c                                  │
└──────────────────────────────────────────────────────────────────────┘
```

### 4.2 每个模块的职责与对外接口

| 模块 | 职责（做什么） | 不做什么 | 对外接口（示例） |
|---|---|---|---|
| `util/crc` | CRC16/CRC32 计算 | 不碰 IO | `uint32_t crc32(const void*, uint32_t)` |
| `util/json` | 极简 JSON 取值/组包（省 Flash，别用 cJSON） | 不做网络 | `json_get_int(buf, key, &out)` |
| `util/ringbuffer` | 串口收发环形缓冲 | 不做协议 | `rb_put/rb_get/rb_used` |
| `bsp/uart/uart2_dma_idle` | USART2 + DMA + IDLE 收帧（**照抄 my_uart.c 的结构**） | 不解析 AT | `u2_send(buf,len)` / `u2_get_frame(&p,&len)` |
| `bsp/storage/w25q64` | 读/页写/块擦、页对齐保护 | 不知道"固件"概念 | `w25_read/w25_pagewrite/w25_erase64k` |
| `bsp/storage/m24c02` | EEPROM 字节读写 | 不知道 OTA 语义 | `eep_read/eep_write` |
| `bsp/net/net_at` | **AT 收发状态机**：发命令→等匹配→超时重试；URC 无请求上报分发 | 不关心业务命令含义 | `at_cmd("AT+CSQ", "OK", 1000, resp)` / `at_register_urc()` |
| `bsp/net/net_4g_*` | 模组时序与状态机：上电/复位、握手、SIM、注网、PDP、开连接、重连退避 | 不解析固件内容 | `net_init()` / `net_state()` / `net_open()` / `net_send()` |
| `service/mqtt/mqtt_client` | MQTT 报文编解码、连接保活、QoS1 去重 | 不做 AT（调 net_*） | `mqtt_connect/subscribe/publish/poll` |
| `service/ota/ota_storage` | 把"写固件到暂存区"这件事封成一个 API | 不做网络 | `ota_store_begin(len)` / `ota_store_chunk(off,buf,len)` / `ota_store_finish(crc)` |
| `service/ota/ota_client` | 业务编排 + 状态机 + 上报 | 不碰寄存器 | `ota_task()` 由 app_main 周期调用 |
| `app/app_main` | 初始化、主循环调度、心跳/看门狗 | 不写底层 | `main()` |

### 4.3 为什么这样切（切分原则）

1. **按"变化频率"切**：4G 模组型号最容易变（EC800/Air780/ML307…）→ 用 `net_4g_port.h` 把它隔离在最底层；业务策略（什么时候升级、失败怎么办）也常变 → 单独放 `ota_client.c`；中间的 AT 通道与 MQTT 报文编解码几乎不变 → 单独成层。
2. **按"能不能单独测"切**：`crc.c`/`json.c`/`mqtt_packet` 是纯函数，**在 PC 上就能跑单元测试**；`net_at.c` 可以拿 PC 串口 + 脚本模拟模组来测；只有最上层的 `ota_client` 需要真机联调。
3. **按"失败影响面"切**：网络层怎么超时/重连都**不允许影响 bootloader**；存储层怎么读写都**不允许越界到 bootloader 的 20KB**（本工程已在 `STM32_WriteFlash`/`STM32_EraseFlash` 里加了地址护栏，App 侧也要照做）。
4. **契约单一化**：`ota_contract.h` 由 bootloader 和 App **共用同一份**，避免两边改歪 —— 你之前踩的"`0x08005000` 读到 bootloader 代码字"就是典型的"两边约定不一致"。

---

## 五、与 bootloader 的"契约"（必须严格一致，建议放共用头文件）

这是**整个远程 OTA 的地基**，任何一项不一致都会导致升级失败甚至变砖：

| 契约项 | 取值 | 谁写 | 谁读 |
|---|---|---|---|
| `OTA_Info` 存放位置 | M24C02 地址 `0x00` 起，80 字节 | App（`M24C02_WriteOTAInfo`） | bootloader（`M24C02_ReadOTAInfo`） |
| 结构体 | `OTA_InfoCB{ uint32 OTA_FLAG; uint32 Firelen[11]; uint8 OTA_ver[32]; }` | — | — |
| "有固件待搬运"标志 | `OTA_FLAG == 0x03020100` | App：升级后置位 | bootloader：上电判断 |
| 固件长度 | `Firelen[0]` = 字节数，**必须 4 的倍数且 ≤ 45056(44KB)** | App | bootloader |
| 暂存位置 | W25Q64 **第 0 块**（地址 `0x000000~0x00FFFF`） | App | bootloader |
| 固件开头 8 字节 | `[0..3]`=初始 MSP（`0x2000xxxx`，8 字节对齐）；`[4..7]`=复位入口（`0x0800xxxx` 且为奇数） | App（就是 App 自己的向量表） | bootloader 校验 |
| 搬运成功后 | bootloader 把 `OTA_FLAG` 清 0 并写回 EEPROM | bootloader | App（下次读） |
| 版本号字符串 | 26 字节，格式 `VER-x.y.z-Y/M/D-h:m` | App | bootloader（菜单 `4` 查询） |

> ⚠️ 建议把上面这张表落成 **`ota_contract.h`**（只放宏 + 结构体，不放代码），
> bootloader 工程和 App 工程都 include 它，从根上消除"两边写歪"。

---

## 六、硬件资源分配（基于你工程的实测占用）

| 外设 | 引脚 | 用途 | 状态 |
|---|---|---|---|
| USART1 | **PA9 / PA10** | bootloader 命令行 + XMODEM（921600） | 已占 |
| SPI1 | PA5(SCK) / PA6(MISO) / PA7(MOSI) | W25Q64 | 已占 |
| GPIO | **PA4** | W25Q64 片选 CS | 已占 |
| 软件 I2C | **PB6(SCL) / PB7(SDA)** | M24C02 | 已占 |
| SWD | PA13 / PA14 | 调试下载（务必保留） | 保留 |
| **USART2** | **PA2(TX) / PA3(RX)** | **建议接 4G 模组** | 空闲 ✅ |
| （备选 USART3） | PB10(TX) / PB11(RX) | 或接 4G | 空闲 ✅ |
| 4G 控制脚 | 任选：PB0/PB1/PA8/PC13… | PWRKEY / RESET / STATUS | 空闲 ✅ |

**DMA 通道也别撞车**（F103C8 的 DMA1）：

| 通道 | 现有用途 | 说明 |
|---|---|---|
| DMA1_CH4 / CH5 | USART1 TX / RX | 已占 |
| **DMA1_CH6 / CH7** | **USART2 RX / TX** | 建议给 4G 用 |
| DMA1_CH2 / CH3 | USART3 RX / TX | 备选 |

**建议**：4G 模组接 **USART2(PA2/PA3) + DMA1_CH6/CH7 + IDLE 中断**，
把现有 `hardware/my_uart.c` 那套"**DMA + IDLE 不定长接收 + 帧指针环形队列**"直接移植一份给 USART2。
（你那套驱动已经调通了，还写过调试日志总结，是最现成的复用资产。）

---

## 七、4G 侧状态机（net_4g_* 的核心）

```
        ┌──────────────┐
        │  POWER_ON    │  拉 PWRKEY 时序 → 等模组就绪(AT 通)
        └──────┬───────┘
               ▼
        ┌──────────────┐  发 AT / ATE0 / AT+CPIN? / AT+CSQ
        │  AT_READY    │  失败重试 N 次 → 超时则硬件复位模组
        └──────┬───────┘
               ▼
        ┌──────────────┐  AT+CREG?/AT+CGATT? 等注网
        │  NET_READY   │  周期上报信号质量(CSQ)
        └──────┬───────┘
               ▼
        ┌──────────────┐  激活 PDP(AT+QIACT / AT+CGACT)
        │  PDP_ACTIVE  │
        └──────┬───────┘
               ▼
        ┌──────────────┐  MQTT CONNECT / TCP 连接 + 订阅 OTA 主题
        │  CONNECTED   │◄──────────────┐
        └──────┬───────┘               │ 断线重连（退避 1s→2s→…→60s）
               ▼                       │
        ┌──────────────┐  心跳 PINGREQ  │
        │   IDLE       │───────────────┘
        │ 监听 URC/OTA │
        └──────┬───────┘
               ▼ 收到升级通知（含版本、长度、CRC）
        ┌──────────────┐  分片接收 → 每片写 W25Q64 → 累计 CRC
        │  OTA_DOWNLOAD│  进度上报；期间心跳交给 URC 处理
        └──────┬───────┘
               ▼ 全部收完
        ┌──────────────┐  整体 CRC 比对 → 写 EEPROM(标志+长度+版本)
        │ OTA_VERIFY   │  校验失败：上报失败、清标志、留在 App
        └──────┬───────┘
               ▼ 成功
        ┌──────────────┐  NVIC_SystemReset() → bootloader 搬运 → 新程序运行
        │  OTA_COMMIT  │
        └──────────────┘
```

**状态机设计要点**
- 每个状态都有 **超时 + 重试上限**，超出就"降级回上一层状态"（如 CONNECTED → PDP_ACTIVE → NET_READY）。
- **下载态要临时改变"谁来吃串口数据"**：正常态由 `net_at` 解析 AT 响应/URC；下载态切到大缓冲/透传模式，避免逐片 AT 命令的开销。
- **看门狗**：网络等待要喂狗；**写 W25Q64 时不喂**（宁可复位重来，也不要半写状态）——或者更稳妥：写入过程中不启用看门狗复位，只在网络态启用。

---

## 八、必须提前想清楚的"坑"

| # | 坑 | 后果 | 建议 |
|---|---|---|---|
| 1 | W25Q64 页写必须 256B 对齐、跨页会回卷 | 固件内容错位 | App 侧复用 bootloader 的 `W25Q64_Write1K()` 思路：整页对齐写 |
| 2 | 固件 > 44KB | bootloader 拒收（`FW_MAX_SIZE`） | 服务器下发前校验；或扩展 `Firelen[]` 支持多块（改两块代码） |
| 3 | 只有 `Firelen` 没有整体 CRC | 内容坏了也照样搬 | 强烈建议在契约里加 **CRC32**（EEPROM 还有空间，见下） |
| 4 | MQTT QoS1 会**重复投递** | 分片重复写、镜像错乱 | 分片要**幂等**：按 offset 覆盖写，或维护"已收分片位图" |
| 5 | 写入过程中断电 | A 区被擦一半 | 现在的语义已能"再搬一次"；要**回滚**就再加一个"备份块" |
| 6 | 4G 模组峰值电流 2A | 下载中掉电重启、升级数据损坏 | 单独 LDO + 大电容（≥470µF）+ 走线加粗 |
| 7 | 升级过程中 App 还在跑业务 | 数据竞争、异常看门狗 | 进入 OTA 态后暂停业务、关闭不必要中断 |
| 8 | EEPROM 结构改动 | 两边不一致 → 升级失败 | 用 `ota_contract.h`；结构体加 `version` 字段做兼容判断 |

**关于 EEPROM 空间**：`M24C02` 有 256 字节，现在 `OTA_InfoCB` 只占 80 字节
（`OTA_FLAG` 4 + `Firelen[11]` 44 + `OTA_ver` 32）。还剩 176 字节，
足够加 `OTA_CRC32`、`OTA_STATE`、`OTA_RETRY`、`OTA_SIZE` 等字段。
**但注意**：`M24C02_WriteOTAInfo()` 是按 `sizeof(OTA_InfoCB)/8` 循环写页的，
结构体大小必须是 8 的倍数（现在 80 正好），改完要确认这一点。

---

## 九、建议的开发顺序（里程碑）

| 阶段 | 目标 | 验收标准 |
|---|---|---|
| M1 | USART2 + DMA + IDLE 收帧驱动（移植 `my_uart.c`） | 用串口助手/PC 发数据，MCU 能正确收到不定长帧 |
| M2 | `net_at` AT 通道 | 手发 `AT` 命令，能打印模组回应；URC 能异步打印 |
| M3 | `net_4g_*` 注网 + 连接 | 能打印 IP、CSQ、连接状态；断网能自动重连 |
| M4 | `mqtt_client` | 能连上服务器、订阅主题、收到一条测试消息 |
| M5 | `ota_storage` + `ota_client`（**先不接网络**） | 用一段固定数据当"固件"，走完"写 W25Q64 → 置标志 → 复位 → bootloader 搬运 → 跳转" ✅ **这一步就能验证与 bootloader 的契约** |
| M6 | 打通云端 | 服务器下发固件，端到端完成一次远程升级 |
| M7 | 救援路径（方案 B） | 把 A 区擦坏，用透传链路远程 XMODEM 救回 |

> **M5 是最关键的里程碑**：它把"网络"这个最大变量排除掉，
> 先证明 **App 写 W25Q64 + EEPROM 契约 + bootloader 搬运** 这条链路是对的。
> 这也是我们这次已经把 bootloader 侧做完的部分，你可以直接拿来当靶子。

---

## 十、需要你先拍板的 6 个决策点

1. **4G 模组型号**？（EC800 / Air780 / ML307 / 其他）—— 决定 AT 指令集、是否内置 MQTT、以及 `net_4g_port.h` 的内容。
2. **云端协议**：MQTT 还是 HTTPS 还是私有 TCP？固件下发是"服务器推"还是"设备拉"？
3. **是否需要断点续传/差分升级**？（决定 `ota_storage` 是否要位图/偏移索引）
4. **是否需要回滚**？（决定 W25Q64 是否要留"备份块"，`OTA_Info` 是否加"上一版"字段）
5. **App 用什么调度**：裸机时间片（省 RAM/Flash）还是 FreeRTOS（20KB RAM 的 F103C8 跑 RTOS + 4G 会比较紧）
6. **升级时设备能不能停机**？（决定"暂停业务"的实现方式，以及是否要双 App 槽位）

---

## 十一、一句话总结每一层的"为什么"

| 层/决策 | 为什么 |
|---|---|
| 4G 放 App 不放 bootloader | bootloader 只剩 2488B 空间，且必须"永不失败" |
| App 只写 W25Q64，不写 A 区 | App 自己运行在 A 区，**不能擦自己**（这正是功能 5/6 存在的理由） |
| 用 EEPROM 传标志 | 复位后 RAM 全丢，只有 EEPROM 能跨复位，且 bootloader 早就在读它 |
| AT 通道与业务分层 | AT 收发/超时/URC 与"下载固件"无关；换模组只改 `net_4g_port.h` |
| 契约放共用头文件 | 消除"bootloader 和 App 两边约定不一致"这类最致命的问题 |
| 模组接 USART2 并复用 `my_uart.c` | USART1 已被命令行/XMODEM 占用，PA2/PA3 空闲；DMA 通道也不冲突 |
| 先做 M5 里程碑 | 先用固定数据打通"App→W25Q64→EEPROM→bootloader 搬运"，把网络变量隔离掉 |
| 并发/看门狗策略 | 网络超时不能拖死系统；但写 Flash 期间被复位会留下半写镜像 |
