# Carnivores: Dinosaur Hunter — PS Vita · v1.0.0 "First Hunt"

> Paste the section below into the GitHub release description. The title above goes in the
> release-title field; suggested tag: `v1.0.0`.

---

**First public build: the Android version of Carnivores: Dinosaur Hunter (1.3.1) runs on PS Vita —
menus, map loading and hunting in the 3D maps, with FMOD sound.**

## Requirements

- PS Vita / PS TV with HENkaku/Enso (3.60–3.74 with a kernel plugin loader).
- `kubridge.skprx` in `*KERNEL` of your taiHEN config.
- `libshacccg.suprx` in `ur0:data/` (use ShaRKBR33D).
- Your own copy of the game APK (Android 1.3.1, `armeabi`).

## Install

1. Install `carnivoresdinosaurhunter.vpk` with VitaShell.
2. Create `ux0:data/carnivoresdinosaurhunter/` and copy into it:
   - the APK, named `CarnivoresDinosaurHunter.apk`
   - `libdinHunter.so` and `libfmodex.so` from the APK's `lib/armeabi/`
   - optional, the two content packs (separate apps on Android):
     `CarnivoresBundleOne.apk` renamed to `bundle1.apk` and `CarnivoresBundleTwo.apk` renamed to
     `bundle2.apk`
3. Launch. The first load takes about 30 seconds on a static screen; that is normal.

## What works

- Boot, full asset loading straight from the APK (textures, fonts, models, sounds).
- Main menu, options and map selection at 60 FPS.
- Map loading and hunting in the 3D maps (confirmed on hardware, no GPU crash).
- FMOD music and sound effects.
- Content packs from their own APKs: **Pack 1** (`com.tatem.dinhunter.bundle.one`) adds areas 3
  and 4 and the sniper rifle; **Pack 2** (`com.tatem.dinhunter.bundle.two`) adds area 6, the
  double-barrel shotgun and the crossbow. A pack is unlocked only when its APK is in the data
  folder; without it, its maps and weapons stay locked instead of crashing.
- Saved games (settings and hunter profile) load on the next boot.
- The touch HUD is drawn at 1% opacity while hunting (the physical controls replace it); the
  compass and the menus are unchanged. `hud_opacity` in `config.txt` sets it (100 = original).
- Front touch screen with the original controls.

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
| Circle / D-pad up | Call |
| Select / D-pad down | Map |
| D-pad left / right | Weapon list / photo zoom in |
| Start | Pause / back |
| Start + Select | PS Vita controls & camera menu (Select alone in menus) |

- Every action can be remapped in game (Start + Select), including the rear touch quadrants.
  Camera options there too: speed, invert up/down and left/right, swap sticks.
- The game's menus work with buttons: d-pad / left stick to move, Cross to press, Left/Right on
  sliders, Circle to go back.
- Fire works with every "firing method" of the game's options.
- Facebook buttons are hidden and its functions disabled.

## Options

Most are in the in-game menu (Start + Select). By hand, in
`ux0:data/carnivoresdinosaurhunter/config.txt`: `look_sensitivity` (10–400), `invert_look_y`,
`invert_look_x`, `swap_sticks`,
`msaa` (0 off / 1 2x / 2 4x), `hud_opacity` (0–100, default 1), `engine_log`, `vfp_float`. See the README for details.

## Under the hood

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

Performance work in this build:

- The engine is `armeabi` (ARMv5TE), so all of its float/double math goes through libgcc's software
  emulation. 32 of those helpers are redirected to the Vita's VFP; every hook was checked against
  the real `.so` with objdump.
- The engine's own logging is dropped before formatting (it wrote several lines per asset while
  loading and forced memory card flushes).
- MSAA off by default, Release build, vitaGL with error checking off and FFP shader cache on.

## Known issues

- The first load is a ~30 s static screen with no progress indicator.
- Frame rate in the 3D maps has not been measured yet.
- Many `FMOD error 'An invalid parameter...'` messages while loading dinosaur sounds. They don't
  crash, but some sounds may be missing.
- Content packs and booting with a saved game are confirmed on hardware. The FMOD
  missing-sound fix and the 1% HUD have not been through a long session yet.
- Physical buttons and the right stick camera are new in this build. They share the Ice Age
  port's code, but are not fully tested here yet.
- Exiting is done from the PS button (the Android "Exit?" dialog from the main menu is skipped).
