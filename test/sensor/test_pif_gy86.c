// SPDX-License-Identifier: BSD-3-Clause
// PifGy86 against the three chips of a GY-86 in memory on a fake I2C port: the MPU-6050 as a register file
// with an auxiliary bus behind it, the HMC5883L on that auxiliary bus, and the MS5611 next to them.
//
// The magnetometer answers on the host bus only while the MPU-6050 has the two buses tied together, as it does
// on the module, so the setup has to go through the bypass to reach it and read it through the external sensor
// registers afterwards.
#include "hmc5883_emu.h"
#include "ms5611_emu.h"

#include "sensor/imu/pif_gy86.h"

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

#define MPU_ADDR		MPU60X0_I2C_ADDR(0)
#define MS5611_ADDR		MS5611_I2C_ADDR(1)
#define MPU_REG_COUNT	0x80


static BOOL s_failed;
static uint32_t s_now_us;

static PifI2cPort s_port;
static Hmc5883Emu s_hmc;
static Ms5611Emu s_ms;
static PifImuSensor s_imu_sensor;
static PifGy86 s_gy86;

static uint8_t s_mpu[MPU_REG_COUNT];	// The register file of the MPU-6050
static int s_mpu_resets;
static BOOL s_mpu_absent;				// Nothing answers at the address of the MPU-6050

static int s_baro_reads;				// Readings reported by the barometer


static uint32_t _timer1us(void)
{
    return s_now_us;
}

static void _mpuReset(void)
{
    // What the chip holds after a reset: asleep, on its internal oscillator, and only WHO_AM_I set.
    memset(s_mpu, 0, sizeof(s_mpu));
    s_mpu[MPU60X0_REG_WHO_AM_I] = MPU60X0_WHO_AM_I_CONST;
    s_mpu[MPU60X0_REG_PWR_MGMT_1] = 0x40;
}

// TRUE while the magnetometer is tied to the host bus by the bypass of the MPU-6050.
static BOOL _bypassOn(void)
{
    return (s_mpu[MPU60X0_REG_INT_PIN_CFG] & MPU60X0_I2C_BYPASS_EN_MASK) != 0;
}

// What the auxiliary master of the MPU-6050 does between two samples: it reads the slave it was given and
// leaves the bytes in the external sensor registers. A slave 0 set up for anything else reads as zeros.
static void _mpuPumpAux(void)
{
    uint8_t len = s_mpu[MPU60X0_REG_I2C_SLV0_CTRL] & MPU60X0_I2C_SLV_LEN_MASK;

    memset(s_mpu + MPU60X0_REG_EXT_SENS_DATA_00, 0, 24);
    if (!(s_mpu[MPU60X0_REG_USER_CTRL] & MPU60X0_I2C_MST_EN_MASK)) return;
    if (!(s_mpu[MPU60X0_REG_I2C_SLV0_CTRL] & MPU60X0_I2C_SLV_EN_MASK)) return;
    if (s_mpu[MPU60X0_REG_I2C_SLV0_ADDR] != (MPU60X0_I2C_SLV_ADDR(HMC5883_I2C_ADDR) | MPU60X0_I2C_SLV_RW(1))) return;

    (void)hmc5883Emu_Read(&s_hmc, s_mpu[MPU60X0_REG_I2C_SLV0_REG], 1,
            s_mpu + MPU60X0_REG_EXT_SENS_DATA_00, len);
}

static PifI2cReturn _mpuRead(uint32_t iaddr, uint8_t isize, uint8_t* p_data, size_t size)
{
    if (isize != 1 || iaddr + size > MPU_REG_COUNT) return IR_ERROR;

    if (iaddr >= MPU60X0_REG_EXT_SENS_DATA_00 && iaddr <= MPU60X0_REG_EXT_SENS_DATA_23) _mpuPumpAux();
    memcpy(p_data, s_mpu + iaddr, size);
    return IR_COMPLETE;
}

