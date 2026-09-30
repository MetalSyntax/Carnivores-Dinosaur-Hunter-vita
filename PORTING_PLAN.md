# Plan de Port — Carnivores Dinosaur Hunter (PS Vita)

> Generado por psvita-port-toolkit el 2026-09-24. Actualizado al 2026-09-30 con análisis e implementación completados.

## 0. Contexto

- **Juego:** Carnivores Dinosaur Hunter
- **Paquete Java:** `com.tatem.dinhunter`
- **APK original:** `CarnivoresDinosaurHunter.apk`
- **TITLEID asignado:** `PSVCDH001`
- **Motor / Arquitectura:** Motor propietario Tatem Games en C/C++ (GLES 1.1 + FMOD Ex + libzip interno para lectura de paquetes `.3dn`/`.tga`/`.wav` desde el APK).

## 1. Detección de Arquitectura y Gráficos

- **ABI(s):** armeabi (ARMv6 soft-float) -> Totalmente compatible con la CPU ARM Cortex-A9 de PS Vita.
- **Gráficos:** OpenGL ES 1.1 (pipeline de función fija: `glMatrixMode`, `glLoadIdentity`, `glVertexPointer`, `glTexCoordPointer`, `glDrawArrays`, etc.). Implementado con `vitaGL` en modo GLES 1.1 a 960x544.

## 2. Binarios .so Analizados

- `libfmodex.so` (888 KB): Sistema de sonido FMOD Ex. Cargado primero en dirección fija `0x98000000`.
- `libdinHunter.so` (490 KB): Motor de juego principal. Depende (`DT_NEEDED`) de `libfmodex.so`. Cargado en `0x98400000` y vinculado vía `so_resolve_link`.

## 3. Exports JNI Confirmados

### `libdinHunter.so`:
- `Java_com_tatem_dinhunter_DinHunterRenderer_nativeApplicationDidFinishLaunching`
- `Java_com_tatem_dinhunter_DinHunterRenderer_setEnvironment`
- `Java_com_tatem_dinhunter_DinHunterRenderer_createFramebuffer`
- `Java_com_tatem_dinhunter_DinHunterRenderer_nativeResize`
- `Java_com_tatem_dinhunter_DinHunterRenderer_layoutSubviews`
- `Java_com_tatem_dinhunter_DinHunterRenderer_nativeTouchesBegan`
- `Java_com_tatem_dinhunter_DinHunterRenderer_nativeTouchesMoved`
- `Java_com_tatem_dinhunter_DinHunterRenderer_nativeTouchesEnded`
- `Java_com_tatem_dinhunter_DinHunterRenderer_nativeTouchesCanceled`
- `Java_com_tatem_dinhunter_DinHunterRenderer_nativeOnBackPressed`
- `Java_com_tatem_dinhunter_DinHunterRenderer_nativeChangeFpsLimit`
- `Java_com_tatem_dinhunter_DinHunterRenderer_nativeSetPhoto`
- `Java_com_tatem_dinhunter_DinHunterRenderer_nativeApplicationDidReceiveMemoryWarning`
- `Java_com_tatem_dinhunter_DinHunterRenderer_nativeApplicationWillResignActive`
- `Java_com_tatem_dinhunter_DinHunterRenderer_nativeApplicationDidBecomeActive`
- `Java_com_tatem_dinhunter_DinHunterRenderer_nativeApplicationWillTerminate`
- `Java_com_tatem_dinhunter_DinHunterRenderer_deleteFramebuffer`
- `Java_com_tatem_dinhunter_utils_FacebookWrapper_nativeInit`
- `Java_com_tatem_dinhunter_utils_FacebookWrapper_nativeOnSessionOpened`
- `Java_com_tatem_dinhunter_utils_FacebookWrapper_nativeOnSessionClosed`
- `Java_com_tatem_dinhunter_utils_SocialUtils_nativeInit`
- `Java_com_tatem_dinhunter_utils_SocialUtils_nativeOnLogin`
- `Java_com_tatem_dinhunter_utils_SocialUtils_nativeOnLogout`

### `libfmodex.so`:
- `Java_org_fmod_FMODAudioDevice_fmodGetInfo`
- `Java_org_fmod_FMODAudioDevice_fmodProcess`

## 4. Checklist de Porting

