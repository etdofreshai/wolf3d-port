# Audio Asset Header Harness

This note records the current AUDIOHED/AUDIOT characterization seam for future PC speaker, AdLib, digitized sound, music, and SDL3 audio work.

## Scope

- `wl_read_audio_header()` reads little-endian 32-bit chunk offsets from `AUDIOHED.*`.
- `wl_read_audio_chunk()` copies bounded raw chunk bytes from `AUDIOT.*` using adjacent offsets.
- `wl_describe_audio_chunk()` classifies the original WL6 chunk ranges and exposes raw size, declared length, sound priority where present, and payload bounds.
- `wl_describe_audio_chunk_with_ranges()` applies the same descriptor logic to caller-supplied range counts, covering SOD's shorter PC/AdLib/digital tables without hardcoded WL6 cutoffs.
- `wl_summarize_audio_range()` summarizes AUDIOHED offset ranges without reading AUDIOT payloads, giving future audio schedulers quick counts/byte totals for PC speaker, AdLib, digital, and music ranges.
- `wl_describe_pc_speaker_sound()` validates the PC speaker declared sample count and exposes first/last sample bytes, the terminating sentinel, and any trailing bytes.
- `wl_get_pc_speaker_sound_sample()` decodes a bounded PC speaker sample byte by index so future SDL3/audio playback code can advance through sound data without raw offset math.
- `wl_advance_pc_speaker_playback_cursor()` advances a PC speaker sample index by a frame/sample delta, reporting the next sample, consumed samples, and completion state for frame-to-frame sound playback.
- `wl_describe_pc_speaker_playback_window()` summarizes a bounded PC speaker sample window from a start index and sample budget for deterministic future mixer steps.
- `wl_describe_pc_speaker_playback_position()` maps an absolute PC speaker sample position to the current sample or completed state without advancing mutable playback state.
- `wl_describe_adlib_sound()` validates AdLib sound chunks by the original common header plus 16-byte instrument block and exposes first/last instrument and sample bytes for future OPL playback seams.
- `wl_get_adlib_instrument_byte()` decodes a bounded 16-byte AdLib instrument byte by index so future OPL setup code can consume operators without raw offset math.
- `wl_describe_adlib_instrument_registers()` maps the 16-byte AdLib instrument block into channel-0 OPL modulator/carrier register/value pairs plus feedback, voice, and mode metadata for future SDL3/OPL setup.
- `wl_get_adlib_sound_sample()` decodes a bounded AdLib sound sample byte by index so future OPL playback code can advance through sound data without raw offset math.
- `wl_advance_adlib_playback_cursor()` advances an AdLib sample index by a frame/sample delta, reporting the next sample, consumed samples, and completion state while matching the PC speaker cursor contract for future shared mixer scheduling.
- `wl_describe_adlib_playback_window()` summarizes a bounded AdLib sample window from a start index and sample budget for deterministic future OPL/mixer steps.
- `wl_describe_adlib_playback_position()` maps an absolute AdLib sample position to the current sample or completed state, matching the PC speaker position descriptor contract.
- `wl_describe_imf_music_chunk()` validates IMF music chunks by their declared byte count and summarizes command count, first/last commands, maximum and zero-delay counts, total delay ticks, and trailing AUDIOT bytes.
- `wl_get_imf_music_command()` decodes a bounded IMF command `(register, value, delay)` by index for future AdLib playback scheduling without exposing callers to raw byte offsets.
- `wl_describe_imf_playback_window()` summarizes deterministic IMF command emission from a start index within a tick budget for future SDL3 audio scheduling.
- `wl_describe_imf_playback_position()` maps an absolute IMF tick position to the active command and delay remainder, giving future SDL3 audio code a deterministic seek/cursor seam.
- `wl_advance_imf_playback_cursor()` advances a command index plus intra-command elapsed delay by a tick delta, reporting the next command cursor, consumed ticks, completed commands, and completion state for frame-to-frame music playback.

## Verified WL6 metadata

