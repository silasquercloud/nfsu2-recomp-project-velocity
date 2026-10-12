# Project Velocity ARM64 — Phase 2

Status: complete ARM64 test APK built and structurally verified; device/gameplay
validation remains unperformed. This report records observed results. Player onboarding remains the original launcher
and external game folder; generation is performed only in the developer workspace.

## Supplied executable

The actual attached SPEED2.EXE was readable and was used as the original input.
No second upload was requested, and the original was not overwritten.

| Property | Supplied file | Upstream documented/runtime contract |
| --- | --- | --- |
| Bytes | 4,800,512 | 4,800,512 |
| SHA-256 | db8f7b6f6c5439a9fec17829efb0cf31771eaf967de08f99f9146d302ec5a680 | f9dd86c054878ce6276beb07c1fd61874f7a1e4bf1f241b084c65b73e24168a7 |
| Format/machine | PE32 / i386 (0x14c) | PE32 / i386 |
| ImageBase | 0x00400000 | 0x00400000 |
| Entry VA/RVA | 0x0075BCC7 / 0x35BCC7 | same |
| SizeOfImage | 0x532000 | same |
| Sections | .text, .rdata, .data, .rsrc, existing relocations, rebuilt imports | same layout |
| Imports | 252 | 252 |

The file is not byte-identical to the documented hash. Its actual headers match
loadImage's required layout, the entry has the documented CRT shape, and known
hook sites decode at the expected instruction addresses. Generation uses this
supplied file, not an assumed reference. Device correctness remains unverified.

The original scripts/normalize_exe.py created only an ignored analysis copy,
restoring the metadata that points to already present relocation blocks. All
raw section bytes were compared before/after and remained identical. This does
not modify instructions or force a different executable layout.

## Original generation chain used

Public tooling: pcrecomp base 35548b8 plus the repository's patches 0001–0014;
Python pefile 2024.8.26 and Capstone 5.0.9. No new recompilation engine was added.

Commands run from the project root (all generated material below is private and
ignored by Git):

```sh
python scripts/normalize_exe.py "$ATTACHED_EXE" work/velocity-input/SPEED2.analysis.exe
python ../pcrecomp/tools/cpp/vtable_scan.py work/velocity-input/SPEED2.analysis.exe \
  --seeds work/velocity-input/vtable_seeds.json
python ../pcrecomp/tools/cpp/rtti.py work/velocity-input/SPEED2.analysis.exe \
  --seeds work/velocity-input/rtti_seeds.json
# Merge the two scanners' address lists, deduplicate and sort into seeds_all.json.
python scripts/run_lift.py catalog work/velocity-input/SPEED2.analysis.exe \
  work/velocity-input/seeds_all.json work/velocity-input/catalog.json
python scripts/run_lift.py lift work/velocity-input/SPEED2.analysis.exe \
  work/velocity-input/catalog.json src/recomp/gen --split 400
```

The scanners found 4,432 vtable methods and 6 RTTI methods, matching the documented
scanner counts. The generated headers/dispatch/imports are emitted by run_lift.py,
not handwritten placeholders. CMake consumes this directory through the original
nfs_guest object target with O2/local registers/X87 window/ThinLTO.

## Protection and testing

Executables, generated C/headers, catalogs, seeds and analysis copies stay in the
private workspace. They must not be committed to the public repository or uploaded
as public CI artifacts. The compiled test APK also stays private in this delivery.

The isolated component checks passed: existing protection, graphics lock/DXT,
COM lifetime/resolution, audio mixer, filesystem and five Velocity CTest checks.
The filesystem fixture initially failed because its temporary root was Android's
/data/local/tmp; only the host test fixture was adjusted to /tmp (Android keeps
its original path). No game/runtime filesystem behavior changed. All 54 built-in
DXVK shaders were generated and validated again.

No Android device/emulator or /dev/kvm is available. Launcher UI, on-device library
load, folder import, installation and gameplay cannot yet be claimed as tested.
Full native link, ABI/ELF/JNI/dependency checks and APK integrity/signature results
will be added after the actual build.

## Generation result

Generation finished in 85 seconds after catalog recovery (242 seconds): 70 C
units, 27,742 functions, 9,002,251 C lines, zero lift failures. Definitions,
declarations, call references, sorted dispatch, 252 imports and all 29 hook
addresses were cross-checked. The generator retains the upstream's known 2,890
3DNow sites (not advertised by virtual CPUID), 17 int traps and configured
exclusion/override boundaries; they were not replaced or silently disabled.

The first full configure passed the generated-source gate, then identified an
uninitialized libadrenotools submodule. The pinned sources were initialized:
8fae8ce254dfc1344527e05301e43f37dea2df80 and nested liblinkernsbypass
 aa3975893d83ef1bc84c321ec60c65fbf1287887. The developer APK helper now initializes
these dependencies when absent. No gitlink revisions were changed.

The host runtime integration target also needed its existing widescreen header
included outside the Android-only guard and widescreen.c added to the test link.
The oversized host generated unit uses the same lossless production splitter.
These are integration/test corrections, not changes to guest game logic.

