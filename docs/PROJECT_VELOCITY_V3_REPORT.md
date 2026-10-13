# Project Velocity ARM64 0.14.0-velocity.3

Experimental modification, not an upstream release. Baseline: Velocity .2 commit
`26ded7ba8a9f5e2ed77e2ecef1d62f5ecb4176f4`. Package remains
`com.nfsu2.androidevolved`; Android 10/API 29+, ARM64, target API 35, versionCode 17.
Velocity .1/.2 releases, import flow and existing language resources are preserved.
No commercial executable, generated guest C, game data or signing key is published.
The original static generator's private output is compiled into the native runtime;
players still install the APK and import their complete legitimate game folder.

## Baseline and evidence

Inspected Gradle/CMake, guest_runtime.cpp, guest_memory.h, d3d9_bridge.cpp,
main.cpp, gpu_probe.cpp, velocity_profile.*, the Activity/controls/options/manifest,
and the pinned DXVK presenter, swapchain, pipeline manager, graphics/compute
compiler and HUD. Existing options include profiling, state cache, experimental
frame pacing, FPS cap, performance/latency queue depth and CPU texture conversion.
Existing .2 optimizations include region-only pitched DXT decoding, guarded guest
memory access and hash dispatch. They remain intact.

Confirmed code behavior, not confirmed Android bottlenecks:

* DXVK's VkPipelineCache started empty every process; the existing state cache
  did not restore the driver's opaque binary pipeline data.
* Disabled profiling scopes still called out-of-line constructors/destructors.
* Gameplay was fixed to one landscape direction; 180-degree display changes can
  occur without an orientation configuration change.
* Presentation forced the identity transform without checking supportedTransforms.
* Persistent returned acquisition errors could repeatedly recreate the swapchain.
* Native-window lifetime was not explicitly retained for each Vulkan surface.

Tester estimates: Adreno 610/Android 16/Vulkan 1.1 around 15–20 FPS; Mali-G615 MC6
(Dimensity 8300 Ultra) around 30 FPS in performance mode with stutters; Dimensity
8400+ around 60 FPS with dips, exact GPU/driver unconfirmed. These are reports,
not instrumented benchmarks. CPU/GPU saturation, pipeline compilation, driver
behavior, DXT uploads and synchronization remain possible causes. No physical
Android device or emulator is connected; no FPS gain or gameplay pass is claimed.

## Winlator comparison: actual inspected source

| Project | Inspected revision | Relevant distinction |
| --- | --- | --- |
| Winlator | b6b2259158cf38d06067c34430d840d56b46d220 | App submodule a030f552f452158a2db64fdb32b490fa19c0b48d exposes configurable graphics wrappers/drivers |
| Box64 | 0684e43871c642f218f20aa8df5816ca755e49aa | Dynamic translation with configurable execution presets; not used by Velocity |
| DXVK | 649c4bc0f0e303fcf9e21fad1c36e30ebce79b02 | Modern backend requires Vulkan 1.3; not a drop-in Vulkan 1.1 upgrade |
| DXVK Native | c8dc91fabd00cac11d697ccf07426e798393cd40 | Velocity's native-1.9.2b pinned source, also matching inspected misyltoad HEAD |
| Mesa | 19b5f13dcc50d55ef89bd7e68d46a2b0c73751b3 | Turnip is a Qualcomm path, not a driver for Mali/Xclipse/PowerVR |

Winlator executes the Windows game through Wine and its selected x86 translation
and graphics/driver stack. Velocity uses statically generated guest functions,
Windows API HLE and DXVK Native on Android Vulkan. Native code alone does not
prove faster execution. Inspected Winlator configuration code selects DXVK minor
1.10.3 or major 2.4.1 depending on driver/API, and exposes optional Turnip and
async variants. Current default driver paths include Vortek/Gladio. The user's
answer “latest” does not identify their actual driver, wrapper, Box64 preset or
resolution, so no Turnip/async/version assumption is made. No driver from another
APK is bundled. No modifications to these reference repositories were made.

## Implemented changes and rationale

### Driver pipeline cache and synchronization

