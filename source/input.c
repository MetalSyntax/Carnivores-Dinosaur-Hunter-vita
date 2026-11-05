/*
 * Copyright (C) 2026 Carnivores Vita port contributors
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

/**
 * @file  input.c
 * @brief Front touch + physical controls -> DinHunterGLSurface touches.
 *
 * The engine only understands touches: DinHunterGLSurface forwards
 * touchesBegan/Moved/Ended(x, y) in surface pixels, and GUIControls.cpp maps
 * each one to a HUD control (or to the background, which rotates the camera).
 * Touches carry no ID: GUI_GetTouchByLocation() re-identifies a touch by the
 * stored location nearest to the event, so every move below is split into
 * small steps and each touch is ended at the exact spot it was last seen.
 *
 * Same Tatem engine as Carnivores: Ice Age; every symbol, the gui_controls[]
 * layout and GUI_GetBackgroundMovements() were re-checked against
 * libdinHunter.so's pseudo-C (2026-09-30).
 *
 * Physical controls are turned into synthetic touches placed on the real HUD
 * controls, read at runtime from the engine's own gui_controls[] table
 * (0x180-byte records, layout from GUI_AddControl / GUI_PointInControl):
 *   +0x00 group      +0x04 subgroup mask   +0x08 type (0 button, 1 slider,
 *   2 stick)         +0x0C x               +0x10 y
 *   +0x1C w          +0x20 h               +0x24 align flags (2: right,
 *   4: h-center, 8: v-center)              +0x28 color (ARGB)
 *   +0x2C scale      +0x32/+0x33 active/visible
 *   +0x174 slider value, +0x178/+0x17C slider min/max
 * Coordinates are the engine's 480x320 logical space, origin bottom-left
 * (GUI_RecalcTouchLocation: lx = x * scaleX, ly = (real_height - y) * scaleY).
 * A synthetic touch is only started if the control is active in the current
 * GUI group.
 *
 * Some actions call the engine directly instead (Game_ProcessPlayerControls,
 * pseudo-C ~20660-21000): the fire button depends on the in-game "firing
 * method" option (0 = tap the screen, 1 = game_alternative_fire -- the
 * default --, 2 = game_fire), and drawing/switching weapons by touch goes
 * through the weapon list. FIRE therefore draws a holstered weapon with
 * Weapon_TakeWeapon() and fires through whichever fire control is visible, or
 * Weapon_Fire() when there is none.
 *
 * Outside the hunting HUD (menus, pause, trophy room) the buttons move a
 * cursor over the active controls instead (see "menu navigation").
 *
 * The right stick does NOT use touches: input_patch() replaces
 * GUI_GetBackgroundMovements() (the per-frame sum of background drags that
 * Game_ProcessPlayerControls() turns into camera rotation, x 0.1875 x the
 * in-game sensitivity slider) and adds the stick to it, in logical px/second.
 * A synthetic drag was capped by the touch re-anchoring and scaled with the
 * frame rate (8 px/frame -> slow at the ~18 FPS of the 3D scenes).
 */

#include "input.h"

#include "overlay.h"
#include "vita_menu.h"
#include "utils/logger.h"
#include "utils/settings.h"

#include <psp2/ctrl.h>
#include <psp2/touch.h>
#include <psp2/kernel/processmgr.h>

#include <falso_jni/FalsoJNI.h>
#include <so_util/so_util.h>

#include <ctype.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

extern so_module so_mod;
extern jobject activity_obj;  // main.c
extern jobject surface_obj;   // main.c

#define CTL_STRIDE      0x180
#define LOGICAL_W       480.0f
#define LOGICAL_H       320.0f
// GUI_GetControllerVector(): the virtual stick saturates at 40 logical px.
#define STICK_RADIUS    40.0f
// Max logical distance per touchesMoved so GUI_GetTouchByLocation() keeps
// matching the same touch.
#define MAX_STEP        12.0f
#define ANALOG_DEADZONE 30
// Right stick at full deflection with look_sensitivity 100, in logical px/s.
#define LOOK_SPEED      360.0f
#define REAL_SLOTS      8
#define TOUCH_SLOTS     16      // gui_touched_locations[16][2]

#define CONTROLS_PATH    DATA_PATH "controls.txt"
// Bumped when the action set changes: older files are replaced by the
// defaults (v1 had FIRE/ALT_FIRE split by HUD control).
#define CONTROLS_VERSION 2

// Rear touch quadrants, reported as extra "buttons".
enum {
    CUSTOM_BTN_L2 = (1 << 24), // Rear touch top-left
    CUSTOM_BTN_R2 = (1 << 25), // Rear touch top-right
    CUSTOM_BTN_L3 = (1 << 26), // Rear touch bottom-left
    CUSTOM_BTN_R3 = (1 << 27), // Rear touch bottom-right
};

typedef void (*touch_fn)(JNIEnv *env, jobject thiz, jfloat x, jfloat y);

static touch_fn touchesBegan, touchesMoved, touchesEnded, touchesCancelled;
static jboolean (*nativeOnBackPressed)(JNIEnv *env, jobject thiz);

static float *p_scaleX, *p_scaleY;
static int *p_real_width, *p_real_height;
static uint8_t *gui_controls;
static int *gui_controls_count, *gui_active_group;
static uint32_t *gui_active_subgroups;

static int *ctl_move, *ctl_menu, *ctl_fire, *ctl_alt_fire, *ctl_weapon, *ctl_binoculars,
           *ctl_call, *ctl_map, *ctl_photo_shot, *ctl_photo_zoom_in, *ctl_photo_zoom_out;
static int *ctl_fb_hunt, *ctl_fb_stats, *ctl_fb_trophy_stat, *ctl_fb_trophy, *ctl_fb_login;

// Weapons (Weapons.cpp): weapons[] records of 0x84 bytes, +0x3C state
// (0 holstered, 1 ready, 2 firing, 4 drawing, 5 holstering).
#define WEAPON_STRIDE  0x84
#define WEAPON_STATE   0x3C
#define WEAPON_CAMERA  6
static uint8_t *weapons;
static int *current_weapon, *using_weapons, *using_weapons_count;
static int *quick_touch, *map_mode, *firing_method;
static char *game_menu_show_photomode;
static void (*Weapon_TakeWeapon)(void);
static void (*Weapon_Fire)(void);
static void (*Weapon_ChangeCurrentWeapon)(int);
static void (*GUI_SetSliderValue)(int, float);
static void (*Sprites_GetSpriteSize)(void *sprite, float *size);
static uint8_t *menu_hunt_cell_empty;

typedef struct {
    int active;
    float x, y;         // surface pixels, last position sent to the engine
    uint64_t since;     // begin timestamp (us)
} vtouch;

typedef enum {
    KIND_TOUCH,         // hold the first usable control of `controls`
    KIND_FIRE,
    KIND_JUMP,
    KIND_HOLSTER,
    KIND_NEXT_WEAPON,
} ActionKind;

typedef struct {
    const char *name;   // controls.txt key
    const char *label;  // port menu
    ActionKind kind;
    uint32_t default_buttons;
    int **controls[3];
    uint32_t buttons;
    vtouch t;
} Action;

enum {
    ACT_FIRE = 0,
    ACT_JUMP,
    ACT_WEAPON,
    ACT_NEXT_WEAPON,
    ACT_WEAPON_MENU,
    ACT_BINOCULARS,
    ACT_CALL,
    ACT_MAP,
    ACT_ZOOM_IN,
    ACT_ZOOM_OUT,
    ACT_COUNT
};

