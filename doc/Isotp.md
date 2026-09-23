# ISO-TP（ISO 15765-2）传输层详解 —— 面向经典CAN 8字节、MCU端实现

先明确一个核心认知：**ISO-TP 本身是双向对称协议**。不管你现在的主要任务是"接收"（比如刷写时MCU接收上位机发来的大块数据），MCU端都必须同时实现**发送状态机（Tx SM）和接收状态机（Rx SM）**两套逻辑，因为：

- 接收多帧数据时，MCU要**发送**流控帧（FC）——这已经是"发送"逻辑了
- 未来响应上位机的请求（比如读大量Flash内容回传）时，MCU要**发送**多帧数据，同时要**接收**对方回的流控帧

---

## 一、四种帧类型（N_PCI）总览

CAN数据帧的第一个字节（有时+第二字节）是 **PCI（Protocol Control Information）**，最高4位（高nibble）决定帧类型：

| PCI高4位 | 帧类型 | 全称 | 作用 |
|---|---|---|---|
| 0x0 | **SF** | Single Frame 单帧 | 数据 ≤7字节，一帧搞定 |
| 0x1 | **FF** | First Frame 首帧 | 多帧传输的第一帧，携带总长度 |
| 0x2 | **CF** | Consecutive Frame 连续帧 | 后续数据帧 |
| 0x3 | **FC** | Flow Control 流控帧 | 由接收方发给发送方，控制节奏 |

四种帧在总线上的角色不对称：**SF/FF/CF 由数据发送方发出，FC 由数据接收方发出**。这就是为什么两端都得有完整的收发逻辑。

---

## 二、每种帧的字节格式（经典CAN 8字节，普通寻址）

> 这里默认"普通寻址"（Normal Addressing），即CAN ID本身区分了源/目的，8个数据字节全部用于ISO-TP。如果以后用"扩展寻址"（多一个地址扩展字节TA），可用数据要减1字节，逻辑不变，先不用管，专心做普通寻址。

### 1. 单帧 SF（Single Frame）

```
Byte0        Byte1  Byte2 ...  ByteN
[0x0 | SF_DL] [data] [data] ... 
```

- Byte0 高4位固定 `0x0`
- Byte0 低4位 = **SF_DL**（数据长度），取值 1~7（经典CAN下最大7，因为1字节被PCI占用）
- 数据紧跟在Byte1开始，长度SF_DL
- 剩余字节可用0x00或0xCC填充（是否强制填充取决于你的实现策略，但建议**固定发送8字节并填充**，方便DLC统一处理，也避免部分总线控制器对短帧的兼容性问题）

**示例**：发送3字节数据 `0x11 0x22 0x33`
```
03 11 22 33 CC CC CC CC
```

### 2. 首帧 FF（First Frame）

```
Byte0           Byte1        Byte2~Byte7
[0x1 | FF_DL高4位] [FF_DL低8位] [数据 最多6字节]
```

- Byte0 高4位固定 `0x1`
- **FF_DL**（总数据长度，12位）= `(Byte0 & 0x0F) << 8 | Byte1`，取值范围 8~4095
  - 注意：FF_DL必须 >7（否则应该用SF，不该用FF）
  - 如果总长度超过4095字节，有"转义格式"（Byte0低4位=0，Byte1=0，用Byte2~5的32位表示长度），刷写场景数据量一般不会超4095，可以先不实现这个转义分支，但**建议预留接口**，以后好扩展
- Byte2~Byte7：本帧携带的前**6字节**数据（FF只能带6字节，因为2字节被PCI占用）

**示例**：要发送总长度20字节的数据，前6字节是 `01 02 03 04 05 06`
```
10 14 01 02 03 04 05 06
```
（`0x1` `0x014` = 20）

### 3. 连续帧 CF（Consecutive Frame）

```
Byte0         Byte1~Byte7
[0x2 | SN]    [数据 最多7字节]
```

- Byte0 高4位固定 `0x2`
- **SN**（序列号，4位）：从 **1** 开始，每发一帧CF递增1，到15后回绕到0，再继续1、2、3...（即 1,2,...,15,0,1,2,...）
- Byte1~Byte7：最多7字节数据。最后一帧CF如果不满7字节，剩余字节填充（0xCC）

