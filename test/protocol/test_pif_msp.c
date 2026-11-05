// SPDX-License-Identifier: BSD-3-Clause
// pif_msp without a transport: bytes go in with pifMsp_ParsingPacket() and the answers come out with
// pifMsp_GetAnswer(). Built with and without PIF_USE_MSP_V2.
#include "protocol/pif_msp.h"

#include <stdio.h>
#include <string.h>


// Works regardless of NDEBUG, and reports where the failure happened.
#define CHECK(COND) \
    do { \
        if (!(COND)) { \
            printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #COND); \
            s_failed = TRUE; \
            return; \
        } \
    } while (0)

static BOOL s_failed;

static PifMsp s_msp;
static int s_received;
static PifMspPacket s_packet;
static uint8_t s_payload[300];
static int s_other;


static void _evtReceive(PifMsp *p_owner, PifMspPacket *p_packet, PifIssuerP p_issuer)
{
    (void)p_owner;
    (void)p_issuer;
    s_received++;
    s_packet = *p_packet;
    pifMsp_ReadData(p_packet, s_payload, p_packet->data_count);
}

static void _evtOther(PifMsp *p_owner, uint8_t data, PifIssuerP p_issuer)
{
    (void)p_owner;
    (void)data;
    (void)p_issuer;
    s_other++;
}

static void _feed(const uint8_t *p_data, uint16_t size)
{
    uint16_t i;

    for (i = 0; i < size; i++) pifMsp_ParsingPacket(&s_msp, p_data[i]);
}

// Takes every queued answer byte into p_out. Returns the count.
static uint16_t _takeAnswer(uint8_t *p_out)
{
    uint16_t total = 0, length;
    uint8_t *p_data;

    while ((length = pifMsp_GetAnswer(&s_msp, &p_data)) > 0) {
        memcpy(p_out + total, p_data, length);
        total += length;
        pifMsp_RemoveAnswer(&s_msp, length);
    }
    return total;
}

static void _setUp(void)
{
    pifMsp_Clear(&s_msp);
    CHECK(pifMsp_Init(&s_msp, NULL, PIF_ID_AUTO));
    pifMsp_AttachEvtReceive(&s_msp, _evtReceive, _evtOther, NULL);
    s_received = 0;
    s_other = 0;
}

// MSPv1 question and answer, the same with or without PIF_USE_MSP_V2.
static void _testV1(void)
{
    uint8_t question[] = { '$', 'M', '<', 2, 100, 0x11, 0x22, 0 };
    uint8_t answer[32];
    uint16_t size;

    _setUp();
    question[7] = pifCheckXor(question + 3, 4);
    _feed(question, sizeof(question));
    CHECK(s_received == 1);
    CHECK(s_packet.version == MV_V1);
    CHECK(s_packet.command == 100);
    CHECK(s_packet.data_count == 2);
    CHECK(s_payload[0] == 0x11 && s_payload[1] == 0x22);

    CHECK(pifMsp_MakeAnswer(&s_msp, &s_packet));
    CHECK(pifMsp_AddAnswer8(&s_msp, 0xA5));
    CHECK(pifMsp_AddAnswer16(&s_msp, 0x1234));
    CHECK(pifMsp_SendAnswer(&s_msp));
    size = _takeAnswer(answer);
    CHECK(size == 9);
    CHECK(answer[0] == '$' && answer[1] == 'M' && answer[2] == '>');
    CHECK(answer[3] == 3 && answer[4] == 100);
    CHECK(answer[5] == 0xA5 && answer[6] == 0x34 && answer[7] == 0x12);
    CHECK(answer[8] == pifCheckXor(answer + 3, 5));

    // Bad checksum: dropped.
    question[7] ^= 1;
    _feed(question, sizeof(question));
    CHECK(s_received == 1);

    // An MSPv1 payload over 255 bytes cannot be sent, even when the answer buffer holds it.
    {
        static uint8_t tx_buffer[400];
        static uint8_t big[256];

        CHECK(pifMsp_AssignAnswerBuffer(&s_msp, sizeof(tx_buffer), tx_buffer));
        CHECK(pifMsp_MakeAnswer(&s_msp, &s_packet));
        CHECK(pifMsp_AddAnswer(&s_msp, big, sizeof(big)));
        CHECK(!pifMsp_SendAnswer(&s_msp));
        CHECK(_takeAnswer(answer) == 0);
    }
}

#ifdef PIF_USE_MSP_V2

static uint16_t _makeV2(uint8_t *p_frame, uint8_t type, uint8_t flags, uint16_t command, const uint8_t *p_payload,
        uint16_t size)
{
    p_frame[0] = '$';
    p_frame[1] = 'X';
    p_frame[2] = type;
    p_frame[3] = flags;
    p_frame[4] = command & 0xFF;
    p_frame[5] = command >> 8;
    p_frame[6] = size & 0xFF;
    p_frame[7] = size >> 8;
    if (size) memcpy(p_frame + 8, p_payload, size);
    p_frame[8 + size] = pifCrc8(p_frame + 3, 5 + size, PIF_CRC8_POLY_DVB_S2);
    return 9 + size;
}

