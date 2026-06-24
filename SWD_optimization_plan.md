# SWD 速度优化方案 — AT32F403A (240MHz)

> 基于 H7-TOOL 项目 SWD 优化方案的移植分析
> 目标芯片：AT32F403A @ 240MHz，GPIO 模拟 SWD (PC0=SWDIO, PC1=SWCLK)

---

## 一、现状分析

### 1.1 当前瓶颈

| 项目 | 当前值 | 问题 |
|------|--------|------|
| `DAP_Data.fast_clock` | **0** (始终关闭!) | 快速模式从未启用 |
| `DAP_DEFAULT_SWJ_CLOCK` | 1 MHz | 默认只有 1MHz |
| 数据发送 | `for` 循环逐位发送 | 循环开销大 |
| 奇偶校验 | 循环中逐位累加 | O(32) 每32位 |
| 引脚方向切换 | `gpio_init()` 库函数 | 极慢（函数调用+多寄存器操作） |
| 时钟周期 | SWCLK/SWDIO 分别操作 | 两个独立寄存器写 |

### 1.2 当前真实速度分析

> ⚠️ 以下为修正后的精确计算。之前的 85 KB/s 估算是错误的。

#### 实际 SWCLK 频率

`CLOCK_DELAY(1MHz) = (240M/2/1M) - 2 = 118`

`PIN_DELAY_SLOW(118)` → `while(--count)` 在 Cortex-M4 上每条迭代约 **3 个周期**（subs + cmp + bne）。

每次 delay ≈ `118 × 3 = 354 CPU 周期`。

一个 SWCLK 周期（`CLR → DELAY → SET → DELAY`）≈ **708 周期 @240MHz = 2.95 µs**。

**实际 SWCLK ≈ 339 kHz**（远低于名义上的 1 MHz）。

#### 有效数据吞吐量（含协议开销）

写一个 32-bit word 到目标内存需要 **4 次 SWD 传输**：
1. `swd_write_ap(CSW)` — 设置访问宽度
2. `swd_write_ap(TAR)` — 设置目标地址
3. `swd_write_ap(DRW)` — 写数据
4. `DP_RDBUFF` dummy read — 等待完成

每次 SWD 传输约 **46 个 SWCLK 周期**（请求头 8bit + ACK 3bit + 数据 33bit + turnaround）。

4 次传输 × 46 周期 × 2.95 µs ≈ **543 µs/word (4 bytes)**。

**有效吞吐量 ≈ 4 / 543µs ≈ 7.4 KB/s**。

#### 92KB 烧录 30 秒的耗时分解

```
┌──────────────────────────────────────────────────────────┐
│ 阶段                      耗时        占比     瓶颈      │
├──────────────────────────────────────────────────────────┤
│ 1. Flash 全片擦除        ~15-18 秒   50-60%   目标芯片   │
│    （物理擦除，与SWD完全无关）                            │
│                                                          │
│ 2. 下载 Flash 算法到RAM    ~0.5 秒     ~2%     SWD写     │
│                                                          │
│ 3. 逐页烧录 92KB          ~5-8 秒    20-25%   混合       │
│    每页: SWD写1KB→135ms + Flash写→15ms + 轮询→5ms       │
│                                                          │
│ 4. 校验（读回或CRC）       ~5-7 秒    20-25%   SWD读     │
│    SWD读比写更慢（dummy read开销）                        │
└──────────────────────────────────────────────────────────┘
```

**结论**：30 秒中有 **50-60% 是目标 Flash 的物理擦除时间**，与 SWD 速度完全无关。SWD 传输本身只占约 **5-8 秒**。

#### 优化后的预期改善

SWD 快速模式优化主要加速阶段 2、3、4 中的 SWD 传输部分。Flash 擦除和物理写入时间不变。

