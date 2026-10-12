// SPDX-License-Identifier: BSD-3-Clause
// Runs the Modbus RTU and ASCII masters against the slaves over in-memory wires. The UART tasks are
// not run: the client callbacks the masters and slaves attach to their UARTs are called directly,
// and the clock is driven by the test.
#include "core/pif_task_manager.h"
#include "protocol/pif_modbus_ascii_master.h"
#include "protocol/pif_modbus_ascii_slave.h"
#include "protocol/pif_modbus_rtu_master.h"
#include "protocol/pif_modbus_rtu_slave.h"

#include <stdio.h>


#define WIRE_SIZE		2048
#define SLAVE_ADDR		7
#define OTHER_ADDR		9
#define REG_COUNT		300
#define COIL_COUNT		64

// Works regardless of NDEBUG, and reports where the failure happened.
#define CHECK(COND) \
    do { \
        if (!(COND)) { \
            printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #COND); \
            s_failed = TRUE; \
            return; \
        } \
    } while (0)

typedef struct
{
    uint8_t data[WIRE_SIZE];
    uint16_t head, tail;
} Wire;

static BOOL s_failed;
static uint32_t s_now;

static PifUart s_uart_m, s_uart_s;
static Wire s_wire_ms, s_wire_sm;		// master to slave, slave to master
static BOOL s_mute_slave;				// Whatever the slave sends is lost

static PifModbusRtuMaster s_rtu_m;
static PifModbusRtuSlave s_rtu_s;
static PifModbusAsciiMaster s_ascii_m;
static PifModbusAsciiSlave s_ascii_s;

static uint16_t s_holding[REG_COUNT];
static uint16_t s_input[REG_COUNT];
static PifModbusBitField s_coils[COIL_COUNT / 8];
static PifModbusBitField s_inputs[COIL_COUNT / 8];
static int s_rw_calls;


static uint32_t _timer1us(void)
{
    return s_now;
}

static void _put(Wire *p_wire, const uint8_t *p_data, uint16_t size)
{
    uint16_t i;

    for (i = 0; i < size; i++) p_wire->data[p_wire->tail++ % WIRE_SIZE] = p_data[i];
}

static uint16_t _get(Wire *p_wire, uint8_t *p_data, uint16_t size)
{
    uint16_t i;

    for (i = 0; i < size && p_wire->head != p_wire->tail; i++) p_data[i] = p_wire->data[p_wire->head++ % WIRE_SIZE];
    return i;
}

static uint16_t _count(Wire *p_wire)
{
    return p_wire->tail - p_wire->head;
}

static uint16_t _sendM(PifUart *p_uart, uint8_t *p_data, uint16_t size)
{
    (void)p_uart;
    _put(&s_wire_ms, p_data, size);
    return size;
}

static uint16_t _sendS(PifUart *p_uart, uint8_t *p_data, uint16_t size)
{
    (void)p_uart;
    if (!s_mute_slave) _put(&s_wire_sm, p_data, size);
    return size;
}

static uint16_t _receiveM(PifUart *p_uart, uint8_t *p_data, uint16_t size)
{
    (void)p_uart;
    return _get(&s_wire_sm, p_data, size);
}

static uint16_t _receiveS(PifUart *p_uart, uint8_t *p_data, uint16_t size)
{
    (void)p_uart;
    return _get(&s_wire_ms, p_data, size);
}

static void _sendingM(void)
{
    (*s_uart_m.__evt_sending)(s_uart_m.__p_client, _sendM);
}

static void _sendingS(void)
{
    (*s_uart_s.__evt_sending)(s_uart_s.__p_client, _sendS);
}

static void _parsingM(void)
{
    (*s_uart_m.__evt_parsing)(s_uart_m.__p_client, _receiveM);
}

static void _parsingS(void)
{
    (*s_uart_s.__evt_parsing)(s_uart_s.__p_client, _receiveS);
}


