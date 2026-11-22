// SPDX-License-Identifier: BSD-3-Clause
// PifHmc5883 against an HMC5883L in memory on a fake I2C port: the identification, the self test that
// calibrates the axes and what it does when it cannot be carried out, the gains in LSb per Gauss, and the
// order and the scaling of a reading.
#include "hmc5883_emu.h"

#include "sensor/mag/pif_hmc5883.h"

#include <stdio.h>


// Works regardless of NDEBUG, and reports where the failure happened.
#define CHECK(COND) \
    do { \
        if (!(COND)) { \
            printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #COND); \
            s_failed = TRUE; \
            return; \
        } \
    } while (0)

#define CHECK_EQ(GOT, EXP) \
    do { \
        if ((long)(GOT) != (long)(EXP)) { \
            printf("  FAIL %s:%d: %s is %ld, expected %ld\n", __FILE__, __LINE__, #GOT, \
                   (long)(GOT), (long)(EXP)); \
            s_failed = TRUE; \
            return; \
        } \
    } while (0)

#define CHECK_NEAR(GOT, EXP) \
    do { \
        double d = (double)(GOT) - (double)(EXP); \
        if (d < -0.001 || d > 0.001) { \
            printf("  FAIL %s:%d: %s is %f, expected %f\n", __FILE__, __LINE__, #GOT, \
                   (double)(GOT), (double)(EXP)); \
            s_failed = TRUE; \
            return; \
        } \
    } while (0)

// Measurements one self test takes: the one thrown away after the gain change, then ten per bias direction.
#define SELF_TEST_MEASUREMENTS	21


static BOOL s_failed;
static uint32_t s_now_us;

static PifI2cPort s_port;
static Hmc5883Emu s_emu;
static PifImuSensor s_imu_sensor;
static PifHmc5883 s_mag;


static uint32_t _timer1us(void)
{
    return s_now_us;
}

static PifI2cReturn _actRead(PifI2cDevice* p_device, uint32_t iaddr, uint8_t isize, uint8_t* p_data, size_t size)
{
    if (p_device->addr != HMC5883_I2C_ADDR) return IR_ERROR;
    return hmc5883Emu_Read(&s_emu, iaddr, isize, p_data, size);
}

static PifI2cReturn _actWrite(PifI2cDevice* p_device, uint32_t iaddr, uint8_t isize, uint8_t* p_data, size_t size)
{
    if (p_device->addr != HMC5883_I2C_ADDR) return IR_ERROR;
    return hmc5883Emu_Write(&s_emu, iaddr, isize, p_data, size);
}

static BOOL _begin(void)
{
    hmc5883Emu_Reset(&s_emu);
    pifImuSensor_Init(&s_imu_sensor);
    return pifHmc5883_Init(&s_mag, PIF_ID_AUTO, &s_port, NULL, &s_imu_sensor);
}

static void _end(void)
{
    pifHmc5883_Clear(&s_mag);
}

static void _testInitParams(void)
{
    PifHmc5883 mag;

    hmc5883Emu_Reset(&s_emu);
    CHECK(!pifHmc5883_Init(NULL, PIF_ID_AUTO, &s_port, NULL, &s_imu_sensor));
    CHECK_EQ(pif_error, E_INVALID_PARAM);
    CHECK(!pifHmc5883_Init(&mag, PIF_ID_AUTO, NULL, NULL, &s_imu_sensor));
    CHECK_EQ(pif_error, E_INVALID_PARAM);
    CHECK(!pifHmc5883_Init(&mag, PIF_ID_AUTO, &s_port, NULL, NULL));
    CHECK_EQ(pif_error, E_INVALID_PARAM);
}

static void _testDetect(void)
{
    hmc5883Emu_Reset(&s_emu);
    CHECK(pifHmc5883_Detect(&s_port, NULL));

    // Another device that answers at the same address is not this one.
    hmc5883Emu_Reset(&s_emu);
    s_emu.ident[1] = '5';
    CHECK(!pifHmc5883_Detect(&s_port, NULL));

    // A bus with pull-ups and nothing on it reads as ones.
    hmc5883Emu_Reset(&s_emu);
    s_emu.ident[0] = s_emu.ident[1] = s_emu.ident[2] = 0xFF;
    CHECK(!pifHmc5883_Detect(&s_port, NULL));

    hmc5883Emu_Reset(&s_emu);
    s_emu.fail_in = 0;
    CHECK(!pifHmc5883_Detect(&s_port, NULL));
}

