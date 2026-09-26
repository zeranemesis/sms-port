"""Print sample marioEU.MAP lines that the live-row regex matches.

Used to confirm exactly which region of the map we are parsing, and whether the
rows are .text symbols or a mixture of sections and data.
"""
import sys
import os

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from map_parse import PAT_LIVE, PAT_UNUSED, MAPF

samples = []
counts = {}
for i, line in enumerate(open(MAPF, 'r', encoding='latin-1', errors='replace')):
    s = line.rstrip('\r\n')
    if PAT_LIVE.match(s):
        counts['live'] = counts.get('live', 0) + 1
        if len(samples) < 40 and counts['live'] <= 200000:
            samples.append((i + 1, s))
    elif PAT_UNUSED.match(s):
        counts['unused'] = counts.get('unused', 0) + 1

print(counts)
# show a spread of the matched live lines
for idx in range(0, len(samples), 4):
    ln, s = samples[idx]
    print('%6d  %s' % (ln, s))

print('\n--- lines around first live match ---')
first = samples[0][0]
all_lines = open(MAPF, 'r', encoding='latin-1', errors='replace').readlines()
for l in range(first - 8, first + 6):
    print('%6d|%s' % (l + 1, all_lines[l].rstrip('\r\n')))
