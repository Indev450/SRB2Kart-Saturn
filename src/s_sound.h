// SONIC ROBO BLAST 2
//-----------------------------------------------------------------------------
// Copyright (C) 1993-1996 by id Software, Inc.
// Copyright (C) 1998-2000 by DooM Legacy Team.
// Copyright (C) 1999-2018 by Sonic Team Junior.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  s_sound.h
/// \brief The not so system specific sound interface

#ifndef __S_SOUND__
#define __S_SOUND__

#include "i_sound.h" // musictype_t
#include "sounds.h"
#include "m_fixed.h"
#include "command.h"
#include "tables.h" // angle_t

#ifdef HAVE_OPENMPT
#include "libopenmpt/libopenmpt.h"
extern openmpt_module *openmpt_mhandle;
#endif

// mask used to indicate sound origin is player item pickup
#define PICKUP_SOUND 0x8000

extern consvar_t stereoreverse;
extern consvar_t cv_soundvolume, cv_digmusicvolume;//, cv_midimusicvolume;
extern consvar_t cv_numChannels;

extern consvar_t cv_audbuffersize;
//extern consvar_t cv_resetmusic;
extern consvar_t cv_gamedigimusic;
#ifndef NO_MIDI
extern consvar_t cv_gamemidimusic, cv_midimusicvolume;
#endif
extern consvar_t cv_gamesounds;
extern consvar_t cv_playmusicifunfocused;
extern consvar_t cv_playsoundifunfocused;
extern consvar_t cv_pausemusic;

#ifdef HAVE_OPENMPT
extern consvar_t cv_modfilter;
extern consvar_t cv_stereosep;
extern consvar_t cv_amigafilter;
#if OPENMPT_API_VERSION_MAJOR < 1 && OPENMPT_API_VERSION_MINOR > 4
extern consvar_t cv_amigatype;
#endif
#endif

//bird music stuff
extern consvar_t cv_music_resync_threshold;

extern consvar_t cv_invincmusicfade;
extern consvar_t cv_growmusicfade;

extern consvar_t cv_respawnfademusicout;
extern consvar_t cv_respawnfademusicback;

extern consvar_t cv_resetspecialmusic;

extern consvar_t cv_resume;
extern consvar_t cv_fading;
extern consvar_t cv_birdmusic;

extern consvar_t cv_keepmusic;
extern consvar_t cv_skipintromusic;
extern consvar_t cv_ignoremusicchanges;
extern boolean keepmusic;
extern boolean skipintromus;
#define MUSICSTARTTIME (starttime + (TICRATE/2))

extern consvar_t precachesound;

typedef enum
{
	SF_TOTALLYSINGLE =  1, // Only play one of these sounds at a time...GLOBALLY
	SF_NOMULTIPLESOUND =  2, // Like SF_NOINTERRUPT, but doesnt care what the origin is
	SF_OUTSIDESOUND  =  4, // Volume is adjusted depending on how far away you are from 'outside'
	SF_X4AWAYSOUND   =  8, // Hear it from 4x the distance away
	SF_X8AWAYSOUND   = 16, // Hear it from 8x the distance away
	SF_NOINTERRUPT   = 32, // Only play this sound if it isn't already playing on the origin
	SF_X2AWAYSOUND   = 64, // Hear it from 2x the distance away
} soundflags_t;

typedef struct {
	fixed_t x, y, z;
	angle_t angle;
} listener_t;

// register sound vars and commands at game startup
void S_RegisterSoundStuff(void);

//
// Initializes sound stuff, including volume
// Sets channels, SFX, allocates channel buffer, sets S_sfx lookup.
//
void S_InitSfxChannels(INT32 sfxVolume);

//
// Per level startup code.
// Kills playing sounds at start of level, determines music if any, changes music.
//
void S_StopSounds(void);
void S_ClearSfx(void);

void S_InitMapMusic(void);
void S_StartMapMusic(void);

void S_CheckMap(void);

// Stops music and restarts it from same position. Used for instant applying changes to amiga filters.
void S_RestartMusic(void);

//
// Basically a W_GetNumForName that adds "ds" at the beginning of the string. Returns a lumpnum.
//
lumpnum_t S_GetSfxLumpNum(sfxinfo_t *sfx);

//
// Sound Status
//

boolean S_SoundDisabled(void);

//
// Start sound for thing at <origin> using <sound_id> from sounds.h
//
void S_StartSound(const void *origin, sfxenum_t sound_id);

// Will start a sound at a given volume.
void S_StartSoundAtVolume(const void *origin, sfxenum_t sound_id, INT32 volume);

// Stop sound for thing at <origin>
void S_StopSound(void *origin);

//
// Music Status
//

boolean S_DigMusicDisabled(void);
boolean S_MIDIMusicDisabled(void);
boolean S_MusicDisabled(void);
boolean S_MusicPlaying(void);
boolean S_MusicPaused(void);
boolean S_MusicNotInFocus(void);
boolean S_PrecacheSound(void);
musictype_t S_MusicType(void);
const char *S_MusicName(void);
boolean S_MusicInfo(char *mname, UINT16 *mflags, boolean *looping);
boolean S_MusicExists(const char *mname, boolean checkMIDI, boolean checkDigi);
#define S_DigExists(a) S_MusicExists(a, false, true)
#define S_MIDIExists(a) S_MusicExists(a, true, false)

