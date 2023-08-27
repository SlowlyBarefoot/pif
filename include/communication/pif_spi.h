#ifndef PIF_SPI_H
#define PIF_SPI_H


#include "core/pif_list.h"


typedef uint16_t PifSpiRegField;

struct StPifSpiDevice;
typedef struct StPifSpiDevice PifSpiDevice;

struct StPifSpiPort;
typedef struct StPifSpiPort PifSpiPort;

typedef void (*PifActSpiTransfer)(PifId id, uint8_t* p_write, uint8_t* p_read, uint16_t size);

typedef BOOL (*PifActSpiRead)(PifId id, uint32_t iaddr, uint8_t isize, uint8_t* p_data, uint16_t size);
typedef BOOL (*PifActSpiWrite)(PifId id, uint32_t iaddr, uint8_t isize, uint8_t* p_data, uint16_t size);

/**
 * @class StPifSpiDevice
 * @brief
 */
struct StPifSpiDevice
{
	// Public Member Variable
    uint8_t timeout;

	// Read-only Member Variable
	PifId _id;

	// Private Member Variable
	PifSpiPort* __p_port;
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

	// Read-only Member Variable
	PifId _id;

	// Private Member Variable
    PifFixList __devices;
    uint16_t __max_transfer_size;
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
 * @param max_transfer_size
 * @return
 */
BOOL pifSpiPort_Init(PifSpiPort* p_owner, PifId id, uint8_t device_count, uint16_t max_transfer_size);

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
BOOL pifSpiDevice_Read(PifSpiDevice* p_owner, uint32_t iaddr, uint8_t isize, uint8_t* p_data, size_t size);

/**
 * @fn pifSpiDevice_ReadRegByte
 * @brief
 * @param p_owner
 * @param reg
 * @param p_data
 * @return
 */
BOOL pifSpiDevice_ReadRegByte(PifSpiDevice* p_owner, uint8_t reg, uint8_t* p_data);

/**
 * @fn pifSpiDevice_ReadRegWord
 * @brief
 * @param p_owner
 * @param reg
 * @param p_data
 * @return
 */
BOOL pifSpiDevice_ReadRegWord(PifSpiDevice* p_owner, uint8_t reg, uint16_t* p_data);

/**
 * @fn pifSpiDevice_ReadRegBytes
 * @brief
 * @param p_owner
 * @param reg
 * @param p_data
 * @param size
 * @return
 */
BOOL pifSpiDevice_ReadRegBytes(PifSpiDevice* p_owner, uint8_t reg, uint8_t* p_data, size_t size);

/**
 * @fn pifSpiDevice_ReadRegBit8
 * @brief
 * @param p_owner
 * @param reg
 * @param field
 * @param p_data
 * @return
 */
BOOL pifSpiDevice_ReadRegBit8(PifSpiDevice* p_owner, uint8_t reg, PifSpiRegField field, uint8_t* p_data);

/**
 * @fn pifSpiDevice_ReadRegBit16
 * @brief
 * @param p_owner
 * @param reg
 * @param field
 * @param p_data
 * @return
 */
BOOL pifSpiDevice_ReadRegBit16(PifSpiDevice* p_owner, uint8_t reg, PifSpiRegField field, uint16_t* p_data);

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
BOOL pifSpiDevice_Write(PifSpiDevice* p_owner,  uint32_t iaddr, uint8_t isize, uint8_t* p_data, size_t size);

/**
 * @fn pifSpiDevice_WriteRegByte
 * @brief
 * @param p_owner
 * @param reg
 * @param data
 * @return
 */
BOOL pifSpiDevice_WriteRegByte(PifSpiDevice* p_owner, uint8_t reg, uint8_t data);

/**
 * @fn pifSpiDevice_WriteRegWord
 * @brief
 * @param p_owner
 * @param reg
 * @param data
 * @return
 */
BOOL pifSpiDevice_WriteRegWord(PifSpiDevice* p_owner, uint8_t reg, uint16_t data);

/**
 * @fn pifSpiDevice_WriteRegBytes
 * @brief
 * @param p_owner
 * @param reg
 * @param p_data
 * @param size
 * @return
 */
BOOL pifSpiDevice_WriteRegBytes(PifSpiDevice* p_owner, uint8_t reg, uint8_t* p_data, size_t size);

/**
 * @fn pifSpiDevice_WriteRegBit8
 * @brief
 * @param p_owner
 * @param reg
 * @param field
 * @param data
 * @return
 */
BOOL pifSpiDevice_WriteRegBit8(PifSpiDevice* p_owner, uint8_t reg, PifSpiRegField field, uint8_t data);

/**
 * @fn pifSpiDevice_WriteRegBit16
 * @brief
 * @param p_owner
 * @param reg
 * @param field
 * @param data
 * @return
 */
BOOL pifSpiDevice_WriteRegBit16(PifSpiDevice* p_owner, uint8_t reg, PifSpiRegField field, uint16_t data);

#ifdef __cplusplus
}
#endif


#endif  // PIF_SPI_H
