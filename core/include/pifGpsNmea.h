#ifndef PIF_GPS_NMEA_H
#define PIF_GPS_NMEA_H


#include "pifComm.h"
#include "pifGps.h"


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

/**
 * @class StPifGpsNmea
 * @brief
 */
typedef struct StPifGpsNmea
{
	// Public Member Variable

	// Read-only Member Variable
    PifGps _gps;

	// Private Member Variable
	PifComm* __p_comm;
    PifGpsNmeaTx __tx;
    uint32_t __process_message_id;
    PifGpsNmeaMessageId __event_message_id;
    PifGpsNmeaTxt* __p_txt;

	// Private Member Variable
    PifEvtGpsNmeaText __evt_text;
} PifGpsNmea;


#ifdef __cplusplus
extern "C" {
#endif

PifGpsNmea* pifGpsNmea_Create(PifId usPifId);
void pifGpsNmea_Destroy(PifGpsNmea** pp_owner);

void pifGpsNmea_AttachComm(PifGpsNmea* p_owner, PifComm* pstComm);
void pifGpsNmea_AttachEvtText(PifGpsNmea* p_owner, PifEvtGpsNmeaText evt_text);

BOOL pifGpsNmea_SetProcessMessageId(PifGpsNmea* p_owner, int count, ...);
void pifGpsNmea_SetEventMessageId(PifGpsNmea* p_owner, PifGpsNmeaMessageId message_id);

BOOL pifGpsNmea_PollRequestGBQ(PifGpsNmea* p_owner, const char* p_mag_id);
BOOL pifGpsNmea_PollRequestGLQ(PifGpsNmea* p_owner, const char* p_mag_id);
BOOL pifGpsNmea_PollRequestGNQ(PifGpsNmea* p_owner, const char* p_mag_id);
BOOL pifGpsNmea_PollRequestGPQ(PifGpsNmea* p_owner, const char* p_mag_id);

#ifdef __cplusplus
}
#endif


#endif  // PIF_GPS_NMEA_H
