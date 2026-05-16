param(
    [string]$BuildDir = "build-debug",
    [int]$TimeoutSeconds = 12,
    [switch]$Verbose
)

$exeCandidates = @(
    (Join-Path $BuildDir "wolf3d.exe"),
    (Join-Path (Join-Path $BuildDir "Debug") "wolf3d.exe"),
    (Join-Path $BuildDir "wolf3d"),
    (Join-Path (Join-Path $BuildDir "Debug") "wolf3d"),
    (Join-Path (Join-Path $BuildDir "Release") "wolf3d.exe"),
    (Join-Path (Join-Path $BuildDir "Release") "wolf3d"),
    (Join-Path $BuildDir "Debug\wolf3d.exe"),
    (Join-Path $BuildDir "Release\wolf3d.exe")
)

$exe = $null
foreach ($candidate in $exeCandidates) {
    if (Test-Path $candidate) {
        $exe = (Resolve-Path $candidate).Path
        break
    }
}

if (-not $exe) {
    throw "Could not find executable in $BuildDir"
}

$shotDir = Split-Path -Parent $exe
$logFile = Join-Path $shotDir "wolf3d-verify.log"
$stdoutFile = Join-Path $shotDir "wolf3d-verify.stdout.log"
$stderrFile = Join-Path $shotDir "wolf3d-verify.stderr.log"
$manifestFile = Join-Path $shotDir "wolf3d-autoshot.sha256"
$expectedShots = $null

if (-not $env:WOLF3D_SCREENSHOT) { $env:WOLF3D_SCREENSHOT = "1" }
if (-not $env:WOLF3D_SCREENSHOT_STRIDE) { $env:WOLF3D_SCREENSHOT_STRIDE = "18" }
if (-not $env:WOLF3D_SCREENSHOT_COUNT) { $env:WOLF3D_SCREENSHOT_COUNT = "9" }
if (-not $env:WOLF3D_SCREENSHOT_SKIP) { $env:WOLF3D_SCREENSHOT_SKIP = "0" }
if (-not $env:WOLF3D_SCREENSHOT_EXIT) { $env:WOLF3D_SCREENSHOT_EXIT = "1" }
if (-not $env:WOLF3D_VERIFY_DEMO) { $env:WOLF3D_VERIFY_DEMO = "0" }
if (-not $env:WOLF3D_REFERENCE_MANIFEST) { $env:WOLF3D_REFERENCE_MANIFEST = "" }
if ($env:WOLF3D_SCREENSHOT_EXPECTED) { $expectedShots = $env:WOLF3D_SCREENSHOT_EXPECTED }
if (-not $expectedShots) { $expectedShots = $env:WOLF3D_SCREENSHOT_COUNT }
if (-not $env:SDL_VIDEODRIVER) {
    $env:SDL_VIDEODRIVER = "dummy"
}
if (-not $env:SDL_AUDIODRIVER) {
    $env:SDL_AUDIODRIVER = "dummy"
}

if ($Verbose) {
    Write-Host "Using executable: $exe"
    Write-Host "Screenshot directory: $shotDir"
    Write-Host "Timeout: ${TimeoutSeconds}s"
    Write-Host "WOLF3D_SCREENSHOT=$env:WOLF3D_SCREENSHOT"
    Write-Host "WOLF3D_SCREENSHOT_STRIDE=$env:WOLF3D_SCREENSHOT_STRIDE"
    Write-Host "WOLF3D_SCREENSHOT_COUNT=$env:WOLF3D_SCREENSHOT_COUNT"
    Write-Host "WOLF3D_SCREENSHOT_SKIP=$env:WOLF3D_SCREENSHOT_SKIP"
    Write-Host "WOLF3D_SCREENSHOT_EXIT=$env:WOLF3D_SCREENSHOT_EXIT"
    Write-Host "WOLF3D_VERIFY_DEMO=$env:WOLF3D_VERIFY_DEMO"
    Write-Host "WOLF3D_REFERENCE_MANIFEST=$env:WOLF3D_REFERENCE_MANIFEST"
    if ($expectedShots) { Write-Host "WOLF3D_SCREENSHOT_EXPECTED=$expectedShots" }
    Write-Host "SDL_VIDEODRIVER=$env:SDL_VIDEODRIVER"
}

