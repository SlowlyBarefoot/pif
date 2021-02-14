#ifndef __PIF_NO_LOG__
#include "pifLog.h"
#endif
#include "pifSensor.h"


/**
 * @fn pifSensor_AttachAction
 * @brief
 * @param pstOwner
 * @param actAcquire
 */
void pifSensor_AttachAction(PIF_stSensor *pstOwner, PIF_actSensorAcquire actAcquire)
{
	pstOwner->__actAcquire = actAcquire;
}

/**
 * @fn pifSensor_AttachEvtChange
 * @brief
 * @param pstOwner
 * @param evtChange
 * @param pvIssuer
 */
void pifSensor_AttachEvtChange(PIF_stSensor *pstOwner, PIF_evtSensorChange evtChange, void *pvIssuer)
{
	pstOwner->__evtChange = evtChange;
	pstOwner->__pvChangeIssuer = pvIssuer;
}

/**
 * @fn pifSensor_DetachEvtChange
 * @brief
 * @param pstOwner
 */
void pifSensor_DetachEvtChange(PIF_stSensor *pstOwner)
{
	pstOwner->__evtChange = NULL;
	pstOwner->__pvChangeIssuer = NULL;
}