static Action actions[ACT_COUNT] = {
    [ACT_FIRE]        = { "FIRE",        "Fire / take photo",     KIND_FIRE,        SCE_CTRL_RTRIGGER, {0} },
    [ACT_JUMP]        = { "JUMP",        "Jump",                  KIND_JUMP,        SCE_CTRL_CROSS,    {0} },
    [ACT_WEAPON]      = { "WEAPON",      "Draw / holster weapon", KIND_HOLSTER,     SCE_CTRL_SQUARE,   {0} },
    [ACT_NEXT_WEAPON] = { "NEXT_WEAPON", "Next weapon",           KIND_NEXT_WEAPON, SCE_CTRL_TRIANGLE, {0} },
    [ACT_WEAPON_MENU] = { "WEAPON_MENU", "Weapon list",           KIND_TOUCH,       SCE_CTRL_LEFT,     { &ctl_weapon } },
    [ACT_BINOCULARS]  = { "BINOCULARS",  "Binoculars",            KIND_TOUCH,       SCE_CTRL_LTRIGGER, { &ctl_binoculars } },
    [ACT_CALL]        = { "CALL",        "Call dinosaurs",        KIND_TOUCH,       SCE_CTRL_CIRCLE | SCE_CTRL_UP,   { &ctl_call } },
    [ACT_MAP]         = { "MAP",         "Map",                   KIND_TOUCH,       SCE_CTRL_SELECT | SCE_CTRL_DOWN, { &ctl_map } },
    [ACT_ZOOM_IN]     = { "ZOOM_IN",     "Photo zoom in",         KIND_TOUCH,       SCE_CTRL_RIGHT,    { &ctl_photo_zoom_in } },
    [ACT_ZOOM_OUT]    = { "ZOOM_OUT",    "Photo zoom out",        KIND_TOUCH,       0,                 { &ctl_photo_zoom_out } },
};

typedef struct {
    const char *name;   // controls.txt spelling (first one per mask is canonical)
    const char *label;  // port menu
    uint32_t mask;
} ButtonName;

static const ButtonName button_names[] = {
    { "CROSS",       "Cross",    SCE_CTRL_CROSS },
    { "CIRCLE",      "Circle",   SCE_CTRL_CIRCLE },
    { "SQUARE",      "Square",   SCE_CTRL_SQUARE },
    { "TRIANGLE",    "Triangle", SCE_CTRL_TRIANGLE },
    { "L1",          "L",        SCE_CTRL_LTRIGGER },
    { "R1",          "R",        SCE_CTRL_RTRIGGER },
    { "UP",          "Up",       SCE_CTRL_UP },
    { "DOWN",        "Down",     SCE_CTRL_DOWN },
    { "LEFT",        "Left",     SCE_CTRL_LEFT },
    { "RIGHT",       "Right",    SCE_CTRL_RIGHT },
    { "SELECT",      "Select",   SCE_CTRL_SELECT },
    { "START",       "Start",    SCE_CTRL_START },
    { "L2",          "Rear TL",  CUSTOM_BTN_L2 },
    { "R2",          "Rear TR",  CUSTOM_BTN_R2 },
    { "L3",          "Rear BL",  CUSTOM_BTN_L3 },
    { "R3",          "Rear BR",  CUSTOM_BTN_R3 },
    // Aliases, only read.
    { "X",           NULL,       SCE_CTRL_CROSS },
    { "O",           NULL,       SCE_CTRL_CIRCLE },
    { "LTRIGGER",    NULL,       SCE_CTRL_LTRIGGER },
    { "L",           NULL,       SCE_CTRL_LTRIGGER },
    { "RTRIGGER",    NULL,       SCE_CTRL_RTRIGGER },
    { "R",           NULL,       SCE_CTRL_RTRIGGER },
    { "DPAD_UP",     NULL,       SCE_CTRL_UP },
    { "DPAD_DOWN",   NULL,       SCE_CTRL_DOWN },
    { "DPAD_LEFT",   NULL,       SCE_CTRL_LEFT },
    { "DPAD_RIGHT",  NULL,       SCE_CTRL_RIGHT },
    { "REAR_UP_L",   NULL,       CUSTOM_BTN_L2 },
    { "REAR_UP_R",   NULL,       CUSTOM_BTN_R2 },
    { "REAR_DOWN_L", NULL,       CUSTOM_BTN_L3 },
    { "REAR_DOWN_R", NULL,       CUSTOM_BTN_R3 },
};
#define BUTTON_NAMES_COUNT (sizeof(button_names) / sizeof(button_names[0]))

static vtouch move_touch;
static float move_cx, move_cy;          // surface-space origin of move_touch
static int fire_drew;                   // FIRE press drew the weapon: don't shoot until released

// GUI_GetBackgroundMovements() replacement state.
static float *touched_locations, *touched_start_locations;
static int *touched_controls;           // -1 = background touch, -500 = free
static float look_dx, look_dy;          // this frame's stick delta, logical px
static uint64_t look_last_us;

static struct {
    int id;             // SceTouchReport.id, -1 when free
    float x, y;
} real[REAL_SLOTS];

static uint32_t old_buttons;
static int start_combo;                 // START held and used for START+SELECT
static int was_hud;
static int ready;

/* --- engine space helpers ---------------------------------------------- */

static float scale_x(void) { return (p_scaleX && *p_scaleX > 0.0f) ? *p_scaleX : 0.5f; }
static float scale_y(void) { return (p_scaleY && *p_scaleY > 0.0f) ? *p_scaleY : LOGICAL_H / 544.0f; }
static float surf_w(void)  { return p_real_width && *p_real_width > 0 ? (float) *p_real_width : 960.0f; }
static float surf_h(void)  { return p_real_height && *p_real_height > 0 ? (float) *p_real_height : 544.0f; }

static void logical_to_surface(float lx, float ly, float *sx, float *sy) {
    *sx = lx / scale_x();
    *sy = surf_h() - ly / scale_y();
}

static uint8_t *control(int idx) {
    if (!gui_controls || !gui_controls_count || idx < 0 || idx >= *gui_controls_count)
        return NULL;
    return gui_controls + idx * CTL_STRIDE;
}

static int control_usable(int idx) {
    uint8_t *c = control(idx);
    if (!c || !gui_active_group || !gui_active_subgroups)
        return 0;
    return *(int *) c == *gui_active_group &&
           (*(uint32_t *) (c + 0x04) & *gui_active_subgroups) != 0 &&
           c[0x32] && c[0x33];
}

// Logical-space rectangle of a control, mirroring GUI_PointInControl().
static void control_rect(int idx, float *x0, float *y0, float *x1, float *y1) {
    uint8_t *c = control(idx);
    float s = *(float *) (c + 0x2C);
    float w = *(float *) (c + 0x1C) * s;
    float h = *(float *) (c + 0x20) * s;
    float x = *(float *) (c + 0x0C);
    float y = *(float *) (c + 0x10);
    uint32_t flags = *(uint32_t *) (c + 0x24);

    if (flags & 2) x -= w;
    if (flags & 4) x -= w * 0.5f;
    if (flags & 8) y -= h * 0.5f;

    *x0 = x; *x1 = x + w;
    *y0 = y; *y1 = y + h;
}

static int control_center_surface(int idx, float *sx, float *sy) {
    if (!control_usable(idx))
        return 0;
    float x0, y0, x1, y1;
    control_rect(idx, &x0, &y0, &x1, &y1);
    logical_to_surface((x0 + x1) * 0.5f, (y0 + y1) * 0.5f, sx, sy);
    return 1;
}

static int sym_usable(int *ctl) {
    return ctl && control_usable(*ctl);
}

// The hunting HUD is up (and not the pause menu, subgroup 0x400, a dialog,
// ...). game_menu (the pause button, subgroups 0x4825) also covers arcade
// mode (0x800), where the movement stick (0x4025) is hidden.
static int hud_active(void) {
    return sym_usable(ctl_move) || sym_usable(ctl_menu);
}

