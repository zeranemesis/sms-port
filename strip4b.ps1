$ErrorActionPreference = 'Stop'
$path = 'C:\Users\BEACALCADM\source\repos\sms\src\JSystem\JParticle\JPADraw.cpp'
$t = [System.IO.File]::ReadAllText($path)
$lines = [System.Collections.Generic.List[string]]::new()
$lines.AddRange(($t -split "`n"))

# sanity checks (1-based)
$assert = @(
  @('331', 'BOOL JPADraw::loadTexture(u8 idx, GXTexMapID map_id)'),
  @('339', ''),
  @('340', 'void JPADraw::setDrawExecVisitorsBeforeCB('),
  @('1236', 's16 JPADraw::getIndTextureID()'),
  @('1262', ''),
  @('1263', 'void JPADraw::loadYBBMtx(MtxPtr mtx)')
)
foreach ($a in $assert) {
  $i = [int]$a[0]
  $actual = $lines[$i-1]
  if ($actual -ne $a[1]) { throw ("MISMATCH at " + $a[0] + " expected=[" + $a[1] + "] actual=[" + $actual + "]") }
}
Write-Host "sanity OK"

# remove bottom-up: 1236..1262, then 331..339
$lines.RemoveRange(1235, 27)   # 1236..1262 inclusive (27 lines)
$lines.RemoveRange(330, 9)     # 331..339 inclusive (9 lines)

$new = ($lines -join "`n")
[System.IO.File]::WriteAllText($path, $new)
Write-Host ("NEWTOTAL=" + $lines.Count)
