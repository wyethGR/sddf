#include <sddf/util/printf.h>

#define DEBUG_DRIVER

#ifdef DEBUG_DRIVER
#define LOG_DRIVER(...) do{ sddf_dprintf("BLK DRIVER|INFO: "); sddf_dprintf(__VA_ARGS__); }while(0)
#else
#define LOG_DRIVER(...) do{}while(0)
#endif

#define LOG_DRIVER_ERR(...) do{ sddf_printf("BLK DRIVER|ERROR: "); sddf_printf(__VA_ARGS__); }while(0)

/* This driver does not support legacy mode */
#define VIRTIO_BLK_DRIVER_VERSION 2

#define VIRTIO_BLK_F_SIZE_MAX   1   /* Indicates maximum segment size */
#define VIRTIO_BLK_F_SEG_MAX    2   /* Indicates maximum # of segments */
#define VIRTIO_BLK_F_GEOMETRY   4   /* Legacy geometry available  */
#define VIRTIO_BLK_F_RO     5   /* Disk is read-only */
#define VIRTIO_BLK_F_BLK_SIZE   6   /* Block size of disk is available*/
#define VIRTIO_BLK_F_TOPOLOGY   10  /* Topology information is available */
#define VIRTIO_BLK_F_MQ     12  /* support more than one vq */
#define VIRTIO_BLK_F_DISCARD    13  /* DISCARD is supported */
#define VIRTIO_BLK_F_WRITE_ZEROES   14  /* WRITE ZEROES is supported */
#define VIRTIO_BLK_F_SECURE_ERASE   16 /* Secure Erase is supported */
#define VIRTIO_BLK_F_ZONED      17  /* Zoned block device */

struct virtio_blk_config {
    uint64_t capacity;
    uint32_t size_max;
    uint32_t seg_max;
    struct virtio_blk_geometry {
        uint16_t cylinders;
        uint8_t heads;
        uint8_t sectors;
    } geometry;
    uint32_t blk_size;
    struct virtio_blk_topology {
        // # of logical blocks per physical block (log2)
        uint8_t physical_block_exp;
        // offset of first aligned logical block
        uint8_t alignment_offset;
        // suggested minimum I/O size in blocks
        uint16_t min_io_size;
        // optimal (suggested maximum) I/O size in blocks
        uint32_t opt_io_size;
    } topology;
    uint8_t writeback;
    uint8_t unused0;
    uint16_t num_queues;
    uint32_t max_discard_sectors;
    uint32_t max_discard_seg;
    uint32_t discard_sector_alignment;
    uint32_t max_write_zeroes_sectors;
    uint32_t max_write_zeroes_seg;
    uint8_t write_zeroes_may_unmap;
    uint8_t unused1[3];
    uint32_t max_secure_erase_sectors;
    uint32_t max_secure_erase_seg;
    uint32_t secure_erase_sector_alignment;
};

static void virtio_blk_print_features(uint64_t features) {
    if (features & ((uint64_t)1 << VIRTIO_BLK_F_SIZE_MAX)) {
        LOG_DRIVER("    VIRTIO_BLK_F_SIZE_MAX\n");
    }
    if (features & ((uint64_t)1 << VIRTIO_BLK_F_SEG_MAX)) {
        LOG_DRIVER("    VIRTIO_BLK_F_SEG_MAX\n");
    }
    if (features & ((uint64_t)1 << VIRTIO_BLK_F_GEOMETRY)) {
        LOG_DRIVER("    VIRTIO_BLK_F_GEOMETRY\n");
    }
    if (features & ((uint64_t)1 << VIRTIO_BLK_F_RO)) {
        LOG_DRIVER("    VIRTIO_BLK_F_RO\n");
    }
    if (features & ((uint64_t)1 << VIRTIO_BLK_F_BLK_SIZE)) {
        LOG_DRIVER("    VIRTIO_BLK_F_BLK_SIZE\n");
    }
    if (features & ((uint64_t)1 << VIRTIO_BLK_F_TOPOLOGY)) {
        LOG_DRIVER("    VIRTIO_BLK_F_TOPOLOGY\n");
    }
    if (features & ((uint64_t)1 << VIRTIO_BLK_F_MQ)) {
        LOG_DRIVER("    VIRTIO_BLK_F_MQ\n");
    }
    if (features & ((uint64_t)1 << VIRTIO_BLK_F_DISCARD)) {
        LOG_DRIVER("    VIRTIO_BLK_F_DISCARD\n");
    }
    if (features & ((uint64_t)1 << VIRTIO_BLK_F_WRITE_ZEROES)) {
        LOG_DRIVER("    VIRTIO_BLK_F_WRITE_ZEROES\n");
    }
    if (features & ((uint64_t)1 << VIRTIO_BLK_F_SECURE_ERASE)) {
        LOG_DRIVER("    VIRTIO_BLK_F_SECURE_ERASE\n");
    }
    if (features & ((uint64_t)1 << VIRTIO_BLK_F_ZONED)) {
        LOG_DRIVER("    VIRTIO_BLK_F_ZONED\n");
    }
    /* The reserved feature bits, that are not device specific, sit in the middle
     * of all the network feature bits, which is why we print them here. */
    virtio_print_reserved_feature_bits(features);
}
