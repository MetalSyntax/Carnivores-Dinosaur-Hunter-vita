/*
 * java.c -- Java environment emulation for Carnivores Dinosaur Hunter via FalsoJNI.
 */

#include <falso_jni/FalsoJNI.h>
#include <falso_jni/FalsoJNI_Impl.h>
#include <falso_jni/FalsoJNI_Logger.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "utils/logger.h"
#include "utils/trophy.h"

enum {
    METHOD_OPEN_MORE_GAMES = 10,
    METHOD_SHOW_GALLERY,
    METHOD_ON_LOADING_COMPLETED,
    METHOD_HIDE_IN_APP_INFO,
    METHOD_HIDE_GALLERY,
    METHOD_SHOW_IN_APP_INFO,
    METHOD_HIDE_TUTORIAL,
    METHOD_SHOW_TUTORIAL,
    METHOD_HIDE_FEINT_BUTTON,
    METHOD_SHOW_FEINT_BUTTON,
    METHOD_PUT_FRAME_ON_PHOTO,
    METHOD_SET_TUTORIAL_FILE,
    METHOD_PUBLISH_FEED,
    METHOD_LOG_OUT,
    METHOD_LOG_IN,
    METHOD_SEND_FLURRY_EVENT,
    METHOD_UNLOCK_ACHIEVEMENT,
    METHOD_OPEN_ACHIEVEMENTS,
    METHOD_REQUEST_PURCHASE,
    METHOD_IS_ONLINE,
    METHOD_IS_LOGGED_IN,
    METHOD_GET_USER_NAME,
    METHOD_INIT_FEINT,
    METHOD_OPEN_DASHBOARD,
    METHOD_FMOD_START,
    METHOD_FMOD_STOP,
};

static void cb_openMoreGames(jmethodID id, va_list args) {
    l_debug("JNI: openMoreGames called");
}

static void cb_showGallery(jmethodID id, va_list args) {
    l_debug("JNI: showGallery called");
}

static void cb_onLoadingCompleted(jmethodID id, va_list args) {
    l_info("JNI: onLoadingCompleted called");
}

static void cb_hideInAppInfo(jmethodID id, va_list args) {
    l_debug("JNI: hideInAppInfo called");
}

static void cb_hideGallery(jmethodID id, va_list args) {
    l_debug("JNI: hideGallery called");
}

static void cb_showInAppInfo(jmethodID id, va_list args) {
    l_debug("JNI: showInAppInfo called");
}

static void cb_hideTutorial(jmethodID id, va_list args) {
    l_debug("JNI: hideTutorial called");
}

static void cb_showTutorial(jmethodID id, va_list args) {
    l_debug("JNI: showTutorial called");
}

static void cb_hideFeintButton(jmethodID id, va_list args) {
    l_debug("JNI: hideFeintButton called");
}

static void cb_showFeintButton(jmethodID id, va_list args) {
    l_debug("JNI: showFeintButton called");
}

static void cb_putFrameOnPhoto(jmethodID id, va_list args) {
    jstring path = va_arg(args, jstring);
    l_debug("JNI: putFrameOnPhoto called");
}

static void cb_setTutorialFile(jmethodID id, va_list args) {
    jstring file = va_arg(args, jstring);
    l_debug("JNI: setTutorialFile called");
}

static void cb_publishFeed(jmethodID id, va_list args) {
    l_debug("JNI: publishFeed called");
}

static void cb_logOut(jmethodID id, va_list args) {
    l_debug("JNI: logOut called");
}

static void cb_logIn(jmethodID id, va_list args) {
    l_debug("JNI: logIn called");
}

static void cb_SendFlurryEvent(jmethodID id, va_list args) {
    l_debug("JNI: SendFlurryEvent called");
}

static void cb_unlockAchievement(jmethodID id, va_list args) {
    jint achId = va_arg(args, jint);
    l_info("JNI: unlockAchievement(%d)", (int)achId);
    trophy_unlock((uint32_t)achId + 1);
}

