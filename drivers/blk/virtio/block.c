#include <microkit.h>
#include <sddf/util/util.h>
#include <sddf/virtio/virtio.h>
#include <sddf/virtio/virtio_queue.h>
#include <sddf/blk/queue.h>
#include "block.h"

#define IRQ_CH 0
#define VIRT_CH 1

/* TODO: this depends on QEMU/the hypervisor */
#ifndef VIRTIO_MMIO_BLK_OFFSET
#define VIRTIO_MMIO_BLK_OFFSET (0xe00)
#endif

#define QUEUE_SIZE 128
#define VIRTQ_NUM_REQUESTS QUEUE_SIZE
#define REQUESTS_REGION_SIZE 0x200000

uintptr_t blk_regs;
uintptr_t requests_vaddr;
uintptr_t requests_paddr;

uintptr_t blk_config;
uintptr_t blk_request;
uintptr_t blk_response;
uintptr_t blk_data;

static volatile virtio_mmio_regs_t *regs;

static struct virtq virtq;
static blk_queue_handle_t blk_queue;

void handle_irq() {
    uint32_t irq_status = regs->InterruptStatus;
    if (irq_status & VIRTIO_MMIO_IRQ_VQUEUE) {
        // TODO:
        // We have handled the used buffer notification
        regs->InterruptACK = VIRTIO_MMIO_IRQ_VQUEUE;
    }

    if (irq_status & VIRTIO_MMIO_IRQ_CONFIG) {
        LOG_DRIVER_ERR("unexpected change in configuration\n");
    }
}

void handle_request() {
    /* Consume all requests and put them in the 'avail' ring of the virtq. */
    while (!blk_req_queue_empty(&blk_queue)) {
        blk_request_code_t req_code;
        uintptr_t addr;
        uint32_t block_number;
        uint16_t count;
        uint32_t id;
        int err = blk_dequeue_req(&blk_queue, &req_code, &addr, &block_number, &count, &id);
        assert(!err);

        switch (req_code) {
            case READ_BLOCKS:
                LOG_DRIVER("handling read request with addr 0x%lx, block_number: 0x%x, count: 0x%x, id: 0x%x\n", addr, block_number, count, id);
                break;
            case WRITE_BLOCKS:
                break;
            default:
                LOG_DRIVER_ERR("unsupported request code: 0x%x\n", req_code);
                // TODO: handle
                break;
        }
    }
}

void virtio_blk_init(void) {
    // Do MMIO device init (section 4.2.3.1)
    if (!virtio_mmio_check_magic(regs)) {
        LOG_DRIVER_ERR("invalid virtIO magic value!\n");
        return;
    }

    if (virtio_mmio_version(regs) != VIRTIO_VERSION) {
        LOG_DRIVER_ERR("not correct virtIO version!\n");
        return;
    }

    if (!virtio_mmio_check_device_id(regs, VIRTIO_DEVICE_ID_BLK)) {
        LOG_DRIVER_ERR("not a virtIO block device!\n");
        return;
    }

    if (virtio_mmio_version(regs) != VIRTIO_BLK_DRIVER_VERSION) {
        LOG_DRIVER_ERR("driver does not support given virtIO version: 0x%x\n", virtio_mmio_version(regs));
    }

    /* First reset the device */
    regs->Status = 0;
    /* Set the ACKNOWLEDGE bit to say we have noticed the device */
    regs->Status = VIRTIO_DEVICE_STATUS_ACKNOWLEDGE;
    /* Set the DRIVER bit to say we know how to drive the device */
    regs->Status = VIRTIO_DEVICE_STATUS_DRIVER;

    // TODO: print config
    volatile struct virtio_blk_config *config = (volatile struct virtio_blk_config *)regs->Config;
    // virtio_blk_print_config(config);

    LOG_DRIVER("config->capacity: 0x%lx\n", config->capacity);
    // TODO: print features
    uint32_t features_low = regs->DeviceFeatures;
    regs->DeviceFeaturesSel = 1;
    uint32_t features_high = regs->DeviceFeatures;
    uint64_t features = features_low | ((uint64_t)features_high << 32);
    virtio_blk_print_features(features);
    /* Select features we want from the device */
    regs->DriverFeatures = 0;
    regs->DriverFeaturesSel = 1;
    regs->DriverFeatures = 0;

    regs->Status |= VIRTIO_DEVICE_STATUS_FEATURES_OK;
    if (!(regs->Status & VIRTIO_DEVICE_STATUS_FEATURES_OK)) {
        LOG_DRIVER_ERR("device status features is not OK!\n");
        return;
    }

    /* Add virtqueues */
    size_t desc_off = 0;
    size_t avail_off = desc_off + (16 * VIRTQ_NUM_REQUESTS);
    size_t used_off = avail_off + (6 + 2 * VIRTQ_NUM_REQUESTS);
    size_t size = used_off + (6 + 8 * VIRTQ_NUM_REQUESTS);

    assert(size <= REQUESTS_REGION_SIZE);

    virtq.num = VIRTQ_NUM_REQUESTS;
    virtq.desc = (struct virtq_desc *)(requests_vaddr + desc_off);
    virtq.avail = (struct virtq_avail *)(requests_vaddr + avail_off);
    virtq.used = (struct virtq_used *)(requests_vaddr + used_off);

    assert(regs->QueueNumMax >= VIRTQ_NUM_REQUESTS);
    regs->QueueSel = 0;
    regs->QueueNum = VIRTQ_NUM_REQUESTS;
    regs->QueueDescLow = (requests_paddr + desc_off) & 0xFFFFFFFF;
    regs->QueueDescHigh = (requests_paddr + desc_off) >> 32;
    regs->QueueDriverLow = (requests_paddr + avail_off) & 0xFFFFFFFF;
    regs->QueueDriverHigh = (requests_paddr + avail_off) >> 32;
    regs->QueueDeviceLow = (requests_paddr + used_off) & 0xFFFFFFFF;
    regs->QueueDeviceHigh = (requests_paddr + used_off) >> 32;
    regs->QueueReady = 1;

    /* Finish initialisation */
    regs->Status |= VIRTIO_DEVICE_STATUS_DRIVER_OK;
    regs->InterruptACK = VIRTIO_MMIO_IRQ_VQUEUE;
}

void init(void) {
    regs = (volatile virtio_mmio_regs_t *) (blk_regs + VIRTIO_MMIO_BLK_OFFSET);
    virtio_blk_init();

    blk_queue_init(&blk_queue, (blk_req_queue_t *)blk_request, (blk_resp_queue_t *)blk_response, QUEUE_SIZE);
}

void notified(microkit_channel ch) {
    switch (ch) {
        case IRQ_CH:
            handle_irq();
            microkit_irq_ack_delayed(ch);
            break;
        case VIRT_CH:
            handle_request();
            break;
        default:
            LOG_DRIVER_ERR("received notification from unknown channel: 0x%x\n", ch);
    }
}
