import json
r = json.load(open('build/GMSP01/report.json'))
def f(x):
    try: return float(x)
    except: return 0.0
units = []
def walk(obj):
    if isinstance(obj, dict):
        if 'measures' in obj and obj.get('name'):
            units.append((obj['name'], f(obj['measures'].get('matched_code_percent',0)),
                          f(obj['measures'].get('matched_data_percent',0)),
                          f(obj['measures'].get('matched_functions_percent',0)),
                          f(obj['measures'].get('total_code',0)), f(obj['measures'].get('matched_code',0)),
                          f(obj['measures'].get('total_data',0)), f(obj['measures'].get('matched_data',0)),
                          obj.get('metadata',{}).get('complete'), 'units' in obj))
        for v in obj.values():
            walk(v)
    elif isinstance(obj, list):
        for v in obj:
            walk(v)
walk(r)
# top-level = units that have child units
top = [u for u in units if u[9]]
print('=== TOP-LEVEL ===')
for u in sorted(top, key=lambda x: x[4]-x[5], reverse=True):
    print('%-35s code %6.2f%% (%8.0f/%8.0f)  data %6.2f%%  fn %6.1f%%  complete=%s' % (u[0], u[1], u[5], u[4], u[2], u[3], u[8]))
print()
leaves = [u for u in units if not u[9]]
work = [u for u in leaves if (u[4]-u[5]) > 0 or (u[7]-u[6]) > 0]
work.sort(key=lambda u: (u[4]-u[5]) + max(0, u[7]-u[6]), reverse=True)
print('=== LEAVES with remaining work (top 45) ===')
print('%-45s %6s %6s %6s %9s %8s' % ('name','code%','data%','fn%','code_rem','data_rem'))
for u in work[:45]:
    print('%-45s %6.1f %6.1f %6.1f %9.0f %8.0f' % (u[0], u[1], u[2], u[3], u[4]-u[5], u[7]-u[6]))
print('TOTAL code_rem = %.0f  data_rem = %.0f  units_with_work = %d / %d leaves' % (sum(u[4]-u[5] for u in work), sum(max(0,u[7]-u[6]) for u in work), len(work), len(leaves)))
