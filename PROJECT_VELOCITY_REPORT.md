# NFSU2 Recompiled — Project Velocity ARM64

Experimental modification: `0.14.0-velocity.1`, not an official upstream release.

## Initial audit (before implementation)

Base: codepdbh/nfsu2-recompiled `125fbcf6c6ac346d046e014d44c6aaae9b1e5d88`.
The selected user repository initially contained only a README. This work uses
an unchanged clone of the latest upstream on `feature/project-velocity-arm64`.

Reviewed: README; Android Gradle/CMake; guest_memory.h; guest_runtime.cpp;
arm64_guest.h; guest_heap.h; d3d9_bridge.cpp; d3d9_locks.inc; dxt_decode.h;
dxt_shadow.h; main.cpp; GameOptions/GameActivity/LauncherActivity/Diagnostics;
DXVK preparation, pinned source and WSI; memory and graphics tests; recent commits.

Architecture: locally lifted x86 C -> native ARM64, portable 4 GiB guest mapping,
serialized FIFO/recursive guest scheduler, guest COM handles -> DXVK Native
1.9.2b (`c8dc91fabd00cac11d697ccf07426e798393cd40`) -> Vulkan. The Vulkan
bootstrap renderer in main.cpp is separate from the game's DXVK renderer.
Local registers, nonvolatile guest accesses, strict aliasing disabled, O2,
hidden visibility, ThinLTO and hashed dispatch already exist. Guest yield checks
are amortized over 64 checkpoints with a 500 us timeslice. Preserve them.

