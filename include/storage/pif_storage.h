#ifndef PIF_STORAGE_H
#define PIF_STORAGE_H


#include "communication/pif_i2c.h"
#include "core/pif_timer_manager.h"


typedef enum EnPifStorageI2cIAddrSize
{
    SIC_I_ADDR_SIZE_1	= 1,
    SIC_I_ADDR_SIZE_2	= 2
} PifStorageI2cIAddrSize;


typedef void* PifStorageDataInfoP;

struct StPifStorage;
typedef struct StPifStorage PifStorage;

/**
 * @enum EnPifStorageState
 * @brief Whether a transfer is on its way. A transfer of more than one sector is carried by the
 *        timer, one sector per release, and this says so while that is happening.
 */
typedef enum EnPifStorageState
{
	SS_IDLE				= 0,	// No transfer is on its way
	SS_TRANSFER			= 1		// One is, and the timer is carrying it
} PifStorageState;

/**
 * @enum EnPifStorageStart
 * @brief What became of a transfer that was asked for. One that fits in a single sector is over
 *        by the time the call returns, and this is what says which of the two happened.
 */
typedef enum EnPifStorageStart
{
	SS_START_FAILURE	= 0,	// Nothing was started, and pif_error says why
	SS_START_TIMED		= 1,	// On its way: the done event reports the end
	SS_START_DONE		= 2		// Already over, with no event involved
} PifStorageStart;

/**
 * @fn PifEvtStorageDone
 * @brief Reports the end of a transfer that was returned as SS_START_TIMED. A transfer that
 *        finished inside the call is reported by its return value instead, so this is never
 *        called from inside one and asking for the next transfer from here is safe.
 * @param p_owner Pointer to the storage instance.
 * @param result TRUE when every sector went through, FALSE when one of them failed.
 */
typedef void (*PifEvtStorageDone)(PifStorage* p_owner, BOOL result);

typedef BOOL (*PifActStorageRead)(PifStorage* p_owner, uint8_t* dst, uint32_t src, size_t size);
typedef BOOL (*PifActStorageWrite)(PifStorage* p_owner, uint32_t src, uint8_t* dst, size_t size);

typedef BOOL (*PifStorage_IsFormat)(PifStorage* p_owner);
typedef BOOL (*PifStorage_Format)(PifStorage* p_owner);
typedef PifStorageDataInfoP (*PifStorage_Create)(PifStorage* p_owner, uint16_t id, uint16_t size);
typedef BOOL (*PifStorage_Delete)(PifStorage* p_owner, uint16_t id);
typedef PifStorageDataInfoP (*PifStorage_Open)(PifStorage* p_owner, uint16_t id);
typedef PifStorageStart (*PifStorage_Read)(PifStorage* p_owner, uint8_t* p_dst, PifStorageDataInfoP p_src, size_t size);
typedef PifStorageStart (*PifStorage_Write)(PifStorage* p_owner, PifStorageDataInfoP p_dst, uint8_t* p_src, size_t size);


/**
 * @class StPifStorage
 * @brief Base storage interface that abstracts fixed and variable storage backends.
 */
struct StPifStorage
{
	// Public Member Variable

	// Read-only Member Variable
    PifId _id;
	PifI2cDevice* _p_i2c;
	uint32_t _storage_volume;
	PifStorageState _state;

	// Private Member Variable
	uint8_t __addr;
	uint8_t __write_delay_ms;
	uint32_t __ready_time;			// When the write cycle of the last write is over
	BOOL __write_pending;			// A write cycle has been started and not waited out yet
	PifTimerManager* __p_timer_manager;
	PifTimer* __p_timer;

	// The transfer that is on its way, a sector at a time
	uint32_t __op_pos;				// Where in the media the next sector goes
	uint8_t* __op_buffer;			// Where in the caller buffer it comes from or goes to
	size_t __op_remain;
	uint16_t __op_sector_size;
	BOOL __op_write;

	// Private Function
	PifStorage_IsFormat __fn_is_format;
	PifStorage_Format __fn_format;
	PifStorage_Create __fn_create;
	PifStorage_Delete __fn_delete;
	PifStorage_Open __fn_open;
	PifStorage_Read __fn_read;
	PifStorage_Write __fn_write;

