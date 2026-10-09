// SPDX-License-Identifier: BSD-3-Clause
#ifndef PIF_RING_BUFFER_H
#define PIF_RING_BUFFER_H


#include "core/pif.h"


#define RB_CHOP_OFF_NONE	0
#define RB_CHOP_OFF_CHAR	1
#define RB_CHOP_OFF_LENGTH	2


typedef struct StPifRingBuffer PifRingBuffer;

/**
 * @fn PifActRingBufferLock
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
 * pifRingBuffer_AttachActLock().
 *
 * Bytes read in place, by DMA for example, should be taken with pifRingBuffer_GetReadPointer(). They stay
 * claimed until the reader removes them, and a chop-off fails rather than drop them, so new data is
 * refused instead of overwriting bytes still on their way out.
 */
struct StPifRingBuffer
{
	// Public Member Variable

	// Read-only Member Variable
	PifId _id;
	struct {
		unsigned int is_static	: 1;
		unsigned int chop_off	: 2;	// RB_CHOP_OFF_
		unsigned int putting	: 1;	// Inside pifRingBuffer_BeginPutting() and its commit or rollback
	} _bt;
    uint16_t _size;

	// Private Member Variable
	const char* __p_name;
    uint8_t* __p_buffer;
    volatile uint16_t __head;		// End of the bytes the reader may take
    volatile uint16_t __tail;
    uint16_t __put_head;			// Where the writer puts next; ahead of __head only inside a put session
    volatile uint16_t __claim;		// Bytes from the tail handed out by pifRingBuffer_GetReadPointer()
    union {
		char chop_off_char;
		uint16_t chop_off_length;
    } __ui;

	// Private Action Function
	PifActRingBufferLock __act_lock;
};


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifRingBuffer_CreateHeap
 * @brief Creates and initializes a new ring buffer instance, then returns its handle when successful.
 * @param id Identifier value for the object or task.
 * @param size Size value used for allocation or capacity.
 * @return Pointer to the resulting object or data, or NULL if unavailable.
 */
PifRingBuffer* pifRingBuffer_CreateHeap(PifId id, uint16_t size);

/**
 * @fn pifRingBuffer_CreateStatic
 * @brief Creates and initializes a new ring buffer instance, then returns its handle when successful.
 * @param id Identifier value for the object or task.
 * @param size Size value used for allocation or capacity.
 * @param p_buffer Pointer to the caller-provided buffer.
 * @return Pointer to the resulting object or data, or NULL if unavailable.
 */
PifRingBuffer* pifRingBuffer_CreateStatic(PifId id, uint16_t size, uint8_t* p_buffer);

/**
 * @fn pifRingBuffer_Destroy
 * @brief Destroys the ring buffer instance and frees all resources associated with it.
 * @param pp_owner Address of the object pointer to destroy or clear.
 */
void pifRingBuffer_Destroy(PifRingBuffer** pp_owner);

/**
 * @fn pifRingBuffer_InitHeap
 * @brief Initializes the ring buffer with heap memory allocation and default runtime state.
 * @param p_owner Pointer to the target object instance.
 * @param id Identifier value for the object or task.
 * @param size Size value used for allocation or capacity.
 * @return TRUE on success, otherwise FALSE.
 */
BOOL pifRingBuffer_InitHeap(PifRingBuffer* p_owner, PifId id, uint16_t size);

/**
 * @fn pifRingBuffer_InitStatic
 * @brief Initializes the ring buffer with caller-provided static memory and default runtime state.
 * @param p_owner Pointer to the target object instance.
 * @param id Identifier value for the object or task.
 * @param size Size value used for allocation or capacity.
 * @param p_buffer Pointer to the caller-provided buffer.
 * @return TRUE on success, otherwise FALSE.
 */
BOOL pifRingBuffer_InitStatic(PifRingBuffer* p_owner, PifId id, uint16_t size, uint8_t* p_buffer);

