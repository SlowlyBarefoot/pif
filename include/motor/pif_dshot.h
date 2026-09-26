#ifndef PIF_DSHOT_H
#define PIF_DSHOT_H


#include "core/pif.h"


/*
 * DShot is the digital protocol flight controllers use to drive brushless ESCs. A frame is 16 bits,
 * sent MSB first:
 *
 *     vvvvvvvvvvv t cccc
 *
 * v is an 11-bit value (0 stops the motor, 1 to 47 are commands, 48 to 2047 are throttle), t asks
 * the ESC for telemetry and c is a checksum of the three nibbles before it. Bidirectional DShot
 * inverts the line and the checksum, and the ESC answers every frame on the same wire with a
 * 21-bit GCR coded frame that carries its eRPM, or with Extended DShot Telemetry (EDT) a
 * temperature, voltage, current or status now and then instead.
 *
 * What is protocol lives here: the frame, the command sequencing, turning captured edges or
 * samples of the answer into a value, and the buffers a timer and DMA send a frame from. What is
 * hardware is left to the client: the timer and DMA or the port bit-banging that put the frame on
 * the wire, and the input capture or port sampling that picks up the answer. Only whole frames
 * cross between the two, since a bit lasts 1.67us at DShot600 and no callback could keep up with
 * it.
 *
 * A PifDshot keeps the throttle of every motor, queues commands and builds the frames of one
 * output cycle in pifDshot_Update(), which hands them to act_write. The helpers of the
 * "Frame and buffer" and "Telemetry" groups below need no instance and can be used on their own.
 */


#ifndef PIF_DSHOT_MAX_MOTORS
#define PIF_DSHOT_MAX_MOTORS			8
#endif

// How many commands may wait in the queue. Each takes PIF_DSHOT_MAX_MOTORS + 12 bytes.
#ifndef PIF_DSHOT_COMMAND_QUEUE_SIZE
#define PIF_DSHOT_COMMAND_QUEUE_SIZE	3
#endif

#define PIF_DSHOT_ALL_MOTORS			0xFF

#define PIF_DSHOT_MIN_THROTTLE			48
#define PIF_DSHOT_MAX_THROTTLE			2047
#define PIF_DSHOT_MAX_COMMAND			47

// 3D mode: 48 to 1047 is one direction, 1048 to 2047 the other.
#define PIF_DSHOT_3D_FORWARD_MIN_THROTTLE	1048

// Timer ticks of a 0 and a 1 bit for a bit period of SYMBOL ticks: 37.5% and 75% high.
#define PIF_DSHOT_BIT0_TICKS(SYMBOL)	((SYMBOL) * 3 / 8)
#define PIF_DSHOT_BIT1_TICKS(SYMBOL)	((SYMBOL) * 3 / 4)

// Entries pifDshot_LoadBuffer() and pifDshot_LoadBufferProshot() fill: the frame and two zero
// entries that hold the line low until the next frame.
#define PIF_DSHOT_BUFFER_SIZE			18
#define PIF_PROSHOT_BUFFER_SIZE			6

// Entries of a bit-bang buffer: 3 slots for each of the 16 bits and 3 for the hold bit.
#define PIF_DSHOT_BITBANG_BUFFER_SIZE	(17 * 3)

// ProShot1000 sends a nibble per pulse, 4 pulses a frame. A pulse is BASE + nibble * WIDTH ticks
// of a 24 MHz timer: 1us to 2.25us in 83.3ns steps, in a 3.33us period.
#define PIF_PROSHOT_BASE_SYMBOL_24MHZ	24
#define PIF_PROSHOT_BIT_WIDTH_24MHZ		3
#define PIF_PROSHOT_PERIOD_24MHZ		80

// Answers of pifDshot_EdgesToGcr() and pifDshot_SamplesToGcr() that are not a GCR frame.
#define PIF_DSHOT_GCR_NONE				0xFFFFFFFEUL	// No answer was seen at all
#define PIF_DSHOT_GCR_INVALID			0xFFFFFFFFUL	// An answer was seen and does not decode


