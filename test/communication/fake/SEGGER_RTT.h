// SPDX-License-Identifier: BSD-3-Clause
// Stands in for SEGGER_RTT.h in the host tests: the calls pif_rtt.c makes, which the test implements.
#ifndef SEGGER_RTT_H
#define SEGGER_RTT_H

#define SEGGER_RTT_MAX_NUM_UP_BUFFERS	3
#define SEGGER_RTT_MODE_NO_BLOCK_SKIP	0

void SEGGER_RTT_Init(void);
unsigned SEGGER_RTT_Write(unsigned BufferIndex, const void *pBuffer, unsigned NumBytes);
unsigned SEGGER_RTT_Read(unsigned BufferIndex, void *pBuffer, unsigned BufferSize);
unsigned SEGGER_RTT_GetAvailWriteSpace(unsigned BufferIndex);
int SEGGER_RTT_ConfigUpBuffer(unsigned BufferIndex, const char *sName, void *pBuffer, unsigned BufferSize, unsigned Flags);
int SEGGER_RTT_ConfigDownBuffer(unsigned BufferIndex, const char *sName, void *pBuffer, unsigned BufferSize, unsigned Flags);

#endif
