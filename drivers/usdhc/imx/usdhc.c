#include "include/usdhc.h"

#include <microkit.h>
#include <sddf/util/printf.h>

imx_usdhc_regs_t *usdhc_regs;

/*
    TODO: Details
        10.3.3.6.3 card interrupt handling.
*/
void usdhc_notified(void) {
    microkit_dbg_puts("usdhc IRQ!\n");

    usdhc_regs->int_status_en |=
      USDHC_INT_STATUS_CCSEN | USDHC_INT_STATUS_TCSEN |
      USDHC_INT_STATUS_CIESEN | USDHC_INT_STATUS_CRMSEN |
      USDHC_INT_STATUS_CTOESEN | USDHC_INT_STATUS_DTOESEN |
      USDHC_INT_STATUS_CIESEN | USDHC_INT_STATUS_DCSESEN |
      USDHC_INT_STATUS_CCESEN | USDHC_INT_STATUS_CINSSEN;

    usdhc_regs->int_signal_en |=
      USDHC_INT_SIGNAL_CCIEN | USDHC_INT_SIGNAL_TCIEN |
      USDHC_INT_SIGNAL_CIEIEN | USDHC_INT_SIGNAL_CRMIEN |
      USDHC_INT_SIGNAL_CTOEIEN | USDHC_INT_SIGNAL_DTOEIEN |
      USDHC_INT_SIGNAL_CIEIEN | USDHC_INT_SIGNAL_DCSEIEN |
      USDHC_INT_SIGNAL_CCEIEN | USDHC_INT_SIGNAL_CINSIEN;
}

void notified(microkit_channel ch) {
    switch (ch) {
        case 1:
            usdhc_notified();
            break;

        default:
            sddf_printf("notification on unknown channel: %d\n", ch);
            break;
    }

    microkit_irq_ack(ch);
}

void usdhc_send_command(uint32_t cmd_arg /* , uint32_t cmd_index, args */) {
    /* Ref: 10.3.4.1 */
    uint32_t cmd_xfr_typ = 0;
    // cmd_xfr_typ = (cmd_index & 0x3f) >> 24;
    /* set CMDTYP, DPSEL, CICEN, CCCEN, RSTTYP, DTDSEL accorind to the command index; */
    // if (iinternal DMA)
    // if (multi-block transfer)

    usdhc_regs->cmd_arg = cmd_arg;
    usdhc_regs->cmd_xfr_typ = cmd_xfr_typ;

    while (!(usdhc_regs->int_status & /* command complete bit */ BIT(0)));

    /* check errors & clear CC / errors ??*/

    usdhc_regs->int_status |= BIT(0); /* command complete field */
}


void usdhc_software_reset(void) {
    /* Ref: See 10.3.4.2.2. */
    usdhc_regs->sys_ctrl |= USDHC_SYS_CTRL_RSTA;
    /* opt: set DTOCV and SDCLKFS and IO pad to 3.0V ??????*/
    /* poll some bits?? */
    usdhc_regs->sys_ctrl |= USDHC_SYS_CTRL_INITA; /* 80 clock ticks for power up */
    usdhc_send_command(0);
}

void init() {
    microkit_dbg_puts("hello from usdhc driver\n");
    sddf_printf("%d\n", usdhc_regs->pres_state);

    usdhc_regs->int_status_en |=
      USDHC_INT_STATUS_CCSEN | USDHC_INT_STATUS_TCSEN |
      USDHC_INT_STATUS_CIESEN | USDHC_INT_STATUS_CRMSEN |
      USDHC_INT_STATUS_CTOESEN | USDHC_INT_STATUS_DTOESEN |
      USDHC_INT_STATUS_CIESEN | USDHC_INT_STATUS_DCSESEN |
      USDHC_INT_STATUS_CCESEN | USDHC_INT_STATUS_CINSSEN;

    usdhc_regs->int_signal_en |=
      USDHC_INT_SIGNAL_CCIEN | USDHC_INT_SIGNAL_TCIEN |
      USDHC_INT_SIGNAL_CIEIEN | USDHC_INT_SIGNAL_CRMIEN |
      USDHC_INT_SIGNAL_CTOEIEN | USDHC_INT_SIGNAL_DTOEIEN |
      USDHC_INT_SIGNAL_CIEIEN | USDHC_INT_SIGNAL_DCSEIEN |
      USDHC_INT_SIGNAL_CCEIEN | USDHC_INT_SIGNAL_CINSIEN;

    usdhc_software_reset();
}