static BOOL _evtReadWrite(uint16_t read_address, uint16_t read_quantity, uint16_t write_address, uint16_t write_quantity)
{
    (void)read_address; (void)read_quantity; (void)write_address; (void)write_quantity;
    s_rw_calls++;
    return TRUE;
}

static BOOL _initUart(PifUart *p_uart)
{
    if (!pifUart_Init(p_uart, PIF_ID_AUTO, 115200)) return FALSE;
    if (!pifUart_AttachRxTask(p_uart, PIF_ID_AUTO, TM_EXTERNAL, 0, NULL)) return FALSE;
    if (!pifUart_AttachTxTask(p_uart, PIF_ID_AUTO, TM_EXTERNAL, 0, NULL)) return FALSE;
    return TRUE;
}

static void _attachData(PifModbusSlave *p_slave)
{
    int i;

    for (i = 0; i < REG_COUNT; i++) {
        s_holding[i] = 1000 + i;
        s_input[i] = 2000 + i;
    }
    for (i = 0; i < COIL_COUNT / 8; i++) {
        s_coils[i] = 0xA5 ^ i;
        s_inputs[i] = 0x3C ^ i;
    }
    s_rw_calls = 0;
    pifModbusSlave_AttachDiscreteInput(p_slave, s_inputs, COIL_COUNT, NULL);
    pifModbusSlave_AttachCoils(p_slave, s_coils, COIL_COUNT, NULL, NULL, NULL);
    pifModbusSlave_AttachInputRegisters(p_slave, s_input, REG_COUNT, NULL);
    pifModbusSlave_AttachHoldingRegisters(p_slave, s_holding, REG_COUNT, NULL, NULL, NULL);
    pifModbusSlave_AttachEvtReadWriteMultipleRegisters(p_slave, _evtReadWrite);
}

static void _resetWires(void)
{
    memset(&s_wire_ms, 0, sizeof(s_wire_ms));
    memset(&s_wire_sm, 0, sizeof(s_wire_sm));
    s_mute_slave = FALSE;
}

static BOOL _setUpRtu(void)
{
    pifModbusRtuMaster_Clear(&s_rtu_m);
    pifModbusRtuSlave_Clear(&s_rtu_s);
    _resetWires();
    if (!pifModbusRtuMaster_Init(&s_rtu_m, PIF_ID_AUTO)) return FALSE;
    if (!pifModbusRtuSlave_Init(&s_rtu_s, PIF_ID_AUTO, SLAVE_ADDR)) return FALSE;
    pifModbusRtuMaster_AttachUart(&s_rtu_m, &s_uart_m);
    pifModbusRtuSlave_AttachUart(&s_rtu_s, &s_uart_s);
    _attachData(&s_rtu_s.parent);
    return TRUE;
}

static BOOL _setUpAscii(void)
{
    pifModbusAsciiMaster_Clear(&s_ascii_m);
    pifModbusAsciiSlave_Clear(&s_ascii_s);
    _resetWires();
    if (!pifModbusAsciiMaster_Init(&s_ascii_m, PIF_ID_AUTO)) return FALSE;
    if (!pifModbusAsciiSlave_Init(&s_ascii_s, PIF_ID_AUTO, SLAVE_ADDR)) return FALSE;
    pifModbusAsciiMaster_AttachUart(&s_ascii_m, &s_uart_m);
    pifModbusAsciiSlave_AttachUart(&s_ascii_s, &s_uart_s);
    _attachData(&s_ascii_s.parent);
    return TRUE;
}

// Runs master, slave and clock until the master reports something other than MBMR_BUSY. Each step is
// longer than 3.5 characters, which is what the callbacks see between two UART task runs.
static PifModbusMasterResult _pump(PifModbusMasterResult (*check)(void *), void *p_master)
{
    PifModbusMasterResult result;
    int i;

    for (i = 0; i < 1000; i++) {
        _sendingM();
        _parsingS();
        _sendingS();
        _parsingM();
        s_now += 1000;
        result = (*check)(p_master);
        if (result != MBMR_BUSY) return result;
    }
    return MBMR_BUSY;
}

