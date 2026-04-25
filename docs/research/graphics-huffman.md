# Graphics Huffman Notes

Research/implementation cycle: 2026-04-24 22:53-23:15 CDT  
Scope: `VGAHEAD`/`VGADICT`/`VGAGRAPH` header parsing, Huffman smoke decoding, `STRUCTPIC` picture-table metadata, indexed-surface conversion, texture-upload metadata, palette/fade/shift metadata, palette-shift state, and palette-shifted upload selection for WL6 and optional SOD, without committing proprietary graphics bytes.

## Original reference

Original source reference, inspected but not modified:

- `source/original/WOLFSRC/ID_CA.C::GRFILEPOS`
  - reads 3-byte graphics offsets when `THREEBYTEGRSTARTS` is enabled.
  - treats `0xffffff` as sparse `-1`.
- `source/original/WOLFSRC/ID_CA.C::CAL_SetupGrFile`
  - loads `VGADICT.*` as 255 Huffman nodes.
  - loads `(NUMCHUNKS + 1)` 3-byte entries from `VGAHEAD.*`.
  - decodes `STRUCTPIC` into the picture table during setup.
- `source/original/WOLFSRC/ID_VH.H`
  - defines `pictabletype` as two 16-bit `int` fields: `width,height`.
- `source/original/WOLFSRC/ID_VH.C::VWB_DrawPic` / `LatchDrawPic`
  - index `pictable[picnum - STARTPICS]` for picture dimensions before drawing.
- `source/original/WOLFSRC/ID_VL.C::VL_MemToScreen`
  - treats graphics chunks as VGA planar memory ordered by plane, copying `width / 4` bytes per row per plane.
- `source/original/WOLFSRC/ID_VL.C::VL_FadeOut` / `VL_FadeIn`
  - interpolates 6-bit VGA palette components with integer `orig + delta * i / steps` math over a start/end palette-index range, then applies the final palette separately.
- `source/original/WOLFSRC/WL_PLAY.C::InitRedShifts` / `UpdatePaletteShifts`
  - precomputes six red damage palettes toward `(64,0,0)` using `REDSTEPS=8`, three bonus palettes toward `(64,62,0)` using `WHITESTEPS=20`, and prioritizes red over white while active.
- `source/original/WOLFSRC/ID_CA.C::CAL_HuffExpand`
  - uses node 254 as the Huffman head node.
  - consumes bits least-significant-bit first from each compressed byte.
  - values `< 256` are literal output bytes; values `>= 256` reference `table[value - 256]`.
- `source/original/WOLFSRC/GFXV_WL6.H`
  - `NUMCHUNKS = 149`, `NUMPICS = 132`, `STRUCTPIC = 0`, `STARTFONT = 1`, `STARTPICS = 3`, `STARTTILE8 = 135`, `STARTEXTERNS = 136`.
- `source/original/WOLFSRC/GFXV_SOD.H`
  - `NUMCHUNKS = 169`, `NUMPICS = 147`, `STRUCTPIC = 0`, `STARTFONT = 1`, `STARTPICS = 3`, `STARTTILE8 = 150`, `STARTEXTERNS = 151`.

## Implemented seam

Added graphics APIs/types to `wl_assets`:

- `WL_GRAPHICS_MAX_CHUNKS`
- `WL_HUFFMAN_NODE_COUNT`
- `wl_huffman_node`
- `wl_graphics_header`
- `wl_read_graphics_header(...)`
- `wl_read_huffman_dictionary(...)`
- `wl_huff_expand(...)`
- `wl_read_graphics_chunk(...)`
- `wl_picture_size`
- `wl_picture_table_metadata`
- `wl_decode_picture_table(...)`
- `wl_surface_format`
- `wl_indexed_surface`
- `wl_wrap_indexed_surface(...)`
- `wl_decode_planar_picture_to_indexed(...)`
- `wl_decode_planar_picture_surface(...)`
- `wl_blit_indexed_surface(...)`
- `wl_texture_upload_descriptor`
- `wl_describe_indexed_texture_upload(...)`
- `wl_expand_indexed_surface_to_rgba(...)`
- `wl_interpolate_palette_range(...)`
- `wl_build_palette_shift(...)`
- `wl_palette_shift_state` / `wl_update_palette_shift_state(...)`
- `wl_select_palette_for_shift(...)`
- `wl_describe_palette_shifted_texture_upload(...)`

