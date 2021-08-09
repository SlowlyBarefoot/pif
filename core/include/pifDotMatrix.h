#ifndef PIF_DOT_MATRIX_H
#define PIF_DOT_MATRIX_H


#include "pifPulse.h"


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


typedef void (*PIF_actDotMatrixDisplay)(uint8_t ucRow, uint8_t *pucData);

typedef void (*PIF_evtDotMatrixShiftFinish)(PIF_usId usPifId);


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
typedef struct _PIF_stDotMatrix
{
	// Public Member Variable

	// Public Event Function
    PIF_evtDotMatrixShiftFinish evtShiftFinish;

	// Read-only Member Variable
	PIF_usId _usPifId;

	// Private Member Variable
    uint16_t __usColSize;
    uint16_t __usRowSize;

	uint8_t __ucPatternIndex;

	struct {
		uint8_t Run			: 1;
		uint8_t Blink		: 1;
	} __bt;

	union {
		PIF_enDotMatrixShift enShift;
		struct {
			uint8_t Direction	: 4;	// 2 : Left, 3 : Right, 4 : Up, 5 : Down
			uint8_t Method		: 4;	// 0 : Off, 2 : Repeat Hor, 3 : Repeat Ver, 4 : PingPong Hor, 5 : PingPong Ver
		} btShift;
	} __ui;

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

	// Private Action Function
   	PIF_actDotMatrixDisplay __actDisplay;
} PIF_stDotMatrix;


#ifdef __cplusplus
extern "C" {
#endif

BOOL pifDotMatrix_Init(PIF_stPulse *pstTimer, uint8_t ucSize);
void pifDotMatrix_Exit();

PIF_stDotMatrix *pifDotMatrix_Add(PIF_usId usPifId, uint16_t usColSize, uint16_t usRowSize,
		PIF_actDotMatrixDisplay actDisplay);

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
BOOL pifDotMatrix_ShiftOn(PIF_stDotMatrix *pstOwner, PIF_enDotMatrixShift enShift, uint16_t usPeriodMs, uint16_t usCount);
void pifDotMatrix_ShiftOff(PIF_stDotMatrix *pstOwner);
void pifDotMatrix_ChangeShiftPeriod(PIF_stDotMatrix *pstOwner, uint16_t usPeriodMs);

// Task Function
uint16_t pifDotMatrix_taskAll(PIF_stTask *pstTask);
uint16_t pifDotMatrix_taskEach(PIF_stTask *pstTask);

#ifdef __cplusplus
}
#endif


#endif	// PIF_DOT_MATRIX_H
