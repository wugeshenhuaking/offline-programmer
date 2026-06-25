# SWD 协议详解

> 基于 ARM Debug Interface v5.2 (ADIv5.2)，适用于 Cortex-M3/M4/M7 等 MCU

---

## 一、SWD vs JTAG

| 特性 | JTAG | SWD |
|------|------|-----|
| 引脚数 | 4 (TCK/TMS/TDI/TDO) | **2 (SWCLK/SWDIO)** |
| 最高速度 | 较低 | 较高 |
| 双向 | 半双工（TDI/TDO 分开） | 半双工（SWDIO 分时复用） |
| 调试功能 | 完整 | 完整（与 JTAG 等效） |

SWD 用 2 根线实现了 JTAG 4 根线的全部调试功能，是 Cortex-M 的主流调试接口。

---

## 二、硬件连接

```
调试器(主机)              目标MCU(从机)
┌──────────┐              ┌──────────┐
│  SWCLK   │──────────────│ SWCLK    │  时钟（主机→从机，单向）
│  SWDIO   │──────┬───────│ SWDIO    │  数据（双向，分时复用）
│  GND     │──────┘───────│ GND      │  共地
│  VCC     │──────────────│ VCC(可选)│  供电参考
└──────────┘              └──────────┘

  SWDIO 需要上拉电阻（通常 4.7K~10K）
  SWCLK 可以不上拉
```

### 关键点
- **SWCLK**：主机驱动，单向时钟
- **SWDIO**：双向数据线，**方向随协议阶段切换**
- **SWDIO 上拉**：线与逻辑，空闲时为高电平

---

## 三、SWD 协议层次结构

```
┌─────────────────────────────────────────────┐
│  应用层   DP/AP 寄存器读写                    │
├─────────────────────────────────────────────┤
│  传输层   SWD 传输（Request + ACK + Data）    │
├─────────────────────────────────────────────┤
│  物理层   位级传输（时钟 + 数据位）            │
└─────────────────────────────────────────────┘
```

---

## 四、物理层：位传输

### 4.1 写 1 个 bit（主机→从机）

```
SWCLK ──┐     ┌──────────┐     ┌──
        │     │          │     │
        └─────┘          └─────┘
        
SWDIO ──X(bit)──────────────────
        ↑ 写数据在 SWCLK 下降沿之前

时序：
1. 主机在 SWDIO 上输出 bit 值
2. SWCLK 拉低（下降沿）
3. 延迟（半个周期）
4. SWCLK 拉高（上升沿）→ 从机在此时采样 SWDIO
5. 延迟（半个周期）
```

### 4.2 读 1 个 bit（从机→主机）

```
SWCLK ──┐     ┌──────────┐     ┌──
        │     │          │     │
        └─────┘          └─────┘
        
SWDIO ──────────X(bit)──────────
                ↑ 从机在 SWCLK 低电平期间输出数据
                ↑ 主机在 SWCLK 上升沿采样

时序：
1. SWCLK 拉低（下降沿）
2. 延迟（半个周期）→ 从机在此期间驱动 SWDIO
3. 主机采样 SWDIO
4. SWCLK 拉高（上升沿）
5. 延迟（半个周期）
```

### 4.3 时钟周期（空闲时钟）

```
SWCLK ──┐     ┌──
        │     │
        └─────┘
SWDIO ──────────（不关心）

用于等待、turnaround 等场景
```

---

## 五、传输层：一次完整的 SWD 传输

一次 SWD 传输 = **8bit Request + 1bit Turnaround + 3bit ACK + 数据(33bit) + Turnaround**

### 5.1 总体结构

```
┌─────────┬──────┬───────┬────────────────────┬──────┐
│ Request │ Turn │  ACK  │ Data (可选)        │ Turn │
│  8 bit  │ 1bit │ 3 bit │ 32+1(parity)=33bit │ 1bit │
└─────────┴──────┴───────┴────────────────────┴──────┘
 主机发送   空闲  从机发送  读:从机发/写:主机发   空闲
```

### 5.2 Request 包格式（8 bit，主机发送，LSB first）

```
 Bit:  7   6   5   4   3   2   1   0
       ┌───┬───┬───┬───┬───┬───┬───┬───┐
       │ P │ 1 │ 0 │ A3│ A2│RnW│APn│ 1 │
       └───┴───┴───┴───┴───┴───┴───┴───┘
        奇   停  停  地   地 读  DP/ 面
        偶   止  止  址   址 写  AP  起
        校           bit  bit
        验

发送顺序: bit0 先发 → bit7 最后发 (LSB first)
```