## Host integration result

The complete generated code was compiled and linked in the existing portable
runtime test. A further pre-existing self-contained-header issue was corrected:
guest_runtime.h now includes cstdint rather than relying on an Android header to
provide uint32_t. After these integration fixes, runtime_check exited 0 using a
private canonical SPEED2.EXE copy from the supplied attachment.

The host mapped the supplied PE, ran heap/copy/scheduler/input/timer/filetime/
critical-section/generated-callback checks and executed the actual lifted CRT
entry, reaching the test's documented Direct3DCreate9 HLE boundary (no host D3D
backend was selected). This is successful incremental runtime bring-up, not
menus/gameplay or Android device validation. The full game folder is absent.

APK compilation includes all 92 production guest units after lossless splitting,
plus DXVK and the six native libraries. The production guest uses local registers,
X87 window, nonvolatile memory, O2, fwrapv, aliasing safeguards and ThinLTO as before.
Guest compilation and full ARM64 link subsequently completed successfully.

## Final APK result

A complete APK was produced by the original Gradle/NDK target, not a launcher-only
or official-library repack. All locally generated guest code is linked with the
modified runtime and DXVK. The incremental repeat build passed in 20 seconds,
reusing the production compilation and ThinLTO cache after the header correction.

| Property | Final private deliverable |
| --- | --- |
| Filename | NFSU2-Project-Velocity-ARM64-v0.14.0-velocity.1.apk |
| Version / code | 0.14.0-velocity.1 / 15 |
| Package | com.nfsu2.androidevolved (unchanged) |
| ABI / minSdk | arm64-v8a / 29 (Android 10+) |
| Bytes | 68,302,087 |
| SHA-256 | 04ee0a6bf4601ad4f0ae66aded5a9ed0f2cf8932ae2d1042f5fe8d80b4225e58 |
| Signature | development Android Debug certificate; APK scheme v2 verified |
| Certificate SHA-256 | a9f5483dbd5a28e74164d47cfbfa224a5532075a40ae3f90ec56602023790b18 |
| Native libraries | Six ELF64 little-endian AArch64 libraries |
| ZIP/signature/alignment | CRC/unique entries, apksigner and zipalign -P 16 checks passed |
| Native load segments | All PT_LOAD segments aligned to at least 16 KiB |
| JNI/native entry | All 17 Java-declared JNI functions and ANativeActivity_onCreate exported |
| Mandatory dynamic imports | All strong imports resolved against packaged dependencies and Android API 29 NDK stubs |
| Game assets/raw executable | Not packaged; game content remains external |

The APK is delivered privately through the chat/workspace with a checksum and
metadata file. It is **not uploaded to the public repository or a public release**;
neither the supplied executable nor derived sources/headers/catalogs are published.
Only open-source tooling fixes and metadata documentation are committed.

A repeat AGP incremental ZIP update initially left ~64.8 MB of an old native
payload as unused archive space. Regenerating only the APK (not C/LTO outputs)
removed that slack. Every live ZIP entry's SHA-256 was compared before/after and
was identical. The compact final package was signature/alignment-verified again.
The helper now removes its generated Debug APK after successful preflight and
before Gradle packaging to avoid stale/oversized repeated packages. It also checks
original lift_stats.json for zero generation failures instead of accepting failed
lift bodies. These are packaging/developer safeguards, not runtime shortcuts.

## Test outcomes and limits

Passed: original generation and cross-file consistency; complete nfs_guest,
libnfsu2_android, DXVK and hook-library compilation/link; all 54 shaders;
existing memory, graphics/COM, PCM audio and filesystem tests; all five Velocity
CTest checks; full generated host runtime bring-up; both applicable APK guard
tests; Debug Java/resources/manifest packaging; final APK integrity, signature,
ABI, ELF alignment, JNI and static dynamic-dependency resolution.

The public-checkout missing-input guard was correctly skipped in the private
workspace after real generation (not reported as a passing absence test).
The Windows-only real-function differential test uses windows.h/MSVC-style SEH
and clang-cl and was not executed on this Linux host; no substitute result is
claimed. No Android device/emulator is available, so launcher startup, actual
on-device dlopen, folder import, installation, graphics/audio/gameplay, save/load
and pause/resume remain unrun. Static NDK-symbol checks do not prove vendor-driver
behavior. No Android FPS or before/after performance gain is measured.

The original supplied EXE SHA-256 was checked again after the build and is
unchanged. Players never run the generator; they install the already compiled
APK and use the original external-game-folder launcher workflow.

The test certificate differs from the official published APK's certificate.
Android therefore cannot update that official installation in place with this
APK despite the unchanged package. Back up saves/settings before uninstalling an
existing differently signed app; no side-by-side package change was introduced.

Remaining known upstream constraints include unsupported GPU Vulkan features
on some Mali/PowerVR devices and unsymbolized runtime crash reports. The Velocity
profiling and experimental toggles are present for real-device diagnosis; they
are not evidence those device-specific issues are fixed.