The implementation:

- parses 3-byte `VGAHEAD` entries and sparse sentinel offsets;
- parses the 255-node `VGADICT` Huffman table;
- ports the original LSB-first Huffman traversal as a pure C function;
- reads explicit-size `VGAGRAPH` chunks and decodes them into caller-provided buffers;
- decodes `STRUCTPIC` as width/height metadata for picture chunks;
- wraps decoded indexed pixels in a renderer-facing `wl_indexed_surface` descriptor with format, width, height, stride, pixel count, and pixel pointer;
- converts decoded planar picture chunks into linear 8-bit indexed surfaces suitable for a future SDL3 texture/upload boundary;
- provides an SDL-free clipped blit helper for indexed surfaces;
- describes indexed-8 + RGB-palette texture upload metadata and expands indexed surfaces to RGBA8888 using 6-bit VGA DAC or 8-bit palette components;
- interpolates palette ranges with the original integer fade math so fade-in/fade-out and palette-flash effects can be tested before SDL presentation;
- builds original-style full-palette red/white shift tables for damage and bonus flashes without touching VGA hardware;
- advances damage/bonus palette-shift counters with the original red-over-white priority and base-palette reset selection;
- selects the effective base/red/white palette from palette-shift state, describes the shifted indexed texture upload without opening a display, and now receives palette-triggering player damage/bonus gameplay events;
- records only sizes/counts/hashes/dimensions in tests and docs, not graphics bytes.

## WL6 committed assertions

`game-files/base/VGAHEAD.WL6` / `VGADICT.WL6` / `VGAGRAPH.WL6`:

- `VGAHEAD.WL6` entries: `150` (`149` chunks plus sentinel)
- `VGAHEAD.WL6` size: `450`
- first offsets: `0`, `354`, chunk 3 offset `9570`, final sentinel `275774`
- decoded chunk `0` (`STRUCTPIC`): compressed `354`, expanded `528`, FNV-1a `0x0a6f459a`
- `STRUCTPIC` picture table: `132` entries, width range `8..320`, height range `8..200`, total declared pixels `342464`
- representative WL6 dimensions: entry `0` `96x88`, entry `3` `320x8`, entry `84` `320x200`, entry `86` `320x200`, entry `87` `224x56`, entry `131` `224x48`
- decoded chunk `1` (`STARTFONT`): compressed `3467`, expanded `8300`, FNV-1a `0xdb48ce2b`
- decoded chunk `3`: compressed `8057`, expanded planar `8448`, planar FNV-1a `0x5c152b5c`, indexed-surface FNV-1a `0xa9c1ea92`, synthetic-palette RGBA upload hash `0xb75bdee9`; palette fade hash `0xa93a5ba5`, range-final hash `0x91f102c5`, faded RGBA sample hash `0x50918d48`, red-shift hash `0xb8462fc5`, red-shift RGBA sample hash `0xfa0a0cd7`, white-shift hash `0x3c8da1ed`, white-shift RGBA sample hash `0x93adda7f`, state-selected red-step-3 hash `0x90a6cdc5`, state-selected red RGBA sample hash `0xd742b64a`, base-palette RGBA reset hash `0xccd1a665`
- decoded chunk `87` (`TITLEPIC`): compressed `45948`, expanded planar `64000`, planar FNV-1a `0x01643ebc`, indexed-surface FNV-1a `0x4b172b02`
- decoded chunk `134` (`GETPSYCHEDPIC`): compressed `5127`, expanded planar `10752`, planar FNV-1a `0xeb393cc0`, indexed-surface FNV-1a `0x46e4bd08`

## Optional SOD committed assertions

When `game-files/base/m1` is present:

- `VGAHEAD.SOD` entries: `170` (`169` chunks plus sentinel)
- `VGAHEAD.SOD` size: `510`
- first offsets: `0`, `383`, final sentinel `947979`
- decoded chunk `0` (`STRUCTPIC`): compressed `383`, expanded `588`, FNV-1a `0x43f617ea`
- `STRUCTPIC` picture table: `147` entries, width range `8..320`, height range `8..200`, total declared pixels `1105792`
- representative SOD dimensions: entry `0` `320x200`, entry `1` `104x16`, entry `84` `320x200`, entry `90` `320x80`, entry `91` `320x120`
- decoded chunk `1` (`STARTFONT`): compressed `4448`, expanded `8300`, FNV-1a `0xdb48ce2b`
- decoded chunk `3`: compressed `42248`, expanded planar `64000`, planar FNV-1a `0x3a6afac3`, indexed-surface FNV-1a `0x5e85d9c1`
- decoded chunk `90`: compressed `10561`, expanded planar `12800`, planar FNV-1a `0xa5d5a6f7`, indexed-surface FNV-1a `0xff61711d`
- decoded chunk `149`: compressed `6243`, expanded planar `10752`, planar FNV-1a `0xeb393cc0`, indexed-surface FNV-1a `0x46e4bd08`

## Verification evidence

Command:

```bash
cd source/modern-c-sdl3
make clean test
```

Observed result:

```text
rm -rf build
mkdir -p build
cc -Iinclude -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -g src/wl_assets.c src/wl_map_semantics.c src/wl_game_model.c tests/test_assets.c -o build/test_assets
cd ../.. && source/modern-c-sdl3/build/test_assets
asset/decompression/semantics/model/vswap/upload-metadata tests passed for game-files/base
```

## Cycle update: STRUCTPIC metadata

Added `wl_decode_picture_table` to interpret decoded `STRUCTPIC` chunks as original `pictabletype` width/height pairs. Tests now assert picture counts, dimension ranges, total declared pixels, and representative WL6/SOD dimensions. This creates a clean metadata bridge from decoded graphics chunks toward a renderer-facing indexed-surface seam.

## Cycle update: indexed surfaces

Added `wl_decode_planar_picture_to_indexed`, a pure C conversion from the original VGA planar chunk layout into a linear 8-bit indexed surface. Tests assert stable hashes for representative WL6 and optional SOD surfaces while keeping all pixel bytes local and uncommitted.

## Cycle update: surface descriptor layer

Added `wl_indexed_surface` and `wl_decode_planar_picture_surface`, wrapping decoded indexed pixels with the metadata a future SDL3 upload path will need: indexed-8 format, width, height, stride, pixel count, and pixel pointer. Tests assert representative WL6/SOD surface descriptors plus stable surface hashes.

## Cycle update: indexed blit smoke test

Added `wl_blit_indexed_surface`, a clipped SDL-free blitter for `wl_indexed_surface`. Tests composite representative decoded WL6 picture surfaces into a synthetic indexed canvas and assert stable canvas hashes. Added `wl_texture_upload_descriptor`, `wl_describe_indexed_texture_upload`, and `wl_expand_indexed_surface_to_rgba` so indexed surfaces can be described for indexed-8 + RGB-palette upload or expanded to RGBA8888 without depending on SDL. This exercises the renderer-facing surface/upload layer without opening a window or committing pixels.

## Cycle update: palette fade metadata

Added `wl_interpolate_palette_range`, a pure C helper that mirrors the original `VL_FadeOut`/`VL_FadeIn` integer palette interpolation over an inclusive start/end index range. The tests use a synthetic 6-bit VGA palette and target fade color to assert full-range fade hash `0xa93a5ba5`, final-range hash `0x91f102c5`, faded RGBA expansion hash `0x50918d48`, and invalid range/component/step validation. This creates a deterministic palette-effect seam for future screen fades, damage/bonus flashes, and SDL3 texture upload without depending on a display or committing proprietary palette bytes.

## Cycle update: damage/bonus palette shifts

Added `wl_build_palette_shift`, a display-free helper for the original gameplay flash tables from `WL_PLAY.C::InitRedShifts`: damage palettes interpolate toward `(64,0,0)` with `REDSTEPS=8`, and bonus palettes interpolate toward `(64,62,0)` with `WHITESTEPS=20`. The headless tests assert the strongest representative damage/bonus hashes (`0xb8462fc5` and `0x3c8da1ed`), corresponding RGBA sample hashes (`0xfa0a0cd7` and `0x93adda7f`), endpoint component values, and invalid target/component/step validation. This gives future gameplay and SDL presentation code deterministic palette-flash data before any real palette upload.

## Cycle update: palette-shift state

