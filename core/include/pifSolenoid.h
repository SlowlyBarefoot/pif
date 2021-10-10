#ifndef PIF_SOLENOID_H
#define PIF_SOLENOID_H


#include "pifPulse.h"
#include "pifRingData.h"


typedef enum EnPifSolenoidType
{
	ST_1POINT		= 0,
	ST_2POINT		= 1,
	ST_3POINT		= 2
} PifSolenoidType;

typedef enum EnPifSolenoidDir
{
    SD_INVALID		= 0,
    SD_LEFT			= 1,
    SD_RIGHT	    = 2
} PifSolenoidDir;

typedef enum EnPifSolenoidCsFlag
{
    SCSF_OFF			= 0,

    SCSF_ACTION_IDX		= 0,
    SCSF_DIR_IDX		= 1,

	SCSF_ACTION_BIT		= 1,
	SCSF_DIR_BIT		= 2,
	SCSF_ALL_BIT		= 3,

    SCSF_COUNT			= 2
} PifSolenoidCsFlag;


struct StPifSolenoid;
typedef struct StPifSolenoid PifSolenoid;

typedef void (*PifActSolenoidControl)(SWITCH action, PifSolenoidDir dir);

typedef void (*PifEvtSolenoid)(PifSolenoid* p_owner);


/**
 * @class StPifSolenoidContent
 * @brief
 */
typedef struct StPifSolenoidContent
{
	uint16_t delay;
	PifSolenoidDir dir;
} PifSolenoidContent;

#ifdef __PIF_COLLECT_SIGNAL__

typedef struct StPifSolenoidColSig
{
	PifSolenoid* p_owner;
	uint8_t flag;
    void* p_device[SCSF_COUNT];
} PifSolenoidColSig;

#endif

/**
 * @class StPifSolenoid
 * @brief 
 */
struct StPifSolenoid
{
	// Public Member Variable
    uint16_t on_time;

	// Public Event Function
    PifEvtSolenoid evt_off;
    PifEvtSolenoid evt_error;

    // Read-only Member Variable
    PifId _id;
    PifSolenoidType _type;

	// Private Member Variable
    PifPulse* __p_timer;
    BOOL __state;
    PifSolenoidDir __current_dir;
    PifPulseItem* __p_timer_on;
	PifPulseItem* __p_timer_delay;
    PifSolenoidDir __dir;
	PIF_stRingData* __p_buffer;
#ifdef __PIF_COLLECT_SIGNAL__
	PifSolenoidColSig* __p_colsig;
#endif

    // Private Action Function
    PifActSolenoidControl __act_control;
};


#ifdef __cplusplus
extern "C" {
#endif

PifSolenoid* pifSolenoid_Create(PifId id, PifPulse* p_timer, PifSolenoidType type, uint16_t on_time,
		PifActSolenoidControl act_control);
void pifSolenoid_Destroy(PifSolenoid** pp_owner);

BOOL pifSolenoid_SetBuffer(PifSolenoid* p_owner, uint16_t size);
void pifSolenoid_SetInvalidDirection(PifSolenoid* p_owner);
BOOL pifSolenoid_SetOnTime(PifSolenoid* p_owner, uint16_t on_time);

void pifSolenoid_ActionOn(PifSolenoid* p_owner, uint16_t delay);
void pifSolenoid_ActionOnDir(PifSolenoid* p_owner, uint16_t delay, PifSolenoidDir dir);
void pifSolenoid_ActionOff(PifSolenoid* p_owner);

#ifdef __PIF_COLLECT_SIGNAL__

void pifSolenoid_SetCsFlagAll(PifSolenoidCsFlag flag);
void pifSolenoid_ResetCsFlagAll(PifSolenoidCsFlag flag);

void pifSolenoid_SetCsFlagEach(PifSolenoid* p_owner, PifSolenoidCsFlag flag);
void pifSolenoid_ResetCsFlagEach(PifSolenoid* p_owner, PifSolenoidCsFlag flag);

#endif

#ifdef __cplusplus
}
#endif


#endif  // PIF_SOLENOID_H