**示例**（接上面FF，剩余14字节需要2个CF）：
```
CF1: 21 07 08 09 0A 0B 0C 0D   (SN=1)
CF2: 22 0E CC CC CC CC CC CC  (SN=2, 只剩1字节有效数据)
```

### 4. 流控帧 FC（Flow Control）

```
Byte0        Byte1   Byte2   Byte3~7
[0x3 | FS]   [BS]    [STmin] [填充]
```

- Byte0 高4位固定 `0x3`
- **FS**（Flow Status，低4位）：
  - `0` = **CTS**（Clear To Send，可以继续发）
  - `1` = **WT**（Wait，先别发，稍后我再发FC）
  - `2` = **OVFLW**（Overflow，缓冲区溢出，发送方应终止传输，属于错误分支）
- **BS**（Block Size，Byte1）：接收方一次允许发送方连续发多少个CF而不用等新的FC
  - `0` = 不限制，发送方可以一口气把剩余CF全发完，不用再等FC
  - 非0 = 每发BS个CF，发送方必须停下来等待接收方再发一个FC
- **STmin**（Separation Time minimum，Byte2）：发送方发送连续两个CF之间**最小间隔时间**
  - `0x00~0x7F` = 0~127 ms
  - `0xF1~0xF9` = 100~900 μs（微秒级，经典CAN控制器和MCU处理能力一般达不到这么细，工程上常直接按0处理或按最小1ms处理）
  - `0x80~0xF0`、`0xFA~0xFF` = 保留值，不应使用

---

## 三、发送端逻辑（Tx State Machine）——MCU需要发多字节数据时

### 场景A：数据 ≤7字节
直接发一个SF，结束。无需FC参与。

### 场景B：数据 >7字节（需要拆包）

**状态机**：

```
IDLE 
  → 发送FF（前6字节），启动定时器N_Bs，进入 WAIT_FC
  
WAIT_FC（等待对方的流控帧）
  收到 FC(CTS):
    - 记录 BS、STmin
    - blockCounter = 0
    - 进入 SEND_CF
  收到 FC(WT):
    - 重启 N_Bs 定时器，继续等
  收到 FC(OVFLW):
    - 中止传输，上报错误，回 IDLE
  N_Bs 超时（默认参考1000ms，工程上通常配置更短）:
    - 中止传输，上报错误，回 IDLE

SEND_CF（按节奏发送连续帧）
  循环：
    - 等待 STmin 时间（发送间隔不能小于STmin）
    - 发送下一个CF（SN递增，取剩余数据中最多7字节）
    - blockCounter++
    - 剩余数据发完了？
        是 → 传输完成，回 IDLE，通知上层"发送成功"
        否 → 继续判断：
              如果 BS != 0 且 blockCounter == BS：
                  停止发CF，启动N_Bs定时器，回到 WAIT_FC（等下一个FC）
              否则：
                  继续发下一个CF（循环）
```

**关键点**：
- **拆包**动作发生在"生成FF、CF"这一步：把大数据buffer按6字节（FF）、7字节（CF）依次切片
- 发送节奏完全由**接收方的FC**（BS/STmin）遥控，发送方自己没有决定权，必须严格遵守
- 每发出一帧CAN帧本身也有一个"总线发送确认"超时 **N_As**（本节点把帧实际发到总线上所用时间，一般由CAN驱动层/邮箱状态保证，逻辑层超时阈值也参考1000ms）

---

## 四、接收端逻辑（Rx State Machine）——MCU接收多字节数据时（你现在的重点）

**状态机**：