/**
 * @fn pifRingBuffer_Clear
 * @brief Clears the ring buffer state and releases resources currently owned by the instance.
 * @param p_owner Pointer to the target object instance.
 */
void pifRingBuffer_Clear(PifRingBuffer* p_owner);

/**
 * @fn pifRingBuffer_Empty
 * @brief Marks the ring buffer as empty by resetting read/write progress to an empty state.
 * @param p_owner Pointer to the target object instance.
 */
void pifRingBuffer_Empty(PifRingBuffer* p_owner);

/**
 * @fn pifRingBuffer_ResizeHeap
 * @brief Resizes internal storage used by the ring buffer while preserving valid runtime constraints.
 * @param p_owner Pointer to the target object instance.
 * @param size Size value used for allocation or capacity.
 * @return TRUE on success, otherwise FALSE.
 */
BOOL pifRingBuffer_ResizeHeap(PifRingBuffer* p_owner, uint16_t size);

/**
 * @fn pifRingBuffer_SetName
 * @brief Sets configuration or runtime state for the ring buffer based on the provided parameters.
 * @param p_owner Pointer to the target object instance.
 * @param p_name Human-readable name string.
 */
void pifRingBuffer_SetName(PifRingBuffer* p_owner, const char* p_name);

/**
 * @fn pifRingBuffer_AttachActLock
 * @brief Gives the instance a way to keep the other side out while the tail moves. Without one, no locking is done.
 * @param p_owner Pointer to the target object instance.
 * @param act_lock Enters and leaves the locked section, or NULL to remove it.
 */
void pifRingBuffer_AttachActLock(PifRingBuffer* p_owner, PifActRingBufferLock act_lock);

/**
 * @fn pifRingBuffer_GetBuffer
 * @brief Returns the start of the buffer memory, _size bytes long, to hand to a circular DMA that
 *        pifRingBuffer_SetHead() follows. A heap buffer moves on pifRingBuffer_ResizeHeap(), so set the DMA up again
 *        after one.
 * @param p_owner Pointer to the target object instance.
 * @return Pointer to the buffer memory, or NULL if there is none.
 */
uint8_t* pifRingBuffer_GetBuffer(PifRingBuffer* p_owner);

/**
 * @fn pifRingBuffer_GetTailPointer
 * @brief Retrieves the requested value or pointer from the ring buffer without changing ownership.
 * @param p_owner Pointer to the target object instance.
 * @param pos Offset position within the buffer.
 * @return Pointer to the resulting object or data, or NULL if unavailable.
 */
uint8_t* pifRingBuffer_GetTailPointer(PifRingBuffer* p_owner, uint16_t pos);

/**
 * @fn pifRingBuffer_GetReadPointer
 * @brief Returns where the bytes from pos after the tail start and how many follow without wrapping, and claims
 *        them so that a chop-off cannot drop them before the reader removes them.
 * @param p_owner Pointer to the target object instance.
 * @param pos Offset from the tail.
 * @param p_length Receives the number of contiguous bytes, 0 if there is nothing from pos on.
 * @return Pointer to the first byte.
 */
uint8_t* pifRingBuffer_GetReadPointer(PifRingBuffer* p_owner, uint16_t pos, uint16_t* p_length);

/**
 * @fn pifRingBuffer_MoveHeadForLinear
 * @brief Executes the pifRingBuffer_MoveHeadForLinear operation for the ring buffer module according to the API contract.
 * @param p_owner Pointer to the target object instance.
 * @param size Size value used for allocation or capacity.
 * @return TRUE on success, otherwise FALSE.
 */
BOOL pifRingBuffer_MoveHeadForLinear(PifRingBuffer* p_owner, uint16_t size);

/**
 * @fn pifRingBuffer_SetHead
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
 * @fn pifRingBuffer_ChopsOffNone
 * @brief Executes the pifRingBuffer_ChopsOffNone operation for the ring buffer module according to the API contract.
 * @param p_owner Pointer to the target object instance.
 */
