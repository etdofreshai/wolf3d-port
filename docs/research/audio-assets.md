# Audio Asset Header Harness

This note records the current AUDIOHED/AUDIOT characterization seam for future PC speaker, AdLib, digitized sound, music, and SDL3 audio work.

## Scope

- `wl_read_audio_header()` reads little-endian 32-bit chunk offsets from `AUDIOHED.*`.
- `wl_read_audio_chunk()` copies bounded raw chunk bytes from `AUDIOT.*` using adjacent offsets.
- `wl_describe_audio_chunk()` classifies the original WL6 chunk ranges and exposes raw size, declared length, sound priority where present, and payload bounds.
- `wl_describe_audio_chunk_with_ranges()` applies the same descriptor logic to caller-supplied range counts, covering SOD's shorter PC/AdLib/digital tables without hardcoded WL6 cutoffs.
- `wl_summarize_audio_range()` summarizes AUDIOHED offset ranges without reading AUDIOT payloads, giving future audio schedulers quick counts/byte totals for PC speaker, AdLib, digital, and music ranges.
- `wl_describe_sound_priority_decision()` captures the single-channel sound arbitration rule for future SDL3/audio playback: start when no sound is active, or when the candidate priority is at least the active sound priority.
- `wl_describe_sound_channel_decision()` applies that priority rule to non-empty PC speaker/AdLib chunk descriptors and reports the next active chunk/priority without mutating playback state.
- `wl_start_sound_channel()` applies that arbitration to a tiny deterministic active-sound state: rejected lower-priority candidates preserve the active sound/sample position, while accepted candidates replace the sound index, priority, and reset playback to sample 0.
- `wl_start_sound_channel_from_chunk()` starts that deterministic channel directly from validated non-empty PC speaker/AdLib chunk metadata, rejecting empty digital/music chunks and chunk ids outside the stored state range.
- `wl_schedule_sound_channel()` applies the same mutable state transition directly from a validated PC speaker/AdLib audio chunk descriptor, carrying candidate kind/priority metadata for future mixer dispatch.
- `wl_schedule_sound_channel_from_chunk()` schedules by size_t chunk id with the same validation, rejecting chunk ids outside the stored 16-bit sound-channel range before truncation.
- `wl_advance_sound_channel()` advances the deterministic active-sound state by a sample delta, preserving inactive channels, moving active playback positions, and deactivating completed sounds at the declared sample count.
- `wl_advance_sound_channel_from_chunk()` applies the same state-only advancement from validated non-empty PC speaker/AdLib chunk metadata, deriving the declared sample count without duplicating mixer cursor logic.
- `wl_tick_sound_channel()` advances an active PC speaker or AdLib channel through the existing bounded sample cursor helpers, preserves inactive channels, reports the current sample, and clears `active` when playback reaches the declared sample end.
- `wl_tick_sound_channel_from_chunk()` applies that tick helper directly from validated non-empty PC speaker/AdLib chunk metadata, rejecting empty, music/digital, and impossible payload descriptors before dispatch.
- `wl_schedule_tick_sound_channel_from_chunk()` combines the metadata-backed scheduler with the metadata-backed tick helper: accepted candidates start/replacement-reset then advance by one sample budget, while held lower-priority candidates preserve the active channel without ticking unrelated chunk data.
- `wl_schedule_advance_sound_channel_from_chunk()` combines metadata-backed scheduling with state-only sample advancement when callers need channel position/completion but not the decoded current sample byte.
- `wl_schedule_describe_sound_channel_window_from_chunk()` combines metadata-backed scheduling with non-mutating sample-window inspection: accepted candidates expose the first bounded playback window from reset sample 0, while held lower-priority candidates preserve the active channel without inspecting unrelated candidate bytes.
- `wl_describe_sound_sample_count_from_chunk()` dispatches validated non-empty PC speaker/AdLib chunk metadata to the matching sample-count descriptor so future scheduling code can bound playback state without format-specific branches.
- `wl_describe_sound_playback_position_from_chunk()` dispatches metadata-backed PC speaker/AdLib sample-position inspection so future audio mixers can query the current sample/completion state without format-specific branches.
- `wl_describe_sound_channel_position_from_chunk()` applies that metadata-backed sample-position inspection to an active channel state's current playback cursor.
- `wl_describe_sound_channel_remaining_from_chunk()` validates an active channel cursor against metadata-backed PC speaker/AdLib sample counts and reports the remaining samples for mixer buffer sizing.
- `wl_describe_sound_channel_window_from_chunk()` applies the metadata-backed sample-window dispatcher to an active channel cursor so future mixer code can inspect the next bounded emission window without mutating channel state.
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
- `wl_describe_sound_playback_window_from_chunk()` routes validated non-empty PC speaker/AdLib chunk metadata into the matching bounded sample-window helper, giving future mixer steps one metadata-backed dispatch seam while rejecting music/digital/empty/impossible payload descriptors.
- `wl_describe_adlib_playback_position()` maps an absolute AdLib sample position to the current sample or completed state, matching the PC speaker position descriptor contract.
- `wl_describe_imf_music_chunk()` validates IMF music chunks by their declared byte count and summarizes command count, first/last commands, maximum and zero-delay counts, total delay ticks, and trailing AUDIOT bytes.
- `wl_get_imf_music_command()` decodes a bounded IMF command `(register, value, delay)` by index for future AdLib playback scheduling without exposing callers to raw byte offsets.
- `wl_describe_imf_playback_window()` summarizes deterministic IMF command emission from a start index within a tick budget for future SDL3 audio scheduling.
- `wl_describe_imf_playback_position()` maps an absolute IMF tick position to the active command and delay remainder, giving future SDL3 audio code a deterministic seek/cursor seam.
- `wl_describe_imf_playback_position_from_chunk()` applies that same seek descriptor from validated music chunk metadata, rejecting sound/digital/empty/impossible descriptors before dispatch.
- `wl_advance_imf_playback_cursor()` advances a command index plus intra-command elapsed delay by a tick delta, reporting the next command cursor, consumed ticks, completed commands, and completion state for frame-to-frame music playback.
- `wl_advance_imf_playback_cursor_from_chunk()` applies the cursor advancement from validated music chunk metadata for future music scheduler code that should not duplicate raw chunk checks.

