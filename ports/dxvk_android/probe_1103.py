"""Reproduce the DXVK 1.10.3 Android porting blocker; never builds a usable backend.

Inputs: official DXVK git checkout containing the pinned tag, existing prepared
native headers, Android NDK. Output is a disposable source tree and compiler log.
Returns the actual compiler status (currently failure), not a passing mock test.
"""
import argparse,io,pathlib,shutil,subprocess,tarfile
PIN='57740c6ac9c62f3f26c4b5237d7edf3c40797941'
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('official',type=pathlib.Path);p.add_argument('prepared_native',type=pathlib.Path)
p.add_argument('ndk',type=pathlib.Path);p.add_argument('output',type=pathlib.Path)
a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
revision=subprocess.check_output(['git','-C',str(a.official),'rev-parse','v1.10.3'],text=True).strip()
if revision!=PIN:raise SystemExit('Official v1.10.3 revision mismatch')
archive=subprocess.check_output(['git','-C',str(a.official),'archive',PIN])
with tarfile.open(fileobj=io.BytesIO(archive)) as t:t.extractall(a.output,filter='data')
# These three existing ARM portability headers remove x86 intrinsic errors only.
# They do not port Windows WSI, monitor enumeration, threading or cursor behavior.
for name in ('util_bit.h','util_vector.h','sync/sync_spinlock.h'):
 shutil.copy2(a.prepared_native/'src/util'/name,a.output/'src/util'/name)
compiler=a.ndk/'toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android29-clang++'
command=[str(compiler),'-std=c++17','-fsyntax-only','-ferror-limit=12','-Wno-deprecated-declarations','-DDXVK_NATIVE','-DDXVK_PLATFORM_LINUX']
for directory in [a.output/'include']+[a.prepared_native/'include'/x for x in ('native','native/windows','native/directx')]:command+=['-I'+str(directory.resolve())]
command+=[str(a.output/'src/d3d9/d3d9_swapchain.cpp')]
result=subprocess.run(command,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
(a.output/'android-probe.log').write_text('Official commit: '+PIN+'\nCommand: '+repr(command)+'\n'+result.stdout)
print(result.stdout);print('Compiler status:',result.returncode,'; log:',a.output/'android-probe.log')
raise SystemExit(result.returncode)
