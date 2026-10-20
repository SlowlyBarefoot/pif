// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file pif_ring_buffer.h
 * @brief Byte ring buffer for one writer and one reader, safe across an interrupt.
 *
 * Used by the UART and the stream drivers. When full, a buffer can refuse new data or chop off old data by a
 * character or a length.
 */
#ifndef PIF_RING_BUFFER_H
#define PIF_RING_BUFFER_H


#include "core/pif.h"


#define RB_CHOP_OFF_NONE	0	///< Refuse new data when full
#define RB_CHOP_OFF_CHAR	1	///< Drop old data up to a chosen character
#define RB_CHOP_OFF_LENGTH	2	///< Drop old data by a chosen length


typedef struct StPifRingBuffer PifRingBuffer;

/**
 * @brief Keeps the other side of the buffer out while the tail moves, by masking its interrupt for example.
 * @param p_owner Pointer to the ring buffer instance.
 * @param lock TRUE to enter, FALSE to leave.
 */
typedef void (*PifActRingBufferLock)(PifRingBuffer* p_owner, BOOL lock);

/**
 * @class StPifRingBuffer
 * @brief Byte ring buffer for one writer and one reader.
 *
 * The writer owns __head and __put_head, the reader owns __tail. Each side may run in an interrupt
 * as long as only it changes what it owns. A few writer calls also move the tail: a chop-off policy
 * dropping old data, pifRingBuffer_SetHead() running over the tail and pifRingBuffer_MoveHeadForLinear().
 * When one of those is used while the other side runs in an interrupt, attach a lock with
 * pifRingBuffer_AttachActLock(), which PIF_USE_RING_BUFFER_LOCK puts in the build.
 *
 * Bytes read in place, by DMA for example, should be taken with pifRingBuffer_GetReadPointer(). They stay
 * claimed until the reader removes them, and a chop-off fails rather than drop them, so new data is
 * refused instead of overwriting bytes still on their way out.
 */
struct StPifRingBuffer
{
	// Public Member Variable

	// Read-only Member Variable
	PifId _id;	///< Instance ID
	struct {
		unsigned int is_static	: 1;	///< The memory is the caller's
		unsigned int chop_off	: 2;	///< RB_CHOP_OFF_
		unsigned int putting	: 1;	///< Inside pifRingBuffer_BeginPutting() and its commit or rollback
	} _bt;	///< Flags
    uint16_t _size;	///< Bytes of the buffer. One fewer byte can be held.

	// Private Member Variable
#ifndef PIF_NO_LOG
	const char* __p_name;
#endif
    uint8_t* __p_buffer;
    volatile uint16_t __head;		///< End of the bytes the reader may take
    volatile uint16_t __tail;
    uint16_t __put_head;			///< Where the writer puts next; ahead of __head only inside a put session
    volatile uint16_t __claim;		///< Bytes from the tail handed out by pifRingBuffer_GetReadPointer()
    union {
		char chop_off_char;
		uint16_t chop_off_length;
    } __ui;

#ifdef PIF_USE_RING_BUFFER_LOCK
	// Private Action Function
	PifActRingBufferLock __act_lock;
#endif
};


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Allocates a ring buffer object with a buffer of size bytes from the heap.
 * @param id Instance ID, or PIF_ID_AUTO.
 * @param size Size of the buffer in bytes. One byte less can be held.
 * @return The new ring buffer, or NULL with pif_error set. Free it with pifRingBuffer_Destroy().
 */
PifRingBuffer* pifRingBuffer_CreateHeap(PifId id, uint16_t size);

/**
 * @brief Allocates a ring buffer object from the heap on a buffer of the caller's.
 * @param id Instance ID, or PIF_ID_AUTO.
 * @param size Size of p_buffer in bytes. One byte less can be held.
 * @param p_buffer Buffer of size bytes. It must stay valid while the ring buffer is used.
 * @return The new ring buffer, or NULL with pif_error set. Free it with pifRingBuffer_Destroy().
 */
PifRingBuffer* pifRingBuffer_CreateStatic(PifId id, uint16_t size, uint8_t* p_buffer);