static PifModbusMasterResult _checkRtu(void *p_master)
{
    return pifModbusRtuMaster_Check((PifModbusRtuMaster *)p_master);
}

static PifModbusMasterResult _checkAscii(void *p_master)
{
    return pifModbusAsciiMaster_Check((PifModbusAsciiMaster *)p_master);
}

// Lets the slave alone run for a while, as its UART tasks would with nothing new on the line.
static void _idleSlave(int steps)
{
    while (steps--) {
        _parsingS();
        _sendingS();
        s_now += 1000;
    }
}

static void _putRtuFrame(Wire *p_wire, uint8_t *p_frame, uint16_t length)
{
    pifModbus_ShortToStream(pifModbusRtu_CalcCrc(p_frame, length), &p_frame[length]);
    _put(p_wire, p_frame, length + 2);
}


static void _testRtuReadHoldingRegisters(void)
{
    uint16_t regs[4] = {0};

    CHECK(_setUpRtu());
    // Above 255 and up to the last register, which needs the 16-bit address and no off-by-one.
    CHECK(pifModbusRtuMaster_ReadHoldingRegisters(&s_rtu_m, SLAVE_ADDR, REG_COUNT - 4, 4, regs));
    CHECK(_pump(_checkRtu, &s_rtu_m) == MBMR_DONE);
    CHECK(regs[0] == 1000 + REG_COUNT - 4 && regs[3] == 1000 + REG_COUNT - 1);
}

static void _testRtuException(void)
{
    uint16_t regs[4] = {0x1234, 0x1234, 0x1234, 0x1234};

    CHECK(_setUpRtu());
    CHECK(pifModbusRtuMaster_ReadHoldingRegisters(&s_rtu_m, SLAVE_ADDR, REG_COUNT - 3, 4, regs));
    CHECK(_pump(_checkRtu, &s_rtu_m) == MBMR_ERROR);
    CHECK(s_rtu_m._error == MBE_ILLEGAL_DATA_ADDRESS);
    CHECK(regs[0] == 0x1234);		// Nothing is copied out of an exception

    // An address past the last coil.
    CHECK(pifModbusRtuMaster_WriteSingleCoil(&s_rtu_m, SLAVE_ADDR, COIL_COUNT, TRUE));
    CHECK(_pump(_checkRtu, &s_rtu_m) == MBMR_ERROR);
    CHECK(s_rtu_m._error == MBE_ILLEGAL_DATA_ADDRESS);
}

static void _testRtuWrites(void)
{
    uint16_t regs[3] = {11, 22, 33}, read[2] = {0};
    PifModbusBitField coils[2] = {0x0F, 0x01}, bits[2] = {0};

    CHECK(_setUpRtu());
    // The response to a single coil has to echo 0xFF00, which the master checks.
    CHECK(pifModbusRtuMaster_WriteSingleCoil(&s_rtu_m, SLAVE_ADDR, 1, FALSE));
    CHECK(_pump(_checkRtu, &s_rtu_m) == MBMR_DONE);
    CHECK(pifModbusRtuMaster_WriteSingleCoil(&s_rtu_m, SLAVE_ADDR, 1, TRUE));
    CHECK(_pump(_checkRtu, &s_rtu_m) == MBMR_DONE);
    CHECK(PIF_MODBUS_READ_BIT_FIELD(s_coils, 1));

    CHECK(pifModbusRtuMaster_WriteSingleRegister(&s_rtu_m, SLAVE_ADDR, 260, 0xBEEF));
    CHECK(_pump(_checkRtu, &s_rtu_m) == MBMR_DONE);
    CHECK(s_holding[260] == 0xBEEF);

    CHECK(pifModbusRtuMaster_WriteMultipleRegisters(&s_rtu_m, SLAVE_ADDR, REG_COUNT - 3, 3, regs));
    CHECK(_pump(_checkRtu, &s_rtu_m) == MBMR_DONE);
    CHECK(s_holding[REG_COUNT - 3] == 11 && s_holding[REG_COUNT - 1] == 33);

    CHECK(pifModbusRtuMaster_WriteMultipleCoils(&s_rtu_m, SLAVE_ADDR, 8, 9, coils));
    CHECK(_pump(_checkRtu, &s_rtu_m) == MBMR_DONE);
    CHECK(s_coils[1] == 0x0F && PIF_MODBUS_READ_BIT_FIELD(s_coils, 16));

    CHECK(pifModbusRtuMaster_ReadCoils(&s_rtu_m, SLAVE_ADDR, 8, 9, bits));
    CHECK(_pump(_checkRtu, &s_rtu_m) == MBMR_DONE);
    CHECK(bits[0] == 0x0F && (bits[1] & 1));

    // The write goes before the read, and the callback is reached.
    CHECK(pifModbusRtuMaster_ReadWriteMultipleRegisters(&s_rtu_m, SLAVE_ADDR, 10, 2, read, 11, 1, regs));
    CHECK(_pump(_checkRtu, &s_rtu_m) == MBMR_DONE);
    CHECK(read[0] == 1010 && read[1] == 11 && s_rw_calls == 1);
}

