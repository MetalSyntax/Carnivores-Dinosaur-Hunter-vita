/*
 * Copyright (C) 2026 Carnivores Vita port contributors
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

/**
 * @file  trophy.c
 * @brief PS Vita native trophies via sceNpTrophy + NoTrpDrm.
 *
 * Same implementation as the Prince of Persia Classic port (v01.55, works on
 * real HW):
 * - comm_id is the bare 9-char base ("CRDH00001"); the TRP <npcommid> and the
 *   VPK folder are "CRDH00001_00". param.sfo carries NO NP_COMMUNICATION_ID.
 * - The TRP must be readable from app0: BEFORE any NP call. v1.0.0 shipped no
 *   TRP: without NoTrpDrm CreateContext failed and the game went on, but with
 *   NoTrpDrm it reached the setup dialog with nothing to install and never
 *   booted.
 * - One persistent handle; the cache bit is set only after the OS confirms.
 * - Platinum (id 0) is never unlocked by hand: the OS does it.
 */

#include "trophy.h"
#include "logger.h"

#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/clib.h>
#include <psp2/io/fcntl.h>
#include <psp2/sysmodule.h>
#include <psp2/display.h>
#include <psp2/common_dialog.h>

#include <vitaGL.h>
#include <string.h>

typedef struct {
    int sdkVersion;
    SceCommonDialogParam commonParam;
    int context;
    int options;
    uint8_t reserved[128];
} SceNpTrophySetupDialogParam;

typedef struct {
    uint32_t bits[4]; // 128 trophies max
} SceNpTrophyFlagArray;

int sceNpTrophyInit(void *unk);
int sceNpTrophyCreateContext(int *context, const char *commId, const void *commSign, uint64_t options);
int sceNpTrophySetupDialogInit(SceNpTrophySetupDialogParam *param);
SceCommonDialogStatus sceNpTrophySetupDialogGetStatus(void);
int sceNpTrophySetupDialogTerm(void);
int sceNpTrophyCreateHandle(int *handle);
int sceNpTrophyDestroyHandle(int handle);
int sceNpTrophyUnlockTrophy(int ctx, int handle, int id, int *plat_id);
int sceNpTrophyGetTrophyUnlockState(int ctx, int handle, SceNpTrophyFlagArray *flags, uint32_t *count);

#define SCE_NP_TROPHY_ERROR_ALREADY_INITIALIZED     ((int)0x80551602)
#define SCE_NP_TROPHY_ERROR_TROPHY_ALREADY_UNLOCKED ((int)0x8055160F)

#define TRP_COMM_ID     "CRDH00001"
#define TRP_PATH        "app0:sce_sys/trophy/" TRP_COMM_ID "_00/TROPHY.TRP"
#define TRP_MAX         128
#define TRP_QUEUE_SIZE  32
// ponytail: safety cap only -- the real dialog finishes in ~1-2 s.
#define TRP_DIALOG_MAX_FRAMES (60 * 30)

static const char trp_comm_id[16] = TRP_COMM_ID;
// Placeholder signature Rinnegatamante ships (NoTrpDrm skips the check).
static const uint8_t trp_comm_sign[160] = { 0xb9, 0xdd, 0xe1, 0x3b, 0x01, 0x00 };

static int trp_ctx = -1;
static int trp_handle = -1;
static SceNpTrophyFlagArray trp_flags;
static uint32_t trp_count = 0;
static int trophies_available = 0;

static uint32_t trp_queue[TRP_QUEUE_SIZE];
static int trp_queue_head = 0;
static int trp_queue_tail = 0;
static SceUID trp_queue_mutex = -1;
static SceUID trp_request_sema = -1;
static SceUID trp_api_mutex = -1; // guards trp_handle + trp_flags

static void trp_flag_set(uint32_t id) { trp_flags.bits[id >> 5] |= 1u << (id & 31u); }
static int  trp_flag_get(uint32_t id) { return (trp_flags.bits[id >> 5] >> (id & 31u)) & 1u; }

// Worker thread: the render loop never blocks on trophy IO.
static int trophies_unlocker(SceSize args, void *argp) {
    while (1) {
        sceKernelWaitSema(trp_request_sema, 1, NULL);

        sceKernelWaitSema(trp_queue_mutex, 1, NULL);
        if (trp_queue_head == trp_queue_tail) {
            sceKernelSignalSema(trp_queue_mutex, 1);
            continue;
        }
        uint32_t id = trp_queue[trp_queue_tail];
        trp_queue_tail = (trp_queue_tail + 1) % TRP_QUEUE_SIZE;
        sceKernelSignalSema(trp_queue_mutex, 1);

        sceKernelWaitSema(trp_api_mutex, 1, NULL);
        int plat_id = -1;
        int res = sceNpTrophyUnlockTrophy(trp_ctx, trp_handle, (int)id, &plat_id);
        if (res == 0 || res == SCE_NP_TROPHY_ERROR_TROPHY_ALREADY_UNLOCKED) {
            trp_flag_set(id);
            if (plat_id >= 0 && plat_id < TRP_MAX)
                trp_flag_set((uint32_t)plat_id);
            l_error("trophy: %u unlocked (res=0x%08X, plat=%d)", (unsigned)id, (unsigned)res, plat_id);
        } else {
            l_error("trophy: UnlockTrophy(%u) failed: 0x%08X", (unsigned)id, (unsigned)res);
        }
        sceKernelSignalSema(trp_api_mutex, 1);
    }
    return 0;
}

