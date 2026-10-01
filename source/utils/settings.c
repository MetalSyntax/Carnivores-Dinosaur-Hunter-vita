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

int  setting_lookSensitivity;
bool setting_invertLookY;
bool setting_invertLookX;
bool setting_swapSticks;
int  setting_msaa;
bool setting_engineLog;
bool setting_vfpFloat;
int  setting_hudOpacity;

void settings_reset() {
    setting_lookSensitivity = 100; // percent, right analog stick camera speed
    setting_invertLookY     = false;
    setting_invertLookX     = false;
    setting_swapSticks      = false; // true: left stick looks, right stick moves
    setting_msaa      = 0;     // 0 off, 1 2x, 2 4x
    setting_engineLog = false; // engine __android_log_* spam -> log file
    setting_vfpFloat  = true;  // run the engine's soft-float math on the VFP
    setting_hudOpacity = 1;    // percent, touch HUD buttons (physical controls)
}

void settings_load() {
    settings_reset();

    char buffer[30];
    int value;

    FILE *config = fopen(CONFIG_FILE_PATH, "r");

    if (config) {
        while (EOF != fscanf(config, "%29[^ ] %d\n", buffer, &value)) {
            if      (strcmp("look_sensitivity", buffer) == 0) setting_lookSensitivity = value;
            else if (strcmp("invert_look_y", buffer) == 0) setting_invertLookY = (bool)value;
            else if (strcmp("invert_look_x", buffer) == 0) setting_invertLookX = (bool)value;
            else if (strcmp("swap_sticks", buffer) == 0) setting_swapSticks = (bool)value;
            else if (strcmp("msaa", buffer) == 0)       setting_msaa      = value;
            else if (strcmp("engine_log", buffer) == 0) setting_engineLog = (bool)value;
            else if (strcmp("vfp_float", buffer) == 0)  setting_vfpFloat  = (bool)value;
            else if (strcmp("hud_opacity", buffer) == 0) setting_hudOpacity = value;
        }
        fclose(config);
    }

    if (setting_msaa < 0 || setting_msaa > 2) setting_msaa = 0;
    if (setting_lookSensitivity < 10) setting_lookSensitivity = 10;
    if (setting_lookSensitivity > 400) setting_lookSensitivity = 400;
    if (setting_hudOpacity < 0) setting_hudOpacity = 0;
    if (setting_hudOpacity > 100) setting_hudOpacity = 100;

    // Rewrite on every boot so new keys show up in config.txt.
    settings_save();
}

void settings_save() {
    FILE *config = fopen(CONFIG_FILE_PATH, "w+");

    if (config) {
        fprintf(config, "%s %d\n", "look_sensitivity", setting_lookSensitivity);
        fprintf(config, "%s %d\n", "invert_look_y", (int)setting_invertLookY);
        fprintf(config, "%s %d\n", "invert_look_x", (int)setting_invertLookX);
        fprintf(config, "%s %d\n", "swap_sticks", (int)setting_swapSticks);
        fprintf(config, "%s %d\n", "msaa", setting_msaa);
        fprintf(config, "%s %d\n", "engine_log", (int)setting_engineLog);
        fprintf(config, "%s %d\n", "vfp_float", (int)setting_vfpFloat);
        fprintf(config, "%s %d\n", "hud_opacity", setting_hudOpacity);
        fclose(config);
    }
}