static void _testRtuOtherSlaveTraffic(void)
{
    uint8_t frame[16];
    uint16_t length;
    uint8_t response[32];

    CHECK(_setUpRtu());
    // A request to another slave whose data bytes include this slave's address, its response, and
    // then a request to this slave, all read in one go.
    frame[0] = OTHER_ADDR; frame[1] = MBF_WRITE_MULTIPLE_REGISTERS;
    frame[2] = 0; frame[3] = 0; frame[4] = 0; frame[5] = 1; frame[6] = 2;
    frame[7] = SLAVE_ADDR; frame[8] = MBF_READ_HOLDING_REGISTERS;
    _putRtuFrame(&s_wire_ms, frame, 9);
    frame[0] = OTHER_ADDR; frame[1] = MBF_READ_HOLDING_REGISTERS; frame[2] = 2; frame[3] = SLAVE_ADDR; frame[4] = 3;
    _putRtuFrame(&s_wire_ms, frame, 5);
    frame[0] = SLAVE_ADDR; frame[1] = MBF_READ_INPUT_REGISTERS;
    frame[2] = 0; frame[3] = 5; frame[4] = 0; frame[5] = 1;
    _putRtuFrame(&s_wire_ms, frame, 6);

    _idleSlave(20);
    length = _get(&s_wire_sm, response, sizeof(response));
    CHECK(length == 7);
    CHECK(response[0] == SLAVE_ADDR && response[1] == MBF_READ_INPUT_REGISTERS && response[2] == 2);
    CHECK(pifModbus_StreamToShort(&response[3]) == 2005);
}

