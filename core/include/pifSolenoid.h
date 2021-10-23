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
    SN_CSF_OFF			= 0,

    SN_CSF_ACTION_IDX	= 0,
    SN_CSF_DIR_IDX		= 1,

	SN_CSF_ACTION_BIT	= 1,
	SN_CSF_DIR_BIT		= 2,
	SN_CSF_ALL_BIT		= 3,

    SN_CSF_COUNT		= 2
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
    void* p_device[SN_CSF_COUNT];
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
 * @fn pifSolenoid_Create
 * @brief
 * @param id
 * @param p_timer
 * @param type
 * @param on_time
 * @param act_control
 * @return
 */
PifSolenoid* pifSolenoid_Create(PifId id, PifPulse* p_timer, PifSolenoidType type, uint16_t on_time,
		PifActSolenoidControl act_control);

/**
 * @fn pifSolenoid_Destroy
 * @brief
 * @param pp_owner
 */
void pifSolenoid_Destroy(PifSolenoid** pp_owner);

/**
 * @fn pifSolenoid_Init
 * @brief
 * @param p_owner
 * @param id
 * @param p_timer
 * @param type
 * @param on_time
 * @param act_control
 * @return
 */
BOOL pifSolenoid_Init(PifSolenoid* p_owner, PifId id, PifPulse* p_timer, PifSolenoidType type, uint16_t on_time,
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
 * @fn pifSolenoid_SetCsFlagAll
 * @brief
 * @param flag
 */
void pifSolenoid_SetCsFlagAll(PifSolenoidCsFlag flag);

/**
 * @fn pifSolenoid_ResetCsFlagAll
 * @brief
 * @param flag
 */
void pifSolenoid_ResetCsFlagAll(PifSolenoidCsFlag flag);

/**
 * @fn pifSolenoid_SetCsFlagEach
 * @brief
 * @param pstSensor
 * @param flag
 */
void pifSolenoid_SetCsFlagEach(PifSolenoid* p_owner, PifSolenoidCsFlag flag);

/**
 * @fn pifSolenoid_ResetCsFlagEach
 * @brief
 * @param pstSensor
 * @param flag
 */
void pifSolenoid_ResetCsFlagEach(PifSolenoid* p_owner, PifSolenoidCsFlag flag);

#endif

#ifdef __cplusplus
}
#endif


#endif  // PIF_SOLENOID_H