| 阶段 | 优化前 | 优化后 | 节省 |
|------|--------|--------|------|
| Flash 擦除 | ~16 秒 | ~16 秒 | **0**（与SWD无关） |
| SWD 数据传输 | ~8 秒 | ~1 秒 | **~7 秒** |
| Flash 物理写入 | ~3 秒 | ~3 秒 | **0**（目标芯片决定） |
| 其他开销 | ~3 秒 | ~2 秒 | ~1 秒 |
| **合计** | **~30 秒** | **~22 秒** | **~8 秒 (27%)** |

> 对比 H7-TOOL 的数据：640KB 程序，ST-LINK 擦除 9 秒 + 编程 13 秒 = 22 秒。H7-TOOL 同类优化后总时间缩减有限，因为擦除时间是硬瓶颈。

---

## 二、AT32F403A 与 STM32H7 寄存器对照表

这是最关键的差异。AT32 的 GPIO 寄存器命名和结构与 STM32 不同：

| 功能 | STM32H7 寄存器 | AT32F403A 寄存器 | 说明 |
|------|---------------|-----------------|------|
| 引脚模式配置 (pin 0-7) | `MODER` | `cfglr` | AT32 用 `cfglr` 一次性配置 mode/out_type/pull |
| 引脚模式配置 (pin 8-15) | `MODER` (同一寄存器高位) | `cfghr` | AT32 分成两个寄存器 |
| 输出类型 | `OTYPER` | 在 `cfglr/cfghr` 的 `iofc[0:15]` 字段 | AT32 合并在配置寄存器 |
| 上下拉 | `PUPDR` | 在 `cfglr/cfghr` 的 `iomc[0:15]` 字段 | AT32 合并在配置寄存器 |
| 输出速度 | `OSPEEDR` | `hdrv` 寄存器 / `gpio_drive_strength` | 独立寄存器 |
| 输入数据 | `IDR` | `idt` | 同名不同命 |
| 输出数据 | `ODR` | `odt` | 同名不同命 |
| **原子置位** | `BSRR[15:0]` | **`scr[15:0]`** (`iosb0-15`) | ✅ 功能一致 |
| **原子复位** | `BSRR[31:16]` | **`scr[31:16]`** (`iocb0-15`) | ✅ 功能一致！ |
| 专用复位 | 无独立寄存器 | `clr[15:0]` | AT32 额外提供 |

### ⭐ 关键发现：AT32 的 `scr` 寄存器 = STM32 的 `BSRR`

AT32 的 `scr` 寄存器结构与 STM32 的 `BSRR` **完全等价**：
```
scr 寄存器 (offset 0x10):
  bits 0-15  (iosb): 写 1 → 对应引脚输出高
  bits 16-31 (iocb): 写 1 → 对应引脚输出低
```

**这意味着 H7-TOOL 的 BSRR 单指令同时操作 SWCLK+SWDIO 的技巧可以直接移植！**

```
STM32:  GPIOD->BSRR = GPIO_PIN_4 | (GPIO_PIN_3 << 16);
        // 同时: PD4=H, PD3=L → 单条指令

AT32:   PORTC->scr = GPIO_PINS_0 | (GPIO_PINS_1 << 16);
        // 同时: PC0(SWDIO)=H, PC1(SWCLK)=L → 单条指令！
```

---

## 三、修改清单

### ⚡ 修改1：DAP.h — 添加奇偶校验查表函数

**文件**: `DAP/DAP.h`

在文件末尾 `#endif` 前添加奇偶校验查找表和函数：

```c
// === 奇偶校验查找表 (256字节，预先计算) ===
static const uint8_t ParityTable256[256] = {
#   define P2(n) n, n^1, n^1, n
#   define P4(n) P2(n), P2(n^1), P2(n^1), P2(n)
#   define P6(n) P4(n), P4(n^1), P4(n^1), P4(n)
    P6(0), P6(1), P6(1), P6(0)
};

// O(1) 奇偶校验，替代逐位累加
static __attribute__((always_inline)) uint8_t GetParity(uint32_t data)
{
    data ^= data >> 16;
    data ^= data >> 8;
    return ParityTable256[data & 0xff];
}
```

