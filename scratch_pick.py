import json
r = json.load(open('scratch_99.json'))
t = [x for x in r if x[3] == 'other']
print(t[0][0], t[0][1])
n = [x for x in r if x[3] == 'NODIFF']
print(n[0][0], n[0][1])
