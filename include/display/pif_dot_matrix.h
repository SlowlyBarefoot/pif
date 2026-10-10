// SPDX-License-Identifier: BSD-3-Clause
#ifndef PIF_DOT_MATRIX_H
#define PIF_DOT_MATRIX_H


#include "core/pif_task_manager.h"
#include "core/pif_timer_manager.h"


// Time in ms to scan all rows once (one frame). Each row gets this divided by the row count.
#define PIF_DOT_MATRIX_FRAME_PERIOD	25


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


/**
 * @brief Outputs one row of the display.
 * @param row Row index to output.
 * @param p_data Row data of (col_size - 1) / 8 + 1 bytes. Bit 0 of the first byte is the leftmost column.
 */
typedef void (*PifActDotMatrixDisplay)(uint8_t row, uint8_t* p_data);

typedef void (*PifEvtDotMatrixShiftFinish)(PifId id);


/**
 * @class StPifDotMatrixPattern
 * @brief One drawable pattern entry stored in the dot-matrix pattern list.
 */
typedef struct StPifDotMatrixPattern
{
	uint16_t col_size;
	uint16_t col_bytes;
	uint16_t row_size;
	uint8_t* p_pattern;
} PifDotMatrixPattern;

/**
 * @class StPifDotMatrix
 * @brief Dot-matrix display controller with pattern, blink, and shift features.
 */
typedef struct StPifDotMatrix
{
	// Public Member Variable

	// Public Event Function
    PifEvtDotMatrixShiftFinish evt_shift_finish;

	// Read-only Member Variable
    PifId _id;

	// Private Member Variable
	PifTimerManager* __p_timer_manager;
	PifTask* __p_task;
    uint16_t __col_size;
    uint16_t __row_size;
    uint16_t __frame_period_1ms;	// PIF_DOT_MATRIX_FRAME_PERIOD

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
    uint8_t* __p_paper;				// __total_bytes of display data, then one blank row

	uint16_t __row_index;
	uint16_t __position_x;
	uint16_t __position_y;
	uint16_t __shift_count;

	PifTimer* __p_timer_blink;
	PifTimer* __p_timer_shift;

	// Private Action Function
   	PifActDotMatrixDisplay __act_display;
} PifDotMatrix;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifDotMatrix_Init
 * @brief Initializes a dot-matrix controller and display output callback.
 * @param p_owner Pointer to the dot-matrix instance to initialize.
 * @param id Unique object identifier. Use `PIF_ID_AUTO` to assign one automatically.
 * @param p_timer_manager Timer manager used for blink and shift timing.
 * @param col_size Logical column size of the display area.
 * @param row_size Logical row size of the display area.
 * @param act_display Callback used to output one row of matrix data.
 * @return `TRUE` if initialization succeeds, otherwise `FALSE`.
 */
BOOL pifDotMatrix_Init(PifDotMatrix* p_owner, PifId id, PifTimerManager* p_timer_manager, uint16_t col_size, uint16_t row_size,
		PifActDotMatrixDisplay act_display);

/**
 * @fn pifDotMatrix_Clear
 * @brief Releases all runtime resources allocated by the dot-matrix controller.
 * @param p_owner Pointer to the dot-matrix instance to clear.
 */
void pifDotMatrix_Clear(PifDotMatrix* p_owner);

/**
 * @fn pifDotMatrix_SetPatternSize
 * @brief Allocates storage for a fixed number of pattern entries.
 *        Patterns added before are released, any shift is stopped, and the position goes back to (0, 0).
 * @param p_owner Pointer to an initialized dot-matrix instance.
 * @param size Number of pattern slots to allocate. Must not be 0.
 * @return `TRUE` if allocation succeeds, otherwise `FALSE`.
 */
BOOL pifDotMatrix_SetPatternSize(PifDotMatrix* p_owner, uint8_t size);

/**
 * @fn pifDotMatrix_AddPattern
 * @brief Adds one pattern bitmap to the internal pattern list.
 * @param p_owner Pointer to an initialized dot-matrix instance.
 * @param col_size Pattern width in columns. Must not be less than the display width.
 * @param row_size Pattern height in rows. Must not be less than the display height.
 * @param p_pattern Pointer to packed pattern bitmap data, (col_size - 1) / 8 + 1 bytes per row.
 * @return `TRUE` if the pattern is added successfully, otherwise `FALSE`.
 */
BOOL pifDotMatrix_AddPattern(PifDotMatrix* p_owner, uint16_t col_size, uint16_t row_size, uint8_t* p_pattern);

/**
 * @fn pifDotMatrix_GetFramePeriod
 * @brief Returns the time to scan all rows once (one frame).
 * @param p_owner Pointer to an initialized dot-matrix instance.
 * @return Frame period in milliseconds.
 */
uint16_t pifDotMatrix_GetFramePeriod(PifDotMatrix* p_owner);

