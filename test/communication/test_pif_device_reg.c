// SPDX-License-Identifier: BSD-3-Clause
// PifDeviceReg over pif_i2c and pif_spi: the same register functions on both buses, and the background reads.
#include "communication/pif_i2c.h"
#include "communication/pif_spi.h"

#include <stdio.h>
#include <time.h>


#define REG_COUNT		256
#define LOG_SIZE		16

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
    uint32_t iaddr;
    uint8_t isize;
    size_t size;
} Call;

static BOOL s_failed;

static PifI2cPort s_i2c_port;
static PifSpiPort s_spi_port;
static PifI2cDevice *s_p_i2c;
static PifSpiDevice *s_p_spi;

// One register file behind each bus.
static uint8_t s_regs[REG_COUNT];

static Call s_reads[LOG_SIZE], s_writes[LOG_SIZE];
static int s_read_count, s_write_count;

static PifI2cReturn s_i2c_answer;		// What act_read answers for a read that does not fail.
static BOOL s_fail;						// The next transfer fails.
static int s_recover_count;

static BOOL s_spi_busy;					// What act_is_busy answers.
static int s_start_transfer_count, s_transfer_count;

static const char *s_bus_name;


static uint32_t _timer1us(void)
{
    return (uint32_t)clock() * (1000000 / CLOCKS_PER_SEC);
}

static BOOL _regRead(uint32_t iaddr, uint8_t isize, uint8_t *p_data, size_t size)
{
    if (s_read_count < LOG_SIZE) {
        s_reads[s_read_count].iaddr = iaddr;
        s_reads[s_read_count].isize = isize;
        s_reads[s_read_count].size = size;
    }
    s_read_count++;
    if (s_fail) {
        s_fail = FALSE;
        return FALSE;
    }
    if (iaddr + size > REG_COUNT) return FALSE;
    memcpy(p_data, s_regs + iaddr, size);
    return TRUE;
}

static BOOL _regWrite(uint32_t iaddr, uint8_t isize, uint8_t *p_data, size_t size)
{
    if (s_write_count < LOG_SIZE) {
        s_writes[s_write_count].iaddr = iaddr;
        s_writes[s_write_count].isize = isize;
        s_writes[s_write_count].size = size;
    }
    s_write_count++;
    if (s_fail) {
        s_fail = FALSE;
        return FALSE;
    }
    if (iaddr + size > REG_COUNT) return FALSE;
    memcpy(s_regs + iaddr, p_data, size);
    return TRUE;
}

static PifI2cReturn _i2cRead(PifI2cDevice *p_owner, uint32_t iaddr, uint8_t isize, uint8_t *p_data, size_t size)
{
    return _regRead(iaddr, isize, p_data, size) ? s_i2c_answer : IR_ERROR;
}

static PifI2cReturn _i2cWrite(PifI2cDevice *p_owner, uint32_t iaddr, uint8_t isize, uint8_t *p_data, size_t size)
{
    return _regWrite(iaddr, isize, p_data, size) ? IR_COMPLETE : IR_ERROR;
}

static void _i2cRecover(PifI2cDevice *p_owner)
{
    s_recover_count++;
}

static BOOL _spiRead(PifSpiDevice *p_owner, uint32_t iaddr, uint8_t isize, uint8_t *p_data, size_t size)
{
    return _regRead(iaddr, isize, p_data, size);
}

static BOOL _spiWrite(PifSpiDevice *p_owner, uint32_t iaddr, uint8_t isize, uint8_t *p_data, size_t size)
{
    return _regWrite(iaddr, isize, p_data, size);
}

static void _spiTransfer(PifSpiDevice *p_owner, uint8_t *p_write, uint8_t *p_read, size_t size)
{
    s_transfer_count++;
    if (p_read) memset(p_read, 0xA5, size);
}

static BOOL _spiStartTransfer(PifSpiDevice *p_owner, uint8_t *p_write, uint8_t *p_read, size_t size)
{
    s_start_transfer_count++;
    if (s_fail) {
        s_fail = FALSE;
        return FALSE;
    }
    if (p_read) memset(p_read, 0x5A, size);
    return TRUE;
}

static BOOL _spiIsBusy(PifSpiDevice *p_owner)
{
    return s_spi_busy;
}

