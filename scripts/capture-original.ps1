param(
    [string]$DosboxPath = "",
    [string]$ConfigPath = "assets/DOSBox/wolf3d.conf",
    [string]$AssetsBase = "assets/base",
    [string]$CaptureDir = "build/original-capture",
    [string]$ManifestPath = "scripts/wolf3d-autoshot-original.sha256",
    [int]$TimeoutSeconds = 120,
    [int]$ExpectedShots = 0,
    [string[]]$AutoexecCommands = @(),
    [switch]$AutoScreenshot,
    [int]$AutoScreenshotIntervalMs = 700,
    [switch]$NoLauncher,
    [switch]$Verbose
)

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$assetsBase = Join-Path $repoRoot $AssetsBase
$captureDir = Join-Path $repoRoot $CaptureDir
$manifestFull = if ([System.IO.Path]::IsPathRooted($ManifestPath)) { $ManifestPath } else { Join-Path $repoRoot $ManifestPath }
$baseConfig = if ([System.IO.Path]::IsPathRooted($ConfigPath)) { $ConfigPath } else { Join-Path $repoRoot $ConfigPath }

if (-not (Test-Path $assetsBase)) {
    throw "Assets base directory not found: $assetsBase"
}
if (-not (Test-Path $baseConfig)) {
    throw "DOSBox config not found: $baseConfig"
}

if (-not $DosboxPath) {
    $defaultDosbox = Join-Path $repoRoot "assets\DOSBox\DOSBox.exe"
    if (Test-Path $defaultDosbox) {
        $DosboxPath = $defaultDosbox
    } else {
        $DosboxPath = "dosbox"
    }
}

if (Test-Path $DosboxPath) {
    if ((Get-Item $DosboxPath).PSIsContainer) {
        throw "DOSBox path is a directory, not an executable: $DosboxPath"
    }
} elseif (-not (Get-Command $DosboxPath -ErrorAction SilentlyContinue)) {
    throw "DOSBox executable not found: $DosboxPath"
}

if (-not $AutoexecCommands -or $AutoexecCommands.Count -eq 0) {
    $dosAssetsPath = $assetsBase -replace '/', '\'
    $AutoexecCommands = @(
        "mount C `"$dosAssetsPath`"",
        "C:",
        $(if ($NoLauncher) { "wolf3d.exe" } else { "launcher\\start.bat" })
    )
}

if ($AutoScreenshot) {
    Add-Type -AssemblyName System.Windows.Forms | Out-Null
    Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;

public class NativeWindow {
    [DllImport("user32.dll", SetLastError = true)]
    public static extern bool SetForegroundWindow(IntPtr hWnd);
}
"@ -PassThru | Out-Null
}

New-Item -ItemType Directory -Path $captureDir -Force | Out-Null

$tmpConfig = New-TemporaryFile
$baseConfigLines = [System.IO.File]::ReadAllLines($baseConfig)
$baseConfigNoAutoexec = [System.Collections.Generic.List[string]]::new()
$inAutoexec = $false
foreach ($line in $baseConfigLines) {
    if (-not $inAutoexec -and $line -match '^\s*\[autoexec\]\s*$') {
        $inAutoexec = $true
        continue
    }
    if ($inAutoexec -and $line -match '^\s*\[[^]]+\]\s*$') {
        $inAutoexec = $false
    }
    if (-not $inAutoexec) {
        $baseConfigNoAutoexec.Add($line)
    }
}
$baseConfigText = [string]::Join("`r`n", $baseConfigNoAutoexec.ToArray())
$patchedConfig = [string]$baseConfigText
$patchedConfig = $patchedConfig.TrimEnd() + "`r`n`r`n"
$patchedConfig += "[dosbox]`r`ncaptures={0}`r`n`r`n" -f ($captureDir -replace '/', '\')
$patchedConfig += "[autoexec]`r`n@ECHO OFF`r`n"
foreach ($command in $AutoexecCommands) {
    $patchedConfig += "$command`r`n"
}
$patchedConfig += "exit`r`n"

[System.IO.File]::WriteAllText($tmpConfig.FullName, $patchedConfig)

if ($Verbose) {
    Write-Host "capture-original: using DOSBox config = $($tmpConfig.FullName)"
    Write-Host "capture-original: capture dir = $captureDir"
}

$args = @("-conf", $tmpConfig.FullName)
$windowStyle = if ($AutoScreenshot) { "Normal" } else { "Hidden" }
$process = Start-Process -FilePath $DosboxPath -ArgumentList $args -PassThru -WindowStyle $windowStyle -ErrorAction Stop

if ($AutoScreenshot -and $ExpectedShots -gt 0) {
    $shotIntervalMs = if ($AutoScreenshotIntervalMs -lt 250) { 250 } else { $AutoScreenshotIntervalMs }
    $startDelaySeconds = 4
    Start-Sleep -Seconds $startDelaySeconds
    for ($i = 0; $i -lt $ExpectedShots; $i++) {
        if ($process.HasExited) { break }
        if ($process.MainWindowHandle -ne 0) {
            [NativeWindow]::SetForegroundWindow($process.MainWindowHandle) | Out-Null
        }
        [System.Windows.Forms.SendKeys]::SendWait("^{F5}")
        if ($i -lt ($ExpectedShots - 1)) {
            Start-Sleep -Milliseconds $shotIntervalMs
        }
    }
}

try {
    $stopped = $process.WaitForExit($TimeoutSeconds * 1000)
    if (-not $stopped) {
        if ($Verbose) { Write-Warning "capture-original: timeout reached, stopping DOSBox" }
        Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
        $process.WaitForExit(2000) | Out-Null
    }
} finally {
    if ($null -ne $process -and -not $process.HasExited) {
        Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
        $process.WaitForExit(2000) | Out-Null
    }
    if (Test-Path $tmpConfig) { Remove-Item -Path $tmpConfig.FullName -Force }
}

$shots = Get-ChildItem -Path $captureDir -File | Where-Object {
    ($_.Extension -ieq ".bmp" -and $_.Name -like "autoshot_*.bmp") -or $_.Extension -ieq ".png"
} | Sort-Object Name
if (-not $shots -or $shots.Count -eq 0) {
    throw "capture-original: no autoshot_*.bmp files found in $captureDir"
}
if ($ExpectedShots -gt 0 -and $shots.Count -ne $ExpectedShots) {
    throw "capture-original: screenshot count mismatch. expected=$ExpectedShots got=$($shots.Count)"
}

$manifestLines = foreach ($shot in $shots) {
    $hash = Get-FileHash -Path $shot.FullName -Algorithm SHA256
    "{0}  {1}" -f $hash.Hash.ToLowerInvariant(), $shot.Name
}
Set-Content -Path $manifestFull -Value $manifestLines -Encoding utf8

Write-Host ("capture-original: wrote manifest: {0}" -f $manifestFull)
Write-Host ("capture-original: captured {0} screenshots in {1}" -f $shots.Count, $captureDir)
