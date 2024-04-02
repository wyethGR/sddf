/*
 * Copyright 2023, UNSW
 * SPDX-License-Identifier: BSD-2-Clause
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

static void rx_provide()
{
    bool reprocess = true;
    while (reprocess) {
        while (!hw_ring_full(&rx, RX_COUNT) && !net_queue_empty_free(&rx_queue)) {
            net_buff_desc_t buffer;
            int err = net_dequeue_free(&rx_queue, &buffer);
            assert(!err);

            // We now need to populate the header, as well as the packet

            // uint32_t cntl = (MAX_RX_FRAME_SZ << DESC_RXCTRL_SIZE1SHFT) & DESC_RXCTRL_SIZE1MASK;
            // if (rx.tail + 1 == RX_COUNT) cntl |= DESC_RXCTRL_RXRINGEND;

            // rx.descr_mdata[rx.tail] = buffer;
            // update_ring_slot(&rx, rx.tail, DESC_RXSTS_OWNBYDMA, cntl, buffer.io_or_offset, 0);
            // eth_dma->rxpolldemand = POLL_DATA;

            // rx.tail = (rx.tail + 1) % RX_COUNT;
        }

        net_request_signal_free(&rx_queue);
        reprocess = false;

        if (!net_queue_empty_free(&rx_queue) && !hw_ring_full(&rx, RX_COUNT)) {
            net_cancel_signal_free(&rx_queue);
            reprocess = true;
        }
    }
}

static void rx_return(void)
{
}

static void tx_provide(void)
{
}

static void tx_return(void)
{
}

static void handle_irq()
{
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
    volatile virtio_mmio_regs_t *regs = (virtio_mmio_regs_t *) (eth_regs + VIRTIO_MMIO_NET_OFFSET);

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
    uint64_t feature = feature_low | ((uint64_t)feature_high << 32);
    LOG_DRIVER("feature: 0x%lx\n", feature);
    print_feature_bits(feature);

    regs->DriverFeatures = 0;
    regs->DriverFeaturesSel = 1;
    regs->DriverFeatures = 0;

    regs->Status |= VIRTIO_DEVICE_STATUS_FEATURES_OK;

    if (!(regs->Status & VIRTIO_DEVICE_STATUS_FEATURES_OK)) {
        LOG_DRIVER_ERR("device status features is not OK!\n");
        return;
    }

    rx_provide();
    tx_provide();

    // Setup the virtqueues

    // TODO: need to have asserts regarding alignment for each desc,avail,used queue

    uintptr_t rx_desc_off = 0;
    uintptr_t rx_avail_off = rx_desc_off + (16 * RX_COUNT);
    uintptr_t rx_used_off = rx_avail_off + (6 + 2 * RX_COUNT);
    uintptr_t tx_desc_off = rx_used_off + (6 + 8 * RX_COUNT);
    uintptr_t tx_avail_off = tx_desc_off + (16 * TX_COUNT);
    uintptr_t tx_used_off = tx_avail_off + (6 + 2 * TX_COUNT);
    uintptr_t size = tx_used_off + (6 + 8 * TX_COUNT);

    assert(size <= HW_RING_SIZE);

    rx_virtq.num = RX_COUNT;
    rx_virtq.desc = hw_ring_buffer_vaddr + rx_desc_off;
    rx_virtq.avail = hw_ring_buffer_vaddr + rx_avail_off;
    rx_virtq.used = hw_ring_buffer_vaddr + rx_used_off;

    for (int i = 0; i < RX_COUNT; i += 2) {
        net_buff_desc_t desc;
        int err = net_dequeue_free(&rx_queue, &desc);
        assert(!err);

        size_t virtio_desc_header = i;
        size_t virtio_desc_packet = i + 1;

        virtio_net_hdr_t *header = virtio_net_headers[virtio_desc_header];
        header->packet = desc.io_or_offset;

        rx_virtq->desc[virtio_desc_header].addr = virtio_net_headers_paddr + sizeof(virtio_net_hdr_t) * virtio_desc_header;
        rx_virtq->desc[virtio_desc_header].len = sizeof(virtio_net_hdr_t);
        rx_virtq->desc[virtio_desc_header].flags = VIRTQ_DESC_F_WRITE | VIRTQ_DESC_F_NEXT;
        rx_virtq->desc[virtio_desc_header].next = virtio_desc_packet;

        // TODO: some check for whether desc.len is valid?
        rx_virtq->desc[virtio_desc_packet].addr = desc.addr;
        rx_virtq->desc[virtio_desc_packet].len = desc.len;
        rx_virtq->desc[virtio_desc_packet].flags = VIRTQ_DESC_F_WRITE;
    }

    tx_virtq.num = TX_COUNT;
    tx_virtq.desc = hw_ring_buffer_vaddr + tx_desc_off;
    tx_virtq.avail = hw_ring_buffer_vaddr + tx_avail_off;
    tx_virtq.used = hw_ring_buffer_vaddr + tx_used_off;

    // Setup RX queue first
    regs->QueueSel = 0;
    // regs->QueueNumMax = RX_COUNT;
    regs->QueueNum = RX_COUNT;
    regs->QueueReady = 1;
    regs->QueueDescLow = (hw_ring_buffer_paddr + rx_desc_off) & 0xFFFFFFFF;
    regs->QueueDescHigh = (hw_ring_buffer_paddr + rx_desc_off) >> 32;
    regs->QueueDriverLow = (hw_ring_buffer_paddr + rx_avail_off) & 0xFFFFFFFF;
    regs->QueueDriverHigh = (hw_ring_buffer_paddr + rx_avail_off) >> 32;
    regs->QueueDeviceLow = (hw_ring_buffer_paddr + rx_used_off) & 0xFFFFFFFF;
    regs->QueueDeviceHigh = (hw_ring_buffer_paddr + rx_used_off) >> 32;

    // Setup TX queue
    regs->QueueSel = 1;
    // regs->QueueNumMax = TX_COUNT;
    regs->QueueNum = TX_COUNT;
    regs->QueueReady = 1;
    regs->QueueDescLow = (hw_ring_buffer_paddr + tx_desc_off) & 0xFFFFFFFF;
    regs->QueueDescHigh = (hw_ring_buffer_paddr + tx_desc_off) >> 32;
    regs->QueueDriverLow = (hw_ring_buffer_paddr + tx_avail_off) & 0xFFFFFFFF;
    regs->QueueDriverHigh = (hw_ring_buffer_paddr + tx_avail_off) >> 32;
    regs->QueueDeviceLow = (hw_ring_buffer_paddr + tx_used_off) & 0xFFFFFFFF;
    regs->QueueDeviceHigh = (hw_ring_buffer_paddr + tx_used_off) >> 32;

    // Set the DRIVER_OK status bit
    regs->Status |= DRIVER_OK;
}

void init(void)
{
    eth_setup();

    net_queue_init(&rx_queue, (net_queue_t *)rx_free, (net_queue_t *)rx_active, RX_QUEUE_SIZE_DRIV);
    net_queue_init(&tx_queue, (net_queue_t *)tx_free, (net_queue_t *)tx_active, TX_QUEUE_SIZE_DRIV);

    rx_provide();
    tx_provide();

    microkit_irq_ack(IRQ_CH);
}

void notified(microkit_channel ch)
{
    switch(ch) {
        case IRQ_CH:
            handle_irq();
            microkit_irq_ack_delayed(ch);
            break;
        case RX_CH:
            rx_provide();
            break;
        case TX_CH:
            tx_provide();
            break;
        default:
            LOG_DRIVER_ERR("received notification on unexpected channel %u\n", ch);
            break;
    }
}
