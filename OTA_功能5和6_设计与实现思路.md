# 功能 5 / 6 实现说明：外部 Flash(W25Q64) 暂存与回搬 A 区

> 对应菜单：
> - `[5] Download Firmware to External Flash` —— 用 XMODEM 把固件下载到 **W25Q64** 暂存
> - `[6] Run Firmware from External Flash` —— 把 W25Q64 里的固件搬进 **A 区** 并运行
>
> 涉及文件：`hardware/boot.c`、`hardware/boot.h`、`Core/Inc/main.h`、`Core/Src/main.c`
> 编译验证：Keil MDK / ARMCC V5.06，**0 Error / 0 Warning**，镜像 **17992 B**（20KB 上限，余 2488 B）

---

## 一、先说清楚这两个功能到底解决什么问题

你现在的 bootloader 有两条升级路径：

| 路径 | 数据流向 | 特点 |
|---|---|---|
| **功能 2**（原有） | 串口 → XMODEM → 直接写内部 Flash A 区 | 快、省事；但**下载过程中一旦断线/掉电，A 区已经被擦掉一半**，旧程序也没了，板子变砖 |
| **功能 5 + 6**（本次新增） | 串口 → XMODEM → **W25Q64 暂存** → 校验 → 搬进 A 区 | 多一步，但**下载全程不碰 A 区**；下载失败只是 W25Q64 里有半份垃圾，A 区里的旧程序照样能跑 |

一句话：**功能 5/6 是"带缓冲的安全升级"，功能 2 是"裸奔式升级"。**

---

## 二、整体数据流

```
                  ┌──────────────────────────────────────────────┐
   串口 XMODEM ───►│ 功能 5：收 128B 包 → 8 个包凑 1KB → 写 W25Q64 │
   (921600)        └───────────────────────┬──────────────────────┘
                                           │ 满 1KB 就写 4 页(256B/页)
                                           ▼
                              ┌────────────────────────────┐
                              │ W25Q64 第 0 块 (0x000000)  │  8MB 外部 Flash
                              │ 固件副本，最大 44KB        │
                              └────────────┬───────────────┘
                                           │
                        EOT：把长度记进 OTA_Info.Firelen[0]
                             置 OTA_Flag = OTA_SET_FLAG
                             存进 M24C02 EEPROM（掉电不丢）
                                           │
              ┌────────────────────────────┴─────────────────────────────┐
              │                                                          │
   功能 6：人工按 6                                     重新上电（自动升级路径）
              │                                                          │
              │  ① 先读 W25Q64 头 8 字节，验向量表                        │
              │     ├─ 不合法 → 打印 "Bad vector, A kept"，**不擦 A 区**   │
              │     └─ 合法   → ② 擦除 A 区(44 页)                        │
              │                 ③ 1KB 一块：W25Q64读→RAM→内部Flash写      │
              │                    每块写完立刻回读比对                    │
              │                 ④ 清 OTA_Flag 并写回 EEPROM               │
              │                 ⑤ LOAD_A(0x08005000) 跳转                │
              ▼                                                          ▼
        ┌──────────────────────────────────────────────────────────────────┐
        │ 内部 Flash A 区 0x08005000~0x0800FFFF (44KB)  ← 新固件在这里运行  │
        └──────────────────────────────────────────────────────────────────┘
```

自动升级路径的代码在 `Core/Src/main.c` 主循环里（原来是一段注释，本次补全）：

```
上电 → bootloder_Brance()
        ├─ 读 EEPROM 的 OTA_Info
        ├─ 给 2 秒机会按 'w' 进命令行
        └─ 没按 'w' 且 OTA_FLAG == OTA_SET_FLAG
              → BootStaFlag |= UPDATA_A_FLAG  → 主循环执行搬运 → 跳 A 区
           没按 'w' 且 OTA_FLAG != OTA_SET_FLAG
              → 直接 LOAD_A(0x08005000)   （和以前一样）
```

---

## 三、三个"存储区"的分工

| 存储 | 地址/大小 | 放什么 | 为什么放这里 |
|---|---|---|---|
| 内部 Flash（bootloader） | `0x08000000` + 20KB | bootloader 自身 | 必须全体待在 `0x08005000` 以下，否则会擦到自己（上次的坑） |
| 内部 Flash（A 区） | `0x08005000` + 44KB | 真正运行的 App | 复位入口/向量表就在 `0x08005000` |
| 外部 Flash W25Q64 | `0x000000` + 64KB（第 0 块） | 待升级固件的**暂存副本** | 掉电不丢、可以反复重写、擦除不影响运行中的程序 |
| EEPROM M24C02 | 0x00 起 80 字节 | `OTA_Info`（标志位 + 长度表 + 版本号） | 只有 80 字节，但**掉电不丢**，用来记录"W25Q64 里有一份待升级固件" |