- `AUDIOHED.WL6`: 1,156 bytes = 289 offsets = 288 chunks plus sentinel.
- First offsets: `0, 15, 28, 44, 102`.
- `AUDIOT.WL6`: 320,209 bytes; sentinel offset is within the file and equals the observed file end.
- Range summaries pin the WL6 PC speaker range as 87/87 non-empty chunks totaling 9,986 bytes (largest chunk 50 at 313 bytes), the AdLib range as 87/87 non-empty chunks totaling 12,969 bytes (largest chunk 137 at 339 bytes), the digitized-sound range as 1/87 non-empty chunks totaling 4 bytes (chunk 260 sentinel/marker), and the music range as 27/27 non-empty chunks totaling 297,250 bytes (largest chunk 280 at 22,578 bytes).
- Representative chunks:
  - chunk 0: 15 bytes, FNV-1a `0x5971ec53`; PC speaker sample count 8, first/last samples `0x83`/`0x84`, bounded sample accessor first/last bytes `0x83`/`0x84`, playback cursor covers zero-delta, last-sample, clamped completion, completed no-op, and invalid completed-delta states; playback windows cover mid-sound samples 2..4 and completion from sample 7; terminator `0x00`, trailing bytes 0.
  - chunk 1: 13 bytes, FNV-1a `0x21985d89`; PC speaker sample count 6, first/last samples `0x2f`/`0x2f`, bounded sample accessor last byte `0x2f`, terminator `0x00`.
  - chunk 87: 41 bytes, FNV-1a `0x799f60b1`; AdLib sample count 8, priority 1, instrument first/last bytes `0x70`/`0x03`, bounded instrument accessor first/last bytes `0x70`/`0x03`, register descriptor maps first modulator/carrier values `0x70`/`0x75`, waveform values `0x00`/`0x00`, feedback `0x01`, voice `0x32`, and mode `0xd5`, sample first/last bytes `0x04`/`0x2e`, bounded sample accessor first/last bytes `0x04`/`0x2e`, playback cursor covers zero-delta, last-sample, clamped completion, completed no-op, and invalid completed-delta states; playback windows cover mid-sound samples 3..6 and completed zero-length-at-end behavior, trailing bytes 11.
  - chunk 174: 0 bytes.
  - chunk 261: 7,546 bytes, FNV-1a `0xea0d69d8`; IMF declared bytes 7,456, command count 1,864, command accessor first `(reg=0, value=0, delay=189)`, accessor last `(reg=0, value=0, delay=1)`, playback windows emit 1 command for 189 ticks and 3 commands for 20,485 ticks, playback positions map tick 188 to command 0 with 1 tick remaining, tick 189 to command 1, and total delay to completed command index 1,864; playback cursor advancement covers intra-command progress, command-boundary carry, multi-command advancement, final completion, and invalid elapsed states; max delay 64,098, zero-delay commands 0, total delay 25,697,407, trailing bytes 86.
  - chunk 287: 20,926 bytes, FNV-1a `0x65998666`; IMF declared bytes 20,836, command count 5,209, first delay 189, last command `(reg=0, value=0, delay=1)`, max delay 65,429, zero-delay commands 0, total delay 71,494,600, trailing bytes 86.

## Verified optional SOD metadata

When local Spear data is present under `game-files/base/m1`:

- `AUDIOHED.SOD`: 1,072 bytes = 268 offsets = 267 chunks plus sentinel.
- First offsets: `0, 15, 66, 82, 174`.
- `AUDIOT.SOD`: 328,620 bytes; sentinel offset equals the file size.
- Range summaries pin the optional SOD PC speaker range as 81/81 non-empty chunks totaling 6,989 bytes (largest chunk 17 at 306 bytes), the AdLib range as 81/81 non-empty chunks totaling 10,607 bytes (largest chunk 119 at 338 bytes), the digitized-sound range as 1/81 non-empty chunks totaling 4 bytes (chunk 242 sentinel/marker), and the music range as 24/24 non-empty chunks totaling 311,020 bytes (largest chunk 258 at 22,578 bytes).
- Representative chunks:
  - chunk 0: 15 bytes, FNV-1a `0x5971ec53`.
  - chunk 1: 51 bytes, FNV-1a `0xfafa57eb`.
  - chunk 87: 69 bytes, FNV-1a `0xf0dfcb70`.
  - chunk 174: 0 bytes.
  - chunk 243: 21,730 bytes; generic SOD range descriptor classifies it as music with declared length 21,640, payload offset 4, and payload size 21,726; IMF command count 5,410, first command `(reg=0, value=0, delay=189)`, last command `(reg=0, value=0, delay=1)`, playback window emits 1 command for 100 ticks, tick-position/cursor advancement at tick 500 lands on command 2 with 303 ticks elapsed and 19,985 remaining; max delay 65,419, zero-delay commands 0, total delay 65,434,029, trailing bytes 86.
  - chunk 261: 13,286 bytes, FNV-1a `0x04a8dbe2`.
  - chunk 266: 6,302 bytes, FNV-1a `0x0fdb4632`.

## Verification

```bash
cd source/modern-c-sdl3
WOLF3D_DATA_DIR='/home/node/.openclaw/tmp/Wolfenstein 3D port/game-files/base' make test
make sdl3-info test-sdl3
```

Result: headless asset/decompression/runtime/audio tests pass; SDL3 smoke test gracefully skips when SDL3 is unavailable in the worktree.