static void _testRtuDamagedFrames(void)
{
    uint8_t frame[300];
    uint8_t response[32];
    uint16_t length;

    CHECK(_setUpRtu());
    // A CRC error is not answered.
    frame[0] = SLAVE_ADDR; frame[1] = MBF_READ_HOLDING_REGISTERS;
    frame[2] = 0; frame[3] = 0; frame[4] = 0; frame[5] = 1;
    pifModbus_ShortToStream(pifModbusRtu_CalcCrc(frame, 6) ^ 1, &frame[6]);
    _put(&s_wire_ms, frame, 8);
    _idleSlave(20);
    CHECK(_count(&s_wire_sm) == 0);

    // A byte count longer than the buffer is not taken in, and not answered.
    frame[0] = SLAVE_ADDR; frame[1] = MBF_WRITE_MULTIPLE_REGISTERS;
    frame[2] = 0; frame[3] = 0; frame[4] = 0; frame[5] = 127; frame[6] = 254;
    memset(&frame[7], 0x55, 256);
    _put(&s_wire_ms, frame, 7 + 256);
    _idleSlave(20);
    CHECK(_count(&s_wire_sm) == 0);
    CHECK(s_holding[0] == 1000);

    // A quantity over the limit is answered with exception 3, and nothing past the buffer is sent.
    frame[0] = SLAVE_ADDR; frame[1] = MBF_READ_HOLDING_REGISTERS;
    frame[2] = 0; frame[3] = 0; frame[4] = 0; frame[5] = 200;
    _putRtuFrame(&s_wire_ms, frame, 6);
    _idleSlave(20);
    length = _get(&s_wire_sm, response, sizeof(response));
    CHECK(length == 5 && response[1] == (0x80 | MBF_READ_HOLDING_REGISTERS) && response[2] == MBE_ILLEGAL_DATA_VALUE);

    // A function the slave lacks is answered with exception 1 once the frame is over.
    frame[0] = SLAVE_ADDR; frame[1] = 8;
    frame[2] = 0; frame[3] = 0; frame[4] = 0x12; frame[5] = 0x34;
    _putRtuFrame(&s_wire_ms, frame, 6);
    _idleSlave(20);
    length = _get(&s_wire_sm, response, sizeof(response));
    CHECK(length == 5 && response[1] == 0x88 && response[2] == MBE_ILLEGAL_FUNCTION);

    // And the slave still answers the next request.
    frame[0] = SLAVE_ADDR; frame[1] = MBF_READ_HOLDING_REGISTERS;
    frame[2] = 0; frame[3] = 1; frame[4] = 0; frame[5] = 1;
    _putRtuFrame(&s_wire_ms, frame, 6);
    _idleSlave(20);
    length = _get(&s_wire_sm, response, sizeof(response));
    CHECK(length == 7 && pifModbus_StreamToShort(&response[3]) == 1001);
}

static void _testRtuBroadcast(void)
{
    uint16_t regs[1];

    CHECK(_setUpRtu());
    CHECK(!pifModbusRtuMaster_ReadHoldingRegisters(&s_rtu_m, PIF_MODBUS_BROADCAST_ADDRESS, 0, 1, regs));
    CHECK(pifModbusRtuMaster_WriteSingleRegister(&s_rtu_m, PIF_MODBUS_BROADCAST_ADDRESS, 3, 77));
    CHECK(_pump(_checkRtu, &s_rtu_m) == MBMR_DONE);
    _idleSlave(5);
    CHECK(s_holding[3] == 77);
    CHECK(_count(&s_wire_sm) == 0);
}

static void _testRtuMasterRecovery(void)
{
    uint8_t noise[3] = {SLAVE_ADDR, MBF_READ_HOLDING_REGISTERS, 2};
    uint16_t regs[1] = {0};

    CHECK(_setUpRtu());
    // Noise while nothing is awaited is dropped and leaves the master idle.
    _put(&s_wire_sm, noise, sizeof(noise));
    _parsingM();
    CHECK(pifModbusRtuMaster_Check(&s_rtu_m) == MBMR_IDLE);

    // A response cut short by the timeout does not run into the next one.
    CHECK(pifModbusRtuMaster_ReadHoldingRegisters(&s_rtu_m, SLAVE_ADDR, 0, 1, regs));
    _sendingM(); _sendingM(); _sendingM();
    _put(&s_wire_sm, noise, sizeof(noise));
    _parsingM();
    s_now += PIF_MODBUS_MASTER_TIMEOUT * 1000UL;
    CHECK(pifModbusRtuMaster_Check(&s_rtu_m) == MBMR_ERROR && s_rtu_m._error == MBE_TIMEOUT);
    _resetWires();
    CHECK(pifModbusRtuMaster_ReadHoldingRegisters(&s_rtu_m, SLAVE_ADDR, 2, 1, regs));
    CHECK(_pump(_checkRtu, &s_rtu_m) == MBMR_DONE);
    CHECK(regs[0] == 1002);
}

