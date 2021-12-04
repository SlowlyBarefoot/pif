#ifndef PIF_I2C_H
#define PIF_I2C_H


#include "pif_list.h"


typedef enum EnPifI2cState
{
	IS_IDLE,
	IS_RUN,
	IS_COMPLETE,
	IS_ERROR
} PifI2cState;

typedef enum EnPifI2cReturn
{
	IR_WAIT,
	IR_COMPLETE,
	IR_ERROR
} PifI2cReturn;


struct StPifI2cDevice;
typedef struct StPifI2cDevice PifI2cDevice;

struct StPifI2cPort;
typedef struct StPifI2cPort PifI2cPort;

typedef PifI2cReturn (*PifActI2cRead)(PifI2cDevice* p_owner, uint16_t size);
typedef PifI2cReturn (*PifActI2cWrite)(PifI2cDevice* p_owner, uint16_t size);

/**
 * @class StPifI2cDevice
 * @brief
 */
struct StPifI2cDevice
{
	// Public Member Variable
    uint8_t addr;
	uint8_t data_size;
	uint8_t* p_data;

	// Read-only Member Variable
	PifId _id;
	volatile PifI2cState _state;

	// Private Member Variable
	PifI2cPort* __p_port;
};

/**
 * @class StPifI2cPort
 * @brief
 */
typedef struct StPifI2cPort
{
	// Public Member Variable

	// Public Action Function
	PifActI2cRead act_read;
	PifActI2cWrite act_write;

	// Read-only Member Variable
	PifId _id;

	// Private Member Variable
    PifFixList __devices;
    volatile PifI2cDevice* __use_device;
} PifI2cPort;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifI2cPort_Init
 * @brief
 * @param p_owner
 * @param id
 * @param size
 * @return
 */
BOOL pifI2cPort_Init(PifI2cPort* p_owner, PifId id, uint8_t size);

/**
 * @fn pifI2cPort_Clear
 * @brief
 * @param p_owner
 */
void pifI2cPort_Clear(PifI2cPort* p_owner);

/**
 * @fn pifI2cPort_AddDevice
 * @brief
 * @param p_owner
 * @param id
 * @param data_size
 * @return
 */
PifI2cDevice* pifI2cPort_AddDevice(PifI2cPort* p_owner, PifId id, uint16_t data_size);

/**
 * @fn pifI2cPort_RemoveDevice
 * @brief
 * @param p_owner
 * @param p_device
 */
void pifI2cPort_RemoveDevice(PifI2cPort* p_owner, PifI2cDevice* p_device);

#ifndef __PIF_NO_LOG__

/**
 * @fn pifI2cPort_ScanAddress
 * @brief
 * @param p_owner
 */
void pifI2cPort_ScanAddress(PifI2cPort* p_owner);

#endif

/**
 * @fn pifI2cDevice_Read
 * @brief
 * @param p_owner
 * @param size
 * @return
 */
BOOL pifI2cDevice_Read(PifI2cDevice* p_owner, uint8_t size);

/**
 * @fn pifI2cDevice_Write
 * @brief
 * @param p_owner
 * @param size
 * @return
 */
BOOL pifI2cDevice_Write(PifI2cDevice* p_owner,  uint8_t size);

/**
 * @fn pifI2cPort_sigEndTransfer
 * @brief
 * @param p_owner
 * @param result
 */
void pifI2cPort_sigEndTransfer(PifI2cPort* p_owner, BOOL result);

#ifdef __cplusplus
}
#endif


#endif  // PIF_I2C_H
