# A/B image comparison for same-spot screenshots. Writes an amplified |A-B| luminance map and prints stats.
# Usage: powershell -NoProfile -File imgdiff.ps1 -A off.png -B on.png -Out diff.png [-Gain 8] [-Region "x,y,w,h"]
# Output stats: mean luminance A/B, mean |diff|, % pixels darker in B by > 2/255 (whole image or region).
param([Parameter(Mandatory)][string]$A, [Parameter(Mandatory)][string]$B, [string]$Out = '', [double]$Gain = 8, [string]$Region = '')
Add-Type -AssemblyName System.Drawing
Add-Type -ReferencedAssemblies System.Drawing @'
using System; using System.Drawing; using System.Drawing.Imaging; using System.Runtime.InteropServices;
public static class ImgDiff {
  static byte[] Bytes(Bitmap b, out int stride) {
    var d = b.LockBits(new Rectangle(0,0,b.Width,b.Height), ImageLockMode.ReadOnly, PixelFormat.Format24bppRgb);
    stride = d.Stride; var buf = new byte[stride*b.Height]; Marshal.Copy(d.Scan0, buf, 0, buf.Length); b.UnlockBits(d); return buf; }
  public static string Run(string pa, string pb, string outp, double gain, int rx, int ry, int rw, int rh) {
    var a = new Bitmap(pa); var b = new Bitmap(pb);
    if (a.Width != b.Width || a.Height != b.Height) return "size mismatch";
    int w = a.Width, h = a.Height, sa, sb; var A = Bytes(a, out sa); var B = Bytes(b, out sb);
    if (rw <= 0) { rx = 0; ry = 0; rw = w; rh = h; }
    var o = new Bitmap(w, h, PixelFormat.Format24bppRgb);
    var od = o.LockBits(new Rectangle(0,0,w,h), ImageLockMode.WriteOnly, PixelFormat.Format24bppRgb); var O = new byte[od.Stride*h];
    double la = 0, lb = 0, ad = 0; long n = 0, darker = 0;
    for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {
      int i = y*sa + x*3, j = y*sb + x*3;
      double ya = 0.114*A[i] + 0.587*A[i+1] + 0.299*A[i+2], yb = 0.114*B[j] + 0.587*B[j+1] + 0.299*B[j+2];
      double d = ya - yb;  // positive = B darker
      int v = (int)Math.Min(255, Math.Abs(d) * gain); int k = y*od.Stride + x*3;
      if (d > 0) { O[k] = 0; O[k+1] = 0; O[k+2] = (byte)v; } else { O[k] = (byte)v; O[k+1] = (byte)v; O[k+2] = 0; }   // red = B darker, cyan = B brighter
      if (x >= rx && x < rx+rw && y >= ry && y < ry+rh) { la += ya; lb += yb; ad += Math.Abs(d); n++; if (d > 2) darker++; }
    }
    Marshal.Copy(O, 0, od.Scan0, O.Length); o.UnlockBits(od);
    if (outp.Length > 0) o.Save(outp, ImageFormat.Png);
    a.Dispose(); b.Dispose(); o.Dispose();
    return String.Format("region {0},{1} {2}x{3}: meanY A={4:F1} B={5:F1}  mean|d|={6:F2}  darkerInB(>2)={7:F1}%", rx, ry, rw, rh, la/n, lb/n, ad/n, 100.0*darker/n);
  }
}
'@
$r = if ($Region) { $Region -split ',' | % { [int]$_ } } else { @(0, 0, 0, 0) }
[ImgDiff]::Run((Resolve-Path $A).Path, (Resolve-Path $B).Path, $Out, $Gain, $r[0], $r[1], $r[2], $r[3])
