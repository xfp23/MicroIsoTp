/**
 * @file MicroIsoTp_tx.c
 * @author https://xfp23.github.io/
 * @brief 
 * @version 0.1
 * @date 2026-09-27
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#include "MicroIsoTp.h"

extern MicroIsoTp_Obj_t MicroIsoTp_Obj;

static MicroIsoTp_Tx_Obj_t *tx_obj = &MicroIsoTp_Obj.tx_obj;

// 硬件发送
static MicroIsoTp_Status_t MicroIsoTp_Tx_HwTransmit(uint8_t *data, size_t len)
{
    if(MicroIsoTp_HwTransmit(data,len) != 0) // 此函数由  #include "MicroIsoTp.h" 提供接口
    {
        return MICROISOTP_HWTX_ERR;
    }

    return MICROISOTP_OK;
}

// 初始化
void MicroIsoTp_Tx_Init(void)
{
    memset(tx_obj, 0, sizeof(MicroIsoTp_Tx_Obj_t));

    tx_obj->buf = MicroIsoTp_Obj.buf;
    tx_obj->N_As.timeout = MICROISOTP_MS_TICK(MICROISOTP_TX_DEFAULT_N_AS_TIMEOUT);
    tx_obj->N_Bs.timeout = MICROISOTP_MS_TICK(MICROISOTP_TX_DEFAULT_N_BS_TIMEOUT);
    tx_obj->N_Cs.timeout = 0;
    tx_obj->reset = false;
}

// 通知上层：本次发送已完成
void __attribute__((weak)) MicroIsoTp_Tx_Confirmation(void)
{
    // 如果需要就重写，不需要就不管
}

// 启动发送
MicroIsoTp_Status_t MicroIsoTp_Tx_TxStart(const uint8_t *data, size_t len)
{
    MICROISOTP_CHECK_PTR(data);

    if (len == 0)
    {
        return MICROISOTP_ERR;
    }

    if (len > 4095) // 未实现FF转义格式，暂不支持单次发送超过4095字节
    {
        return MICROISOTP_OVERFLOW;
    }

    if (tx_obj->step != MICROISOTP_TX_STEP_IDLE)
    {
        return MICROISOTP_BUSY;
    }

    memset(tx_obj->buf, 0, MICROISOTP_BUFFER_SIZE);
    memcpy(tx_obj->buf, data, len);

    tx_obj->tx_len     = len;
    tx_obj->buf_offset = 0;
    tx_obj->buf_remain = len;

    if (len < 8)
    {
        tx_obj->step = MICROISOTP_TX_STEP_SF;
    }
    else
    {
        tx_obj->step = MICROISOTP_TX_STEP_FF;
    }

    return MICROISOTP_OK;
}

void MicroIsoTp_Tx_TickHandler(void)
{
    if (tx_obj->N_As.en)
    {
        tx_obj->N_As.tick++;
    }
    else
    {
        tx_obj->N_As.tick = 0;
    }

    if (tx_obj->N_Bs.en)
    {
        tx_obj->N_Bs.tick++;
    }
    else
    {
        tx_obj->N_Bs.tick = 0;
    }

    if (tx_obj->N_Cs.en)
    {
        tx_obj->N_Cs.tick++;
    }
    else
    {
        tx_obj->N_Cs.tick = 0;
    }
}

void MicroIsoTp_Tx_TimerHandler(void)
{
    if (tx_obj->N_As.en)
    {
        if (tx_obj->N_As.tick >= tx_obj->N_As.timeout)
        {
            tx_obj->N_As.en = false;
            tx_obj->reset = true;
            tx_obj->step  = MICROISOTP_TX_STEP_IDLE; // 帧迟迟没发出去，复位
        }
    }

    if (tx_obj->N_Bs.en)
    {
        if (tx_obj->N_Bs.tick >= tx_obj->N_Bs.timeout)
        {
            tx_obj->N_Bs.en = false;
            tx_obj->reset = true;
            tx_obj->step  = MICROISOTP_TX_STEP_IDLE; // 迟迟等不到流控，复位
        }
    }

    if (tx_obj->N_Cs.en)
    {
        if (tx_obj->N_Cs.tick >= tx_obj->N_Cs.timeout)
        {
            uint8_t send_len = (tx_obj->buf_remain >= 7) ? 7 : (uint8_t)tx_obj->buf_remain;

            tx_obj->N_Cs.tick = 0;
            tx_obj->N_Cs.en   = false;
            tx_obj->N_As.en   = true;

            memset(tx_obj->CF.data, 0, 8);
            tx_obj->CF.data[0] = (uint8_t)(MICROISOTP_PCI_CF << 4 | tx_obj->CF.sn);
            memcpy(&tx_obj->CF.data[1], tx_obj->buf + tx_obj->buf_offset, send_len);

            tx_obj->buf_offset += send_len;
            tx_obj->buf_remain -= send_len;

            MicroIsoTp_Tx_HwTransmit(tx_obj->CF.data, 8);
        }
    }

    switch (tx_obj->step)
    {
    case MICROISOTP_TX_STEP_IDLE:

        if (tx_obj->reset)
        {
            tx_obj->reset = false;

            memset((void *)tx_obj, 0, sizeof(MicroIsoTp_Tx_Obj_t));
            tx_obj->buf = MicroIsoTp_Obj.buf;

            tx_obj->N_As.timeout = MICROISOTP_MS_TICK(MICROISOTP_TX_DEFAULT_N_AS_TIMEOUT);
            tx_obj->N_Bs.timeout = MICROISOTP_MS_TICK(MICROISOTP_TX_DEFAULT_N_BS_TIMEOUT);
            tx_obj->N_Cs.timeout = 0;
        }
        break;

    case MICROISOTP_TX_STEP_SF: // 发送单帧

        memset(tx_obj->SF, 0, 8);
        tx_obj->SF[0] = (uint8_t)(MICROISOTP_PCI_SF << 4) | (uint8_t)(tx_obj->tx_len & 0x0F);
        memcpy(&tx_obj->SF[1], tx_obj->buf, tx_obj->tx_len);

        tx_obj->N_As.en = true;
        MicroIsoTp_Tx_HwTransmit(tx_obj->SF, 8);
        tx_obj->step = MICROISOTP_TX_STEP_SF_DONE;
        break;

    case MICROISOTP_TX_STEP_SF_DONE: // 等待SF硬件发送确认
        break;

    case MICROISOTP_TX_STEP_FF: // 发送首帧

        memset(tx_obj->FF, 0, 8);
        tx_obj->FF[0] = (uint8_t)(MICROISOTP_PCI_FF << 4) | (uint8_t)((tx_obj->tx_len >> 8) & 0x0F);
        tx_obj->FF[1] = (uint8_t)(tx_obj->tx_len & 0xFF);
        memcpy(&tx_obj->FF[2], tx_obj->buf, 6);

        tx_obj->buf_offset = 6;
        tx_obj->buf_remain -= 6;
        tx_obj->CF.sn = 1;

        MicroIsoTp_Tx_HwTransmit(tx_obj->FF, 8);
        tx_obj->N_As.en = true;
        tx_obj->step = MICROISOTP_TX_STEP_FC; // 等待流控
        break;

    case MICROISOTP_TX_STEP_FC: // 等流控
        break;

    case MICROISOTP_TX_STEP_CF: // 准备发下一个连续帧，交给N_Cs节拍触发实际发送
        tx_obj->N_Cs.en = true;
        tx_obj->N_Cs.tick = 0;
        tx_obj->N_Cs.timeout = MicroIsoTp_Tx_StminToTick(tx_obj->FC.Stmin);
        tx_obj->step = MICROISOTP_TX_STEP_CF_DONE;
        break;

    case MICROISOTP_TX_STEP_CF_DONE: // 等待连续帧硬件发送确认
        break;

    case MICROISOTP_TX_STEP_COMPLETE:
        MicroIsoTp_Tx_Confirmation(); // 通知上层：本次发送已完成
        tx_obj->reset = true;
        tx_obj->step = MICROISOTP_TX_STEP_IDLE;
        break;

    default:
        break;
    }
}

// CAN驱动收到一帧数据时调用（Tx方向只需要处理流控帧，不需要地址类型，
// 因为流控帧永远只会从当前正在进行的这次物理会话上回来）
void MicroIsoTp_Tx_HandleCanFrame(const uint8_t *data, size_t dlc)
{
    if (tx_obj->step != MICROISOTP_TX_STEP_FC)
    {
        return;
    }

    if (data == NULL || dlc == 0 || dlc > 8)
    {
        return;
    }

    uint8_t pci = (uint8_t)(data[0] >> 4);

    if (pci != MICROISOTP_PCI_FC)
    {
        return; // 不是流控帧
    }

    tx_obj->FC.Fs    = data[0] & 0x0F;
    tx_obj->FC.Bs    = data[1];
    tx_obj->FC.Stmin = data[2];

    memcpy(tx_obj->FC.data, data, dlc);

    switch (tx_obj->FC.Fs)
    {
    case MICROISOTP_FS_CTS:
        tx_obj->N_Bs.en = false;
        tx_obj->bs_count = tx_obj->FC.Bs;
        tx_obj->step = MICROISOTP_TX_STEP_CF;
        break;

    case MICROISOTP_FS_WAIT:
        tx_obj->N_Bs.en = true;
        tx_obj->N_Bs.tick = 0;
        break;

    case MICROISOTP_FS_OVERFL:
        tx_obj->N_Bs.en = false;
        tx_obj->reset = true;
        tx_obj->step = MICROISOTP_TX_STEP_IDLE; // 终止本次发送
        break;

    default:
        break;
    }
}

// 硬件发送完成回调
void MicroIsoTp_Tx_HwTransmitDone(void)
{
    tx_obj->N_As.en = false;

    if (tx_obj->step == MICROISOTP_TX_STEP_SF_DONE)
    {
        tx_obj->step = MICROISOTP_TX_STEP_COMPLETE;
    }
    else if (tx_obj->step == MICROISOTP_TX_STEP_CF_DONE)
    {
        if (++tx_obj->CF.sn > 15)
        {
            tx_obj->CF.sn = 0;
        }

        if (tx_obj->FC.Bs != 0)
        {
            tx_obj->bs_count--;
        }

        if (tx_obj->buf_remain == 0)
        {
            tx_obj->step = MICROISOTP_TX_STEP_COMPLETE;
        }
        else if (tx_obj->FC.Bs != 0 && tx_obj->bs_count == 0)
        {
            tx_obj->step = MICROISOTP_TX_STEP_FC; // 这个Block发完了，等新的流控
        }
        else
        {
            tx_obj->step = MICROISOTP_TX_STEP_CF; // 继续发下一个CF
        }
    }
    else if (tx_obj->step == MICROISOTP_TX_STEP_FC)
    {
        tx_obj->N_Bs.en = true;
        tx_obj->N_Bs.tick = 0;
    }
}

void MicroIsoTp_Tx_SetNAsTimeout(uint32_t tick)
{
    tx_obj->N_As.timeout = tick;
}

void MicroIsoTp_Tx_SetNBsTimeout(uint32_t tick)
{
    tx_obj->N_Bs.timeout = tick;
}