static void _testV2(void)
{
    uint8_t frame[64], answer[64];
    uint8_t payload[3] = { 1, 2, 3 };
    uint16_t size;

    _setUp();
    // Check value of CRC-8/DVB-S2, so that the frames below are not checked only against themselves.
    CHECK(pifCrc8((const uint8_t *)"123456789", 9, PIF_CRC8_POLY_DVB_S2) == 0xBC);

    size = _makeV2(frame, '<', 0x00, 0x3001, payload, 3);
    _feed(frame, size);
    CHECK(s_received == 1);
    CHECK(s_packet.version == MV_V2_NATIVE);
    CHECK(s_packet.command == 0x3001);
    CHECK(s_packet.data_count == 3);
    CHECK(s_packet.flags == 0);
    CHECK(memcmp(s_payload, payload, 3) == 0);

    CHECK(pifMsp_MakeAnswer(&s_msp, &s_packet));
    CHECK(pifMsp_AddAnswer32(&s_msp, 0x04030201));
    CHECK(pifMsp_SendAnswer(&s_msp));
    size = _takeAnswer(answer);
    CHECK(size == 13);
    CHECK(answer[0] == '$' && answer[1] == 'X' && answer[2] == '>');
    CHECK(answer[3] == 0 && answer[4] == 0x01 && answer[5] == 0x30);
    CHECK(answer[6] == 4 && answer[7] == 0);
    CHECK(answer[8] == 1 && answer[11] == 4);
    CHECK(answer[12] == pifCrc8(answer + 3, 9, PIF_CRC8_POLY_DVB_S2));

    // Error answer in MSPv2.
    CHECK(pifMsp_MakeError(&s_msp, &s_packet));
    CHECK(pifMsp_SendAnswer(&s_msp));
    size = _takeAnswer(answer);
    CHECK(size == 9);
    CHECK(answer[1] == 'X' && answer[2] == '!');
    CHECK(answer[8] == pifCrc8(answer + 3, 5, PIF_CRC8_POLY_DVB_S2));

    // Bad CRC: dropped, and the next good frame still goes through.
    size = _makeV2(frame, '<', 0, 0x3002, payload, 3);
    frame[size - 1] ^= 0xFF;
    _feed(frame, size);
    CHECK(s_received == 1);
    size = _makeV2(frame, '<', 0, 0x3002, NULL, 0);
    _feed(frame, size);
    CHECK(s_received == 2);
    CHECK(s_packet.command == 0x3002 && s_packet.data_count == 0);

    // Payload too big for the receive buffer: dropped.
    frame[0] = '$'; frame[1] = 'X'; frame[2] = '<'; frame[3] = 0; frame[4] = 1; frame[5] = 0;
    frame[6] = PIF_MSP_RX_PACKET_SIZE - 5; frame[7] = 0;	// 1 byte over
    _feed(frame, 8);
    CHECK(s_msp.__rx.state == MRS_IDLE);
    // The largest payload that fits is received.
    {
        static uint8_t big_frame[PIF_MSP_RX_PACKET_SIZE + 3];
        static uint8_t big_payload[PIF_MSP_RX_PACKET_SIZE - 6];

        memset(big_payload, 0x5A, sizeof(big_payload));
        size = _makeV2(big_frame, '<', 0, 0x3003, big_payload, sizeof(big_payload));
        _feed(big_frame, size);
        CHECK(s_received == 3);
        CHECK(s_packet.data_count == sizeof(big_payload));
    }

    // MSPv1 and MSPv2 mixed with other bytes on one port.
    {
        uint8_t v1[] = { '$', 'M', '<', 0, 5, 5 };

        s_other = 0;
        _feed((const uint8_t *)"ab", 2);
        _feed(v1, sizeof(v1));
        size = _makeV2(frame, '<', 0, 7, NULL, 0);
        _feed(frame, size);
        CHECK(s_other == 2);
        CHECK(s_received == 5);
        CHECK(s_packet.version == MV_V2_NATIVE && s_packet.command == 7);
    }

    // Answers wrap around the end of the ring buffer.
    {
        int round;

        for (round = 0; round < 20; round++) {
            CHECK(pifMsp_MakeAnswer(&s_msp, &s_packet));
            CHECK(pifMsp_AddAnswer(&s_msp, payload, 3));
            CHECK(pifMsp_AddAnswer8(&s_msp, (uint8_t)round));
            CHECK(pifMsp_SendAnswer(&s_msp));
            size = _takeAnswer(answer);
            CHECK(size == 13);
            CHECK(answer[11] == (uint8_t)round);
            CHECK(answer[12] == pifCrc8(answer + 3, 9, PIF_CRC8_POLY_DVB_S2));
        }
    }
}

#else

static void _testV2Off(void)
{
    uint8_t frame[] = { '$', 'X', '<', 0, 1, 0, 0, 0, 0 };

    _setUp();
    frame[8] = pifCrc8(frame + 3, 5, PIF_CRC8_POLY_DVB_S2);
    _feed(frame, sizeof(frame));
    CHECK(s_received == 0);
}

#endif

int main(void)
{
    s_failed = FALSE;

    _testV1();
#ifdef PIF_USE_MSP_V2
    _testV2();
    printf("test_pif_msp (MSPv2): %s\n", s_failed ? "FAIL" : "OK");
#else
    _testV2Off();
    printf("test_pif_msp: %s\n", s_failed ? "FAIL" : "OK");
#endif
    pifMsp_Clear(&s_msp);
    return s_failed ? 1 : 0;
}
