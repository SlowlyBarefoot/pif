#ifndef PIF_RING_DATA_H
#define PIF_RING_DATA_H


#include "core/pif.h"


/**
 * @class StPifRingData
 * @brief
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
 * @brief
 * @param id
 * @param data_size
 * @param data_count
 * @return
 */
PifRingData* pifRingData_Create(PifId id, uint16_t data_size, uint16_t data_count);

/**
 * @fn pifRingData_Destroy
 * @brief
 * @param pp_owner
 */
void pifRingData_Destroy(PifRingData** pp_owner);

/**
 * @fn pifRingData_Init
 * @brief
 * @param p_owner
 * @param id
 * @param data_size
 * @param data_count
 * @return
 */
BOOL pifRingData_Init(PifRingData* p_owner, PifId id, uint16_t data_size, uint16_t data_count);

/**
 * @fn pifRingData_Clear
 * @brief
 * @param p_owner
 */
void pifRingData_Clear(PifRingData* p_owner);

/**
 * @fn pifRingData_IsEmpty
 * @brief
 * @param p_owner
 * @return
 */
BOOL pifRingData_IsEmpty(PifRingData* p_owner);

/**
 * @fn pifRingData_GetData
 * @brief
 * @param p_owner
 * @param index
 * @return
 */
void* pifRingData_GetData(PifRingData* p_owner, uint16_t index);

/**
 * @fn pifRingData_GetFirstData
 * @brief
 * @param p_owner
 * @return
 */
void* pifRingData_GetFirstData(PifRingData* p_owner);

/**
 * @fn pifRingData_GetNextData
 * @brief
 * @param p_owner
 * @return
 */
void* pifRingData_GetNextData(PifRingData* p_owner);

/**
 * @fn pifRingData_GetFillSize
 * @brief
 * @param p_owner
 * @return
 */
uint16_t pifRingData_GetFillSize(PifRingData* p_owner);

/**
 * @fn pifRingData_GetRemainSize
 * @brief
 * @param p_owner
 * @return
 */
uint16_t pifRingData_GetRemainSize(PifRingData* p_owner);

/**
 * @fn pifRingData_Add
 * @brief
 * @param p_owner
 * @return
 */
void* pifRingData_Add(PifRingData* p_owner);

/**
 * @fn pifRingData_Remove
 * @brief
 * @param p_owner
 * @return
 */
void* pifRingData_Remove(PifRingData* p_owner);

#ifdef __cplusplus
}
#endif


#endif	// PIF_RING_DATA_H
