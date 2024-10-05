#ifndef PIF_ADS1X1X_H
#define PIF_ADS1X1X_H


#include "communication/pif_i2c.h"


#define ADS1X1X_I2C_ADDR(N)		(0x48 + (N))


typedef enum EnPifAds1x1xType
{
	ADS1X1X_TYPE_1115		= 0,
	ADS1X1X_TYPE_1114		= 1,
	ADS1X1X_TYPE_1113		= 2,
	ADS1X1X_TYPE_1015		= 3,
	ADS1X1X_TYPE_1014		= 4,
	ADS1X1X_TYPE_1013		= 5
} PifAds1x1xType;

typedef enum EnPifAds1x1xReg
{
    ADS1X1X_REG_CONVERSION,
	ADS1X1X_REG_CONFIG,
	ADS1X1X_REG_LO_THRESH,
	ADS1X1X_REG_HI_THRESH
} PifAds1x1xReg;


// Register : Config

typedef enum EnPifAds1x1xSscs
{
    ADS1X1X_SSCS_NO_EFECT			= 0 << 15,
	ADS1X1X_SSCS_SINGLE				= 1 << 15
} PifAds1x1xSscs;

typedef enum ENPifAds1x1xMux
{
    ADS1X1X_MUX_DIFF_0_1			= 0 < 12, 	// default
	ADS1X1X_MUX_DIFF_0_3			= 1 < 12,
	ADS1X1X_MUX_DIFF_1_3			= 2 < 12,
	ADS1X1X_MUX_DIFF_2_3			= 3 < 12,
	ADS1X1X_MUX_SINGLE_0			= 4 < 12,
	ADS1X1X_MUX_SINGLE_1			= 5 < 12,
	ADS1X1X_MUX_SINGLE_2			= 6 < 12,
	ADS1X1X_MUX_SINGLE_3			= 7 < 12
} PifAds1x1xMux;

typedef enum EnPifAds1x1xPGA
{
    ADS1X1X_PGA_FSR_6_144V			= 0 << 9,
	ADS1X1X_PGA_FSR_4_096V			= 1 << 9,
	ADS1X1X_PGA_FSR_2_048V			= 2 << 9,	// default
	ADS1X1X_PGA_FSR_1_024V			= 3 << 9,
	ADS1X1X_PGA_FSR_0_512V			= 4 << 9,
	ADS1X1X_PGA_FSR_0_256V			= 5 << 9
} PifAds1x1xPGA;

typedef enum EnPifAds1x1xMode
{
    ADS1X1X_MODE_CONTINUOUS			= 0 << 8,
	ADS1X1X_MODE_SINGLE_SHOT		= 1 << 8
} PifAds1x1xMode;

typedef enum EnPifAds1x1xDR
{
    // for 12bit model
    ADS1X1X_DR_12B_0128_SPS			= 0 << 5,
	ADS1X1X_DR_12B_0250_SPS			= 1 << 5,
	ADS1X1X_DR_12B_0490_SPS			= 2 << 5,
	ADS1X1X_DR_12B_0920_SPS			= 3 << 5,
	ADS1X1X_DR_12B_1600_SPS			= 4 << 5,	// default
	ADS1X1X_DR_12B_2400_SPS			= 5 << 5,
	ADS1X1X_DR_12B_3300_SPS			= 6 << 5,

    // for 16bit model
	ADS1X1X_DR_16B_0008_SPS			= 0 << 5,
	ADS1X1X_DR_16B_0016_SPS			= 1 << 5,
	ADS1X1X_DR_16B_0032_SPS			= 2 << 5,
	ADS1X1X_DR_16B_0064_SPS			= 3 << 5,
	ADS1X1X_DR_16B_0128_SPS			= 4 << 5,	// default
	ADS1X1X_DR_16B_0250_SPS			= 5 << 5,
	ADS1X1X_DR_16B_0475_SPS			= 6 << 5,
	ADS1X1X_DR_16B_0860_SPS			= 7 << 5
} PifAds1x1xDR;

typedef enum EnPifAds1x1xCompMode
{
    ADS1X1X_COMP_MODE_TRADITIONAL	= 0 << 4, 	// default
	ADS1X1X_COMP_MODE_WINDOW		= 1 << 4
} PifAds1x1xCompMode;

typedef enum EnPifAds1x1xCompPol
{
    ADS1X1X_COMP_POL_ACTIVE_L		= 0 << 3, 	// default
	ADS1X1X_COMP_POL_ACTIVE_H		= 1 << 3
} PifAds1x1xCompPol;

typedef enum EnPifAds1x1xCompLat
{
    ADS1X1X_COMP_LAT_DISABLE		= 0 << 2, 	// default
	ADS1X1X_COMP_LAT_ENABLE			= 1 << 2
} PifAds1x1xCompLat;

