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

typedef enum _PIF_enSolenoidCsFlag
{
    SnCsF_enOff			= 0,

    SnCsF_enActionIdx	= 0,
    SnCsF_enDirIdx		= 1,

	SnCsF_enActionBit	= 1,
	SnCsF_enDirBit		= 2,
	SnCsF_enAllBit		= 3,

    SnCsF_enCount		= 2
} PIF_enSolenoidCsFlag;


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

#ifdef __PIF_COLLECT_SIGNAL__

typedef struct
{
	PIF_stSolenoid* p_owner;
	uint8_t flag;
    void* p_device[SnCsF_enCount];
} PIF_SolenoidColSig;

#endif

/**
 * @class _PIF_stSolenoid
 * @brief 
 */
struct _PIF_stSolenoid
{
	// Public Member Variable
    uint16_t usOnTime;

	// Public Event Function
    PIF_evtSolenoid evtOff;
    PIF_evtSolenoid evtError;

    // Read-only Member Variable
    PifId _usPifId;
    PIF_enSolenoidType _enType;

	// Private Member Variable
    PIF_stPulse* __pstTimer;
    BOOL __bState;
    PIF_enSolenoidDir __enCurrentDir;
    PIF_stPulseItem *__pstTimerOn;
	PIF_stPulseItem *__pstTimerDelay;
    PIF_enSolenoidDir __enDir;
	PIF_stRingData *__pstBuffer;
#ifdef __PIF_COLLECT_SIGNAL__
	PIF_SolenoidColSig* __p_colsig;
#endif

    // Private Action Function
    PIF_actSolenoidControl __actControl;
};


#ifdef __cplusplus
extern "C" {
#endif

PIF_stSolenoid* pifSolenoid_Create(PifId usPifId, PIF_stPulse* pstTimer, PIF_enSolenoidType enType, uint16_t usOnTime,
		PIF_actSolenoidControl actControl);
void pifSolenoid_Destroy(PIF_stSolenoid** pp_owner);

BOOL pifSolenoid_SetBuffer(PIF_stSolenoid *pstOwner, uint16_t usSize);
void pifSolenoid_SetInvalidDirection(PIF_stSolenoid *pstOwner);
BOOL pifSolenoid_SetOnTime(PIF_stSolenoid *pstOwner, uint16_t usOnTime);

void pifSolenoid_ActionOn(PIF_stSolenoid *pstOwner, uint16_t usDelay);
void pifSolenoid_ActionOnDir(PIF_stSolenoid *pstOwner, uint16_t usDelay, PIF_enSolenoidDir enDir);
void pifSolenoid_ActionOff(PIF_stSolenoid *pstOwner);

#ifdef __PIF_COLLECT_SIGNAL__

void pifSolenoid_SetCsFlagAll(PIF_enSolenoidCsFlag enFlag);
void pifSolenoid_ResetCsFlagAll(PIF_enSolenoidCsFlag enFlag);

void pifSolenoid_SetCsFlagEach(PIF_stSolenoid *pstOwner, PIF_enSolenoidCsFlag enFlag);
void pifSolenoid_ResetCsFlagEach(PIF_stSolenoid *pstOwner, PIF_enSolenoidCsFlag enFlag);

#endif

#ifdef __cplusplus
}
#endif


#endif  // PIF_SOLENOID_H
