#ifndef PIF_RING_DATA_H
#define PIF_RING_DATA_H


#include "pif.h"


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

PifRingData* pifRingData_Create(PifId id, uint16_t data_size, uint16_t data_count);
void pifRingData_Destroy(PifRingData** pp_owner);

BOOL pifRingData_IsEmpty(PifRingData* p_owner);

void* pifRingData_GetData(PifRingData* p_owner, uint16_t index);
void* pifRingData_GetFirstData(PifRingData* p_owner);
void* pifRingData_GetNextData(PifRingData* p_owner);
uint16_t pifRingData_GetFillSize(PifRingData* p_owner);
uint16_t pifRingData_GetRemainSize(PifRingData* p_owner);

void* pifRingData_Add(PifRingData* p_owner);
void* pifRingData_Remove(PifRingData* p_owner);

#ifdef __cplusplus
}
#endif


#endif	// PIF_RING_DATA_H
