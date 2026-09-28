import json
r = json.load(open('build/GMSP01/report.json'))
byunit = {u['name']: u for u in r.get('units', [])}
for name in ['marioEU/Animal/AnimalManager', 'marioEU/Camera/CameraBck', 'marioEU/JSystem/J2D/J2DPrint', 'marioEU/Enemy/spider']:
    u = byunit.get(name)
    if not u:
        print(name, 'NOT FOUND'); continue
    print(f"\n{name}: code={u.get('matched_code_percent',0):.2f}% data={u.get('matched_data_percent',0):.2f}%")
    for f in u.get('functions', []):
        print(f"    {f.get('fuzzy_match_percent',0):6.2f}%  {f.get('name')}  ({f.get('size','?')}B)")