/* --- touch primitives --------------------------------------------------- */

static float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

static void t_begin(vtouch *t, float x, float y) {
    x = clampf(x, 0.0f, surf_w() - 1.0f);
    y = clampf(y, 0.0f, surf_h() - 1.0f);
    touchesBegan(&jni, surface_obj, x, y);
    t->active = 1;
    t->x = x;
    t->y = y;
    t->since = sceKernelGetProcessTimeWide();
}

static void t_move(vtouch *t, float x, float y) {
    if (!t->active)
        return;
    x = clampf(x, 0.0f, surf_w() - 1.0f);
    y = clampf(y, 0.0f, surf_h() - 1.0f);

    // Split into steps of at most MAX_STEP logical px.
    float dx = (x - t->x) * scale_x();
    float dy = (y - t->y) * scale_y();
    float dist = sqrtf(dx * dx + dy * dy);
    int steps = (int) ceilf(dist / MAX_STEP);
    if (steps < 1) {
        if (x == t->x && y == t->y)
            return;
        steps = 1;
    }
    float sx = t->x, sy = t->y;
    for (int i = 1; i <= steps; i++) {
        float k = (float) i / (float) steps;
        touchesMoved(&jni, surface_obj, sx + (x - sx) * k, sy + (y - sy) * k);
    }
    t->x = x;
    t->y = y;
}

static void t_end(vtouch *t) {
    if (!t->active)
        return;
    touchesEnded(&jni, surface_obj, t->x, t->y);
    t->active = 0;
}

static int t_begin_on(vtouch *t, int idx) {
    float sx, sy;
    if (!control_center_surface(idx, &sx, &sy))
        return 0;
    t_begin(t, sx, sy);
    return 1;
}

/* --- front touch panel -------------------------------------------------- */

// Returns the number of fingers on the screen.
static int update_real_touch(void) {
    SceTouchData touch;
    if (sceTouchPeek(SCE_TOUCH_PORT_FRONT, &touch, 1) < 0)
        touch.reportNum = 0;

    int seen[REAL_SLOTS] = {0};
    float sw = surf_w(), sh = surf_h();

    for (int r = 0; r < (int) touch.reportNum && r < SCE_TOUCH_MAX_REPORT; r++) {
        int id = touch.report[r].id;
        float x = clampf(touch.report[r].x * sw / 1920.0f, 0.0f, sw - 1.0f);
        float y = clampf(touch.report[r].y * sh / 1088.0f, 0.0f, sh - 1.0f);

        int slot = -1;
        for (int s = 0; s < REAL_SLOTS; s++) {
            if (real[s].id == id) { slot = s; break; }
        }

        if (slot < 0) {
            for (int s = 0; s < REAL_SLOTS; s++) {
                if (real[s].id == -1) { slot = s; break; }
            }
            if (slot < 0)
                continue; // more fingers than slots: ignore the extra one
            real[slot].id = id;
            real[slot].x = x;
            real[slot].y = y;
            touchesBegan(&jni, surface_obj, x, y);
        } else if (real[slot].x != x || real[slot].y != y) {
            real[slot].x = x;
            real[slot].y = y;
            touchesMoved(&jni, surface_obj, x, y);
        }
        seen[slot] = 1;
    }

    for (int s = 0; s < REAL_SLOTS; s++) {
        if (real[s].id != -1 && !seen[s]) {
            touchesEnded(&jni, surface_obj, real[s].x, real[s].y);
            real[s].id = -1;
        }
    }
    return (int) touch.reportNum;
}

static void release_real_touch(void) {
    for (int s = 0; s < REAL_SLOTS; s++) {
        if (real[s].id != -1) {
            touchesEnded(&jni, surface_obj, real[s].x, real[s].y);
            real[s].id = -1;
        }
    }
}

static uint32_t poll_rear_touch(void) {
    SceTouchData touch;
    if (sceTouchPeek(SCE_TOUCH_PORT_BACK, &touch, 1) <= 0)
        return 0;

    uint32_t mask = 0;
    for (int r = 0; r < (int) touch.reportNum && r < SCE_TOUCH_MAX_REPORT; r++) {
        int x = touch.report[r].x;
        int y = touch.report[r].y;
        if (y < 544) {
            if (x < 960) mask |= CUSTOM_BTN_L2;
            else         mask |= CUSTOM_BTN_R2;
        } else {
            if (x < 960) mask |= CUSTOM_BTN_L3;
            else         mask |= CUSTOM_BTN_R3;
        }
    }
    return mask;
}

/* --- sticks ------------------------------------------------------------- */

static float axis(uint8_t v) {
    int d = (int) v - 128;
    if (d > -ANALOG_DEADZONE && d < ANALOG_DEADZONE)
        return 0.0f;
    float f = (d > 0 ? (float) (d - ANALOG_DEADZONE) : (float) (d + ANALOG_DEADZONE))
              / (127.0f - ANALOG_DEADZONE);
    return clampf(f, -1.0f, 1.0f);
}

static void update_move(float ax, float ay) {
    int idx = ctl_move ? *ctl_move : -1;

    if (ax == 0.0f && ay == 0.0f) {
        t_end(&move_touch);
        return;
    }

    if (!move_touch.active) {
        if (!control_center_surface(idx, &move_cx, &move_cy))
            return;
        t_begin(&move_touch, move_cx, move_cy);
    }

    // Stick up (ay < 0) = surface up = logical up = forward.
    float tx = move_cx + ax * STICK_RADIUS / scale_x();
    float ty = move_cy + ay * STICK_RADIUS / scale_y();
    t_move(&move_touch, tx, ty);
}

// Engine's GUI_GetBackgroundMovements(float *dx, float *dy), reimplemented
// 1:1 from the pseudo-C (sum of location - start over background touches,
// then start = location) plus the right stick delta of this frame. Only
// caller: Game_ProcessPlayerControls(), i.e. in-game camera only.
static void GUI_GetBackgroundMovements_hook(float *dx, float *dy) {
    float sx = 0.0f, sy = 0.0f;
    for (int i = 0; i < TOUCH_SLOTS; i++) {
        if (touched_controls[i] != -1)
            continue;
        float *loc = &touched_locations[i * 2], *start = &touched_start_locations[i * 2];
        sx += loc[0] - start[0];
        sy += loc[1] - start[1];
        start[0] = loc[0];
        start[1] = loc[1];
    }
    *dx = sx + look_dx;
    *dy = sy + look_dy;
    look_dx = look_dy = 0.0f;
}

static void update_look(float ax, float ay, int hud) {
    uint64_t now = sceKernelGetProcessTimeWide();
    float dt = look_last_us ? (float) (now - look_last_us) / 1000000.0f : 0.0f;
    look_last_us = now;
    if (dt > 0.1f)
        dt = 0.1f; // after a long frame (loading), don't jump

    // Only while the in-game HUD is up; the hook is not called elsewhere, and
    // a stale delta must not be applied when the game resumes.
    if ((ax == 0.0f && ay == 0.0f) || !hud) {
        look_dx = look_dy = 0.0f;
        return;
    }

    // Quadratic response curve for fine aiming near the center. Logical space
    // is y-up, so stick up (ay < 0) is a positive y drag, like a finger
    // moving up the screen.
    float speed = LOOK_SPEED * (float) setting_lookSensitivity / 100.0f * dt;
    look_dx = ax * fabsf(ax) * speed * (setting_invertLookX ? -1.0f : 1.0f);
    look_dy = -ay * fabsf(ay) * speed * (setting_invertLookY ? -1.0f : 1.0f);
}

/* --- actions ------------------------------------------------------------ */

static int weapon_state(void) {
    if (!weapons || !current_weapon || *current_weapon < 0)
        return -1;
    return *(int *) (weapons + *current_weapon * WEAPON_STRIDE + WEAPON_STATE);
}

