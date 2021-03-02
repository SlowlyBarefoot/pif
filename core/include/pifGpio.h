#ifndef PIF_GPIO_H
#define PIF_GPIO_H


#include "pif.h"


typedef enum _PIF_enGpioCsFlag
{
    GpCsF_enOff			= 0,

	GpCsF_enStateIdx	= 0,

	GpCsF_enStateBit	= 1,
	GpCsF_enAllBit		= 1,

	GpCsF_enCount		= 1
} PIF_enGpioCsFlag;


typedef uint8_t (*PIF_actGpioIn)(PIF_usId usPifId);
typedef void (*PIF_actGpioOut)(PIF_usId usPifId, uint8_t ucState);


/**
 * @class _PIF_stGpio
 * @brief
 */
typedef struct _PIF_stGpio
{
	// Public Member Variable
    uint8_t ucGpioCount;

	// Read-only Member Variable
	PIF_usId _usPifId;

	// Private Member Variable
	uint8_t __ucIndex;
	uint8_t __ucState;

#ifdef __PIF_COLLECT_SIGNAL__
    uint8_t __ucCsFlag;
    int8_t __cCsIndex[GpCsF_enCount];
#endif

	// Private Action Function
	union {
		PIF_actGpioIn __actIn;
		PIF_actGpioOut __actOut;
	};
} PIF_stGpio;


#ifdef __cplusplus
extern "C" {
#endif

BOOL pifGpio_Init(uint8_t ucSize);
void pifGpio_Exit();

PIF_stGpio *pifGpio_AddIn(PIF_usId usPifId, uint8_t ucCount, PIF_actGpioIn actIn);
PIF_stGpio *pifGpio_AddOut(PIF_usId usPifId, uint8_t ucCount, PIF_actGpioOut actOut);

uint8_t pifGpio_ReadAll(PIF_stGpio *pstOwner);
SWITCH pifGpio_ReadBit(PIF_stGpio *pstOwner, uint8_t ucIndex);

void pifGpio_WriteAll(PIF_stGpio *pstOwner, uint8_t ucState);
void pifGpio_WriteBit(PIF_stGpio *pstOwner, uint8_t ucIndex, SWITCH swState);

#ifdef __PIF_COLLECT_SIGNAL__

void pifGpio_SetCsFlagAll(PIF_enGpioCsFlag enFlag);
void pifGpio_ResetCsFlagAll(PIF_enGpioCsFlag enFlag);

void pifGpio_SetCsFlagEach(PIF_stGpio *pstSensor, PIF_enGpioCsFlag enFlag);
void pifGpio_ResetCsFlagEach(PIF_stGpio *pstSensor, PIF_enGpioCsFlag enFlag);

#endif

#ifdef __cplusplus
}
#endif


#endif  // PIF_GPIO_H
