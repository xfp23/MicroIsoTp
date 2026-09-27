#include "MicroIsoTp.h"

extern MicroIsoTp_Obj_t MicroIsoTp_Obj;

MicroIsoTp_Rx_Obj_t *rx_obj = &MicroIsoTp_Obj.rx_obj;

static MicroIsoTp_Status_t MicroIsoTp_Rx_HwTransmit(uint8_t *data, size_t len)
{
    
    if(MicroIsoTp_HwTransmit(data,len) != 0) 
    {
        return MICROISOTP_HWTX_ERR;
    }

    return MICROISOTP_OK;
}

/**
 * @brief CAN驱动通知：上一次发的那一帧（这里特指流控帧）已经真正发到总线上了
 */
void MicroIsoTp_Rx_HwTransmitDone(void)
{
    rx_obj->N_Ar.en = false;
}

MicroIsoTp_Status_t MicroIsoTp_Rx_Init(void)
{
    memset((void *)rx_obj, 0, sizeof(MicroIsoTp_Rx_Obj_t));
    rx_obj->buf = MicroIsoTp_Obj.buf;

    rx_obj->N_Cr.timeout = MICROISOTP_MS_TICK(MICROISOTP_RX_DEFAULT_N_CR_TIMEOUT);
    rx_obj->N_Ar.timeout = MICROISOTP_MS_TICK(MICROISOTP_RX_DEFAULT_N_AR_TIMEOUT);
    rx_obj->N_Br.timeout = MICROISOTP_MS_TICK(MICROISOTP_RX_DEFAULT_N_BR_TIMEOUT);

    rx_obj->reset = false;
    return MICROISOTP_OK;
}

void MicroIsoTp_Rx_TimerHandler(void)
{
    if (rx_obj->N_Cr.en)
    {
        if (rx_obj->N_Cr.tick >= rx_obj->N_Cr.timeout)
        {
            rx_obj->N_Cr.en = false;
            rx_obj->reset = true;
            rx_obj->step = MICROISOTP_RX_STEP_IDLE; // 等CF等超时，复位
        }
    }

    if (rx_obj->N_Ar.en)
    {
        if (rx_obj->N_Ar.tick >= rx_obj->N_Ar.timeout)
        {
            rx_obj->N_Ar.en = false;
            rx_obj->reset = true;
            rx_obj->step = MICROISOTP_RX_STEP_IDLE; // 流控帧迟迟没发出去，复位
        }
    }

    if (rx_obj->N_Br.en)
    {
        if (rx_obj->N_Br.tick >= rx_obj->N_Br.timeout)
        {
            rx_obj->step = MICROISOTP_RX_STEP_FC; // 收完了一个Block，重新发流控
            rx_obj->N_Br.en = false;
        }
    }

    switch (rx_obj->step)
    {
    case MICROISOTP_RX_STEP_IDLE:

        if (rx_obj->reset)
        {
            memset(rx_obj, 0, sizeof(MicroIsoTp_Rx_Obj_t));
            rx_obj->buf = MicroIsoTp_Obj.buf;

            rx_obj->N_Cr.timeout = MICROISOTP_MS_TICK(MICROISOTP_RX_DEFAULT_N_CR_TIMEOUT);
            rx_obj->N_Ar.timeout = MICROISOTP_MS_TICK(MICROISOTP_RX_DEFAULT_N_AR_TIMEOUT);
            rx_obj->N_Br.timeout = MICROISOTP_MS_TICK(MICROISOTP_RX_DEFAULT_N_BR_TIMEOUT);

            rx_obj->reset = false;
        }
        break;

    case MICROISOTP_RX_STEP_SF:

        MicroIsoTp_Rx_Indication(rx_obj->addr_type, rx_obj->buf, rx_obj->rx_len);
        rx_obj->reset = true;
        rx_obj->step = MICROISOTP_RX_STEP_IDLE;
        break;

    case MICROISOTP_RX_STEP_FC: // 开始/继续多帧：发一个流控

        MicroIsoTp_Rx_HwTransmit(rx_obj->FC.data, 8);
        rx_obj->N_Ar.en = true;
        rx_obj->step = MICROISOTP_RX_STEP_FC_COMP;
        break;

    case MICROISOTP_RX_STEP_FC_COMP:

        if (rx_obj->Exit)
        {
            rx_obj->step  = MICROISOTP_RX_STEP_IDLE;
            rx_obj->reset = true;
            break;
        }

        rx_obj->N_Cr.en = true;           // 发送流控之后开始等CF
        rx_obj->bs_count = rx_obj->FC.Bs; // copy本次Block大小
        break;

    case MICROISOTP_RX_STEP_FF: // 首帧收完了

        if (!rx_obj->FC.External_en) // 外部没预设流控参数的话，用默认值
        {
            MicroIsoTp_Rx_SetFlowControl(MICROISOTP_FS_CTS, MICROISOTP_RX_DEFAULT_BS, MICROISOTP_RX_DEFAULT_STMIN);
            rx_obj->FC.External_en = false; // 把标志位抢过来
        }
        rx_obj->step = MICROISOTP_RX_STEP_FC;
        break;

    case MICROISOTP_RX_CF_COMP:
        rx_obj->N_Cr.en = false;
        MicroIsoTp_Rx_Indication(rx_obj->addr_type, rx_obj->buf, rx_obj->rx_len); // 给上层把数据丢过去
        rx_obj->step  = MICROISOTP_RX_STEP_IDLE;
        rx_obj->reset = true;
        break;

    default:
        break;
    }
}

