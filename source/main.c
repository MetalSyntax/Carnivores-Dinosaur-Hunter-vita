#include "utils/init.h"
#include "utils/glutil.h"
#include "utils/logger.h"
#include "utils/utils.h"
#include "utils/dialog.h"
#include "reimpl/audio.h"

#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/ctrl.h>
#include <psp2/touch.h>
#include <psp2/power.h>
#include <psp2/io/stat.h>

#include <falso_jni/FalsoJNI.h>
#include <so_util/so_util.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int _newlib_heap_size_user = 256 * 1024 * 1024;

#ifdef USE_SCELIBC_IO
int sceLibcHeapSize = 4 * 1024 * 1024;
#endif

so_module so_mod_fmod;
so_module so_mod;

// JNI Entry points from libdinHunter.so
static jboolean (*isNativeAppInitialized)(void *env, void *thiz);
static void (*nativeApplicationDidFinishLaunching)(void *env, void *thiz, jstring baseDir, jstring apkPath);
static void (*nativeResume)(void *env, void *thiz);
static void (*nativePause)(void *env, void *thiz);
static void (*nativeOnBackPressed)(void *env, void *thiz);
static void (*destroyFramebuffer)(void *env, void *thiz);
static void (*nativeSetBundlesPaths)(void *env, void *thiz, jstring bundle1, jstring bundle2);
static void (*nativeSetProductPurchased)(void *env, void *thiz, jstring prodId, jboolean purchased, jstring prodPath);

static void (*touchesBegan)(void *env, void *thiz, jfloat x, jfloat y);
static void (*touchesMoved)(void *env, void *thiz, jfloat x, jfloat y);
static void (*touchesEnded)(void *env, void *thiz, jfloat x, jfloat y);
static void (*touchesCancelled)(void *env, void *thiz, jfloat x, jfloat y);

static void (*createFramebuffer)(void *env, void *thiz, jint w, jint h);
static void (*setEnvironment)(void *env, void *thiz);
static void (*nativeResize)(void *env, void *thiz, jint w, jint h);
static void (*layoutSubviews)(void *env, void *thiz);

static void (*facebook_nativeInit)(void *env, void *thiz);
static void (*social_nativeInit)(void *env, void *thiz);

static void (*Weapon_Fire)(void) = NULL;
static int *game_stage_ptr = NULL;

struct TouchSlot {
    int id;
    float x;
    float y;
};
static struct TouchSlot active_touches[5];

