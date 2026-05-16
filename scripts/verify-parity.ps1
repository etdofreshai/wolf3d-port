param(
    [string]$BuildDir = "build-debug",
    [int]$TimeoutSeconds = 12,
    [string]$ReferenceManifest = "",
    [int]$Demo = 0,
    [string]$OriginalManifest = "",
    [switch]$IgnoreOriginalFileNames,
    [switch]$PixelCompare,
    [switch]$Verbose
)

$ErrorActionPreference = "Stop"
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
if ([string]::IsNullOrWhiteSpace($BuildDir)) {
    $BuildDir = "build-debug"
}
if ([System.IO.Path]::IsPathRooted($BuildDir)) {
    $buildDirFull = [System.IO.Path]::GetFullPath($BuildDir)
} else {
    $buildDirFromCwd = [System.IO.Path]::GetFullPath((Join-Path (Get-Location).Path $BuildDir))
    $buildDirFromRepo = [System.IO.Path]::GetFullPath((Join-Path $repoRoot $BuildDir))
    if (Test-Path $buildDirFromRepo) {
        $buildDirFull = $buildDirFromRepo
    } else {
        $buildDirFull = $buildDirFromCwd
    }
}

if (-not (Test-Path $buildDirFull)) {
    throw "Build directory not found: $buildDirFull"
}

if ([string]::IsNullOrWhiteSpace($ReferenceManifest)) {
    throw "reference manifest is required"
}

function Resolve-ManifestPath {
    param([string]$Candidate)

    if ([string]::IsNullOrWhiteSpace($Candidate)) { return "" }

    $repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
    $workRoot = (Get-Location).Path
    $candidatePath = [string]$Candidate

    $paths = @()
    if (-not [System.IO.Path]::IsPathRooted($candidatePath)) {
        $paths = @(
            (Join-Path $PSScriptRoot $candidatePath),
            (Join-Path $repoRoot $candidatePath),
            (Join-Path $workRoot $candidatePath)
        )
    } else {
        $paths = @($candidatePath)
    }

    foreach ($path in $paths) {
        if (Test-Path $path) { return (Resolve-Path $path).Path }
    }

    return ""
}

function Get-ManifestEntries {
    param([string]$ManifestPath)

    return Get-Content $ManifestPath | ForEach-Object {
        $line = $_.Trim()
        if (-not $line) { return }
        $parts = $line -split '\s+', 2
        if ($parts.Count -lt 2) { throw "invalid manifest line: $line" }
        [pscustomobject]@{
            Hash = $parts[0].ToLowerInvariant()
            File = $parts[1]
        }
    }
}

