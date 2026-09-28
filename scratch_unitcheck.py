import json
r = json.load(open('build/GMSP01/report.json'))
want = ['JAIBasic', 'J3DMaterialFactory', 'MSoundBGM', 'egggen', 'JPADraw', 'NpcManager', 'effectObj', 'JKRExpHeap', '/Item']
for u in r['units']:
    if any(w in u['name'] for w in want):
        m = u['measures']
        fns = [f for f in u.get('functions', []) if f.get('fuzzy_match_percent', 0) < 100.0]
        print(f"{u['name']}: code={m.get('matched_code_percent')} complete={u.get('complete')} nonmatch_fns={len(fns)}")
        for f in fns:
            print(f"   {f['name']}: {f.get('fuzzy_match_percent')}")
