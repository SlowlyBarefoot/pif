// SPDX-License-Identifier: BSD-3-Clause
// pifPidControl_Init() keeps the collect signal channels a controller has added: an application may set the
// controller up again on every start of a motor while its signals are recorded. Built with
// PIF_COLLECT_SIGNAL_NO_VCD, so the collect signal is the part a backend uses.
#include "core/pif_pid_control.h"

#include <stdio.h>
#include <string.h>


#define CHECK(COND) \
    do { \
        if (!(COND)) { \
            printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #COND); \
            s_failed = TRUE; \
            return; \
        } \
    } while (0)

static BOOL s_failed;
static PifPidControl s_pid;
static PifCollectSignalChannel s_after;


// The channels of the collect signal, in their order: n of them into p_list.
static int _list(const PifCollectSignalChannel **p_list, int max)
{
    const PifCollectSignalChannel *p_channel;
    int n = 0;

    for (p_channel = pifCollectSignal_GetFirstChannel(); p_channel && n < max;
            p_channel = pifCollectSignal_GetNextChannel(p_channel)) {
        p_list[n++] = p_channel;
    }
    return n;
}

static void _testInitKeepsTheAddedChannels(void)
{
    const PifCollectSignalChannel *list[8];
    uint8_t zero[sizeof(PifCollectSignalChannel)];

    pifCollectSignal_Init("pid", NULL, 0);
    pifPidControl_Init(&s_pid, 1.0f, 0.0f, 0.0f, 0.0f);
    CHECK(pifPidControl_SetCsFlag(&s_pid, PIF_CS_INDEX_ALL, PID_CSF_SETPOINT_BIT | PID_CSF_OUTPUT_BIT, 7));
    CHECK(pifCollectSignal_AddChannel(&s_after, "after", PIF_ID_AUTO, CSVT_WIRE, 1, 0));

    s_pid.err_sum = 3.0f;
    // A channel that is not added holds what the memory held: Init clears it.
    memset(&s_pid.__cs[PID_CSF_ERROR_IDX], 0xA5, sizeof(PifCollectSignalChannel));

    pifPidControl_Init(&s_pid, 2.0f, 0.0f, 0.0f, 0.0f);
    CHECK(s_pid.kp == 2.0f && s_pid.err_sum == 0.0f);
    CHECK(_list(list, 8) == 3);
    CHECK(list[0] == &s_pid.__cs[PID_CSF_SETPOINT_IDX] && list[1] == &s_pid.__cs[PID_CSF_OUTPUT_IDX] && list[2] == &s_after);
    CHECK(strcmp(s_pid.__cs[PID_CSF_SETPOINT_IDX]._name, "PIDSP") == 0 && s_pid.__cs[PID_CSF_SETPOINT_IDX]._id == 7);
    memset(zero, 0, sizeof(zero));
    CHECK(memcmp(&s_pid.__cs[PID_CSF_ERROR_IDX], zero, sizeof(zero)) == 0);

    // The kept channels still record and can still be removed.
    CHECK(pifPidControl_SetCsFlag(&s_pid, PIF_CS_INDEX_ALL, PID_CSF_ERROR_BIT, 7));
    CHECK(_list(list, 8) == 4 && list[3] == &s_pid.__cs[PID_CSF_ERROR_IDX]);
    pifPidControl_ResetCsFlag(&s_pid, PIF_CS_INDEX_ALL, PID_CSF_ALL_BIT);
    CHECK(_list(list, 8) == 1 && list[0] == &s_after);
}

static void _testInitOnMemoryThatWasNeverCleared(void)
{
    PifPidControl pid;
    const PifCollectSignalChannel *list[8];

    // A controller on the stack: its channels hold whatever was there, none of them is added.
    memset(&pid, 0x5A, sizeof(pid));
    pifPidControl_Init(&pid, 1.0f, 0.0f, 0.0f, 0.0f);
    CHECK(pifPidControl_SetCsFlag(&pid, PIF_CS_INDEX_ALL, PID_CSF_MEASUREMENT_BIT, PIF_ID_AUTO));
    CHECK(_list(list, 8) == 2 && list[1] == &pid.__cs[PID_CSF_MEASUREMENT_IDX]);
    pifPidControl_ResetCsFlag(&pid, PIF_CS_INDEX_ALL, PID_CSF_ALL_BIT);
    CHECK(_list(list, 8) == 1);
}

int main(void)
{
    _testInitKeepsTheAddedChannels();
    if (!s_failed) _testInitOnMemoryThatWasNeverCleared();
    printf("%s pid control: Init keeps the added channels\n", s_failed ? "FAIL" : "ok  ");
    printf("%d failed\n", s_failed ? 1 : 0);
    return s_failed ? 1 : 0;
}
