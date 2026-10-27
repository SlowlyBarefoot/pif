// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file pif_collect_signal.h
 * @brief Records value changes of signals as a VCD file for a waveform viewer.
 *
 * Records value changes of channels as a VCD (Value Change Dump) file, which a waveform viewer
 * such as GTKWave shows directly.
 *
 * A channel is a PifCollectSignalChannel the caller owns, usually a member of the object whose
 * signal it records, so no memory is allocated per channel. Any code can add one:
 *
 *     static PifCollectSignalChannel s_level;
 *
 *     pifCollectSignal_AddChannel(&s_level, "level", PIF_ID_AUTO, CSVT_REG, 12, 0);
 *     pifCollectSignal_Start();
 *     ...
 *     pifCollectSignal_Put(&s_level, adc_value);
 *
 * pifCollectSignal_Put() may be called whether or not the channel is added or a capture is
 * running. It always keeps the last value, so the value at pifCollectSignal_Start() becomes the
 * initial value of the dump, and it records only a change made during a capture.
 *
 * CSM_LOG prints each change through pifLog at once. CSM_BUFFER, chosen by InitHeap/InitStatic,
 * stores each change as a 10 byte record and prints the dump later with
 * pifCollectSignal_PrintLog(), so a capture costs no formatting and no UART time. What a capture
 * that fills the buffer keeps is chosen with pifCollectSignal_ChangeOverflow(), and the dump says
 * how many changes were dropped.
 *
 * A backend, such as a recorder, can take the changes instead of the VCD output. Attached with
 * pifCollectSignal_AttachBackend(), it is told when a capture starts and stops, and gets every change
 * recorded during it. It finds the channels with pifCollectSignal_GetFirstChannel() and
 * pifCollectSignal_GetNextChannel().
 *
 * PIF_COLLECT_SIGNAL_NO_VCD leaves the VCD output out, CSM_LOG and CSM_BUFFER with their functions, for an
 * application whose captures all go to a backend. pifCollectSignal_Start() then needs a backend.
 */
#ifndef PIF_COLLECT_SIGNAL_H
#define PIF_COLLECT_SIGNAL_H


#include "core/pif_ring_buffer.h"
#include "core/pif_task_manager.h"


#ifdef PIF_COLLECT_SIGNAL

/// Characters of text a transfer task pass hands to pifLog at most, beyond the line it is on.
#ifndef PIF_COLLECT_SIGNAL_TEXT_SIZE
#define PIF_COLLECT_SIGNAL_TEXT_SIZE	96
#endif

/// Bytes a channel keeps for its name, the terminating NUL included.
#ifndef PIF_COLLECT_SIGNAL_NAME_SIZE
#define PIF_COLLECT_SIGNAL_NAME_SIZE	8
#endif


/**
 * @brief Time unit of the timestamps in the dump.
 */
typedef enum EnPifCollectSignalScale
{
	CSS_1S				= 0,	///< Seconds
	CSS_1MS				= 1,	///< Default
	CSS_1US				= 2	///< Microseconds
} PifCollectSignalScale;

/**
 * @brief Where the changes go during a capture.
 */
typedef enum EnPifCollectSignalMethod
{
	CSM_LOG				= 0,	///< Printed through pifLog at once
	CSM_BUFFER			= 1	///< Stored in a buffer and printed later with pifCollectSignal_PrintLog()
} PifCollectSignalMethod;

/// What CSM_BUFFER keeps when a capture fills the buffer. The end time of the capture is kept
/// either way.
typedef enum EnPifCollectSignalOverflow
{
	/// Default. The changes from the start, until the buffer is full. Fits a capture started
	/// right before the event of interest.
	CSO_KEEP_FIRST		= 0,
	/// The latest changes, which push the oldest out. The dump then begins at the time of the last
	/// change pushed out, with the values of that time. Fits a capture stopped right after the
	/// event of interest.
	CSO_KEEP_LAST		= 1
} PifCollectSignalOverflow;

/**
 * @brief VCD variable type of a channel.
 */
typedef enum EnPifCollectSignalVarType
{
	CSVT_INTEGER		= 0,	///< integer
	CSVT_REAL			= 1,	///< Put with pifCollectSignal_PutReal(). The width is ignored.
	CSVT_REG			= 2,	///< reg, a value of the given width in bits
	CSVT_WIRE			= 3	///< wire, a value of the given width in bits
} PifCollectSignalVarType;


/**
 * @struct StPifCollectSignalChannel
 * @brief One signal of the dump. The caller owns it and zero initializes it, or leaves that to
 *        static storage, before its first use.
 */
