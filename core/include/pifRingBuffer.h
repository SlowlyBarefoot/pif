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
	struct {
		uint8_t btShare		: 1;
		uint8_t btChopOff	: 2;	// RB_CHOP_OFF_
	};
    uint16_t usSize;

	// Private Member Variable
    char *__pcBuffer;
    uint16_t __usHead;
    uint16_t __usTail;
    uint16_t __usBackupHead;
    union {
		char __cChopOffChar;
		uint16_t __usChopOffLength;
    };
} PIF_stRingBuffer;


#ifdef __cplusplus
extern "C" {
#endif

void pifRingBuffer_Init(PIF_stRingBuffer *pstOwner);
BOOL pifRingBuffer_InitAlloc(PIF_stRingBuffer *pstOwner, uint16_t usSize);
void pifRingBuffer_InitShare(PIF_stRingBuffer *pstOwner, uint16_t usSize, char *pcBuffer);
void pifRingBuffer_Exit(PIF_stRingBuffer *pstOwner);

void pifRingBuffer_ChopOffNone(PIF_stRingBuffer *pstOwner);
void pifRingBuffer_ChopOffChar(PIF_stRingBuffer *pstOwner, char cChar);
void pifRingBuffer_ChopOffLength(PIF_stRingBuffer *pstOwner, uint16_t usLength);

BOOL pifRingBuffer_IsAlloc(PIF_stRingBuffer *pstOwner);
BOOL pifRingBuffer_IsEmpty(PIF_stRingBuffer *pstOwner);

uint16_t pifRingBuffer_GetFillSize(PIF_stRingBuffer *pstOwner);
uint16_t pifRingBuffer_GetRemainSize(PIF_stRingBuffer *pstOwner);

void pifRingBuffer_BackupHead(PIF_stRingBuffer *pstOwner);
void pifRingBuffer_RestoreHead(PIF_stRingBuffer *pstOwner);

BOOL pifRingBuffer_PushByte(PIF_stRingBuffer *pstOwner, uint8_t ucData);
BOOL pifRingBuffer_PushData(PIF_stRingBuffer *pstOwner, uint8_t *pucData, uint16_t usLength);
BOOL pifRingBuffer_PushString(PIF_stRingBuffer *pstOwner, char *pcString);
BOOL pifRingBuffer_Pop(PIF_stRingBuffer *pstOwner, uint8_t *pucData);

uint16_t pifRingBuffer_CopyToArray(uint8_t *punDst, PIF_stRingBuffer *pstSrc, uint16_t usCount);
uint16_t pifRingBuffer_CopyAll(PIF_stRingBuffer *pstDst, PIF_stRingBuffer *pstSrc);
BOOL pifRingBuffer_CopyLength(PIF_stRingBuffer *pstDst, PIF_stRingBuffer *pstSrc, uint16_t usPos, uint16_t usLength);

void pifRingBuffer_Remove(PIF_stRingBuffer *pstOwner, uint16_t usSize);

#ifdef __cplusplus
}
#endif


#endif  // PIF_RING_BUFFER_H
