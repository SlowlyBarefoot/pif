#include "filter/pif_noise_filter_manager.h"


/**
 * @fn _evtClear
 * @brief Callback that clears and frees one filter entry from the manager array.
 * @param it Pointer-array iterator for the item being removed.
 */
static void _evtClear(PifPtrArrayIterator it)
{
	PifNoiseFilter* p_filter = (PifNoiseFilter*)it->p_data;

	if (p_filter->__fn_clear) (*p_filter->__fn_clear)(p_filter);

	free(it->p_data);
	it->p_data = NULL;
}

BOOL pifNoiseFilterManager_Init(PifNoiseFilterManager* p_owner, uint8_t count)
{
	if (!p_owner || !count) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

    memset(p_owner, 0, sizeof(PifNoiseFilterManager));

    if (!pifPtrArray_Init(&p_owner->__filters, count, _evtClear)) return FALSE;

    p_owner->_count = count;
    return TRUE;
}

void pifNoiseFilterManager_Clear(PifNoiseFilterManager* p_owner)
{
	pifPtrArray_Clear(&p_owner->__filters);
}
