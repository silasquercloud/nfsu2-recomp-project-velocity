# Project Velocity ARM64 0.14.0-velocity.2 — pt-BR / low-end GPU investigation

This is an experimental modification, not an official upstream release. Velocity .1 remains available at its existing release/tag and its original APK checksum is unchanged. The existing feature branch and review PR #1 are retained. Generated game C/headers and the supplied executable remain in the ignored, private workspace. Players install the APK and use the original game-folder workflow; no recompilation step is added to the launcher.

## Diagnosis and evidence boundaries

The user reports that NFSU2 runs well in Winlator on Adreno 610, Vulkan 1.1, Android 16, but Velocity .1 remains very slow even at lower resolution. This is a user-observed comparison, not an instrumented benchmark collected by this build. Winlator version, driver, DXVK version, container resolution and presets have been requested and are not yet available. Lower resolution alone does not identify the limiting CPU, driver, upload or synchronization path.

Confirmed by code inspection:

* Velocity uses pinned DXVK Native **1.9.2b**, commit `c8dc91fabd00cac11d697ccf07426e798393cd40`; its instance requests Vulkan 1.1. Current upstream DXVK requests Vulkan 1.3, so replacing the backend with current DXVK would exclude the reported Vulkan 1.1 system driver.
* Before .2, CPU DXT fallback decoded each changed region into a newly allocated RGBA vector and copied those pixels again to the mapped DXVK resource. The native resource was already writable. This allocation, initialization and extra full-region copy are removable without changing compressed shadow bytes or lock semantics.
* Surface Release logging was unbounded. Texture CPU-fallback logging was repeated for every allocation. These are bounded to eight messages each per game process in .2.
* Velocity .1 disabled the DXVK state cache unless the user enabled it. The cache uses a writable app-private directory; .2 defaults to enabled for unset preferences and preserves an explicitly saved off setting. Failure to create the directory uses the existing disabled/default path with a translated warning. This is a pipeline-state cache; it does not promise a persistent driver binary cache or removal of all first-use stutter.
* Winlator is configurable: its official application source selects wrappers/drivers per container and can enable a variant marked async. That does not establish that this user's installation uses Turnip or async DXVK.

Not confirmed on the target device: BC support of the actual loaded driver, amount of DXT conversion during gameplay, GPU saturation, guest-runtime CPU share, repeated lock waits, pipeline stalls, present mode limits, thermal throttling, or why Winlator is faster. No FPS improvement is claimed.

## Inspected sources and architectural comparison

Pinned read-only investigation:

| Project | Revision inspected | Relevant files and findings |
| --- | --- | --- |
| Official Winlator | `b6b2259158cf38d06067c34430d840d56b46d220` | `.gitmodules` points to the actual `winlator-app` source; README describes Wine/Box86/Box64. |
| Winlator application, exact pinned submodule | `a030f552f452158a2db64fdb32b490fa19c0b48d` | `container/GraphicsDrivers.java`, `Container.java`, `core/DefaultVersion.java`, `contentdialog/DXVKConfigDialog.java`, `TurnipConfigDialog.java`, `box64/Box64PresetManager.java`, `XServerDisplayActivity.java`. Current defaults are Vortek/Gladio; DXVK defaults choose 1.10.3 or 2.4.1 depending on driver/API. These are source defaults, not this user's configuration. |
| DXVK upstream | `649c4bc0f0e303fcf9e21fad1c36e30ebce79b02` | `src/dxvk/dxvk_instance.h`, `dxvk_instance.cpp`, `dxvk_device_info.cpp`: current required API is Vulkan 1.3. |
| Box64 | `0684e43871c642f218f20aa8df5816ca755e49aa` | `docs/USAGE.md`, ARM64 dynarec sources: dynamic translation, configurable flags/strong-memory/native-flags policies. Winlator performance presets trade semantics and aggressiveness; these cannot be copied into a static runtime by disabling memory protection. |
| Mesa / Turnip | `19b5f13dcc50d55ef89bd7e68d46a2b0c73751b3` | `src/freedreno/vulkan/tu_device.cc`, `tu_formats.cc`: advertised features depend on GPU capabilities; BC support differs by device. No Mesa code, downloaded APK driver or proprietary driver is integrated. |
| Velocity / original port base | upstream `125fbcf6c6ac346d046e014d44c6aaae9b1e5d88` plus Velocity .1 | `guest_memory.h`, `arm64_guest.h`, `guest_runtime.cpp`, `d3d9_bridge.cpp`, `d3d9_locks.inc`, `dxt_decode.h`, `main.cpp`, `gpu_probe.cpp`, Android Java activities, DXVK preparation and WSI. |

