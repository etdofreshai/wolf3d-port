param(
    [Parameter(Mandatory = $true)]
    [string]$CaptureDir,

    [string]$ManifestPath = "scripts/wolf3d-autoshot-original.sha256",

    [string]$Pattern = "autoshot_*.bmp",

    [switch]$NormalizeToAutoshot,
    [int]$StartIndex = 0,
    [string]$TargetExtension = ""
)

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
if (-not [System.IO.Path]::IsPathRooted($ManifestPath)) {
    $ManifestPath = Join-Path $repoRoot $ManifestPath
}

if (-not (Test-Path $CaptureDir)) {
    throw "capture directory not found: $CaptureDir"
}

$shots = Get-ChildItem -Path $CaptureDir -Filter $Pattern -File | Sort-Object Name
if (-not $shots -or $shots.Count -eq 0) {
    throw "no matching files found in $CaptureDir (pattern=$Pattern)"
}

New-Item -ItemType Directory -Path (Split-Path $ManifestPath -Parent) -Force | Out-Null

$manifestLines = [System.Collections.Generic.List[string]]::new()
for ($i = 0; $i -lt $shots.Count; $i++) {
    $shot = $shots[$i]
    $name = $shot.Name
    if ($NormalizeToAutoshot) {
        $ext = $shot.Extension.TrimStart('.')
        if ($TargetExtension) { $ext = $TargetExtension.TrimStart('.') }
        $name = ('autoshot_{0:D3}.{1}' -f ($i + $StartIndex), $ext)
    }

    $hash = Get-FileHash -Path $shot.FullName -Algorithm SHA256
    $manifestLines.Add(("{0}  {1}" -f $hash.Hash.ToLowerInvariant(), $name))
}

Set-Content -Path $ManifestPath -Value $manifestLines -Encoding utf8
Write-Host "Wrote manifest from capture directory: $ManifestPath"
Write-Host "Captured files: $($shots.Count)"
Write-Host "Pattern: $Pattern"
