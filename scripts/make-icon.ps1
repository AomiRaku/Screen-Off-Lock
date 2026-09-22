#Requires -Version 7.0
<#
    生成程序图标：src\app.ico

    图标用 Windows 11 自带的 Segoe Fluent Icons 字形拼成：
        U+E7F4  显示器
        U+E72E  闭合的锁
    因此它的风格和系统本身完全一致，也不需要引入任何外部素材。
    成品是一个 Win11 风格的圆角方块底 + 白色“显示器 + 锁”。

    用法：pwsh -File scripts\make-icon.ps1
#>

[CmdletBinding()]
param(
    [string]$Output = (Join-Path (Split-Path -Parent $PSScriptRoot) 'src\app.ico')
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

$BgColor = [System.Drawing.Color]::FromArgb(255, 15, 108, 188)   # 纯色底，不用渐变
$Sizes   = @(16, 20, 24, 32, 40, 48, 64, 96, 128, 256)

function New-RoundedRectPath([single]$x, [single]$y, [single]$w, [single]$h, [single]$r) {
    $p = New-Object System.Drawing.Drawing2D.GraphicsPath
    $d = $r * 2
    $p.AddArc($x, $y, $d, $d, 180, 90)
    $p.AddArc(($x + $w - $d), $y, $d, $d, 270, 90)
    $p.AddArc(($x + $w - $d), ($y + $h - $d), $d, $d, 0, 90)
    $p.AddArc($x, ($y + $h - $d), $d, $d, 90, 90)
    $p.CloseFigure()
    return $p
}

function New-IconBitmap([int]$S) {
    $bmp = New-Object System.Drawing.Bitmap($S, $S, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode     = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $g.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::AntiAlias
    $g.Clear([System.Drawing.Color]::Transparent)

    # 纯色底
    $path = New-RoundedRectPath ($S * 0.03) ($S * 0.03) ($S * 0.94) ($S * 0.94) ($S * 0.22)
    $bg   = New-Object System.Drawing.SolidBrush($BgColor)
    $g.FillPath($bg, $path)

    # 一把锁：锁体是实心圆角矩形，锁梁是圆头粗弧线。
    # 刻意不用图标字体的线条字形——那些笔画在 16px 下只有半个像素，会糊成一团。
    $body = New-RoundedRectPath ($S * 0.28) ($S * 0.50) ($S * 0.44) ($S * 0.36) ($S * 0.09)
    $g.FillPath([System.Drawing.Brushes]::White, $body)

    $pen = New-Object System.Drawing.Pen([System.Drawing.Color]::White, ($S * 0.085))
    $pen.StartCap = [System.Drawing.Drawing2D.LineCap]::Round
    $pen.EndCap   = [System.Drawing.Drawing2D.LineCap]::Round
    $arcRect = New-Object System.Drawing.RectangleF(($S * 0.35), ($S * 0.25), ($S * 0.30), ($S * 0.30))
    $g.DrawArc($pen, $arcRect, 180, 180)

    $pen.Dispose()
    $bg.Dispose()
    $path.Dispose()
    $body.Dispose()
    $g.Dispose()
    return $bmp
}

# --- 各尺寸渲染为 PNG，再打包成 ICO ---------------------------------------
# ICO 内嵌 PNG 需要 Vista 及以上；目标系统是 Windows 11，没有问题。

$images = @()
foreach ($s in $Sizes) {
    $bmp = New-IconBitmap $s
    $ms  = New-Object System.IO.MemoryStream
    $bmp.Save($ms, [System.Drawing.Imaging.ImageFormat]::Png)
    $images += ,@{ Size = $s; Data = $ms.ToArray() }
    $ms.Dispose()
    $bmp.Dispose()
}

$dir = Split-Path -Parent $Output
if (-not (Test-Path $dir)) { New-Item -ItemType Directory -Path $dir | Out-Null }

$fs = [System.IO.File]::Create($Output)
$bw = New-Object System.IO.BinaryWriter($fs)

$bw.Write([uint16]0)                  # 保留位
$bw.Write([uint16]1)                  # 1 = 图标
$bw.Write([uint16]$images.Count)

$offset = 6 + 16 * $images.Count
foreach ($img in $images) {
    $dim = if ($img.Size -ge 256) { 0 } else { $img.Size }   # 0 表示 256
    $bw.Write([byte]$dim)             # 宽
    $bw.Write([byte]$dim)             # 高
    $bw.Write([byte]0)                # 调色板色数
    $bw.Write([byte]0)                # 保留
    $bw.Write([uint16]1)              # 平面数
    $bw.Write([uint16]32)             # 位深
    $bw.Write([uint32]$img.Data.Length)
    $bw.Write([uint32]$offset)
    $offset += $img.Data.Length
}
foreach ($img in $images) { $bw.Write($img.Data) }

$bw.Flush()
$bw.Dispose()
$fs.Dispose()

Write-Host ("[+] {0}  ({1} 个尺寸, {2:N1} KB)" -f `
    $Output, $images.Count, ((Get-Item $Output).Length / 1KB)) -ForegroundColor Green
