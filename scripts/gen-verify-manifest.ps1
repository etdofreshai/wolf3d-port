param(
    [string]$BuildDir = "build-debug",
    [int]$Demo = 0,
    [int]$ScreenshotStride = 18,
    [int]$ScreenshotCount = 9,
    [string]$ReferencePath = "",
    [int]$TimeoutSeconds = 12,
    [switch]$Verbose
)

if ($Demo -lt 0 -or $Demo -gt 3) {
    throw "Demo must be between 0 and 3"
}

if (-not (Test-Path $BuildDir)) {
    throw "Build directory not found: $BuildDir"
}
$buildDirFull = (Resolve-Path $BuildDir).Path

if (-not $ReferencePath) {
    $ReferencePath = "scripts/wolf3d-autoshot-demo${Demo}-reference.sha256"
}
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
if (-not [System.IO.Path]::IsPathRooted($ReferencePath)) {
    $ReferencePath = Join-Path $repoRoot $ReferencePath
}

# Preserve deterministic demo-mode verification settings.
$env:WOLF3D_VERIFY_DEMO = [string]$Demo
$env:WOLF3D_SCREENSHOT = "1"
$env:WOLF3D_SCREENSHOT_STRIDE = [string]$ScreenshotStride
$env:WOLF3D_SCREENSHOT_COUNT = [string]$ScreenshotCount
$env:WOLF3D_SCREENSHOT_EXIT = "1"
$env:WOLF3D_SCREENSHOT_EXPECTED = [string]$ScreenshotCount
$env:WOLF3D_REFERENCE_MANIFEST = ""

# Run the existing verifier and capture the generated manifest.
if ($Verbose) {
    Write-Host "Generating reference manifest from demo $Demo in $BuildDir"
}

& (Join-Path $PSScriptRoot "verify.ps1") -BuildDir $BuildDir -TimeoutSeconds $TimeoutSeconds -Verbose:$Verbose

$candidates = Get-ChildItem -Path $buildDirFull -Recurse -File
if ($Verbose) {
    Write-Host "Manifest candidates under ${buildDirFull}:"
    $candidates | Where-Object { $_.Name -eq "wolf3d-autoshot.sha256" } | ForEach-Object { Write-Host (" - $($_.FullName)") }
}

$generated = $candidates |
    Where-Object { $_.Name -eq "wolf3d-autoshot.sha256" } |
    Sort-Object FullName |
    Select-Object -First 1

if (-not $generated) {
    throw "Could not find generated wolf3d-autoshot.sha256 under $BuildDir"
}

$manifestPath = Join-Path $generated.DirectoryName "wolf3d-autoshot.sha256"

Copy-Item -Path $manifestPath -Destination $ReferencePath -Force
Write-Host "Wrote reference manifest: $ReferencePath"
Write-Host "Source manifest: $manifestPath"