function Resolve-ManifestImagePath {
    param(
        [string]$ManifestPath,
        [string]$FileName,
        [int]$FrameIndex = -1
    )

    $repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
    $manifestDir = Split-Path -Parent $ManifestPath
    $fileBase = [System.IO.Path]::GetFileNameWithoutExtension($FileName)
    $fileExt = [System.IO.Path]::GetExtension($FileName)

    if ([System.IO.Path]::IsPathRooted($FileName)) {
        if (Test-Path $FileName) {
            return (Resolve-Path $FileName).Path
        }
        return ""
    }

    # Preferred resolution for names is exact path in manifest directory or CWD/workspace.
    $candidateCandidates = @(
        (Join-Path $manifestDir $FileName),
        (Join-Path $PSScriptRoot $FileName),
        (Join-Path (Join-Path $repoRoot "scripts") $FileName),
        (Join-Path $repoRoot $FileName),
        (Join-Path (Get-Location).Path $FileName)
    )
    foreach ($candidate in $candidateCandidates) {
        if (Test-Path $candidate) {
            return (Resolve-Path $candidate).Path
        }
    }

    # If extension differs between reference and generated captures, try alternate extension
    # in the same high-signal locations before frame-based fallback.
    $alternateExtensions = switch ($fileExt.ToLowerInvariant()) {
        '.png' { @('.bmp') }
        '.bmp' { @('.png') }
        default { @('.png', '.bmp') }
    }

    if ($alternateExtensions -and $fileBase) {
        $extensionCandidates = @(
            $manifestDir,
            $PSScriptRoot,
            (Join-Path $repoRoot "scripts"),
            $repoRoot,
            (Get-Location).Path
        ) | Where-Object { $_ -and (Test-Path $_) } | Select-Object -Unique

        foreach ($altExtension in $alternateExtensions) {
            $altFileName = $fileBase + $altExtension
            foreach ($candidate in $extensionCandidates) {
                $altPath = Join-Path $candidate $altFileName
                if (Test-Path $altPath) {
                    return (Resolve-Path $altPath).Path
                }
            }
        }
    }

    # If a frame index is provided, allow stable frame-based matching to the
    # manifest folder and common build artifact directories.
    if ($FrameIndex -ge 0) {
        $tag = "{0:D3}" -f $FrameIndex
        $frameCandidates = @(
            $manifestDir,
            (Join-Path $repoRoot "scripts"),
            (Join-Path $repoRoot "build"),
            (Join-Path $repoRoot "build-debug"),
            (Join-Path (Join-Path $repoRoot "build") "original-capture"),
            (Join-Path (Join-Path $repoRoot "build-debug") "original-capture"),
            $repoRoot,
            (Get-Location).Path
        ) | Where-Object { $_ -and (Test-Path $_) } | Select-Object -Unique

        $namePrefix = $null
        $prefixMatch = [regex]::Match($FileName, '^(.*?)(\d{3})')
        if ($prefixMatch.Success -and $prefixMatch.Groups[1].Value.Trim()) {
            $namePrefix = [regex]::Escape($prefixMatch.Groups[1].Value.Trim())
        }

        foreach ($root in $frameCandidates) {
            $exact = Join-Path $root $FileName
            if (Test-Path $exact) {
                return (Resolve-Path $exact).Path
            }

            foreach ($altExtension in $alternateExtensions) {
                $altFramePath = Join-Path $root ($fileBase + $altExtension)
                if (Test-Path $altFramePath) {
                    return (Resolve-Path $altFramePath).Path
                }
            }

            $direct = Get-ChildItem -Path $root -File -ErrorAction SilentlyContinue
            if ($namePrefix) {
                $direct = $direct | Where-Object { $_.Extension -match '^(?i:\.png|\.bmp)$' -and $_.Name -match "^$namePrefix.*$tag" }
            } else {
                $direct = $direct | Where-Object { $_.Extension -match '^(?i:\.png|\.bmp)$' -and $_.Name -match [regex]::Escape($tag) }
            }
            $direct = $direct | Sort-Object FullName
            if ($direct) {
                return $direct[0].FullName
            }

            if (-not $direct) {
                $wildName = "*$tag*"
                $fallback = Get-ChildItem -Path $root -Recurse -File -ErrorAction SilentlyContinue |
                    Where-Object { $_.Extension -match '^(?i:\.png|\.bmp)$' -and $_.Name -like $wildName }
                $fallback = $fallback | Sort-Object FullName
                if ($fallback) {
                    return $fallback[0].FullName
                }
            }
        }
    }

    $found = Get-ChildItem -Path $repoRoot -Recurse -Filter $FileName -File -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($found) {
        return $found.FullName
    }

    return ""
}

