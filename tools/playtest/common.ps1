# Shared helpers for tools/playtest. Dot-source from the scripts in this folder:  . "$PSScriptRoot\common.ps1"
# Everything here is read-and-screenshot: nothing writes into the game install except tyrender.ini, which the caller backs up and restores.
Add-Type -AssemblyName System.Drawing
$ToolsDir = Split-Path $PSScriptRoot -Parent          # tools\ (holds key.ps1 and ty_play.ps1)

# Ty's region in fractions of the frame (x0,y0,x1,y1). Skipped by the flicker and diff metrics because he animates.
$TyX0 = 0.41; $TyY0 = 0.39; $TyX1 = 0.605; $TyY1 = 0.90

Add-Type -ReferencedAssemblies System.Drawing @'
using System; using System.Drawing; using System.Drawing.Imaging;
public static class PlaytestImg {
  // Luma (0-255) as 4x4 block averages. W and H are the grid size.
  public static float[] Luma(string p, out int W, out int H) {
    using (var A = new Bitmap(p)) {
      int w = A.Width, h = A.Height;
      var d = A.LockBits(new Rectangle(0, 0, w, h), ImageLockMode.ReadOnly, PixelFormat.Format24bppRgb);
      var buf = new byte[d.Stride * h];
      System.Runtime.InteropServices.Marshal.Copy(d.Scan0, buf, 0, buf.Length);
      A.UnlockBits(d);
      int s = d.Stride; W = w / 4; H = h / 4; var o = new float[W * H];
      for (int j = 0; j < H; j++) for (int i = 0; i < W; i++) {
        float sum = 0;
        for (int y = 0; y < 4; y++) for (int x = 0; x < 4; x++) { int k = (j * 4 + y) * s + (i * 4 + x) * 3; sum += 0.2126f * buf[k + 2] + 0.7152f * buf[k + 1] + 0.0722f * buf[k]; }
        o[j * W + i] = sum / 16f;
      }
      return o;
    }
  }
  static bool Skip(int i, int j, int W, int H, double x0, double y0, double x1, double y1) {
    double fx = (i + 0.5) / W, fy = (j + 0.5) / H;
    return fx >= x0 && fx <= x1 && fy >= y0 && fy <= y1;
  }
  // Mean absolute luma difference of two same-size frames, outside the skipped rectangle. Returns -1 on size mismatch.
  public static double MeanAbsDiff(string a, string b, double x0, double y0, double x1, double y1) {
    int W, H, W2, H2;
    float[] A = Luma(a, out W, out H), B = Luma(b, out W2, out H2);
    if (W != W2 || H != H2) return -1;
    double sum = 0; long n = 0;
    for (int j = 0; j < H; j++) for (int i = 0; i < W; i++) {
      if (Skip(i, j, W, H, x0, y0, x1, y1)) continue;
      sum += Math.Abs(A[j * W + i] - B[j * W + i]); n++;
    }
    return n > 0 ? sum / n : -1;
  }
  // Temporal std-dev of luma over the frames (static camera), outside the skipped rectangle. Returns {whole, upper half, lower half}.
  public static double[] TemporalStd(string[] files, double x0, double y0, double x1, double y1) {
    int n = files.Length, W = 0, H = 0; var fr = new float[n][];
    for (int f = 0; f < n; f++) { int w, h; fr[f] = Luma(files[f], out w, out h); W = w; H = h; }
    double all = 0, up = 0, lo = 0; long na = 0, nu = 0, nl = 0;
    for (int j = 0; j < H; j++) for (int i = 0; i < W; i++) {
      if (Skip(i, j, W, H, x0, y0, x1, y1)) continue;
      double m = 0; for (int f = 0; f < n; f++) m += fr[f][j * W + i]; m /= n;
      double v = 0; for (int f = 0; f < n; f++) { double d = fr[f][j * W + i] - m; v += d * d; }
      double sd = Math.Sqrt(v / (n - 1));
      all += sd; na++; if (j < H / 2) { up += sd; nu++; } else { lo += sd; nl++; }
    }
    return new double[] { all / Math.Max(1, na), up / Math.Max(1, nu), lo / Math.Max(1, nl) };
  }
}
'@

