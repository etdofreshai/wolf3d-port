// id_sd.h - Sound Manager header (SDL3 port)
#ifndef ID_SD_H
#define ID_SD_H

#include "wl_types.h"
#include "audiowl6.h"

#define TickBase 70

typedef enum { sdm_Off, sdm_PC, sdm_AdLib } SDMode;
typedef enum { smm_Off, smm_AdLib } SMMode;
typedef enum { sds_Off, sds_PC, sds_SoundSource, sds_SoundBlaster } SDSMode;

typedef struct {
    longword length;
    word priority;
} SoundCommon;

typedef struct {
    SoundCommon common;
    byte data[1];
} PCSound;

typedef struct {
    SoundCommon common;
    word hertz;
    byte bits, reference, data[1];
} SampledSound;

typedef struct {
    byte mChar, cChar, mScale, cScale, mAttack, cAttack;
    byte mSus, cSus, mWave, cWave, nConn, voice, mode;
    byte unused[3];
} Instrument;

typedef struct {
    SoundCommon common;
    Instrument inst;
    byte block, data[1];
} AdLibSound;

typedef struct {
    word length;
    word values[1];
} MusicGroup;

extern boolean AdLibPresent, SoundSourcePresent, SoundBlasterPresent;
extern boolean NeedsMusic, SoundPositioned;
extern SDMode SoundMode;
extern SMMode MusicMode;
extern SDSMode DigiMode;
extern boolean DigiPlaying;
extern word DigiMap[256];
extern longword TimeCount;

void SD_Startup(void);
void SD_Shutdown(void);
void SD_Default(boolean gotit, SDMode sd, SMMode sm);
void SD_SetSoundMode(SDMode mode);
void SD_SetMusicMode(SMMode mode);
void SD_SetDigiDevice(SDSMode mode);
int  SD_PlaySound(soundnames sound);
void SD_PositionSound(int x, int y);
void SD_SetPosition(int x, int y);
void SD_StopSound(void);
void SD_WaitSoundDone(void);
int  SD_SoundPlaying(void);
void SD_PlayDigitized(word which, int leftpos, int rightpos);
void SD_StopDigitized(void);
void SD_StartMusic(MusicGroup *music);
void SD_StartMusicByName(musicnames music);
void SD_MusicOn(void);
void SD_MusicOff(void);
void SD_FadeOutMusic(void);
int  SD_MusicPlaying(void);
void SD_SetUserHook(void (*hook)(void));
void SD_Poll(void);

#endif