static void _testInitCalibratesTheAxes(void)
{
    CHECK(_begin());

    // The self test drives each axis with both bias currents ten times, after one measurement that the
    // gain change spoils and that the driver throws away.
    CHECK_EQ(s_emu.measurements, SELF_TEST_MEASUREMENTS + 1);	// The continuous mode starts the last one

    // 1.16 Gauss on X and Y and 1.08 on Z, at 660 LSb per Gauss, against what the device gave back.
    CHECK_NEAR(s_mag.scale[AXIS_X], 1.16f * 660 * 20 / (HMC5883_EMU_BIAS_XY * 20.0f));
    CHECK_NEAR(s_mag.scale[AXIS_Y], 1.16f * 660 * 20 / (HMC5883_EMU_BIAS_XY * 20.0f));
    CHECK_NEAR(s_mag.scale[AXIS_Z], 1.08f * 660 * 20 / (HMC5883_EMU_BIAS_Z * 20.0f));

    // The device is left measuring the field itself, over and over, at the gain it was found at.
    CHECK_EQ(s_emu.config_a, HMC5883_MEASURE_MODE_NORMAL | HMC5883_SAMPLES_DEFAULT | HMC5883_DATARATE_DEFAULT);
    CHECK_EQ(s_emu.mode & HMC5883_MODE_MASK, HMC5883_MODE_CONTINOUS);
    CHECK_EQ(s_emu.config_b & HMC5883_GAIN_MASK, HMC5883_GAIN_1_3GA);
    CHECK_NEAR(s_imu_sensor._mag_gain, 1090);
    CHECK(s_imu_sensor._measure & IMU_MEASURE_MAGNETO);
    CHECK(s_mag._id != PIF_ID_AUTO);
    _end();

    // The device is off the port, so another one fits in its place.
    CHECK(_begin());
    _end();
}

static void _testSelfTestGivesUp(void)
{
    int axis, transfers;

    // An axis that reads the overflow value has no measurement in it, so no scale comes of the self test.
    // The device is still set up, and reports what it reads without a correction.
    hmc5883Emu_Reset(&s_emu);
    pifImuSensor_Init(&s_imu_sensor);
    s_emu.pos_bias[AXIS_Y] = -4096;
    CHECK(pifHmc5883_Init(&s_mag, PIF_ID_AUTO, &s_port, NULL, &s_imu_sensor));
    for (axis = 0; axis < AXIS_COUNT; axis++) CHECK_NEAR(s_mag.scale[axis], 1.0f);
    CHECK_EQ(s_emu.mode & HMC5883_MODE_MASK, HMC5883_MODE_CONTINOUS);
    _end();

    // A device that never reports a measurement ready is given up on after the timeout.
    hmc5883Emu_Reset(&s_emu);
    pifImuSensor_Init(&s_imu_sensor);
    s_emu.not_ready_reads = 10000;
    s_emu.ms_per_status_read = 10;
    CHECK(pifHmc5883_Init(&s_mag, PIF_ID_AUTO, &s_port, NULL, &s_imu_sensor));
    for (axis = 0; axis < AXIS_COUNT; axis++) CHECK_NEAR(s_mag.scale[axis], 1.0f);
    _end();

    // One that stops answering is not set up at all.
    hmc5883Emu_Reset(&s_emu);
    pifImuSensor_Init(&s_imu_sensor);
    s_emu.fail_in = 0;
    CHECK(!pifHmc5883_Init(&s_mag, PIF_ID_AUTO, &s_port, NULL, &s_imu_sensor));
    CHECK(!(s_imu_sensor._measure & IMU_MEASURE_MAGNETO));

    // One that stops at the very last transfer of the setup, which is checked like all the others.
    CHECK(_begin());
    transfers = s_emu.transfers;
    _end();
    hmc5883Emu_Reset(&s_emu);
    pifImuSensor_Init(&s_imu_sensor);
    s_emu.fail_in = transfers - 1;
    CHECK(!pifHmc5883_Init(&s_mag, PIF_ID_AUTO, &s_port, NULL, &s_imu_sensor));
    CHECK(!(s_imu_sensor._measure & IMU_MEASURE_MAGNETO));
}

static void _testGain(void)
{
    static const struct { PifHmc5883Gain gain; int lsb; } table[] = {
        { HMC5883_GAIN_0_88GA, 1370 }, { HMC5883_GAIN_1_3GA, 1090 }, { HMC5883_GAIN_1_9GA, 820 },
        { HMC5883_GAIN_2_5GA, 660 }, { HMC5883_GAIN_4GA, 440 }, { HMC5883_GAIN_4_7GA, 390 },
        { HMC5883_GAIN_5_6GA, 330 }, { HMC5883_GAIN_8_1GA, 230 }
    };
    size_t t;

    CHECK(_begin());
    for (t = 0; t < sizeof(table) / sizeof(table[0]); t++) {
        CHECK(pifHmc5883_SetGain(&s_mag, table[t].gain));
        CHECK_EQ(s_emu.config_b & HMC5883_GAIN_MASK, table[t].gain);
        CHECK_NEAR(s_imu_sensor._mag_gain, table[t].lsb);
    }

    // The gain is the top three bits of the register, and the rest of it stays as it was.
    s_emu.config_b = 0x1F;
    CHECK(pifHmc5883_SetGain(&s_mag, HMC5883_GAIN_4GA));
    CHECK_EQ(s_emu.config_b, HMC5883_GAIN_4GA | 0x1F);

    s_emu.fail_in = 0;
    CHECK(!pifHmc5883_SetGain(&s_mag, HMC5883_GAIN_1_3GA));
    _end();
}

