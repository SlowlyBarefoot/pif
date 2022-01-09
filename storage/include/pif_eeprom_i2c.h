#ifndef PIF_EEPROM_I2C_H
#define PIF_EEPROM_I2C_H


#include "pif_i2c.h"
#include "pif_storage.h"


typedef enum EnPifEepromI2cIAddrSize
{
    EEPROM_I2C_I_ADDR_SIZE_1	= 1,
    EEPROM_I2C_I_ADDR_SIZE_2	= 2
} PifEepromI2cIAddrSize;


/**
 * @class StPifEepromI2c
 * @brief
 */
typedef struct StPifEepromI2c
{
	// Public Member Variable

	// Read-only Member Variable
	PifId _id;
	PifI2cDevice* _p_i2c;
	PifStorage _storage;

	// Private Member Variable
    uint8_t __addr;
    uint8_t __write_delay_ms;
} PifEepromI2c;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifEepromI2c_Init
 * @brief
 * @param p_owner
 * @param id
 * @param min_data_info_count
 * @param sector_size
 * @param storage_volume
 * @param p_port
 * @param addr
 * @param i_addr_size
 * @param write_delay_ms
 * @return
 */
BOOL pifEepromI2c_Init(PifEepromI2c* p_owner, PifId id, uint8_t min_data_info_count, uint16_t sector_size, uint32_t storage_volume,
		PifI2cPort* p_port, uint8_t addr, PifEepromI2cIAddrSize i_addr_size, uint8_t write_delay_ms);

/**
 * @fn pifEepromI2c_Clear
 * @brief
 * @param p_owner
 */
void pifEepromI2c_Clear(PifEepromI2c* p_owner);

#ifdef __cplusplus
}
#endif


#endif  // PIF_EEPROM_I2C_H
