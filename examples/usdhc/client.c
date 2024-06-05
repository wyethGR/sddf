#include <stdint.h>
#include <microkit.h>

void
notified(microkit_channel ch)
{

}

void
init(void)
{
    microkit_dbg_puts("Hello from client\n");
}