`OTA_Info` 结构（`Core/Inc/main.h`）：

```c
typedef struct {
    uint32_t OTA_FLAG;      /* == OTA_SET_FLAG(0x03020100) 表示"有待升级固件" */
    uint32_t Firelen[11];   /* 每个 64KB 块里的固件长度；A 区只用 Firelen[0] */
    uint8_t  OTA_ver[32];   /* 版本号字符串 */
} OTA_InfoCB;               /* 共 80 字节 = M24C02 的 10 个页 */
```

---

## 四、关键设计决策（重点：为什么这样写）

### 1) 为什么功能 5 要写到 W25Q64，而不是直接写 A 区？

因为 **bootloader 自己运行在内部 Flash 上**。如果按功能 2 那样一边收一边擦写 A 区：

- 下载到一半断线 → A 区已被擦掉一部分 → 旧程序废了，新程序不完整 → 只能拆下来用 ST-Link 救；
- 写内部 Flash 时 CPU 会因为 Flash 忙而停住，XMODEM 的 ACK 被拖慢，容易超时重传。

写到 W25Q64 就没有这两个问题：**擦除/写入都不影响正在运行的 bootloader**，也不影响 A 区里跑着的旧程序。

### 2) 为什么 W25Q64 必须先擦后写，而且必须按 256 字节一页写？

- W25Q64 的 NOR Flash 特性：**只能把 1 写成 0，不能把 0 写成 1**。所以写之前必须先把整块擦成 `0xFF`（块擦除指令 `0xD8`，一次 64KB，约 150ms）。代码里在按 `5` 的第一时间就执行 `W25Q64_Erase64K(0)`。
- 页编程指令 `0x02` **一次最多写 256 字节，且起始地址必须 256 对齐**。如果写超过一页或者地址没对齐，超出的部分会在**页内回卷**，把这一页开头的数据覆盖掉 —— 这是 W25Q64 最经典的坑。

1KB 的缓冲因此被拆成 4 次页写，这一步单独封装成 `W25Q64_Write1K()`：

```c
static uint8_t W25Q64_Write1K(uint32_t waddr)
{
    uint8_t  k;
    uint16_t page;

    if ((waddr + (uint32_t)STM_PAGE_SIZE) > W25Q64_BLOCK_SIZE)
        return 1U;                              /* 会跨出 64KB 块：拒绝 */

    page = (uint16_t)(waddr / 256U);            /* 用"地址"换算页号，避免两套编号混用 */

    for (k = 0U; k < (STM_PAGE_SIZE / 256U); k++)   /* 1024/256 = 4 页 */
        W25Q64_PageWrite(&updataA.updatabuff[(uint32_t)k * 256U], (uint16_t)(page + k));

    return 0U;
}
```

### 3) 为什么"最后不足 1KB 的部分"也要按整 1KB 写？

XMODEM 是按 128 字节包发的，1KB = 8 个包。最后一个包之后如果不满 8 个包（例如只收到 3 个包 = 384 字节），直接写 384 字节会**跨页**（因为它在 1KB 里不是 256 的整数倍）。

所以 EOT 时干脆把"包含这 384 字节的整 1KB"一起写下去：

```c
if(remain)                                  /* remain = (xmodem_NB % 8) * 128 */
    W25Q64_Write1K(total - remain);         /* total-remain 一定 1KB 对齐 */
```

多写进去的那几百字节是 RAM 缓冲里的**残留数据**，但因为固件真正的长度记在 `Firelen[0] = total`，**回搬时根本不会读到 `total` 之后的字节**，所以完全无害。这样就不用为了"页对齐"再准备第二个缓冲。

### 4) 为什么功能 6 要"先验向量表，再擦 A 区"？

顺序颠倒的后果很严重：如果 W25Q64 里是空的（比如用户没按过 5 就直接按 6），或者外部 Flash 没接好（读回来全是 `0xFF`），先擦 A 区就等于**把唯一能跑的固件也毁了**。

所以先把头 8 字节读出来当作"向量表"检查一遍（判据和 `LOAD_A()` 完全一致）：