int main(int argc, char *argv[]) {
    l_info("--- Carnivores Dinosaur Hunter (PS Vita) starting ---");

    for (int i = 0; i < 5; i++) {
        active_touches[i].id = -1;
    }

    soloader_init_all();

    int (*JNI_OnLoad_fmod)(void *jvm) = (void *)so_symbol(&so_mod_fmod, "JNI_OnLoad");
    if (JNI_OnLoad_fmod) {
        l_info("Calling JNI_OnLoad for FMOD...");
        JNI_OnLoad_fmod(&jvm);
    }

    int (*JNI_OnLoad)(void *jvm) = (void *)so_symbol(&so_mod, "JNI_OnLoad");
    if (JNI_OnLoad) {
        l_info("Calling JNI_OnLoad for Game...");
        JNI_OnLoad(&jvm);
    }

    // Resolve native symbols
    isNativeAppInitialized = (void *)so_symbol(&so_mod, "Java_com_tatem_dinhunter_DinHunterAndroid_isNativeAppInitialized");
    nativeApplicationDidFinishLaunching = (void *)so_symbol(&so_mod, "Java_com_tatem_dinhunter_DinHunterAndroid_nativeApplicationDidFinishLaunching");
    nativeResume = (void *)so_symbol(&so_mod, "Java_com_tatem_dinhunter_DinHunterAndroid_nativeResume");
    nativePause = (void *)so_symbol(&so_mod, "Java_com_tatem_dinhunter_DinHunterAndroid_nativePause");
    nativeOnBackPressed = (void *)so_symbol(&so_mod, "Java_com_tatem_dinhunter_DinHunterAndroid_nativeOnBackPressed");
    destroyFramebuffer = (void *)so_symbol(&so_mod, "Java_com_tatem_dinhunter_DinHunterAndroid_destroyFramebuffer");
    nativeSetBundlesPaths = (void *)so_symbol(&so_mod, "Java_com_tatem_dinhunter_DinHunterAndroid_nativeSetBundlesPaths");
    nativeSetProductPurchased = (void *)so_symbol(&so_mod, "Java_com_tatem_dinhunter_DinHunterAndroid_nativeSetProductPurchased");

    touchesBegan = (void *)so_symbol(&so_mod, "Java_com_tatem_dinhunter_DinHunterGLSurface_touchesBegan");
    touchesMoved = (void *)so_symbol(&so_mod, "Java_com_tatem_dinhunter_DinHunterGLSurface_touchesMoved");
    touchesEnded = (void *)so_symbol(&so_mod, "Java_com_tatem_dinhunter_DinHunterGLSurface_touchesEnded");
    touchesCancelled = (void *)so_symbol(&so_mod, "Java_com_tatem_dinhunter_DinHunterGLSurface_touchesCancelled");

    createFramebuffer = (void *)so_symbol(&so_mod, "Java_com_tatem_dinhunter_DinHunterRenderer_createFramebuffer");
    setEnvironment = (void *)so_symbol(&so_mod, "Java_com_tatem_dinhunter_DinHunterRenderer_setEnvironment");
    nativeResize = (void *)so_symbol(&so_mod, "Java_com_tatem_dinhunter_DinHunterRenderer_nativeResize");
    layoutSubviews = (void *)so_symbol(&so_mod, "Java_com_tatem_dinhunter_DinHunterRenderer_layoutSubviews");

    facebook_nativeInit = (void *)so_symbol(&so_mod, "Java_com_tatem_dinhunter_utils_FacebookWrapper_nativeInit");
    social_nativeInit = (void *)so_symbol(&so_mod, "Java_com_tatem_dinhunter_utils_SocialUtils_nativeInit");

    Weapon_Fire = (void *)so_symbol(&so_mod, "_Z11Weapon_Firev");
    game_stage_ptr = (void *)so_symbol(&so_mod, "game_stage");

    l_info("Native symbols resolved.");

    // Find APK path
    char apk_path[256];
    if (file_exists(DATA_PATH "CarnivoresDinosaurHunter.apk")) {
        snprintf(apk_path, sizeof(apk_path), DATA_PATH "CarnivoresDinosaurHunter.apk");
    } else if (file_exists(DATA_PATH "main.apk")) {
        snprintf(apk_path, sizeof(apk_path), DATA_PATH "main.apk");
    } else if (file_exists(DATA_PATH "game.apk")) {
        snprintf(apk_path, sizeof(apk_path), DATA_PATH "game.apk");
    } else {
        fatal_error("Error: could not find CarnivoresDinosaurHunter.apk in %s", DATA_PATH);
    }
    l_info("APK path: %s", apk_path);

    char storage_dir[256];
    snprintf(storage_dir, sizeof(storage_dir), "%s", DATA_PATH);
    size_t slen = strlen(storage_dir);
    if (slen > 0 && storage_dir[slen - 1] == '/') {
        storage_dir[slen - 1] = '\0';
    }

    // Initialize OpenGL via VitaGL
    gl_init();
    l_success("OpenGL initialized.");


    // Call game initialization callbacks
    if (facebook_nativeInit) facebook_nativeInit(&jni, (jobject)0x1);
    if (social_nativeInit) social_nativeInit(&jni, (jobject)0x2);

    jstring baseDirStr = jni->NewStringUTF(&jni, storage_dir);
    jstring apkPathStr = jni->NewStringUTF(&jni, apk_path);

    if (nativeSetBundlesPaths) {
        l_info("Calling nativeSetBundlesPaths...");
        nativeSetBundlesPaths(&jni, NULL, apkPathStr, apkPathStr);
        l_success("nativeSetBundlesPaths finished.");
    }

    l_info("Calling nativeApplicationDidFinishLaunching...");
    if (nativeApplicationDidFinishLaunching) {
        nativeApplicationDidFinishLaunching(&jni, (jobject)0x3, baseDirStr, apkPathStr);
    }
    l_success("nativeApplicationDidFinishLaunching finished.");

    if (setEnvironment) setEnvironment(&jni, NULL);

    l_info("Creating framebuffer (960x544)...");
    if (createFramebuffer) createFramebuffer(&jni, NULL, 960, 544);
    if (nativeResize) nativeResize(&jni, NULL, 960, 544);
    l_success("Framebuffer created.");

    // Unlock DLC packs
    if (nativeSetProductPurchased) {
        jstring pack1Str = jni->NewStringUTF(&jni, "com.tatem.dinhunter.bundle.one");
        jstring pack2Str = jni->NewStringUTF(&jni, "com.tatem.dinhunter.bundle.two");
        jstring emptyStr = jni->NewStringUTF(&jni, "");
        nativeSetProductPurchased(&jni, NULL, pack1Str, JNI_TRUE, emptyStr);
        nativeSetProductPurchased(&jni, NULL, pack2Str, JNI_TRUE, emptyStr);
        l_success("DLC Packs 1 & 2 unlocked.");
    }

    // Start Audio
    audio_init();
    l_success("Audio subsystem initialized.");

    // Start Touch Sampling
    sceTouchSetSamplingState(SCE_TOUCH_PORT_FRONT, SCE_TOUCH_SAMPLING_STATE_START);
    sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG);

    l_info("Entering main game loop.");

    uint32_t old_buttons = 0;

    while (1) {
        // Handle physical buttons
        SceCtrlData pad;
        sceCtrlPeekBufferPositive(0, &pad, 1);
        uint32_t pressed = pad.buttons & ~old_buttons;
        old_buttons = pad.buttons;

        if (pressed & (SCE_CTRL_CIRCLE | SCE_CTRL_START)) {
            if (nativeOnBackPressed) {
                nativeOnBackPressed(&jni, NULL);
            }
        }

        // In-game shooting with Cross / R-Trigger
        if (game_stage_ptr && *game_stage_ptr == 9) {
            if ((pressed & SCE_CTRL_CROSS) || (pressed & SCE_CTRL_RTRIGGER)) {
                if (Weapon_Fire) {
                    Weapon_Fire();
                }
            }
        }

        // Handle front touch input
        SceTouchData touch;
        sceTouchPeek(SCE_TOUCH_PORT_FRONT, &touch, 1);

        int seen[5] = {0};

        for (int r = 0; r < touch.reportNum && r < 5; r++) {
            int hid = touch.report[r].id;
            float x = (float)touch.report[r].x * 960.0f / 1920.0f;
            float y = (float)touch.report[r].y * 544.0f / 1088.0f;

            if (x < 0.0f) x = 0.0f;
            if (x > 960.0f) x = 960.0f;
            if (y < 0.0f) y = 0.0f;
            if (y > 544.0f) y = 544.0f;

            int slot = -1;
            for (int s = 0; s < 5; s++) {
                if (active_touches[s].id == hid) {
                    slot = s;
                    break;
                }
            }

            if (slot == -1) {
                for (int s = 0; s < 5; s++) {
                    if (active_touches[s].id == -1) {
                        slot = s;
                        active_touches[s].id = hid;
                        active_touches[s].x = x;
                        active_touches[s].y = y;
                        if (touchesBegan) touchesBegan(&jni, NULL, x, y);
                        break;
                    }
                }
            } else {
                if (active_touches[slot].x != x || active_touches[slot].y != y) {
                    active_touches[slot].x = x;
                    active_touches[slot].y = y;
                    if (touchesMoved) touchesMoved(&jni, NULL, x, y);
                }
            }

            if (slot >= 0 && slot < 5) {
                seen[slot] = 1;
            }
        }

        for (int s = 0; s < 5; s++) {
            if (active_touches[s].id != -1 && !seen[s]) {
                if (touchesEnded) touchesEnded(&jni, NULL, active_touches[s].x, active_touches[s].y);
                active_touches[s].id = -1;
            }
        }

        // Render current frame
        if (layoutSubviews) {
            layoutSubviews(&jni, NULL);
        }
        gl_swap();
    }

    audio_term();
    sceKernelExitDeleteThread(0);
    return 0;
}