typedef enum EnPifDshotCommand
{
	DSC_MOTOR_STOP					= 0,
	DSC_BEACON1						= 1,
	DSC_BEACON2						= 2,
	DSC_BEACON3						= 3,
	DSC_BEACON4						= 4,
	DSC_BEACON5						= 5,
	DSC_ESC_INFO					= 6,
	DSC_SPIN_DIRECTION_1			= 7,
	DSC_SPIN_DIRECTION_2			= 8,
	DSC_3D_MODE_OFF					= 9,
	DSC_3D_MODE_ON					= 10,
	DSC_SETTINGS_REQUEST			= 11,
	DSC_SAVE_SETTINGS				= 12,
	DSC_EXTENDED_TELEMETRY_ENABLE	= 13,
	DSC_EXTENDED_TELEMETRY_DISABLE	= 14,
	DSC_SPIN_DIRECTION_NORMAL		= 20,
	DSC_SPIN_DIRECTION_REVERSED		= 21,
	DSC_LED0_ON						= 22,	// BLHeli32 only
	DSC_LED1_ON						= 23,
	DSC_LED2_ON						= 24,
	DSC_LED3_ON						= 25,
	DSC_LED0_OFF					= 26,
	DSC_LED1_OFF					= 27,
	DSC_LED2_OFF					= 28,
	DSC_LED3_OFF					= 29,
	DSC_AUDIO_STREAM_MODE_ON_OFF	= 30,	// KISS only
	DSC_SILENT_MODE_ON_OFF			= 31	// KISS only
} PifDshotCommand;

typedef enum EnPifDshotTelemetryType
{
	DTT_ERPM			= 0,	// value: electrical RPM, 0 when the motor stands still
	DTT_TEMPERATURE		= 1,	// value: degrees Celsius
	DTT_VOLTAGE			= 2,	// value: millivolts, in steps of 250
	DTT_CURRENT			= 3,	// value: milliamperes, in steps of 1000
	DTT_DEBUG1			= 4,	// value: as the ESC firmware defines it
	DTT_DEBUG2			= 5,
	DTT_STRESS			= 6,	// value: stress level, 0 to 255
	DTT_STATUS			= 7		// value: status and event bits, as the ESC firmware defines them
} PifDshotTelemetryType;

typedef enum EnPifDshotCommandState
{
	DCS_IDLE_WAIT		= 0,	// Waiting for every motor to be at 0
	DCS_START_DELAY		= 1,	// Holding the output before a sequence of commands
	DCS_ACTIVE			= 2,	// Sending the command, repeated as the command needs
	DCS_POST_DELAY		= 3		// Holding the output after it
} PifDshotCommandState;


typedef struct StPifDshotTelemetry
{
	PifDshotTelemetryType type;
	uint32_t value;
} PifDshotTelemetry;

typedef struct StPifDshotMotor
{
	uint16_t value;				// Throttle: 0, or PIF_DSHOT_MIN_THROTTLE to PIF_DSHOT_MAX_THROTTLE
	BOOL request_telemetry;		// Sets the telemetry bit of the next frame only

	// Read-only: bidirectional telemetry of this motor
	uint32_t _erpm;				// Last eRPM the ESC answered with
	uint32_t _telemetry_count;	// Answers that decoded
	uint32_t _error_count;		// Answers that did not
	uint32_t _missing_count;	// Frames the ESC did not answer
} PifDshotMotor;

typedef struct StPifDshotCommandControl
{
	PifDshotCommandState state;
	uint32_t delay_cycles;		// Output cycles left to hold
	uint32_t post_delay_us;
	uint8_t repeats;
	uint8_t command[PIF_DSHOT_MAX_MOTORS];
} PifDshotCommandControl;


struct StPifDshot;
typedef struct StPifDshot PifDshot;

/**
 * @fn PifActDshotWrite
 * @brief Puts the frames of one output cycle on the wire, one per motor. It is called from
 *        pifDshot_Update() and has to return quickly: load the DMA buffers, with
 *        pifDshot_LoadBuffer() for instance, and start the transfer.
 * @param p_owner Pointer to the DShot instance.
 * @param p_frames Frame of each motor, _motor_count of them. Valid only during the call.
 * @param count Number of frames.
 */
typedef void (*PifActDshotWrite)(PifDshot* p_owner, const uint16_t* p_frames, uint8_t count);

/**
 * @fn PifEvtDshotTelemetry
 * @brief Reports a bidirectional telemetry answer that decoded. It is called from
 *        pifDshot_PutGcr(), in whatever context the client calls that from.
 * @param p_owner Pointer to the DShot instance.
 * @param index Motor that answered.
 * @param p_telemetry What it answered with.
 */
typedef void (*PifEvtDshotTelemetry)(PifDshot* p_owner, uint8_t index, const PifDshotTelemetry* p_telemetry);


