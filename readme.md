[中文](./README.zh.md)

# MicroIsoTp

A lightweight ISO 15765-2 (ISO-TP) transport layer implementation for classic CAN (8-byte frames), targeting bare-metal MCUs. This library implements **only the ISO-TP transport layer** — Single Frame / First Frame / Consecutive Frame / Flow Control framing, segmentation, reassembly, timing, and physical/functional addressing. It does not implement any application layer (e.g. UDS); it is designed to sit underneath one.

## Features

- Classic CAN only (8-byte data frames), no CAN FD
- Full duplex: independent Rx and Tx state machines running side by side
- Single Frame and multi-frame (First Frame + Consecutive Frame) send/receive
- Flow Control handling on both sides (as receiver: issuing FC; as sender: obeying FC)
- Block Size (BS) and STmin pacing, including the STmin reserved-value fallback (see below)
- Physical and functional addressing, with the functional-addressing restriction enforced (broadcast traffic may only be a Single Frame; any First/Consecutive Frame received on the functional address is discarded)
- All ISO-TP timers implemented: N_As, N_Bs, N_Cs (Tx side), N_Ar, N_Br, N_Cr (Rx side)
- No dynamic allocation; one shared static payload buffer sized by `MICROISOTP_BUFFER_SIZE`
- Hardware transmit and upper-layer notification hooks are weak functions, meant to be overridden by the CAN driver and the application layer respectively

## Not implemented (by design, for now)

- The 32-bit "escape" FF_DL format for payloads larger than 4095 bytes (`MicroIsoTp_TxStart` rejects lengths above this)
- Extended addressing (an extra address-extension byte in the payload)
- CAN FD

## Architecture

```
Upper layer (e.g. UDS)
        |
        |  MicroIsoTp_TxStart() / MicroIsoTp_Rx_Indication() / MicroIsoTp_Tx_Confirmation()
        v
+-----------------------------------------------------+
|                     MicroIsoTp                       |
|                                                       |
|   Rx state machine        Tx state machine            |
|   (IDLE/SF/FF/FC/...)     (IDLE/SF/FF/FC/CF/...)       |
|                                                       |
|   shared payload buffer (MicroIsoTp_Obj.buf)          |
+-----------------------------------------------------+
        ^
        |  MicroIsoTp_Physical_RxIndication() / MicroIsoTp_Functional_RxIndication()
        |  MicroIsoTp_HwTransmitDone()
        v
CAN driver  <---  MicroIsoTp_HwTransmit()  (weak, overridden by the driver)
```

Rx and Tx share one static payload buffer. Because of this, only one direction can be active at a time: while a Tx transmission is in progress, any incoming frame is routed to the Tx state machine (interpreted as a Flow Control frame) instead of the Rx state machine, and `MicroIsoTp_TxStart()` refuses to start a new transmission while a reception is in progress. This routing is load-bearing for buffer safety — do not change it without re-verifying that guarantee.

## Timers

