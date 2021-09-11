#ifndef PIF_I2C_H
#define PIF_I2C_H


#include "pif.h"


typedef enum _PIF_enI2cState
{
	IS_enIdle,
	IS_enRun,
	IS_enComplete,
	IS_enError
} PIF_enI2cState;


struct _PIF_stI2c;
typedef struct _PIF_stI2c PIF_stI2c;

typedef BOOL (*PIF_actI2cRead)(PIF_stI2c *pstOwner, uint16_t usSize);
typedef BOOL (*PIF_actI2cWrite)(PIF_stI2c *pstOwner, uint16_t usSize);

/**
 * @class _PIF_stI2c
 * @brief
 */
struct _PIF_stI2c
{
	// Public Member Variable
    uint8_t ucAddr;
	uint8_t ucDataSize;
	uint8_t *pucData;

	// Read-only Member Variable
	PIF_usId _usPifId;
	volatile PIF_enI2cState _enStateRead;
	volatile PIF_enI2cState _enStateWrite;

	// Private Member Variable

	// Private Action Function
	PIF_actI2cRead __actRead;
	PIF_actI2cWrite __actWrite;
};


#ifdef __cplusplus
extern "C" {
#endif

BOOL pifI2c_Add(PIF_stI2c *pstOwner, PIF_usId usPifId, uint16_t ucDataSize);

void pifI2c_ScanAddress(PIF_stI2c *pstOwner);

BOOL pifI2c_Read(PIF_stI2c *pstOwner, uint8_t ucSize);
BOOL pifI2c_Write(PIF_stI2c *pstOwner,  uint8_t ucSize);

void pifI2c_sigEndRead(PIF_stI2c *pstOwner, BOOL bResult);
void pifI2c_sigEndWrite(PIF_stI2c *pstOwner, BOOL bResult);

void pifI2c_AttachAction(PIF_stI2c *pstOwner, PIF_actI2cRead actRead, PIF_actI2cWrite actWrite);

#ifdef __cplusplus
}
#endif


#endif  // PIF_I2C_H