	// Private Action Function
	PifActStorageRead __act_read;
	PifActStorageWrite __act_write;

	// Private Event Function
	PifEvtStorageDone __evt_done;
};


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifStorage_AttachActStorage
 * @brief Attaches low-level read and write callbacks to a storage instance.
 * @param p_owner Pointer to the storage instance.
 * @param act_read Callback used to read bytes from the underlying media.
 * @param act_write Callback used to write bytes to the underlying media.
 * @return `TRUE` if callbacks are valid and attached, otherwise `FALSE`.
 */
BOOL pifStorage_AttachActStorage(PifStorage* p_owner, PifActStorageRead act_read, PifActStorageWrite act_write);

/**
 * @fn pifStorage_AttachI2c
 * @brief Attaches an I2C device and configures internal address handling callbacks.
 * @param p_owner Pointer to the storage instance.
 * @param p_port Pointer to the I2C port used to access the storage device.
 * @param addr Device I2C address or address base.
 * @param p_client Optional client pointer passed to the I2C device registration.
 * @param i_addr_size Internal storage address width in bytes.
 * @param write_delay_ms The write cycle of the device, in milliseconds. It has to pass before the
 *        device is touched again, and it is not something the device can be asked about. Nothing
 *        is waited for when a write returns: the time is noted, and the next access gives back
 *        whatever is left of it, which is usually nothing. Zero for a device with no write cycle,
 *        such as an FRAM.
 * @return `TRUE` if the I2C device is attached successfully, otherwise `FALSE`.
 */
BOOL pifStorage_AttachI2c(PifStorage* p_owner, PifI2cPort* p_port, uint8_t addr, void *p_client, PifStorageI2cIAddrSize i_addr_size, uint8_t write_delay_ms);

/**
 * @fn pifStorage_DetachI2c
 * @brief Detaches the currently attached I2C device and clears access callbacks.
 * @param p_owner Pointer to the storage instance.
 */
void pifStorage_DetachI2c(PifStorage* p_owner);

/**
 * @fn pifStorage_IsFormat
 * @brief Checks whether the storage media is currently formatted.
 * @param p_owner Pointer to the storage instance.
 * @return `TRUE` when media format metadata is valid, otherwise `FALSE`.
 */
BOOL pifStorage_IsFormat(PifStorage* p_owner);

/**
 * @fn pifStorage_Format
 * @brief Formats the storage media and initializes allocation metadata.
 * @param p_owner Pointer to the storage instance.
 * @return `TRUE` on success, otherwise `FALSE`.
 */
BOOL pifStorage_Format(PifStorage* p_owner);

/**
 * @fn pifStorage_Create
 * @brief Creates a data entry in storage with the requested id and size.
 * @param p_owner Pointer to the storage instance.
 * @param id Logical data identifier.
 * @param size Requested payload size in bytes.
 * @return Opaque data info pointer on success, or `NULL` on failure.
 */
PifStorageDataInfoP pifStorage_Create(PifStorage* p_owner, uint16_t id, uint16_t size);

/**
 * @fn pifStorage_Delete
 * @brief Deletes a data entry identified by id from storage metadata.
 * @param p_owner Pointer to the storage instance.
 * @param id Logical data identifier to remove.
 * @return `TRUE` if the entry is removed, otherwise `FALSE`.
 */
BOOL pifStorage_Delete(PifStorage* p_owner, uint16_t id);

/**
 * @fn pifStorage_Open
 * @brief Opens an existing data entry and returns its metadata handle.
 * @param p_owner Pointer to the storage instance.
 * @param id Logical data identifier to open.
 * @return Opaque data info pointer on success, or `NULL` if not found.
 */
PifStorageDataInfoP pifStorage_Open(PifStorage* p_owner, uint16_t id);

