#ifndef PIF_GPS_NMEA_H
#define PIF_GPS_NMEA_H


#include "pif_comm.h"
#include "pif_gps.h"


#define NMEA_MESSAGE_ID_NONE	0

#define NMEA_MESSAGE_ID_DTM		1		// Output
#define NMEA_MESSAGE_ID_GBS		2		// Output
#define NMEA_MESSAGE_ID_GGA		3		// Output
#define NMEA_MESSAGE_ID_GLL		4		// Output
#define NMEA_MESSAGE_ID_GNS		5		// Output
#define NMEA_MESSAGE_ID_GRS		6		// Output
#define NMEA_MESSAGE_ID_GSA		7		// Output
#define NMEA_MESSAGE_ID_GST		8		// Output
#define NMEA_MESSAGE_ID_GSV		9		// Output
#define NMEA_MESSAGE_ID_RMC		10		// Output
#define NMEA_MESSAGE_ID_THS		11		// Output
#define NMEA_MESSAGE_ID_TXT		12		// Output
#define NMEA_MESSAGE_ID_VLW		13		// Output
#define NMEA_MESSAGE_ID_VTG		14		// Output
#define NMEA_MESSAGE_ID_ZDA		15		// Output

#define NMEA_MESSAGE_ID_MAX		15

#define NMEA_MESSAGE_ID_GBQ		21		// Poll Request
#define NMEA_MESSAGE_ID_GLQ		22		// Poll Request
#define NMEA_MESSAGE_ID_GNQ		23		// Poll Request
#define NMEA_MESSAGE_ID_GPQ		24		// Poll Request


typedef uint8_t PifGpsNmeaMessageId;

typedef enum EnPifGpsNmeaTxState
{
	GPTS_IDLE			= 0,
	GPTS_SENDING		= 1,
	GPTS_WAIT_SENDED	= 2,
	GPTS_WAIT_RESPONSE	= 3
} PifGpsNmeaTxState;

typedef struct StPifGpsNmeaTx
{
    PifRingBuffer buffer;
	PifGpsNmeaTxState state;
	union {
		uint8_t info[4];
		struct {
			uint8_t length;
			uint8_t response;
			uint16_t command;
		} st;
	} ui;
	uint8_t pos;
} PifGpsNmeaTx;

typedef struct StPifGpsNmeaTxt
{
	uint8_t total;
	uint8_t num;
	uint8_t type;
	char text[PIF_GPS_NMEA_VALUE_SIZE];
} PifGpsNmeaTxt;

typedef void (*PifEvtGpsNmeaText)(PifGpsNmeaTxt* p_txt);
typedef void (*PifEvtGpsNmeaFrame)(char* p_frame);

/**
 * @class StPifGpsNmea
 * @brief
 */
typedef struct StPifGpsNmea
{
	// Public Member Variable

	// Public Event Variable
    PifEvtGpsNmeaText evt_text;
    PifEvtGpsNmeaFrame evt_frame;

	// Read-only Member Variable
    PifGps _gps;

	// Private Member Variable
	PifComm* __p_comm;
    PifGpsNmeaTx __tx;
    uint32_t __process_message_id;
    PifGpsNmeaMessageId __event_message_id;
    PifGpsNmeaTxt* __p_txt;
} PifGpsNmea;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifGpsNmea_Init
 * @brief
 * @param p_owner
 * @param id
 * @return
 */
BOOL pifGpsNmea_Init(PifGpsNmea* p_owner, PifId usPifId);

/**
 * @fn pifGpsNmea_Clear
 * @brief
 * @param p_owner
 */
void pifGpsNmea_Clear(PifGpsNmea* p_owner);

/**
 * @fn pifGpsNmea_AttachComm
 * @brief
 * @param p_owner
 * @param p_comm
 */
void pifGpsNmea_AttachComm(PifGpsNmea* p_owner, PifComm* pstComm);

/**
 * @fn pifGpsNmea_SetProcessMessageId
 * @brief
 * @param p_owner
 * @param count
 * @return
 */
BOOL pifGpsNmea_SetProcessMessageId(PifGpsNmea* p_owner, int count, ...);

/**
 * @fn pifGpsNmea_SetEventMessageId
 * @brief
 * @param p_owner
 * @param message_id
 */
void pifGpsNmea_SetEventMessageId(PifGpsNmea* p_owner, PifGpsNmeaMessageId message_id);

/**
 * @fn pifGpsNmea_PollRequestGBQ
 * @brief
 * @param p_owner
 * @param p_mag_id
 * @return
 */
BOOL pifGpsNmea_PollRequestGBQ(PifGpsNmea* p_owner, const char* p_mag_id);

/**
 * @fn pifGpsNmea_PollRequestGLQ
 * @brief
 * @param p_owner
 * @param p_mag_id
 * @return
 */
BOOL pifGpsNmea_PollRequestGLQ(PifGpsNmea* p_owner, const char* p_mag_id);

/**
 * @fn pifGpsNmea_PollRequestGNQ
 * @brief
 * @param p_owner
 * @param p_mag_id
 * @return
 */
BOOL pifGpsNmea_PollRequestGNQ(PifGpsNmea* p_owner, const char* p_mag_id);

/**
 * @fn pifGpsNmea_PollRequestGPQ
 * @brief
 * @param p_owner
 * @param p_mag_id
 * @return
 */
BOOL pifGpsNmea_PollRequestGPQ(PifGpsNmea* p_owner, const char* p_mag_id);

#ifdef __cplusplus
}
#endif


#endif  // PIF_GPS_NMEA_H
