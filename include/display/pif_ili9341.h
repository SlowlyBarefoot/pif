#ifndef PIF_ILI9341_H
#define PIF_ILI9341_H


#include "display/pif_tft_lcd.h"


#define ILI9341_WIDTH   		240
#define ILI9341_HEIGHT  		320

#define ILI9341_MADCTL_MY  		0x80
#define ILI9341_MADCTL_MX  		0x40
#define ILI9341_MADCTL_MV  		0x20
#define ILI9341_MADCTL_ML  		0x10
#define ILI9341_MADCTL_MH  		0x04
#define ILI9341_MADCTL_BGR 		0x08
#define ILI9341_MADCTL_RGB 		0x00


typedef enum EnPifIli9341Cmd
{
	// Level 1 Command
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

	// Level 2 Command
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

	// Extend register command
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

typedef enum EnPifIli9341PixelFormat
{
	ILI9341_PF_16BIT					= 5,
	ILI9341_PF_18BIT					= 6,
	ILI9341_PF_16BIT_RIM				= 13,
	ILI9341_PF_18BIT_RIM				= 14
} PifIli9341PixelFormat;

struct StPifIli9341;
typedef struct StPifIli9341 PifIli9341;

typedef uint8_t (*PifConvertColor)(PifIli9341* p_owner, PifColor color, uint32_t* p_data);

/**
 * @class StPifIli9341
 * @brief
 */
struct StPifIli9341
{
	// The parent variable must be at the beginning of this structure.
	PifTftLcd parent;

	// Public Member Variable

	// Read-only Member Variable
    uint16_t _view_size;
    uint32_t _ic_id_04;
    uint32_t _ic_id_D3;

	// Private Member Variable
    const uint8_t* __p_rotation;
    PifIli9341Interface __interface;
    PifIli9341PixelFormat __pixel_format;
    uint8_t __mdt;
    uint8_t __command_shift;

	// Private Function
    PifConvertColor __fn_convert_color;
};


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifIli9341_Init
 * @brief
 * @param p_owner
 * @param id
 * @param interface
 * @return
 */
BOOL pifIli9341_Init(PifIli9341* p_owner, PifId id, PifIli9341Interface interface);

/**
 * @fn pifIli9341_AttachActParallel
 * @brief
 * @param p_owner
 * @param act_reset
 * @param act_chip_select
 * @param act_read_cmd
 * @param act_write_cmd
 * @param act_write_data
 * @param act_write_repeat
 * @return
 */
BOOL pifIli9341_AttachActParallel(PifIli9341* p_owner, PifActLcdReset act_reset, PifActLcdChipSelect act_chip_select, PifActLcdReadCmd act_read_cmd,
		PifActLcdWriteCmd act_write_cmd, PifActLcdWriteData act_write_data, PifActLcdWriteRepeat act_write_repeat);

/**
 * @fn pifIli9341_Setup
 * @brief
 * @param p_owner
 * @param p_setup
 * @param p_rotation
 * @return
 */
BOOL pifIli9341_Setup(PifIli9341* p_owner, const uint8_t* p_setup, const uint8_t* p_rotation);

/**
 * @fn pifIli9341_SetRotation
 * @brief
 * @param p_parent
 * @param rotation
 * @return
 */
BOOL pifIli9341_SetRotation(PifTftLcd* p_parent, PifTftLcdRotation rotation);

/**
 * @fn pifIli9341_DrawPixel
 * @brief
 * @param p_parent
 * @param x
 * @param y
 * @param color
 */
void pifIli9341_DrawPixel(PifTftLcd* p_parent, uint16_t x, uint16_t y, PifColor color);

/**
 * @fn pifIli9341_DrawHorLine
 * @brief
 * @param p_parent
 * @param x
 * @param y
 * @param len
 * @param color
 */
void pifIli9341_DrawHorLine(PifTftLcd* p_parent, uint16_t x, uint16_t y, uint16_t len, PifColor color);

/**
 * @fn pifIli9341_DrawVerLine
 * @brief
 * @param p_parent
 * @param x
 * @param y
 * @param len
 * @param color
 */
void pifIli9341_DrawVerLine(PifTftLcd* p_parent, uint16_t x, uint16_t y, uint16_t len, PifColor color);

/**
 * @fn pifIli9341_DrawFillRect
 * @brief
 * @param p_parent
 * @param x1
 * @param y1
 * @param x2
 * @param y2
 * @param color
 */
void pifIli9341_DrawFillRect(PifTftLcd* p_parent, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, PifColor color);

/**
 * @fn pifIli9341_DrawArea
 * @brief
 * @param p_parent
 * @param x1
 * @param y1
 * @param x2
 * @param y2
 * @param p_color_map
 */
void pifIli9341_DrawArea(PifTftLcd *p_parent, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, PifColor *p_color_map);

#ifdef __cplusplus
}
#endif


#endif  // PIF_ILI9341_H
