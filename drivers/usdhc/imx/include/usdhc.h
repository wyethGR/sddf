#pragma once

#include <stdint.h>

/* The driver is based on the i.MX 8M Mini Applications Processor Reference Manual Rev 3, 11/2020.
   The following register descriptions and layout are from section 10.3.7.1.1.

    uSDHC1 base address: 30B4_0000h
    uSDHC2 base address: 30B5_0000h
    uSDHC3 base address: 30B6_0000h
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
