// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file pif_dshot.h
 * @brief DShot, ProShot and bidirectional DShot telemetry for brushless ESCs.
 *
 * DShot puts a 16-bit frame where the servo pulse of an ESC used to be, one frame per output cycle,
 * sent MSB first:
 *
 *     value (11 bits) | telemetry (1 bit) | crc (4 bits)
 *
 * The value is 0 to stop the motor, 1 to 47 for a command and 48 to 2047 for a throttle. The
 * telemetry bit asks the ESC for a telemetry frame on its own wire, and the settings commands are
 * only taken with it set. The crc is the three nibbles above it XORed together.
 *
 * A bit is a fixed period of which a 0 is high for three eighths and a 1 for three quarters, so the
 * name of the protocol is its bit rate: 6.67 us a bit for DShot150, 3.33 us for DShot300 and 1.67 us
 * for DShot600. ProShot1000 instead sends a nibble as the width of one pulse, four pulses a frame.
 *
 * Bidirectional DShot turns the line around: the frame goes out inverted and with an inverted crc,
 * and the ESC answers on the same wire with 21 bit periods at five fourths of the DShot bit rate.
 * That answer carries the eRPM of the motor, or, with Extended DShot Telemetry (EDT) turned on, a
 * temperature, voltage, current or status instead now and then.
 *
 * This module is the protocol alone: it keeps the throttle of every motor, sequences commands,
 * builds the frames of a cycle, fills the buffers a timer sends them from, and turns the edges or
 * samples of an answer into a value. Driving the wire is left to the client, since a bit lasts
 * 1.67 us at DShot600 and no callback can keep up with that - only whole frames cross the line
 * between the two. A client therefore pairs pifDshot_Update() with a timer and DMA on the way out,
 * and input capture or port sampling with pifDshot_PutGcr() on the way back.
 *
 * The helpers under "Frame and buffer" and "Telemetry" work on their own and need no instance.
 */
#ifndef PIF_DSHOT_H
#define PIF_DSHOT_H


#include "core/pif.h"
#if defined(PIF_COLLECT_SIGNAL) && defined(PIF_COLLECT_SIGNAL_DSHOT)
	#define COLLECT_SIGNAL_DSHOT	///< Defined when PIF_COLLECT_SIGNAL is on and PIF_COLLECT_SIGNAL_DSHOT is too
	#include "monitor/pif_collect_signal.h"
#endif


/// Motors one instance drives at most.
#ifndef PIF_DSHOT_MAX_MOTORS
#define PIF_DSHOT_MAX_MOTORS			8
#endif

/// Commands that may wait in the queue. Each one takes PIF_DSHOT_MAX_MOTORS + 12 bytes.
#ifndef PIF_DSHOT_COMMAND_QUEUE_SIZE
#define PIF_DSHOT_COMMAND_QUEUE_SIZE	3
#endif

#if PIF_DSHOT_MAX_MOTORS > 254
#error "PIF_DSHOT_MAX_MOTORS is limited to 254, since PIF_DSHOT_ALL_MOTORS takes the last index"
#endif

#define PIF_DSHOT_ALL_MOTORS			0xFF	///< Motor index that means every motor of the instance

#define PIF_DSHOT_MIN_THROTTLE			48		///< Lowest throttle value, just above the commands
#define PIF_DSHOT_MAX_THROTTLE			2047	///< Highest throttle value
#define PIF_DSHOT_MAX_COMMAND			47		///< Highest command value

/// In 3D mode the throttle is split in two: 48 to 1047 turns one way, this and above the other.
#define PIF_DSHOT_3D_FORWARD_MIN_THROTTLE	1048

/// Compare value of a 0 bit for a bit period of SYMBOL timer ticks, three eighths of it.
#define PIF_DSHOT_BIT0_TICKS(SYMBOL)	((SYMBOL) * 3 / 8)
/// Compare value of a 1 bit for a bit period of SYMBOL timer ticks, three quarters of it.
#define PIF_DSHOT_BIT1_TICKS(SYMBOL)	((SYMBOL) * 3 / 4)

