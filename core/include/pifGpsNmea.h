#ifndef PIF_GPS_NMEA_H
#define PIF_GPS_NMEA_H


#include "pifComm.h"
#include "pifGps.h"


#ifndef PIF_GPS_NMEA_TX_SIZE
#define PIF_GPS_NMEA_TX_SIZE		32
#endif

#ifndef PIF_GPS_NMEA_VALUE_SIZE
#define PIF_GPS_NMEA_VALUE_SIZE		32
#endif

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


typedef uint8_t PIF_ucGpsNmeaMessageId;

typedef enum _PIF_enGpsNmeaTxState
{
	GPTS_enIdle			= 0,
	GPTS_enSending		= 1,
	GPTS_enWaitSended	= 2,
	GPTS_enWaitResponse	= 3
} PIF_enGpsNmeaTxState;

typedef struct _PIF_stGpsNmeaTx
{
    PIF_stRingBuffer *pstBuffer;
	PIF_enGpsNmeaTxState enState;
	union {
		uint8_t ucInfo[4];
		struct {
			uint8_t ucLength;
			uint8_t ucResponse;
			uint16_t usCommand;
		} stInfo;
	} ui;
	uint8_t ucPos;
} PIF_stGpsNmeaTx;

typedef struct _PIF_stGpsNmeaTxt
{
	uint8_t ucTotal;
	uint8_t ucNum;
	uint8_t ucType;
	char acText[PIF_GPS_NMEA_VALUE_SIZE];
} PIF_stGpsNmeaTxt;

typedef void (*PIF_evtGpsNmeaText)(PIF_stGpsNmeaTxt *pstTxt);

/**
 * @class _PIF_stGpsNmea
 * @brief
 */
typedef struct _PIF_stGpsNmea
{
	// Public Member Variable

	// Public Event Function

	// Read-only Member Variable
    PIF_usId _usPifId;

	// Private Member Variable
	PIF_stComm *__pstComm;
    PIF_stGpsNmeaTx __stTx;
    uint32_t __unProcessMessageId;
    PIF_ucGpsNmeaMessageId __ucEventMessageId;
    PIF_stGpsNmeaTxt *__pstTxt;

	// Private Member Variable
    PIF_evtGpsNmeaText __evtText;
} PIF_stGpsNmea;


#ifdef __cplusplus
extern "C" {
#endif

PIF_stGps *pifGpsNmea_Add(PIF_usId usPifId);

void pifGpsNmea_AttachComm(PIF_stGps *pstParent, PIF_stComm *pstComm);
void pifGpsNmea_AttachEvtText(PIF_stGps *pstParent, PIF_evtGpsNmeaText evtText);

BOOL pifGpsNmea_SetProcessMessageId(PIF_stGps *pstParent, int nCount, ...);
void pifGpsNmea_SetEventMessageId(PIF_stGps *pstParent, PIF_ucGpsNmeaMessageId ucMessageId);

BOOL pifGpsNmea_PollRequestGBQ(PIF_stGps *pstParent, const char *pcMagId);
BOOL pifGpsNmea_PollRequestGLQ(PIF_stGps *pstParent, const char *pcMagId);
BOOL pifGpsNmea_PollRequestGNQ(PIF_stGps *pstParent, const char *pcMagId);
BOOL pifGpsNmea_PollRequestGPQ(PIF_stGps *pstParent, const char *pcMagId);

#ifdef __cplusplus
}
#endif


#endif  // PIF_GPS_NMEA_H
