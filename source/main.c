#include "utils/init.h"
#include "utils/glutil.h"
#include "utils/logger.h"
#include "utils/utils.h"
#include "utils/dialog.h"
#include "reimpl/audio.h"
#include "input.h"
#include "utils/trophy.h"

#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/processmgr.h>
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

// Placeholder Java objects: the engine keeps them and only ever uses them as
// receivers for Call*Method, never dereferences them. input.c uses both.
static int activity_placeholder, surface_placeholder;
jobject activity_obj = (jobject) &activity_placeholder;
jobject surface_obj  = (jobject) &surface_placeholder;

// JNI Entry points from libdinHunter.so
static jboolean (*isNativeAppInitialized)(void *env, void *thiz);
static void (*nativeApplicationDidFinishLaunching)(void *env, void *thiz, jstring baseDir, jstring apkPath);
static void (*nativeResume)(void *env, void *thiz);
static void (*nativePause)(void *env, void *thiz);
static void (*destroyFramebuffer)(void *env, void *thiz);
static void (*nativeSetProductPurchased)(void *env, void *thiz, jstring prodId, jboolean purchased, jstring prodPath);

static void (*createFramebuffer)(void *env, void *thiz, jint w, jint h);
static void (*setEnvironment)(void *env, void *thiz);
static void (*nativeResize)(void *env, void *thiz, jint w, jint h);
static void (*layoutSubviews)(void *env, void *thiz);

static void (*facebook_nativeInit)(void *env, void *thiz);
static void (*social_nativeInit)(void *env, void *thiz);

int main(int argc, char *argv[]) {
    l_info("--- Carnivores Dinosaur Hunter (PS Vita) starting ---");


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
    destroyFramebuffer = (void *)so_symbol(&so_mod, "Java_com_tatem_dinhunter_DinHunterAndroid_destroyFramebuffer");
    nativeSetProductPurchased = (void *)so_symbol(&so_mod, "Java_com_tatem_dinhunter_DinHunterAndroid_nativeSetProductPurchased");


    createFramebuffer = (void *)so_symbol(&so_mod, "Java_com_tatem_dinhunter_DinHunterRenderer_createFramebuffer");
    setEnvironment = (void *)so_symbol(&so_mod, "Java_com_tatem_dinhunter_DinHunterRenderer_setEnvironment");
    nativeResize = (void *)so_symbol(&so_mod, "Java_com_tatem_dinhunter_DinHunterRenderer_nativeResize");
    layoutSubviews = (void *)so_symbol(&so_mod, "Java_com_tatem_dinhunter_DinHunterRenderer_layoutSubviews");

    facebook_nativeInit = (void *)so_symbol(&so_mod, "Java_com_tatem_dinhunter_utils_FacebookWrapper_nativeInit");
    social_nativeInit = (void *)so_symbol(&so_mod, "Java_com_tatem_dinhunter_utils_SocialUtils_nativeInit");


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

    // Initialize trophies (requires NoTrpDrm + trophy pack in ux0:app/PSVCDH001/sce_sys/trophy/)
    trophy_init();


    // Call game initialization callbacks
    if (facebook_nativeInit) facebook_nativeInit(&jni, (jobject)0x1);
    if (social_nativeInit) social_nativeInit(&jni, (jobject)0x2);

    jstring baseDirStr = jni->NewStringUTF(&jni, storage_dir);
    jstring apkPathStr = jni->NewStringUTF(&jni, apk_path);

    l_info("Calling nativeApplicationDidFinishLaunching...");
    if (nativeApplicationDidFinishLaunching) {
        nativeApplicationDidFinishLaunching(&jni, activity_obj, baseDirStr, apkPathStr);
    }
    l_success("nativeApplicationDidFinishLaunching finished.");

    if (setEnvironment) setEnvironment(&jni, NULL);

    l_info("Creating framebuffer (960x544)...");
    if (createFramebuffer) createFramebuffer(&jni, NULL, 960, 544);
    if (nativeResize) nativeResize(&jni, NULL, 960, 544);
    l_success("Framebuffer created.");

    // Content packs. On Android they are separate apps
    // (com.tatem.dinhunter.bundle.one/two): BundleChecker passes the pack's
    // APK path, the engine keeps it as bundle1Path/bundle2Path and
    // Files_OpenFileOfType() looks there for files that are not in the main
    // APK. A pack marked as owned without its APK makes the engine offer
    // dinosaurs/maps whose files don't exist (crash in log 010), so only mark
    // a pack as owned when its APK is present -- like DinHunterAndroid does.
    if (nativeSetProductPurchased) {
        static const struct { const char *id, *path; } packs[] = {
            { "com.tatem.dinhunter.bundle.one", DATA_PATH "bundle1.apk" },
            { "com.tatem.dinhunter.bundle.two", DATA_PATH "bundle2.apk" },
        };
        for (int i = 0; i < 2; i++) {
            jstring id = jni->NewStringUTF(&jni, packs[i].id);
            if (file_exists(packs[i].path)) {
                jstring path = jni->NewStringUTF(&jni, packs[i].path);
                nativeSetProductPurchased(&jni, activity_obj, id, JNI_TRUE, path);
                l_success("Content pack %d found: %s", i + 1, packs[i].path);
            } else {
                nativeSetProductPurchased(&jni, activity_obj, id, JNI_FALSE, NULL);
                l_info("Content pack %d not installed (%s)", i + 1, packs[i].path);
            }
        }
    }

    // Start Audio
    audio_init();
    l_success("Audio subsystem initialized.");

    input_init();

    l_info("Entering main game loop.");

    while (1) {
        input_update();

        // Render current frame
        if (layoutSubviews) {
            layoutSubviews(&jni, NULL);
        }
        gl_swap();
    }

    trophy_term();
    audio_term();
    sceKernelExitDeleteThread(0);
    return 0;
}
