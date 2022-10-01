#ifndef PIF_SOLENOID_H
#define PIF_SOLENOID_H


#include "pif_timer.h"
#include "pif_ring_data.h"


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

typedef enum EnPifSolenoidCsFlag
{
    SN_CSF_OFF			= 0,

    SN_CSF_ACTION_IDX	= 0,
    SN_CSF_DIR_IDX		= 1,

	SN_CSF_ACTION_BIT	= 1,
	SN_CSF_DIR_BIT		= 2,
	SN_CSF_ALL_BIT		= 3,

    SN_CSF_COUNT		= 2
} PifSolenoidCsFlag;

typedef struct StPifSolenoidColSig
{
	PifSolenoid* p_owner;
	uint8_t flag;
    void* p_device[SN_CSF_COUNT];
} PifSolenoidColSig;

#endif	// __PIF_COLLECT_SIGNAL__

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
    PifTimerManager* __p_timer_manager;
    BOOL __state;
    PifSolenoidDir __current_dir;
    PifTimer* __p_timer_on;
	PifTimer* __p_timer_delay;
    PifSolenoidDir __dir;
	PifRingData* __p_buffer;
#ifdef __PIF_COLLECT_SIGNAL__
	PifSolenoidColSig* __p_colsig;
#endif

    // Private Action Function
    PifActSolenoidControl __act_control;
};


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifSolenoid_Init
 * @brief
 * @param p_owner
 * @param id
 * @param p_timer_manager
 * @param type
 * @param on_time
 * @param act_control
 * @return
 */
BOOL pifSolenoid_Init(PifSolenoid* p_owner, PifId id, PifTimerManager* p_timer_manager, PifSolenoidType type, uint16_t on_time,
		PifActSolenoidControl act_control);

/**
 * @fn pifSolenoid_Clear
 * @brief
 * @param p_owner
 */
void pifSolenoid_Clear(PifSolenoid* p_owner);

/**
 * @fn pifSolenoid_SetBuffer
 * @brief
 * @param p_owner
 * @param size
 * @return
 */
BOOL pifSolenoid_SetBuffer(PifSolenoid* p_owner, uint16_t size);

/**
 * @fn pifSolenoid_SetInvalidDirection
 * @brief
 * @param p_owner
 */
void pifSolenoid_SetInvalidDirection(PifSolenoid* p_owner);

/**
 * @fn pifSolenoid_SetOnTime
 * @brief
 * @param p_owner
 * @param on_time
 * @return
 */
BOOL pifSolenoid_SetOnTime(PifSolenoid* p_owner, uint16_t on_time);

/**
 * @fn pifSolenoid_ActionOn
 * @brief
 * @param p_owner
 * @param delay
 */
void pifSolenoid_ActionOn(PifSolenoid* p_owner, uint16_t delay);

/**
 * @fn pifSolenoid_ActionOnDir
 * @brief 
 * @param p_owner
 * @param delay
 * @param dir
 */
void pifSolenoid_ActionOnDir(PifSolenoid* p_owner, uint16_t delay, PifSolenoidDir dir);

/**
 * @fn pifSolenoid_ActionOff
 * @brief
 * @param p_owner
 */
void pifSolenoid_ActionOff(PifSolenoid* p_owner);


#ifdef __PIF_COLLECT_SIGNAL__

/**
 * @fn pifSolenoid_SetCsFlag
 * @brief
 * @param pstSensor
 * @param flag
 */
void pifSolenoid_SetCsFlag(PifSolenoid* p_owner, PifSolenoidCsFlag flag);

/**
 * @fn pifSolenoid_ResetCsFlag
 * @brief
 * @param pstSensor
 * @param flag
 */
void pifSolenoid_ResetCsFlag(PifSolenoid* p_owner, PifSolenoidCsFlag flag);

/**
 * @fn pifSolenoidColSig_SetFlag
 * @brief
 * @param flag
 */
void pifSolenoidColSig_SetFlag(PifSolenoidCsFlag flag);

/**
 * @fn pifSolenoidColSig_ResetFlag
 * @brief
 * @param flag
 */
void pifSolenoidColSig_ResetFlag(PifSolenoidCsFlag flag);

#endif	// __PIF_COLLECT_SIGNAL__

#ifdef __cplusplus
}
#endif


#endif  // PIF_SOLENOID_H
