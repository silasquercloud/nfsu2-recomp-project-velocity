# Project Velocity: official APK build audit

The work remains **blocked**, not complete: no functional Velocity APK has been
built. Players keep the original experience: install an APK, import/copy their
legitimate PC game folder through the existing launcher workflow, then play.
They are not required to own a development PC, run a compiler or generate C.
The generation described below is a **developer/release-builder prerequisite**.

## Public release evidence

Audited the original repository, tags v0.13.0/v0.13.1, complete public tree,
Android Gradle/CMake, generation scripts, history, release metadata, Actions
workflows/runs/artifacts, and the actual v0.13.1 ARM64 APK.

- The public original repository has no `.github/workflows` files. The GitHub
  workflows API returned no workflows; Actions runs and artifacts APIs each
  returned `total_count: 0`. No public prior CI/lift artifact source was found.
- Both public releases have only ARM64/ARMv7 APK assets. GitHub source archives
  contain tracked files and cannot supply the gitignored generated directory.
  No tracked `src/recomp/gen` history or additional public build branch was found.
- README's Pipeline section describes developer-local normalization, seed
  scanning, catalog generation and lifting. `docs/ANDROID_RUNTIME.md` explicitly
  states the complete local APK includes game-derived generated code from the
  owner's copy and generation/build products remain gitignored. Some Android
  status documents are historical and predate playable releases.
- The README and Android port build notes invoke `assembleDebug`; tag v0.13.1's
  Gradle contains no custom signing or release automation. The published APK
  is debuggable and uses an Android Debug certificate, consistent with that
  documented signed Debug build. The publisher's exact private commands,
  workspace and any private pipeline cannot be observed; do not claim to have
  recovered an unpublished CI workflow.
- Release asset is uploaded by `codepdbh`. Merely downloading its compiled
  native library is not a source pipeline for the modified Velocity runtime.

Official APK inspected (baseline only, **not a Velocity deliverable**):

https://github.com/codepdbh/nfsu2-recompiled/releases/download/v0.13.1/NFSU2-AndroidEvolved-v0.13.1-arm64.apk

| Property | Observed value |
| --- | --- |
| Bytes | 68,226,907 |
| SHA-256 | `e04752c50d3d396caa34010191d95698ff480e056c9592c3d27542554af2f9a2` |
| Integrity | Matches GitHub asset SHA-256 digest |
| Package / version | com.nfsu2.androidevolved / 0.13.1 / code 14 |
| SDK | min 29, target/compile 35 |
| Manifest | application-debuggable |
| Signature | apksigner verification passed, scheme v2 |
| Certificate | C=US, O=Android, CN=Android Debug |
| Certificate SHA-256 | `53f31cba4e09e3d22206639f9705f78bc8aa887f649e465d3590fe70db12b4cf` |
| Alignment | zipalign -c -P 16 4 passed |
| Native ABI | arm64-v8a |
| Runtime | libnfsu2_android.so, uncompressed ELF size 154,873,056 bytes |
| Other libraries | nfsu2_diagnostics, main_hook, file_redirect_hook, gsl_alloc_hook, hook_impl |
| Generated .c/.h entries | None |
| Game EXE/DLL/BUN/BIG/video entries, assets directory | None found |

The binary exports connectGuestRuntime and dispatchGuestD3D9; it already contains
the compiled native port. There is no reusable generated-source bundle inside
this APK. The author's Debug private key is not available and was not extracted.

## Where generated C enters the official build

The public, documented developer chain is:

1. A locally available, legitimate supported SPEED2.EXE (README: 4,800,512 bytes;
   SHA-256 f9dd86c054878ce6276beb07c1fd61874f7a1e4bf1f241b084c65b73e24168a7).
2. scripts/normalize_exe.py creates a separate analysis copy with relocations.
3. Patched pcrecomp scans seeds and scripts/run_lift.py catalog constructs the
   basic-block/function catalog. README refers to local work/seeds_all.json;
   the seed aggregation is not supplied as a complete automated release job.
4. scripts/run_lift.py lift reads executable bytes and the catalog, applies
   config overrides/exclusions/hooks, and emits src/recomp/gen/recomp_NNNN.c,
   recomp_funcs.h, recomp_dispatch.c and recomp_imports.c. It marks generated
   output DO NOT COMMIT. All these files remain local/gitignored.
5. Prepare the pinned DXVK native source and validate its shaders. Android
   Gradle invokes CMake with NDK 28.2.13676358 and CMake 3.30.5.
6. Android CMake globs src/recomp/gen/recomp_*.c, creates object target nfs_guest,
   compiles it as native ARM64 with the patched runtime headers, and links its
   object files into libnfsu2_android.so alongside runtime, D3D9 bridge and DXVK.
   ThinLTO participates in this link; lifted code is not a separate APK plugin.
