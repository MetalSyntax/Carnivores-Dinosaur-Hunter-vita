/*
 * Copyright (C) 2023 Volodymyr Atamanenko
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

/**
 * @file  patch.c
 * @brief Patching some of the .so internal functions or bridging them to native
 *        for better compatibility.
 */

#include <kubridge.h>
#include <so_util/so_util.h>

#include <stdint.h>
#include <string.h>

#include "input.h"
#include "reimpl/softfloat.h"
#include "utils/logger.h"
#include "utils/settings.h"

extern so_module so_mod;

// _FileHandler layout (Files_OpenFileOfType): +0x00 data (malloc'ed),
// +0x9c size in bytes.
#define FH_DATA 0x00
#define FH_SIZE 0x9c

static int (*Files_OpenFileOfType)(uint8_t *fh, char *name, char *ext);

// Files_OpenFileAltType(), reimplemented 1:1 from the pseudo-C (try `ext`,
// then the name's own extension) plus one fix: when the file is found nowhere
// the engine leaves the handler untouched, and Sounds_AddSound() doesn't check
// the result, so FMOD_System_CreateSound() got stack garbage as data/length
// and crashed copying from it (log 010). Clearing the handler makes FMOD fail
// cleanly with INVALID_PARAM and the sound slot stays -1, which every
// Sounds_* function ignores.
static int Files_OpenFileAltType_hook(uint8_t *fh, char *name, char *ext) {
    *(void **) (fh + FH_DATA) = NULL;
    *(uint32_t *) (fh + FH_SIZE) = 0;

    char *dot = strrchr(name, '.');
    if (!dot)
        return 0;

    char base[128];
    size_t len = dot - name;
    if (len >= sizeof(base))
        len = sizeof(base) - 1;
    memcpy(base, name, len);
    base[len] = '\0';

    if (Files_OpenFileOfType(fh, base, ext) || Files_OpenFileOfType(fh, base, dot + 1))
        return 1;

    *(void **) (fh + FH_DATA) = NULL;
    *(uint32_t *) (fh + FH_SIZE) = 0;
    return 0;
}

// The .so's libzip was built against bionic, whose ferror() is a macro that
// reads fp->_flags (u16 at +0x0C) & __SERR (0x40) straight from the FILE. Our
// FILE* come from SceLibc, a different layout: whether that bit is set depends
// on which FILE slot fopen() hands out. With the first slot it happens to be 0
// and the APK opens; LoadGameData() never fclose()s CarnivoresData.dt, so once
// a save exists the APK gets the second slot, _zip_find_central_dir() sees a
// "read error", zip_open() fails, every asset is missing -> black screen
// (log 011). Each site is Thumb `ldrh r3, [r3, #12]`; replace it with
// `movs r3, #0` so ferror() is always 0 (real errors still show up as short
// reads, which libzip checks separately).
static const uint32_t zip_ferror_sites[] = {
    0x6816c, // _zip_cdir_write
    0x688f0, // _zip_dirent_write
    0x69d64, // _zip_readcdir
    0x69d8a, // _zip_readcdir
    0x6a464, // _zip_find_central_dir
};

static void zip_ferror_patch(void) {
    static const uint16_t ldrh_r3 = 0x899b, movs_r3_0 = 0x2300;
    int n = 0;
    for (unsigned i = 0; i < sizeof(zip_ferror_sites) / sizeof(zip_ferror_sites[0]); i++) {
        uint16_t *p = (uint16_t *) (so_mod.text_base + zip_ferror_sites[i]);
        if (*p != ldrh_r3) {
            l_error("zip ferror patch: unexpected 0x%04x at 0x%05x", *p, (unsigned) zip_ferror_sites[i]);
            continue;
        }
        kuKernelCpuUnrestrictedMemcpy(p, &movs_r3_0, sizeof(movs_r3_0));
        n++;
    }
    l_success("zip ferror patch: %d/%d sites patched.", n,
              (int) (sizeof(zip_ferror_sites) / sizeof(zip_ferror_sites[0])));
}

#ifdef DEBUG_SOLOADER
// Debug only: log every zip_open() with libzip's error code (*zep), e.g.
// 5 ZIP_ER_READ, 11 ZIP_ER_OPEN, 19 ZIP_ER_NOZIP, 21 ZIP_ER_INCONS.
static so_hook zip_open_hook;

static void *zip_open_dbg(const char *path, int flags, int *zep) {
    void *za = SO_CONTINUE(void *, zip_open_hook, path, flags, zep);
    if (za)
        l_debug("zip_open(%s): %p", path ? path : "(null)", za);
    else
        l_warn("zip_open(%s) failed: zip error %d", path ? path : "(null)", zep ? *zep : -1);
    return za;
}
#endif

void so_patch(void) {
    // Soft-float emulation -> VFP (the biggest CPU cost of an armeabi .so).
    if (setting_vfpFloat)
        softfloat_patch();

    // libzip must not read bionic FILE fields from a SceLibc FILE (see above).
    zip_ferror_patch();
#ifdef DEBUG_SOLOADER
    uintptr_t zo = so_symbol(&so_mod, "zip_open");
    if (zo)
        zip_open_hook = hook_addr(zo, (uintptr_t) &zip_open_dbg);
#endif

    // Missing files must not reach FMOD as garbage (see above).
    Files_OpenFileOfType = (void *) so_symbol(&so_mod, "_Z20Files_OpenFileOfTypeP12_FileHandlerPcS1_");
    uintptr_t alt = so_symbol(&so_mod, "_Z21Files_OpenFileAltTypeP12_FileHandlerPcS1_");
    if (Files_OpenFileOfType && alt)
        hook_addr(alt, (uintptr_t) &Files_OpenFileAltType_hook);

    // Right stick camera: feed the stick into the engine's own camera input.
    input_patch();
}
