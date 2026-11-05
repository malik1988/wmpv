param(
    [string]$Out = "assets\icon.ico"
)

# =============================================================================
#  生成与窗口内 Logo 完全一致的多分辨率 ICO（16/32/48/64/128/256）。
#  造型、比例、配色均直接取自源码，改主题时只需同步下面的常量：
#    * src/ui.cpp  Chrome::drawLogo / 欢迎页大 Logo
#        - 圆角方块半径 = 边长 * 0.30（Scale(9) / 30px）
#        - 外侧光晕外扩 = 边长 * 0.10（3px / 30px），accent 透明度 45/255
#        - 白色播放三角 s = 边长 * 0.16，左 -0.72s，右 +s
#    * src/theme.h  MakeDarkTheme()
#        - accent  #5C8AFF  RGB(92, 138, 255)   渐变起点（上）
#        - accent2 #AA6AFF  RGB(170, 106, 255)  渐变终点（下）
#  渲染核心用内联 C# 做 4x 超采样 + alpha 加权下采样，各尺寸边缘平滑；
#  不依赖 System.Drawing，Windows PowerShell 5.1 与 PowerShell 7 均可运行。
# =============================================================================

$sizes = @(16, 32, 48, 64, 128, 256)
$aa = 4

$aR = 92;  $aG = 138; $aB = 255
$bR = 170; $bG = 106; $bB = 255
$haloA = 45

if (-not ('WmpvLogo' -as [type])) {
    Add-Type -TypeDefinition @'
using System;

public static class WmpvLogo {
    static bool InRound(double x, double y, double cx, double cy,
                        double w, double h, double r) {
        double px = Math.Abs(x - cx) - (w / 2.0 - r);
        double py = Math.Abs(y - cy) - (h / 2.0 - r);
        double ox = Math.Max(px, 0.0), oy = Math.Max(py, 0.0);
        return Math.Sqrt(ox * ox + oy * oy) +
               Math.Min(Math.Max(px, py), 0.0) - r <= 0.0;
    }

    static bool InTri(double x, double y,
                      double ax, double ay, double bx, double by,
                      double cx, double cy) {
        double d1 = (x - bx) * (ay - by) - (ax - bx) * (y - by);
        double d2 = (x - cx) * (by - cy) - (bx - cx) * (y - cy);
        double d3 = (x - ax) * (cy - ay) - (cx - ax) * (y - ay);
        bool neg = d1 < 0 || d2 < 0 || d3 < 0;
        bool pos = d1 > 0 || d2 > 0 || d3 > 0;
        return !(neg && pos);
    }

    // 返回 ICO 需要的 bottom-up 32bpp BGRA 像素
    public static byte[] Render(int size, int aa,
                                int aR, int aG, int aB,
                                int bR, int bG, int bB, int haloA) {
        int W = size * aa;
        double c = W / 2.0;
        double inset = W * 0.10;
        double sq = W - 2.0 * inset;
        double r = sq * 0.30;
        double pad = sq * 0.10;
        double halo = sq + 2.0 * pad;
        double haloR = r + pad * 0.70;
        double top = c - sq / 2.0;

        double ts = sq * 0.16;
        double ax = c - 0.72 * ts, ay = c - ts;
        double bx = c - 0.72 * ts, by = c + ts;
        double tx = c + ts,         ty = c;

        byte[] hi = new byte[W * W * 4];
        for (int yy = 0; yy < W; ++yy) {
            double t = (yy + 0.5 - top) / sq;
            if (t < 0) t = 0; else if (t > 1) t = 1;
            byte gr = (byte)(aR + (bR - aR) * t);
            byte gg = (byte)(aG + (bG - aG) * t);
            byte gb = (byte)(aB + (bB - aB) * t);
            int row = yy * W * 4;
            for (int xx = 0; xx < W; ++xx) {
                double px = xx + 0.5, py = yy + 0.5;
                int i = row + xx * 4;
                if (InRound(px, py, c, c, sq, sq, r)) {
                    hi[i] = gb; hi[i+1] = gg; hi[i+2] = gr; hi[i+3] = 255;
                } else if (InRound(px, py, c, c, halo, halo, haloR)) {
                    hi[i] = (byte)aB; hi[i+1] = (byte)aG; hi[i+2] = (byte)aR;
                    hi[i+3] = (byte)haloA;
                }
                if (px >= ax && px <= tx && py >= ay && py <= by &&
                    InTri(px, py, ax, ay, bx, by, tx, ty)) {
                    hi[i] = 255; hi[i+1] = 255; hi[i+2] = 255; hi[i+3] = 255;
                }
            }
        }

        byte[] o = new byte[size * size * 4];
        for (int y = 0; y < size; ++y) {
            for (int x = 0; x < size; ++x) {
                long sr = 0, sg = 0, sb = 0, sa = 0;
                for (int sy = 0; sy < aa; ++sy) {
                    int hrow = (y * aa + sy) * W * 4;
                    for (int sx = 0; sx < aa; ++sx) {
                        int i = hrow + (x * aa + sx) * 4;
                        int a = hi[i+3];
                        sr += hi[i+2] * a;
                        sg += hi[i+1] * a;
                        sb += hi[i] * a;
                        sa += a;
                    }
                }
                if (sa == 0) continue;
                int oi = ((size - 1 - y) * size + x) * 4;   // flip to bottom-up
                o[oi]     = (byte)(sb / sa);
                o[oi + 1] = (byte)(sg / sa);
                o[oi + 2] = (byte)(sr / sa);
                o[oi + 3] = (byte)(sa / (double)(aa * aa));
            }
        }
        return o;
    }
}
'@
}

