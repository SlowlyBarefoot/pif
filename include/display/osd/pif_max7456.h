// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file pif_max7456.h
 * @brief Driver of the MAX7456 on-screen display (OSD) chip, and of the compatibles, on SPI.
 *
 * The chip keeps a picture of characters, 30 columns by 16 rows on PAL and by 13 on NTSC, and lays it over the
 * video signal on its input. The driver puts characters on that picture, chooses the video standard, turns the
 * picture on and off, sets the brightness and the inversion of the characters, and stores a character pattern in
 * the font the chip holds in its character memory.
 *
 * The register map, the bit fields and the sequences the functions follow are those of the MAX7456 datasheet.
 */
#ifndef PIF_MAX7456_H
#define PIF_MAX7456_H


#include "communication/pif_spi.h"


/**
 * @brief Register addresses. W_ is the write address and R_ the read address of a register.
 */
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
	MAX7456_REG_W_OSDBL	= 0x6C,

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
	MAX7456_REG_R_OSDBL	= 0xEC
} PifMax7456Reg;

#define MAX7456_CHAR_DATA_SIZE	54		///< Bytes of one character pattern in character memory.

#define MAX7456_COLUMNS			30		///< Characters of a row on the picture
#define MAX7456_PAL_ROWS		16		///< Rows of characters on a PAL picture
#define MAX7456_NTSC_ROWS		13		///< Rows on an NTSC one
#define MAX7456_ESCAPE			0xFF	///< Written to DMDI, it ends an auto-increment run: no character of its own

/**
 * @brief Video signal the chip sees on its input (STAT).
 */
typedef enum EnPifMax7456Video
{
	MAX7456_VIDEO_NONE	= 0,	///< Nothing the chip locked on to
	MAX7456_VIDEO_PAL	= 1,
	MAX7456_VIDEO_NTSC	= 2
} PifMax7456Video;


// Register : Video Mode 0 (VM0)
/** @name VM0 register
 *  Video Mode 0: video standard, sync source, OSD enable and reset.
 *  @{
 */

/**
 * @brief VM0 bit 0: video buffer.
 */
typedef enum EnPifMax7456VideoBufferEnable
{
	MAX7456_VIDEO_BUFFER_ENABLE				= 0,
	MAX7456_VIDEO_BUFFER_DISABLE			= 1
} PifMax7456VideoBufferEnable;

#define MAX7456_SOFTWARE_RESET_BIT(N)		((N) << 1)	///< Puts N into the software reset bit field

/**
 * @brief VM0 bit 2: when an OSD enable takes effect.
 */
typedef enum EnPifMax7456VertSyncOSD
{
	MAX7456_VERT_SYNC_OSD_IMMEDIATELY		= 0 << 2,
	MAX7456_VERT_SYNC_OSD_VSYNC				= 1 << 2
} PifMax7456VertSyncOSD;

#define MAX7456_ENABLE_DISP_OSD_IMAGE(N)	((N) << 3)	///< Puts N into the enable disp osd image field

/**
 * @brief VM0 bits 4-5: sync source.
 */
typedef enum EnPifMax7456SyncSelectMode
{
	MAX7456_SYNC_SELECT_MODE_AUTO			= 0 << 4,
	MAX7456_SYNC_SELECT_MODE_EXTERNAL		= 2 << 4,
	MAX7456_SYNC_SELECT_MODE_INTERNAL		= 3 << 4
} PifMax7456SyncSelectMode;

/**
 * @brief VM0 bit 6: video standard.
 */
typedef enum EnPifMax7456VideoStandardSelect
{
	MAX7456_VIDEO_STANDARD_SELECT_NTSC		= 0 << 6,
	MAX7456_VIDEO_STANDARD_SELECT_PAL		= 1 << 6
} PifMax7456VideoStandardSelect;

