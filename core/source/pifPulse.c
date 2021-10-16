#ifndef __PIF_NO_LOG__
#include "pifLog.h"
#endif
#include "pifPulse.h"


#define PIF_PULSE_INDEX_NULL   0xFF


/**
 * @fn pifPulse_Create
 * @brief Pulse를 추가한다.
 * @param id
 * @param period1us
 * @return Pulse 구조체 포인터를 반환한다.
 */
PifPulse* pifPulse_Create(PifId id, uint32_t period1us)
{
	PifPulse* p_owner = NULL;

    if (!period1us) {
        pif_error = E_INVALID_PARAM;
        goto fail;
    }

    p_owner = calloc(sizeof(PifPulse), 1);
    if (!p_owner) {
    	pif_error = E_OUT_OF_HEAP;
		goto fail;
	}

    if (id == PIF_ID_AUTO) id = pif_id++;
    p_owner->_id = id;
    if (!pifDList_Init(&p_owner->__items)) goto fail;

    p_owner->_period1us = period1us;
    return p_owner;

fail:
	if (p_owner) free(p_owner);
    return NULL;
}

/**
 * @fn pifPulse_Destroy
 * @brief Pulse용 메모리를 반환한다.
 * @param pp_owner Pulse 자신
 */
void pifPulse_Destroy(PifPulse** pp_owner)
{
	if (*pp_owner) {
		pifDList_Clear(&(*pp_owner)->__items);
		free(*pp_owner);
		*pp_owner = NULL;
	}
}

/**
 * @fn pifPulse_AddItem
 * @brief Pulse의 항목을 추가한다.
 * @param p_owner Pulse 자신
 * @param type Pulse의 종류
 * @return Pulse 항목 구조체 포인터를 반환한다.
 */
PifPulseItem* pifPulse_AddItem(PifPulse* p_owner, PifPulseType type)
{
    PifPulseItem* p_item = (PifPulseItem*)pifDList_AddLast(&p_owner->__items, sizeof(PifPulseItem));
    if (!p_item) return NULL;

    p_item->_type = type;
    p_item->__evt_finish = NULL;
    p_item->__p_finish_issuer = NULL;

    p_item->_step = PS_STOP;
    return p_item;
}

/**
 * @fn pifPulse_RemoveItem
 * @brief Pulse 항목을 삭제한다.
 * @param p_item Pulse 항목 포인터
 */
void pifPulse_RemoveItem(PifPulseItem* p_item)
{
    p_item->_step = PS_REMOVE;
}

/**
 * @fn pifPulse_StartItem
 * @brief Pulse 항목을 시작한다.
 * @param p_item Pulse 항목 포인터
 * @param target 이동 pulse 수
 * @return
 */
BOOL pifPulse_StartItem(PifPulseItem* p_item, uint32_t target)
{
	if (!target) {
		pif_error = E_INVALID_PARAM;
	    return FALSE;
    }

    if (p_item->_step == PS_STOP) {
    	p_item->_step = PS_RUNNING;
    	p_item->__event = FALSE;
    }
    p_item->target = target;
    p_item->__current = target;

    if (p_item->_type == PT_PWM) {
    	p_item->__pwm_duty = 0;
    }
    return TRUE;
}

/**
 * @fn pifPulse_StopItem
 * @brief Pulse 항목을 종료한다.
 * @param p_item Pulse 항목 포인터
 */
void pifPulse_StopItem(PifPulseItem* p_item)
{
	p_item->__current = 0;
	p_item->_step = PS_STOP;
	if (p_item->_type == PT_PWM) {
		(*p_item->__act_pwm)(OFF);
	}
}

/**
 * @fn pifPulse_SetPwmDuty
 * @brief Pulse 항목의 이동 pulse를 재설정한다.
 * @param p_item Pulse 항목 포인터
 * @param duty 이동 pulse수. 단, 현재 동작에는 영향이 없고 다음 동작부터 변경된다.
 */
void pifPulse_SetPwmDuty(PifPulseItem* p_item, uint16_t duty)
{
	p_item->__pwm_duty = p_item->target * duty / PIF_PWM_MAX_DUTY;
	if (p_item->__pwm_duty == p_item->target) {
		(*p_item->__act_pwm)(ON);
	}
}

