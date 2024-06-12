#include "usdhc.h"

#include <microkit.h>
#include <sddf/util/printf.h>

imx_usdhc_regs_t *usdhc_regs;
volatile uint32_t *iomuxc_regs;

#define USDHC_INT_CHANNEL 1

void usdhc_debug(void) {
    sddf_printf("uSDHC: PRES_STATE: %u, PROT_CTRL: %u, SYS_CTLR: %u, INT_STATUS: %u, INT_STATUS_EN: %u, INT_SIGNAL_EN: %u, VEND_SPEC: %u\n", usdhc_regs->pres_state, usdhc_regs->prot_ctrl, usdhc_regs->sys_ctrl, usdhc_regs->int_status, usdhc_regs->int_status_en, usdhc_regs->int_signal_en, usdhc_regs->vend_spec);
    sddf_printf("uSDHC: CMD_RSP0: %u, CMD_RSP1: %u, CMD_RSP2: %u, CMD_RSP3: %u\n", usdhc_regs->cmd_rsp0, usdhc_regs->cmd_rsp1, usdhc_regs->cmd_rsp2, usdhc_regs->cmd_rsp3);
}

void usdhc_notified(void)
{
    microkit_dbg_puts("usdhc IRQ!\n");
    usdhc_debug();
}

void notified(microkit_channel ch)
{
    switch (ch)
    {
    case USDHC_INT_CHANNEL:
        usdhc_notified();
        break;

    default:
        sddf_printf("notification on unknown channel: %d\n", ch);
        break;
    }

    microkit_irq_ack(ch);
}

void usdhc_mask_interrupts() {
    usdhc_regs->int_signal_en = 0;
}

void usdhc_unmask_interrupts() {
    usdhc_regs->int_signal_en = 0xfffffff;
}

/* Ref: 10.3.4.1 Command send & response receive basic operation.

    cmd_index: These bits are set to the command number that is specified in
               bits 45-40 of the command-format in the SD Memory Card Physical
               Layer Specification and SDIO Card Specification.
 */
void usdhc_send_command_poll(uint8_t cmd_index)
{
    // The host driver checks the Command Inhibit DAT field (PRES_STATE[CDIHB]) and
    // the \Command Inhibit CMD field (PRES_STATE[CIHB]) in the Present State register
    // before writing to this register.
    if (usdhc_regs->pres_state & (USDHC_PRES_STATE_CIHB | USDHC_PRES_STATE_CDIHB)) {
        sddf_printf("no work :(\n");
        while (usdhc_regs->pres_state & (USDHC_PRES_STATE_CIHB | USDHC_PRES_STATE_CDIHB));
    }


    uint32_t cmd_arg = 0; // TODO:
    /* "set CMDTYP, DPSEL, CICEN, CCCEN, RSTTYP, DTDSEL accorind to the command index" */
    uint32_t cmd_xfr_typ = 0;     // CMD_XFR_TYP see 10.3.7.1.5 of the spec for details of fields
    cmd_xfr_typ |= (cmd_index & 0b111111) << 24; /* 29-24 CMDINX */
    cmd_xfr_typ |= (0b00 << 22); /* CMDTYP (23-22, normal) */
    cmd_xfr_typ &= BIT(21); /* set to 0b for no data. FOR GO_IDLE_STATE; TODO!!!! */
    cmd_xfr_typ &= BIT(20); /* don't check CICEN -> COMMAND DEPENDANT, see table 10-42 */
    cmd_xfr_typ &= BIT(19); /* don't check CCCEN (crc) */
    cmd_xfr_typ |= (0b00 << 16); /* response type: no response */

    // if (iinternal DMA)
    // if (multi-block transfer)

    usdhc_mask_interrupts();
    usdhc_regs->cmd_arg = cmd_arg;
    usdhc_regs->cmd_xfr_typ = cmd_xfr_typ;
    usdhc_unmask_interrupts();

    // wait for command completion (polling; TODO: interrupt!; also timeout?)
    while (!(usdhc_regs->int_status & USDHC_INT_STATUS_CC));

    uint32_t status = usdhc_regs->int_status;
    if (status & USDHC_INT_STATUS_CTOE) {
        /* command timeout error */
        sddf_printf("command timeout error\n");
    } else if (status & USDHC_INT_STATUS_CCE) {
        /* command CRC error */
        sddf_printf("command crc error\n");
    } else if (status & USDHC_INT_STATUS_CIE) {
        /* command index error */
        sddf_printf("command index error\n");
    } else if (status & USDHC_INT_STATUS_CEBE) {
        /* command end bit error */
        sddf_printf("command end bit error\n");
    } else {
        sddf_printf("success!\n");
    }

    /* clear CC bit and all command error bits... */
    usdhc_regs->int_status |= USDHC_INT_STATUS_CC;

    // TODO: for getting a response...
    // uint32_t rsp0 = usdhc_regs->cmd_rsp0;
}

