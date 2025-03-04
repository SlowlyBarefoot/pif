#ifndef PIF_RING_BUFFER_H
#define PIF_RING_BUFFER_H


#include "core/pif.h"


#define RB_CHOP_OFF_NONE	0
#define RB_CHOP_OFF_CHAR	1
#define RB_CHOP_OFF_LENGTH	2


/**
 * @class StPifRingBuffer
 * @brief 환형 버퍼용 구조체
 */
typedef struct StPifRingBuffer
{
	// Public Member Variable

	// Read-only Member Variable
	PifId _id;
	struct {
		unsigned int is_static	: 1;
		unsigned int chop_off	: 2;	// RB_CHOP_OFF_
	} _bt;
    uint16_t _size;

	// Private Member Variable
	const char* __p_name;
    uint8_t* __p_buffer;
    volatile uint16_t __head;
    volatile uint16_t __tail;
    uint16_t __backup_head;
    union {
		char chop_off_char;
		uint16_t chop_off_length;
    } __ui;
} PifRingBuffer;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifRingBuffer_CreateHeap
 * @brief
 * @param id
 * @param size
 * @return 
 */
PifRingBuffer* pifRingBuffer_CreateHeap(PifId id, uint16_t size);

/**
 * @fn pifRingBuffer_CreateStatic
 * @brief
 * @param id
 * @param size
 * @param p_buffer
 * @return
 */
PifRingBuffer* pifRingBuffer_CreateStatic(PifId id, uint16_t size, uint8_t* p_buffer);

/**
 * @fn pifRingBuffer_Destroy
 * @brief
 * @param pp_owner
 */
void pifRingBuffer_Destroy(PifRingBuffer** pp_owner);

/**
 * @fn pifRingBuffer_InitHeap
 * @brief
 * @param p_owner
 * @param id
 * @param size
 * @return
 */
BOOL pifRingBuffer_InitHeap(PifRingBuffer* p_owner, PifId id, uint16_t size);

/**
 * @fn pifRingBuffer_InitStatic
 * @brief
 * @param p_owner
 * @param id
 * @param size
 * @param p_buffer
 * @return
 */
BOOL pifRingBuffer_InitStatic(PifRingBuffer* p_owner, PifId id, uint16_t size, uint8_t* p_buffer);

/**
 * @fn pifRingBuffer_Clear
 * @brief 
 * @param p_owner
 */
void pifRingBuffer_Clear(PifRingBuffer* p_owner);

/**
 * @fn pifRingBuffer_Empty
 * @brief
 * @param p_owner
 */
void pifRingBuffer_Empty(PifRingBuffer* p_owner);

/**
 * @fn pifRingBuffer_ResizeHeap
 * @brief
 * @param p_owner
 * @param size
 * @return
 */
BOOL pifRingBuffer_ResizeHeap(PifRingBuffer* p_owner, uint16_t size);

/**
 * @fn pifRingBuffer_SetName
 * @brief
 * @param p_owner
 * @param p_name
 */
void pifRingBuffer_SetName(PifRingBuffer* p_owner, const char* p_name);

/**
 * @fn pifRingBuffer_GetTailPointer
 * @brief
 * @param p_owner
 * @param pos
 * @return
 */
uint8_t* pifRingBuffer_GetTailPointer(PifRingBuffer* p_owner, uint16_t pos);

/**
 * @fn pifRingBuffer_MoveHeadForLinear
 * @brief
 * @param p_owner
 * @param size
 * @return
 */
BOOL pifRingBuffer_MoveHeadForLinear(PifRingBuffer* p_owner, uint16_t size);

/**
 * @fn pifRingBuffer_ChopsOffNone
 * @brief
 * @param p_owner
 */
void pifRingBuffer_ChopsOffNone(PifRingBuffer* p_owner);

/**
 * @fn pifRingBuffer_ChopsOffChar
 * @brief
 * @param p_owner
 * @param ch
 */
void pifRingBuffer_ChopsOffChar(PifRingBuffer* p_owner, char ch);

/**
 * @fn pifRingBuffer_ChopsOffLength
 * @brief
 * @param p_owner
 * @param length
 */
void pifRingBuffer_ChopsOffLength(PifRingBuffer* p_owner, uint16_t length);

/**
 * @fn pifRingBuffer_IsBuffer
 * @brief
 * @param p_owner
 * @return
 */