static void _testMasterTimeout(void)
{
    uint16_t regs[1] = {0};
    uint8_t frame[8];
    int i;

    CHECK(_setUpRtu());
    s_mute_slave = TRUE;
    CHECK(pifModbusRtuMaster_ReadHoldingRegisters(&s_rtu_m, SLAVE_ADDR, 0, 1, regs));
    // The wait starts once the request is out, and nothing but the clock ends it.
    for (i = 0; i < PIF_MODBUS_MASTER_TIMEOUT - 10; i++) {
        _sendingM(); _parsingS(); _sendingS(); _parsingM();
        s_now += 1000;
        CHECK(pifModbusRtuMaster_Check(&s_rtu_m) == MBMR_BUSY);
    }
    s_now += 20000;
    CHECK(pifModbusRtuMaster_Check(&s_rtu_m) == MBMR_ERROR && s_rtu_m._error == MBE_TIMEOUT);

    // A response that turns up after the timeout is not taken, even before Check() runs.
    _resetWires();
    CHECK(pifModbusRtuMaster_ReadHoldingRegisters(&s_rtu_m, SLAVE_ADDR, 0, 1, regs));
    _sendingM(); _sendingM(); _sendingM();
    s_now += (PIF_MODBUS_MASTER_TIMEOUT + 1) * 1000UL;
    frame[0] = SLAVE_ADDR; frame[1] = MBF_READ_HOLDING_REGISTERS; frame[2] = 2; frame[3] = 0x12; frame[4] = 0x34;
    _putRtuFrame(&s_wire_sm, frame, 5);
    _parsingM();
    CHECK(pifModbusRtuMaster_Check(&s_rtu_m) == MBMR_ERROR && s_rtu_m._error == MBE_TIMEOUT);
    CHECK(regs[0] == 0);

    // A response whose bytes come slowly, each within the timeout, is taken however long it takes.
    _resetWires();
    CHECK(pifModbusRtuMaster_ReadHoldingRegisters(&s_rtu_m, SLAVE_ADDR, 0, 1, regs));
    _sendingM(); _sendingM(); _sendingM();
    pifModbus_ShortToStream(pifModbusRtu_CalcCrc(frame, 5), &frame[5]);
    for (i = 0; i < 7; i++) {
        s_now += (PIF_MODBUS_MASTER_TIMEOUT - 100) * 1000UL;
        _put(&s_wire_sm, &frame[i], 1);
        _parsingM();
        CHECK(pifModbusRtuMaster_Check(&s_rtu_m) == MBMR_BUSY);
    }
    s_now += 1000;
    CHECK(pifModbusRtuMaster_Check(&s_rtu_m) == MBMR_DONE && regs[0] == 0x1234);

    // The same holds for the ASCII master.
    CHECK(_setUpAscii());
    s_mute_slave = TRUE;
    CHECK(pifModbusAsciiMaster_ReadHoldingRegisters(&s_ascii_m, SLAVE_ADDR, 0, 1, regs));
    CHECK(_pump(_checkAscii, &s_ascii_m) == MBMR_ERROR && s_ascii_m._error == MBE_TIMEOUT);
}