typedef struct StPifCollectSignalChannel
{
	// Read-only Member Variable
	char _name[PIF_COLLECT_SIGNAL_NAME_SIZE];	///< Name of the signal in the dump
	uint32_t _value;				///< Last value, raw bits of a float for CSVT_REAL
	PifId _id;	///< ID of the object, added to the name
	uint8_t _var_type;				///< PifCollectSignalVarType
	uint8_t _width;	///< Width in bits

	// Private Member Variable
	uint8_t __state;
	uint16_t __index;
	uint32_t __start_value;
	struct StPifCollectSignalChannel *__p_next;
} PifCollectSignalChannel;

/**
 * @struct StPifCollectSignalBackend
 * @brief Takes the changes of a capture in place of the VCD output.
 */
typedef struct StPifCollectSignalBackend
{
	/// Called by pifCollectSignal_Start() once the channels of the capture are fixed. Returning FALSE
	/// refuses the start.
	BOOL (*evt_start)(void *p_client);
	/// A change of a channel of the capture. Called from where pifCollectSignal_Put() is, which may be
	/// an interrupt.
	void (*evt_change)(void *p_client, const PifCollectSignalChannel *p_channel, uint32_t value);
	/// Called by pifCollectSignal_Stop().
	void (*evt_stop)(void *p_client);
} PifCollectSignalBackend;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initializes the collector for CSM_LOG, which prints each change at once.
 * @param p_module_name Scope name of the dump. It must stay valid while the collector is used.
 */
void pifCollectSignal_Init(const char *p_module_name);

#ifndef PIF_COLLECT_SIGNAL_NO_VCD

/**
 * @brief Initializes the collector for CSM_BUFFER with a buffer allocated from the heap.
 * @param p_module_name Scope name of the dump. It must stay valid while the collector is used.
 * @param size Buffer size in bytes. Each change takes 10 bytes, and 21 bytes is the least.
 * @return TRUE on success, otherwise FALSE.
 */
BOOL pifCollectSignal_InitHeap(const char *p_module_name, uint16_t size);

/**
 * @brief Initializes the collector for CSM_BUFFER with a buffer the caller provides.
 * @param p_module_name Scope name of the dump. It must stay valid while the collector is used.
 * @param size Buffer size in bytes. Each change takes 10 bytes, and 21 bytes is the least.
 * @param p_buffer Buffer of size bytes.
 * @return TRUE on success, otherwise FALSE.
 */
BOOL pifCollectSignal_InitStatic(const char *p_module_name, uint16_t size, uint8_t *p_buffer);

#endif	// PIF_COLLECT_SIGNAL_NO_VCD

/**
 * @brief Removes every channel, stops a capture or a transfer and releases the buffer.
 */
void pifCollectSignal_Clear();

#ifndef PIF_COLLECT_SIGNAL_NO_VCD

/**
 * @brief Gets the period of the transfer task of CSM_BUFFER.
 * @return Period in microseconds.
 */
uint32_t pifCollectSignal_GetTransferPeriod();

/**
 * @brief Sets the period of the transfer task of CSM_BUFFER.
 * @param period1ms Period in milliseconds. It must not be 0.
 * @return TRUE on success, otherwise FALSE.
 */
BOOL pifCollectSignal_SetTransferPeriod(uint16_t period1ms);

/**
 * @brief Changes the time unit of the dump. Not allowed during a capture or a transfer.
 * @param scale Time unit.
 * @return TRUE on success, otherwise FALSE.
 */
BOOL pifCollectSignal_ChangeScale(PifCollectSignalScale scale);

/**
 * @brief Changes how changes are output. Not allowed during a capture or a transfer, and
 *        CSM_BUFFER needs the buffer of InitHeap/InitStatic.
 * @param method Output method.
 * @return TRUE on success, otherwise FALSE.
 */
BOOL pifCollectSignal_ChangeMethod(PifCollectSignalMethod method);

/**
 * @brief Chooses what CSM_BUFFER keeps when a capture fills the buffer. Not allowed during a
 *        capture or a transfer.
 * @param overflow CSO_KEEP_FIRST (default) or CSO_KEEP_LAST.
 * @return TRUE on success, otherwise FALSE.
 */
BOOL pifCollectSignal_ChangeOverflow(PifCollectSignalOverflow overflow);

#endif	// PIF_COLLECT_SIGNAL_NO_VCD

