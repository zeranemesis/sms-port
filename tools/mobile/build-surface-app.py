"""Package the real ARM64 renderer diagnostic with its ELF dependency closure.

No game data and no SMS_main entry are included. Native builds are separate.
"""
from pathlib import Path
import hashlib
import json
import os
import re
import shutil
import subprocess
import zipfile
import argparse

parser = argparse.ArgumentParser()
parser.add_argument('--game', action='store_true', help='Package the actual game runtime instead of the surface test')
args = parser.parse_args()

ROOT = Path(__file__).resolve().parents[2]
SOURCE = Path(__file__).resolve().parent
SDK = Path(os.environ['LOCALAPPDATA']) / 'Android/Sdk'
NDK = SDK / 'ndk/29.0.14206865/toolchains/llvm/prebuilt/windows-x86_64'
JAVA = Path(os.environ.get('JAVA_HOME', 'C:/Program Files/Android/Android Studio/jbr'))
TOOLS = SDK / 'build-tools/36.1.0'
ANDROID = SDK / 'platforms/android-35/android.jar'
AURORA = ROOT / 'build/android-arm64-aurora'
SDL = AURORA / '_deps/sdl-src'
ENTRY = ROOT / ('build/android-arm64-game/libsms.so' if args.game else 'build/android-arm64-game/libsms_mobile_surface_probe.so')
OUT = ROOT / ('build/android-game-app' if args.game else 'build/android-surface-app')
OUT.mkdir(parents=True, exist_ok=True)
env = dict(os.environ, JAVA_HOME=str(JAVA))


def run(*args):
    subprocess.run([str(arg) for arg in args], cwd=ROOT, env=env, check=True)


# Only public Android NDK libraries may remain outside the APK. All other
# DT_NEEDED entries must resolve to this Android build, never a host library.
SYSTEM = {'libc.so', 'libm.so', 'libdl.so', 'liblog.so', 'libandroid.so',
          'libz.so', 'libEGL.so', 'libGLESv1_CM.so', 'libGLESv2.so',
          'libvulkan.so', 'libOpenSLES.so', 'libaaudio.so', 'libjnigraphics.so'}
available = {}
for library in sorted(AURORA.rglob('*.so')) + [ENTRY, NDK / 'sysroot/usr/lib/aarch64-linux-android/libc++_shared.so']:
    if not library.is_file():
        raise FileNotFoundError(library)
    old = available.get(library.name)
    if old and old.read_bytes() != library.read_bytes():
        raise RuntimeError(f'Ambiguous Android library: {old}, {library}')
    available[library.name] = library
closure = {}
pending = [ENTRY.name]
while pending:
    name = pending.pop()
    if name in closure or name in SYSTEM:
        continue
    if name not in available:
        raise RuntimeError(f'Unpackaged native dependency: {name}')
    path = available[name]
    header = subprocess.check_output([str(NDK / 'bin/llvm-readelf.exe'), '-h', str(path)], text=True)
    if 'AArch64' not in header:
        raise RuntimeError(f'Non-ARM64 library: {path}')
    dynamic = subprocess.check_output([str(NDK / 'bin/llvm-readelf.exe'), '-d', str(path)], text=True)
    needed = re.findall(r'\(NEEDED\).*?\[(.*?)\]', dynamic)
    closure[name] = dict(source=str(path), needed=needed, sha256=hashlib.sha256(path.read_bytes()).hexdigest())
    pending.extend(needed)

classes, dex = OUT / 'classes', OUT / 'dex'
classes.mkdir(exist_ok=True)
dex.mkdir(exist_ok=True)
# Do not retain Java classes from an earlier UI implementation in the APK.
if classes.resolve().parent != OUT.resolve():
    raise RuntimeError('Java output escaped the APK build directory')
for stale_class in classes.rglob('*.class'):
    stale_class.unlink()
java_sources = sorted((SDL / 'android-project/app/src/main/java/org/libsdl/app').glob('*.java'))
if not java_sources:
    raise RuntimeError('Pinned SDL3 Java sources missing')