**原因**: H7-TOOL 使用 256 字节查表法，消除 32 次循环中的逐位 parity 累加。

---

### ⚡ 修改2：DAP.h — 修改 PIN_DELAY_FAST 为 0 延迟

**文件**: `DAP/DAP.h`，约第 262 行

**已确认**: 当前 `DELAY_FAST_CYCLES = 0`，快速模式延迟函数已经是空操作。✅ 无需修改。

---

### ⚡ 修改3：DAP_config.h — 重写 SWD 位操作宏（核心优化）

**文件**: `DAP_Config/DAP_config.h`

**当前代码** (分散操作，有延迟):

```c
#define SW_CLOCK_CYCLE()                \
  PIN_SWCLK_CLR();                      \
  PIN_DELAY();                          \
  PIN_SWCLK_SET();                      \
  PIN_DELAY()

#define SW_WRITE_BIT(bit)               \
  PIN_SWDIO_OUT(bit);                   \
  PIN_SWCLK_CLR();                      \
  PIN_DELAY();                          \
  PIN_SWCLK_SET();                      \
  PIN_DELAY()
```

**替换为** (利用 `scr` 原子操作，零延迟):

```c
// ============ 快速模式（零延迟）的位操作宏 ============
// 利用 scr 寄存器的原子操作：同时控制 SWDIO 和 SWCLK

#define SW_CLOCK_CYCLE_FAST()           \
  PORTC->scr = (GPIO_PINS_1 << 16);     /* 只清 SWCLK */  \
  PORTC->scr = GPIO_PINS_1;            /* 只置 SWCLK */

// 写 1 bit（快速模式）：
//   要写 1 → scr = SWDIO_set | SWCLK_clr → SWDIO=H, SWCLK→L → 再 SWCLK→H
//   要写 0 → scr = SWDIO_clr | SWCLK_clr → SWDIO=L, SWCLK→L → 再 SWCLK→H
#define SW_WRITE_BIT_FAST(bit)          \
  if (bit & 1) {                        \
    PORTC->scr = GPIO_PINS_0 | ((uint32_t)GPIO_PINS_1 << 16);  \
    PORTC->scr = GPIO_PINS_1;           \
  } else {                              \
    PORTC->scr = ((uint32_t)(GPIO_PINS_0 | GPIO_PINS_1) << 16); \
    PORTC->scr = GPIO_PINS_1;           \
  }

// 读 1 bit（快速模式）
#define SW_READ_BIT_FAST(bit)           \
  PORTC->scr = (GPIO_PINS_1 << 16);     /* SWCLK→L */         \
  bit = (PORTC->idt & GPIO_PINS_0) ? 1 : 0;  /* 读 SWDIO */  \
  PORTC->scr = GPIO_PINS_1;            /* SWCLK→H */

// ============ 慢速模式的位操作宏（保留兼容）============
#define SW_CLOCK_CYCLE_SLOW()           \
  PORTC->clr = GPIO_PINS_1;             \
  PIN_DELAY_SLOW(DAP_Data.clock_delay); \
  PORTC->scr = GPIO_PINS_1;             \
  PIN_DELAY_SLOW(DAP_Data.clock_delay)

#define SW_WRITE_BIT_SLOW(bit)          \
  if (bit & 1) { PORTC->scr = GPIO_PINS_0; }  \
  else        { PORTC->clr = GPIO_PINS_0; }   \
  PORTC->clr = GPIO_PINS_1;             \
  PIN_DELAY_SLOW(DAP_Data.clock_delay); \
  PORTC->scr = GPIO_PINS_1;             \
  PIN_DELAY_SLOW(DAP_Data.clock_delay)

#define SW_READ_BIT_SLOW(bit)           \
  PORTC->clr = GPIO_PINS_1;             \
  PIN_DELAY_SLOW(DAP_Data.clock_delay); \
  bit = (PORTC->idt & GPIO_PINS_0) ? 1 : 0; \
  PORTC->scr = GPIO_PINS_1;             \
  PIN_DELAY_SLOW(DAP_Data.clock_delay)
```

