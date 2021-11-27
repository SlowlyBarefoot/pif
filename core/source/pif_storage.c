#include "pif_storage.h"
#ifndef __PIF_NO_LOG__
#include "pif_log.h"
#endif


#define DATA_NODE_NULL	0xFFFF


static uint16_t _getNewDataNode(PifStorage* p_owner)
{
	uint16_t node;

	node = p_owner->_p_info->free_node;
	if (node == DATA_NODE_NULL) {
		pif_error = E_OVERFLOW_BUFFER;
		return DATA_NODE_NULL;
	}

	p_owner->_p_info->free_node = p_owner->__p_data_info[node].next_node;
	return node;
}

BOOL pifStorage_Init(PifStorage* p_owner, PifId id, PifActStorageRead act_read, PifActStorageWrite act_write, 
		uint8_t min_data_info_count, uint16_t sector_size, uint32_t storage_volume)
{
    PifStorageInfo* p_info;

    if (!p_owner || !act_read || !act_write || !min_data_info_count || sector_size < 16 || !storage_volume) {
    	pif_error = E_INVALID_PARAM;
	    return FALSE;
    }

    uint32_t max_sector_count = storage_volume / sector_size;
    if (!max_sector_count || max_sector_count > 65535) {
    	pif_error = E_INVALID_PARAM;
	    return FALSE;
    }

	memset(p_owner, 0, sizeof(PifStorage));

	p_owner->__info_sectors = (sizeof(PifStorageInfo) + sizeof(PifStorageDataInfo) * min_data_info_count + sector_size - 1) / sector_size;
	p_owner->__info_bytes = p_owner->__info_sectors * sector_size;

    p_owner->__p_info_buffer = calloc(1, p_owner->__info_bytes);
    if (!p_owner->__p_info_buffer) {
		pif_error = E_OUT_OF_HEAP;
        return FALSE;
	}

    if (id == PIF_ID_AUTO) id = pif_id++;
    p_owner->_id = id;

	p_owner->__act_read = act_read;
	p_owner->__act_write = act_write;

    if (!(*p_owner->__act_read)(p_owner->__p_info_buffer, 0, p_owner->__info_bytes)) {
    	pif_error = E_ACCESS_FAILED;
    	goto fail;
    }
    p_owner->_p_info = (PifStorageInfo*)p_owner->__p_info_buffer;
    p_owner->__p_data_info = (PifStorageDataInfo*)(p_owner->__p_info_buffer + sizeof(PifStorageInfo));

    p_info = p_owner->_p_info;

    if (p_info->magin_code[0] != 'p' || p_info->magin_code[1] != 'i' ||
            p_info->magin_code[2] != 'f' || p_info->magin_code[3] != 's') {
        goto set;
    }
    if (p_info->crc_16 != pifCrc16(p_owner->__p_info_buffer, sizeof(PifStorageInfo) - 6)) {
        goto set;
    }
	p_owner->_is_format = TRUE;
    return TRUE;

set:
	p_info->magin_code[0] = 'p';
	p_info->magin_code[1] = 'i';
	p_info->magin_code[2] = 'f';
	p_info->magin_code[3] = 's';
	p_info->verion = 1;
	p_info->max_data_info_count = (p_owner->__info_bytes - sizeof(PifStorageInfo)) / sizeof(PifStorageDataInfo);
	p_info->sector_size = sector_size;
	p_info->max_sector_count = max_sector_count;
	return TRUE;

fail:
    pifStorage_Clear(p_owner);
    return FALSE;
}

void pifStorage_Clear(PifStorage* p_owner)
{
    if (p_owner->__p_info_buffer) {
        free(p_owner->__p_info_buffer);
        p_owner->__p_info_buffer = NULL;
    }
}

BOOL pifStorage_Format(PifStorage* p_owner)
{
    PifStorageInfo* p_info = p_owner->_p_info;
    PifStorageDataInfo* p_data_info;

    if (!p_owner) {
    	pif_error = E_INVALID_PARAM;
	    return FALSE;
    }

	p_info->first_node = DATA_NODE_NULL;
	p_info->free_node = 0;
    p_info->crc_16 = pifCrc16((uint8_t*)p_info, sizeof(PifStorageInfo) - 6);

    memset(p_owner->__p_info_buffer + sizeof(PifStorageInfo), 0xFF, p_owner->__info_bytes - sizeof(PifStorageInfo));

    for (int i = 0; i < p_info->max_data_info_count - 1; i++) {
    	p_data_info = &p_owner->__p_data_info[i];
    	p_data_info->next_node = i + 1;
    	p_data_info->crc_16 = pifCrc16((uint8_t*)p_data_info, sizeof(PifStorageDataInfo) - 6);
	}
	p_data_info = &p_owner->__p_data_info[p_info->max_data_info_count - 1];
	p_data_info->crc_16 = pifCrc16((uint8_t*)p_data_info, sizeof(PifStorageDataInfo) - 6);

    if (!(*p_owner->__act_write)(0, p_owner->__p_info_buffer, p_owner->__info_bytes)) {
    	pif_error = E_ACCESS_FAILED;
        return FALSE;
    }

    p_owner->_is_format = TRUE;
	return TRUE;
}