| 字段 | 位 | 说明 |
|------|-----|------|
| Start | bit0 | 固定 `1` |
| APnDP | bit1 | `0`=访问 DP 寄存器，`1`=访问 AP 寄存器 |
| RnW | bit2 | `0`=写，`1`=读 |
| A2:A3 | bit3:bit4 | 寄存器地址（0~3） |
| Parity | bit5 | bit1~bit4 的偶校验 |
| Stop | bit6 | 固定 `0` |
| Park | bit7 | 固定 `1` |

**Request 计算示例**：
```
读 DP IDCODE (地址0, 读, DP):
  Start=1, APnDP=0, RnW=1, A2=0, A3=0, Parity=1, Stop=0, Park=1
  = 0b10100001 = 0xA5

写 AP CSW (地址0, 写, AP):
  Start=1, APnDP=1, RnW=0, A2=0, A3=0, Parity=1, Stop=0, Park=1
  = 0b10000011 = 0x83
```

### 5.3 Turnaround（方向切换）

```
Request 发完后，主机释放 SWDIO（切换为输入）
1 个空闲时钟周期用于 turnaround
然后从机开始驱动 SWDIO 发送 ACK
```

### 5.4 ACK 响应（3 bit，从机发送，LSB first）

```
 ACK[2:0]   含义              处理
 ─────────  ────────────────  ──────────────────
  001       OK                正常，继续数据传输
  010       WAIT              从机忙，主机应重试
  100       FAULT             错误，需读 DP CTRL/STAT 清除
  其他      协议错误          重新初始化
```

### 5.5 Data 传输（33 bit = 32 数据 + 1 奇偶校验）

#### 读操作（从机→主机）

```
ACK=OK 后：
┌──────────────────────────────────┬─────┐
│  Data[31:0]  (LSB first)         │ Par │
│  32 bit                          │ 1bit│
└──────────────────────────────────┴─────┘
 从机发送

  Parity = Data[31:0] 的偶校验
  主机校验错误则置 ACK=ERROR
```

#### 写操作（主机→从机）

```
ACK=OK 后，先 1 个 turnaround（从机→主机 切换 主机→从机）：
┌──────┬──────────────────────────────────┬─────┐
│ Turn │  Data[31:0]  (LSB first)         │ Par │
│ 1bit │  32 bit                          │ 1bit│
└──────┴──────────────────────────────────┴─────┘
       主机发送
```

### 5.6 传输结束

```
数据传输完后：
- 读操作：1 个 turnaround（从机→主机 切换 主机→从机）
- 写操作：主机驱动 SWDIO=1（空闲态）
```

---

## 六、一次完整 SWD 读传输的波形

以读 DP IDCODE 为例：

```
SWCLK  ──┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌─
        └─┘ └─┘ └─┘ └─┘ └─┘ └─┘ └─┘ └─┘ └─┘ └─┘ └─┘ └─┘ └─┘ └─┘ └─┘ └─┘ └─┘ └─

SWDIO   1 0 1 0 0 1 0 1  X  0 1 0  D0 D1 D2 ... D31 P  1
        │ Request(0xA5)  │T │ACK=OK│  Data(32bit)+Par  │T│
        │ 读 DP IDCODE   │u │      │  IDCODE=0x2BA01477│ │
        │                │r │      │                    │ │
        主机发送 ─────────┘n └─从机发送──────────────────┘ └主机

T = Turnaround (1 个空闲时钟，方向切换)
```

---

## 七、SWD 初始化序列

### 7.1 从 JTAG 切换到 SWD

上电后，目标 MCU 默认可能在 JTAG 模式，需要发送切换序列：

```
步骤1: 发送 >50 个 SWCLK 时钟（SWDIO=1，复位调试口）
步骤2: 发送 0xE79E（16 bit，JTAG-to-SWD 切换码）
       bit 顺序: 1110 0111 1001 1110 (LSB first)
步骤3: 发送 >50 个 SWCLK 时钟（SWDIO=1，再次复位）
步骤4: 发送空操作，读 IDCODE 确认切换成功
```

### 7.2 切换码

| 切换方向 | 16 bit 码 | 说明 |
|---------|-----------|------|
| JTAG → SWD | `0xE79E` | 切换到 SWD 模式 |
| SWD → JTAG | `0x873A` | 切换回 JTAG 模式 |