typedef enum EnPifAds1x1xCompQue
{
    ADS1X1X_COMP_QUE_ONE			= 0,
	ADS1X1X_COMP_QUE_TWO			= 1,
	ADS1X1X_COMP_QUE_FOUR			= 2,
	ADS1X1X_COMP_QUE_DISABLE		= 3
} PifAds1x1xCompQue;

#define ADS1X1X_COMP_QUE_MASK		0x0003	// Comparator queue and disable (ADS1114 and ADS1115 only)
#define ADS1X1X_COMP_LAT_MASK		0x0004	// Latching comparator (ADS1114 and ADS1115 only)
#define ADS1X1X_COMP_POL_MASK		0x0008	// Comparator polarity (ADS1114 and ADS1115 only)
#define ADS1X1X_COMP_MODE_MASK		0x0010	// Comparator mode (ADS1114 and ADS1115 only)
#define ADS1X1X_DR_MASK				0x00E0	// Data rate
#define ADS1X1X_MODE_MASK			0x0100	// Device operating mode
#define ADS1X1X_PGA_MASK			0x0E00	// Programmable gain amplifier configuration
#define ADS1X1X_MUX_MASK			0x7000	// Input multiplexer configuration (ADS1115 only)
#define ADS1X1X_OS_SSCS_MASK		0x8000 	// Operational Status(R) or single-shot conversion start(W) */


/**
 * @class StPifAds1x1x
 * @brief
 */
typedef struct StPifAds1x1x
{
	// Public Member Variable
	double convert_voltage;

	// Read-only Member Variable
	PifId _id;
	PifAds1x1xType _type;
	PifI2cDevice* _p_i2c;
    uint16_t _config;

	// Private Member Variable
    uint8_t __resolution;
    uint8_t __channels;
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
 * @param port
 * @param addr
 * @return
 */
BOOL pifAds1x1x_Init(PifAds1x1x* p_owner, PifId id, PifAds1x1xType type, PifI2cPort* p_port, uint8_t addr);

/**
 * @fn pifAds1x1x_Clear
 * @brief
 * @param p_owner
 */
void pifAds1x1x_Clear(PifAds1x1x* p_owner);

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
int16_t pifAds1x1x_ReadMux(PifAds1x1x* p_owner, PifAds1x1xMux mux);

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
double pifAds1x1x_VoltageMux(PifAds1x1x* p_owner, PifAds1x1xMux mux);

/**
 * @fn pifAds1x1x_SetConfig
 * @brief
 * @param p_owner
 * @param config
 * @return
 */
BOOL pifAds1x1x_SetConfig(PifAds1x1x* p_owner, uint16_t config);

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
BOOL pifAds1x1x_SetMux(PifAds1x1x* p_owner, PifAds1x1xMux mux);

/**
 * @fn pifAds1x1x_SetGain
 * @brief
 * @param p_owner
 * @param pga
 * @return
 */
BOOL pifAds1x1x_SetGain(PifAds1x1x* p_owner, PifAds1x1xPGA pga);

/**
 * @fn pifAds1x1x_SetMode
 * @brief
 * @param p_owner
 * @param mode
 * @return
 */
BOOL pifAds1x1x_SetMode(PifAds1x1x* p_owner, PifAds1x1xMode mode);

/**
 * @fn pifAds1x1x_SetDataRate
 * @brief
 * @param p_owner
 * @param dr
 * @return
 */
BOOL pifAds1x1x_SetDataRate(PifAds1x1x* p_owner, PifAds1x1xDR dr);

/**
 * @fn pifAds1x1x_SetCompMode
 * @brief
 * @param p_owner
 * @param comp_mode
 * @return
 */
BOOL pifAds1x1x_SetCompMode(PifAds1x1x* p_owner, PifAds1x1xCompMode comp_mode);

/**
 * @fn pifAds1x1x_SetCompPol
 * @brief
 * @param p_owner
 * @param comp_pol
 * @return
 */
BOOL pifAds1x1x_SetCompPol(PifAds1x1x* p_owner, PifAds1x1xCompPol comp_pol);

/**
 * @fn pifAds1x1x_SetCompLat
 * @brief
 * @param p_owner
 * @param comp_lat
 * @return
 */
BOOL pifAds1x1x_SetCompLat(PifAds1x1x* p_owner, PifAds1x1xCompLat compLat);

/**
 * @fn pifAds1x1x_SetCompQue
 * @brief
 * @param p_owner
 * @param comp_que
 * @return
 */
BOOL pifAds1x1x_SetCompQue(PifAds1x1x* p_owner, PifAds1x1xCompQue comp_que);

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
