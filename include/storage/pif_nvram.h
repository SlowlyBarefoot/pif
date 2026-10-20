// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file pif_nvram.h
 * @brief Non-volatile memory written in place (EEPROM, FRAM), as raw slots or as records safe from a cut write.
 *
 * Non-volatile memory that is written in place, a byte at a time: an I2C or SPI EEPROM, an FRAM, a
 * battery backed RAM. The media is cut into slots of one size, numbered from 0.
 *
 * In NM_RAW mode a slot is a plain area that is read and written as it is. Nothing is added to it,
 * so nothing tells a slot that was written from one that was not, or from one whose write was cut
 * short.
 *
 * pifNvram_SetMedia() makes every slot the same size. pifNvram_SetLayout() gives each slot or
 * record a size of its own instead, so a short counter does not take as much room as a long table.
 *
 * In NM_RECORD mode two slots hold one record: record n lives in slots 2n and 2n+1. Each copy
 * carries a sequence number, its length and a CRC over both and the data. A write goes to the copy
 * that is not the latest, so the latest stays as it was until the new one is complete, and a read
 * takes the newest copy whose CRC is right. A write that is cut short by a reset or a power loss
 * leaves the record whole: as it was before that write, or as the write meant it when every byte
 * that differs had already gone in, never half of each.
 *
 * NM_RECORD is built only with PIF_USE_NVRAM_RECORD. Without it pifNvram_SetMedia() and pifNvram_SetLayout()
 * refuse NM_RECORD with E_INVALID_PARAM.
 *
 * Media that has to be erased before it is written, such as the flash of an MCU or a NOR flash, is
 * not what this is for: see pif_flash for a record in MCU flash and pif_nor_flash for an SPI NOR
 * flash.
 */
#ifndef PIF_NVRAM_H
#define PIF_NVRAM_H


#include "communication/pif_i2c.h"
#include "core/pif_timer_manager.h"


#ifdef PIF_USE_NVRAM_RECORD
/// Bytes a record adds in front of its data in each slot: sequence number, length and CRC.
#define PIF_NVRAM_RECORD_HEADER	6
#endif


/**
 * @brief Size of the internal address of an I2C memory.
 */
typedef enum EnPifNvramI2cIAddrSize
{
    NIC_I_ADDR_SIZE_1	= 1,	///< One byte
    NIC_I_ADDR_SIZE_2	= 2	///< Two bytes
} PifNvramI2cIAddrSize;

/**
 * @enum EnPifNvramMode
 * @brief How the slots are used.
 */
typedef enum EnPifNvramMode
{
	NM_RAW				= 0,	///< A slot is read and written as it is
	NM_RECORD			= 1		///< Two slots hold one record, kept safe from a write cut short
} PifNvramMode;

/**
 * @enum EnPifNvramState
 * @brief Whether a transfer is on its way. A transfer of more than one page is carried by the
 *        timer, one page per release, and this says so while that is happening.
 */
typedef enum EnPifNvramState
{
	NS_IDLE				= 0,	///< No transfer is on its way
	NS_TRANSFER			= 1		///< One is, and the timer is carrying it
} PifNvramState;

/**
 * @enum EnPifNvramStart
 * @brief What became of a transfer that was asked for. One that fits in a single page is over by
 *        the time the call returns, and this is what says which of the two happened.
 */
typedef enum EnPifNvramStart
{
	NS_START_FAILURE	= 0,	///< Nothing was started, and pif_error says why
	NS_START_TIMED		= 1,	///< On its way: the done event reports the end
	NS_START_DONE		= 2		///< Already over, with no event involved
} PifNvramStart;

struct StPifNvram;
typedef struct StPifNvram PifNvram;

/**
 * @brief Reports the end of a transfer that was returned as NS_START_TIMED. A transfer that
 *        finished inside the call is reported by its return value instead, so this is never
 *        called from inside one and asking for the next transfer from here is safe.
 * @param p_owner Pointer to the NVRAM instance.
 * @param result TRUE when every page went through, FALSE when one of them failed.
 */
typedef void (*PifEvtNvramDone)(PifNvram* p_owner, BOOL result);

/**
 * @brief Reads bytes from the media.
 * @param p_owner Pointer to the NVRAM instance.
 * @param p_dst Where the bytes go.
 * @param src Where in the media they come from, in bytes.
 * @param size Number of bytes.
 * @return TRUE on success, otherwise FALSE.
 */
typedef BOOL (*PifActNvramRead)(PifNvram* p_owner, uint8_t* p_dst, uint32_t src, size_t size);

