// SPDX-License-Identifier: BSD-3-Clause
#ifndef PIF_RING_DATA_H
#define PIF_RING_DATA_H


#include "core/pif.h"


/**
 * @class StPifRingData
 * @brief Provides a type or declaration used by this module.
 */
typedef struct StPifRingData
{
	// Public Member Variable

    // Read-only Member Variable
	PifId _id;
    uint16_t _data_size;
    uint16_t _data_count;

	// Private Member Variable
    uint16_t __head;
    uint16_t __tail;
    uint16_t __index;
    uint16_t __slot_count;
    uint8_t* __p_data;
} PifRingData;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifRingData_Create
 * @brief Creates and initializes a new ring data instance, then returns its handle when successful.
 * @param id Identifier value for the object or task.
 * @param data_size Size of one data item in bytes.
 * @param data_count Maximum number of data items the ring can hold (1 to 65534).
 * @return Pointer to the resulting object or data, or NULL if unavailable.
 */
PifRingData* pifRingData_Create(PifId id, uint16_t data_size, uint16_t data_count);

/**
 * @fn pifRingData_Destroy
 * @brief Destroys the ring data instance and frees all resources associated with it.
 * @param pp_owner Address of the object pointer to destroy or clear.
 */
void pifRingData_Destroy(PifRingData** pp_owner);

/**
 * @fn pifRingData_Init
 * @brief Initializes the ring data instance and prepares all internal fields for safe use.
 * @param p_owner Pointer to the target object instance.
 * @param id Identifier value for the object or task.
 * @param data_size Size of one data item in bytes.
 * @param data_count Maximum number of data items the ring can hold (1 to 65534).
 * @return TRUE on success, otherwise FALSE.
 */
BOOL pifRingData_Init(PifRingData* p_owner, PifId id, uint16_t data_size, uint16_t data_count);

/**
 * @fn pifRingData_Clear
 * @brief Releases the data buffer owned by the instance. Use pifRingData_Reset to only empty the ring.
 * @param p_owner Pointer to the target object instance.
 */
void pifRingData_Clear(PifRingData* p_owner);

/**
 * @fn pifRingData_Reset
 * @brief Discards all stored items and makes the ring empty without releasing the buffer.
 * @param p_owner Pointer to the target object instance.
 */
void pifRingData_Reset(PifRingData* p_owner);

/**
 * @fn pifRingData_IsEmpty
 * @brief Checks whether the ring holds no items.
 * @param p_owner Pointer to the target object instance.
 * @return TRUE if the ring is empty, otherwise FALSE.
 */
BOOL pifRingData_IsEmpty(PifRingData* p_owner);

/**
 * @fn pifRingData_GetData
 * @brief Returns the item at the given position without removing it.
 * @param p_owner Pointer to the target object instance.
 * @param index Zero-based index counted from the oldest item.
 * @return Pointer to the item, or NULL if index is not less than the fill size.
 */
void* pifRingData_GetData(PifRingData* p_owner, uint16_t index);

/**
 * @fn pifRingData_GetFirstData
 * @brief Starts iteration and returns the oldest item without removing it.
 * @details The iteration position is shared by the instance, so iterations cannot be nested
 *          and Add/Remove should not be called while iterating.
 * @param p_owner Pointer to the target object instance.
 * @return Pointer to the oldest item, or NULL if the ring is empty.
 */
void* pifRingData_GetFirstData(PifRingData* p_owner);

/**
 * @fn pifRingData_GetNextData
 * @brief Returns the next item of the iteration started by pifRingData_GetFirstData.
 * @param p_owner Pointer to the target object instance.
 * @return Pointer to the next item, or NULL when the iteration has reached the end.
 */
void* pifRingData_GetNextData(PifRingData* p_owner);

/**
 * @fn pifRingData_GetFillSize
 * @brief Returns the number of items currently stored.
 * @param p_owner Pointer to the target object instance.
 * @return Number of stored items.
 */
uint16_t pifRingData_GetFillSize(PifRingData* p_owner);

/**
 * @fn pifRingData_GetRemainSize
 * @brief Returns the number of items that can still be added.
 * @param p_owner Pointer to the target object instance.
 * @return Number of free item slots.
 */
uint16_t pifRingData_GetRemainSize(PifRingData* p_owner);

/**
 * @fn pifRingData_Add
 * @brief Reserves a slot for a new item at the end of the ring.
 * @details The returned slot is already counted as stored, so the caller must fill it immediately.
 *          Not safe when Add and Remove run in different contexts (e.g. task and interrupt).
 * @param p_owner Pointer to the target object instance.
 * @return Pointer to the slot to fill, or NULL if the ring is full.
 */
void* pifRingData_Add(PifRingData* p_owner);

/**
 * @fn pifRingData_Remove
 * @brief Removes the oldest item from the ring.
 * @details The returned pointer stays valid only until the next pifRingData_Add call.
 * @param p_owner Pointer to the target object instance.
 * @return Pointer to the removed item, or NULL if the ring is empty.
 */
void* pifRingData_Remove(PifRingData* p_owner);

#ifdef __cplusplus
}
#endif


#endif	// PIF_RING_DATA_H
