/**
 * @file MicroIsoTp_conf.h
 * @author https://xfp23.github.io/
 * @brief MicroIsoTp configuration file
 * @version 0.1
 * @date 2026-09-27
 *
 * @copyright Copyright (c) 2026
 *
 */

#ifndef MICROISOTP_CONF_H
#define MICROISOTP_CONF_H

#ifdef __cplusplus
extern "C"
{
#endif

/** @brief Stack version string */
#define MICROISOTP_VERSION "0.0.1"

/** @brief Tick frequency of the driving timer, in Hz (e.g. 1000 = 1 tick per ms) */
#define MICROISOTP_FREQ_HZ (1000U)

/** @brief Shared Rx/Tx payload buffer size, in bytes */
#define MICROISOTP_BUFFER_SIZE (4096U)

/** @brief Default Block Size (BS) we request from the sender when receiving. 0 = unlimited (no block boundary). */
#define MICROISOTP_RX_DEFAULT_BS (0U)

/** @brief Default STmin (in ms, protocol raw value) we request from the sender when receiving. */
#define MICROISOTP_RX_DEFAULT_STMIN (127U)

/* All timeout values below are expressed in milliseconds. */

/** @brief N_Cr: max time to wait for the next Consecutive Frame while receiving. */
#define MICROISOTP_RX_DEFAULT_N_CR_TIMEOUT (500U)

/** @brief N_Ar: max time to wait for our own Flow Control frame to actually go out on the bus. */
#define MICROISOTP_RX_DEFAULT_N_AR_TIMEOUT (500U)

/** @brief N_Br: internal delay before re-issuing a new Flow Control after a Block completes. */
#define MICROISOTP_RX_DEFAULT_N_BR_TIMEOUT (250U)

/** @brief N_As: max time to wait for our own frame (SF/FF/CF) to actually go out on the bus. */
#define MICROISOTP_TX_DEFAULT_N_AS_TIMEOUT (700U)

/** @brief N_Bs: max time to wait for the peer's Flow Control frame while sending. */
#define MICROISOTP_TX_DEFAULT_N_BS_TIMEOUT (1000U)

#ifdef __cplusplus
}
#endif

#endif