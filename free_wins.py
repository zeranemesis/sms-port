import json, re
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
cfg = open('configure.py', encoding='utf-8', errors='replace').read()
cand = []
for u in units:
    m = u['measures']
    if u['metadata'].get('complete'):
        continue
    code = f(m.get('matched_code_percent',0))
    data = f(m.get('matched_data_percent',0))
    # require 100% code and (100% data OR no data)
    tot_data = f(m.get('total_data',0))
    data_ok = (tot_data == 0) or (data >= 100.0)
    if code >= 100.0 and data_ok:
        # find source path
        src = u['metadata'].get('source_path','')
        # check configure line status
        pat = re.escape(src.replace('src/','').replace('.cpp','.cpp'))
        line = None
        for L in cfg.splitlines():
            if src.replace('src/','') in L:
                line = L.strip()
        cand.append((u['name'].replace('marioEU/',''), src,
                     int(f(m.get('matched_functions',0))), int(f(m.get('total_functions',0))),
                     line))
cand.sort(key=lambda c: -c[2])
print('units at 100%% code+data but NOT complete: %d' % len(cand))
for name, src, mf, tf, line in cand:
    flag = 'Matching' if line and 'Matching,' in line and 'NonMatching' not in line else 'NonMatching/??'
    print('%-42s fns %d/%d  cfg=[%s]' % (name, mf, tf, flag))
    print('    src=%s' % src)
    if line: print('    cfgline=%s' % line)
