# Audio Asset Header Harness

This note records the current AUDIOHED/AUDIOT characterization seam for future PC speaker, AdLib, digitized sound, music, and SDL3 audio work.

## Scope

- `wl_read_audio_header()` reads little-endian 32-bit chunk offsets from `AUDIOHED.*`.
- `wl_read_audio_chunk()` copies bounded raw chunk bytes from `AUDIOT.*` using adjacent offsets.
- Tests intentionally treat the bytes as opaque for now; format-specific PC speaker, AdLib, digitized sound, and IMF parsing can build on this seam later.

## Verified WL6 metadata

- `AUDIOHED.WL6`: 1,156 bytes = 289 offsets = 288 chunks plus sentinel.
- First offsets: `0, 15, 28, 44, 102`.
- `AUDIOT.WL6`: 320,209 bytes; sentinel offset is within the file and equals the observed file end.
- Representative chunks:
  - chunk 0: 15 bytes, FNV-1a `0x5971ec53`.
  - chunk 1: 13 bytes, FNV-1a `0x21985d89`.
  - chunk 87: 41 bytes, FNV-1a `0x799f60b1`.
  - chunk 174: 0 bytes.
  - chunk 261: 7,546 bytes, FNV-1a `0xea0d69d8`.
  - chunk 287: 20,926 bytes, FNV-1a `0x65998666`.

## Verified optional SOD metadata

When local Spear data is present under `game-files/base/m1`:

- `AUDIOHED.SOD`: 1,072 bytes = 268 offsets = 267 chunks plus sentinel.
- First offsets: `0, 15, 66, 82, 174`.
- `AUDIOT.SOD`: 328,620 bytes; sentinel offset equals the file size.
- Representative chunks:
  - chunk 0: 15 bytes, FNV-1a `0x5971ec53`.
  - chunk 1: 51 bytes, FNV-1a `0xfafa57eb`.
  - chunk 87: 69 bytes, FNV-1a `0xf0dfcb70`.
  - chunk 174: 0 bytes.
  - chunk 261: 13,286 bytes, FNV-1a `0x04a8dbe2`.
  - chunk 266: 6,302 bytes, FNV-1a `0x0fdb4632`.

## Verification

```bash
cd source/modern-c-sdl3
WOLF3D_DATA_DIR='/home/node/.openclaw/tmp/Wolfenstein 3D port/game-files/base' make test
make sdl3-info test-sdl3
```

Result: headless asset/decompression/runtime/audio tests pass; SDL3 smoke test gracefully skips when SDL3 is unavailable in the worktree.