static PifI2cReturn _mpuWrite(uint32_t iaddr, uint8_t isize, uint8_t* p_data, size_t size)
{
    size_t i;

    if (isize != 1 || iaddr + size > MPU_REG_COUNT) return IR_ERROR;

    for (i = 0; i < size; i++) {
        if (iaddr + i == MPU60X0_REG_PWR_MGMT_1 && (p_data[i] & MPU60X0_DEVICE_RESET_MASK)) {
            _mpuReset();
            s_mpu_resets++;
            continue;
        }
        s_mpu[iaddr + i] = p_data[i];
    }
    return IR_COMPLETE;
}

static PifI2cReturn _actRead(PifI2cDevice* p_device, uint32_t iaddr, uint8_t isize, uint8_t* p_data, size_t size)
{
    switch (p_device->addr) {
    case MPU_ADDR:
        return s_mpu_absent ? IR_ERROR : _mpuRead(iaddr, isize, p_data, size);

    case HMC5883_I2C_ADDR:
        return _bypassOn() ? hmc5883Emu_Read(&s_hmc, iaddr, isize, p_data, size) : IR_ERROR;

    case MS5611_ADDR:
        return ms5611Emu_Read(&s_ms, iaddr, isize, p_data, size);
    }
    return IR_ERROR;
}

static PifI2cReturn _actWrite(PifI2cDevice* p_device, uint32_t iaddr, uint8_t isize, uint8_t* p_data, size_t size)
{
    switch (p_device->addr) {
    case MPU_ADDR:
        return s_mpu_absent ? IR_ERROR : _mpuWrite(iaddr, isize, p_data, size);

    case HMC5883_I2C_ADDR:
        return _bypassOn() ? hmc5883Emu_Write(&s_hmc, iaddr, isize, p_data, size) : IR_ERROR;

    case MS5611_ADDR:
        return ms5611Emu_Write(&s_ms, iaddr, isize, p_data, size);
    }
    return IR_ERROR;
}

static void _evtBaroRead(float pressure, float temperature)
{
    s_baro_reads++;
}

static void _param(PifGy86Param* p_param)
{
    memset(p_param, 0, sizeof(PifGy86Param));
    p_param->mpu60x0_afs_sel = MPU60X0_AFS_SEL_8G;
    p_param->mpu60x0_clksel = MPU60X0_CLKSEL_PLL_ZGYRO;
    p_param->mpu60x0_dlpf_cfg = MPU60X0_DLPF_CFG_A44HZ_G42HZ;
    p_param->mpu60x0_fs_sel = MPU60X0_FS_SEL_2000DPS;
    p_param->mpu60x0_i2c_mst_clk = MPU60X0_I2C_MST_CLK_400KHZ;
    p_param->hmc5883_data_rate = HMC5883_DATARATE_75HZ;
    p_param->hmc5883_gain = HMC5883_GAIN_1_3GA;
    p_param->hmc5883_mode = HMC5883_MODE_CONTINOUS;
    p_param->hmc5883_samples = HMC5883_SAMPLES_8;
    p_param->ms5611_osr = MS5611_OSR_4096;
    p_param->ms5611_read_period = 25;
    p_param->ms5611_evt_read = _evtBaroRead;
}

static void _devicesReset(void)
{
    _mpuReset();
    s_mpu_resets = 0;
    s_mpu_absent = FALSE;
    hmc5883Emu_Reset(&s_hmc);
    ms5611Emu_Reset(&s_ms);
    s_baro_reads = 0;
    pifImuSensor_Init(&s_imu_sensor);
}

static BOOL _begin(PifGy86Param* p_param)
{
    _devicesReset();
    return pifGy86_Init(&s_gy86, PIF_ID_AUTO, &s_port, p_param, &s_imu_sensor);
}

static void _end(void)
{
    pifGy86_Clear(&s_gy86);
    pifTaskManager_Loop();
}

static void _testInitParams(void)
{
    PifGy86 gy86;

    _devicesReset();
    CHECK(!pifGy86_Init(NULL, PIF_ID_AUTO, &s_port, NULL, &s_imu_sensor));
    CHECK_EQ(pif_error, E_INVALID_PARAM);
    CHECK(!pifGy86_Init(&gy86, PIF_ID_AUTO, NULL, NULL, &s_imu_sensor));
    CHECK_EQ(pif_error, E_INVALID_PARAM);
    CHECK(!pifGy86_Init(&gy86, PIF_ID_AUTO, &s_port, NULL, NULL));
    CHECK_EQ(pif_error, E_INVALID_PARAM);
}

