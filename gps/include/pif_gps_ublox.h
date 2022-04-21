#ifndef PIF_GPS_UBLOXX_H
#define PIF_GPS_UBLOXX_H


#include "pif_comm.h"
#include "pif_gps.h"
#include "pif_ring_buffer.h"
#include "pif_timer.h"


#ifndef PIF_GPS_UBLOX_TX_SIZE
#define PIF_GPS_UBLOX_TX_SIZE				128
#endif

//#define __DEBUG_PACKET__


typedef enum EnPifGpsUbxClassId
{
	GUCI_ACK		= 0x05,
	GUCI_AID		= 0x0B,
	GUCI_CFG		= 0x06,
	GUCI_ESF		= 0x10,
	GUCI_HNR		= 0x28,
	GUCI_INF		= 0x04,
	GUCI_LOG		= 0x21,
	GUCI_MGA		= 0x13,
	GUCI_MON		= 0x0A,
	GUCI_NAV		= 0x01,
	GUCI_RXM		= 0x02,
	GUCI_SEC		= 0x27,
	GUCI_TIM		= 0x0F,
	GUCI_UPD		= 0x09,

	GUCI_NMEA_PUBX	= 0xF1,
	GUCI_NMEA_STD	= 0xF0,
} PifGpsUbxClassId;

