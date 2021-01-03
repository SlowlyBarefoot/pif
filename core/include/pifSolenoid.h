#ifndef PIF_SOLENOID_H
#define PIF_SOLENOID_H


#include "pifPulse.h"
#include "pifRingData.h"


typedef enum _PIF_enSolenoidType
{
	ST_en1Point		= 0,
	ST_en2Point		= 1,
	ST_en3Point		= 2
} PIF_enSolenoidType;

typedef enum _PIF_enSolenoidDir
{
    SD_enInvalid	= 0,
    SD_enLeft		= 1,
    SD_enRight	    = 2
} PIF_enSolenoidDir;


struct _PIF_stSolenoid;
typedef struct _PIF_stSolenoid PIF_stSolenoid;

typedef void (*PIF_actSolenoidControl)(SWITCH swAction, PIF_enSolenoidDir enDir);

typedef void (*PIF_evtSolenoid)(PIF_stSolenoid *pstOwner);


/**
 * @class _PIF_stSolenoidContent
 * @brief
 */
typedef struct _PIF_stSolenoidContent
{
	uint16_t usDelay;
	PIF_enSolenoidDir enDir;
} PIF_stSolenoidContent;

/**
 * @class _PIF_stSolenoid
 * @brief 
 */
struct _PIF_stSolenoid
{
	// Public Member Variable
    PIF_unDeviceCode unDeviceCode;
    PIF_enSolenoidType enType;
    uint16_t usOnTime;
};


#ifdef __cplusplus
extern "C" {
#endif

BOOL pifSolenoid_Init(PIF_stPulse *pstTimer, uint8_t ucSize);
void pifSolenoid_Exit();

PIF_stSolenoid *pifSolenoid_Add(PIF_unDeviceCode unDeviceCode, PIF_enSolenoidType enType, uint16_t usOnTime,
		PIF_actSolenoidControl actControl);

void pifSolenoid_AttachEvent(PIF_stSolenoid *pstOwner, PIF_evtSolenoid evtOff, PIF_evtSolenoid evtError);

BOOL pifSolenoid_SetBuffer(PIF_stSolenoid *pstOwner, uint16_t usSize);
void pifSolenoid_SetInvalidDirection(PIF_stSolenoid *pstOwner);
BOOL pifSolenoid_SetOnTime(PIF_stSolenoid *pstOwner, uint16_t usOnTime);

void pifSolenoid_ActionOn(PIF_stSolenoid *pstOwner, uint16_t usDelay);
void pifSolenoid_ActionOnDir(PIF_stSolenoid *pstOwner, uint16_t usDelay, PIF_enSolenoidDir enDir);
void pifSolenoid_ActionOff(PIF_stSolenoid *pstOwner);

#ifdef __cplusplus
}
#endif


#endif  // PIF_SOLENOID_H
