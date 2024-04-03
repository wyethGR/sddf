#include <stdint.h>
#include <sddf/util/printf.h>

#define DEBUG_DRIVER

#ifdef DEBUG_DRIVER
#define LOG_DRIVER(...) do{ sddf_dprintf("ETH DRIVER|INFO: "); sddf_dprintf(__VA_ARGS__); }while(0)
#else
#define LOG_DRIVER(...) do{}while(0)
#endif

#define LOG_DRIVER_ERR(...) do{ sddf_printf("ETH DRIVER|ERROR: "); sddf_printf(__VA_ARGS__); }while(0)

#define VIRTIO_NET_RX_QUEUE 0
#define VIRTIO_NET_TX_QUEUE 1

/* The feature bitmap for virtio net */
#define VIRTIO_NET_F_CSUM   0   /* Host handles pkts w/ partial csum */
#define VIRTIO_NET_F_GUEST_CSUM 1   /* Guest handles pkts w/ partial csum */
#define VIRTIO_NET_F_CTRL_GUEST_OFFLOADS 2 /* Dynamic offload configuration. */
#define VIRTIO_NET_F_MTU    3   /* Initial MTU advice */
#define VIRTIO_NET_F_MAC    5   /* Host has given MAC address. */
#define VIRTIO_NET_F_GUEST_TSO4 7   /* Guest can handle TSOv4 in. */
#define VIRTIO_NET_F_GUEST_TSO6 8   /* Guest can handle TSOv6 in. */
#define VIRTIO_NET_F_GUEST_ECN  9   /* Guest can handle TSO[6] w/ ECN in. */
#define VIRTIO_NET_F_GUEST_UFO  10  /* Guest can handle UFO in. */
#define VIRTIO_NET_F_HOST_TSO4  11  /* Host can handle TSOv4 in. */
#define VIRTIO_NET_F_HOST_TSO6  12  /* Host can handle TSOv6 in. */
#define VIRTIO_NET_F_HOST_ECN   13  /* Host can handle TSO[6] w/ ECN in. */
#define VIRTIO_NET_F_HOST_UFO   14  /* Host can handle UFO in. */
#define VIRTIO_NET_F_MRG_RXBUF  15  /* Host can merge receive buffers. */
#define VIRTIO_NET_F_STATUS 16  /* virtio_net_config.status available */
#define VIRTIO_NET_F_CTRL_VQ    17  /* Control channel available */
#define VIRTIO_NET_F_CTRL_RX    18  /* Control channel RX mode support */
#define VIRTIO_NET_F_CTRL_VLAN  19  /* Control channel VLAN filtering */
#define VIRTIO_NET_F_CTRL_RX_EXTRA 20   /* Extra RX mode control support */
#define VIRTIO_NET_F_GUEST_ANNOUNCE 21  /* Guest can announce device on the
                     * network */
#define VIRTIO_NET_F_MQ 22  /* Device supports Receive Flow
                     * Steering */
#define VIRTIO_NET_F_CTRL_MAC_ADDR 23   /* Set MAC address */
#define VIRTIO_NET_F_HOST_USO   56  /* Host can handle USO in. */
#define VIRTIO_NET_F_HASH_REPORT  57    /* Supports hash report */
#define VIRTIO_NET_F_GUEST_HDRLEN  59   /* Guest provides the exact hdr_len value. */
#define VIRTIO_NET_F_RSS      60    /* Supports RSS RX steering */
#define VIRTIO_NET_F_RSC_EXT      61    /* extended coalescing info */
#define VIRTIO_NET_F_STANDBY      62    /* Act as standby for another device
                     * with the same MAC.
                     */
#define VIRTIO_NET_F_SPEED_DUPLEX 63    /* Device set linkspeed and duplex */

#define VIRTIO_NET_S_LINK_UP 1
#define VIRTIO_NET_S_ANNOUNCE 2

#define VIRTIO_NET_HDR_GSO_NONE 0

typedef struct virtio_net_config {
    uint8_t mac[6];
    uint16_t status;
    uint16_t max_virtqueue_pairs;
    uint16_t mtu;
    uint32_t speed;
    uint8_t duplex;
    uint8_t rss_max_key_size;
    uint16_t rss_max_indirection_table_length;
    uint32_t supported_hash_types;
} virtio_net_config_t;

typedef struct virtio_net_hdr {
    /* See VIRTIO_NET_HDR_F_* */
    uint8_t flags;
    /* See VIRTIO_NET_HDR_GSO_* */
    uint8_t gso_type;
    uint16_t hdr_len;     /* Ethernet + IP + tcp/udp hdrs */
    uint16_t gso_size;        /* Bytes to append to hdr_len per frame */
    uint16_t csum_start;  /* Position to start checksumming from */
    uint16_t csum_offset; /* Offset after that to place checksum */
    // uint16_t num_buffers;
} virtio_net_hdr_t;


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
    if (features & ((uint64_t)1 << VIRTIO_NET_F_MAC)) {
        LOG_DRIVER("    VIRTIO_NET_F_MAC\n");
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
    if (features & ((uint64_t)1 << VIRTIO_NET_F_HOST_ECN)) {
        LOG_DRIVER("    VIRTIO_NET_F_HOST_ECN\n");
    }
    if (features & ((uint64_t)1 << VIRTIO_NET_F_HOST_UFO)) {
        LOG_DRIVER("    VIRTIO_NET_F_HOST_UFO\n");
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