#define MAX7456_VIDEO_BUFFER_ENABLE_MASK	0x01	///< Mask of the video buffer enable field
#define MAX7456_SOFTWARE_RESET_BIT_MASK		0x02	///< Mask of the software reset bit field
#define MAX7456_VERT_SYNC_OSD_MASK			0x04	///< Mask of the vert sync osd field
#define MAX7456_ENABLE_DISP_OSD_IMAGE_MASK	0x08	///< Mask of the enable disp osd image field
#define MAX7456_SYNC_SELECT_MODE_MASK		0x30	///< Mask of the sync select mode field
#define MAX7456_VIDEO_STANDARD_SELECT_MASK	0x40	///< Mask of the video standard select field

/** @} */

// Register : Video Mode 1 (VM1)
/** @name VM1 register
 *  Video Mode 1: background mode and brightness, blink time and duty cycle.
 *  @{
 */

/**
 * @brief VM1 bits 0-1: blink duty cycle, in units of the blink time BT.
 */
typedef enum EnPifMax7456BlinkDutyCycle
{
	MAX7456_BLINK_DUTY_CYCLE_BT_BT		= 0,
	MAX7456_BLINK_DUTY_CYCLE_BT_2BT		= 1,
	MAX7456_BLINK_DUTY_CYCLE_BT_3BT		= 2,
	MAX7456_BLINK_DUTY_CYCLE_3BT_BT		= 3
} PifMax7456BlinkDutyCycle;

/**
 * @brief VM1 bits 2-3: blink time in fields.
 */
typedef enum EnPifMax7456BlinkTime
{
	MAX7456_BLINK_TIME_2				= 0 << 2,
	MAX7456_BLINK_TIME_4				= 1 << 2,
	MAX7456_BLINK_TIME_6				= 2 << 2,
	MAX7456_BLINK_TIME_8				= 3 << 2
} PifMax7456BlinkTime;

/**
 * @brief VM1 bits 4-6: brightness of the background, in percent of OSD white.
 */
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

/**
 * @brief VM1 bit 7: background mode.
 */
typedef enum EnPifMax7456BgMode
{
	MAX7456_BG_MODE_EACH				= 0 << 7,
	MAX7456_BG_MODE_GRAY				= 1 << 7
} PifMax7456BgMode;

#define MAX7456_BLINK_DUTY_CYCLE_MASK	0x03	///< Mask of the blink duty cycle field
#define MAX7456_BLINK_TIME_MASK			0x0C	///< Mask of the blink time field
#define MAX7456_BG_MODE_BRIGHT_MASK		0x70	///< Mask of the bg mode bright field
#define MAX7456_BG_MODE_MASK			0x80	///< Mask of the bg mode field

/** @} */

// Register : Display Memory Mode (DMM)
/** @name DMM register
 *  Display Memory Mode: operation mode, attributes, clear and auto increment.
 *  @{
 */

#define MAX7456_AUTO_INCR_MODE(N)		(N)	///< Puts N into the auto incr mode field

/**
 * @brief DMM bit 1: when a clear of the display memory starts.
 */
typedef enum EnPifMax7456VertSyncClear
{
	MAX7456_VERT_SYNC_CLEAR_IMMED		= 0 << 1,
	MAX7456_VERT_SYNC_CLEAR_VSYNC		= 1 << 1
} PifMax7456VertSyncClear;

/**
 * @brief DMM bit 2: clear of the display memory.
 */
typedef enum EnPifMax7456ClearDispMem
{
	MAX7456_CLEAR_DISP_MEM_INACTIVE		= 0 << 2,
	MAX7456_CLEAR_DISP_MEM_CLEAR		= 1 << 2
} PifMax7456ClearDispMem;

/**
 * @brief DMM bit 3: inverted characters.
 */
typedef enum EnPifMax7456InvertBit
{
	MAX7456_INVERT_BIT_NORMAL			= 0 << 3,
	MAX7456_INVERT_BIT_INVERT			= 1 << 3
} PifMax7456InvertBit;

#define MAX7456_BLINK_BIT(N)			((N) << 4)	///< Puts N into the blink bit field

/**
 * @brief DMM bit 5: background of the characters.
 */
