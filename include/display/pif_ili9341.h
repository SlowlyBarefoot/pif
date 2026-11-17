// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file pif_ili9341.h
 * @brief Driver of the ILI9341 TFT LCD controller (240 x 320), as a PifTftLcd.
 *
 * Supports the MCU parallel and the serial interfaces of the controller in 16 or 18 bit pixel formats. Drawing
 * is done with the pifTftLcd functions on &lcd.parent.
 */
#ifndef PIF_ILI9341_H
#define PIF_ILI9341_H


#include "display/pif_tft_lcd.h"


#define ILI9341_WIDTH   		240	///< Width in pixels at TLR_0_DEGREE
#define ILI9341_HEIGHT  		320	///< Height in pixels at TLR_0_DEGREE

/** @name MADCTL bits
 *  Bits of the Memory Access Control register (36h), which set the scan direction and the color order.
 *  @{
 */
#define ILI9341_MADCTL_MY  		0x80	///< Row address order
#define ILI9341_MADCTL_MX  		0x40	///< Column address order
#define ILI9341_MADCTL_MV  		0x20	///< Row and column exchange
#define ILI9341_MADCTL_ML  		0x10	///< Vertical refresh order
#define ILI9341_MADCTL_MH  		0x04	///< Horizontal refresh order
#define ILI9341_MADCTL_BGR 		0x08	///< BGR color order
#define ILI9341_MADCTL_RGB 		0x00	///< RGB color order
/** @} */


/**
 * @brief Commands of the ILI9341, as named in its datasheet.
 */
typedef enum EnPifIli9341Cmd
{
	/// Level 1 Command
	ILI9341_CMD_NOP						= 0x00,
	ILI9341_CMD_SOFT_RESET				= 0x01,
	ILI9341_CMD_READ_DISP_ID_INFO		= 0x04,
	ILI9341_CMD_SLEEP_IN    	    	= 0x10,
	ILI9341_CMD_SLEEP_OUT       	 	= 0x11,
	ILI9341_CMD_NORMAL_MODE_ON      	= 0x13,
	ILI9341_CMD_INVERSION_OFF	       	= 0x20,
	ILI9341_CMD_INVERSION_ON    	    = 0x21,
	ILI9341_CMD_GAMMA_SET        		= 0x26,
	ILI9341_CMD_DISP_OFF 	     		= 0x28,
	ILI9341_CMD_DISP_ON     	  		= 0x29,
	ILI9341_CMD_COL_ADDR_SET    	  	= 0x2A,
	ILI9341_CMD_PAGE_ADDR_SET     		= 0x2B,
	ILI9341_CMD_MEM_WRITE    	 		= 0x2C,
	ILI9341_CMD_MEM_READ    	 		= 0x2E,
	ILI9341_CMD_MEM_ACCESS_CTRL			= 0x36,
	ILI9341_CMD_PIXEL_FORMAT_SET   		= 0x3A,

	/// Level 2 Command
	ILI9341_CMD_FRAME_RATE_CTRL_NORMAL	= 0xB1,
	ILI9341_CMD_DISP_FUNC_CTRL	     	= 0xB6,
	ILI9341_CMD_ENTRY_MODE_SET  	    = 0xB7,
	ILI9341_CMD_POWER_CTRL1   			= 0xC0,
	ILI9341_CMD_POWER_CTRL2 	  		= 0xC1,
	ILI9341_CMD_VCOM_CTRL1    			= 0xC5,
	ILI9341_CMD_VCOM_CTRL2    			= 0xC7,
	ILI9341_CMD_READ_ID_4    			= 0xD3,
	ILI9341_CMD_POS_GAMMA_CORRECT		= 0xE0,
	ILI9341_CMD_NEG_GAMMA_CORRECT		= 0xE1,

	/// Extend register command
	ILI9341_CMD_POWER_CTRL_A  	 		= 0xCB,
	ILI9341_CMD_POWER_CTRL_B   			= 0xCF,
	ILI9341_CMD_DRIVER_TIM_CTRL_A1  	= 0xE8,
	ILI9341_CMD_DRIVER_TIM_CTRL_A2		= 0xE9,
	ILI9341_CMD_DRIVER_TIM_CTRL_B   	= 0xEA,
	ILI9341_CMD_POWER_SEQ_CTRL			= 0xED,
	ILI9341_CMD_ENABLE_3G    			= 0xF2,
	ILI9341_CMD_INTERFACE_CTRL			= 0xF6,
	ILI9341_CMD_PUMP_RATIO_CTRL			= 0xF7
} PifIli9341Cmd;

/**
 * @brief Interface of the controller, as set by its IM pins.
 *
 * MCU is the 8080 parallel bus and 3_WIRE and 4_WIRE the serial bus. I and II are the two pin assignments of the
 * datasheet.
 */
