#ifndef PIF_RING_DATA_H
#define PIF_RING_DATA_H


#include "pif.h"


/**
 * @class _PIF_stRingData
 * @brief
 */
typedef struct _PIF_stRingData
{
	// Public Member Variable

    // Read-only Member Variable
	PifId _usPifId;
    uint16_t _usDataSize;
    uint16_t _usDataCount;

	// Private Member Variable
    uint16_t __usHead;
    uint16_t __usTail;
    uint16_t __usIndex;
    void *__pvData;
} PIF_stRingData;


#ifdef __cplusplus
extern "C" {
#endif

PIF_stRingData *pifRingData_Create(PifId usPifId, uint16_t usDataSize, uint16_t usDataCount);
void pifRingData_Destroy(PIF_stRingData** pp_owner);

BOOL pifRingData_IsEmpty(PIF_stRingData *pstOwner);

void *pifRingData_GetData(PIF_stRingData *pstOwner, uint16_t usIndex);
void *pifRingData_GetFirstData(PIF_stRingData *pstOwner);
void *pifRingData_GetNextData(PIF_stRingData *pstOwner);
uint16_t pifRingData_GetFillSize(PIF_stRingData *pstOwner);
uint16_t pifRingData_GetRemainSize(PIF_stRingData *pstOwner);

void *pifRingData_Add(PIF_stRingData *pstOwner);
void *pifRingData_Remove(PIF_stRingData *pstOwner);

#ifdef __cplusplus
}
#endif


#endif	// PIF_RING_DATA_H