static int first_usable(int **const *list, int n) {
    for (int i = 0; i < n; i++) {
        if (list[i] && *list[i] && control_usable(**list[i]))
            return **list[i];
    }
    return -1;
}

// FIRE: whatever the firing method, shoot with the button; a holstered weapon
// is drawn first (the original needs the weapon button, which also opens the
// weapon list).
static void fire_update(Action *a, int down, int pressed) {
    if (!down) {
        t_end(&a->t);
        fire_drew = 0;
        return;
    }
    if (a->t.active || fire_drew)
        return;

    // Photo mode: the shutter reacts on release, so only on a fresh press.
    if (pressed && sym_usable(ctl_photo_shot)) {
        t_begin_on(&a->t, *ctl_photo_shot);
        return;
    }

    // Firing methods 1/2: hold the visible fire control (it only shows up
    // while the weapon is ready, so holding the button keeps shooting).
    int **const fire_ctls[] = { &ctl_fire, &ctl_alt_fire };
    int idx = first_usable(fire_ctls, 2);
    if (idx >= 0) {
        t_begin_on(&a->t, idx);
        return;
    }

    int state = weapon_state();
    if (pressed && state == 0 && Weapon_TakeWeapon) {
        Weapon_TakeWeapon();
        if (map_mode)
            *map_mode = 0;
        if (game_menu_show_photomode)
            *game_menu_show_photomode = *current_weapon == WEAPON_CAMERA;
        fire_drew = 1;
        return;
    }

    // Firing method 0 (tap the screen): no fire control at all. Weapon_Fire()
    // only shoots in state 1, like the engine's quick-touch path.
    if (state == 1 && Weapon_Fire && firing_method && *firing_method == 0)
        Weapon_Fire();
}

static void holster_toggle(void) {
    int state = weapon_state();
    if (state < 0 || !Weapon_TakeWeapon)
        return;
    // Weapon_TakeWeapon() toggles: holstered -> draw, drawn -> holster.
    Weapon_TakeWeapon();
    if (state == 0) {
        if (map_mode)
            *map_mode = 0;
        if (game_menu_show_photomode)
            *game_menu_show_photomode = *current_weapon == WEAPON_CAMERA;
    }
}

// Same as picking the next entry of the weapon list (Game_ProcessPlayerControls,
// game_weapons[] loop): Weapon_ChangeCurrentWeapon() + photo mode flag.
static void next_weapon(void) {
    if (!using_weapons || !using_weapons_count || !current_weapon || !Weapon_ChangeCurrentWeapon)
        return;
    int n = *using_weapons_count;
    if (n < 2)
        return;
    int i = 0;
    while (i < n && using_weapons[i] != *current_weapon)
        i++;
    int next = using_weapons[i < n ? (i + 1) % n : 0];
    Weapon_ChangeCurrentWeapon(next);
    if (game_menu_show_photomode)
        *game_menu_show_photomode = next == WEAPON_CAMERA;
}

static void update_actions(uint32_t held, uint32_t pressed) {
    for (int i = 0; i < ACT_COUNT; i++) {
        Action *a = &actions[i];
        int down = (held & a->buttons) != 0;
        int press = (pressed & a->buttons) != 0;

        switch (a->kind) {
        case KIND_FIRE:
            fire_update(a, down, press);
            break;
        case KIND_TOUCH:
            if (!down) {
                t_end(&a->t);
            } else if (press && !a->t.active) {
                int idx = first_usable(a->controls, 3);
                if (idx >= 0)
                    t_begin_on(&a->t, idx);
            }
            break;
        case KIND_JUMP:
            // A quick tap on the background (GUI_TouchesEnded sets
            // quick_touch): jump, or fire with firing method 0 and the
            // weapon ready, exactly like tapping the screen.
            if (press && quick_touch)
                *quick_touch = 1;
            break;
        case KIND_HOLSTER:
            if (press)
                holster_toggle();
            break;
        case KIND_NEXT_WEAPON:
            if (press)
                next_weapon();
            break;
        }
    }
}

static void release_actions(void) {
    for (int i = 0; i < ACT_COUNT; i++)
        t_end(&actions[i].t);
    t_end(&move_touch);
    fire_drew = 0;
}

static void press_back(void) {
    if (!nativeOnBackPressed)
        return;
    // true = main menu; Android would show "Exit?". Exiting is done from the
    // PS button on Vita, so this is only logged.
    if (nativeOnBackPressed(&jni, activity_obj))
        l_info("input: back pressed at main menu (exit dialog skipped)");
}

/* --- Facebook controls ---------------------------------------------------- */

static int is_social_control(int idx) {
    int *fb_controls[] = { ctl_fb_hunt, ctl_fb_stats, ctl_fb_trophy_stat, ctl_fb_trophy, ctl_fb_login };
    for (unsigned i = 0; i < sizeof(fb_controls) / sizeof(fb_controls[0]); i++)
        if (fb_controls[i] && *fb_controls[i] == idx)
            return 1;
    return 0;
}

static void hide_social_controls(void) {
    int *fb_controls[] = { ctl_fb_hunt, ctl_fb_stats, ctl_fb_trophy_stat, ctl_fb_trophy, ctl_fb_login };
    for (unsigned i = 0; i < sizeof(fb_controls) / sizeof(fb_controls[0]); i++) {
        if (fb_controls[i] && *fb_controls[i] >= 0) {
            uint8_t *c = control(*fb_controls[i]);
            if (c) {
                c[0x32] = 0; // enabled / active = false
                c[0x33] = 0; // visible = false
                *(float *)(c + 0x0C) = -9999.0f;
                *(float *)(c + 0x10) = -9999.0f;
                *(float *)(c + 0x1C) = 0.0f;
                *(float *)(c + 0x20) = 0.0f;
            }
        }
    }
}

/* --- menu navigation ------------------------------------------------------ */

// Outside the hunting HUD the d-pad / left stick move a cursor over the
// buttons and sliders of the active GUI group, Cross presses the selected one
// (a synthetic touch held while Cross is held), Left/Right move a slider and
// Circle is "back".
enum { DIR_UP, DIR_DOWN, DIR_LEFT, DIR_RIGHT };

#define NAV_REPEAT_DELAY 400000
#define NAV_REPEAT_RATE  130000

static int nav_sel = -1;
static int nav_group = -1;
static uint32_t nav_subgroups;
static int nav_visible;
static vtouch nav_touch;
static uint32_t nav_prev_dirs;
static uint64_t nav_repeat_at;
static uint64_t nav_hint_until;

static int control_type(int idx) {
    return *(int *) (control(idx) + 0x08);
}

static int nav_candidate(int idx) {
    if (!control_usable(idx) || is_social_control(idx))
        return 0;
    int type = control_type(idx);
    if (type != 0 && type != 1)
        return 0;
    float x0, y0, x1, y1;
    control_rect(idx, &x0, &y0, &x1, &y1);
    float w = x1 - x0, h = y1 - y0;
    if (w < 2.0f || h < 2.0f)
        return 0;
    // Full-screen tap catchers (photo gallery, ...) are not buttons.
    if (w * h > 0.6f * LOGICAL_W * LOGICAL_H)
        return 0;
    // Nothing that can't be seen (the hunt menu's other pages, ...).
    float cx = (x0 + x1) * 0.5f, cy = (y0 + y1) * 0.5f;
    return cx >= 0.0f && cx <= LOGICAL_W && cy >= 0.0f && cy <= LOGICAL_H;
}

