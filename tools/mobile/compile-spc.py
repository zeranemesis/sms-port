"""Compile the five adapted game units without linking or touching PC binaries."""
from pathlib import Path
import json
import os
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'artifacts/android-spc'
SDK = Path(os.environ['LOCALAPPDATA']) / 'Android/Sdk'
LLVM = SDK / 'ndk/29.0.14206865/toolchains/llvm/prebuilt/windows-x86_64'
commands = json.loads((ROOT / 'build/windows-64-pal-aurora/compile_commands.json').read_text())
names = json.loads((OUT / 'bindings.json').read_text())
results = []
for name in names:
    command = next(entry for entry in commands if entry['file'].replace('\\', '/').endswith('/' + name))
    tokens = shlex.split(command['command'], posix=False)[1:]
    args = []
    i = 0
    while i < len(tokens):
        token = tokens[i].strip('"')
        if token in ('-o', '-c'):
            i += 2
            continue
        if token not in ('-O2', '-g'):
            args.append(token)
        i += 1
    # The generated Android patch mirror shadows the prior PC mirror.
    prefix = ['-I' + str(OUT / 'patched/include'), '-I' + str(ROOT / 'platform/mobile')]
    source = OUT / 'patched' / name
    for target in ('windows', 'android-arm64'):
        folder = OUT / target
        folder.mkdir(exist_ok=True)
        obj = folder / (name.replace('/', '_') + '.o')
        if target == 'windows':
            compiler = Path('C:/msys64/mingw64/bin/g++.exe')
            selected = args
            extra = []
        else:
            compiler = LLVM / 'bin/clang++.exe'
            excluded = {'-m64', '-msse2', '-mfpmath=sse', '-fno-pie', '-fno-unreachable-traps',
                        '-finput-charset=UTF-8', '-fexec-charset=CP932'}
            selected = [arg for arg in args if arg not in excluded]
            extra = ['--target=aarch64-linux-android28', '--sysroot=' + str(LLVM / 'sysroot'),
                     '-fPIC', '-fms-extensions', '-Wno-invalid-source-encoding']
        cmd = [str(compiler)] + prefix + selected + extra + ['-O0', '-g0', '-c', str(source), '-o', str(obj)]
        env = dict(os.environ, PATH='C:/msys64/mingw64/bin;C:/msys64/usr/bin;' + os.environ['PATH'])
        run = subprocess.run(cmd, cwd=ROOT, env=env, capture_output=True)
        (folder / (obj.stem + '.log')).write_bytes(run.stdout + run.stderr)
        results.append(dict(source=name, target=target, exit_code=run.returncode,
                            object=str(obj), command=cmd))
        print(target, name, run.returncode, flush=True)
(OUT / 'compile-results.json').write_text(json.dumps(results, indent=2) + '\n')
raise SystemExit(any(result['exit_code'] for result in results))