**同时删除** DAP_config.h 中低效的 `PIN_SWDIO_OUT_ENABLE()` / `PIN_SWDIO_OUT_DISABLE()` 函数（它们用 `gpio_init()` 太慢），替换为直接寄存器操作：

```c
// 快速切换 SWDIO 方向（直接操作 cfglr 寄存器，比 gpio_init() 快数十倍）
// PC0 对应的 cfglr 位: bit[3:0] = {iofc0, iomc0} = {bit[3:2], bit[1:0]}
// 输出模式: iomc[1:0]=01(输出) + iofc[3:2]=00(推挽) = 0x1
// 输入模式: iomc[1:0]=00(输入) = 0x0

static __inline void PIN_SWDIO_OUT_ENABLE(void)
{
    PORTC->cfglr = (PORTC->cfglr & ~0x0000000F) | 0x00000001;  // PC0→输出
}

static __inline void PIN_SWDIO_OUT_DISABLE(void)
{
    PORTC->cfglr = (PORTC->cfglr & ~0x0000000F);  // PC0→输入（低2位=00）
}
```

---

### ⚡ 修改4：SW_DP.c — 重写快速传输函数（核心优化）

**文件**: `DAP/SW_DP.c`

**替换整个 `#if (DAP_SWD != 0)` 块的内容**。

关键改动：
1. 32 位数据发送完全展开（无 for 循环）
2. 使用查表法奇偶校验
3. 快速/慢速分别使用不同的宏

新代码结构：

