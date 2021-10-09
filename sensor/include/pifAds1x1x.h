#ifndef PIF_ADS1X1X_H
#define PIF_ADS1X1X_H


#include "pifI2c.h"


typedef enum _PIF_enAds1x1xType
{
	AT_en1115		= 0,
	AT_en1114		= 1,
	AT_en1113		= 2,
	AT_en1015		= 3,
	AT_en1014		= 4,
	AT_en1013		= 5
} PIF_enAds1x1xType;

typedef enum _PIF_enAds1x1xReg
{
    AR_enCONVERSION,
	AR_enCONFIG,
	AR_enLO_THRESH,
	AR_enHI_THRESH
} PIF_enAds1x1xReg;

typedef enum _PIF_enAds1x1xConfigMux
{
    ACM_enDIFF_0_1, // default
	ACM_enDIFF_0_3,
	ACM_enDIFF_1_3,
	ACM_enDIFF_2_3,
	ACM_enSINGLE_0,
	ACM_enSINGLE_1,
	ACM_enSINGLE_2,
	ACM_enSINGLE_3
} PIF_enAds1x1xConfigMux;

typedef enum _PIF_enAds1x1xConfigPGA
{
    ACP_enFSR_6_144V,
	ACP_enFSR_4_096V,
	ACP_enFSR_2_048V, // default
	ACP_enFSR_1_024V,
	ACP_enFSR_0_512V,
	ACP_enFSR_0_256V
} PIF_enAds1x1xConfigPGA;

typedef enum _PIF_enAds1x1xConfigMode
{
    ACM_enCONTINUOUS,
	ACM_enSINGLE_SHOT
} PIF_enAds1x1xConfigMode;

typedef enum _PIF_enAds1x1xConfigDR
{
    // for 12bit model
    ACD_enDR_12B_0128_SPS = 0x00,
	ACD_enDR_12B_0250_SPS,
	ACD_enDR_12B_0490_SPS,
	ACD_enDR_12B_0920_SPS,
	ACD_enDR_12B_1600_SPS, // default
	ACD_enDR_12B_2400_SPS,
	ACD_enDR_12B_3300_SPS,

    // for 16bit model
	ACD_enDR_16B_0008_SPS = 0x00,
	ACD_enDR_16B_0016_SPS,
	ACD_enDR_16B_0032_SPS,
	ACD_enDR_16B_0064_SPS,
	ACD_enDR_16B_0128_SPS, // default
	ACD_enDR_16B_0250_SPS,
	ACD_enDR_16B_0475_SPS,
	ACD_enDR_16B_0860_SPS
} PIF_enAds1x1xConfigDR;

typedef enum _PIF_enAds1x1xConfigCompMode
{
    ACCM_enTRADITIONAL, // default
	ACCM_enWINDOW
} PIF_enAds1x1xConfigCompMode;

typedef enum _PIF_enAds1x1xConfigCompPol
{
    ACCP_enACTIVE_L, // default
	ACCP_enACTIVE_H
} PIF_enAds1x1xConfigCompPol;

typedef enum _PIF_enAds1x1xConfigCompLat
{
    ACCL_enDISABLE, // default
	ACCL_enENABLE
} PIF_enAds1x1xConfigCompLat;

typedef enum _PIF_enAds1x1xConfigCompQue
{
    ACCQ_enONE,
	ACCQ_enTWO,
	ACCQ_enFOUR,
	ACCQ_enDISABLE
} PIF_enAds1x1xConfigCompQue;

typedef union _PIF_stAds1x1xConfig
{
	uint16_t usAll;
	struct {
		uint16_t COMP_QUE	: 2;	// Comparator queue and disable (ADS1114 and ADS1115 only)
		uint16_t COMP_LAT	: 1;	// Latching comparator (ADS1114 and ADS1115 only)
		uint16_t COMP_POL	: 1;	// Comparator polarity (ADS1114 and ADS1115 only)
		uint16_t COMP_MODE	: 1;	// Comparator mode (ADS1114 and ADS1115 only)
		uint16_t DR			: 3;	// Data rate
		uint16_t MODE		: 1;	// Device operating mode
		uint16_t PGA		: 3;	// Programmable gain amplifier configuration
		uint16_t MUX		: 3;	// Input multiplexer configuration (ADS1115 only)
		uint16_t OS_SSCS	: 1;	// Operational Status(R) or single-shot conversion start(W) */
	} bt;
} PIF_stAds1x1xConfig;

/**
 * @class _PIF_stAds1x1x
 * @brief
 */