```c
W25Q64_Read(updataA.updatabuff, fwbase, 8U);
msp = *(volatile uint32_t *)&updataA.updatabuff[0];   /* 第 1 个字 = 初始 MSP */
pc  = *(volatile uint32_t *)&updataA.updatabuff[4];   /* 第 2 个字 = 复位入口  */

if ((msp < 0x20000100U) || (msp > STM_INIT_MSP_MAX) || ((msp & 0x07U) != 0U) ||
    (pc  < STM_A_ADDR)   || (pc  > STM_A_END_ADDR)   || ((pc  & 0x01U) == 0U))
{
    u0_printf("Bad vector, A kept\r\n");
    return 1U;          /* 直接返回，不擦 A 区 */
}
```

顺带的好处：**W25Q64 没插/没接好时，这个检查会兜住**（读到 `0xFFFFFFFF` 必然不合法），不需要额外写"读 ID"的探测代码。

### 5) 为什么固件长度要记在 `Firelen[]` 里，它为什么天然满足 4 字节对齐？

内部 Flash 只能按 **4 字节字**写，而 `STM32_WriteFlash()` 的形状是"`wnum` 字节、必须是 4 的倍数"。

XMODEM 的每个数据包是 128 字节，所以 `xmodem_NB * 128` 一定是 **128 的倍数**，而 128 是 4 的倍数 → **`Firelen[0]` 天然 4 字节对齐**，不需要额外补齐。这也让 `BootLoader_UpdateAFromW25Q64()` 里"每块搬 `n` 字节"的 `n` 永远是 4 的倍数。

```c
total = updataA.xmodem_NB * 128U;              /* 一定是 4 的倍数 */
OTA_Info.Firelen[FW_STORE_BLOCK] = total;
```

> 代价：XMODEM 会在最后一个包里用 `0x1A` 填充到 128 字节，所以 A 区尾部会多出几个 `0x1A`。
> 这不影响运行 —— App 的向量表在开头，尾部填充没人读。

### 6) 为什么拷贝时要"每 1KB 回读校验"？

内部 Flash 写入失败通常**不会报错**（`HAL_FLASH_Program` 返回 OK 但内容是错的，比如没擦干净、写保护、电压不足）。回读比对是唯一便宜的兜底手段：

```c
pram   = (uint32_t *)updataA.updatabuff;
pflash = (uint32_t *)(STM_A_ADDR + i);
for (j = 0U; j < (n / 4U); j++)
{
    if (pflash[j] != pram[j]) { u0_printf("verify fail\r\n"); return 1U; }
}
```

代价只有几十字节代码，收益是"绝不把一份错的固件当成好的跳过去"。

### 7) 为什么只用一个 64KB 块？`Firelen[11]` 不是能记 11 块吗？

因为 **A 区只有 44KB**。固件最终必须完全装进 A 区才有意义，44KB < 64KB，所以一个块足够。

`Firelen[11]` 是为"未来 A 区变大/B 区方案"预留的。本次实现里：

- 下载超过 44KB → 直接发 `CAN(0x18)` 中止；
- 搬运时只用 `FW_STORE_BLOCK = 0` 这一块。

想支持多块，只要把 `FW_STORE_BLOCK` 换成变量、按 `地址 / 65536` 推块号，并把每块长度分别写进 `Firelen[]` 即可（文档末尾"可扩展"一节有说明）。

### 8) 为什么用一个标志位 `CMD5_XMODEM_FLAG` 复用整套 XMODEM 流程？

功能 2 和功能 5 的**收包逻辑完全一样**（校验包号、CRC、组 1KB、EOT 收尾），唯一区别是"1KB 落到哪里"。所以不要去复制一份 XMODEM 状态机，而是在落盘那一步分叉：

```c
if(BootStaFlag & CMD5_XMODEM_FLAG)      /* 功能 5：落外部 Flash */
    W25Q64_Write1K(waddr);
else                                    /* 功能 2：落内部 Flash A 区 */
    STM32_WriteFlash(...);
```

好处：包序号校验、重传包处理、CRC 这些修复（上一轮修过的坑）**两条路径同时受益**，将来只改一处。

---

## 五、代码结构与调用关系