/* false -> clock is changing frequency and not stable (poll until is).
   true  -> clock is stable.
   Ref: 10.3.7.1.11.4 */
bool is_usdhc_clock_stable() {
    return usdhc_regs->pres_state & USDHC_PRES_STATE_SDSTB;
}

void usdhc_setup_clock() {
    /* Ref: 10.3.6.7:
       - Clear the FRC_SDCLK_ON when changing SDCLKFS or setting RSTA bit
       - Also, make sure that the SDSTB field is high.
    */
    usdhc_regs->pres_state &= ~USDHC_VEND_SPEC_FRC_SDCLK_ON;
    while (!is_usdhc_clock_stable()); // TODO: ... timeout.

    /*
     TODO: Clock driver! No exist
     ... let's just assume what i found elsewhere is right...
     ... https://elixir.bootlin.com/linux/v6.9.3/source/arch/arm64/boot/dts/freescale/imx8mq.dtsi#L1499
     ... 0x400000000 => 400 MHz base clock...

     So (0x400_000_000 / (256 * 16)) == 0x400_000; divisor = 16, prescalar = 256
    */

    /* TODO: We assume single data rate mode (DDR_EN of MIX_CTLR = 0) */
    // TODO: do this in a better, generic way.
    uint32_t sys_ctrl = usdhc_regs->sys_ctrl;
    sys_ctrl &= ~(0b111111111111 << 4); // clear bits 4 to 15.
    sys_ctrl |= BIT(15); // this is 0x80h into SDCLKFS;
    sys_ctrl |= BIT(4) | BIT(5) | BIT(6) | BIT(7); // divisor = 16.
    usdhc_regs->sys_ctrl;

    while (!is_usdhc_clock_stable()); // TODO: ... timeout
}

/* Ref: See 10.3.4.2.2 "Reset" */
void usdhc_reset(void)
{
    // Perform software reset of all components
    usdhc_regs->sys_ctrl |= USDHC_SYS_CTRL_RSTA;

    usdhc_setup_clock(/* 400 kHz */);

    usdhc_regs->int_status_en |= USDHC_INT_STATUS_TCSEN | USDHC_INT_STATUS_DINTSEN
                              | USDHC_INT_STATUS_BRRSEN | USDHC_INT_STATUS_CINTSEN
                              | USDHC_INT_STATUS_CTOESEN | USDHC_INT_STATUS_CCESEN
                              | USDHC_INT_STATUS_CEBESEN | USDHC_INT_STATUS_CIESEN
                              | USDHC_INT_STATUS_DTOESEN | USDHC_INT_STATUS_DCSESEN
                              | USDHC_INT_STATUS_DEBESEN;

    while (usdhc_regs->pres_state & (USDHC_PRES_STATE_CIHB | USDHC_PRES_STATE_CDIHB));

    /* 80 clock ticks for power up, self-clearing when done */
    usdhc_regs->sys_ctrl |= USDHC_SYS_CTRL_INITA;
    while (!(usdhc_regs->sys_ctrl & USDHC_SYS_CTRL_INITA));

    usdhc_send_command_poll(USDHC_CMD_GO_IDLE_STATE);
}

