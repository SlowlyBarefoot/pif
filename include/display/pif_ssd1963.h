#ifndef PIF_SSD1963_H
#define PIF_SSD1963_H


#include "display/pif_tft_lcd.h"


#define SSD1963_WIDTH   		480
#define SSD1963_HEIGHT  		272


typedef enum EnPifSsd1963Cmd
{
	SSD1963_CMD_NOP					= 0x00,
	SSD1963_CMD_SOFT_RESET			= 0x01,
	SSD1963_CMD_GET_POWER_MODE		= 0x0A,
	SSD1963_CMD_GET_ADDR_MODE  		= 0x0B,
	SSD1963_CMD_GET_DISP_MODE	   	= 0x0D,
	SSD1963_CMD_GET_TEAR_EFF_STAT	= 0x0E,

	SSD1963_CMD_ENTER_SLEEP_MODE    = 0x10,
	SSD1963_CMD_EXIT_SLEEP_MODE     = 0x11,
	SSD1963_CMD_ENTER_PARTIAL_MODE 	= 0x12,
	SSD1963_CMD_ENTER_NORMAL_MODE   = 0x13,

	SSD1963_CMD_EXIT_INVERT_MODE    = 0x20,
	SSD1963_CMD_ENTER_INVERT_MODE 	= 0x21,
	SSD1963_CMD_SET_GAMMA_CURVE     = 0x26,
	SSD1963_CMD_SET_DISP_OFF	 	= 0x28,
	SSD1963_CMD_SET_DISP_ON 	    = 0x29,
	SSD1963_CMD_SET_COL_ADDR     	= 0x2A,
	SSD1963_CMD_SET_PAGE_ADDR    	= 0x2B,
	SSD1963_CMD_WRITE_MEM_START	    = 0x2C,
	SSD1963_CMD_READ_MEM_START    	= 0x2E,

	SSD1963_CMD_SET_PARTIAL_AREA    = 0x30,
	SSD1963_CMD_SET_SCROLL_AREA		= 0x33,
	SSD1963_CMD_SET_TEAR_OFF		= 0x34,
	SSD1963_CMD_SET_TEAR_ON		    = 0x35,
	SSD1963_CMD_SET_ADDR_MODE    	= 0x36,
	SSD1963_CMD_SET_SCROLL_START	= 0x37,
	SSD1963_CMD_EXIT_IDLE_MODE		= 0x38,
	SSD1963_CMD_ENTER_IDLE_MODE	    = 0x39,
	SSD1963_CMD_WRITE_MEM_CONT		= 0x3C,
	SSD1963_CMD_READ_MEM_CONT   	= 0x3E,

	SSD1963_CMD_SET_TEAR_SCANLINE	= 0x44,
	SSD1963_CMD_GET_SCANLINE	   	= 0x45,

	SSD1963_CMD_READ_DDB		   	= 0xA1,

	SSD1963_CMD_SET_LCD_MODE  		= 0xB0,
	SSD1963_CMD_GET_LCD_MODE  		= 0xB1,
	SSD1963_CMD_SET_HORI_PERIOD		= 0xB4,
	SSD1963_CMD_GET_HORI_PERIOD		= 0xB5,
	SSD1963_CMD_SET_VERT_PERIOD		= 0xB6,
	SSD1963_CMD_GET_VERT_PERIOD		= 0xB7,
	SSD1963_CMD_SET_GPIO_CONF		= 0xB8,
	SSD1963_CMD_GET_GPIO_CONF		= 0xB9,
	SSD1963_CMD_SET_GPIO_VALUE		= 0xBA,
	SSD1963_CMD_GET_GPIO_STAT		= 0xBB,
	SSD1963_CMD_SET_POST_PROC		= 0xBC,
	SSD1963_CMD_GET_POST_PROC		= 0xBD,
	SSD1963_CMD_SET_PWM_CONF		= 0xBE,
	SSD1963_CMD_GET_PWM_CONF		= 0xBF,

	SSD1963_CMD_SET_LCD_GEN0  		= 0xC0,
	SSD1963_CMD_GET_LCD_GEN0	  	= 0xC1,
	SSD1963_CMD_SET_LCD_GEN1  		= 0xC2,
	SSD1963_CMD_GET_LCD_GEN1  		= 0xC3,
	SSD1963_CMD_SET_LCD_GEN2  		= 0xC4,
	SSD1963_CMD_GET_LCD_GEN2	  	= 0xC5,
	SSD1963_CMD_SET_LCD_GEN3  		= 0xC6,
	SSD1963_CMD_GET_LCD_GEN3  		= 0xC7,
	SSD1963_CMD_SET_GPIO0_ROP		= 0xC8,
	SSD1963_CMD_GET_GPIO0_ROP		= 0xC9,
	SSD1963_CMD_SET_GPIO1_ROP		= 0xCA,
	SSD1963_CMD_GET_GPIO1_ROP		= 0xCB,
	SSD1963_CMD_SET_GPIO2_ROP		= 0xCC,
	SSD1963_CMD_GET_GPIO2_ROP		= 0xCD,
	SSD1963_CMD_SET_GPIO3_ROP		= 0xCE,
	SSD1963_CMD_GET_GPIO3_ROP		= 0xCF,

	SSD1963_CMD_SET_DBC_CONF		= 0xD0,
	SSD1963_CMD_GET_DBC_CONF		= 0xD1,
	SSD1963_CMD_SET_DBC_TH			= 0xD4,
	SSD1963_CMD_GET_DBC_TH			= 0xD5,

	SSD1963_CMD_SET_PLL				= 0xE0,
	SSD1963_CMD_SET_PLL_MN			= 0xE2,
	SSD1963_CMD_GET_PLL_MN			= 0xE3,
	SSD1963_CMD_GET_PLL_STAT		= 0xE4,
	SSD1963_CMD_SET_DEEP_SLEEP		= 0xE5,
	SSD1963_CMD_SET_LSHIFT_FREQ		= 0xE6,
	SSD1963_CMD_GET_LSHIFT_FREQ		= 0xE7,

	SSD1963_CMD_SET_PIX_DATA_IF		= 0xF0,
	SSD1963_CMD_GET_PIX_DATA_IF		= 0xF1
} PifSsd1963Cmd;