typedef enum EnPifGpsUbxMessageId
{
	GUMI_ACK_ACK			= 0x01,
	GUMI_ACK_NAK			= 0x00,

	GUMI_AID_ALM			= 0x30,
	GUMI_AID_ACP			= 0x33,
	GUMI_AID_EPH			= 0x31,
	GUMI_AID_HUI			= 0x02,
	GUMI_AID_INI			= 0x01,

	GUMI_CFG_ANT			= 0x13,
	GUMI_CFG_BATCH			= 0x93,
	GUMI_CFG_CFG			= 0x09,
	GUMI_CFG_DAT			= 0x06,
	GUMI_CFG_DGNSS			= 0x70,
	GUMI_CFG_DISC			= 0x61,
	GUMI_CFG_ESFALG			= 0x56,
	GUMI_CFG_ESFA			= 0x4C,
	GUMI_CFG_ESFG			= 0x4D,
	GUMI_CFG_ESFWT			= 0x82,
	GUMI_CFG_ESRC			= 0x60,
	GUMI_CFG_GEOFENCE		= 0x69,
	GUMI_CFG_GNSS			= 0x3E,
	GUMI_CFG_HNR			= 0x5C,
	GUMI_CFG_INF			= 0x02,
	GUMI_CFG_ITFM			= 0x39,
	GUMI_CFG_LOGFILTER		= 0x47,
	GUMI_CFG_MSG			= 0x01,
	GUMI_CFG_NAV5			= 0x24,
	GUMI_CFG_NAVX5			= 0x23,
	GUMI_CFG_NMEA			= 0x17,
	GUMI_CFG_ODO			= 0x1E,
	GUMI_CFG_PM2			= 0x3B,
	GUMI_CFG_PMS			= 0x86,
	GUMI_CFG_PRT			= 0x00,
	GUMI_CFG_PWR			= 0x57,
	GUMI_CFG_RATE			= 0x08,
	GUMI_CFG_RINV			= 0x34,
	GUMI_CFG_RST			= 0x04,
	GUMI_CFG_RXM			= 0x11,
	GUMI_CFG_SBAS			= 0x16,
	GUMI_CFG_SENIF			= 0x88,
	GUMI_CFG_SLAS			= 0x8D,
	GUMI_CFG_SMGR			= 0x62,
	GUMI_CFG_SPT			= 0x64,
	GUMI_CFG_TMODE2			= 0x3D,
	GUMI_CFG_TMODE3			= 0x71,
	GUMI_CFG_TP5			= 0x31,
	GUMI_CFG_TXSLOT			= 0x53,
	GUMI_CFG_USB			= 0x1B,

	GUMI_ESF_ALG			= 0x14,
	GUMI_ESF_INS			= 0x15,
	GUMI_ESF_MEAS			= 0x02,
	GUMI_ESF_RAW			= 0x03,
	GUMI_ESF_STATUS			= 0x10,

	GUMI_HNR_ATT			= 0x01,
	GUMI_HNR_INS			= 0x02,
	GUMI_HNR_PVT			= 0x00,

	GUMI_INF_DEBUG			= 0x04,
	GUMI_INF_ERROR			= 0x00,
	GUMI_INF_NOTICE			= 0x02,
	GUMI_INF_TEST			= 0x03,
	GUMI_INF_WARNING		= 0x01,

	GUMI_LOG_BATCH			= 0x11,
	GUMI_LOG_CREATE			= 0x07,
	GUMI_LOG_ERASE			= 0x03,
	GUMI_LOG_FINDTIME		= 0x0E,
	GUMI_LOG_INFO			= 0x08,
	GUMI_LOG_RETRIEVEBA_	= 0x10,
	GUMI_LOG_RETRIEVEPO_	= 0x0F,
	GUMI_LOG_RETRIEVEPOS	= 0x0B,
	GUMI_LOG_RETRIEVEST_	= 0x0D,
	GUMI_LOG_RETRIEVE		= 0x09,
	GUMI_LOG_STRING			= 0x04,

	GUMI_MGA_ACK_DATAO		= 0x60,
	GUMI_MGA_ANO			= 0x76,
	GUMI_MGA_BDS			= 0x03,
	GUMI_MGA_DBD			= 0x80,
	GUMI_MGA_FLASH			= 0x21,
	GUMI_MGA_GAL			= 0x02,
	GUMI_MGA_GLO			= 0x06,
	GUMI_MGA_GPS			= 0x00,
	GUMI_MGA_INI			= 0x40,
	GUMI_MGA_QZSS			= 0x05,

	GUMI_MON_BATCH			= 0x32,
	GUMI_MON_GNSS			= 0x28,
	GUMI_MON_HW2			= 0x0B,
	GUMI_MON_HW				= 0x09,
	GUMI_MON_IO				= 0x02,
	GUMI_MON_MSGPP			= 0x06,
	GUMI_MON_PATCH			= 0x27,
	GUMI_MON_RXBUF			= 0x07,
	GUMI_MON_RXR			= 0x21,
	GUMI_MON_SMGR			= 0x2E,
	GUMI_MON_SPT			= 0x2F,
	GUMI_MON_TXBUF			= 0x08,
	GUMI_MON_VER			= 0x04,

	GUMI_NAV_AOPSTATUS		= 0x60,
	GUMI_NAV_ATT			= 0x05,
	GUMI_NAV_CLOCK			= 0x22,
	GUMI_NAV_COV			= 0x36,
	GUMI_NAV_DGPS			= 0x31,
	GUMI_NAV_DOP			= 0x04,
	GUMI_NAV_EELL			= 0x3D,
	GUMI_NAV_EOE			= 0x61,
	GUMI_NAV_GEOFENCE		= 0x39,
	GUMI_NAV_HPPOSECEF		= 0x13,
	GUMI_NAV_HPPOSLLH		= 0x14,
	GUMI_NAV_NMI			= 0x28,
	GUMI_NAV_ODO			= 0x09,
	GUMI_NAV_ORB			= 0x34,
	GUMI_NAV_POSECEF		= 0x01,
	GUMI_NAV_POSLLH			= 0x02,
	GUMI_NAV_PVT			= 0x07,
	GUMI_NAV_RELPOSNED		= 0x3C,
	GUMI_NAV_RESETODO		= 0x10,
	GUMI_NAV_SAT			= 0x35,
	GUMI_NAV_SBAS			= 0x32,
	GUMI_NAV_SLAS			= 0x42,
	GUMI_NAV_SOL			= 0x06,
	GUMI_NAV_STATUS			= 0x03,
	GUMI_NAV_SVINFO			= 0x30,
	GUMI_NAV_SVIN			= 0x3B,
	GUMI_NAV_TIMEBDS		= 0x24,
	GUMI_NAV_TIMEGAL		= 0x25,
	GUMI_NAV_TIMEGLO		= 0x23,
	GUMI_NAV_TIMEGPS		= 0x20,
	GUMI_NAV_TIMELS			= 0x26,
	GUMI_NAV_TIMEUTC		= 0x21,
	GUMI_NAV_VELEVEF		= 0x11,
	GUMI_NAV_VELNED			= 0x12,

	GUMI_RXM_IMES			= 0x61,
	GUMI_RXM_MEASX			= 0x14,
	GUMI_RXM_PMREQ			= 0x41,
	GUMI_RXM_RAWX			= 0x15,
	GUMI_RXM_RLM			= 0x59,
	GUMI_RXM_RTCM			= 0x32,
	GUMI_RXM_SFRBX			= 0x13,
	GUMI_RXM_SVSI			= 0x20,

	GUMI_SEC_UNIQID			= 0x03,

	GUMI_TIM_DOSC			= 0x11,
	GUMI_TIM_FCHG			= 0x16,
	GUMI_TIM_HOC			= 0x17,
	GUMI_TIM_SMEAS			= 0x13,
	GUMI_TIM_SVIN			= 0x04,
	GUMI_TIM_TM2			= 0x03,
	GUMI_TIM_TOS			= 0x12,
	GUMI_TIM_TP				= 0x01,
	GUMI_TIM_VCOCAL			= 0x15,
	GUMI_TIM_VRFY			= 0x06,

	GUMI_UPD_SOS			= 0x14,
} PifGpsUbxMessageId;

