#ifndef PIF_MAX7456_H
#define PIF_MAX7456_H


#include "communication/pif_spi.h"


typedef enum EnPifMax7456Reg
{
	MAX7456_REG_W_VM0	= 0x00,
	MAX7456_REG_W_VM1	= 0x01,
	MAX7456_REG_W_HOS	= 0x02,
	MAX7456_REG_W_VOS	= 0x03,
	MAX7456_REG_W_DMM	= 0x04,
	MAX7456_REG_W_DMAH	= 0x05,
	MAX7456_REG_W_DMAL	= 0x06,
	MAX7456_REG_W_DMDI	= 0x07,
	MAX7456_REG_W_CMM	= 0x08,
	MAX7456_REG_W_CMAH	= 0x09,
	MAX7456_REG_W_CMAL	= 0x0A,
	MAX7456_REG_W_CMDI	= 0x0B,
	MAX7456_REG_W_OSDM	= 0x0C,
	MAX7456_REG_W_RB0	= 0x10,
	MAX7456_REG_W_RB1	= 0x11,
	MAX7456_REG_W_RB2	= 0x12,
	MAX7456_REG_W_RB3	= 0x13,
	MAX7456_REG_W_RB4	= 0x14,
	MAX7456_REG_W_RB5	= 0x15,
	MAX7456_REG_W_RB6	= 0x16,
	MAX7456_REG_W_RB7	= 0x17,
	MAX7456_REG_W_RB8	= 0x18,
	MAX7456_REG_W_RB9	= 0x19,
	MAX7456_REG_W_RB10	= 0x1A,
	MAX7456_REG_W_RB11	= 0x1B,
	MAX7456_REG_W_RB12	= 0x1C,
	MAX7456_REG_W_RB13	= 0x1D,
	MAX7456_REG_W_RB14	= 0x1E,
	MAX7456_REG_W_RB15	= 0x1F,

	MAX7456_REG_R_VM0	= 0x80,
	MAX7456_REG_R_VM1	= 0x81,
	MAX7456_REG_R_HOS	= 0x82,
	MAX7456_REG_R_VOS	= 0x83,
	MAX7456_REG_R_DMM	= 0x84,
	MAX7456_REG_R_DMAH	= 0x85,
	MAX7456_REG_R_DMAL	= 0x86,
	MAX7456_REG_R_DMDI	= 0x87,
	MAX7456_REG_R_CMM	= 0x88,
	MAX7456_REG_R_CMAH	= 0x89,
	MAX7456_REG_R_CMAL	= 0x8A,
	MAX7456_REG_R_CMDI	= 0x8B,
	MAX7456_REG_R_OSDM	= 0x8C,
	MAX7456_REG_R_RB0	= 0x90,
	MAX7456_REG_R_RB1	= 0x91,
	MAX7456_REG_R_RB2	= 0x92,
	MAX7456_REG_R_RB3	= 0x93,
	MAX7456_REG_R_RB4	= 0x94,
	MAX7456_REG_R_RB5	= 0x95,
	MAX7456_REG_R_RB6	= 0x96,
	MAX7456_REG_R_RB7	= 0x97,
	MAX7456_REG_R_RB8	= 0x98,
	MAX7456_REG_R_RB9	= 0x99,
	MAX7456_REG_R_RB10	= 0x9A,
	MAX7456_REG_R_RB11	= 0x9B,
	MAX7456_REG_R_RB12	= 0x9C,
	MAX7456_REG_R_RB13	= 0x9D,
	MAX7456_REG_R_RB14	= 0x9E,
	MAX7456_REG_R_RB15	= 0x9F,
	MAX7456_REG_R_STAT	= 0xA0,
	MAX7456_REG_R_DMDO	= 0xB0,
	MAX7456_REG_R_CMDO	= 0xC0,
	MAX7456_REG_R_OSDBL	= 0x6C
} PifMax7456Reg;


// Register : Video Mode 0 (VM0)

