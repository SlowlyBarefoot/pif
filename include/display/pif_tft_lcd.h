#ifndef PIF_TFT_LCD_H
#define PIF_TFT_LCD_H


#include "core/pif.h"


#define BLACK   		0x000000
#define BLUE    		0x0000FF
#define GREEN   		0x00FF00
#define RED     		0xFF0000
#define GRAY   			0x7F7F7F
#define CYAN    		0x00FFFF
#define MAGENTA 		0xFF00FF
#define YELLOW  		0xFFFF00
#define WHITE   		0xFFFFFF

#define TFT_SETUP_DELAY_MS	0xFF


typedef uint8_t PifTftLcdCmd;

typedef enum EnPifTftLcdRotation
{
	TLR_0_DEGREE	= 0,
	TLR_90_DEGREE,
	TLR_180_DEGREE,
	TLR_270_DEGREE
} PifTftLcdRotation;


struct StPifTftLcd;
typedef struct StPifTftLcd PifTftLcd;

typedef void (*PifActLcdReset)();
typedef void (*PifActLcdChipSelect)(SWITCH sw);
typedef void (*PifActLcdReadCmd)(PifTftLcdCmd cmd, uint32_t* p_data, uint32_t size);
typedef void (*PifActLcdWriteCmd)(PifTftLcdCmd cmd, uint32_t* p_data, uint32_t size);
typedef void (*PifActLcdWriteData)(uint32_t* p_data, uint32_t size);
typedef void (*PifActLcdWriteRepeat)(uint32_t* p_data, uint8_t size, uint32_t len);
typedef void (*PifActLcdBackLight)(uint8_t level);

typedef void (*PifTftLcdSetRotation)(PifTftLcd* p_parent, PifTftLcdRotation rotation);
typedef void (*PifTftLcdDrawPixel)(PifTftLcd* p_parent, uint16_t x, uint16_t y, uint32_t color);
typedef void (*PifTftLcdDrawHorLine)(PifTftLcd* p_parent, uint16_t x, uint16_t y, uint16_t len, uint32_t color);
typedef void (*PifTftLcdDrawVerLine)(PifTftLcd* p_parent, uint16_t x, uint16_t y, uint16_t len, uint32_t color);
typedef void (*PifTftLcdDrawFillRect)(PifTftLcd* p_parent, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint32_t color);

/**
 * @class StPifTftLcd
 * @brief
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
 * @brief
 * @param p_owner
 * @param id
 * @param width
 * @param height
 * @param rotation
 * @return
 */
BOOL pifTftLcd_Init(PifTftLcd* p_owner, PifId id, uint16_t width, uint16_t height, PifTftLcdRotation rotation);

#ifdef __cplusplus
}
#endif


#endif  // PIF_TFT_LCD_H
