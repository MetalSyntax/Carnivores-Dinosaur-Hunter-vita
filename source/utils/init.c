/*
 * Copyright (C) 2021      Andy Nguyen
 * Copyright (C) 2021-2022 Rinnegatamante
 * Copyright (C) 2022-2024 Volodymyr Atamanenko
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

#include "utils/init.h"

#include "utils/dialog.h"
#include "utils/glutil.h"
#include "utils/logger.h"
#include "utils/utils.h"
#include "utils/settings.h"

#include <stdio.h>
#include <string.h>

#include <psp2/appmgr.h>
#include <psp2/apputil.h>
#include <psp2/kernel/clib.h>
#include <psp2/power.h>
#include <psp2/io/stat.h>

#include <falso_jni/FalsoJNI.h>
#include <so_util/so_util.h>
#include <fios/fios.h>

#define LOAD_ADDRESS_FMOD 0x98000000
#define LOAD_ADDRESS_GAME 0x98400000

extern so_module so_mod_fmod;
extern so_module so_mod;

void soloader_init_all() {
    // Launch `app0:configurator.bin` on `-config` init param
    sceAppUtilInit(&(SceAppUtilInitParam){}, &(SceAppUtilBootParam){});
    SceAppUtilAppEventParam eventParam;
    sceClibMemset(&eventParam, 0, sizeof(SceAppUtilAppEventParam));
    sceAppUtilReceiveAppEvent(&eventParam);
    if (eventParam.type == 0x05) {
        char buffer[2048];
        sceAppUtilAppEventParseLiveArea(&eventParam, buffer);
        if (strstr(buffer, "-config"))
            sceAppMgrLoadExec("app0:/configurator.bin", NULL, NULL);
    }

    // Set default overclock values
    scePowerSetArmClockFrequency(444);
    scePowerSetBusClockFrequency(222);
    scePowerSetGpuClockFrequency(222);
    scePowerSetGpuXbarClockFrequency(166);

    // Create required directories
    sceIoMkdir(DATA_PATH, 0777);
    sceIoMkdir(DATA_PATH "logs", 0777);
    sceIoMkdir(DATA_PATH ".dinhunter", 0777);
    sceIoMkdir(DATA_PATH ".dinhunter/photos", 0777);

#ifdef USE_SCELIBC_IO
    if (fios_init(DATA_PATH) == 0)
        l_success("FIOS initialized.");
#endif

    if (!module_loaded("kubridge")) {
        l_fatal("kubridge is not loaded.");
        fatal_error("Error: kubridge.skprx is not installed.");
    }
    l_success("kubridge check passed.");

    // 1. Load FMOD library
    char fmod_path[256];
    if (file_exists(DATA_PATH "libfmodex.so")) {
        snprintf(fmod_path, sizeof(fmod_path), DATA_PATH "libfmodex.so");
    } else if (file_exists(DATA_PATH "fmod.so")) {
        snprintf(fmod_path, sizeof(fmod_path), DATA_PATH "fmod.so");
    } else {
        fatal_error("Error: could not find libfmodex.so in %s", DATA_PATH);
    }

    l_info("Loading FMOD library: %s", fmod_path);
    if (so_file_load(&so_mod_fmod, fmod_path, LOAD_ADDRESS_FMOD) < 0) {
        l_fatal("FMOD SO could not be loaded.");
        fatal_error("Error: could not load %s.", fmod_path);
    }

    so_relocate(&so_mod_fmod);
    l_success("FMOD SO relocated.");

    resolve_imports(&so_mod_fmod);
    l_success("FMOD SO imports resolved.");

    so_flush_caches(&so_mod_fmod);
    l_success("FMOD SO caches flushed.");

    so_initialize(&so_mod_fmod);
    l_success("FMOD SO initialized.");

    // 2. Load Game library
    char game_path[256];
    if (file_exists(DATA_PATH "libdinHunter.so")) {
        snprintf(game_path, sizeof(game_path), DATA_PATH "libdinHunter.so");
    } else if (file_exists(SO_PATH)) {
        snprintf(game_path, sizeof(game_path), "%s", SO_PATH);
    } else {
        fatal_error("Error: could not find libdinHunter.so or main.so in %s", DATA_PATH);
    }

    l_info("Loading Game library: %s", game_path);
    if (so_file_load(&so_mod, game_path, LOAD_ADDRESS_GAME) < 0) {
        l_fatal("Game SO could not be loaded.");
        fatal_error("Error: could not load %s.", game_path);
    }

    settings_load();
    l_success("Settings loaded.");

    so_relocate(&so_mod);
    l_success("Game SO relocated.");

    resolve_imports(&so_mod);
    l_success("Game SO imports resolved.");

    so_patch();
    l_success("Game SO patched.");

    so_flush_caches(&so_mod);
    l_success("Game SO caches flushed.");

    so_initialize(&so_mod);
    l_success("Game SO initialized.");

    gl_preload();
    l_success("OpenGL preloaded.");

    jni_init();
    l_success("FalsoJNI initialized.");
}