typedef enum EnPifMax7456LocalBgCtrlBit
{
	MAX7456_LOCAL_BG_CTRL_BIT_VIDEO_IN	= 0 << 5,
	MAX7456_LOCAL_BG_CTRL_BIT_VM1		= 1 << 5
} PifMax7456LocalBgCtrlBit;

/**
 * @brief DMM bit 6: 16 or 8 bit operation of the display memory.
 */
typedef enum EnPifMax7456OperModeSelect
{
	MAX7456_OPER_MODE_SELECT_16BIT		= 0 << 6,
	MAX7456_OPER_MODE_SELECT_8BIT		= 1 << 6
} PifMax7456OperModeSelect;

#define MAX7456_AUTO_INCR_MODE_MASK		0x01	///< Mask of the auto incr mode field
#define MAX7456_VERT_SYNC_CLEAR_MASK	0x02	///< Mask of the vert sync clear field
#define MAX7456_CLEAR_DISP_MEM_MASK		0x04	///< Mask of the clear disp mem field
#define MAX7456_INVERT_BIT_MASK			0x08	///< Mask of the invert bit field
#define MAX7456_BLINK_BIT_MASK			0x10	///< Mask of the blink bit field
#define MAX7456_LOCAL_BG_CTRL_BIT_MASK	0x20	///< Mask of the local bg ctrl bit field
#define MAX7456_OPER_MODE_SELECT_MASK	0x40	///< Mask of the oper mode select field

/** @} */

// Register : Display Memory Address High (DMAH)
/** @name DMAH register
 *  Display Memory Address High: address bit 8 and byte select.
 *  @{
 */

#define MAX7456_DISP_MEM_ADDR_BIT_8(N)	(N)	///< Puts N into the disp mem addr bit 8 field

/**
 * @brief DMAH bit 1: whether the display memory address selects the character or its attribute.
 */
typedef enum EnPifMax7456ByteSelectBit
{
	MAX7456_BYTE_SELECT_BIT_ADDR		= 0 << 1,
	MAX7456_BYTE_SELECT_BIT_ATTR		= 1 << 1
} PifMax7456ByteSelectBit;

#define MAX7456_DISP_MEM_ADDR_BIT_8_MASK	0x01	///< Mask of the disp mem addr bit 8 field
#define MAX7456_BYTE_SELECT_BIT_MASK		0x02	///< Mask of the byte select bit field

/** @} */

// Register : Character Memory Mode (CMM)
/** @name CMM register
 *  Character Memory Mode: what to do between the character NVM and the shadow RAM of one character.
 *  @{
 */

#define MAX7456_CMM_WRITE_NVM		0xA0	///< Stores the shadow RAM in the character NVM, in about 12 ms
#define MAX7456_CMM_READ_NVM		0x50	///< Loads the shadow RAM from the character NVM

/** @} */

// Register : Character Memory Data In (CMDI), Character Memory Data Out (CMDO)
/** @name CMDI and CMDO registers
 *  Character Memory Data In and Out: the 4 pixels of a byte, 2 bits each.
 *  @{
 */

#define MAX7456_RIGHT_MOST(N)		(N)	///< Puts N into the right most field
#define MAX7456_RIGHT_CENTER(N)		((N) << 2)	///< Puts N into the right center field
#define MAX7456_LEFT_CENTER(N)		((N) << 4)	///< Puts N into the left center field
#define MAX7456_LEFT_MOST(N)		((N) << 6)	///< Puts N into the left most field

#define MAX7456_RIGHT_MOST_MASK		0x03	///< Mask of the right most field
#define MAX7456_RIGHT_CENTER_MASK	0x0C	///< Mask of the right center field
#define MAX7456_LEFT_CENTER_MASK	0x30	///< Mask of the left center field
#define MAX7456_LEFT_MOST_MASK		0xC0	///< Mask of the left most field

/** @} */

// Register : OSD Insertion Mux (OSDM)
/** @name OSDM register
 *  OSD Insertion Mux: rise and fall time and mux switching time.
 *  @{
 */