/**
 * @fn pifDotMatrix_SetFramePeriod
 * @brief Sets the time to scan all rows once (one frame). Each row is output every period1ms / row_size.
 * @param p_owner Pointer to an initialized dot-matrix instance.
 * @param period1ms Frame period in milliseconds.
 * @return `TRUE` if period update succeeds, otherwise `FALSE`.
 */
BOOL pifDotMatrix_SetFramePeriod(PifDotMatrix* p_owner, uint16_t period1ms);

/**
 * @fn pifDotMatrix_Start
 * @brief Starts periodic matrix refresh output.
 * @param p_owner Pointer to an initialized dot-matrix instance.
 */
void pifDotMatrix_Start(PifDotMatrix* p_owner);

/**
 * @fn pifDotMatrix_Stop
 * @brief Turns all rows off and stops periodic matrix refresh output. Blink and shift are stopped too.
 * @param p_owner Pointer to an initialized dot-matrix instance.
 */
void pifDotMatrix_Stop(PifDotMatrix* p_owner);

/**
 * @fn pifDotMatrix_SelectPattern
 * @brief Selects which registered pattern is currently rendered.
 *        A position outside the new pattern is moved to its nearest edge.
 * @param p_owner Pointer to an initialized dot-matrix instance.
 * @param pattern_index Zero-based index of the pattern to display.
 * @return `TRUE` if the index is valid and selected, otherwise `FALSE`.
 */
BOOL pifDotMatrix_SelectPattern(PifDotMatrix* p_owner, uint8_t pattern_index);

/**
 * @fn pifDotMatrix_BlinkOn
 * @brief Enables blink mode for the dot-matrix output.
 * @param p_owner Pointer to an initialized dot-matrix instance.
 * @param period1ms Blink period in milliseconds.
 * @return `TRUE` if blink timer starts successfully, otherwise `FALSE`.
 */
BOOL pifDotMatrix_BlinkOn(PifDotMatrix* p_owner, uint16_t period1ms);

/**
 * @fn pifDotMatrix_BlinkOff
 * @brief Disables blink mode and keeps display continuously active.
 * @param p_owner Pointer to an initialized dot-matrix instance.
 */
void pifDotMatrix_BlinkOff(PifDotMatrix* p_owner);

/**
 * @fn pifDotMatrix_ChangeBlinkPeriod
 * @brief Changes blink timer period while blink mode is active.
 * @param p_owner Pointer to an initialized dot-matrix instance.
 * @param period1ms New blink period in milliseconds.
 * @return `TRUE` if the period is changed, otherwise `FALSE`.
 */
BOOL pifDotMatrix_ChangeBlinkPeriod(PifDotMatrix* p_owner, uint16_t period1ms);

/**
 * @fn pifDotMatrix_SetPosition
 * @brief Sets top-left render position for the selected pattern and redraws it.
 * @param p_owner Pointer to an initialized dot-matrix instance.
 * @param pos_x Horizontal start position, from 0 to pattern width - display width.
 * @param pos_y Vertical start position, from 0 to pattern height - display height.
 * @return `TRUE` if position is valid and applied, otherwise `FALSE`.
 */
BOOL pifDotMatrix_SetPosition(PifDotMatrix* p_owner, uint16_t pos_x, uint16_t pos_y);

/**
 * @fn pifDotMatrix_ShiftOn
 * @brief Starts automatic pattern shifting with direction and repetition policy.
 *        evt_shift_finish is called when the shift ends by itself.
 * @param p_owner Pointer to an initialized dot-matrix instance.
 * @param shift_direction Direction of movement for each shift step.
 * @param shift_method Shift repetition behavior (once, repeat, or ping-pong).
 *        The `_HOR` methods go with `DMSD_LEFT`/`DMSD_RIGHT` and the `_VER` ones with `DMSD_UP`/`DMSD_DOWN`.
 * @param period1ms Shift timer period in milliseconds.
 * @param count Number of shift steps before stopping. 0 shifts until the method ends it.
 * @return `TRUE` if shift operation starts successfully, otherwise `FALSE`.
 */
BOOL pifDotMatrix_ShiftOn(PifDotMatrix* p_owner, PifDotMatrixShiftDir shift_direction,
		PifDotMatrixShiftMethod shift_method, uint16_t period1ms, uint16_t count);

/**
 * @fn pifDotMatrix_ShiftOff
 * @brief Stops automatic pattern shifting and redraws the pattern at position (0, 0).
 * @param p_owner Pointer to an initialized dot-matrix instance.
 */
void pifDotMatrix_ShiftOff(PifDotMatrix* p_owner);

/**
 * @fn pifDotMatrix_ChangeShiftPeriod
 * @brief Changes shift timer period while shift mode is active.
 * @param p_owner Pointer to an initialized dot-matrix instance.
 * @param period1ms New shift period in milliseconds.
 * @return `TRUE` if the period is changed, otherwise `FALSE`.
 */
BOOL pifDotMatrix_ChangeShiftPeriod(PifDotMatrix* p_owner, uint16_t period1ms);

#ifdef __cplusplus
}
#endif


#endif	// PIF_DOT_MATRIX_H