| Path | Winlator | Velocity |
| --- | --- | --- |
| Game CPU | Windows executable through Wine and Box86/Box64 or its selected integration | Original static lifted x86 C compiled to ARM64 with local registers, non-volatile guest memory and ThinLTO; guest HLE and cooperative scheduling |
| D3D9 | Selected Windows DXVK DLL, WineD3D or alternative wrapper | Guest COM/memory bridge → statically linked DXVK Native 1.9.2b |
| Vulkan driver | Container driver selection, including Turnip or Vortek in inspected source | Android system driver or an explicitly imported compatible adrenotools driver; none bundled |
| Presentation | Wine/X11/Winlator WSI and selectable Turnip present settings | Android native surface/Android WSI, DXVK fences and swapchain; guest presentation settings retained |
| Cache | Winlator sets `DXVK_STATE_CACHE_PATH`, Mesa cache settings and optional async variant flag | App-private DXVK state cache; driver internal caching depends on the actual driver |
| BC/DXT | Depends on selected driver and DXVK version | GPU BC when supported; otherwise CPU conversion to RGBA8, preserving visual quality |

The Winlator Turnip dialog sets direct-rendering and present-mode environment variables for its own WSI. They are not portable performance switches for Android native WSI. Its optional async flag is enabled only for versions whose name includes `async`; .2 does not import unofficial asynchronous patches or skip draw calls. Turnip capabilities in the inspected Mesa source do not prove that a particular imported build supports Adreno 610 or Android 16. Mali G52 and PowerVR continue using their supported paths.

DXT1 uses 8-byte 4×4 blocks versus 64-byte RGBA8 blocks (8× storage difference); DXT3/5 use 16-byte blocks (4×). These are format sizes, not measured gameplay traffic. The CPU fallback, its larger resource footprint and stock-driver behavior remain plausible explanations requiring measurements.

## Changes and technical justification

### Textures and logging

* `dxt_decode.h`: validated `decodeDXTRegionInto` writes directly to a bounded, caller-owned pitched destination. `decodeDXTRegion` and `decodeDXT` retain their vector interfaces. Bounds, compressed source extent, block alignment and partial mip edges are validated before allocation/writes. Destination arithmetic uses an overflow-safe extent check.
* `d3d9_locks.inc::copyLockBack`: decode changed DXT regions directly into the already locked native buffer. Compressed shadows, cube face/mip addressing, guest pitch, readonly locks and neighboring/padding bytes are preserved. No fences, barriers, dirty-resource uploads or locks are removed.
* `d3d9_bridge.cpp`: bound diagnostic messages; count CPU texture fallback creation when profiling is active. The GuestMemory .1 optimization is retained without weakening checks. No additional guest-dispatch/CPU semantic change is justified by current evidence.

### Profiling and compatibility

* Bounded 600-presentation-interval ring remains opt-in, compiled out when disabled. No file writes/logging in frame-hot paths.
* Existing CPU/process, sampled D3D9, Present, draw count, Vulkan submit/present, fence, pipeline compile/bind, DXT and frame-limiter metrics are retained.
* New timings isolate texture/buffer lock operations, DXVK resource waits, command-thread synchronization, `vkAcquireNextImageKHR` and D3D9 shader translation. `vulkan_present_host` now measures `vkQueuePresentKHR` alone, separate from pre-acquisition of the next image. Host timings overlap when nested or parallel; they are not GPU timestamps.
* `texture_upload_copy` in .2 covers the whole `copyLockBack` operation, including its nested DXT decode and buffer/texture copies; .1 covered only the final RGBA copy. Do not compare that column directly as if its scope were unchanged. `copied_bytes` records unlock copies to native resources/compressed shadows; decoded pixels describe direct RGBA writes, not measured GPU transfer bytes.
* New counters: decoded pixels, copied guest/resource bytes, texture fallback creation, observed Vulkan submit/present/acquire errors/suboptimal results, swapchain creation/recreation. Counts include an explicit whole-session summary and per-window events; initialization/first-interval events are not silently represented as gameplay-frame timing.
* CSV headers include configured version/device/Android/options and actual DXVK-selected GPU/API/vendor/device/driver-version/BC capability. Diagnostic ZIP can include the separately performed GPU probe. Its selection may differ from a later run; actual DXVK device metadata remains authoritative for that run.
* `vulkan_compat.h`/`gpu_probe.cpp`: probe device creation requests the supported subset of core D3D9 features instead of probing an empty feature mask. No Vulkan 1.2/1.3 requirement, unsupported extension or feature is added. This tests core creation, not an Android surface or all DXVK extension combinations.
* `scripts/velocity_report.py` summarizes per-path host timing and counters in an exported CSV. Launcher exports CSV after returning from the game; full problem-report ZIP remains available even if no frame was completed.

