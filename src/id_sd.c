// id_sd.c - Sound Manager (SDL3 audio implementation)
// Plays digitized sound effects and Adlib/OPL2 music via SDL3 audio.
// SDL3 handles format conversion and mixing automatically.
// Music uses Nuked OPL3 emulator for FM synthesis.

#include "id_sd.h"
#include "id_ca.h"

#include <stdlib.h>
#include <string.h>
#include <SDL3/SDL.h>

#include "opl3.h"

extern void Quit(char *error);

// -----------------------------------------------------------------------
// Public globals
// -----------------------------------------------------------------------

boolean  AdLibPresent       = false;
boolean  SoundSourcePresent = false;
boolean  SoundBlasterPresent = false;
boolean  NeedsMusic         = false;
boolean  SoundPositioned    = false;
SDMode   SoundMode          = sdm_Off;
SMMode   MusicMode          = smm_Off;
SDSMode  DigiMode           = sds_Off;
boolean  DigiPlaying        = false;
word     DigiMap[256];
longword TimeCount          = 0;

// -----------------------------------------------------------------------
// Internal state
// -----------------------------------------------------------------------

static void (*userHook)(void) = NULL;

static SDL_AudioDeviceID audio_device = 0;
static SDL_AudioSpec device_spec;

// OPL3 music state
static opl3_chip opl3;
static SDL_AudioStream *music_stream = NULL;
static word *music_hack = NULL;      // start of music data (for looping)
static word *music_ptr = NULL;       // current position in word pairs
static int   music_len = 0;          // remaining bytes
static int   music_seqlen = 0;       // total sequence length (for looping)
static long  music_time = 0;         // accumulated tick counter (alTimeCount)
static long  music_next_tick = 0;    // tick value for next event
static int   music_active = 0;
#define MUSIC_RATE 49716    // OPL3 native rate
#define MUSIC_BUFFER_SAMPLES 4096

// OPL2 register addresses
#define OPL_FREQH      0xB0    // frequency high / key-on, channels 0-8
#define OPL_EFFECTS    0xBD    // percussion/effects register

#define MAX_CHANNELS 4
static struct {
    SDL_AudioStream *stream;
    int active;
} channels[MAX_CHANNELS];

// Number of the sound most recently started.  The original SD_SoundPlaying()
// returns this enum value (not a boolean) while a sound is active, so callers
// like UpdateFace() can test SD_SoundPlaying() == GETGATLINGSND.
static int SoundNumber = 0;

// ========================================================================
// Startup / Shutdown
// ========================================================================

void SD_Startup(void)
{
    TimeCount = 0;
    memset(DigiMap, 0, sizeof(DigiMap));
    DigiPlaying = false;
    userHook = NULL;
    memset(channels, 0, sizeof(channels));

    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        return;
    }

    SDL_AudioSpec desired;
    SDL_zero(desired);
    desired.format = SDL_AUDIO_S16;
    desired.channels = 1;
    desired.freq = 48000;

    audio_device = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &desired);
    if (audio_device == 0) {
        return;
    }

    // Query actual device format
    SDL_AudioSpec obtained;
    int sample_frames = 0;
    if (SDL_GetAudioDeviceFormat(audio_device, &obtained, &sample_frames)) {
        device_spec = obtained;
    } else {
        device_spec = desired;
    }

    SDL_ResumeAudioDevice(audio_device);

    // Init OPL3 emulator for music
    OPL3_Reset(&opl3, MUSIC_RATE);
    AdLibPresent = true;

    // Enable sound modes
    SoundBlasterPresent = true;
    DigiMode = sds_SoundBlaster;
    SoundMode = sdm_AdLib;
}

