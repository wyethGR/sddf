/*
 * Copyright 2024, UNSW
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * Main things to figre out are now:
 * how to allocate and free descriptors?
 */

#include <stdbool.h>
#include <stdint.h>
#include <microkit.h>
#include <sddf/network/queue.h>
#include <sddf/util/fence.h>
#include <sddf/util/util.h>
#include <sddf/util/printf.h>
#include <sddf/virtio/virtio.h>
#include <sddf/virtio/virtio_queue.h>
#include <ethernet_config.h>

#include "ethernet.h"

#define VIRTIO_MMIO_NET_OFFSET (0xe00)

#define IRQ_CH 0
#define TX_CH  1
#define RX_CH  2

uintptr_t eth_regs;
uintptr_t hw_ring_buffer_vaddr;
uintptr_t hw_ring_buffer_paddr;

uintptr_t rx_free;
uintptr_t rx_active;
uintptr_t tx_free;
uintptr_t tx_active;

#define RX_COUNT 128
#define TX_COUNT 128
#define MAX_COUNT MAX(RX_COUNT, TX_COUNT)

#define HW_RING_SIZE (0x10000)

struct virtq rx_virtq;
struct virtq tx_virtq;
uint16_t rx_last_seen_used = 0;
uint16_t tx_last_seen_used = 0;

net_queue_handle_t rx_queue;
net_queue_handle_t tx_queue;

uintptr_t virtio_net_headers_vaddr;
uintptr_t virtio_net_headers_paddr;
virtio_net_hdr_t *virtio_net_headers;

volatile virtio_mmio_regs_t *regs;

struct desc_entry {
    uint16_t used;
    uint16_t next;
};

struct desc_entry rx_descriptors[RX_COUNT];
struct desc_entry tx_descriptors[TX_COUNT];

static inline bool virtio_avail_full(struct virtq *virtq) {
    return virtq->avail->idx % virtq->num == virtq->num - 1;
}

static inline bool virtio_used_empty(struct virtq *virtq) {
    return virtq->used->idx % virtq->num == 0;
}

static void rx_provide(void)
{
    // bool reprocess = true;
    // while (reprocess) {
    //     while (!virtio_avail_full(&rx_virtq) && !net_queue_empty_free(&rx_queue)) {
    //         net_buff_desc_t buffer;
    //         int err = net_dequeue_free(&rx_queue, &buffer);
    //         assert(!err);

    //         // TODO
    //     }

    //     net_request_signal_free(&rx_queue);
    //     reprocess = false;

    //     if (!net_queue_empty_free(&rx_queue) && !virtio_avail_full(&rx_virtq)) {
    //         net_cancel_signal_free(&rx_queue);
    //         reprocess = true;
    //     }
    // }
}

static void rx_return(void)
{
    /* Extract RX buffers from the 'used' and pass them up to the client by putting them
     * in our sDDF 'active' queues. */
    bool packets_transferred = false;
    // TODO: always handle wrapping with indexes
    size_t i = rx_last_seen_used;
    while (i != rx_virtq.used->idx) {
        LOG_DRIVER("i: 0x%lx\n", i);
        struct virtq_used_elem used = rx_virtq.used->ring[i];
        LOG_DRIVER("used id: 0x%x, len: 0x%x\n", used.id, used.len);
        uint64_t addr = rx_virtq.desc[used.id].addr + sizeof(virtio_net_hdr_t);
        uint32_t len = used.len - sizeof(virtio_net_hdr_t);
        // TODO: assert that len > 0?
        LOG_DRIVER("descriptor addr: 0x%lx, len: 0x%x\n", addr, len);
        net_buff_desc_t buffer = { addr, len };
        int err = net_enqueue_active(&rx_queue, buffer);
        assert(!err);

        assert(!(rx_virtq.desc[used.id].flags & VIRTQ_DESC_F_NEXT));

        // while (rx_virtq.desc[used.id].flags & VIRTQ_DESC_F_NEXT) {
        //     LOG_DRIVER("has next!\n");
        //     used = rx_virtq.used->ring[rx_virtq.desc[used.id].next];
        //     uint64_t addr = rx_virtq.desc[used.id].addr;
        //     uint32_t len = rx_virtq.desc[used.id].len;
        //     LOG_DRIVER("addr: 0x%lx, len: 0x%x\n", addr, len);
        //     net_buff_desc_t buffer = { addr, len };
        //     int err = net_enqueue_active(&rx_queue, buffer);
        //     assert(!err);
        // }
        packets_transferred = true;
        i++;
    }

    rx_last_seen_used = rx_virtq.used->idx;

    if (packets_transferred && net_require_signal_active(&rx_queue)) {
        LOG_DRIVER("signalling RX\n");
        net_cancel_signal_active(&rx_queue);
        microkit_notify(RX_CH);
    }
}

