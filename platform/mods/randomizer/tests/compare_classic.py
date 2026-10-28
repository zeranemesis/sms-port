"""Compare complete native seeds with the original generator, using 80 cases.

Usage: compare_classic.py Randomizer.kxe Sunshine-US.iso rules_probe.exe classic_probe.exe
The reference executes the original PPC generator with an archive fixture;
it does not run mission scripts, collection hooks, rendering or the save system.
"""
import json
import subprocess
import sys

from original_seed import generate, reference_scenes


def main():
    module, iso, rules, native = sys.argv[1:]
    scenes = reference_scenes(rules)
    if len(scenes) != 110:
        raise AssertionError('Expected the original 110-scene archive fixture')
    seeds = list(range(32)) + [0xffff, 0x10000, 0x7fffffff, 0x80000000, 0xfffffffe, 0xffffffff]
    random = 0x534d5352
    for _ in range(26):
        random = (random * 1664525 + 1013904223) & 0xffffffff
        seeds.append(random)
    cases = [(seed, False, False) for seed in seeds] + [
        (seed, True, False) for seed in (0, 1, 2, 3, 42, 12345, 0x80000000, 0xffffffff)]
    cases += [(seed, all_unlocks, True) for seed in (0, 1, 42, 0xffffffff) for all_unlocks in (False, True)]
    for index, (seed, all_unlocks, pal) in enumerate(cases):
        archives = sorted(scenes + [0x0c01]) if pal else scenes
        original = generate(module, iso, archives, seed, start_with_all_unlocks=all_unlocks)
        flags = 'pal-all' if pal and all_unlocks else 'pal' if pal else 'all' if all_unlocks else ''
        command = [native, str(seed)] + ([flags] if flags else [])
        output = subprocess.run(command, capture_output=True, text=True, check=True)
        routes, rewards = [], []
        for line in output.stdout.splitlines():
            kind, a, b = line.split()
            if kind == 'route':
                routes.append((int(a), int(b)))
            elif kind == 'reward' and int(a) == len(rewards):
                rewards.append(int(b))
            else:
                raise AssertionError('Invalid native seed probe output')
        if len(routes) != 110 or len(rewards) != 120:
            raise AssertionError('Incomplete native seed output')
        if routes != original['routes']:
            mismatch = next((i, a, b) for i, (a, b) in enumerate(zip(original['routes'], routes)) if a != b)
            raise AssertionError(f'Seed {seed}: original/native route mismatch {mismatch}')
        if rewards != original['rewards']:
            mismatch = next((i, a, b) for i, (a, b) in enumerate(zip(original['rewards'], rewards)) if a != b)
            raise AssertionError(f'Seed {seed}: original/native reward mismatch {mismatch}')
        if len({a for a, b in routes}) != 110 or len({b for a, b in routes}) != 110:
            raise AssertionError(f'Seed {seed}: non-bijective routes')
        placed = [reward for reward in rewards if reward]
        initial, count = (0x7fffff, 0) if all_unlocks else (0x400, 22)
        if len(placed) != count or len(set(placed)) != count or (sum(placed) | initial) != 0x7fffff:
            raise AssertionError(f'Seed {seed}: invalid ability placement')
        if (index + 1) % 8 == 0:
            print(f'{index + 1}/{len(cases)} complete seeds matched', file=sys.stderr, flush=True)
    print(json.dumps(dict(cases=len(cases), progressive_seeds=len(seeds), all_unlocks_start_seeds=8,
                          pal_archive_cases=8,
                          routes_per_seed=110, rewards_compared_per_seed=120,
                          matched_original=True, scope='complete generator; archive fixture and retail service stubs',
                          full_mod_gameplay_validated=False)))


if __name__ == '__main__':
    main()
