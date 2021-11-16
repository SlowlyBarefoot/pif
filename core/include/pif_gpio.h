#ifndef PIF_GPIO_H
#define PIF_GPIO_H


#include "pif_task.h"


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

#ifdef __PIF_COLLECT_SIGNAL__

/**
 * @fn pifGpio_SetCsFlagAll
 * @brief
 * @param flag
 */
void pifGpio_SetCsFlagAll(PifGpioCsFlag flag);

/**
 * @fn pifGpio_ResetCsFlagAll
 * @brief
 * @param flag
 */
void pifGpio_ResetCsFlagAll(PifGpioCsFlag flag);

/**
 * @fn pifGpio_SetCsFlagEach
 * @brief
 * @param p_owner
 * @param flag
 */
void pifGpio_SetCsFlagEach(PifGpio* p_owner, PifGpioCsFlag flag);

/**
 * @fn pifGpio_ResetCsFlagEach
 * @brief
 * @param p_owner
 * @param flag
 */
void pifGpio_ResetCsFlagEach(PifGpio* p_owner, PifGpioCsFlag flag);

#endif

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
 * @param period Mode에 따라 주기의 단위가 변경된다.
 * @param start 즉시 시작할지를 지정한다.
 * @return Task 구조체 포인터를 반환한다.
 */
PifTask* pifGpio_AttachTaskIn(PifGpio* p_owner, PifTaskMode mode, uint16_t period, BOOL start);

#ifdef __cplusplus
}
#endif


#endif  // PIF_GPIO_H
