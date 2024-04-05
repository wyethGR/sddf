#include <microkit.h>
#include <sddf/util/util.h>
#include <sddf/virtio/virtio.h>
#include <sddf/virtio/virtio_queue.h>
#include <sddf/blk/queue.h>
#include "block.h"

/* TODO: this depends on QEMU/the hypervisor */
#ifndef VIRTIO_MMIO_BLK_OFFSET
#define VIRTIO_MMIO_BLK_OFFSET (0xe00)
#endif

#define VIRTQ_NUM_REQUESTS 128
#define REQUESTS_REGION_SIZE 0x200000

uintptr_t blk_regs;
uintptr_t requests_vaddr;
uintptr_t requests_paddr;

static volatile virtio_mmio_regs_t *regs;

static struct virtq virtq;

void blk_init(void) {
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
    blk_init();
}

void notified(microkit_channel ch) {
}