### Launcher localization and build

* Complete resource inventories in `values/strings.xml` (English default), `values-pt-rBR/strings.xml` and preserved Spanish locale resources. Launcher, options, controls, text entry, driver import/test, storage gateway, dialogs, errors, crash/report flows and update messages use resources rather than product-language literals.
* `UiText` applies the Android locale by default and offers manual Português (Brasil) or English. Activity configuration is wrapped before view creation in launcher, game and GPU-probe processes. Unsupported locales use the complete English default set. Game-language files/preferences remain separate and unchanged.
* Native startup/common game-data/probe/HLE failures use diagnostic codes translated at the UI boundary. Technical driver/OS error details and third-party logs can still retain their original wording.
* Package remains `com.nfsu2.androidevolved`; versionName `0.14.0-velocity.2`, versionCode `16`. Build helper produces ARM64 **Release**, not debuggable, using the existing development certificate to allow upgrading Velocity .1. This is not the official project's certificate.

## Validation

Completed:

* Six host Velocity CTest checks with GCC and UBSan: profiling enabled/compiled-out, memory ranges/protections, randomized DXT regions and pitched destinations with canaries/truncation checks, frame pacing, synthetic Vulkan 1.1 feature masks.
* Existing host graphics memory/DXT, COM lifetime/resolution, guest memory protections, PCM/audio mixer and filesystem checks passed.
* Full real generated guest runtime host check passed after re-linking the changed runtime. Actual lifted CRT reaches the expected `hle_missing:d3d9.dll!Direct3DCreate9` boundary in the renderless host test. This is runtime bring-up, not gameplay.
* All 54 embedded SPIR-V shaders passed `spirv-val --target-env vulkan1.1`.
* All six component executables compile with NDK Clang for ARM64/API 29; these cross-compiled checks are not executed on a phone.
* Four localization/fallback structural tests passed; default and pt-BR inventories are complete, references resolve, manual locale is applied in every activity, and user-facing widget literals were audited. Java compilation passed during integration; final Gradle compilation is recorded with the APK below. No on-device screenshot/layout assertion is claimed.
* Real upstream APK packaging guard passed and rejects an unchanged upstream game library as Velocity. The missing-generated-input guard is skipped in this workspace because valid private generated inputs exist.

Final APK build, library, signing and private-delivery evidence is recorded below after validation. No Android device/emulator is connected; installation, import, launcher display, native `dlopen`, gameplay, sound/controls/saving on the target GPUs and FPS cannot be claimed from compilation or static tests.

## Reproduce device comparison

1. Record phone model/SoC, Android/build, driver name/version, Vulkan API and BC support from **Testar driver**. Record Winlator version, container driver/DXVK version, Box64 preset, resolution, in-game detail settings and VSync/present mode. Do not infer these from defaults.
2. Back up saves. Keep the same game files, scene/race, camera, resolution and detail settings. Compare .1 and .2 on the same device, with the same driver and frame cap. Use a warm-up, then three equal-length captures; monitor thermal state/power mode.
3. In .2, enable **Coleta de desempenho**, play, return to the launcher and **Exportar relatório de desempenho**. Export the diagnostic ZIP and include the CSV/GPU probe when available. No game assets are included.
4. Run `python3 scripts/velocity_report.py capture.csv --budget-ms 33.333333`. Compare p50/p95/p99/worst wall interval, process CPU, isolated acquire/present/fence/resource/CS timings, pipeline/shader activity, decoded pixels/copy bytes and fallback/error/recreate counts. Session counters include startup; per-frame counters cover only captured intervals.
5. Capture an Android Perfetto trace with app tracing enabled (`atrace` app `com.nfsu2.androidevolved`) to examine guest and DXVK scheduling against the scoped host events. CPU process time may exceed wall time because worker threads overlap. GPU timing requires a compatible real driver/device; it is not inferred by subtracting host metrics.
6. Repeat with profiling off to quantify collection overhead. Test shader cache off/on with saved configurations and both cold/warm sessions. Disable the experimental CPU pacer if it changes perceived responsiveness. Do not change visual quality while comparing an optimization.