static BOOL _setUp(void)
{
    int i;

    pifI2cPort_Clear(&s_i2c_port);
    pifSpiPort_Clear(&s_spi_port);
    if (!pifI2cPort_Init(&s_i2c_port, PIF_ID_AUTO, 2) || !pifSpiPort_Init(&s_spi_port, PIF_ID_AUTO, 2)) return FALSE;
    s_i2c_port.act_read = _i2cRead;
    s_i2c_port.act_write = _i2cWrite;
    s_i2c_port.act_recover = _i2cRecover;
    s_spi_port.act_read = _spiRead;
    s_spi_port.act_write = _spiWrite;
    s_spi_port.act_transfer = _spiTransfer;
    s_p_i2c = pifI2cPort_AddDevice(&s_i2c_port, PIF_ID_AUTO, 0x50, NULL);
    s_p_spi = pifSpiPort_AddDevice(&s_spi_port, PIF_ID_AUTO, NULL);
    if (!s_p_i2c || !s_p_spi) return FALSE;

    for (i = 0; i < REG_COUNT; i++) s_regs[i] = (uint8_t)(i * 3 + 1);
    s_read_count = s_write_count = 0;
    s_i2c_answer = IR_COMPLETE;
    s_fail = FALSE;
    s_recover_count = 0;
    s_spi_busy = FALSE;
    s_start_transfer_count = s_transfer_count = 0;
    return TRUE;
}


// The register functions, run on each bus by _testOnBothBuses().
static void _registerFunctions(PifDeviceReg *p_device)
{
    uint8_t byte, bytes[4];
    uint16_t word;

    CHECK(pifDeviceReg_ReadRegByte(p_device, 0x10, &byte) && byte == s_regs[0x10]);
    CHECK(s_read_count == 1 && s_reads[0].iaddr == 0x10 && s_reads[0].isize == 1 && s_reads[0].size == 1);

    // Words go high byte first.
    s_regs[0x20] = 0x12;
    s_regs[0x21] = 0x34;
    CHECK(pifDeviceReg_ReadRegWord(p_device, 0x20, &word) && word == 0x1234);
    CHECK(pifDeviceReg_WriteRegWord(p_device, 0x22, 0xABCD) && s_regs[0x22] == 0xAB && s_regs[0x23] == 0xCD);

    CHECK(pifDeviceReg_WriteRegByte(p_device, 0x30, 0x77) && s_regs[0x30] == 0x77);
    bytes[0] = 1; bytes[1] = 2; bytes[2] = 3; bytes[3] = 4;
    CHECK(pifDeviceReg_WriteRegBytes(p_device, 0x40, bytes, 4) && !memcmp(s_regs + 0x40, bytes, 4));
    memset(bytes, 0, sizeof(bytes));
    CHECK(pifDeviceReg_ReadRegBytes(p_device, 0x40, bytes, 4) && bytes[0] == 1 && bytes[3] == 4);

    // A field under a mask: the other bits stay, and nothing is written when it does not change.
    s_regs[0x50] = 0xF0;
    CHECK(pifDeviceReg_ReadRegBit8(p_device, 0x50, 0x30, &byte) && byte == 0x30);
    CHECK(pifDeviceReg_WriteRegBit8(p_device, 0x50, 0x0C, 0x08) && s_regs[0x50] == 0xF8);
    s_write_count = 0;
    CHECK(pifDeviceReg_WriteRegBit8(p_device, 0x50, 0x0C, 0x08) && !s_write_count);
    CHECK(!pifDeviceReg_WriteRegBit8(p_device, 0x50, 0x0C, 0x10) && pif_error == E_WRONG_DATA);

    s_regs[0x60] = 0x12;
    s_regs[0x61] = 0x34;
    CHECK(pifDeviceReg_ReadRegBit16(p_device, 0x60, 0x0FF0, &word) && word == 0x0230);
    CHECK(pifDeviceReg_WriteRegBit16(p_device, 0x60, 0x0FF0, 0x0560) && s_regs[0x60] == 0x15 && s_regs[0x61] == 0x64);

    // Longer than max_transfer_size: pieces, each from its own address.
    p_device->max_transfer_size = 4;
    s_read_count = 0;
    CHECK(pifDeviceReg_Read(p_device, 0x80, 1, bytes, 0) && !s_read_count);
    {
        uint8_t data[10];

        CHECK(pifDeviceReg_Read(p_device, 0x80, 1, data, 10) && !memcmp(data, s_regs + 0x80, 10));
        CHECK(s_read_count == 3 && s_reads[1].iaddr == 0x84 && s_reads[2].iaddr == 0x88 && s_reads[2].size == 2);
    }
    p_device->max_transfer_size = 0;

    // A failed transfer.
    s_fail = TRUE;
    CHECK(!pifDeviceReg_ReadRegByte(p_device, 0x10, &byte));
    CHECK(!pifDeviceReg_IsBusy(p_device));
}