typedef enum EnPifGpsUbxRxState
{
	GURS_SYNC_CHAR_1	= 0,
	GURS_SYNC_CHAR_2	= 1,
	GURS_CLASS			= 2,
	GURS_ID				= 3,
	GURS_LENGTH_LOW		= 4,
	GURS_LENGTH_HIGH	= 5,
	GURS_PAYLOAD		= 6,
	GURS_CK_A			= 7,
	GURS_CK_B			= 8,
	GURS_NMEA			= 9,
	GURS_DONE			= 10
} PifGpsUbxRxState;

typedef enum EnPifGpsUbloxTxState
{
	GUTS_IDLE			= 0,
	GUTS_SENDING		= 1,
	GUTS_WAIT_SENDED	= 2,
	GUTS_WAIT_RESPONSE	= 3
} PifGpsUbloxTxState;


typedef struct {
    uint32_t time;              // GPS msToW
    int32_t longitude;
    int32_t latitude;
    int32_t altitude_ellipsoid;
    int32_t altitude_msl;
    uint32_t horizontal_accuracy;
    uint32_t vertical_accuracy;
} PifGpsUbxNavPosllh;

typedef struct {
    uint32_t time;              // iTOW
    uint8_t gps_fix;
    uint8_t flags;
    uint8_t fix_stat;
    uint8_t flags2;
    uint32_t ttff;				// time_to_first_fix
    uint32_t uptime;            // milliseconds
} PifGpsUbxNavStatus;

typedef struct {
    uint32_t time;
    int32_t time_nsec;
    int16_t week;
    uint8_t gps_fix;
    uint8_t flags;
    int32_t ecef_x;
    int32_t ecef_y;
    int32_t ecef_z;
    uint32_t position_accuracy_3d;
    int32_t ecef_x_velocity;
    int32_t ecef_y_velocity;
    int32_t ecef_z_velocity;
    uint32_t speed_accuracy;
    uint16_t position_DOP;
    uint8_t res1;
    uint8_t satellites;
    uint32_t res2;
} PifGpsUbxNavSolution;

typedef struct {
    uint32_t time;              // GPS msToW
    int32_t ned_north;
    int32_t ned_east;
    int32_t ned_down;
    uint32_t speed_3d;
    uint32_t speed_2d;
    int32_t heading_2d;
    uint32_t speed_accuracy;
    uint32_t heading_accuracy;
} PifGpsUbxNavVelned;

typedef struct {
    uint8_t chn;                // Channel number, 255 for SVx not assigned to channel
    uint8_t svid;               // Satellite ID
    uint8_t flags;              // Bitmask
    uint8_t quality;            // Bitfield
    uint8_t cno;                // Carrier to Noise Ratio (Signal Strength)
    uint8_t elev;               // Elevation in integer degrees
    int16_t azim;               // Azimuth in integer degrees
    int32_t prRes;              // Pseudo range residual in centimetres
} PifGpsUbxNavSvinfoChannel;

typedef struct {
    uint32_t time;              // GPS Millisecond time of week
    uint8_t numCh;              // Number of channels
    uint8_t globalFlags;        // Bitmask, Chip hardware generation 0:Antaris, 1:u-blox 5, 2:u-blox 6
    uint16_t reserved2;         // Reserved
    PifGpsUbxNavSvinfoChannel channel[16];         // 16 satellites * 12 byte
} PifGpsUbxNavSvinfo;

/**
 * @class StPifGpsUbxPacket
 * @brief
 */
typedef struct StPifGpsUbxPacket
{
	uint8_t class_id;
	uint8_t message_id;
	uint16_t length;
	union {
	    PifGpsUbxNavPosllh posllh;
	    PifGpsUbxNavStatus status;
	    PifGpsUbxNavSolution solution;
	    PifGpsUbxNavVelned velned;
	    PifGpsUbxNavSvinfo svinfo;
	    uint8_t bytes[1];
	} payload;
} PifGpsUbxPacket;


struct StPifGpsUblox;
typedef struct StPifGpsUblox PifGpsUblox;

typedef void (*PifEvtGpsUbxReceive)(PifGpsUbxPacket* p_packet);
typedef void (*PifEvtGpsUbloxError)(PifId id);
typedef void (*PifEvtGpsUbloxOtherPacket)(PifGpsUblox* p_owner, uint8_t data);

typedef struct StPifGpsUbxRx
{
	PifGpsUbxRxState state;
	uint8_t payload_count;
	PifGpsUbxPacket packet;
	uint16_t checksum;
} PifGpsUbxRx;

