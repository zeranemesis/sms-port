import json
r = json.load(open('build/GMSP01/report.json'))
m = r['measures']
print("TOP:", {k: (round(v,2) if isinstance(v,(int,float)) else v) for k,v in m.items()})
units = r['units']
done = sum(1 for u in units if u.get('complete'))
print("units complete:", done, "/", len(units))
