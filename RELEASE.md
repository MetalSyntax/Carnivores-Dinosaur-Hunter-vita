# Carnivores: Dinosaur Hunter — PS Vita · v1.1.0 "Controls overhaul"

> Paste the section below into the GitHub release description. The title above goes in the
> release-title field; suggested tag: `v1.1.0`.

---

**Firing that works out of the box, an in-game menu to remap every button and tune the camera,
and the whole game playable with buttons only, menus included.**

## What's new since v1.0.0

### Firing and weapons
- **Fire works with every firing method.** The game's default firing method hides the fire button
  that R/Cross were mapped to in v1.0.0, so only L fired. **R** now fires whichever fire button the
  game shows, fires directly in "tap the screen" mode, and draws a holstered weapon first.
- New actions: **jump** (Cross), **draw / holster** (Square), **next weapon** (Triangle),
  **weapon list** (Left), **photo zoom in / out** (Right / unbound).
- Circle calls dinosaurs (like D-pad up); Start alone is pause / back.

### In-game "PS Vita controls & camera" menu
Open it with **Start + Select** while hunting (the hunt pauses), or **Select** in the menus.
- Remap every action, including the rear touch corners. Cross sets a button, Square adds a
  second one, Triangle unbinds. A button belongs to one action at a time.
- Camera: right stick speed, invert up/down, invert left/right, swap sticks.
- Touch HUD opacity. Restore defaults.
- Saved to `controls.txt` and `config.txt` on close (both still editable by hand).

### Menus with buttons
D-pad or left stick moves a highlight, Cross presses, Left/Right move sliders, Circle goes back.
The highlight is fitted to what each item looks like on screen, and it works in the hunt menu
(areas, dinosaurs, weapons). Touching the screen hides it; the next D-pad press brings it back.

### Other
- Facebook share / login buttons are hidden and their functions disabled (no network on Vita).
- Small performance fix: showing or hiding GUI items no longer patches code on every call (the
  game does it dozens of times per frame).

## Requirements

- PS Vita / PS TV with HENkaku/Enso (3.60–3.74 with a kernel plugin loader).
- `kubridge.skprx` in `*KERNEL` of your taiHEN config.
- `libshacccg.suprx` in `ur0:data/` (use ShaRKBR33D).
- Your own copy of the game APK (Android **1.3.1**, `com.tatem.dinhunter`, `armeabi`).

Not compatible: **Carnivores: Dinosaur Hunter HD** (`com.tatem.dinhunterhd`, 1.6.5) and its OBB
(`main.423.com.tatem.dinhunterhd.obb`). That is a newer OpenGL ES 2 engine with a different
asset layout and formats; this port can't use any of it.

## Install / upgrade

1. Install `carnivoresdinosaurhunter.vpk` with VitaShell (over v1.0.0 is fine: saves and packs are
   kept).
2. Create `ux0:data/carnivoresdinosaurhunter/` and copy into it:
   - the APK, named `CarnivoresDinosaurHunter.apk`
   - `libdinHunter.so` and `libfmodex.so` from the APK's `lib/armeabi/`
   - optional, the two content packs (separate apps on Android):
     `CarnivoresBundleOne.apk` renamed to `bundle1.apk` and `CarnivoresBundleTwo.apk` renamed to
     `bundle2.apk`
3. Launch. The first load takes about 30 seconds on a static screen; that is normal.

A `controls.txt` from v1.0.0 is replaced by the new defaults on first boot.

## Controls

| Vita | Action |
|---|---|
| Touch screen | Original touch controls |
| Left stick | Move |
| Right stick | Look (frame-rate independent) |
| R | Fire (draws a holstered weapon first) / take photo |
| Cross | Jump |
| Square | Draw / holster weapon |
| Triangle | Next weapon |
| L | Binoculars |
| Circle / D-pad up | Call dinosaurs |
| Select / D-pad down | Map |
| D-pad left / right | Weapon list / photo zoom in |
| Start | Pause / back |
| Start + Select | PS Vita controls & camera menu (Select alone in menus) |