app_sources = sorted((SOURCE / 'android').rglob('*.java')) if args.game else [SOURCE / 'android/org/smsunified/mobile/AuroraActivity.java', SOURCE / 'surface-app/SurfaceActivity.java']
run(JAVA / 'bin/javac.exe', '-encoding', 'UTF-8', '-source', '8', '-target', '8',
    '-bootclasspath', str(TOOLS / 'core-lambda-stubs.jar') + os.pathsep + str(ANDROID),
    '-d', classes, *java_sources, *app_sources)
jar = OUT / 'activity.jar'
with zipfile.ZipFile(jar, 'w') as archive:
    for path in classes.rglob('*.class'):
        archive.write(path, path.relative_to(classes).as_posix())
run(TOOLS / 'd8.bat', '--min-api', '28', '--lib', ANDROID, '--output', dex, jar)
unsigned = OUT / 'surface-unsigned.apk'
run(TOOLS / 'aapt.exe', 'package', '-f', '-M', SOURCE / ('game-app/AndroidManifest.xml' if args.game else 'surface-app/AndroidManifest.xml'),
    '-I', ANDROID, '-F', unsigned)
native = OUT / 'native'
native.mkdir(exist_ok=True)
with zipfile.ZipFile(unsigned, 'a', compression=zipfile.ZIP_DEFLATED) as archive:
    archive.write(dex / 'classes.dex', 'classes.dex')
    if args.game:
        # Portable pipeline descriptions are compiled for the phone's own GPU
        # before gameplay. Never bundle a driver-specific dawn_cache.db.
        seed = ROOT / 'res/mobile/initial_pipeline_cache.db'
        if seed.is_file():
            archive.write(seed, 'assets/initial_pipeline_cache.db')
        # Reuse the PartyBoard styles/fonts already shared with Mario Party 4.
        # The optional proprietary NewRodin font is never bundled.
        for resource in sorted((ROOT / 'res/partyboard').iterdir()):
            if resource.is_file() and resource.suffix.lower() != '.otf':
                archive.write(resource, 'assets/partyboard/' + resource.name)
    for name, metadata in sorted(closure.items()):
        # Keep original build symbols; strip only the APK copy.
        target = native / name
        shutil.copy2(metadata['source'], target)
        run(NDK / 'bin/llvm-strip.exe', '--strip-debug', target)
        metadata['apk_sha256'] = hashlib.sha256(target.read_bytes()).hexdigest()
        archive.write(target, f'lib/arm64-v8a/{name}')
aligned = OUT / 'surface-aligned.apk'
run(TOOLS / 'zipalign.exe', '-f', '-P', '16', '4', unsigned, aligned)
key = ROOT / 'build/android-diagnostic-app/diagnostic-debug.keystore'
if not key.is_file():
    raise RuntimeError('Build the memory diagnostic first to create the local debug key')
apk = OUT / ('Sunshine-Aurora-arm64-v8a.apk' if args.game else 'Sunshine-Aurora-Surface-arm64-v8a.apk')
run(TOOLS / 'apksigner.bat', 'sign', '--ks', key, '--ks-pass', 'pass:android',
    '--key-pass', 'pass:android', '--out', apk, aligned)
run(TOOLS / 'apksigner.bat', 'verify', apk)
run(TOOLS / 'zipalign.exe', '-c', '-P', '16', '4', apk)
manifest = dict(apk=str(apk), sha256=hashlib.sha256(apk.read_bytes()).hexdigest(),
                abi='arm64-v8a', libraries=closure, system_libraries=sorted(SYSTEM),
                purpose=('Game runtime, user-imported data required; gameplay validation separate' if args.game else 'Real GX/FIFO, SDL3 PCM lifecycle and Aurora Vulkan surface; no game assets'))
(OUT / 'build.json').write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')
print(json.dumps(dict(apk=str(apk), sha256=manifest['sha256'], native_libraries=len(closure)), indent=2))