void MicroIsoTp_Rx_TickHandler(void)
{
    if (rx_obj->N_Cr.en)
    {
        rx_obj->N_Cr.tick++;
    }
    else
    {
        rx_obj->N_Cr.tick = 0;
    }

    if (rx_obj->N_Ar.en)
    {
        rx_obj->N_Ar.tick++;
    }
    else
    {
        rx_obj->N_Ar.tick = 0;
    }

    if (rx_obj->N_Br.en)
    {
        rx_obj->N_Br.tick++;
    }
    else
    {
        rx_obj->N_Br.tick = 0;
    }
}

// 接收一个CAN帧
MicroIsoTp_Status_t MicroIsoTp_Rx_HandleCanFrame(MicroIsoTp_AddrType_t type,const uint8_t *data, size_t len)
{
    MICROISOTP_CHECK_PTR(data);

    if (len == 0 || len > 8)
    {
        return MICROISOTP_PARAM_INVALID;
    }

    uint8_t pci = ((data[0] & 0xF0) >> 4);
    uint16_t rx_len = 0;

    if (type == MICROISOTP_ADDR_FUC && pci != MICROISOTP_PCI_SF)
    {
        return MICROISOTP_OK;
    }

    switch (pci)
    {
    case MICROISOTP_PCI_SF:
    {
        rx_len = data[0] & 0x0F;

        if (rx_len == 0 || rx_len > 7)
        {
            break;
        }

        rx_obj->rx_len = rx_len;
        memcpy(rx_obj->buf, &data[1], rx_len);
        rx_obj->step = MICROISOTP_RX_STEP_SF;
        rx_obj->addr_type = type;
    }
    break;

    case MICROISOTP_PCI_FF:
    {
        if (rx_obj->step != MICROISOTP_RX_STEP_IDLE)
        {
            return MICROISOTP_BUSY;
        }

        rx_len = (uint16_t)((data[0] & 0x0F) << 8 | data[1]);

        if (rx_len < 8 || rx_len >= MICROISOTP_BUFFER_SIZE) // 应该走单帧或者超出缓冲区
        {
            MicroIsoTp_Rx_SetFlowControl(MICROISOTP_FS_OVERFL, 0, 0);
            rx_obj->Exit = true;
            rx_obj->FC.External_en = false;
            rx_obj->step = MICROISOTP_RX_STEP_FC; // 发一个流控告诉对方溢出
            return MICROISOTP_ERR;
        }

        rx_obj->Exit = false;
        memcpy(rx_obj->buf, &data[2], 6);
        rx_obj->rx_len = rx_len;
        rx_obj->len_count = 6;
        rx_obj->next_sn = 1;
        rx_obj->addr_type = type;
        rx_obj->step = MICROISOTP_RX_STEP_FF;
    }
    break;

    case MICROISOTP_PCI_CF:
    {
        uint8_t sn = data[0] & 0x0F;

        if (rx_obj->step != MICROISOTP_RX_STEP_FC_COMP)
        {
            return MICROISOTP_ERR; // 错帧
        }

        if (sn != rx_obj->next_sn)
        {
            return MICROISOTP_ERR; // 乱帧
        }

        if (rx_obj->FC.Bs != 0 && rx_obj->bs_count == 0)
        {
            rx_obj->N_Br.en = true;
            return MICROISOTP_BUSY;
        }

        if (rx_obj->FC.Bs != 0)
        {
            rx_obj->bs_count--;
        }

        rx_obj->N_Cr.tick = 0;

        rx_obj->next_sn++;
        if (rx_obj->next_sn > 15)
        {
            rx_obj->next_sn = 0;
        }

        {
            uint16_t remain = rx_obj->rx_len - rx_obj->len_count;
            uint8_t  copy_len = (remain < 7) ? (uint8_t)remain : 7;
            memcpy(rx_obj->buf + rx_obj->len_count, &data[1], copy_len);
            rx_obj->len_count += copy_len;
        }

        if (rx_obj->len_count >= rx_obj->rx_len)
        {
            rx_obj->step = MICROISOTP_RX_CF_COMP; // 收完了
        }
    }
    break;

    case MICROISOTP_PCI_FC: // Rx模块不可能收到流控帧，忽略
        break;
    }

    return MICROISOTP_OK;
}

void __attribute__((weak)) MicroIsoTp_Rx_Indication(MicroIsoTp_AddrType_t type, uint8_t *data, size_t len) // 组包完成，通知上层
{
    (void)type;
    (void)data;
    (void)len;
}

// 构造接收侧要发送的流控帧内容（只构造，不发送）
void MicroIsoTp_Rx_SetFlowControl(MicroIsoTp_FS_t Fs, uint8_t Bs, uint8_t Stmin)
{
    memset(rx_obj->FC.data,0,8);
    
    rx_obj->FC.Bs = Bs;
    rx_obj->FC.Fs = Fs;
    rx_obj->FC.Stmin = Stmin;

    rx_obj->FC.data[0] = ((uint8_t)MICROISOTP_PCI_FC << 4) | rx_obj->FC.Fs;
    rx_obj->FC.data[1] = Bs;
    rx_obj->FC.data[2] = Stmin;

    rx_obj->FC.External_en = true;
}

void MicroIsoTp_Rx_SetNCrTimeout(uint32_t tick)
{
    rx_obj->N_Cr.timeout = tick;
}

void MicroIsoTp_Rx_SetNBrTimeout(uint32_t tick)
{
    rx_obj->N_Br.timeout = tick;
}

void MicroIsoTp_Rx_SetNArTimeout(uint32_t tick)
{
    rx_obj->N_Ar.timeout = tick;
}