/**
 * @brief Writes bytes to the media. They never cross a page boundary.
 * @param p_owner Pointer to the NVRAM instance.
 * @param dst Where in the media they go, in bytes.
 * @param p_src The bytes.
 * @param size Number of bytes.
 * @return TRUE on success, otherwise FALSE.
 */
typedef BOOL (*PifActNvramWrite)(PifNvram* p_owner, uint32_t dst, uint8_t* p_src, size_t size);

#ifdef PIF_USE_NVRAM_RECORD

/**
 * @class StPifNvramRecord
 * @brief What is known of one record in NM_RECORD mode.
 */
typedef struct StPifNvramRecord
{
	uint16_t seq;				///< Sequence number of the latest copy
	uint8_t slot;				///< Which of the two slots holds it, or one of the RECORD_ values
} PifNvramRecord;

#endif

/**
 * @class StPifNvram
 * @brief Non-volatile memory that is written in place, cut into slots of one size.
 */
struct StPifNvram
{
	// Public Member Variable

	// Read-only Member Variable
    PifId _id;						///< Instance ID
	PifDeviceReg *_p_device;		///< I2C or SPI device of the memory
	uint32_t _capacity;				///< Bytes of the memory
	uint32_t _slot_size;			///< 0 after pifNvram_SetLayout(), where every slot has its own size
	uint16_t _slot_count;			///< Two per record in NM_RECORD mode
	PifNvramMode _mode;				///< NM_RAW or NM_RECORD
	PifNvramState _state;			///< State of the operation in progress

	// Private Member Variable
	uint8_t __addr;
	uint8_t __write_delay_ms;
	uint16_t __page_size;
	uint32_t __ready_time;			///< When the write cycle of the last write is over
	BOOL __write_pending;			///< A write cycle has been started and not waited out yet
	PifTimerManager* __p_timer_manager;
	PifTimer* __p_timer;
#ifdef PIF_USE_NVRAM_RECORD
	PifNvramRecord* __p_record;	///< One per record in NM_RECORD mode, NULL in NM_RAW mode
#endif
	uint32_t* __p_offset;			///< From pifNvram_SetLayout(): where each slot or record begins, and the end

	/// The transfer that is on its way, a page at a time
	uint32_t __op_pos;				///< Where in the media the next page goes
	uint8_t* __op_buffer;			///< Where in the caller buffer it comes from or goes to
	size_t __op_remain;
	BOOL __op_write;
#ifdef PIF_USE_NVRAM_RECORD
	uint8_t __op_head[PIF_NVRAM_RECORD_HEADER];	///< Header of the record that is being written
	uint8_t __op_head_remain;		///< Bytes of it that have not been written yet
	uint16_t __op_record;			///< Record that is being written, if one is
	uint16_t __op_seq;
	uint8_t __op_slot;
#endif

	// Private Action Function
	PifActNvramRead __act_read;
	PifActNvramWrite __act_write;

	// Private Event Function
	PifEvtNvramDone __evt_done;
};


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initializes an NVRAM instance. The media comes next, with pifNvram_SetMedia(), and the
 *        way to reach it, with pifNvram_AttachActMedia() or pifNvram_AttachI2c().
 * @param p_owner Pointer to the NVRAM instance.
 * @param id Object id, or `PIF_ID_AUTO` to allocate automatically.
 * @return `TRUE` on success, otherwise `FALSE`.
 */
BOOL pifNvram_Init(PifNvram* p_owner, PifId id);

/**
 * @brief Gives back what the instance holds: the record table, the timer and the I2C device.
 * @param p_owner Pointer to the NVRAM instance.
 */
void pifNvram_Clear(PifNvram* p_owner);

/**
 * @brief Describes the media and cuts it into slots.
 * @param p_owner Pointer to the NVRAM instance.
 * @param capacity Size of the media, or of the part of it to use, in bytes.
 * @param page_size Size of a write page of the device, which no write crosses: a device that pages
 *        its writes wraps within the page rather than carrying on into the next one. 64 on a
 *        24C256, for instance. Zero for media without pages, such as an FRAM.
 * @param slot_size Size of a slot in bytes. In NM_RECORD mode it holds PIF_NVRAM_RECORD_HEADER
 *        bytes more than the longest record, which is at most 65535 bytes.
 * @param mode NM_RAW or NM_RECORD. In NM_RECORD mode a table of the records is allocated, three
 *        bytes for each of them.
 * @return `TRUE` on success, otherwise `FALSE`.
 */
BOOL pifNvram_SetMedia(PifNvram* p_owner, uint32_t capacity, uint16_t page_size, uint32_t slot_size, PifNvramMode mode);

