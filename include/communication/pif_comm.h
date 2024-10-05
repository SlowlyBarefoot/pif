#ifndef PIF_COMM_H
#define PIF_COMM_H


#include "core/pif.h"


typedef uint16_t PifRegMask;

typedef void PifDevice;

typedef BOOL (*PifDeviceReadRegByte)(PifDevice* p_owner, uint8_t reg, uint8_t* p_data);
typedef BOOL (*PifDeviceReadRegWord)(PifDevice* p_owner, uint8_t reg, uint16_t* p_data);
typedef BOOL (*PifDeviceReadRegBytes)(PifDevice* p_owner, uint8_t reg, uint8_t* p_data, size_t size);
typedef BOOL (*PifDeviceReadRegBit8)(PifDevice* p_owner, uint8_t reg, PifRegMask mask, uint8_t* p_data);
typedef BOOL (*PifDeviceReadRegBit16)(PifDevice* p_owner, uint8_t reg, PifRegMask mask, uint16_t* p_data);

typedef BOOL (*PifDeviceWriteRegByte)(PifDevice* p_owner, uint8_t reg, uint8_t data);
typedef BOOL (*PifDeviceWriteRegWord)(PifDevice* p_owner, uint8_t reg, uint16_t data);
typedef BOOL (*PifDeviceWriteRegBytes)(PifDevice* p_owner, uint8_t reg, uint8_t* p_data, size_t size);
typedef BOOL (*PifDeviceWriteRegBit8)(PifDevice* p_owner, uint8_t reg, PifRegMask mask, uint8_t data);
typedef BOOL (*PifDeviceWriteRegBit16)(PifDevice* p_owner, uint8_t reg, PifRegMask mask, uint16_t data);

typedef struct StPifDeviceReg8Func
{
	void *p_device;

	PifDeviceReadRegByte read_byte;
	PifDeviceReadRegBytes read_bytes;
	PifDeviceReadRegBit8 read_bit;

	PifDeviceWriteRegByte write_byte;
	PifDeviceWriteRegBytes write_bytes;
	PifDeviceWriteRegBit8 write_bit;
} PifDeviceReg8Func;

typedef struct StPifDeviceReg16Func
{
	void *p_device;

	PifDeviceReadRegWord read_word;
	PifDeviceReadRegBytes read_bytes;
	PifDeviceReadRegBit16 read_bit;

	PifDeviceWriteRegWord write_word;
	PifDeviceWriteRegBytes write_bytes;
	PifDeviceWriteRegBit16 write_bit;
} PifDeviceReg16Func;

#endif  // PIF_COMM_H
