"""Check the Android archive's architecture and game allocation symbol rewrite."""
from pathlib import Path
import hashlib
import json
import os
import re
import subprocess
import argparse

ROOT = Path(__file__).resolve().parents[2]
SDK = Path(os.environ['LOCALAPPDATA']) / 'Android/Sdk'
BIN = SDK / 'ndk/29.0.14206865/toolchains/llvm/prebuilt/windows-x86_64/bin'
ARCHIVE = ROOT / 'build/android-arm64-game/libsms_game.a'
parser = argparse.ArgumentParser()
parser.add_argument('--output-dir', type=Path, default=ROOT / 'artifacts/android-spc')
args = parser.parse_args()
OUT = args.output_dir.resolve()
OUT.mkdir(parents=True, exist_ok=True)

def output(tool, *args):
    return subprocess.check_output([str(BIN / tool)] + [str(arg) for arg in args],
                                   text=True, encoding='utf-8', errors='replace')

members = output('llvm-ar.exe', 't', ARCHIVE).splitlines()
headers = output('llvm-readelf.exe', '-h', ARCHIVE)
machines = re.findall(r'Machine:\s*(.+)', headers)
assert len(members) >= 588, len(members)
assert len(machines) == len(members), (len(machines), len(members))
assert all(machine.strip() == 'AArch64' for machine in machines), set(machines)
symbols = output('llvm-nm.exe', '--defined-only', '--format=posix', ARCHIVE)
names = {line.split()[0] for line in symbols.splitlines() if line.split()}
assert '__sms_game_new' in names and '__sms_game_new_array' in names
assert '_Znwm' not in names and '_Znam' not in names
with ARCHIVE.open('rb') as stream:
    digest = hashlib.file_digest(stream, 'sha256').hexdigest()
report = dict(archive=str(ARCHIVE), sha256=digest, size=ARCHIVE.stat().st_size,
              members=len(members), architecture='AArch64', game_allocator_rename='PASS',
              scope='Compiled game archive only; not a linked runtime or APK; no gameplay validation')
(OUT / 'archive-members.txt').write_text('\n'.join(members) + '\n')
(OUT / 'archive-validation.json').write_text(json.dumps(report, indent=2) + '\n')
print(json.dumps(report, indent=2))
