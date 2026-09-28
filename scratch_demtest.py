import re
def argcount(m):
    i = m.find('__')
    if i < 0:
        return None
    rest = m[i + 2:]
    mm = re.match(r'\d+[A-Za-z_][A-Za-z0-9_]*(?:<.*>)?', rest)
    t = rest[mm.end():] if mm else rest
    t = t.lstrip('F')
    if t == 'v' or t == '':
        return 0
    n, depth = 0, 0
    i = 0
    while i < len(t):
        c = t[i]
        if c == '<':
            depth += 1
        elif c == '>':
            depth -= 1
        elif depth == 0 and c in 'bhctijlmfdr':
            n += 1
        elif depth == 0 and c == 'v':
            return n
        elif depth == 0 and c in 'PQ':
            pass
        elif depth == 0 and c.isupper():
            n += 1
        i += 1
    return n

tests = {
 'receiveMessage__10gatekeeperF14NerveMsgTag13NerveMsgData10TLiveActor': 3,
 'init__10AnimalBaseFv': 0,
 'execMotionBlend___8TBaseNPCFv': 0,
 'getNozzleTopPos___15CPolarSubCameraCFPQ29JGeometry8TVec3<f>': 1,
 'changeCamModeSpecifyCamMapTool___15CPolarSubCameraFPC14TCameraMapTool': 1,
 'isPolWaitCEffectEmitTime___8TBaseNPCCFv': 0,
 'execute__21TNerveBEelTearsMoveUpCFP24TSpineBase<10TLiveActor>': 1,
 'setParamSoundOutputMode__18JAIGlobalParameterFUl': 1,
 'makeObjDefault__10MapObjBaseFv': 0,
}
for t, exp in tests.items():
    got = argcount(t)
    print(('OK ' if got == exp else 'BAD'), got, exp, t)
