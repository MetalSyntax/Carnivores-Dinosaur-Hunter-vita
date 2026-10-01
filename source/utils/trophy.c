/*
 * Copyright (C) 2026 Carnivores Vita port contributors
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

/**
 * @file  trophy.c
 * @brief PS Vita native trophy system integration via sceNpTrophy.
 */

#include "trophy.h"
#include "logger.h"

#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/clib.h>
#include <psp2/sysmodule.h>
#include <psp2/common_dialog.h>

#include <vitaGL.h>
#include <stdio.h>
#include <string.h>

#define TROPHY_COMM_ID "PSVCDH001"
#define TROPHY_QUEUE_SIZE 16

static char comm_id[16] = TROPHY_COMM_ID;
static char signature[160] = { 0xb9, 0xdd, 0xe1, 0x3b, 0x01, 0x00 };

static int trp_ctx = -1;
static int plat_id = -1;
static int trophies_available = 0;

typedef struct {
    int sdkVersion;
    SceCommonDialogParam commonParam;
    int context;
    int options;
    uint8_t reserved[128];
} SceNpTrophySetupDialogParam;

typedef struct {
    uint32_t unk[4];
} SceNpTrophyUnlockState;

static SceNpTrophyUnlockState trophies_unlocks;

// SceNpTrophy function prototypes
int sceNpTrophyInit(void *unk);
int sceNpTrophyTerm(void);
int sceNpTrophyCreateContext(int *context, const char *commId, const char *commSign, uint64_t options);
int sceNpTrophyDestroyContext(int context);
int sceNpTrophySetupDialogInit(SceNpTrophySetupDialogParam *param);
SceCommonDialogStatus sceNpTrophySetupDialogGetStatus(void);
int sceNpTrophySetupDialogTerm(void);
int sceNpTrophyCreateHandle(int *handle);
int sceNpTrophyDestroyHandle(int handle);
int sceNpTrophyUnlockTrophy(int ctx, int handle, int id, int *plat_id);
int sceNpTrophyGetTrophyUnlockState(int ctx, int handle, SceNpTrophyUnlockState *state, uint32_t *count);

// Asynchronous unlock queue
static volatile int trp_queue[TROPHY_QUEUE_SIZE];
static volatile int trp_q_head = 0;
static volatile int trp_q_tail = 0;
static SceUID trp_request_sema = -1;
static SceUID trp_worker_thd = -1;
static volatile int trp_worker_running = 0;

static int trophies_unlocker(SceSize args, void *argp) {
    l_info("trophy: worker thread started");
    while (trp_worker_running) {
        sceKernelWaitSema(trp_request_sema, 1, NULL);
        if (!trp_worker_running)
            break;

        while (trp_q_tail != trp_q_head) {
            int id = trp_queue[trp_q_tail];
            trp_q_tail = (trp_q_tail + 1) % TROPHY_QUEUE_SIZE;

            int trp_handle = -1;
            int ret = sceNpTrophyCreateHandle(&trp_handle);
            if (ret >= 0 && trp_handle >= 0) {
                int unlock_res = sceNpTrophyUnlockTrophy(trp_ctx, trp_handle, id, &plat_id);
                l_info("trophy: unlocked ID %d, res = 0x%08X", id, unlock_res);
                sceNpTrophyDestroyHandle(trp_handle);
            } else {
                l_warn("trophy: sceNpTrophyCreateHandle failed: 0x%08X", ret);
            }
        }
    }
    l_info("trophy: worker thread exiting");
    return 0;
}

