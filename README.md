# Carnivores: Dinosaur Hunter — PS Vita port

<p align="center">
  <img src="extras/livearea/bg0.png" alt="Carnivores: Dinosaur Hunter PS Vita port">
</p>

so-loader port of the Android version (1.3.1) of *Carnivores: Dinosaur Hunter* (Tatem Games / Action Forms).
It runs the original `libdinHunter.so` + `libfmodex.so` with an emulated Java layer (FalsoJNI).
You need your own copy of the game APK; nothing from the game is distributed here.

> Status: menus and hunting (3D maps) run on real hardware. See [`RELEASE.md`](RELEASE.md) and
> `port_progress.md`.

## Install

1. Install `kubridge.skprx` and `libshacccg.suprx` (ShaRKBR33D).
2. Install `carnivoresdinosaurhunter.vpk`.
3. Create `ux0:data/carnivoresdinosaurhunter/` and copy into it:
   - the original APK, named `CarnivoresDinosaurHunter.apk` (`main.apk` or `game.apk` also work)
   - `libdinHunter.so` and `libfmodex.so` from the APK's `lib/armeabi/` folder
4. Launch. The first boot loads for about 30 seconds on a static screen (on Android a splash
   screen covers it). Logs go to `ux0:data/carnivoresdinosaurhunter/logs/`.

Content packs (optional): on Android they are separate apps. Copy their APKs to the same folder:
- `CarnivoresBundleOne.apk` (`com.tatem.dinhunter.bundle.one`, areas 3-4 + sniper rifle) as `bundle1.apk`
- `CarnivoresBundleTwo.apk` (`com.tatem.dinhunter.bundle.two`, area 6 + double-barrel shotgun +
  crossbow) as `bundle2.apk`

A pack is unlocked only when its APK is present; without it, its maps and weapons stay locked.

## Controls

| Vita | Action |
|---|---|
| Touch screen | Original touch controls |
| Left stick | Move |
| Right stick | Look |
| R / Cross | Fire |
| L | Alternative fire |
| Square | Switch weapon |
| Triangle | Binoculars |
| D-pad up | Call |
| Select / D-pad down | Map |
| Start / Circle | Pause / back |

The buttons press the game's own HUD controls, so they only act while that control is on screen
(in menus, use the touch screen).

While hunting, the touch HUD buttons (move stick, fire, alternative fire, weapon, binoculars, call,
map, pause and the photo mode buttons) are drawn at 1% opacity, since the physical controls replace
them. They still respond to touch where they normally are. The compass and the menus keep their
normal look. Change it with `hud_opacity` (100 = original look).

## Options

`ux0:data/carnivoresdinosaurhunter/config.txt` (rewritten on every boot, so new keys show up):

| Key | Default | Meaning |
|---|---|---|
| `look_sensitivity` | `100` | Right stick camera speed in percent (10–400). Frame-rate independent; stacks with the in-game sensitivity slider |
| `invert_look_y` | `0` | Invert right stick vertical axis |
| `msaa` | `0` | Anti-aliasing: 0 off (fastest), 1 2x, 2 4x |
| `engine_log` | `0` | Write the game's own debug messages to the log (slow: several lines per asset while loading) |
| `hud_opacity` | `1` | Opacity of the touch HUD buttons while hunting, in percent (0–100; 100 = original). The compass is not affected |
| `vfp_float` | `1` | Run the game's software floating point on the Vita FPU (big speedup; 0 only to rule it out if something looks wrong) |

## Building

Requires VitaSDK. vitaGL is vendored in `vendor/vitaGL` and built automatically with
`SOFTFP_ABI=1 NO_SPLASHSCREEN=1 NO_DEBUG=1 HAVE_SHADER_CACHE=1`. Build and deploy with
psvita-port-toolkit (`psvita-toolkit build`, `psvita-toolkit deploy --vpk`), or plain CMake:

```sh
mkdir -p build && cd build && cmake .. -DCMAKE_POLICY_VERSION_MINIMUM=3.5 && make
```

The default build type is Release (the log only keeps errors). Configure with
`-DCMAKE_BUILD_TYPE=Debug` (`psvita-toolkit build --preset debug`) to get the full log back: in
Debug the game's own messages are always logged (whatever `engine_log` says), and every libzip
`zip_open()` is logged with its error code.

Two vitaGL gotchas this port depends on:

- The prebuilt `libvitaGL.a` from VitaSDK is not built with `SOFTFP_ABI=1`, and this loader is
  softfp: with it the game runs at 60 FPS on a permanently black screen.
- Do **not** enable `DRAW_SPEEDHACK` (1 or 2): it hands the large terrain vertex arrays straight
  from game memory to the GPU and the console GPU-crashes as soon as a hunt starts.

And one engine gotcha: the game's built-in libzip was compiled for Android and reads a bionic
`FILE` field to check `ferror()`, but here every `FILE*` comes from SceLibc. `source/patch.c`
patches those five checks. Without that patch the APK stops opening once a saved game exists
(the engine never closes its save file), which gives a black screen, and the content packs never
open.

## Credits

Based on [soloader-boilerplate](https://github.com/v-atamanenko/soloader-boilerplate) (TheFloW, Rinnegatamante,
Volodymyr Atamanenko) and FalsoJNI. vitaGL by Rinnegatamante, kubridge by bythos.
Input mapping, soft-float VFP hooks and vitaGL setup shared with the Carnivores: Ice Age Vita port
(same Tatem engine).

## License

The loader is MIT-licensed (see [LICENSE](LICENSE)). The game, its assets and its `.so` files remain
the property of Tatem Games.
