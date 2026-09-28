import json, sys, io
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8')
r = json.load(open('build/GMSP01/report.json'))
m = r['measures']
def n(x):
    if isinstance(x, str): return int(x.replace(',', ''))
    return int(x)
def pct(x):
    if isinstance(x, str): x = float(x.replace(',', ''))
    return '%.2f%%' % x
print('=== GLOBAL (marioEU) ===')
print('code      : %s / %s  = %s' % (m['matched_code'], m['total_code'], pct(m['matched_code_percent'])))
print('data      : %s / %s  = %s' % (m['matched_data'], m['total_data'], pct(m['matched_data_percent'])))
print('fonctions : %s / %s  = %s' % (m['matched_functions'], m['total_functions'], pct(m['matched_functions_percent'])))
print('unités    : %s / %s complètes' % (m['complete_units'], m['total_units']))
print()
print('=== PAR CATÉGORIE ===')
for c in r['categories']:
    cm = c['measures']
    print('%-22s code %7s  data %7s  fn %7s' % (
        c['name'], pct(cm['matched_code_percent']), pct(cm['matched_data_percent']),
        pct(cm.get('matched_functions_percent', 0))))
us = r['units']
comp = [u for u in us if u.get('metadata', {}).get('complete')]
print()
print('unités feuilles: %d   complètes: %d' % (len(us), len(comp)))