typedef enum EnPifMax7456VideoBufferEnable
{
	MAX7456_VIDEO_BUFFER_ENABLE				= 0,
	MAX7456_VIDEO_BUFFER_DISABLE			= 1
} PifMax7456VideoBufferEnable;

#define MAX7456_SOFTWARE_RESET_BIT(N)		((N) << 1)

typedef enum EnPifMax7456VertSyncOSD
{
	MAX7456_VERT_SYNC_OSD_IMMEDIATELY		= 0 << 2,
	MAX7456_VERT_SYNC_OSD_VSYNC				= 1 << 2
} PifMax7456VertSyncOSD;

#define MAX7456_ENABLE_DISP_OSD_IMAGE(N)	((N) << 3)

typedef enum EnPifMax7456SyncSelectMode
{
	MAX7456_SYNC_SELECT_MODE_AUTO			= 0 << 4,
	MAX7456_SYNC_SELECT_MODE_EXTERNAL		= 2 << 4,
	MAX7456_SYNC_SELECT_MODE_INTERNAL		= 3 << 4
} PifMax7456SyncSelectMode;

typedef enum EnPifMax7456VideoStandardSelect
{
	MAX7456_VIDEO_STANDARD_SELECT_NTSC		= 0 << 6,
	MAX7456_VIDEO_STANDARD_SELECT_PAL		= 1 << 6
} PifMax7456VideoStandardSelect;

#define MAX7456_VIDEO_BUFFER_ENABLE_MASK	0x01
#define MAX7456_SOFTWARE_RESET_BIT_MASK		0x02
#define MAX7456_VERT_SYNC_OSD_MASK			0x04
#define MAX7456_ENABLE_DISP_OSD_IMAGE_MASK	0x08
#define MAX7456_SYNC_SELECT_MODE_MASK		0x30
#define MAX7456_VIDEO_STANDARD_SELECT_MASK	0x40


// Register : Video Mode 1 (VM1)

typedef enum EnPifMax7456BlinkDutyCycle
{
	MAX7456_BLINK_DUTY_CYCLE_BT_BT		= 0,
	MAX7456_BLINK_DUTY_CYCLE_BT_2BT		= 1,
	MAX7456_BLINK_DUTY_CYCLE_BT_3BT		= 2,
	MAX7456_BLINK_DUTY_CYCLE_3BT_BT		= 3
} PifMax7456BlinkDutyCycle;

typedef enum EnPifMax7456BlinkTime
{
	MAX7456_BLINK_TIME_2				= 0 << 2,
	MAX7456_BLINK_TIME_4				= 1 << 2,
	MAX7456_BLINK_TIME_6				= 2 << 2,
	MAX7456_BLINK_TIME_8				= 3 << 2
} PifMax7456BlinkTime;

typedef enum EnPifMax7456BgModeBright
{
	MAX7456_BG_MODE_BRIGHT_0			= 0 << 4,
	MAX7456_BG_MODE_BRIGHT_7			= 1 << 4,
	MAX7456_BG_MODE_BRIGHT_14			= 2 << 4,
	MAX7456_BG_MODE_BRIGHT_21			= 3 << 4,
	MAX7456_BG_MODE_BRIGHT_28			= 4 << 4,
	MAX7456_BG_MODE_BRIGHT_35			= 5 << 4,
	MAX7456_BG_MODE_BRIGHT_42			= 6 << 4,
	MAX7456_BG_MODE_BRIGHT_49			= 7 << 4
} PifMax7456BgModeBright;

typedef enum EnPifMax7456BgMode
{
	MAX7456_BG_MODE_EACH				= 0 << 7,
	MAX7456_BG_MODE_GRAY				= 1 << 7
} PifMax7456BgMode;

#define MAX7456_BLINK_DUTY_CYCLE_MASK	0x03
#define MAX7456_BLINK_TIME_MASK			0x0C
#define MAX7456_BG_MODE_BRIGHT_MASK		0x70
#define MAX7456_BG_MODE_MASK			0x80