```
IDLE
  收到一帧CAN，看PCI高4位：
  
  == 0x0 (SF) ==
    - 取SF_DL，取数据，直接组好一包完整数据，交给上层（比如刷写数据处理模块）
    - 保持 IDLE
    
  == 0x1 (FF) ==
    - 解析 FF_DL（总长度）
    - 把这一帧自带的前6字节数据存入接收缓冲区，已接收计数 = 6
    - 期望的下一个SN = 1
    - blockCounter = 0
    - 根据自己的能力选定 own_BS（比如0=不限制，或者一个具体值比如8）、own_STmin（比如0或者你MCU处理一帧CF需要的最短时间）
    - 立即发送 FC(CTS, own_BS, own_STmin)
    - 启动定时器 N_Cr（等待第一个CF）
    - 进入 RECEIVING

  == 0x2 (CF)（在IDLE状态下收到）==
    - 没有正在进行的会话，视为异常/孤立帧，丢弃（不处理）

  == 0x3 (FC)（在IDLE状态下收到）==
    - MCU此时是接收方角色，不该收到FC，丢弃（这是发送逻辑该处理的帧）

RECEIVING（正在组包）
  收到 CF：
    - 重启 N_Cr 定时器
    - 校验 SN 是否等于期望值：
        不等 → 序列错误，中止本次接收，上报错误，回 IDLE
        相等 → 继续
    - 把这帧的数据（最多7字节，注意最后一帧可能不满7字节，要按剩余需要的字节数截取）追加进缓冲区
    - 已接收计数 += 本帧有效字节数
    - 期望SN = (期望SN + 1) & 0x0F   // 15之后回绕到0
    - blockCounter++
    - 判断是否收满：
        已接收计数 >= FF_DL？
          是 → 组包完成！把完整缓冲区交给上层，回 IDLE
          否 → 判断是否需要再发一次FC：
                own_BS != 0 且 blockCounter == own_BS：
                    发送新的 FC(CTS, own_BS, own_STmin)
                    blockCounter = 0
                    重启 N_Cr 定时器
                否则：
                    继续等下一个CF（保持RECEIVING）
                    
  收到新的 FF（在RECEIVING状态下又来一个FF）：
    - 属于"新会话打断旧会话"，工程上常见做法：放弃当前未完成的接收缓冲区，按新FF重新开始（也可以选择直接拒绝，看你的策略，多数实现选择"新会话覆盖旧会话"）
    
  N_Cr 超时（迟迟没等到下一个CF，默认参考1000ms，工程上常配更短，比如几百ms）：
    - 中止接收，清空缓冲区，上报错误，回 IDLE
```

**关键点**：
- **组包**动作就是"把FF里的6字节 + 每个CF里的≤7字节，按SN顺序依次拼到一个大buffer里"，直到达到FF_DL
- **MCU作为接收方，是FC的制造者**，BS和STmin是MCU自己根据处理能力设定的，不是对方指定的。比如你的MCU如果Flash写入较慢、RAM缓冲区不大，可以设 `BS=8`（每收8帧就先歇一下、发个新FC控制对方节奏），或者干脆 `BS=0`（相信自己处理得过来，一口气接收所有CF）
- 判断SN是否连续是**必须**做的校验，防止总线上丢帧、乱序导致组包出错还不自知（刷写场景数据出错是很致命的，这一步不能省）

---

## 五、定时器汇总表

| 定时器 | 归属方 | 含义 | 典型阈值 |
|---|---|---|---|
| **N_As** | 发送方 | 发送方把一帧（SF/FF/CF）实际发送到总线上所需时间 | 规范上限1000ms，实际由CAN驱动层/发送成功中断保证，一般远小于此 |
| **N_Ar** | 接收方 | 接收方把FC帧发送到总线上所需时间 | 同上 |
| **N_Bs** | 发送方 | 发完FF（或一个Block的CF后）**等待对方FC**的最长时间 | 规范上限1000ms，工程上常配几百ms |
| **N_Br** | 接收方 | 接收方收完一个Block后，到发出下一个FC之间预留的时间（内部时间，非网络超时） | 一般不需要严格卡这个值，属实现细节 |
| **N_Cs** | 发送方 | 发送方两个CF之间的实际发送间隔（应 ≥ 对方要求的STmin） | 由STmin决定 |
| **N_Cr** | 接收方 | 接收完一个CF后，**等待下一个CF**的最长时间 | 规范上限1000ms，工程上常配几百ms |