```
Core/Inc/main.h
  ├─ STM_A_SIZE          = (64-20)*1024 = 44KB     A 区容量
  ├─ W25Q64_BLOCK_SIZE   = 64KB                    W25Q64 块大小
  ├─ FW_STORE_BLOCK      = 0                       固件暂存的块号
  ├─ FW_MAX_SIZE         = STM_A_SIZE              固件上限
  └─ CMD5_XMODEM_FLAG    0x20                      "本次 XMODEM 写外部 Flash"

hardware/boot.h
  └─ uint8_t BootLoader_UpdateAFromW25Q64(void);   ← 功能 6 的核心，也供上电自动升级调用

hardware/boot.c
  ├─ static uint8_t W25Q64_Write1K(uint32_t waddr)          1KB → 4 页写
  ├─ uint8_t BootLoader_UpdateAFromW25Q64(void)             验向量表→擦A→搬运→校验→跳转
  └─ void bootloder_Event(...)
       ├─ '5' 分支：擦 W25Q64 块 → 打开 XMODEM(带 CMD5_XMODEM_FLAG)
       ├─ '6' 分支：调用 BootLoader_UpdateAFromW25Q64()
       └─ XMODEM 分支
            ├─ 收包(133B) → 满 1KB → 按 CMD5_XMODEM_FLAG 分叉落盘
            └─ EOT(0x04)  → 功能5：写尾块 + 存长度/标志 + 回菜单
                             功能2：写尾块 + 复位（原逻辑）

Core/Src/main.c
  └─ 主循环 if(BootStaFlag & UPDATA_A_FLAG) → BootLoader_UpdateAFromW25Q64()
```

### 标志位速查

| 标志 | 值 | 含义 | 谁置位 | 谁清位 |
|---|---|---|---|---|
| `UPDATA_A_FLAG` | 0x01 | "请把 W25Q64 里的固件搬进 A 区" | `bootloder_Brance()`（上电时看 EEPROM） | 搬运前/搬运后 |
| `IAP_XMODEMC_FLAG` | 0x02 | 每 100ms 发 `C` 催主机开始 | 命令 2 / 5 | EOT 或出错 |
| `IAP_XMODEMD_FLAG` | 0x04 | XMODEM 收包状态机使能 | 命令 2 / 5 | EOT 或出错 |
| `CMD5_XMODEM_FLAG` | 0x20 | **本次数据写 W25Q64**（而不是 A 区） | 命令 5 | EOT 或出错 |
| `SET_VERSION_FLAG` | 0x08 | 下一帧 26 字节当版本号 | 命令 3 | 校验成功后 |

---

## 六、操作手册

### 方式 A：手动升级（推荐先这样验证）
```
1) 上电，2 秒内按 w 进命令行（或等它自己进 A 区后按复位再来）
2) 按 5          → 串口打印 "Download to W25Q64"
3) 串口助手切 XMODEM(128B + CRC16)，发送 App 的 .bin
   → 每包回 ACK(06)，结束打印 "OK xxxx B" 并列菜单
4) 按 6          → 打印 "Ext len=... / MSP=... PC=... / Run A"
   → 搬运完自动跳进 A 区运行
（此时 A 区里原来的程序被替换；如果第 4 步之前断电，A 区旧程序仍然可跑）
```

### 方式 B：全自动升级（现场无人值守）
```
1) 按 5 把固件下载进 W25Q64（此时 EEPROM 已置 OTA_FLAG）
2) 按 7 复位（或直接断电再上电）
3) 2 秒内不按 w  →  bootloader 读到 OTA_FLAG，自动搬运并运行新固件
   （搬运成功后会清掉 OTA_FLAG，下次上电不会重复搬运）
```

### 串口参数
`921600, 8, N, 1`，**不要勾"发送新行"**（XMODEM 是二进制协议）。

---

## 七、验证清单（怎么确认真的成功）

| 检查点 | 期望现象 | 说明 |
|---|---|---|
| 按 5 后 | 打印 `Download to W25Q64` | W25Q64 块擦除已完成 |
| 下载中 | 每包一个 `06` | CRC 正确；若回 `15` 是重传，正常 |
| EOT 后 | `OK xxxx B`，xxxx = 固件字节数（128 的倍数） | 长度已存进 EEPROM |
| 按 6 后 | `Ext len=xxxx` / `MSP=0x2000xxxx PC=0x0800xxxx` | 头 8 字节读出来是对的向量表 |
| 搬运中 | 逐块无 `verify fail` | 每 1KB 回读都一致 |
| 搬运后 | `Run A`，随后 App 自己的输出 | 跳转成功 |
| 失败时 | `Bad vector, A kept` | **A 区未被破坏**，旧程序还能跑 |
| 超长时 | `W25Q64 err` + 主机收到 `18`(CAN) | 固件 > 44KB 被拒 |

**用 Memory 窗口复核**（最硬的证据）：
- `0x08005000` 前 8 字节应为 `合法 MSP(0x2000xxxx)` + `合法 PC(0x0800xxxx 奇数)`
- 与 App 工程 `.map` 里的 `__initial_sp`、`Reset_Handler` 完全一致

---

## 八、Flash 预算：为什么顺手把优化等级改成了 -O1

补完功能 5/6 后第一次编译**超了 20KB**：

