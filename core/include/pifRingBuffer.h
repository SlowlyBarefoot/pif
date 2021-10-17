#ifndef PIF_RING_BUFFER_H
#define PIF_RING_BUFFER_H


#include "pif.h"


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
		uint8_t is_static	: 1;
		uint8_t chop_off	: 2;	// RB_CHOP_OFF_
	} _bt;
    uint16_t _size;

	// Private Member Variable
	const char* __p_name;
    uint8_t* __p_buffer;
    uint16_t __head;
    uint16_t __tail;
    uint16_t __backup_head;
    union {
		char chop_off_char;
		uint16_t chop_off_length;
    } __ui;
} PifRingBuffer;


#ifdef __cplusplus
extern "C" {
#endif

PifRingBuffer* pifRingBuffer_CreateHeap(PifId id, uint16_t size);
PifRingBuffer* pifRingBuffer_CreateStatic(PifId id, uint16_t size, uint8_t* p_buffer);
void pifRingBuffer_Destroy(PifRingBuffer** pp_owner);

BOOL pifRingBuffer_InitHeap(PifRingBuffer* p_owner, PifId id, uint16_t size);
BOOL pifRingBuffer_InitStatic(PifRingBuffer* p_owner, PifId id, uint16_t size, uint8_t* p_buffer);
void pifRingBuffer_Clear(PifRingBuffer* p_owner);

BOOL pifRingBuffer_ResizeHeap(PifRingBuffer* p_owner, uint16_t size);

void pifRingBuffer_SetName(PifRingBuffer* p_owner, const char* p_name);

uint8_t* pifRingBuffer_GetTailPointer(PifRingBuffer* p_owner, uint16_t pos);

void pifRingBuffer_ChopsOffNone(PifRingBuffer* p_owner);
void pifRingBuffer_ChopsOffChar(PifRingBuffer* p_owner, char ch);
void pifRingBuffer_ChopsOffLength(PifRingBuffer* p_owner, uint16_t length);

BOOL pifRingBuffer_IsBuffer(PifRingBuffer* p_owner);
BOOL pifRingBuffer_IsEmpty(PifRingBuffer* p_owner);

uint16_t pifRingBuffer_GetFillSize(PifRingBuffer* p_owner);
uint16_t pifRingBuffer_GetLinerSize(PifRingBuffer* p_owner, uint16_t pos);
uint16_t pifRingBuffer_GetRemainSize(PifRingBuffer* p_owner);

void pifRingBuffer_BackupHead(PifRingBuffer* p_owner);
void pifRingBuffer_RestoreHead(PifRingBuffer* p_owner);

BOOL pifRingBuffer_PutByte(PifRingBuffer* p_owner, uint8_t data);
BOOL pifRingBuffer_PutData(PifRingBuffer* p_owner, uint8_t* p_data, uint16_t length);
BOOL pifRingBuffer_PutString(PifRingBuffer* p_owner, char* p_string);
BOOL pifRingBuffer_GetByte(PifRingBuffer* p_owner, uint8_t* p_data);

uint16_t pifRingBuffer_CopyToArray(uint8_t* p_dst, uint16_t count, PifRingBuffer* p_src, uint16_t pos);
uint16_t pifRingBuffer_CopyAll(PifRingBuffer* p_dst, PifRingBuffer* p_src, uint16_t pos);
BOOL pifRingBuffer_CopyLength(PifRingBuffer* p_dst, PifRingBuffer* p_src, uint16_t pos, uint16_t length);

void pifRingBuffer_Remove(PifRingBuffer* p_owner, uint16_t size);

#ifdef __cplusplus
}
#endif


#endif  // PIF_RING_BUFFER_H
