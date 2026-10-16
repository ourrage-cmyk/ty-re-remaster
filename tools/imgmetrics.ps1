# Image metrics for objective A/B checks (dot-source: . C:\CLAUDE\TOOLS\imgmetrics.ps1)
#   [ImgM]::Mad(a,b,x,y,w,h)        mean absolute colour difference (0-255) in a region
#   [ImgM]::GradCorr(a,b,x,y,w,h)   Pearson correlation (-1..1) of the Sobel edge-magnitude maps in a region: colour/fog changes barely move it,
#                                   exploded or garbled geometry (e.g. Ty's skinned mesh) drops it sharply. Rule of thumb from calibration: < 0.55 = SUSPECT.
#   [ImgM]::MeanLuma / ::MeanSat    region means (for "did far rock get less saturated" style checks)
# Regions are in 2560x1440 pixels. Named regions: Ty (default view) 1050,560,500,740 ; Rock wall 1100,80,1200,500.
Add-Type -AssemblyName System.Drawing
if (-not ('ImgM' -as [type])) {
Add-Type -ReferencedAssemblies System.Drawing @'
using System; using System.Drawing; using System.Drawing.Imaging;
public static class ImgM {
  static byte[] Load(string p, int x, int y, int w, int h, out int stride, out int W, out int H) {
    using (var A = new Bitmap(p)) { W = A.Width; H = A.Height; x = Math.Min(x, W - 1); y = Math.Min(y, H - 1); w = Math.Min(w, W - x); h = Math.Min(h, H - y);
      var r = new Rectangle(x, y, w, h); var d = A.LockBits(r, ImageLockMode.ReadOnly, PixelFormat.Format24bppRgb); stride = d.Stride; var buf = new byte[d.Stride * h];
      System.Runtime.InteropServices.Marshal.Copy(d.Scan0, buf, 0, buf.Length); A.UnlockBits(d); W = w; H = h; return buf; } }
  public static double Mad(string a, string b, int x, int y, int w, int h) {
    int sa, sb, W, H, W2, H2; var pa = Load(a, x, y, w, h, out sa, out W, out H); var pb = Load(b, x, y, w, h, out sb, out W2, out H2); w = Math.Min(W, W2); h = Math.Min(H, H2);
    double sum = 0; long n = 0; for (int j = 0; j < h; j += 2) for (int i = 0; i < w; i += 2) { int oa = j * sa + i * 3, ob = j * sb + i * 3; sum += (Math.Abs(pa[oa] - pb[ob]) + Math.Abs(pa[oa + 1] - pb[ob + 1]) + Math.Abs(pa[oa + 2] - pb[ob + 2])) / 3.0; n++; }
    return n == 0 ? 0 : sum / n; }
  static float[] Edge(byte[] p, int stride, int w, int h) {
    var g = new float[w * h]; for (int j = 0; j < h; j++) for (int i = 0; i < w; i++) { int o = j * stride + i * 3; g[j * w + i] = 0.299f * p[o + 2] + 0.587f * p[o + 1] + 0.114f * p[o]; }
    var e = new float[w * h]; for (int j = 1; j < h - 1; j++) for (int i = 1; i < w - 1; i++) { int k = j * w + i;
      float gx = -g[k - w - 1] - 2 * g[k - 1] - g[k + w - 1] + g[k - w + 1] + 2 * g[k + 1] + g[k + w + 1]; float gy = -g[k - w - 1] - 2 * g[k - w] - g[k - w + 1] + g[k + w - 1] + 2 * g[k + w] + g[k + w + 1];
      e[k] = (float)Math.Sqrt(gx * gx + gy * gy); } return e; }
  public static double GradCorr(string a, string b, int x, int y, int w, int h) {
    int sa, sb, W, H, W2, H2; var pa = Load(a, x, y, w, h, out sa, out W, out H); var pb = Load(b, x, y, w, h, out sb, out W2, out H2); w = Math.Min(W, W2); h = Math.Min(H, H2);
    var ea = Edge(pa, sa, w, h); var eb = Edge(pb, sb, w, h); double ma = 0, mb = 0; int n = w * h; for (int k = 0; k < n; k++) { ma += ea[k]; mb += eb[k]; } ma /= n; mb /= n;
    double sab = 0, saa = 0, sbb = 0; for (int k = 0; k < n; k++) { double da = ea[k] - ma, db = eb[k] - mb; sab += da * db; saa += da * da; sbb += db * db; }
    return (saa <= 0 || sbb <= 0) ? 0 : sab / Math.Sqrt(saa * sbb); }
  public static double MeanLuma(string a, int x, int y, int w, int h) { int s, W, H; var p = Load(a, x, y, w, h, out s, out W, out H); double sum = 0; long n = 0; for (int j = 0; j < H; j += 2) for (int i = 0; i < W; i += 2) { int o = j * s + i * 3; sum += 0.299 * p[o + 2] + 0.587 * p[o + 1] + 0.114 * p[o]; n++; } return n == 0 ? 0 : sum / n; }
  public static double MeanSat(string a, int x, int y, int w, int h) { int s, W, H; var p = Load(a, x, y, w, h, out s, out W, out H); double sum = 0; long n = 0; for (int j = 0; j < H; j += 2) for (int i = 0; i < W; i += 2) { int o = j * s + i * 3; double r = p[o + 2], g = p[o + 1], b = p[o]; double mx = Math.Max(r, Math.Max(g, b)), mn = Math.Min(r, Math.Min(g, b)); sum += mx <= 0 ? 0 : (mx - mn) / mx; n++; } return n == 0 ? 0 : sum / n; }
}
'@ }
