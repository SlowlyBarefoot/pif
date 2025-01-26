#ifndef PIF_GPS_UBLOXX_H
#define PIF_GPS_UBLOXX_H


#include "communication/pif_uart.h"
#include "communication/pif_i2c.h"
#include "core/pif_ring_buffer.h"
#include "core/pif_timer.h"
#include "gps/pif_gps.h"


#ifndef PIF_GPS_UBLOX_TX_SIZE
#define PIF_GPS_UBLOX_TX_SIZE				64
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

	GUMI_NMEA_DTM			= 0x0A,
	GUMI_NMEA_GBQ			= 0x44,
	GUMI_NMEA_GBS			= 0x09,
	GUMI_NMEA_GGA			= 0x00,
	GUMI_NMEA_GLL			= 0x01,
	GUMI_NMEA_GLQ			= 0x43,
	GUMI_NMEA_GNQ			= 0x42,
	GUMI_NMEA_GNS			= 0x0D,
	GUMI_NMEA_GPQ			= 0x40,
	GUMI_NMEA_GRS			= 0x06,
	GUMI_NMEA_GSA			= 0x02,
	GUMI_NMEA_GST			= 0x07,
	GUMI_NMEA_GSV			= 0x03,
	GUMI_NMEA_RMC			= 0x04,
	GUMI_NMEA_THS			= 0x0E,
	GUMI_NMEA_TXT			= 0x41,
	GUMI_NMEA_VLW			= 0x0F,
	GUMI_NMEA_VTG			= 0x05,
	GUMI_NMEA_ZDA			= 0x08,

	GUMI_NMEA_CONFIG		= 0x41,
	GUMI_NMEA_POSITION		= 0x00,
	GUMI_NMEA_RATE			= 0x40,
	GUMI_NMEA_SVSTATUS		= 0x03,
	GUMI_NMEA_TIME			= 0x04
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
	GURS_DONE			= 9
} PifGpsUbxRxState;

typedef enum EnPifGpsUbxTxState
{
	GUTS_IDLE			= 0,
	GUTS_SENDING		= 1,
	GUTS_WAIT_SENDED	= 2,
	GUTS_WAIT_RESPONSE	= 3
} PifGpsUbxTxState;

typedef enum EnPifGpsUbxRequestState
{
	GURS_NONE			= 0,
	GURS_SEND			= 1,
	GURS_NAK			= 2,
	GURS_ACK			= 3,
	GURS_TIMEOUT		= 4,
	GURS_FAILURE		= 5
} PifGpsUbxRequestState;


typedef struct {
    uint32_t i_tow;			// ms, GPS time of week of the navigation epoch
    int32_t lon;			// 1e-7 deg, Longitude
    int32_t lat;			// 1e-7 deg, Latitude
    int32_t height;			// mm, Height above ellipsoid
    int32_t h_msl;			// mm, Height above mean sea level
    uint32_t h_acc;			// mm, Horizontal accuracy estimate
    uint32_t v_acc;			// mm, Vertical accuracy estimate
} PifGpsUbxNavPosllh;