/**
 * @brief OSDM bits 0-2: switching time of the OSD insertion mux.
 */
typedef enum EnPifMax7456InsertMuxSwitchTime
{
	MAX7456_INSERT_MUX_SWITCH_TIME_30NS			= 0,
	MAX7456_INSERT_MUX_SWITCH_TIME_35NS			= 1,
	MAX7456_INSERT_MUX_SWITCH_TIME_50NS			= 2,
	MAX7456_INSERT_MUX_SWITCH_TIME_75NS			= 3,
	MAX7456_INSERT_MUX_SWITCH_TIME_100NS		= 4,
	MAX7456_INSERT_MUX_SWITCH_TIME_120NS		= 5
} PifMax7456InsertMuxSwitchTime;

/**
 * @brief OSDM bits 3-5: rise and fall time of the OSD pixels.
 */
typedef enum EnPifMax7456RiseAndFallTime
{
	MAX7456_RISE_AND_FALL_TIME_20NS				= 0 << 3,
	MAX7456_RISE_AND_FALL_TIME_30NS				= 1 << 3,
	MAX7456_RISE_AND_FALL_TIME_35NS				= 2 << 3,
	MAX7456_RISE_AND_FALL_TIME_60NS				= 3 << 3,
	MAX7456_RISE_AND_FALL_TIME_80NS				= 4 << 3,
	MAX7456_RISE_AND_FALL_TIME_110NS			= 5 << 3
} PifMax7456RiseAndFallTime;

#define MAX7456_INSERT_MUX_SWITCH_TIME_MASK		0x07	///< Mask of the insert mux switch time field
#define MAX7456_RISE_AND_FALL_TIME_MASK			0x38	///< Mask of the rise and fall time field

/** @} */

// Register : Row N Brightness (RB0 - RB15)
/** @name RB0 to RB15 registers
 *  Row Brightness: black and white level of the characters of a row.
 *  @{
 */

/**
 * @brief RBn bits 0-1: white level of the characters, in percent.
 */
typedef enum EnPifMax7456CharWhiteLevel
{
	MAX7456_CHAR_WHITE_LEVEL_120			= 0,
	MAX7456_CHAR_WHITE_LEVEL_100			= 1,
	MAX7456_CHAR_WHITE_LEVEL_90				= 2,
	MAX7456_CHAR_WHITE_LEVEL_80				= 3
} PifMax7456CharWhiteLevel;

/**
 * @brief RBn bits 2-3: black level of the characters, in percent.
 */
typedef enum EnPifMax7456CharBlackLevel
{
	MAX7456_CHAR_BLACK_LEVEL_0				= 0 << 2,
	MAX7456_CHAR_BLACK_LEVEL_10				= 1 << 2,
	MAX7456_CHAR_BLACK_LEVEL_20				= 2 << 2,
	MAX7456_CHAR_BLACK_LEVEL_30				= 3 << 2
} PifMax7456CharBlackLevel;

#define MAX7456_CHAR_WHITE_LEVEL_MASK		0x03	///< Mask of the char white level field
#define MAX7456_CHAR_BLACK_LEVEL_MASK		0x0C	///< Mask of the char black level field

/** @} */

// Register : OSD Black Level (OSDBL)
/** @name OSDBL register
 *  OSD Black Level: factory preset and automatic black level control.
 *  @{
 */

#define MAX7456_FACTORY_PRESET(N)				(N)	///< Puts N into the factory preset field

/**
 * @brief OSDBL bit 4: automatic black level control of the image.
 */
typedef enum EnPifMax7456ImageBlackLevelCtrl
{
	MAX7456_IMAGE_BLACK_LEVEL_CTRL_ENABLE		= 0 << 4,
	MAX7456_IMAGE_BLACK_LEVEL_CTRL_DISABLE		= 1 << 4
} PifMax7456ImageBlackLevelCtrl;

