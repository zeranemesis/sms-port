import json
r = json.load(open('build/GMSP01/report.json'))
for u in r['units']:
    if u['name'] == 'marioEU/Enemy/hamukuri':
        print({k: v for k, v in u.items() if k not in ('functions',)})
        break
print('---')
for u in r['units']:
    if u.get('complete') and u['name'] == 'marioEU/JSystem/JAudio/JAInterface/JAIBasic':
        print({k: v for k, v in u.items() if k not in ('functions',)})
        break
# count units with matched_code_percent > 0
nz = [u for u in r['units'] if u.get('matched_code_percent', 0) > 0]
print('units with matched_code_percent>0:', len(nz))
nz2 = [u for u in r['units'] if u.get('fuzzy_match_percent', 0) > 0]
print('units with fuzzy_match_percent>0:', len(nz2))