## Options

Most are in the in-game menu (Start + Select). By hand, in
`ux0:data/carnivoresdinosaurhunter/config.txt`: `look_sensitivity` (10–400), `invert_look_y`,
`invert_look_x`, `swap_sticks`, `msaa` (0 off / 1 2x / 2 4x), `hud_opacity` (0–100, default 1),
`engine_log`, `vfp_float`. See the README for details.

## What works (confirmed on hardware)

- Boot, full asset loading straight from the APK, menus at 60 FPS.
- Map loading and hunting in the 3D maps; a tester reports 30–60 FPS while hunting.
- FMOD music and sound effects.
- Content packs (**Pack 1**: areas 3–4 + sniper rifle; **Pack 2**: area 6 + double-barrel shotgun
  + crossbow), unlocked only when their APK is present.
- Saved games load on the next boot.
- Touch screen with the original controls, left/right stick movement and camera.

## Earlier fixes (v1.0.0)

Five bugs stood between the APK and a playable game, each found on real hardware:

- **The game couldn't read its own APK.** With SceLibc I/O, `fopen`/`fread` went to SceLibc while
  `fseeko`/`ftello`/`clearerr` still went to newlib with a SceLibc `FILE*`, so libzip computed a
  garbage archive size and every asset came back "not found". Those three now go through the
  SceLibc bridge too.
- **Permanent black screen at 60 FPS.** The prebuilt `libvitaGL.a` in VitaSDK is not built with
  `SOFTFP_ABI=1`, but the loader is softfp, so GXM's viewport received its floats in the wrong
  registers and every draw, even plain clears, landed off screen. vitaGL is now vendored and built
  with the softfp ABI.
- **GPU crash as soon as a hunt started.** vitaGL's `DRAW_SPEEDHACK` hands vertex arrays above a
  size threshold straight from game memory to the GPU. Menu sprites are small and got copied, but
  the terrain is not, and the GPU faulted reading memory it has no mapping for. The speedhack is off.
- **Black screen on the second launch.** The engine opens its save file and never closes it, so once
  a save exists the APK gets a different `FILE*`. The libzip inside the game was built for Android
  and checks `ferror()` by reading a bionic `FILE` field directly; on a SceLibc `FILE` that field
  is meaningless. With the first `FILE` it happened to read as "no error", with any other one the
  archive failed to open and nothing loaded. The five inlined checks are patched out. This is also
  what lets the content-pack APKs open next to the main one.
- **Crash in FMOD when a sound file is missing.** `Sounds_AddSound()` doesn't check whether the
  file was found, and passed leftover stack data to FMOD as the sound buffer. A missing file now
  gives FMOD an empty buffer, which it rejects cleanly.

Performance work:

- The engine is `armeabi` (ARMv5TE), so all of its float/double math goes through libgcc's software
  emulation. 32 of those helpers are redirected to the Vita's VFP; every hook was checked against
  the real `.so` with objdump.
- The engine's own logging is dropped before formatting (it wrote several lines per asset while
  loading and forced memory card flushes).
- MSAA off by default, Release build, vitaGL with error checking off and FFP shader cache on.

## Known issues

- **New in v1.1.0 and not yet confirmed on hardware:** the new fire/weapon actions, jump, the
  controls & camera menu (text placement and size), button navigation and its highlight. Please
  report anything that looks off, with a screenshot if you can.
- The first load is a ~30 s static screen with no progress indicator.
- Many `FMOD error 'An invalid parameter...'` messages while loading dinosaur sounds. They don't
  crash, but some sounds may be missing.
- The FMOD missing-sound fix has not been through a very long session yet.
- Exiting is done from the PS button (the Android "Exit?" dialog from the main menu is skipped).
- PS Vita trophies: code is in place but no trophy pack ships with the VPK, so they are disabled.
