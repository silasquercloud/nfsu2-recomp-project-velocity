#!/usr/bin/env python3
"""Upload an existing verified APK to a PRIVATE GitHub draft release.

Uses Actions to avoid a cloud proxy that authenticates api.github.com/git but not
uploads.github.com. Only a public certificate and encrypted APK transit through
a temporary branch. Its single-use decryption key stays on the ephemeral runner.
No game sources, executables, assets, or app signing keys are transmitted.
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import time


def run(args, cwd=None, capture=False):
    return subprocess.run(args,cwd=cwd,check=True,text=True,
                          stdout=subprocess.PIPE if capture else None).stdout


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('apk',type=Path)
    parser.add_argument('--version',default='0.14.0-velocity.2')
    parser.add_argument('--public',action='store_true',help='Publish experimental release only after verified draft attachment')
    parser.add_argument('--repo',default='silasquercloud/nfsu2-recomp-project-velocity')
    args=parser.parse_args()
    apk=args.apk.resolve()
    expected='NFSU2-Project-Velocity-ARM64-v'+args.version+('-ptBR' if args.version=='0.14.0-velocity.2' else '')+'.apk'
    if apk.name!=expected:
        parser.error('Expected the complete verified versioned Velocity ARM64 APK')
    root=Path(__file__).resolve().parents[1]
    source=run(['git','rev-parse','HEAD'],root,True).strip()
    with apk.open('rb') as stream:
        digest=hashlib.file_digest(stream,'sha256').hexdigest()
    branch='release-private-upload/'+args.version+'-'+digest[:10]
    transport=root/'build/private-release-upload'/digest[:10]
    if transport.exists():parser.error('Transport already exists; inspect its run before retrying')
    transport.mkdir(parents=True)
    (transport/'.github/workflows').mkdir(parents=True)
    workflow='''name: Upload private Velocity APK
on:
  push:
    branches: [BRANCH]
    paths: ['.github/workflows/private-upload.yml']
permissions:
  contents: write
jobs:
  upload:
    runs-on: ubuntu-24.04
    timeout-minutes: 20
    steps:
      - uses: actions/checkout@v4
      - name: Receive encrypted APK and attach it to a private draft
        env:
          GH_TOKEN: ${{ github.token }}
          GH_REPO: ${{ github.repository }}
        run: |
          set -euo pipefail
          umask 077
          keyfile=$(mktemp)
          clearapk=/tmp/APKNAME
          trap 'rm -f "$keyfile" "$clearapk"' EXIT
          openssl req -x509 -newkey rsa:3072 -nodes -keyout "$keyfile" -out transport-cert.pem -days 1 -subj /CN=Velocity-One-Time-Transport 2>/dev/null
          git config user.name 'Project Velocity Artifact Transport'
          git config user.email 'project-velocity-build@users.noreply.github.com'
          git add transport-cert.pem
          git commit -m 'transport: publish single-use encryption certificate'
          git push origin HEAD:BRANCH
          received=false
          for attempt in $(seq 1 150); do
            git fetch --quiet origin BRANCH
            if git cat-file -e FETCH_HEAD:payload.cms 2>/dev/null; then
              git show FETCH_HEAD:payload.cms > /tmp/payload.cms
              received=true
              break
            fi
            sleep 5
          done
          test "$received" = true
          openssl cms -decrypt -binary -inform DER -in /tmp/payload.cms -recip transport-cert.pem -inkey "$keyfile" -out "$clearapk"
          printf '%s  %s\\n' DIGEST "$clearapk" | sha256sum --check
          test "$(stat -c %s "$clearapk")" = APKSIZE
          cat > /tmp/notes.md <<'NOTES'
          Private development/test APK: Project Velocity ARM64 VELOCITYVERSION, Android 10+. New HUD, two landscape directions, persistent Vulkan pipeline cache and optional profiling. Existing language resources preserved.
          Initially uploaded as a private draft; publication depends on explicit --public developer instruction.
          ARM64 Release build with development signature matching Velocity .1. Gameplay/FPS on Adreno 610, Mali G52 and other physical devices remain unverified.
          SHA-256: DIGEST
          Report: https://github.com/REPOSITORY/blob/feature/project-velocity-arm64/docs/PROJECT_VELOCITY_V3_REPORT.md
          NOTES
          gh release create vVELOCITYVERSION "$clearapk" --draft --prerelease --target SOURCECOMMIT --title 'Project Velocity ARM64 VELOCITYVERSION — Universal Performance Overhaul' --notes-file /tmp/notes.md
'''
    replacements={'BRANCH':branch,'APKNAME':apk.name,'DIGEST':digest,'APKSIZE':str(apk.stat().st_size),
                  'REPOSITORY':args.repo,'SOURCECOMMIT':source,'VELOCITYVERSION':args.version}
    for key,value in replacements.items():workflow=workflow.replace(key,value)
    (transport/'.github/workflows/private-upload.yml').write_text(workflow)
    (transport/'README.md').write_text('Temporary encrypted transport to a private GitHub draft release. No plaintext APK or private key is committed.\n')
    run(['git','init','--initial-branch='+branch],transport)
    run(['git','config','user.name','Project Velocity Build'],transport)
    run(['git','config','user.email','project-velocity-build@users.noreply.github.com'],transport)
    run(['git','remote','add','origin','https://github.com/'+args.repo+'.git'],transport)
    run(['git','add','README.md','.github/workflows/private-upload.yml'],transport)
    run(['git','commit','-m','release: prepare private encrypted APK upload'],transport)
    run(['git','push','origin','HEAD'],transport)
    deadline=time.monotonic()+300
    while time.monotonic()<deadline:
        run(['git','fetch','--quiet','origin',branch],transport)
        found=subprocess.run(['git','cat-file','-e','FETCH_HEAD:transport-cert.pem'],cwd=transport,
                             stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL).returncode==0
        if found:break
        time.sleep(5)
    else:raise RuntimeError('Actions did not publish its certificate; inspect the upload run')
    run(['git','reset','--hard','FETCH_HEAD'],transport)
    run(['openssl','cms','-encrypt','-binary','-aes-256-cbc','-in',str(apk),'-out','payload.cms',
         '-outform','DER','transport-cert.pem'],transport)
    run(['git','add','payload.cms'],transport)
    run(['git','commit','-m','transport: send encrypted verified APK to private release runner'],transport)
    run(['git','push','origin','HEAD'],transport)
    print('Encrypted transport sent. Waiting for Actions (APK remains a private draft).',flush=True)
    deadline=time.monotonic()+300
    runs=[]
    while time.monotonic()<deadline:
        runs=json.loads(run(['gh','run','list','--repo',args.repo,'--branch',branch,'--limit','1',
                            '--json','databaseId,status,conclusion,url'],capture=True))
        if runs and runs[0]['status']=='completed':break
        time.sleep(5)
    if not runs or runs[0]['conclusion']!='success':raise RuntimeError('Private upload workflow failed or timed out: '+str(runs))
    releases=json.loads(run(['gh','api','repos/'+args.repo+'/releases'],capture=True))
    release=next(x for x in releases if x['tag_name']=='v'+args.version)
    assert release['draft'],'Refuse an unexpectedly public release'
    asset=next(x for x in release['assets'] if x['name']==apk.name)
    assert asset['size']==apk.stat().st_size
    assert asset.get('digest')=='sha256:'+digest
    if args.public:
        release=json.loads(run(['gh','api','--method','PATCH','repos/'+args.repo+'/releases/'+str(release['id']),'-F','draft=false','-F','prerelease=true'],capture=True))
        assert not release['draft']
        asset=next(x for x in release['assets'] if x['name']==apk.name)
    # Remove our temporary encrypted transport ref after the verified private upload.
    run(['git','push','origin','--delete',branch],transport)
    result={'release_url':release['html_url'],'asset_url':asset['browser_download_url'],
            'asset_api_url':asset['url'],'draft':release['draft'],'bytes':asset['size'],'sha256':digest,'run':runs[0]['url']}
    (transport/'delivery.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(result,indent=2))

if __name__=='__main__':main()
