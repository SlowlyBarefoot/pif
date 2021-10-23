#ifndef PIF_GPIO_H
#define PIF_GPIO_H


#include "pifTask.h"


#define PIF_GPIO_MAX_COUNT		7


typedef enum EnPifGpioCsFlag
{
    GP_CSF_OFF			= 0,

	GP_CSF_STATE_IDX	= 0,

	GP_CSF_STATE_BIT	= 1,
	GP_CSF_ALL_BIT		= 1,

	GP_CSF_COUNT		= 1
} PifGpioCsFlag;


struct StPifGpio;
typedef struct StPifGpio PifGpio;

typedef uint8_t (*PifActGpioIn)(PifId id);
typedef void (*PifActGpioOut)(PifId id, uint8_t state);

typedef void (*PifEvtGpioIn)(uint8_t index, uint8_t state);


#ifdef __PIF_COLLECT_SIGNAL__

typedef struct
{
	PifGpio* p_owner;
    uint8_t flag;
    void* p_device[GP_CSF_COUNT];
} PIF_GpioColSig;

#endif

/**
 * @class StPifGpio
 * @brief
 */
struct StPifGpio
{
	// Public Member Variable
    uint8_t count;

	// Public Event Function
	PifEvtGpioIn evt_in;

	// Read-only Member Variable
	PifId _id;

	// Private Member Variable
	uint8_t __read_state;
	uint8_t __write_state;

#ifdef __PIF_COLLECT_SIGNAL__
	PIF_GpioColSig* __p_colsig;
#endif

	// Private Action Function
	union {
		PifActGpioIn act_in;
		PifActGpioOut act_out;
	} __ui;
};


#ifdef __cplusplus
extern "C" {
#endif

PifGpio* pifGpio_Create(PifId id, uint8_t count);
void pifGpio_Destroy(PifGpio** pp_owner);

BOOL pifGpio_Init(PifGpio* p_owner, PifId id, uint8_t count);
void pifGpio_Clear(PifGpio* p_owner);

uint8_t pifGpio_ReadAll(PifGpio* p_owner);
SWITCH pifGpio_ReadCell(PifGpio* p_owner, uint8_t index);

BOOL pifGpio_WriteAll(PifGpio* p_owner, uint8_t state);
BOOL pifGpio_WriteCell(PifGpio* p_owner, uint8_t index, SWITCH state);

#ifdef __PIF_COLLECT_SIGNAL__

void pifGpio_SetCsFlagAll(PifGpioCsFlag flag);
void pifGpio_ResetCsFlagAll(PifGpioCsFlag flag);

void pifGpio_SetCsFlagEach(PifGpio* p_owner, PifGpioCsFlag flag);
void pifGpio_ResetCsFlagEach(PifGpio* p_owner, PifGpioCsFlag flag);

#endif

// Signal Function
void pifGpio_sigData(PifGpio* p_owner, uint8_t index, SWITCH state);

// Attach Action Function
void pifGpio_AttachActIn(PifGpio* p_owner, PifActGpioIn act_in);
void pifGpio_AttachActOut(PifGpio* p_owner, PifActGpioOut act_out);

// Task Function
PifTask* pifGpio_AttachTaskIn(PifGpio* p_owner, PifTaskMode mode, uint16_t period, BOOL start);

#ifdef __cplusplus
}
#endif


#endif  // PIF_GPIO_H
