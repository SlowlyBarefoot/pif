#ifndef PIF_COLLECT_SIGNAL_H
#define PIF_COLLECT_SIGNAL_H


#ifdef __PIF_COLLECT_SIGNAL__


#include "pifRingBuffer.h"
#include "pifTask.h"


#define PIF_COLLECT_SIGNAL_GET_FLAG(FLAG, IDX)	(((FLAG)[(IDX) / 2] >> (((IDX) & 1) * 4)) & 0x0F)


typedef enum EnPifCollectSignalFlag
{
	CSF_GPIO				= 0,
	CSF_SENSOR_DIGITAL		= 1,
	CSF_SENSOR_SWITCH		= 2,
	CSF_SEQUENCE			= 3,
	CSF_SOLENOID			= 4,
} PifCollectSignalFlag;

typedef enum EnPifCollectSignalMethod
{
	CSM_REALTIME		= 0,
	CSM_BUFFER			= 1
} PifCollectSignalMethod;

typedef enum EnPifCollectSignalVarType
{
	CSVT_EVENT			= 0,
	CSVT_INTEGER		= 1,
	CSVT_PARAMETER		= 2,
	CSVT_REAL			= 3,
	CSVT_REG			= 4,
	CSVT_SUPPLY0		= 5,
	CSVT_SUPPLY1		= 6,
	CSVT_TIME			= 7,
	CSVT_TRI			= 8,
	CSVT_TRIAND			= 9,
	CSVT_TRIOR			= 10,
	CSVT_TRIREG			= 11,
	CSVT_TRI0			= 12,
	CSVT_TRI1			= 13,
	CSVT_WAND			= 14,
	CSVT_WIRE			= 15,
	CSVT_WOR			= 16
} PifCollectSignalVarType;


typedef void (*PifAddCollectSignalDevice)();


#ifdef __cplusplus
extern "C" {
#endif

void pifCollectSignal_Init(const char* p_module_name);
BOOL pifCollectSignal_InitHeap(const char* p_module_name, uint16_t size);
BOOL pifCollectSignal_InitStatic(const char* p_module_name, uint16_t size, uint8_t* p_buffer);
void pifCollectSignal_Clear();

void pifCollectSignal_ChangeFlag(uint8_t* p_flag, uint8_t index, uint8_t flag);
BOOL pifCollectSignal_ChangeMethod(PifCollectSignalMethod method);

void pifCollectSignal_Attach(PifCollectSignalFlag flag, PifAddCollectSignalDevice add_device);
void* pifCollectSignal_AddDevice(PifId id, PifCollectSignalVarType var_type, uint16_t size,
		const char* p_reference, uint16_t initial_value);

void pifCollectSignal_Start();
void pifCollectSignal_Stop();

void pifCollectSignal_AddSignal(void* p_dev, uint16_t state);

void pifCollectSignal_PrintLog();

// Task Function
PifTask* pifCollectSignal_AttachTask(PifTaskMode mode, uint16_t period, BOOL start);

#ifdef __cplusplus
}
#endif


#endif	// __PIF_COLLECT_SIGNAL__


#endif	// PIF_COLLECT_SIGNAL_H
