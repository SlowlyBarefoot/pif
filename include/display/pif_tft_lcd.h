// SPDX-License-Identifier: BSD-3-Clause
#ifndef PIF_TFT_LCD_H
#define PIF_TFT_LCD_H


#include "core/pif.h"


#define PIF_COLOR_MAKE16(r8, g8, b8) 	((((b8) & 0xF8) >> 3) + (((g8) & 0xFC) << 3) + (((r8) & 0xF8) << 8))
#define PIF_COLOR_MAKE32(r8, g8, b8) 	((b8) + ((g8) << 8) + ((uint32_t)(r8) << 16))

#define PIF_COLOR_MAKE(r8, g8, b8) 		PIF_CONCAT2(PIF_COLOR_MAKE, PIF_COLOR_DEPTH)(r8, g8, b8)

#define PIF_COLOR_BLACK   				((PifColor)PIF_COLOR_MAKE(0x00, 0x00, 0x00))
#define PIF_COLOR_BLUE    				((PifColor)PIF_COLOR_MAKE(0x00, 0x00, 0xFF))
#define PIF_COLOR_GREEN   				((PifColor)PIF_COLOR_MAKE(0x00, 0xFF, 0x00))
#define PIF_COLOR_RED     				((PifColor)PIF_COLOR_MAKE(0xFF, 0x00, 0x00))
#define PIF_COLOR_GRAY   				((PifColor)PIF_COLOR_MAKE(0x7F, 0x7F, 0x7F))
#define PIF_COLOR_CYAN    				((PifColor)PIF_COLOR_MAKE(0x00, 0xFF, 0xFF))
#define PIF_COLOR_MAGENTA 				((PifColor)PIF_COLOR_MAKE(0xFF, 0x00, 0xFF))
#define PIF_COLOR_YELLOW  				((PifColor)PIF_COLOR_MAKE(0xFF, 0xFF, 0x00))
#define PIF_COLOR_WHITE   				((PifColor)PIF_COLOR_MAKE(0xFF, 0xFF, 0xFF))

#define TFT_SETUP_DELAY_MS				0xFF


typedef uint16_t PifColor16;
typedef uint32_t PifColor32;

typedef PIF_CONCAT2(PifColor, PIF_COLOR_DEPTH) PifColor;

typedef uint32_t PifTftLcdCmd;

typedef enum EnPifTftLcdRotation
{
	TLR_0_DEGREE	= 0,
	TLR_90_DEGREE,
	TLR_180_DEGREE,
	TLR_270_DEGREE
} PifTftLcdRotation;


struct StPifTftLcd;
typedef struct StPifTftLcd PifTftLcd;

typedef void (*PifActLcdReset)(void);
typedef void (*PifActLcdChipSelect)(SWITCH sw);
typedef void (*PifActLcdReadCmd)(PifTftLcdCmd cmd, uint32_t* p_data, uint32_t size);
typedef void (*PifActLcdWriteCmd)(PifTftLcdCmd cmd, uint32_t* p_data, uint32_t size);
typedef void (*PifActLcdWriteData)(uint32_t* p_data, uint32_t size);
typedef void (*PifActLcdWriteRepeat)(uint32_t* p_data, uint8_t size, uint32_t len);
typedef void (*PifActLcdBackLight)(uint8_t level);

typedef BOOL (*PifTftLcdSetRotation)(PifTftLcd* p_parent, PifTftLcdRotation rotation);
typedef void (*PifTftLcdDrawPixel)(PifTftLcd* p_parent, uint16_t x, uint16_t y, PifColor color);
typedef void (*PifTftLcdDrawHorLine)(PifTftLcd* p_parent, uint16_t x, uint16_t y, uint16_t len, PifColor color);
typedef void (*PifTftLcdDrawVerLine)(PifTftLcd* p_parent, uint16_t x, uint16_t y, uint16_t len, PifColor color);
typedef void (*PifTftLcdDrawFillRect)(PifTftLcd* p_parent, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, PifColor color);

/**
 * @class StPifTftLcd
 * @brief Base TFT LCD driver context shared by controller-specific implementations.
 */
struct StPifTftLcd
{
	// Public Member Variable

	// Read-only Member Variable
    PifId _id;
    uint16_t _width;
    uint16_t _height;
    PifTftLcdRotation _rotation;

	// Private Member Variable

	// Read-only Function
    PifTftLcdSetRotation _fn_set_rotation;
    PifTftLcdDrawPixel _fn_draw_pixel;
    PifTftLcdDrawHorLine _fn_draw_hor_line;
    PifTftLcdDrawVerLine _fn_draw_ver_line;
    PifTftLcdDrawFillRect _fn_draw_fill_rect;

	// Public Action Function
    PifActLcdBackLight act_backlight;

	// Private Action Function
    PifActLcdReset __act_reset;
    PifActLcdChipSelect __act_chip_select;
    PifActLcdReadCmd __act_read_cmd;
    PifActLcdWriteCmd __act_write_cmd;
    PifActLcdWriteData __act_write_data;
    PifActLcdWriteRepeat __act_write_repeat;
};


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifTftLcd_Init
 * @brief Initializes the common TFT LCD context with geometry and initial rotation.
 * @param p_owner Pointer to the TFT LCD instance to initialize.
 * @param id Unique object identifier. Use `PIF_ID_AUTO` to assign one automatically.
 * @param width Native panel width in pixels.
 * @param height Native panel height in pixels.
 * @param rotation Rotation the panel is in. Only recorded, with width and height swapped for 90 and
 *        270 degrees; the controller itself is set by the setup stream or the set-rotation function.
 * @return `TRUE` if initialization succeeds, otherwise `FALSE`.
 */
BOOL pifTftLcd_Init(PifTftLcd* p_owner, PifId id, uint16_t width, uint16_t height, PifTftLcdRotation rotation);

/**
 * @fn pifTftLcd_ClipArea
 * @brief Clips an inclusive rectangle to the current panel size.
 * @param p_owner Pointer to the TFT LCD instance.
 * @param p_x1 Left X coordinate.
 * @param p_y1 Top Y coordinate.
 * @param p_x2 Right X coordinate, lowered to the last column if it lies beyond it.
 * @param p_y2 Bottom Y coordinate, lowered to the last row if it lies beyond it.
 * @return `FALSE` if nothing of the rectangle is on the panel or the corners are reversed, otherwise `TRUE`.
 */
BOOL pifTftLcd_ClipArea(PifTftLcd* p_owner, uint16_t* p_x1, uint16_t* p_y1, uint16_t* p_x2, uint16_t* p_y2);

#ifdef __cplusplus
}
#endif


#endif  // PIF_TFT_LCD_H
