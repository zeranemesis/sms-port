$ErrorActionPreference = 'Stop'
$path = 'C:\Users\BEACALCADM\source\repos\sms\src\JSystem\JParticle\JPADraw.cpp'
$t = [System.IO.File]::ReadAllText($path)
# find exact start/end of each of the 4 functions by unique signature
$starts = @(
  'BOOL JPADraw::loadTexture(u8 idx',
  's16 JPADraw::getIndTextureID()',
  's16 JPADraw::getIndSubTextureID()',
  's16 JPADraw::getSecondTextureID()'
)
$ends = @(
  'void JPADraw::setDrawExecVisitorsBeforeCB(',
  'void JPADraw::loadYBBMtx(MtxPtr mtx)',
  'void JPADraw::loadYBBMtx(MtxPtr mtx)',
  'void JPADraw::loadYBBMtx(MtxPtr mtx)'
)
# compute char ranges (start = line beginning of start sig; end = line beginning of end sig)
$ranges = @()
for ($i = 0; $i -lt 4; $i++) {
  $si = $t.IndexOf($starts[$i])
  if ($si -lt 0) { throw ("start not found: " + $starts[$i]) }
  $lineStart = $t.LastIndexOf("`n", $si) + 1
  $ei = $t.IndexOf($ends[$i], $si)
  if ($ei -lt 0) { throw ("end not found for " + $starts[$i]) }
  $endLineStart = $t.LastIndexOf("`n", $ei) + 1
  $ranges += , @($lineStart, $endLineStart)
  Write-Host ("fn " + $i + " : [" + $lineStart + " .. " + $endLineStart + ") len=" + ($endLineStart - $lineStart))
}
# remove bottom-up
$new = $t
for ($i = 3; $i -ge 0; $i--) {
  $s = $ranges[$i][0]; $e = $ranges[$i][1]
  $new = $new.Remove($s, $e - $s)
}
[System.IO.File]::WriteAllText($path, $new)
Write-Host ("NEWLEN=" + $new.Length + " OLDLEN=" + $t.Length + " REMOVED=" + ($t.Length - $new.Length))
