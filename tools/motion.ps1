# Motion-stability (flicker under camera motion) test, zero LLM tokens. In ONE game session, per variant (live ini keys), the DLL yaws the camera smoothly (yawStep deg per frame) for N frames and
# stores every frame (640x360 gray). Flicker = temporal SECOND difference |f[k+1]-2f[k]+f[k-1]| (cancels smooth motion, keeps noise/shimmer), mean over pixels outside Ty. Variant 'off' = reference.
#   powershell -NoProfile -File motion.ps1 -Name mo1 -Dll <dll> -AB "a=a.ini;b=b.ini" [-Step 0.12] [-Frames 16] [-Cam "x y z yaw pitch"] [-Reps 2]   -> results\<Name>\motion.md + heat_<variant>.png
param([Parameter(Mandatory)][string]$Name, [Parameter(Mandatory)][string]$Dll, [Parameter(Mandatory)][string]$AB, [double]$Step = 0.12, [int]$Frames = 16, [string]$Cam = '0 0 0 0 0', [int]$Reps = 2)
Add-Type -ReferencedAssemblies System.Drawing @'
using System; using System.IO; using System.Drawing; using System.Drawing.Imaging;
public static class Mot {
  // returns [whole, upper, lower, p99 of per-pixel mean|d2|]; optionally saves a heat map
  public static double[] Analyze(string[] files, string heatPng) {
    int W = 0, H = 0, K = 0; double[] acc = null; int reps = 0;
    foreach (var f in files) { var b = File.ReadAllBytes(f); W = BitConverter.ToInt32(b, 0); H = BitConverter.ToInt32(b, 4); K = BitConverter.ToInt32(b, 8); if (acc == null) acc = new double[W * H];
      for (int k = 1; k < K - 1; k++) { int o0 = 12 + (k - 1) * W * H, o1 = 12 + k * W * H, o2 = 12 + (k + 1) * W * H; for (int i = 0; i < W * H; i++) acc[i] += Math.Abs(b[o2 + i] - 2.0 * b[o1 + i] + b[o0 + i]); }
      reps += K - 2; }
    double all = 0, up = 0, lo = 0; long na = 0, nu = 0, nl = 0; var vals = new double[W * H]; int vi = 0;
    for (int j = 0; j < H; j++) for (int i = 0; i < W; i++) { double v = acc[j * W + i] / reps; acc[j * W + i] = v; if (i > 262 && i < 388 && j > 140 && j < 325) continue; all += v; na++; if (j < H / 2) { up += v; nu++; } else { lo += v; nl++; } vals[vi++] = v; }
    Array.Sort(vals, 0, vi); double p99 = vals[(int)(vi * 0.99)];
    if (heatPng != null) { using (var bm = new Bitmap(W, H, PixelFormat.Format24bppRgb)) { for (int j = 0; j < H; j++) for (int i = 0; i < W; i++) { int g = (int)Math.Min(255, acc[j * W + i] * 25); bm.SetPixel(i, j, Color.FromArgb(g, g, g)); } bm.Save(heatPng, ImageFormat.Png); } }
    return new double[] { all / na, up / nu, lo / nl, p99 }; } }
'@
$out = "C:\CLAUDE\RESEARCH\results\$Name"; New-Item -ItemType Directory -Force $out | Out-Null
$firstIni = ((($AB -split ';')[0]) -split '=', 2)[1].Trim()
$views = (1..$Reps | % { 'mot{0}={1} {2} {3}' -f $_, $Step, $Frames, $Cam }) -join ';'
$lines = & powershell -NoProfile -File C:\CLAUDE\TOOLS\agent_test.ps1 -Dll $Dll -IniFile $firstIni -AB $AB -CamSeq $views -OutDir $out -CamWait 5 2>&1 | % { "$_" }; $lines | Set-Content "$out\harness_output.txt" -Encoding utf8
$vn = @($AB -split ';' | ? { $_.Trim() } | % { ($_ -split '=', 2)[0].Trim() })
$md = "# Motion flicker (mean temporal 2nd difference of luma, 0-255; step $Step deg/frame, $Frames frames x $Reps; Ty excluded)`n| variant | whole | upper | lower | p99 pixel |`n|---|---|---|---|---|`n"; $ok = 0
foreach ($v in $vn) { $files = @(1..$Reps | % { "$out\${v}__mot$_.raw" } | ? { Test-Path $_ }); if (-not $files) { $md += "| $v | no data | | | |`n"; continue }
    $s = [Mot]::Analyze($files, "$out\heat_$v.png"); $ok++; $md += "| $v | {0:N3} | {1:N3} | {2:N3} | {3:N2} |`n" -f $s[0], $s[1], $s[2], $s[3] }
[IO.File]::WriteAllText("$out\motion.md", $md, (New-Object Text.UTF8Encoding $false)); $md
if ($ok -eq $vn.Count) { 'OK motion' } else { 'FAIL missing data' }
