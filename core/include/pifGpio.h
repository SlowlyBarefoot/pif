#ifndef PIF_GPIO_H
#define PIF_GPIO_H


#include "pifTask.h"


#define PIF_GPIO_MAX_COUNT		7


typedef enum _PIF_enGpioCsFlag
{
    GpCsF_enOff			= 0,

	GpCsF_enStateIdx	= 0,

	GpCsF_enStateBit	= 1,
	GpCsF_enAllBit		= 1,

	GpCsF_enCount		= 1
} PIF_enGpioCsFlag;


struct _PIF_stGpio;
typedef struct _PIF_stGpio PIF_stGpio;

typedef uint8_t (*PIF_actGpioIn)(PifId usPifId);
typedef void (*PIF_actGpioOut)(PifId usPifId, uint8_t ucState);

typedef void (*PIF_evtGpioIn)(uint8_t index, uint8_t state);


#ifdef __PIF_COLLECT_SIGNAL__

typedef struct
{
	PIF_stGpio* p_owner;
    uint8_t flag;
    void* p_device[GpCsF_enCount];
} PIF_GpioColSig;

#endif

/**
 * @class _PIF_stGpio
 * @brief
 */
struct _PIF_stGpio
{
	// Public Member Variable
    uint8_t ucGpioCount;

	// Public Event Function
	PIF_evtGpioIn evtIn;

	// Read-only Member Variable
	PifId _usPifId;

	// Private Member Variable
	uint8_t __read_state;
	uint8_t __write_state;

#ifdef __PIF_COLLECT_SIGNAL__
	PIF_GpioColSig* __p_colsig;
#endif

	// Private Action Function
	union {
		PIF_actGpioIn actIn;
		PIF_actGpioOut actOut;
	} __ui;
};


#ifdef __cplusplus
extern "C" {
#endif

PIF_stGpio* pifGpio_Create(PifId usPifId, uint8_t ucCount);
void pifGpio_Destroy(PIF_stGpio** pp_owner);

uint8_t pifGpio_ReadAll(PIF_stGpio *pstOwner);
SWITCH pifGpio_ReadCell(PIF_stGpio *pstOwner, uint8_t ucIndex);

BOOL pifGpio_WriteAll(PIF_stGpio *pstOwner, uint8_t ucState);
BOOL pifGpio_WriteCell(PIF_stGpio *pstOwner, uint8_t ucIndex, SWITCH swState);

#ifdef __PIF_COLLECT_SIGNAL__

void pifGpio_SetCsFlagAll(PIF_enGpioCsFlag enFlag);
void pifGpio_ResetCsFlagAll(PIF_enGpioCsFlag enFlag);

void pifGpio_SetCsFlagEach(PIF_stGpio *pstOwner, PIF_enGpioCsFlag enFlag);
void pifGpio_ResetCsFlagEach(PIF_stGpio *pstOwner, PIF_enGpioCsFlag enFlag);

#endif

// Signal Function
void pifGpio_sigData(PIF_stGpio *p_owner, uint8_t index, SWITCH state);

// Attach Action Function
void pifGpio_AttachActIn(PIF_stGpio* p_owner, PIF_actGpioIn act_in);
void pifGpio_AttachActOut(PIF_stGpio* p_owner, PIF_actGpioOut act_out);

// Task Function
PIF_stTask *pifGpio_AttachTaskIn(PIF_stGpio *p_owner, PIF_enTaskMode mode, uint16_t period, BOOL start);

#ifdef __cplusplus
}
#endif


#endif  // PIF_GPIO_H
