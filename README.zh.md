[EN](./readme.md)

# MicroIsoTp

一个面向经典CAN（8字节数据帧）的轻量级 ISO 15765-2（ISO-TP）传输层实现，用于裸机MCU开发。本库**只实现ISO-TP传输层**——单帧/首帧/连续帧/流控帧的组帧、拆包、组包、时序控制、物理/功能寻址，不包含任何应用层（如UDS）的内容，设计上就是供应用层挂在它上面使用的。

## 特性

- 仅支持经典CAN（8字节数据帧），不支持CAN FD
- 全双工：接收（Rx）与发送（Tx）两套独立状态机并行运行
- 支持单帧收发，也支持多帧（首帧+连续帧）收发
- 双方向的流控处理（作为接收方：发出流控帧；作为发送方：遵守对方的流控帧）
- 支持Block Size（BS）与STmin节奏控制，包含对STmin保留值的兜底处理（见下文）
- 支持物理寻址与功能寻址，并强制执行功能寻址的限制（广播报文只能是单帧，收到的首帧/连续帧一律丢弃）
- 实现了ISO-TP规定的全部定时器：N_As、N_Bs、N_Cs（发送侧），N_Ar、N_Br、N_Cr（接收侧）
- 无动态内存分配，Rx/Tx共用一块由 `MICROISOTP_BUFFER_SIZE` 决定大小的静态缓冲区
- 硬件发送接口与上层通知接口均为弱函数（weak function），分别由CAN驱动层和应用层重写

## 暂未实现的内容（当前阶段的设计取舍）

- 超过4095字节时的32位"转义"FF_DL格式（`MicroIsoTp_TxStart` 会直接拒绝超过此长度的请求）
- 扩展寻址（payload中额外携带地址扩展字节）
- CAN FD

## 架构

```
上层（例如 UDS）
        |
        |  MicroIsoTp_TxStart() / MicroIsoTp_Rx_Indication() / MicroIsoTp_Tx_Confirmation()
        v
+-----------------------------------------------------+
|                     MicroIsoTp                       |
|                                                       |
|   Rx 状态机                Tx 状态机                    |
|   (IDLE/SF/FF/FC/...)     (IDLE/SF/FF/FC/CF/...)       |
|                                                       |
|   共享数据缓冲区 (MicroIsoTp_Obj.buf)                   |
+-----------------------------------------------------+
        ^
        |  MicroIsoTp_Physical_RxIndication() / MicroIsoTp_Functional_RxIndication()
        |  MicroIsoTp_HwTransmitDone()
        v
CAN驱动  <---  MicroIsoTp_HwTransmit()（弱函数，由驱动层重写）
```

Rx与Tx共用同一块静态缓冲区，因此同一时刻只能有一个方向在工作：当一次发送（Tx）正在进行时，任何新收到的CAN帧都会被路由给Tx状态机（当作流控帧处理），而不是Rx状态机；`MicroIsoTp_TxStart()` 也会在有接收正在进行时直接拒绝启动新的发送。这一层路由判断是共享缓冲区安全性的唯一保障——如果要改动这部分逻辑，必须重新确认这个保障依然成立。

## 定时器

| 定时器 | 归属方 | 含义 | 默认值 |
|---|---|---|---|
| N_As | 发送方 | 把自己的一帧（SF/FF/CF）实际发到总线上所需的最长时间 | 700 ms |
| N_Bs | 发送方 | 等待对方流控帧的最长时间 | 1000 ms |
| N_Cs | 发送方 | 发送两个连续帧之间的间隔（由对方要求的STmin换算而来） | 由STmin换算 |
| N_Ar | 接收方 | 把自己的流控帧实际发到总线上所需的最长时间 | 500 ms |
| N_Br | 接收方 | 收完一个Block后，重新发流控前的内部延时 | 250 ms |
| N_Cr | 接收方 | 等待下一个连续帧的最长时间 | 500 ms |

默认值定义在 `MicroIsoTp_conf.h` 中，也可以通过 `MicroIsoTp_Tx_SetNAsTimeout`、`MicroIsoTp_Tx_SetNBsTimeout`、`MicroIsoTp_Rx_SetNArTimeout`、`MicroIsoTp_Rx_SetNBrTimeout`、`MicroIsoTp_Rx_SetNCrTimeout` 在运行时覆盖。

## 寻址

本库不负责判断某个CAN ID是物理地址还是功能地址——这个映射关系属于CAN驱动层/过滤器配置的职责。驱动层只需要根据收到的CAN ID，调用对应的入口函数：

- 收到物理（点对点）CAN ID上的帧，调用 `MicroIsoTp_Physical_RxIndication()`
- 收到功能（广播）CAN ID上的帧，调用 `MicroIsoTp_Functional_RxIndication()`

不管请求是从物理地址还是功能地址来的，回复永远应该使用物理响应CAN ID发出——本库内部不存在"功能寻址的响应"这个概念。

## 集成步骤

1. 提供你自己的 `MicroIsoTp_conf.h`，配置好缓冲区大小、Tick频率和各定时器默认值。
2. 启动时调用一次 `MicroIsoTp_Init()`。
3. 以固定频率 `MICROISOTP_FREQ_HZ` 调用 `MicroIsoTp_TickHandler()`（比如放在1ms硬件定时器中断里）。这个函数只做计数递增，务必保持轻量。
4. 在主循环或一个低优先级任务里周期性调用 `MicroIsoTp_TimerHandler()`，它负责判断各定时器是否超时、驱动Rx/Tx两套状态机往前走。
5. 在CAN驱动的接收回调里，判断收到的CAN ID是物理地址还是功能地址，分别调用 `MicroIsoTp_Physical_RxIndication()` / `MicroIsoTp_Functional_RxIndication()`。
6. 在CAN驱动层重写弱函数 `MicroIsoTp_HwTransmit()`，真正把一帧数据发送到总线上；并在该帧确认发送完成后（比如CAN发送完成中断里）调用 `MicroIsoTp_HwTransmitDone()`。
7. 在上层（比如UDS）重写弱函数 `MicroIsoTp_Rx_Indication()` 以接收组包完成的完整数据；如果需要知道 `MicroIsoTp_TxStart()` 发起的发送何时完成，可以选择性重写 `MicroIsoTp_Tx_Confirmation()`。

