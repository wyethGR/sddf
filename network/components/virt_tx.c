#include <microkit.h>
#include <sddf/network/queue.h>
#include <sddf/util/cache.h>
#include <sddf/util/fence.h>
#include <sddf/util/printf.h>
#include <sddf/util/util.h>

#include "config.h"

#define DRIVER    0
#define CLIENT_CH 1

uintptr_t tx_free_drv;
uintptr_t tx_active_drv;
uintptr_t tx_free_virt;
uintptr_t tx_active_virt;

uintptr_t tx_buffer_data_region_vaddr;
uintptr_t tx_buffer_data_region_paddr;

net_queue_handle_t tx_queue_drv;
net_queue_handle_t tx_queue_client;

void tx_provide(void) {
    bool enqueued  = false;
    bool reprocess = true;
    while (reprocess) {
        while (!net_queue_empty_active(&tx_queue_client)) {
            net_buff_desc_t buffer;
            int err = net_dequeue_active(&tx_queue_client, &buffer);
            assert(!err);

            if (buffer.io_or_offset % NET_BUFFER_SIZE ||
                buffer.io_or_offset >= NET_BUFFER_SIZE * tx_queue_client.size) {
                sddf_dprintf("VIRT_TX|LOG: Client provided offset %lx which is "
                             "not buffer aligned or outside of buffer region\n",
                             buffer.io_or_offset);
                err = net_enqueue_free(&tx_queue_client, buffer);
                assert(!err);
                continue;
            }

            cache_clean(buffer.io_or_offset + tx_buffer_data_region_vaddr,
                        buffer.io_or_offset + tx_buffer_data_region_vaddr +
                            buffer.len);

            buffer.io_or_offset =
                buffer.io_or_offset + tx_buffer_data_region_paddr;
            err = net_enqueue_active(&tx_queue_drv, buffer);
            assert(!err);
            enqueued = true;
        }

        net_request_signal_active(&tx_queue_client);
        reprocess = false;

        if (!net_queue_empty_active(&tx_queue_client)) {
            net_cancel_signal_active(&tx_queue_client);
            reprocess = true;
        }
    }

    if (enqueued && net_require_signal_active(&tx_queue_drv)) {
        net_cancel_signal_active(&tx_queue_drv);
        microkit_notify_delayed(DRIVER);
    }
}

void tx_return(void) {
    bool reprocess     = true;
    bool notify_client = false;
    while (reprocess) {
        while (!net_queue_empty_free(&tx_queue_drv)) {
            net_buff_desc_t buffer;
            int err = net_dequeue_free(&tx_queue_drv, &buffer);
            assert(!err);

            buffer.io_or_offset -= tx_buffer_data_region_paddr;
            err = net_enqueue_free(&tx_queue_client, buffer);
            assert(!err);
            notify_client = true;
        }

        net_request_signal_free(&tx_queue_drv);
        reprocess = false;

        if (!net_queue_empty_free(&tx_queue_drv)) {
            net_cancel_signal_free(&tx_queue_drv);
            reprocess = true;
        }
    }

    if (notify_client && net_require_signal_free(&tx_queue_client)) {
        net_cancel_signal_free(&tx_queue_client);
        microkit_notify(CLIENT_CH);
    }
}

void notified(microkit_channel ch) {
    tx_return();
    tx_provide();
}

void init(void) {
    net_queue_init(&tx_queue_drv,
                   (net_queue_t *) tx_free_drv,
                   (net_queue_t *) tx_active_drv,
                   NUM_BUFS);
    net_queue_init(&tx_queue_client,
                   (net_queue_t *) tx_free_virt,
                   (net_queue_t *) tx_active_virt,
                   NUM_BUFS);

    tx_provide();
}
