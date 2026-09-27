/**
 * @file MicroIsoTp.c
 * @author https://xfp23.github.io/
 * @brief 
 * @version 0.1
 * @date 2026-09-27
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#include "MicroIsoTp.h"

MicroIsoTp_Obj_t MicroIsoTp_Obj = {0};

extern MicroIsoTp_Status_t MicroIsoTp_Rx_Init(void);

extern MicroIsoTp_Status_t MicroIsoTp_Rx_HandleCanFrame(MicroIsoTp_AddrType_t type, const uint8_t *data, size_t len);

extern void MicroIsoTp_Rx_TickHandler(void);

extern void MicroIsoTp_Rx_TimerHandler(void);

extern void MicroIsoTp_Rx_HwTransmitDone(void);

extern void MicroIsoTp_Tx_Init(void);

extern void MicroIsoTp_Tx_TickHandler(void);

extern void MicroIsoTp_Tx_TimerHandler(void);

extern void MicroIsoTp_Tx_HandleCanFrame(const uint8_t *data, size_t dlc);

extern MicroIsoTp_Status_t MicroIsoTp_Tx_TxStart(const uint8_t *data, size_t len);

extern void MicroIsoTp_Tx_HwTransmitDone(void);

// 物理地址
void MicroIsoTp_Physical_RxIndication(const uint8_t *data, uint8_t dlc)
{
    if (MicroIsoTp_Obj.tx_obj.step == MICROISOTP_TX_STEP_IDLE)
    {
        MicroIsoTp_Rx_HandleCanFrame(MICROISOTP_ADDR_PHY, data, dlc);
    }
    else
    {
        MicroIsoTp_Tx_HandleCanFrame(data, dlc);
    }
}

// 功能地址
void MicroIsoTp_Functional_RxIndication(const uint8_t *data, uint8_t dlc)
{
    if (MicroIsoTp_Obj.tx_obj.step == MICROISOTP_TX_STEP_IDLE)
    {
        MicroIsoTp_Rx_HandleCanFrame(MICROISOTP_ADDR_FUC, data, dlc);
    }
    // MicroIsoTp_Tx_HandleCanFrame(data,dlc); // 功能地址应该不会给tx回复
}

int __attribute__((weak)) MicroIsoTp_HwTransmit(const uint8_t *data, uint8_t dlc)
{
    (void)data;
    (void)dlc;
    return 0;
}

void MicroIsoTp_Init(void)
{
    MicroIsoTp_Tx_Init();
    MicroIsoTp_Rx_Init();

    memset(&MicroIsoTp_Obj.buf, 0, MICROISOTP_BUFFER_SIZE);
}

void MicroIsoTp_TickHandler(void)
{
    MicroIsoTp_Tx_TickHandler();

    MicroIsoTp_Rx_TickHandler();
}

void MicroIsoTp_TimerHandler(void)
{
    MicroIsoTp_Tx_TimerHandler();

    MicroIsoTp_Rx_TimerHandler();
}

MicroIsoTp_Status_t MicroIsoTp_TxStart(const uint8_t *data, size_t len)
{
    if (MicroIsoTp_Obj.rx_obj.step != MICROISOTP_RX_STEP_IDLE)
    {
        return MICROISOTP_BUSY;
    }

    return MicroIsoTp_Tx_TxStart(data, len);
}

// 硬件回调函数
void MicroIsoTp_HwTransmitDone(void)
{
    if (MicroIsoTp_Obj.tx_obj.step != MICROISOTP_TX_STEP_IDLE)
    {
        MicroIsoTp_Tx_HwTransmitDone();
    }
    else if (MicroIsoTp_Obj.rx_obj.step != MICROISOTP_RX_STEP_IDLE)
    {
        MicroIsoTp_Rx_HwTransmitDone();
    }
}

void MicroIsoTp_TxStop(void)
{
    MicroIsoTp_Obj.tx_obj.step = MICROISOTP_TX_STEP_IDLE;
    MicroIsoTp_Obj.tx_obj.reset = true;
}
