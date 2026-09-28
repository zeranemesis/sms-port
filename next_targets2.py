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
# rank by remaining code bytes ascending (smallest gap to 100%)
rows = []
for u in inc:
    m = u['measures']
    tc = f(m.get('total_code',0)); mc = f(m.get('matched_code',0))
    tf = int(f(m.get('total_functions',0))); mf = int(f(m.get('matched_functions',0)))
    rem = tc - mc
    rows.append((rem, u['name'].replace('marioEU/',''), mc, tc, mf, tf))
rows.sort()
print('incomplete: %d' % len(inc))
print('--- 20 units with SMALLEST remaining code (bytes) ---')
for rem, name, mc, tc, mf, tf in rows[:20]:
    print('%-46s rem=%6.0fB  code=%.1f%%  fns %d/%d' % (name, rem, 100*mc/tc if tc else 0, mf, tf))
