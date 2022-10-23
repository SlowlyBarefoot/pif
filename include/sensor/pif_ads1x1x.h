#ifndef PIF_ADS1X1X_H
#define PIF_ADS1X1X_H


#include "core/pif_i2c.h"


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

typedef enum ENPifAds1x1xMux
{
    ADS1X1X_MUX_DIFF_0_1, 			// default
	ADS1X1X_MUX_DIFF_0_3,
	ADS1X1X_MUX_DIFF_1_3,
	ADS1X1X_MUX_DIFF_2_3,
	ADS1X1X_MUX_SINGLE_0,
	ADS1X1X_MUX_SINGLE_1,
	ADS1X1X_MUX_SINGLE_2,
	ADS1X1X_MUX_SINGLE_3
} PifAds1x1xMux;

typedef enum EnPifAds1x1xPGA
{
    ADS1X1X_PGA_FSR_6_144V,
	ADS1X1X_PGA_FSR_4_096V,
	ADS1X1X_PGA_FSR_2_048V,			// default
	ADS1X1X_PGA_FSR_1_024V,
	ADS1X1X_PGA_FSR_0_512V,
	ADS1X1X_PGA_FSR_0_256V
} PifAds1x1xPGA;

typedef enum EnPifAds1x1xMode
{
    ADS1X1X_MODE_CONTINUOUS,
	ADS1X1X_MODE_SINGLE_SHOT
} PifAds1x1xMode;

typedef enum EnPifAds1x1xDR
{
    // for 12bit model
    ADS1X1X_DR_12B_0128_SPS = 0x00,
	ADS1X1X_DR_12B_0250_SPS,
	ADS1X1X_DR_12B_0490_SPS,
	ADS1X1X_DR_12B_0920_SPS,
	ADS1X1X_DR_12B_1600_SPS,		// default
	ADS1X1X_DR_12B_2400_SPS,
	ADS1X1X_DR_12B_3300_SPS,

    // for 16bit model
	ADS1X1X_DR_16B_0008_SPS = 0x00,
	ADS1X1X_DR_16B_0016_SPS,
	ADS1X1X_DR_16B_0032_SPS,
	ADS1X1X_DR_16B_0064_SPS,
	ADS1X1X_DR_16B_0128_SPS,		// default
	ADS1X1X_DR_16B_0250_SPS,
	ADS1X1X_DR_16B_0475_SPS,
	ADS1X1X_DR_16B_0860_SPS
} PifAds1x1xDR;

typedef enum EnPifAds1x1xCompMode
{
    ADS1X1X_COMP_MODE_TRADITIONAL, 	// default
	ADS1X1X_COMP_MODE_WINDOW
} PifAds1x1xCompMode;

typedef enum EnPifAds1x1xCompPol
{
    ADS1X1X_COMP_POL_ACTIVE_L, 		// default
	ADS1X1X_COMP_POL_ACTIVE_H
} PifAds1x1xCompPol;

typedef enum EnPifAds1x1xCompLat
{
    ADS1X1X_COMP_LAT_DISABLE, 		// default
	ADS1X1X_COMP_LAT_ENABLE
} PifAds1x1xCompLat;

typedef enum EnPifAds1x1xCompQue
{
    ADS1X1X_COMP_QUE_ONE,
	ADS1X1X_COMP_QUE_TWO,
	ADS1X1X_COMP_QUE_FOUR,
	ADS1X1X_COMP_QUE_DISABLE
} PifAds1x1xCompQue;

#define ADS1X1X_CONFIG_COMP_QUE		0x0002
#define ADS1X1X_CONFIG_COMP_LAT		0x0201
#define ADS1X1X_CONFIG_COMP_POL		0x0301
#define ADS1X1X_CONFIG_COMP_MODE	0x0401
#define ADS1X1X_CONFIG_DR			0x0503
#define ADS1X1X_CONFIG_MODE			0x0801
#define ADS1X1X_CONFIG_PGA			0x0903
#define ADS1X1X_CONFIG_MUX			0x0C03
#define ADS1X1X_CONFIG_OS_SSCS		0x0F01

typedef union StPifAds1x1xConfig
{
	uint16_t word;
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
	} bit;
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
	PifId _id;
	PifAds1x1xType _type;
	PifI2cDevice* _p_i2c;
    PifAds1x1xConfig _config;

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
 * @param p_config
 * @return
 */
BOOL pifAds1x1x_SetConfig(PifAds1x1x* p_owner, PifAds1x1xConfig* p_config);

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