/**
 * @brief Describes the media and lays the slots or records out on it one after the other from its
 *        start, each with a size of its own. This is what pifNvram_SetMedia() does with slots of
 *        one size, without the room a small record leaves unused in a slot sized for the largest.
 *        In NM_RECORD mode each record takes twice its size and PIF_NVRAM_RECORD_HEADER, for its
 *        two copies. Nothing is aligned to pages, so a record that is not a multiple of the page
 *        size makes the ones after it straddle a page boundary, which costs one more write and
 *        write cycle each; pad the sizes to whole pages if that matters.
 *        Once records have been written, adding records at the end is the only change that keeps
 *        them. A new size moves the second copy of that record and both copies of every record
 *        after it, which then read as never written or, worse, as an older copy. Give a record
 *        that may grow in a later version of the firmware room for that from the start: a record
 *        shorter than its size is fine, and pifNvram_ReadRecord() reads one into a longer buffer.
 * @param p_owner Pointer to the NVRAM instance.
 * @param capacity Size of the media, or of the part of it to use, in bytes.
 * @param page_size Size of a write page of the device, as for pifNvram_SetMedia().
 * @param p_size Size of the data of each slot or record, by its number. It is copied, so it need
 *        not stay. In NM_RECORD mode the header is not part of it, and 0 is allowed.
 * @param count Number of slots or records.
 * @param mode NM_RAW or NM_RECORD.
 * @return `TRUE` on success, otherwise `FALSE` with pif_error set to E_OVERFLOW_BUFFER when they do
 *         not fit in the capacity.
 */
BOOL pifNvram_SetLayout(PifNvram* p_owner, uint32_t capacity, uint16_t page_size, const uint16_t* p_size, uint16_t count,
		PifNvramMode mode);

/**
 * @brief Attaches low-level read and write callbacks to an NVRAM instance.
 * @param p_owner Pointer to the NVRAM instance.
 * @param act_read Callback used to read bytes from the underlying media.
 * @param act_write Callback used to write bytes to the underlying media.
 * @return `TRUE` if callbacks are valid and attached, otherwise `FALSE`.
 */
BOOL pifNvram_AttachActMedia(PifNvram* p_owner, PifActNvramRead act_read, PifActNvramWrite act_write);

/**
 * @brief Attaches an I2C device and configures internal address handling callbacks. A write goes
 *        out in pieces of max_transfer_size of the device when that is set, each piece a write
 *        cycle of its own.
 * @param p_owner Pointer to the NVRAM instance.
 * @param p_port Pointer to the I2C port used to access the device.
 * @param addr Device I2C address or address base.
 * @param p_client Optional client pointer passed to the I2C device registration.
 * @param i_addr_size Width of the internal address in bytes.
 * @param write_delay_ms The write cycle of the device, in milliseconds. It has to pass before the
 *        device is touched again, and it is not something the device can be asked about. Nothing
 *        is waited for when a write returns: the time is noted, and the next access gives back
 *        whatever is left of it, which is usually nothing. Zero for a device with no write cycle,
 *        such as an FRAM.
 * @return `TRUE` if the I2C device is attached successfully, otherwise `FALSE`.
 */
BOOL pifNvram_AttachI2c(PifNvram* p_owner, PifI2cPort* p_port, uint8_t addr, void *p_client, PifNvramI2cIAddrSize i_addr_size, uint8_t write_delay_ms);

/**
 * @brief Detaches the currently attached I2C device and clears access callbacks.
 * @param p_owner Pointer to the NVRAM instance.
 */
void pifNvram_DetachI2c(PifNvram* p_owner);

/**
 * @brief Gives the instance the timer it needs to carry a transfer of more than one page without
 *        holding the CPU. A device with a write cycle has to be left alone between pages, and
 *        there is nothing to ask it about, so that wait is what the timer is for. Raw reads have
 *        nothing to wait for and are spread over the same timer only so that one page goes per
 *        release rather than all of them at once.
 *        The done event runs from the timer process of the task manager, which is the same
 *        context a task runs in, so it may touch the NVRAM, start the next transfer, and do
 *        anything else a task may do.
 *        Without a timer a transfer still completes, but every page and every write cycle of it
 *        is paid for inside the call: 16 pages of a device with a 5ms cycle hold the CPU for
 *        about 80ms.
 * @param p_owner Pointer to the NVRAM instance.
 * @param p_timer_manager Timer manager the transfer timer is taken from.
 * @param evt_done Called once per transfer that did not finish inside its own call. May be NULL,
 *        in which case _state is what a caller watches instead.
 * @return `TRUE` on success, otherwise `FALSE`.
 */
BOOL pifNvram_AttachTimer(PifNvram* p_owner, PifTimerManager* p_timer_manager, PifEvtNvramDone evt_done);