/**
 * @brief Frees a ring buffer made by pifRingBuffer_CreateHeap() or pifRingBuffer_CreateStatic(), and its
 *        buffer if it allocated one.
 * @param pp_owner Address of the pointer to the ring buffer. It is set to NULL.
 */
void pifRingBuffer_Destroy(PifRingBuffer** pp_owner);

/**
 * @brief Sets up a ring buffer object of the caller's with a buffer of size bytes from the heap.
 * @param p_owner Ring buffer to set up.
 * @param id Instance ID, or PIF_ID_AUTO.
 * @param size Size of the buffer in bytes. One byte less can be held.
 * @return TRUE on success, FALSE with pif_error set otherwise.
 */
BOOL pifRingBuffer_InitHeap(PifRingBuffer* p_owner, PifId id, uint16_t size);

/**
 * @brief Sets up a ring buffer object of the caller's on a buffer of the caller's.
 * @param p_owner Ring buffer to set up.
 * @param id Instance ID, or PIF_ID_AUTO.
 * @param size Size of p_buffer in bytes. One byte less can be held.
 * @param p_buffer Buffer of size bytes. It must stay valid while the ring buffer is used.
 * @return TRUE on success, FALSE with pif_error set otherwise.
 */
BOOL pifRingBuffer_InitStatic(PifRingBuffer* p_owner, PifId id, uint16_t size, uint8_t* p_buffer);

/**
 * @brief Frees the buffer if the ring buffer allocated it, and leaves the object empty.
 * @param p_owner Ring buffer.
 */
void pifRingBuffer_Clear(PifRingBuffer* p_owner);

/**
 * @brief Marks the ring buffer as empty by resetting read/write progress to an empty state.
 * @param p_owner Pointer to the target object instance.
 */
void pifRingBuffer_Empty(PifRingBuffer* p_owner);

/**
 * @brief Resizes internal storage used by the ring buffer while preserving valid runtime constraints.
 * @param p_owner Pointer to the target object instance.
 * @param size New size of the buffer in bytes.
 * @return TRUE on success, otherwise FALSE.
 */
BOOL pifRingBuffer_ResizeHeap(PifRingBuffer* p_owner, uint16_t size);

/**
 * @brief Sets configuration or runtime state for the ring buffer based on the provided parameters.
 * @param p_owner Pointer to the target object instance.
 * @param p_name Human-readable name string. With PIF_NO_LOG it is not kept.
 */
#ifndef PIF_NO_LOG
void pifRingBuffer_SetName(PifRingBuffer* p_owner, const char* p_name);
#else
PIF_STATIC_INLINE void pifRingBuffer_SetName(PifRingBuffer* p_owner, const char* p_name)
{
	(void)p_owner;
	(void)p_name;
}
#endif

#ifdef PIF_USE_RING_BUFFER_LOCK

/**
 * @brief Gives the instance a way to keep the other side out while the tail moves. Without one, no locking is done.
 *        Only with PIF_USE_RING_BUFFER_LOCK.
 * @param p_owner Pointer to the target object instance.
 * @param act_lock Enters and leaves the locked section, or NULL to remove it.
 */
void pifRingBuffer_AttachActLock(PifRingBuffer* p_owner, PifActRingBufferLock act_lock);

#endif

/**
 * @brief Returns the start of the buffer memory, _size bytes long, to hand to a circular DMA that
 *        pifRingBuffer_SetHead() follows. A heap buffer moves on pifRingBuffer_ResizeHeap(), so set the DMA up again
 *        after one.
 * @param p_owner Pointer to the target object instance.
 * @return Pointer to the buffer memory, or NULL if there is none.
 */
uint8_t* pifRingBuffer_GetBuffer(PifRingBuffer* p_owner);

/**
 * @brief Gives the address of a byte of the buffer, pos bytes after the tail. Nothing is removed.
 * @param p_owner Ring buffer.
 * @param pos Offset from the tail. It is not checked against the fill size.
 * @return Address of the byte.
 */
uint8_t* pifRingBuffer_GetTailPointer(PifRingBuffer* p_owner, uint16_t pos);

/**
 * @brief Returns where the bytes from pos after the tail start and how many follow without wrapping, and claims
 *        them so that a chop-off cannot drop them before the reader removes them.
 * @param p_owner Pointer to the target object instance.
 * @param pos Offset from the tail.
 * @param p_length Receives the number of contiguous bytes, 0 if there is nothing from pos on.
 * @return Pointer to the first byte.
 */
