#ifndef PIF_I2C_H
#define PIF_I2C_H


#include "communication/pif_comm.h"
#include "core/pif_obj_array.h"


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

typedef PifI2cReturn (*PifActI2cRead)(PifI2cDevice *p_owner, uint32_t iaddr, uint8_t isize, uint8_t* p_data, uint16_t size);
typedef PifI2cReturn (*PifActI2cWrite)(PifI2cDevice *p_owner, uint32_t iaddr, uint8_t isize, uint8_t* p_data, uint16_t size);

/**
 * @class StPifI2cDevice
 * @brief
 */
struct StPifI2cDevice
{
	// Public Member Variable
    uint8_t addr;
    uint16_t timeout;
    uint16_t max_transfer_size;

	// Read-only Member Variable
	volatile PifI2cState _state;
	PifId _id;
	PifI2cPort *_p_port;
	void *_p_client;
};

/**
 * @class StPifI2cPort
 * @brief
 */
struct StPifI2cPort
{
	// Public Member Variable
	uint16_t error_count;

	// Public Action Function
	PifActI2cRead act_read;
	PifActI2cWrite act_write;

	// Read-only Member Variable
	PifId _id;

	// Private Member Variable
    PifObjArray __devices;
    volatile PifI2cDevice* __use_device;
};


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifI2cPort_Init
 * @brief
 * @param p_owner
 * @param id
 * @param device_count
 * @return
 */
BOOL pifI2cPort_Init(PifI2cPort *p_owner, PifId id, uint8_t device_count);

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
 * @param addr
 * @param p_client
 * @return
 */
PifI2cDevice* pifI2cPort_AddDevice(PifI2cPort* p_owner, PifId id, uint8_t addr, void *p_client);

/**
 * @fn pifI2cPort_RemoveDevice
 * @brief
 * @param p_owner
 * @param p_device
 */
void pifI2cPort_RemoveDevice(PifI2cPort* p_owner, PifI2cDevice* p_device);

/**
 * @fn pifI2cPort_TemporaryDevice
 * @brief
 * @param p_owner
 * @param addr
 * @return
 */
PifI2cDevice* pifI2cPort_TemporaryDevice(PifI2cPort* p_owner, uint8_t addr);

#ifndef PIF_NO_LOG

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
 * @param iaddr
 * @param isize
 * @param p_data
 * @param size
 * @return
 */
BOOL pifI2cDevice_Read(PifDevice* p_owner, uint32_t iaddr, uint8_t isize, uint8_t* p_data, size_t size);

/**
 * @fn pifI2cDevice_ReadRegByte
 * @brief
 * @param p_owner
 * @param reg
 * @param p_data
 * @return
 */
BOOL pifI2cDevice_ReadRegByte(PifDevice* p_owner, uint8_t reg, uint8_t* p_data);

/**
 * @fn pifI2cDevice_ReadRegWord
 * @brief
 * @param p_owner
 * @param reg
 * @param p_data
 * @return
 */
BOOL pifI2cDevice_ReadRegWord(PifDevice* p_owner, uint8_t reg, uint16_t* p_data);

/**
 * @fn pifI2cDevice_ReadRegBytes
 * @brief
 * @param p_owner
 * @param reg
 * @param p_data
 * @param size
 * @return
 */
BOOL pifI2cDevice_ReadRegBytes(PifDevice* p_owner, uint8_t reg, uint8_t* p_data, size_t size);

/**
 * @fn pifI2cDevice_ReadRegBit8
 * @brief
 * @param p_owner
 * @param reg
 * @param mask
 * @param p_data
 * @return
 */
BOOL pifI2cDevice_ReadRegBit8(PifDevice* p_owner, uint8_t reg, PifRegMask mask, uint8_t* p_data);

/**
 * @fn pifI2cDevice_ReadRegBit16
 * @brief
 * @param p_owner
 * @param reg
 * @param mask
 * @param p_data
 * @return
 */
BOOL pifI2cDevice_ReadRegBit16(PifDevice* p_owner, uint8_t reg, PifRegMask mask, uint16_t* p_data);

/**
 * @fn pifI2cDevice_Write
 * @brief
 * @param p_owner
 * @param iaddr
 * @param isize
 * @param p_data
 * @param size
 * @return
 */
BOOL pifI2cDevice_Write(PifDevice* p_owner,  uint32_t iaddr, uint8_t isize, uint8_t* p_data, size_t size);

/**
 * @fn pifI2cDevice_WriteRegByte
 * @brief
 * @param p_owner
 * @param reg
 * @param data
 * @return
 */
BOOL pifI2cDevice_WriteRegByte(PifDevice* p_owner, uint8_t reg, uint8_t data);

/**
 * @fn pifI2cDevice_WriteRegWord
 * @brief
 * @param p_owner
 * @param reg
 * @param data
 * @return
 */
BOOL pifI2cDevice_WriteRegWord(PifDevice* p_owner, uint8_t reg, uint16_t data);

/**
 * @fn pifI2cDevice_WriteRegBytes
 * @brief
 * @param p_owner
 * @param reg
 * @param p_data
 * @param size
 * @return
 */
BOOL pifI2cDevice_WriteRegBytes(PifDevice* p_owner, uint8_t reg, uint8_t* p_data, size_t size);

/**
 * @fn pifI2cDevice_WriteRegBit8
 * @brief
 * @param p_owner
 * @param reg
 * @param mask
 * @param data
 * @return
 */
BOOL pifI2cDevice_WriteRegBit8(PifDevice* p_owner, uint8_t reg, PifRegMask mask, uint8_t data);

/**
 * @fn pifI2cDevice_WriteRegBit16
 * @brief
 * @param p_owner
 * @param reg
 * @param mask
 * @param data
 * @return
 */
BOOL pifI2cDevice_WriteRegBit16(PifDevice* p_owner, uint8_t reg, PifRegMask mask, uint16_t data);

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