typedef struct _PIF_stAds1x1x
{
	// Public Member Variable
	double dConvertVoltage;

	// Read-only Member Variable
	PIF_enAds1x1xType _enType;
	PIF_stI2c _stI2c;

	// Private Member Variable
    uint8_t __unResolution;
    uint8_t __unChannels;
    PIF_stAds1x1xConfig __stConfig;
    uint8_t __ucBitOffset;
    uint32_t __unConversionDelay;
} PIF_stAds1x1x;


#ifdef __cplusplus
extern "C" {
#endif

PIF_stAds1x1x *pifAds1x1x_Create(PifId usPifId, PIF_enAds1x1xType enType);
void pifAds1x1x_Destroy(PIF_stAds1x1x **ppstOwner);

void pifAds1x1x_SetAddress(PIF_stAds1x1x *pstOwner, uint8_t ucAddr);

int16_t pifAds1x1x_Read(PIF_stAds1x1x *pstOwner);
int16_t pifAds1x1x_ReadMux(PIF_stAds1x1x *pstOwner, PIF_enAds1x1xConfigMux enMux);

double pifAds1x1x_Voltage(PIF_stAds1x1x *pstOwner);
double pifAds1x1x_VoltageMux(PIF_stAds1x1x *pstOwner, PIF_enAds1x1xConfigMux enMux);

BOOL pifAds1x1x_SetConfig(PIF_stAds1x1x *pstOwner, PIF_stAds1x1xConfig *pstConfig);
PIF_stAds1x1xConfig pifAds1x1x_GetConfig(PIF_stAds1x1x *pstOwner);

BOOL pifAds1x1x_SingleShotConvert(PIF_stAds1x1x *pstOwner);

BOOL pifAds1x1x_SetMux(PIF_stAds1x1x *pstOwner, PIF_enAds1x1xConfigMux enMux);
PIF_enAds1x1xConfigMux pifAds1x1x_GetMux(PIF_stAds1x1x *pstOwner);

BOOL pifAds1x1x_SetGain(PIF_stAds1x1x *pstOwner, PIF_enAds1x1xConfigPGA enPGA);
PIF_enAds1x1xConfigPGA pifAds1x1x_GetGain(PIF_stAds1x1x *pstOwner);

BOOL pifAds1x1x_SetMode(PIF_stAds1x1x *pstOwner, PIF_enAds1x1xConfigMode enMode);
PIF_enAds1x1xConfigMode pifAds1x1x_GetMode(PIF_stAds1x1x *pstOwner);

BOOL pifAds1x1x_SetDataRate(PIF_stAds1x1x *pstOwner, PIF_enAds1x1xConfigDR enDR);
PIF_enAds1x1xConfigDR pifAds1x1x_GetDataRate(PIF_stAds1x1x *pstOwner);

BOOL pifAds1x1x_SetCompMode(PIF_stAds1x1x *pstOwner, PIF_enAds1x1xConfigCompMode enCompMode);
PIF_enAds1x1xConfigCompMode pifAds1x1x_GetCompMode(PIF_stAds1x1x *pstOwner);

BOOL pifAds1x1x_SetCompPol(PIF_stAds1x1x *pstOwner, PIF_enAds1x1xConfigCompPol enCompPol);
PIF_enAds1x1xConfigCompPol pifAds1x1x_GetCompPol(PIF_stAds1x1x *pstOwner);

BOOL pifAds1x1x_SetCompLat(PIF_stAds1x1x *pstOwner, PIF_enAds1x1xConfigCompLat enCompLat);
PIF_enAds1x1xConfigCompLat pifAds1x1x_GetCompLat(PIF_stAds1x1x *pstOwner);

BOOL pifAds1x1x_SetCompQue(PIF_stAds1x1x *pstOwner, PIF_enAds1x1xConfigCompQue enCompQue);
PIF_enAds1x1xConfigCompQue pifAds1x1x_GetCompQue(PIF_stAds1x1x *pstOwner);

BOOL pifAds1x1x_SetLoThresh(PIF_stAds1x1x *pstOwner, int16_t sThreshold);
int16_t pifAds1x1x_GetLoThresh(PIF_stAds1x1x *pstOwner);

BOOL pifAds1x1x_SetLoThreshVoltage(PIF_stAds1x1x *pstOwner, double sThreshold);
double pifAds1x1x_GetLoThreshVoltage(PIF_stAds1x1x *pstOwner);

BOOL pifAds1x1x_SetHiThresh(PIF_stAds1x1x *pstOwner, int16_t sThreshold);
int16_t pifAds1x1x_GetHiThresh(PIF_stAds1x1x *pstOwner);

BOOL pifAds1x1x_SetHiThreshVoltage(PIF_stAds1x1x *pstOwner, double sThreshold);
double pifAds1x1x_GetHiThreshVoltage(PIF_stAds1x1x *pstOwner);

#ifdef __cplusplus
}
#endif


#endif  // PIF_ADS1X1X_H