int trophy_init(void) {
    sceClibMemset(&trp_flags, 0, sizeof(trp_flags));

    SceUID fd = sceIoOpen(TRP_PATH, SCE_O_RDONLY, 0);
    if (fd < 0) {
        l_error("trophy: %s not readable (0x%08X), trophies disabled", TRP_PATH, (unsigned)fd);
        return fd;
    }
    sceIoClose(fd);

    sceSysmoduleLoadModule(SCE_SYSMODULE_NP_TROPHY);

    int res = sceNpTrophyInit(NULL);
    if (res < 0 && res != SCE_NP_TROPHY_ERROR_ALREADY_INITIALIZED) {
        l_error("trophy: sceNpTrophyInit failed: 0x%08X", (unsigned)res);
        return res;
    }

    res = sceNpTrophyCreateContext(&trp_ctx, trp_comm_id, trp_comm_sign, 0);
    if (res < 0) {
        l_error("trophy: CreateContext failed: 0x%08X (NoTrpDrm missing?)", (unsigned)res);
        return res;
    }

    // Installs/registers the TRP on first run; needs a live display (after gl_init).
    SceNpTrophySetupDialogParam setup;
    sceClibMemset(&setup, 0, sizeof(setup));
    _sceCommonDialogSetMagicNumber(&setup.commonParam);
    setup.sdkVersion = PSP2_SDK_VERSION;
    setup.context = trp_ctx;

    res = sceNpTrophySetupDialogInit(&setup);
    if (res >= 0) {
        int frames = 0;
        while (sceNpTrophySetupDialogGetStatus() == SCE_COMMON_DIALOG_STATUS_RUNNING) {
            if (++frames > TRP_DIALOG_MAX_FRAMES) {
                l_error("trophy: setup dialog stuck, trophies disabled");
                sceNpTrophySetupDialogTerm();
                return -1;
            }
            sceDisplayWaitVblankStart();
            vglSwapBuffers(GL_TRUE);
        }
        sceNpTrophySetupDialogTerm();
    } else {
        l_error("trophy: SetupDialogInit returned 0x%08X", (unsigned)res);
    }

    res = sceNpTrophyCreateHandle(&trp_handle);
    if (res < 0) {
        l_error("trophy: CreateHandle failed: 0x%08X", (unsigned)res);
        return res;
    }

    uint32_t count = 0;
    res = sceNpTrophyGetTrophyUnlockState(trp_ctx, trp_handle, &trp_flags, &count);
    if (res < 0 || count == 0 || count > TRP_MAX) {
        l_error("trophy: GetTrophyUnlockState failed: 0x%08X (count=%u)", (unsigned)res, (unsigned)count);
        sceNpTrophyDestroyHandle(trp_handle);
        trp_handle = -1;
        return res < 0 ? res : -1;
    }
    trp_count = count;

    trp_queue_mutex  = sceKernelCreateSema("trp_queue_mutex", 0, 1, 1, NULL);
    trp_request_sema = sceKernelCreateSema("trp_request_sema", 0, 0, TRP_QUEUE_SIZE, NULL);
    trp_api_mutex    = sceKernelCreateSema("trp_api_mutex", 0, 1, 1, NULL);
    if (trp_queue_mutex < 0 || trp_request_sema < 0 || trp_api_mutex < 0)
        return -1;

    SceUID thid = sceKernelCreateThread("trophies_unlocker", trophies_unlocker, 0x10000100, 0x10000, 0, 0, NULL);
    if (thid < 0 || sceKernelStartThread(thid, 0, NULL) < 0)
        return -1;

    trophies_available = 1;
    l_error("trophy: OK, %u trophies ready", (unsigned)trp_count);
    return 0;
}

uint8_t trophy_is_unlocked(uint32_t id) {
    if (!trophies_available || id >= trp_count)
        return 0;
    sceKernelWaitSema(trp_api_mutex, 1, NULL);
    int v = trp_flag_get(id);
    sceKernelSignalSema(trp_api_mutex, 1);
    return (uint8_t)v;
}

void trophy_unlock(uint32_t id) {
    // id 0 is the platinum: only the OS may unlock it.
    if (!trophies_available || id == 0 || id >= trp_count || trophy_is_unlocked(id))
        return;

    sceKernelWaitSema(trp_queue_mutex, 1, NULL);
    int next = (trp_queue_head + 1) % TRP_QUEUE_SIZE;
    if (next != trp_queue_tail) {
        trp_queue[trp_queue_head] = id;
        trp_queue_head = next;
        sceKernelSignalSema(trp_request_sema, 1);
    } else {
        l_error("trophy: queue full, dropped %u", (unsigned)id);
    }
    sceKernelSignalSema(trp_queue_mutex, 1);
}