static void cb_openAchievements(jmethodID id, va_list args) {
    l_debug("JNI: openAchievements called");
}

static jboolean cb_requestPurchase(jmethodID id, va_list args) {
    l_info("JNI: requestPurchase called");
    return JNI_TRUE;
}

static jboolean cb_isOnline(jmethodID id, va_list args) {
    return JNI_TRUE;
}

static jboolean cb_isLoggedIn(jmethodID id, va_list args) {
    return JNI_FALSE;
}

static jobject cb_getUserName(jmethodID id, va_list args) {
    return (jobject)jni->NewStringUTF(&jni, "Hunter");
}

static void cb_initFeint(jmethodID id, va_list args) {
    l_debug("JNI: initFeint called");
}

static void cb_openDashboard(jmethodID id, va_list args) {
    l_debug("JNI: openDashboard called");
}

static void cb_fmodStart(jmethodID id, va_list args) {
    l_info("JNI: FMODAudioDevice.start called");
}

static void cb_fmodStop(jmethodID id, va_list args) {
    l_info("JNI: FMODAudioDevice.stop called");
}

NameToMethodID nameToMethodId[] = {
    { METHOD_OPEN_MORE_GAMES,      "openMoreGames",      METHOD_TYPE_VOID },
    { METHOD_SHOW_GALLERY,        "showGallery",        METHOD_TYPE_VOID },
    { METHOD_ON_LOADING_COMPLETED,"onLoadingCompleted", METHOD_TYPE_VOID },
    { METHOD_HIDE_IN_APP_INFO,    "hideInAppInfo",      METHOD_TYPE_VOID },
    { METHOD_HIDE_GALLERY,        "hideGallery",        METHOD_TYPE_VOID },
    { METHOD_SHOW_IN_APP_INFO,    "showInAppInfo",      METHOD_TYPE_VOID },
    { METHOD_HIDE_TUTORIAL,       "hideTutorial",       METHOD_TYPE_VOID },
    { METHOD_SHOW_TUTORIAL,       "showTutorial",       METHOD_TYPE_VOID },
    { METHOD_HIDE_FEINT_BUTTON,   "hideFeintButton",   METHOD_TYPE_VOID },
    { METHOD_SHOW_FEINT_BUTTON,   "showFeintButton",   METHOD_TYPE_VOID },
    { METHOD_PUT_FRAME_ON_PHOTO,  "putFrameOnPhoto",    METHOD_TYPE_VOID },
    { METHOD_SET_TUTORIAL_FILE,   "setTutorialFile",   METHOD_TYPE_VOID },
    { METHOD_PUBLISH_FEED,        "publishFeed",        METHOD_TYPE_VOID },
    { METHOD_LOG_OUT,             "logOut",             METHOD_TYPE_VOID },
    { METHOD_LOG_IN,              "logIn",              METHOD_TYPE_VOID },
    { METHOD_SEND_FLURRY_EVENT,   "SendFlurryEvent",   METHOD_TYPE_VOID },
    { METHOD_UNLOCK_ACHIEVEMENT,  "unlockAchievement",  METHOD_TYPE_VOID },
    { METHOD_OPEN_ACHIEVEMENTS,   "openAchievements",   METHOD_TYPE_VOID },
    { METHOD_REQUEST_PURCHASE,    "requestPurchase",    METHOD_TYPE_BOOLEAN },
    { METHOD_IS_ONLINE,           "isOnline",           METHOD_TYPE_BOOLEAN },
    { METHOD_IS_LOGGED_IN,        "isLoggedIn",         METHOD_TYPE_BOOLEAN },
    { METHOD_GET_USER_NAME,       "getUserName",        METHOD_TYPE_OBJECT },
    { METHOD_INIT_FEINT,          "initFeint",          METHOD_TYPE_VOID },
    { METHOD_OPEN_DASHBOARD,      "openDashboard",      METHOD_TYPE_VOID },
    { METHOD_FMOD_START,          "start",              METHOD_TYPE_VOID },
    { METHOD_FMOD_STOP,           "stop",               METHOD_TYPE_VOID },
};