typedef enum EnPifSsd1963PixelFormat
{
	SSD1963_PF_8BIT				= 0,
	SSD1963_PF_12BIT			= 1,
	SSD1963_PF_16BIT_PACKED		= 2,
	SSD1963_PF_16BIT_565		= 3,
	SSD1963_PF_18BIT			= 4,
	SSD1963_PF_24BIT			= 5,
	SSD1963_PF_9BIT				= 6
} PifSsd1963PixelFormat;

struct StPifSsd1963;
typedef struct StPifSsd1963 PifSsd1963;

typedef uint8_t (*PifConvertColor)(PifColor color, uint32_t* p_data);

/**
 * @class StPifSsd1963
 * @brief SSD1963 controller driver state built on top of the generic TFT LCD base.
 */
struct StPifSsd1963
{
	// The parent variable must be at the beginning of this structure.
	PifTftLcd parent;

	// Public Member Variable

	// Read-only Member Variable
	uint16_t _view_size;

	// Private Member Variable
    const uint8_t* __p_rotation;
	PifSsd1963PixelFormat __pixel_format;

	// Private Function
    PifConvertColor __fn_convert_color;
};


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifSsd1963_Init
 * @brief Initializes the SSD1963 driver instance and binds drawing callbacks.
 * @param p_owner Pointer to the SSD1963 instance to initialize.
 * @param id Unique object identifier. Use `PIF_ID_AUTO` to assign one automatically.
 * @return `TRUE` if initialization succeeds, otherwise `FALSE`.
 */
BOOL pifSsd1963_Init(PifSsd1963* p_owner, PifId id);