## API 一览

### 生命周期

| 函数 | 说明 |
|---|---|
| `MicroIsoTp_Init(void)` | 初始化Rx/Tx状态机与共享缓冲区，启动时调用一次。 |
| `MicroIsoTp_TickHandler(void)` | 周期性调用（频率为 `MICROISOTP_FREQ_HZ`），只做计数递增。 |
| `MicroIsoTp_TimerHandler(void)` | 周期性调用（放在主循环里），判断超时并驱动状态机。 |

### CAN驱动 → 本库（接收方向）

| 函数 | 说明 |
|---|---|
| `MicroIsoTp_Physical_RxIndication(data, dlc)` | 喂给一帧从物理CAN ID收到的数据。 |
| `MicroIsoTp_Functional_RxIndication(data, dlc)` | 喂给一帧从功能CAN ID收到的数据，非单帧会被丢弃。 |
| `MicroIsoTp_HwTransmitDone(void)` | 通知本库：上一次交给 `MicroIsoTp_HwTransmit()` 的那一帧已经真正发到总线上了。 |

### 本库 → CAN驱动（发送方向）

| 函数 | 说明 |
|---|---|
| `MicroIsoTp_HwTransmit(data, dlc)` | 弱函数，由CAN驱动层重写，真正发送一帧CAN数据，成功返回0。 |

### 上层 → 本库（发起一次发送）

| 函数 | 说明 |
|---|---|
| `MicroIsoTp_TxStart(data, len)` | 发起一次发送（自动选择单帧还是首帧+连续帧），`len` 取值范围1~4095字节。如果当前已有发送或接收正在进行，返回 `MICROISOTP_BUSY`。 |
| `MicroIsoTp_TxStop(void)` | 中止当前发送，把Tx状态机复位到空闲。 |
| `MicroIsoTp_Tx_Confirmation(void)` | 弱函数，重写它以在 `MicroIsoTp_TxStart()` 发起的这次发送完成时收到通知。 |
| `MicroIsoTp_Tx_SetNAsTimeout(tick)` | 覆盖N_As超时时间。 |
| `MicroIsoTp_Tx_SetNBsTimeout(tick)` | 覆盖N_Bs超时时间。 |

### 本库 → 上层（接收到一整包数据）

| 函数 | 说明 |
|---|---|
| `MicroIsoTp_Rx_Indication(type, data, len)` | 弱函数，重写它以接收组包完成的完整数据，`type` 告知这包数据是从物理还是功能寻址收到的。 |
| `MicroIsoTp_Rx_SetFlowControl(Fc, Bs, Stmin)` | 预先配置下一次接收要使用的流控参数（Block Size / STmin）。如果不调用，则使用 `MICROISOTP_RX_DEFAULT_BS` / `MICROISOTP_RX_DEFAULT_STMIN`。 |
| `MicroIsoTp_Rx_SetNArTimeout(tick)` | 覆盖N_Ar超时时间。 |
| `MicroIsoTp_Rx_SetNBrTimeout(tick)` | 覆盖N_Br超时时间。 |
| `MicroIsoTp_Rx_SetNCrTimeout(tick)` | 覆盖N_Cr超时时间。 |

## 配置项（`MicroIsoTp_conf.h`）

| 宏 | 含义 | 默认值 |
|---|---|---|
| `MICROISOTP_FREQ_HZ` | 驱动所有定时器的Tick频率 | 1000 |
| `MICROISOTP_BUFFER_SIZE` | Rx/Tx共享的负载缓冲区大小（字节） | 4096 |
| `MICROISOTP_RX_DEFAULT_BS` | 默认给发送方的Block Size | 0（不限制） |
| `MICROISOTP_RX_DEFAULT_STMIN` | 默认要求发送方遵守的STmin | 127 |
| `MICROISOTP_RX_DEFAULT_N_CR_TIMEOUT` | N_Cr默认值，单位ms | 500 |
| `MICROISOTP_RX_DEFAULT_N_AR_TIMEOUT` | N_Ar默认值，单位ms | 500 |
| `MICROISOTP_RX_DEFAULT_N_BR_TIMEOUT` | N_Br默认值，单位ms | 250 |
| `MICROISOTP_TX_DEFAULT_N_AS_TIMEOUT` | N_As默认值，单位ms | 700 |
| `MICROISOTP_TX_DEFAULT_N_BS_TIMEOUT` | N_Bs默认值，单位ms | 1000 |

## 状态码（`MicroIsoTp_Status_t`）

| 值 | 含义 |
|---|---|
| `MICROISOTP_OK` | 成功 |
| `MICROISOTP_ERR` | 通用错误（例如协议时序/序号错误） |
| `MICROISOTP_BUSY` | 当前已有发送或接收正在进行 |
| `MICROISOTP_OVERFLOW` | 请求或收到的长度超过了共享缓冲区大小，或超过了当前支持的FF_DL范围 |
| `MICROISOTP_PARAM_INVALID` | 传入的指针/参数非法 |

## 许可协议

[许可证](./LICENSE)