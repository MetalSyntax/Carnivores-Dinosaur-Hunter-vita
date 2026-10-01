/*
 * Copyright (C) 2026 Carnivores Vita port contributors
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

/**
 * @file  trophy.h
 * @brief PS Vita native trophy system integration via sceNpTrophy.
 */

#ifndef SOLOADER_TROPHY_H
#define SOLOADER_TROPHY_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize PS Vita trophy subsystem using the game's Title ID.
 * @return 0 on success, or negative error code if unavailable (e.g. NoTrpDrm not installed).
 */
int trophy_init(void);

/**
 * @brief Unlock a trophy by ID (1-based index matching TRP definition).
 * @param id Trophy index (1..N).
 */
void trophy_unlock(uint32_t id);

/**
 * @brief Check if a trophy has already been unlocked.
 * @param id Trophy index (1..N).
 * @return 1 if unlocked, 0 otherwise.
 */
uint8_t trophy_is_unlocked(uint32_t id);

/**
 * @brief Terminate PS Vita trophy subsystem and clean up resources.
 */
void trophy_term(void);

#ifdef __cplusplus
};
#endif

#endif // SOLOADER_TROPHY_H