/**
 * @fn pifSsd1963_AttachActParallel
 * @brief Attaches low-level parallel bus control callbacks required by the SSD1963 driver.
 * @param p_owner Pointer to an initialized SSD1963 instance.
 * @param act_reset Callback that resets the LCD controller hardware.
 * @param act_chip_select Callback that enables or disables the LCD chip-select line.
 * @param act_read_cmd Callback that reads command response data from the controller.
 * @param act_write_cmd Callback that writes one command and optional parameters.
 * @param act_write_data Callback that writes raw pixel/data payload bytes.
 * @param act_write_repeat Callback that repeats the same pixel/data sequence multiple times.
 * @return `TRUE` if all required callbacks are valid and attached, otherwise `FALSE`.
 */
BOOL pifSsd1963_AttachActParallel(PifSsd1963* p_owner, PifActLcdReset act_reset, PifActLcdChipSelect act_chip_select, PifActLcdReadCmd act_read_cmd,
		PifActLcdWriteCmd act_write_cmd, PifActLcdWriteData act_write_data, PifActLcdWriteRepeat act_write_repeat);

/**
 * @fn pifSsd1963_Setup
 * @brief Applies panel initialization commands and optional rotation mapping data.
 * @param p_owner Pointer to an initialized SSD1963 instance with attached hardware callbacks.
 * @param p_setup Pointer to a setup command stream terminated by `0`.
 * @param p_rotation Pointer to rotation register values for 0/90/180/270 degrees, or `NULL`.
 * @return `TRUE` if setup commands are parsed and applied successfully, otherwise `FALSE`.
 */
BOOL pifSsd1963_Setup(PifSsd1963* p_owner, const uint8_t* p_setup, const uint8_t* p_rotation);

/**
 * @fn pifSsd1963_DrawPixel
 * @brief Draws a single pixel at the specified coordinate.
 * @param p_parent Pointer to the base TFT LCD context.
 * @param x Horizontal pixel coordinate.
 * @param y Vertical pixel coordinate.
 * @param color Pixel color value in `PifColor` format.
 */
void pifSsd1963_DrawPixel(PifTftLcd* p_parent, uint16_t x, uint16_t y, PifColor color);

/**
 * @fn pifSsd1963_DrawHorLine
 * @brief Draws a horizontal line using repeated pixel writes.
 * @param p_parent Pointer to the base TFT LCD context.
 * @param x Start X coordinate.
 * @param y Fixed Y coordinate.
 * @param len Number of pixels to draw.
 * @param color Line color in `PifColor` format.
 */
void pifSsd1963_DrawHorLine(PifTftLcd* p_parent, uint16_t x, uint16_t y, uint16_t len, PifColor color);

/**
 * @fn pifSsd1963_DrawVerLine
 * @brief Draws a vertical line using repeated pixel writes.
 * @param p_parent Pointer to the base TFT LCD context.
 * @param x Fixed X coordinate.
 * @param y Start Y coordinate.
 * @param len Number of pixels to draw.
 * @param color Line color in `PifColor` format.
 */
void pifSsd1963_DrawVerLine(PifTftLcd* p_parent, uint16_t x, uint16_t y, uint16_t len, PifColor color);

/**
 * @fn pifSsd1963_DrawFillRect
 * @brief Fills a rectangular area with a single color.
 * @param p_parent Pointer to the base TFT LCD context.
 * @param x1 Left X coordinate of the rectangle.
 * @param y1 Top Y coordinate of the rectangle.
 * @param x2 Right X coordinate of the rectangle.
 * @param y2 Bottom Y coordinate of the rectangle.
 * @param color Fill color in `PifColor` format.
 */
void pifSsd1963_DrawFillRect(PifTftLcd* p_parent, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, PifColor color);

/**
 * @fn pifSsd1963_DrawArea
 * @brief Draws a rectangular bitmap area from a color buffer.
 * @param p_parent Pointer to the base TFT LCD context.
 * @param x1 Left X coordinate of the destination area.
 * @param y1 Top Y coordinate of the destination area.
 * @param x2 Right X coordinate of the destination area.
 * @param y2 Bottom Y coordinate of the destination area.
 * @param p_color_map Pointer to source pixel data in row-major order.
 */
void pifSsd1963_DrawArea(PifTftLcd *p_parent, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, PifColor *p_color_map);

#ifdef __cplusplus
}
#endif


#endif  // PIF_SSD1963_H