Added a small `wl_palette_shift_state` seam with reset/start/update helpers mirroring `WL_PLAY.C::StartBonusFlash`, `StartDamageFlash`, and `UpdatePaletteShifts`. The update helper computes the palette selection before decrementing counters, clamps red to six shifts and white to three shifts, prioritizes red over white when both are active, requests a base-palette reset after the last shifted frame, and then returns no-op updates when already clear. Tests cover the no-op path, bonus-only white selection, overlapping damage+bonus red priority, counter clamping to zero, base reset, and invalid negative/null inputs.

## Cycle update: palette-shifted upload selection

Added `wl_select_palette_for_shift` and `wl_describe_palette_shifted_texture_upload`, bridging the palette-shift state machine to renderer upload metadata. Headless tests precompute original-style red/white shift tables, step through bonus-only, red-over-white, and base-reset states, select the effective palette, describe indexed texture upload with that palette, and expand a small indexed surface to RGBA. Assertions cover selected palette pointers/hashes, shifted RGBA hashes (`0x93adda7f`, `0xd742b64a`, `0xccd1a665`), and invalid out-of-range shift selection. This is the SDL-free boundary future presentation code can call before real texture upload.

## Next step

Add a minimal SDL3 presentation seam using the upload/palette metadata, or connect this palette-shifted upload path to future live gameplay/player damage and bonus events. Keep headless tests comparing metadata/hashes before requiring a display.


## Cycle update: player gameplay events

Added a small `wl_gameplay` seam that connects original-style player events to the palette-shift state without depending on the DOS status bar or VGA hardware. `wl_apply_player_damage` mirrors `TakeDamage` behavior for victory no-op, baby-difficulty quarter damage, god-mode health preservation, death state, `gotgatgun` reset, and `StartDamageFlash` damage accumulation. `wl_start_player_bonus_flash` routes bonus events to the white-flash counter. `wl_award_player_points` mirrors `GivePoints` score/extra-life threshold progression using `EXTRAPOINTS=40000`, including advancing thresholds even when lives are capped at 9. Headless tests assert the resulting health, score, lives, thresholds, and palette-shift counters.


## Cycle update: bonus pickup events

Extended `wl_gameplay` to model original-style pickup events from `GetBonus`, `GiveAmmo`, `GiveWeapon`, and `GiveKey`. The seam now covers ammo restoration from knife, 99-ammo clamp, ammo-full pickup rejection, key bitmasks, machinegun/chaingun weapon promotion, chaingun face flag, treasure point awards, first-aid/food/alpo/gibs health gates, full-heal one-up/ammo/treasure behavior, Spear completion state, and bonus flash triggering only for picked-up items. Headless tests assert state transitions without depending on sound, status-bar drawing, or VGA hardware.


## Cycle update: runtime static pickup hooks

Connected bonus pickup semantics to runtime static descriptors with `wl_try_pickup_static_bonus`. Active bonus statics map from `statinfo` type ids to `wl_bonus_item`, call the same gameplay event seam, and are deactivated only when the pickup succeeds, matching the original `GetBonus` behavior where full-health/full-ammo pickups remain in the world. Scene-ref collection skips inactive statics, giving renderer-facing proof that a consumed bonus disappears from future sprite input without mutating asset bytes.


## Cycle update: live tick palette bridge

The gameplay palette-shift state is now advanced through `wl_step_live_tick`, alongside movement/use/door/pushwall state. The new tick test proves a food pickup can start the bonus flash and return a white palette-selection result from the same headless update call, keeping future SDL3 texture-upload/palette selection tied to gameplay events without needing a display.


## Cycle update: live tick palette upload

Extended the live tick palette bridge into upload metadata. A movement tick that picks up food now returns a white palette-shift result, which is immediately passed to `wl_describe_palette_shifted_texture_upload`; the test asserts the selected white-shift palette pointer and RGBA sample hash `0x93adda7f`. This ties gameplay-triggered palette flashes to the SDL-free texture upload descriptor seam.


## Cycle update: actor bite palette damage

The red palette-shift path now receives actor-driven damage, not only direct player-damage calls. A deterministic dog-bite test routes `damage_roll >> 4` through `wl_apply_player_damage`, verifies baby-difficulty scaling and red shift selection, and stays SDL-free for future texture-upload integration.


## Cycle update: actor shooting palette damage

