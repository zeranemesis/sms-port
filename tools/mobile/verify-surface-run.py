"""Record the most recent completed renderer diagnostic, with APK identity."""
from pathlib import Path
import argparse
import hashlib
import json
import os
import re
import subprocess

ROOT = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser()
parser.add_argument('--serial', default='emulator-5580')
args = parser.parse_args()
ADB = Path(os.environ['LOCALAPPDATA']) / 'Android/Sdk/platform-tools/adb.exe'
OUT = ROOT / 'artifacts/android-aurora/surface-validation'
OUT.mkdir(parents=True, exist_ok=True)
PACKAGE = 'org.smsunified.surface'


def adb(*command):
    return subprocess.check_output([str(ADB), '-s', args.serial, *command], text=True,
                                   encoding='utf-8', errors='replace').strip()


build = json.loads((ROOT / 'build/android-surface-app/build.json').read_text(encoding='utf-8'))
bridge = Path(build['libraries']['libsms_aurora.so']['source'])
assert hashlib.sha256(bridge.read_bytes()).hexdigest() == build['libraries']['libsms_aurora.so']['sha256'], 'Bridge changed after packaging'
readelf = Path(os.environ['LOCALAPPDATA']) / 'Android/Sdk/ndk/29.0.14206865/toolchains/llvm/prebuilt/windows-x86_64/bin/llvm-readelf.exe'
symbols = subprocess.check_output([str(readelf), '--dyn-syms', '--wide', str(bridge)], text=True)
exports = []
for line in symbols.splitlines():
    fields = line.split()
    if len(fields) >= 8 and fields[4] in ('GLOBAL', 'WEAK') and fields[6] != 'UND':
        exports.append(fields[7])
jni = 'Java_org_smsunified_mobile_AuroraActivity_nativeSurfaceReady'
assert jni in exports and 'sab_init' in exports, 'Missing Android bridge ABI'
assert all(name.startswith('sab_') or name == jni for name in exports), 'Unexpected bridge export/GX interposition risk'
(OUT / 'bridge-exports.txt').write_text('\n'.join(sorted(exports)) + '\n', encoding='utf-8')
apk = Path(build['apk'])
expected = hashlib.sha256(apk.read_bytes()).hexdigest()
assert expected == build['sha256'], 'APK changed since packaging'
remote = adb('shell', 'pm', 'path', PACKAGE).splitlines()[0].removeprefix('package:')
installed = adb('shell', 'sha256sum', remote).split()[0]
assert installed == expected, 'Installed APK differs from the recorded build'
full_result = adb('shell', 'run-as', PACKAGE, 'cat', 'files/surface-result.txt')
assert 'START ' in full_result, 'No diagnostic start recorded'
result = 'START ' + full_result.rsplit('START ', 1)[1]
requirements = ['PASS real GX API/FIFO:', 'PASS SDL3 PCM:',
                'PASS Aurora initialized with Vulkan and SDL Android surface',
                'PASS Vulkan surface: 90 submitted frames,']
assert all(message in result for message in requirements), result
assert 'FAIL' not in result, result
activity_logs = adb('logcat', '-d', '-s', 'SunshineSurface', '-v', 'brief')
pids = re.findall(r'SunshineSurface\(\s*(\d+)\): START ', activity_logs)
assert pids, 'Cannot associate the result with a process log'
logs = adb('logcat', '-d', '--pid=' + pids[-1], '-v', 'threadtime')
assert 'Device: llvmpipe' in logs, 'This proof expects the documented Lavapipe emulator'
assert 'Fatal signal' not in logs, 'The diagnostic process crashed'
package = adb('shell', 'dumpsys', 'package', PACKAGE)
assert 'primaryCpuAbi=arm64-v8a' in package, 'ARM64 process not confirmed'
(OUT / 'result.txt').write_text(result + '\n', encoding='utf-8')
(OUT / 'logcat.txt').write_text(logs + '\n', encoding='utf-8')
(OUT / 'build.json').write_text(json.dumps(build, indent=2) + '\n', encoding='utf-8')
report = dict(result='PASS', apk_sha256=expected, installed_sha256=installed,
              serial=args.serial, boot_completed=adb('shell', 'getprop', 'sys.boot_completed'),
              device_abis=adb('shell', 'getprop', 'ro.product.cpu.abilist'),
              translator=adb('shell', 'getprop', 'ro.dalvik.vm.native.bridge'),
              process_abi='arm64-v8a', pid=pids[-1], driver='Lavapipe/llvmpipe CPU Vulkan',
              submitted_frames=90,
              bridge_export_count=len(exports), bridge_exports='sab_* plus explicit Android surface JNI; GX hidden',
              validated=['real GX texture API/FIFO address transport', 'SDL3 PCM callback lifecycle',
                         'Vulkan device and Android SDL surface', 'frame submission', 'shutdown return'],
              not_validated=['game execution', 'rendered scene pixels', 'GPU colour/depth copy correctness',
                             'nonempty shader cache precompilation', 'surface recreation',
                             'game DSP/audio synchronization', 'phone performance', 'iOS'])
(OUT / 'validation.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
print(json.dumps(report, indent=2))