static void _testReadMag(void)
{
    int16_t mag[AXIS_COUNT];

    CHECK(_begin());
    // The gain went back at the end of the setup, so the measurement that follows is still at the old one,
    // as the datasheet says. It is read and thrown away here.
    CHECK(pifHmc5883_ReadMag(&s_mag, mag));

    s_mag.scale[AXIS_X] = 1.0f;
    s_mag.scale[AXIS_Y] = 1.0f;
    s_mag.scale[AXIS_Z] = 1.0f;
    s_emu.field[AXIS_X] = -1234;
    s_emu.field[AXIS_Y] = 2345;
    s_emu.field[AXIS_Z] = -456;
    // The measurement in the output registers is the one before these, so one reading passes first.
    CHECK(pifHmc5883_ReadMag(&s_mag, mag));
    CHECK(pifHmc5883_ReadMag(&s_mag, mag));
    CHECK_EQ(mag[AXIS_X], -1234);
    CHECK_EQ(mag[AXIS_Y], 2345);
    CHECK_EQ(mag[AXIS_Z], -456);

    // The calibration of the axes multiplies what comes out.
    s_mag.scale[AXIS_X] = 2.0f;
    s_mag.scale[AXIS_Z] = 0.5f;
    CHECK(pifHmc5883_ReadMag(&s_mag, mag));
    CHECK_EQ(mag[AXIS_X], -2468);
    CHECK_EQ(mag[AXIS_Y], 2345);
    CHECK_EQ(mag[AXIS_Z], -228);

    // Nothing is read while the device has no measurement to give.
    s_emu.ready = FALSE;
    CHECK(!pifHmc5883_ReadMag(&s_mag, mag));

    s_emu.fail_in = 0;
    CHECK(!pifHmc5883_ReadMag(&s_mag, mag));
    _end();
}

static void _testReadMagThroughTheImuSensor(void)
{
    int16_t raw[AXIS_COUNT];
    float mag[AXIS_COUNT];

    CHECK(_begin());
    CHECK(pifHmc5883_ReadMag(&s_mag, raw));

    s_mag.scale[AXIS_X] = 1.0f;
    s_mag.scale[AXIS_Y] = 1.0f;
    s_mag.scale[AXIS_Z] = 1.0f;
    s_emu.field[AXIS_X] = 1090;
    s_emu.field[AXIS_Y] = -545;
    s_emu.field[AXIS_Z] = 0;
    CHECK(pifHmc5883_ReadMag(&s_mag, raw));

    // The driver registered itself as the magnetometer of the IMU sensor, which turns the raw values into
    // Gauss with the gain of the device.
    CHECK(pifImuSensor_ReadMag(&s_imu_sensor, mag));
    CHECK_NEAR(mag[AXIS_X], 1.0f);
    CHECK_NEAR(mag[AXIS_Y], -0.5f);
    CHECK_NEAR(mag[AXIS_Z], 0.0f);
    _end();
}

static const struct
{
    void (*run)(void);
    const char *p_name;
} s_tests[] = {
    { _testInitParams, "init parameters" },
    { _testDetect, "detect" },
    { _testInitCalibratesTheAxes, "init calibrates the axes" },
    { _testSelfTestGivesUp, "the self test gives up" },
    { _testGain, "gain" },
    { _testReadMag, "read" },
    { _testReadMagThroughTheImuSensor, "read through the IMU sensor" },
};

static BOOL _portSetup(void)
{
    return TRUE;
}

int main(void)
{
    int failed = 0;
    size_t t;

    if (!pif_Init(_timer1us, _portSetup, NULL)) {
        printf("pif_hmc5883: setup failed (%d)\n", pif_error);
        return 1;
    }
    if (!pifI2cPort_Init(&s_port, PIF_ID_AUTO, 1)) {
        printf("pif_hmc5883: port failed (%d)\n", pif_error);
        return 1;
    }
    s_port.act_read = _actRead;
    s_port.act_write = _actWrite;

    printf("pif_hmc5883\n");
    for (t = 0; t < sizeof(s_tests) / sizeof(s_tests[0]); t++) {
        s_failed = FALSE;
        (*s_tests[t].run)();
        // A test that gave up in the middle left its device on the port, which only takes one.
        pifHmc5883_Clear(&s_mag);
        printf("  %s %s\n", s_failed ? "FAIL" : "ok  ", s_tests[t].p_name);
        if (s_failed) failed++;
    }
    pifI2cPort_Clear(&s_port);
    printf("%d failed\n", failed);
    return failed ? 1 : 0;
}