void pifRingBuffer_ChopsOffNone(PifRingBuffer* p_owner);

/**
 * @fn pifRingBuffer_ChopsOffChar
 * @brief Executes the pifRingBuffer_ChopsOffChar operation for the ring buffer module according to the API contract.
 * @param p_owner Pointer to the target object instance.
 * @param ch Single character value to output.
 */
void pifRingBuffer_ChopsOffChar(PifRingBuffer* p_owner, char ch);

/**
 * @fn pifRingBuffer_ChopsOffLength
 * @brief Executes the pifRingBuffer_ChopsOffLength operation for the ring buffer module according to the API contract.
 * @param p_owner Pointer to the target object instance.
 * @param length Length value in bytes or elements.
 */
void pifRingBuffer_ChopsOffLength(PifRingBuffer* p_owner, uint16_t length);

/**
 * @fn pifRingBuffer_IsBuffer
 * @brief Checks whether the ring buffer currently satisfies the requested condition.
 * @param p_owner Pointer to the target object instance.
 * @return TRUE on success, otherwise FALSE.
 */
BOOL pifRingBuffer_IsBuffer(PifRingBuffer* p_owner);

/**
 * @fn pifRingBuffer_IsEmpty
 * @brief Checks whether the ring buffer currently satisfies the requested condition.
 * @param p_owner Pointer to the target object instance.
 * @return TRUE on success, otherwise FALSE.
 */
BOOL pifRingBuffer_IsEmpty(PifRingBuffer* p_owner);

/**
 * @fn pifRingBuffer_GetFillSize
 * @brief Returns the number of bytes the reader may take. Bytes of an open put session are not counted.
 * @param p_owner Pointer to the target object instance.
 * @return Result value returned by this API.
 */
uint16_t pifRingBuffer_GetFillSize(PifRingBuffer* p_owner);

/**
 * @fn pifRingBuffer_GetRemainSize
 * @brief Returns the room left for the writer. Bytes of an open put session count as used.
 * @param p_owner Pointer to the target object instance.
 * @return Result value returned by this API.
 */
uint16_t pifRingBuffer_GetRemainSize(PifRingBuffer* p_owner);

/**
 * @fn pifRingBuffer_BeginPutting
 * @brief Starts a put session. Bytes put until pifRingBuffer_CommitPutting() stay hidden from the reader, and
 *        pifRingBuffer_RollbackPutting() discards them. An unfinished earlier session is discarded.
 * @param p_owner Pointer to the target object instance.
 */
void pifRingBuffer_BeginPutting(PifRingBuffer* p_owner);

/**
 * @fn pifRingBuffer_CommitPutting
 * @brief Ends the put session and hands all of its bytes to the reader at once.
 * @param p_owner Pointer to the target object instance.
 */
void pifRingBuffer_CommitPutting(PifRingBuffer* p_owner);

/**
 * @fn pifRingBuffer_RollbackPutting
 * @brief Ends the put session and discards its bytes. Does nothing outside a session.
 * @param p_owner Pointer to the target object instance.
 */
void pifRingBuffer_RollbackPutting(PifRingBuffer* p_owner);

/**
 * @fn pifRingBuffer_GetPointerPutting
 * @brief Returns a byte of the open put session, to patch it before the commit.
 * @param p_owner Pointer to the target object instance.
 * @param pos Offset from the start of the session.
 * @return Pointer to the resulting object or data, or NULL if unavailable.
 */
uint8_t* pifRingBuffer_GetPointerPutting(PifRingBuffer* p_owner, uint16_t pos);

/**
 * @fn pifRingBuffer_PutByte
 * @brief Writes input data into the ring buffer buffer while respecting capacity and overflow policies.
 * @param p_owner Pointer to the target object instance.
 * @param data Input byte value used in the operation.
 * @return TRUE on success, otherwise FALSE.
 */
