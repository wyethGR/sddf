#include <microkit.h>
#include <sddf/util/util.h>
#include <sddf/blk/queue.h>

#define QUEUE_SIZE 128
#define VIRT_CH 0

uintptr_t blk_config;
uintptr_t blk_request;
uintptr_t blk_response;
uintptr_t blk_data;

static blk_queue_handle_t blk_queue;

void init(void) {
    blk_queue_init(&blk_queue, (blk_req_queue_t *)blk_request, (blk_resp_queue_t *)blk_response, QUEUE_SIZE);

    int err = blk_enqueue_req(&blk_queue, READ_BLOCKS, 0, 0, 1, 1);
    assert(!err);
    microkit_notify(VIRT_CH);
}

void notified(microkit_channel ch) {
}
