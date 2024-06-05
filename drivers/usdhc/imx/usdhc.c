#include "include/usdhc.h"

#include <microkit.h>
#include <sddf/util/printf.h>

imx_usdhc_regs_t *usdhc1_regs;
imx_usdhc_regs_t *usdhc2_regs;
imx_usdhc_regs_t *usdhc3_regs;

/*
    TODO: Details
        10.3.3.6.3 card interrupt handling.
*/

void notified(microkit_channel ch) {
    microkit_dbg_puts("notification!\n");
}
void init() {
    microkit_dbg_puts("hello from usdhc driver\n");
    sddf_printf("%d %d %d\n", usdhc1_regs->pres_state, usdhc2_regs->pres_state, usdhc3_regs->pres_state);
}