- [x] Repo creado desde soloader-boilerplate, git init, .gitignore anti-DMCA.
- [x] APK decompilado (jadx) y .so decompilado(s) (Ghidra).
- [x] Análisis del motor real (ciclo de vida nativo, doble .so, FMOD Ex, libzip interno).
- [x] Bootstrap del loader: `so_file_load` para ambos `.so`, tabla de símbolos dinámicos completa (0 símbolos faltantes).
- [x] Tabla JNI (FalsoJNI): callbacks hacia Java (sonido, achievements, UI) y packs de contenido desde sus propios APKs (ver §7) y soporte de direct buffers para audio.
- [x] Gráficos: Inicialización de `vitaGL` en modo GLES 1.1, resolución 960x544, buffers de profundidad y stencil.
- [x] Input: Mapeo táctil frontal con tracking de ranuras virtuales (prevención de desbordamiento/corrupción de heap) y mapeo de botones físicos (Start/Círculo para retroceder, Cruz/R1 para disparo de arma `_Z11Weapon_Firev`).
- [x] Audio: Hilo de audio nativo dedicado (`sceAudioOutOpenPort(SCE_AUDIO_OUT_PORT_TYPE_BGM, ...)` a 24000 Hz estéreo) alimentado por FMOD.
- [x] Assets y Empaquetado: Lectura transparente del APK (`ux0:data/carnivoresdinosaurhunter/CarnivoresDinosaurHunter.apk`), assets de LiveArea indexados a 8-bit colormap, generación limpia de `eboot.bin` y `carnivoresdinosaurhunter.vpk`.
- [x] libzip interno sobre FILE de SceLibc: `fseeko/ftello/clearerr` por el bridge (Bug #1) y los 5 `ferror()` inlineados de bionic parcheados (Bug #5, ver §7).
- [x] Pruebas en hardware real: menú, carga de mapas y caza confirmados (logs 008/009 + build 13:09).
- [x] Confirmado en consola (2026-09-30, reporte del usuario): arranque con partida guardada (Bug #5) y packs de contenido.
- [x] HUD táctil al 1 % (`hud_opacity`), menos la brújula (ver §8).
- [ ] Sesión larga: confirmar que ya no crashea FMOD por un sonido faltante (Bug #4) y probar el HUD al 1 %.
- [ ] Medir FPS en 3D; investigar los `FMOD error 'An invalid parameter'` de `Sounds_AddSound` (logs 003/009).

## 5. Estándar de Logs en Consola

El log del juego vive en `<DATA_PATH>logs/carnivoresdinosaurhunter_NNN.log`, incremental de 001 a 999 con `next.idx` (administrado por `source/utils/logger.c`). Subida periódica y volcado forzado en crash/salida.

## 6. Ciclo de Vida del Motor en `main.c`

1. Inicialización de logger, almacenamiento y runtime de Vita (`scePowerSetArmClockFrequency(444)`, etc.).
2. Carga secuencial de librerías dinámicas: `libfmodex.so` en `0x98000000`, `libdinHunter.so` en `0x98400000`.
3. Inicialización del entorno FalsoJNI (`jni_init()`).
4. Inicialización gráfica con `gl_init()` y creación de framebuffer 960x544 (`createFramebuffer`).
5. Llamada a `nativeApplicationDidFinishLaunching(&jni, NULL)`.
6. Configuración de entorno de almacenamiento (`setEnvironment`).
7. Dimensionamiento de viewport (`nativeResize(960, 544)`).
8. Inicio del hilo de audio dedicado (`audio_start()`).
9. Bucle principal de renderizado (`layoutSubviews`) y muestreo de controles/táctil con swap de buffers (`gl_swap()`).
10. Packs de contenido: `nativeSetProductPurchased(id, JNI_TRUE, <ruta>)` solo si existe `bundle1.apk` / `bundle2.apk` en `DATA_PATH`; si no, `JNI_FALSE`. `nativeSetBundlesPaths` ya no se llama.

## 7. Packs de contenido y libzip

- En Android los packs son apps aparte: `com.tatem.dinhunter.bundle.one` (`CarnivoresBundleOne.apk`: area3, area4, rifle sniper; 227 archivos) y `com.tatem.dinhunter.bundle.two` (`CarnivoresBundleTwo.apk`: area6, escopeta doble `dbsgun`, ballesta `x_bow`; 134 archivos). Ningún archivo se repite con el APK principal, que solo trae previews de esas áreas.
- `nativeSetProductPurchased` (pseudo-C `out_ghidra.c:42499`) pone `packN_purchased = 1` y `bundleNPath = <ruta>`. `Files_OpenFileOfType` busca cada archivo en `apkPath`, después en `bundle1Path` y después en `bundle2Path` (`zip_name_locate` con `ZIP_FL_NODIR`, así que no importa el directorio dentro del zip), y cierra los packs después de cada lectura.
- Instalación: copiar a `ux0:data/carnivoresdinosaurhunter/` como `bundle1.apk` / `bundle2.apk`. Marcar un pack como comprado sin su APK hace que el motor ofrezca mapas y armas cuyos archivos no existen.
- libzip del `.so` compilado contra bionic: `ferror(fp)` inlineado = `*(u16 *)(fp + 0x0C) & 0x40`. Sobre un FILE de SceLibc el resultado depende del slot. `EAGLView::LoadGameData()` nunca hace `fclose` del save, así que con partida guardada el APK recibe otro slot y `zip_open` fallaba. `zip_ferror_patch()` en `source/patch.c` cambia los 5 `ldrh r3,[r3,#12]` (Thumb `0x899b`) por `movs r3,#0` en `0x6816c`, `0x688f0`, `0x69d64`, `0x69d8a` y `0x6a464`. Sin este parche tampoco abrían los packs: son un segundo y tercer `zip_open` simultáneos.

## 8. Opacidad del HUD táctil

- `GUI_DrawControls()` (ARM, `0x2d7c4`, pseudo-C `out_ghidra.c:15865`) dibuja cada control de `gui_controls[]` (registros de 0x180 bytes) con el color de `+0x28` (ARGB, alfa en el byte alto; el HUD usa `0x80ffffff`, lo setea `GUI_SetControlColor`). Ese color también pasa a `Font_PrintText` para el texto del control.
- `input.c` engancha `GUI_DrawControls` (`hook_addr` + `SO_CONTINUE`): antes de dibujar escala el alfa de los controles del HUD de caza a `hud_opacity` % (por defecto 1: `0x80` → 2), y después restaura el color original. Se hace en cada frame porque el motor reescribe algunos colores mientras se juega (la llamada se desvanece, línea ~20772).
- Controles: `game_movement_controller`, `game_fire`, `game_alternative_fire`, `game_weapon`, `game_binoculars`, `game_call`, `game_map`, `game_menu`, `game_photomode_shot/zoom_in/zoom_out` (creados en `Menu_Init`, ~22380-22726). Los menús no se tocan.
- La brújula no es un control GUI: es el modelo `compas.3dn` con `compas.tga` (~26164-26254), así que no le afecta. Tampoco a los sprites que se dibujan aparte del control (círculo y ondas de la llamada, barra de vida).
- `hud_opacity 100` desactiva el hook.