/// Entries pifDshot_LoadBuffer() fills: the 16 bits and 2 zeros that leave the line at rest after them.
#define PIF_DSHOT_BUFFER_SIZE			18
/// Entries pifDshot_LoadBufferProshot() fills: 4 nibble pulses and 2 zeros.
#define PIF_PROSHOT_BUFFER_SIZE			6
/// Entries of a bit-bang buffer: 3 slots for each of the 16 bits and 3 for the hold bit after them.
#define PIF_DSHOT_BITBANG_BUFFER_SIZE	(17 * 3)

/// A ProShot1000 pulse is 1 us plus 125 ns a nibble step, 1 us to 2.875 us, in a 3.33 us period.
/// These are those three times in the ticks of a 24 MHz timer.
#define PIF_PROSHOT_BASE_SYMBOL_24MHZ	24
#define PIF_PROSHOT_BIT_WIDTH_24MHZ		3	///< Ticks one nibble step adds
#define PIF_PROSHOT_PERIOD_24MHZ		80	///< Ticks of a pulse period

/// Answers of pifDshot_EdgesToGcr() and pifDshot_SamplesToGcr() that are no GCR frame.
#define PIF_DSHOT_GCR_NONE				0xFFFFFFFEUL	///< The ESC said nothing at all
#define PIF_DSHOT_GCR_INVALID			0xFFFFFFFFUL	///< It said something that is not 21 bits


/**
 * @brief DShot commands, sent as the value of a frame while the motors stand still.
 *
 * ESCs want most of them repeated and followed by a pause, which the command queue takes care of.
 */
typedef enum EnPifDshotCommand
{
	DSC_MOTOR_STOP					= 0,	///< Stops the motor
	DSC_BEACON1						= 1,	///< Beeps tone 1
	DSC_BEACON2						= 2,	///< Beeps tone 2
	DSC_BEACON3						= 3,	///< Beeps tone 3
	DSC_BEACON4						= 4,	///< Beeps tone 4
	DSC_BEACON5						= 5,	///< Beeps tone 5
	DSC_ESC_INFO					= 6,	///< Asks the ESC for its information
	DSC_SPIN_DIRECTION_1			= 7,	///< Sets spin direction 1
	DSC_SPIN_DIRECTION_2			= 8,	///< Sets spin direction 2
	DSC_3D_MODE_OFF					= 9,	///< Turns 3D mode off
	DSC_3D_MODE_ON					= 10,	///< Turns 3D mode on
	DSC_SETTINGS_REQUEST			= 11,	///< Asks the ESC for its settings
	DSC_SAVE_SETTINGS				= 12,	///< Saves the settings in the ESC
	DSC_EXTENDED_TELEMETRY_ENABLE	= 13,	///< Turns Extended DShot Telemetry on
	DSC_EXTENDED_TELEMETRY_DISABLE	= 14,	///< Turns Extended DShot Telemetry off
	DSC_SPIN_DIRECTION_NORMAL		= 20,	///< Sets the normal spin direction
	DSC_SPIN_DIRECTION_REVERSED		= 21,	///< Sets the reversed spin direction
	DSC_LED0_ON						= 22,	///< BLHeli32 only
	DSC_LED1_ON						= 23,
	DSC_LED2_ON						= 24,
	DSC_LED3_ON						= 25,
	DSC_LED0_OFF					= 26,
	DSC_LED1_OFF					= 27,
	DSC_LED2_OFF					= 28,
	DSC_LED3_OFF					= 29,
	DSC_AUDIO_STREAM_MODE_ON_OFF	= 30,	///< KISS only
	DSC_SILENT_MODE_ON_OFF			= 31	///< KISS only
} PifDshotCommand;

/**
 * @brief What an answer of the ESC carries. Everything but DTT_ERPM needs EDT.
 */