typedef enum EnPifIli9341Interface
{
	ILI9341_IF_MCU_8BIT_I				= 0x00,
	ILI9341_IF_MCU_16BIT_I				= 0x01,
	ILI9341_IF_MCU_9BIT_I				= 0x02,
	ILI9341_IF_MCU_18BIT_I				= 0x03,
	ILI9341_IF_3_WIRE_9BIT_I			= 0x05,
	ILI9341_IF_4_WIRE_8BIT_I			= 0x06,
	ILI9341_IF_MCU_8BIT_II				= 0x08,
	ILI9341_IF_MCU_16BIT_II				= 0x09,
	ILI9341_IF_MCU_9BIT_II				= 0x0A,
	ILI9341_IF_MCU_18BIT_II				= 0x0B,
	ILI9341_IF_3_WIRE_9BIT_II			= 0x0D,
	ILI9341_IF_4_WIRE_8BIT_II			= 0x0E
} PifIli9341Interface;

/**
 * @brief Pixel format of the controller (COLMOD, 3Ah).
 */
typedef enum EnPifIli9341PixelFormat
{
	ILI9341_PF_16BIT					= 5,	///< 16 bits per pixel, RGB 565
	ILI9341_PF_18BIT					= 6,	///< 18 bits per pixel, RGB 666
	ILI9341_PF_16BIT_RIM				= 13,	///< 16 bits per pixel in RGB interface mode
	ILI9341_PF_18BIT_RIM				= 14	///< 18 bits per pixel in RGB interface mode
} PifIli9341PixelFormat;

struct StPifIli9341;
typedef struct StPifIli9341 PifIli9341;

/**
 * @class StPifIli9341
 * @brief ILI9341 controller driver state derived from the common TFT LCD base.
 */
struct StPifIli9341
{
	/// The parent variable must be at the beginning of this structure.
	PifTftLcd parent;

	// Public Member Variable

	// Read-only Member Variable
    uint32_t _ic_id_04;	///< Display ID read with command 04h
    uint32_t _ic_id_D3;	///< IC ID read with command D3h

	// Private Member Variable
    const uint8_t* __p_rotation;
    PifIli9341Interface __interface;
    PifIli9341PixelFormat __pixel_format;
    uint8_t __mdt;
    uint8_t __command_shift;

	// Private Function
};


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initializes the ILI9341 driver and configures interface-specific defaults.
 * @param p_owner Pointer to the ILI9341 instance to initialize.
 * @param id Unique object identifier. Use `PIF_ID_AUTO` to assign one automatically.
 * @param interface Bus/interface mode used to communicate with the controller.
 * @return `TRUE` if initialization succeeds, otherwise `FALSE`.
 */
BOOL pifIli9341_Init(PifIli9341* p_owner, PifId id, PifIli9341Interface interface);

/**
 * @brief Hands over the drawing surface of the panel, which the pifGraphics functions draw on.
 * @param p_owner Pointer to an initialized ILI9341 instance.
 * @return The PifGraphics of the panel.
 */
PIF_STATIC_INLINE PifGraphics* pifIli9341_Graphics(PifIli9341* p_owner) { return &p_owner->parent.parent; }

/**
 * @brief Attaches parallel bus callback handlers used for low-level LCD transactions.
 * @param p_owner Pointer to an initialized ILI9341 instance.
 * @param act_reset Callback that resets the LCD controller.
 * @param act_chip_select Callback that toggles the chip-select line.
 * @param act_read_cmd Callback that reads command response data.
 * @param act_write_cmd Callback that writes one command and optional parameters.
 * @param act_write_data Callback that writes data payload bytes.
 * @param act_write_repeat Callback that repeats the same data payload multiple times.
 * @return `TRUE` if all required callbacks are valid and attached, otherwise `FALSE`.
 */
BOOL pifIli9341_AttachActParallel(PifIli9341* p_owner, PifActLcdReset act_reset, PifActLcdChipSelect act_chip_select, PifActLcdReadCmd act_read_cmd,
		PifActLcdWriteCmd act_write_cmd, PifActLcdWriteData act_write_data, PifActLcdWriteRepeat act_write_repeat);

/**
 * @brief Applies controller initialization commands and optional rotation register values.
 * @param p_owner Pointer to an initialized ILI9341 instance with attached callbacks.
 * @param p_setup Pointer to a setup command stream terminated by `0`. Each entry has at most 16 parameters.
 * @param p_rotation Pointer to rotation command values for 0/90/180/270 degrees, or `NULL`.
 * @return `TRUE` if setup is applied successfully, otherwise `FALSE`: a parameter is invalid, the callbacks
 *         are not attached, an entry is too long, or an 18-bit pixel format is set on a 16-bit bus with MDT 0.
 */
BOOL pifIli9341_Setup(PifIli9341* p_owner, const uint8_t* p_setup, const uint8_t* p_rotation);

/**
 * @brief Updates panel rotation and applies matching controller address mode settings.
 * @param p_parent Pointer to the drawing surface of the panel.
 * @param rotation Target rotation value.
 * @return `TRUE` if the rotation is applied, otherwise `FALSE` (rotation out of range or no rotation table).
 */
BOOL pifIli9341_SetRotation(PifTftLcd* p_parent, PifTftLcdRotation rotation);

#ifdef __cplusplus
}
#endif


#endif  // PIF_ILI9341_H
