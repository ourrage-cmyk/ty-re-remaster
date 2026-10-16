$env:TY_SETTLE = '1'; $env:TY_CAMWAIT = '1'
$v = (1..44 | % { 'p{0:D2}=' -f $_ }) -join ';'
powershell -NoProfile -File C:\CLAUDE\TOOLS\abshots.ps1 -Slot 2 -Name level2_pool -Views $v