static void tx_provide(void)
{
    bool reprocess = true;
    while (reprocess) {
        while (!virtio_avail_full(&tx_virtq) && !net_queue_empty_active(&tx_queue)) {
            net_buff_desc_t buffer;
            int err = net_dequeue_active(&tx_queue, &buffer);
            assert(!err);

            /* Now we need to put our buffer into the virtIO ring */
            size_t desc_idx = tx_virtq.desc_free;
            /* We should not run out of descriptors assuming that the avail ring is not full. */
            assert(desc_idx < tx_virtq.num);
            tx_virtq.avail->ring[tx_virtq.avail->idx] = desc_idx;
            tx_virtq.desc_free++;

            virtio_net_hdr_t *hdr = &virtio_net_headers[desc_idx];
            hdr->flags = 0;
            hdr->gso_type = VIRTIO_NET_HDR_GSO_NONE;
            hdr->hdr_len = 0;  /* not used unless we have segmentation offload */
            hdr->gso_size = 0; /* same */
            hdr->csum_start = 0;
            hdr->csum_offset = 0;
            tx_virtq.desc[desc_idx].addr = virtio_net_headers_paddr + (desc_idx * sizeof(virtio_net_hdr_t));
            tx_virtq.desc[desc_idx].len = sizeof(virtio_net_hdr_t);
            tx_virtq.desc[desc_idx].next = tx_virtq.desc_free;
            tx_virtq.desc[desc_idx].flags = VIRTQ_DESC_F_NEXT;

            LOG_DRIVER("header desc_idx: 0x%lx addr: 0x%lx, len: 0x%x, next: 0x%x\n", desc_idx,
                tx_virtq.desc[desc_idx].addr, tx_virtq.desc[desc_idx].len, tx_virtq.desc[desc_idx].next);

            desc_idx = tx_virtq.desc_free;
            /* We should not run out of descriptors assuming that the avail ring is not full. */
            assert(desc_idx < tx_virtq.num);
            tx_virtq.desc_free++;

            tx_virtq.desc[desc_idx].addr = buffer.io_or_offset;
            tx_virtq.desc[desc_idx].len = buffer.len;
            tx_virtq.desc[desc_idx].flags = 0;

            LOG_DRIVER("enqueueing into virtIO avail TX ring, desc_idx: 0x%lx, addr: 0x%lx, len: 0x%x\n",
                        desc_idx, buffer.io_or_offset, buffer.len);

            /* @ivanv: use a memory fence. If a memory fence is used, it is more optimal
             * to update this number only once */
            tx_virtq.avail->idx++;
        }

        net_request_signal_active(&tx_queue);
        reprocess = false;

        if (!virtio_avail_full(&tx_virtq) && !net_queue_empty_active(&tx_queue)) {
            net_cancel_signal_active(&tx_queue);
            reprocess = true;
        }
    }

    // Finally, need to notify the queue
    /* This assumes VIRTIO_F_NOTIFICATION_DATA has not been negotiated */
    regs->QueueNotify = VIRTIO_NET_TX_QUEUE;
}

static void tx_return(void)
{
    /* We must look through the 'used' ring of the TX virtqueue and place them in our
     * sDDF *free* queue. I understand the terminology is confusing. */
    bool enqueued = false;
    // size_t virtq_idx = tx_virtq.used->idx;
    // while () {
        /* Ensure that this buffer has been sent by the device */
        // volatile struct descriptor *d = &(tx.descr[tx.head]);
        // if (d->status & DESC_TXSTS_OWNBYDMA) break;
        // net_buff_desc_t buffer = tx.descr_mdata[tx.head];
        // THREAD_MEMORY_ACQUIRE();

        // int err = net_enqueue_free(&tx_queue, buffer);
        // assert(!err);
        // enqueued = true;
        // tx.head = (tx.head + 1) % TX_COUNT;
    // }

    if (enqueued && net_require_signal_free(&tx_queue)) {
        net_cancel_signal_free(&tx_queue);
        microkit_notify(TX_CH);
    }
}

