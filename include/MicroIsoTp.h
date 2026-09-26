/**
 * @file MicroIsoTp.h
 * @author xfp23
 * @brief 
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

// RX區域

/**
 * @brief Setting N_Ar Timer
 * 
 * @param tick 
 */
void MicroIsoTp_Rx_SetNArTimeout(uint32_t tick);

/**
 * @brief Setting N_Br Timer 
 * 
 * @param tick 
 */
void MicroIsoTp_Rx_SetNBrTimeout(uint32_t tick);

/**
 * @brief Setting N_Cr Timer
 * 
 * @param tick 
 */
void MicroIsoTp_Rx_SetNCrTimeout(uint32_t tick);

/**
 * @brief when your Can Frame Send Complete, use the funciton
 * 
 */
void MicroIsoTp_Rx_HwTransmitDone(void);

/**
 * @brief rx success noticion , overridden by the user.
 * 
 * @param type 
 * @param data 
 * @param len 
 */
extern void MicroIsoTp_Rx_Indication(MicroIsoTp_AddrType_t type, uint8_t *data, size_t len);

/**
 * @brief Setting the Stack's Flow control
 * 
 * @param Fc 
 * @param Bs 
 * @param Stmin 
 */
extern void MicroIsoTp_Rx_SetFlowControl(MicroIsoTp_FS_t Fc, uint8_t Bs, uint8_t Stmin);

/**
 * @brief Timer driver function, `MICROISOTP_FREQ_HZ` tick
 * 
 */
extern void MicroIsoTp_TickHandler(void);

/**
 * @brief Init
 * 
 */
extern void MicroIsoTp_Init(void);
/**
 * @brief Hardware transmission function; to be overridden by the user.
 * 
 * @param data 
 * @param dlc 
 * @return int 
 */
extern int MicroIsoTp_HwTransmit(const uint8_t *data, uint8_t dlc);

/**
 * @brief Physical rx callback
 * 
 * @param data 
 * @param dlc 
 */
extern void MicroIsoTp_Physical_RxIndication(const uint8_t *data, uint8_t dlc);

/**
 * @brief 
 * 
 * @param data 
 * @param dlc 
 */
extern void MicroIsoTp_Functional_RxIndication(const uint8_t *data, uint8_t dlc);

/**
 * @brief TimerHandler
 * 
 */
extern void MicroIspTp_TimerHandler(void);

#ifdef __cplusplus
}
#endif

#endif 
