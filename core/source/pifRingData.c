#ifndef __PIF_NO_LOG__
#include "pifLog.h"
#endif
#include "pifRingData.h"


/**
 * @fn pifRingData_Create
 * @brief
 * @param id
 * @param data_size
 * @param data_count
 * @return
 */
PifRingData* pifRingData_Create(PifId id, uint16_t data_size, uint16_t data_count)
{
	PifRingData* p_owner;

	p_owner = calloc(sizeof(PifRingData) + data_size * data_count, 1);
	if (!p_owner) {
		pif_error = E_OUT_OF_HEAP;
	    return NULL;
	}

	if (id == PIF_ID_AUTO) id = pif_id++;
    p_owner->_id = id;
    p_owner->_data_size = data_size;
    p_owner->_data_count = data_count;
    p_owner->__head = 0;
    p_owner->__tail = 0;
    p_owner->__p_data = (uint8_t*)p_owner + sizeof(PifRingData);
    return p_owner;
}

/**
 * @fn pifRingData_Destroy
 * @brief
 * @param pp_owner
 */
void pifRingData_Destroy(PifRingData** pp_owner)
{
	if (*pp_owner) {
        free(*pp_owner);
        *pp_owner = NULL;
    }
}

/**
 * @fn pifRingData_IsEmpty
 * @brief
 * @param p_owner
 * @return
 */
BOOL pifRingData_IsEmpty(PifRingData* p_owner)
{
	return p_owner->__head == p_owner->__tail;
}

/**
 * @fn pifRingData_GetData
 * @brief
 * @param p_owner
 * @param index
 * @return
 */
void* pifRingData_GetData(PifRingData* p_owner, uint16_t index)
{
	return p_owner->__p_data + (index * p_owner->_data_size);
}

/**
 * @fn pifRingData_GetFirstData
 * @brief
 * @param p_owner
 * @return
 */
void* pifRingData_GetFirstData(PifRingData* p_owner)
{
	if (p_owner->__head == p_owner->__tail) return NULL;
	p_owner->__index = p_owner->__tail;
	return p_owner->__p_data + (p_owner->__index * p_owner->_data_size);
}

/**
 * @fn pifRingData_GetNextData
 * @brief
 * @param p_owner
 * @return
 */
void* pifRingData_GetNextData(PifRingData* p_owner)
{
	p_owner->__index++;
	if (p_owner->__index >= p_owner->_data_count) p_owner->__index = 0;
	if (p_owner->__index == p_owner->__head) return NULL;
	return p_owner->__p_data + (p_owner->__index * p_owner->_data_size);
}

/**
 * @fn pifRingData_GetFillSize
 * @brief
 * @param p_owner
 * @return
 */
uint16_t pifRingData_GetFillSize(PifRingData* p_owner)
{
	if (p_owner->__head > p_owner->__tail) {
    	return p_owner->__head - p_owner->__tail;
    }
    else {
    	return p_owner->_data_count - p_owner->__tail + p_owner->__head;
    }
}

/**
 * @fn pifRingData_GetRemainSize
 * @brief
 * @param p_owner
 * @return
 */
uint16_t pifRingData_GetRemainSize(PifRingData* p_owner)
{
	uint16_t usRemain;

    if (p_owner->__head < p_owner->__tail) {
    	usRemain = p_owner->__tail - p_owner->__head;
    }
    else {
    	usRemain = p_owner->_data_count - p_owner->__head + p_owner->__tail;
    }
    return usRemain - 1;
}

/**
 * @fn pifRingData_Add
 * @brief
 * @param p_owner
 * @return
 */
void* pifRingData_Add(PifRingData* p_owner)
{
	uint16_t next =	p_owner->__head + 1;

	if (next >= p_owner->_data_count) next = 0;
	if (next == p_owner->__tail) {
		pif_error = E_OVERFLOW_BUFFER;
		return NULL;
	}

	uint8_t* p_data = p_owner->__p_data + (p_owner->__head * p_owner->_data_size);
	p_owner->__head = next;
	return p_data;
}

/**
 * @fn pifRingData_Remove
 * @brief
 * @param p_owner
 * @return
 */
void* pifRingData_Remove(PifRingData* p_owner)
{
	if (p_owner->__head == p_owner->__tail) {
		pif_error = E_EMPTY_IN_BUFFER;
		return NULL;
	}

	uint8_t* p_data = p_owner->__p_data + (p_owner->__tail * p_owner->_data_size);
	p_owner->__tail++;
	if (p_owner->__tail >= p_owner->_data_count) p_owner->__tail = 0;
	return p_data;
}
