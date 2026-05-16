param(
    [string]$BuildDir = "build-debug",
    [int]$TimeoutSeconds = 12,
    [int]$Demo = 0,
    [string]$ReferenceManifest = "",
    [int]$ScreenshotCount = 9,
    [int]$ScreenshotStride = 18,
    [switch]$SkipAssetCheck,
    [switch]$Verbose
)

if (-not [System.IO.Path]::IsPathRooted($BuildDir)) {
    $BuildDir = [System.IO.Path]::GetFullPath((Join-Path (Get-Location).Path $BuildDir))
}

if (-not (Test-Path $BuildDir)) {
    throw "Build directory not found: $BuildDir"
}

if ($Demo -lt 0 -or $Demo -gt 3) {
    throw "Demo must be between 0 and 3"
}

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$AssetManifest = "scripts/assets-manifest.sha256"
if (-not [System.IO.Path]::IsPathRooted($AssetManifest)) {
    $AssetManifest = Join-Path $repoRoot $AssetManifest
}

if ([string]::IsNullOrWhiteSpace($ReferenceManifest)) {
    $ReferenceManifest = "scripts/wolf3d-autoshot-demo$Demo-reference.sha256"
}
$repoRootRef = $ReferenceManifest
if (-not [System.IO.Path]::IsPathRooted($repoRootRef)) {
    $repoRootRef = Join-Path $repoRoot $repoRootRef
}
if (-not (Test-Path $repoRootRef)) {
    throw "Reference manifest not found: $ReferenceManifest (resolved: $repoRootRef)"
}

$env:WOLF3D_VERIFY_DEMO = [string]$Demo
$env:WOLF3D_SCREENSHOT = "1"
$env:WOLF3D_SCREENSHOT_STRIDE = [string]$ScreenshotStride
$env:WOLF3D_SCREENSHOT_COUNT = [string]$ScreenshotCount
if (-not $env:WOLF3D_SCREENSHOT_SKIP) { $env:WOLF3D_SCREENSHOT_SKIP = "0" }
$env:WOLF3D_SCREENSHOT_EXIT = "1"
$env:WOLF3D_SCREENSHOT_EXPECTED = [string]$ScreenshotCount
$env:WOLF3D_REFERENCE_MANIFEST = $ReferenceManifest
$env:SDL_VIDEODRIVER = "dummy"
$env:SDL_AUDIODRIVER = "dummy"

if ($Verbose) {
    Write-Host "verify-ci: buildDir=$BuildDir"
    Write-Host "verify-ci: demo=$Demo"
    Write-Host "verify-ci: reference=$ReferenceManifest"
    Write-Host "verify-ci: screenshot_count=$ScreenshotCount"
    Write-Host "verify-ci: screenshot_stride=$ScreenshotStride"
    Write-Host "verify-ci: screenshot_skip=$($env:WOLF3D_SCREENSHOT_SKIP)"
}

if ([string]::IsNullOrWhiteSpace($env:WOLF3D_REFERENCE_MANIFEST)) {
    $env:WOLF3D_REFERENCE_MANIFEST = ""
}

& (Join-Path $PSScriptRoot "verify.ps1") -BuildDir $BuildDir -TimeoutSeconds $TimeoutSeconds -Verbose:$Verbose

if (-not $SkipAssetCheck) {
    Push-Location $repoRoot
    try {
        & (Join-Path $PSScriptRoot "check-assets.ps1") -ManifestPath $AssetManifest -Mode "check"
    } finally {
        Pop-Location
    }
}

if ($Verbose) {
    Write-Host "verify-ci: complete"
}
