#ifndef PIF_DOT_MATRIX_H
#define PIF_DOT_MATRIX_H


#include "pifPulse.h"


typedef enum EnPifDotMatrixShiftDir
{
	DMSD_NONE			= 0,
	DMSD_LEFT			= 2,
	DMSD_RIGHT			= 3,
	DMSD_UP				= 4,
	DMSD_DOWN			= 5,
} PifDotMatrixShiftDir;

typedef enum EnPifDotMatrixShiftMethod
{
	DMSM_ONCE			= 0,
	DMSM_REPEAT_HOR		= 2,
	DMSM_REPEAT_VER		= 3,
	DMSM_PING_PONG_HOR	= 4,
	DMSM_PING_PONG_VER	= 5
} PifDotMatrixShiftMethod;


typedef void (*PifActDotMatrixDisplay)(uint8_t row, uint8_t* p_data);

typedef void (*PifEvtDotMatrixShiftFinish)(PifId id);


/**
 * @class StPifDotMatrixPattern
 * @brief
 */
typedef struct StPifDotMatrixPattern
{
	uint8_t col_size;
	uint8_t col_bytes;
	uint8_t row_size;
	uint8_t* p_pattern;
} PifDotMatrixPattern;

/**
 * @class StPifDotMatrix
 * @brief
 */
typedef struct StPifDotMatrix
{
	// Public Member Variable

	// Public Event Function
    PifEvtDotMatrixShiftFinish evt_shift_finish;

	// Read-only Member Variable
    PifId _id;

	// Private Member Variable
	PifPulse* __p_timer;
    uint16_t __col_size;
    uint16_t __row_size;

	uint8_t __pattern_index;

	struct {
		uint8_t run			: 1;
		uint8_t blink		: 1;
		uint8_t led			: 1;
	} __bt;

	PifDotMatrixShiftDir __shift_direction;
	PifDotMatrixShiftMethod __shift_method;

    uint16_t __col_bytes;
    uint16_t __total_bytes;

    uint8_t __pattern_size;
    uint8_t __pattern_count;
    PifDotMatrixPattern* __p_pattern;
    uint8_t* __p_paper;

	uint8_t __row_index;
	uint16_t __position_x;
	uint16_t __position_y;
	uint16_t __shift_count;

	PifPulseItem* __p_timer_blink;
	PifPulseItem* __p_timer_shift;

	// Private Action Function
   	PifActDotMatrixDisplay __act_display;
} PifDotMatrix;


#ifdef __cplusplus
extern "C" {
#endif

PifDotMatrix* pifDotMatrix_Create(PifId id, PifPulse* p_timer, uint16_t col_size, uint16_t row_size,
		PifActDotMatrixDisplay act_display);
void pifDotMatrix_Destroy(PifDotMatrix** pp_owner);

BOOL pifDotMatrix_Init(PifDotMatrix* p_owner, PifId id, PifPulse* p_timer, uint16_t col_size, uint16_t row_size,
		PifActDotMatrixDisplay act_display);
void pifDotMatrix_Clear(PifDotMatrix* p_owner);

BOOL pifDotMatrix_SetPatternSize(PifDotMatrix* p_owner, uint8_t size);
BOOL pifDotMatrix_AddPattern(PifDotMatrix* p_owner, uint8_t col_size, uint8_t row_size, uint8_t* p_pattern);

void pifDotMatrix_Start(PifDotMatrix* p_owner);
void pifDotMatrix_Stop(PifDotMatrix* p_owner);

BOOL pifDotMatrix_SelectPattern(PifDotMatrix* p_owner, uint8_t pattern_index);

BOOL pifDotMatrix_BlinkOn(PifDotMatrix* p_owner, uint16_t period1ms);
void pifDotMatrix_BlinkOff(PifDotMatrix* p_owner);
void pifDotMatrix_ChangeBlinkPeriod(PifDotMatrix* p_owner, uint16_t period1ms);

BOOL pifDotMatrix_SetPosition(PifDotMatrix* p_owner, uint16_t pos_x, uint16_t pos_y);
BOOL pifDotMatrix_ShiftOn(PifDotMatrix* p_owner, PifDotMatrixShiftDir shift_direction,
		PifDotMatrixShiftMethod shift_method, uint16_t period1ms, uint16_t count);
void pifDotMatrix_ShiftOff(PifDotMatrix* p_owner);
void pifDotMatrix_ChangeShiftPeriod(PifDotMatrix* p_owner, uint16_t period1ms);

// Task Function
PifTask* pifDotMatrix_AttachTask(PifDotMatrix* p_owner, PifTaskMode mode, uint16_t period, BOOL start);

#ifdef __cplusplus
}
#endif


#endif	// PIF_DOT_MATRIX_H
