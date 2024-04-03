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

#define DEBUG_DRIVER

#ifdef DEBUG_DRIVER
#define LOG_DRIVER(...) do{ sddf_dprintf("ETH DRIVER|INFO: "); sddf_dprintf(__VA_ARGS__); }while(0)
#else
#define LOG_DRIVER(...) do{}while(0)
#endif

#define LOG_DRIVER_ERR(...) do{ sddf_printf("ETH DRIVER|ERROR: "); sddf_printf(__VA_ARGS__); }while(0)

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

net_queue_handle_t rx_queue;
net_queue_handle_t tx_queue;

uintptr_t virtio_net_headers_vaddr;
uintptr_t virtio_net_headers_paddr;

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
    return virtq->used->idx == 0;
}

static void rx_provide()
{
}

static void rx_return(void)
{
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

            tx_virtq.desc[desc_idx].addr = buffer.io_or_offset;
            tx_virtq.desc[desc_idx].len = buffer.len;

            LOG_DRIVER("enqueueing into virtIO avail TX ring, desc_idx: 0x%lx, addr: 0x%lx, len: 0x%lx\n", desc_idx, buffer.io_or_offset, buffer.len);

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
    size_t virtq_idx = tx_virtq.used->idx;
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

static void print_feature_bits(uint64_t features) {
    if (features & ((uint64_t)1 << VIRTIO_NET_F_CSUM)) {
        LOG_DRIVER("    VIRTIO_NET_F_CSUM\n");
    }
    if (features & ((uint64_t)1 << VIRTIO_NET_F_GUEST_CSUM)) {
        LOG_DRIVER("    VIRTIO_NET_F_GUEST_CSUM\n");
    }
    if (features & ((uint64_t)1 << VIRTIO_NET_F_CTRL_GUEST_OFFLOADS)) {
        LOG_DRIVER("    VIRTIO_NET_F_CTRL_GUEST_OFFLOADS\n");
    }
    if (features & ((uint64_t)1 << VIRTIO_NET_F_MTU)) {
        LOG_DRIVER("    VIRTIO_NET_F_MTU\n");
    }
    if (features & ((uint64_t)1 << VIRTIO_NET_F_GUEST_TSO4)) {
        LOG_DRIVER("    VIRTIO_NET_F_GUEST_TSO4\n");
    }
    if (features & ((uint64_t)1 << VIRTIO_NET_F_GUEST_TSO6)) {
        LOG_DRIVER("    VIRTIO_NET_F_GUEST_TSO6\n");
    }
    if (features & ((uint64_t)1 << VIRTIO_NET_F_GUEST_ECN)) {
        LOG_DRIVER("    VIRTIO_NET_F_GUEST_ECN\n");
    }
    if (features & ((uint64_t)1 << VIRTIO_NET_F_GUEST_UFO)) {
        LOG_DRIVER("    VIRTIO_NET_F_GUEST_UFO\n");
    }
    if (features & ((uint64_t)1 << VIRTIO_NET_F_HOST_TSO4)) {
        LOG_DRIVER("    VIRTIO_NET_F_HOST_TSO4\n");
    }
    if (features & ((uint64_t)1 << VIRTIO_NET_F_HOST_TSO6)) {
        LOG_DRIVER("    VIRTIO_NET_F_HOST_TSO6\n");
    }
    if (features & ((uint64_t)1 << VIRTIO_NET_F_MRG_RXBUF)) {
        LOG_DRIVER("    VIRTIO_NET_F_MRG_RXBUF\n");
    }
    if (features & ((uint64_t)1 << VIRTIO_NET_F_STATUS)) {
        LOG_DRIVER("    VIRTIO_NET_F_STATUS\n");
    }
    if (features & ((uint64_t)1 << VIRTIO_NET_F_CTRL_VQ)) {
        LOG_DRIVER("    VIRTIO_NET_F_CTRL_VQ\n");
    }
    if (features & ((uint64_t)1 << VIRTIO_NET_F_CTRL_RX)) {
        LOG_DRIVER("    VIRTIO_NET_F_CTRL_RX\n");
    }
    if (features & ((uint64_t)1 << VIRTIO_NET_F_CTRL_VLAN)) {
        LOG_DRIVER("    VIRTIO_NET_F_CTRL_VLAN\n");
    }
    if (features & ((uint64_t)1 << VIRTIO_NET_F_CTRL_RX_EXTRA)) {
        LOG_DRIVER("    VIRTIO_NET_F_CTRL_RX_EXTRA\n");
    }
    if (features & ((uint64_t)1 << VIRTIO_NET_F_GUEST_ANNOUNCE)) {
        LOG_DRIVER("    VIRTIO_NET_F_GUEST_ANNOUNCE\n");
    }
    if (features & ((uint64_t)1 << VIRTIO_NET_F_MQ)) {
        LOG_DRIVER("    VIRTIO_NET_F_MQ\n");
    }
    if (features & ((uint64_t)1 << VIRTIO_NET_F_CTRL_MAC_ADDR)) {
        LOG_DRIVER("    VIRTIO_NET_F_CTRL_MAC_ADDR\n");
    }
    if (features & ((uint64_t)1 << VIRTIO_NET_F_HOST_USO)) {
        LOG_DRIVER("    VIRTIO_NET_F_HOST_USO\n");
    }
    if (features & ((uint64_t)1 << VIRTIO_NET_F_HASH_REPORT)) {
        LOG_DRIVER("    VIRTIO_NET_F_HASH_REPORT\n");
    }
    if (features & ((uint64_t)1 << VIRTIO_NET_F_GUEST_HDRLEN)) {
        LOG_DRIVER("    VIRTIO_NET_F_GUEST_HDRLEN\n");
    }
    if (features & ((uint64_t)1 << VIRTIO_NET_F_RSS)) {
        LOG_DRIVER("    VIRTIO_NET_F_RSS\n");
    }
    if (features & ((uint64_t)1 << VIRTIO_NET_F_RSC_EXT)) {
        LOG_DRIVER("    VIRTIO_NET_F_RSC_EXT\n");
    }
    if (features & ((uint64_t)1 << VIRTIO_NET_F_STANDBY)) {
        LOG_DRIVER("    VIRTIO_NET_F_STANDBY\n");
    }
    if (features & ((uint64_t)1 << VIRTIO_NET_F_SPEED_DUPLEX)) {
        LOG_DRIVER("    VIRTIO_NET_F_SPEED_DUPLEX\n");
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

    LOG_DRIVER("init\n");

    LOG_DRIVER("version: 0x%lx\n", virtio_mmio_version(regs));

    volatile virtio_net_config_t *config = (virtio_net_config_t *)regs->Config;
    for (int i = 0; i < 6; i++) {
        LOG_DRIVER("mac[%d]: 0x%lx\n", i, config->mac[i]);
    }
    LOG_DRIVER("mtu: 0x%lx\n", config->mtu);
    LOG_DRIVER("speed: 0x%lx\n", config->speed);

    // Do normal device initialisation (section 3.2)

    // First reset the device
    regs->Status = 0;

    // Set the ACKNOWLEDGE bit to say we have noticed the device
    regs->Status = VIRTIO_DEVICE_STATUS_ACKNOWLEDGE;
    // Set the DRIVER bit to say we know how to drive the device
    regs->Status |= VIRTIO_DEVICE_STATUS_DRIVER;

    LOG_DRIVER("device feature bits:\n");
    uint32_t feature_low = regs->DeviceFeatures;
    LOG_DRIVER("feature low: 0x%lx\n", feature_low);
    regs->DeviceFeaturesSel = 1;
    uint32_t feature_high = regs->DeviceFeatures;
    LOG_DRIVER("feature high: 0x%lx\n", feature_high);
    uint64_t feature = feature_low | ((uint64_t)feature_high << 32);
    LOG_DRIVER("feature: 0x%lx\n", feature);
    print_feature_bits(feature);
    virtio_print_reserved_feature_bits(feature);

    regs->DriverFeatures = 0;
    regs->DriverFeaturesSel = 1;
    regs->DriverFeatures = 0;

    regs->Status |= VIRTIO_DEVICE_STATUS_FEATURES_OK;

    if (!(regs->Status & VIRTIO_DEVICE_STATUS_FEATURES_OK)) {
        LOG_DRIVER_ERR("device status features is not OK!\n");
        return;
    }

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
    rx_virtq.desc = hw_ring_buffer_vaddr + rx_desc_off;
    rx_virtq.avail = hw_ring_buffer_vaddr + rx_avail_off;
    rx_virtq.used = hw_ring_buffer_vaddr + rx_used_off;
    rx_virtq.desc_free = 0;

    for (int i = 0; i < RX_COUNT; i++) {
        net_buff_desc_t desc;
        LOG_DRIVER("here 1!\n");
        int err = net_dequeue_free(&rx_queue, &desc);
        LOG_DRIVER("here 2!\n");
        assert(!err);

        // TODO: some check for whether desc.len is valid?
        LOG_DRIVER("(%d): addr: 0x%lx, len: 0x%x\n", i, desc.io_or_offset, NET_BUFFER_SIZE);
        rx_virtq.desc[i].addr = desc.io_or_offset;
        rx_virtq.desc[i].len = NET_BUFFER_SIZE;
        rx_virtq.desc[i].flags = VIRTQ_DESC_F_WRITE;
        rx_virtq.avail->idx++;
    }

    tx_virtq.num = TX_COUNT;
    tx_virtq.desc = (struct virtq_desc *)(hw_ring_buffer_vaddr + tx_desc_off);
    tx_virtq.avail = (struct virtq_avail *)(hw_ring_buffer_vaddr + tx_avail_off);
    tx_virtq.used = (struct virtq_used *)(hw_ring_buffer_vaddr + tx_used_off);
    tx_virtq.desc_free = 0;

    // Setup RX queue first
    assert(regs->QueueNumMax >= RX_COUNT);
    regs->QueueSel = VIRTIO_NET_RX_QUEUE;
    regs->QueueNum = RX_COUNT;
    regs->QueueReady = 1;
    regs->QueueDescLow = (hw_ring_buffer_paddr + rx_desc_off) & 0xFFFFFFFF;
    regs->QueueDescHigh = (hw_ring_buffer_paddr + rx_desc_off) >> 32;
    regs->QueueDriverLow = (hw_ring_buffer_paddr + rx_avail_off) & 0xFFFFFFFF;
    regs->QueueDriverHigh = (hw_ring_buffer_paddr + rx_avail_off) >> 32;
    regs->QueueDeviceLow = (hw_ring_buffer_paddr + rx_used_off) & 0xFFFFFFFF;
    regs->QueueDeviceHigh = (hw_ring_buffer_paddr + rx_used_off) >> 32;

    // Setup TX queue
    assert(regs->QueueNumMax >= TX_COUNT);
    regs->QueueSel = VIRTIO_NET_TX_QUEUE;
    regs->QueueNum = TX_COUNT;
    regs->QueueReady = 1;
    regs->QueueDescLow = (hw_ring_buffer_paddr + tx_desc_off) & 0xFFFFFFFF;
    regs->QueueDescHigh = (hw_ring_buffer_paddr + tx_desc_off) >> 32;
    regs->QueueDriverLow = (hw_ring_buffer_paddr + tx_avail_off) & 0xFFFFFFFF;
    regs->QueueDriverHigh = (hw_ring_buffer_paddr + tx_avail_off) >> 32;
    regs->QueueDeviceLow = (hw_ring_buffer_paddr + tx_used_off) & 0xFFFFFFFF;
    regs->QueueDeviceHigh = (hw_ring_buffer_paddr + tx_used_off) >> 32;

    // Set the DRIVER_OK status bit
    regs->Status |= VIRTIO_DEVICE_STATUS_DRIVER_OK;
}

void init(void)
{
    regs = (volatile virtio_mmio_regs_t *) (eth_regs + VIRTIO_MMIO_NET_OFFSET);

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