PifStorageDataInfo* pifStorage_Alloc(PifStorage* p_owner, uint16_t id, uint16_t size)
{
	PifStorageInfo* p_info = p_owner->_p_info;
	PifStorageDataInfo* p_cur_data;
	PifStorageDataInfo* p_new_data;
	uint16_t cur_node, new_node, last, sector_size = p_info->sector_size;
	uint16_t sectors = (size + sector_size - 1) / sector_size;

	if (!p_owner->_is_format) {
		pif_error = E_IS_NOT_FORMATED;
		return NULL;
	}

	if (p_info->first_node == DATA_NODE_NULL) {
		last = p_owner->__info_sectors;

		new_node = _getNewDataNode(p_owner);
		if (new_node == DATA_NODE_NULL) return NULL;

		p_new_data = &p_owner->__p_data_info[new_node];
		p_new_data->next_node = p_info->first_node;
		p_new_data->prev_node = DATA_NODE_NULL;

		p_info->first_node = new_node;
		goto save;
	}
	else {
		cur_node = p_info->first_node;
		p_cur_data = &p_owner->__p_data_info[cur_node];
		last = p_owner->__info_sectors;
		if (p_cur_data->first_sector != last) {
			if (p_cur_data->first_sector - last >= sectors) {
				new_node = _getNewDataNode(p_owner);
				if (new_node == DATA_NODE_NULL) return NULL;

				p_new_data = &p_owner->__p_data_info[new_node];
				p_new_data->next_node = p_info->first_node;
				p_new_data->prev_node = DATA_NODE_NULL;

				p_info->first_node = new_node;
				p_cur_data->prev_node = new_node;
				goto save;
			}
		}
		while (cur_node != DATA_NODE_NULL) {
			p_cur_data = &p_owner->__p_data_info[cur_node];
			last = p_cur_data->first_sector + (p_cur_data->size + sector_size - 1) / sector_size;
			if (p_cur_data->next_node == DATA_NODE_NULL) {
				if (p_info->max_sector_count - last >= sectors) {
					new_node = _getNewDataNode(p_owner);
					if (new_node == DATA_NODE_NULL) return NULL;

					p_new_data = &p_owner->__p_data_info[new_node];
					p_new_data->next_node = p_cur_data->next_node;
					p_new_data->prev_node = cur_node;

					p_cur_data->next_node = new_node;
					goto save;
				}
			}
			else {
				if (p_owner->__p_data_info[p_cur_data->next_node].first_sector - last >= sectors) {
					new_node = _getNewDataNode(p_owner);
					if (new_node == DATA_NODE_NULL) return NULL;

					p_new_data = &p_owner->__p_data_info[new_node];
					p_new_data->next_node = p_cur_data->next_node;
					p_new_data->prev_node = cur_node;

					p_owner->__p_data_info[p_cur_data->next_node].prev_node = new_node;
					p_cur_data->next_node = new_node;
					goto save;
				}
			}
			cur_node = p_cur_data->next_node;
		}
	}
	pif_error = E_OVERFLOW_BUFFER;
	return NULL;

save:
	p_new_data->id = id;
	p_new_data->size = size;
	p_new_data->first_sector = last;
	p_new_data->crc_16 = pifCrc16((uint8_t*)p_new_data, sizeof(PifStorageDataInfo) - 6);

    if (!(*p_owner->__act_write)(0, p_owner->__p_info_buffer, p_owner->__info_bytes)) {
    	pif_error = E_ACCESS_FAILED;
        return NULL;
    }
	return p_new_data;
}

