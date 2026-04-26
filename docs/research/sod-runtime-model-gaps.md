# Optional SOD Runtime Model Gap Scan

Scope: a headless scan of optional Spear of Destiny (`game-files/base/m1`) maps through the current WL6-first runtime model, without committing proprietary data.

## Findings

The existing `wl_build_game_model` path is intentionally WL6-oriented. Running the optional SOD maps through it shows that the shared map-plane/decompression layer works, but SOD-specific info-tile tables are not yet fully classified.

Representative scan results using `MAPHEAD.SOD` / `GAMEMAPS.SOD`:

- `Tunnels 1`: player `32,59`, `17` doors, `147` statics, `8` actors, `8` kill total, `45` treasures, `5` secrets, `2` unknown info tiles.
- `Tunnel Boss`: player `50,31`, `18` doors, `174` statics, `12` actors, `12` kill total, `42` treasures, `12` secrets, `16` unknown info tiles.
- `Death Knight`: player `30,41`, `9` doors, `91` statics, `10` actors, `10` kill total, `2` treasures, `1` secret, `40` unknown info tiles.
- `Angel of Death`: player `31,22`, `1` door, `180` statics, `0` currently-classified actors, `14` treasures, `5` secrets, `121` unknown info tiles.

The high unknown counts are expected until SOD-specific actor/boss/static tile constants are added. A naive widening of the WL6 static tile range is unsafe because info tiles `90..97` are already path markers and `98` is the pushwall marker in the current model.

These gap counts are now pinned by optional headless assertions in `source/modern-c-sdl3/tests/test_assets.c::check_optional_sod()`. The model also records a stable unknown-info FNV hash plus first and last unknown tile/x/y descriptors for each scanned map, so future SOD classification patches can prove exactly which gap set changed instead of relying on counts alone. The executable coverage uses map index `17` for `Death Knight`; map index `18` is `Secret 1`.

## Follow-up full-map unknown histogram

A local diagnostic scan of all 21 SOD maps shows the remaining unknowns are concentrated in a small set of tile IDs:

- `72`: common SOD decoration/static gap across ordinary and boss maps.
- `73`: appears in `Castle 1` only, two adjacent placements near the south edge.
- `74`: appears in `Death Knight` at the boss-room entrance/start area.
- `106`: dominates `Angel of Death`; this is likely a SOD-specific actor/boss/spawn table entry and should not be guessed as a static.
- `107`: appears once in `Angel of Death` near another `106` placement.
- `125`: appears once in `Tunnel Boss`.
- `142`: appears once in `Castle Boss`.
- `143`: appears once in `Dungeon Boss`.
- `161`: appears once in `Death Knight`.

Recommended classification order: first source-confirm tiles `72..74` because they affect many maps but look static-like; then source-confirm boss/special tiles `106`, `107`, `125`, `142`, `143`, and `161` before changing actor counts. Do not infer these from coordinates alone.

## Next useful step

Add explicit SOD info-tile classification tables instead of broadening WL6 ranges. Start with SOD boss maps (`Tunnel Boss`, `Dungeon Boss`, `Castle Boss`, `Death Knight`, `Angel of Death`) and assert unknown counts fall only when source-backed tile mappings are known.

## Verification

```bash
cd source/modern-c-sdl3
WOLF3D_DATA_DIR='/home/node/.openclaw/tmp/Wolfenstein 3D port/game-files/base' make test
```

Result:

```text
asset/decompression/semantics/model/vswap/runtime-present-chase-attack-frame/audio+sod-audio tests passed for /home/node/.openclaw/tmp/Wolfenstein 3D port/game-files/base
```