static void _testDetect(void)
{
    _devicesReset();
    CHECK(pifGy86_Detect(&s_port));

    s_mpu[MPU60X0_REG_WHO_AM_I] = 0x70;
    CHECK(!pifGy86_Detect(&s_port));

    s_mpu_absent = TRUE;
    CHECK(!pifGy86_Detect(&s_port));
}

static void _testInitSetsUpTheMpu6050(void)
{
    PifGy86Param param;

    _param(&param);
    CHECK(_begin(&param));

    CHECK_EQ(s_mpu_resets, 1);
    CHECK_EQ(s_mpu[MPU60X0_REG_SMPLRT_DIV], 0);
    CHECK_EQ(s_mpu[MPU60X0_REG_PWR_MGMT_1], MPU60X0_CLKSEL_PLL_ZGYRO);
    CHECK_EQ(s_mpu[MPU60X0_REG_CONFIG] & MPU60X0_DLPF_CFG_MASK, MPU60X0_DLPF_CFG_A44HZ_G42HZ);
    CHECK_EQ(s_mpu[MPU60X0_REG_GYRO_CONFIG] & MPU60X0_FS_SEL_MASK, MPU60X0_FS_SEL_2000DPS);
    CHECK_EQ(s_mpu[MPU60X0_REG_ACCEL_CONFIG] & MPU60X0_AFS_SEL_MASK, MPU60X0_AFS_SEL_8G);
    CHECK_EQ(s_mpu[MPU60X0_REG_I2C_MST_CTRL] & MPU60X0_I2C_MST_CLK_MASK, MPU60X0_I2C_MST_CLK_400KHZ);
    CHECK_EQ(s_mpu[MPU60X0_REG_INT_ENABLE] & MPU60X0_DATA_RDY_EN_MASK, MPU60X0_DATA_RDY_EN(1));
    CHECK_EQ(s_mpu[MPU60X0_REG_INT_PIN_CFG] & MPU60X0_INT_RD_CLEAR_MASK, MPU60X0_INT_RD_CLEAR(1));

    // The gyroscope and the accelerometer went to the IMU sensor with the ranges they were given.
    CHECK(s_imu_sensor._measure & IMU_MEASURE_GYROSCOPE);
    CHECK(s_imu_sensor._measure & IMU_MEASURE_ACCELERO);
    _end();
}

static void _testInitSetsUpTheMagnetometer(void)
{
    PifGy86Param param;

    _param(&param);
    CHECK(_begin(&param));

    // The magnetometer was reached through the bypass and set up as the parameters ask.
    CHECK(s_hmc.measurements > 0);
    CHECK_EQ(s_hmc.config_a, HMC5883_MEASURE_MODE_NORMAL | HMC5883_SAMPLES_8 | HMC5883_DATARATE_75HZ);
    CHECK_EQ(s_hmc.config_b & HMC5883_GAIN_MASK, HMC5883_GAIN_1_3GA);
    CHECK_EQ(s_hmc.mode & HMC5883_MODE_MASK, HMC5883_MODE_CONTINOUS);
    CHECK(s_gy86._hmc5883.scale[AXIS_X] > 0.9f && s_gy86._hmc5883.scale[AXIS_X] < 1.1f);

    // The bypass is taken away again, so the magnetometer is no longer on the host bus at all.
    CHECK(!_bypassOn());
    CHECK(!pifHmc5883_Detect(&s_port, NULL));

    // The MPU-6050 is master of the auxiliary bus and reads the six measurement registers.
    CHECK_EQ(s_mpu[MPU60X0_REG_USER_CTRL] & MPU60X0_I2C_MST_EN_MASK, MPU60X0_I2C_MST_EN(1));
    CHECK_EQ(s_mpu[MPU60X0_REG_I2C_SLV0_ADDR],
            MPU60X0_I2C_SLV_ADDR(HMC5883_I2C_ADDR) | MPU60X0_I2C_SLV_RW(1));
    CHECK_EQ(s_mpu[MPU60X0_REG_I2C_SLV0_REG], HMC5883_REG_OUT_X_M);
    CHECK_EQ(s_mpu[MPU60X0_REG_I2C_SLV0_CTRL] & MPU60X0_I2C_SLV_LEN_MASK, 6);
    CHECK_EQ(s_mpu[MPU60X0_REG_I2C_SLV0_CTRL] & MPU60X0_I2C_SLV_EN_MASK, MPU60X0_I2C_SLV_EN(1));

    // The IMU sensor reads the magnetometer through this driver from here on, not through the one of the
    // magnetometer, which can no longer be reached.
    CHECK(s_imu_sensor._measure & IMU_MEASURE_MAGNETO);
    CHECK(s_imu_sensor.__mag_info.read == (PifImuSensorRead)pifGy86_ReadMag);
    CHECK(s_imu_sensor.__mag_info.p_issuer == &s_gy86);
    _end();
}

