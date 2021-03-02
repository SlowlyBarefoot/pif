#ifndef PIF_COLLECT_SIGNAL_H
#define PIF_COLLECT_SIGNAL_H


#ifdef __PIF_COLLECT_SIGNAL__


#include "pifRingBuffer.h"
#include "pifTask.h"


#define PIF_COLLECT_SIGNAL_GET_FLAG(FLAG, IDX)	(((FLAG)[(IDX) / 2] >> (((IDX) & 1) * 4)) & 0x0F)


typedef enum _PIF_enCollectSignalFlag
{
	CSF_enGpio				= 0,
	CSF_enSensorDigital		= 1,
	CSF_enSensorSwitch		= 2,
	CSF_enSequence			= 3,
	CSF_enSolenoid			= 4,
} PIF_enCollectSignalFlag;

typedef enum _PIF_enCollectSignalMethod
{
	CSM_enRealTime		= 0,
	CSM_enBuffer		= 1
} PIF_enCollectSignalMethod;

typedef enum _PIF_enCollectSignalVarType
{
	CSVT_enEvent		= 0,
	CSVT_enInteger		= 1,
	CSVT_enParameter	= 2,
	CSVT_enReal			= 3,
	CSVT_enReg			= 4,
	CSVT_enSupply0		= 5,
	CSVT_enSupply1		= 6,
	CSVT_enTime			= 7,
	CSVT_enTri			= 8,
	CSVT_enTriand		= 9,
	CSVT_enTrior		= 10,
	CSVT_enTriReg		= 11,
	CSVT_enTri0			= 12,
	CSVT_enTri1			= 13,
	CSVT_enWand			= 14,
	CSVT_enWire			= 15,
	CSVT_enWor			= 16
} PIF_enCollectSignalVarType;


typedef void (*PIF_fnCollectSignalDevice)();


#ifdef __cplusplus
extern "C" {
#endif

void pifCollectSignal_Init(const char *c_pcModuleName);
BOOL pifCollectSignal_InitHeap(const char *c_pcModuleName, uint16_t usSize);
BOOL pifCollectSignal_InitStatic(const char *c_pcModuleName, uint16_t usSize, char *pcBuffer);
void pifCollectSignal_Exit();

void pifCollectSignal_ChangeFlag(uint8_t *pucFlag, uint8_t ucIndex, uint8_t ucFlag);
BOOL pifCollectSignal_ChangeMethod(PIF_enCollectSignalMethod enMethod);

void pifCollectSignal_Attach(PIF_enCollectSignalFlag enFlag, PIF_fnCollectSignalDevice fnDevice);
int8_t pifCollectSignal_AddDevice(PIF_usId usPifId, PIF_enCollectSignalVarType enVarType, uint16_t usSize, const char *pcReference,
		uint16_t usInitialValue);

void pifCollectSignal_Start();
void pifCollectSignal_Stop();

void pifCollectSignal_AddSignal(int8_t cIndex, uint16_t usState);

void pifCollectSignal_PrintLog();

// Task Function
void pifCollectSignal_taskAll(PIF_stTask *pstTask);

#ifdef __cplusplus
}
#endif


#endif	// __PIF_COLLECT_SIGNAL__


#endif	// PIF_COLLECT_SIGNAL_H
