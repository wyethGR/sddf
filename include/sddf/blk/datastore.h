#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/**
 * This file provides a "datastore" implementation that stores any data type in an array
 * and provides an ID to the caller to retrieve the data later. The implementation
 * uses a linked list to keep track of free storage slots.
 */

typedef struct datastore {
    void *storage; /* array of custom data to be stored */
    size_t data_type_size; /* size in bytes of storage data type */
    uint64_t *nextfree; /* linked list pointing to next free slot */
    bool *used; /* array indicating whether storage index is occupied */
    uint64_t size; /* length of storage, nextfree, used arrays */
    uint64_t head; /* index of first free slot in storage */
    uint64_t tail; /* index of last free slot in storage */
    uint64_t num_free; /* number of free slots in storage */
} datastore_t;

/**
 * Check if the data store is full.
 * 
 * @param rs pointer to the data store struct.
 * @return true if data store is full, false otherwise.
 */
static inline bool datastore_full(datastore_t *rs)
{
    return rs->num_free == 0;
}

/**
 * Allocate a storage slot for a new data entry by copying what
 * the data pointer points to into the storage array.
 * 
 * @param ds pointer to the data store struct.
 * @param data pointer to data to store in a storage slot 
 * @param id pointer to data ID allocated
 * @return 0 on success, -1 if storage is full
 */
int datastore_alloc(datastore_t *ds, void *data, uint64_t *id);

/**
 * Retrieve and free a storage slot.
 * 
 * @param ds pointer to the data store struct.
 * @param id data ID to be freed
 * @param data pointer to data to be retrieved
 * @return 0 on success, -1 if data ID is invalid
 */
int datastore_retrieve(datastore_t *ds, uint64_t id, void *data);

/**
 * Initialise the data store.
 * 
 * @param ds pointer to the data store struct.
 * @param storage pointer to the storage array.
 * @param data_type_size size in bytes of storage array.
 * @param nextfree pointer to the linked list array storing the next free storage slot.
 * @param used pointer to the used array indicating whether storage index is occupied.
 * @param size length of storage, nextfree, used arrays.
 */
void datastore_init(datastore_t *ds, void *storage, size_t data_type_size, uint64_t *nextfree, bool *used, uint64_t size);