static void _testSlaveTimeout(void)
{
    uint8_t frame[8], response[32];
    const char *p_ascii = ":070300020001F3\r\n";
    uint16_t length, i;

    CHECK(_setUpRtu());
    // Bytes that come slowly, each within the timeout, still make one frame.
    frame[0] = SLAVE_ADDR; frame[1] = MBF_READ_HOLDING_REGISTERS;
    frame[2] = 0; frame[3] = 4; frame[4] = 0; frame[5] = 1;
    pifModbus_ShortToStream(pifModbusRtu_CalcCrc(frame, 6), &frame[6]);
    for (i = 0; i < 8; i++) {
        _put(&s_wire_ms, &frame[i], 1);
        _parsingS();
        s_now += (PIF_MODBUS_SLAVE_TIMEOUT - 100) * 1000UL;
    }
    _idleSlave(5);
    length = _get(&s_wire_sm, response, sizeof(response));
    CHECK(length == 7 && pifModbus_StreamToShort(&response[3]) == 1004);

    // A frame that stops for longer is dropped, and the next one is taken from its start.
    _put(&s_wire_ms, frame, 3);
    _parsingS();
    s_now += (PIF_MODBUS_SLAVE_TIMEOUT + 1) * 1000UL;
    _put(&s_wire_ms, frame, 8);
    _idleSlave(5);
    length = _get(&s_wire_sm, response, sizeof(response));
    CHECK(length == 7 && pifModbus_StreamToShort(&response[3]) == 1004);

    CHECK(_setUpAscii());
    for (i = 0; i < strlen(p_ascii); i++) {
        _put(&s_wire_ms, (const uint8_t *)&p_ascii[i], 1);
        _parsingS();
        s_now += (PIF_MODBUS_SLAVE_TIMEOUT - 100) * 1000UL;
    }
    _idleSlave(5);
    length = _get(&s_wire_sm, response, sizeof(response));
    CHECK(length == 15 && !memcmp(response, ":07030203EA", 11));

    _put(&s_wire_ms, (const uint8_t *)p_ascii, 8);
    _parsingS();
    s_now += (PIF_MODBUS_SLAVE_TIMEOUT + 1) * 1000UL;
    _put(&s_wire_ms, (const uint8_t *)p_ascii + 8, strlen(p_ascii) - 8);
    _idleSlave(5);
    CHECK(_count(&s_wire_sm) == 0);
}

static void _testAsciiLrc(void)
{
    uint16_t regs[1];
    uint8_t frame[32];
    uint16_t length;

    CHECK(_setUpAscii());
    // The LRC of 01 03 00 00 00 01 is FB, from the bytes and not the hex digits.
    CHECK(pifModbusAsciiMaster_ReadHoldingRegisters(&s_ascii_m, 1, 0, 1, regs));
    _sendingM();
    length = _get(&s_wire_ms, frame, sizeof(frame));
    CHECK(length == 17 && !memcmp(frame, ":010300000001FB\r\n", 17));
}

static void _testAsciiRoundTrip(void)
{
    uint16_t regs[3] = {0}, values[2] = {5, 6};
    PifModbusBitField bits[1] = {0};

    CHECK(_setUpAscii());
    CHECK(pifModbusAsciiMaster_ReadHoldingRegisters(&s_ascii_m, SLAVE_ADDR, 297, 3, regs));
    CHECK(_pump(_checkAscii, &s_ascii_m) == MBMR_DONE);
    CHECK(regs[0] == 1297 && regs[2] == 1299);

    CHECK(pifModbusAsciiMaster_WriteSingleCoil(&s_ascii_m, SLAVE_ADDR, 1, TRUE));
    CHECK(_pump(_checkAscii, &s_ascii_m) == MBMR_DONE);
    CHECK(pifModbusAsciiMaster_ReadCoils(&s_ascii_m, SLAVE_ADDR, 0, 8, bits));
    CHECK(_pump(_checkAscii, &s_ascii_m) == MBMR_DONE);
    CHECK(bits[0] == (0xA5 | 0x02));

    CHECK(pifModbusAsciiMaster_WriteMultipleRegisters(&s_ascii_m, SLAVE_ADDR, 100, 2, values));
    CHECK(_pump(_checkAscii, &s_ascii_m) == MBMR_DONE);
    CHECK(s_holding[100] == 5 && s_holding[101] == 6);

    CHECK(pifModbusAsciiMaster_ReadInputRegisters(&s_ascii_m, SLAVE_ADDR, REG_COUNT, 1, regs));
    CHECK(_pump(_checkAscii, &s_ascii_m) == MBMR_ERROR);
    CHECK(s_ascii_m._error == MBE_ILLEGAL_DATA_ADDRESS);

    CHECK(pifModbusAsciiMaster_WriteSingleRegister(&s_ascii_m, PIF_MODBUS_BROADCAST_ADDRESS, 4, 44));
    CHECK(_pump(_checkAscii, &s_ascii_m) == MBMR_DONE);
    _idleSlave(5);
    CHECK(s_holding[4] == 44 && _count(&s_wire_sm) == 0);
}

