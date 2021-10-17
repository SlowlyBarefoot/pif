#include "pifLog.h"
#include "pifDutyMotor.h"


static uint16_t _doTask(PifTask* p_task)
{
	PifDutyMotor* p_owner = p_task->_p_client;

    if (p_owner->__control) (*p_owner->__control)(p_owner);

    if (p_owner->__error) {
        if (p_owner->_state < MS_BREAK) {
			(*p_owner->__act_set_duty)(0);
			p_owner->_current_duty = 0;

			p_owner->_state = MS_BREAK;
        }

        if (p_owner->evt_error) (*p_owner->evt_error)(p_owner);
    }

	if (p_owner->_state == MS_STOP) {
		p_task->pause = TRUE;
		p_owner->_state = MS_IDLE;
		if (p_owner->evt_stop) (*p_owner->evt_stop)(p_owner);
	}
	return 0;
}

static void _evtTimerBreakFinish(void* p_issuer)
{
    PifDutyMotor* p_owner = (PifDutyMotor*)p_issuer;

    if (p_owner->_state > MS_IDLE && p_owner->_state < MS_REDUCE) {
    	p_owner->_state = MS_REDUCE;
    }
    else {
    	(*p_owner->__act_operate_break)(0);
    }
}

/**
 * @fn pifDutyMotor_Create
 * @brief 
 * @param id
 * @param p_timer
 * @param max_duty
 * @return 
 */
PifDutyMotor* pifDutyMotor_Create(PifId id, PifPulse* p_timer, uint16_t max_duty)
{
    PifDutyMotor* p_owner = NULL;

    p_owner = calloc(sizeof(PifDutyMotor), 1);
    if (!p_owner) {
		pif_error = E_OUT_OF_HEAP;
	    return NULL;
	}

    if (!pifDutyMotor_Init(p_owner, id, p_timer, max_duty)) return NULL;
    return p_owner;
}

/**
 * @fn pifDutyMotor_Destroy
 * @brief
 * @param pp_owner
 */
void pifDutyMotor_Destroy(PifDutyMotor** pp_owner)
{
	if (*pp_owner) {
		pifDutyMotor_Clear(*pp_owner);
		free(*pp_owner);
		*pp_owner = NULL;
	}
}

/**
 * @fn pifDutyMotor_Init
 * @brief 
 * @param p_owner
 * @param id
 * @param p_timer
 * @param max_duty
 * @return 
 */
BOOL pifDutyMotor_Init(PifDutyMotor* p_owner, PifId id, PifPulse* p_timer, uint16_t max_duty)
{
    if (!p_timer) {
		pif_error = E_INVALID_PARAM;
	    return FALSE;
	}

    p_owner->_p_timer = p_timer;
    if (id == PIF_ID_AUTO) id = pif_id++;
    p_owner->_id = id;
    p_owner->_state = MS_IDLE;
    p_owner->_max_duty = max_duty;
    return TRUE;
}

/**
 * @fn pifDutyMotor_Clear
 * @brief
 * @param p_owner
 */
void pifDutyMotor_Clear(PifDutyMotor* p_owner)
{
	if (p_owner->__p_timer_delay) {
		pifPulse_RemoveItem(p_owner->__p_timer_delay);
		p_owner->__p_timer_delay = NULL;
	}
	if (p_owner->__p_timer_break) {
		pifPulse_RemoveItem(p_owner->__p_timer_break);
		p_owner->__p_timer_break = NULL;
	}
}

/**
 * @fn pifDutyMotor_AttachAction
 * @brief
 * @param p_owner
 * @param act_set_duty
 * @param act_set_direction
 * @param act_operate_break
 */
void pifDutyMotor_AttachAction(PifDutyMotor* p_owner, PifActDutyMotorSetDuty act_set_duty,
		PifActDutyMotorSetDirection act_set_direction, PifActDutyMotorOperateBreak act_operate_break)
{
    p_owner->__act_set_duty = act_set_duty;
    p_owner->__act_set_direction = act_set_direction;
    p_owner->__act_operate_break = act_operate_break;
}

/**
 * @fn pifDutyMotor_SetDirection
 * @brief
 * @param p_owner
 * @param direction
 */