```c
#if (DAP_SWD != 0)

// ====== 32位数据展开发送宏（快速模式）======
// 利用 scr 寄存器同时操作 SWDIO + SWCLK
#define SEND_32BIT_ONCE_FAST()          \
  if (val & 1) {                        \
    PORTC->scr = GPIO_PINS_0 | ((uint32_t)GPIO_PINS_1 << 16);  \
    val >>= 1;                          \
    PORTC->scr = GPIO_PINS_1;           \
  } else {                              \
    PORTC->scr = ((uint32_t)(GPIO_PINS_0 | GPIO_PINS_1) << 16); \
    val >>= 1;                          \
    PORTC->scr = GPIO_PINS_1;           \
  }

// ====== 32位数据展开发送宏（慢速模式）======
#define SEND_32BIT_ONCE_SLOW()          \
  if (val & 1) {                        \
    PORTC->scr = GPIO_PINS_0;           \
  } else {                              \
    PORTC->clr = GPIO_PINS_0;           \
  }                                     \
  val >>= 1;                            \
  PORTC->clr = GPIO_PINS_1;             \
  PIN_DELAY_SLOW(DAP_Data.clock_delay); \
  PORTC->scr = GPIO_PINS_1;             \
  PIN_DELAY_SLOW(DAP_Data.clock_delay)

// ====== SWD_TransferFast (重写) ======
uint8_t SWD_TransferFast(uint32_t request, uint32_t *data)
{
  uint32_t ack;
  uint32_t bit;
  uint32_t val;
  uint32_t parity;
  uint32_t n;

  /* Packet Request — 使用快速宏 */
  parity = 0U;
  SW_WRITE_BIT_FAST(1U);                     /* Start Bit */
  bit = request >> 0;
  SW_WRITE_BIT_FAST(bit);                    /* APnDP Bit */
  parity += bit;
  bit = request >> 1;
  SW_WRITE_BIT_FAST(bit);                    /* RnW Bit */
  parity += bit;
  bit = request >> 2;
  SW_WRITE_BIT_FAST(bit);                    /* A2 Bit */
  parity += bit;
  bit = request >> 3;
  SW_WRITE_BIT_FAST(bit);                    /* A3 Bit */
  parity += bit;
  SW_WRITE_BIT_FAST(parity);                 /* Parity Bit */
  SW_WRITE_BIT_FAST(0U);                     /* Stop Bit */
  SW_WRITE_BIT_FAST(1U);                     /* Park Bit */

  /* Turnaround */
  PIN_SWDIO_OUT_DISABLE();
  for (n = DAP_Data.swd_conf.turnaround; n; n--) {
    SW_CLOCK_CYCLE_FAST();
  }

  /* Acknowledge response */
  SW_READ_BIT_FAST(bit);
  ack  = bit << 0;
  SW_READ_BIT_FAST(bit);
  ack |= bit << 1;
  SW_READ_BIT_FAST(bit);
  ack |= bit << 2;

  if (ack == DAP_TRANSFER_OK) {
    if (request & DAP_TRANSFER_RnW) {
      /* Read data — 用查表法 Parity */
      val = 0U;
      for (n = 32U; n; n--) {
        SW_READ_BIT_FAST(bit);
        val >>= 1;
        val  |= bit << 31;
      }
      parity = GetParity(val);               /* ⭐ 查表法 */

      SW_READ_BIT_FAST(bit);
      if ((parity ^ bit) & 1U) {
        ack = DAP_TRANSFER_ERROR;
      }
      if (data) { *data = val; }
      for (n = DAP_Data.swd_conf.turnaround; n; n--) {
        SW_CLOCK_CYCLE_FAST();
      }
      PIN_SWDIO_OUT_ENABLE();
    } else {
      /* Write data — 完全展开的32位发送 */
      for (n = DAP_Data.swd_conf.turnaround; n; n--) {
        SW_CLOCK_CYCLE_FAST();
      }
      PIN_SWDIO_OUT_ENABLE();

      val = *data;
      parity = GetParity(val);               /* ⭐ 查表法 */

      /* ⭐ 32位完全展开，无循环开销 */
      SEND_32BIT_ONCE_FAST(); SEND_32BIT_ONCE_FAST();
      SEND_32BIT_ONCE_FAST(); SEND_32BIT_ONCE_FAST();
      SEND_32BIT_ONCE_FAST(); SEND_32BIT_ONCE_FAST();
      SEND_32BIT_ONCE_FAST(); SEND_32BIT_ONCE_FAST();
      SEND_32BIT_ONCE_FAST(); SEND_32BIT_ONCE_FAST();
      SEND_32BIT_ONCE_FAST(); SEND_32BIT_ONCE_FAST();
      SEND_32BIT_ONCE_FAST(); SEND_32BIT_ONCE_FAST();
      SEND_32BIT_ONCE_FAST(); SEND_32BIT_ONCE_FAST();
      SEND_32BIT_ONCE_FAST(); SEND_32BIT_ONCE_FAST();
      SEND_32BIT_ONCE_FAST(); SEND_32BIT_ONCE_FAST();
      SEND_32BIT_ONCE_FAST(); SEND_32BIT_ONCE_FAST();
      SEND_32BIT_ONCE_FAST(); SEND_32BIT_ONCE_FAST();
      SEND_32BIT_ONCE_FAST(); SEND_32BIT_ONCE_FAST();

      SW_WRITE_BIT_FAST(parity);             /* Write Parity Bit */
    }
    /* Idle cycles */
    n = DAP_Data.transfer.idle_cycles;
    if (n) {
      PORTC->clr = GPIO_PINS_0;              /* SWDIO = 0 */
      for (; n; n--) {
        SW_CLOCK_CYCLE_FAST();
      }
    }
    PORTC->scr = GPIO_PINS_0;                /* SWDIO = 1 */
    return ((uint8_t)ack);
  }

  /* WAIT / FAULT / Protocol Error — 同原逻辑，使用 FAST 宏 */
  if ((ack == DAP_TRANSFER_WAIT) || (ack == DAP_TRANSFER_FAULT)) {
    if (DAP_Data.swd_conf.data_phase && ((request & DAP_TRANSFER_RnW) != 0U)) {
      for (n = 32U+1U; n; n--) { SW_CLOCK_CYCLE_FAST(); }
    }
    for (n = DAP_Data.swd_conf.turnaround; n; n--) { SW_CLOCK_CYCLE_FAST(); }
    PIN_SWDIO_OUT_ENABLE();
    if (DAP_Data.swd_conf.data_phase && ((request & DAP_TRANSFER_RnW) == 0U)) {
      PORTC->clr = GPIO_PINS_0;
      for (n = 32U+1U; n; n--) { SW_CLOCK_CYCLE_FAST(); }
    }
    PORTC->scr = GPIO_PINS_0;
    return ((uint8_t)ack);
  }

  /* Protocol error */
  for (n = DAP_Data.swd_conf.turnaround + 32U + 1U; n; n--) {
    SW_CLOCK_CYCLE_FAST();
  }
  PIN_SWDIO_OUT_ENABLE();
  PORTC->scr = GPIO_PINS_0;
  return ((uint8_t)ack);
}

// ====== SWD_TransferSlow (保留，使用慢速宏) ======
// 逻辑同原版，但改用 SW_WRITE_BIT_SLOW / SW_READ_BIT_SLOW / SW_CLOCK_CYCLE_SLOW
// 32位写也展开为 SEND_32BIT_ONCE_SLOW() × 32
// 奇偶校验用 GetParity()

// ... (同原逻辑，略)

#undef  PIN_DELAY
#define PIN_DELAY() PIN_DELAY_FAST()
// SWD_TransferFast 在上面已手动实现，这里不再用宏生成

#undef  PIN_DELAY
#define PIN_DELAY() PIN_DELAY_SLOW(DAP_Data.clock_delay)
// SWD_TransferSlow 在上面已手动实现

uint8_t SWD_Transfer(uint32_t request, uint32_t *data)
{
    if (DAP_Data.fast_clock)
        return SWD_TransferFast(request, data);
    else
        return SWD_TransferSlow(request, data);
}

#endif  /* (DAP_SWD != 0) */
```

