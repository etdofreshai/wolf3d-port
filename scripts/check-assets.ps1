param(
    [Alias("Manifest")]
    [string]$ManifestPath = "scripts/assets-manifest.sha256",
    [ValidateSet("check","write")]
    [string]$Mode = "check"
)

$ErrorActionPreference = "Stop"
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path

function Get-RootHashManifest {
    param([string]$OutputPath)

    if (-not (Test-Path (Join-Path $repoRoot "original"))) {
        throw "check-assets: original/ directory not found at $repoRoot\original"
    }
    if (-not (Test-Path (Join-Path $repoRoot "assets"))) {
        throw "check-assets: assets/ directory not found at $repoRoot\assets"
    }

    Get-ChildItem -Path (Join-Path $repoRoot "original"), (Join-Path $repoRoot "assets") -Recurse -File |
      Sort-Object FullName |
      Where-Object {
          $relative = $_.FullName.Substring($repoRoot.Length + 1)
          $normalized = $relative -replace '\\','/'
          $normalized -notmatch '^assets/DOSBox/(stdout|stderr)\.txt$'
      } |
      ForEach-Object {
          $hash = Get-FileHash -Path $_.FullName -Algorithm SHA256
          $rel = $_.FullName.Substring($repoRoot.Length + 1)
        "$($hash.Hash.ToLowerInvariant())  $rel"
      } | ForEach-Object {
          $_ -replace '\\','/'
      } | Set-Content -Path $OutputPath -Encoding utf8
}

$temp = New-TemporaryFile
try {
    Get-RootHashManifest -OutputPath $temp.FullName

    if ($Mode -eq "write") {
        Copy-Item -Path $temp.FullName -Destination $ManifestPath -Force
        Write-Host "check-assets: wrote manifest to $ManifestPath"
    } else {
        if (-not (Test-Path $ManifestPath)) {
            throw "check-assets: manifest not found: $ManifestPath; initialize with -Mode write"
        }

        $expected = Get-Content $ManifestPath -Encoding utf8
        $actual = Get-Content $temp.FullName -Encoding utf8
        if (@($expected).Count -ne @($actual).Count -or (Compare-Object $expected $actual)) {
            Write-Error "check-assets: FAIL (asset fingerprints changed)"
            Write-Host "check-assets: diff:"
            Write-Output (Compare-Object $expected $actual | Out-String)
            exit 1
        }
        Write-Host "check-assets: PASS (original and assets match manifest)"
    }
}
finally {
    if (Test-Path $temp) { Remove-Item $temp -Force }
}
