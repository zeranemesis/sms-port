run1 = bytes.fromhex('83818382838a82aa91ab82e882dc82b982f10a00')
run2 = bytes.fromhex('4d4163746f724d747843616c63547970655f426173696320'
                     '834e838983568362834e83588350815b838b826e826d0000')
src1 = "メモリが足りません\n"
src2 = "MActorMtxCalcType_Basic クラシックスケールＯＮ"
s1 = src1.encode('shift_jis')
s2 = src2.encode('shift_jis')
print('run1 len=%d  src1 len=%d  MATCH=%s' % (len(run1), len(s1), s1 == run1))
print('  run1 =', run1.hex())
print('  src1 =', s1.hex())
print('run2 len=%d  src2 len=%d  MATCH=%s' % (len(run2), len(s2), s2 == run2))
print('  run2 =', run2.hex())
print('  src2 =', s2.hex())
# show decoded to utf-8 for sanity
import sys
sys.stdout.reconfigure(encoding='utf-8')
print('run1 decodes to:', run1.decode('shift_jis'))
print('run2 decodes to:', run2.decode('shift_jis'))