/* What the control looks like on screen, which is not always its touch
 * rectangle. Mirrors GUI_DrawControls() (pseudo-C ~15865):
 *  - sprite (+0x30) at +0x38, x scale (+0x2C), drawn by Sprites_DrawSprite()
 *    with flags (ctl & 1) | (ctl & 2) | (ctl & 8 ? 0 : 8): 1 = left edge at x,
 *    2 = right edge at x, neither = centered; 8 = bottom edge at y, else
 *    centered. Without control flag 1 a sprite is centered on x while its
 *    touch rectangle starts at x.
 *  - slider (type 1): plus the knob sprite (+0x40) at x + (+0x14), y + (+0x18).
 *  - text (+0x31, buttons only) at x + (+0x16C), y + (+0x170), font +0x48,
 *    text +0x6C, font scale +0x68; Font_PrintText() flags 1 right (ctl 2),
 *    2 h-center (ctl 4), 4 v-center (ctl 8). Its first line's top is
 *    y + line height (y + half of it when v-centered).
 *  - hunt menu cells (Menu_GetCellButtonParams, ~18835): invisible 90x68
 *    buttons whose touch rectangle starts 16 px above the cell sprite the menu
 *    draws there (menu_hunt_cell_empty, Render_Menu ~25140). */
#define HUNT_CELL_W     90.0f
#define HUNT_CELL_H     68.0f
#define HUNT_CELL_DROP  16.0f

typedef struct { float x0, y0, x1, y1; int set; } Box;

static void box_add(Box *b, float x0, float y0, float x1, float y1) {
    if (x1 <= x0 || y1 <= y0)
        return;
    if (!b->set) {
        b->x0 = x0; b->y0 = y0; b->x1 = x1; b->y1 = y1;
        b->set = 1;
        return;
    }
    if (x0 < b->x0) b->x0 = x0;
    if (y0 < b->y0) b->y0 = y0;
    if (x1 > b->x1) b->x1 = x1;
    if (y1 > b->y1) b->y1 = y1;
}

static int sprite_size(void *sprite, float *w, float *h) {
    float size[2] = { 0.0f, 0.0f };
    // Sprites_GetSpriteSize() leaves `size` untouched for an invalid handle.
    if (Sprites_GetSpriteSize && sprite)
        Sprites_GetSpriteSize(sprite, size);
    *w = size[0];
    *h = size[1];
    return size[0] > 0.0f && size[1] > 0.0f;
}

static void box_add_sprite(Box *b, void *sprite, float x, float y, float scale, uint32_t sflags) {
    float w, h;
    if (!sprite_size(sprite, &w, &h))
        return;
    w *= scale;
    h *= scale;
    float x0 = (sflags & 1) ? x : (sflags & 2) ? x - w : x - w * 0.5f;
    float y0 = (sflags & 8) ? y : (sflags & 4) ? y - h : y - h * 0.5f;
    box_add(b, x0, y0, x0 + w, y0 + h);
}

static void control_visual_rect(int idx, float *x0, float *y0, float *x1, float *y1) {
    uint8_t *c = control(idx);
    float x = *(float *) (c + 0x0C), y = *(float *) (c + 0x10);
    float scale = *(float *) (c + 0x2C);
    uint32_t flags = *(uint32_t *) (c + 0x24);
    int type = control_type(idx);
    Box b = {0};

    if (c[0x30]) {
        uint32_t sflags = (flags & 3) | ((flags & 8) ? 0 : 8);
        box_add_sprite(&b, c + 0x38, x, y, scale, sflags);
        if (type == 1)
            box_add_sprite(&b, c + 0x40, x + *(float *) (c + 0x14), y + *(float *) (c + 0x18),
                           scale, sflags);
    }

    const char *text = (const char *) (c + 0x6C);
    if (c[0x31] && type == 0 && text[0]) {
        float fs = *(float *) (c + 0x68), tw, th, lh;
        overlay_text_size(text, (const char *) (c + 0x48), &tw, &th);
        overlay_text_size("A", (const char *) (c + 0x48), NULL, &lh);
        tw *= fs; th *= fs; lh *= fs;
        float tx = x + *(float *) (c + 0x16C), ty = y + *(float *) (c + 0x170);
        float left = (flags & 2) ? tx - tw : (flags & 4) ? tx - tw * 0.5f : tx;
        float top = (flags & 8) ? ty + lh * 0.5f : ty + lh;
        box_add(&b, left, top - th, left + tw, top);
    }

    float hx0, hy0, hx1, hy1;
    control_rect(idx, &hx0, &hy0, &hx1, &hy1);

    if (!b.set && !c[0x30] && !c[0x31] && *(int *) c == 1 &&
        fabsf(*(float *) (c + 0x1C) - HUNT_CELL_W) < 0.5f &&
        fabsf(*(float *) (c + 0x20) - HUNT_CELL_H) < 0.5f) {
        float w, h;
        if (!sprite_size(menu_hunt_cell_empty, &w, &h)) {
            w = HUNT_CELL_W;
            h = HUNT_CELL_H + HUNT_CELL_DROP;
        }
        box_add(&b, hx0, hy0 - HUNT_CELL_DROP, hx0 + w, hy0 - HUNT_CELL_DROP + h);
    }

    if (!b.set)
        box_add(&b, hx0, hy0, hx1, hy1);
    *x0 = b.x0; *y0 = b.y0; *x1 = b.x1; *y1 = b.y1;
}

static void nav_center(int idx, float *cx, float *cy) {
    float x0, y0, x1, y1;
    control_visual_rect(idx, &x0, &y0, &x1, &y1);
    *cx = (x0 + x1) * 0.5f;
    *cy = (y0 + y1) * 0.5f;
}

// Top-most (y up), then left-most.
static int nav_first(void) {
    int best = -1;
    float bx = 0.0f, by = 0.0f;
    for (int i = 0; i < *gui_controls_count; i++) {
        if (!nav_candidate(i))
            continue;
        float cx, cy;
        nav_center(i, &cx, &cy);
        if (best < 0 || cy > by + 4.0f || (fabsf(cy - by) <= 4.0f && cx < bx)) {
            best = i;
            bx = cx;
            by = cy;
        }
    }
    return best;
}

static int nav_find(int from, int dir) {
    float fx, fy;
    nav_center(from, &fx, &fy);
    int best = -1;
    float best_score = 0.0f;
    for (int i = 0; i < *gui_controls_count; i++) {
        if (i == from || !nav_candidate(i))
            continue;
        float cx, cy;
        nav_center(i, &cx, &cy);
        float dx = cx - fx, dy = cy - fy, along, across;
        switch (dir) {
        case DIR_UP:    along = dy;  across = fabsf(dx); break;
        case DIR_DOWN:  along = -dy; across = fabsf(dx); break;
        case DIR_LEFT:  along = -dx; across = fabsf(dy); break;
        default:        along = dx;  across = fabsf(dy); break;
        }
        if (along < 2.0f)
            continue;
        float score = along + across * 2.5f;
        if (best < 0 || score < best_score) {
            best = i;
            best_score = score;
        }
    }
    return best;
}

static void nav_slider(int idx, int dir) {
    if (!GUI_SetSliderValue)
        return;
    uint8_t *c = control(idx);
    float v = *(float *) (c + 0x174), lo = *(float *) (c + 0x178), hi = *(float *) (c + 0x17C);
    float step = (hi - lo) / 20.0f;
    v = clampf(v + (dir == DIR_RIGHT ? step : -step), lo < hi ? lo : hi, lo < hi ? hi : lo);
    GUI_SetSliderValue(idx, v);
}

static int pick_dir(uint32_t dirs) {
    if (dirs & SCE_CTRL_UP)    return DIR_UP;
    if (dirs & SCE_CTRL_DOWN)  return DIR_DOWN;
    if (dirs & SCE_CTRL_LEFT)  return DIR_LEFT;
    if (dirs & SCE_CTRL_RIGHT) return DIR_RIGHT;
    return -1;
}

