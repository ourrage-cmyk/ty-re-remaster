# Create a copy of TY.exe that also imports tyrender.dll (adds a new PE section holding a rebuilt import directory).
# The input exe is never modified. Usage:
#   powershell -NoProfile -File patch_imports.ps1 [-In <TY.exe>] [-Out <TY_tyrender.exe>]
# Works on any PE32 with room for one more section header (checked). Bound-import directory is cleared.
param([string]$In = 'C:\CLAUDE\TY the Tasmanian Tiger\TY.exe', [string]$Out = 'C:\CLAUDE\TY the Tasmanian Tiger\TY_tyrender.exe',
      [string]$Dll = 'tyrender.dll', [string]$Func = 'tyrender_init')
$ErrorActionPreference = 'Stop'
[byte[]]$b = [IO.File]::ReadAllBytes($In)
function U16($o) { [BitConverter]::ToUInt16($b, $o) }
function U32($o) { [BitConverter]::ToUInt32($b, $o) }
function W16($o, $v) { [BitConverter]::GetBytes([uint16]$v).CopyTo($b, $o) }
function W32($o, $v) { [BitConverter]::GetBytes([uint32]$v).CopyTo($b, $o) }
function Align($v, $a) { [uint32]([Math]::Ceiling($v / $a) * $a) }

$pe = U32 0x3C
if ((U32 $pe) -ne 0x4550) { throw 'not a PE file' }
$nsec = U16 ($pe + 6); $optSize = U16 ($pe + 20); $opt = $pe + 24
if ((U16 $opt) -ne 0x10B) { throw 'not PE32' }
$secAlign = U32 ($opt + 32); $fileAlign = U32 ($opt + 36); $sizeOfHeaders = U32 ($opt + 60)
$dd = $opt + 96
$impRva = U32 ($dd + 8); $impSize = U32 ($dd + 12)
$secTab = $opt + $optSize
$newHdr = $secTab + $nsec * 40
$firstRaw = ($(for ($i = 0; $i -lt $nsec; $i++) { U32 ($secTab + $i * 40 + 20) }) | Where-Object { $_ -gt 0 } | Measure-Object -Minimum).Minimum
if ($newHdr + 40 -gt [Math]::Min($sizeOfHeaders, $firstRaw)) { throw 'no room for another section header' }
for ($i = 0; $i -lt $nsec; $i++) { if ([Text.Encoding]::ASCII.GetString($b, $secTab + $i * 40, 8).TrimEnd([char]0) -eq '.tyimp') { throw 'already patched' } }

# RVA -> file offset
function Off($rva) {
    for ($i = 0; $i -lt $nsec; $i++) {
        $s = $secTab + $i * 40; $va = U32 ($s + 12); $vs = [Math]::Max((U32 ($s + 8)), (U32 ($s + 16)))
        if ($rva -ge $va -and $rva -lt $va + $vs) { return $rva - $va + (U32 ($s + 20)) }
    }
    throw "rva $rva not in any section"
}
# existing descriptors
$descs = New-Object Collections.Generic.List[byte[]]
$o = Off $impRva
while ($true) { $d = $b[$o..($o + 19)]; if (($d | Measure-Object -Sum).Sum -eq 0) { break }; $descs.Add([byte[]]$d); $o += 20 }

# new section layout (relative offsets)
$last = $secTab + ($nsec - 1) * 40
$newVa = Align ((U32 ($last + 12)) + [Math]::Max((U32 ($last + 8)), (U32 ($last + 16)))) $secAlign
$rawEnd = (U32 ($last + 20)) + (U32 ($last + 16))
if ($rawEnd -ne $b.Length) { Write-Warning "file has $($b.Length - $rawEnd) bytes of overlay after the last section; they are kept before the new section" }
$newRaw = Align $b.Length $fileAlign
$n = $descs.Count + 1
$descOff = 0; $descLen = ($n + 1) * 20
$oftOff = $descLen; $ftOff = $oftOff + 8; $hnOff = $ftOff + 8
$hn = [byte[]](0, 0) + [Text.Encoding]::ASCII.GetBytes($Func) + [byte[]](0, 0)
$dllOff = $hnOff + $hn.Length
$dllB = [Text.Encoding]::ASCII.GetBytes($Dll) + [byte[]](0)
$vsize = $dllOff + $dllB.Length
$rawSize = Align $vsize $fileAlign
$sec = New-Object byte[] $rawSize
for ($i = 0; $i -lt $descs.Count; $i++) { $descs[$i].CopyTo($sec, $i * 20) }
$nd = $descs.Count * 20
[BitConverter]::GetBytes([uint32]($newVa + $oftOff)).CopyTo($sec, $nd + 0)   # OriginalFirstThunk
[BitConverter]::GetBytes([uint32]($newVa + $dllOff)).CopyTo($sec, $nd + 12)  # Name
[BitConverter]::GetBytes([uint32]($newVa + $ftOff)).CopyTo($sec, $nd + 16)   # FirstThunk
[BitConverter]::GetBytes([uint32]($newVa + $hnOff)).CopyTo($sec, $oftOff)
[BitConverter]::GetBytes([uint32]($newVa + $hnOff)).CopyTo($sec, $ftOff)
$hn.CopyTo($sec, $hnOff); $dllB.CopyTo($sec, $dllOff)

# section header
$name = [Text.Encoding]::ASCII.GetBytes('.tyimp'); $hdr = New-Object byte[] 40; $name.CopyTo($hdr, 0)
[BitConverter]::GetBytes([uint32]$vsize).CopyTo($hdr, 8); [BitConverter]::GetBytes([uint32]$newVa).CopyTo($hdr, 12)
[BitConverter]::GetBytes([uint32]$rawSize).CopyTo($hdr, 16); [BitConverter]::GetBytes([uint32]$newRaw).CopyTo($hdr, 20)
[BitConverter]::GetBytes([uint32]3221225536).CopyTo($hdr, 36)   # initialized data | read | write
$hdr.CopyTo($b, $newHdr)
W16 ($pe + 6) ($nsec + 1)
# Large Address Aware: TY.exe is a 32-bit exe limited to 2 GB of address space; the HD texture pack + GPU driver + renderer buffers exhaust it
# ("CMKHeap - Out of memory allocating N" crash while loading). The copy gets 4 GB on 64-bit Windows; the original TY.exe is never modified.
W16 ($pe + 22) ((U16 ($pe + 22)) -bor 0x20)
W32 ($opt + 56) (Align ($newVa + $vsize) $secAlign)   # SizeOfImage
W32 ($opt + 64) 0                                     # CheckSum (not verified for exes)
W32 ($dd + 8) ($newVa + $descOff); W32 ($dd + 12) $descLen
W32 ($dd + 88) 0; W32 ($dd + 92) 0                    # bound imports: cleared

$outBytes = New-Object byte[] ($newRaw + $rawSize)
[Array]::Copy($b, $outBytes, $b.Length); [Array]::Copy($sec, 0, $outBytes, $newRaw, $rawSize)
[IO.File]::WriteAllBytes($Out, $outBytes)
"wrote $Out : $($descs.Count) existing imports + $Dll!$Func, new section .tyimp VA=0x{0:X} raw=0x{1:X}" -f $newVa, $newRaw
