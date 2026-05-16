# Wolf3D SDL3 Port Build & Verification

## Quick builds

- Windows (existing workflow):
  - `cmake -S . -B build-debug`
  - `cmake --build build-debug --config Debug -j 4`

- Linux/macOS:
  - `./scripts/build.sh` (defaults to `build/`, `Unix Makefiles`)
  - `cmake --build build -j 4`

Linux/macOS verification:
- `chmod +x scripts/build.sh scripts/verify.sh`
- `WOLF3D_VERIFY_TIMEOUT=12 ./scripts/verify.sh`  
  (runs with `WOLF3D_SCREENSHOT=1` and `SDL_VIDEODRIVER=dummy`, then prints screenshot hashes)

Windows verification:
- `.\scripts\verify.ps1 -BuildDir build-debug -TimeoutSeconds 12 -Verbose`

Portable PowerShell verification (no bash required):
- `.\scripts\verify-ci.ps1 -BuildDir build-debug -TimeoutSeconds 12 -Demo 0 -Verbose`

To refresh deterministic reference manifests:
- `.\scripts\gen-verify-manifest.ps1 -BuildDir build-debug -Demo 0 -TimeoutSeconds 12`
- `./scripts/gen-verify-manifest.sh build-debug 0 12`

The verification scripts now run deterministic screenshot capture by default:
- `WOLF3D_SCREENSHOT` defaults to `1` (enable capture).
- `WOLF3D_SCREENSHOT_STRIDE` defaults to `18` (capture every 18th `VL_Present` call).
- `WOLF3D_SCREENSHOT_COUNT` defaults to `9` (capture target frame count).
- `WOLF3D_SCREENSHOT_SKIP` defaults to `0` (number of qualifying capture events to skip before writing).
- `WOLF3D_SCREENSHOT_EXIT=1` causes the process to exit after target count is reached.
- `WOLF3D_SCREENSHOT_EXPECTED` defaults to `WOLF3D_SCREENSHOT_COUNT` (manifest size assertion).
- `WOLF3D_VERIFY_DEMO` may be set to `0-3` to force a deterministic single demo playback during verification.
- `WOLF3D_REFERENCE_MANIFEST` may be set to a manifest path to assert exact byte-for-byte screenshot output for regression control.

You can disable SDL fetch on systems with SDL3 installed:
- `WOLF3D_FETCH_SDL3=OFF ./scripts/build.sh`

## Asset/config integrity checks

- Preserve `original/` and `assets/` as provided and avoid regenerating them.
- After build, verify source-tree integrity with filesystem hashing when needed:
  - Linux/macOS: `shasum -a 256 original/** assets/**`
  - Windows PowerShell: `Get-FileHash -Path original -Recurse -Algorithm SHA256`

To run the project’s manifest-based immutable-content check:

- Windows:
  - `powershell -ExecutionPolicy Bypass -File .\\scripts\\check-assets.ps1 -ManifestPath scripts/assets-manifest.sha256 -Mode check`
- Linux/macOS:
  - `./scripts/check-assets.sh scripts/assets-manifest.sha256 check`

## Current scope

- This repo tracks a modernization pass focused on SDL3 integration, removing
  DOS/Borland dependencies, and preserving legacy assets/code structure.

## CI verification

- GitHub Actions workflow: `.github/workflows/ci.yml`
- Matrix runs `configure -> build -> verify` on:
  - `windows-latest`
  - `ubuntu-latest`
  - `macos-latest`

### Optional original-parity verification

- If you maintain a manifest captured from the original DOS build, compare both
  artifacts with the parity wrapper:
  - `.\scripts\verify-parity.ps1 -BuildDir build-debug -ReferenceManifest scripts/wolf3d-autoshot-demo0-reference.sha256 -OriginalManifest scripts/wolf3d-autoshot-demo0-original.sha256 -Verbose`
  - `./scripts/verify-parity.sh build-debug scripts/wolf3d-autoshot-demo0-reference.sha256 scripts/wolf3d-autoshot-demo0-original.sha256 12`

- Build an original-manifest from captured screenshots before parity verification:
  - `.\scripts\gen-original-manifest.ps1 -CaptureDir "path\\to\\captures" -ManifestPath scripts/wolf3d-autoshot-demo0-original.sha256`
  - `./scripts/gen-original-manifest.sh /path/to/captures scripts/wolf3d-autoshot-demo0-original.sha256`

- Optionally run DOSBox and capture directly into a manifest:
  - `.\scripts\capture-original.ps1 -CaptureDir "build/original-capture" -ManifestPath "scripts/wolf3d-autoshot-demo0-original.sha256" -TimeoutSeconds 120 -ExpectedShots 9 -NoLauncher`
  - `./scripts/capture-original.sh "assets/DOSBox/wolf3d.conf" "assets/base" "build/original-capture" "scripts/wolf3d-autoshot-demo0-original.sha256" 120 9`
  - use `-AutoexecCommands` (PowerShell) to set a deterministic boot sequence inside DOSBox when your environment supports scripted screenshot capture.

- If your original captures are PNG and port captures are BMP, use pixel-level verification to avoid container-hash false mismatches:
  - `powershell -ExecutionPolicy Bypass -File .\\scripts\\verify-parity.ps1 -BuildDir build-debug -ReferenceManifest scripts\\wolf3d-autoshot-demo0-reference.sha256 -OriginalManifest scripts\\wolf3d-autoshot-demo0-original-nolaunch-normalized.sha256 -IgnoreOriginalFileNames -PixelCompare -Verbose`
  - `./scripts/verify-parity.sh build-debug scripts/wolf3d-autoshot-demo0-reference.sha256 scripts/wolf3d-autoshot-demo0-original-nolaunch-normalized.sha256 12 0 1`

- The optional original baseline is not included in CI by default because
  DOSBox-based capture tooling is environment-specific; use this wrapper when you
  have a stable baseline manifest to gate parity manually.