// Register : Display Memory Mode (DMM)

#define MAX7456_AUTO_INCR_MODE(N)		(N)

typedef enum EnPifMax7456VertSyncClear
{
	MAX7456_VERT_SYNC_CLEAR_IMMED		= 0 << 1,
	MAX7456_VERT_SYNC_CLEAR_VSYNC		= 1 << 1
} PifMax7456VertSyncClear;

typedef enum EnPifMax7456ClearDispMem
{
	MAX7456_CLEAR_DISP_MEM_INACTIVE		= 0 << 2,
	MAX7456_CLEAR_DISP_MEM_CLEAR		= 1 << 2
} PifMax7456ClearDispMem;

typedef enum EnPifMax7456InvertBit
{
	MAX7456_INVERT_BIT_NORMAL			= 0 << 3,
	MAX7456_INVERT_BIT_INVERT			= 1 << 3
} PifMax7456InvertBit;

#define MAX7456_BLINK_BIT(N)			((N) << 4)

typedef enum EnPifMax7456LocalBgCtrlBit
{
	MAX7456_LOCAL_BG_CTRL_BIT_VIDEO_IN	= 0 << 5,
	MAX7456_LOCAL_BG_CTRL_BIT_VM1		= 1 << 5
} PifMax7456LocalBgCtrlBit;

typedef enum EnPifMax7456OperModeSelect
{
	MAX7456_OPER_MODE_SELECT_16BIT		= 0 << 6,
	MAX7456_OPER_MODE_SELECT_8BIT		= 1 << 6
} PifMax7456OperModeSelect;

#define MAX7456_AUTO_INCR_MODE_MASK		0x01
#define MAX7456_VERT_SYNC_CLEAR_MASK	0x02
#define MAX7456_CLEAR_DISP_MEM_MASK		0x04
#define MAX7456_INVERT_BIT_MASK			0x08
#define MAX7456_BLINK_BIT_MASK			0x10
#define MAX7456_LOCAL_BG_CTRL_BIT_MASK	0x20
#define MAX7456_OPER_MODE_SELECT_MASK	0x40


// Register : Display Memory Address High (DMAH)

#define MAX7456_DISP_MEM_ADDR_BIT_8(N)	(N)

typedef enum EnPifMax7456ByteSelectBit
{
	MAX7456_BYTE_SELECT_BIT_ADDR		= 0 << 1,
	MAX7456_BYTE_SELECT_BIT_ATTR		= 1 << 1
} PifMax7456ByteSelectBit;

#define MAX7456_DISP_MEM_ADDR_BIT_8_MASK	0x01
#define MAX7456_BYTE_SELECT_BIT_MASK		0x02


// Register : Character Memory Data In (CMDI), Character Memory Data Out (CMDO)

#define MAX7456_RIGHT_MOST(N)		(N)
#define MAX7456_RIGHT_CENTER(N)		((N) << 2)
#define MAX7456_LEFT_CENTER(N)		((N) << 4)
#define MAX7456_LEFT_MOST(N)		((N) << 6)

#define MAX7456_RIGHT_MOST_MASK		0x03
#define MAX7456_RIGHT_CENTER_MASK	0x0C
#define MAX7456_LEFT_CENTER_MASK	0x30
#define MAX7456_LEFT_MOST_MASK		0xC0


// Register : OSD Insertion Mux (OSDM)

typedef enum EnPifMax7456InsertMuxSwitchTime
{
	MAX7456_INSERT_MUX_SWITCH_TIME_30NS			= 0,
	MAX7456_INSERT_MUX_SWITCH_TIME_35NS			= 1,
	MAX7456_INSERT_MUX_SWITCH_TIME_50NS			= 2,
	MAX7456_INSERT_MUX_SWITCH_TIME_75NS			= 3,
	MAX7456_INSERT_MUX_SWITCH_TIME_100NS		= 4,
	MAX7456_INSERT_MUX_SWITCH_TIME_120NS		= 5
} PifMax7456InsertMuxSwitchTime;

