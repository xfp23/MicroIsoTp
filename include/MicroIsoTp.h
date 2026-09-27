/**
 * @file MicroIsoTp.h
 * @author https://xfp23.github.io/
 * @brief Public API of the MicroIsoTp stack (ISO 15765-2, classic CAN, 8-byte frames)
 * @version 0.1
 * @date 2026-09-27
 *
 * @copyright Copyright (c) 2026
 *
 */
#ifndef MICROISOTP_H
#define MICROISOTP_H

#include "MicroIsoTp_types.h"

#ifdef __cplusplus
extern "C"
{
#endif

/* ---------------------------------------------------------------------- */
/* Lifecycle                                                              */
/* ---------------------------------------------------------------------- */

/** @brief Initialize the stack (Rx and Tx state machines, shared buffer). Call once at startup. */
extern void MicroIsoTp_Init(void);

/**
 * @brief Periodic tick counter, to be called at a fixed rate of MICROISOTP_FREQ_HZ.
 * @note Only increments internal timer counters. Keep this ISR/tick-context safe and cheap;
 *       all timeout evaluation and state machine work happens in MicroIsoTp_TimerHandler().
 */
extern void MicroIsoTp_TickHandler(void);

/**
 * @brief Periodic main-function, drives the Rx and Tx state machines.
 * @note Call this regularly from the main loop (or a lower-priority periodic task).
 *       Evaluates all timeouts (N_As/N_Bs/N_Cs/N_Ar/N_Br/N_Cr) and advances whichever
 *       state machine (Rx/Tx) is currently active.
 */
extern void MicroIsoTp_TimerHandler(void);

/* ---------------------------------------------------------------------- */
/* CAN driver -> stack: frame reception entry points                      */
/* ---------------------------------------------------------------------- */

/**
 * @brief Call this from the CAN driver whenever a frame arrives on the physical (point-to-point) CAN ID.
 * @note Internally routes the frame to either the Rx state machine (new SF/FF/CF) or the
 *       Tx state machine (Flow Control for an ongoing transmission), based on whether a
 *       Tx transaction is currently in progress. Both machines share one payload buffer,
 *       so this routing is also what keeps that shared buffer from being corrupted -
 *       do not change the routing condition without re-checking that guarantee.
 * @param data CAN frame payload
 * @param dlc  CAN frame data length (1-8)
 */
extern void MicroIsoTp_Physical_RxIndication(const uint8_t *data, uint8_t dlc);

/**
 * @brief Call this from the CAN driver whenever a frame arrives on the functional (broadcast) CAN ID.
 * @note Per ISO 15765-2, functional addressing only allows Single Frames; any FF/CF received
 *       here is discarded by the Rx state machine.
 * @param data CAN frame payload
 * @param dlc  CAN frame data length (1-8)
 */
extern void MicroIsoTp_Functional_RxIndication(const uint8_t *data, uint8_t dlc);

/* ---------------------------------------------------------------------- */
/* CAN driver -> stack: hardware transmit confirmation                    */
/* ---------------------------------------------------------------------- */

/**
 * @brief Call this from the CAN driver once the frame most recently handed to
 *        MicroIsoTp_HwTransmit() has actually gone out on the bus.
 * @note Internally routed to whichever of Rx (Flow Control frame) or Tx (SF/FF/CF frame)
 *       currently has a frame in flight.
 */
extern void MicroIsoTp_HwTransmitDone(void);

/* ---------------------------------------------------------------------- */
/* Stack -> CAN driver: hardware transmit request                        */
/* ---------------------------------------------------------------------- */

/**
 * @brief Hardware transmit function. Weak by default; override in the CAN driver.
 * @param data frame payload to send
 * @param dlc  data length (1-8)
 * @return 0 on success, non-zero on failure
 */
extern int MicroIsoTp_HwTransmit(const uint8_t *data, uint8_t dlc);

/* ---------------------------------------------------------------------- */
/* Upper layer (e.g. UDS) -> stack: send a message                       */
/* ---------------------------------------------------------------------- */

/**
 * @brief Start sending a message (single- or multi-frame, chosen automatically by length).
 * @param data payload to send
 * @param len  payload length in bytes (must be > 0 and <= 4095; the escape/32-bit FF_DL
 *             format for lengths above 4095 is not implemented)
 * @return MICROISOTP_OK on success, MICROISOTP_BUSY if a Tx or Rx transaction is already
 *         in progress, MICROISOTP_OVERFLOW if len exceeds what a single FF_DL field can carry.
 */
extern MicroIsoTp_Status_t MicroIsoTp_TxStart(const uint8_t *data, size_t len);

/** @brief Abort whatever transmission is currently in progress and return the Tx state machine to idle. */
extern void MicroIsoTp_TxStop(void);

/**
 * @brief Notification that the last MicroIsoTp_TxStart() message finished sending.
 *        Weak by default; override to be notified from the upper layer (e.g. UDS).
 */
extern void MicroIsoTp_Tx_Confirmation(void);

/** @brief Override the default N_As timeout (ms worth of ticks), for the Tx state machine. */
extern void MicroIsoTp_Tx_SetNAsTimeout(uint32_t tick);

/** @brief Override the default N_Bs timeout (ms worth of ticks), for the Tx state machine. */
extern void MicroIsoTp_Tx_SetNBsTimeout(uint32_t tick);

/* ---------------------------------------------------------------------- */
/* Stack -> upper layer (e.g. UDS): message received                      */
/* ---------------------------------------------------------------------- */

/**
 * @brief Notification that a full message (single- or multi-frame) has been reassembled.
 *        Weak by default; override to receive the data in the upper layer (e.g. UDS).
 * @param type addressing type the message arrived on (physical or functional)
 * @param data pointer to the reassembled payload (valid only until the next reception starts)
 * @param len  length of the reassembled payload
 */
extern void MicroIsoTp_Rx_Indication(MicroIsoTp_AddrType_t type, uint8_t *data, size_t len);

/**
 * @brief Pre-configure the Flow Control parameters used for the next reception.
 *        If not called before a First Frame arrives, MICROISOTP_RX_DEFAULT_BS /
 *        MICROISOTP_RX_DEFAULT_STMIN are used instead.
 * @param Fc    flow status to send (normally MICROISOTP_FC_CTS)
 * @param Bs    Block Size we grant to the sender (0 = unlimited)
 * @param Stmin minimum separation time we require between Consecutive Frames
 */
extern void MicroIsoTp_Rx_SetFlowControl(MicroIsoTp_FS_t Fc, uint8_t Bs, uint8_t Stmin);

/** @brief Override the default N_Ar timeout (ms worth of ticks), for the Rx state machine. */
extern void MicroIsoTp_Rx_SetNArTimeout(uint32_t tick);

/** @brief Override the default N_Br timeout (ms worth of ticks), for the Rx state machine. */
extern void MicroIsoTp_Rx_SetNBrTimeout(uint32_t tick);

/** @brief Override the default N_Cr timeout (ms worth of ticks), for the Rx state machine. */
extern void MicroIsoTp_Rx_SetNCrTimeout(uint32_t tick);

#ifdef __cplusplus
}
#endif

#endif