int trophy_init(void) {
    l_info("trophy: initializing PS Vita trophies for %s...", comm_id);

    int ret = sceSysmoduleLoadModule(SCE_SYSMODULE_NP_TROPHY);
    if (ret < 0) {
        l_warn("trophy: sceSysmoduleLoadModule(SCE_SYSMODULE_NP_TROPHY) failed: 0x%08X", ret);
        return ret;
    }

    ret = sceNpTrophyInit(NULL);
    if (ret < 0) {
        l_warn("trophy: sceNpTrophyInit failed: 0x%08X", ret);
        return ret;
    }

    ret = sceNpTrophyCreateContext(&trp_ctx, comm_id, signature, 0);
    if (ret < 0) {
        l_warn("trophy: sceNpTrophyCreateContext returned 0x%08X (trophies disabled or NoTrpDrm missing)", ret);
        return ret;
    }

    // Run trophy setup dialog
    SceNpTrophySetupDialogParam setupParam;
    sceClibMemset(&setupParam, 0, sizeof(SceNpTrophySetupDialogParam));
    _sceCommonDialogSetMagicNumber(&setupParam.commonParam);
    setupParam.sdkVersion = PSP2_SDK_VERSION;
    setupParam.options = 0;
    setupParam.context = trp_ctx;

    ret = sceNpTrophySetupDialogInit(&setupParam);
    if (ret >= 0) {
        while (sceNpTrophySetupDialogGetStatus() == SCE_COMMON_DIALOG_STATUS_RUNNING) {
            vglSwapBuffers(GL_TRUE);
        }
        sceNpTrophySetupDialogTerm();
    }

    // Start background unlocker thread
    trp_worker_running = 1;
    trp_request_sema = sceKernelCreateSema("trps_request_sema", 0, 0, TROPHY_QUEUE_SIZE, NULL);
    trp_worker_thd = sceKernelCreateThread("trophies_unlocker", &trophies_unlocker, 0x10000100, 0x10000, 0, 0, NULL);
    if (trp_worker_thd >= 0) {
        sceKernelStartThread(trp_worker_thd, 0, NULL);
    }

    // Fetch initial unlock state
    int trp_handle = -1;
    uint32_t count = 0;
    ret = sceNpTrophyCreateHandle(&trp_handle);
    if (ret >= 0) {
        sceNpTrophyGetTrophyUnlockState(trp_ctx, trp_handle, &trophies_unlocks, &count);
        sceNpTrophyDestroyHandle(trp_handle);
    }

    trophies_available = 1;
    l_success("trophy: native trophies successfully initialized and active.");
    return 0;
}

uint8_t trophy_is_unlocked(uint32_t id) {
    if (!trophies_available || id == 0 || id > 128)
        return 0;

    uint32_t word = (id - 1) >> 5;
    uint32_t bit  = (id - 1) & 31;
    if (word < 4) {
        return (trophies_unlocks.unk[word] & (1u << bit)) != 0;
    }
    return 0;
}

void trophy_unlock(uint32_t id) {
    if (!trophies_available || id == 0 || id > 128)
        return;

    if (trophy_is_unlocked(id)) {
        l_debug("trophy: ID %u already unlocked, skipping.", (unsigned)id);
        return;
    }

    // Mark as unlocked in local cache immediately
    uint32_t word = (id - 1) >> 5;
    uint32_t bit  = (id - 1) & 31;
    if (word < 4) {
        trophies_unlocks.unk[word] |= (1u << bit);
    }

    // Enqueue for async worker
    int next_head = (trp_q_head + 1) % TROPHY_QUEUE_SIZE;
    if (next_head != trp_q_tail) {
        trp_queue[trp_q_head] = (int)id;
        trp_q_head = next_head;
        sceKernelSignalSema(trp_request_sema, 1);
    } else {
        l_warn("trophy: unlock queue full, dropped ID %u", (unsigned)id);
    }
}

void trophy_term(void) {
    if (!trophies_available)
        return;

    trophies_available = 0;
    trp_worker_running = 0;

    if (trp_request_sema >= 0) {
        sceKernelSignalSema(trp_request_sema, 1);
        if (trp_worker_thd >= 0) {
            sceKernelWaitThreadEnd(trp_worker_thd, NULL, NULL);
            sceKernelDeleteThread(trp_worker_thd);
            trp_worker_thd = -1;
        }
        sceKernelDeleteSema(trp_request_sema);
        trp_request_sema = -1;
    }

    if (trp_ctx >= 0) {
        sceNpTrophyDestroyContext(trp_ctx);
        trp_ctx = -1;
    }

    sceNpTrophyTerm();
    sceSysmoduleUnloadModule(SCE_SYSMODULE_NP_TROPHY);
    l_info("trophy: system terminated.");
}
