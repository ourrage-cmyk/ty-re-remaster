# RKV (Krome Merkury, Ty 1 PC) lister/extractor.  Layout (from Dnawrkshp/ty-1-tools): [data][file entries 0x40 each][dir entries 0x100 each][u32 fileCount][u32 dirCount]
# List :  powershell -File rkvtool.ps1 -Rkv <file> -Mode list [-Filter regex] [-Csv out.csv]
# Repack : powershell -File rkvtool.ps1 -Rkv <orig.rkv> -Mode repack -Src <dir with replacement files by relative path> -Out <new.rkv>  (entries keep all fields; data re-laid 2048-aligned)
# Extract: powershell -File rkvtool.ps1 -Rkv <file> -Mode extract -Filter regex -OutDir <dir>
param([string]$Rkv, [ValidateSet('list','extract','repack')][string]$Mode='list', [string]$Filter='', [string]$OutDir='', [string]$Csv='', [string]$Src='', [string]$Out='')
Add-Type -TypeDefinition @'
using System; using System.IO; using System.Text; using System.Collections.Generic;
public class RkvEntry { public string Dir, Name; public int Size, Offset, Unk28, Unk30, LastWrite, Unk38, Unk3C; public string Path { get { return (Dir.Length>0 ? Dir.TrimEnd('\\','/') + "\\" : "") + Name; } } }
public static class Rkv {
  static string Str(byte[] b, int o, int n){ int e=o; while(e<o+n && b[e]!=0) e++; return Encoding.GetEncoding(28591).GetString(b,o,e-o); }
  public static List<RkvEntry> Read(string path){
    var res = new List<RkvEntry>();
    using(var fs = File.OpenRead(path)){
      var br = new BinaryReader(fs);
      fs.Seek(-8, SeekOrigin.End); int fc = br.ReadInt32(); int dc = br.ReadInt32();
      long footer = fs.Length - 8 - (long)dc*0x100 - (long)fc*0x40;
      fs.Seek(footer, SeekOrigin.Begin);
      byte[] fb = br.ReadBytes(fc*0x40); byte[] db = br.ReadBytes(dc*0x100);
      var dirs = new string[dc]; for(int i=0;i<dc;i++) dirs[i]=Str(db,i*0x100,0x100);
      for(int i=0;i<fc;i++){ int o=i*0x40; var e=new RkvEntry();
        e.Name=Str(fb,o,0x20); int di=BitConverter.ToInt32(fb,o+0x20); e.Dir=(di>=0&&di<dc)?dirs[di]:"?";
        e.Size=BitConverter.ToInt32(fb,o+0x24); e.Unk28=BitConverter.ToInt32(fb,o+0x28); e.Offset=BitConverter.ToInt32(fb,o+0x2C);
        e.Unk30=BitConverter.ToInt32(fb,o+0x30); e.LastWrite=BitConverter.ToInt32(fb,o+0x34); e.Unk38=BitConverter.ToInt32(fb,o+0x38); e.Unk3C=BitConverter.ToInt32(fb,o+0x3C);
        res.Add(e); }
    }
    return res;
  }
  public static void Repack(string origPath, string srcDir, string outPath){
    int fc, dc; byte[] fb, db;
    using(var fs = File.OpenRead(origPath)){ var br=new BinaryReader(fs); fs.Seek(-8, SeekOrigin.End); fc=br.ReadInt32(); dc=br.ReadInt32();
      long footer = fs.Length - 8 - (long)dc*0x100 - (long)fc*0x40; fs.Seek(footer, SeekOrigin.Begin); fb=br.ReadBytes(fc*0x40); db=br.ReadBytes(dc*0x100); }
    var ents = Read(origPath);
    using(var src = File.OpenRead(origPath)) using(var o = File.Create(outPath)){
      long pos = 0;
      for(int i=0;i<fc;i++){
        var e = ents[i]; byte[] data; string rep = System.IO.Path.Combine(srcDir, e.Path);
        if(File.Exists(rep)) data = File.ReadAllBytes(rep);
        else { src.Seek((uint)e.Offset, SeekOrigin.Begin); data = new byte[e.Size]; int r=0; while(r<data.Length){ int n=src.Read(data,r,data.Length-r); if(n<=0)break; r+=n; } }
        long aligned = (pos + 2047) / 2048 * 2048; if(aligned > pos){ o.Write(new byte[aligned-pos],0,(int)(aligned-pos)); pos = aligned; }
        o.Write(data,0,data.Length);
        BitConverter.GetBytes(data.Length).CopyTo(fb, i*0x40+0x24);
        BitConverter.GetBytes((int)pos).CopyTo(fb, i*0x40+0x2C);
        pos += data.Length;
      }
      long aligned2 = (pos + 2047) / 2048 * 2048; if(aligned2 > pos){ o.Write(new byte[aligned2-pos],0,(int)(aligned2-pos)); }
      o.Write(fb,0,fb.Length); o.Write(db,0,db.Length);
      o.Write(BitConverter.GetBytes(fc),0,4); o.Write(BitConverter.GetBytes(dc),0,4);
    }
  }  public static void Extract(string path, RkvEntry e, string outFile){
    Directory.CreateDirectory(System.IO.Path.GetDirectoryName(outFile));
    using(var fs=File.OpenRead(path)) using(var o=File.Create(outFile)){ fs.Seek((uint)e.Offset, SeekOrigin.Begin); var buf=new byte[Math.Max(0,e.Size)]; int r=0; while(r<buf.Length){ int n=fs.Read(buf,r,buf.Length-r); if(n<=0)break; r+=n; } o.Write(buf,0,r); }
  }
}
'@
if($Mode -eq 'repack'){ [Rkv]::Repack($Rkv,$Src,$Out); ('repacked -> {0} ({1} bytes)' -f $Out,(Get-Item $Out).Length); return }
$all = [Rkv]::Read($Rkv)
$sel = if($Filter){ $all | ? { $_.Path -match $Filter } } else { $all }
if($Mode -eq 'list'){
  if($Csv){ $sel | Select Path,Size,Offset,Unk28,Unk30,LastWrite | Export-Csv $Csv -NoTypeInformation -Encoding utf8 }
  "{0} entries total, {1} selected" -f $all.Count, @($sel).Count
  $sel | Select -First 200 | % { "{0,10}  {1}" -f $_.Size, $_.Path }
} else {
  foreach($e in $sel){ [Rkv]::Extract($Rkv, $e, (Join-Path $OutDir $e.Path)) }
  "extracted {0} files to {1}" -f @($sel).Count, $OutDir
}

