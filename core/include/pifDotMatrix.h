#ifndef PIF_DOT_MATRIX_H
#define PIF_DOT_MATRIX_H


#include "pifPulse.h"


#define PIF_DM_CONTROL_PERIOD_DEFAULT		2


typedef enum _PIF_enDotMatrixShift
{
	DMS_enNone			= 0x00,

	DMS_enLeft			= 0x02,
	DMS_enRight			= 0x03,
	DMS_enUp			= 0x04,
	DMS_enDown			= 0x05,

	DMS_enRepeatHor		= 0x20,
	DMS_enRepeatVer		= 0x30,
	DMS_enPingPongHor	= 0x40,
	DMS_enPingPongVer	= 0x50,

	DMS_btRepeatHor		= 0x02,
	DMS_btRepeatVer		= 0x03,
	DMS_btPingPongHor	= 0x04,
	DMS_btPingPongVer	= 0x05
} PIF_enDotMatrixShift;


struct _PIF_stDotMatrix;
typedef struct _PIF_stDotMatrix PIF_stDotMatrix;

typedef void (*PIF_actDotMatrixDisplaySingle)(uint8_t ucCol, uint8_t ucRow, uint8_t ucData, uint8_t ucColor);
typedef void (*PIF_actDotMatrixDisplayMulti)(uint8_t ucColBlock, uint8_t ucRowBlock, uint8_t ucCol, uint8_t ucRow, uint8_t ucData, uint8_t ucColor);

typedef void (*PIF_evtDotMatrixShiftFinish)(PIF_stDotMatrix *pstOwner);


/**
 * @class _PIF_stDotMatrixPattern
 * @brief
 */
typedef struct _PIF_stDotMatrixPattern
{
	uint8_t ucColSize;
	uint8_t ucColBytes;
	uint8_t ucRowSize;
	uint8_t *pucPattern;
} PIF_stDotMatrixPattern;

/**
 * @class _PIF_stDotMatrix
 * @brief
 */
struct _PIF_stDotMatrix
{
	// Public Member Variable
	PIF_unDeviceCode unDeviceCode;

    uint8_t ucUnitColSize;
    uint8_t ucUnitRowSize;
    uint8_t ucBlockColSize;
    uint8_t ucBlockRowSize;
    uint16_t usColSize;
    uint16_t usRowSize;

	uint8_t ucPatternIndex;

	struct {
		uint8_t btRun			: 1;
		uint8_t btMulti			: 1;
		uint8_t btBlink			: 1;
	    uint8_t btColorCount	: 5;
	};

	union {
		PIF_enDotMatrixShift enShift;
		struct {
			uint8_t btDirection	: 4;	// 2 : Left, 3 : Right, 4 : Up, 5 : Down
			uint8_t btMethod	: 4;	// 0 : Off, 2 : Repeat Hor, 3 : Repeat Ver, 4 : PingPong Hor, 5 : PingPong Ver
		} btShift;
	};

	// Private Member Variable
    uint8_t __ucUnitColBytes;
    uint8_t __ucUnitBytes;
    uint16_t __usColBytes;
    uint16_t __usTotalBytes;

    uint8_t __ucPatternSize;
    uint8_t __ucPatternCount;
    PIF_stDotMatrixPattern *__pstPattern;
    uint8_t *__pucPattern;

	uint8_t __ucRowIndex;
	uint16_t __usPositionX;
	uint16_t __usPositionY;
	uint16_t __usShiftCount;

    uint16_t __usControlPeriodMs;
	uint16_t __usPretimeMs;

	PIF_stPulseItem *__pstTimerBlink;
	PIF_stPulseItem *__pstTimerShift;

	PIF_enTaskLoop __enTaskLoop;

	// Private Member Function
    union {
    	PIF_actDotMatrixDisplaySingle __actDisplaySingle;
    	PIF_actDotMatrixDisplayMulti __actDisplayMulti;
    };
    PIF_evtDotMatrixShiftFinish __evtShiftFinish;
};


#ifdef __cplusplus
extern "C" {
#endif

BOOL pifDotMatrix_Init(PIF_stPulse *pstTimer1ms, uint8_t ucSize);
void pifDotMatrix_Exit();

PIF_stDotMatrix *pifDotMatrix_AddSingle(PIF_unDeviceCode unDeviceCode, uint8_t ucUnitColSize, uint8_t ucUnitRowSize,
		uint8_t ucColorCount, PIF_actDotMatrixDisplaySingle actDisplay);
PIF_stDotMatrix *pifDotMatrix_AddMulti(PIF_unDeviceCode unDeviceCode, uint8_t ucUnitColSize, uint8_t ucUnitRowSize,
		uint8_t ucBlockColSize, uint8_t ucBlockRowSize, uint8_t ucColorCount, PIF_actDotMatrixDisplayMulti actDisplay);

BOOL pifDotMatrix_SetControlPeriod(PIF_stDotMatrix *pstOwner, uint16_t usPeriodMs);

BOOL pifDotMatrix_SetPatternSize(PIF_stDotMatrix *pstOwner, uint8_t ucSize);
BOOL pifDotMatrix_AddPattern(PIF_stDotMatrix *pstOwner, uint8_t ucColSize, uint8_t ucRowSize, uint8_t *pucPattern);

void pifDotMatrix_Start(PIF_stDotMatrix *pstOwner);
void pifDotMatrix_Stop(PIF_stDotMatrix *pstOwner);

BOOL pifDotMatrix_SelectPattern(PIF_stDotMatrix *pstOwner, uint8_t ucPatternIndex);

BOOL pifDotMatrix_BlinkOn(PIF_stDotMatrix *pstOwner, uint16_t usPeriodMs);
void pifDotMatrix_BlinkOff(PIF_stDotMatrix *pstOwner);
void pifDotMatrix_ChangeBlinkPeriod(PIF_stDotMatrix *pstOwner, uint16_t usPeriodMs);

BOOL pifDotMatrix_SetPosition(PIF_stDotMatrix *pstOwner, uint16_t usX, uint16_t usY);
void pifDotMatrix_AttachEvtShiftFinish(PIF_stDotMatrix *pstOwner, PIF_evtDotMatrixShiftFinish evtShiftFinish);
BOOL pifDotMatrix_ShiftOn(PIF_stDotMatrix *pstOwner, PIF_enDotMatrixShift enShift, uint16_t usPeriodMs, uint16_t usCount);
void pifDotMatrix_ShiftOff(PIF_stDotMatrix *pstOwner);
void pifDotMatrix_ChangeShiftPeriod(PIF_stDotMatrix *pstOwner, uint16_t usPeriodMs);

// Task Function
void pifDotMatrix_LoopAll(PIF_stTask *pstTask);
void pifDotMatrix_LoopEach(PIF_stTask *pstTask);

#ifdef __cplusplus
}
#endif


#endif	// PIF_DOT_MATRIX_H