A reduction at lower resolution suggests a GPU/pixel-dependent component; no reduction makes CPU/driver/synchronization hypotheses more relevant but does not prove them. High decode volume supports investigating fallback/conversion, long acquire/fence/resource times require correlating the GPU and CPU trace, and pipeline peaks distinguish cold-cache stutter from steady-state cost.

## Remaining limits / reverting

No target GPU benchmark, no Vulkan GPU timestamp queries, no exact guest-only runtime CPU attribution, no confirmed Winlator configuration and no tested Turnip package for this device. The core capability probe does not guarantee rendering or device-loss recovery. Upstream HLE/driver-specific bugs can remain. Existing graphics synchronization/recovery logic and optional features are preserved; no GPU-specific performance hack is added.

Restore .1 from its existing signed APK/tag to revert the experiment; back up saves before downgrading versionCode. Individual changes are separated into commits. Profiling and experimental pacing can be disabled in settings; persistent DXVK cache can be disabled without deleting saves. Launcher language override can be returned to Android language. Resource translations do not translate commercial game menus, dialogue or subtitles.

Private-delivery tooling uses an ephemeral Actions runner and single-use certificate: only encrypted APK bytes enter a temporary public transport branch; the decryption key remains on the runner, the plaintext APK is attached only to a writer-visible draft release, and the transport branch is removed after upload verification. Signing keys and generated game sources never enter the transport.

## Verified final APK

* File: `NFSU2-Project-Velocity-ARM64-v0.14.0-velocity.2-ptBR.apk`
* Bytes: **62,145,816**
* SHA-256: `c78d2f696ec3c882cf9c3ee9d5c2997124a304ae1c4d57597bb20c54be77de44`
* Complete `:app:assembleRelease` passed, including native guest/DXVK linking, Java/resources/manifest and packaging. The first integration run used an old generated R table after additional strings were added during native compilation; the final incremental rerun regenerated resources and passed. No unresolved compilation failure remains.
* Package/version/ABI, ZIP integrity/unique entries, six packaged ELF64 AArch64 libraries, native Velocity .2 marker, absence of commercial executable/asset entries and `zipalign -c -P 16 4` passed.
* Native static verification found all **17 JNI exports** plus `ANativeActivity_onCreate`, 16 KB-compatible ELF load alignment and **zero unresolved required imports** against packaged dependencies and Android API 29 NDK stubs. This is not an on-device loader test.
* APK Signature Scheme v2 verified. Certificate SHA-256: `a9f5483dbd5a28e74164d47cfbfa224a5532075a40ae3f90ec56602023790b18`, identical to Velocity .1. Release is not debuggable; shell profiling is enabled only in the profiling-enabled build. Compiled APK resources include default, Spanish and **pt-rBR** configurations.
* Velocity .1 remains **68,302,087 bytes**, SHA-256 `04ee0a6bf4601ad4f0ae66aded5a9ed0f2cf8932ae2d1042f5fe8d80b4225e58`, at its existing release.

A signed, smaller Release APK does not establish higher gameplay FPS. No before/after device result is available. Android installation, launcher rendering, import and gameplay still require the player's/device tester's validation.

## GitHub delivery verified — published with user authorization

The upload workflow completed successfully: https://github.com/silasquercloud/nfsu2-recomp-project-velocity/actions/runs/38324465123

* [Public experimental release](https://github.com/silasquercloud/nfsu2-recomp-project-velocity/releases/tag/v0.14.0-velocity.2)
* [Public APK asset](https://github.com/silasquercloud/nfsu2-recomp-project-velocity/releases/download/v0.14.0-velocity.2/NFSU2-Project-Velocity-ARM64-v0.14.0-velocity.2-ptBR.apk)
* Initially delivered as a private draft with an authenticated download and anonymous HTTP 404. On 2026-10-13 the user explicitly requested publication. The release is now public/experimental; anonymous download returned HTTP 200 and the exact SHA-256 and 62,145,816-byte size were verified again. Generated sources, executable and signing keys remain private.
* The single-use decryption key was never committed; the temporary encrypted transport branch was removed. Only the APK is attached to the release, no generated sources or keys.
* Both remote component/guard CI runs at implementation head `11a4b3e` passed: https://github.com/silasquercloud/nfsu2-recomp-project-velocity/actions/runs/38324463423 and https://github.com/silasquercloud/nfsu2-recomp-project-velocity/actions/runs/38324467093. These checks are separate from the full local/private-input APK build.
