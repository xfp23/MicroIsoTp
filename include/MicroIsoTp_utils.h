/**
 * @file MicroIsoTp_utils.h
 * @author https://xfp23.github.io/
 * @brief Helper macros and inline functions shared across the ISO-TP stack
 * @version 0.1
 * @date 2026-09-27
 *
 * @copyright Copyright (c) 2026
 *
 */
#ifndef MICROISOTP_UTILS_H
#define MICROISOTP_UTILS_H

#include "MicroIsoTp_conf.h"
#include "stdlib.h"
#include "stdint.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief Bail out with MICROISOTP_OVERFLOW if x is not smaller than the shared buffer size.
 * @note Intended for use inside functions returning MicroIsoTp_Status_t.
 */
#define MICROISOTP_CHECK_SIZE(x)             \
    do                                       \
    {                                        \
        if (x >= MICROISOTP_BUFFER_SIZE)     \
        {                                    \
            return MICROISOTP_OVERFLOW;      \
        }                                    \
    } while (0)

/**
 * @brief Bail out with MICROISOTP_PARAM_INVALID if x is NULL.
 * @note Intended for use inside functions returning MicroIsoTp_Status_t.
 */
#define MICROISOTP_CHECK_PTR(x)                  \
    do                                           \
    {                                            \
        if (x == NULL)                           \
            return MICROISOTP_PARAM_INVALID;     \
    } while (0)

/** @brief Convert a millisecond value into a tick count, based on MICROISOTP_FREQ_HZ. */
#define MICROISOTP_MS_TICK(ms) (((uint32_t)(ms) * MICROISOTP_FREQ_HZ) / 1000U)

/**
 * @brief Convert a protocol-level STmin byte into a local tick count.
 *
 * Handles all three STmin ranges defined by ISO 15765-2:
 *  - 0x00-0x7F : 0-127 ms, converted directly via MICROISOTP_MS_TICK
 *  - 0xF1-0xF9 : 100-900 microseconds, approximated to whole ticks
 *    (values below 500us round down to 0, values at/above round up to 1 tick)
 *  - anything else (reserved values) : treated conservatively as the maximum
 *    valid STmin (0x7F / 127 ms), so an unexpected/corrupt value never causes
 *    us to send faster than the peer can actually handle.
 *
 * @param Stmin raw STmin byte as received in (or to be sent in) a Flow Control frame
 * @return tick count to wait between two Consecutive Frames
 */
static inline uint32_t MicroIsoTp_Tx_StminToTick(uint8_t Stmin)
{
    if (Stmin <= 0x7FU)
    {
        return MICROISOTP_MS_TICK(Stmin);
    }
    else if (Stmin >= 0xF1U && Stmin <= 0xF9U)
    {
        if (Stmin < 0xF5U)
            return 0U;
        else
            return MICROISOTP_MS_TICK(1U);
    }

    /* Reserved value: fall back to the most conservative valid STmin. */
    return MICROISOTP_MS_TICK(0x7FU);
}

#ifdef __cplusplus
}
#endif

#endif