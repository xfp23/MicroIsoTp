#ifndef MICROISOTP_UTILS_H
#define MICROISOTP_UTILS_H

#include "MicroIsoTp_conf.h"
#include "stdlib.h"
#include "stdint.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define MICROISOTP_CHECK_SIZE(x)         \
do                                   \
{                                    \
    if (x >= MICROISOTP_BUFFER_SIZE) \
    {                                \
        return MICROISOTP_OVERFLOW;  \
    }                                \
} while (0)

#define MICROISOTP_CHECK_PTR(x)              \
do                                       \
{                                        \
    if (x == NULL)                       \
        return MICROISOTP_PARAM_INVALID; \
} while (0)

#define MICROISOTP_MS_TICK(ms) (((uint32_t)(ms) * MICROISOTP_FREQ_HZ) / 1000U)

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
    return 0U;
}

#ifdef __cplusplus
}
#endif

#endif