uint8_t* pifRingBuffer_GetReadPointer(PifRingBuffer* p_owner, uint16_t pos, uint16_t* p_length);

/**
 * @brief Prepares the buffer for size bytes that have to lie in one piece, by moving the empty buffer
 *        back to its start when they would not fit before its end.
 * @param p_owner Ring buffer.
 * @param size Bytes that are to be written in one piece.
 * @return TRUE when they can be written in one piece, FALSE when the buffer is not empty, size is too
 *         large or a put session is open.
 */
BOOL pifRingBuffer_MoveHeadForLinear(PifRingBuffer* p_owner, uint16_t size);

/**
 * @brief Moves the head to where a writer outside the library, a circular DMA for example, has written up to.
 *
 * For a circular RX DMA over the whole buffer, pass _size minus the remaining transfer count from the half,
 * complete and idle interrupts. Calls must come before the DMA writes a full buffer since the last one,
 * since a whole lap looks the same as no bytes at all.
 * @param p_owner Pointer to the target object instance.
 * @param pos Index the next byte will be written to, below _size.
 * @return TRUE on success. FALSE if pos is out of range, in which case nothing changes, or if the bytes ran over
 *         the tail, in which case the head still moves and the tail is moved just past it, dropping the
 *         overwritten bytes.
 */
BOOL pifRingBuffer_SetHead(PifRingBuffer* p_owner, uint16_t pos);

/**
 * @brief Refuses new data when the buffer is full. This is the default.
 * @param p_owner Ring buffer.
 */
void pifRingBuffer_ChopsOffNone(PifRingBuffer* p_owner);

/**
 * @brief When the buffer is full, drops old data up to an occurrence of ch, for example the start of a
 *        line, so that whole records are dropped.
 * @param p_owner Ring buffer.
 * @param ch Character that starts a record. The dropped data ends before it.
 */
void pifRingBuffer_ChopsOffChar(PifRingBuffer* p_owner, char ch);

/**
 * @brief When the buffer is full, drops old data in steps of length bytes, for records of a fixed size.
 * @param p_owner Ring buffer.
 * @param length Size of a record in bytes.
 */
void pifRingBuffer_ChopsOffLength(PifRingBuffer* p_owner, uint16_t length);

/**
 * @brief Tells whether the ring buffer has a buffer, that is whether it was set up.
 * @param p_owner Ring buffer.
 * @return TRUE when it has a buffer.
 */
BOOL pifRingBuffer_IsBuffer(PifRingBuffer* p_owner);

/**
 * @brief Tells whether the buffer holds no data.
 * @param p_owner Ring buffer.
 * @return TRUE when it is empty.
 */
BOOL pifRingBuffer_IsEmpty(PifRingBuffer* p_owner);

/**
 * @brief Returns the number of bytes the reader may take. Bytes of an open put session are not counted.
 * @param p_owner Pointer to the target object instance.
 * @return Bytes held.
 */
uint16_t pifRingBuffer_GetFillSize(PifRingBuffer* p_owner);

/**
 * @brief Returns the room left for the writer. Bytes of an open put session count as used.
 * @param p_owner Pointer to the target object instance.
 * @return Bytes that can still be added.
 */
uint16_t pifRingBuffer_GetRemainSize(PifRingBuffer* p_owner);

/**
 * @brief Starts a put session. Bytes put until pifRingBuffer_CommitPutting() stay hidden from the reader, and
 *        pifRingBuffer_RollbackPutting() discards them. An unfinished earlier session is discarded.
 * @param p_owner Pointer to the target object instance.
 */
void pifRingBuffer_BeginPutting(PifRingBuffer* p_owner);

/**
 * @brief Ends the put session and hands all of its bytes to the reader at once.
 * @param p_owner Pointer to the target object instance.
 */
void pifRingBuffer_CommitPutting(PifRingBuffer* p_owner);

/**
 * @brief Ends the put session and discards its bytes. Does nothing outside a session.
 * @param p_owner Pointer to the target object instance.
 */
void pifRingBuffer_RollbackPutting(PifRingBuffer* p_owner);

