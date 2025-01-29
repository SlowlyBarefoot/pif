#ifndef PIF_SPI_H
#define PIF_SPI_H


#include "communication/pif_comm.h"
#include "core/pif_obj_array.h"


struct StPifSpiDevice;
typedef struct StPifSpiDevice PifSpiDevice;

struct StPifSpiPort;
typedef struct StPifSpiPort PifSpiPort;

typedef void (*PifActSpiTransfer)(PifSpiDevice *p_owner, uint8_t* p_write, uint8_t* p_read, uint16_t size);

typedef BOOL (*PifActSpiRead)(PifSpiDevice *p_owner, uint32_t iaddr, uint8_t isize, uint8_t* p_data, uint16_t size);
typedef BOOL (*PifActSpiWrite)(PifSpiDevice *p_owner, uint32_t iaddr, uint8_t isize, uint8_t* p_data, uint16_t size);

typedef BOOL (*PifActSpiIsBusy)(PifSpiDevice *p_owner);

/**
 * @class StPifSpiDevice
 * @brief
 */
struct StPifSpiDevice
{
	// Public Member Variable
    uint8_t timeout;
    uint16_t max_transfer_size;

	// Read-only Member Variable
	PifId _id;
	PifSpiPort *_p_port;
};

/**
 * @class StPifSpiPort
 * @brief
 */
typedef struct StPifSpiPort
{
	// Public Member Variable
	uint16_t error_count;

	// Public Action Function
	PifActSpiTransfer act_transfer;
	PifActSpiRead act_read;
	PifActSpiWrite act_write;
	PifActSpiIsBusy act_is_busy;

	// Read-only Member Variable
	PifId _id;
	void *_p_client;

	// Private Member Variable
    PifObjArray __devices;
} PifSpiPort;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifSpiPort_Init
 * @brief
 * @param p_owner
 * @param id
 * @param device_count
 * @param p_client
 * @return
 */
BOOL pifSpiPort_Init(PifSpiPort* p_owner, PifId id, uint8_t device_count, void *p_client);

/**
 * @fn pifSpiPort_Clear
 * @brief
 * @param p_owner
 */
void pifSpiPort_Clear(PifSpiPort* p_owner);

/**
 * @fn pifSpiPort_AddDevice
 * @brief
 * @param p_owner
 * @param id
 * @return
 */
PifSpiDevice* pifSpiPort_AddDevice(PifSpiPort* p_owner, PifId id);

/**
 * @fn pifSpiPort_RemoveDevice
 * @brief
 * @param p_owner
 * @param p_device
 */
void pifSpiPort_RemoveDevice(PifSpiPort* p_owner, PifSpiDevice* p_device);

/**
 * @fn pifSpiPort_TemporaryDevice
 * @brief
 * @param p_owner
 * @return
 */
PifSpiDevice* pifSpiPort_TemporaryDevice(PifSpiPort* p_owner);

/**
 * @fn pifSpiDevice_Transfer
 * @brief
 * @param p_owner
 * @param p_write
 * @param p_read
 * @param size
 * @return
 */
BOOL pifSpiDevice_Transfer(PifSpiDevice* p_owner, uint8_t* p_write, uint8_t* p_read, size_t size);

/**
 * @fn pifSpiDevice_Read
 * @brief
 * @param p_owner
 * @param iaddr
 * @param isize
 * @param p_data
 * @param size
 * @return
 */
BOOL pifSpiDevice_Read(PifDevice* p_owner, uint32_t iaddr, uint8_t isize, uint8_t* p_data, size_t size);

/**
 * @fn pifSpiDevice_ReadRegByte
 * @brief
 * @param p_owner
 * @param reg
 * @param p_data
 * @return
 */
BOOL pifSpiDevice_ReadRegByte(PifDevice* p_owner, uint8_t reg, uint8_t* p_data);

/**
 * @fn pifSpiDevice_ReadRegWord
 * @brief
 * @param p_owner
 * @param reg
 * @param p_data
 * @return
 */
BOOL pifSpiDevice_ReadRegWord(PifDevice* p_owner, uint8_t reg, uint16_t* p_data);

/**
 * @fn pifSpiDevice_ReadRegBytes
 * @brief
 * @param p_owner
 * @param reg
 * @param p_data
 * @param size
 * @return
 */
BOOL pifSpiDevice_ReadRegBytes(PifDevice* p_owner, uint8_t reg, uint8_t* p_data, size_t size);

/**
 * @fn pifSpiDevice_ReadRegBit8
 * @brief
 * @param p_owner
 * @param reg
 * @param mask
 * @param p_data
 * @return
 */
BOOL pifSpiDevice_ReadRegBit8(PifDevice* p_owner, uint8_t reg, PifRegMask mask, uint8_t* p_data);

/**
 * @fn pifSpiDevice_ReadRegBit16
 * @brief
 * @param p_owner
 * @param reg
 * @param mask
 * @param p_data
 * @return
 */
BOOL pifSpiDevice_ReadRegBit16(PifDevice* p_owner, uint8_t reg, PifRegMask field, uint16_t* p_data);

/**
 * @fn pifSpiDevice_Write
 * @brief
 * @param p_owner
 * @param iaddr
 * @param isize
 * @param p_data
 * @param size
 * @return
 */
BOOL pifSpiDevice_Write(PifDevice* p_owner,  uint32_t iaddr, uint8_t isize, uint8_t* p_data, size_t size);

/**
 * @fn pifSpiDevice_WriteRegByte
 * @brief
 * @param p_owner
 * @param reg
 * @param data
 * @return
 */
BOOL pifSpiDevice_WriteRegByte(PifDevice* p_owner, uint8_t reg, uint8_t data);

/**
 * @fn pifSpiDevice_WriteRegWord
 * @brief
 * @param p_owner
 * @param reg
 * @param data
 * @return
 */
BOOL pifSpiDevice_WriteRegWord(PifDevice* p_owner, uint8_t reg, uint16_t data);

/**
 * @fn pifSpiDevice_WriteRegBytes
 * @brief
 * @param p_owner
 * @param reg
 * @param p_data
 * @param size
 * @return
 */
BOOL pifSpiDevice_WriteRegBytes(PifDevice* p_owner, uint8_t reg, uint8_t* p_data, size_t size);

/**
 * @fn pifSpiDevice_WriteRegBit8
 * @brief
 * @param p_owner
 * @param reg
 * @param mask
 * @param data
 * @return
 */
BOOL pifSpiDevice_WriteRegBit8(PifDevice* p_owner, uint8_t reg, PifRegMask mask, uint8_t data);

/**
 * @fn pifSpiDevice_WriteRegBit16
 * @brief
 * @param p_owner
 * @param reg
 * @param mask
 * @param data
 * @return
 */
BOOL pifSpiDevice_WriteRegBit16(PifDevice* p_owner, uint8_t reg, PifRegMask mask, uint16_t data);

/**
 * @fn pifSpiDevice_IsBusy
 * @brief
 * @param p_owner
 * @return
 */
BOOL pifSpiDevice_IsBusy(PifDevice* p_owner);

/**
 * @fn pifSpiDevice_Wait
 * @brief
 * @param p_owner
 * @return
 */
BOOL pifSpiDevice_Wait(PifDevice* p_owner);

#ifdef __cplusplus
}
#endif


#endif  // PIF_SPI_H
