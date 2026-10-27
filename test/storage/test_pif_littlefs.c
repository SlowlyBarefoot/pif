// SPDX-License-Identifier: BSD-3-Clause
// littlefs through pif_littlefs on an SPI NOR flash in memory: files kept across a remount, and a file rewritten
// with the power cut at every program along the way.
#include "storage/pif_littlefs.h"
#include "nor_flash_emu.h"


// Works regardless of NDEBUG, and reports where the failure happened.
#define CHECK(COND) \
    do { \
        if (!(COND)) { \
            printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #COND); \
            s_failed = TRUE; \
            return; \
        } \
    } while (0)

#define FILE_SIZE		300

static BOOL s_failed;
static PifNorFlash s_flash;
static PifLittlefs s_bd;
static lfs_t s_lfs;


static uint32_t _timer1us(void)
{
    return 0;
}

static BOOL _setUp(void)
{
    // A failed test returns early, so free what it left behind.
    pifNorFlash_Clear(&s_flash);
    if (!_emuInit()) return FALSE;
    if (!pifNorFlash_Init(&s_flash, PIF_ID_AUTO, &s_emu_port, NULL, 0)) return FALSE;
    return pifLittlefs_Init(&s_bd, &s_flash, 0, 0);
}

// Power comes back: the device is idle and takes programs again.
static void _powerOn(void)
{
    s_emu_power_cut = FALSE;
    s_emu_cut_after = -1;
    s_emu_busy = 0;
    s_emu_wel = FALSE;
}

static int _writeFile(const char* p_name, uint8_t value)
{
    uint8_t data[FILE_SIZE];
    lfs_file_t file;
    int err;

    memset(data, value, sizeof(data));
    err = lfs_file_open(&s_lfs, &file, p_name, LFS_O_WRONLY | LFS_O_CREAT | LFS_O_TRUNC);
    if (err) return err;
    if (lfs_file_write(&s_lfs, &file, data, sizeof(data)) != (lfs_ssize_t)sizeof(data)) err = LFS_ERR_IO;
    if (lfs_file_close(&s_lfs, &file) && !err) err = LFS_ERR_IO;
    return err;
}

// What the file holds: one value throughout, or -1.
static int _readFile(const char* p_name)
{
    uint8_t data[FILE_SIZE];
    lfs_file_t file;
    lfs_ssize_t size;
    size_t i;

    if (lfs_file_open(&s_lfs, &file, p_name, LFS_O_RDONLY)) return -1;
    size = lfs_file_read(&s_lfs, &file, data, sizeof(data));
    lfs_file_close(&s_lfs, &file);
    if (size != (lfs_ssize_t)sizeof(data)) return -1;
    for (i = 1; i < sizeof(data); i++) {
        if (data[i] != data[0]) return -1;
    }
    return data[0];
}

static void testInit(void)
{
    CHECK(_setUp());
    CHECK(s_bd.cfg.block_size == EMU_SECTOR && s_bd.cfg.block_count == EMU_CAPACITY / EMU_SECTOR);

    // An area that does not start or end on a sector, or does not fit.
    CHECK(!pifLittlefs_Init(&s_bd, &s_flash, 0x100, 0) && pif_error == E_INVALID_PARAM);
    CHECK(!pifLittlefs_Init(&s_bd, &s_flash, 0, 0x1100) && pif_error == E_INVALID_PARAM);
    CHECK(!pifLittlefs_Init(&s_bd, &s_flash, 0x8000, 0x9000) && pif_error == E_INVALID_PARAM);
    CHECK(pifLittlefs_Init(&s_bd, &s_flash, 0x8000, 0));
    CHECK(s_bd.cfg.block_count == 8);
}

