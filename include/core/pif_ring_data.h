// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file pif_ring_data.h
 * @brief Ring buffer of fixed-size records for one writer and one reader.
 *
 * Used for queues of commands, frames and events. The writer adds a record and fills it in place, the reader
 * takes the oldest one.
 */
#ifndef PIF_RING_DATA_H
#define PIF_RING_DATA_H


#include "core/pif.h"


/**
 * @class StPifRingData
 * @brief Queue of up to _data_count records of _data_size bytes.
 */
typedef struct StPifRingData
{
	// Public Member Variable

    // Read-only Member Variable
	PifId _id;	///< Instance ID
    uint16_t _data_size;	///< Bytes of one record
    uint16_t _data_count;	///< Number of records the queue holds

	// Private Member Variable
    volatile uint16_t __head;		///< Moved only by the writer.
    volatile uint16_t __tail;		///< Moved only by the reader.
    uint16_t __index;
    uint16_t __slot_count;
    uint8_t* __p_data;
} PifRingData;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Allocates a queue of data_count records of data_size bytes from the heap.
 * @param id Instance ID, or PIF_ID_AUTO.
 * @param data_size Size of one data item in bytes.
 * @param data_count Maximum number of data items the ring can hold (1 to 65534).
 * @return The new queue, or NULL with pif_error set. Free it with pifRingData_Destroy().
 */
PifRingData* pifRingData_Create(PifId id, uint16_t data_size, uint16_t data_count);

/**
 * @brief Frees a queue made by pifRingData_Create().
 * @param pp_owner Address of the pointer to the queue. It is set to NULL.
 */
void pifRingData_Destroy(PifRingData** pp_owner);

/**
 * @brief Sets up a queue object of the caller's with data_count records of data_size bytes from the heap.
 * @param p_owner Queue to set up.
 * @param id Instance ID, or PIF_ID_AUTO.
 * @param data_size Size of one data item in bytes.
 * @param data_count Maximum number of data items the ring can hold (1 to 65534).
 * @return TRUE on success, FALSE with pif_error set otherwise.
 */
BOOL pifRingData_Init(PifRingData* p_owner, PifId id, uint16_t data_size, uint16_t data_count);

/**
 * @brief Releases the data buffer owned by the instance. Use pifRingData_Reset to only empty the ring.
 * @param p_owner Pointer to the target object instance.
 */
void pifRingData_Clear(PifRingData* p_owner);

/**
 * @brief Discards all stored items and makes the ring empty without releasing the buffer.
 * @param p_owner Pointer to the target object instance.
 */
void pifRingData_Reset(PifRingData* p_owner);

/**
 * @brief Checks whether the ring holds no items.
 * @param p_owner Pointer to the target object instance.
 * @return TRUE if the ring is empty, otherwise FALSE.
 */
BOOL pifRingData_IsEmpty(PifRingData* p_owner);

/**
 * @brief Returns the item at the given position without removing it.
 * @param p_owner Pointer to the target object instance.
 * @param index Zero-based index counted from the oldest item.
 * @return Pointer to the item, or NULL if index is not less than the fill size.
 */
void* pifRingData_GetData(PifRingData* p_owner, uint16_t index);

/**
 * @brief Starts iteration and returns the oldest item without removing it.
 * @details The iteration position is shared by the instance, so iterations cannot be nested
 *          and Add/Remove should not be called while iterating.
 * @param p_owner Pointer to the target object instance.
 * @return Pointer to the oldest item, or NULL if the ring is empty.
 */
void* pifRingData_GetFirstData(PifRingData* p_owner);

/**
 * @brief Returns the next item of the iteration started by pifRingData_GetFirstData.
 * @param p_owner Pointer to the target object instance.
 * @return Pointer to the next item, or NULL when the iteration has reached the end.
 */
void* pifRingData_GetNextData(PifRingData* p_owner);

/**
 * @brief Returns the number of items currently stored.
 * @param p_owner Pointer to the target object instance.
 * @return Number of stored items.
 */
uint16_t pifRingData_GetFillSize(PifRingData* p_owner);

/**
 * @brief Returns the number of items that can still be added.
 * @param p_owner Pointer to the target object instance.
 * @return Number of free item slots.
 */
uint16_t pifRingData_GetRemainSize(PifRingData* p_owner);

/**
 * @brief Reserves a slot for a new item at the end of the ring.
 * @details The returned slot is already counted as stored, so the caller must fill it immediately.
 *          Not safe when Add and Remove run in different contexts (e.g. task and interrupt), since the
 *          reader can take the slot before it is filled. Use pifRingData_BeginAdd() and
 *          pifRingData_CommitAdd() there.
 * @param p_owner Pointer to the target object instance.
 * @return Pointer to the slot to fill, or NULL if the ring is full.
 */
void* pifRingData_Add(PifRingData* p_owner);

/**
 * @brief Returns the slot for a new item at the end of the ring without storing it yet.
 * @details The reader does not see the slot until pifRingData_CommitAdd(), so the writer and the reader
 *          may run in different contexts, e.g. an interrupt and a task, as long as each side has one.
 *          Calling it again before the commit returns the same slot.
 * @param p_owner Pointer to the target object instance.
 * @return Pointer to the slot to fill, or NULL if the ring is full.
 */
void* pifRingData_BeginAdd(PifRingData* p_owner);

/**
 * @brief Stores the slot filled after pifRingData_BeginAdd(), which must have returned it.
 * @param p_owner Pointer to the target object instance.
 */
void pifRingData_CommitAdd(PifRingData* p_owner);

/**
 * @brief Removes the oldest item from the ring.
 * @details The returned pointer stays valid only until the next item is added, which a writer in another
 *          context can do at once. Such a reader takes the item with pifRingData_GetData() at index 0
 *          first, and removes it when it is done with it.
 * @param p_owner Pointer to the target object instance.
 * @return Pointer to the removed item, or NULL if the ring is empty.
 */
void* pifRingData_Remove(PifRingData* p_owner);

#ifdef __cplusplus
}
#endif


#endif	// PIF_RING_DATA_H
