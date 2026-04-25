# Source Inventory and Test-Driven Port Strategy

Research cycle: 2026-04-24 22:05-22:15 CDT  
Scope: pristine original source at `source/original/WOLFSRC`, local game data at `game-files/base`, target implementation under `source/modern-c-sdl3`.

## Ground Rules Verified

- `source/original/` is the pristine id Software source submodule and was not modified.
- Proprietary game data is present under `game-files/base/` and is ignored by git through `game-files/.gitignore`.
- Existing target skeleton is `source/modern-c-sdl3/README.md`; no implementation exists yet.
- Existing test area is `tests/README.md`; no runnable test harness exists yet.

## Original Source Architecture Summary

The original code is a Borland/DOS-era C codebase with inline assembly and separate `.ASM` modules. The engine is organized around id's reusable DOS subsystems plus Wolfenstein-specific gameplay/rendering files.

### Core id Engine Subsystems

| Area | Primary files | Role in original |
| --- | --- | --- |
| Common/types/includes | `ID_HEADS.H`, `ID_HEAD.H`, `VERSION.H`, version headers | Central include hub, compile-time SKU selection (`WL6`, `SOD`, etc.), Borland/DOS headers, common types. |
| Cache / data decompression | `ID_CA.C`, `ID_CA.H` | Opens game data files, reads map/graphics/audio headers, performs Huffman, Carmack, and RLEW expansion, caches chunks. |
| Memory manager | `ID_MM.C`, `ID_MM.H` | DOS segmented memory allocator, purge/lock model, near/far memory handling. |
| Page manager | `ID_PM.C`, `ID_PM.H` | Loads paged assets from `VSWAP.*`; manages sprite/sound page ranges and DOS main/EMS/XMS memory. |
| Video low-level | `ID_VL.C`, `ID_VL.H`, `ID_VL_A.ASM` | VGA mode, palette, planar screen/latch operations, blits, fades. |
| Video high-level | `ID_VH.C`, `ID_VH.H` | Higher-level draw helpers that sit above `ID_VL`. |
| Input | `ID_IN.C`, `ID_IN.H` | Keyboard/mouse/joystick polling, control state, acknowledgements. |
| Sound | `ID_SD.C`, `ID_SD.H`, `ID_SD_A.ASM`, audio headers | AdLib/Sound Blaster/PC speaker music and sound effects, hardware detection and playback. |
| User/UI support | `ID_US_1.C`, `ID_US.H`, `ID_US_A.ASM` | Text windows, random number helpers, UI utility behavior. |

### Wolfenstein Game Layer

| Area | Primary files | Role in original |
| --- | --- | --- |
| Entry/startup | `WL_MAIN.C` | `main`, `CheckForEpisodes`, `InitGame`, `DemoLoop`, `Quit`; subsystem initialization order. |
| Menus/control panel | `WL_MENU.C`, `WL_MENU.H` | Episode/data-file detection, config/save naming, menus, copy protection/intro flows. |
| Game loop/level setup | `WL_GAME.C` | `SetupGameLevel`, map loading, wall/door/actor/static spawn preparation. |
| Player/agent | `WL_AGENT.C` | Player control, weapons, damage, movement. |
| Actor behavior | `WL_ACT1.C`, `WL_ACT2.C`, `WL_STATE.C` | Enemy/static object spawning and state machines. |
| Play/render loop | `WL_PLAY.C`, `WL_DRAW.C` | Per-frame play behavior, raycasting preparation, visible object handling. |
| Scaling/rendering | `WL_SCALE.C`, `CONTIGSC.C`, `OLDSCALE.C`, `WL_ASM.ASM`, `WL_DR_A.ASM` | Compiled scaler generation, sprite/wall scaling, VGA planar optimized drawing. |
| Intermission/text/debug | `WL_INTER.C`, `WL_TEXT.C`, `WL_DEBUG.C` | End-level stats, text pages, debug tooling. |

### Startup Shape

The important startup sequence in `WL_MAIN.C::InitGame` is:

1. `MM_Startup()`
2. `SignonScreen()`
3. `VW_Startup()`
4. `IN_Startup()`
5. `PM_Startup()` then `PM_UnlockMainMem()`
6. `SD_Startup()`
7. `CA_Startup()`
8. `US_Startup()`
9. Build lookup/update tables, read config, cache font/latch assets, build trig/scaling/wall tables.

