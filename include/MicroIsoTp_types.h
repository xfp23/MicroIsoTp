#ifndef MICROIOSTP_TYPES_H
#define MICROIOSTP_TYPES_H

#include "MicroIsoTp_conf.h"
#include "MicroIsoTp_utils.h"
#include "stdint.h"
#include "string.h"
#include "stdlib.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef enum 
{
    MICROIOSTP_OK,
    MICROIOSTP_ERR,
}MicroIsoTp_Status_t; // 状态码

#ifdef __cplusplus
}
#endif


#endif