| Timer  | Side | Meaning | Default |
|---|---|---|---|
| N_As | Tx | Max time to get one of our own frames (SF/FF/CF) onto the bus | 700 ms |
| N_Bs | Tx | Max time to wait for the peer's Flow Control frame | 1000 ms |
| N_Cs | Tx | Delay between two Consecutive Frames we send (derived from the peer's STmin) | derived from STmin |
| N_Ar | Rx | Max time to get our own Flow Control frame onto the bus | 500 ms |
| N_Br | Rx | Internal delay before re-issuing Flow Control after a Block completes | 250 ms |
| N_Cr | Rx | Max time to wait for the next Consecutive Frame | 500 ms |

Defaults live in `MicroIsoTp_conf.h` and can be overridden at runtime via `MicroIsoTp_Tx_SetNAsTimeout`, `MicroIsoTp_Tx_SetNBsTimeout`, `MicroIsoTp_Rx_SetNArTimeout`, `MicroIsoTp_Rx_SetNBrTimeout`, `MicroIsoTp_Rx_SetNCrTimeout`.

## Addressing

The stack does not decide which CAN ID is physical vs. functional — that mapping lives in the CAN driver / filter configuration. The driver simply calls the matching entry point:

- `MicroIsoTp_Physical_RxIndication()` for frames received on the physical (point-to-point) CAN ID
- `MicroIsoTp_Functional_RxIndication()` for frames received on the functional (broadcast) CAN ID

Regardless of which addressing a request arrived on, a response should always be sent using the physical response CAN ID — the stack itself has no concept of "functional response".

## Integration

1. Provide `MicroIsoTp_conf.h` with your buffer size, tick frequency, and default timeouts.
2. Call `MicroIsoTp_Init()` once at startup.
3. Call `MicroIsoTp_TickHandler()` at a fixed rate of `MICROISOTP_FREQ_HZ` (e.g. from a 1 ms hardware timer ISR). Keep it cheap — it only increments tick counters.
4. Call `MicroIsoTp_TimerHandler()` periodically from your main loop or a lower-priority task. This evaluates timeouts and drives both state machines.
5. In your CAN driver's receive callback, look up whether the incoming CAN ID is physical or functional and call `MicroIsoTp_Physical_RxIndication()` / `MicroIsoTp_Functional_RxIndication()` accordingly.
6. In your CAN driver, override the weak function `MicroIsoTp_HwTransmit()` to actually place a frame on the bus, and call `MicroIsoTp_HwTransmitDone()` once that frame has been confirmed sent (e.g. from the CAN Tx-complete interrupt).
7. In your upper layer (e.g. UDS), override the weak function `MicroIsoTp_Rx_Indication()` to receive fully reassembled messages, and optionally override `MicroIsoTp_Tx_Confirmation()` to be notified when a message you started with `MicroIsoTp_TxStart()` has finished sending.

## API reference

### Lifecycle

| Function | Description |
|---|---|
| `MicroIsoTp_Init(void)` | Initialize Rx/Tx state machines and the shared buffer. Call once at startup. |
| `MicroIsoTp_TickHandler(void)` | Periodic tick, call at `MICROISOTP_FREQ_HZ`. Only increments timer counters. |
| `MicroIsoTp_TimerHandler(void)` | Periodic main-function, call from the main loop. Evaluates timeouts and drives the state machines. |

### CAN driver → stack (reception)

| Function | Description |
|---|---|
| `MicroIsoTp_Physical_RxIndication(data, dlc)` | Feed a frame received on the physical CAN ID. |
| `MicroIsoTp_Functional_RxIndication(data, dlc)` | Feed a frame received on the functional CAN ID. Non-SF frames are discarded. |
| `MicroIsoTp_HwTransmitDone(void)` | Notify the stack that the last frame handed to `MicroIsoTp_HwTransmit()` has gone out on the bus. |

### Stack → CAN driver (transmission)

| Function | Description |
|---|---|
| `MicroIsoTp_HwTransmit(data, dlc)` | Weak. Override in the CAN driver to actually transmit one CAN frame. Return 0 on success. |

### Upper layer → stack (sending a message)

| Function | Description |
|---|---|
| `MicroIsoTp_TxStart(data, len)` | Start sending a message (SF or FF+CF, chosen automatically). `len` must be 1-4095 bytes. Returns `MICROISOTP_BUSY` if a Tx or Rx transaction is already in progress. |
| `MicroIsoTp_TxStop(void)` | Abort the current transmission and return the Tx state machine to idle. |
| `MicroIsoTp_Tx_Confirmation(void)` | Weak. Override to be notified when the last `MicroIsoTp_TxStart()` message has finished sending. |
| `MicroIsoTp_Tx_SetNAsTimeout(tick)` | Override the N_As timeout. |
| `MicroIsoTp_Tx_SetNBsTimeout(tick)` | Override the N_Bs timeout. |

### Stack → upper layer (receiving a message)

| Function | Description |
|---|---|
| `MicroIsoTp_Rx_Indication(type, data, len)` | Weak. Override to receive a fully reassembled message. `type` tells you whether it arrived via physical or functional addressing. |
| `MicroIsoTp_Rx_SetFlowControl(Fc, Bs, Stmin)` | Pre-configure the Flow Control parameters (Block Size / STmin) to use for the next reception. If not called, `MICROISOTP_RX_DEFAULT_BS` / `MICROISOTP_RX_DEFAULT_STMIN` are used. |
| `MicroIsoTp_Rx_SetNArTimeout(tick)` | Override the N_Ar timeout. |
| `MicroIsoTp_Rx_SetNBrTimeout(tick)` | Override the N_Br timeout. |
| `MicroIsoTp_Rx_SetNCrTimeout(tick)` | Override the N_Cr timeout. |

## Configuration (`MicroIsoTp_conf.h`)

| Macro | Meaning | Default |
|---|---|---|
| `MICROISOTP_FREQ_HZ` | Tick frequency driving all timers | 1000 |
| `MICROISOTP_BUFFER_SIZE` | Shared Rx/Tx payload buffer size (bytes) | 4096 |
| `MICROISOTP_RX_DEFAULT_BS` | Default Block Size we grant senders | 0 (unlimited) |
| `MICROISOTP_RX_DEFAULT_STMIN` | Default STmin we require from senders | 127 |
| `MICROISOTP_RX_DEFAULT_N_CR_TIMEOUT` | N_Cr default, ms | 500 |
| `MICROISOTP_RX_DEFAULT_N_AR_TIMEOUT` | N_Ar default, ms | 500 |
| `MICROISOTP_RX_DEFAULT_N_BR_TIMEOUT` | N_Br default, ms | 250 |
| `MICROISOTP_TX_DEFAULT_N_AS_TIMEOUT` | N_As default, ms | 700 |
| `MICROISOTP_TX_DEFAULT_N_BS_TIMEOUT` | N_Bs default, ms | 1000 |

## Status codes (`MicroIsoTp_Status_t`)

| Value | Meaning |
|---|---|
| `MICROISOTP_OK` | Success |
| `MICROISOTP_ERR` | Generic error (e.g. protocol sequencing error) |
| `MICROISOTP_BUSY` | A Tx or Rx transaction is already in progress |
| `MICROISOTP_OVERFLOW` | Requested/received length exceeds the shared buffer or the supported FF_DL range |
| `MICROISOTP_PARAM_INVALID` | A required pointer/argument was invalid |

## License

Add your license here.