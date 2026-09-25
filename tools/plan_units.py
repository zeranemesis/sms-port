#!/usr/bin/env python3
"""Analyze build/GMSP01/report.json: rank game units by % matched / fuzzy."""
import json
import sys

with open('build/GMSP01/report.json') as f:
    data = json.load(f)

units = [u for u in data['units']
         if 'game' in u.get('metadata', {}).get('progress_categories', [])]

def pct(u, key, default=0.0):
    v = u['measures'].get(key)
    return float(v) if v is not None else default

mode = sys.argv[1] if len(sys.argv) > 1 else 'least-matched'

if mode == 'least-matched':
    worst = sorted(units, key=lambda u: pct(u, 'matched_code_percent'))
    print('=== GAME units, least matched code % (bottom 25) ===')
    for u in worst[:25]:
        m = u['measures']
        print('{matched:6.1f}%  fuzzy={fuzzy:6.1f}%  fn={fn}/{tfn}  bytes={bc}/{tc}  {name}'.format(
            matched=pct(u, 'matched_code_percent'),
            fuzzy=pct(u, 'fuzzy_match_percent'),
            fn=m.get('matched_functions'), tfn=m.get('total_functions'),
            bc=m.get('matched_code'), tc=m.get('total_code'),
            name=u['name']))
elif mode == 'least-fuzzy':
    worst = sorted(units, key=lambda u: pct(u, 'fuzzy_match_percent'))
    print('=== GAME units, lowest fuzzy % (not decompiled / stubbed, bottom 25) ===')
    for u in worst[:25]:
        m = u['measures']
        print('{fuzzy:6.1f}%  matched={matched:6.1f}%  fn={fn}/{tfn}  bytes={bc}/{tc}  {name}'.format(
            fuzzy=pct(u, 'fuzzy_match_percent'),
            matched=pct(u, 'matched_code_percent'),
            fn=m.get('matched_functions'), tfn=m.get('total_functions'),
            bc=m.get('matched_code'), tc=m.get('total_code'),
            name=u['name']))
elif mode == 'most-fn':
    by_fn = sorted(units, key=lambda u: -(int(u['measures'].get('total_functions') or 0)
                                          - int(u['measures'].get('matched_functions') or 0)))
    print('=== GAME units with the most unmatched functions (top 25) ===')
    for u in by_fn[:25]:
        m = u['measures']
        left = (int(m.get('total_functions') or 0) - int(m.get('matched_functions') or 0))
        print('left={left:4d}  matched={matched:6.1f}%  fuzzy={fuzzy:6.1f}%  fn={fn}/{tfn}  {name}'.format(
            left=left, matched=pct(u, 'matched_code_percent'),
            fuzzy=pct(u, 'fuzzy_match_percent'),
            fn=m.get('matched_functions'), tfn=m.get('total_functions'),
            name=u['name']))