# 组装 ICO：32bpp BI_RGB，biHeight = 2*height（XOR + AND），
# 1bpp AND mask 全 0（透明信息由 alpha 通道提供）。
# 用 ArrayList 而非 @() +=，后者会把嵌套 byte[] 摊平。
$imgs = [System.Collections.ArrayList]::new()
foreach ($sz in $sizes) {
    $px = [WmpvLogo]::Render($sz, $aa, $aR, $aG, $aB, $bR, $bG, $bB, $haloA)
    $andRow = [int]([math]::Ceiling($sz / 32.0) * 4)
    $andLen = $andRow * $sz
    $and = New-Object byte[] $andLen
    $sizeImage = $px.Length + $and.Length
    $hdr = [System.Collections.Generic.List[byte]]::new()
    $hdr.AddRange([byte[]]@(40,0,0,0))                       # biSize
    $hdr.AddRange([BitConverter]::GetBytes([uint32]$sz))      # biWidth
    $hdr.AddRange([BitConverter]::GetBytes([uint32]($sz*2)))  # biHeight (XOR + AND)
    $hdr.AddRange([byte[]]@(1,0))                            # planes
    $hdr.AddRange([byte[]]@(32,0))                           # bit count
    $hdr.AddRange([byte[]]@(0,0,0,0))                       # BI_RGB
    $hdr.AddRange([BitConverter]::GetBytes([uint32]$sizeImage))
    $hdr.AddRange([byte[]]@(0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0))
    $img = New-Object byte[] ($hdr.Count + $px.Length + $and.Length)
    [Buffer]::BlockCopy($hdr.ToArray(), 0, $img, 0, $hdr.Count)
    [Buffer]::BlockCopy($px, 0, $img, $hdr.Count, $px.Length)
    [Buffer]::BlockCopy($and, 0, $img, $hdr.Count + $px.Length, $and.Length)
    $imgs.Add($img) | Out-Null
}

$ms = New-Object System.IO.MemoryStream
$bw = New-Object System.IO.BinaryWriter($ms)
$bw.Write([uint16]0)
$bw.Write([uint16]1)
$bw.Write([uint16]$sizes.Count)
$offset = 6 + $sizes.Count * 16
foreach ($i in 0..($sizes.Count - 1)) {
    $sz = $sizes[$i]
    $wb = if ($sz -ge 256) { [byte]0 } else { [byte]$sz }
    $bytesInRes = $imgs[$i].Length
    $bw.Write($wb); $bw.Write($wb); $bw.Write([byte]0); $bw.Write([byte]0)
    $bw.Write([byte]1); $bw.Write([byte]0); $bw.Write([byte]32); $bw.Write([byte]0)
    $bw.Write([uint32]$bytesInRes)
    $bw.Write([uint32]$offset)
    $offset += $bytesInRes
}
foreach ($img in $imgs) {
    $bw.Write($img)
}
[IO.File]::WriteAllBytes($Out, $ms.ToArray())
Write-Output "Wrote $Out"