typedef struct {
	uint32_t i_tow;			// 1ms, GPS time of week of the navigation epoch
	uint16_t year;			// Year (UTC)
	uint8_t month;			// Month, range 1..12 (UTC)
	uint8_t day;			// Day of month, range 1..31 (UTC)
	uint8_t hour;			// Hour of day, range 0..23 (UTC)
	uint8_t min;			// Minute of hour, range 0..59 (UTC)
	uint8_t sec;			// Seconds of minute, range 0..60 (UTC)
	uint8_t valid;			// Validity flags (see graphic below)
	uint32_t t_acc;			// ms, Time accuracy estimate (UTC)
	int32_t nano;			// ns, Fraction of second, range -1e9 .. 1e9 (UTC)
	uint8_t fix_type;		// GNSSfix Type:
	uint8_t flags;			// Fix status flags
	uint8_t flags2;			// Additional flags
	uint8_t num_sv;			// Number of satellites used in Nav Solution
	int32_t lon;			// 1e-7 deg, Longitude
	int32_t lat;			// 1e-7 deg, Latitude
	int32_t height;			// mm, Height above ellipsoid
	int32_t h_msl;			// mm, Height above mean sea level
	uint32_t h_acc;			// mm, Horizontal accuracy estimate
	uint32_t v_acc;			// mm, Vertical accuracy estimate
	int32_t val_n;			// mm/s, NED north velocity
	int32_t val_e;			// mm/s, NED east velocity
	int32_t val_d;			// mm/s, NED down velocity
	int32_t g_speed;		// mm/s, Ground Speed (2-D)
	int32_t head_mot;		// 1e-5 deg, Heading of motion (2-D)
	uint32_t s_acc;			// mm/s, Speed accuracy estimate
	uint32_t head_acc;		// 1e-5 deg, Heading accuracy estimate (both motion and vehicle)
	uint16_t p_dop;			// 0.01, Position DOP
	uint16_t flag3;			// Additional flags
	uint32_t reserved1;
	int32_t head_veh;		// 1e-5 deg, Heading of vehicle (2-D), this is only valid when headVehValid is set, otherwise the output is set to the heading of motion
	int16_t mag_dec;		// 1e-2 deg, Magnetic declination. Only supported in ADR 4.10 and later.
	uint16_t mag_acc;		// 1e-2 deg, Magnetic declination accuracy. Only supported in ADR 4.10 and later.
} PifGpsUbxNavPvt;

typedef struct {
    uint32_t i_tow;			// ms, GPS time of week of the navigation epoch
    int32_t f_tow;			// ns, Fractional part of iTOW (range: +/-500000).
    int16_t week;			// GPS week number of the navigation epoch
    uint8_t gps_fix;		// GPSfix Type, range 0..5
    uint8_t flags;			// Fix Status Flags
    int32_t ecef_x;			// cm, ECEF X coordinate
    int32_t ecef_y;			// cm, ECEF Y coordinate
    int32_t ecef_z;			// cm, ECEF Z coordinate
    uint32_t p_acc;			// cm, 3D Position Accuracy Estimate
    int32_t ecef_vx;		// cm/s, ECEF X velocity
    int32_t ecef_vy;		// cm/s, ECEF Y velocity
    int32_t ecef_vz;		// cm/s, ECEF Z velocity
    uint32_t s_acc;			// cm/s, Speed Accuracy Estimate
    uint16_t p_dop;			// 0.01, Position DOP
    uint8_t reserved1;
    uint8_t num_sv;			// Number of SVs used in Nav Solution
    uint32_t reserved2;
} PifGpsUbxNavSol;

typedef struct {
    uint32_t i_tow;			// ms, GPS time of week of the navigation epoch
    uint8_t gps_fix;		// GPSfix Type, this value does not qualify a fix as valid and within the limits.
    uint8_t flags;			// Navigation Status Flags
    uint8_t fix_stat;		// Fix Status Information
    uint8_t flags2;			// further information about navigation output
    uint32_t ttff;			// ms, Time to first fix (millisecond time tag)
    uint32_t uptime;		// ms, Milliseconds since Startup / Reset
} PifGpsUbxNavStatus;

typedef struct {
    uint8_t chn;            // Channel number, 255 for SVx not assigned to channel
    uint8_t svid;           // Satellite ID
    uint8_t flags;          // Bitmask
    uint8_t quality;        // Bitfield
    uint8_t cno;            // dBHz, Carrier to Noise Ratio (Signal Strength)
    int8_t elev;            // deg, Elevation in integer degrees
    int16_t azim;           // deg, Azimuth in integer degrees
    int32_t pr_res;         // cm, Pseudo range residual in centimetres
} PifGpsUbxNavSvInfoChannel;

typedef struct {
    uint32_t i_tow;         // ms, GPS time of week of the navigation epoch
    uint8_t num_ch;         // Number of channels
    uint8_t global_flags;   // Bitmask, Chip hardware generation 0:Antaris, 1:u-blox 5, 2:u-blox 6
    uint16_t reserved1;
    PifGpsUbxNavSvInfoChannel channel[32];	// 16 satellites * 12 byte
} PifGpsUbxNavSvInfo;