BOOL pifStorage_Free(PifStorage* p_owner, uint16_t id)
{
	PifStorageInfo* p_info = p_owner->_p_info;
	PifStorageDataInfo* p_data_info;
	uint16_t node;

	if (!p_owner->_is_format) {
		pif_error = E_IS_NOT_FORMATED;
		return FALSE;
	}

	node = p_info->first_node;
	while (node != DATA_NODE_NULL) {
		p_data_info = &p_owner->__p_data_info[node];
		if (id == p_data_info->id) {
			memset(p_data_info, 0xFF, sizeof(PifStorageDataInfo) - 6);
			if (p_data_info->prev_node != DATA_NODE_NULL) {
				p_owner->__p_data_info[p_data_info->prev_node].next_node = p_data_info->next_node;
			}
			else {
				p_info->first_node = p_data_info->next_node;
			}
			if (p_data_info->next_node != DATA_NODE_NULL) {
				p_owner->__p_data_info[p_data_info->next_node].prev_node = p_data_info->prev_node;
			}
			p_data_info->next_node = p_info->free_node;
			p_data_info->prev_node = DATA_NODE_NULL;
			p_data_info->crc_16 = pifCrc16((uint8_t*)p_data_info, sizeof(PifStorageDataInfo) - 6);
			p_info->free_node = node;

			if (!(*p_owner->__act_write)(0, p_owner->__p_info_buffer, p_owner->__info_bytes)) {
		    	pif_error = E_ACCESS_FAILED;
				return FALSE;
			}
			return TRUE;
		}
		node = p_data_info->next_node;
	}
	pif_error = E_CANNOT_FOUND;
	return FALSE;
}

PifStorageDataInfo* pifStorage_GetDataInfo(PifStorage* p_owner, uint16_t id)
{
	PifStorageInfo* p_info = p_owner->_p_info;
	PifStorageDataInfo* p_data_info;
	uint16_t node;

	if (!p_owner->_is_format) {
		pif_error = E_IS_NOT_FORMATED;
		return NULL;
	}

	node = p_info->first_node;
	while (node != DATA_NODE_NULL) {
		p_data_info = &p_owner->__p_data_info[node];
		if (id == p_data_info->id) {
			if (p_data_info->crc_16 != pifCrc16((uint8_t*)p_data_info, sizeof(PifStorageDataInfo) - 6)) {
		        pif_error = E_MISMATCH_CRC;
				return NULL;
			}
			return p_data_info;
		}
		node = p_data_info->next_node;
	}
	pif_error = E_CANNOT_FOUND;
	return NULL;
}

BOOL pifStorage_Write(PifStorage* p_owner, PifStorageDataInfo* p_data_info, uint8_t* p_data)
{
	uint16_t i, sector_size = p_owner->_p_info->sector_size;

	if (!p_owner->_is_format) {
		pif_error = E_IS_NOT_FORMATED;
		return FALSE;
	}

	for (i = 0; i < (p_data_info->size + sector_size - 1) / sector_size; i++) {
		if (!(*p_owner->__act_write)((p_data_info->first_sector + i) * sector_size, p_data + i * sector_size, p_data_info->size)) {
	    	pif_error = E_ACCESS_FAILED;
			return FALSE;
		}
	}
	return TRUE;
}

BOOL pifStorage_Read(PifStorage* p_owner, PifStorageDataInfo* p_data_info, uint8_t* p_data)
{
	uint16_t i, sector_size = p_owner->_p_info->sector_size;

	if (!p_owner->_is_format) {
		pif_error = E_IS_NOT_FORMATED;
		return FALSE;
	}

	for (i = 0; i < (p_data_info->size + sector_size - 1) / sector_size; i++) {
		if (!(*p_owner->__act_read)(p_data + i * sector_size, (p_data_info->first_sector + i) * sector_size, p_data_info->size)) {
	    	pif_error = E_ACCESS_FAILED;
			return FALSE;
		}
	}
	return TRUE;
}

#if defined(__PIF_DEBUG__) && !defined(__PIF_NO_LOG__)

void pifStorage_PrintInfo(PifStorage* p_owner)
{
	uint16_t i, b, p = 0;

	pifLog_Printf(LT_NONE, "\nis_format: %d", p_owner->_is_format);
	pifLog_Printf(LT_NONE, "\ninfo_bytes: %lu", p_owner->__info_bytes);
	pifLog_Printf(LT_NONE, "\ninfo_sectors: %u", p_owner->__info_sectors);
	pifLog_Printf(LT_NONE, "\n\n%04X: ", 0);
	for (i = 0; i < sizeof(PifStorageInfo); i++, p++) {
		pifLog_Printf(LT_NONE, "%02X ", p_owner->__p_info_buffer[p]);
	}

	for (i = 0; i < p_owner->_p_info->max_data_info_count; i++) {
		pifLog_Printf(LT_NONE, "\n%04X: ", p);
		for (b = 0; b < sizeof(PifStorageDataInfo); b++, p++) {
			pifLog_Printf(LT_NONE, "%02X ", p_owner->__p_info_buffer[p]);
		}
	}
	pifLog_Printf(LT_NONE, "\n");
}

#endif
