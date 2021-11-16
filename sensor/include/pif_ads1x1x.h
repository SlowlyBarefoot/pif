#ifndef PIF_ADS1X1X_H
#define PIF_ADS1X1X_H


#include "pif_i2c.h"


typedef enum EnPifAds1x1xType
{
	AT_1115		= 0,
	AT_1114		= 1,
	AT_1113		= 2,
	AT_1015		= 3,
	AT_1014		= 4,
	AT_1013		= 5
} PifAds1x1xType;

typedef enum EnPifAds1x1xReg
{
    AR_CONVERSION,
	AR_CONFIG,
	AR_LO_THRESH,
	AR_HI_THRESH
} PifAds1x1xReg;

typedef enum ENPifAds1x1xConfigMux
{
    ACM_DIFF_0_1, // default
	ACM_DIFF_0_3,
	ACM_DIFF_1_3,
	ACM_DIFF_2_3,
	ACM_SINGLE_0,
	ACM_SINGLE_1,
	ACM_SINGLE_2,
	ACM_SINGLE_3
} PifAds1x1xConfigMux;

typedef enum EnPifAds1x1xConfigPGA
{
    ACP_FSR_6_144V,
	ACP_FSR_4_096V,
	ACP_FSR_2_048V, // default
	ACP_FSR_1_024V,
	ACP_FSR_0_512V,
	ACP_FSR_0_256V
} PifAds1x1xConfigPGA;

typedef enum EnPifAds1x1xConfigMode
{
    ACM_CONTINUOUS,
	ACM_SINGLE_SHOT
} PifAds1x1xConfigMode;

typedef enum EnPifAds1x1xConfigDR
{
    // for 12bit model
    ACD_DR_12B_0128_SPS = 0x00,
	ACD_DR_12B_0250_SPS,
	ACD_DR_12B_0490_SPS,
	ACD_DR_12B_0920_SPS,
	ACD_DR_12B_1600_SPS, // default
	ACD_DR_12B_2400_SPS,
	ACD_DR_12B_3300_SPS,

    // for 16bit model
	ACD_DR_16B_0008_SPS = 0x00,
	ACD_DR_16B_0016_SPS,
	ACD_DR_16B_0032_SPS,
	ACD_DR_16B_0064_SPS,
	ACD_DR_16B_0128_SPS, // default
	ACD_DR_16B_0250_SPS,
	ACD_DR_16B_0475_SPS,
	ACD_DR_16B_0860_SPS
} PifAds1x1xConfigDR;

typedef enum EnPifAds1x1xConfigCompMode
{
    ACCM_TRADITIONAL, // default
	ACCM_WINDOW
} PifAds1x1xConfigCompMode;

typedef enum EnPifAds1x1xConfigCompPol
{
    ACCP_ACTIVE_L, // default
	ACCP_ACTIVE_H
} PifAds1x1xConfigCompPol;

typedef enum EnPifAds1x1xConfigCompLat
{
    ACCL_DISABLE, // default
	ACCL_ENABLE
} PifAds1x1xConfigCompLat;

typedef enum EnPifAds1x1xConfigCompQue
{
    ACCQ_ONE,
	ACCQ_TWO,
	ACCQ_FOUR,
	ACCQ_DISABLE
} PifAds1x1xConfigCompQue;

typedef union StPifAds1x1xConfig
{
	uint16_t all;
	struct {
		uint16_t comp_que	: 2;	// Comparator queue and disable (ADS1114 and ADS1115 only)
		uint16_t comp_lat	: 1;	// Latching comparator (ADS1114 and ADS1115 only)
		uint16_t comp_pol	: 1;	// Comparator polarity (ADS1114 and ADS1115 only)
		uint16_t comp_mode	: 1;	// Comparator mode (ADS1114 and ADS1115 only)
		uint16_t dr			: 3;	// Data rate
		uint16_t mode		: 1;	// Device operating mode
		uint16_t pga		: 3;	// Programmable gain amplifier configuration
		uint16_t mux		: 3;	// Input multiplexer configuration (ADS1115 only)
		uint16_t os_sscs	: 1;	// Operational Status(R) or single-shot conversion start(W) */
	} bt;
} PifAds1x1xConfig;

/**
 * @class StPifAds1x1x
 * @brief
 */
