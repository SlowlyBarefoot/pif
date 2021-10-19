#ifndef PIF_COLLECT_SIGNAL_H
#define PIF_COLLECT_SIGNAL_H


#ifdef __PIF_COLLECT_SIGNAL__


#include "pifRingBuffer.h"
#include "pifTask.h"


#define PIF_COLLECT_SIGNAL_GET_FLAG(FLAG, IDX)	(((FLAG)[(IDX) / 2] >> (((IDX) & 1) * 4)) & 0x0F)


typedef enum EnPifCollectSignalScale
{
	CSS_1S				= 0,
	CSS_1MS				= 1,	// Default
	CSS_1US				= 2,
	CSS_1NS				= 3,	// Not support
	CSS_1PS				= 4,	// Not support
	CSS_1FS				= 5		// Not support
} PifCollectSignalScale;

typedef enum EnPifCollectSignalFlag
{
	CSF_GPIO			= 0,
	CSF_SENSOR_DIGITAL	= 1,
	CSF_SENSOR_SWITCH	= 2,
	CSF_SEQUENCE		= 3,
	CSF_SOLENOID		= 4,
} PifCollectSignalFlag;

typedef enum EnPifCollectSignalMethod
{
	CSM_LOG				= 0,
	CSM_BUFFER			= 1
} PifCollectSignalMethod;

typedef enum EnPifCollectSignalVarType
{
	CSVT_EVENT			= 0,	// Not support
	CSVT_INTEGER		= 1,
	CSVT_PARAMETER		= 2,	// Not support
	CSVT_REAL			= 3,
	CSVT_REG			= 4,
	CSVT_SUPPLY0		= 5,	// Not support
	CSVT_SUPPLY1		= 6,	// Not support
	CSVT_TIME			= 7,	// Not support
	CSVT_TRI			= 8,	// Not support
	CSVT_TRIAND			= 9,	// Not support
	CSVT_TRIOR			= 10,	// Not support
	CSVT_TRIREG			= 11,	// Not support
	CSVT_TRI0			= 12,	// Not support
	CSVT_TRI1			= 13,	// Not support
	CSVT_WAND			= 14,	// Not support
	CSVT_WIRE			= 15,
	CSVT_WOR			= 16	// Not support
} PifCollectSignalVarType;


typedef void (*PifAddCollectSignalDevice)();


#ifdef __cplusplus
extern "C" {
#endif

void pifCollectSignal_Init(const char* p_module_name);
void pifCollectSignal_Clear();

BOOL pifCollectSignal_AllocHeap(const char* p_module_name, uint16_t size);
BOOL pifCollectSignal_AllocStatic(const char* p_module_name, uint16_t size, uint8_t* p_buffer);

BOOL pifCollectSignal_ChangeScale(PifCollectSignalScale scale);
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