7. The documented assembleDebug task packages the complete native libraries,
   launcher/resources/manifest and applies a development signature. The
   signed APK is the distributable the player installs.
8. On the phone, the existing launcher/runtime reads the user's external game
   folder, maps PE sections, binds imports and dispatches **already compiled**
   guest functions. It does not run pcrecomp/Clang or produce .c/.so files.

No scripts/build.sh release mechanism substitutes for this: that script builds
MSVC Windows x86. No publicly documented artifact-fetch stage supplies the
missing game-derived source in a fresh source checkout.

## Velocity comparison and corrected build entry point

Velocity is based on upstream 125fbcf (the v0.13.1 release tag is 9fca492).
The newer public HEAD observed in this audit, bd1f20e, adds touch/memmove/network
changes, not a source-delivery or APK build pipeline. It was inspected but not
merged into this scoped build investigation.

Velocity did not alter GEN_DIR, generated-source glob, nfs_guest object linkage,
patched pcrecomp path, guest compile flags/ThinLTO, SDK/NDK/CMake pins, package,
launcher data detection, or PE import/loading architecture. Its CMake changes
add profiler compilation and a definition; its Gradle changes add experimental
version/BuildConfig flags. The existing source prerequisite was already fatal
in upstream. It was not introduced by the optimization changes.

The previous attempt chose assembleRelease, which is unsigned by upstream
configuration. To reproduce the documented signed testing path, the new
**developer-only** script scripts/build_velocity_apk.py invokes:

```sh
cd android
bash gradlew --no-daemon :app:assembleDebug \
  -PnfsAbi=arm64-v8a -PnfsVelocityProfiling=true
```

It validates actual ZIP integrity, ARM64 ELF headers, all six native libraries,
Velocity's native marker (rejects simply reusing the official library), package,
version/code, prohibited game-file entries, zip alignment and APK signature
before exposing the requested final filename. It does not alter CMake, generate
stubs, copy the official binary into the build or modify the launcher.

Developer usage once authorized source inputs are available:

```sh
# Developer build workspace, not a player instruction.
export ANDROID_HOME=/path/to/android-sdk
python3 scripts/build_velocity_apk.py
# This prepared cloud environment can use its trusted proxy/TLS wrapper:
# python3 scripts/build_velocity_apk.py --gradle-runner /workspace/velocity-tools/run-gradle.py
```

Expected output after a real successful build:
`build/android/NFSU2-Project-Velocity-ARM64-v0.14.0-velocity.1.apk`.
That file **does not currently exist**. No signing, hash, size or download link
for Velocity can be claimed. Verification in the script checks packaging,
not gameplay. Its success path cannot yet be exercised with a Velocity APK.

## Actual retry and tests

- The direct signed Debug task was retried in this cloud environment and
  stopped at configureCMakeDebug[arm64-v8a] with the same fatal missing-generated
  source prerequisite as Release: `Generate the local game C first`.
- The new developer entry point detects the exact missing lifted functions,
  declaration header, dispatch table and import table before launching Gradle.
  It exits nonzero and creates no substitute output.
- Three Python guard tests passed using this public checkout and the real,
  checksum-verified official APK: missing-input/no-output handling, official
  ARM64/library packaging, and rejection of an unchanged official game library
  as a Velocity APK. No fake C or APK fixtures were produced.
- Baseline integrity/signature/alignment inspection passed. The existing
  Velocity component, Java, native object and DXVK validations are recorded in
  PROJECT_VELOCITY_REPORT.md; no C/C++ or launcher application code changed in
  this build investigation.

## Remaining dependency and what is possible now

Public tooling setup, pinned pcrecomp/DXVK preparation, shader validation, Java
compilation, independent runtime component tests and ARM64 DXVK builds can be
reproduced without commercial inputs. They do not supply the compiled game.

The remaining release-builder input is an authorized, compatible complete
locally generated source set (functions plus headers, dispatch and imports),
or developer-side access to the supported legitimate executable and the local
catalog/seed generation pipeline. No source/CI artifact mechanism supplying it
was found publicly. The original developer has such local generated inputs
according to the documentation; their actual current workspace is inaccessible.

This is an input/access blocker for the developer's rebuild, not a change to
player onboarding. Once it is supplied to an authorized build workspace, the
new entry point can build/sign/verify the complete modified port. The missing
input cannot be replaced by relabeling the official APK, retaining its unmodified
native library or shipping only the launcher. No private/proprietary inputs have
been downloaded from unauthorized sources, committed or redistributed.