The red palette-shift path now receives deterministic actor shooting damage as well as bite damage. The headless shooting test feeds explicit hit/damage rolls through `wl_try_actor_shoot_player`, verifies difficulty scaling and damage counters through `wl_apply_player_damage`, and keeps all presentation work on the existing SDL-free palette/upload boundary.


## Cycle update: projectile palette damage

Projectile impacts now feed the same red palette-shift path as direct, bite, and shooting damage. The headless projectile test routes needle/rocket/fire impact damage through `wl_apply_player_damage`, verifies difficulty scaling and damage counters, and keeps the future presentation boundary on existing SDL-free palette/upload metadata.


## Cycle update: live projectile tick palette damage

Projectile impact damage now reaches palette selection through live tick orchestration. `wl_step_live_projectile_tick` steps projectile damage before `wl_update_palette_shift_state`, so the same frame returns a red palette-shift result suitable for the existing shifted texture-upload descriptor path.


## Cycle update: live actor attack palette damage

Actor attack damage now reaches palette selection through live tick orchestration. `wl_step_live_actor_tick` steps deterministic bite/shoot damage before `wl_update_palette_shift_state`, so same-frame dog bites and guard shots return red palette-shift metadata for the existing shifted texture-upload boundary.


## Cycle update: live combat palette damage

Cumulative actor/projectile frame damage now reaches palette selection through a single live combat tick. `wl_step_live_combat_tick` applies actor and projectile damage before `wl_update_palette_shift_state`, so the returned red palette-shift metadata reflects total same-frame damage for the existing shifted texture-upload boundary.


## Cycle update: live combat upload selection

Cumulative combat damage now reaches the existing shifted texture-upload metadata. `wl_step_live_combat_tick` returns a red palette result that is fed to `wl_describe_palette_shifted_texture_upload`; tests assert the selected red-shift palette pointer, palette hash, and RGBA expansion hash for the same frame.


## Cycle update: actor kill gameplay state

Actor kill state now feeds player score/extra-life progression before presentation. `wl_apply_actor_damage` awards kill points through `wl_award_player_points`, records representative drop metadata, and stays independent of SDL/audio/status-bar code so later actor death visuals can consume deterministic gameplay state.


## Cycle update: actor drop pickup bridge

Actor kill drops now bridge into the runtime static/pickup path before presentation. Spawned drop statics carry source/type metadata compatible with existing pickup and future scene-ref rendering, keeping kill rewards deterministic and SDL-free.


## Cycle update: live actor damage/drop tick

Actor damage/drop state now reaches the same live tick boundary that future presentation will consume. The new live actor damage tick applies kill state, spawns a renderer-compatible runtime static drop, and advances palette state after the frame's gameplay mutations, keeping actor kill rewards ready for scene-ref rendering and shifted upload selection without an SDL dependency.


## Cycle update: live actor drop scene rendering

Renderer-facing upload/composition coverage now includes drops created by actor kill state during a live tick. The spawned static is collected as a scene ref, decoded through the local VSWAP sprite cache, and composited in the indexed scene renderer with stable hash `0x707dbe4e`, keeping the future SDL3 path grounded in deterministic headless metadata.


## Cycle update: full live combat palette selection

The broader live combat tick now coalesces outgoing actor damage/drop effects with incoming bite/projectile damage before palette advancement. A same-frame guard kill, dog bite, and needle projectile hit yields one deterministic red palette result, preserving the SDL-free shifted upload boundary for future presentation.


## Cycle update: actor death-state presentation metadata

The gameplay layer now emits deterministic death-frame source indices and tic progression for killed actors, including a one-shot death-scream marker and final dead-frame behavior. This gives future SDL3/render upload code a display-free contract for death visuals before decoded VSWAP frames are attached to live scene refs.


## Cycle update: actor death-state scene rendering

Death-frame presentation is now covered before SDL3. The current death-state sprite source index becomes a scene ref, is decoded from local VSWAP data, and is composited by the indexed renderer with stable hash `0x2e8b4819`, keeping the future SDL upload path deterministic and headless.


## Cycle update: full live combat death refs

Death-frame presentation metadata is now part of full live combat output. A same-frame kill starts death-state progression and returns a scene ref for the current death sprite, so future SDL3 presentation can consume death visuals from the same frame result as palette shifts and drop spawning.