### 7.3 初始化流程

```
1. JTAG2SWD() 切换序列
2. 读 DP IDCODE → 确认连接
3. 写 DP ABORT (0x00) → 清除错误标志
4. 写 DP SELECT (0x08) = 0 → 选择 DP BANK 0
5. 写 DP CTRL/STAT (0x04) → 上电 (CSYSPWRUPREQ | CDBGPWRUPREQ)
6. 轮询读 DP CTRL/STAT → 确认上电完成
7. 写 DP CTRL/STAT → 设置传输模式
8. 写 DP SELECT = 0 → 完成
```

---

## 八、DP 和 AP 寄存器

### 8.1 DP（Debug Port）寄存器

DP 寄存器通过 `APnDP=0` 访问，地址 A[3:2]：

| 地址 | 寄存器 | 读/写 | 说明 |
|------|--------|-------|------|
| 0x0 | IDCODE | R | 调试口 ID |
| 0x0 | ABORT | W | 中断/清错误 |
| 0x4 | CTRL/STAT | R/W | 控制/状态（上电等） |
| 0x8 | SELECT | R/W | AP 选择 + DP BANK |
| 0x8 | RESEND | R | 重发上次数据 |
| 0xC | RDBUFF | R | 读缓冲（AP 读的缓冲值） |

### 8.2 AP（Access Port）寄存器

AP 寄存器通过 `APnDP=1` 访问，先写 DP SELECT 选择 AP 编号：

| 地址 | 寄存器 | 说明 |
|------|--------|------|
| 0x0 | CSW | 控制和状态（访问宽度、自增等） |
| 0x4 | TAR | 传输地址寄存器 |
| 0x8 | DRW | 数据读写（读/写目标内存） |
| 0xC | IDR | AP ID |

### 8.3 读 AP 寄存器的两步过程

```
AP 寄存器读需要两次 SWD 传输：

第1次: 写 DP SELECT = (AP_num << 24) | (AP_reg_addr >> 2)
        → 选择目标 AP 和寄存器

第2次: 读 AP (APnDP=1, RnW=1, A=0)
        → 读到的数据在 RDBUFF 里

第3次: 读 DP RDBUFF
        → 取出真正的数据（前一次读的结果）
```

### 8.4 写 AP 寄存器

```
第1次: 写 DP SELECT = (AP_num << 24) | (AP_reg_addr >> 2)

第2次: 写 AP (APnDP=1, RnW=0, A=0) = data
        → 直接写入，不需要 RDBUFF
```

---

## 九、访问目标内存

通过 MEM-AP 访问目标 MCU 的内存/寄存器。

### 9.1 写内存（4 字节）

```
1. 写 AP CSW  = 0x23000050  (32bit访问, 自增)
2. 写 AP TAR  = 目标地址
3. 写 AP DRW  = 要写入的数据
4. 读 DP RDBUFF (dummy read, 等待写入完成)
```

### 9.2 读内存（4 字节）

```
1. 写 AP CSW  = 0x23000050  (32bit访问, 自增)
2. 写 AP TAR  = 目标地址
3. 读 AP DRW  (发起读，数据在下一次)
4. 读 DP RDBUFF = 真正的数据
```

### 9.3 批量读写（利用 TAR 自增）

```
CSW 配置为地址自增后，连续读写 DRW 即可访问连续地址：

写 1KB:
1. 写 CSW = 0x23000050  (32bit, auto-increment)
2. 写 TAR = 起始地址
3. 循环 256 次: 写 DRW = data[i]   (TAR 自动 +4)
4. 读 RDBUFF (等待完成)
```

---

## 十、Flash 烧录原理

### 10.1 Flash 算法（Flash Algorithm）

目标 MCU 的 flash 不能直接写，需要运行一段代码来执行擦写。这段代码叫 **Flash 算法**，特点：

```
1. Flash 算法是预编译的二进制代码（flash_blob）
2. 通过 SWD 下载到目标 MCU 的 SRAM
3. 通过 SWD 设置目标 MCU 的寄存器（R0-R15, PC, LR, SP）
4. 让目标 MCU 从算法入口地址开始执行
5. 轮询等待目标 MCU halt（算法执行完毕会 BKPT）
6. 读取 R0 获取返回值（0=成功）
```

### 10.2 Flash 算法结构

