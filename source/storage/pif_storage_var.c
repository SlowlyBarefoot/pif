#ifndef PIF_NO_LOG
	#include "core/pif_log.h"
#endif
#include "storage/pif_storage_var.h"


#define DATA_NODE_NULL	0xFFFF


/**
 * @fn _getNewDataNode
 * @brief Allocates one metadata node from the free-node list.
 * @param p_owner Pointer to the variable storage instance.
 * @return New node index, or `DATA_NODE_NULL` if no free node is available.
 */
static uint16_t _getNewDataNode(PifStorageVar* p_owner)
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

/**
 * @fn _readData
 * @brief Reads a byte range from media in sector-limited chunks.
 * @param p_owner Pointer to the variable storage instance.
 * @param dst Destination buffer.
 * @param src Source byte offset in media.
 * @param size Number of bytes to read.
 * @param sector_size Maximum chunk size per low-level read.
 * @return `TRUE` on success, otherwise `FALSE`.
 */
static BOOL _readData(PifStorageVar* p_owner, uint8_t* dst, uint32_t src, size_t size, uint16_t sector_size)
{
	uint32_t ptr, len;

	ptr = 0;
	while (size) {
		len = size > sector_size ? sector_size : size;
		if (!(*p_owner->parent.__act_read)(&p_owner->parent, dst + ptr, src + ptr, len)) return FALSE;

		ptr += len;
		size -= len;
	}
	return TRUE;
}

/**
 * @fn _writeData
 * @brief Writes a byte range to media in sector-limited chunks.
 * @param p_owner Pointer to the variable storage instance.
 * @param dst Destination byte offset in media.
 * @param src Source buffer.
 * @param size Number of bytes to write.
 * @return `TRUE` on success, otherwise `FALSE`.
 */
static BOOL _writeData(PifStorageVar* p_owner, uint32_t dst, uint8_t* src, size_t size)
{
	uint16_t sector_size = p_owner->_p_info->sector_size;
	uint32_t ptr, len;

	ptr = 0;
	while (size) {
		len = size > sector_size ? sector_size : size;
		if (!(*p_owner->parent.__act_write)(&p_owner->parent, dst + ptr, src + ptr, len)) return FALSE;

		ptr += len;
		size -= len;
	}
	return TRUE;
}

BOOL pifStorageVar_Init(PifStorageVar* p_owner, PifId id)
{
    if (!p_owner) {
    	pif_error = E_INVALID_PARAM;
	    return FALSE;
    }

	memset(p_owner, 0, sizeof(PifStorageVar));

    if (id == PIF_ID_AUTO) id = pif_id++;
    p_owner->parent._id = id;

	p_owner->parent.__fn_is_format = pifStorageVar_IsFormat;
	p_owner->parent.__fn_format = pifStorageVar_Format;
	p_owner->parent.__fn_create = pifStorageVar_Create;
	p_owner->parent.__fn_delete = pifStorageVar_Delete;
	p_owner->parent.__fn_open = pifStorageVar_Open;
	p_owner->parent.__fn_read = pifStorageVar_Read;
	p_owner->parent.__fn_write = pifStorageVar_Write;
	return TRUE;
}

void pifStorageVar_Clear(PifStorageVar* p_owner)
{
    if (p_owner->__p_info_buffer) {
        free(p_owner->__p_info_buffer);
        p_owner->__p_info_buffer = NULL;
    }
	p_owner->parent.__fn_is_format = NULL;
	p_owner->parent.__fn_format = NULL;
	p_owner->parent.__fn_create = NULL;
	p_owner->parent.__fn_delete = NULL;
	p_owner->parent.__fn_open = NULL;
	p_owner->parent.__fn_read = NULL;
	p_owner->parent.__fn_write = NULL;
}