typedef struct {
    uint32_t i_tow;			// ms, GPS time of week of the navigation epoch
    uint32_t t_acc;			// ns, Time accuracy estimate (UTC)
    int32_t nano;			// ns, Fraction of second, range -1e9 .. 1e9 (UTC)
    uint16_t year;			// Year, range 1999..2099 (UTC)
    uint8_t month;			// Month, range 1..12 (UTC)
    uint8_t day;			// Day of month, range 1..31 (UTC)
    uint8_t hour;			// Hour of day, range 0..23 (UTC)
    uint8_t min;			// Minute of hour, range 0..59 (UTC)
    uint8_t sec;			// Seconds of minute, range 0..60 (UTC)
    uint8_t valid;			// Validity Flags
} PifGpsUbxNavTimeUtc;

typedef struct {
    uint32_t i_tow;			// ms, GPS time of week of the navigation epoch
    int32_t val_n;			// cm/s, North velocity component
    int32_t val_e;			// cm/s, East velocity component
    int32_t val_d;			// cm/s, Down velocity component
    uint32_t speed;			// cm/s, Speed (3-D)
    uint32_t g_speed;		// cm/s, Ground speed (2-D)
    int32_t heading;		// 1e-5 deg, Heading of motion 2-D
    uint32_t s_acc;			// cm/s, Speed accuracy Estimate
    uint32_t c_acc;			// 1e-5 deg, Course / Heading accuracy estimate
} PifGpsUbxNavVelned;

/**
 * @class StPifGpsUbxPacket
 * @brief
 */
typedef struct StPifGpsUbxPacket
{
	uint8_t class_id;
	uint8_t msg_id;
	uint16_t length;
	union {
	    PifGpsUbxNavPosllh posllh;
	    PifGpsUbxNavPvt pvt;
	    PifGpsUbxNavSol sol;
	    PifGpsUbxNavStatus status;
	    PifGpsUbxNavSvInfo sv_info;
	    PifGpsUbxNavTimeUtc time_utc;
		PifGpsUbxNavVelned velned;
	    uint8_t bytes[1];
	} payload;
} PifGpsUbxPacket;


struct StPifGpsUblox;
typedef struct StPifGpsUblox PifGpsUblox;

typedef BOOL (*PifEvtGpsUbxReceive)(PifGpsUblox* p_owner, PifGpsUbxPacket* p_packet);
typedef void (*PifEvtGpsUbloxError)(PifId id);
typedef void (*PifEvtGpsUbloxOtherPacket)(PifGpsUblox* p_owner, uint8_t data);

typedef struct StPifGpsUbxRx
{
	PifGpsUbxRxState state;
	uint16_t payload_count;
	PifGpsUbxPacket packet;
	uint16_t checksum;
} PifGpsUbxRx;