> 说明：ISO 15765-2标准里这些超时的"规范默认最大值"都是1000ms，但那是"上限"，很多实际车厂/项目会配得比这个小很多（比如100ms~150ms级别），具体取决于你的总线负载和MCU响应能力，可以做成可配置参数，而不是写死。

---

## 六、异常处理要点（写代码时容易漏的坑）

1. **SN不连续**：立即中止当前会话，不要尝试"容错拼接"，宁可整包作废重传，也不要拼出一包错误数据（刷写场景尤其致命）
2. **FF_DL与实际收到字节数对不上**：以FF_DL为准做终止判断，最后一个CF里超出FF_DL的填充字节要丢弃，不能计入有效数据
3. **N_Cr / N_Bs 超时**：必须有独立定时器（软件定时器或硬件Tick计数），不能靠"CAN中断触发才检查"这种被动方式，否则永远等不到"没收到"这个事件
4. **接收方缓冲区不够大**：如果FF_DL超过你预分配的接收buffer，应立即回复 `FC(OVFLW)` 并中止，而不是缓冲区溢出写坏内存
5. **同一时刻只允许一个会话**：MCU作为接收方，同一个CAN ID/N_TA上同一时间只应有一个"正在进行的多帧接收"，新FF进来如何处理（覆盖/拒绝）要有明确策略，别悄悄地状态错乱
6. **发送方收到意外的FC**：比如在IDLE（没有正在发送任何东西）时收到FC，应直接丢弃，不能拿去当作某个"幽灵会话"处理
7. **CF中SN=0的第一帧**：注意SN从**1**开始，如果收到的第一个CF的SN是0，那是不对的（除非之前已经绕回过），要按上面"期望SN"比对逻辑，不要硬编码"第一个CF必须是1"（回绕情况下第一个CF也可能不是1，取决于你从哪接上的，但对于一次全新的FF会话，第一个CF期望值一定是1）

---

## 七、建议的代码结构（两个独立状态机 + 定时器Tick）

```c
// 两套独立的状态机，互不干扰，可以同时运行
// （比如MCU一边在接收上位机发来的Flash数据，一边在发送自己的响应，
//   虽然刷写场景通常是"一问一答"不会真并发，但状态机设计上应该解耦）

typedef enum {
    ISO_RX_IDLE,
    ISO_RX_RECEIVING,
} IsoRxState;

typedef enum {
    ISO_TX_IDLE,
    ISO_TX_WAIT_FC,
    ISO_TX_SEND_CF,
} IsoTxState;

typedef struct {
    IsoRxState state;
    uint8_t  buffer[MAX_BUF_LEN];
    uint16_t total_len;      // FF_DL
    uint16_t received_len;
    uint8_t  expected_sn;
    uint8_t  block_counter;
    uint8_t  own_bs;
    uint8_t  own_stmin;
    uint32_t timer_n_cr;     // 定时器计数
} IsoTpRxCtx;

typedef struct {
    IsoTxState state;
    const uint8_t *data;
    uint16_t total_len;
    uint16_t sent_len;
    uint8_t  sn;
    uint8_t  peer_bs;
    uint8_t  peer_stmin;
    uint8_t  block_counter;
    uint32_t timer_n_bs;
    uint32_t timer_stmin;
} IsoTpTxCtx;

// 主要接口：
void IsoTp_RxHandleCanFrame(const uint8_t *data, uint8_t dlc); // CAN中断/回调里喂帧
void IsoTp_TxStart(const uint8_t *data, uint16_t len);          // 上层调用发起发送
void IsoTp_Tick(uint32_t elapsed_ms);                             // 周期调用，驱动所有定时器和STmin节拍
```

- `IsoTp_RxHandleCanFrame`：CAN接收中断里调用，只做"分类到SF/FF/CF/FC分支+更新状态机"，不要在中断里做耗时操作
- `IsoTp_Tick`：放在一个固定周期（比如1ms）的定时任务里跑，负责检查N_Bs/N_Cr是否超时、以及SEND_CF状态下STmin节拍是否到了该发下一帧CF
- 组包完成后，把 `buffer` 和 `received_len` 交给上层（未来的UDS层）去处理，ISO-TP层到此为止，不掺和后面服务层怎么解析

---