void usdhc_read_support_voltages() {
    uint32_t host_cap = usdhc_regs->host_ctrl_cap;
    sddf_printf("host caps: %u\n", host_cap);
}

#define IOMUX_ALT0 0b000
#define IOMUX_ALT1 0b001
#define IOMUX_ALT5 0b101

// value after reset, sane defaults
#define IOMUX_PAD_CTL_PULLUP   BIT(6) /* defaults to pulldown. */

/* Ref: Section 8.
Also see 10.3.2 for settings? TODO: i have no idea if the pad ctl setitngs are correct
*/
void usdhc_setup_iomuxc() {
    /*
        Page 1316-7 ! -- pad mux registers
    */
    /* 8.2.5.12 */
    *(iomuxc_regs + 0x40) = IOMUX_ALT5; /* USDHC1_CD_B : IOMUXC_SW_MUX_CTL_PAD_GPIO1_IO06 */

    /* 8.2.5.36 onwards; PAD mux registers... */
    *(iomuxc_regs + 0xA0) = IOMUX_ALT0; /* USDHC1_CLK     : IOMUXC_SW_MUX_CTL_PAD_SD1_CLK */
    *(iomuxc_regs + 0xA4) = IOMUX_ALT0; /* USDHC1_CMD     : IOMUXC_SW_MUX_CTL_PAD_SD1_CMD */
    *(iomuxc_regs + 0xA8) = IOMUX_ALT0; /* USDHC1_DATA0   : IOMUXC_SW_MUX_CTL_PAD_SD1_DATA0 */
    *(iomuxc_regs + 0xAC) = IOMUX_ALT0; /* USDHC1_DATA1   : IOMUXC_SW_MUX_CTL_PAD_SD1_DATA1 */
    *(iomuxc_regs + 0xB0) = IOMUX_ALT0; /* USDHC1_DATA2   : IOMUXC_SW_MUX_CTL_PAD_SD1_DATA2 */
    *(iomuxc_regs + 0xB4) = IOMUX_ALT0; /* USDHC1_DATA3   : IOMUXC_SW_MUX_CTL_PAD_SD1_DATA3 */
    *(iomuxc_regs + 0xB8) = IOMUX_ALT0; /* USDHC1_DATA4   : IOMUXC_SW_MUX_CTL_PAD_SD1_DATA4 */
    *(iomuxc_regs + 0xBC) = IOMUX_ALT0; /* USDHC1_DATA5   : IOMUXC_SW_MUX_CTL_PAD_SD1_DATA5 */
    *(iomuxc_regs + 0xC0) = IOMUX_ALT0; /* USDHC1_DATA6   : IOMUXC_SW_MUX_CTL_PAD_SD1_DATA6 */
    *(iomuxc_regs + 0xC4) = IOMUX_ALT0; /* USDHC1_DATA7   : IOMUXC_SW_MUX_CTL_PAD_SD1_DATA7 */
    *(iomuxc_regs + 0xC8) = IOMUX_ALT0; /* USDHC1_RESET_B : IOMUXC_SW_MUX_CTL_PAD_SD1_RESET_B */
    *(iomuxc_regs + 0xCC) = IOMUX_ALT0; /* USDHC1_STROBE  : IOMUXC_SW_MUX_CTL_PAD_SD1_STROBE */

    /* 8.2.5.9 */
    *(iomuxc_regs + 0x34) = IOMUX_ALT1; /* USDHC1_VSELECT : IOMUXC_SW_MUX_CTL_PAD_GPIO1_IO03 */
    *(iomuxc_regs + 0x44) = IOMUX_ALT5; /* USDHC1_WP      : IOMUXC_SW_MUX_CTL_PAD_GPIO1_IO07 */

    /*
        Pad control registers; pg 1429, 8.2.5.190 onwrads
    */
   // tie low => card attached, otherwise, high = can card detect...
    *(iomuxc_regs + 0x2A8) |= IOMUX_PAD_CTL_PULLUP; /* USDHC1_CD_B : IOMUXC_SW_PAD_CTL_PAD_GPIO1_IO06 */

    *(iomuxc_regs + 0x308) |= 0; /* USDHC1_CLK     : IOMUXC_SW_PAD_CTL_PAD_SD1_CLK */
    *(iomuxc_regs + 0x30C) |= IOMUX_PAD_CTL_PULLUP; /* USDHC1_CMD     : IOMUXC_SW_PAD_CTL_PAD_SD1_CMD */
    *(iomuxc_regs + 0x310) |= IOMUX_PAD_CTL_PULLUP; /* USDHC1_DATA0   : IOMUXC_SW_PAD_CTL_PAD_SD1_DATA0 */
    *(iomuxc_regs + 0x314) |= IOMUX_PAD_CTL_PULLUP; /* USDHC1_DATA1   : IOMUXC_SW_PAD_CTL_PAD_SD1_DATA1 */
    *(iomuxc_regs + 0x318) |= IOMUX_PAD_CTL_PULLUP; /* USDHC1_DATA2   : IOMUXC_SW_PAD_CTL_PAD_SD1_DATA2 */
    *(iomuxc_regs + 0x31C) |= IOMUX_PAD_CTL_PULLUP; /* USDHC1_DATA3   : IOMUXC_SW_PAD_CTL_PAD_SD1_DATA3 */
    *(iomuxc_regs + 0x320) |= IOMUX_PAD_CTL_PULLUP; /* USDHC1_DATA4   : IOMUXC_SW_PAD_CTL_PAD_SD1_DATA4 */
    *(iomuxc_regs + 0x324) |= IOMUX_PAD_CTL_PULLUP; /* USDHC1_DATA5   : IOMUXC_SW_PAD_CTL_PAD_SD1_DATA5 */
    *(iomuxc_regs + 0x328) |= IOMUX_PAD_CTL_PULLUP; /* USDHC1_DATA6   : IOMUXC_SW_PAD_CTL_PAD_SD1_DATA6 */
    *(iomuxc_regs + 0x32C) |= IOMUX_PAD_CTL_PULLUP; /* USDHC1_DATA7   : IOMUXC_SW_PAD_CTL_PAD_SD1_DATA7 */
    *(iomuxc_regs + 0x330) |= IOMUX_PAD_CTL_PULLUP; /* USDHC1_RESET_B : IOMUXC_SW_PAD_CTL_PAD_SD1_RESET_B */ /* active low*/
    *(iomuxc_regs + 0x334) |= 0; /* USDHC1_STROBE  : IOMUXC_SW_PAD_CTL_PAD_SD1_STROBE */

    *(iomuxc_regs + 0x29C) |= 0; /* USDHC1_VSELECT : IOMUXC_SW_PAD_CTL_PAD_GPIO1_IO03 */
    // tied high for card write protect detect
    *(iomuxc_regs + 0x2AC) |= IOMUX_PAD_CTL_PULLUP; /* USDHC1_WP : IOMUXC_SW_PAD_CTL_PAD_GPIO1_IO07 */
}

void init()
{
    microkit_dbg_puts("hello from usdhc driver\n");
    // usdhc_read_support_voltages();

    // this doesn't do anything...???? (maybe it does but it looks the same so)
    usdhc_setup_iomuxc();
    usdhc_reset();

    if (usdhc_regs->pres_state & USDHC_PRES_STATE_CINST) {
        sddf_printf("card inserted\n");
    } else {
        sddf_printf("card not inserted or power on reset\n");
    }

    usdhc_debug();

    // TODO: 10.3.4.2.1 Card detect
    // TODO: I think I need to set up GPIO pinmux etc?
}