#define MAX7456_FACTORY_PRESET_MASK				0x0F	///< Mask of the factory preset field
#define MAX7456_IMAGE_BLACK_LEVEL_CTRL_MASK		0x10	///< Mask of the image black level ctrl field

/** @} */

// Register : Status (STAT)
/** @name STAT register
 *  Status: detected video standard, sync, character memory and reset state.
 *  @{
 */

#define MAX7456_PAL_SIGNAL_DETECT(N)			(N)	///< Puts N into the pal signal detect field
#define MAX7456_NTSC_SIGNAL_DETECT(N)			((N) << 1)	///< Puts N into the ntsc signal detect field

/**
 * @brief STAT bit 2: loss of sync.
 */
typedef enum EnPifMax7456LossOfSync
{
	MAX7456_LOSS_OF_SYNC_ACTIVE					= 0 << 2,
	MAX7456_LOSS_OF_SYNC_NO_SYNC				= 1 << 2
} PifMax7456LossOfSync;

/**
 * @brief STAT bit 3: HSYNC output level.
 */
typedef enum EnPifMax7456HSyncOutputLevel
{
	MAX7456_HSYNC_OUTPUT_LEVEL_ACTIVE			= 0 << 3,
	MAX7456_HSYNC_OUTPUT_LEVEL_INACTIVE			= 1 << 3
} PifMax7456HSyncOutputLevel;

/**
 * @brief STAT bit 4: VSYNC output level.
 */
typedef enum EnPifMax7456VSyncOutputLevel
{
	MAX7456_VSYNC_OUTPUT_LEVEL_ACTIVE			= 0 << 4,
	MAX7456_VSYNC_OUTPUT_LEVEL_INACTIVE			= 1 << 4
} PifMax7456VSyncOutputLevel;

/**
 * @brief STAT bit 5: status of the character memory.
 */
typedef enum EnPifMax7456CharMemStatus
{
	MAX7456_CHAR_MEM_STATUS_AVAILABLE			= 0 << 5,
	MAX7456_CHAR_MEM_STATUS_UNAVAILABLE			= 1 << 5
} PifMax7456CharMemStatus;

#define MAX7456_RESET_MODE(N)					((N) << 6)	///< Puts N into the reset mode field

#define MAX7456_PAL_SIGNAL_DETECT_MASK			0x01	///< Mask of the pal signal detect field
#define MAX7456_NTSC_SIGNAL_DETECT_MASK			0x02	///< Mask of the ntsc signal detect field
#define MAX7456_LOSS_OF_SYNC_MASK				0x04	///< Mask of the loss of sync field
#define MAX7456_HSYNC_OUTPUT_LEVEL_MASK			0x08	///< Mask of the hsync output level field
#define MAX7456_VSYNC_OUTPUT_LEVEL_MASK			0x10	///< Mask of the vsync output level field
#define MAX7456_CHAR_MEM_STATUS_MASK			0x20	///< Mask of the char mem status field
#define MAX7456_RESET_MODE_MASK					0x40	///< Mask of the reset mode field
/** @} */


/**
 * @class StPifMax7456
 * @brief One MAX7456 on an SPI port.
 *
 * The driver is the only writer of VM0, DMM and the row brightness registers, so it keeps what it put there and
 * writes a whole register rather than reading one back to change a field of it.
 */
typedef struct StPifMax7456
{
	// Public Member Variable
	BOOL detected;					///< Set by pifMax7456_Detect(), or by the caller that knows the chip is there.

	// Read-only Member Variable
	PifId _id;						///< Instance ID
	PifDeviceReg *_p_device;		///< SPI device of the chip

	// Private Member Variable
	uint8_t __vm0;					///< What the driver put in VM0: the standard, the sync and the picture on or off
	uint8_t __invert;				///< The invert bit of DMM, as pifMax7456_Invert() left it
	uint8_t __brightness;			///< What the row brightness registers hold, 0xFF when that is not known
} PifMax7456;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initializes a MAX7456 instance and adds it to an SPI port as a device.
 * @param p_owner Pointer to the instance storage to initialize.
 * @param id Requested object ID. If `PIF_ID_AUTO`, an ID is assigned automatically.
 * @param p_port Pointer to the SPI port the chip is on.
 * @param p_client Opaque SPI client handle passed through to the SPI layer.
 * @return `TRUE` on success, or `FALSE` with pif_error set for a bad parameter or a port that could not take
 *         another device.
 */