typedef enum EnPifDshotTelemetryType
{
	DTT_ERPM			= 0,	///< value: electrical RPM, 0 while the motor stands still
	DTT_TEMPERATURE		= 1,	///< value: degrees Celsius
	DTT_VOLTAGE			= 2,	///< value: millivolts, in steps of 250
	DTT_CURRENT			= 3,	///< value: milliamperes, in steps of 1000
	DTT_DEBUG1			= 4,	///< value: whatever the ESC firmware puts there
	DTT_DEBUG2			= 5,
	DTT_STRESS			= 6,	///< value: stress level, 0 to 255
	DTT_STATUS			= 7		///< value: status and event bits of the ESC firmware
} PifDshotTelemetryType;

/**
 * @brief Where a queued command stands.
 */
typedef enum EnPifDshotCommandState
{
	DCS_IDLE_WAIT		= 0,	///< Waiting for the throttle of every motor to reach 0
	DCS_START_DELAY		= 1,	///< Holding the output before the first frame of a sequence
	DCS_ACTIVE			= 2,	///< Sending the command, as often as it needs
	DCS_POST_DELAY		= 3		///< Holding the output after the last frame of it
} PifDshotCommandState;

#ifdef COLLECT_SIGNAL_DSHOT

/**
 * @brief Signals of the command queue for pifDshot_SetCsFlag() and pifDshot_ResetCsFlag().
 *
 * The _IDX values index the signals, the _BIT values select them. Both are recorded by
 * pifDshot_Update(), so a change shows at the cycle that made it.
 */
typedef enum EnPifDshotCsFlag
{
	DS_CSF_OFF			= 0,

	DS_CSF_STATE_IDX	= 0,	///< PifDshotCommandState of the command going out, PIF_DSHOT_CS_NO_COMMAND without one
	DS_CSF_COMMAND_IDX	= 1,	///< PifDshotCommand going out, 0 without one

	DS_CSF_STATE_BIT	= 1,
	DS_CSF_COMMAND_BIT	= 2,
	DS_CSF_ALL_BIT		= 3,

	DS_CSF_COUNT		= 2
} PifDshotCsFlag;

#define PIF_DSHOT_CS_NO_COMMAND		4	///< DS_CSF_STATE_IDX value while no command goes out

#endif	// COLLECT_SIGNAL_DSHOT


/**
 * @class StPifDshotTelemetry
 * @brief One answer of an ESC, decoded.
 */
typedef struct StPifDshotTelemetry
{
	PifDshotTelemetryType type;	///< What value carries
	uint32_t value;				///< Value, in the unit of type
} PifDshotTelemetry;

/**
 * @class StPifDshotMotor
 * @brief Throttle and telemetry of one motor.
 */
typedef struct StPifDshotMotor
{
	uint16_t value;				///< Throttle: 0, or PIF_DSHOT_MIN_THROTTLE to PIF_DSHOT_MAX_THROTTLE
	BOOL request_telemetry;		///< Sets the telemetry bit of the next frame and is cleared with it

	// Read-only Member Variable
	uint32_t _erpm;				///< eRPM of the last answer that carried one
	uint32_t _telemetry_count;	///< Answers that decoded
	uint32_t _error_count;		///< Answers that did not
	uint32_t _missing_count;	///< Frames the ESC left unanswered
} PifDshotMotor;

/**
 * @class StPifDshotCommandControl
 * @brief One queued command and where its sequence stands.
 */
typedef struct StPifDshotCommandControl
{
	PifDshotCommandState state;	///< Step of the sequence
	uint32_t delay_cycles;		///< Output cycles the step still holds the output for
	uint32_t post_delay_us;		///< Pause after the last frame of the command, in microseconds
	uint8_t repeats;			///< Frames of the command left to send
	uint8_t command[PIF_DSHOT_MAX_MOTORS];	///< What each motor gets, DSC_MOTOR_STOP where the command is not meant for it
} PifDshotCommandControl;


struct StPifDshot;
typedef struct StPifDshot PifDshot;

/**
 * @brief Puts the frames of one output cycle on the wire, one per motor.
 *
 * It is called from pifDshot_Update() and has to come back quickly: fill the buffers, with
 * pifDshot_LoadBuffer() for instance, start the transfer and leave.
 *
 * @param p_owner DShot instance the frames are of.
 * @param p_frames Frame of each motor, count of them. It only lives for the call.
 * @param count Number of frames, the motor count of the instance.
 */
