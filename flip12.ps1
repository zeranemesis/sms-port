$ErrorActionPreference = 'Stop'
$p = 'C:\Users\BEACALCADM\source\repos\sms\configure.py'
$t = [System.IO.File]::ReadAllText($p)
$pairs = @(
  'Object(NonMatching, "JSystem/JAudio/JAInterface/JAIGFrameStream.cpp")',
  'Object(NonMatching, "JSystem/JAudio/JAInterface/JAIGlobalParameter.cpp")',
  'Object(NonMatching, "JSystem/JParticle/JPAParticle.cpp")',
  'PCHObject(NonMatching, "MoveBG/MapObjOption.cpp")',
  'Object(NonMatching, "JSystem/JAudio/JASystem/JASDSPChannel.cpp")',
  'PCHObject(NonMatching, "Map/MapModel.cpp")',
  'PCHObject(NonMatching, "M3DUtil/M3UModel.cpp")',
  'Object(NonMatching, "Camera/CameraMarioData.cpp")',
  'PCHObject(NonMatching, "Player/MarioBlend.cpp")',
  'Object(NonMatching, "Camera/CameraTalk.cpp")',
  'Object(NonMatching, "JSystem/JDrama/JDRDisplay.cpp")',
  'Object(NonMatching, "Map/MapArea.cpp")'
)
$done = 0
foreach ($s in $pairs) {
  $cnt = ([regex]::Matches($t, [regex]::Escape($s))).Count
  if ($cnt -ne 1) { throw ("expected 1 occurrence of [$s], found $cnt") }
  $rep = $s -replace 'NonMatching', 'Matching'
  $t = $t.Replace($s, $rep)
  $done++
}
[System.IO.File]::WriteAllText($p, $t)
Write-Host ("flipped $done / " + $pairs.Count)
