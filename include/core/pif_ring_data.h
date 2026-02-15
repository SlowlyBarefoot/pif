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
 * @param data_count Number of data items in the ring.
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
 * @param data_count Number of data items in the ring.
 * @return TRUE on success, otherwise FALSE.
 */
BOOL pifRingData_Init(PifRingData* p_owner, PifId id, uint16_t data_size, uint16_t data_count);

/**
 * @fn pifRingData_Clear
 * @brief Clears the ring data state and releases resources currently owned by the instance.
 * @param p_owner Pointer to the target object instance.
 */
void pifRingData_Clear(PifRingData* p_owner);

/**
 * @fn pifRingData_IsEmpty
 * @brief Checks whether the ring data currently satisfies the requested condition.
 * @param p_owner Pointer to the target object instance.
 * @return TRUE on success, otherwise FALSE.
 */
BOOL pifRingData_IsEmpty(PifRingData* p_owner);

/**
 * @fn pifRingData_GetData
 * @brief Retrieves the requested value or pointer from the ring data without changing ownership.
 * @param p_owner Pointer to the target object instance.
 * @param index Zero-based index of the target item.
 * @return Pointer to the resulting object or data, or NULL if unavailable.
 */
void* pifRingData_GetData(PifRingData* p_owner, uint16_t index);

/**
 * @fn pifRingData_GetFirstData
 * @brief Retrieves the requested value or pointer from the ring data without changing ownership.
 * @param p_owner Pointer to the target object instance.
 * @return Pointer to the resulting object or data, or NULL if unavailable.
 */
void* pifRingData_GetFirstData(PifRingData* p_owner);

/**
 * @fn pifRingData_GetNextData
 * @brief Retrieves the requested value or pointer from the ring data without changing ownership.
 * @param p_owner Pointer to the target object instance.
 * @return Pointer to the resulting object or data, or NULL if unavailable.
 */
void* pifRingData_GetNextData(PifRingData* p_owner);

/**
 * @fn pifRingData_GetFillSize
 * @brief Retrieves the requested value or pointer from the ring data without changing ownership.
 * @param p_owner Pointer to the target object instance.
 * @return Result value returned by this API.
 */
uint16_t pifRingData_GetFillSize(PifRingData* p_owner);

/**
 * @fn pifRingData_GetRemainSize
 * @brief Retrieves the requested value or pointer from the ring data without changing ownership.
 * @param p_owner Pointer to the target object instance.
 * @return Result value returned by this API.
 */
uint16_t pifRingData_GetRemainSize(PifRingData* p_owner);

/**
 * @fn pifRingData_Add
 * @brief Adds an item to the ring data and updates internal bookkeeping for subsequent operations.
 * @param p_owner Pointer to the target object instance.
 * @return Pointer to the resulting object or data, or NULL if unavailable.
 */
void* pifRingData_Add(PifRingData* p_owner);

/**
 * @fn pifRingData_Remove
 * @brief Removes an item from the ring data and updates internal bookkeeping for consistency.
 * @param p_owner Pointer to the target object instance.
 * @return Pointer to the resulting object or data, or NULL if unavailable.
 */
void* pifRingData_Remove(PifRingData* p_owner);

#ifdef __cplusplus
}
#endif


#endif	// PIF_RING_DATA_H