BOOL pifMax7456_Init(PifMax7456* p_owner, PifId id, PifSpiPort* p_port, void *p_client);

/**
 * @brief Gives the SPI device of the instance back to its port.
 * @param p_owner Pointer to an initialized MAX7456 instance.
 * @return `TRUE` if the device was given back, otherwise `FALSE` with pif_error set to `E_INVALID_STATE` while a
 *         background transfer still holds the port for it. The instance keeps the device then, so the call can be
 *         made again later.
 */
BOOL pifMax7456_Clear(PifMax7456* p_owner);

/**
 * @brief Looks for the chip by reading OSDM, which the driver never writes, so it still holds its power-on value
 *        0x1B. Sets `detected` to what it found.
 * @param p_owner Pointer to an initialized MAX7456 instance.
 * @return `TRUE` if the chip answered with that value, otherwise `FALSE`, with pif_error set when the read failed.
 */
BOOL pifMax7456_Detect(PifMax7456* p_owner);

/**
 * @brief Resets the chip with the software reset bit of VM0 and waits for it to come back, about 100 us.
 * @details Every register goes back to its power-on value, the display memory included, so the picture is blank
 *          afterwards and pifMax7456_Start() has to be called again. What the driver holds of VM0, of the invert
 *          bit and of the brightness goes back with it.
 * @param p_owner Pointer to an initialized MAX7456 instance.
 * @return `TRUE` when the chip answered with the bit clear again, otherwise `FALSE` with pif_error set to
 *         `E_TIMEOUT`, or to the error of a failed access.
 */
BOOL pifMax7456_Reset(PifMax7456* p_owner);

/**
 * @brief The video signal on the input, read from STAT.
 * @details The chip needs about a field (20 ms) after power-up or a reset to lock on to a signal.
 * @param p_owner Pointer to an initialized MAX7456 instance.
 * @return Which signal it sees, MAX7456_VIDEO_NONE when there is none or the read failed.
 */
PifMax7456Video pifMax7456_VideoSignal(PifMax7456* p_owner);

/**
 * @brief Sets the video standard of the OSD and turns the picture on (VM0).
 * @details A standard that is not the one of the signal rolls the characters, so give the one
 *          pifMax7456_VideoSignal() reports when there is a signal.
 * @param p_owner Pointer to an initialized MAX7456 instance.
 * @param standard MAX7456_VIDEO_STANDARD_SELECT_PAL or _NTSC.
 * @return `TRUE` on success, otherwise `FALSE` with pif_error set.
 */
BOOL pifMax7456_Start(PifMax7456* p_owner, PifMax7456VideoStandardSelect standard);

/**
 * @brief Shows or hides the characters, keeping the rest of VM0 as pifMax7456_Start() left it.
 * @param p_owner Pointer to an initialized MAX7456 instance.
 * @param on `TRUE` to show them.
 * @return `TRUE` on success, otherwise `FALSE` with pif_error set.
 */
BOOL pifMax7456_Enable(PifMax7456* p_owner, BOOL on);

/**
 * @brief Blanks the display memory and waits for the chip to be through with it, about 20 us.
 * @param p_owner Pointer to an initialized MAX7456 instance.
 * @return `TRUE` on success, otherwise `FALSE` with pif_error set to `E_TIMEOUT`, or to the error of a failed
 *         access.
 */
BOOL pifMax7456_ClearDisplay(PifMax7456* p_owner);

