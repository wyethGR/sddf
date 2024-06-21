#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <microkit.h>
#include <sddf/network/queue.h>
#include <sddf/network/constants.h>
#include <sddf/util/fence.h>
#include <sddf/util/util.h>
#include <sddf/util/printf.h>
#include <ethernet_config.h>

/* Notification channels */
#define DRIVER_CH 0
#define CLIENT_CH 1

/* Queue regions */
uintptr_t rx_free_drv;
uintptr_t rx_active_drv;
uintptr_t rx_free_virt;
uintptr_t rx_active_virt;

/* Buffer data regions */
uintptr_t rx_buffer_data_region_vaddr;
uintptr_t rx_buffer_data_region_paddr;

typedef struct state {
    net_queue_handle_t rx_queue_drv;
    net_queue_handle_t rx_queue_clients[NUM_CLIENTS];
    uint8_t mac_addrs[NUM_CLIENTS][ETH_HWADDR_LEN];
} state_t;

state_t state;

/* Boolean to indicate whether a packet has been enqueued into the driver's free queue during notification handling */
static bool notify_drv;

/* Return the client ID if the Mac address is a match. */
int get_client(struct ethernet_header * buffer)
{
    for (int client = 0; client < NUM_CLIENTS; client++) {
        bool match = true;
        for (int i = 0; (i < ETH_HWADDR_LEN) && match; i++) if (buffer->dest.addr[i] != state.mac_addrs[client][i]) match = false;
        if (match) return client;
    }
    return -1;
}

void rx_return(void)
{
    bool reprocess = true;
    bool notify_clients[NUM_CLIENTS] = {false};
    while (reprocess) {
        while (!net_queue_empty_active(&state.rx_queue_drv)) {
            net_buff_desc_t buffer;
            int err = net_dequeue_active(&state.rx_queue_drv, &buffer);
            assert(!err);

            buffer.io_or_offset = buffer.io_or_offset - rx_buffer_data_region_paddr;
            microkit_arm_vspace_data_invalidate(buffer.io_or_offset + rx_buffer_data_region_vaddr, buffer.io_or_offset + rx_buffer_data_region_vaddr + ROUND_UP(buffer.len, CONFIG_L1_CACHE_LINE_SIZE_BITS));

            int client = get_client((struct ethernet_header *) (buffer.io_or_offset + rx_buffer_data_region_vaddr));
            if (client >= 0) {
                err = net_enqueue_active(&state.rx_queue_clients[client], buffer);
                assert(!err);
                notify_clients[client] = true;
            } else {
                buffer.io_or_offset = buffer.io_or_offset + rx_buffer_data_region_paddr;
                err = net_enqueue_free(&state.rx_queue_drv, buffer);
                assert(!err);
                notify_drv = true;
            }
        }

        net_request_signal_active(&state.rx_queue_drv);
        reprocess = false;

        if (!net_queue_empty_active(&state.rx_queue_drv)) {
            net_cancel_signal_active(&state.rx_queue_drv);
            reprocess = true;
        }
    }

    for (int client = 0; client < NUM_CLIENTS; client++) {
        if (notify_clients[client] && net_require_signal_active(&state.rx_queue_clients[client])) {
            net_cancel_signal_active(&state.rx_queue_clients[client]);
            microkit_notify(client + CLIENT_CH);
        }
    }    
}

void rx_provide(void)
{
    for (int client = 0; client < NUM_CLIENTS; client++) {
        bool reprocess = true;
        while (reprocess) {
            while (!net_queue_empty_free(&state.rx_queue_clients[client])) {
                net_buff_desc_t buffer;
                int err = net_dequeue_free(&state.rx_queue_clients[client], &buffer);
                assert(!err);
                assert(!(buffer.io_or_offset % NET_BUFFER_SIZE) && 
                       (buffer.io_or_offset < NET_BUFFER_SIZE * state.rx_queue_clients[client].size));

                buffer.io_or_offset = buffer.io_or_offset + rx_buffer_data_region_paddr;
                err = net_enqueue_free(&state.rx_queue_drv, buffer);
                assert(!err);
                notify_drv = true;
            }

            net_request_signal_free(&state.rx_queue_clients[client]);
            reprocess = false;

            if (!net_queue_empty_free(&state.rx_queue_clients[client])) {
                net_cancel_signal_free(&state.rx_queue_clients[client]);
                reprocess = true;
            }
        }
    }

    if (notify_drv && net_require_signal_free(&state.rx_queue_drv)) {
        net_cancel_signal_free(&state.rx_queue_drv);
        microkit_notify_delayed(DRIVER_CH);
    }
}

void notified(microkit_channel ch)
{
    rx_return();
    rx_provide();
}

void init(void)
{
    virt_mac_addr_init_sys(microkit_name, (uint8_t *) state.mac_addrs);

    net_queue_init(&state.rx_queue_drv, (net_queue_t *)rx_free_drv, (net_queue_t *)rx_active_drv, RX_QUEUE_SIZE_DRIV);
    net_queue_init(&state.rx_queue_clients[0], (net_queue_t *)rx_free_virt, (net_queue_t *)rx_active_virt, RX_QUEUE_SIZE_CLI0);
    net_buffers_init(&state.rx_queue_drv, rx_buffer_data_region_paddr);

    if (net_require_signal_free(&state.rx_queue_drv)) {
        net_cancel_signal_free(&state.rx_queue_drv);
        microkit_notify_delayed(DRIVER_CH);
    }
}