`main()` first calls `CheckForEpisodes()` in `WL_MENU.C`, which detects available file extensions (`WL6`, `WL3`, `WL1`, `SOD`, `SDM`, Japanese variants) and appends the selected extension to names such as `VSWAP.`, `AUDIO.`, config, saves, and demos. For this repo, `game-files/base` contains the full `WL6` data and `game-files/base/m1` contains `SOD` data.

## Likely DOS / Platform Abstraction Seams

These should become explicit modern interfaces instead of being ported literally.

### File and Asset I/O

- Original: `open/read/lseek/filelength/findfirst`, DOS paths, `CA_FarRead`, `CA_FarWrite`, in-place globals.
- Modern seam: `wl_file`/`wl_assets` module using normal C stdio or SDL IO streams.
- Testability: excellent. Data files can be parsed offline with deterministic fixtures and no SDL window.

### Memory Management

- Original: `ID_MM` purge/lock segmented allocator, `memptr`, `_seg`, `far`, `huge`, EMS/XMS in `ID_PM`.
- Modern seam: ordinary heap allocation, explicit owned buffers, optional cache layer preserving chunk semantics but not DOS memory behavior.
- Testability: medium. Exact allocation/purge behavior is not worth reproducing initially; chunk availability and bytes should be tested instead.

### Data Decompression

- Original: `CAL_CarmackExpand`, `CA_RLEWexpand`, `CAL_HuffExpand`; map caching expands each 64x64 plane.
- Modern seam: pure C decompression functions with byte/word-oriented inputs and explicit output sizes.
- Testability: excellent. This is the best first bridge between original data and modern implementation.

### Page/VSWAP Management

- Original: `ID_PM` opens `VSWAP.*`, reads header (`ChunksInFile`, `PMSpriteStart`, `PMSoundStart`), offsets, lengths, then pages data into DOS memory.
- Modern seam: parse VSWAP header and return immutable chunk descriptors / buffers on demand.
- Testability: excellent for header/chunk parsing; medium for render semantics until shape decoding is implemented.

### Graphics / VGA / Palette

- Original: planar VGA writes (`SCREENSEG`, map masks, latch memory), palette DAC, split screen, compiled scalers, assembly blitters.
- Modern seam: software 8-bit indexed framebuffer plus palette, then SDL3 texture presentation. Keep renderer-visible intermediate buffers deterministic.
- Testability: good if tests compare palette-index framebuffers or decoded assets before SDL output; poor if comparing actual DOS VGA side effects.

### Input

- Original: keyboard scan codes, mouse/joystick hardware polling, interrupt-ish state arrays.
- Modern seam: SDL3 event translation into original-style control state (`ControlInfo`, button bits, scan-code compatibility map).
- Testability: good with synthetic SDL/control events; original-vs-modern comparison is less useful than state-machine tests.

### Sound / Music

- Original: AdLib/Sound Blaster/PC speaker hardware paths, interrupt/timer-driven playback, audio chunks from `AUDIOT.*` and `VSWAP.*`.
- Modern seam: decode original sound/music formats into deterministic event/sample streams, then SDL3 audio output.
- Testability: medium. Header/chunk parsing first; exact hardware synthesis and timing can be characterized later.

### Timing

- Original: DOS timer ticks (`TickBase`, `tics`, VBL waits) and hardware waits.
- Modern seam: fixed simulation tick clock separated from SDL real-time presentation.
- Testability: good for deterministic game logic once time is injected.

## Game Data Files / Formats to Test First

Observed local data:

### Wolfenstein 3D full (`game-files/base`)

- `MAPHEAD.WL6` — 402 bytes. First word RLEW tag verified as `0xabcd`; followed by 100 32-bit map-header offset slots. `ID_CA.H` defines `NUMMAPS` as 60 for the original WL6 build, so the modern parser should read the table generically but the first WL6 gameplay target should constrain itself to the original playable map count.
- `GAMEMAPS.WL6` — 150,652 bytes. Contains map headers/plane data. First map header at offset 2250, name `Wolf1 Map1`, dimensions 64x64, plane starts `(11, 1445, 2240)`, plane lengths `(1434, 795, 10)`.
- `VSWAP.WL6` — 1,544,376 bytes. Header verified: `ChunksInFile=663`, `PMSpriteStart=106`, `PMSoundStart=542`; first chunk offsets are 4096, 8192, 12288, 16384, 20480.
- `VGAHEAD.WL6` — 450 bytes. Three-byte graphics chunk offset table; first bytes `00 00 00 62 01 00 ed 0e 00 62 25 00 db 44 00 8a`.
- `VGADICT.WL6` — 1024 bytes. Huffman dictionary for `VGAGRAPH.WL6`.
- `VGAGRAPH.WL6` — 275,774 bytes. Huffman-compressed graphics chunks.
- `AUDIOHED.WL6` — 1,156 bytes. 32-bit chunk offset table; first offsets `(0, 15, 28, 44, 102)`.
- `AUDIOT.WL6` — 320,209 bytes. Audio data chunks.
- `CONFIG.WL6`, `SAVEGAM0.WL6`, `wolf3d.exe`, launcher files are present but should not drive the first port milestones.

