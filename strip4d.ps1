$ErrorActionPreference = 'Stop'
$path = 'C:\Users\BEACALCADM\source\repos\sms\src\JSystem\JParticle\JPADraw.cpp'
$t = [System.IO.File]::ReadAllText($path)
$lines = [System.Collections.Generic.List[string]]::new()
$lines.AddRange(($t -split "`n"))
Write-Host ("TOTAL=" + $lines.Count)

function FindLine([string]$needle) {
  for ($i = 0; $i -lt $lines.Count; $i++) {
    if ($lines[$i].StartsWith($needle)) { return ($i + 1) }   # 1-based
  }
  throw ("line not found: " + $needle)
}

# Define each function: start line, and the NEXT function's start line (exclusive end)
$fnStarts = @(
  'BOOL JPADraw::loadTexture(',
  's16 JPADraw::getIndTextureID()',
  's16 JPADraw::getIndSubTextureID()',
  's16 JPADraw::getSecondTextureID()'
)
$nextStarts = @(
  'void JPADraw::setDrawExecVisitorsBeforeCB(',
  's16 JPADraw::getIndSubTextureID()',
  's16 JPADraw::getSecondTextureID()',
  'void JPADraw::loadYBBMtx('
)

$ranges = @()
for ($i = 0; $i -lt 4; $i++) {
  $s = FindLine $fnStarts[$i]
  $e = FindLine $nextStarts[$i]
  if ($e -le $s) { throw ("bad range fn " + $i) }
  $ranges += , @($s, $e)
  Write-Host ("fn " + $i + " : lines " + $s + " .. " + ($e-1) + "  (end marker " + $e + ")")
}

# verify each removed block's last non-empty line is '}'
foreach ($r in $ranges) {
  $last = $r[1] - 2
  while ($last -ge $r[0] -and ($lines[$last-1].Trim() -eq '')) { $last-- }
  if ($lines[$last-1].Trim() -ne '}') { throw ("fn block ends with [" + $lines[$last-1].Trim() + "] not '}'") }
}
Write-Host "verify OK"

# remove bottom-up
for ($i = 3; $i -ge 0; $i--) {
  $s = $ranges[$i][0]; $e = $ranges[$i][1]
  $lines.RemoveRange($s - 1, $e - $s)
}
$new = ($lines -join "`n")
[System.IO.File]::WriteAllText($path, $new)
Write-Host ("NEWTOTAL=" + $lines.Count + " REMOVED_LINES=" + (1289 - $lines.Count))