/**
 * @brief Adds a channel to the dump. A channel added during a capture joins the next one, and
 *        adding a channel that is already added changes nothing.
 * @param p_channel Channel the caller owns.
 * @param p_name Signal name without spaces, shorter than PIF_COLLECT_SIGNAL_NAME_SIZE. It is copied
 *        into the channel, so it may be built in a temporary buffer. A longer name is refused rather
 *        than cut, which could make two channels look alike.
 * @param id Appended to the name as "_<hex>" to tell instances apart, or PIF_ID_AUTO for none.
 * @param var_type VCD variable type.
 * @param width Bits of the value, 1 to 32. Ignored for CSVT_REAL.
 * @param initial_value Value until the first pifCollectSignal_Put().
 * @return TRUE on success, otherwise FALSE.
 */
BOOL pifCollectSignal_AddChannel(PifCollectSignalChannel *p_channel, const char *p_name, PifId id,
		PifCollectSignalVarType var_type, uint8_t width, uint32_t initial_value);

/**
 * @brief Removes a channel. Allowed at any time, and a channel that is not added is ignored.
 * @param p_channel Channel to remove.
 */
void pifCollectSignal_RemoveChannel(PifCollectSignalChannel *p_channel);

/**
 * @brief Starts a capture of the channels added so far, from time 0. A capture that was not
 *        printed yet is discarded. The channels are numbered from 0 in the order they were added,
 *        see pifCollectSignal_GetIndex().
 * @return TRUE on success, otherwise FALSE (E_CANNOT_USE without a backend under PIF_COLLECT_SIGNAL_NO_VCD).
 */
BOOL pifCollectSignal_Start();

/**
 * @brief Stops the capture and records its end time.
 */
void pifCollectSignal_Stop();

/**
 * @brief Tells whether a capture is running, from pifCollectSignal_Start() until
 *        pifCollectSignal_Stop().
 * @return TRUE while capturing.
 */
BOOL pifCollectSignal_IsCollecting();

/**
 * @brief Sets the value of a channel and records it if it changed during a capture.
 * @param p_channel Target channel.
 * @param value New value. Bits above the width are ignored.
 */
void pifCollectSignal_Put(PifCollectSignalChannel *p_channel, uint32_t value);

/**
 * @brief Sets the value of a CSVT_REAL channel and records it if it changed during a capture.
 * @param p_channel Target channel.
 * @param value New value.
 */
void pifCollectSignal_PutReal(PifCollectSignalChannel *p_channel, float value);

#ifndef PIF_COLLECT_SIGNAL_NO_VCD

/**
 * @brief Prints the stopped capture of CSM_BUFFER through pifLog from the transfer task.
 *        Other log output is held back until it is done.
 * @return TRUE on success, otherwise FALSE.
 */
BOOL pifCollectSignal_PrintLog();

/**
 * @brief Tells whether pifCollectSignal_PrintLog() is still transferring.
 * @return TRUE while transferring.
 */
BOOL pifCollectSignal_IsPrinting();

/**
 * @brief Gets how many changes of the last capture did not fit in the buffer.
 * @return Dropped changes.
 */
uint32_t pifCollectSignal_GetDropCount();

#endif	// PIF_COLLECT_SIGNAL_NO_VCD

/**
 * @brief Sends the captures to a backend instead of the VCD output, or back to it. Not allowed during
 *        a capture or a transfer. pifCollectSignal_Init() and pifCollectSignal_Clear() detach it.
 * @param p_backend Backend that stays valid while it is attached, or NULL to detach it.
 * @param p_client Passed to the functions of the backend.
 * @return TRUE on success, otherwise FALSE.
 */
BOOL pifCollectSignal_AttachBackend(const PifCollectSignalBackend *p_backend, void *p_client);

/**
 * @brief Gets the first channel added, to walk them all with pifCollectSignal_GetNextChannel().
 * @return First channel in the order they were added, or NULL when there is none.
 */
const PifCollectSignalChannel *pifCollectSignal_GetFirstChannel();

/**
 * @brief Gets the channel added after the given one.
 * @param p_channel A channel that is added.
 * @return Next channel, or NULL after the last.
 */
const PifCollectSignalChannel *pifCollectSignal_GetNextChannel(const PifCollectSignalChannel *p_channel);

/**
 * @brief Tells whether a channel is in the running capture, that is, was added before it started.
 * @param p_channel Channel to check.
 * @return TRUE when its changes are recorded now.
 */
BOOL pifCollectSignal_IsCapturing(const PifCollectSignalChannel *p_channel);

/**
 * @brief Gets the number pifCollectSignal_Start() gave a channel: 0 for the first channel added, then
 *        one more for each. A backend can use it as a compact channel ID.
 * @param p_channel A channel that is in the running capture.
 * @return Number of the channel in the capture.
 */
uint16_t pifCollectSignal_GetIndex(const PifCollectSignalChannel *p_channel);

#ifdef __cplusplus
}
#endif


#endif	// PIF_COLLECT_SIGNAL


#endif	// PIF_COLLECT_SIGNAL_H