### Spear of Destiny (`game-files/base/m1`)

Useful as a second-format validation target after WL6:

- `MAPHEAD.SOD` — 402 bytes, RLEW tag `0xabcd`, 100 map offset slots.
- `GAMEMAPS.SOD` — first map at offset 2097, name `Tunnels 1`, dimensions 64x64.
- `VSWAP.SOD` — `ChunksInFile=666`, `PMSpriteStart=134`, `PMSoundStart=555`.
- `VGAHEAD.SOD`, `VGADICT.SOD`, `VGAGRAPH.SOD`, `AUDIOHED.SOD`, `AUDIOT.SOD` present.

### Recommended First Test Order

1. **File presence and non-secret metadata**: verify required WL6 files exist and match expected sizes/header constants without committing assets.
2. **MAPHEAD/GAMEMAPS parser**: parse RLEW tag, 100 map offset slots, original playable map count, map headers, dimensions and names.
3. **Carmack + RLEW expansion**: decode WL6 map 0 planes to 64x64 word arrays, hash/fixture the result.
4. **VSWAP parser**: parse chunk count/ranges, offsets/lengths, sparse pages, first chunk descriptors.
5. **VGAHEAD/VGAGRAPH/VGADICT parser**: parse 3-byte offsets and Huffman dictionary, then decode one small graphics chunk such as `STRUCTPIC`/pictable once constants are wired.

## Original-vs-Modern Comparison Feasibility

### Best Candidates for Direct Comparison

Direct comparison is feasible where original behavior is pure data transformation or can be extracted from source into a host-side harness outside `source/original/`:

- `CAL_CarmackExpand` behavior.
- `CA_RLEWexpand` behavior (the original has an inactive C version and active asm; the algorithm is visible and simple enough to port faithfully).
- `CA_RLEWCompress` behavior if compression tests are useful later.
- `CAL_HuffExpand` behavior for graphics chunks.
- `MAPHEAD`/`GAMEMAPS` header parsing and decoded plane arrays.
- `VSWAP` header/chunk descriptors.
- Fixed-point/trig/table generation (`BuildTables`, ray angle tables) once dependencies are isolated.
- Actor/map spawn classification from `SetupGameLevel` after decoded maps are available.

### Hard or Low-Value Candidates for Exact Comparison

- DOS memory purge/lock/EMS/XMS behavior in `ID_MM`/`ID_PM`: replicate chunk results, not memory placement.
- VGA planar side effects, latch memory, CRTC state, DAC fade timing: compare indexed framebuffer/palette outputs instead.
- Compiled scaler machine code generation in `CONTIGSC.C`: preserve visual results and table semantics, not self-modifying/compiled x86 code.
- Sound card hardware writes and interrupt timing: compare decoded music/sound event streams before SDL audio output.
- Interactive menu/input flows: better covered by synthetic control-state tests and smoke tests.

### Practical Harness Approach

Avoid trying to compile the full original DOS program on modern Linux first. It depends on Borland headers, segmented memory keywords, inline assembly, hardware interrupts, and VGA registers. Instead:

1. Implement modern pure-C modules from the original algorithms under `source/modern-c-sdl3/src/`.
2. Keep tiny reference snippets/tests outside `source/original/` that are either:
   - direct independent reproductions of documented algorithms, or
   - generated fixtures from local data plus known hashes.
3. Use local data-derived fixtures as the oracle for asset loaders. Do not commit proprietary bytes; commit only small metadata/hashes where safe.
4. When exact original-vs-modern comparison is needed, build a minimal extracted reference harness under `tests/reference/` that includes copied/adapted algorithm code with attribution, not modifications to the submodule.

This gives high confidence without being blocked by DOS build tooling.

## First 5 Concrete Test Milestones

