#ifndef MICROISOTP_TYPES_H
#define MICROISOTP_TYPES_H

#include "MicroIsoTp_conf.h"
#include "MicroIsoTp_utils.h"
#include "stdbool.h"
#include "stdint.h"
#include "string.h"
#include "stdlib.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef enum
{
    MICROISOTP_OK,
    MICROISOTP_ERR,
    MICROISOTP_BUSY,
    MICROISOTP_OVERFLOW,
    MICROISOTP_PARAM_INVALID, // 参数无效 Invalid parameter
    MICROISOTP_HWTX_ERR,      // 硬件發送錯誤
} MicroIsoTp_Status_t;        // 状态码

typedef enum
{
    MICROISOTP_ADDR_PHY, // 物理地址
    MICROISOTP_ADDR_FUC, // 功能地址
} MicroIsoTp_AddrType_t;

typedef enum
{
    MICROISOTP_PCI_SF, // 单帧
    MICROISOTP_PCI_FF, // 首帧
    MICROISOTP_PCI_CF, // 连续帧
    MICROISOTP_PCI_FC, // 流控帧
} MicroIsoTp_Pci_t;

typedef enum
{
    MICROISOTP_FS_CTS,    // 继续发送
    MICROISOTP_FS_WAIT,   // 等待下一次流控
    MICROISOTP_FS_OVERFL, // 错误分支， 数据超过缓冲区
} MicroIsoTp_FS_t;

typedef enum
{
    MICROISOTP_RX_STEP_IDLE,    // 空闲
    MICROISOTP_RX_STEP_SF,      // 接收单帧
    MICROISOTP_RX_STEP_FF,      // 接收到首帧
    MICROISOTP_RX_STEP_FC,      // 发一个流控
    MICROISOTP_RX_STEP_FC_COMP, // 流控发完了
    MICROISOTP_RX_CF_COMP,      // 连续帧收完了
} MicroIsoTp_Rx_Step_t;

typedef enum
{
    MICROISOTP_TX_STEP_IDLE, // 空闲
    MICROISOTP_TX_STEP_SF, // 发个单帧
    MICROISOTP_TX_STEP_SF_DONE,
    MICROISOTP_TX_STEP_FF, // 发送首帧
    MICROISOTP_TX_STEP_FC, // 等待流控
    MICROISOTP_TX_STEP_CF, // 发送连续帧
    MICROISOTP_TX_STEP_CF_DONE, // 等连续帧发送完成
    MICROISOTP_TX_STEP_COMPLETE, // 发送完成
} MicroIsoTp_Tx_Step_t;

typedef struct
{
    uint32_t timeout;
    volatile uint32_t tick;

    volatile bool en;
} MicroIsoTp_N_Timer_t;

typedef struct
{
    MicroIsoTp_FS_t Fs;
    uint8_t Bs;
    uint8_t Stmin; // 最小时间
    volatile bool External_en;

    uint8_t data[8];
} MicroIsoTp_FlowControl_t;

typedef struct 
{
    uint8_t sn;

    uint8_t data[8];
}MicroIsoTp_CF_t;

typedef struct
{
    volatile MicroIsoTp_Rx_Step_t step;

    volatile uint16_t rx_len;
    uint8_t *buf;

    MicroIsoTp_AddrType_t addr_type; // 地址类型

    volatile uint16_t next_sn;
    volatile uint16_t len_count;

    volatile bool Exit;

    volatile bool reset; // 復位
    MicroIsoTp_FlowControl_t FC;

    MicroIsoTp_N_Timer_t N_Cr; // n_cr定时器
    MicroIsoTp_N_Timer_t N_Ar; // n_ar定时器
    MicroIsoTp_N_Timer_t N_Br; // n_br定时器

    volatile uint16_t bs_count;

} MicroIsoTp_Rx_Obj_t;

typedef struct
{
    MicroIsoTp_Tx_Step_t step;
    uint8_t *buf; 
    size_t tx_len;

    uint32_t buf_offset;
    uint32_t buf_remain; // 剩余

    volatile bool reset; // 重置 

    uint32_t bs_count; // bs计数

    MicroIsoTp_FlowControl_t FC; // 流控

    MicroIsoTp_CF_t CF; // 连续帧

    uint8_t SF[8];
    uint8_t FF[8];

    MicroIsoTp_N_Timer_t N_As; // 发送方把一帧（SF/FF/CF）实际发送到总线上所需时间
    MicroIsoTp_N_Timer_t N_Bs; // 发完FF（或一个Block的CF后）等待对方FC的最长时间
    MicroIsoTp_N_Timer_t N_Cs; // 发送方两个CF之间的实际发送间隔（应 ≥ 对方要求的STmin

} MicroIsoTp_Tx_Obj_t;

typedef struct
{
    MicroIsoTp_Rx_Obj_t rx_obj;
    MicroIsoTp_Tx_Obj_t tx_obj;

    uint8_t buf[MICROISOTP_BUFFER_SIZE];
} MicroIsoTp_Obj_t;

#ifdef __cplusplus
}
#endif

#endif
