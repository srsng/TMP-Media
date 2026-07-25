[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

Add-Type -AssemblyName PresentationCore
Add-Type -AssemblyName WindowsBase

$repositoryRoot = Split-Path -Parent $PSScriptRoot
$sourceDirectory = Join-Path $repositoryRoot 'assets\mdi'
$outputDirectory = Join-Path $repositoryRoot 'TrafficMonitorMedia\res'
$sizes = @(16, 20, 24, 32, 48)

$icons = [ordered]@{
    'play' = 'play.svg'
    'pause' = 'pause.svg'
    'no-media' = 'music-off.svg'
}

$themes = [ordered]@{
    'on-dark' = '#F5F5F5'
    'on-light' = '#202020'
}

function Get-SvgPathData {
    param([Parameter(Mandatory)][string]$Path)

    [xml]$document = Get-Content -LiteralPath $Path -Raw -Encoding UTF8
    $pathNodes = @($document.SelectNodes("/*[local-name()='svg']/*[local-name()='path']"))
    $pathNode = $pathNodes |
        Where-Object { $_.fill -eq 'currentColor' -and -not [string]::IsNullOrWhiteSpace($_.d) } |
        Select-Object -First 1
    if ($null -eq $pathNode) {
        throw "SVG does not contain a currentColor path: $Path"
    }
    return [string]$pathNode.d
}

function Convert-PathToPng {
    param(
        [Parameter(Mandatory)][string]$PathData,
        [Parameter(Mandatory)][int]$Size,
        [Parameter(Mandatory)][string]$Color
    )

    $geometry = [System.Windows.Media.Geometry]::Parse($PathData)
    $brush = [System.Windows.Media.SolidColorBrush]::new(
        [System.Windows.Media.ColorConverter]::ConvertFromString($Color))
    $brush.Freeze()

    $padding = [Math]::Max(1.0, $Size / 16.0)
    $scale = ($Size - 2.0 * $padding) / 24.0
    $transform = [System.Windows.Media.MatrixTransform]::new($scale, 0.0, 0.0, $scale, $padding, $padding)
    $transform.Freeze()

    $visual = [System.Windows.Media.DrawingVisual]::new()
    $context = $visual.RenderOpen()
    try {
        $context.PushTransform($transform)
        $context.DrawGeometry($brush, $null, $geometry)
        $context.Pop()
    }
    finally {
        $context.Close()
    }

    $bitmap = [System.Windows.Media.Imaging.RenderTargetBitmap]::new(
        $Size,
        $Size,
        96.0,
        96.0,
        [System.Windows.Media.PixelFormats]::Pbgra32)
    $bitmap.Render($visual)

    $encoder = [System.Windows.Media.Imaging.PngBitmapEncoder]::new()
    $encoder.Frames.Add([System.Windows.Media.Imaging.BitmapFrame]::Create($bitmap))
    $stream = [System.IO.MemoryStream]::new()
    try {
        $encoder.Save($stream)
        return $stream.ToArray()
    }
    finally {
        $stream.Dispose()
    }
}

function Write-IcoFile {
    param(
        [Parameter(Mandatory)][string]$OutputPath,
        [Parameter(Mandatory)][object[]]$Frames
    )

    $stream = [System.IO.File]::Open($OutputPath, [System.IO.FileMode]::Create)
    $writer = [System.IO.BinaryWriter]::new($stream)
    try {
        $writer.Write([UInt16]0)
        $writer.Write([UInt16]1)
        $writer.Write([UInt16]$Frames.Count)

        $offset = 6 + 16 * $Frames.Count
        foreach ($frame in $Frames) {
            $writer.Write([byte]$frame.Size)
            $writer.Write([byte]$frame.Size)
            $writer.Write([byte]0)
            $writer.Write([byte]0)
            $writer.Write([UInt16]1)
            $writer.Write([UInt16]32)
            $writer.Write([UInt32]$frame.Bytes.Length)
            $writer.Write([UInt32]$offset)
            $offset += $frame.Bytes.Length
        }

        foreach ($frame in $Frames) {
            [byte[]]$bytes = $frame.Bytes
            $writer.Write($bytes, 0, $bytes.Length)
        }
    }
    finally {
        $writer.Dispose()
        $stream.Dispose()
    }
}

New-Item -ItemType Directory -Force -Path $outputDirectory | Out-Null

foreach ($icon in $icons.GetEnumerator()) {
    $pathData = Get-SvgPathData -Path (Join-Path $sourceDirectory $icon.Value)
    foreach ($theme in $themes.GetEnumerator()) {
        $frames = @(
            foreach ($size in $sizes) {
                [pscustomobject]@{
                    Size = $size
                    Bytes = [byte[]](Convert-PathToPng -PathData $pathData -Size $size -Color $theme.Value)
                }
            }
        )
        $outputPath = Join-Path $outputDirectory ("status-{0}-{1}.ico" -f $icon.Key, $theme.Key)
        Write-IcoFile -OutputPath $outputPath -Frames $frames
        Write-Output "Generated: $outputPath"
    }
}