typedef void (*PifActDshotWrite)(PifDshot *p_owner, const uint16_t *p_frames, uint8_t count);

/**
 * @brief Reports an answer of an ESC that decoded.
 *
 * It is called from pifDshot_PutGcr(), in whatever context the client calls that from.
 *
 * @param p_owner DShot instance the answer came to.
 * @param index Motor that answered.
 * @param p_telemetry What it answered. It only lives for the call.
 */
typedef void (*PifEvtDshotTelemetry)(PifDshot *p_owner, uint8_t index, const PifDshotTelemetry *p_telemetry);


/**
 * @class StPifDshot
 * @brief Throttle, commands and bidirectional telemetry of up to PIF_DSHOT_MAX_MOTORS DShot ESCs.
 *
 * pifDshot_Update() drives it, once every cycle_us, from a task or a timer of the client.
 */
struct StPifDshot
{
	// Public Member Variable
	/// TRUE once the ESCs have taken DSC_EXTENDED_TELEMETRY_ENABLE, which is when an answer may be
	/// a temperature or a voltage rather than an eRPM. It is left to the client because only the
	/// ESC knows whether the command got through.
	BOOL extended_telemetry;

	// Public Event Function
	PifEvtDshotTelemetry evt_telemetry;	///< Called for every answer that decodes. May be NULL.

	// Read-only Member Variable
	PifId _id;					///< Instance ID
	uint8_t _motor_count;		///< Number of motors
	BOOL _bidirectional;		///< TRUE for bidirectional DShot, which inverts the crc
	uint32_t _cycle_us;			///< Period pifDshot_Update() is called at
	PifDshotMotor _motor[PIF_DSHOT_MAX_MOTORS];	///< Throttle and telemetry of each motor

	// Private Member Variable
	PifDshotCommandControl __queue[PIF_DSHOT_COMMAND_QUEUE_SIZE + 1];
	uint8_t __queue_head;
	uint8_t __queue_tail;

	// Private Action Function
	PifActDshotWrite __act_write;
#ifdef COLLECT_SIGNAL_DSHOT
	PifCollectSignalChannel __cs[DS_CSF_COUNT];
#endif
};