static void _testOnBothBuses(void)
{
    CHECK(_setUp());
    s_bus_name = "i2c";
    _registerFunctions(&s_p_i2c->parent);
    CHECK(!s_failed && s_i2c_port.error_count == 1);

    CHECK(_setUp());
    s_bus_name = "spi";
    _registerFunctions(&s_p_spi->parent);
    CHECK(!s_failed && s_spi_port.error_count == 1);
}

static void _testNullDevice(void)
{
    uint8_t byte;

    CHECK(_setUp());
    pif_error = E_SUCCESS;
    CHECK(!pifDeviceReg_ReadRegByte(NULL, 0, &byte) && pif_error == E_INVALID_PARAM);
    CHECK(!pifDeviceReg_WriteRegByte(NULL, 0, 0));
    CHECK(!pifDeviceReg_StartRead(NULL, 0, 1, &byte, 1));
    CHECK(pifDeviceReg_CheckTransfer(NULL) == DRS_IDLE && !pifDeviceReg_IsBusy(NULL));
}

static void _testDefaults(void)
{
    CHECK(_setUp());
    CHECK(s_p_i2c->parent.timeout == 10 && s_p_spi->parent.timeout == 10);
    CHECK(s_p_i2c->parent._state == DRS_IDLE && s_p_spi->parent._state == DRS_IDLE);
    CHECK(s_p_i2c->addr == 0x50 && s_p_i2c->_p_port == &s_i2c_port && s_p_spi->_p_port == &s_spi_port);
}

static void _testI2cBackground(void)
{
    PifDeviceReg *p_device;
    PifI2cDevice *p_other;
    uint8_t data[4], byte;

    CHECK(_setUp());
    p_device = &s_p_i2c->parent;
    p_other = pifI2cPort_AddDevice(&s_i2c_port, PIF_ID_AUTO, 0x51, NULL);
    CHECK(p_other);
    s_i2c_answer = IR_WAIT;

    CHECK(pifDeviceReg_StartReadRegBytes(p_device, 0x10, data, 4));
    CHECK(pifDeviceReg_CheckTransfer(p_device) == DRS_RUN);
    // The port is held for every device on it.
    CHECK(pifDeviceReg_IsBusy(p_device) && pifDeviceReg_IsBusy(&p_other->parent));
    CHECK(!pifDeviceReg_ReadRegByte(&p_other->parent, 0, &byte) && pif_error == E_INVALID_STATE);
    CHECK(!pifDeviceReg_StartRead(&p_other->parent, 0, 1, &byte, 1) && pif_error == E_INVALID_STATE);
    CHECK(pifDeviceReg_CheckTransfer(&p_other->parent) == DRS_IDLE);
    CHECK(!pifI2cPort_RemoveDevice(&s_i2c_port, s_p_i2c) && pif_error == E_INVALID_STATE);

    pifI2cPort_sigEndTransfer(&s_i2c_port, TRUE);
    pifI2cPort_sigEndTransfer(&s_i2c_port, FALSE);		// A second signal changes nothing.
    CHECK(pifDeviceReg_CheckTransfer(p_device) == DRS_COMPLETE);
    CHECK(!memcmp(data, s_regs + 0x10, 4));
    CHECK(pifDeviceReg_CheckTransfer(p_device) == DRS_IDLE && !pifDeviceReg_IsBusy(p_device));

    // Larger than max_transfer_size, a background read is refused.
    p_device->max_transfer_size = 2;
    CHECK(!pifDeviceReg_StartRead(p_device, 0, 1, data, 4) && pif_error == E_INVALID_PARAM);
}