$oldLocation = Get-Location
Set-Location $shotDir
$restoreLocation = $oldLocation
$shotOld = Join-Path $shotDir "autoshot_*.bmp"
Get-ChildItem -Path $shotOld -ErrorAction SilentlyContinue | Remove-Item -Force
Set-Content -Path $logFile -Value @()
Set-Content -Path $stdoutFile -Value @()
Set-Content -Path $stderrFile -Value @()
$exitCode = 124
$process = $null
try {
    $process = Start-Process -FilePath $exe `
        -PassThru -NoNewWindow `
        -RedirectStandardOutput $stdoutFile `
        -RedirectStandardError $stderrFile
    $ended = $process.WaitForExit($TimeoutSeconds * 1000)
    if (-not $ended) {
        Write-Warning "Timeout reached, terminating process."
        if (-not $process.HasExited) {
            Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
            $process.WaitForExit(2000) | Out-Null
        }
        $exitCode = 124
    } else {
        $exitCode = 0
        if ($process.ExitCode -is [int]) {
            $exitCode = [int]$process.ExitCode
        }
    }
    if ($exitCode -ne 0 -and $exitCode -ne 124) {
        Write-Error "verify: executable exited with code $exitCode"
        exit $exitCode
    }
    Set-Content -Path $manifestFile -Value @()
}
finally {
    if ($process -and -not $process.HasExited) {
        Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
        $process.WaitForExit(2000) | Out-Null
    }
    if ($process) {
        $process.Dispose()
    }
    Set-Location $restoreLocation
}

if (Test-Path $stdoutFile) {
    Write-Host "=== stdout ==="
    Get-Content $stdoutFile | Select-Object -Last 20
    Get-Content $stdoutFile | Add-Content $logFile
}
if (Test-Path $stderrFile) {
    Write-Host "=== stderr ==="
    Get-Content $stderrFile | Select-Object -Last 20
    if ((Test-Path $logFile) -and (Get-Content $stderrFile).Count -gt 0) {
        "=== stderr ===" | Add-Content $logFile
    }
    Get-Content $stderrFile | Add-Content $logFile
}

Push-Location $shotDir
$shots = Get-ChildItem -Path . -Filter "autoshot_*.bmp" -File | Sort-Object Name
if (-not $shots -or $shots.Count -eq 0) {
    Write-Error "verify: no autoshot_*.bmp files found in $shotDir"
    exit 1
}
foreach ($shot in $shots) {
    $hash = Get-FileHash -Path $shot.FullName -Algorithm SHA256
    $line = ("{0}  {1}" -f $hash.Hash.ToLower(), $shot.Name)
    Write-Output $line
    Add-Content -Path $manifestFile -Value $line
}
if ($expectedShots -ne $null) {
    $expected = 0
    if (-not [int]::TryParse($expectedShots, [ref]$expected)) {
        Write-Error "verify: invalid WOLF3D_SCREENSHOT_EXPECTED value '$expectedShots'"
        exit 1
    }
    if ($shots.Count -ne $expected) {
        Write-Error "verify: expected $expected screenshot(s), got $($shots.Count)"
        exit 1
    }
}
Write-Output "verify: captured $($shots.Count) screenshot(s)"
Pop-Location

if ($env:WOLF3D_REFERENCE_MANIFEST) {
    $referenceManifest = $env:WOLF3D_REFERENCE_MANIFEST
    $possibleManifests = @($referenceManifest)

    if (-not [System.IO.Path]::IsPathRooted($referenceManifest)) {
        $possibleManifests += Join-Path $PSScriptRoot $referenceManifest
        $possibleManifests += Join-Path (Split-Path $PSScriptRoot -Parent) $referenceManifest
    }

    $referenceManifest = $null
    foreach ($candidate in $possibleManifests) {
        if (Test-Path $candidate) {
            $referenceManifest = $candidate
            break
        }
    }

    if (-not (Test-Path $referenceManifest)) {
        Write-Error "verify: reference manifest not found: $referenceManifest"
        exit 1
    }

    $referenceLines = Get-Content $referenceManifest | ForEach-Object { $_.Trim().ToLowerInvariant() } | Where-Object { $_ }
    $actualLines    = Get-Content $manifestFile | ForEach-Object { $_.Trim().ToLowerInvariant() } | Where-Object { $_ }
    if ($referenceLines.Count -ne $actualLines.Count) {
        Write-Error ("verify: reference manifest line count mismatch (expected {0}, got {1})" -f $referenceLines.Count, $actualLines.Count)
        exit 1
    }

    for ($i = 0; $i -lt $referenceLines.Count; $i++) {
        if ($referenceLines[$i] -ne $actualLines[$i]) {
            Write-Error "verify: reference manifest mismatch at line $($i + 1)"
            Write-Error "  reference: $($referenceLines[$i])"
            Write-Error "  actual:    $($actualLines[$i])"
            exit 1
        }
    }

    Write-Output "verify: reference manifest matched: $referenceManifest"
}