## Verified WL6 metadata

- `AUDIOHED.WL6`: 1,156 bytes = 289 offsets = 288 chunks plus sentinel.
- First offsets: `0, 15, 28, 44, 102`.
- `AUDIOT.WL6`: 320,209 bytes; sentinel offset is within the file and equals the observed file end.
- Range summaries pin the WL6 PC speaker range as 87/87 non-empty chunks totaling 9,986 bytes (largest chunk 50 at 313 bytes), the AdLib range as 87/87 non-empty chunks totaling 12,969 bytes (largest chunk 137 at 339 bytes), the digitized-sound range as 1/87 non-empty chunks totaling 4 bytes (chunk 260 sentinel/marker), and the music range as 27/27 non-empty chunks totaling 297,250 bytes (largest chunk 280 at 22,578 bytes).
- Priority arbitration is pinned headlessly for inactive sounds, lower-priority rejection, equal-priority replacement, higher-priority replacement, and invalid active-state/null-output rejection; channel decisions are pinned for inactive PC-speaker start, lower-priority hold, and equal-priority AdLib replacement; channel-start state coverage also pins inactive start, lower-priority state preservation, equal-priority replacement, sample-position reset, metadata-backed PC/AdLib starts, digital/oversized chunk rejection, and invalid input handling; channel-schedule coverage pins inactive PC-speaker start, lower-priority hold, equal-priority AdLib replacement, metadata-backed chunk scheduling, oversized chunk rejection, candidate kind/priority metadata, and invalid input handling; channel-advance coverage pins inactive preservation, partial active advancement, completion/deactivation at the declared sample count, zero-delta completion at end, metadata-backed PC speaker/AdLib advancement, and invalid active/out-of-range/null/truncated input rejection; channel-tick coverage pins PC speaker and AdLib sample advancement, completion deactivation, inactive no-op behavior, metadata-backed PC-speaker ticking, metadata-backed schedule+tick, schedule+advance, and schedule+window accept/hold behavior, metadata-backed PC speaker/AdLib sample-count, sample-position, active-channel-position/remaining/window inspection, metadata-backed AdLib instrument-register dispatch, and invalid kind/metadata/input handling; metadata-backed playback-window coverage pins PC speaker and AdLib dispatch plus invalid metadata rejection.
- Representative chunks:
  - chunk 0: 15 bytes, FNV-1a `0x5971ec53`; PC speaker sample count 8, first/last samples `0x83`/`0x84`, bounded sample accessor first/last bytes `0x83`/`0x84`, playback cursor covers zero-delta, last-sample, clamped completion, completed no-op, and invalid completed-delta states; playback windows cover mid-sound samples 2..4 and completion from sample 7; terminator `0x00`, trailing bytes 0.
  - chunk 1: 13 bytes, FNV-1a `0x21985d89`; PC speaker sample count 6, first/last samples `0x2f`/`0x2f`, bounded sample accessor last byte `0x2f`, terminator `0x00`.
  - chunk 87: 41 bytes, FNV-1a `0x799f60b1`; AdLib sample count 8, priority 1, instrument first/last bytes `0x70`/`0x03`, bounded instrument accessor first/last bytes `0x70`/`0x03`, register descriptor maps first modulator/carrier values `0x70`/`0x75`, waveform values `0x00`/`0x00`, feedback `0x01`, voice `0x32`, and mode `0xd5`, sample first/last bytes `0x04`/`0x2e`, bounded sample accessor first/last bytes `0x04`/`0x2e`, playback cursor covers zero-delta, last-sample, clamped completion, completed no-op, and invalid completed-delta states; playback windows cover mid-sound samples 3..6 and completed zero-length-at-end behavior, trailing bytes 11.
  - chunk 174: 0 bytes.
  - chunk 261: 7,546 bytes, FNV-1a `0xea0d69d8`; IMF declared bytes 7,456, command count 1,864, command accessor first `(reg=0, value=0, delay=189)`, accessor last `(reg=0, value=0, delay=1)`, playback windows emit 1 command for 189 ticks and 3 commands for 20,485 ticks, playback positions map tick 188 to command 0 with 1 tick remaining, tick 189 to command 1, and total delay to completed command index 1,864; metadata-backed playback position and cursor-advance wrappers cover valid music descriptors plus non-music and impossible payload rejection; playback cursor advancement covers intra-command progress, command-boundary carry, multi-command advancement, final completion, and invalid elapsed states; max delay 64,098, zero-delay commands 0, total delay 25,697,407, trailing bytes 86.
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
