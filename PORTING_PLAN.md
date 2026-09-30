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
- [x] Tabla JNI (FalsoJNI): callbacks hacia Java (sonido, achievements, UI, DLCs auto-desbloqueados) y soporte de direct buffers para audio.
- [x] Gráficos: Inicialización de `vitaGL` en modo GLES 1.1, resolución 960x544, buffers de profundidad y stencil.
- [x] Input: Mapeo táctil frontal con tracking de ranuras virtuales (prevención de desbordamiento/corrupción de heap) y mapeo de botones físicos (Start/Círculo para retroceder, Cruz/R1 para disparo de arma `_Z11Weapon_Firev`).
- [x] Audio: Hilo de audio nativo dedicado (`sceAudioOutOpenPort(SCE_AUDIO_OUT_PORT_TYPE_BGM, ...)` a 24000 Hz estéreo) alimentado por FMOD.
- [x] Assets y Empaquetado: Lectura transparente del APK (`ux0:data/carnivoresdinosaurhunter/CarnivoresDinosaurHunter.apk`), assets de LiveArea indexados a 8-bit colormap, generación limpia de `eboot.bin` y `carnivoresdinosaurhunter.vpk`.
- [ ] Pruebas en hardware real (instalación en consola física PS Vita, verificación de rendimiento y audio).

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
