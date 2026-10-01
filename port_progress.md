# Registro de Progreso — Carnivores Dinosaur Hunter (PS Vita)

**Estado:** 🚀 Implementación completa y binarios empaquetados. Listo para pruebas en hardware real.  
**Última actualización:** 2026-09-30  
**TITLEID:** `PSVCDH001`  
**Paquete original:** `com.tatem.dinhunter` (`CarnivoresDinosaurHunter.apk`)  

---

## Índice de Fases

1. [Fase 1: Configuración y Preparación](#fase-1-configuración-y-preparación-completada)
2. [Fase 2: Decompilación y Análisis Estático](#fase-2-decompilación-y-análisis-estático-completada)
3. [Fase 3: Arquitectura e Ingeniería Inversa del Motor](#fase-3-arquitectura-e-ingeniería-inversa-del-motor-completada)
4. [Fase 4: Bootstrap del Loader y Resolución de Símbolos](#fase-4-bootstrap-del-loader-y-resolución-de-símbolos-completada)
5. [Fase 5: FalsoJNI y Puente con Java](#fase-5-falsojni-y-puente-con-java-completada)
6. [Fase 6: Pipeline Gráfico OpenGL ES 1.1](#fase-6-pipeline-gráfico-opengl-es-11-completada)
7. [Fase 7: Subsistema de Audio Nativo FMOD + SceAudioOut](#fase-7-subsistema-de-audio-nativo-fmod--sceaudioout-completada)
8. [Fase 8: Input Táctil y Mapeo de Botones Físicos](#fase-8-input-táctil-y-mapeo-de-botones-físicos-completada)
9. [Fase 9: Sistema de Logs y Diagnóstico de Crashes](#fase-9-sistema-de-logs-y-diagnóstico-de-crashes-completada)
10. [Fase 10: Recursos de LiveArea y Empaquetado VPK](#fase-10-recursos-de-livearea-y-empaquetado-vpk-completada)
11. [Fase 11: Pruebas en Hardware Real](#fase-11-pruebas-en-hardware-real-pendiente)

---

## Fase 1: Configuración y Preparación (Completada)
- [x] Repositorio inicializado con estructura limpia y `.gitignore` estricto anti-DMCA.
- [x] Extracción del APK `CarnivoresDinosaurHunter.apk`.
- [x] Detección de binarios en arquitectura `armeabi` (ARMv6 soft-float, 100% ejecutable en el Cortex-A9 de PS Vita).
- [x] Identificación de librerías nativas: `libdinHunter.so` (490 KB) y `libfmodex.so` (888 KB).

---

## Fase 2: Decompilación y Análisis Estático (Completada)
- [x] Decompilación de código Java completo mediante `jadx` (`decompiled/apk_jadx/`).
- [x] Decompilación de `libdinHunter.so` y `libfmodex.so` mediante `ghidra` a pseudo-C (`decompiled/libdinHunter_armeabi/ghidra/`).
- [x] Auditoría de símbolos con `arm-vita-eabi-readelf` y `objdump`:
  - 23 funciones JNI exportadas con convención `Java_com_tatem_dinhunter_*`.
  - 2 funciones JNI exportadas en `libfmodex.so` (`Java_org_fmod_FMODAudioDevice_*`).
  - No requiere `RegisterNatives` manual, todo vinculado por tabla de exports.

---

## Fase 3: Arquitectura e Ingeniería Inversa del Motor (Completada)
- **Dependencias entre módulos:** `libdinHunter.so` posee una entrada `DT_NEEDED` apuntando a `libfmodex.so`.
- **Carga de Assets:** El motor incluye internamente una copia estática de `libzip`. Abre el archivo APK directamente llamando a `zip_open(apk_path, ...)` y ubica los recursos (`.3dn`, `.tga`, `.wav`, etc.) con el flag `ZIP_FL_NODIR` (`zip_name_locate(..., 2)`). Esto elimina la necesidad de descomprimir assets o alterar nombres de rutas en tiempo de ejecución.
- **Ciclo de Vida Nativo:**
  1. `JNI_OnLoad` en ambos módulos.
  2. `nativeApplicationDidFinishLaunching` inicializa el motor Tatem.
  3. `setEnvironment(data_path, apk_path)` establece los directorios de trabajo y búsqueda.
  4. `createFramebuffer(width, height)` configura los buffers internos de renderizado.
  5. `nativeResize(width, height)` adapta la proyección de la cámara a la resolución de pantalla.
  6. En cada frame: `layoutSubviews(&jni, NULL)` procesa la simulación de juego y dibuja la escena.
- **DLCs y Compras In-App:**
  - El APK original consulta la tienda de Google Play para paquetes de dinosaurios (`com.tatem.dinhunter.bundle.one` y `com.tatem.dinhunter.bundle.two`).
  - Implementado auto-desbloqueo en el puente JNI para otorgar todo el contenido completo al jugador de forma nativa y sin conexión.

---

## Fase 4: Bootstrap del Loader y Resolución de Símbolos (Completada)
- [x] Offsets de carga de memoria mapeados de forma segura para evitar solapamientos:
  - `LOAD_ADDRESS_FMOD` = `0x98000000`
  - `LOAD_ADDRESS_GAME` = `0x98400000`
- [x] Mapeo de librerías en `source/utils/init.c`:
  - `libfmodex.so` cargado y reubicado primero.
  - `libdinHunter.so` cargado y enlazado contra los símbolos exportados de `libfmodex.so` usando `so_resolve_link`.
- [x] Auditoría integral de dependencias dinámicas:
  - Creado script de comprobación automática de símbolos indefinidos contra `source/dynlib.c`.
  - Implementadas todas las funciones requeridas de POSIX, libc, libm, libz, pthreads y rutinas AEABI (`__aeabi_d2f`, `__aeabi_d2iz`, `__aeabi_dcmpeq`, `__aeabi_dcmpge`, `__aeabi_dcmple`, `__aeabi_dsub`, `__aeabi_f2d`, `__dso_handle`, `chown`, `inet_addr`, `pthread_attr_setschedpolicy`, `clearerr`, `umask`).
  - **Resultado:** 0 símbolos faltantes / 0 unresolved symbols.

---

## Fase 5: FalsoJNI y Puente con Java (Completada)
- [x] Copiado y configurado el runtime de `FalsoJNI` en `lib/falso_jni/`.
- [x] Corrección crítica en `lib/falso_jni/FalsoJNI.c`:
  - `GetDirectBufferAddress` corregido para devolver el puntero directo al búfer (requerido por el mezclador PCM de FMOD).
- [x] Implementación exhaustiva de métodos y campos en `source/java.c`:
  - Mapeo por ID de métodos (`nameToMethodId`): `openMoreGames`, `showGallery`, `onLoadingCompleted`, `putFrameOnPhoto`, `setTutorialFile`, `publishFeed`, `unlockAchievement`, `requestPurchase`, etc.
  - Mapeo de campos estáticos y de instancia (`WINDOW_SERVICE`, `SDK_INT`, `purchaseManager`).
  - Stubs limpios para integración con redes sociales (Facebook/SocialUtils) reportando éxito inmediato para evitar bloqueos del motor.

---

## Fase 6: Pipeline Gráfico OpenGL ES 1.1 (Completada)
- [x] Inicialización de contexto `vitaGL` en modo fixed-function GLES 1.1 a 960x544.
- [x] Configuración de búferes de profundidad (depth) y plantilla (stencil) necesarios para las sombras y modelos 3D de Carnivores.
- [x] Eliminado conflicto de duplicidad de símbolos EGL entre el boilerplate y `vitaGL`.
- [x] Bucle principal optimizado con `gl_swap()` síncrono a 60 FPS con v-sync.

---

## Fase 7: Subsistema de Audio Nativo FMOD + SceAudioOut (Completada)
- [x] Implementación de `source/reimpl/audio.c` y `source/reimpl/audio.h`.
- [x] Hilo nativo de audio dedicado en prioridad `0x10000100` con `SCE_KERNEL_HIGHEST_PRIORITY_USER + 1`.
- [x] Apertura de puerto BGM con `sceAudioOutOpenPort(SCE_AUDIO_OUT_PORT_TYPE_BGM, 4096, 24000, SCE_AUDIO_OUT_MODE_STEREO)`.
- [x] Bucle de procesamiento de audio:
  - Invoca `Java_org_fmod_FMODAudioDevice_fmodProcess` pasando buffers directos.
  - Salida directa de muestras PCM de 16 bits estéreo a los altavoces / auriculares de la consola.
- [x] Limpieza y terminación segura del hilo con `audio_stop()`.

---

## Fase 8: Input Táctil y Mapeo de Botones Físicos (Completada)
- [x] Muestreo del panel táctil frontal (`SCE_TOUCH_PORT_FRONT`) con `sceTouchSetSamplingState` activado.
- [x] Prevención de corrupción de memoria del motor:
  - Seguimiento con 5 ranuras virtuales (`touch_slots`), mapeando el `id` arbitrario de hardware de Vita a índices contiguos `[0..4]` (según directriz `input_handling.md`).
- [x] Mapeo de controles físicos mediante `sceCtrlPeekBufferPositive`:
  - **Botón Círculo / Start:** Invoca `Java_com_tatem_dinhunter_DinHunterRenderer_nativeOnBackPressed` (volver a menú / pausar).
  - **Botón Cruz / Gatillo R:** Invoca la función nativa del motor de disparo `_Z11Weapon_Firev` (`Weapon_Fire()`).

---

## Fase 9: Sistema de Logs y Diagnóstico de Crashes (Completada)
- [x] Estandarización de logs según directrices del toolkit:
  - Archivos rotativos incrementales `ux0:data/carnivoresdinosaurhunter/logs/carnivoresdinosaurhunter_NNN.log` utilizando `next.idx`.
  - Buffer intermedio en RAM (`64 KB`) para no degradar el rendimiento con accesos constantes a tarjeta de memoria.
  - Flush periódico (cada 2 segundos) y flush forzado en el gestor de excepciones de Vita.
  - Trazas condicionales preparadas bajo macro `PORT_TRACE`.

---

## Fase 10: Recursos de LiveArea y Empaquetado VPK (Completada)
- [x] Limpieza completa de archivos temporales y metadatos de macOS (`._*`).
- [x] Verificación de todas las imágenes de LiveArea en `extras/livearea/contents/`:
  - `bg0.png`: 840x500 (PNG indexado a 8-bit colormap).
  - `icon0.png`: 128x128 (PNG indexado a 8-bit colormap).
  - `pic0.png`: 960x544 (PNG indexado a 8-bit colormap).
  - `startup.png`: 280x158 (PNG indexado a 8-bit colormap).
  - `template.xml`: Sintaxis válida con TITLEID `PSVCDH001`.
- [x] Configuración de compilación con soporte para rutas con espacios y política CMake 3.5.
- [x] Generación exitosa de binarios:
  - `build/eboot.bin` (562 KB, ejecutable firmado para PS Vita).
  - `build/carnivoresdinosaurhunter.vpk` (643 KB, paquete instalable completo).

---

## Fase 11: Pruebas en Hardware Real (Pendiente)

El port está 100% implementado, enlazado y empaquetado. El único paso pendiente es la ejecución en una consola física PS Vita.

### Requisitos de instalación para el usuario:
1. Instalar `carnivoresdinosaurhunter.vpk` en la PS Vita (mediante VitaShell).
2. Crear la carpeta en la tarjeta de memoria:
   ```
   ux0:data/carnivoresdinosaurhunter/
   ```
3. Copiar a dicha carpeta los siguientes archivos:
   - `CarnivoresDinosaurHunter.apk` (APK original)
   - `libdinHunter.so` (extraído del APK en `lib/armeabi/`)
   - `libfmodex.so` (extraído del APK en `lib/armeabi/`)
4. Iniciar el juego desde la burbuja de LiveArea.
5. En caso de incidencia, consultar el log generado en:
   ```
   ux0:data/carnivoresdinosaurhunter/logs/carnivoresdinosaurhunter_001.log
   ```

---

## Bug #1 — APK no abre (libzip) → texturas/fuentes/sonidos "not found" + Data abort (log 002)

- **Síntoma (log `carnivoresdinosaurhunter_002.log`):** `fopen(<apk>)` OK, pero siempre
  `Failed to open archive ...CarnivoresDinosaurHunter.apk`. En cascada: todas las texturas/fuentes/
  modelos "not found" y `Sounds_AddSound("menumusic_cmpr")` → `FMOD error 'Unsupported file or audio format'`.
- **El error de FMOD NO es la causa:** `Sounds_AddSound` llama `FMOD_System_CreateSound` con
  `FMOD_OPENMEMORY` (flags `0x8200a48`) sobre el buffer de `Files_OpenFileAltType`, que quedó vacío
  porque el archivo no se pudo leer del APK. FMOD rechaza el buffer vacío con ERR_FORMAT y el juego lo
  maneja (retorna -1).
- **Dump:** Data abort en `SceLibKernel seg1+0x56` (R2=0xffffffff); stack con `zip_open+0x674` y
  `_zip_file_get_offset` (LR en `__muldf3` es valor viejo).
- **Causa real:** con `USE_SCELIBC_IO=ON`, `fopen`/`fread`/`fclose` van a SceLibc, pero `fseeko`,
  `ftello` y `clearerr` estaban mapeados a **newlib** → recibían un `FILE*` de SceLibc. `zip_open`
  hace `fseeko(fp,0,SEEK_END); ftello(fp)` (pseudo-C `out_ghidra.c:53149`) y `_zip_find_central_dir`
  usa `clearerr`/`fseeko`/`ftello` → tamaño basura → falla abrir el zip, y newlib escribe/bloquea sobre
  campos ajenos → crash.
- **Fix:** `fseeko_soloader`/`ftello_soloader`/`clearerr_soloader` en `source/reimpl/io.c` (usan
  `sceLibcBridge_fseek/ftell`; bionic armeabi `off_t` = long 32-bit, ABI compatible). Enrutados en
  `source/dynlib.c`.
- **Estado:** compilado, pendiente de prueba en consola.
- **Resultado en consola (log 003):** ✅ el APK abre, todas las texturas/fuentes/modelos cargan, hay sonido.

---

## Bug #2 — Pantalla negra / 0 FPS tras la carga (log 003) — EN DIAGNÓSTICO

- **Síntoma:** carga completa (`Game_Init` dentro del primer `layoutSubviews`, 5.7 s → 28.9 s,
  termina con `onLoadingCompleted`/`showFeintButton`), después no hay más log, pantalla negra, 0 FPS.
- **Descartado (estático):** orden `setEnvironment`/`createFramebuffer`/`nativeResize` correcto
  (`real_width/real_height` = 960x544); todos los imports GL van directo a vitaGL; `gettimeofday` es
  ABI-compatible (`struct timeval` de newlib = 8 bytes, igual que bionic); `Process()`/`Render()` no
  tienen esperas propias.
- **Instrumentación agregada (`source/main.c`):** tiempos de `layoutSubviews`/`gl_swap` de los primeros
  5 frames, FPS cada 5 s y un watchdog (`frame_watchdog`, CPU 2): si el loop no avanza 10 s después del
  primer frame, loguea el estado del hilo principal (`status`/`waitType`/`waitId`) y fuerza un Data
  abort para que el `.psp2dmp` capture el PC del hilo principal.
- **Aparte (no bloqueante):** muchos `FMOD error 'An invalid parameter...'` en `Sounds_AddSound` al
  cargar sonidos de dinosaurios — revisar después.

### Bug #2 — seguimiento (log 004/005)

- **Log 004:** el loop corre a 60 FPS (`watchdog: frames=110 fps=60.0`) → no es un cuelgue, es un
  problema de *qué* se ve en pantalla.
- **Cambios de otra IA (sin confirmar, no resolvieron):** MSAA 4X→NONE + `vglUseCachedMem` en
  `glutil.c`, ignorar `glEnable(GL_CULL_FACE)`, llamar `nativeSetBundlesPaths`, clear color forzado a
  azul marino. Se dejan por ahora pero ninguno está validado como causa.
- **Log 005:** solo tiene líneas `error` hasta 4.5 s (sin `info`, sin watchdog) — no sirve para
  diagnosticar; hay que capturar uno nuevo con la corrida completa.
- **Descartado estáticamente:** `Render_ApplyBrightness` (brightness por defecto 2.0 → aclara, no
  oscurece); `GUI_RenderFade` solo dibuja si `gui_fade_time > 0` (se loguea para confirmar).
- **Instrumentación nueva:** `diag_frame()` en `main.c` (frames 1, 2, 30, 120 y cada 600): `game_stage`,
  draws/binds por frame, `glGetError`, globals de render (`brightness`, fade, `render_sx/sy`,
  `real_width/height`...), `glReadPixels` en 5 puntos antes del swap, y un **cuadrado rojo 48x48**
  dibujado por el loader en una esquina.
- **Log 006:** idéntico a 005 (solo `error`). Causa: `build/CMakeCache.txt` tenía
  `CMAKE_BUILD_TYPE` vacío → sin `-DDEBUG_SOLOADER` → `l_info/l_warn/l_success` compilados vacíos
  (incluido el diagnóstico y el watchdog). En consola: pantalla negra, **tampoco** se vio el cuadrado rojo.
- **Fix de tooling:** `CMakeLists.txt` ahora usa `Debug` por defecto. Líneas `diag` pasadas a
  `l_error`. Nueva prueba de arranque: 2 s de verde sólido justo después de `gl_init()`, antes de que
  el juego toque GL (`diag: boot green test done`).
- **Log 007 (build Debug, con diagnóstico):** la prueba verde de arranque hizo 120 swaps en ~2 s
  (vsync OK) y **no se vio**. Motor sano: `stage=0`, 4-5 `glDrawArrays`/frame, `glerr=0x0`,
  `brightness=2.0`, fade termina (`fade_k=0`), `render=480x320`, `real=960x544`. `glReadPixels` =
  `00000000` en todos los puntos, alfa incluido. → vitaGL "presenta" pero nada llega al display.
- **Causa raíz:** la `libvitaGL.a` del sysroot de vitasdk no está compilada con `SOFTFP_ABI=1` (no
  contiene `sceGxmSetViewport_sfp`). El loader compila con `-mfloat-abi=softfp` → el viewport de GXM
  recibe floats en registros equivocados → todo (incluidos los clears, que vitaGL dibuja como quads) cae
  fuera de pantalla. Mismo síntoma y misma causa que Zenonia4 Fase 124 / Zenonia3.
- **Fix:** vitaGL vendorizada en `vendor/vitaGL` (copia del árbol probado de Zenonia4: upstream
  cd3791e + mods + `#if 0` de system_app en `gxm.c`), compilada vía `ExternalProject` con
  `SOFTFP_ABI=1 NO_DEBUG=1 HAVE_SHADER_CACHE=1 NO_SPLASHSCREEN=1 HAVE_GLSL_UBOS=1 SAMPLERS_SPEEDHACK=1
  DRAW_SPEEDHACK=2`. Verificado: `sceGxmSetViewport_sfp` presente en el ELF final.
- **Estado:** compilado, pendiente de prueba en consola.
- **Resultado en consola (log 008/009):** ✅ **se ve.** Verde de arranque visible, menú a 60 FPS con
  píxeles reales (`392f23ff`...), `stage` 0 → 1 → 5. Bug #2 cerrado: causa = libvitaGL del SDK sin
  `SOFTFP_ABI`.

---

## Bug #3 — GPUCRASH al entrar al mapa (log 009 + `...-GPUCRASH.psp2dmp`)

- **Síntoma:** tras `Map loading started` (stage 5) el loop cae a 0-3 FPS y la GPU crashea. PC en
  `SceGpuEs4User` (driver), sin frames del juego en el backtrace.
- **Causa:** la vitaGL vendorizada se compilaba con los flags de Zenonia4, incluido `DRAW_SPEEDHACK=2`
  (`-DSAFER_DRAW_SPEEDHACK`). En `ffp.c`/`custom_shaders.c` eso hace que los arrays de vértices mayores
  que `SAFE_DRAW_SIZE_THRESHOLD` se pasen **directo desde la memoria del juego a la GPU** sin copiarlos a
  memoria mapeada. El menú dibuja arrays chicos (se copian, funciona); el terreno 3D
  (`glDrawElements` de `Terrain_Render`, arrays en `.bss`/heap del `.so`) supera el umbral → la GPU lee
  memoria no mapeada → GPUCRASH. El port hermano Carnivores-Ice-Age-vita (mismo motor, mismo árbol
  vitaGL) ya documentaba "NO usar DRAW_SPEEDHACK".
- **Fix:** flags de vitaGL = `SOFTFP_ABI=1 NO_SPLASHSCREEN=1 NO_DEBUG=1 HAVE_SHADER_CACHE=1` (verificado
  con `make -n -B`: sin `SAFER_DRAW_SPEEDHACK` ni `SAMPLERS_SPEEDHACK`). CMake pasa a `add_custom_target`
  + `build_vitagl.sh` (hace `make clean` cuando los flags cambian; con ExternalProject quedaban objetos
  viejos). `gl_init` igual a Ice Age: `vglInitExtended(0, 960, 544, 6 MB, msaa)` sin `vglUseCachedMem`.
- **Resultado en consola (build 13:09):** ✅ entra al mapa y se juega sin GPUCRASH (reporte del usuario,
  2026-09-30). Mismo build con softfloat VFP + `engine_log 0` + Release: sin regresiones visibles. FPS
  en 3D sin medir todavía.

## Limpieza + rendimiento (2026-09-30)

- **Quitado todo el diagnóstico:** `diag_frame`, cuadrado rojo, prueba verde, watchdog, tiempos por
  frame, contadores de draws, clear color azul forzado. **Revertido** el hack de `glEnable(GL_CULL_FACE)`
  de la otra IA (Ice Age usa el culling del motor tal cual y se ve bien).
- **Build Release por defecto** (sin `DEBUG_SOLOADER`: el log solo guarda errores). Para depurar:
  `-DCMAKE_BUILD_TYPE=Debug`.
- **`engine_log 0`** (por defecto, `config.txt`): `__android_log_*` sale antes de formatear (salvo
  FATAL). El motor loguea varias líneas por asset durante la carga y marca progreso como ERROR, lo que
  forzaba flushes a la tarjeta.
- **Soft-float → VFP** (`source/reimpl/softfloat.c`, `vfp_float 1`): portado de Ice Age. El `.so` es
  armeabi v5TE; su libgcc ieee754 está en 0x6bb00..0x6cc98. 32 entradas enganchadas con trampolín ARM de
  8 bytes. Verificado con objdump sobre `libdinHunter.so`: ninguna entrada a <8 bytes de otra, y los
  únicos saltos a mitad de una entrada salen de `__gesf2/__lesf2/__gedf2/__ledf2` (también enganchados).
  0 saltos problemáticos en todo el `.so`. `softfloat.c` compila con `-fno-fast-math`.
- **`msaa 0`** por defecto (1 = 2x, 2 = 4x).
- `config.txt` en `ux0:data/carnivoresdinosaurhunter/`: `msaa N`, `engine_log 0/1`, `vfp_float 0/1`.

## Controles físicos (2026-09-30) — mismo mapeo que Carnivores-Ice-Age-vita

- `source/input.c` portado de Ice Age (mismo motor Tatem). Cambian solo los nombres JNI
  (`com_tatem_dinhunter_DinHunterGLSurface_*` / `DinHunterAndroid_nativeOnBackPressed`).
- Re-verificado contra `libdinHunter.so`: todos los símbolos existen (`gui_controls` 0xc000 = 128 ×
  0x180; `gui_touched_*` de 16 slots; `game_fire/alternative_fire/weapon/binoculars/call/map/
  movement_controller`). Offsets de `GUI_PointInControl` iguales (+0x0C/+0x10 x/y, +0x1C/+0x20 w/h,
  +0x24 flags, +0x2C scale, +0x33 visible). `GUI_GetBackgroundMovements` idéntica al pseudo-C de Ice Age
  → el hook del stick derecho aplica 1:1. `GUI_RecalcTouchLocation` usa `v_sy - y*scaleY`, equivalente a
  `(real_height - y)*scaleY` con v_sy=320/real=544.
- Mapeo: stick izq. mover (toque sintético sobre `game_movement_controller`), stick der. cámara (hook),
  R/✕ disparar, L disparo alternativo, □ arma, △ binoculares, ↑ llamar, Select/↓ mapa, Start/◯ atrás.
- Quitado de `main.c`: el manejo de touch propio y el hack `Weapon_Fire()` con ✕/R.
- Nuevas opciones en `config.txt`: `look_sensitivity` (10–400), `invert_look_y`. `config.txt` se
  reescribe en cada arranque.
- **Estado:** compilado (13:35), pendiente de prueba en consola.
- Docs: `README.md` (formato de los otros ports) y `RELEASE.md` (v1.0.0).

## Build con psvita-toolkit (2026-09-30)

- **Error:** `psvita-toolkit build --preset release --clean` fallaba en `vitaGL_lib` con `No rule to
  make target 'clean'` / `no makefile found`. El toolkit copia el código a un directorio temporal
  respetando `.gitignore`, y la regla `Makefile` (pensada para el Makefile que genera CMake) también
  ignoraba `vendor/vitaGL/Makefile`. **Tampoco había entrado en el commit a8a408f.**
- **Fix:** en `.gitignore`, `Makefile` → `/Makefile` y `cmake_install.cmake` → `/cmake_install.cmake`.
  Era el único archivo del árbol afectado (`git ls-files --others --ignored`).
- **Build OK** con el toolkit (Release, vitaGL `SOFTFP_ABI=1 NO_SPLASHSCREEN=1 NO_DEBUG=1
  HAVE_SHADER_CACHE=1`). Único warning propio: `mktemp` deprecado en `dynlib.c` (inofensivo).
- **Ojo:** el toolkit reportó el VPK como `build/._carnivoresdinosaurhunter.vpk` (AppleDouble de 4 KB
  del disco externo). Correr `psvita-toolkit clean-junk` antes de desplegar.

---

## Bug #4 — Data abort en FMOD al cargar un sonido que no existe (log 010 + dump 1790791802)

- **Síntoma:** a los ~231 s de juego, Data abort en `memcpy` (PC `SceLibKernel seg1+0x56`,
  `R2=0xffffffff`); LR `0x980afeec` = `libfmodex.so+0xafeec` (lectura de un archivo `FMOD_OPENMEMORY`),
  copiando 3 bytes desde `0xda981568` (basura).
- **Causa:** `Sounds_AddSound()` no chequea el retorno de `Files_OpenFileAltType(..., "ogg")`; si el
  archivo no está en ningún zip, `Files_OpenFileOfType` no toca el `_FileHandler` y FMOD recibe basura
  de la pila como datos (+0x00) / tamaño (+0x9c).
- **Fix:** `Files_OpenFileAltType` enganchada en `source/patch.c` (1:1 con el pseudo-C) y limpia el
  handler si el archivo no aparece → FMOD falla con INVALID_PARAM, el slot queda -1.
- **Estado:** compilado (16:57, incluido en las builds 17:12 y 17:20). Sin crash en la prueba del usuario, falta una sesión larga para confirmarlo.

## Bug #5 — Pantalla negra al arrancar con partida guardada (log 011)

- **Síntoma:** build Debug de las 16:49, pantalla negra. vitaGL OK (`sceGxmSetViewport_sfp` presente,
  no es el Bug #2). El log entra al main loop y hace ~224 `stat/fopen/fclose` del APK en 650 ms (uno por
  asset), después nada más.
- **Causa:** ahora existe `.dinhunter/CarnivoresData.dt`. `EAGLView::LoadGameData()` lo abre con
  `fopen` y **nunca lo cierra** (pseudo-C `out_ghidra.c:~40980`), así que el APK recibe el segundo FILE
  de SceLibc (`0x81710130` en vez de `0x81700010`). El libzip del `.so` trae `ferror()` de bionic
  inlineado: lee `fp->_flags` (u16 en +0x0C) `& 0x40` de un FILE de SceLibc. Con el 1º slot da 0 por
  casualidad; con el 2º da "error" → `_zip_find_central_dir` falla → `zip_open` falla → ningún asset
  carga. (Lo mismo explicaba en el log 009 los `Failed to open archive` al abrir el APK como pack 1
  mientras el APK principal ya estaba abierto.)
- **Fix:** `zip_ferror_patch()` en `source/patch.c`: los 5 `ldrh r3,[r3,#12]` (Thumb `0x899b`) de
  los chequeos de `ferror` → `movs r3,#0` (`0x2300`), verificando el opcode antes de escribir.
  Sitios: `0x6816c` `_zip_cdir_write`, `0x688f0` `_zip_dirent_write`, `0x69d64`/`0x69d8a`
  `_zip_readcdir`, `0x6a464` `_zip_find_central_dir` (todos los que Ghidra muestra como
  `_IO_read_base & 0x40`; objdump confirma que no hay más).
- **Build:** `psvita-toolkit build --preset release --clean` (16:57) + `clean-junk`. Verificado en el
  ELF: `sceGxmSetViewport_sfp` presente, flags vitaGL `SOFTFP_ABI=1 NO_SPLASHSCREEN=1 NO_DEBUG=1
  HAVE_SHADER_CACHE=1` (sin `DRAW_SPEEDHACK`).
- **Resultado en consola (build Debug 17:12):** ✅ arranca con partida guardada, carga assets y packs (reporte del usuario, 2026-09-30).

## Packs de contenido (2026-09-30)

- `CarnivoresBundleOne.apk` / `CarnivoresBundleTwo.apk` confirmados como packs de **Dinosaur Hunter**
  (manifest: `com.tatem.dinhunter.bundle.one` / `.two`, los IDs que compara
  `nativeSetProductPurchased`). One = area3 + area4 + sniper (227 archivos); Two = area6 + dbsgun +
  x_bow (134 archivos). No repiten ningún archivo del APK principal.
- `main.c` ya los carga si existen `ux0:data/carnivoresdinosaurhunter/bundle1.apk` / `bundle2.apk`.
  Dependen del fix de libzip del Bug #5 (antes, un segundo `zip_open` fallaba: `Failed to open archive`
  en el log 009).
- Docs: README (instalación), RELEASE.md (qué funciona, bugs #4/#5, problemas conocidos),
  PORTING_PLAN.md §7.
- **Resultado en consola (build Debug 17:12):** ✅ funciona (reporte del usuario, 2026-09-30): arranca
  con partida guardada y carga los packs.

## Build Debug con logs de verdad (2026-09-30)

- La build Release de las 16:57 no dejaba log útil: Release solo guarda `l_error`, y `config.txt`
  tenía `engine_log 0`.
- En Debug (`DEBUG_SOLOADER`), `reimpl/log.c` ahora loguea **siempre** los mensajes del motor, sin
  importar `engine_log`. `patch.c` registra `zip ferror patch: N/5 sites patched.` y, solo en Debug,
  cada `zip_open()` con el código de error de libzip (hook + `SO_CONTINUE`).
- Con esa build (17:12) el usuario confirmó que el Bug #5 está resuelto y que los packs funcionan.

## HUD táctil al 1 % (2026-09-30)

- Pedido: todos los botones virtuales al 1 % de opacidad, menos la brújula.
- `GUI_DrawControls` enganchada en `input.c`: el alfa (byte alto de `+0x28`) de los 11 controles del
  HUD de caza se escala a `hud_opacity` % antes de dibujar y se restaura después. El motor cambia esos
  colores mientras se juega, por eso se hace en cada frame. La brújula es el modelo `compas.3dn`, no un
  control, así que queda igual. Los menús no se tocan. Detalle en PORTING_PLAN.md §8.
- Nueva opción `hud_opacity` en `config.txt` (0–100, por defecto 1; 100 = desactivado).
- **Build:** Release 17:20 (`psvita-toolkit build --preset release --clean` + `clean-junk`).
  Verificado en el ELF: `GUI_DrawControls_hook`, parche de libzip, `sceGxmSetViewport_sfp`.
- **Estado:** pendiente de prueba en consola.

## ◯ llama a los animales (2026-09-30)

- Igual que Carnivores-Ice-Age-vita: ◯ se suma a ↑ sobre `game_call`, y "atrás"
  (`nativeOnBackPressed`) queda solo en Start. Reemplaza el "Start/◯ atrás" de la sección de
  controles físicos. README y RELEASE.md actualizados.
- **Estado:** compilado (Release), pendiente de prueba en consola.

## Trofeos Nativos PS Vita, Controles Remapeables y Limpieza de Facebook (2026-10-01)

- **Trofeos PS Vita:**
  - Implementado `source/utils/trophy.c` y `source/utils/trophy.h` usando `sceNpTrophy` (`SceNpTrophy_stub` + `SceSysmodule_stub`).
  - Hilo de trabajo asíncrono no-bloqueante (`trophies_unlocker`) con semáforo y cola circular para evitar stutters al desbloquear trofeos.
  - Conectado a `SocialUtils.unlockAchievement` en `source/java.c` (`trophy_unlock(achId + 1)`).
  - Manejo seguro de ausencia de `NoTrpDrm` o paquete TRP con degradación elegante (`trophies_available = 0`).
- **Controles Personalizados:**
  - Creador y parser de `ux0:data/carnivoresdinosaurhunter/controls.txt`.
  - Soporte bidireccional (`ACTION = BOTON1, BOTON2` y `BOTON = ACCION`).
  - Soporte para panel táctil trasero (`SCE_TOUCH_PORT_BACK`) con cuadrantes L2, R2, L3, R3.
  - Integración transparente con PhotoMode (`game_photomode_shot`, `game_photomode_zoom_in`).
- **Desactivación de Elementos de Facebook:**
  - `patch.c`: Hook a `GUI_SetControlVisible` y `GUI_SetControlActive` forzando visibilidad 0 en todos los controles sociales.
  - No-op a las funciones sociales del `.so`: `Facebook_Login`, `Facebook_Logout`, `Facebook_PublishFeed`, `Facebook_PublishTrophy`, etc.
  - `input.c`: Rutina `hide_social_controls()` en cada frame y antes de dibujar el HUD para reubicar controles sociales fuera de pantalla (`x = -9999`, `w = 0, h = 0`).
- **Estado:** Compilado exitosamente en `.vpk` y `eboot.bin`. Listo para pruebas en consola.

## Feedback de usuario: remapeo in-game, disparo, menús con botones (2026-10-01)

Feedback (tester): disparar es torpe (cuadrado para sacar el arma, triángulo para disparar), pide
remapear, quitar Facebook y poder usar los menús con botones.

- **Causa del disparo torpe:** la opción del juego `firing_method` vale 1 por defecto, y con 1 el
  motor oculta `game_fire` y dispara con `game_alternative_fire` (pseudo-C ~20676). El mapeo v1
  mandaba R/✕ a `game_fire`, siempre inactivo, así que solo disparaba L. Encima sacar el arma con
  `game_weapon` abre también la lista de armas.
- **Fix:** `FIRE` dispara con el control que esté visible, desenfunda con `Weapon_TakeWeapon()` si el
  arma está guardada y usa `Weapon_Fire()` con el método 0 (tocar pantalla). Acciones nuevas:
  `JUMP` (✕, `quick_touch`), `WEAPON` (desenfundar/enfundar), `NEXT_WEAPON`, `WEAPON_MENU`,
  `ZOOM_IN/OUT`. `controls.txt` pasa a `VERSION = 2` y uno viejo se reemplaza por los defaults.
- **Menú "PS Vita controls & camera"** (START+SELECT, o SELECT en menús): remapeo (✕ reemplazar,
  □ agregar, △ quitar), velocidad de cámara, invertir X/Y, intercambiar sticks, opacidad del HUD.
  Dibujado con las fuentes del motor desde un hook de `Font_Render()` (`source/overlay.c`).
- **Menús con botones:** cursor sobre los controles activos, ✕ pulsa, ←/→ sliders, ◯ atrás.
- **Facebook:** ya estaba deshabilitado; los hooks de `GUI_SetControlVisible/Active` usaban
  `SO_CONTINUE` (4 llamadas al kernel + flush por llamada, decenas de llamadas por frame). Ahora son
  reimplementaciones directas.
- **Build:** Release 11:32 (`psvita-toolkit build --preset release`), sin warnings en los archivos
  nuevos. En el ELF: `Font_Render_hook`, `vita_menu_draw`, `draw_overlay`,
  `GUI_SetControlVisible_hook`, `sceGxmSetViewport_sfp`.
- **Estado:** pendiente de prueba en consola. Lo más incierto sin hardware: posición/escala del
  texto del overlay (formato de color y flags de `Font_PrintText` deducidos del pseudo-C).