/**
 * @class StPifDshot
 * @brief Throttle, commands and bidirectional telemetry of a set of DShot ESCs.
 */
struct StPifDshot
{
	// Public Member Variable
	// TRUE once the ESCs have been told DSC_EXTENDED_TELEMETRY_ENABLE, which is when an answer
	// can be a temperature or voltage rather than an eRPM. Left to the client, since only the
	// ESC knows whether it took the command.
	BOOL extended_telemetry;

	// Public Event Function
	PifEvtDshotTelemetry evt_telemetry;

	// Read-only Member Variable
	PifId _id;
	uint8_t _motor_count;
	BOOL _bidirectional;
	uint32_t _cycle_us;			// Period pifDshot_Update() is called at
	PifDshotMotor _motor[PIF_DSHOT_MAX_MOTORS];

	// Private Member Variable
	PifDshotCommandControl __queue[PIF_DSHOT_COMMAND_QUEUE_SIZE + 1];
	uint8_t __queue_head;
	uint8_t __queue_tail;

	// Private Action Function
	PifActDshotWrite __act_write;
};


#ifdef __cplusplus
extern "C" {
#endif

// -------- Frame and buffer --------------------

/**
 * @fn pifDshot_MakeFrame
 * @brief Builds a frame.
 * @param value 11-bit value: 0, a command, or a throttle. Anything above 2047 is cut to 11 bits.
 * @param telemetry TRUE to set the telemetry bit.
 * @param bidirectional TRUE for bidirectional DShot, which inverts the checksum.
 * @return The 16-bit frame.
 */
uint16_t pifDshot_MakeFrame(uint16_t value, BOOL telemetry, BOOL bidirectional);

/**
 * @fn pifDshot_LoadBuffer
 * @brief Fills a timer compare buffer with a frame, a compare value per bit and two zeros after
 *        them, PIF_DSHOT_BUFFER_SIZE entries in all.
 * @param p_buffer Buffer to fill.
 * @param stride Distance between two entries, in entries. 1 for a buffer of its own; the number
 *        of channels when one DMA burst feeds several channels of a timer.
 * @param frame Frame from pifDshot_MakeFrame().
 * @param bit0 Compare value of a 0 bit, PIF_DSHOT_BIT0_TICKS() of the bit period.
 * @param bit1 Compare value of a 1 bit, PIF_DSHOT_BIT1_TICKS() of the bit period.
 * @return Number of entries filled, PIF_DSHOT_BUFFER_SIZE.
 */
uint8_t pifDshot_LoadBuffer(uint32_t* p_buffer, uint8_t stride, uint16_t frame, uint32_t bit0, uint32_t bit1);

/**
 * @fn pifDshot_LoadBufferProshot
 * @brief Fills a timer compare buffer with a frame in ProShot1000 form, a pulse per nibble and
 *        two zeros after them, PIF_PROSHOT_BUFFER_SIZE entries in all.
 * @param p_buffer Buffer to fill.
 * @param stride Distance between two entries, in entries.
 * @param frame Frame from pifDshot_MakeFrame().
 * @param base Compare value of nibble 0, PIF_PROSHOT_BASE_SYMBOL_24MHZ at 24 MHz.
 * @param width Compare value added per nibble step, PIF_PROSHOT_BIT_WIDTH_24MHZ at 24 MHz.
 * @return Number of entries filled, PIF_PROSHOT_BUFFER_SIZE.
 */
uint8_t pifDshot_LoadBufferProshot(uint32_t* p_buffer, uint8_t stride, uint16_t frame, uint32_t base, uint32_t width);

/*
 * Bit-banged DShot drives the pins of a GPIO port from a buffer that a timer's DMA requests write
 * into the port's set/reset register (BSRR on STM32), one entry per slot. A bit takes three slots:
 * every pin goes active in the first and back to rest in the third, and a 0 goes back already in
 * the second, so a 0 is active for a third of the bit and a 1 for two thirds. After the 16 bits
 * comes a hold bit that keeps the pins at rest, so that the ESC has sampled the last bit before a
 * bidirectional line is turned around to listen. Several motors on one port share a buffer.
 *
 * Only the middle slots differ from frame to frame, so the buffer is set up once with
 * pifDshot_InitBitbangBuffer() and each frame then only clears and fills those.
 */

/**
 * @fn pifDshot_InitBitbangBuffer
 * @brief Sets up the slots of a bit-bang buffer that are the same in every frame, for one pin.
 *        Call it once per pin of the port on a zeroed buffer.
 * @param p_buffer Buffer of PIF_DSHOT_BITBANG_BUFFER_SIZE entries.
 * @param set_mask What drives the pin high in an entry, (1 << pin) for BSRR.
 * @param reset_mask What drives it low, (1 << (pin + 16)) for BSRR.
 * @param inverted TRUE for bidirectional DShot, where the line rests high and pulses low.
 */
void pifDshot_InitBitbangBuffer(uint32_t* p_buffer, uint32_t set_mask, uint32_t reset_mask, BOOL inverted);

/**
 * @fn pifDshot_ClearBitbangBuffer
 * @brief Clears the slots of a bit-bang buffer that carry the frames, before the next ones are
 *        loaded.
 * @param p_buffer Buffer set up with pifDshot_InitBitbangBuffer().
 */
void pifDshot_ClearBitbangBuffer(uint32_t* p_buffer);

/**
 * @fn pifDshot_LoadBitbangBuffer
 * @brief Loads the frame of one pin into a bit-bang buffer, ORed with the other pins of the port.
 * @param p_buffer Buffer set up with pifDshot_InitBitbangBuffer() and cleared with
 *        pifDshot_ClearBitbangBuffer() since the last frame.
 * @param set_mask What drives the pin high in an entry.
 * @param reset_mask What drives it low.
 * @param frame Frame from pifDshot_MakeFrame().
 * @param inverted TRUE for bidirectional DShot.
 */
void pifDshot_LoadBitbangBuffer(uint32_t* p_buffer, uint32_t set_mask, uint32_t reset_mask, uint16_t frame, BOOL inverted);

// -------- Telemetry --------------------

/**
 * @fn pifDshot_EdgesToGcr
 * @brief Turns the times of the edges of an answer, as input capture takes them, into its GCR
 *        frame. Every edge stands for a 1 and the time to the next one for the 0s after it; the
 *        run after the last edge is whatever is left of the 21 bits, since the line rests at the
 *        level the last bit leaves it at.
 * @param p_edges Capture times of the edges, the first one being the start of the answer.
 * @param count Number of edges.
 * @param bit_ticks Capture timer ticks of a telemetry bit, which is 4/5 of a DShot bit.
 * @return The 21-bit GCR frame, PIF_DSHOT_GCR_NONE if there are no edges or PIF_DSHOT_GCR_INVALID
 *         if they do not add up to 21 bits.
 */
uint32_t pifDshot_EdgesToGcr(const uint32_t* p_edges, uint8_t count, uint32_t bit_ticks);

/**
 * @fn pifDshot_SamplesToGcr
 * @brief Turns samples of a port taken at a fixed rate, as bit-banged DShot takes them, into the
 *        GCR frame of the answer on one pin. The line idles high; the answer starts at the first
 *        low sample.
 * @param p_samples Port samples.
 * @param count Number of samples.
 * @param mask Bit of the pin in a sample.
 * @param oversample Samples per telemetry bit, 3 for instance.
 * @return The 21-bit GCR frame, PIF_DSHOT_GCR_NONE if the pin never goes low or
 *         PIF_DSHOT_GCR_INVALID if what it does is not 21 bits.
 */
uint32_t pifDshot_SamplesToGcr(const uint16_t* p_samples, uint16_t count, uint16_t mask, uint8_t oversample);

/**
 * @fn pifDshot_DecodeGcr
 * @brief Decodes a GCR frame into the 12-bit value of the answer and checks its checksum.
 * @param gcr 21-bit GCR frame from pifDshot_EdgesToGcr() or pifDshot_SamplesToGcr().
 * @param p_value Where the 12-bit value goes.
 * @return TRUE if it decodes and its checksum holds, otherwise FALSE.
 */
BOOL pifDshot_DecodeGcr(uint32_t gcr, uint16_t* p_value);

/**
 * @fn pifDshot_DecodeTelemetry
 * @brief Tells what a 12-bit answer carries and converts it.
 * @param value 12-bit value from pifDshot_DecodeGcr().
 * @param extended TRUE if the ESC sends Extended DShot Telemetry, in which case values with a
 *        form no eRPM period takes are temperatures, voltages and so on.
 * @param p_telemetry Where the result goes.
 * @return TRUE if it converts, FALSE for an eRPM period of 0, which no ESC sends.
 */
BOOL pifDshot_DecodeTelemetry(uint16_t value, BOOL extended, PifDshotTelemetry* p_telemetry);

// -------- Instance --------------------

/**
 * @fn pifDshot_Init
 * @brief Initializes a DShot instance. Every motor starts at 0.
 * @param p_owner Pointer to the instance.
 * @param id Identifier, or PIF_ID_AUTO.
 * @param motor_count Number of motors, 1 to PIF_DSHOT_MAX_MOTORS.
 * @param bidirectional TRUE for bidirectional DShot.
 * @param cycle_us Period pifDshot_Update() is called at, which command delays are counted in.
 * @param act_write Puts the frames of a cycle on the wire.
 * @return TRUE on success, otherwise FALSE.
 */
BOOL pifDshot_Init(PifDshot* p_owner, PifId id, uint8_t motor_count, BOOL bidirectional, uint32_t cycle_us, PifActDshotWrite act_write);

/**
 * @fn pifDshot_SetCyclePeriod
 * @brief Changes the period pifDshot_Update() is called at. A command already queued keeps the
 *        delay it was counting down.
 * @param p_owner Pointer to the instance.
 * @param cycle_us New period.
 */
void pifDshot_SetCyclePeriod(PifDshot* p_owner, uint32_t cycle_us);

/**
 * @fn pifDshot_SetThrottle
 * @brief Sets the throttle of a motor. 1 to 47 would be read as a command and are raised to
 *        PIF_DSHOT_MIN_THROTTLE; above PIF_DSHOT_MAX_THROTTLE is cut to it.
 * @param p_owner Pointer to the instance.
 * @param index Motor, or PIF_DSHOT_ALL_MOTORS.
 * @param value 0 to stop, or PIF_DSHOT_MIN_THROTTLE to PIF_DSHOT_MAX_THROTTLE.
 */
void pifDshot_SetThrottle(PifDshot* p_owner, uint8_t index, uint16_t value);

/**
 * @fn pifDshot_Command
 * @brief Queues a command. It goes out from pifDshot_Update() once every motor is at 0, repeated
 *        as often as the command needs (10 times for the ones the ESC stores, once for the rest),
 *        with the pauses ESCs expect around it: at least 10ms before a sequence, 1ms between
 *        repeats, and after it 100ms for a beacon, 12ms for ESC info and 1ms otherwise. The output
 *        is held for those, whole cycles rounded up, and a pause ends with the cycle that sends
 *        the next frame, so two frames of a sequence are a cycle further apart than the pause.
 *        Motors the command is not for get DSC_MOTOR_STOP meanwhile.
 * @param p_owner Pointer to the instance.
 * @param index Motor, or PIF_DSHOT_ALL_MOTORS.
 * @param command Command, up to PIF_DSHOT_MAX_COMMAND.
 * @return TRUE if queued, otherwise FALSE (E_INVALID_PARAM, or E_OVERFLOW_BUFFER with the queue
 *         full).
 */
BOOL pifDshot_Command(PifDshot* p_owner, uint8_t index, uint8_t command);

/**
 * @fn pifDshot_IsCommandBusy
 * @brief Tells whether a command is queued or going out.
 * @param p_owner Pointer to the instance.
 * @return TRUE while the queue is not empty.
 */
BOOL pifDshot_IsCommandBusy(PifDshot* p_owner);

/**
 * @fn pifDshot_Update
 * @brief Builds the frames of one output cycle and hands them to act_write, or holds the output
 *        for this cycle while a command sequence needs a pause. Call it every cycle_us.
 * @param p_owner Pointer to the instance.
 * @return TRUE if frames were written, FALSE if the output is held this cycle.
 */
BOOL pifDshot_Update(PifDshot* p_owner);

/**
 * @fn pifDshot_PutGcr
 * @brief Hands the instance the answer of a motor to its last frame, as pifDshot_EdgesToGcr() or
 *        pifDshot_SamplesToGcr() returned it. It is decoded, counted, and reported through
 *        evt_telemetry; an eRPM is kept in _erpm as well.
 * @param p_owner Pointer to the instance.
 * @param index Motor.
 * @param gcr GCR frame, PIF_DSHOT_GCR_NONE or PIF_DSHOT_GCR_INVALID.
 * @return TRUE if it decoded, otherwise FALSE.
 */
BOOL pifDshot_PutGcr(PifDshot* p_owner, uint8_t index, uint32_t gcr);

#ifdef __cplusplus
}
#endif


#endif  // PIF_DSHOT_H