#ifdef __cplusplus
extern "C" {
#endif

// -------- Frame and buffer --------------------

/**
 * @brief Builds a DShot frame.
 * @param value Value of the frame: 0, a command or a throttle. Only its low 11 bits are taken.
 * @param telemetry TRUE to set the telemetry bit.
 * @param bidirectional TRUE for bidirectional DShot, where the crc is inverted.
 * @return The 16-bit frame, to be sent MSB first.
 */
uint16_t pifDshot_MakeFrame(uint16_t value, BOOL telemetry, BOOL bidirectional);

/**
 * @brief Fills a timer compare buffer with a frame: a compare value per bit, and two zeros that
 *        leave the line at rest until the next frame.
 * @param p_buffer Buffer to fill, PIF_DSHOT_BUFFER_SIZE entries at the given stride.
 * @param stride Entries from one entry of the frame to the next. 1 for a buffer of its own, or the
 *        number of channels where one DMA burst feeds several channels of a timer.
 * @param frame Frame of pifDshot_MakeFrame().
 * @param bit0 Compare value of a 0 bit, PIF_DSHOT_BIT0_TICKS() of the bit period.
 * @param bit1 Compare value of a 1 bit, PIF_DSHOT_BIT1_TICKS() of the bit period.
 * @return Entries filled, PIF_DSHOT_BUFFER_SIZE.
 */
uint8_t pifDshot_LoadBuffer(uint32_t *p_buffer, uint8_t stride, uint16_t frame, uint32_t bit0, uint32_t bit1);

/**
 * @brief Fills a timer compare buffer with a frame the ProShot1000 way: a pulse per nibble, and two
 *        zeros after them.
 * @param p_buffer Buffer to fill, PIF_PROSHOT_BUFFER_SIZE entries at the given stride.
 * @param stride Entries from one pulse to the next.
 * @param frame Frame of pifDshot_MakeFrame().
 * @param base Compare value of nibble 0, PIF_PROSHOT_BASE_SYMBOL_24MHZ at 24 MHz.
 * @param width Compare value one nibble step adds, PIF_PROSHOT_BIT_WIDTH_24MHZ at 24 MHz.
 * @return Entries filled, PIF_PROSHOT_BUFFER_SIZE.
 */
uint8_t pifDshot_LoadBufferProshot(uint32_t *p_buffer, uint8_t stride, uint16_t frame, uint32_t base, uint32_t width);

/*
 * Bit-banged DShot has no timer channel per motor. The DMA requests of one timer write a buffer
 * into the set/reset register of a GPIO port (BSRR on STM32) instead, one entry per slot, so that
 * every motor on the port is driven from the same buffer and any pin will do.
 *
 * A bit takes three slots: all pins go active in the first and back to rest in the third, and a 0
 * goes back already in the second, which leaves a 0 active for a third of the bit and a 1 for two
 * thirds. A hold bit of three slots at rest follows the 16 bits, so that the ESC has sampled the
 * last one before a bidirectional line is turned around to listen.
 *
 * Only the middle slot of a bit differs from frame to frame. The buffer is therefore set up once
 * with pifDshot_InitBitbangBuffer(), and each frame then only clears those slots and fills them.
 */

/**
 * @brief Writes the slots of a bit-bang buffer that stay the same in every frame, for one pin. Call
 *        it on a zeroed buffer, once for each pin of the port.
 * @param p_buffer Buffer of PIF_DSHOT_BITBANG_BUFFER_SIZE entries.
 * @param set_mask What drives the pin high in an entry, 1 << pin for BSRR.
 * @param reset_mask What drives it low, 1 << (pin + 16) for BSRR.
 * @param inverted TRUE for bidirectional DShot, where the line rests high and pulses low.
 */
void pifDshot_InitBitbangBuffer(uint32_t *p_buffer, uint32_t set_mask, uint32_t reset_mask, BOOL inverted);

/**
 * @brief Clears the slots of a bit-bang buffer that carry the frames, before the next ones go in.
 * @param p_buffer Buffer set up by pifDshot_InitBitbangBuffer().
 */
void pifDshot_ClearBitbangBuffer(uint32_t *p_buffer);

/**
 * @brief Puts the frame of one pin into a bit-bang buffer, beside the frames of the other pins.
 * @param p_buffer Buffer set up by pifDshot_InitBitbangBuffer() and cleared by
 *        pifDshot_ClearBitbangBuffer() since the last frame.
 * @param set_mask What drives the pin high in an entry.
 * @param reset_mask What drives it low.
 * @param frame Frame of pifDshot_MakeFrame().
 * @param inverted TRUE for bidirectional DShot.
 */
void pifDshot_LoadBitbangBuffer(uint32_t *p_buffer, uint32_t set_mask, uint32_t reset_mask, uint16_t frame, BOOL inverted);

// -------- Telemetry --------------------

/*
 * The answer of an ESC is 21 bit periods in which a change of level is a 1 and a period without one
 * a 0. The first bit is the start bit, which the line going from rest to active makes, and the 20
 * after it are the frame: four 5-bit GCR symbols, each one a nibble of a 16-bit word, which is a
 * 12-bit value and an inverted crc of it.
 *
 * Since a level only changes on a bit boundary, the hardware need not sample 21 bits: capturing the
 * edges is enough, and that is what pifDshot_EdgesToGcr() takes. pifDshot_SamplesToGcr() is for the
 * bit-bang path, which has the port sampled by DMA anyway. Both answer the 21 bits, which
 * pifDshot_DecodeGcr() and pifDshot_DecodeTelemetry() turn into a value.
 */

/**
 * @brief Turns the edges of an answer, as input capture takes them, into its 21 bits. The time from
 *        one edge to the next tells how many bits the run between them is: the edge itself is a 1
 *        and the rest of the run 0s.
 * @param p_edges Capture times of the edges, the first one being where the answer starts.
 * @param count Number of edges.
 * @param bit_ticks Capture timer ticks of a telemetry bit, four fifths of a DShot bit.
 * @return The 21 bits, PIF_DSHOT_GCR_NONE without an edge or PIF_DSHOT_GCR_INVALID if the edges do
 *         not add up to 21 bits.
 */
uint32_t pifDshot_EdgesToGcr(const uint32_t *p_edges, uint8_t count, uint32_t bit_ticks);

/**
 * @brief Turns samples of a port taken at a steady rate, as the bit-bang path takes them, into the
 *        21 bits of the answer on one of its pins. The line rests high, so the answer starts at the
 *        first low sample.
 * @param p_samples Samples of the port.
 * @param count Number of samples.
 * @param mask Bit of the pin in a sample.
 * @param oversample Samples a telemetry bit is given, 3 for instance.
 * @return The 21 bits, PIF_DSHOT_GCR_NONE if the pin never goes low or the answer breaks off, and
 *         PIF_DSHOT_GCR_INVALID if what it does is not 21 bits.
 */
uint32_t pifDshot_SamplesToGcr(const uint16_t *p_samples, uint16_t count, uint16_t mask, uint8_t oversample);

/**
 * @brief Decodes the 21 bits of an answer into its 12-bit value and checks the crc.
 * @param gcr The 21 bits, as pifDshot_EdgesToGcr() or pifDshot_SamplesToGcr() answered them.
 * @param p_value Where the 12-bit value goes. May be NULL to only check the answer.
 * @return TRUE if every symbol is a nibble and the crc holds; otherwise FALSE.
 */
BOOL pifDshot_DecodeGcr(uint32_t gcr, uint16_t *p_value);

/**
 * @brief Tells what a 12-bit value carries and converts it into its unit.
 * @param value 12-bit value of pifDshot_DecodeGcr().
 * @param extended TRUE if the ESC has EDT turned on, where values of a shape no eRPM takes carry a
 *        temperature, a voltage and so on.
 * @param p_telemetry Where the result goes.
 * @return TRUE on success, FALSE with E_INVALID_PARAM when p_telemetry is NULL, and FALSE without an
 *         error for an eRPM period of 0, which no ESC sends.
 */
BOOL pifDshot_DecodeTelemetry(uint16_t value, BOOL extended, PifDshotTelemetry *p_telemetry);

// -------- Instance --------------------

/**
 * @brief Initializes an instance of motor_count motors, every one of them at throttle 0.
 * @param p_owner Target DShot instance to initialize.
 * @param id Instance ID. If set to PIF_ID_AUTO, an ID is assigned automatically.
 * @param motor_count Number of motors, 1 to PIF_DSHOT_MAX_MOTORS.
 * @param bidirectional TRUE for bidirectional DShot, where the crc of a frame is inverted and the
 *        ESC answers it.
 * @param cycle_us Period pifDshot_Update() is called at, greater than 0. The pauses of a command
 *        sequence are counted in it.
 * @param act_write Hardware callback that puts the frames of a cycle on the wire.
 * @return TRUE if initialization succeeds; otherwise FALSE with E_INVALID_PARAM.
 */
BOOL pifDshot_Init(PifDshot *p_owner, PifId id, uint8_t motor_count, BOOL bidirectional, uint32_t cycle_us, PifActDshotWrite act_write);

/**
 * @brief Releases what pifDshot_Init() took. Call it before the instance is freed or goes out of scope.
 * @param p_owner DShot instance to clear.
 */
void pifDshot_Clear(PifDshot *p_owner);

/**
 * @brief Changes the period pifDshot_Update() is called at. A pause already running keeps the
 *        number of cycles it was counting down.
 * @param p_owner DShot instance to set.
 * @param cycle_us New period. 0 is ignored.
 */
void pifDshot_SetCyclePeriod(PifDshot *p_owner, uint32_t cycle_us);

/**
 * @brief Sets the throttle a motor gets from the next cycle on.
 * @param p_owner DShot instance to set.
 * @param index Motor, or PIF_DSHOT_ALL_MOTORS for every one of them.
 * @param value 0 to stop, or PIF_DSHOT_MIN_THROTTLE to PIF_DSHOT_MAX_THROTTLE. Since 1 to 47 would
 *        read as a command, they are raised to PIF_DSHOT_MIN_THROTTLE, and anything above the range
 *        is held at PIF_DSHOT_MAX_THROTTLE.
 */
void pifDshot_SetThrottle(PifDshot *p_owner, uint8_t index, uint16_t value);

/**
 * @brief Queues a command.
 *
 * It goes out from pifDshot_Update() once every motor is at throttle 0, as often as the command
 * needs - ten times for the ones an ESC stores, once for the rest - and with the pauses ESCs expect
 * around it: 10 ms before its first frame, 1 ms between two of its frames, and after the last one
 * 100 ms for a beacon, 12 ms for ESC info and 1 ms for anything else. The output is held for a
 * pause, rounded up to whole cycles. Motors the command is not meant for are sent DSC_MOTOR_STOP
 * meanwhile. A command queued behind another one waits for it and then starts with its own 10 ms.
 *
 * @param p_owner DShot instance to command.
 * @param index Motor, or PIF_DSHOT_ALL_MOTORS for every one of them.
 * @param command Command, up to PIF_DSHOT_MAX_COMMAND.
 * @return TRUE if the command is queued, FALSE with E_INVALID_PARAM for an index or a command out of
 *         range, or with E_OVERFLOW_BUFFER if the queue is full.
 */
BOOL pifDshot_Command(PifDshot *p_owner, uint8_t index, uint8_t command);

/**
 * @brief Tells whether a command is queued or going out.
 * @param p_owner DShot instance to ask.
 * @return TRUE while the queue is not empty.
 */
BOOL pifDshot_IsCommandBusy(PifDshot *p_owner);

/**
 * @brief Builds the frames of one output cycle and hands them to act_write, or holds the output for
 *        this cycle where a command sequence wants a pause. Call it every cycle_us.
 * @param p_owner DShot instance to update.
 * @return TRUE if frames went out, FALSE if the output is held this cycle.
 */
BOOL pifDshot_Update(PifDshot *p_owner);

/**
 * @brief Hands the instance what a motor answered to its last frame, as pifDshot_EdgesToGcr() or
 *        pifDshot_SamplesToGcr() has it. It is decoded, counted, kept in _erpm where it is an eRPM,
 *        and reported through evt_telemetry.
 * @param p_owner DShot instance the answer came to.
 * @param index Motor that answered.
 * @param gcr The 21 bits of the answer, PIF_DSHOT_GCR_NONE or PIF_DSHOT_GCR_INVALID.
 * @return TRUE if the answer decoded, FALSE if it did not or with E_INVALID_PARAM for an index out
 *         of range.
 */
BOOL pifDshot_PutGcr(PifDshot *p_owner, uint8_t index, uint32_t gcr);

#ifdef COLLECT_SIGNAL_DSHOT

/**
 * @brief Adds the selected signals of the command queue to pifCollectSignal as channels.
 * @param p_owner DShot instance to watch.
 * @param index Device of the instance for a module that controls several, as PifSolenoid;
 *        PIF_CS_INDEX_ALL or any value from its count on for all. Ignored by a module of one device.
 * @param flag Bit mask of the signals to add.
 * @return TRUE if the channels are added; otherwise FALSE.
 */
BOOL pifDshot_SetCsFlag(PifDshot *p_owner, uint8_t index, PifDshotCsFlag flag);

/**
 * @brief Removes the selected signals of the command queue from pifCollectSignal.
 * @param p_owner DShot instance to stop watching.
 * @param index Device of the instance for a module that controls several, as PifSolenoid;
 *        PIF_CS_INDEX_ALL or any value from its count on for all. Ignored by a module of one device.
 * @param flag Bit mask of the signals to remove.
 */
void pifDshot_ResetCsFlag(PifDshot *p_owner, uint8_t index, PifDshotCsFlag flag);

#endif	// COLLECT_SIGNAL_DSHOT


#ifdef __cplusplus
}
#endif


#endif	// PIF_DSHOT_H