static void testFiles(void)
{
    uint32_t boot, read;
    lfs_file_t file;
    int i;

    CHECK(_setUp());
    // A device that was never formatted does not mount.
    CHECK(lfs_mount(&s_lfs, &s_bd.cfg) != 0);
    CHECK(lfs_format(&s_lfs, &s_bd.cfg) == 0);

    // A boot counter, kept across remounts.
    for (i = 0; i < 5; i++) {
        CHECK(lfs_mount(&s_lfs, &s_bd.cfg) == 0);
        boot = 0;
        CHECK(lfs_file_open(&s_lfs, &file, "boot", LFS_O_RDWR | LFS_O_CREAT) == 0);
        lfs_file_read(&s_lfs, &file, &boot, sizeof(boot));
        CHECK(boot == (uint32_t)i);
        boot++;
        CHECK(lfs_file_rewind(&s_lfs, &file) == 0);
        CHECK(lfs_file_write(&s_lfs, &file, &boot, sizeof(boot)) == sizeof(boot));
        CHECK(lfs_file_close(&s_lfs, &file) == 0);
        CHECK(lfs_unmount(&s_lfs) == 0);
    }
    CHECK(lfs_mount(&s_lfs, &s_bd.cfg) == 0);
    CHECK(lfs_file_open(&s_lfs, &file, "boot", LFS_O_RDONLY) == 0);
    CHECK(lfs_file_read(&s_lfs, &file, &read, sizeof(read)) == sizeof(read) && read == 5);
    CHECK(lfs_file_close(&s_lfs, &file) == 0);
    CHECK(lfs_unmount(&s_lfs) == 0);
    CHECK(!s_emu_crossed_page && !s_emu_ignored);
}

static void testPowerCut(void)
{
    int cut, programs, value, old_count = 0, new_count = 0;

    CHECK(_setUp());
    CHECK(lfs_format(&s_lfs, &s_bd.cfg) == 0);
    CHECK(lfs_mount(&s_lfs, &s_bd.cfg) == 0);
    CHECK(_writeFile("data", 0x11) == 0);
    CHECK(lfs_unmount(&s_lfs) == 0);

    // How many programs the rewrite takes when nothing goes wrong.
    CHECK(lfs_mount(&s_lfs, &s_bd.cfg) == 0);
    programs = s_emu_program_count;
    CHECK(_writeFile("data", 0x11) == 0);
    programs = s_emu_program_count - programs;
    CHECK(lfs_unmount(&s_lfs) == 0);
    CHECK(programs > 0);

    // Cut at each of them in turn: after power comes back the file is whole, the old one or the new one.
    for (cut = 0; cut <= programs; cut++) {
        CHECK(lfs_mount(&s_lfs, &s_bd.cfg) == 0);
        CHECK(_readFile("data") == 0x11);
        s_emu_cut_after = s_emu_program_count + cut;
        _writeFile("data", 0x22);
        // Whatever littlefs does after the cut never reaches the media.
        _powerOn();

        CHECK(lfs_mount(&s_lfs, &s_bd.cfg) == 0);
        value = _readFile("data");
        CHECK(value == 0x11 || value == 0x22);
        if (value == 0x22) new_count++;
        else old_count++;
        // Back to the old file for the next round.
        if (value == 0x22) CHECK(_writeFile("data", 0x11) == 0);
        CHECK(lfs_unmount(&s_lfs) == 0);
    }
    // Cut before the end it is the old file; let through to the end it is the new one.
    CHECK(old_count > 0 && new_count > 0);
}

static const struct {
    void (*run)(void);
    const char *p_name;
} s_tests[] = {
    { testInit, "init" },
    { testFiles, "files across remounts" },
    { testPowerCut, "power cut while rewriting" },
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
        printf("pif_littlefs: setup failed (%d)\n", pif_error);
        return 1;
    }

    printf("pif_littlefs\n");
    for (t = 0; t < sizeof(s_tests) / sizeof(s_tests[0]); t++) {
        s_failed = FALSE;
        (*s_tests[t].run)();
        printf("  %s %s\n", s_failed ? "FAIL" : "ok  ", s_tests[t].p_name);
        if (s_failed) failed++;
    }
    pifNorFlash_Clear(&s_flash);
    pifSpiPort_Clear(&s_emu_port);
    printf("%d failed\n", failed);
    return failed ? 1 : 0;
}