| 阶段 | Total ROM | 说明 |
|---|---|---|
| 未加 5/6（-O0） | 18008 B | 余量 2472 B |
| 加完 5/6（-O0，未精简） | 20988 B | **超 508 B，链接失败** |
| 加完 5/6（精简打印 + **-O1**） | **17992 B** | 余量 **2488 B** ✅ |

多出来的 2980 字节主要来自：
- **HAL SPI 状态机**（`HAL_SPI_Transmit/TransmitReceive`）约 1030 B —— 之前从没被调用过，现在 W25Q64 要用；
- **W25Q64 驱动**（Read/PageWrite/Erase64K/WaitBusy/WaitEnable）约 378 B —— 原来也只被注释掉的代码引用；
- 新增函数本体 `BootLoader_UpdateAFromW25Q64` 242 B + `W25Q64_Write1K` 48 B；
- XMODEM 分支变复杂，`bootloder_Event` 变大；
- 一堆调试字符串。

两处应对（都在工程/源码里改好了）：
1. **精简调试字符串**：新功能的打印从 17 条压到 9 条并缩短（`"Ext FW vector error, A kept"` → `"Bad vector, A kept"` 等），两条中止路径合并共用一段收尾；
2. **优化等级 `Level 0 (-O0)` → `Level 1 (-O1)`**（`MDK-ARM/bootloder.uvprojx` 的 `<Optim>`）。

> **关于 -O1 的安全性**：这个工程唯一对时序敏感的是软件模拟 I2C（`hardware/my_iic.c`），
> 而它的所有延时都是 `delay_us()` 函数调用，`delay_us()` 内部读的是 volatile 的 `SysTick->VAL`，
> 任何优化等级都不会把它优化掉；GPIO 读写走 HAL 外部函数，也不会被重排。
> 所以 -O1 对 I2C 时序是安全的。
> 如果以后你出于别的原因要退回 -O0，那这 17992 B 会变回约 21KB，
> 届时要另想办法（例如把 `STM_B_PAGE_NUM` 从 20 改成 24，A 区起点挪到 `0x08006000`，bootloader 就有 24KB）。

---

## 九、已知限制与可扩展方向

| 限制 | 现状 | 想扩展怎么做 |
|---|---|---|
| 只支持单个 64KB 块 | `FW_STORE_BLOCK = 0`，固件上限 44KB | 用 `地址/65536` 推块号，每块长度写进 `Firelen[blockNB]`，块切换时先 `W25Q64_Erase64K` |
| 没有整体 CRC/校验和 | 只做了"向量表合法性 + 逐块回读" | 在 W25Q64 里固件尾部存 4 字节 CRC32，搬运前整体校验 |
| 搬运中断电 | A 区会被擦一半 | 存一份"备份固件"到 W25Q64 的另一块，上电发现 `OTA_FLAG` 未清就重搬（本实现已能重搬，但备份块的机制未做） |
| 不支持外部 Flash 直接运行(XIP) | `[6]` 是"搬进 A 区再跑" | STM32F1 不支持从 SPI Flash 取指，只能搬 |
| 没有校验 W25Q64 器件 ID | `W25Q_ReadID()` 现成但未用 | 按 5 时先 `if((W25Q_ReadID() & 0xFFFF) != 0xEF17) 报错返回`；不过"向量表预校验"已经能兜住没接好的情况 |
| 版本号与固件没有绑定 | `OTA_ver` 独立存在 EEPROM | EOT 时把 `OTA_ver` 一起写进 W25Q64 尾部，搬运后由 App 读 |

---

## 十、一句话总结每个设计的"为什么"

| 做法 | 为什么 |
|---|---|
| 先下到 W25Q64 | 下载失败不毁 A 区，旧程序永远是"可回退的底" |
| 先擦块再写 | NOR Flash 只能 1→0，不擦写不进去 |
| 1KB 拆 4 页、最后按整 1KB 写 | 页编程上限 256B 且必须页对齐，否则页内回卷 |
| 搬前先验向量表 | 唯一的"防止把好固件擦掉换垃圾"的门闩 |
| 逐块回读校验 | 内部 Flash 写失败常常不报错，只能靠自己比对 |
| 长度记 EEPROM | 掉电后仍知道 W25Q64 里是什么、有多长、要不要升级 |
| 复用 XMODEM 状态机 | 一份协议处理代码，两个落地目标，修一次两个功能都受益 |
| 超 44KB 直接 CAN | A 区装不下，收完也没用，早停早省事 |
| 搬运成功才清 OTA_FLAG | 保证"升级没成功就还会再试一次" |