void SD_Shutdown(void)
{
    SD_MusicOff();
    for (int ch = 0; ch < MAX_CHANNELS; ch++) {
        if (channels[ch].stream) {
            SDL_UnbindAudioStream(channels[ch].stream);
            SDL_DestroyAudioStream(channels[ch].stream);
            channels[ch].stream = NULL;
        }
    }
    if (audio_device) {
        SDL_CloseAudioDevice(audio_device);
        audio_device = 0;
    }
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

// ========================================================================
// Mode selection
// ========================================================================

void SD_Default(boolean gotit, SDMode sd, SMMode sm)
{
    if (!gotit) {
        SoundMode = sd;
        MusicMode = sm;
    }
}

void SD_SetSoundMode(SDMode mode)
{
    SD_StopSound();
    SoundMode = mode;
    if (mode == sdm_AdLib)
        AdLibPresent = true;
}

void SD_SetMusicMode(SMMode mode)
{
    SD_MusicOff();
    MusicMode = mode;
}

void SD_SetDigiDevice(SDSMode mode)
{
    DigiMode = mode;
}

// ========================================================================
// Internal: play raw 8-bit unsigned PCM data through SDL
// ========================================================================

static int SD_PlayRaw(byte *data, int datalen, int hertz)
{
    if (!audio_device || !data || datalen <= 0 || hertz <= 0) return -1;

    // Find a free channel
    int ch = -1;
    for (int i = 0; i < MAX_CHANNELS; i++) {
        if (!channels[i].active) { ch = i; break; }
    }
    if (ch == -1) ch = 0; // steal channel 0

    // Clean up old stream if present
    if (channels[ch].stream) {
        SDL_UnbindAudioStream(channels[ch].stream);
        SDL_DestroyAudioStream(channels[ch].stream);
    }

    // Create stream: source is 16-bit signed mono at the sound's rate, dest is device format
    SDL_AudioSpec src_spec;
    src_spec.format = SDL_AUDIO_S16;
    src_spec.channels = 1;
    src_spec.freq = hertz;

    channels[ch].stream = SDL_CreateAudioStream(&src_spec, &device_spec);
    if (!channels[ch].stream) return -1;

    if (!SDL_BindAudioStream(audio_device, channels[ch].stream)) {
        SDL_DestroyAudioStream(channels[ch].stream);
        channels[ch].stream = NULL;
        return -1;
    }

    // Convert 8-bit unsigned to 16-bit signed and queue
    int16_t *converted = (int16_t *)malloc((size_t)datalen * sizeof(int16_t));
    if (!converted) return -1;
    for (int i = 0; i < datalen; i++) {
        converted[i] = (int16_t)(((int)data[i] - 128) << 8);
    }

    SDL_PutAudioStreamData(channels[ch].stream, converted, datalen * (int)sizeof(int16_t));
    SDL_FlushAudioStream(channels[ch].stream); // no more data coming
    free(converted);

    channels[ch].active = 1;
    return ch;
}

// ========================================================================
// Sound playback
// ========================================================================

int SD_PlaySound(soundnames sound)
{
    if (sound < 0 || sound >= LASTSOUND) return 0;
    if (!audio_device) return 0;

    // Try digitized sound first
    int digi_index = STARTDIGISOUNDS + sound;
    if (digi_index < NUMSNDCHUNKS) {
        if (!audiosegs[digi_index])
            CA_CacheAudioChunk(digi_index);
        if (audiosegs[digi_index]) {
            SampledSound *sfx = (SampledSound *)audiosegs[digi_index];
            int header_size = (int)((intptr_t)sfx->data - (intptr_t)sfx);
            int data_len = (int)sfx->common.length - header_size;
            if (data_len > 0 && sfx->hertz > 0) {
                int ch = SD_PlayRaw(sfx->data, data_len, sfx->hertz);
                if (ch >= 0) {
                    DigiPlaying = true;
                    SoundNumber = sound;
                    return sound + 1;
                }
            }
        }
    }

    return 0;
}

void SD_PositionSound(int x, int y)
{
    (void)x;
    (void)y;
}

void SD_SetPosition(int x, int y)
{
    (void)x;
    (void)y;
}

void SD_StopSound(void)
{
    for (int ch = 0; ch < MAX_CHANNELS; ch++) {
        if (channels[ch].stream) {
            SDL_UnbindAudioStream(channels[ch].stream);
            SDL_DestroyAudioStream(channels[ch].stream);
            channels[ch].stream = NULL;
        }
        channels[ch].active = 0;
    }
    DigiPlaying = false;
}

void SD_WaitSoundDone(void)
{
    while (SD_SoundPlaying()) {
        SDL_Delay(10);
    }
}

int SD_SoundPlaying(void)
{
    int playing = 0;
    for (int ch = 0; ch < MAX_CHANNELS; ch++) {
        if (channels[ch].active && channels[ch].stream) {
            if (SDL_GetAudioStreamAvailable(channels[ch].stream) > 0) {
                playing = 1;
                continue;
            }
            // Stream finished
            SDL_UnbindAudioStream(channels[ch].stream);
            SDL_DestroyAudioStream(channels[ch].stream);
            channels[ch].stream = NULL;
            channels[ch].active = 0;
        }
    }
    // Match the original: return the active sound's number, not a boolean.
    return playing ? SoundNumber : 0;
}

// ========================================================================
// Digitized sound (high-level)
// ========================================================================

void SD_PlayDigitized(word which, int leftpos, int rightpos)
{
    (void)leftpos;
    (void)rightpos;

    int digi_index = STARTDIGISOUNDS + which;
    if (digi_index >= NUMSNDCHUNKS || !audiosegs[digi_index]) return;

    SampledSound *sfx = (SampledSound *)audiosegs[digi_index];
    int header_size = (int)((intptr_t)sfx->data - (intptr_t)sfx);
    int data_len = (int)sfx->common.length - header_size;
    if (data_len > 0 && sfx->hertz > 0) {
        SD_PlayRaw(sfx->data, data_len, sfx->hertz);
        DigiPlaying = true;
    }
}

void SD_StopDigitized(void)
{
    DigiPlaying = false;
    SD_StopSound();
}

// ========================================================================
// Music - OPL3 via Nuked OPL3 emulator + SDL AudioStream
// ========================================================================

// Feed OPL3 samples into the music audio stream
static void SD_FillMusicStream(void)
{
    if (!music_stream || !music_active) return;

    int avail = SDL_GetAudioStreamAvailable(music_stream);
    if (avail > MUSIC_BUFFER_SAMPLES * 4) return;

    // OPL3_GenerateResampled outputs 2 samples (stereo L+R)
    int16_t buf[MUSIC_BUFFER_SAMPLES * 2];
    for (int i = 0; i < MUSIC_BUFFER_SAMPLES * 2; i += 2) {
        OPL3_GenerateResampled(&opl3, &buf[i]);
    }
    SDL_PutAudioStreamData(music_stream, buf, MUSIC_BUFFER_SAMPLES * 2 * sizeof(int16_t));
}

void SD_StartMusic(MusicGroup *music)
{
    SD_MusicOff();

    if (!audio_device || MusicMode != smm_AdLib || !music) return;

    music_hack = music->values;
    music_ptr = music->values;
    music_seqlen = music_len = music->length;
    music_time = 0;
    music_next_tick = 0;
    music_active = 1;

    // Create/resume music stream: OPL3 outputs at MUSIC_RATE, stereo 16-bit
    if (music_stream) {
        SDL_UnbindAudioStream(music_stream);
        SDL_DestroyAudioStream(music_stream);
    }

    SDL_AudioSpec src_spec;
    src_spec.format = SDL_AUDIO_S16;
    src_spec.channels = 2;
    src_spec.freq = MUSIC_RATE;

    music_stream = SDL_CreateAudioStream(&src_spec, &device_spec);
    if (music_stream) {
        SDL_BindAudioStream(audio_device, music_stream);
    }

    NeedsMusic = true;
}

void SD_StartMusicByName(musicnames music)
{
    if (MusicMode == smm_Off) return;

    int idx = STARTMUSIC + music;
    if (idx >= NUMSNDCHUNKS) return;

    if (!audiosegs[idx])
        CA_CacheAudioChunk(idx);
    if (audiosegs[idx])
        SD_StartMusic((MusicGroup *)audiosegs[idx]);
}

void SD_MusicOn(void)
{
    music_active = 1;
    NeedsMusic = true;
}

void SD_MusicOff(void)
{
    if (!music_active) return;

    music_active = 0;
    NeedsMusic = false;

    // Silence all OPL channels
    OPL3_WriteReg(&opl3, OPL_EFFECTS, 0);
    for (int i = 0; i < 9; i++)
        OPL3_WriteReg(&opl3, (uint16_t)(OPL_FREQH + i), 0);

    if (music_stream) {
        SDL_UnbindAudioStream(music_stream);
        SDL_DestroyAudioStream(music_stream);
        music_stream = NULL;
    }
}

void SD_FadeOutMusic(void)
{
    SD_MusicOff();
}

int SD_MusicPlaying(void)
{
    return music_active;
}

// ========================================================================
// User hook / poll
// ========================================================================

void SD_SetUserHook(void (*hook)(void))
{
    userHook = hook;
}

// TickBase = 70Hz. TimeCount should advance 70 ticks per real second.
#define TICKS_PER_SEC 70
#define TICK_MS (1000 / TICKS_PER_SEC)  // ~14ms per tick

void SD_Poll(void)
{
    // TimeCount is advanced by CalcTics during gameplay (70Hz).
    // Do NOT increment TimeCount here — that was double-counting.

    // Rate-limit music events to ~70Hz based on real time
    static Uint32 last_music_ms = 0;
    static long music_accumulator = 0;
    Uint32 now = SDL_GetTicks();
    if (last_music_ms == 0) last_music_ms = now;

    Uint32 elapsed = now - last_music_ms;
    unsigned music_ticks = 0;
    if (elapsed > 0) {
        music_accumulator += (long)elapsed * TICKS_PER_SEC;
        music_ticks = (unsigned)(music_accumulator / 1000);
        music_accumulator %= 1000;
        last_music_ms = now;
    }

    // Process OPL3 music events (sqHack pattern from original ID_SD.C)
    if (music_active && music_len > 0) {
        while (music_len && (music_next_tick <= music_time)) {
            word w = *music_ptr++;
            music_next_tick = music_time + *music_ptr++;
            byte reg = w & 0xFF;
            byte val = (w >> 8) & 0xFF;
            OPL3_WriteReg(&opl3, reg, val);
            music_len -= 4;
        }
        music_time += music_ticks;
        if (!music_len) {
            music_ptr = music_hack;
            music_len = music_seqlen;
            music_time = 0;
            music_next_tick = 0;
        }
    }

    // Feed OPL3 samples to audio stream
    SD_FillMusicStream();

    if (userHook) {
        userHook();
    }
}
