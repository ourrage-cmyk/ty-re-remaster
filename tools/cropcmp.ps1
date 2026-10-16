# Side-by-side zoomed crops of the same region from several same-spot screenshots (for judging AA/sharpening/AO detail).
# Usage: powershell -NoProfile -File cropcmp.ps1 -Images a.png,b.png,c.png -Region "x,y,w,h" -Zoom 3 -Out cmp.png
param([Parameter(Mandatory)][string[]]$Images, [Parameter(Mandatory)][string]$Region, [int]$Zoom = 3, [Parameter(Mandatory)][string]$Out)
Add-Type -AssemblyName System.Drawing
$Images = @($Images | % { $_ -split ',' } | ? { $_ })   # powershell -File passes "a,b,c" as one string
$r =$Region -split ',' | % { [int]$_ }
$w = $r[2] * $Zoom; $h = $r[3] * $Zoom; $gap = 6
$outBmp = New-Object Drawing.Bitmap (($w + $gap) * $Images.Count - $gap), $h
$g = [Drawing.Graphics]::FromImage($outBmp); $g.InterpolationMode = 'NearestNeighbor'; $g.PixelOffsetMode = 'Half'; $g.Clear([Drawing.Color]::Black)
for ($i = 0; $i -lt $Images.Count; $i++) {
    $img = [Drawing.Image]::FromFile((Resolve-Path $Images[$i]).Path)
    $g.DrawImage($img, (New-Object Drawing.Rectangle (($w + $gap) * $i), 0, $w, $h), (New-Object Drawing.Rectangle $r[0], $r[1], $r[2], $r[3]), 'Pixel')
    $img.Dispose()
}
$g.Dispose(); $outBmp.Save($Out); $outBmp.Dispose(); $Out