```c
const program_target_t flash_algo = {
    init,           // 初始化函数入口地址
    uninit,         // 清理函数入口地址
    erase_chip,     // 全片擦除入口地址
    erase_sector,   // 扇区擦除入口地址
    program_page,   // 编程入口地址
    {breakpoint, static_base, stack_pointer},  // 系统调用参数
    program_buffer, // 目标 RAM 缓冲地址
    algo_start,     // 算法在目标 RAM 的起始地址
    algo_size,      // 算法大小
    algo_blob,      // 算法二进制数据
    program_buffer_size,  // 编程缓冲大小
};
```

### 10.3 烧录流程

```
1. swd_set_target_state_hw(RESET_PROGRAM)  → 复位并 halt 目标
2. swd_write_memory(algo_start, algo_blob, algo_size)  → 下载算法到 SRAM
3. swd_flash_syscall_exec(init, flash_start, ...)  → 执行 init
4. swd_flash_syscall_exec(erase_chip/erase_sector, ...)  → 擦除
5. 循环每页:
   a. swd_write_memory(program_buffer, page_data, page_size)  → 写数据到目标 RAM
   b. swd_flash_syscall_exec(program_page, addr, ...)  → 执行编程
6. swd_read_memory(addr, verify_buf, size)  → 回读校验
7. swd_set_target_reset(0)  → 复位运行
```

---

## 十一、SWD 协议状态机

```
                    ┌──────────┐
         ──────────→│   IDLE   │
         │          └──────────┘
         │               │
         │          发送 Request
         │               │
         │          ┌────▼─────┐
         │          │ REQUEST  │
         │          └────┬─────┘
         │               │
         │         Turnaround
         │               │
         │          ┌────▼─────┐
         │     OK   │   ACK    │──┐ WAIT/FAULT
         │   ┌──────│          │  │
         │   │      └──────────┘  │ 重试
         │   │                    │
         │   ▼                    ▼
         │ ┌──────┐          ┌────────┐
         │ │ DATA │          │ RETRY  │
         │ └──┬───┘          └────────┘
         │    │
         └────┘
         传输完成，回到 IDLE
```

---

## 十二、常见问题

### 12.1 为什么 SWDIO 需要上拉？

SWDIO 是双向线，使用线与逻辑：
- 空闲时上拉为高电平
- 任一方可以拉低
- 方向切换时的 turnaround 周期，双方都释放，靠上拉维持高电平

### 12.2 为什么读 AP 需要 RDBUFF？

AP 是异步的，读 AP DRW 时数据还没准备好。数据在下一个 DP RDBUFF 读时才可用。这是 ADIv5 的流水线设计。

### 12.3 SWCLK 最高频率

| 因素 | 限制 |
|------|------|
| 目标 MCU DAP 同步器 | 通常 < 10 MHz |
| PCB 走线长度 | 长→低 |
| SWDIO 上拉 RC | 大→低 |
| 软件位翻转 | 循环精度限制 |

### 12.4 如何降速

```
通过增加 clock_delay 循环次数来降速：

clock_delay = (CPU_CLOCK / 2 / 目标频率) - IO_PORT_WRITE_CYCLES

示例 (240MHz CPU, 想要 1MHz):
  clock_delay = (240M / 2 / 1M) - 2 = 118
```

---

## 十三、协议速查表

### Request 字段速查

| 操作 | APnDP | RnW | A[3:2] | Parity |
|------|-------|-----|--------|--------|
| 读 DP IDCODE | 0 | 1 | 00 | 1 |
| 写 DP ABORT | 0 | 0 | 00 | 0 |
| 读 DP CTRL/STAT | 0 | 1 | 01 | 0 |
| 写 DP CTRL/STAT | 0 | 0 | 01 | 1 |
| 写 DP SELECT | 0 | 0 | 10 | 1 |
| 读 DP RDBUFF | 0 | 1 | 11 | 1 |
| 读 AP CSW/TAR/DRW/IDR | 1 | 1 | 0~3 | 计算得到 |
| 写 AP CSW/TAR/DRW | 1 | 0 | 0~3 | 计算得到 |

### ACK 速查

| ACK[2:0] | 含义 |
|-----------|------|
| 001 | OK |
| 010 | WAIT |
| 100 | FAULT |
| 其他 | 协议错误 |

---

## 参考文档

- ARM Debug Interface v5.2 Architecture Specification (ARM IHI 0031)
- ARM CoreSight Components Technical Reference Manual
- CMSIS-DAP Interface Firmware (DAPLink)