MethodsVoid methodsVoid[] = {
    { METHOD_OPEN_MORE_GAMES,      cb_openMoreGames },
    { METHOD_SHOW_GALLERY,        cb_showGallery },
    { METHOD_ON_LOADING_COMPLETED, cb_onLoadingCompleted },
    { METHOD_HIDE_IN_APP_INFO,    cb_hideInAppInfo },
    { METHOD_HIDE_GALLERY,        cb_hideGallery },
    { METHOD_SHOW_IN_APP_INFO,    cb_showInAppInfo },
    { METHOD_HIDE_TUTORIAL,       cb_hideTutorial },
    { METHOD_SHOW_TUTORIAL,       cb_showTutorial },
    { METHOD_HIDE_FEINT_BUTTON,   cb_hideFeintButton },
    { METHOD_SHOW_FEINT_BUTTON,   cb_showFeintButton },
    { METHOD_PUT_FRAME_ON_PHOTO,  cb_putFrameOnPhoto },
    { METHOD_SET_TUTORIAL_FILE,   cb_setTutorialFile },
    { METHOD_PUBLISH_FEED,        cb_publishFeed },
    { METHOD_LOG_OUT,             cb_logOut },
    { METHOD_LOG_IN,              cb_logIn },
    { METHOD_SEND_FLURRY_EVENT,   cb_SendFlurryEvent },
    { METHOD_UNLOCK_ACHIEVEMENT,  cb_unlockAchievement },
    { METHOD_OPEN_ACHIEVEMENTS,   cb_openAchievements },
    { METHOD_INIT_FEINT,          cb_initFeint },
    { METHOD_OPEN_DASHBOARD,      cb_openDashboard },
    { METHOD_FMOD_START,          cb_fmodStart },
    { METHOD_FMOD_STOP,           cb_fmodStop },
};

MethodsBoolean methodsBoolean[] = {
    { METHOD_REQUEST_PURCHASE,    cb_requestPurchase },
    { METHOD_IS_ONLINE,           cb_isOnline },
    { METHOD_IS_LOGGED_IN,        cb_isLoggedIn },
};

MethodsObject methodsObject[] = {
    { METHOD_GET_USER_NAME,       cb_getUserName },
};

MethodsByte methodsByte[] = {};
MethodsChar methodsChar[] = {};
MethodsDouble methodsDouble[] = {};
MethodsFloat methodsFloat[] = {};
MethodsInt methodsInt[] = {};
MethodsLong methodsLong[] = {};
MethodsShort methodsShort[] = {};

/*
 * JNI Fields
 */
char WINDOW_SERVICE[] = "window";
const int SDK_INT = 19;
static void* dummy_purchase_manager = (void*)0x1234;

NameToFieldID nameToFieldId[] = {
    { 0, "WINDOW_SERVICE",   FIELD_TYPE_OBJECT },
    { 1, "SDK_INT",          FIELD_TYPE_INT },
    { 2, "purchaseManager",  FIELD_TYPE_OBJECT },
};

FieldsObject fieldsObject[] = {
    { 0, WINDOW_SERVICE },
    { 2, (jobject)&dummy_purchase_manager },
};

FieldsInt fieldsInt[] = {
    { 1, SDK_INT },
};

FieldsBoolean fieldsBoolean[] = {};
FieldsByte fieldsByte[] = {};
FieldsChar fieldsChar[] = {};
FieldsDouble fieldsDouble[] = {};
FieldsFloat fieldsFloat[] = {};
FieldsLong fieldsLong[] = {};
FieldsShort fieldsShort[] = {};

__FALSOJNI_IMPL_CONTAINER_SIZES