`velocity_pipeline_cache.h`, `velocity_pipecache.*`, and `prepare.py` implement a
persistent Vulkan binary cache under the application's private cache directory.
Identity includes vendor/device, raw driver/API version and pipelineCacheUUID.
Before passing data to the driver, validate the Vulkan header, bounded size
(32 MiB), wrapper length/checksum and driver/API identity. Corrupt, missing or
incompatible files fall back to an empty cache; driver rejection retries empty.
Writes use a temporary file and rename. Shader Cache OFF disables persistence.

Graphics/compute compilers share a read lock, retaining parallel compilation;
background snapshots exclusively lock vkGetPipelineCacheData. A registry lock
protects cache lifetime. Saving runs from the existing background export on pause,
not each frame or the UI thread. This can reduce repeated driver compilation but
its effectiveness depends on the driver. Abrupt process death before pause can
lose the latest snapshot. Disk cleanup/capacity failures preserve execution.
Existing DXVK state cache remains separate. Graphics Pipeline Library is not
implemented by this pinned backend and is not falsely enabled.

`android_wsi.cpp` now retains a native-window reference until its Vulkan surface
is destroyed. Presenter transform selection checks supported capabilities:
IDENTITY when supported, otherwise INHERIT when supported. Devices requiring an
application-specific pre-rotation transform with neither capability currently
receive an explicit error; that path is a remaining compatibility limitation.
Returned image-acquisition failures have three recovery attempts, then an explicit
error instead of an unbounded recreate loop. This does not cure a driver that
never returns. Required queue waits, barriers, fences and semaphore ownership are
preserved; no uncontrolled increase in frames in flight is introduced.

### Profiling and runtime

`velocity_profile.*` inline the inactive scope guard; active timing stays opt-in.
Import lookup timing samples 1/256 lookups and D3D9 dispatch samples 1/64 calls:
these are estimates, not full HLE execution time. Existing runtime, draw, shader,
texture conversion/upload, fence, submit and present instrumentation is retained.
The bounded CSV captures 600 frames and now has 47 columns. Export adds selected
GPU/driver/API, format support and limits; probe JSON adds supported extensions,
optional features and hardware limits. Probe and actual selected-device metadata
are distinct. Guest committed bytes and process peak RSS are sampled on export,
not every frame. CPU times are process/thread measurements as labeled, not GPU
timestamps. GPU execution timing/utilization is not available in this build.

A reproducible synthetic x86_64 host measurement (GCC -O2, process CPU clock,
10,000,000 disabled scopes, three alternating runs) produced:

| Build | CPU times (ms) | Median (ms) |
| --- | --- | --- |
| .2 scope | 32.297, 32.123, 31.159 | 32.123 |
| .3 scope | 16.835, 17.190, 16.568 | 16.835 |

This confirms reduced inactive profiling overhead on this host only (~47.6%).
It is not an Android benchmark and does not imply a game FPS improvement.
Guest protections, x87 semantics, dispatch and original game logic are unchanged.

### HUD

The existing DXVK Vulkan HUD implements OFF/compact/detailed options; compact is
default. It draws inside the game, upper left with cutout offsets, without an
Android overlay window or permission. Compact shows actual selected GPU, measured
presentation-stream FPS and Vulkan version. Detailed adds last frametime, average
FPS over up to 600 intervals, process CPU ms/frame, existing pipeline counts,
background compiler activity and tracked DXVK memory. Presentation throughput is
not physical panel scanout rate. Compiler activity is not a GPU execution timer.
Stats reset on pause/resume; formatted strings/CPU sampling update twice per
second. OFF avoids constructing the HUD. The default touch pause button moves
right when the HUD is enabled; custom user layouts may intentionally overlap.

### Both landscape directions

`ScreenPolicy.java`, `GameActivity.java`, the manifest and controls expose saved
Automatic/normal/reverse landscape choices. Automatic uses USER_LANDSCAPE and
respects the Android auto-rotation lock; portrait gameplay stays disabled.
A display listener handles 180-degree changes, while configuration/layout/inset
callbacks refresh native surface dimensions and HUD/touch safe areas. Pending
input is released and tilt recalibrated. Same-window resize retains runtime and
window generation; surface changes use existing synchronized swapchain recreation.
A rotation does not deliberately restart the guest or recompile its shaders.
Physical Android lifecycle/driver behavior still needs validation.

## Validation actually executed

