#pragma once

#include <stdint.h>

#include <sddf/util/util.h>

/* The driver is based on the i.MX8 Quad Applications Processor Reference Manual Rev 3.1, 06/2021.
   The following register descriptions and layout are from section 10.3.7.1.1.

    uSDHC1 base address: 30B4_0000h
    uSDHC2 base address: 30B5_0000h
*/

struct imx_usdhc_regs {
    uint32_t ds_addr; /* 0h DMA System Address (DS_ADDR) RW */
    uint32_t blk_att; /* 4h  Block Attributes (BLK_ATT) RW */
    uint32_t cmd_arg; /* 8h   Command Argument (CMD_ARG) RW */
    uint32_t cmd_xfr_typ; /* Ch   Command Transfer Type (CMD_XFR_TYP) RW */
    uint32_t cmd_rsp0; /* 10h Command Response0 (CMD_RSP0) RO */
    uint32_t cmd_rsp1; /* 14h Command Response1 (CMD_RSP1) RO */
    uint32_t cmd_rsp2; /* 18h Command Response2 (CMD_RSP2) RO */
    uint32_t cmd_rsp3; /* 1Ch Command Response3 (CMD_RSP3) RO */
    uint32_t data_buff_acc_port; /* 20h Data Buffer Access Port (DATA_BUFF_ACC_PORT) RW */
    uint32_t pres_state; /* 24h Present State (PRES_STATE) ROT */
    uint32_t prot_ctrl; /* 28h Protocol Control (PROT_CTRL) RW */
    uint32_t sys_ctrl; /* 2Ch System Control (SYS_CTRL) RW */
    uint32_t int_status; /* 30h Interrupt Status (INT_STATUS) W1C */
    uint32_t int_status_en; /* 34h Interrupt Status Enable (INT_STATUS_EN) RW */
    uint32_t int_signal_en; /* 38h Interrupt Signal Enable (INT_SIGNAL_EN) RW */
    uint32_t autocmd12_err_status; /* 3Ch Auto CMD12 Error Status (AUTOCMD12_ERR_STATUS) RW */
    uint32_t host_ctrl_cap; /* 40h Host Controller Capabilities (HOST_CTRL_CAP) RW */
    uint32_t wtmk_lvl; /* 44h Watermark Level (WTMK_LVL) RW */
    uint32_t mix_ctrl;/* 48h Mixer Control (MIX_CTRL) RW */
    uint32_t force_event; /* 50h Force Event (FORCE_EVENT) WORZ */
    uint32_t adma_err_status; /* 54h ADMA Error Status (ADMA_ERR_STATUS) RO */
    uint32_t adma_sys_addr; /* 58h ADMA System Address (ADMA_SYS_ADDR) RW */
    uint32_t dll_ctrl; /* 60h DLL (Delay Line) Control (DLL_CTRL) RW */
    uint32_t dll_status; /* 64h DLL Status (DLL_STATUS) RO */
    uint32_t clk_tune_ctrl_status; /* 68h CLK Tuning Control and Status (CLK_TUNE_CTRL_STATUS) RW */
    uint32_t strobe_dll_ctrl; /* 70h Strobe DLL control (STROBE_DLL_CTRL) RW */
    uint32_t strobe_dll_status; /* 74h Strobe DLL status (STROBE_DLL_STATUS) RO */
    uint32_t vend_spec; /* C0h Vendor Specific Register (VEND_SPEC) RW */
    uint32_t mmc_boot; /* C4h MMC Boot (MMC_BOOT) RW */
    uint32_t vend_spec2; /* C8h Vendor Specific 2 Register (VEND_SPEC2) RW */
    uint32_t tuning_ctrl; /* CCh Tuning Control (TUNING_CTRL) RW */
    uint32_t cqe; /* 100h Command Queue (CQE) ROZ */
};
typedef volatile struct imx_usdhc_regs imx_usdhc_regs_t;

/* SYS_CTRL Bits. See 10.3.7.1.13 */
#define USDHC_SYS_CTRL_RSTA  BIT(24)  /* Software reset for all */
#define USDHC_SYS_CTRL_RSTC  BIT(25)  /* Software reset for CMD line */
#define USDHC_SYS_CTRL_RSTD  BIT(26)  /* Software reset for data line */
#define USDHC_SYS_CTRL_INITA BIT(27)  /* Initialization active */
#define USDHC_SYS_CTRL_RSTT  BIT(28)  /* Reset tuning */