typedef enum EnPifMax7456RiseAndFallTime
{
	MAX7456_RISE_AND_FALL_TIME_20NS				= 0 << 3,
	MAX7456_RISE_AND_FALL_TIME_30NS				= 1 << 3,
	MAX7456_RISE_AND_FALL_TIME_35NS				= 2 << 3,
	MAX7456_RISE_AND_FALL_TIME_60NS				= 3 << 3,
	MAX7456_RISE_AND_FALL_TIME_80NS				= 4 << 3,
	MAX7456_RISE_AND_FALL_TIME_110NS			= 5 << 3
} PifMax7456RiseAndFallTime;

#define MAX7456_INSERT_MUX_SWITCH_TIME_MASK		0x0E
#define MAX7456_RISE_AND_FALL_TIME_MASK			0x38


// Register : Row N Brightness (RB0 - RB15)

typedef enum EnPifMax7456CharWhiteLevel
{
	MAX7456_CHAR_WHITE_LEVEL_120			= 0,
	MAX7456_CHAR_WHITE_LEVEL_100			= 1,
	MAX7456_CHAR_WHITE_LEVEL_90				= 2,
	MAX7456_CHAR_WHITE_LEVEL_80				= 3
} PifMax7456CharWhiteLevel;

typedef enum EnPifMax7456CharBlackLevel
{
	MAX7456_CHAR_BLACK_LEVEL_0				= 0 << 2,
	MAX7456_CHAR_BLACK_LEVEL_10				= 1 << 2,
	MAX7456_CHAR_BLACK_LEVEL_20				= 2 << 2,
	MAX7456_CHAR_BLACK_LEVEL_30				= 3 << 2
} PifMax7456CharBlackLevel;

#define MAX7456_CHAR_WHITE_LEVEL_MASK		0x03
#define MAX7456_CHAR_BLACK_LEVEL_MASK		0x0C


// Register : OSD Black Level (OSDBL)

#define MAX7456_FACTORY_PRESET(N)				(N)

typedef enum EnPifMax7456ImageBlackLevelCtrl
{
	MAX7456_IMAGE_BLACK_LEVEL_CTRL_ENABLE		= 0 << 4,
	MAX7456_IMAGE_BLACK_LEVEL_CTRL_DISABLE		= 1 << 4
} PifMax7456ImageBlackLevelCtrl;

#define MAX7456_FACTORY_PRESET_MASK				0x0F
#define MAX7456_IMAGE_BLACK_LEVEL_CTRL_MASK		0x10


// Register : Status (STAT)

#define MAX7456_PAL_SIGNAL_DETECT(N)			(N)
#define MAX7456_NTSC_SIGNAL_DETECT(N)			((N) << 1)

typedef enum EnPifMax7456LossOfSync
{
	MAX7456_LOSS_OF_SYNC_ACTIVE					= 0 << 2,
	MAX7456_LOSS_OF_SYNC_NO_SYNC				= 1 << 2
} PifMax7456LossOfSync;

typedef enum EnPifMax7456HSyncOutputLevel
{
	MAX7456_HSYNC_OUTPUT_LEVEL_ACTIVE			= 0 << 3,
	MAX7456_HSYNC_OUTPUT_LEVEL_INACTIVE			= 1 << 3
} PifMax7456HSyncOutputLevel;

typedef enum EnPifMax7456VSyncOutputLevel
{
	MAX7456_VSYNC_OUTPUT_LEVEL_ACTIVE			= 0 << 4,
	MAX7456_VSYNC_OUTPUT_LEVEL_INACTIVE			= 1 << 4
} PifMax7456VSyncOutputLevel;

typedef enum EnPifMax7456CharMemStatus
{
	MAX7456_CHAR_MEM_STATUS_AVAILABLE			= 0 << 5,
	MAX7456_CHAR_MEM_STATUS_UNAVAILABLE			= 1 << 5
} PifMax7456CharMemStatus;