function Get-ImageHash {
    param([string]$Path)

    Add-Type -AssemblyName System.Drawing | Out-Null
    try {
        $image = [System.Drawing.Image]::FromFile($Path)
        try {
            $bitmap = New-Object System.Drawing.Bitmap $image
            try {
                $rect = New-Object System.Drawing.Rectangle 0, 0, $bitmap.Width, $bitmap.Height
                $data = $bitmap.LockBits($rect, [System.Drawing.Imaging.ImageLockMode]::ReadOnly, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
                try {
                    $bytes = New-Object byte[] ($data.Stride * $bitmap.Height)
                    [Runtime.InteropServices.Marshal]::Copy($data.Scan0, $bytes, 0, $bytes.Length)
                    $sha = [System.Security.Cryptography.SHA256]::Create()
                    try {
                        return ([System.BitConverter]::ToString($sha.ComputeHash($bytes))).Replace("-", "").ToLowerInvariant()
                    }
                    finally {
                        $sha.Dispose()
                    }
                }
                finally {
                    $bitmap.UnlockBits($data)
                }
            }
            finally {
                $bitmap.Dispose()
            }
        }
        finally {
            $image.Dispose()
        }
    }
    catch {
        throw "failed to compute image hash for '$Path': $($_.Exception.Message)"
    }
}

function Find-ExecutableAndManifest {
    param([string]$Root)

    $exeCandidates = @(
        (Join-Path $Root "wolf3d.exe"),
        (Join-Path (Join-Path $Root "Debug") "wolf3d.exe"),
        (Join-Path (Join-Path $Root "Release") "wolf3d.exe"),
        (Join-Path $Root "wolf3d"),
        (Join-Path (Join-Path $Root "Debug") "wolf3d"),
        (Join-Path (Join-Path $Root "Release") "wolf3d"),
        (Join-Path $Root "Debug\\wolf3d.exe"),
        (Join-Path $Root "Release\\wolf3d.exe")
    )

    $exePath = $null
    foreach ($exe in $exeCandidates) {
        if (Test-Path $exe) {
            $exePath = (Resolve-Path $exe).Path
            break
        }
    }
    if (-not $exePath) {
        return $null
    }

    $manifest = Join-Path (Split-Path -Parent $exePath) "wolf3d-autoshot.sha256"
    if (-not (Test-Path $manifest)) {
        $manifest = Get-ChildItem -Path (Split-Path -Parent $exePath) -Recurse -Filter "wolf3d-autoshot.sha256" -File -ErrorAction SilentlyContinue | Select-Object -First 1
        if (-not $manifest) {
            return $null
        }
        $manifest = $manifest.FullName
    }

    return @{
        Exe = $exePath
        Manifest = $manifest
    }
}

function Compare-Manifest {
    param(
        [string]$ActualManifest,
        [string]$ExpectedManifest,
        [string]$Label,
        [switch]$IgnoreFileNames
    )

    if ($PixelCompare) {
        $actualEntries = Get-ManifestEntries -ManifestPath $ActualManifest
        $expectedEntries = Get-ManifestEntries -ManifestPath $ExpectedManifest

        if ($actualEntries.Count -ne $expectedEntries.Count) {
            throw "$Label mismatch: line counts differ (expected=$($expectedEntries.Count), actual=$($actualEntries.Count))"
        }

        for ($i = 0; $i -lt $actualEntries.Count; $i++) {
            $actualFile = if ($IgnoreFileNames) {
                Resolve-ManifestImagePath -ManifestPath $ActualManifest -FileName $actualEntries[$i].File -FrameIndex $i
            } else {
                Resolve-ManifestImagePath -ManifestPath $ActualManifest -FileName $actualEntries[$i].File
            }

            $expectedFile = if ($IgnoreFileNames) {
                Resolve-ManifestImagePath -ManifestPath $ExpectedManifest -FileName $expectedEntries[$i].File -FrameIndex $i
            } else {
                Resolve-ManifestImagePath -ManifestPath $ExpectedManifest -FileName $expectedEntries[$i].File
            }

            $expectedFile = if ($expectedFile) { $expectedFile } else { "" }
            $expectedHash = $expectedEntries[$i].Hash.ToLowerInvariant()
            $comparePixelHash = $false
            if ($expectedFile -and $PixelCompare) {
                try {
                    $expectedHash = Get-ImageHash -Path $expectedFile
                    $comparePixelHash = $true
                }
                catch {
                    $expectedFile = ""
                    $expectedHash = $expectedEntries[$i].Hash.ToLowerInvariant()
                }
            }
            if (-not $actualFile) {
                throw "$Label missing actual image: $($actualEntries[$i].File)"
            }
            $actualHash = if ($comparePixelHash) {
                Get-ImageHash -Path $actualFile
            } else {
                (Get-FileHash -Path $actualFile -Algorithm SHA256).Hash.ToLowerInvariant()
            }
            if ($actualHash -ne $expectedHash) {
                $expectedFileText = if ($expectedFile) { $expectedFile } else { "(unresolved, using manifest hash)" }
                throw "$Label mismatch at line $($i + 1)`n  expected file: $expectedFileText`n  actual file:   $actualFile"
            }
        }
        return
    }

    $actualLines = Get-Content $ActualManifest | ForEach-Object { $_.Trim().ToLowerInvariant() } | Where-Object { $_ }
    $expectedLines = Get-Content $ExpectedManifest | ForEach-Object { $_.Trim().ToLowerInvariant() } | Where-Object { $_ }

    $actualExt = $actualLines | ForEach-Object { ($_ -split "\\s+",2)[1] } | ForEach-Object { [System.IO.Path]::GetExtension($_).ToLowerInvariant() } | Sort-Object -Unique
    $expectedExt = $expectedLines | ForEach-Object { ($_ -split "\\s+",2)[1] } | ForEach-Object { [System.IO.Path]::GetExtension($_).ToLowerInvariant() } | Sort-Object -Unique
    if (($actualExt.Count -ne $expectedExt.Count) -or (($actualExt -join ",") -ne ($expectedExt -join ","))) {
        Write-Warning "manifest formats differ (actual=$($actualExt -join ',') expected=$($expectedExt -join ',')); re-run with -PixelCompare"
    }

    $actual = if ($IgnoreFileNames) { $actualLines | ForEach-Object { ($_ -split "\\s+",2)[0] } } else { $actualLines }
    $expected = if ($IgnoreFileNames) { $expectedLines | ForEach-Object { ($_ -split "\\s+",2)[0] } } else { $expectedLines }

    if ($actual.Count -ne $expected.Count) {
        throw "$Label mismatch: line counts differ (expected=$($expected.Count), actual=$($actual.Count))"
    }

    for ($i = 0; $i -lt $actual.Count; $i++) {
        if ($actual[$i] -ne $expected[$i]) {
            throw "$Label mismatch at line $($i + 1)`n  expected: $($expected[$i])`n  actual:   $($actual[$i])"
        }
    }
}

$referenceManifest = Resolve-ManifestPath $ReferenceManifest
if (-not $referenceManifest) {
    throw "reference manifest not found: $ReferenceManifest"
}

$originalManifest = Resolve-ManifestPath $OriginalManifest

$env:WOLF3D_REFERENCE_MANIFEST = $referenceManifest
$env:WOLF3D_VERIFY_DEMO = [string]$Demo
if (-not $env:WOLF3D_SCREENSHOT_SKIP) {
    if ($Demo -eq 0) {
        $env:WOLF3D_SCREENSHOT_SKIP = "0"
    } else {
        $env:WOLF3D_SCREENSHOT_SKIP = "0"
    }
}

if ($Verbose) {
    Write-Host "verify-parity: BuildDir=$BuildDir"
    Write-Host "verify-parity: Timeout=${TimeoutSeconds}s"
    Write-Host "verify-parity: Demo=$Demo"
    Write-Host "verify-parity: ReferenceManifest=$referenceManifest"
    Write-Host "verify-parity: ScreenshotSkip=$($env:WOLF3D_SCREENSHOT_SKIP)"
    if ($IgnoreOriginalFileNames) { Write-Host "verify-parity: IgnoreOriginalFileNames=True" }
    if ($originalManifest) { Write-Host "verify-parity: OriginalManifest=$originalManifest" }
}

$manifestItem = Find-ExecutableAndManifest -Root $buildDirFull
if (-not $manifestItem -or -not $manifestItem.Manifest -or -not (Test-Path $manifestItem.Manifest)) {
    throw "could not locate generated wolf3d-autoshot.sha256 under $buildDirFull"
}

& (Join-Path $PSScriptRoot "verify.ps1") -BuildDir $buildDirFull -TimeoutSeconds $TimeoutSeconds -Verbose:$Verbose

$manifestItem = Get-ChildItem -Path $buildDirFull -Recurse -Filter "wolf3d-autoshot.sha256" -File -ErrorAction SilentlyContinue |
    Select-Object -First 1

if (-not $manifestItem -or -not (Test-Path $manifestItem.FullName)) {
    throw "could not locate generated wolf3d-autoshot.sha256 under $buildDirFull"
}
$manifestFile = $manifestItem.FullName

if ($originalManifest) {
    Compare-Manifest -ActualManifest $manifestFile -ExpectedManifest $originalManifest -Label "port vs original" -IgnoreFileNames:$IgnoreOriginalFileNames
    Write-Output "verify-parity: original baseline matched: $originalManifest"
}

Write-Output "verify-parity: complete"
