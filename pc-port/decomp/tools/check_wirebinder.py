import json
with open('build/GMSP01/report.json') as f:
    r = json.load(f)
for u in r['units']:
    if 'wireBinder' in u['name']:
        print('wireBinder complete:', u.get('complete', False))
        print('Measures:', json.dumps(u['measures'], indent=2))
        for fn in u.get('functions', []):
            p = fn.get('fuzzy_match_percent', 100)
            if p < 100:
                print('  non-matching:', fn['name'], '@', p, '%')
        break