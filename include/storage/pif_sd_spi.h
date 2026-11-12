// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file pif_sd_spi.h
 * @brief SD card in SPI mode as a block device of 512-byte sectors.
 *
 * Written from the SD Physical Layer Simplified Specification. pifSdSpi_Start() wakes the card and finds what it
 * is: power-up clocks, CMD0 (into SPI mode), CMD8 (version 2?), ACMD41 until ready, CMD58 (block addressing?),
 * CMD16 for a standard capacity card, CMD9 (capacity). SDSC version 1 and 2 and SDHC/SDXC are taken; MMC is not.
 * Reads are CMD17, or CMD18 and CMD12 for more sectors; writes CMD24, or CMD25 with the stop token. CRC is off,
 * as SPI mode starts, except for CMD0 and CMD8, which need a valid one.
 *
 * The card needs its chip select held low across a whole command, its answer and its data, which a PifSpiPort
 * transfer does not do. So the port gives three actions instead: act_select drives the chip select, act_transfer
 * moves bytes with the chip select left as it is, and act_set_clock, optional, switches between the start-up clock
 * (400kHz at most) and the full one (25MHz at most).
 *
 * Every function waits for the card with the CPU held: a write can keep it busy for 100ms or more now and then.
 * Call them from a task that may be held that long, never from a control loop. The timeouts count
 * pif_cumulative_timer1ms, so pif_sigTimer1ms() must run.
 */
#ifndef PIF_SD_SPI_H
#define PIF_SD_SPI_H


#include "core/pif.h"


#define PIF_SD_SPI_SECTOR_SIZE		512


/**
 * @brief What card was found.
 */
typedef enum EnPifSdSpiType
{
	SDST_NONE		= 0,	///< None, or not started
	SDST_SDSC_V1	= 1,	///< Standard capacity, version 1: byte addresses
	SDST_SDSC_V2	= 2,	///< Standard capacity, version 2: byte addresses
	SDST_SDHC		= 3		///< High or extended capacity: block addresses
} PifSdSpiType;

/**
 * @brief Step that failed last, for a message to the user.
 */
typedef enum EnPifSdSpiStep
{
	SDSS_NONE		= 0,
	SDSS_CMD0		= 1,	///< No answer at all: usually no card
	SDSS_CMD8		= 2,
	SDSS_ACMD41		= 3,	///< The card did not become ready
	SDSS_CMD58		= 4,
	SDSS_CMD16		= 5,
	SDSS_CSD		= 6,
	SDSS_READ		= 7,
	SDSS_WRITE		= 8
} PifSdSpiStep;

typedef struct StPifSdSpi PifSdSpi;

/** @brief Drives the chip select: TRUE pulls it low. */
typedef void (*PifActSdSpiSelect)(PifSdSpi *p_owner, BOOL select);

/**
 * @brief Moves size bytes in full duplex with the chip select left as it is. p_write NULL sends 0xFF bytes;
 *        p_read NULL drops what comes in.
 */
typedef void (*PifActSdSpiTransfer)(PifSdSpi *p_owner, const uint8_t *p_write, uint8_t *p_read, size_t size);

/** @brief Sets the SPI clock: FALSE 400kHz at most (start-up), TRUE the full clock (25MHz at most). */
typedef void (*PifActSdSpiSetClock)(PifSdSpi *p_owner, BOOL fast);

/**
 * @class StPifSdSpi
 * @brief SD card on SPI.
 */
struct StPifSdSpi
{
	// Public Member Variable
	void *p_client;					///< Free for the port, such as its SPI handle
	uint16_t init_timeout;			///< Longest the card may take to become ready, in ms. 1000 by default.
	uint16_t read_timeout;			///< Longest a sector may take to come, in ms. 200 by default.
	uint16_t write_timeout;			///< Longest the card may stay busy after a sector, in ms. 500 by default.

	// Read-only Member Variable
	PifId _id;						///< Instance ID
	PifSdSpiType _type;				///< What pifSdSpi_Start() found
	uint32_t _sectors;				///< Capacity in sectors
	uint32_t _ocr;					///< OCR register, 0 for a version 1 card
	PifSdSpiStep _fail_step;		///< Step that failed last, SDSS_NONE after a success
	uint8_t _fail_r1;				///< R1 answer of that step, 0xFF for none

	// Private Action Function
	PifActSdSpiSelect __act_select;
	PifActSdSpiTransfer __act_transfer;
	PifActSdSpiSetClock __act_set_clock;
};


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Sets the instance up. No card access yet: pifSdSpi_Start() does that.
 * @param p_owner Pointer to the instance.
 * @param id Object id, or `PIF_ID_AUTO` to allocate automatically.
 * @param act_select Drives the chip select.
 * @param act_transfer Moves bytes.
 * @param act_set_clock Sets the clock, or NULL when the port keeps it at 400kHz or below all the time.
 * @return `TRUE` on success, otherwise `FALSE` with pif_error set to E_INVALID_PARAM.
 */
BOOL pifSdSpi_Init(PifSdSpi *p_owner, PifId id, PifActSdSpiSelect act_select, PifActSdSpiTransfer act_transfer,
		PifActSdSpiSetClock act_set_clock);

/**
 * @brief Wakes the card and finds its type and capacity. Call it again after a card change.
 * @param p_owner Pointer to the instance.
 * @return `TRUE` when a card is ready, otherwise `FALSE` with _fail_step and pif_error set: E_CANNOT_FOUND when
 *         nothing answers CMD0, E_TIMEOUT when the card did not become ready, E_ACCESS_FAILED for a refused
 *         command, E_WRONG_DATA for a voltage range the card does not take.
 */
BOOL pifSdSpi_Start(PifSdSpi *p_owner);

/**
 * @brief Reads sectors, with CMD18 when there is more than one.
 * @param p_owner Pointer to the instance.
 * @param sector First sector.
 * @param p_data count * 512 bytes.
 * @param count Number of sectors.
 * @return `TRUE` on success, otherwise `FALSE` with pif_error set: E_INVALID_STATE without a started card,
 *         E_INVALID_PARAM past the end, E_ACCESS_FAILED for a refused command, E_TIMEOUT when no data came.
 */
BOOL pifSdSpi_Read(PifSdSpi *p_owner, uint32_t sector, uint8_t *p_data, uint32_t count);

/**
 * @brief Writes sectors, with CMD25 when there is more than one, and waits until the card has stored them.
 * @param p_owner Pointer to the instance.
 * @param sector First sector.
 * @param p_data count * 512 bytes.
 * @param count Number of sectors.
 * @return `TRUE` on success, otherwise `FALSE` with pif_error set as pifSdSpi_Read() does, and E_WRONG_DATA when
 *         the card rejected a sector.
 */
BOOL pifSdSpi_Write(PifSdSpi *p_owner, uint32_t sector, const uint8_t *p_data, uint32_t count);

#ifdef __cplusplus
}
#endif


#endif	// PIF_SD_SPI_H
