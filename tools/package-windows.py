#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Bundle the Windows build with its transitive DLLs, OCR data and notices."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import uuid
import zipfile


def run(arguments, **kwargs):
    return subprocess.run([str(x) for x in arguments], check=True, **kwargs)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('source', 'build', 'prefix', 'output'):
        parser.add_argument('--' + name, required=True, type=Path)
    args = parser.parse_args()
    version = re.search(r'project\(hover_translate VERSION ([0-9.]+)', (args.source/'CMakeLists.txt').read_text()).group(1)
    args.output.mkdir(parents=True, exist_ok=True)
    stage = args.output/'staging'/('package-' + uuid.uuid4().hex[:8])
    stage.mkdir(parents=True)
    binary = args.prefix/'bin'
    os.environ['PATH'] = str(binary) + os.pathsep + os.environ.get('PATH', '')
    run([binary/'cmake.exe','--install',args.build,'--prefix',stage])
    deploy = binary/'windeployqt6.exe'
    if not deploy.is_file(): deploy = binary/'windeployqt.exe'
    run([deploy,'--release','--no-translations','--no-compiler-runtime',stage/'hover-translate.exe'])
    # Include all dependencies of the executable, Qt plugins and bundled DLLs.
    available = {p.name.lower():p for p in binary.glob('*.dll')}
    seen = set()
    pending = [stage/'hover-translate.exe', *stage.rglob('*.dll')]
    while pending:
        path = pending.pop()
        if path.name.lower() in seen: continue
        seen.add(path.name.lower())
        report = run([binary/'objdump.exe','-p',path],capture_output=True,text=True,encoding='utf-8').stdout
        for name in re.findall(r'DLL Name:\s*(\S+)', report):
            dependency = available.get(name.lower())
            if dependency is not None and name.lower() not in seen:
                target = stage/dependency.name
                if not target.exists(): shutil.copy2(dependency,target)
                pending.append(target)
    tessdata=stage/'tessdata'
    tessdata.mkdir()
    for name in ('eng.traineddata','chi_sim.traineddata'):
        shutil.copy2(args.prefix/'share/tessdata'/name,tessdata/name)
    licenses=args.prefix/'share/licenses'
    if licenses.is_dir(): shutil.copytree(licenses,stage/'third-party-licenses')
    # A package must work without the compiler/Qt SDK on PATH.
    environment = dict(os.environ)
    system = Path(os.environ.get('SystemRoot', r'C:\Windows'))
    environment['PATH'] = str(system/'System32') + os.pathsep + str(system)
    environment['QT_QPA_PLATFORM'] = 'offscreen'
    diagnostic = run([stage/'hover-translate.exe','--version'],capture_output=True,
                     text=True,encoding='utf-8',timeout=15,env=environment,cwd=stage)
    if diagnostic.stdout.strip() != f'HoverTranslate {version}':
        raise RuntimeError('The packaged executable did not report its expected version.')
    archive=args.output/f'hover-translate-{version}-windows-x64.zip'
    if archive.exists(): raise FileExistsError(f'Refusing to overwrite {archive}')
    with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as zipped:
        for path in sorted(stage.rglob('*')):
            if path.is_file(): zipped.write(path,Path(f'HoverTranslate-{version}')/path.relative_to(stage))
    print(json.dumps({'archive':str(archive),'stage':str(stage),'sha256':hashlib.sha256(archive.read_bytes()).hexdigest()},indent=2))


if __name__ == '__main__': main()
