#include "audio.h"
#include "utils/logger.h"

#include <psp2/audioout.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/clib.h>
#include <falso_jni/FalsoJNI.h>
#include <so_util/so_util.h>
#include <stdlib.h>

extern so_module so_mod_fmod;

static SceUID audio_thread_id = -1;
static volatile int audio_running = 0;
static volatile int audio_paused = 0;
static int audio_port = -1;

static int (*fmodGetInfo)(void *env, void *thiz, int info) = NULL;
static int (*fmodProcess)(void *env, void *thiz, void *buf) = NULL;

static int audio_thread_func(SceSize args, void *argp) {
    l_info("Audio thread started.");

    fmodGetInfo = (void *)so_symbol(&so_mod_fmod, "Java_org_fmod_FMODAudioDevice_fmodGetInfo");
    fmodProcess = (void *)so_symbol(&so_mod_fmod, "Java_org_fmod_FMODAudioDevice_fmodProcess");

    if (!fmodGetInfo || !fmodProcess) {
        l_error("Failed to resolve FMODAudioDevice native symbols!");
        return -1;
    }

    int audio_initialized = 0;
    int buffer_len = 512;
    int sample_rate = 44100;
    short *pcm_buffer = NULL;

    while (audio_running) {
        if (audio_paused) {
            sceKernelDelayThread(20000);
            continue;
        }

        if (!audio_initialized) {
            int sr = fmodGetInfo(&jni, NULL, 0); // FMOD_INFO_SAMPLERATE
            if (sr > 0) {
                sample_rate = sr;
                int bl = fmodGetInfo(&jni, NULL, 1); // FMOD_INFO_DSPBUFFERLENGTH
                if (bl > 0) {
                    buffer_len = bl;
                }
                // Vita expects buffer length as a multiple of 64 between 64 and 4096
                if (buffer_len < 64) buffer_len = 64;
                if (buffer_len > 4096) buffer_len = 4096;
                buffer_len = (buffer_len + 63) & ~63;

                l_info("FMOD audio init: sample_rate=%d, buffer_len=%d", sample_rate, buffer_len);

                audio_port = sceAudioOutOpenPort(SCE_AUDIO_OUT_PORT_TYPE_BGM, buffer_len, sample_rate, SCE_AUDIO_OUT_PARAM_FORMAT_S16_STEREO);
                if (audio_port < 0) {
                    l_error("sceAudioOutOpenPort failed: 0x%08x", audio_port);
                    sceKernelDelayThread(100000);
                    continue;
                }

                pcm_buffer = (short *)malloc(buffer_len * 2 * sizeof(short)); // Stereo 16-bit
                audio_initialized = 1;
            } else {
                sceKernelDelayThread(50000); // 50ms wait
            }
        } else {
            // Check if mixer is running
            if (fmodGetInfo(&jni, NULL, 3) == 1) { // FMOD_INFO_MIXERRUNNING
                fmodProcess(&jni, NULL, pcm_buffer);
                sceAudioOutOutput(audio_port, pcm_buffer);
            } else {
                sceKernelDelayThread(10000); // 10ms wait
            }
        }
    }

    if (audio_port >= 0) {
        sceAudioOutReleasePort(audio_port);
        audio_port = -1;
    }
    if (pcm_buffer) {
        free(pcm_buffer);
        pcm_buffer = NULL;
    }

    l_info("Audio thread finished.");
    return 0;
}

void audio_init(void) {
    if (audio_running) return;

    audio_running = 1;
    audio_paused = 0;

    audio_thread_id = sceKernelCreateThread("fmod_audio_thread", audio_thread_func, 0x10000100 - 10, 0x10000, 0, 0, NULL);
    if (audio_thread_id >= 0) {
        sceKernelStartThread(audio_thread_id, 0, NULL);
    } else {
        l_error("Failed to create fmod_audio_thread: 0x%08x", audio_thread_id);
    }
}

void audio_term(void) {
    audio_running = 0;
    if (audio_thread_id >= 0) {
        sceKernelWaitThreadEnd(audio_thread_id, NULL, NULL);
        sceKernelDeleteThread(audio_thread_id);
        audio_thread_id = -1;
    }
}

void audio_pause(void) {
    audio_paused = 1;
}

void audio_resume(void) {
    audio_paused = 0;
}
