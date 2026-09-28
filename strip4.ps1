$ErrorActionPreference = 'Stop'
$path = 'C:\Users\BEACALCADM\source\repos\sms\src\JSystem\JParticle\JPADraw.cpp'
$t = [System.IO.File]::ReadAllText($path)
$lines = [System.Collections.Generic.List[string]]::new()
$lines.AddRange(($t -split "`n"))
Write-Host ("TOTAL=" + $lines.Count)
# Show lines around the 4 functions (1-based)
foreach ($n in @(@(329,342), @(1234,1263))) {
  Write-Host ("--- block " + $n[0] + ".." + $n[1] + " ---")
  for ($i = $n[0]; $i -le $n[1]; $i++) {
    if ($i -le $lines.Count) {
      $s = $lines[$i-1]
      $s = $s -replace "`t", "<TAB>"
      $s = $s -replace "`r", "<CR>"
      Write-Host ("{0,4}|{1}" -f $i, $s)
    }
  }
}
