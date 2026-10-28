"""Run isolated preview/classic runtime tests without launching the game.

Usage: runtime_suite.py runtime_test.exe classic_runtime_test.exe OUTPUT_DIRECTORY
Keeps its small fixtures under a new output directory for inspection.
"""
import json
import os
from pathlib import Path
import subprocess
import sys


def main():
    preview, classic, directory = sys.argv[1:]
    root = Path(directory).resolve()
    root.mkdir(parents=True, exist_ok=False)
    base = dict(os.environ)
    base.update(SMS_RANDOMIZER_MODE='off', SMS_RANDOMIZER_SEED='1')
    count = 0

    def run(executable, case, folder, **environment):
        nonlocal count
        env = dict(base, SMS_SETTINGS=str(root / folder / 'settings.txt'))
        env.update(environment)
        result = subprocess.run([executable, case], env=env, capture_output=True, text=True)
        if result.returncode:
            raise AssertionError(f'{case}, {environment}: {result.stdout}\n{result.stderr}')
        count += 1

    run(preview, 'off', 'off')
    for mode, seed in [('unknown', '1'), ('entrances', '-1'), ('entrances', '4294967296'),
                       ('entrances', '1x'), ('classic-test', '-1'), ('classic-test', '4294967296'),
                       ('classic', '-1'), ('classic', '4294967296')]:
        run(preview, 'invalid', 'invalid', SMS_RANDOMIZER_MODE=mode, SMS_RANDOMIZER_SEED=seed)
    for executable, mode, version, folder in [
            (preview, 'entrances', 'randomizer-native-v1', 'preview'),
            (classic, 'classic', 'randomizer-classic-test-v1', 'classic')]:
        run(executable, 'valid', folder, SMS_RANDOMIZER_MODE=mode)
        manifest = root / folder / 'saves' / version / '1' / 'entrances.txt'
        before = manifest.read_bytes()
        run(executable, 'valid', folder, SMS_RANDOMIZER_MODE=mode)
        if manifest.read_bytes() != before:
            raise AssertionError('Repeated startup changed the stored seed')
        if mode == 'classic':
            run(executable, 'valid', folder, SMS_RANDOMIZER_MODE='classic-test')
            if manifest.read_bytes() != before:
                raise AssertionError('Legacy test mode changed the classic profile')
        manifest.write_bytes(before + b'modified fixture\n')
        run(executable, 'changed', folder, SMS_RANDOMIZER_MODE=mode)
    for folder, content in [('bad-asset-magic', bytes(32)),
                            ('bad-asset-size', b'J3D2bmd3' + bytes(24))]:
        asset = root / folder / 'rom' / 'randomizer-beta-0.3.6' / 'unlockshine.bmd'
        asset.parent.mkdir(parents=True)
        asset.write_bytes(content)
        run(classic, 'valid', folder, SMS_RANDOMIZER_MODE='classic-test')
    sentinel = root / 'preservation' / 'saves' / 'card-a' / 'original-card-sentinel'
    sentinel.parent.mkdir(parents=True)
    sentinel.write_bytes(b'ordinary save preserved')
    preview_card = root / 'preservation' / 'saves' / 'randomizer-native-v1' / '1' / 'card-a' / 'preview-card-sentinel'
    preview_card.parent.mkdir(parents=True)
    preview_card.write_bytes(b'preview save preserved')
    run(classic, 'valid', 'preservation', SMS_RANDOMIZER_MODE='classic-test')
    if sentinel.read_bytes() != b'ordinary save preserved' or preview_card.read_bytes() != b'preview save preserved':
        raise AssertionError('Classic mode changed another save profile')
    report = dict(cases=count, passed=True, checks=['disabled passthrough', 'input rejection',
        'isolated preview and classic profiles', 'stable manifests', 'changed manifest refusal',
        'original classic first route', 'ability collection and removal', 'flag-based restoration',
        'movement and nozzle eligibility', 'missing and malformed visual assets', 'ordinary and preview save preservation'],
        actual_game_card_reload_validated=False)
    (root / 'results.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    print(json.dumps(report))


if __name__ == '__main__':
    main()