BOOL pifRingBuffer_PutByte(PifRingBuffer* p_owner, uint8_t data);

/**
 * @fn pifRingBuffer_PutData
 * @brief Writes input data into the ring buffer buffer while respecting capacity and overflow policies.
 * @param p_owner Pointer to the target object instance.
 * @param p_data Pointer to input or output data buffer.
 * @param length Length value in bytes or elements.
 * @return TRUE on success, otherwise FALSE.
 */
BOOL pifRingBuffer_PutData(PifRingBuffer* p_owner, uint8_t* p_data, uint16_t length);

/**
 * @fn pifRingBuffer_PutString
 * @brief Writes input data into the ring buffer buffer while respecting capacity and overflow policies.
 * @param p_owner Pointer to the target object instance.
 * @param p_string Pointer to a null-terminated text string.
 * @return TRUE on success, otherwise FALSE.
 */
BOOL pifRingBuffer_PutString(PifRingBuffer* p_owner, char* p_string);

/**
 * @fn pifRingBuffer_GetByte
 * @brief Retrieves the requested value or pointer from the ring buffer without changing ownership.
 * @param p_owner Pointer to the target object instance.
 * @param p_data Pointer to input or output data buffer.
 * @return TRUE on success, otherwise FALSE.
 */
BOOL pifRingBuffer_GetByte(PifRingBuffer* p_owner, uint8_t* p_data);

/**
 * @fn pifRingBuffer_GetBytes
 * @brief Retrieves the requested value or pointer from the ring buffer without changing ownership.
 * @param p_owner Pointer to the target object instance.
 * @param p_data Pointer to input or output data buffer.
 * @param length Length value in bytes or elements.
 * @return Result value returned by this API.
 */
uint16_t pifRingBuffer_GetBytes(PifRingBuffer* p_owner, uint8_t* p_data, uint16_t length);

/**
 * @fn pifRingBuffer_CopyToArray
 * @brief Copies data managed by the ring buffer according to the requested range and boundary rules.
 * @param p_dst Pointer to destination buffer or object.
 * @param count Number of items or channels.
 * @param p_src Pointer to source buffer or object.
 * @param pos Offset position within the buffer.
 * @return Result value returned by this API.
 */
uint16_t pifRingBuffer_CopyToArray(uint8_t* p_dst, uint16_t count, PifRingBuffer* p_src, uint16_t pos);

/**
 * @fn pifRingBuffer_CopyAll
 * @brief Copies data managed by the ring buffer according to the requested range and boundary rules.
 * @param p_dst Pointer to destination buffer or object.
 * @param p_src Pointer to source buffer or object.
 * @param pos Offset position within the buffer.
 * @return Result value returned by this API.
 */
uint16_t pifRingBuffer_CopyAll(PifRingBuffer* p_dst, PifRingBuffer* p_src, uint16_t pos);

/**
 * @fn pifRingBuffer_CopyLength
 * @brief Copies data managed by the ring buffer according to the requested range and boundary rules.
 * @param p_dst Pointer to destination buffer or object.
 * @param p_src Pointer to source buffer or object.
 * @param pos Offset position within the buffer.
 * @param length Length value in bytes or elements.
 * @return TRUE on success, otherwise FALSE.
 */
BOOL pifRingBuffer_CopyLength(PifRingBuffer* p_dst, PifRingBuffer* p_src, uint16_t pos, uint16_t length);

/**
 * @fn pifRingBuffer_Remove
 * @brief Drops bytes from the tail and releases what pifRingBuffer_GetReadPointer() claimed.
 * @param p_owner Pointer to the target object instance.
 * @param size Size value used for allocation or capacity.
 */
void pifRingBuffer_Remove(PifRingBuffer* p_owner, uint16_t size);

#ifdef __cplusplus
}
#endif


#endif  // PIF_RING_BUFFER_H