typedef struct StPifGpsUbloxTx
{
    PifRingBuffer buffer;
    PifGpsUbloxTxState state;
	union {
		uint8_t info[4];
		struct {
			uint8_t length;
			uint8_t response;
			uint16_t command;
		} st;
	} ui;
	uint8_t pos;
} PifGpsUbloxTx;

/**
 * @class StPifGpsUblox
 * @brief
 */
typedef struct StPifGpsUblox
{
	// Public Member Variable

    // Public Event Function
	PifEvtGpsUbxReceive evt_ubx_receive;
	PifEvtGpsUbloxOtherPacket evt_other_packet;

	// Read-only Member Variable
    PifGps _gps;
	uint8_t _numCh;                 // Number of channels
	uint8_t _svinfo_chn[32];        // Channel number
	uint8_t _svinfo_svid[32];       // Satellite ID
	uint8_t _svinfo_quality[32];	// Bitfield Qualtity
	uint8_t _svinfo_cno[32];        // Carrier to Noise Ratio (Signal Strength)
	uint32_t _svinfo_rate[2];       // GPS svinfo updating rate (column 0 = last update time, 1 = current update ms)

	// Private Member Variable
	PifComm* __p_comm;
    PifGpsUbxRx __rx;
	PifGpsUbloxTx __tx;
} PifGpsUblox;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifGpsUblox_Init
 * @brief
 * @param p_owner
 * @param id
 * @return
 */
BOOL pifGpsUblox_Init(PifGpsUblox* p_owner, PifId id);

/**
 * @fn pifGpsUblox_Clear
 * @brief
 * @param p_owner
 */
void pifGpsUblox_Clear(PifGpsUblox* p_owner);

/**
 * @fn pifGpsUblox_AttachComm
 * @brief
 * @param p_owner
 * @param p_comm
 */
void pifGpsUblox_AttachComm(PifGpsUblox* p_owner, PifComm* p_comm);

/**
 * @fn pifGpsUblox_DetachComm
 * @brief
 * @param p_owner
 */
void pifGpsUblox_DetachComm(PifGpsUblox* p_owner);

/**
 * @fn pifGpsUblox_PollRequestGBQ
 * @brief
 * @param p_owner
 * @param p_mag_id
 * @param blocking
 * @return
 */
BOOL pifGpsUblox_PollRequestGBQ(PifGpsUblox* p_owner, const char* p_mag_id, BOOL blocking);

/**
 * @fn pifGpsUblox_PollRequestGLQ
 * @brief
 * @param p_owner
 * @param p_mag_id
 * @param blocking
 * @return
 */
BOOL pifGpsUblox_PollRequestGLQ(PifGpsUblox* p_owner, const char* p_mag_id, BOOL blocking);

/**
 * @fn pifGpsUblox_PollRequestGNQ
 * @brief
 * @param p_owner
 * @param p_mag_id
 * @param blocking
 * @return
 */
BOOL pifGpsUblox_PollRequestGNQ(PifGpsUblox* p_owner, const char* p_mag_id, BOOL blocking);

/**
 * @fn pifGpsUblox_PollRequestGPQ
 * @brief
 * @param p_owner
 * @param p_mag_id
 * @param blocking
 * @return
 */
BOOL pifGpsUblox_PollRequestGPQ(PifGpsUblox* p_owner, const char* p_mag_id, BOOL blocking);

/**
 * @fn pifGpsUblox_SetPubxConfig
 * @brief
 * @param p_owner
 * @param port_id
 * @param in_proto
 * @param out_proto
 * @param baudrate
 * @param blocking
 * @return
 */
BOOL pifGpsUblox_SetPubxConfig(PifGpsUblox* p_owner, uint8_t port_id, uint16_t in_proto, uint16_t out_proto, uint32_t baudrate, BOOL blocking);

/**
 * @fn pifGpsUblox_SetPubxRate
 * @brief
 * @param p_owner
 * @param p_mag_id
 * @param rddc
 * @param rus1
 * @param rus2
 * @param rusb
 * @param rspi
 * @param blocking
 * @return
 */
BOOL pifGpsUblox_SetPubxRate(PifGpsUblox* p_owner, const char* p_mag_id, uint8_t rddc, uint8_t rus1, uint8_t rus2, uint8_t rusb, uint8_t rspi, BOOL blocking);

/**
 * @fn pifGpsUblox_SendUbxMsg
 * @brief
 * @param p_owner
 * @param class_id
 * @param msg_id
 * @param length
 * @param payload
 * @param blocking
 * @return
 */
BOOL pifGpsUblox_SendUbxMsg(PifGpsUblox* p_owner, uint8_t class_id, uint8_t msg_id, uint16_t length, uint8_t* payload, BOOL blocking);

#ifdef __cplusplus
}
#endif


#endif  // PIF_GPS_UBLOXX_H
