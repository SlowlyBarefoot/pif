#ifndef PIF_DOT_MATRIX_H
#define PIF_DOT_MATRIX_H


#include "core/pif_task_manager.h"
#include "core/pif_timer_manager.h"


#define PIF_DOT_MATRIX_PERIOD_PER_ROW	25


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
 * @brief One drawable pattern entry stored in the dot-matrix pattern list.
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
    uint16_t __period_per_row_1ms;	// PIF_DOT_MATRIX_PERIOD_PER_ROW

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
 * @param p_timer_manager Timer manager used for refresh, blink, and shift timing.
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
 * @param p_owner Pointer to an initialized dot-matrix instance.
 * @param size Number of pattern slots to allocate.
 * @return `TRUE` if allocation succeeds, otherwise `FALSE`.
 */
BOOL pifDotMatrix_SetPatternSize(PifDotMatrix* p_owner, uint8_t size);

/**
 * @fn pifDotMatrix_AddPattern
 * @brief Adds one pattern bitmap to the internal pattern list.
 * @param p_owner Pointer to an initialized dot-matrix instance.
 * @param col_size Pattern width in columns.
 * @param row_size Pattern height in rows.
 * @param p_pattern Pointer to packed pattern bitmap data.
 * @return `TRUE` if the pattern is added successfully, otherwise `FALSE`.
 */
BOOL pifDotMatrix_AddPattern(PifDotMatrix* p_owner, uint8_t col_size, uint8_t row_size, uint8_t* p_pattern);

/**
 * @fn pifDotMatrix_GetPeriodPerRow
 * @brief Returns current scan period per row.
 * @param p_owner Pointer to an initialized dot-matrix instance.
 * @return Row scan period in milliseconds.
 */
uint16_t pifDotMatrix_GetPeriodPerRow(PifDotMatrix* p_owner);

/**
 * @fn pifDotMatrix_SetPeriodPerRow
 * @brief Sets row scan period used by multiplex refresh.
 * @param p_owner Pointer to an initialized dot-matrix instance.
 * @param period1ms Row scan period in milliseconds.
 * @return `TRUE` if period update succeeds, otherwise `FALSE`.
 */
BOOL pifDotMatrix_SetPeriodPerRow(PifDotMatrix* p_owner, uint16_t period1ms);

/**
 * @fn pifDotMatrix_Start
 * @brief Starts periodic matrix refresh output.
 * @param p_owner Pointer to an initialized dot-matrix instance.
 */
void pifDotMatrix_Start(PifDotMatrix* p_owner);

/**
 * @fn pifDotMatrix_Stop
 * @brief Stops periodic matrix refresh output.
 * @param p_owner Pointer to an initialized dot-matrix instance.
 */
void pifDotMatrix_Stop(PifDotMatrix* p_owner);

/**
 * @fn pifDotMatrix_SelectPattern
 * @brief Selects which registered pattern is currently rendered.
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
 */
void pifDotMatrix_ChangeBlinkPeriod(PifDotMatrix* p_owner, uint16_t period1ms);

/**
 * @fn pifDotMatrix_SetPosition
 * @brief Sets top-left render position for the selected pattern.
 * @param p_owner Pointer to an initialized dot-matrix instance.
 * @param pos_x Horizontal start position in display coordinates.
 * @param pos_y Vertical start position in display coordinates.
 * @return `TRUE` if position is valid and applied, otherwise `FALSE`.
 */
BOOL pifDotMatrix_SetPosition(PifDotMatrix* p_owner, uint16_t pos_x, uint16_t pos_y);

/**
 * @fn pifDotMatrix_ShiftOn
 * @brief Starts automatic pattern shifting with direction and repetition policy.
 * @param p_owner Pointer to an initialized dot-matrix instance.
 * @param shift_direction Direction of movement for each shift step.
 * @param shift_method Shift repetition behavior (once, repeat, or ping-pong).
 * @param period1ms Shift timer period in milliseconds.
 * @param count Number of shift steps or cycles depending on method.
 * @return `TRUE` if shift operation starts successfully, otherwise `FALSE`.
 */
BOOL pifDotMatrix_ShiftOn(PifDotMatrix* p_owner, PifDotMatrixShiftDir shift_direction,
		PifDotMatrixShiftMethod shift_method, uint16_t period1ms, uint16_t count);

/**
 * @fn pifDotMatrix_ShiftOff
 * @brief Stops automatic pattern shifting.
 * @param p_owner Pointer to an initialized dot-matrix instance.
 */
void pifDotMatrix_ShiftOff(PifDotMatrix* p_owner);

/**
 * @fn pifDotMatrix_ChnageShiftPeriod
 * @brief Changes shift timer period while shift mode is active.
 * @param p_owner Pointer to an initialized dot-matrix instance.
 * @param period1ms New shift period in milliseconds.
 */
void pifDotMatrix_ChangeShiftPeriod(PifDotMatrix* p_owner, uint16_t period1ms);

#ifdef __cplusplus
}
#endif


#endif	// PIF_DOT_MATRIX_H