typedef struct StPifAds1x1x
{
	// Public Member Variable
	double convert_voltage;

	// Read-only Member Variable
	PifAds1x1xType _type;
	PifI2c _i2c;

	// Private Member Variable
    uint8_t __resolution;
    uint8_t __channels;
    PifAds1x1xConfig __config;
    uint8_t __bit_offset;
    uint32_t __conversion_delay;
} PifAds1x1x;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifAds1x1x_Init
 * @brief
 * @param p_owner
 * @param id
 * @param type
 * @return
 */
BOOL pifAds1x1x_Init(PifAds1x1x* p_owner, PifId id, PifAds1x1xType type);

/**
 * @fn pifAds1x1x_Clear
 * @brief
 * @param p_owner
 */
void pifAds1x1x_Clear(PifAds1x1x* p_owner);

/**
 * @fn pifAds1x1x_SetAddress
 * @brief
 * @param p_owner
 * @param addr
 */
void pifAds1x1x_SetAddress(PifAds1x1x* p_owner, uint8_t addr);

/**
 * @fn pifAds1x1x_Read
 * @brief
 * @param p_owner
 * @return
 */
int16_t pifAds1x1x_Read(PifAds1x1x* p_owner);

/**
 * @fn pifAds1x1x_ReadMux
 * @brief
 * @param p_owner
 * @param mux
 * @return
 */
int16_t pifAds1x1x_ReadMux(PifAds1x1x* p_owner, PifAds1x1xConfigMux mux);

/**
 * @fn pifAds1x1x_Voltage
 * @brief
 * @param p_owner
 * @return
 */
double pifAds1x1x_Voltage(PifAds1x1x* p_owner);

/**
 * @fn pifAds1x1x_VoltageMux
 * @brief
 * @param p_owner
 * @param mux
 * @return
 */
double pifAds1x1x_VoltageMux(PifAds1x1x* p_owner, PifAds1x1xConfigMux mux);

/**
 * @fn pifAds1x1x_SetConfig
 * @brief
 * @param p_owner
 * @param p_config
 * @return
 */
BOOL pifAds1x1x_SetConfig(PifAds1x1x* p_owner, PifAds1x1xConfig* p_config);

/**
 * @fn pifAds1x1x_GetConfig
 * @brief
 * @param p_owner
 * @return
 */
PifAds1x1xConfig pifAds1x1x_GetConfig(PifAds1x1x* p_owner);

/**
 * @fn pifAds1x1x_SingleShotConvert
 * @brief
 * @param p_owner
 * @return
 */
BOOL pifAds1x1x_SingleShotConvert(PifAds1x1x* p_owner);

/**
 * @fn pifAds1x1x_SetMux
 * @brief
 * @param p_owner
 * @param mux
 * @return
 */
BOOL pifAds1x1x_SetMux(PifAds1x1x* p_owner, PifAds1x1xConfigMux mux);

/**
 * @fn pifAds1x1x_GetMux
 * @brief
 * @param p_owner
 * @return
 */
PifAds1x1xConfigMux pifAds1x1x_GetMux(PifAds1x1x* p_owner);

/**
 * @fn pifAds1x1x_SetGain
 * @brief
 * @param p_owner
 * @param pga
 * @return
 */
BOOL pifAds1x1x_SetGain(PifAds1x1x* p_owner, PifAds1x1xConfigPGA pga);

/**
 * @fn pifAds1x1x_GetGain
 * @brief
 * @param p_owner
 * @return
 */
PifAds1x1xConfigPGA pifAds1x1x_GetGain(PifAds1x1x* p_owner);

/**
 * @fn pifAds1x1x_SetMode
 * @brief
 * @param p_owner
 * @param mode
 * @return
 */
BOOL pifAds1x1x_SetMode(PifAds1x1x* p_owner, PifAds1x1xConfigMode mode);

/**
 * @fn pifAds1x1x_GetMode
 * @brief
 * @param p_owner
 * @return
 */
PifAds1x1xConfigMode pifAds1x1x_GetMode(PifAds1x1x* p_owner);

/**
 * @fn pifAds1x1x_SetDataRate
 * @brief
 * @param p_owner
 * @param dr
 * @return
 */
BOOL pifAds1x1x_SetDataRate(PifAds1x1x* p_owner, PifAds1x1xConfigDR dr);

