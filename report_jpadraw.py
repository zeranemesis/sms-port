import json
r = json.load(open('build/GMSP01/report.json'))
def walk(obj):
    if isinstance(obj, dict):
        if obj.get('name','').endswith('JPADraw'):
            m = obj.get('measures', {})
            print('name=%s' % obj['name'])
            def n(x):
                try: return float(x)
                except: return 0.0
            print('  code=%.0f/%.0f (%.2f%%)  data=%.0f/%.0f (%.2f%%)  funcs=%.0f/%.0f' % (
                n(m.get('matched_code')), n(m.get('total_code')),
                n(m.get('matched_code_percent')),
                n(m.get('matched_data')), n(m.get('total_data')),
                n(m.get('matched_data_percent')),
                n(m.get('matched_functions')), n(m.get('total_functions'))))
            print('  complete=%s' % obj.get('metadata',{}).get('complete'))
            for s in obj.get('sections', []):
                try:
                    print('  section %s size=%d fuzzy=%.2f%%' % (
                        s.get('name'), int(s.get('size',0)), float(s.get('fuzzy_match_percent',0))))
                except:
                    print('  section %s' % s.get('name'))
            for f in obj.get('functions', []):
                if f.get('fuzzy_match_percent',100) < 100:
                    print('  FN-NONMATCH %s %.2f%%' % (f.get('name'), f.get('fuzzy_match_percent',0)))
        for v in obj.values():
            walk(v)
    elif isinstance(obj, list):
        for v in obj:
            walk(v)
walk(r)
