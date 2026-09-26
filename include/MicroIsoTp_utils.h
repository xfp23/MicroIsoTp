#ifndef MICROISOTP_UTILS_H
#define MICROISOTP_UTILS_H

#include "MicroIsoTp_conf.h"

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

#ifdef __cplusplus
}
#endif

#endif