BOOL pifStorageVar_SetMedia(PifStorageVar* p_owner, uint16_t sector_size, uint32_t storage_volume, uint8_t data_info_count)
{
    PifStorageVarInfo* p_info;

    if (!p_owner || sector_size < 16 || !storage_volume || !data_info_count) {
    	pif_error = E_INVALID_PARAM;
	    return FALSE;
    }

    uint32_t max_sector_count = storage_volume / sector_size;
    if (!max_sector_count || max_sector_count > 65535) {
    	pif_error = E_INVALID_PARAM;
	    return FALSE;
    }

	p_owner->__info_sectors = (sizeof(PifStorageVarInfo) + sizeof(PifStorageVarDataInfo) * data_info_count + sector_size - 1) / sector_size;
	p_owner->__info_bytes = p_owner->__info_sectors * sector_size;

    p_owner->__p_info_buffer = calloc(1, p_owner->__info_bytes);
    if (!p_owner->__p_info_buffer) {
		pif_error = E_OUT_OF_HEAP;
        return FALSE;
	}

    if (!_readData(p_owner, p_owner->__p_info_buffer, 0, p_owner->__info_bytes, sector_size)) {
    	pif_error = E_ACCESS_FAILED;
    	goto fail;
    }
    p_owner->parent._storage_volume = storage_volume;
    p_owner->_p_info = (PifStorageVarInfo*)p_owner->__p_info_buffer;
    p_owner->__p_data_info = (PifStorageVarDataInfo*)(p_owner->__p_info_buffer + sizeof(PifStorageVarInfo));

    p_info = p_owner->_p_info;

    if (p_info->magin_code[0] != 'p' || p_info->magin_code[1] != 'i' ||
            p_info->magin_code[2] != 'f' || p_info->magin_code[3] != 's') {
        goto set;
    }
    if (p_info->data_info_count != data_info_count) {
        goto set;
    }
    if (p_info->crc_16 != pifCrc16(p_owner->__p_info_buffer, sizeof(PifStorageVarInfo) - 6)) {
        goto set;
    }
	p_owner->__is_format = TRUE;
	return TRUE;

set:
	p_info->magin_code[0] = 'p';
	p_info->magin_code[1] = 'i';
	p_info->magin_code[2] = 'f';
	p_info->magin_code[3] = 's';
	p_info->verion = 1;
	p_info->data_info_count = data_info_count;
	p_info->sector_size = sector_size;
	p_info->max_sector_count = max_sector_count;
	return TRUE;

fail:
	if (p_owner->__p_info_buffer) {
		free(p_owner->__p_info_buffer);
		p_owner->__p_info_buffer = NULL;
	}
    return FALSE;
}

PIF_INLINE BOOL pifStorageVar_AttachActStorage(PifStorageVar* p_owner, PifActStorageRead act_read, PifActStorageWrite act_write)
{
	return pifStorage_AttachActStorage(&p_owner->parent, act_read, act_write);
}

PIF_INLINE BOOL pifStorageVar_AttachI2c(PifStorageVar* p_owner, PifI2cPort* p_port, uint8_t addr, PifStorageI2cIAddrSize i_addr_size, uint8_t write_delay_ms)
{
	return pifStorage_AttachI2c(&p_owner->parent, p_port, addr, NULL, i_addr_size, write_delay_ms);
}

BOOL pifStorageVar_IsFormat(PifStorage* p_parent)
{
	return ((PifStorageVar*)p_parent)->__is_format;
}

BOOL pifStorageVar_Format(PifStorage* p_parent)
{
	PifStorageVar* p_owner = (PifStorageVar*)p_parent;
    PifStorageVarInfo* p_info = p_owner->_p_info;
    PifStorageVarDataInfo* p_data_info;
    uint8_t ptr, remain, k, len, data[16];

    if (!p_owner) {
    	pif_error = E_INVALID_PARAM;
	    return FALSE;
    }

	p_info->first_node = DATA_NODE_NULL;
	p_info->free_node = 0;
    p_info->crc_16 = pifCrc16((uint8_t*)p_info, sizeof(PifStorageVarInfo) - 6);

    memset(p_owner->__p_info_buffer + sizeof(PifStorageVarInfo), 0xFF, p_owner->__info_bytes - sizeof(PifStorageVarInfo));

    for (int i = 0; i < p_info->data_info_count - 1; i++) {
    	p_data_info = &p_owner->__p_data_info[i];
    	p_data_info->next_node = i + 1;
    	p_data_info->crc_16 = pifCrc16((uint8_t*)p_data_info, sizeof(PifStorageVarDataInfo) - 6);
	}
	p_data_info = &p_owner->__p_data_info[p_info->data_info_count - 1];
	p_data_info->crc_16 = pifCrc16((uint8_t*)p_data_info, sizeof(PifStorageVarDataInfo) - 6);

    if (!_writeData(p_owner, 0, p_owner->__p_info_buffer, p_owner->__info_bytes)) {
    	pif_error = E_ACCESS_FAILED;
        return FALSE;
    }

    ptr = 0;
    remain = p_owner->__info_bytes;
    while (remain) {
    	len = remain > 16 ? 16 : remain;
        if (!(*p_owner->parent.__act_read)(p_parent, data, ptr, len)) {
        	pif_error = E_ACCESS_FAILED;
            return FALSE;
        }
        for (k = 0; k < len; k++) {
        	if (p_owner->__p_info_buffer[ptr + k] != data[k]) {
            	pif_error = E_IS_NOT_FORMATED;
        		return FALSE;
        	}
        }
        ptr += len;
        remain -= len;
    }

    p_owner->__is_format = TRUE;
	return TRUE;
}