static void _testAsciiDamagedFrames(void)
{
    uint8_t response[64];
    uint16_t length;
    const char *p_bad_lrc = ":070300000001F0\r\n";
    const char *p_short = ":0703000000F6\r\n";		// LRC right, one byte missing
    const char *p_unknown = ":070800000001F0\r\n";
    const char *p_garbage = "xx:07GG\r\n";

    CHECK(_setUpAscii());
    _put(&s_wire_ms, (const uint8_t *)p_bad_lrc, strlen(p_bad_lrc));
    _idleSlave(5);
    CHECK(_count(&s_wire_sm) == 0);

    _put(&s_wire_ms, (const uint8_t *)p_garbage, strlen(p_garbage));
    _idleSlave(5);
    CHECK(_count(&s_wire_sm) == 0);

    _put(&s_wire_ms, (const uint8_t *)p_short, strlen(p_short));
    _idleSlave(5);
    length = _get(&s_wire_sm, response, sizeof(response));
    CHECK(length == 11 && !memcmp(response, ":078303", 7));

    _put(&s_wire_ms, (const uint8_t *)p_unknown, strlen(p_unknown));
    _idleSlave(5);
    length = _get(&s_wire_sm, response, sizeof(response));
    CHECK(length == 11 && !memcmp(response, ":078801", 7));
}

static void _testAsciiHelpers(void)
{
    uint8_t text[8];

    CHECK(pifModbusAscii_AsciiToChar((uint8_t *)"7f") == 0x7F);
    CHECK(pifModbusAscii_AsciiToChar((uint8_t *)"G1") == -1);
    CHECK(pifModbusAscii_AsciiToShort((uint8_t *)"12x4") == -1L);
    memcpy(text, "0103", 4);
    CHECK(pifModbusAscii_Decode(text, 4) == 2 && text[0] == 1 && text[1] == 3);
    CHECK(pifModbusAscii_Decode(text, 3) == -1);
}

static const struct
{
    const char *p_name;
    void (*run)(void);
} s_tests[] = {
    { "rtu read holding registers", _testRtuReadHoldingRegisters },
    { "rtu exception", _testRtuException },
    { "rtu writes", _testRtuWrites },
    { "rtu other slave traffic", _testRtuOtherSlaveTraffic },
    { "rtu damaged frames", _testRtuDamagedFrames },
    { "rtu broadcast", _testRtuBroadcast },
    { "rtu master recovery", _testRtuMasterRecovery },
    { "master timeout", _testMasterTimeout },
    { "slave timeout", _testSlaveTimeout },
    { "ascii lrc", _testAsciiLrc },
    { "ascii round trip", _testAsciiRoundTrip },
    { "ascii damaged frames", _testAsciiDamagedFrames },
    { "ascii helpers", _testAsciiHelpers },
};

int main(void)
{
    int failed = 0;
    size_t t;

    if (!pif_Init(_timer1us) || !pifTaskManager_Init(8, 2) ||
            !_initUart(&s_uart_m) || !_initUart(&s_uart_s)) {
        printf("pif_modbus: setup failed (%d)\n", pif_error);
        return 1;
    }
    // With act_send_data set and no act_get_tx_rate, a frame counts as out once it is handed over.
    s_uart_m.act_send_data = _sendM;
    s_uart_s.act_send_data = _sendS;

#ifdef PIF_NO_LOG
    printf("pif_modbus (PIF_NO_LOG)\n");
#else
    printf("pif_modbus\n");
#endif
    for (t = 0; t < sizeof(s_tests) / sizeof(s_tests[0]); t++) {
        s_failed = FALSE;
        (*s_tests[t].run)();
        printf("  %s %s\n", s_failed ? "FAIL" : "ok  ", s_tests[t].p_name);
        if (s_failed) failed++;
    }
    printf("%d failed\n", failed);
    return failed ? 1 : 0;
}