/**
 * @fn pifStorage_AttachTimer
 * @brief Gives the instance the timer it needs to carry a transfer of more than one sector
 *        without holding the CPU. A device with a write cycle has to be left alone between
 *        sectors, and there is nothing to ask it about, so that wait is what the timer is for.
 *        Reads have nothing to wait for and are spread over the same timer only so that one
 *        sector goes per release rather than all of them at once.
 *        The done event runs from the timer process of the task manager, which is the same
 *        context a task runs in, so it may touch the storage, start the next transfer, and do
 *        anything else a task may do.
 *        Without a timer a transfer still completes, but every sector and every write cycle of it
 *        is paid for inside the call: 16 sectors of a device with a 5ms cycle hold the CPU for
 *        about 80ms.
 * @param p_owner Pointer to the storage instance.
 * @param p_timer_manager Timer manager the transfer timer is taken from.
 * @param evt_done Called once per transfer that did not finish inside its own call. May be NULL,
 *        in which case _state is what a caller watches instead.
 * @return `TRUE` on success, otherwise `FALSE`.
 */
BOOL pifStorage_AttachTimer(PifStorage* p_owner, PifTimerManager* p_timer_manager, PifEvtStorageDone evt_done);

/**
 * @fn pifStorage_DetachTimer
 * @brief Gives the transfer timer back. A transfer on its way is abandoned and its event never
 *        comes.
 * @param p_owner Pointer to the storage instance.
 */
void pifStorage_DetachTimer(PifStorage* p_owner);

/**
 * @fn pifStorage_Read
 * @brief Reads a whole storage entry into a destination buffer. The first sector is read before
 *        this returns; a read that needs more than one sector is carried by the timer from there,
 *        a sector per release, and ends with the done event.
 *        The buffer has to stay where it is until the transfer is over, which SS_START_DONE says
 *        has already happened and the done event says otherwise.
 * @param p_owner Pointer to the storage instance.
 * @param p_dst Destination buffer.
 * @param p_src Opaque metadata handle of the source storage entry.
 * @param size Number of bytes to read.
 * @return SS_START_DONE when the whole read is already finished, SS_START_TIMED while the rest of
 *         it is still coming, SS_START_FAILURE when nothing was read.
 */
PifStorageStart pifStorage_Read(PifStorage* p_owner, uint8_t* p_dst, PifStorageDataInfoP p_src, size_t size);

/**
 * @fn pifStorage_Write
 * @brief Writes a whole source buffer into a storage entry. The first sector is written before
 *        this returns; a write that needs more than one sector is carried by the timer from
 *        there, a sector per release, and ends with the done event. Nothing has to be paged by
 *        the caller.
 *        The buffer has to stay where it is until the transfer is over, which SS_START_DONE says
 *        has already happened and the done event says otherwise.
 * @param p_owner Pointer to the storage instance.
 * @param p_dst Opaque metadata handle of the destination storage entry.
 * @param p_src Source buffer.
 * @param size Number of bytes to write.
 * @return SS_START_DONE when the whole write is already finished, SS_START_TIMED while the rest
 *         of it is still going, SS_START_FAILURE when nothing was written.
 */
PifStorageStart pifStorage_Write(PifStorage* p_owner, PifStorageDataInfoP p_dst, uint8_t* p_src, size_t size);

/**
 * @fn pifStorage_StartTransfer
 * @brief Hands the base class one transfer to carry. It is what the storage implementations call
 *        from their own read and write, having worked out where in the media the entry begins and
 *        how large a sector is; nothing else should need it.
 *        The first sector goes at once, because there is nothing to wait for before it. What is
 *        left is carried by the timer if there is one, and inside this call if there is not.
 * @param p_owner Pointer to the storage instance.
 * @param write TRUE to write the buffer to the media, FALSE to read the media into it.
 * @param pos Where in the media the transfer begins, in bytes.
 * @param p_buffer Caller buffer, which has to stay where it is until the transfer is over.
 * @param size Number of bytes to transfer.
 * @param sector_size Size of a sector, which is what each step of the transfer is clipped to. A
 *        step never crosses a sector boundary: a device that pages its writes wraps within the
 *        page rather than carrying on into the next one.
 * @return SS_START_DONE, SS_START_TIMED or SS_START_FAILURE, as pifStorage_Write() describes.
 */
PifStorageStart pifStorage_StartTransfer(PifStorage* p_owner, BOOL write, uint32_t pos, uint8_t* p_buffer, size_t size, uint16_t sector_size);

#ifdef __cplusplus
}
#endif


#endif  // PIF_STORAGE_H