/**
 * @brief Gives the transfer timer back. A transfer on its way is abandoned and its event never
 *        comes. A record it was writing is looked for again on its next access.
 * @param p_owner Pointer to the NVRAM instance.
 */
void pifNvram_DetachTimer(PifNvram* p_owner);

/**
 * @brief Reads a slot in NM_RAW mode. The first page is read before this returns; a read that
 *        needs more than one page is carried by the timer from there, a page per release, and
 *        ends with the done event.
 *        The buffer has to stay where it is until the transfer is over, which NS_START_DONE says
 *        has already happened and the done event says otherwise.
 * @param p_owner Pointer to the NVRAM instance.
 * @param id Slot number.
 * @param p_dst Destination buffer.
 * @param size Number of bytes to read, at most the slot size.
 * @return NS_START_DONE when the whole read is already finished, NS_START_TIMED while the rest of
 *         it is still coming, NS_START_FAILURE when nothing was read.
 */
PifNvramStart pifNvram_Read(PifNvram* p_owner, uint16_t id, uint8_t* p_dst, size_t size);

/**
 * @brief Writes a slot in NM_RAW mode, from its beginning. The first page is written before this
 *        returns; a write that needs more than one page is carried by the timer from there, a page
 *        per release, and ends with the done event. Nothing has to be paged by the caller.
 *        The buffer has to stay where it is until the transfer is over, which NS_START_DONE says
 *        has already happened and the done event says otherwise.
 * @param p_owner Pointer to the NVRAM instance.
 * @param id Slot number.
 * @param p_src Source buffer.
 * @param size Number of bytes to write, at most the slot size.
 * @return NS_START_DONE when the whole write is already finished, NS_START_TIMED while the rest
 *         of it is still going, NS_START_FAILURE when nothing was written.
 */
PifNvramStart pifNvram_Write(PifNvram* p_owner, uint16_t id, uint8_t* p_src, size_t size);

#ifdef PIF_USE_NVRAM_RECORD

/**
 * @brief Reads the latest intact copy of a record in NM_RECORD mode. The copy is checked against
 *        its CRC before a byte of it is put in the buffer, so the buffer is either filled from an
 *        intact copy or left as it was. This is done before it returns, holding the CPU for the
 *        reads, which on an I2C EEPROM at 400kHz take about 25us a byte, twice over for the check.
 *        A record shorter than the buffer leaves the rest of the buffer as it was, so a buffer
 *        filled with defaults first takes a record written by an earlier, shorter version of
 *        itself. A longer one fills the buffer and the rest of it is not read.
 * @param p_owner Pointer to the NVRAM instance.
 * @param id Record number, from 0 to half the slot count less one.
 * @param p_dst Destination buffer.
 * @param size Size of the buffer.
 * @param p_size Where the length of the record goes. May be NULL.
 * @return `TRUE` when the buffer was filled from an intact copy, otherwise `FALSE` with pif_error
 *         set to E_CANNOT_FOUND when neither copy is intact, which is also what a record that was
 *         never written reads as.
 */
BOOL pifNvram_ReadRecord(PifNvram* p_owner, uint16_t id, uint8_t* p_dst, size_t size, uint16_t* p_size);

/**
 * @brief Writes a record in NM_RECORD mode, over the copy that is not the latest. The latest copy
 *        stays as it is until the new one is complete, so a write that never ends leaves the
 *        record as it was. Pages go as pifNvram_Write() sends them.
 *        The first write of a record after start-up looks at both of its copies first, which is
 *        what pifNvram_ReadRecord() does as well, so reading the record at start-up saves it.
 * @param p_owner Pointer to the NVRAM instance.
 * @param id Record number.
 * @param p_src Source buffer, which has to stay where it is and as it is until the transfer is
 *        over, because its CRC is worked out when the write starts.
 * @param size Number of bytes to write, at most the slot size less PIF_NVRAM_RECORD_HEADER, and
 *        at most 65535.
 * @return NS_START_DONE, NS_START_TIMED or NS_START_FAILURE, as pifNvram_Write() describes.
 */
PifNvramStart pifNvram_WriteRecord(PifNvram* p_owner, uint16_t id, uint8_t* p_src, size_t size);

/**
 * @brief Spoils the header of both copies of a record in NM_RECORD mode, so it reads as never
 *        written. It is done before this returns.
 * @param p_owner Pointer to the NVRAM instance.
 * @param id Record number.
 * @return `TRUE` on success, otherwise `FALSE`.
 */
BOOL pifNvram_DeleteRecord(PifNvram* p_owner, uint16_t id);

#endif

#ifdef __cplusplus
}
#endif


#endif  // PIF_NVRAM_H