static void _testInitSetsUpTheBarometer(void)
{
    PifGy86Param param;

    _param(&param);
    CHECK(_begin(&param));

    CHECK_EQ(s_ms.resets, 1);
    CHECK_EQ(s_ms.prom_reads, 8);
    CHECK_EQ(s_gy86._ms5611._over_sampling_rate, MS5611_OSR_4096);
    CHECK_EQ(s_gy86._ms5611._conversion_time, 10);
    // The reading task is added, and left for the application to start.
    CHECK(s_gy86._ms5611._p_task && s_gy86._ms5611._p_task->_pause);
    _end();

    // Without a read event there is nothing to give the readings to, so the barometer is left alone.
    param.ms5611_evt_read = NULL;
    CHECK(_begin(&param));
    CHECK_EQ(s_ms.resets, 0);
    CHECK(!s_gy86._ms5611._p_task);
    CHECK(!s_gy86._ms5611._p_device);
    _end();
}

static void _testInitWithoutParameters(void)
{
    CHECK(_begin(NULL));

    // The clock of the gyroscope PLL and the default clock of the auxiliary master.
    CHECK_EQ(s_mpu[MPU60X0_REG_PWR_MGMT_1], MPU60X0_CLKSEL_PLL_ZGYRO);
    CHECK_EQ(s_mpu[MPU60X0_REG_I2C_MST_CTRL] & MPU60X0_I2C_MST_CLK_MASK, MPU60X0_I2C_MST_CLK_DEFAULT);
    // The magnetometer keeps what its own setup left, and the barometer is not used.
    CHECK_EQ(s_hmc.mode & HMC5883_MODE_MASK, HMC5883_MODE_CONTINOUS);
    CHECK(!s_gy86._ms5611._p_device);
    _end();
}

static void _testReadMag(void)
{
    PifGy86Param param;
    int16_t mag[AXIS_COUNT];
    float gauss[AXIS_COUNT];

    _param(&param);
    CHECK(_begin(&param));

    s_gy86._hmc5883.scale[AXIS_X] = 1.0f;
    s_gy86._hmc5883.scale[AXIS_Y] = 1.0f;
    s_gy86._hmc5883.scale[AXIS_Z] = 1.0f;
    s_hmc.field[AXIS_X] = -1234;
    s_hmc.field[AXIS_Y] = 2345;
    s_hmc.field[AXIS_Z] = -456;
    // The magnetometer is measuring continuously, so two readings pass before these values are in the
    // output registers the auxiliary master copies from.
    CHECK(pifGy86_ReadMag(&s_gy86, mag));
    CHECK(pifGy86_ReadMag(&s_gy86, mag));
    CHECK(pifGy86_ReadMag(&s_gy86, mag));
    CHECK_EQ(mag[AXIS_X], -1234);
    CHECK_EQ(mag[AXIS_Y], 2345);
    CHECK_EQ(mag[AXIS_Z], -456);

    // The calibration of the axes multiplies what comes out, as it does on a direct reading.
    s_gy86._hmc5883.scale[AXIS_X] = 2.0f;
    CHECK(pifGy86_ReadMag(&s_gy86, mag));
    CHECK_EQ(mag[AXIS_X], -2468);

    // And the IMU sensor turns it into Gauss with the gain of the magnetometer.
    s_gy86._hmc5883.scale[AXIS_X] = 1.0f;
    s_hmc.field[AXIS_X] = 1090;
    s_hmc.field[AXIS_Y] = 0;
    s_hmc.field[AXIS_Z] = 0;
    CHECK(pifGy86_ReadMag(&s_gy86, mag));
    CHECK(pifGy86_ReadMag(&s_gy86, mag));
    CHECK(pifImuSensor_ReadMag(&s_imu_sensor, gauss));
    CHECK(gauss[AXIS_X] > 0.999f && gauss[AXIS_X] < 1.001f);

    // A slave 0 pointed somewhere else leaves the external sensor registers as they are, which is zeros.
    s_mpu[MPU60X0_REG_I2C_SLV0_REG] = HMC5883_REG_STATUS;
    CHECK(pifGy86_ReadMag(&s_gy86, mag));
    CHECK_EQ(mag[AXIS_X], 0);
    _end();
}