typedef struct StPifGpsUbloxTx
{
    PifRingBuffer buffer;
    volatile PifGpsUbxTxState state;
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
struct StPifGpsUblox
{
	// Public Member Variable

    // Public Event Function
	PifEvtGpsUbxReceive evt_ubx_receive;
	PifEvtGpsUbloxOtherPacket evt_other_packet;

	// Read-only Member Variable
    PifGps _gps;
    PifTask* _p_task;
    volatile PifGpsUbxRequestState _request_state;
	uint8_t _num_ch;                // Number of channels
	uint8_t _svinfo_chn[16];        // Channel number
	uint8_t _svinfo_svid[16];       // Satellite ID
	uint8_t _svinfo_quality[16];	// Bitfield Qualtity
	uint8_t _svinfo_cno[16];        // Carrier to Noise Ratio (Signal Strength)
	uint32_t _svinfo_rate[2];       // GPS svinfo updating rate (column 0 = last update time, 1 = current update ms)

	// Private Member Variable
	PifUart* __p_uart;
	PifI2cPort* __p_i2c_port;
	PifI2cDevice* __p_i2c_device;
    PifGpsUbxRx __rx;
	PifGpsUbloxTx __tx;
	PifGpsUbxMessageId __cfg_msg_id;
    BOOL __next_fix;
    uint16_t __length;
};


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
 * @fn pifGpsUblox_AttachUart
 * @brief
 * @param p_owner
 * @param p_uart
 */
void pifGpsUblox_AttachUart(PifGpsUblox* p_owner, PifUart* p_uart);

/**
 * @fn pifGpsUblox_DetachUart
 * @brief
 * @param p_owner
 */
void pifGpsUblox_DetachUart(PifGpsUblox* p_owner);

/**
 * @fn pifGpsUblox_AttachI2c
 * @brief
 * @param p_owner
 * @param p_i2c
 * @param addr
 * @param max_transfer_size
 * @param period
 * @param start
 * @param name
 * @return
 */
BOOL pifGpsUblox_AttachI2c(PifGpsUblox* p_owner, PifI2cPort* p_i2c, uint8_t addr, uint16_t max_transfer_size, uint16_t period, BOOL start, const char* name);

/**
 * @fn pifGpsUblox_DetachI2c
 * @brief
 * @param p_owner
 */
void pifGpsUblox_DetachI2c(PifGpsUblox* p_owner);

/**
 * @fn pifGpsUblox_PollRequestGBQ
 * @brief
 * @param p_owner
 * @param p_mag_id
 * @param blocking
 * @param waiting
 * @return
 */
BOOL pifGpsUblox_PollRequestGBQ(PifGpsUblox* p_owner, const char* p_mag_id, BOOL blocking, uint16_t waiting);

/**
 * @fn pifGpsUblox_PollRequestGLQ
 * @brief
 * @param p_owner
 * @param p_mag_id
 * @param blocking
 * @param waiting
 * @return
 */
BOOL pifGpsUblox_PollRequestGLQ(PifGpsUblox* p_owner, const char* p_mag_id, BOOL blocking, uint16_t waiting);

/**
 * @fn pifGpsUblox_PollRequestGNQ
 * @brief
 * @param p_owner
 * @param p_mag_id
 * @param blocking
 * @param waiting
 * @return
 */
BOOL pifGpsUblox_PollRequestGNQ(PifGpsUblox* p_owner, const char* p_mag_id, BOOL blocking, uint16_t waiting);

/**
 * @fn pifGpsUblox_PollRequestGPQ
 * @brief
 * @param p_owner
 * @param p_mag_id
 * @param blocking
 * @param waiting
 * @return
 */
BOOL pifGpsUblox_PollRequestGPQ(PifGpsUblox* p_owner, const char* p_mag_id, BOOL blocking, uint16_t waiting);

/**
 * @fn pifGpsUblox_SetPubxConfig
 * @brief
 * @param p_owner
 * @param port_id
 * @param in_proto
 * @param out_proto
 * @param baudrate
 * @param blocking
 * @param waiting
 * @return
 */
BOOL pifGpsUblox_SetPubxConfig(PifGpsUblox* p_owner, uint8_t port_id, uint16_t in_proto, uint16_t out_proto, uint32_t baudrate, BOOL blocking, uint16_t waiting);

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
 * @param waiting
 * @return
 */
BOOL pifGpsUblox_SetPubxRate(PifGpsUblox* p_owner, const char* p_mag_id, uint8_t rddc, uint8_t rus1, uint8_t rus2, uint8_t rusb, uint8_t rspi, BOOL blocking, uint16_t waiting);

/**
 * @fn pifGpsUblox_SendUbxMsg
 * @brief
 * @param p_owner
 * @param class_id
 * @param msg_id
 * @param length
 * @param payload
 * @param blocking
 * @param waiting
 * @return
 */
BOOL pifGpsUblox_SendUbxMsg(PifGpsUblox* p_owner, uint8_t class_id, uint8_t msg_id, uint16_t length, uint8_t* payload, BOOL blocking, uint16_t waiting);

#ifdef __cplusplus
}
#endif


#endif  // PIF_GPS_UBLOXX_H
