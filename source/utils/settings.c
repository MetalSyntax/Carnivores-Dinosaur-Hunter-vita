/*
 * Copyright (C) 2021      Andy Nguyen
 * Copyright (C) 2022-2023 Volodymyr Atamanenko
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

#include <stdio.h>
#include <string.h>
#include "settings.h"

#define CONFIG_FILE_PATH DATA_PATH"config.txt"

int  setting_msaa;
bool setting_engineLog;
bool setting_vfpFloat;

void settings_reset() {
    setting_msaa      = 0;     // 0 off, 1 2x, 2 4x
    setting_engineLog = false; // engine __android_log_* spam -> log file
    setting_vfpFloat  = true;  // run the engine's soft-float math on the VFP
}

void settings_load() {
    settings_reset();

    char buffer[30];
    int value;

    FILE *config = fopen(CONFIG_FILE_PATH, "r");

    if (config) {
        while (EOF != fscanf(config, "%29[^ ] %d\n", buffer, &value)) {
            if      (strcmp("msaa", buffer) == 0)       setting_msaa      = value;
            else if (strcmp("engine_log", buffer) == 0) setting_engineLog = (bool)value;
            else if (strcmp("vfp_float", buffer) == 0)  setting_vfpFloat  = (bool)value;
        }
        fclose(config);
    }

    if (setting_msaa < 0 || setting_msaa > 2) setting_msaa = 0;
}

void settings_save() {
    FILE *config = fopen(CONFIG_FILE_PATH, "w+");

    if (config) {
        fprintf(config, "%s %d\n", "msaa", setting_msaa);
        fprintf(config, "%s %d\n", "engine_log", (int)setting_engineLog);
        fprintf(config, "%s %d\n", "vfp_float", (int)setting_vfpFloat);
        fclose(config);
    }
}