static void _testI2cTimeout(void)
{
    PifDeviceReg *p_device;
    uint8_t data[2];

    CHECK(_setUp());
    p_device = &s_p_i2c->parent;
    s_i2c_answer = IR_WAIT;

    CHECK(pifDeviceReg_StartRead(p_device, 0, 1, data, 2));
    pif_cumulative_timer1ms += p_device->timeout + 1;
    CHECK(pifDeviceReg_CheckTransfer(p_device) == DRS_ERROR && pif_error == E_TIMEOUT);
    CHECK(s_recover_count == 1 && s_i2c_port.error_count == 1 && !pifDeviceReg_IsBusy(p_device));
}

static void _testSpiBackgroundWithoutStartRead(void)
{
    PifDeviceReg *p_device;
    uint8_t data[3];

    CHECK(_setUp());
    p_device = &s_p_spi->parent;

    // The read is made at once, and still reported once and held until then.
    CHECK(pifDeviceReg_StartRead(p_device, 0x30, 1, data, 3));
    CHECK(!memcmp(data, s_regs + 0x30, 3));
    CHECK(pifDeviceReg_IsBusy(p_device));
    CHECK(pifDeviceReg_CheckTransfer(p_device) == DRS_COMPLETE);
    CHECK(pifDeviceReg_CheckTransfer(p_device) == DRS_IDLE && !pifDeviceReg_IsBusy(p_device));

    s_fail = TRUE;
    CHECK(!pifDeviceReg_StartRead(p_device, 0x30, 1, data, 3) && pif_error == E_TRANSFER_FAILED);
    CHECK(!pifDeviceReg_IsBusy(p_device) && s_spi_port.error_count == 1);
}

static void _testSpiBackgroundSignal(void)
{
    PifDeviceReg *p_device;
    PifSpiDevice *p_other;
    uint8_t data[3], byte;

    CHECK(_setUp());
    p_device = &s_p_spi->parent;
    p_other = pifSpiPort_AddDevice(&s_spi_port, PIF_ID_AUTO, NULL);
    CHECK(p_other);
    s_spi_port.act_start_read = _spiRead;

    CHECK(pifDeviceReg_StartRead(p_device, 0x30, 1, data, 3));
    CHECK(pifDeviceReg_CheckTransfer(p_device) == DRS_RUN);
    CHECK(!pifDeviceReg_WriteRegByte(&p_other->parent, 0, 0) && pif_error == E_INVALID_STATE);
    CHECK(!pifSpiDevice_Transfer(p_other, &byte, NULL, 1) && pif_error == E_INVALID_STATE);
    CHECK(!pifSpiPort_RemoveDevice(&s_spi_port, s_p_spi) && pif_error == E_INVALID_STATE);

    pifSpiPort_sigEndTransfer(&s_spi_port, FALSE);
    CHECK(pifDeviceReg_CheckTransfer(p_device) == DRS_ERROR && s_spi_port.error_count == 1);
    CHECK(!pifDeviceReg_IsBusy(p_device));
    CHECK(pifSpiPort_RemoveDevice(&s_spi_port, s_p_spi));
}

static void _testSpiBackgroundPolled(void)
{
    PifDeviceReg *p_device;
    uint8_t data[3];

    CHECK(_setUp());
    p_device = &s_p_spi->parent;
    s_spi_port.act_start_read = _spiRead;
    s_spi_port.act_is_busy = _spiIsBusy;

    s_spi_busy = TRUE;
    CHECK(pifDeviceReg_StartRead(p_device, 0x30, 1, data, 3));
    CHECK(pifDeviceReg_CheckTransfer(p_device) == DRS_RUN);
    s_spi_busy = FALSE;
    CHECK(pifDeviceReg_CheckTransfer(p_device) == DRS_COMPLETE);

    // Without an end, the timeout gives the port back.
    s_spi_busy = TRUE;
    CHECK(pifDeviceReg_StartRead(p_device, 0x30, 1, data, 3));
    pif_cumulative_timer1ms += p_device->timeout + 1;
    CHECK(pifDeviceReg_CheckTransfer(p_device) == DRS_ERROR && pif_error == E_TIMEOUT);
    CHECK(!pifDeviceReg_IsBusy(p_device));
}