---

### ⚡ 修改5：DAP.c — 启用快速模式

**文件**: `DAP/DAP.c`，函数 `DAP_Setup()` (约第 1128 行)

```c
// 修改前:
void DAP_Setup(void)
{
    DAP_Data.debug_port = 0U;
    DAP_Data.fast_clock = 0U;                          // ← 永远是慢速
    DAP_Data.clock_delay = CLOCK_DELAY(DAP_DEFAULT_SWJ_CLOCK);
    ...
}

// 修改后:
void DAP_Setup(void)
{
    DAP_Data.debug_port = 0U;
    DAP_Data.fast_clock = 1U;                          // ← 启用快速模式！
    DAP_Data.clock_delay = 1U;                         // ← 快速模式下最少延迟
    ...
}
```

---

### ⚡ 修改6：DAP_config.h — 提高默认时钟

**文件**: `DAP_Config/DAP_config.h`，约第 16 行

```c
// 修改前:
#define DAP_DEFAULT_SWJ_CLOCK 1000000  // 1 MHz

// 修改后:
#define DAP_DEFAULT_SWJ_CLOCK 10000000 // 10 MHz（快速模式下实际由 fast_clock 控制）
```

---

### ⚡ 修改7：DAP_config.h — 删除低效的方向切换函数

**文件**: `DAP_Config/DAP_config.h`

**删除** 当前第 145-170 行的 `PIN_SWDIO_OUT_ENABLE()` 和 `PIN_SWDIO_OUT_DISABLE()` 函数（用了 `gpio_init()`），替换为修改 3 中描述的 `cfglr` 直接操作版本。

同时**删除**对应的 `PIN_SWDIO_TMS_OUT_DISABLE()` / `PIN_SWDIO_TMS_OUT_ENABLE()` 宏（第 110-143 行），因为新的快速切换函数不再需要这些宏。

---

### ⚡ 修改8：DAP_config.h — 简化 GPIO 初始化宏

**文件**: `DAP_Config/DAP_config.h`

当前 `PIN_SWDIO_TMS_SET/CLR` 已经在用 `scr/clr`，无需修改。