static void handle_irq()
{
    uint32_t irq_status = regs->InterruptStatus;
    if (irq_status & VIRTIO_MMIO_IRQ_VQUEUE) {
        // We don't know whether the IRQ is related to a change to the RX queue
        // or TX queue, so we check both.
        rx_return();
        tx_return();
        // We have handled the used buffer notification
        regs->InterruptACK = VIRTIO_MMIO_IRQ_VQUEUE;
    }

    if (irq_status & VIRTIO_MMIO_IRQ_CONFIG) {
        LOG_DRIVER_ERR("unexpected change in configuration\n");
    }
}

static void eth_setup(void)
{
    // Do MMIO device init (section 4.2.3.1)
    if (!virtio_mmio_check_magic(regs)) {
        LOG_DRIVER_ERR("invalid virtIO magic value!\n");
        return;
    }

    if (virtio_mmio_version(regs) != VIRTIO_VERSION) {
        LOG_DRIVER_ERR("not correct virtIO version!\n");
        return;
    }

    if (!virtio_mmio_check_device_id(regs, VIRTIO_DEVICE_ID_NET)) {
        LOG_DRIVER_ERR("not a virtIO network device!\n");
        return;
    }

    LOG_DRIVER("version: 0x%x\n", virtio_mmio_version(regs));

    // Do normal device initialisation (section 3.2)

    // First reset the device
    regs->Status = 0;

    // Set the ACKNOWLEDGE bit to say we have noticed the device
    regs->Status = VIRTIO_DEVICE_STATUS_ACKNOWLEDGE;
    // Set the DRIVER bit to say we know how to drive the device
    regs->Status = VIRTIO_DEVICE_STATUS_DRIVER;

    LOG_DRIVER("device feature bits:\n");
    uint32_t feature_low = regs->DeviceFeatures;
    LOG_DRIVER("feature low: 0x%x\n", feature_low);
    regs->DeviceFeaturesSel = 1;
    uint32_t feature_high = regs->DeviceFeatures;
    LOG_DRIVER("feature high: 0x%x\n", feature_high);
    uint64_t feature = feature_low | ((uint64_t)feature_high << 32);
    LOG_DRIVER("feature: 0x%lx\n", feature);
    virtio_net_print_features(feature);

    regs->DriverFeatures = VIRTIO_NET_F_MAC;
    regs->DriverFeaturesSel = 1;
    regs->DriverFeatures = 0;

    regs->Status = VIRTIO_DEVICE_STATUS_FEATURES_OK;

    if (!(regs->Status & VIRTIO_DEVICE_STATUS_FEATURES_OK)) {
        LOG_DRIVER_ERR("device status features is not OK!\n");
        return;
    }

    volatile virtio_net_config_t *config = (virtio_net_config_t *)regs->Config;
    virtio_net_print_config(config);

    // Setup the virtqueues

    // TODO: need to have asserts regarding alignment for each desc,avail,used queue

    size_t rx_desc_off = 0;
    size_t rx_avail_off = rx_desc_off + (16 * RX_COUNT);
    size_t rx_used_off = rx_avail_off + (6 + 2 * RX_COUNT);
    size_t tx_desc_off = rx_used_off + (6 + 8 * RX_COUNT);
    size_t tx_avail_off = tx_desc_off + (16 * TX_COUNT);
    size_t tx_used_off = tx_avail_off + (6 + 2 * TX_COUNT);
    size_t size = tx_used_off + (6 + 8 * TX_COUNT);

    assert(size <= HW_RING_SIZE);

    rx_virtq.num = RX_COUNT;
    rx_virtq.desc = (struct virtq_desc *)(hw_ring_buffer_vaddr + rx_desc_off);
    rx_virtq.avail = (struct virtq_avail *)(hw_ring_buffer_vaddr + rx_avail_off);
    rx_virtq.used = (struct virtq_used *)(hw_ring_buffer_vaddr + rx_used_off);
    rx_virtq.desc_free = 0;

    for (int i = 0; i < RX_COUNT - 1; i++) {
        net_buff_desc_t desc;
        // LOG_DRIVER("here 1!\n");
        int err = net_dequeue_free(&rx_queue, &desc);
        // LOG_DRIVER("here 2!\n");
        assert(!err);

        // TODO: some check for whether desc.len is valid?
        LOG_DRIVER("(%d): addr: 0x%lx, len: 0x%x\n", i, desc.io_or_offset, NET_BUFFER_SIZE);
        rx_virtq.desc[i].addr = desc.io_or_offset;
        rx_virtq.desc[i].len = NET_BUFFER_SIZE;
        rx_virtq.desc[i].flags = VIRTQ_DESC_F_WRITE;
        rx_virtq.avail->ring[rx_virtq.avail->idx] = i;
        rx_virtq.avail->idx++;
    }

    assert(virtio_avail_full(&rx_virtq));

    tx_virtq.num = TX_COUNT;
    tx_virtq.desc = (struct virtq_desc *)(hw_ring_buffer_vaddr + tx_desc_off);
    tx_virtq.avail = (struct virtq_avail *)(hw_ring_buffer_vaddr + tx_avail_off);
    tx_virtq.used = (struct virtq_used *)(hw_ring_buffer_vaddr + tx_used_off);
    tx_virtq.desc_free = 0;

    // Setup RX queue first
    assert(regs->QueueNumMax >= RX_COUNT);
    regs->QueueSel = VIRTIO_NET_RX_QUEUE;
    regs->QueueNum = RX_COUNT;
    regs->QueueDescLow = (hw_ring_buffer_paddr + rx_desc_off) & 0xFFFFFFFF;
    regs->QueueDescHigh = (hw_ring_buffer_paddr + rx_desc_off) >> 32;
    regs->QueueDriverLow = (hw_ring_buffer_paddr + rx_avail_off) & 0xFFFFFFFF;
    regs->QueueDriverHigh = (hw_ring_buffer_paddr + rx_avail_off) >> 32;
    regs->QueueDeviceLow = (hw_ring_buffer_paddr + rx_used_off) & 0xFFFFFFFF;
    regs->QueueDeviceHigh = (hw_ring_buffer_paddr + rx_used_off) >> 32;
    regs->QueueReady = 1;

    // Setup TX queue
    assert(regs->QueueNumMax >= TX_COUNT);
    regs->QueueSel = VIRTIO_NET_TX_QUEUE;
    regs->QueueNum = TX_COUNT;
    regs->QueueDescLow = (hw_ring_buffer_paddr + tx_desc_off) & 0xFFFFFFFF;
    regs->QueueDescHigh = (hw_ring_buffer_paddr + tx_desc_off) >> 32;
    regs->QueueDriverLow = (hw_ring_buffer_paddr + tx_avail_off) & 0xFFFFFFFF;
    regs->QueueDriverHigh = (hw_ring_buffer_paddr + tx_avail_off) >> 32;
    regs->QueueDeviceLow = (hw_ring_buffer_paddr + tx_used_off) & 0xFFFFFFFF;
    regs->QueueDeviceHigh = (hw_ring_buffer_paddr + tx_used_off) >> 32;
    regs->QueueReady = 1;

    // Set the DRIVER_OK status bit
    regs->Status = VIRTIO_DEVICE_STATUS_DRIVER_OK;
    regs->InterruptACK = VIRTIO_MMIO_IRQ_VQUEUE;
}

void init(void)
{
    regs = (volatile virtio_mmio_regs_t *) (eth_regs + VIRTIO_MMIO_NET_OFFSET);
    virtio_net_headers = (virtio_net_hdr_t *) virtio_net_headers_vaddr;

    net_queue_init(&rx_queue, (net_queue_t *)rx_free, (net_queue_t *)rx_active, RX_QUEUE_SIZE_DRIV);
    net_queue_init(&tx_queue, (net_queue_t *)tx_free, (net_queue_t *)tx_active, TX_QUEUE_SIZE_DRIV);

    eth_setup();

    // rx_provide();
    // tx_provide();

    microkit_irq_ack(IRQ_CH);
}

void notified(microkit_channel ch)
{
    switch(ch) {
        case IRQ_CH:
            LOG_DRIVER("==================== got IRQ!\n");
            handle_irq();
            microkit_irq_ack_delayed(ch);
            break;
        case RX_CH:
            LOG_DRIVER("got message from RX!\n");
            rx_provide();
            break;
        case TX_CH:
            LOG_DRIVER("got message from TX!\n");
            tx_provide();
            break;
        default:
            LOG_DRIVER_ERR("received notification on unexpected channel %u\n", ch);
            break;
    }
}