Actual build pins: SDK 35, minSdk 29 (README's 26 is stale), NDK
28.2.13676358, CMake 3.30.5, AGP 8.7.3, Gradle 8.9, arm64-v8a by default.
The checkout intentionally excludes generated C and commercial game files.
Complete native/APK builds require locally generated recomp_*.c and patched
pcrecomp runtime. No device, game executable or generated sources were present
at initial inspection. No FPS or gameplay baseline is available.

## Issues and evidence boundaries

Read issues and comments #189, #250, #268, #278, #169 and #276.
Performance reports do not identify a bottleneck. #189's device title is
SM-F741B; do not substitute a different device based on the task description.
#250 reports MT6897 and ~5 FPS; #268 reports MT6989/14T Pro. Their supplied
launcher logs are not CPU/GPU profiles. #278 reports missing depthClamp and
fillModeNonSolid on Mali-G76; enabling unsupported features is unsafe, and
emulation requires separate rendering correctness tests. #169 is a startup
MessageBoxA boundary, not evidence of Winlator's relative runtime overhead;
recent upstream commit 2a3c440 already addresses MessageBoxA and Mali optional
features. #276's partial native backtrace lacks enough symbolized information
to diagnose the fault. Upstream also recently added sockets and ARMv7 support.

Confirmed redundant work by code inspection (not measured gameplay bottlenecks):
GuestMemory::accessible does a map lookup for every page although default
committed pages are RW; CPU DXT fallback decodes/allocates an entire mip on each
writable unlock, even when uploading only a small block-aligned rectangle.
Existing bridge clock sampling runs even without a profiling opt-in.

Hypotheses requiring device traces: scheduler contention, CPU guest execution,
D3D call volume, pipeline compilation stutter, BC conversion, submission/fence
waits, thermal throttling and presentation pacing. Do not change synchronization
or compiler floating-point semantics on those hypotheses alone.

Risks: protection metadata must remain coherent after protect/commit; compressed
partial updates must preserve block edges, mip/face ownership and native pitch;
profiling estimates must not be described as GPU execution time; app options
cross a launcher/game process boundary; cache changes require writable private
storage and must not bypass driver feature checks. Preserve package name,
original import workflow, controls, audio and game logic.

## Implementation and validation

Pending; recorded below after tests. No proprietary inputs will be committed.

### Changes and technical justification

- **Runtime/memory:** `GuestMemory::accessible` still checks wraparound, address
  space size and committed-region coverage. It traverses explicit protection
  overrides once, starting at the first covered page; default committed RW
  pages require no individual map lookup. Complexity changes from
  O(pages × log(overrides)) to O(log(overrides) + covered overrides). No
  unchecked pointer fast path, permission cache, removal of protections or
  guest instruction/FP semantic change was introduced.
- **Textures:** `decodeDXTRegion` validates source length and block alignment,
  retains full-mip source stride, and converts only the modified rectangle.
  `copyLockBack` uploads that rectangle with its native row pitch. A 4×4
  update in a 1024×1024 mip now allocates 64 RGBA bytes instead of 4 MiB and
  decodes one block instead of 65,536. This is an algorithmic reduction for
  partial updates, not evidence of the frequency of such updates in gameplay.
  DXT1 transparency and DXT3/5 alpha algorithms are unchanged. Full-mip API
  compatibility remains. Existing compressed shadow data still stores each
  mip/face. No extra texture cache/UI knob is added without a safe invalidation
  and resource-lifetime design; identical unlocks are not skipped (DISCARD and
  driver memory ownership prohibit assuming the native pixels still exist).
- **Profiling:** `velocity_profile.{h,cpp}` has an opt-in, 600-interval ring,
  process CPU clock, sampled D3D9 durations (1/64, weighted estimate), exact
  host Present durations, attempted draw counts, compressed decode/copy
  timings, scheduler yield/save/restore/reacquisition time and limiter sleeps.
  Runtime collection is OFF by default; compile out with
  `-PnfsVelocityProfiling=false` / `NFS_VELOCITY_PROFILING=OFF`. The old
  unconditional bridge clock sampling/120-frame logging is replaced by the
  opt-in collector. No CSV I/O or periodic logs occur inside gameplay frames.
  Returning to the launcher exports an atomic CSV snapshot on a background
  thread. First intervals are discarded; background time is excluded by
  resetting the baseline. A killed process may lose its unexported history.
- **DXVK/Vulkan:** reproducible `prepare.py` patches time host queue submission,
  queue presentation, fence synchronization and graphics/compute pipeline
  creation, and count pipeline bind commands. These hooks use Android ATrace
  sections when tracing is enabled. No fence, barrier, semaphore, command-pool
  lifetime or frames-in-flight change was made. These are host timings,
  **not Vulkan GPU timestamp queries**. GPU execution timestamps and exact
  lifted-runtime attribution are not implemented. Asynchronous metrics are
  assigned to the interval in which the scope completes; concurrent/nested
  metrics overlap and must not be subtracted from wall time to invent a guest
  runtime metric. D3D sampling can alias repetitive call sequences and should
  be corroborated with Perfetto CPU sampling. Draw/pipeline counts include
  attempted operations, not exclusively successful driver operations.
- **Frame pacing:** `FramePacer::deadline` retains upstream fixed-deadline
  catch-up by default. Experimental mode clamps overdue deadlines instead of
  producing repeated uncapped catch-up frames. Cap/mode changes and >200 ms
  gaps reset the schedule. This is a CPU frame cap, not Android Choreographer
  or a Vulkan presentation-mode change. No measured pacing gain is claimed.
- **Launcher:** functional section **Project Velocity — Performance** provides
  profiling ON/OFF, default/experimental frame pacing, opt-in DXVK state cache
  and CSV export. The shader option selects the existing DXVK state cache,
  stored in the app's private writable directory, not a new GPU binary cache.
  Failure to create that directory falls back to disabled caching. Default
  cache behavior remains OFF. Profiling controls disappear in builds that
  compile it out. General diagnostic ZIPs include the CSV when available.
  Version name is `0.14.0-velocity.1`, versionCode 15; package remains
  `com.nfsu2.androidevolved`. No side-by-side package/signing change was made.

### Validation results

| Check | Observed outcome |
| --- | --- |
| Existing guest protection test | Passed |
| Existing graphics lock/DXT test + new partial cube/pitch/guard/read-only cases | Passed |
| Existing COM ownership / resolution modes test | Passed |
| New CTest component suite | 5/5 passed with Clang 19; 5/5 passed with GCC 14 + UBSan |
| Permission oracle | 3,000 randomized protect/update/range comparisons passed |
| DXT rectangle comparisons | DXT1/3/5, partial edges, full-mip stride and invalid inputs passed |
| Profiler contract | 600-row bounded ring, sample weights, real RAII scope, multi-thread events, disabled/reset/error paths passed |
| Frame pacing contract | Default debt, experimental clamp, cap/mode changes and long-gap reset passed |
| ARM64 component suite | Five executables compiled/linked with NDK r28c, API 29; not executed on Android |
| Modified native integration | guest_runtime/main/bridge/profiler + four patched DXVK files compiled as AArch64 objects with profiling ON and OFF |
| Complete DXVK Android backend | 116 compilation/link steps succeeded, static ARM64 library; final regenerated patches also rebuilt successfully |
| Built-in shaders | 54 generated and validated by spirv-val for Vulkan 1.1 |
| Release Java/resources | `:app:compileReleaseJavaWithJavac` passed with profiling enabled and disabled |
| Full Android Release | **Blocked** at configureCMakeRelWithDebInfo: `Generate the local game C first; see the project README` |
| APK, package ABI contents, zipalign, signature, APK SHA-256 | Unrun: no APK exists; no test signing key generated |
| Device startup/gameplay/audio/control/save/resume/GPU compatibility | Unrun: `adb devices` reports no attached devices |
| CI workflow | Added for component checks; remote run status reported separately, not assumed passed |

Initial setup errors were resolved: user account lacks root/sudo (tools installed
under /workspace); Ninja activation required PATH; GCC caught existing compact
indentation in the touched memory function (separated the statements); Clang
caught an unused host-only trace field in new profiling code (guarded for
Android); direct native test command needed VK_USE_PLATFORM_ANDROID_KHR.
Java downloads required the environment's HTTPS proxy and supplied CA in a local
Java truststore. Android CLI latest no longer handles legacy --licenses, so
compatible official tools 19 were used for AGP 8.7.3. SDK/NDK/CMake archives were
checked against Google's repository checksums; Debian shader tools used signed
APT indices and package checksums. Verification was never disabled.
These setup corrections do not supply the intentionally absent game-derived C.

### Measured synthetic host results (not Android/FPS benchmarks)

AMD EPYC 9V74 x86_64 cloud host, Clang 19 `-O2`, same synthetic inputs, three runs.
Upstream headers from base HEAD compiled as the baseline; modified headers as
candidate. 10,000 checks cover a committed 4 MiB range with no overrides;
100 texture updates cover a 4×4 rectangle of a 1024×1024 DXT1 mip. Checksums and
valid counts match. Wall times are noisy under shared cloud load:

| Workload | Before, ms (three runs) | After, ms (three runs) |
| --- | --- | --- |
| 10,000 memory validations | 4.232291 / 3.025879 / 3.037366 | 5.990600 / 0.038358 / 0.053862 |
| 100 partial DXT updates | 221.014040 / 223.312676 / 225.954811 | 0.009003 / 0.006610 / 0.035733 |

The first candidate memory run was slower under load; do not present this as a
stable speedup or extrapolate to any phone. These workloads demonstrate the
specific avoided work, not that it dominates NFSU2 frames. No Android FPS,
CPU/frame improvement or 30/60 FPS target attainment has been measured.

### Reproduce component validation

Use an existing checkout in its isolated cloud task; do not create a worktree.
Prepare DXVK at its pinned revision as in ports/dxvk_android/README.md, then:

```sh
# Run from repository root; use Clang and CMake/Ninja on PATH.
CXX=clang++ cmake -S tests/velocity -B build/velocity -G Ninja
cmake --build build/velocity --parallel 4
ctest --test-dir build/velocity --output-on-failure
CXX=clang++ cmake -S tests/android_memory -B build/memory -G Ninja
cmake --build build/memory && build/memory/memory_check
CXX=clang++ cmake -S tests/android_graphics -B build/graphics -G Ninja
cmake --build build/graphics --parallel 4
build/graphics/graphics_memory_check
build/graphics/graphics_com_check
cmake -S tests/velocity -B build/arm64 -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$ANDROID_HOME/ndk/28.2.13676358/build/cmake/android.toolchain.cmake" \
  -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-29 -DANDROID_STL=c++_static
cmake --build build/arm64 --parallel 4
# Compilation is not execution. With a device, push each test to /data/local/tmp
# and execute there; the profiler test uses that directory for its CSV.
```

Synthetic microbenchmark: save base HEAD's guest_memory.h/dxt_decode.h in a local
ignored build directory. Compile tests/velocity/microbench.cpp with `-O2` and that
include directory for baseline; compile against the current cpp include directory
with `-DVELOCITY_REGION_BENCH` for candidate. Run repeatedly with identical inputs,
record all results and host load. This is intentionally outside normal CTest.

### Reproduce device benchmarks

1. Generate local game C using the upstream documented pipeline and your
   legitimate executable; retain generated inputs only in ignored local storage.
   Copying/importing game files in the launcher does not run that build pipeline.
2. Build baseline and candidate with the same pinned tools and generated inputs.
   Use `cd android; bash gradlew :app:assembleRelease -PnfsAbi=arm64-v8a`.
   Release is unsigned by upstream default. Test-sign locally with zipalign and
   apksigner using a private development keystore stored outside the repository.
   Verify with `apksigner verify --verbose --print-certs`, `zipalign -c`, inspect
   `lib/arm64-v8a/` and manifest version, then compute SHA-256. A different test
   signature cannot update the official installation with the same package;
   back up user saves before any uninstall. No official signing key is available.
3. Test menus, race loading, the same race/replay and camera, audio, touchscreen,
   gamepad, save/load and pause/resume on Adreno, Mali and PowerVR. Record device,
   OS, Vulkan driver, resolution, effects, FPS cap, CPU/GPU sync, temperature and
   battery/power mode. Keep the original user's game import workflow.
4. Warm up the same scene and shaders; measure separate cold and warm runs.
   Run repeated 60-second scenarios, capture Perfetto CPU scheduling/frequency
   and app ATrace (`com.nfsu2.androidevolved`) for deeper attribution. Sampling
   tracing requires device permissions/tool support; do not claim traces captured
   in this cloud host. Export the last 600 intervals after returning to launcher.
5. `python3 scripts/velocity_report.py Project-Velocity-performance.csv
   --budget-ms 33.333333` (or 16.666667 for 60 FPS). Compare p50/p95/p99/worst,
   CPU process time, over-budget intervals, draws, conversion and host waits.
   The presentation rate is a rate within the captured window, not proof of
   rendered gameplay on an untested device. Avoid adding overlapping metrics.
6. A/B profiling OFF/ON for overhead; A/B default/experimental pacing and cold/
   warm state cache independently. Discard loading/paused intervals explicitly,
   retain raw files, and repeat under matched thermal conditions.

### Limitations and rollback

No device benchmark or gameplay regression check is possible here. #278's
unsupported depthClamp/fillModeNonSolid requires an independently tested fallback;
#276 requires a full symbolized tombstone matching the crashing library. Vulkan
GPU timestamps, exact guest-runtime CPU attribution and additional texture cache
invalidation are deferred, rather than represented by fake controls or metrics.
The 600-interval ring is a recent window, not a full-session database; a process
crash before pause/export loses history. Host traces/profile timing perturb work
when enabled, and deterministic 1/64 sampling can alias the call mix.

Disable profiling, shader cache and experimental pacing in the launcher; compile
profiling entirely out using the Gradle property above. To revert texture or
permission changes, revert their individual area commits. Keep native fences,
barriers and the upstream FIFO scheduler. No user game code/assets/executable,
private keys, local credentials or build caches are included in Git commits.

### APK and publication status

No APK was generated: src/recomp/gen/recomp_*.c is absent and the actual Release
build stops at that explicit upstream prerequisite. Packaging, signature, size,
APK checksum and direct download link cannot be supplied. This is not an APK
release or a claim that the complete environment/game is ready.
GitHub denies fork creation with HTTP 403 `Resource not accessible by integration`;
upstream permissions are read-only, while the selected user repository permits
branch writes. Publication results are appended after actual GitHub operations.

Publication confirmed:

- Branch: https://github.com/silasquercloud/nfsu2-recomp-project-velocity/tree/feature/project-velocity-arm64
- Area commits: d6236c1 (profiling), e59df9c (runtime), 86a4f07 (textures),
  638b0c3 (DXVK), 5b5577d (launcher), 36e0551 (tests/report).
- Draft review PR: https://github.com/silasquercloud/nfsu2-recomp-project-velocity/pull/1
  targets `upstream/nfsu2-125fbcf`, a separate branch with the exact original
  base. The selected user's main was an unrelated README-only history; this
  review base shows only Velocity changes instead of importing all upstream
  files into a main PR. Neither main nor upstream main was changed or merged.
  The user repository is **not an upstream fork**. This PR is a fallback review
  location, **not a PR submitted to codepdbh/nfsu2-recompiled**.
- Original fork creation and original-target PR API POST both returned HTTP 403
  `Resource not accessible by integration`. Original-target submission needs
  GitHub integration access to create that fork/PR; do not misreport it as done.
- Remote component CI passed at 36e0551:
  https://github.com/silasquercloud/nfsu2-recomp-project-velocity/actions/runs/38311691079
  (host tests and ARM64 component compilation; not APK/gameplay validation).
- Working tree remained free of untracked game inputs, credentials and binaries.
  There is no APK download, signing result, file size or SHA-256 to report.

## Follow-up: official release build audit

See [PROJECT_VELOCITY_BUILD_AUDIT.md](docs/PROJECT_VELOCITY_BUILD_AUDIT.md).
Player experience remains install APK + import the original game folder. Source
generation is the developer's release-build step, never a new player requirement.
The public upstream exposes no workflows/runs/artifacts supplying generated C.
The actual v0.13.1 APK was checksum-verified, signature-verified and inspected:
it is a full ARM64 Debug-signed/debuggable build, without generated source files
or external commercial game assets. The documented developer build path is
assembleDebug; exact private release commands are not publicly observable.
The new developer-only build entry point uses that signed path and refuses
incomplete native packaging or an unchanged official library presented as Velocity.
A direct Debug retry still fails at the original CMake source prerequisite.
Three real-input build guard tests passed. No Velocity APK exists; this delivery
remains incomplete until authorized developer-generated inputs are accessible.


## Phase 2: supplied executable unblocked the complete build

The previous missing-source blocker is resolved in the private developer workspace.
The supplied file's actual hash/layout were verified, the original pipeline
recovered 27,742 functions and generated 70 C units with zero lift failures, and
the complete modified ARM64 APK compiled/linked, packaged and test-signed.
See [Phase 2 evidence](docs/PROJECT_VELOCITY_PHASE2.md) for exact artifact hash,
symbol/ABI/signature checks, integration fixes and remaining device-test limits.
Earlier blocked-build sections above are historical, not the final build state.
The final 68,302,087-byte APK is a private chat/workspace artifact; no commercial
executable, generated code or APK is posted to this public repository. Player
install/import behavior remains intact. Android installation/gameplay and FPS
still require a real device and the complete user's game folder.