//
// Music Effects
//

// Set Speed of Music
boolean S_SpeedMusic(float speed);

// Music credits
typedef struct musicdef_s
{
	char name[7];
	UINT32 hash;
	char usage[256];
	char source[256];
	char filename[256+1];
	// for the music test stuff
	// generally if these are present on vanilla the game would throw up a warning
	// a sacrifice i suppose
	char title[256];
	char alttitle[256];
	char authors[256];
	boolean use_info;
	struct musicdef_s *next;
} musicdef_t;

extern musicdef_t *musicdefstart;
extern musicdef_t **soundtestdefs;
extern INT32 numsoundtestdefs;
extern UINT8 soundtestpage;

extern struct cursongcredit
{
	musicdef_t *def;
	UINT16 anim;
	fixed_t x;
	UINT8 trans;
} cursongcredit;


void S_LoadMusicDefs(UINT16 wadnum);
void S_InitMusicDefs(void);
void S_LoadMTDefs(UINT16 wadnum);
void S_InitMTDefs(void);
musicdef_t *S_FindMusicCredit(const char *musname);
void S_ShowSpecifiedMusicCredit(const char *musname);
void S_ShowMusicCredit(void);

boolean S_PrepareSoundTest(void);

//
// Music Seeking
//

// Get Length of Music
UINT32 S_GetMusicLength(void);

// Set LoopPoint of Music
boolean S_SetMusicLoopPoint(UINT32 looppoint);

// Get LoopPoint of Music
UINT32 S_GetMusicLoopPoint(void);

// Set Position of Music
boolean S_SetMusicPosition(UINT32 position);

// Get Position of Music
UINT32 S_GetMusicPosition(void);

//
// Music Playback
//

// Start music track, arbitrary, given its name, and set whether looping
// note: music flags 12 bits for tracknum (gme, other formats with more than one track)
//       13-15 aren't used yet
//       and the last bit we ignore (internal game flag for resetting music on reload)
void S_ChangeMusicEx(const char *mmusic, UINT16 mflags, boolean looping, UINT32 position, UINT32 prefadems, UINT32 fadeinms);
#define S_ChangeMusicInternal(a,b) S_ChangeMusicEx(a,0,b,0,0,0)
#define S_ChangeMusic(a,b,c) S_ChangeMusicEx(a,b,c,0,0,0)

void S_ChangeMusicSpecial (const char *mmusic);

void S_SetRestoreMusicFadeInCvar (consvar_t *cvar);
#define S_ClearRestoreMusicFadeInCvar() \
	S_SetRestoreMusicFadeInCvar(0)
int  S_GetRestoreMusicFadeIn (void);

// Stops the music.
void S_StopMusic(void);

// Stop and resume music, during game PAUSE.
void S_PauseAudio(void);
void S_ResumeAudio(void);

//
// Music Fading
//

void S_SetInternalMusicVolume(INT32 volume);
void S_StopFadingMusic(void);
boolean S_FadeMusicFromVolume(UINT8 target_volume, INT16 source_volume, UINT32 ms);
#define S_FadeMusic(a, b) S_FadeMusicFromVolume(a, -1, b)
#define S_FadeInChangeMusic(a,b,c,d) S_ChangeMusicEx(a,b,c,0,0,d)
boolean S_FadeOutStopMusic(UINT32 ms);

//
// Updates music & sounds
//
void S_UpdateSounds(void);

FUNCMATH fixed_t S_CalculateSoundDistance(fixed_t px1, fixed_t py1, fixed_t pz1, fixed_t px2, fixed_t py2, fixed_t pz2);

void S_SetSfxVolume(INT32 volume);
void S_SetMusicVolume(INT32 digvolume, INT32 seqvolume);
#define S_SetDigMusicVolume(a) S_SetMusicVolume(a,-1)
#define S_SetMIDIMusicVolume(a) S_SetMusicVolume(-1,a)
#define S_InitMusicVolume() S_SetMusicVolume(-1,-1)

INT32 S_OriginPlaying(void *origin);
INT32 S_IdPlaying(sfxenum_t id);
INT32 S_SoundPlaying(void *origin, sfxenum_t id);

void S_StartSoundName(void *mo, const  char *soundname);

void S_StopSoundByID(void *origin, sfxenum_t sfx_id);
void S_StopSoundByNum(sfxenum_t sfxnum);

#ifndef HW3SOUND
#define S_StartAttackSound S_StartSound
#define S_StartScreamSound S_StartSound
#endif

#ifdef MUSICSLOT_COMPATIBILITY
// For compatibility with code/scripts relying on older versions
// This is a list of all the "special" slot names and their associated numbers
extern const char *compat_special_music_slots[16];
#endif

#endif