* Android ARM64 Release build succeeded with NDK 28.2.13676358/CMake 3.30.5.
* Nine host CTest checks passed with UBSan: profiling enabled/compiled out, memory
  ranges, DXT regions, frame pacing, Vulkan 1.1 feature masks, frame statistics,
  pipeline-cache corruption/identity/atomic I/O, surface capability policy.
* The nine component executables also cross-compiled for ARM64/API 29; they were
  not run on Android.
* Existing graphics memory/DXT, COM lifetime, guest memory protection, PCM mixer
  and guest file checks passed. Full generated-runtime host bring-up reached the
  expected renderless Direct3DCreate9 boundary; this is not a gameplay test.
* 54 SPIR-V modules passed Vulkan 1.1 validation.
* Three rotation integration guards and the executable Java orientation policy
  check passed. Four existing localization regression checks passed; no language
  work was done. Build artifact guards passed two applicable cases, with the
  missing-private-input case skipped because real generated input was present.
* Six packaged libraries are ELF64 AArch64 with >=16 KiB LOAD alignment; all 19
  Java native exports and NativeActivity entry point are present. Required dynamic
  symbols resolve statically against packaged libraries and Android API 29 stubs.
  This does not prove on-device dlopen or initialization.
* APK ZIP integrity, ARM64-only packaging, version/manifest, 16 KiB alignment and
  APK Signature Scheme v2 passed. Release uses the same development certificate
  as Velocity .1/.2, not the official upstream signing certificate.

Final APK: `NFSU2-Project-Velocity-ARM64-v0.14.0-velocity.3.apk`, 62,110,024 bytes.
SHA-256: `1f21c3bd5dd6c8ad367d018110149167281407d1018331e19ba02ee28858757c`.
Certificate SHA-256: `a9f5483dbd5a28e74164d47cfbfa224a5532075a40ae3f90ec56602023790b18`.

## GPU coverage and limitations

| Family | Implemented policy | Physical validation |
| --- | --- | --- |
| Adreno, including 610 | Vulkan 1.1 baseline, capability/format checks, device-specific cache | Pending |
| Mali/Immortalis | Same capability policy; no Turnip; compiler/cache diagnostics | Pending |
| Xclipse | Same policy; no assumed AMD/desktop feature support | Pending |
| PowerVR/other Vulkan GPUs | Same policy; optional features remain optional | Pending |

No universal performance guarantee is made. Vulkan initialization still depends
on the actual driver meeting backend requirements. Optional features are not
forced. Shader stuttering may persist on cold caches or driver compilation paths.
No real device test of launch/import, menus, career/race/save, visuals, audio,
gamepad/touch, installation, recovery or resume was possible in this environment.

## Reproduce device measurements and rollback

1. Record device/Android, selected GPU/API/raw driver, battery/thermal state,
   Winlator container version/driver/DXVK/preset, scene and resolution. Use the
   same legitimate game folder, save, graphics options, FPS cap and queue mode.
2. Compare .2/.3 with HUD OFF and profiling ON; restart before each capture.
   Export from the launcher using the existing report action. The capture is
   bounded to 600 frames; repeat identically rather than treating estimated
   tester FPS as baseline. Compare median/p95/p99 frame intervals and labeled
   CPU/render/wait/compile/texture events. CSV timings can overlap; do not sum
   scopes as if they were disjoint GPU times.
3. Test cold and warm cache separately. For a cold run remove only diagnostic
   shader caches, not saves/game data; for warm run pause normally to save then
   restart. Correlate stutters with pipeline compilation/waits. Use Perfetto or
   Android tracing when available; profiling itself can affect results.
4. Test compact/detailed/OFF HUD and ten rotation cases: normal launch; 180-degree
   rotation; return; repeat in gameplay; repeat in menus; touch alignment; HUD;
   pause/resume; fixed normal; fixed reverse. Verify race/save state persists,
   cutouts and aspect ratio, and test with system auto-rotation lock ON/OFF.
5. Repeat on Adreno, Mali/Immortalis, Xclipse and PowerVR when available. Do not
   generalize one driver's success to all families.

Rollback by disabling HUD, shader persistence and experimental pacing separately,
using fixed landscape if needed. Velocity .2 remains downloadable; installation
of an older version may require Android downgrade tooling or uninstall, which can
remove private data, so back up saves first. Revert the relevant feature commits
for a source rollback; never replace generated functions with stubs.
