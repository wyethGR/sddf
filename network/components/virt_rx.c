#include <microkit.h>
#include <sddf/network/constants.h>
#include <sddf/network/queue.h>
#include <sddf/util/cache.h>
#include <sddf/util/fence.h>
#include <sddf/util/printf.h>
#include <sddf/util/util.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "config.h"

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

net_queue_handle_t rx_queue_drv;
net_queue_handle_t rx_queue_client;

/* Boolean to indicate whether a packet has been enqueued into the driver's free
 * queue during notification handling */
static bool notify_drv;

void rx_return(void) {
    bool reprocess     = true;
    bool notify_client = false;
    while (reprocess) {
        while (!net_queue_empty_active(&rx_queue_drv)) {
            net_buff_desc_t buffer;
            int err = net_dequeue_active(&rx_queue_drv, &buffer);
            assert(!err);

            buffer.io_or_offset =
                buffer.io_or_offset - rx_buffer_data_region_paddr;
            uintptr_t buffer_vaddr = buffer.io_or_offset + rx_buffer_data_region_vaddr;
            cache_clean_and_invalidate(buffer_vaddr, buffer_vaddr + buffer.len);
            //microkit_arm_vspace_data_invalidate(
            //    buffer.io_or_offset + rx_buffer_data_region_vaddr,
            //    buffer.io_or_offset + rx_buffer_data_region_vaddr +
            //        ROUND_UP(buffer.len, CONFIG_L1_CACHE_LINE_SIZE_BITS));

            err = net_enqueue_active(&rx_queue_client, buffer);
            assert(!err);
            notify_client = true;
        }

        net_request_signal_active(&rx_queue_drv);
        reprocess = false;

        if (!net_queue_empty_active(&rx_queue_drv)) {
            net_cancel_signal_active(&rx_queue_drv);
            reprocess = true;
        }
    }

    if (notify_client && net_require_signal_active(&rx_queue_client)) {
        net_cancel_signal_active(&rx_queue_client);
        microkit_notify(CLIENT_CH);
    }
}

void rx_provide(void) {
    bool reprocess = true;
    while (reprocess) {
        while (!net_queue_empty_free(&rx_queue_client)) {
            net_buff_desc_t buffer;
            int err = net_dequeue_free(&rx_queue_client, &buffer);
            assert(!err);
            assert(
                !(buffer.io_or_offset % NET_BUFFER_SIZE) &&
                (buffer.io_or_offset < NET_BUFFER_SIZE * rx_queue_client.size));

            buffer.io_or_offset =
                buffer.io_or_offset + rx_buffer_data_region_paddr;
            err = net_enqueue_free(&rx_queue_drv, buffer);
            assert(!err);
            notify_drv = true;
        }

        net_request_signal_free(&rx_queue_client);
        reprocess = false;

        if (!net_queue_empty_free(&rx_queue_client)) {
            net_cancel_signal_free(&rx_queue_client);
            reprocess = true;
        }
    }

    if (notify_drv && net_require_signal_free(&rx_queue_drv)) {
        net_cancel_signal_free(&rx_queue_drv);
        microkit_notify_delayed(DRIVER_CH);
        notify_drv = false;
    }
}

void notified(microkit_channel ch) {
    rx_return();
    rx_provide();
}

void init(void) {
    net_queue_init(&rx_queue_drv,
                   (net_queue_t *) rx_free_drv,
                   (net_queue_t *) rx_active_drv,
                   BUFS_PER_DIR);
    net_queue_init(&rx_queue_client,
                   (net_queue_t *) rx_free_virt,
                   (net_queue_t *) rx_active_virt,
                   BUFS_PER_DIR);
    net_buffers_init(&rx_queue_drv, rx_buffer_data_region_paddr);

    if (net_require_signal_free(&rx_queue_drv)) {
        net_cancel_signal_free(&rx_queue_drv);
        microkit_notify_delayed(DRIVER_CH);
    }
}