删除不再需要的辅助宏：
```c
// 这些可以删除（新代码不再使用）
#define GPIO_INIT(port, data) ...
#define PIN_MODE_MASK(pin) ...
#define PIN_MODE(mode, pin) ...
```

---

## 四、预期效果

### 4.1 SWD 线速改善

| 指标 | 修改前（慢速） | 修改后（快速） |
|------|--------------|--------------|
| SWCLK 频率 | ~339 kHz | **~8-10 MHz** |
| SWD 写吞吐量 | ~7.4 KB/s | **~180-220 KB/s** |
| SWD 读吞吐量 | ~5 KB/s | **~100-130 KB/s** |
| 提升倍数 | — | **~25-30x** |

### 4.2 对实际烧录时间的影响

> ⚠️ Flash 擦除时间是硬瓶颈，与 SWD 速度无关。

以 92KB bin 文件为例：

| 阶段 | 优化前 | 优化后 | 节省 | 瓶颈归属 |
|------|--------|--------|------|---------|
| Flash 全片擦除 | ~16 秒 | ~16 秒 | **0** | 目标芯片硬件 |
| SWD 数据传输 | ~8 秒 | ~1 秒 | **~7 秒** | SWD 速度 ✅ |
| Flash 物理写入 | ~3 秒 | ~3 秒 | **0** | 目标芯片硬件 |
| 其他开销 | ~3 秒 | ~2 秒 | ~1 秒 | 混合 |
| **合计** | **~30 秒** | **~22 秒** | **~8 秒 (27%)** |

### 4.3 如果擦除不是瓶颈

如果你的目标芯片 Flash 擦除很快（比如只擦除用到的扇区而非全片），或者烧录前不擦除：

| 场景 | 优化前 | 优化后 | 节省 |
|------|--------|--------|------|
| 不擦除，只烧录 92KB | ~14 秒 | ~6 秒 | **~8 秒 (57%)** |
| 烧录 512KB 大文件 + 全片擦除 | ~60+ 秒 | ~45+ 秒 | ~15 秒 (25%) |
| 烧录 512KB 大文件 + 不擦除 | ~40 秒 | ~12 秒 | **~28 秒 (70%)** |

**结论**：SWD 速度优化的收益取决于 Flash 擦除时间占比。擦除占比越高，优化收益越低。但无论如何，SWD 传输本身能提速 25-30 倍。

---

## 五、注意事项

1. **信号完整性**：10 MHz SWCLK 对 PCB 走线有要求，SWCLK/SWDIO 走线应尽量短，必要时串 22-100Ω 电阻

2. **目标芯片兼容性**：某些低速目标芯片（如 STM32F030）可能无法在 10 MHz 下稳定工作，需保留慢速模式作为降级选项

3. **编译优化**：确保 Keil 编译器优化等级为 `-O2` 或更高，`__attribute__((always_inline))` 需要编译器支持

4. **验证方法**：修改后用逻辑分析仪测量 SWCLK 频率，对比修改前后的烧录时间

5. **慢速模式保留**：修改 4 中保留了 `SWD_TransferSlow`，通过在 Lua 脚本中设置 `SWD_CLOCK_DELAY > 0` 可降级到慢速模式

---

## 六、文件修改汇总

| 文件 | 修改内容 | 优先级 |
|------|---------|--------|
| `DAP/DAP.h` | 添加 `ParityTable256` + `GetParity()` | ⭐⭐⭐ |
| `DAP_Config/DAP_config.h` | 重写位操作宏 + 快速方向切换 | ⭐⭐⭐ |
| `DAP/SW_DP.c` | 重写 `SWD_TransferFast` + `SWD_TransferSlow` | ⭐⭐⭐ |
| `DAP/DAP.c` | `DAP_Setup()` 中启用 `fast_clock = 1` | ⭐⭐⭐ |
| `DAP_Config/DAP_config.h` | 提高 `DAP_DEFAULT_SWJ_CLOCK` | ⭐⭐ |
| `DAP_Config/DAP_config.h` | 清理废弃宏 | ⭐ |
