import json
r = json.load(open('build/GMSP01/report.json'))
units = []
def walk(obj):
    if isinstance(obj, dict):
        if 'measures' in obj and 'name' in obj and 'metadata' in obj and obj['name'].startswith('marioEU'):
            units.append(obj)
        for v in obj.values():
            walk(v)
    elif isinstance(obj, list):
        for v in obj:
            walk(v)
walk(r)
def f(x):
    try: return float(x)
    except: return 0.0
inc = [u for u in units if not u['metadata'].get('complete')]
# rank by (matched_functions/total_functions) desc, then code pct
inc.sort(key=lambda u: (-f(u['measures'].get('matched_functions_percent',0)), -f(u['measures'].get('matched_code_percent',0))))
print('incomplete units: %d' % len(inc))
print('--- top 15 by function-match (best finishing targets) ---')
for u in inc[:15]:
    m = u['measures']
    tf = int(f(m.get('total_functions',0))); mf = int(f(m.get('matched_functions',0)))
    print('%-45s fns %d/%d (%.1f%%)  code %.1f%%' % (
        u['name'].replace('marioEU/',''), mf, tf,
        f(m.get('matched_functions_percent',0)), f(m.get('matched_code_percent',0))))