void pifDutyMotor_SetDirection(PifDutyMotor* p_owner, uint8_t direction)
{
	p_owner->_direction = direction;

	if (p_owner->__act_set_direction) (*p_owner->__act_set_direction)(p_owner->_direction);
}

/**
 * @fn pifDutyMotor_SetDuty
 * @brief
 * @param p_owner
 * @param duty
 */
void pifDutyMotor_SetDuty(PifDutyMotor* p_owner, uint16_t duty)
{
    if (duty > p_owner->_max_duty) duty = p_owner->_max_duty;
	p_owner->_current_duty = duty;

	(*p_owner->__act_set_duty)(p_owner->_current_duty);
}

/**
 * @fn pifDutyMotor_SetOperatingTime
 * @brief
 * @param p_owner
 * @param operating_time
 */
BOOL pifDutyMotor_SetOperatingTime(PifDutyMotor* p_owner, uint32_t operating_time)
{
	if (!p_owner->__p_timer_break) {
		p_owner->__p_timer_break = pifPulse_AddItem(p_owner->_p_timer, PT_ONCE);
	}
	if (p_owner->__p_timer_break) {
		pifPulse_AttachEvtFinish(p_owner->__p_timer_break, _evtTimerBreakFinish, p_owner);
		if (pifPulse_StartItem(p_owner->__p_timer_break, operating_time)) {
			return TRUE;
		}
	}
	return FALSE;
}

/**
 * @fn pifDutyMotor_Start
 * @brief
 * @param p_owner
 * @param duty
 * @return
 */
BOOL pifDutyMotor_Start(PifDutyMotor* p_owner, uint16_t duty)
{
    if (!p_owner->__act_set_duty || !p_owner->__act_set_direction) {
        pif_error = E_INVALID_PARAM;
		return FALSE;
    }

    (*p_owner->__act_set_direction)(p_owner->_direction);

   	p_owner->_current_duty = duty;

    (*p_owner->__act_set_duty)(p_owner->_current_duty);
	return TRUE;
}

/**
 * @fn pifDutyMotor_BreakRelease
 * @brief
 * @param p_owner
 * @param break_time
 */
void pifDutyMotor_BreakRelease(PifDutyMotor* p_owner, uint16_t break_time)
{
    p_owner->_current_duty = 0;

    (*p_owner->__act_set_duty)(p_owner->_current_duty);

    if (break_time && p_owner->__act_operate_break) {
	    if (!p_owner->__p_timer_break) {
	    	p_owner->__p_timer_break = pifPulse_AddItem(p_owner->_p_timer, PT_ONCE);
	    }
	    if (p_owner->__p_timer_break) {
	    	pifPulse_AttachEvtFinish(p_owner->__p_timer_break, _evtTimerBreakFinish, p_owner);
			if (pifPulse_StartItem(p_owner->__p_timer_break, break_time)) {
		    	(*p_owner->__act_operate_break)(1);
			}
	    }
	}
}

/**
 * @fn pifDutyMotor_StartControl
 * @brief 
 * @param p_owner
 * @return 
 */
BOOL pifDutyMotor_StartControl(PifDutyMotor* p_owner)
{
	if (!p_owner->__p_task) {
        pif_error = E_NOT_SET_TASK;
	    return FALSE;
    }

    p_owner->__p_task->pause = FALSE;
    return TRUE;
}

/**
 * @fn pifDutyMotor_StopControl
 * @brief
 * @param p_owner
 * @return
 */
BOOL pifDutyMotor_StopControl(PifDutyMotor* p_owner)
{
	if (!p_owner->__p_task) {
        pif_error = E_NOT_SET_TASK;
	    return FALSE;
    }

    p_owner->__p_task->pause = TRUE;
    return TRUE;
}

/**
 * @fn pifComm_AttachTask
 * @brief Task를 추가한다.
 * @param p_owner
 * @param mode Task의 Mode를 설정한다.
 * @param period Mode에 따라 주기의 단위가 변경된다.
 * @return Task 구조체 포인터를 반환한다.
 */
PifTask* pifDutyMotor_AttachTask(PifDutyMotor* p_owner, PifTaskMode mode, uint16_t period)
{
	p_owner->__p_task = pifTaskManager_Add(mode, period, _doTask, p_owner, FALSE);
	return p_owner->__p_task;
}
