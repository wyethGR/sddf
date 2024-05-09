#pragma once

#include <microkit.h>
#include <sddf/serial/queue.h>

/* Convert received carriage returns to newlines in the rx virtualiser. */
#define CARRIAGE_TO_NEWLINE 1

/* Associate a colour with each client's output. */
#define SERIAL_WITH_COLOUR 1

/* Control character to switch input stream - ctrl \. To input character input twice. */
#define SERIAL_SWITCH_CHAR 28

/* Control character to terminate client number input. */
#define SERIAL_TERM_NUM '\n'

/* Default baud rate of the uart device */
#define UART_DEFAULT_BAUD 115200

/* String to be printed by the driver to start console input */
#define SERIAL_CONSOLE_BEGIN_STRING "UART|LOG: Init complete\n"
#define SERIAL_CONSOLE_BEGIN_STRING_LEN 25

#define SERIAL_NUM_CLIENTS 2

#define SERIAL_CLI0_NAME "client0"
#define SERIAL_CLI1_NAME "client1"
#define SERIAL_VIRT_RX_NAME "virt_rx"
#define SERIAL_VIRT_TX_NAME "virt_tx"
#define SERIAL_DRIVER_NAME "uart"

#define SERIAL_QUEUE_SIZE                          0x1000
#define SERIAL_DATA_REGION_SIZE                    0x200000

#define TX_SERIAL_DATA_REGION_SIZE_DRIV            SERIAL_DATA_REGION_SIZE
#define TX_SERIAL_DATA_REGION_SIZE_CLI0            SERIAL_DATA_REGION_SIZE
#define TX_SERIAL_DATA_REGION_SIZE_CLI1            SERIAL_DATA_REGION_SIZE

#define RX_SERIAL_DATA_REGION_SIZE_DRIV            SERIAL_DATA_REGION_SIZE
#define RX_SERIAL_DATA_REGION_SIZE_CLI0            SERIAL_DATA_REGION_SIZE
#define RX_SERIAL_DATA_REGION_SIZE_CLI1            SERIAL_DATA_REGION_SIZE

#define SERIAL_MAX_TX_DATA_SIZE MAX(TX_SERIAL_DATA_REGION_SIZE_DRIV, MAX(TX_SERIAL_DATA_REGION_SIZE_CLI0, TX_SERIAL_DATA_REGION_SIZE_CLI1))
#define SERIAL_MAX_RX_DATA_SIZE MAX(RX_SERIAL_DATA_REGION_SIZE_DRIV, MAX(RX_SERIAL_DATA_REGION_SIZE_CLI0, RX_SERIAL_DATA_REGION_SIZE_CLI1))
#define SERIAL_MAX_DATA_SIZE MAX(SERIAL_MAX_TX_DATA_SIZE, SERIAL_MAX_RX_DATA_SIZE)
_Static_assert(SERIAL_MAX_DATA_SIZE < UINT32_MAX, "Data regions must be smaller than UINT32 max to correctly use queue data structure.");

static inline bool __serial_str_match(const char *s0, const char *s1)
{
    while (*s0 != '\0' && *s1 != '\0' && *s0 == *s1) {
        s0++, s1++;
    }
    return *s0 == *s1;
}

static inline void serial_cli_queue_init_sys(char *pd_name, serial_queue_handle_t *rx_queue_handle, uintptr_t rx_queue,
                                serial_queue_handle_t *tx_queue_handle, uintptr_t tx_queue)
{
    if (__serial_str_match(pd_name, SERIAL_CLI0_NAME)) {
        serial_queue_init(rx_queue_handle, (serial_queue_t *) rx_queue, RX_SERIAL_DATA_REGION_SIZE_CLI0);
        serial_queue_init(tx_queue_handle, (serial_queue_t *) tx_queue, TX_SERIAL_DATA_REGION_SIZE_CLI0);
    } else if (__serial_str_match(pd_name, SERIAL_CLI1_NAME)) {
        serial_queue_init(rx_queue_handle, (serial_queue_t *) rx_queue, RX_SERIAL_DATA_REGION_SIZE_CLI1);
        serial_queue_init(tx_queue_handle, (serial_queue_t *) tx_queue, TX_SERIAL_DATA_REGION_SIZE_CLI1);
    }
}

static inline void serial_virt_queue_init_sys(char *pd_name, serial_queue_handle_t *cli_queue_handle, uintptr_t cli_queue)
{
    if (__serial_str_match(pd_name, SERIAL_VIRT_RX_NAME)) {
        serial_queue_init(cli_queue_handle, (serial_queue_t *) cli_queue, RX_SERIAL_DATA_REGION_SIZE_CLI0);
        serial_queue_init(&cli_queue_handle[1], (serial_queue_t *) (cli_queue + SERIAL_QUEUE_SIZE), RX_SERIAL_DATA_REGION_SIZE_CLI1);
    } else if (__serial_str_match(pd_name, SERIAL_VIRT_TX_NAME)) {
        serial_queue_init(cli_queue_handle, (serial_queue_t *) cli_queue, TX_SERIAL_DATA_REGION_SIZE_CLI0);
        serial_queue_init(&cli_queue_handle[1], (serial_queue_t *) (cli_queue + SERIAL_QUEUE_SIZE), TX_SERIAL_DATA_REGION_SIZE_CLI1);
    }
}

static inline void serial_mem_region_init_sys(char *pd_name, uintptr_t *mem_regions, uintptr_t start_region) {
    if (__serial_str_match(pd_name, SERIAL_VIRT_TX_NAME)) {
        mem_regions[0] = start_region;
        mem_regions[1] = start_region + TX_SERIAL_DATA_REGION_SIZE_CLI0;
    } else if (__serial_str_match(pd_name, SERIAL_VIRT_RX_NAME)) {
        mem_regions[0] = start_region;
        mem_regions[1] = start_region + RX_SERIAL_DATA_REGION_SIZE_CLI0;
    }
}

#if SERIAL_WITH_COLOUR
static inline void serial_channel_names_init(char **client_names) {
    client_names[0] = SERIAL_CLI0_NAME;
    client_names[1] = SERIAL_CLI1_NAME;
}
#endif