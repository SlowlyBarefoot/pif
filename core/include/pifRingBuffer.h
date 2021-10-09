#ifndef PIF_RING_BUFFER_H
#define PIF_RING_BUFFER_H


#include "pif.h"


#define RB_CHOP_OFF_NONE	0
#define RB_CHOP_OFF_CHAR	1
#define RB_CHOP_OFF_LENGTH	2


/**
 * @class _PIF_stRingBuffer
 * @brief 환형 버퍼용 구조체
 */
typedef struct _PIF_stRingBuffer
{
	// Public Member Variable

	// Read-only Member Variable
	PifId _usPifId;
	struct {
		uint8_t Static	: 1;
		uint8_t ChopOff	: 2;	// RB_CHOP_OFF_
	} _bt;
    uint16_t _usSize;

	// Private Member Variable
	const char *__psName;
    uint8_t *__pucBuffer;
    uint16_t __usHead;
    uint16_t __usTail;
    uint16_t __usBackupHead;
    union {
		char cChopOffChar;
		uint16_t usChopOffLength;
    } __ui;
} PIF_stRingBuffer;


#ifdef __cplusplus
extern "C" {
#endif

PIF_stRingBuffer *pifRingBuffer_InitHeap(PifId usPifId, uint16_t usSize);
PIF_stRingBuffer *pifRingBuffer_InitStatic(PifId usPifId, uint16_t usSize, uint8_t *pucBuffer);
void pifRingBuffer_Exit(PIF_stRingBuffer *pstOwner);

BOOL pifRingBuffer_ResizeHeap(PIF_stRingBuffer *pstOwner, uint16_t usSize);

void pifRingBuffer_SetName(PIF_stRingBuffer *pstOwner, const char *psName);

uint8_t *pifRingBuffer_GetTailPointer(PIF_stRingBuffer *pstOwner, uint16_t usPos);

void pifRingBuffer_ChopsOffNone(PIF_stRingBuffer *pstOwner);
void pifRingBuffer_ChopsOffChar(PIF_stRingBuffer *pstOwner, char cChar);
void pifRingBuffer_ChopsOffLength(PIF_stRingBuffer *pstOwner, uint16_t usLength);

BOOL pifRingBuffer_IsBuffer(PIF_stRingBuffer *pstOwner);
BOOL pifRingBuffer_IsEmpty(PIF_stRingBuffer *pstOwner);

uint16_t pifRingBuffer_GetFillSize(PIF_stRingBuffer *pstOwner);
uint16_t pifRingBuffer_GetLinerSize(PIF_stRingBuffer *pstOwner, uint16_t usPos);
uint16_t pifRingBuffer_GetRemainSize(PIF_stRingBuffer *pstOwner);

void pifRingBuffer_BackupHead(PIF_stRingBuffer *pstOwner);
void pifRingBuffer_RestoreHead(PIF_stRingBuffer *pstOwner);

BOOL pifRingBuffer_PutByte(PIF_stRingBuffer *pstOwner, uint8_t ucData);
BOOL pifRingBuffer_PutData(PIF_stRingBuffer *pstOwner, uint8_t *pucData, uint16_t usLength);
BOOL pifRingBuffer_PutString(PIF_stRingBuffer *pstOwner, char *pcString);
BOOL pifRingBuffer_GetByte(PIF_stRingBuffer *pstOwner, uint8_t *pucData);

uint16_t pifRingBuffer_CopyToArray(uint8_t *pucDst, uint16_t usCount, PIF_stRingBuffer *pstSrc, uint16_t usPos);
uint16_t pifRingBuffer_CopyAll(PIF_stRingBuffer *pstDst, PIF_stRingBuffer *pstSrc, uint16_t usPos);
BOOL pifRingBuffer_CopyLength(PIF_stRingBuffer *pstDst, PIF_stRingBuffer *pstSrc, uint16_t usPos, uint16_t usLength);

void pifRingBuffer_Remove(PIF_stRingBuffer *pstOwner, uint16_t usSize);

#ifdef __cplusplus
}
#endif


#endif  // PIF_RING_BUFFER_H