#define MAX7456_RESET_MODE(N)					((N) << 6)

#define MAX7456_PAL_SIGNAL_DETECT_MASK			0x01
#define MAX7456_NTSC_SIGNAL_DETECT_MASK			0x02
#define MAX7456_LOSS_OF_SYNC_MASK				0x04
#define MAX7456_HSYNC_OUTPUT_LEVEL_MASK			0x08
#define MAX7456_VSYNC_OUTPUT_LEVEL_MASK			0x10
#define MAX7456_CHAR_MEM_STATUS_MASK			0x20
#define MAX7456_RESET_MODE_MASK					0x40


/**
 * @class StPifMax7456
 * @brief Runtime context for a MAX7456 OSD device instance.
 *
 * This structure stores device detection state, SPI binding, display mode cache,
 * and internal values used to avoid redundant register writes.
 */
typedef struct StPifMax7456
{
	// Public Member Variable
	BOOL detected;

	// Read-only Member Variable
	PifId _id;
	PifSpiDevice* _p_spi;
	BOOL _font_is_loading;
	uint8_t _display_memory_mode;

	// Read-only Function

	// Private Member Variable
	uint8_t __previous_invert;
	uint8_t __previous_brightness;

	// Private Event Function
} PifMax7456;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifMax7456_Init
 * @brief Initializes a MAX7456 instance and registers it on an SPI port.
 * @param p_owner Pointer to the instance storage to initialize.
 * @param id Requested object ID. If `PIF_ID_AUTO`, an ID is assigned automatically.
 * @param p_port Pointer to the SPI port used by the MAX7456 device.
 * @param p_client Opaque SPI client handle passed through to the SPI layer.
 * @return `TRUE` on success, or `FALSE` if parameters are invalid or device registration fails.
 */
BOOL pifMax7456_Init(PifMax7456* p_owner, PifId id, PifSpiPort* p_port, void *p_client);

/**
 * @fn pifMax7456_Clear
 * @brief Releases SPI resources associated with a MAX7456 instance.
 * @param p_owner Pointer to an initialized MAX7456 instance.
 */
void pifMax7456_Clear(PifMax7456* p_owner);

/**
 * @fn pifMax7456_WriteNvm
 * @brief Writes one character font pattern from shadow RAM to character NVM.
 * @param p_owner Pointer to an initialized and detected MAX7456 instance.
 * @param char_address Character index in MAX7456 character memory.
 * @param font_data Pointer to 54-byte character bitmap data.
 * @return `TRUE` when the write sequence completes, otherwise `FALSE`.
 */
BOOL pifMax7456_WriteNvm(PifMax7456* p_owner, uint8_t char_address, const uint8_t *font_data);

/**
 * @fn pifMax7456_Invert
 * @brief Enables or disables black/white inversion in display memory mode.
 * @param p_owner Pointer to an initialized MAX7456 instance.
 * @param invert `TRUE` to invert pixels, `FALSE` to keep normal polarity.
 * @return `TRUE` if a register update was applied, otherwise `FALSE`.
 */
BOOL pifMax7456_Invert(PifMax7456* p_owner, BOOL invert);

/**
 * @fn pifMax7456_InitBrightness
 * @brief Resets the cached brightness state.
 * @param p_owner Pointer to an initialized MAX7456 instance.
 */
void pifMax7456_InitBrightness(PifMax7456* p_owner);

/**
 * @fn pifMax7456_Brightness
 * @brief Updates row brightness registers for black and white character levels.
 * @param p_owner Pointer to an initialized MAX7456 instance.
 * @param black Black level value in range 0 to 3.
 * @param white White level value in range 0 to 3.
 */
void pifMax7456_Brightness(PifMax7456* p_owner, uint8_t black, uint8_t white);

#ifdef __cplusplus
}
#endif


#endif  // PIF_MAX7456_H
