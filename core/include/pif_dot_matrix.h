#ifndef PIF_DOT_MATRIX_H
#define PIF_DOT_MATRIX_H


#include "pif_pulse.h"


#define PIF_DOT_MATRIX_DEFAULT_PERIOD_PER_ROW_1MS	25


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
	PifTask* __p_task;
    uint16_t __col_size;
    uint16_t __row_size;
    uint16_t __period_per_row_1ms;	// PIF_DOT_MATRIX_DEFAULT_PERIOD_PER_ROW_1MS

	uint8_t __pattern_index;

	struct {
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

/**
 * @fn pifDotMatrix_Create
 * @brief
 * @param id
 * @param p_timer
 * @param col_size
 * @param row_size
 * @param act_display
 * @return
 */
PifDotMatrix* pifDotMatrix_Create(PifId id, PifPulse* p_timer, uint16_t col_size, uint16_t row_size,
		PifActDotMatrixDisplay act_display);

/**
 * @fn pifDotMatrix_Destroy
 * @brief
 * @param pp_owner
 */
void pifDotMatrix_Destroy(PifDotMatrix** pp_owner);

/**
 * @fn pifDotMatrix_Init
 * @brief
 * @param p_owner
 * @param id
 * @param p_timer
 * @param col_size
 * @param row_size
 * @param act_display
 * @return
 */
BOOL pifDotMatrix_Init(PifDotMatrix* p_owner, PifId id, PifPulse* p_timer, uint16_t col_size, uint16_t row_size,
		PifActDotMatrixDisplay act_display);

/**
 * @fn pifDotMatrix_Clear
 * @brief
 * @param p_owner
 */
void pifDotMatrix_Clear(PifDotMatrix* p_owner);

/**
 * @fn pifDotMatrix_SetPatternSize
 * @brief
 * @param p_owner
 * @param size
 * @return
 */
BOOL pifDotMatrix_SetPatternSize(PifDotMatrix* p_owner, uint8_t size);

/**
 * @fn pifDotMatrix_AddPattern
 * @brief
 * @param p_owner
 * @param col_size
 * @param row_size
 * @param p_pattern
 * @return
 */
BOOL pifDotMatrix_AddPattern(PifDotMatrix* p_owner, uint8_t col_size, uint8_t row_size, uint8_t* p_pattern);

/**
 * @fn pifDotMatrix_GetPeriodPerRow1ms
 * @brief
 * @param p_owner
 * @return
 */
uint16_t pifDotMatrix_GetPeriodPerRow1ms(PifDotMatrix* p_owner);

/**
 * @fn pifDotMatrix_SetPeriodPerRow1ms
 * @brief
 * @param p_owner
 * @param period1ms
 * @return
 */
BOOL pifDotMatrix_SetPeriodPerRow1ms(PifDotMatrix* p_owner, uint16_t period1ms);

/**
 * @fn pifDotMatrix_Start
 * @brief
 * @param p_owner
 */
void pifDotMatrix_Start(PifDotMatrix* p_owner);

/**
 * @fn pifDotMatrix_Stop
 * @brief
 * @param p_owner
 */
void pifDotMatrix_Stop(PifDotMatrix* p_owner);

/**
 * @fn pifDotMatrix_SelectPattern
 * @brief
 * @param p_owner
 * @param pattern_index
 * @return
 */
BOOL pifDotMatrix_SelectPattern(PifDotMatrix* p_owner, uint8_t pattern_index);

/**
 * @fn pifDotMatrix_BlinkOn
 * @brief
 * @param p_owner
 * @param period1ms
 * @return
 */
BOOL pifDotMatrix_BlinkOn(PifDotMatrix* p_owner, uint16_t period1ms);

/**
 * @fn pifDotMatrix_BlinkOff
 * @brief
 * @param p_owner
 */
void pifDotMatrix_BlinkOff(PifDotMatrix* p_owner);

/**
 * @fn pifDotMatrix_ChangeBlinkPeriod
 * @brief
 * @param p_owner
 * @param period1ms
 */
void pifDotMatrix_ChangeBlinkPeriod(PifDotMatrix* p_owner, uint16_t period1ms);

/**
 * @fn pifDotMatrix_SetPosition
 * @brief
 * @param p_owner
 * @param pos_x
 * @param pos_y
 * @return
 */
BOOL pifDotMatrix_SetPosition(PifDotMatrix* p_owner, uint16_t pos_x, uint16_t pos_y);

/**
 * @fn pifDotMatrix_ShiftOn
 * @brief
 * @param p_owner
 * @param shift_direction
 * @param shift_method
 * @param period1ms
 * @param count
 * @return
 */
BOOL pifDotMatrix_ShiftOn(PifDotMatrix* p_owner, PifDotMatrixShiftDir shift_direction,
		PifDotMatrixShiftMethod shift_method, uint16_t period1ms, uint16_t count);

/**
 * @fn pifDotMatrix_ShiftOff
 * @brief
 * @param p_owner
 */
void pifDotMatrix_ShiftOff(PifDotMatrix* p_owner);

/**
 * @fn pifDotMatrix_ChnageShiftPeriod
 * @brief
 * @param p_owner
 * @param period1ms
 */
void pifDotMatrix_ChangeShiftPeriod(PifDotMatrix* p_owner, uint16_t period1ms);

#ifdef __cplusplus
}
#endif


#endif	// PIF_DOT_MATRIX_H