/**
 * @brief Writes one character of the display memory, in 8 bit operation.
 * @param p_owner Pointer to an initialized MAX7456 instance.
 * @param address Place on the picture: row * MAX7456_COLUMNS + column, below 480.
 * @param character Index in character memory. What it looks like is the font the chip holds.
 * @return `TRUE` on success, otherwise `FALSE` with pif_error set.
 */
BOOL pifMax7456_WriteChar(PifMax7456* p_owner, uint16_t address, uint8_t character);

/**
 * @brief Writes characters that follow one another, in one transfer (auto-increment mode).
 * @details MAX7456_ESCAPE is what ends the run on the wire, so it cannot be one of the characters; a run that
 *          holds it is written character by character instead, as is a run longer than a row or one too long for
 *          the port.
 * @param p_owner Pointer to an initialized MAX7456 instance.
 * @param address Place of the first one: row * MAX7456_COLUMNS + column.
 * @param p_characters The characters.
 * @param count How many, 1 to MAX7456_COLUMNS for one transfer. The transfer takes 2 * (count + 4) bytes.
 * @return `TRUE` on success, otherwise `FALSE` with pif_error set.
 */
BOOL pifMax7456_WriteChars(PifMax7456* p_owner, uint16_t address, const uint8_t* p_characters, uint16_t count);

/**
 * @brief Turns the inversion of the characters on or off (the invert bit of DMM), from now on.
 * @details The bit goes to the chip with this call and with every character written afterwards, so a failed call
 *          changes nothing that the next write does not put right.
 * @param p_owner Pointer to an initialized MAX7456 instance.
 * @param invert `TRUE` for white on black, `FALSE` for black on white.
 * @return `TRUE` on success, otherwise `FALSE` with pif_error set.
 */
BOOL pifMax7456_Invert(PifMax7456* p_owner, BOOL invert);

/**
 * @brief Sets the black and the white level of the characters of every row (RB0 - RB15).
 * @details The rows are written only when the levels are not the ones written last; a failed write is tried
 *          again by the next call. All rows go under one chip select, or one row at a time on a port whose
 *          max_transfer_size is below 32 bytes.
 * @param p_owner Pointer to an initialized MAX7456 instance.
 * @param black Black level, 0 to 3 for 0%, 10%, 20% and 30%.
 * @param white White level, 0 to 3 for 80%, 90%, 100% and 120%.
 * @return `TRUE` if the chip holds the levels asked for, otherwise `FALSE` with pif_error set:
 *         `E_INVALID_PARAM` for a level above 3, or the error of the failed transfer.
 */
BOOL pifMax7456_Brightness(PifMax7456* p_owner, uint8_t black, uint8_t white);

/**
 * @brief Forgets what the row brightness registers hold, so that the next pifMax7456_Brightness() writes them
 *        whatever levels it is given.
 * @param p_owner Pointer to an initialized MAX7456 instance.
 */
void pifMax7456_InitBrightness(PifMax7456* p_owner);

/**
 * @brief Stores one character pattern in the font of the chip: into the shadow RAM of the character, and from
 *        there into the character NVM.
 * @details The picture is turned off while the NVM is written, as the datasheet asks, and turned back on
 *          afterwards. The NVM takes about 12 ms, which the call waits for, at most 50 ms. A pattern that did not
 *          reach the shadow RAM in full is not stored.
 * @param p_owner Pointer to an initialized MAX7456 instance whose `detected` is set.
 * @param char_address Index of the character in character memory.
 * @param font_data `MAX7456_CHAR_DATA_SIZE` (54) bytes of pattern.
 * @return `TRUE` when the pattern was stored, otherwise `FALSE` with pif_error set: `E_INVALID_PARAM` for a NULL
 *         font_data, `E_INVALID_STATE` when the chip was not detected or a background transfer holds the port,
 *         `E_TIMEOUT` when the NVM stayed busy (the picture is left off then), or the error of a failed access.
 */
BOOL pifMax7456_WriteNvm(PifMax7456* p_owner, uint8_t char_address, const uint8_t *font_data);

#ifdef __cplusplus
}
#endif


#endif  // PIF_MAX7456_H
