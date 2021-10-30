#ifndef PIF_I2C_H
#define PIF_I2C_H


#include "pif.h"


typedef enum EnPifI2cState
{
	IS_IDLE,
	IS_RUN,
	IS_COMPLETE,
	IS_ERROR
} PifI2cState;


struct StPifI2c;
typedef struct StPifI2c PifI2c;

typedef BOOL (*PifActI2cRead)(PifI2c* p_owner, uint16_t size);
typedef BOOL (*PifActI2cWrite)(PifI2c* p_owner, uint16_t size);

/**
 * @class StPifI2c
 * @brief
 */
struct StPifI2c
{
	// Public Member Variable
    uint8_t addr;
	uint8_t data_size;
	uint8_t* p_data;

	// Public Action Function
	PifActI2cRead act_read;
	PifActI2cWrite act_write;

	// Read-only Member Variable
	PifId _id;
	volatile PifI2cState _state_read;
	volatile PifI2cState _state_write;

	// Private Member Variable
};


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifI2c_Create
 * @brief
 * @param id
 * @param data_size
 * @return
 */
PifI2c* pifI2c_Create(PifId id, uint16_t data_size);

/**
 * @fn pifI2c_Destroy
 * @brief
 * @param pp_owner
 */
void pifI2c_Destroy(PifI2c** pp_owner);

/**
 * @fn pifI2c_Init
 * @brief
 * @param p_owner
 * @param id
 * @param data_size
 * @return
 */
BOOL pifI2c_Init(PifI2c* p_owner, PifId id, uint16_t data_size);

/**
 * @fn pifI2c_Clear
 * @brief
 * @param p_owner
 */
void pifI2c_Clear(PifI2c* p_owner);

#ifndef __PIF_NO_LOG__

/**
 * @fn pifI2c_ScanAddress
 * @brief
 * @param p_owner
 */
void pifI2c_ScanAddress(PifI2c* p_owner);

#endif

/**
 * @fn pifI2c_Read
 * @brief
 * @param p_owner
 * @param size
 * @return
 */
BOOL pifI2c_Read(PifI2c* p_owner, uint8_t size);

/**
 * @fn pifI2c_Write
 * @brief
 * @param p_owner
 * @param size
 * @return
 */
BOOL pifI2c_Write(PifI2c* p_owner,  uint8_t size);

/**
 * @fn pifI2c_sigEndRead
 * @brief
 * @param p_owner
 * @param result
 */
void pifI2c_sigEndRead(PifI2c* p_owner, BOOL result);

/**
 * @fn pifI2c_sigEndWrite
 * @brief
 * @param p_owner
 * @param result
 */
void pifI2c_sigEndWrite(PifI2c* p_owner, BOOL result);

#ifdef __cplusplus
}
#endif


#endif  // PIF_I2C_H