/* INT_STATUS_EN Bits. See 10.3.7.1.15 */
#define USDHC_INT_STATUS_CCSEN    BIT(0)   /* Command complete status enable */
#define USDHC_INT_STATUS_TCSEN    BIT(1)   /* Transfer complete status enable */
#define USDHC_INT_STATUS_BGESEN   BIT(2)   /* Block gap event status enable */
#define USDHC_INT_STATUS_DINTSEN  BIT(3)   /* DMA interrupt status enable */
#define USDHC_INT_STATUS_BWRSEN   BIT(4)   /* Buffer write ready status enable */
#define USDHC_INT_STATUS_BRRSEN   BIT(5)   /* Buffer read ready status enable */
#define USDHC_INT_STATUS_CINSSEN  BIT(6)   /* Card insertion status enable */
#define USDHC_INT_STATUS_CRMSEN   BIT(7)   /* Card removal status enable */
#define USDHC_INT_STATUS_CINTSEN  BIT(8)   /* Card interrupt status enable */
#define USDHC_INT_STATUS_RTESEN   BIT(12)  /* Re-tuning event status enable */
#define USDHC_INT_STATUS_TPSEN    BIT(13)  /* Tuning pass status enable */
#define USDHC_INT_STATUS_CQISEN   BIT(14)  /* Command queuing status enable */
#define USDHC_INT_STATUS_CTOESEN  BIT(16)  /* Command timeout error status enable */
#define USDHC_INT_STATUS_CCESEN   BIT(17)  /* Command CRC error status enable */
#define USDHC_INT_STATUS_CEBESEN  BIT(18)  /* Command end bit error status enable */
#define USDHC_INT_STATUS_CIESEN   BIT(19)  /* Command indx error status enable */
#define USDHC_INT_STATUS_DTOESEN  BIT(20)  /* Data timeout error status enable*/
#define USDHC_INT_STATUS_DCSESEN  BIT(21)  /* Data CRC error status enable */
#define USDHC_INT_STATUS_DEBESEN  BIT(22)  /* Data end bit error status enable */
#define USDHC_INT_STATUS_AC12ESEN BIT(24)  /* Auto CMD12 error status enable */
#define USDHC_INT_STATUS_TNESEN   BIT(26)  /* Tuning error status enable */
#define USDHC_INT_STATUS_DMAESEN  BIT(28)  /* DMA error status enable */

/* INT_SIGNAL_EN Bits. See 10.3.7.1.16 */
#define USDHC_INT_SIGNAL_CCIEN    BIT(0)   /* Command complete interrupt enable */
#define USDHC_INT_SIGNAL_TCIEN    BIT(1)   /* Transfer complete interrupt enable */
#define USDHC_INT_SIGNAL_BGEIEN   BIT(2)   /* Block gap event interrupt enable */
#define USDHC_INT_SIGNAL_DINTIEN  BIT(3)   /* DMA interrupt interrupt enable */
#define USDHC_INT_SIGNAL_BWRIEN   BIT(4)   /* Buffer write ready interrupt enable */
#define USDHC_INT_SIGNAL_BRRIEN   BIT(5)   /* Buffer read ready interrupt enable */
#define USDHC_INT_SIGNAL_CINSIEN  BIT(6)   /* Card insertion interrupt enable */
#define USDHC_INT_SIGNAL_CRMIEN   BIT(7)   /* Card removal interrupt enable */
#define USDHC_INT_SIGNAL_CINTIEN  BIT(8)   /* Card interrupt enable */
#define USDHC_INT_SIGNAL_RTEIEN   BIT(12)  /* Re-tuning event interrupt enable */
#define USDHC_INT_SIGNAL_TPIEN    BIT(13)  /* Tuning pass interrupt enable */
#define USDHC_INT_SIGNAL_CQIIEN   BIT(14)  /* Command queuing interrupt enable */
#define USDHC_INT_SIGNAL_CTOEIEN  BIT(16)  /* Command timeout error interrupt enable */
#define USDHC_INT_SIGNAL_CCEIEN   BIT(17)  /* Command CRC error interrupt enable */
#define USDHC_INT_SIGNAL_CEBEIEN  BIT(18)  /* Command end bit error interrupt enable */
#define USDHC_INT_SIGNAL_CIEIEN   BIT(19)  /* Command indx error interrupt enable */
#define USDHC_INT_SIGNAL_DTOEIEN  BIT(20)  /* Data timeout error interrupt enable*/
#define USDHC_INT_SIGNAL_DCSEIEN  BIT(21)  /* Data CRC error interrupt enable */
#define USDHC_INT_SIGNAL_DEBEIEN  BIT(22)  /* Data end bit error interrupt enable */
#define USDHC_INT_SIGNAL_AC12EIEN BIT(24)  /* Auto CMD12 error interrupt enable */
#define USDHC_INT_SIGNAL_TNEIEN   BIT(26)  /* Tuning error interrupt enable */
#define USDHC_INT_SIGNAL_DMAEIEN  BIT(28)  /* DMA error interrupt enable */