BOOL pifRingBuffer_IsBuffer(PifRingBuffer* p_owner);

/**
 * @fn pifRingBuffer_IsEmpty
 * @brief
 * @param p_owner
 * @return
 */
BOOL pifRingBuffer_IsEmpty(PifRingBuffer* p_owner);

/**
 * @fn pifRingBuffer_GetFillSize
 * @brief
 * @param p_owner
 * @return
 */
uint16_t pifRingBuffer_GetFillSize(PifRingBuffer* p_owner);

/**
 * @fn pifRingBuffer_GetLinerSize
 * @brief
 * @param p_owner
 * @param pos
 * @return
 */
uint16_t pifRingBuffer_GetLinerSize(PifRingBuffer* p_owner, uint16_t pos);

/**
 * @fn pifRingBuffer_GetRemainSize
 * @brief
 * @param p_owner
 * @return
 */
uint16_t pifRingBuffer_GetRemainSize(PifRingBuffer* p_owner);

/**
 * @fn pifRingBuffer_BeginPutting
 * @brief
 * @param p_owner
 */
void pifRingBuffer_BeginPutting(PifRingBuffer* p_owner);

/**
 * @fn pifRingBuffer_CommitPutting
 * @brief
 * @param p_owner
 */
void pifRingBuffer_CommitPutting(PifRingBuffer* p_owner);

/**
 * @fn pifRingBuffer_RollbackPutting
 * @brief
 * @param p_owner
 */
void pifRingBuffer_RollbackPutting(PifRingBuffer* p_owner);

/**
 * @fn pifRingBuffer_GetPointerPutting
 * @brief
 * @param p_owner
 * @param pos
 * @return
 */
uint8_t* pifRingBuffer_GetPointerPutting(PifRingBuffer* p_owner, uint16_t pos);

/**
 * @fn pifRingBuffer_PutByte
 * @brief
 * @param p_owner
 * @param data
 * @return
 */
BOOL pifRingBuffer_PutByte(PifRingBuffer* p_owner, uint8_t data);

/**
 * @fn pifRingBuffer_PutData
 * @brief
 * @param p_owner
 * @param p_data
 * @param length
 * @return
 */
BOOL pifRingBuffer_PutData(PifRingBuffer* p_owner, uint8_t* p_data, uint16_t length);

/**
 * @fn pifRingBuffer_PutString
 * @brief
 * @param p_owner
 * @param p_string
 * @return
 */
BOOL pifRingBuffer_PutString(PifRingBuffer* p_owner, char* p_string);

/**
 * @fn pifRingBuffer_GetByte
 * @brief
 * @param p_owner
 * @param p_data
 * @return
 */
BOOL pifRingBuffer_GetByte(PifRingBuffer* p_owner, uint8_t* p_data);

/**
 * @fn pifRingBuffer_GetBytes
 * @brief
 * @param p_owner
 * @param p_data
 * @param length
 * @return
 */
uint16_t pifRingBuffer_GetBytes(PifRingBuffer* p_owner, uint8_t* p_data, uint16_t length);

/**
 * @fn pifRingBuffer_CopyToArray
 * @brief
 * @param p_dst
 * @param count
 * @param p_src
 * @param pos
 * @return
 */
uint16_t pifRingBuffer_CopyToArray(uint8_t* p_dst, uint16_t count, PifRingBuffer* p_src, uint16_t pos);

/**
 * @fn pifRingBuffer_CopyAll
 * @brief
 * @param p_dst
 * @param p_src
 * @param pos
 * @return
 */
uint16_t pifRingBuffer_CopyAll(PifRingBuffer* p_dst, PifRingBuffer* p_src, uint16_t pos);

/**
 * @fn pifRingBuffer_CopyLength
 * @brief
 * @param p_dst
 * @param p_src
 * @param pos
 * @param length
 * @return
 */
BOOL pifRingBuffer_CopyLength(PifRingBuffer* p_dst, PifRingBuffer* p_src, uint16_t pos, uint16_t length);

/**
 * @fn pifRingBuffer_Remove
 * @brief
 * @param p_owner
 * @param size
 */
void pifRingBuffer_Remove(PifRingBuffer* p_owner, uint16_t size);

#ifdef __cplusplus
}
#endif


#endif  // PIF_RING_BUFFER_H
