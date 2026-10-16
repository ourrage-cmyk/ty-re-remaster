# Temporal-stability (flicker) measurement, zero LLM tokens. Static camera, N frames per variant in ONE game session (variants differ by live ini keys; boot ini = first variant).
#   powershell -NoProfile -File flicker.ps1 -Name fl1 -Dll <dll> -AB "ultra=a.ini;noao=b.ini" [-Cam "0 -60 300 -50 8" | ''] [-Frames 10]
# Metric per variant: mean over pixels (Ty region excluded) of the temporal std-dev of luma over the N frames (0-255 scale), for whole frame / upper half (rocks, sky, fog) / lower half (ground).
# Compare against the 'off' variant (animated grass/clouds give a baseline); the excess is flicker. Output: results\<Name>\flicker.md + frames.
param([Parameter(Mandatory)][string]$Name, [Parameter(Mandatory)][string]$Dll, [Parameter(Mandatory)][string]$AB, [string]$Cam = '', [int]$Frames = 10, [string]$Region = '')
Add-Type -ReferencedAssemblies System.Drawing @'
using System; using System.Drawing; using System.Drawing.Imaging;
public static class Flick {
  public static float[] Luma4(string p, out int W, out int H) {
    using (var A = new Bitmap(p)) { int w = A.Width, h = A.Height; var d = A.LockBits(new Rectangle(0, 0, w, h), ImageLockMode.ReadOnly, PixelFormat.Format24bppRgb); var buf = new byte[d.Stride * h];
      System.Runtime.InteropServices.Marshal.Copy(d.Scan0, buf, 0, buf.Length); int s = d.Stride; A.UnlockBits(d); W = w / 4; H = h / 4; var o = new float[W * H];
      for (int j = 0; j < H; j++) for (int i = 0; i < W; i++) { float sum = 0; for (int y = 0; y < 4; y++) for (int x = 0; x < 4; x++) { int k = (j * 4 + y) * s + (i * 4 + x) * 3; sum += 0.2126f * buf[k + 2] + 0.7152f * buf[k + 1] + 0.0722f * buf[k]; } o[j * W + i] = sum / 16f; } return o; } }
  public static double[] Std(string[] files) {
    int W = 0, H = 0; var fr = new float[files.Length][]; for (int f = 0; f < files.Length; f++) { int w, h; fr[f] = Luma4(files[f], out w, out h); W = w; H = h; }
    double all = 0, up = 0, lo = 0; long na = 0, nu = 0, nl = 0; int n = files.Length;
    for (int j = 0; j < H; j++) for (int i = 0; i < W; i++) { if (i * 4 > 1050 && i * 4 < 1550 && j * 4 > 560 && j * 4 < 1300) continue;   // Ty (animated)
      double m = 0; for (int f = 0; f < n; f++) m += fr[f][j * W + i]; m /= n; double v = 0; for (int f = 0; f < n; f++) { double d = fr[f][j * W + i] - m; v += d * d; } double sd = Math.Sqrt(v / (n - 1));
      all += sd; na++; if (j < H / 2) { up += sd; nu++; } else { lo += sd; nl++; } }
    return new double[] { all / na, up / nu, lo / nl }; } }
'@
$out = "C:\CLAUDE\RESEARCH\results\$Name"; New-Item -ItemType Directory -Force $out | Out-Null
$firstIni = ((($AB -split ';')[0]) -split '=', 2)[1].Trim()
$views = (1..$Frames | % { 'f{0:D2}={1}' -f $_, $Cam }) -join ';'
$lines = & powershell -NoProfile -File C:\CLAUDE\TOOLS\agent_test.ps1 -Dll $Dll -IniFile $firstIni -AB $AB -CamSeq $views -OutDir $out 2>&1 | % { "$_" }; $lines | Set-Content "$out\harness_output.txt" -Encoding utf8
$vn = @($AB -split ';' | ? { $_.Trim() } | % { ($_ -split '=', 2)[0].Trim() }); $md = "# Flicker (temporal std of luma, 0-255; Ty excluded; $Frames frames, cam='$Cam')`n| variant | whole | upper (rocks/sky/fog) | lower (ground) |`n|---|---|---|---|`n"; $res = @{}
foreach ($v in $vn) { $files = @(1..$Frames | % { "$out\${v}__f{0:D2}.png" -f $_ } | ? { Test-Path $_ }); if ($files.Count -lt 3) { $md += "| $v | no frames ($($files.Count)) | | |`n"; continue }
    $s = [Flick]::Std($files); $res[$v] = $s; $md += "| $v | {0:N3} | {1:N3} | {2:N3} |`n" -f $s[0], $s[1], $s[2] }
[IO.File]::WriteAllText("$out\flicker.md", $md, (New-Object Text.UTF8Encoding $false)); $md
if ($res.Count -eq $vn.Count) { 'OK flicker' } else { 'FAIL missing frames' }