/**
 * @brief Returns a byte of the open put session, to patch it before the commit.
 * @param p_owner Pointer to the target object instance.
 * @param pos Offset from the start of the session.
 * @return Address of the byte pos bytes after the start of the put session.
 */
uint8_t* pifRingBuffer_GetPointerPutting(PifRingBuffer* p_owner, uint16_t pos);

/**
 * @brief Adds one byte. When the buffer is full the chop-off policy decides.
 * @param p_owner Ring buffer.
 * @param data Byte to add.
 * @return TRUE when it was added, FALSE with E_OVERFLOW_BUFFER when there was no room.
 */
BOOL pifRingBuffer_PutByte(PifRingBuffer* p_owner, uint8_t data);

/**
 * @brief Adds a block of bytes, all or none. When the buffer is full the chop-off policy decides.
 * @param p_owner Ring buffer.
 * @param p_data Bytes to add.
 * @param length Number of bytes.
 * @return TRUE when they were added, FALSE with E_OVERFLOW_BUFFER when there was no room.
 */
BOOL pifRingBuffer_PutData(PifRingBuffer* p_owner, uint8_t* p_data, uint16_t length);

/**
 * @brief Adds a string without its NUL, all or none.
 * @param p_owner Ring buffer.
 * @param p_string String to add.
 * @return TRUE when it was added, FALSE with E_OVERFLOW_BUFFER when there was no room.
 */
BOOL pifRingBuffer_PutString(PifRingBuffer* p_owner, char* p_string);

/**
 * @brief Removes the oldest byte.
 * @param p_owner Ring buffer.
 * @param p_data Where the byte goes.
 * @return TRUE when a byte was taken, FALSE when the buffer is empty.
 */
BOOL pifRingBuffer_GetByte(PifRingBuffer* p_owner, uint8_t* p_data);

/**
 * @brief Removes up to length of the oldest bytes.
 * @param p_owner Ring buffer.
 * @param p_data Where the bytes go.
 * @param length Most bytes to take.
 * @return Number of bytes taken.
 */
uint16_t pifRingBuffer_GetBytes(PifRingBuffer* p_owner, uint8_t* p_data, uint16_t length);

/**
 * @brief Copies bytes from a ring buffer into an array, leaving them in the ring buffer.
 * @param p_dst Destination array.
 * @param count Most bytes to copy.
 * @param p_src Ring buffer to copy from.
 * @param pos Offset from the tail of p_src where the copy starts.
 * @return Number of bytes copied.
 */
uint16_t pifRingBuffer_CopyToArray(uint8_t* p_dst, uint16_t count, PifRingBuffer* p_src, uint16_t pos);

/**
 * @brief Copies the bytes of a ring buffer from pos on into another, as many as fit, leaving them in the
 *        source.
 * @param p_dst Ring buffer to add to.
 * @param p_src Ring buffer to copy from.
 * @param pos Offset from the tail of p_src where the copy starts.
 * @return Number of bytes copied.
 */
uint16_t pifRingBuffer_CopyAll(PifRingBuffer* p_dst, PifRingBuffer* p_src, uint16_t pos);

/**
 * @brief Copies length bytes of a ring buffer from pos on into another, all or none, leaving them in the
 *        source.
 * @param p_dst Ring buffer to add to.
 * @param p_src Ring buffer to copy from.
 * @param pos Offset from the tail of p_src where the copy starts.
 * @param length Number of bytes to copy.
 * @return TRUE on success, FALSE with E_INVALID_PARAM when p_src holds fewer bytes, or
 *         E_OVERFLOW_BUFFER when p_dst has no room.
 */
BOOL pifRingBuffer_CopyLength(PifRingBuffer* p_dst, PifRingBuffer* p_src, uint16_t pos, uint16_t length);

/**
 * @brief Drops bytes from the tail and releases what pifRingBuffer_GetReadPointer() claimed.
 * @param p_owner Pointer to the target object instance.
 * @param size Bytes to remove from the tail. All of them when there are fewer.
 */
void pifRingBuffer_Remove(PifRingBuffer* p_owner, uint16_t size);

#ifdef __cplusplus
}
#endif


#endif  // PIF_RING_BUFFER_H