## Cycle update: full combat death-ref scene rendering

Full live combat death refs now reach indexed scene composition before SDL3. The returned death sprite ref is decoded from local VSWAP data and rendered with the door-aware scene renderer, producing hash `0x2e8b4819` and keeping presentation verification headless.


## Cycle update: actor death final-frame refs

Actor death progression now has a persistent renderer-facing final-frame bridge. Once death state finishes, the runtime actor scene ref can be overridden to the final death sprite, so future SDL3 presentation can continue drawing the corpse from normal scene-ref collection instead of a one-off combat result.


## Cycle update: live actor death tick

The renderer-facing death animation path now has a live ticking contract. Each tick can return the current death-frame scene ref, and the final tick installs the corpse sprite override so ordinary scene collection keeps drawing the final frame for future SDL3 presentation.


## Cycle update: full combat death tick orchestration

Death animation ticking is now available from the full combat frame boundary that future SDL3 presentation will consume. The combined result exposes ordinary combat output plus the current death-frame ref/final-frame override, keeping presentation state deterministic and headless.


## Cycle update: full combat death-tick final scene

The final corpse frame emitted by combined full-combat/death ticking now reaches indexed scene composition. The renderer test decodes the local VSWAP sprite and produces hash `0x81c10dcf`, giving future SDL3 presentation a deterministic headless contract for finalized actor deaths.


## Cycle update: broadened actor death final refs

The persistent corpse-frame presentation bridge now covers more actor classes. Finalized officer, SS, dog, mutant, and boss death states flow through ordinary scene-ref collection with verified source/VSWAP metadata, preparing the SDL3 renderer path for non-guard corpse sprites.


## Cycle update: patrol actor scene refs

The actor movement seam now has presentation-input coverage before SDL3. Patrol stepping updates the runtime actor tile, then scene-ref collection reports deterministic guard sprite metadata and world-center coordinates for future wall+sprite composition.


## Cycle update: patrol actor scene render

The patrol actor movement/ref seam now reaches indexed scene composition before SDL3. A patrol guard ref is projected through the runtime door-aware scene renderer with stable source/visibility/hash assertions, keeping presentation verification headless.


## Cycle update: live actor AI patrol tick

The actor AI movement seam now returns from a presentation-facing live frame result. Patrol movement can be advanced alongside palette/environment tick output before later scene-ref collection and SDL3 upload.


## Cycle update: live AI patrol scene refs

Live AI patrol movement now has a deterministic presentation-input contract: scene refs expose stable sprite source/chunk metadata and moved world coordinates for later sprite-cache rendering and SDL3 upload.


## Cycle update: live AI patrol rendering

AI-updated patrol actor refs now have a deterministic headless presentation test. The door-aware scene renderer composites the moved guard sprite and verifies visible sprite metadata plus canvas hash `0x6ee1f8bf`, preserving the SDL3 upload boundary for later.


## Cycle update: patrol fine-position refs

Renderer-facing actor refs now can use fixed-point partial patrol positions. This gives the SDL3 presentation bridge smoother movement input without requiring SDL or decoded sprite assets in git.


## Cycle update: live AI fine-position patrol rendering

The headless renderer now verifies sub-tile patrol placement emitted by live AI. Fine actor coordinates feed scene projection and produce stable hash `0xcf61b07b`, preparing smoother future SDL3 presentation without requiring SDL in tests.


## Cycle update: patrol remainder accumulation

Renderer-facing actor movement can now receive accumulated sub-tile patrol state across frames, giving the future SDL3 path smoother movement inputs without changing sprite assets.


## Cycle update: live AI remainder rendering

The renderer now verifies accumulated sub-tile patrol state across live AI frames. Fine-position and tile-centered guard placement produce distinct stable hashes before any SDL3 dependency is introduced.


## Cycle update: live AI dog fine-position rendering

Headless presentation coverage now includes a fine-position patrol dog. The scene renderer projects the dog sprite from live AI refs and verifies stable hash `0x08ab64f0` before SDL3.


## Cycle update: live AI officer fine-position rendering

Headless presentation coverage now includes a fine-position patrol officer. The scene renderer projects the officer sprite from live AI refs and verifies stable hash `0xa6544334` before SDL3.
