# Optional SOD Runtime Model Gap Scan

Scope: a headless scan of optional Spear of Destiny (`game-files/base/m1`) maps through the current WL6-first runtime model, without committing proprietary data.

## Findings

The existing `wl_build_game_model` path is intentionally WL6-oriented. Running the optional SOD maps through it shows that the shared map-plane/decompression layer works, but SOD-specific info-tile tables are not yet fully classified.

Representative scan results using `MAPHEAD.SOD` / `GAMEMAPS.SOD`:

- `Tunnels 1`: player `32,59`, `17` doors, `149` statics, `8` actors, `8` kill total, `45` treasures, `5` secrets, `0` unknown info tiles.
- `Tunnel Boss`: player `50,31`, `18` doors, `189` statics, `13` actors, `13` kill total, `42` treasures, `12` secrets, `0` unknown info tiles.
- `Death Knight`: player `30,41`, `9` doors, `130` statics, `11` actors, `11` kill total, `2` treasures, `1` secret, `0` unknown info tiles.
- `Angel of Death`: player `31,22`, `1` door, `263` statics, `38` actors, `38` kill total, `14` treasures, `5` secrets, `0` unknown info tiles.

SOD-specific static and boss tile constants are now source-confirmed in the runtime model. A naive widening of the WL6 static tile range remains unsafe because info tiles `90..97` are path markers and `98` is the pushwall marker in the current model.

These gap counts are now pinned by optional headless assertions in `source/modern-c-sdl3/tests/test_assets.c::check_optional_sod()`. The model also records a stable unknown-info FNV hash plus first and last unknown tile/x/y descriptors for each scanned map, so future SOD classification patches can prove exactly which gap set changed instead of relying on counts alone. The executable coverage uses map index `17` for `Death Knight`; map index `18` is `Secret 1`.

## Follow-up full-map unknown histogram

Executable optional assertions now scan all 21 SOD maps and verify the unknown-info histogram is empty.

Source-backed SOD boss/special tiles `106` (`SpawnSpectre`), `107` (`SpawnAngel`), `125` (`SpawnTrans`), `142` (`SpawnUber`), `143` (`SpawnWill`), and `161` (`SpawnDeath`) are classified as runtime boss actors. Source-backed SOD static tiles `72..74` now resolve through the original `SpawnStatic(x, y, tile - 23)` path as types `49..51`: marble pillar (`block`), 25-ammo clip (`bonus`), and truck (`block`). Type `52` / `bo_spear` is also modeled for future Spear pickup coverage even though it is not part of the latest unknown histogram.

## Runtime scene coverage

Optional headless assertions now collect scene sprite refs for the representative SOD maps after the static/boss classifications. They pin scene-ref counts and hashes for `Tunnels 1`, `Tunnel Boss`, `Death Knight`, and `Angel of Death`, and explicitly verify Spear static type placements used by the new `72..74` classifications. This proves the newly classified SOD statics/bosses reach renderer-facing source/VSWAP metadata instead of only shrinking unknown counts.

## Next useful step

Broaden optional SOD pickup/gameplay behavior around the `bo_25clip` and eventual `bo_spear` paths, or route a small SOD scene through cached sprite surfaces/rendering when useful.

## Verification

```bash
cd source/modern-c-sdl3
WOLF3D_DATA_DIR='/home/node/.openclaw/tmp/Wolfenstein 3D port/game-files/base' make test
```

Result:

```text
asset/decompression/semantics/model/vswap/runtime-present-chase-attack-frame/audio+sod-audio tests passed for /home/node/.openclaw/tmp/Wolfenstein 3D port/game-files/base
```

## Cycle update: SOD bonus pickup mapping

SOD static traits for type `50` (25-ammo clip) and type `52` (Spear pickup) now feed the same gameplay pickup seam as WL6 statics: type `50` maps to `WL_BONUS_25CLIP` with ammo capped at 99, and type `52` maps to `WL_BONUS_SPEAR` / level completion. Headless regression coverage uses synthetic active SOD statics so optional SOD classification can safely drive live pickup/removal later.

## Cycle update: SOD boss scene sprite refs

Added `wl_collect_spear_scene_sprite_refs()` as an explicit Spear/SOD scene-ref collection seam. It preserves the existing WL6 scene-ref API while using Spear sprite ordinals for shared actors and the source-backed SOD boss/special map tiles: Spectre `106 -> SPR_SPECTRE_W1 (377)`, Angel `107 -> SPR_ANGEL_W1 (385)`, Trans `125 -> SPR_TRANS_W1 (326)`, Uber `142 -> SPR_UBER_W1 (349)`, Wilhelm `143 -> SPR_WILL_W1 (337)`, and Death Knight `161 -> SPR_DEATH_W1 (362)`. A synthetic headless regression verifies those source tiles now produce actor scene refs and that the WL6 collector still intentionally skips them.