/**
 * @fn pifAds1x1x_GetDataRate
 * @brief
 * @param p_owner
 * @return
 */
PifAds1x1xConfigDR pifAds1x1x_GetDataRate(PifAds1x1x* p_owner);

/**
 * @fn pifAds1x1x_SetCompMode
 * @brief
 * @param p_owner
 * @param comp_mode
 * @return
 */
BOOL pifAds1x1x_SetCompMode(PifAds1x1x* p_owner, PifAds1x1xConfigCompMode comp_mode);

/**
 * @fn pifAds1x1x_GetCompMode
 * @brief
 * @param p_owner
 * @return
 */
PifAds1x1xConfigCompMode pifAds1x1x_GetCompMode(PifAds1x1x* p_owner);

/**
 * @fn pifAds1x1x_SetCompPol
 * @brief
 * @param p_owner
 * @param comp_pol
 * @return
 */
BOOL pifAds1x1x_SetCompPol(PifAds1x1x* p_owner, PifAds1x1xConfigCompPol comp_pol);

/**
 * @fn pifAds1x1x_GetCompPol
 * @brief
 * @param p_owner
 * @return
 */
PifAds1x1xConfigCompPol pifAds1x1x_GetCompPol(PifAds1x1x* p_owner);

/**
 * @fn pifAds1x1x_SetCompLat
 * @brief
 * @param p_owner
 * @param comp_lat
 * @return
 */
BOOL pifAds1x1x_SetCompLat(PifAds1x1x* p_owner, PifAds1x1xConfigCompLat compLat);

/**
 * @fn pifAds1x1x_GetCompLat
 * @brief
 * @param p_owner
 * @return
 */
PifAds1x1xConfigCompLat pifAds1x1x_GetCompLat(PifAds1x1x* p_owner);

/**
 * @fn pifAds1x1x_SetCompQue
 * @brief
 * @param p_owner
 * @param comp_que
 * @return
 */
BOOL pifAds1x1x_SetCompQue(PifAds1x1x* p_owner, PifAds1x1xConfigCompQue comp_que);

/**
 * @fn pifAds1x1x_GetCompQue
 * @brief
 * @param p_owner
 * @return
 */
PifAds1x1xConfigCompQue pifAds1x1x_GetCompQue(PifAds1x1x* p_owner);

/**
 * @fn pifAds1x1x_SetLoThresh
 * @brief
 * @param p_owner
 * @param threshold
 * @return
 */
BOOL pifAds1x1x_SetLoThresh(PifAds1x1x* p_owner, int16_t threshold);

/**
 * @fn pifAds1x1x_GetLoThresh
 * @brief
 * @param p_owner
 * @return
 */
int16_t pifAds1x1x_GetLoThresh(PifAds1x1x* p_owner);

/**
 * @fn pifAds1x1x_SetLoThreshVoltage
 * @brief
 * @param p_owner
 * @param threshold
 * @return
 */
BOOL pifAds1x1x_SetLoThreshVoltage(PifAds1x1x* p_owner, double threshold);

/**
 * @fn pifAds1x1x_GetLoThreshVoltage
 * @brief
 * @param p_owner
 * @return
 */
double pifAds1x1x_GetLoThreshVoltage(PifAds1x1x* p_owner);

/**
 * @fn pifAds1x1x_SetHiThresh
 * @brief
 * @param p_owner
 * @param threshold
 * @return
 */
BOOL pifAds1x1x_SetHiThresh(PifAds1x1x* p_owner, int16_t threshold);

/**
 * @fn pifAds1x1x_GetHiThresh
 * @brief
 * @param p_owner
 * @return
 */
int16_t pifAds1x1x_GetHiThresh(PifAds1x1x* p_owner);

/**
 * @fn pifAds1x1x_SetHiThreshVoltage
 * @brief
 * @param p_owner
 * @param threshold
 * @return
 */
BOOL pifAds1x1x_SetHiThreshVoltage(PifAds1x1x* p_owner, double threshold);

/**
 * @fn pifAds1x1x_GetHiThreshVoltage
 * @brief
 * @param p_owner
 * @return
 */
double pifAds1x1x_GetHiThreshVoltage(PifAds1x1x* p_owner);

#ifdef __cplusplus
}
#endif


#endif  // PIF_ADS1X1X_H