static void nav_update(uint32_t held, uint32_t pressed, float lx, float ly) {
    if (!gui_controls || !gui_controls_count || !gui_active_group || !gui_active_subgroups)
        return;

    // New screen: start over from its first control.
    if (*gui_active_group != nav_group || *gui_active_subgroups != nav_subgroups) {
        nav_group = *gui_active_group;
        nav_subgroups = *gui_active_subgroups;
        t_end(&nav_touch);
        nav_sel = -1;
    }
    if (nav_sel >= 0 && !nav_candidate(nav_sel)) {
        t_end(&nav_touch);
        nav_sel = -1;
    }

    uint32_t dirs = held & (SCE_CTRL_UP | SCE_CTRL_DOWN | SCE_CTRL_LEFT | SCE_CTRL_RIGHT);
    if (ly < -0.6f) dirs |= SCE_CTRL_UP;
    if (ly >  0.6f) dirs |= SCE_CTRL_DOWN;
    if (lx < -0.6f) dirs |= SCE_CTRL_LEFT;
    if (lx >  0.6f) dirs |= SCE_CTRL_RIGHT;

    uint64_t now = sceKernelGetProcessTimeWide();
    uint32_t new_dirs = dirs & ~nav_prev_dirs;
    nav_prev_dirs = dirs;
    int dir = -1;
    if (new_dirs) {
        dir = pick_dir(new_dirs);
        nav_repeat_at = now + NAV_REPEAT_DELAY;
    } else if (dirs && now >= nav_repeat_at) {
        dir = pick_dir(dirs);
        nav_repeat_at = now + NAV_REPEAT_RATE;
    }

    // The first press after touching the screen only brings the cursor back.
    if (!nav_visible) {
        if (dir >= 0 || (pressed & SCE_CTRL_CROSS)) {
            nav_visible = 1;
            if (nav_sel < 0)
                nav_sel = nav_first();
        }
        if (pressed & SCE_CTRL_CIRCLE)
            press_back();
        return;
    }

    if (nav_sel < 0)
        nav_sel = nav_first();
    if (nav_sel < 0)
        return;

    if (dir >= 0) {
        if (control_type(nav_sel) == 1 && (dir == DIR_LEFT || dir == DIR_RIGHT)) {
            nav_slider(nav_sel, dir);
        } else {
            int next = nav_find(nav_sel, dir);
            if (next >= 0) {
                t_end(&nav_touch);
                nav_sel = next;
            }
        }
    }

    if ((pressed & SCE_CTRL_CROSS) && !nav_touch.active)
        t_begin_on(&nav_touch, nav_sel);
    else if (!(held & SCE_CTRL_CROSS))
        t_end(&nav_touch);

    if (pressed & SCE_CTRL_CIRCLE)
        press_back();
}

static void draw_overlay(void) {
    if (vita_menu_active()) {
        vita_menu_draw();
        return;
    }
    if (hud_active())
        return;

    if (nav_visible && nav_sel >= 0 && nav_candidate(nav_sel)) {
        float x0, y0, x1, y1;
        control_visual_rect(nav_sel, &x0, &y0, &x1, &y1);
        x0 -= 2.0f; y0 -= 2.0f; x1 += 2.0f; y1 += 2.0f;
        // Corner brackets only, so the control itself stays readable; a faint
        // fill while Cross is held is the "pressed" feedback.
        const uint32_t col = 0xb4ffffff;
        const float t = 1.5f;
        float lx = (x1 - x0) * 0.25f, ly = (y1 - y0) * 0.25f;
        if (lx > 10.0f) lx = 10.0f;
        if (ly > 10.0f) ly = 10.0f;
        if (nav_touch.active)
            overlay_rect(x0, y0, x1, y1, 0x28ffffff);
        overlay_rect(x0 - t, y0 - t, x0 + lx, y0, col);   // top-left
        overlay_rect(x0 - t, y0, x0, y0 + ly, col);
        overlay_rect(x1 - lx, y0 - t, x1 + t, y0, col);   // top-right
        overlay_rect(x1, y0, x1 + t, y0 + ly, col);
        overlay_rect(x0 - t, y1, x0 + lx, y1 + t, col);   // bottom-left
        overlay_rect(x0 - t, y1 - ly, x0, y1, col);
        overlay_rect(x1 - lx, y1, x1 + t, y1 + t, col);   // bottom-right
        overlay_rect(x1, y1 - ly, x1 + t, y1, col);
    }

    if (sceKernelGetProcessTimeWide() < nav_hint_until) {
        float w = overlay_w();
        overlay_rect(w * 0.5f - 120.0f, 3.0f, w * 0.5f + 120.0f, 19.0f, 0xa0000000);
        overlay_text(w * 0.5f, 11.0f, 0.8f, 0xffffffff,
                     "SELECT: Vita controls & camera", OVL_HCENTER | OVL_VCENTER, OVL_FONT);
    }
}

/* --- bindings ------------------------------------------------------------- */

int input_action_count(void) { return ACT_COUNT; }

const char *input_action_label(int action) {
    return action >= 0 && action < ACT_COUNT ? actions[action].label : "";
}

uint32_t input_action_buttons(int action) {
    return action >= 0 && action < ACT_COUNT ? actions[action].buttons : 0;
}

void input_action_bind(int action, uint32_t button, int add) {
    if (action < 0 || action >= ACT_COUNT)
        return;
    for (int i = 0; i < ACT_COUNT; i++)
        actions[i].buttons &= ~button;
    actions[action].buttons = add ? (actions[action].buttons | button) : button;
}

void input_action_clear(int action) {
    if (action >= 0 && action < ACT_COUNT)
        actions[action].buttons = 0;
}

void input_controls_defaults(void) {
    for (int i = 0; i < ACT_COUNT; i++)
        actions[i].buttons = actions[i].default_buttons;
}

uint32_t input_bindable_buttons(void) {
    uint32_t mask = 0;
    for (unsigned i = 0; i < BUTTON_NAMES_COUNT; i++)
        if (button_names[i].label)
            mask |= button_names[i].mask;
    return mask & ~SCE_CTRL_START;
}

static void buttons_join(uint32_t mask, char *out, size_t size, int labels) {
    size_t len = 0;
    out[0] = '\0';
    for (unsigned i = 0; i < BUTTON_NAMES_COUNT; i++) {
        const ButtonName *b = &button_names[i];
        if (!b->label || !(mask & b->mask))
            continue;
        int n = snprintf(out + len, size - len, "%s%s", len ? ", " : "", labels ? b->label : b->name);
        if (n < 0 || (size_t) n >= size - len)
            break;
        len += n;
    }
    if (!len)
        snprintf(out, size, "%s", labels ? "-" : "NONE");
}

void input_buttons_text(uint32_t mask, char *out, size_t size) {
    buttons_join(mask, out, size, 1);
}

// Copies the first word of `s` (up to whitespace, ',', '=', ':', '#', ';'),
// upper-cased.
static void word(const char *s, char *out, size_t size) {
    while (*s == ' ' || *s == '\t')
        s++;
    size_t len = 0;
    while (*s && !strchr(" \t,=:#;\r\n", *s) && len < size - 1)
        out[len++] = (char) toupper((unsigned char) *s++);
    out[len] = '\0';
}

static uint32_t parse_button_token(const char *tok) {
    char clean[32];
    word(tok, clean, sizeof(clean));
    if (!clean[0] || strcmp(clean, "NONE") == 0)
        return 0;
    for (unsigned i = 0; i < BUTTON_NAMES_COUNT; i++) {
        if (strcmp(clean, button_names[i].name) == 0)
            return button_names[i].mask;
    }
    l_warn("input: unknown button '%s'", clean);
    return 0;
}

