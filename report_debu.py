import json
r = json.load(open('build/GMSP01/report.json'))
def f(x):
    try: return float(x)
    except: return 0.0
def walk(obj):
    if isinstance(obj, dict):
        if 'measures' in obj and obj.get('name','').endswith('DebuTelesa'):
            m = obj['measures']
            print('name=%s complete=%s' % (obj['name'], obj['metadata'].get('complete')))
            for k in ('matched_code','total_code','matched_code_percent',
                      'matched_data','total_data','matched_data_percent',
                      'matched_functions','total_functions','matched_functions_percent'):
                print('  %-26s = %s' % (k, m.get(k)))
            print('  --- sections ---')
            for s in obj.get('sections', []):
                print('  %-10s size=%s fuzzy=%s' % (s.get('name'), s.get('size'), s.get('fuzzy_match_percent')))
            print('  --- non-100 fns ---')
            for fn in obj.get('functions', []):
                if f(fn.get('fuzzy_match_percent',100)) < 100:
                    print('  FN %-40s %.2f%% size=%s' % (fn.get('name'), f(fn.get('fuzzy_match_percent',0)), fn.get('size')))
        for v in obj.values():
            walk(v)
    elif isinstance(obj, list):
        for v in obj:
            walk(v)
walk(r)
