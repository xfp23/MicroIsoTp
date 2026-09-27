/**
 * @file example.c
 * @brief MicroIsoTp 集成示例
 *
 * 本文件演示如何把 MicroIsoTp 这套ISO-TP传输层，接到你自己的CAN驱动和上层
 * （比如未来的UDS）之间。示例里CAN驱动部分是伪代码/占位实现，具体替换成你
 * MCU平台真实的CAN外设API即可，整体对接的结构和调用顺序不需要改变。
 */

#include "MicroIsoTp.h"
#include <stdio.h>

/* ------------------------------------------------------------------------ */
/* 1. CAN ID 配置：这是你项目里唯一需要区分"物理/功能"的地方                    */
/*    MicroIsoTp本身不关心这几个数字，它只关心你调用了哪个入口函数              */
/* ------------------------------------------------------------------------ */

#define CAN_ID_PHYSICAL_REQUEST   (0x7E0U) /* 上位机 -> 本ECU，物理请求 */
#define CAN_ID_PHYSICAL_RESPONSE  (0x7E8U) /* 本ECU -> 上位机，物理响应（发送永远用这个ID） */
#define CAN_ID_FUNCTIONAL_REQUEST (0x7DFU) /* 上位机 -> 所有ECU，功能广播 */

/* ------------------------------------------------------------------------ */
/* 2. 你自己平台的CAN驱动接口占位（替换成真实的HAL/寄存器操作）                 */
/* ------------------------------------------------------------------------ */

/**
 * @brief 占位：真正把一帧数据发送到CAN总线上
 * @note 换成你MCU平台真实的CAN发送函数，比如HAL_CAN_AddTxMessage之类
 */
static int CanDriver_HwSend(uint32_t can_id, const uint8_t *data, uint8_t dlc)
{
    printf("[CAN TX] id=0x%03X dlc=%u data=", can_id, dlc);
    for (uint8_t i = 0; i < dlc; i++)
    {
        printf("%02X ", data[i]);
    }
    printf("\n");

    /* 假设发送总是立刻成功，真实平台上这里只是把帧塞进发送邮箱，
       真正的"发送完成"要等CAN Tx中断触发后再调用 MicroIsoTp_HwTransmitDone() */
    return 0;
}

/* ------------------------------------------------------------------------ */
/* 3. 实现 MicroIsoTp 需要的弱函数（发送方向）                                 */
/* ------------------------------------------------------------------------ */

/**
 * @brief 重写弱函数：MicroIsoTp要发一帧数据时，最终会调用到这里
 */
int MicroIsoTp_HwTransmit(const uint8_t *data, uint8_t dlc)
{
    /* 响应永远走物理响应ID发送，不管这次数据是Rx模块发的流控帧，
       还是Tx模块发的SF/FF/CF，本ECU作为纯粹的接收/响应方，
       发送目标从始至终只有这一个固定ID */
    int ret = CanDriver_HwSend(CAN_ID_PHYSICAL_RESPONSE, data, dlc);

    if (ret == 0)
    {
        /* 示例环境没有真实的CAN Tx中断，这里直接模拟"已经发送完成"，
           立刻回调通知MicroIsoTp。真实MCU上，这一行应该挪到
           CAN发送完成中断服务函数里去调用，不能在这里同步调用。 */
        MicroIsoTp_HwTransmitDone();
    }

    return ret;
}

/* ------------------------------------------------------------------------ */
/* 4. 实现 MicroIsoTp 需要的弱函数（上层通知方向）                             */
/* ------------------------------------------------------------------------ */

/**
 * @brief 重写弱函数：一整包数据组包完成后，MicroIsoTp会调用这里
 * @note 未来接UDS的话，这里就是把data/len转交给UDS_ProcessRequest()的地方，
 *       本示例里只是简单打印出来
 */
