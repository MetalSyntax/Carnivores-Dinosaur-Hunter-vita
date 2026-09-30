/*
 * Copyright (C) 2022-2023 Volodymyr Atamanenko
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

/**
 * @file  settings.h
 * @brief Loader settings that can be set via a configurator app.
 */

#ifndef SOLOADER_SETTINGS_H
#define SOLOADER_SETTINGS_H

#include "stdbool.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Right analog stick camera speed, percent (10..400). */
extern int  setting_lookSensitivity;
extern bool setting_invertLookY;
/** 0 off, 1 2x, 2 4x MSAA (GPU cost at 960x544 in the 3D scenes). */
extern int  setting_msaa;
/** Forward the engine's __android_log_* spam to the log file. */
extern bool setting_engineLog;
/** Run the engine's soft-float math on the VFP (reimpl/softfloat.c). */
extern bool setting_vfpFloat;
/** Opacity of the touch HUD buttons, percent (0..100; compass not affected). */
extern int  setting_hudOpacity;

void settings_load();
void settings_save();
void settings_reset();

#ifdef __cplusplus
};
#endif

#endif // SOLOADER_SETTINGS_H