static uint32_t parse_button_list(const char *p) {
    uint32_t mask = 0;
    while (*p && *p != '#' && *p != ';' && *p != '\r' && *p != '\n') {
        mask |= parse_button_token(p);
        while (*p && *p != ',' && *p != '#' && *p != ';' && *p != '\r' && *p != '\n')
            p++;
        if (*p == ',')
            p++;
    }
    return mask;
}

static int find_action_index(const char *name) {
    char clean[32];
    word(name, clean, sizeof(clean));
    for (int i = 0; i < ACT_COUNT; i++) {
        if (strcmp(clean, actions[i].name) == 0)
            return i;
    }
    return -1;
}

void input_controls_save(void) {
    FILE *f = fopen(CONTROLS_PATH, "w");
    if (!f) {
        l_error("input: cannot write %s", CONTROLS_PATH);
        return;
    }
    fprintf(f,
        "# Carnivores: Dinosaur Hunter - PS Vita controls\n"
        "# Also editable in game: START + SELECT (or SELECT in the menus).\n"
        "#\n"
        "# Buttons: CROSS, CIRCLE, SQUARE, TRIANGLE, L1, R1, UP, DOWN, LEFT, RIGHT,\n"
        "#   SELECT, L2 / R2 (rear touch top-left / top-right),\n"
        "#   L3 / R3 (rear touch bottom-left / bottom-right), NONE.\n"
        "#   START is always pause / back.\n"
        "#\n"
        "# Actions:\n"
        "#   FIRE         Fire (draws the weapon first if holstered) / take photo\n"
        "#   JUMP         Jump (same as a quick tap on the screen)\n"
        "#   WEAPON       Draw / holster the current weapon\n"
        "#   NEXT_WEAPON  Switch to the next weapon\n"
        "#   WEAPON_MENU  Weapon button of the touch HUD (weapon list)\n"
        "#   BINOCULARS   Binoculars on / off\n"
        "#   CALL         Call dinosaurs\n"
        "#   MAP          Map\n"
        "#   ZOOM_IN / ZOOM_OUT  Photo mode zoom\n"
        "#\n"
        "# ACTION = BUTTON, BUTTON ...   (or BUTTON = ACTION)\n"
        "\n"
        "VERSION = %d\n", CONTROLS_VERSION);
    for (int i = 0; i < ACT_COUNT; i++) {
        char list[128];
        buttons_join(actions[i].buttons, list, sizeof(list), 0);
        fprintf(f, "%s = %s\n", actions[i].name, list);
    }
    fclose(f);
}

void input_reload_controls(void) {
    input_controls_defaults();

    FILE *f = fopen(CONTROLS_PATH, "r");
    if (!f) {
        input_controls_save();
        l_info("input: generated default %s", CONTROLS_PATH);
        return;
    }

    uint32_t parsed[ACT_COUNT] = {0};
    int seen[ACT_COUNT] = {0};
    int version = 1;
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        char *p = line;
        while (*p == ' ' || *p == '\t')
            p++;
        if (*p == '#' || *p == ';' || *p == '\r' || *p == '\n' || *p == '\0')
            continue;
        char *eq = strpbrk(p, "=:");
        if (!eq)
            continue;
        *eq = '\0';
        char *right = eq + 1;

        char key[32];
        word(p, key, sizeof(key));
        if (strcmp(key, "VERSION") == 0) {
            sscanf(right, "%d", &version);
            continue;
        }

        int act = find_action_index(p);
        if (act >= 0) {
            seen[act] = 1;
            parsed[act] |= parse_button_list(right);
        } else {
            act = find_action_index(right);
            uint32_t btn = parse_button_token(p);
            if (act >= 0 && btn) {
                seen[act] = 1;
                parsed[act] |= btn;
            }
        }
    }
    fclose(f);

    if (version < CONTROLS_VERSION) {
        // v1 bound FIRE to game_fire only, which the default firing method
        // hides: start from the new defaults instead of mixing both sets.
        input_controls_save();
        l_info("input: %s was version %d, replaced with the defaults", CONTROLS_PATH, version);
    } else {
        for (int i = 0; i < ACT_COUNT; i++)
            if (seen[i])
                actions[i].buttons = parsed[i];
    }

    for (int i = 0; i < ACT_COUNT; i++)
        l_info("input: %-11s -> 0x%08X", actions[i].name, actions[i].buttons);
}

/* --- HUD opacity --------------------------------------------------------- */

// The touch HUD is not needed with physical controls, so its buttons are drawn
// at setting_hudOpacity percent (config.txt `hud_opacity`, 1 by default).
// GUI_DrawControls() draws every control with the color at +0x28 (ARGB, alpha
// in the top byte; the engine sets 0x80ffffff for these). The engine rewrites
// some of those colors while playing (the call button fades), so the alpha is
// scaled right before each draw and the original color put back afterwards.
// Only the hunting HUD buttons: menus keep their look, and the compass is not
// a GUI control (it is the compas.3dn model), so it is left as it is.
// The hook is always installed: the port menu changes the opacity live.
#define CTL_COLOR 0x28

static const char *hud_names[] = {
    "game_movement_controller", "game_fire", "game_alternative_fire",
    "game_weapon", "game_binoculars", "game_call", "game_map", "game_menu",
    "game_photomode_shot", "game_photomode_zoom_in", "game_photomode_zoom_out",
};
#define HUD_COUNT (sizeof(hud_names) / sizeof(hud_names[0]))

static int *hud_controls[HUD_COUNT];
static so_hook draw_controls_hook;

static void GUI_DrawControls_hook(void) {
    hide_social_controls();

    if (setting_hudOpacity >= 100) {
        SO_CONTINUE(int, draw_controls_hook);
        return;
    }

    uint32_t saved[HUD_COUNT];
    uint8_t *c[HUD_COUNT];

    for (unsigned i = 0; i < HUD_COUNT; i++) {
        c[i] = hud_controls[i] ? control(*hud_controls[i]) : NULL;
        if (!c[i])
            continue;
        uint32_t *color = (uint32_t *) (c[i] + CTL_COLOR);
        saved[i] = *color;
        uint32_t a = ((saved[i] >> 24) * setting_hudOpacity + 99) / 100;
        *color = (saved[i] & 0x00ffffff) | (a << 24);
    }

    SO_CONTINUE(int, draw_controls_hook);

    // Restore in reverse: a control listed twice gets its real color back.
    for (int i = HUD_COUNT - 1; i >= 0; i--)
        if (c[i])
            *(uint32_t *) (c[i] + CTL_COLOR) = saved[i];
}

static void hud_opacity_patch(void) {
    gui_controls       = (uint8_t *) so_symbol(&so_mod, "gui_controls");
    gui_controls_count = (int *) so_symbol(&so_mod, "gui_controls_count");
    uintptr_t fn = so_symbol(&so_mod, "_Z16GUI_DrawControlsv");
    if (!gui_controls || !gui_controls_count || !fn) {
        l_error("input: GUI_DrawControls not hooked, HUD opacity unchanged");
        return;
    }
    for (unsigned i = 0; i < HUD_COUNT; i++) {
        hud_controls[i] = (int *) so_symbol(&so_mod, hud_names[i]);
        if (!hud_controls[i])
            l_error("input: symbol %s not found", hud_names[i]);
    }
    draw_controls_hook = hook_addr(fn, (uintptr_t) &GUI_DrawControls_hook);
}

/* --- public API --------------------------------------------------------- */

#define SYM(var, name) do { \
        var = (void *) so_symbol(&so_mod, name); \
        if (!var) l_error("input: symbol %s not found", name); \
    } while (0)