static void _testInitFailsWithoutAChip(void)
{
    PifGy86Param param;

    _param(&param);

    // No MPU-6050: nothing else can be reached either.
    _devicesReset();
    s_mpu_absent = TRUE;
    CHECK(!pifGy86_Init(&s_gy86, PIF_ID_AUTO, &s_port, &param, &s_imu_sensor));
    _end();

    // A magnetometer that does not answer behind the bypass.
    _devicesReset();
    s_hmc.ident[0] = 'Q';
    CHECK(!pifGy86_Init(&s_gy86, PIF_ID_AUTO, &s_port, &param, &s_imu_sensor));
    _end();

    // A barometer whose PROM is not there.
    _devicesReset();
    memset(s_ms.prom, 0, sizeof(s_ms.prom));
    CHECK(!pifGy86_Init(&s_gy86, PIF_ID_AUTO, &s_port, &param, &s_imu_sensor));
    _end();

    // All three are off the port again afterwards, so a module that does answer still goes on.
    CHECK(_begin(&param));
    _end();
}

static const struct
{
    void (*run)(void);
    const char *p_name;
} s_tests[] = {
    { _testInitParams, "init parameters" },
    { _testDetect, "detect" },
    { _testInitSetsUpTheMpu6050, "init sets up the MPU-6050" },
    { _testInitSetsUpTheMagnetometer, "init sets up the magnetometer" },
    { _testInitSetsUpTheBarometer, "init sets up the barometer" },
    { _testInitWithoutParameters, "init without parameters" },
    { _testReadMag, "read the magnetometer" },
    { _testInitFailsWithoutAChip, "init fails without a chip" },
};

static BOOL _portSetup(void)
{
    // One task for the reading task of the barometer.
    return pifTaskManager_Init(1, 0);
}

int main(void)
{
    int failed = 0;
    size_t t;

    if (!pif_Init(_timer1us, _portSetup, NULL)) {
        printf("pif_gy86: setup failed (%d)\n", pif_error);
        return 1;
    }
    if (!pifI2cPort_Init(&s_port, PIF_ID_AUTO, 3)) {
        printf("pif_gy86: port failed (%d)\n", pif_error);
        return 1;
    }
    s_port.act_read = _actRead;
    s_port.act_write = _actWrite;

    printf("pif_gy86\n");
    for (t = 0; t < sizeof(s_tests) / sizeof(s_tests[0]); t++) {
        s_failed = FALSE;
        (*s_tests[t].run)();
        // A test that gave up in the middle left its devices on the port, which only takes three.
        pifGy86_Clear(&s_gy86);
        pifTaskManager_Loop();
        printf("  %s %s\n", s_failed ? "FAIL" : "ok  ", s_tests[t].p_name);
        if (s_failed) failed++;
    }
    pifI2cPort_Clear(&s_port);
    printf("%d failed\n", failed);
    return failed ? 1 : 0;
}