PifStorageDataInfoP pifStorageVar_Create(PifStorage* p_parent, uint16_t id, uint16_t size)
{
	PifStorageVar* p_owner = (PifStorageVar*)p_parent;
	PifStorageVarInfo* p_info = p_owner->_p_info;
	PifStorageVarDataInfo* p_cur_data;
	PifStorageVarDataInfo* p_new_data;
	uint16_t cur_node, new_node, last, sector_size = p_info->sector_size;
	uint16_t sectors = (size + sector_size - 1) / sector_size;

    if (!p_owner || id == 0xFF) {
    	pif_error = E_INVALID_PARAM;
	    return NULL;
    }

	if (!p_owner->__is_format) {
		pif_error = E_IS_NOT_FORMATED;
		return NULL;
	}

	if (p_info->first_node == DATA_NODE_NULL) {
		if (sectors > p_owner->_p_info->max_sector_count) {
			pif_error = E_OVERFLOW_BUFFER;
			return NULL;
		}

		last = p_owner->__info_sectors;

		new_node = _getNewDataNode(p_owner);

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
	p_new_data->crc_16 = pifCrc16((uint8_t*)p_new_data, sizeof(PifStorageVarDataInfo) - 6);

    if (!_writeData(p_owner, 0, p_owner->__p_info_buffer, p_owner->__info_bytes)) {
    	pif_error = E_ACCESS_FAILED;
        return NULL;
    }
	return (PifStorageDataInfoP)p_new_data;
}

BOOL pifStorageVar_Delete(PifStorage* p_parent, uint16_t id)
{
	PifStorageVar* p_owner = (PifStorageVar*)p_parent;
	PifStorageVarInfo* p_info = p_owner->_p_info;
	PifStorageVarDataInfo* p_data_info;
	uint16_t node;

	if (!p_owner->__is_format) {
		pif_error = E_IS_NOT_FORMATED;
		return FALSE;
	}

	node = p_info->first_node;
	while (node != DATA_NODE_NULL) {
		p_data_info = &p_owner->__p_data_info[node];
		if (id == p_data_info->id) {
			memset(p_data_info, 0xFF, sizeof(PifStorageVarDataInfo) - 6);
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
			p_data_info->crc_16 = pifCrc16((uint8_t*)p_data_info, sizeof(PifStorageVarDataInfo) - 6);
			p_info->free_node = node;

			if (!_writeData(p_owner, 0, p_owner->__p_info_buffer, p_owner->__info_bytes)) {
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

PifStorageDataInfoP pifStorageVar_Open(PifStorage* p_parent, uint16_t id)
{
	PifStorageVar* p_owner = (PifStorageVar*)p_parent;
	PifStorageVarInfo* p_info = p_owner->_p_info;
	PifStorageVarDataInfo* p_data_info;
	uint16_t node;

	if (!p_owner->__is_format) {
		pif_error = E_IS_NOT_FORMATED;
		return NULL;
	}

	node = p_info->first_node;
	while (node != DATA_NODE_NULL) {
		p_data_info = &p_owner->__p_data_info[node];
		if (id == p_data_info->id) {
			if (p_data_info->crc_16 != pifCrc16((uint8_t*)p_data_info, sizeof(PifStorageVarDataInfo) - 6)) {
		        pif_error = E_MISMATCH_CRC;
				return NULL;
			}
			return (PifStorageDataInfoP)p_data_info;
		}
		node = p_data_info->next_node;
	}
	pif_error = E_CANNOT_FOUND;
	return NULL;
}

BOOL pifStorageVar_Read(PifStorage* p_parent, uint8_t* p_dst, PifStorageDataInfoP p_src, size_t size)
{
	PifStorageVar* p_owner = (PifStorageVar*)p_parent;

	if (!p_owner->__is_format) {
		pif_error = E_IS_NOT_FORMATED;
		return FALSE;
	}

	return _readData(p_owner, p_dst, ((PifStorageVarDataInfo*)p_src)->first_sector * p_owner->_p_info->sector_size, size, p_owner->_p_info->sector_size);
}

BOOL pifStorageVar_Write(PifStorage* p_parent, PifStorageDataInfoP p_dst, uint8_t* p_src, size_t size)
{
	PifStorageVar* p_owner = (PifStorageVar*)p_parent;

	if (!p_owner->__is_format) {
		pif_error = E_IS_NOT_FORMATED;
		return FALSE;
	}

	return _writeData(p_owner, ((PifStorageVarDataInfo*)p_dst)->first_sector * p_owner->_p_info->sector_size, p_src, size);
}

#if defined(PIF_DEBUG) && !defined(PIF_NO_LOG)

void pifStorageVar_PrintInfo(PifStorageVar* p_owner, BOOL human)
{
	uint16_t i, b, p = 0;

	pifLog_Printf(LT_NONE, "\nIs Format: %d", p_owner->__is_format);
	pifLog_Printf(LT_NONE, "\nInfo Bytes: %lu", p_owner->__info_bytes);
	pifLog_Printf(LT_NONE, "\nInfo Sectors: %u\n", p_owner->__info_sectors);
	if (human) {
		pifLog_Printf(LT_NONE, "\nVerion: %u", p_owner->_p_info->verion);
		pifLog_Printf(LT_NONE, "\nData Info Count: %u", p_owner->_p_info->data_info_count);
		pifLog_Printf(LT_NONE, "\nSector Size: %u", p_owner->_p_info->sector_size);
		pifLog_Printf(LT_NONE, "\nMax Sector Count: %u\n", p_owner->_p_info->max_sector_count);

		for (i = 0; i < p_owner->_p_info->data_info_count; i++) {
			if (p_owner->__p_data_info[i].id < 0xFF) {
				pifLog_Printf(LT_NONE, "\n Id: %2d Size: %u FS: %d", p_owner->__p_data_info[i].id,
						p_owner->__p_data_info[i].size, p_owner->__p_data_info[i].first_sector);
			}
		}
	}
	else {
		pifLog_Printf(LT_NONE, "\n%04X: ", 0);
		for (i = 0; i < sizeof(PifStorageVarInfo); i++, p++) {
			pifLog_Printf(LT_NONE, "%02X ", p_owner->__p_info_buffer[p]);
		}

		for (i = 0; i < p_owner->_p_info->data_info_count; i++) {
			pifLog_Printf(LT_NONE, "\n%04X: ", p);
			for (b = 0; b < sizeof(PifStorageVarDataInfo); b++, p++) {
				pifLog_Printf(LT_NONE, "%02X ", p_owner->__p_info_buffer[p]);
			}
		}
		pifLog_Printf(LT_NONE, "\n");
	}
}

void pifStorageVar_Dump(PifStorageVar* p_owner, uint32_t pos, uint32_t length)
{
	uint32_t i;
	uint8_t b, data[16];

	for (i = 0; i < length;) {
		if (!(*p_owner->parent.__act_read)(&p_owner->parent, data, pos + i, 16)) {
	    	pif_error = E_ACCESS_FAILED;
			return;
		}
		pifLog_Printf(LT_NONE, "\n%08X: ", pos + i);
		for (b = 0; b < 16; b++, i++) {
			pifLog_Printf(LT_NONE, "%02X ", data[b]);
		}
	}
}

#endif
