#ifndef PIF_GPIO_H
#define PIF_GPIO_H


#include "core/pif_task.h"


#define PIF_GPIO_MAX_COUNT		7


struct StPifGpio;
typedef struct StPifGpio PifGpio;

typedef uint8_t (*PifActGpioIn)(PifId id);
typedef void (*PifActGpioOut)(PifId id, uint8_t state);

typedef void (*PifEvtGpioIn)(uint8_t index, uint8_t state);


#ifdef PIF_COLLECT_SIGNAL

typedef enum EnPifGpioCsFlag
{
    GP_CSF_OFF			= 0,

	GP_CSF_STATE_IDX	= 0,

	GP_CSF_STATE_BIT	= 1,
	GP_CSF_ALL_BIT		= 1,

	GP_CSF_COUNT		= 1
} PifGpioCsFlag;

typedef struct
{
	PifGpio* p_owner;
    uint8_t flag;
    void* p_device[GP_CSF_COUNT];
} PifGpioColSig;

#endif	// PIF_COLLECT_SIGNAL

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

#ifdef PIF_COLLECT_SIGNAL
	PifGpioColSig* __p_colsig;
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

/**
 * @fn pifGpio_Init
 * @brief
 * @param p_owner
 * @param id
 * @param count
 * @return
 */
BOOL pifGpio_Init(PifGpio* p_owner, PifId id, uint8_t count);

/**
 * @fn pifGpio_Clear
 * @brief
 * @param p_owner
 */
void pifGpio_Clear(PifGpio* p_owner);

/**
 * @fn pifGpio_ReadAll
 * @brief
 * @param p_owner
 * @return
 */
uint8_t pifGpio_ReadAll(PifGpio* p_owner);

/**
 * @fn pifGpio_ReadCell
 * @brief
 * @param p_owner
 * @param index
 * @return
 */
SWITCH pifGpio_ReadCell(PifGpio* p_owner, uint8_t index);

/**
 * @fn pifGpio_WriteAll
 * @brief
 * @param p_owner
 * @param state
 * @return
 */
BOOL pifGpio_WriteAll(PifGpio* p_owner, uint8_t state);

/**
 * @fn pifGpio_WriteCell
 * @brief
 * @param p_owner
 * @param ucIndex
 * @param swState
 * @return
 */
BOOL pifGpio_WriteCell(PifGpio* p_owner, uint8_t index, SWITCH state);

/**
 * @fn pifGpio_sigData
 * @brief
 * @param p_owner
 * @param index
 * @param state
 */
void pifGpio_sigData(PifGpio* p_owner, uint8_t index, SWITCH state);

/**
 * @fn pifGpio_AttachActIn
 * @brief
 * @param p_owner
 * @param act_in
 */
void pifGpio_AttachActIn(PifGpio* p_owner, PifActGpioIn act_in);

/**
 * @fn pifGpio_AttachActOut
 * @brief
 * @param p_owner
 * @param act_out
 */
void pifGpio_AttachActOut(PifGpio* p_owner, PifActGpioOut act_out);

/**
 * @fn pifGpio_AttachTaskIn
 * @brief Task를 추가한다.
 * @param p_owner
 * @param mode Task의 Mode를 설정한다.
 * @param id Task의 ID를 설정한다.
 * @param period Mode에 따라 주기의 단위가 변경된다.
 * @param start 즉시 시작할지를 지정한다.
 * @return Task 구조체 포인터를 반환한다.
 */
PifTask* pifGpio_AttachTaskIn(PifGpio* p_owner, PifId id, PifTaskMode mode, uint16_t period, BOOL start);


#ifdef PIF_COLLECT_SIGNAL

/**
 * @fn pifGpio_SetCsFlag
 * @brief
 * @param p_owner
 * @param flag
 */
void pifGpio_SetCsFlag(PifGpio* p_owner, PifGpioCsFlag flag);

/**
 * @fn pifGpio_ResetCsFlag
 * @brief
 * @param p_owner
 * @param flag
 */
void pifGpio_ResetCsFlag(PifGpio* p_owner, PifGpioCsFlag flag);

/**
 * @fn pifGpioColSig_SetFlag
 * @brief
 * @param flag
 */
void pifGpioColSig_SetFlag(PifGpioCsFlag flag);

/**
 * @fn pifGpioColSig_ResetFlag
 * @brief
 * @param flag
 */
void pifGpioColSig_ResetFlag(PifGpioCsFlag flag);

#endif	// PIF_COLLECT_SIGNAL

#ifdef __cplusplus
}
#endif


#endif  // PIF_GPIO_H