# Set one key in one ini section, creating the section or key when missing. Only touches the text lines; comments are kept.
function Set-IniKey([string]$Path, [string]$Section, [string]$Key, [string]$Value) {
    $lines = New-Object System.Collections.Generic.List[string]
    if (Test-Path $Path) { $lines.AddRange([string[]](Get-Content $Path -Encoding UTF8)) }
    $inSec = $false; $secLine = -1; $keyLine = -1
    for ($i = 0; $i -lt $lines.Count; $i++) {
        $l = $lines[$i]
        if ($l -match '^\s*\[(.+?)\]') { if ($inSec) { break }; if ($matches[1] -eq $Section) { $inSec = $true; $secLine = $i }; continue }
        if ($inSec -and $l -match ('^\s*' + [regex]::Escape($Key) + '\s*=')) { $keyLine = $i; break }
    }
    if ($keyLine -ge 0) { $lines[$keyLine] = "$Key=$Value" }
    elseif ($secLine -ge 0) { $lines.Insert($secLine + 1, "$Key=$Value") }
    else { $lines.Add("[$Section]"); $lines.Add("$Key=$Value") }
    [IO.File]::WriteAllLines($Path, $lines.ToArray())
}

function Get-GameShots([string]$Game) {
    $dir = Join-Path $Game 'Screenshots'
    if (Test-Path $dir) { return @(Get-ChildItem $dir -Filter *.png | Sort-Object LastWriteTime | ForEach-Object { $_.FullName }) }
    return @()
}

# Only the game processes running out of $Game count. Other copies of the game on the same PC are never touched.
function Get-GameProcs([string]$Game, [string[]]$Names = @('TY_tyrender', 'TY')) {
    $root = [IO.Path]::GetFullPath($Game).TrimEnd('\') + '\'
    return @(Get-Process -Name $Names -ErrorAction SilentlyContinue | Where-Object { $_.Path -and $_.Path.StartsWith($root, [StringComparison]::OrdinalIgnoreCase) })
}

# Press one key in the game window. Targets the game process under $Game by PID, and key.ps1 fails visibly if focus is lost.
function Send-GameKey([string]$Game, [string]$Proc, [string]$Key, [int]$DelayMs = 800) {
    $p = Get-GameProcs $Game -Names @($Proc) | Select-Object -First 1
    if (-not $p) { throw "no $Proc running from $Game" }
    $null = & powershell -NoProfile -File (Join-Path $ToolsDir 'key.ps1') -Keys $Key -TargetPid $p.Id -DelayMs $DelayMs 2>&1
}

# Press the A/B screenshot key once (F11 by default, which writes 2 PNGs into <game>\Screenshots) and copy the new pair to $Dest as <Prefix>_a.png / <Prefix>_b.png.
# Returns the copied paths (empty when the game wrote nothing).
function Take-Pair([string]$Game, [string]$Proc, [string]$Dest, [string]$Prefix, [string]$ShotKey = 'F11') {
    $before = @(Get-GameShots $Game)
    Send-GameKey $Game $Proc $ShotKey 300
    $new = @()
    for ($t = 0; $t -lt 16 -and $new.Count -lt 2; $t++) {
        Start-Sleep -Milliseconds 500
        $new = @(Get-GameShots $Game | Where-Object { $before -notcontains $_ })
    }
    $out = @()
    $i = 0
    foreach ($f in ($new | Select-Object -First 2)) {
        $to = Join-Path $Dest ("{0}_{1}.png" -f $Prefix, [char](97 + $i))
        Copy-Item $f $to -Force
        $out += $to; $i++
    }
    return ,$out
}

# Contact sheet: one row per Rows entry (@{ Label = '...'; Files = @(...) }), cells of CellW pixels, 16:9.
function New-ContactSheet([object[]]$Rows, [string]$OutFile, [int]$CellW = 480) {
    $labelW = 190; $pad = 6; $cellH = [int]($CellW * 9 / 16)
    $cols = [Math]::Max(1, (@($Rows | ForEach-Object { $_.Files.Count }) | Measure-Object -Maximum).Maximum)
    $bmp = New-Object System.Drawing.Bitmap ($labelW + $cols * ($CellW + $pad) + $pad), ($Rows.Count * ($cellH + $pad) + $pad)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.Clear([System.Drawing.Color]::FromArgb(30, 30, 30))
    $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $font = New-Object System.Drawing.Font 'Segoe UI', 11
    for ($r = 0; $r -lt $Rows.Count; $r++) {
        $y = $pad + $r * ($cellH + $pad)
        $g.DrawString([string]$Rows[$r].Label, $font, [System.Drawing.Brushes]::White, 6, $y + 6)
        for ($c = 0; $c -lt $Rows[$r].Files.Count; $c++) {
            $src = [System.Drawing.Image]::FromFile($Rows[$r].Files[$c])
            $g.DrawImage($src, $labelW + $c * ($CellW + $pad), $y, $CellW, $cellH)
            $src.Dispose()
        }
    }
    $bmp.Save($OutFile, [System.Drawing.Imaging.ImageFormat]::Png)
    $font.Dispose(); $g.Dispose(); $bmp.Dispose()
}