void input_patch(void) {
    hud_opacity_patch();

    touched_locations       = (float *) so_symbol(&so_mod, "gui_touched_locations");
    touched_start_locations = (float *) so_symbol(&so_mod, "gui_touched_start_locations");
    touched_controls        = (int *) so_symbol(&so_mod, "gui_touched_controls");
    uintptr_t fn = so_symbol(&so_mod, "_Z26GUI_GetBackgroundMovementsPfS_");
    if (!touched_locations || !touched_start_locations || !touched_controls || !fn) {
        l_error("input: GUI_GetBackgroundMovements not hooked, right stick camera disabled");
        return;
    }
    hook_addr(fn, (uintptr_t) &GUI_GetBackgroundMovements_hook);
}

void input_init(void) {
    SYM(touchesBegan,        "Java_com_tatem_dinhunter_DinHunterGLSurface_touchesBegan");
    SYM(touchesMoved,        "Java_com_tatem_dinhunter_DinHunterGLSurface_touchesMoved");
    SYM(touchesEnded,        "Java_com_tatem_dinhunter_DinHunterGLSurface_touchesEnded");
    SYM(touchesCancelled,    "Java_com_tatem_dinhunter_DinHunterGLSurface_touchesCancelled");
    SYM(nativeOnBackPressed, "Java_com_tatem_dinhunter_DinHunterAndroid_nativeOnBackPressed");

    SYM(p_scaleX,             "scaleX");
    SYM(p_scaleY,             "scaleY");
    SYM(p_real_width,         "real_width");
    SYM(p_real_height,        "real_height");
    SYM(gui_controls,         "gui_controls");
    SYM(gui_controls_count,   "gui_controls_count");
    SYM(gui_active_group,     "gui_active_group");
    SYM(gui_active_subgroups, "gui_active_subgroups");

    SYM(ctl_move,           "game_movement_controller");
    SYM(ctl_menu,           "game_menu");
    SYM(ctl_fire,           "game_fire");
    SYM(ctl_alt_fire,       "game_alternative_fire");
    SYM(ctl_weapon,         "game_weapon");
    SYM(ctl_binoculars,     "game_binoculars");
    SYM(ctl_call,           "game_call");
    SYM(ctl_map,            "game_map");
    SYM(ctl_photo_shot,     "game_photomode_shot");
    SYM(ctl_photo_zoom_in,  "game_photomode_zoom_in");
    SYM(ctl_photo_zoom_out, "game_photomode_zoom_out");

    SYM(weapons,                    "weapons");
    SYM(current_weapon,             "current_weapon");
    SYM(using_weapons,              "using_weapons");
    SYM(using_weapons_count,        "using_weapons_count");
    SYM(quick_touch,                "quick_touch");
    SYM(map_mode,                   "map_mode");
    SYM(firing_method,              "firing_method");
    SYM(game_menu_show_photomode,   "game_menu_show_photomode");
    SYM(Weapon_TakeWeapon,          "_Z17Weapon_TakeWeaponv");
    SYM(Weapon_Fire,                "_Z11Weapon_Firev");
    SYM(Weapon_ChangeCurrentWeapon, "_Z26Weapon_ChangeCurrentWeaponi");
    SYM(GUI_SetSliderValue,         "_Z18GUI_SetSliderValueif");
    SYM(Sprites_GetSpriteSize,      "_Z21Sprites_GetSpriteSizeP14_SpriteHandlerP9_Vector2D");
    SYM(menu_hunt_cell_empty,       "menu_hunt_cell_empty");

    ctl_fb_hunt        = (int *) so_symbol(&so_mod, "game_share_hunt_statistic_with_facebook");
    ctl_fb_stats       = (int *) so_symbol(&so_mod, "game_share_statistics_with_facebook");
    ctl_fb_trophy_stat = (int *) so_symbol(&so_mod, "game_share_trophy_statistic_with_facebook");
    ctl_fb_trophy      = (int *) so_symbol(&so_mod, "game_share_trophy_with_facebook");
    ctl_fb_login       = (int *) so_symbol(&so_mod, "menu_options_facebook_login");

    input_reload_controls();

    for (int s = 0; s < REAL_SLOTS; s++)
        real[s].id = -1;

    sceTouchSetSamplingState(SCE_TOUCH_PORT_FRONT, SCE_TOUCH_SAMPLING_STATE_START);
    sceTouchSetSamplingState(SCE_TOUCH_PORT_BACK,  SCE_TOUCH_SAMPLING_STATE_START);
    sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG_WIDE);

    ready = touchesBegan && touchesMoved && touchesEnded && touchesCancelled &&
            gui_controls && gui_controls_count && gui_active_group && gui_active_subgroups;
    if (!ready) {
        l_error("input: DinHunterGLSurface natives or GUI tables missing, input disabled");
        return;
    }

    overlay_set_callback(draw_overlay);
    nav_hint_until = sceKernelGetProcessTimeWide() + 12000000;
}

static void open_port_menu(int hud) {
    release_actions();
    t_end(&nav_touch);
    release_real_touch();
    look_dx = look_dy = 0.0f;
    if (hud)
        press_back(); // pause the hunt underneath
    nav_hint_until = 0;
    vita_menu_open();
}

void input_update(void) {
    if (!ready)
        return;

    hide_social_controls();

    SceCtrlData pad;
    memset(&pad, 0, sizeof(pad));
    sceCtrlPeekBufferPositive(0, &pad, 1);

    uint32_t buttons = pad.buttons | poll_rear_touch();
    uint32_t pressed = buttons & ~old_buttons;
    uint32_t released = old_buttons & ~buttons;
    old_buttons = buttons;

    if (vita_menu_active()) {
        if (!(buttons & SCE_CTRL_START))
            start_combo = 0;
        vita_menu_update(buttons, pressed);
        // Closed with START: its release must not count as "back".
        if (!vita_menu_active() && (buttons & SCE_CTRL_START))
            start_combo = 1;
        update_look(0.0f, 0.0f, 0);
        return;
    }

    int hud = hud_active();

    // START + SELECT (either order) opens the port menu; START alone is the
    // Android back key (pause in game, previous page in menus), sent on
    // release so the combo doesn't also pause. In the game's own menus SELECT
    // alone is enough.
    if (((buttons & SCE_CTRL_START) && (pressed & SCE_CTRL_SELECT)) ||
        ((buttons & SCE_CTRL_SELECT) && (pressed & SCE_CTRL_START))) {
        start_combo = 1;
        open_port_menu(hud);
        return;
    }
    if (!hud && (pressed & SCE_CTRL_SELECT)) {
        open_port_menu(0);
        return;
    }
    if (released & SCE_CTRL_START) {
        if (!start_combo)
            press_back();
        start_combo = 0;
    }

    if (hud != was_hud) {
        release_actions();
        t_end(&nav_touch);
        nav_prev_dirs = 0;
        if (was_hud && !hud)
            nav_hint_until = sceKernelGetProcessTimeWide() + 6000000;
        was_hud = hud;
    }

    if (update_real_touch() > 0)
        nav_visible = 0;

    float lx = axis(pad.lx), ly = axis(pad.ly), rx = axis(pad.rx), ry = axis(pad.ry);
    float mx = lx, my = ly, cx = rx, cy = ry;
    if (setting_swapSticks) {
        mx = rx; my = ry;
        cx = lx; cy = ly;
    }

    if (hud) {
        uint32_t act_held = buttons, act_pressed = pressed;
        if (buttons & SCE_CTRL_START) {
            act_held &= ~SCE_CTRL_SELECT;
            act_pressed &= ~SCE_CTRL_SELECT;
        }
        update_actions(act_held, act_pressed);
        update_move(mx, my);
    } else {
        nav_update(buttons, pressed, lx, ly);
    }
    update_look(cx, cy, hud);
}

void input_release_all(void) {
    if (!ready)
        return;
    release_actions();
    t_end(&nav_touch);
    look_dx = look_dy = 0.0f;
    release_real_touch();
    old_buttons = 0;
}
