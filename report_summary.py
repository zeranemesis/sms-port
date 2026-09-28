import json
r = json.load(open('build/GMSP01/report.json'))
m = r['measures']
def f(x):
    try: return float(x)
    except: return 0.0
print('code=%.2f%% data=%.2f%% funcs=%.2f%%' % (
    f(m['matched_code_percent']), f(m['matched_data_percent']),
    f(m['matched_functions_percent'])))
print('units: %.0f/%.0f complete' % (f(m['complete_units']), f(m['total_units'])))
# also find JPADraw unit status (units may be nested under categories)
def walk(obj):
    if isinstance(obj, dict):
        if obj.get('name','').endswith('JPADraw'):
            print('JPADraw unit complete=%s code%%=%.2f' % (
                obj.get('metadata',{}).get('complete'),
                obj.get('measures',{}).get('matched_code_percent',-1)))
        for v in obj.values():
            walk(v)
    elif isinstance(obj, list):
        for v in obj:
            walk(v)
walk(r)