1. **Portable test skeleton and asset locator**
   - Add a minimal C test runner or CMake/CTest layout under `source/modern-c-sdl3` / `tests`.
   - Locate `game-files/base` via repo-relative path or `WOLF3D_DATA_DIR`.
   - Test only metadata: required WL6 files exist, expected sizes match the observed local install, and no assets are copied into git.

2. **Map header parser test**
   - Implement `wl_maphead_parse` and `wl_gamemaps_read_header`.
   - Assert WL6 `MAPHEAD` length 402, RLEW tag `0xabcd`, 100 offset slots, original `NUMMAPS` target of 60 for WL6 gameplay, map 0 offset 2250, map 0 name `Wolf1 Map1`, dimensions 64x64, plane starts/lengths as observed.
   - Add SOD as optional secondary assertions if `game-files/base/m1` exists.

3. **Map plane decompression test**
   - Implement little-endian word readers, Carmack expansion (`NEARTAG=0xa7`, `FARTAG=0xa8`), and RLEW expansion.
   - Decode WL6 map 0 planes 0 and 1 to 4096 words each.
   - Assert output sizes, stable hashes, and key semantic counts (walls/floors/actor info) rather than embedding map bytes.

4. **VSWAP parser test**
   - Implement `wl_vswap_parse` to read chunk count, sprite/sound starts, offsets, lengths, and validate monotonic/nonzero chunks.
   - Assert WL6 `663/106/542` and first offsets `4096,8192,12288,16384,20480`; assert SOD `666/134/555` when available.
   - Add a first chunk read smoke test without decoding shape semantics yet.

5. **Graphics header/Huffman smoke test**
   - Implement 3-byte `VGAHEAD` offset reader and `VGADICT` node parsing.
   - Port `CAL_HuffExpand` as a pure function.
   - Decode a small known graphics chunk or the `STRUCTPIC` pictable once chunk constants are imported from `GFXV_WL6.H`; assert decompressed length and hash.

After these five, the project should have a reliable asset/decompression foundation and can start porting `SetupGameLevel`/spawn classification in a test-first way.

## Risks and Blockers

- **Borland/DOS coupling is deep.** Full original compilation is not a near-term oracle because of segmented memory qualifiers, inline assembly, DOS interrupts, VGA register I/O, EMS/XMS, and Borland-specific headers.
- **Version/SKU selection is compile-time plus runtime.** `VERSION.H` and sibling version headers choose `WOLF`/`SPEAR` constants, while `CheckForEpisodes` chooses data extension at runtime. Modern code needs an explicit detected `wl_asset_set` rather than relying on global extension strings.
- **Map constants differ by SKU.** Header files such as `MAPSWL6.H`, `GFXV_WL6.H`, `AUDIOSOD.H` define enum values/chunk counts. The first implementation should target WL6 only, but design parsers to accept SOD metadata.
- **Original behavior often hides in globals.** `SetupGameLevel`, actor spawning, and rendering assume many global arrays (`mapsegs`, `tilemap`, `actorat`, `farmapylookup`, `gamestate`). Tests will need controlled initialization.
- **Renderer fidelity is not just SDL output.** The original uses planar VGA and compiled scalers. SDL3 should present an 8-bit indexed framebuffer or equivalent palette-converted texture; tests should compare pre-SDL buffers.
- **Audio fidelity can become a rabbit hole.** Header/chunk parsing is straightforward, but AdLib/PC speaker/Sound Blaster exact synthesis should wait until core data/map/play loop is stable.
- **Commercial assets cannot be committed.** Tests must run against local data or use tiny synthetic fixtures. Commit only code, metadata expectations, hashes, and docs.
- **Potential legal caution.** The original source appears present as a submodule and local purchased data is ignored. Avoid public distribution of combined source+assets and do not upload game files.

## Recommended Next Implementation Step

Create the first modern C test harness and implement the WL6 map header parser plus metadata tests.

Rationale:

- It is the narrowest useful vertical slice.
- It validates local data discovery, endian handling, original struct layout (`maptype`), and SKU selection.
- It does not require SDL3 yet, so setup friction is low.
- It sets up the exact module boundaries needed for subsequent Carmack/RLEW tests.

Concrete next commit should add:

- `source/modern-c-sdl3/include/wl_assets.h`
- `source/modern-c-sdl3/src/wl_assets.c`
- `source/modern-c-sdl3/tests/test_assets.c` or a repo-level `tests` C runner
- build instructions/scripts for running the metadata test
- assertions for the verified WL6 values listed above

Keep SDL3 integration deferred until after asset parsing/decompression has deterministic tests.