static void _testSpiTransfer(void)
{
    uint8_t write[4] = { 1, 2, 3, 4 }, read[4];
    PifSpiDevice *p_temp;

    CHECK(_setUp());
    CHECK(pifSpiDevice_Transfer(s_p_spi, write, read, 4) && s_transfer_count == 1 && read[0] == 0xA5);
    CHECK(!pifSpiDevice_Transfer(s_p_spi, NULL, NULL, 4) && pif_error == E_INVALID_PARAM);

    // Without act_start_transfer a background transfer is made at once.
    CHECK(pifSpiDevice_StartTransfer(s_p_spi, write, read, 4) && s_transfer_count == 2);
    CHECK(pifDeviceReg_CheckTransfer(&s_p_spi->parent) == DRS_COMPLETE);

    s_spi_port.act_start_transfer = _spiStartTransfer;
    CHECK(pifSpiDevice_StartTransfer(s_p_spi, write, read, 4) && s_start_transfer_count == 1 && read[0] == 0x5A);
    CHECK(pifDeviceReg_CheckTransfer(&s_p_spi->parent) == DRS_RUN);

    // The temporary device cannot be handed out while the port is held for it.
    p_temp = pifSpiPort_TemporaryDevice(&s_spi_port, NULL);
    CHECK(p_temp && !pifSpiDevice_StartTransfer(p_temp, write, NULL, 4) && pif_error == E_INVALID_STATE);
    pifSpiPort_sigEndTransfer(&s_spi_port, TRUE);
    CHECK(pifDeviceReg_CheckTransfer(&s_p_spi->parent) == DRS_COMPLETE);

    CHECK(pifSpiDevice_StartTransfer(p_temp, write, NULL, 4));
    CHECK(!pifSpiPort_TemporaryDevice(&s_spi_port, NULL) && pif_error == E_INVALID_STATE);
    pifSpiPort_sigEndTransfer(&s_spi_port, TRUE);
    CHECK(pifDeviceReg_CheckTransfer(&p_temp->parent) == DRS_COMPLETE);

    s_fail = TRUE;
    CHECK(!pifSpiDevice_StartTransfer(s_p_spi, write, NULL, 4) && pif_error == E_TRANSFER_FAILED);
    CHECK(!pifDeviceReg_IsBusy(&s_p_spi->parent));
}

static void _testI2cTemporaryDevice(void)
{
    PifI2cDevice *p_temp;
    uint8_t byte;

    CHECK(_setUp());
    p_temp = pifI2cPort_TemporaryDevice(&s_i2c_port, 0x60, NULL);
    CHECK(p_temp && p_temp->addr == 0x60 && p_temp->parent.timeout == 10);
    CHECK(pifDeviceReg_ReadRegByte(&p_temp->parent, 0x10, &byte) && byte == s_regs[0x10]);
    // Detect functions pass what pifI2cPort_TemporaryDevice() gave, NULL included.
    CHECK(!pifDeviceReg_ReadRegByte((PifDeviceReg *)NULL, 0x10, &byte));
}


static const struct {
    void (*run)(void);
    const char *p_name;
} s_tests[] = {
    { _testOnBothBuses, "register functions on i2c and spi" },
    { _testNullDevice, "null device" },
    { _testDefaults, "defaults" },
    { _testI2cBackground, "i2c background read" },
    { _testI2cTimeout, "i2c timeout" },
    { _testSpiBackgroundWithoutStartRead, "spi background read made at once" },
    { _testSpiBackgroundSignal, "spi background read ended by signal" },
    { _testSpiBackgroundPolled, "spi background read polled" },
    { _testSpiTransfer, "spi transfer" },
    { _testI2cTemporaryDevice, "i2c temporary device" },
};

int main(void)
{
    int failed = 0;
    size_t t;

    if (!pif_Init(_timer1us)) {
        printf("pif_device_reg: setup failed (%d)\n", pif_error);
        return 1;
    }

    printf("pif_device_reg\n");
    for (t = 0; t < sizeof(s_tests) / sizeof(s_tests[0]); t++) {
        s_failed = FALSE;
        s_bus_name = NULL;
        (*s_tests[t].run)();
        printf("  %s %s%s%s\n", s_failed ? "FAIL" : "ok  ", s_tests[t].p_name,
                s_failed && s_bus_name ? " on " : "", s_failed && s_bus_name ? s_bus_name : "");
        if (s_failed) failed++;
    }
    pifI2cPort_Clear(&s_i2c_port);
    pifSpiPort_Clear(&s_spi_port);
    printf("%d failed\n", failed);
    return failed ? 1 : 0;
}