void MicroIsoTp_Rx_Indication(MicroIsoTp_AddrType_t type, uint8_t *data, size_t len)
{
    printf("[ISO-TP RX] addr=%s len=%zu data=",
           (type == MICROISOTP_ADDR_PHY) ? "physical" : "functional", len);

    for (size_t i = 0; i < len; i++)
    {
        printf("%02X ", data[i]);
    }
    printf("\n");

    /* 示例：收到什么就原样回一遍，模拟"响应"这个动作 */
    MicroIsoTp_Status_t status = MicroIsoTp_TxStart(data, len);
    if (status != MICROISOTP_OK)
    {
        printf("[ISO-TP TX] start failed, status=%d\n", status);
    }
}

/**
 * @brief 重写弱函数：MicroIsoTp_TxStart()发起的这次发送完成后，会调用这里
 */
void MicroIsoTp_Tx_Confirmation(void)
{
    printf("[ISO-TP TX] message sent complete\n");
}

/* ------------------------------------------------------------------------ */
/* 5. CAN驱动收到帧后的入口：根据CAN ID分流到物理/功能入口                     */
/*    这一步是MicroIsoTp明确要求"外部完成"的分流判断，模块内部不做             */
/* ------------------------------------------------------------------------ */

/**
 * @brief 占位：CAN驱动接收中断里应该调用这个函数
 */
static void CanDriver_OnRxFrame(uint32_t can_id, const uint8_t *data, uint8_t dlc)
{
    if (can_id == CAN_ID_PHYSICAL_REQUEST)
    {
        MicroIsoTp_Physical_RxIndication(data, dlc);
    }
    else if (can_id == CAN_ID_FUNCTIONAL_REQUEST)
    {
        MicroIsoTp_Functional_RxIndication(data, dlc);
    }
    else
    {
        /* 不相关的CAN ID，忽略 */
    }
}

/* ------------------------------------------------------------------------ */
/* 6. 主流程：初始化 + 周期调用 + 模拟总线上收到的帧                           */
/* ------------------------------------------------------------------------ */

int main(void)
{
    MicroIsoTp_Init();

    /* 模拟收到一个单帧请求：03 11 22 33（数据 11 22 33，3字节） */
    uint8_t sf_frame[8] = {0x03, 0x11, 0x22, 0x33, 0x00, 0x00, 0x00, 0x00};
    CanDriver_OnRxFrame(CAN_ID_PHYSICAL_REQUEST, sf_frame, 8);

    /* 真实MCU上，下面这两个函数分别挂在1ms硬件定时器中断和主循环里，
       这里只是用一个简单的计数循环模拟"跑够1000个tick"这个过程，
       方便观察多帧场景下超时判断和状态机推进是否正常 */
    for (int i = 0; i < 1000; i++)
    {
        MicroIsoTp_TickHandler();  /* 挂在固定频率的定时器中断里 */
        MicroIsoTp_TimerHandler(); /* 挂在主循环/低优先级任务里 */
    }

    /* 模拟收到一个首帧请求：总长度20字节，首帧自带前6字节 01~06 */
    uint8_t ff_frame[8] = {0x10, 0x14, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
    CanDriver_OnRxFrame(CAN_ID_PHYSICAL_REQUEST, ff_frame, 8);

    for (int i = 0; i < 10; i++)
    {
        MicroIsoTp_TickHandler();
        MicroIsoTp_TimerHandler(); /* 这一轮会驱动Rx状态机发出流控帧(FC) */
    }

    /* 模拟对方收到流控后，按SN顺序发来的两个连续帧 */
    uint8_t cf1[8] = {0x21, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D};
    uint8_t cf2[8] = {0x22, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13, 0x14};
    CanDriver_OnRxFrame(CAN_ID_PHYSICAL_REQUEST, cf1, 8);
    CanDriver_OnRxFrame(CAN_ID_PHYSICAL_REQUEST, cf2, 8);

    for (int i = 0; i < 10; i++)
    {
        MicroIsoTp_TickHandler();
        MicroIsoTp_TimerHandler(); /* 这一轮会触发组包完成，回调MicroIsoTp_Rx_Indication */
    }

    return 0;
}