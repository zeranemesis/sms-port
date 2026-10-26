"""Build the diagnostic APK using the installed SDK, without Gradle downloads."""
from pathlib import Path
import hashlib
import json
import os
import subprocess
import zipfile
import argparse

parser = argparse.ArgumentParser()
parser.add_argument('--abi', choices=('both', 'arm64-v8a', 'x86_64'), default='both')
args = parser.parse_args()
ABIS = ['arm64-v8a', 'x86_64'] if args.abi == 'both' else [args.abi]

ROOT = Path(__file__).resolve().parents[2]
SOURCE = Path(__file__).resolve().parent
SDK = Path(os.environ['LOCALAPPDATA']) / 'Android/Sdk'
JAVA = Path(os.environ.get('JAVA_HOME', 'C:/Program Files/Android/Android Studio/jbr'))
TOOLS = SDK / 'build-tools/36.1.0'
ANDROID = SDK / 'platforms/android-35/android.jar'
OUT = ROOT / 'build/android-diagnostic-app'
OUT.mkdir(parents=True, exist_ok=True)
env = dict(os.environ, JAVA_HOME=str(JAVA))

def run(*args):
    subprocess.run([str(arg) for arg in args], cwd=ROOT, env=env, check=True)

for abi in ABIS:
    run('powershell.exe', '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File',
        SOURCE / 'build-probe.ps1', '-Abi', abi)

classes = OUT / 'classes'
dex = OUT / 'dex'
classes.mkdir(exist_ok=True)
dex.mkdir(exist_ok=True)
run(JAVA / 'bin/javac.exe', '-encoding', 'UTF-8', '-source', '8', '-target', '8',
    '-bootclasspath', str(TOOLS / 'core-lambda-stubs.jar') + os.pathsep + str(ANDROID),
    '-d', classes, SOURCE / 'app/ProbeActivity.java')
jar = OUT / 'activity.jar'
with zipfile.ZipFile(jar, 'w') as archive:
    for path in classes.rglob('*.class'):
        archive.write(path, path.relative_to(classes).as_posix())
run(TOOLS / 'd8.bat', '--min-api', '28', '--lib', ANDROID, '--output', dex, jar)
unsigned = OUT / 'diagnostic-unsigned.apk'
run(TOOLS / 'aapt.exe', 'package', '-f', '-M', SOURCE / 'app/AndroidManifest.xml',
    '-I', ANDROID, '-F', unsigned)
with zipfile.ZipFile(unsigned, 'a', compression=zipfile.ZIP_DEFLATED) as archive:
    archive.write(dex / 'classes.dex', 'classes.dex')
    for abi, folder in [('arm64-v8a', 'android-arm64-probe'), ('x86_64', 'android-x86_64-probe')]:
        if abi not in ABIS:
            continue
        archive.write(ROOT / 'build' / folder / 'libsms_mobile_probe_jni.so',
                      f'lib/{abi}/libsms_mobile_probe_jni.so')
aligned = OUT / 'diagnostic-aligned.apk'
run(TOOLS / 'zipalign.exe', '-f', '4', unsigned, aligned)
key = OUT / 'diagnostic-debug.keystore'
if not key.exists():
    run(JAVA / 'bin/keytool.exe', '-genkeypair', '-keystore', key, '-storepass', 'android',
        '-keypass', 'android', '-alias', 'androiddebugkey', '-dname', 'CN=Android Debug,O=Android,C=US',
        '-keyalg', 'RSA', '-keysize', '2048', '-validity', '10000')
suffix = '' if args.abi == 'both' else '-' + args.abi
apk = OUT / ('Sunshine-Diagnostic' + suffix + '.apk')
run(TOOLS / 'apksigner.bat', 'sign', '--ks', key, '--ks-pass', 'pass:android',
    '--key-pass', 'pass:android', '--out', apk, aligned)
run(TOOLS / 'apksigner.bat', 'verify', apk)
run(TOOLS / 'zipalign.exe', '-c', '4', apk)
manifest = dict(apk=str(apk), sha256=hashlib.sha256(apk.read_bytes()).hexdigest(),
                abis=ABIS, signature='local debug key',
                purpose='Runtime memory diagnostic in Android application; no game assets')
(OUT / ('build' + suffix + '.json')).write_text(json.dumps(manifest, indent=2) + '\n')
print(json.dumps(manifest, indent=2))