/**
 * @fn pifPulse_RemainItem
 * @brief Pulse 항목의 남은 pulse 수를 얻는다.
 * @param p_item Pulse 항목 포인터
 * @return 남은 pulse 수를 반환한다.
 */
uint32_t pifPulse_RemainItem(PifPulseItem* p_item)
{
	if (p_item->_step != PS_RUNNING) return 0;
	else return p_item->__current;
}

/**
 * @fn pifPulse_ElapsedItem
 * @brief Pulse 항목의 경과한 pulse 수를 얻는다.
 * @param p_item Pulse 항목 포인터
 * @return 경과한 pulse 수를 반환한다.
 */
uint32_t pifPulse_ElapsedItem(PifPulseItem* p_item)
{
	if (p_item->_step != PS_RUNNING) return 0;
	else return p_item->target - p_item->__current;
}

/**
 * @fn pifPulse_sigTick
 * @brief Pulse를 발생하는 Interrupt 함수에서 호출한다.
 * @param p_owner Pulse 자신
 */
void pifPulse_sigTick(PifPulse* p_owner)
{
	PifDListIterator it_remove = NULL;

    if (!p_owner) return;

	PifDListIterator it = pifDList_Begin(&p_owner->__items);
	while (it) {
		PifPulseItem* p_item = (PifPulseItem*)it->data;

		if (p_item->_step == PS_REMOVE) {
			if (!it_remove) it_remove = it;
		}
		else if (p_item->__current) {
			p_item->__current--;
			switch (p_item->_type) {
			case PT_ONCE:
				if (!p_item->__current) {
					p_item->_step = PS_STOP;
					p_item->__event = TRUE;
				}
				break;

			case PT_REPEAT:
				if (!p_item->__current) {
					p_item->__current = p_item->target;
					p_item->__event = TRUE;
				}
				break;

			case PT_PWM:
				if (p_item->__pwm_duty != p_item->target) {
					if (!p_item->__current) {
						(*p_item->__act_pwm)(OFF);
						p_item->__current = p_item->target;
					}
					if (p_item->__current == p_item->__pwm_duty) {
						(*p_item->__act_pwm)(ON);
					}
				}
				else {
					if (!p_item->__current) {
						p_item->__current = p_item->target;
					}
				}
				break;
			}
		}

		it = pifDList_Next(it);
	}

	if (it_remove) pifDList_Remove(&p_owner->__items, it_remove);
}

/**
 * @fn pifPulse_AttachAction
 * @brief
 * @param p_item Pulse 항목 포인터
 * @param act_pwm 연결시킬 Action
 */
void pifPulse_AttachAction(PifPulseItem* p_item, PifActPulsePwm act_pwm)
{
	p_item->__act_pwm = act_pwm;
}

/**
 * @fn pifPulse_AttachEvtFinish
 * @brief Pulse 항목의 이동 완료시 발생시킬 이벤트를 연결한다.
 * @param p_item Pulse 항목 포인터
 * @param evt_finish 연결시킬 이벤트
 * @param p_issuer 이벤트 발생시 전달할 발행자
 */
void pifPulse_AttachEvtFinish(PifPulseItem* p_item, PifEvtPulseFinish evt_finish, void* p_issuer)
{
	p_item->__evt_finish = evt_finish;
	p_item->__p_finish_issuer = p_issuer;
}

static uint16_t _doTask(PifTask* p_task)
{
	PifPulse* p_owner = p_task->_p_client;

	PifDListIterator it = pifDList_Begin(&p_owner->__items);
	while (it) {
		PifPulseItem* p_item = (PifPulseItem*)it->data;

		if (p_item->_type != PT_PWM) {
			if (p_item->__event) {
				p_item->__event = FALSE;

				if (p_item->__evt_finish) (*p_item->__evt_finish)(p_item->__p_finish_issuer);
			}
		}

		it = pifDList_Next(it);
	}
	return 0;
}

/**
 * @fn pifPulse_AttachTask
 * @brief Task를 추가한다.
 * @param p_owner
 * @param mode Task의 Mode를 설정한다.
 * @param period Mode에 따라 주기의 단위가 변경된다.
 * @param start 즉시 시작할지를 지정한다.
 * @return Task 구조체 포인터를 반환한다.
 */
PifTask* pifPulse_AttachTask(PifPulse* p_owner, PifTaskMode mode, uint16_t period, BOOL start)
{
	return pifTaskManager_Add(mode, period, _doTask, p_owner, start);
}
