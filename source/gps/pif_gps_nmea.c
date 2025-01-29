#ifndef PIF_NO_LOG
	#include "core/pif_log.h"
#endif
#include "gps/pif_gps_nmea.h"


static void _evtParsing(void* p_client, PifActUartReceiveData act_receive_data)
{
	PifGpsNmea *p_owner = (PifGpsNmea *)p_client;
	uint8_t c;

	while ((*act_receive_data)(p_owner->__p_uart, &c, 1)) {
		pifGps_ParsingNmea(&p_owner->_gps, c);
	}
}

#define DATA_SIZE	128

static uint16_t _doTask(PifTask* p_task)
{
	PifGpsNmea *p_owner = p_task->_p_client;
	uint8_t data[DATA_SIZE];
	uint16_t size, i;
#ifndef PIF_NO_LOG
	int line;
	static uint32_t timer1ms;
#endif

	if (!p_owner->__length) {
		if (!pifI2cDevice_ReadRegWord(p_owner->_p_i2c_device, 0xFD, &p_owner->__length)) {
#ifndef PIF_NO_LOG
			line = __LINE__;
#endif
			goto fail;
		}
		if (!p_owner->__length) return 0;
		if (p_owner->__length == 0xFFFF) { p_owner->__length = 0; return 0; }
		if (p_owner->__length & 0x8000) {
#ifndef PIF_NO_LOG
			line = __LINE__;
#endif
			goto fail;
		}
#ifndef PIF_NO_LOG
		timer1ms = pif_cumulative_timer1ms;
		pifLog_Printf(LT_INFO, "GN(%u): Start L=%d bytes", __LINE__, p_owner->__length);
#endif
	}

	size = p_owner->__length > DATA_SIZE ? DATA_SIZE : p_owner->__length;
	if (!pifI2cDevice_Read(p_owner->_p_i2c_device, 0, 0, data, size)) {
#ifndef PIF_NO_LOG
			line = __LINE__;
#endif
			goto fail;
	}
	for (i = 0; i < size; i++) {
		pifGps_ParsingNmea(&p_owner->_gps, data[i]);
	}
	p_owner->__length -= size;
	if (p_owner->__length) pifTask_SetTrigger(p_task);
#ifndef PIF_NO_LOG
	else {
		pifLog_Printf(LT_INFO, "GN(%u): End T=%ld ms", __LINE__, pif_cumulative_timer1ms - timer1ms);
	}
#endif
	return 0;

fail:
#ifndef PIF_NO_LOG
	pifLog_Printf(LT_ERROR, "GN(%u): len=%d", line, p_owner->__length);
#endif
	p_owner->__length = 0;
	return 0;
}

BOOL pifGpsNmea_Init(PifGpsNmea* p_owner, PifId id)
{
	if (!p_owner) {
		pif_error = E_INVALID_PARAM;
	    return FALSE;
	}

	memset(p_owner, 0, sizeof(PifGpsNmea));

    if (!pifGps_Init(&p_owner->_gps, id)) goto fail;
    return TRUE;

fail:
	pifGpsNmea_Clear(p_owner);
    return FALSE;
}

void pifGpsNmea_Clear(PifGpsNmea* p_owner)
{
	pifGps_Clear(&p_owner->_gps);
}

void pifGpsNmea_AttachUart(PifGpsNmea* p_owner, PifUart* p_uart)
{
	p_owner->__p_uart = p_uart;
	pifUart_AttachClient(p_uart, p_owner, _evtParsing, NULL);
}

void pifGpsNmea_DetachUart(PifGpsNmea* p_owner)
{
	pifUart_DetachClient(p_owner->__p_uart);
	p_owner->__p_uart = NULL;
}

BOOL pifGpsNmea_AttachI2c(PifGpsNmea* p_owner, PifI2cPort* p_i2c, uint8_t addr, uint16_t period, BOOL start, const char* name)
{
    p_owner->_p_i2c_device = pifI2cPort_AddDevice(p_i2c, PIF_ID_AUTO, addr);
    if (!p_owner->_p_i2c_device) goto fail;

    p_owner->_p_task = pifTaskManager_Add(TM_PERIOD_MS, period, _doTask, p_owner, start);
	if (!p_owner->_p_task) goto fail;

    p_owner->__p_i2c_port = p_i2c;
	if (name) p_owner->_p_task->name = name;
	else p_owner->_p_task->name = "GPS NMEA";
    return TRUE;

fail:
	pifGpsNmea_DetachI2c(p_owner);
	return FALSE;
}

void pifGpsNmea_DetachI2c(PifGpsNmea* p_owner)
{
	if (p_owner->_p_i2c_device) {
		pifI2cPort_RemoveDevice(p_owner->__p_i2c_port, p_owner->_p_i2c_device);
		p_owner->_p_i2c_device = NULL;
	}
}
