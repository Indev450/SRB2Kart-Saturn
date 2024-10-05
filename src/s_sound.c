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
/// \file  s_sound.c
/// \brief System-independent sound and music routines

#include "d_netcmd.h"
#include "doomdef.h"
#include "doomstat.h"
#include "command.h"
#include "g_game.h"
#include "m_argv.h"
#include "r_main.h" // R_PointToAngle2() used to calc stereo sep.
#include "r_things.h" // for skins
#include "i_system.h"
#include "i_sound.h"
#include "s_sound.h"
#include "w_wad.h"
#include "z_zone.h"
#include "d_main.h"
#include "r_sky.h" // skyflatnum
#include "p_local.h" // camera info
#include "m_misc.h" // for tunes command
#include "m_cond.h" // for conditionsets
#include "m_menu.h" // bird music stuff

#include "lua_hook.h" // MusicChange hook

#ifdef HW3SOUND
// 3D Sound Interface
#include "hardware/hw3sound.h"
#else
static boolean S_AdjustSoundParams(const mobj_t *listener, const mobj_t *source, INT32 *vol, INT32 *sep, INT32 *pitch, sfxinfo_t *sfxinfo);
#endif

static void SetChannelsNum(void);
static void Command_Tunes_f(void);
static void Command_RestartAudio_f(void);
static void Command_RestartMusic_f(void); //mhhhm amiga type filters here i come uwu
static void Command_ShowMusicCredit_f(void);

// Sound system toggles
#ifndef NO_MIDI
static void GameMIDIMusic_OnChange(void);
#endif
static void GameSounds_OnChange(void);
static void SoundPrecache_OnChange(void);
static void GameDigiMusic_OnChange(void);
static void BufferSize_OnChange(void);

#ifdef HAVE_OPENMPT
static void ModFilter_OnChange(void);
static void StereoSep_OnChange(void);
static void AmigaFilter_OnChange(void);
#if OPENMPT_API_VERSION_MAJOR < 1 && OPENMPT_API_VERSION_MINOR > 4
static void AmigaType_OnChange(void);
#endif
#endif

consvar_t cv_samplerate = {"samplerate", "44100", 0, CV_Unsigned, NULL, 22050, NULL, NULL, 0, 0, NULL}; //Alam: For easy hacking?

static CV_PossibleValue_t audbuffersize_cons_t[] = {{256, "256"}, {512, "512"}, {1024, "1024"}, {2048, "2048"}, {4096, "4096"}, {0, NULL}};
consvar_t cv_audbuffersize = {"buffersize", "2048", CV_SAVE, audbuffersize_cons_t, BufferSize_OnChange, 0, NULL, NULL, 0, 0, NULL};

// stereo reverse
consvar_t stereoreverse = {"stereoreverse", "Off", CV_SAVE, CV_OnOff, NULL, 0, NULL, NULL, 0, 0, NULL};

// if true, all sounds are loaded at game startup
consvar_t precachesound = {"precachesound", "Off", CV_SAVE|CV_CALL|CV_NOINIT, CV_OnOff, SoundPrecache_OnChange, 0, NULL, NULL, 0, 0, NULL};

// actual general (maximum) sound & music volume, saved into the config
static CV_PossibleValue_t soundvolume_cons_t[] = {{0, "MIN"}, {31, "MAX"}, {0, NULL}};
consvar_t cv_soundvolume = {"soundvolume", "18", CV_SAVE, soundvolume_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_digmusicvolume = {"digmusicvolume", "18", CV_SAVE, soundvolume_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL};
#ifndef NO_MIDI
consvar_t cv_midimusicvolume = {"midimusicvolume", "18", CV_SAVE, soundvolume_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL};
#endif

// number of channels available
consvar_t cv_numChannels = {"snd_channels", "64", CV_SAVE|CV_CALL, CV_Unsigned, SetChannelsNum, 0, NULL, NULL, 0, 0, NULL};

//consvar_t cv_resetmusic = {"resetmusic", "No", CV_SAVE|CV_NOSHOWHELP, CV_YesNo, NULL, 0, NULL, NULL, 0, 0, NULL};

// Sound system toggles, saved into the config
consvar_t cv_gamedigimusic = {"digimusic", "On", CV_SAVE|CV_CALL|CV_NOINIT, CV_OnOff, GameDigiMusic_OnChange, 0, NULL, NULL, 0, 0, NULL};
#ifndef NO_MIDI
consvar_t cv_gamemidimusic = {"midimusic", "On", CV_SAVE|CV_CALL|CV_NOINIT, CV_OnOff, GameMIDIMusic_OnChange, 0, NULL, NULL, 0, 0, NULL};
#endif
consvar_t cv_gamesounds = {"sounds", "On", CV_SAVE|CV_CALL|CV_NOINIT, CV_OnOff, GameSounds_OnChange, 0, NULL, NULL, 0, 0, NULL};

consvar_t cv_playmusicifunfocused = {"playmusicifunfocused",  "No", CV_SAVE, CV_YesNo, NULL, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_playsoundifunfocused = {"playsoundsifunfocused", "No", CV_SAVE, CV_YesNo, NULL, 0, NULL, NULL, 0, 0, NULL};

consvar_t cv_pausemusic = {"playmusicifpaused",  "No", CV_SAVE, CV_YesNo, NULL, 0, NULL, NULL, 0, 0, NULL};

//bird music stuff
static CV_PossibleValue_t music_resync_threshold_cons_t[] = {
	{0,    "MIN"},
	{1000, "MAX"},

	{0}
};
consvar_t cv_music_resync_threshold = {"music_resync_threshold", "0", CV_SAVE|CV_CALL, music_resync_threshold_cons_t, I_UpdateSongLagThreshold, 0, NULL, NULL, 0, 0, NULL};

consvar_t cv_invincmusicfade = {"invincmusicfade", "300", CV_SAVE, CV_Unsigned, NULL, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_growmusicfade = {"growmusicfade", "500", CV_SAVE, CV_Unsigned, NULL, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_respawnfademusicout = {"respawnfademusicout", "1000", CV_SAVE, CV_Unsigned, NULL, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_respawnfademusicback = {"respawnfademusicback", "500", CV_SAVE, CV_Unsigned, NULL, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_resetspecialmusic = {"resetspecialmusic", "No", CV_SAVE, CV_YesNo, NULL, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_resume = {"resume", "No", CV_SAVE, CV_YesNo, NULL, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_fading = {"fading", "Off", CV_SAVE|CV_CALL, CV_OnOff, Bird_menu_Onchange, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_birdmusic = {"birdmusicstuff", "No", CV_SAVE|CV_CALL, CV_YesNo, Bird_menu_Onchange, 0, NULL, NULL, 0, 0, NULL};


consvar_t cv_keepmusic = {"keepmusic", "No", CV_SAVE, CV_YesNo, NULL, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_skipintromusic = {"skipintromusic", "No", CV_SAVE, CV_YesNo, NULL, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_ignoremusicchanges = {"ignoremusicchanges", "No", CV_SAVE, CV_YesNo, NULL, 0, NULL, NULL, 0, 0, NULL};

boolean keepmusic = false;
static void S_CheckEventMus(const char *newmus);

#ifdef HAVE_OPENMPT
openmpt_module *openmpt_mhandle = NULL;

static CV_PossibleValue_t interpolationfilter_cons_t[] = {{0, "Default"}, {1, "None"}, {2, "Linear"}, {4, "Cubic"}, {8, "Windowed sinc"}, {0, NULL}};
consvar_t cv_modfilter = {"modfilter", "4", CV_SAVE|CV_CALL, interpolationfilter_cons_t, ModFilter_OnChange, 0, NULL, NULL, 0, 0, NULL};

static CV_PossibleValue_t stereosep_cons_t[] = {{0, "MIN"}, {200, "MAX"}, {0, NULL}};
consvar_t cv_stereosep = {"stereoseperation", "100", CV_SAVE|CV_CALL, stereosep_cons_t, StereoSep_OnChange, 0, NULL, NULL, 0, 0, NULL}; //some tracker modules have nauseously high stereo width

static CV_PossibleValue_t amigafilter_cons_t[] = {{0, "Off"}, {1, "On"}, {0, NULL}};
consvar_t cv_amigafilter = {"amigafilter", "1", CV_SAVE|CV_CALL, amigafilter_cons_t, AmigaFilter_OnChange, 0, NULL, NULL, 0, 0, NULL};

#if OPENMPT_API_VERSION_MAJOR < 1 && OPENMPT_API_VERSION_MINOR > 4
static CV_PossibleValue_t amigatype_cons_t[] = {{0, "auto"}, {1, "a500"}, {2, "a1200"}, {0, NULL}};
consvar_t cv_amigatype = {"amigatype", "0", CV_SAVE|CV_CALL|CV_NOINIT, amigatype_cons_t, AmigaType_OnChange, 0, NULL, NULL, 0, 0, NULL};
#endif
#endif

#define S_MAX_VOLUME 127

// when to clip out sounds
// Does not fit the large outdoor areas.
// added 2-2-98 in 8 bit volume control (before (1200*0x10000))
#define S_CLIPPING_DIST (1536*0x10000)

// Distance to origin when sounds should be maxed out.
// This should relate to movement clipping resolution
// (see BLOCKMAP handling).
// Originally: (200*0x10000).
// added 2-2-98 in 8 bit volume control (before (160*0x10000))
#define S_CLOSE_DIST (160*0x10000)

// added 2-2-98 in 8 bit volume control (before remove the +4)
#define S_ATTENUATOR ((S_CLIPPING_DIST-S_CLOSE_DIST)>>(FRACBITS+4))

// Adjustable by menu.
#define NORM_VOLUME snd_MaxVolume

#define NORM_PITCH 128
#define NORM_PRIORITY 64
#define NORM_SEP 128

#define S_PITCH_PERTURB 1
#define S_STEREO_SWING (96*0x10000)

// percent attenuation from front to back
#define S_IFRACVOL 30

typedef struct
{
	// sound information (if null, channel avail.)
	sfxinfo_t *sfxinfo;

	// origin of sound
	const void *origin;

	// initial volume of sound, which is applied after distance and direction
	INT32 volume;

	// handle of the sound being played
	INT32 handle;

} channel_t;

// the set of channels available
static channel_t *channels = NULL;
static INT32 numofchannels = 0;

//
// Internals.
//
static void S_StopChannel(INT32 cnum);

//
// S_getChannel
//
// If none available, return -1. Otherwise channel #.
//
static INT32 S_getChannel(const void *origin, sfxinfo_t *sfxinfo)
{
	// channel number to use
	INT32 cnum;

	// Find an open channel
	for (cnum = 0; cnum < numofchannels; cnum++)
	{
		if (!channels[cnum].sfxinfo)
			break;

		// Now checks if same sound is being played, rather
		// than just one sound per mobj
		else if (sfxinfo == channels[cnum].sfxinfo && (sfxinfo->pitch & SF_NOMULTIPLESOUND))
		{
			return -1;
		}
		else if (sfxinfo == channels[cnum].sfxinfo && sfxinfo->singularity == true)
		{
			S_StopChannel(cnum);
			break;
		}
		else if (origin && channels[cnum].origin == origin && channels[cnum].sfxinfo == sfxinfo)
		{
			if (sfxinfo->pitch & SF_NOINTERRUPT)
				return -1;
			else
				S_StopChannel(cnum);
			break;
		}
		else if (origin && channels[cnum].origin == origin
			&& channels[cnum].sfxinfo->name != sfxinfo->name
			&& (channels[cnum].sfxinfo->pitch & SF_TOTALLYSINGLE) && (sfxinfo->pitch & SF_TOTALLYSINGLE))
		{
			S_StopChannel(cnum);
			break;
		}
	}

	// None available
	if (cnum == numofchannels)
	{
		// Look for lower priority
		for (cnum = 0; cnum < numofchannels; cnum++)
			if (channels[cnum].sfxinfo->priority <= sfxinfo->priority)
				break;

		if (cnum == numofchannels)
		{
			// No lower priority. Sorry, Charlie.
			return -1;
		}
		else
		{
			// Otherwise, kick out lower priority.
			S_StopChannel(cnum);
		}
	}

	return cnum;
}

void S_RegisterSoundStuff(void)
{
	if (dedicated)
	{
		sound_disabled = true;
		return;
	}

	CV_RegisterVar(&stereoreverse);
	CV_RegisterVar(&precachesound);
	CV_RegisterVar(&cv_samplerate);
	//CV_RegisterVar(&cv_resetmusic);
	CV_RegisterVar(&cv_gamesounds);
	CV_RegisterVar(&cv_gamedigimusic);
#ifndef NO_MIDI
	CV_RegisterVar(&cv_gamemidimusic);
#endif

	//bird music stuff
	CV_RegisterVar(&cv_playmusicifunfocused);
	CV_RegisterVar(&cv_playsoundifunfocused);
	CV_RegisterVar(&cv_pausemusic);

	CV_RegisterVar(&cv_music_resync_threshold);
	
	CV_RegisterVar(&cv_invincmusicfade);
	CV_RegisterVar(&cv_growmusicfade);

	CV_RegisterVar(&cv_respawnfademusicout);
	CV_RegisterVar(&cv_respawnfademusicback);

	CV_RegisterVar(&cv_resetspecialmusic);

	CV_RegisterVar(&cv_resume);
	CV_RegisterVar(&cv_fading);
	CV_RegisterVar(&cv_birdmusic);

	CV_RegisterVar(&cv_keepmusic);
	CV_RegisterVar(&cv_skipintromusic);
	CV_RegisterVar(&cv_ignoremusicchanges);

	COM_AddCommand("tunes", Command_Tunes_f);
	COM_AddCommand("restartaudio", Command_RestartAudio_f);
	COM_AddCommand("restartmusic", Command_RestartMusic_f);
	COM_AddCommand("showmusiccredit", Command_ShowMusicCredit_f);
}

static void SetChannelsNum(void)
{
	// Allocating the internal channels for mixing
	// (the maximum number of sounds rendered
	// simultaneously) within zone memory.
	if (channels)
		S_StopSounds();

	Z_Free(channels);
	channels = NULL;

	if (cv_numChannels.value == 999999999) //Alam_GBC: OH MY ROD!(ROD rimmiced with GOD!)
		CV_StealthSet(&cv_numChannels,cv_numChannels.defaultvalue);

#ifdef HW3SOUND
	if (hws_mode != HWS_DEFAULT_MODE)
	{
		HW3S_SetSourcesNum();
		return;
	}
#endif

	if (cv_numChannels.value)
		channels = (channel_t *)Z_Calloc(cv_numChannels.value * sizeof (channel_t), PU_STATIC, NULL);
	numofchannels = (channels ? cv_numChannels.value : 0);
}

// Retrieve the lump number of sfx
//
lumpnum_t S_GetSfxLumpNum(sfxinfo_t *sfx)
{
	char namebuf[9];
	lumpnum_t sfxlump;

	sprintf(namebuf, "ds%s", sfx->name);

	sfxlump = W_CheckNumForName(namebuf);
	if (sfxlump != LUMPERROR)
		return sfxlump;

	strlcpy(namebuf, sfx->name, sizeof namebuf);

	sfxlump = W_CheckNumForName(namebuf);
	if (sfxlump != LUMPERROR)
		return sfxlump;

	return W_GetNumForName("dsthok");
}

//
// Sound Status
//

boolean S_SoundDisabled(void)
{
	return (
			sound_disabled ||
			( window_notinfocus && ! cv_playsoundifunfocused.value )
	);
}

boolean S_PrecacheSound(void)
{
	return (!sound_disabled && (M_CheckParm("-precachesound") || precachesound.value));
}

// Stop all sounds, load level info, THEN start sounds.
void S_StopSounds(void)
{
	INT32 cnum;

#ifdef HW3SOUND
	if (hws_mode != HWS_DEFAULT_MODE)
	{
		HW3S_StopSounds();
		return;
	}
#endif

	// kill all playing sounds at start of level
	for (cnum = 0; cnum < numofchannels; cnum++)
		if (channels[cnum].sfxinfo)
			S_StopChannel(cnum);
}

void S_StopSoundByID(void *origin, sfxenum_t sfx_id)
{
	INT32 cnum;

	// Sounds without origin can have multiple sources, they shouldn't
	// be stopped by new sounds.
	if (!origin)
		return;
#ifdef HW3SOUND
	if (hws_mode != HWS_DEFAULT_MODE)
	{
		HW3S_StopSoundByID(origin, sfx_id);
		return;
	}
#endif
	for (cnum = 0; cnum < numofchannels; cnum++)
	{
		if (channels[cnum].sfxinfo == &S_sfx[sfx_id] && channels[cnum].origin == origin)
		{
			S_StopChannel(cnum);
		}
	}
}

void S_StopSoundByNum(sfxenum_t sfxnum)
{
	INT32 cnum;

#ifdef HW3SOUND
	if (hws_mode != HWS_DEFAULT_MODE)
	{
		HW3S_StopSoundByNum(sfxnum);
		return;
	}
#endif
	for (cnum = 0; cnum < numofchannels; cnum++)
	{
		if (channels[cnum].sfxinfo == &S_sfx[sfxnum])
		{
			S_StopChannel(cnum);
		}
	}
}

static INT32 S_ScaleVolumeWithSplitscreen(INT32 volume)
{
	fixed_t root = INT32_MAX;

	if (splitscreen == 0)
	{
		return volume;
	}

	root = FixedSqrt((splitscreen + 1) * (FRACUNIT/3));

	return FixedDiv(
		volume * FRACUNIT,
		root
	) / FRACUNIT;
}

void S_StartSoundAtVolume(const void *origin_p, sfxenum_t sfx_id, INT32 volume)
{
	const mobj_t *origin = (const mobj_t *)origin_p;
	const boolean reverse = (stereoreverse.value ^ encoremode);
	const INT32 initial_volume = (origin ? S_ScaleVolumeWithSplitscreen(volume) : volume);

	sfxinfo_t *sfx;
	INT32 sep, pitch, priority, cnum;
	boolean anyListeners = false;
	boolean itsUs = false;
	INT32 i;

	listener_t listener[MAXSPLITSCREENPLAYERS];
	mobj_t *listenmobj[MAXSPLITSCREENPLAYERS];

	if (S_SoundDisabled() || !sound_started)
		return;

	// Don't want a sound? Okay then...
	if (sfx_id == sfx_None)
		return;

	for (i = 0; i <= splitscreen; i++)
	{
		player_t *player = &players[displayplayers[i]];

		memset(&listener[i], 0, sizeof (listener[i]));
		listenmobj[i] = NULL;

		if (i == 0 && democam.soundmobj)
		{
			listenmobj[i] = democam.soundmobj;
		}
		else if (player->awayviewtics)
		{
			listenmobj[i] = player->awayviewmobj;
		}
		else
		{
			listenmobj[i] = player->mo;
		}

		if (origin && origin == listenmobj[i])
		{
			itsUs = true;
		}
	}

#ifdef HW3SOUND
	if (hws_mode != HWS_DEFAULT_MODE)
	{
		HW3S_StartSound(origin, sfx_id);
		return;
	};
#endif

	for (i = 0; i <= splitscreen; i++)
	{
		player_t *player = &players[displayplayers[i]];

		if (camera[i].chase && !player->awayviewtics)
		{
			listener[i].x = camera[i].x;
			listener[i].y = camera[i].y;
			listener[i].z = camera[i].z;
			listener[i].angle = camera[i].angle;
			anyListeners = true;
		}
		else if (listenmobj[i])
		{
			listener[i].x = listenmobj[i]->x;
			listener[i].y = listenmobj[i]->y;
			listener[i].z = listenmobj[i]->z;
			listener[i].angle = listenmobj[i]->angle;
			anyListeners = true;
		}
	}

	if (origin && anyListeners == false)
	{
		// If a mobj is trying to make a noise, and no one is around to hear it, does it make a sound?
		return;
	}

	// check for bogus sound #
	I_Assert(sfx_id >= 1);
	I_Assert(sfx_id < NUMSFX);

	sfx = &S_sfx[sfx_id];

	if (sfx->skinsound != -1 && origin && origin->skin)
	{
		// redirect player sound to the sound in the skin table
		sfx_id = ((skin_t *)( (origin->localskin) ? origin->localskin : origin->skin ))->soundsid[sfx->skinsound];
		sfx = &S_sfx[sfx_id];
	}

	// Initialize sound parameters
	pitch = NORM_PITCH;
	priority = NORM_PRIORITY;
	sep = NORM_SEP;

	i = 0; // sensible default

	{
		// Check to see if it is audible, and if not, modify the params
		if (origin && !itsUs)
		{
			boolean audible = false;
			if (splitscreen > 0)
			{
				fixed_t recdist = INT32_MAX;
				UINT8 j = 0;

				for (; j <= splitscreen; j++)
				{
					fixed_t thisdist = INT32_MAX;

					if (!listenmobj[j])
					{
						continue;
					}

					thisdist = P_AproxDistance(listener[j].x - origin->x, listener[j].y - origin->y);

					if (thisdist >= recdist)
					{
						continue;
					}

					recdist = thisdist;
					i = j;
				}
			}

			if (listenmobj[i])
			{
				audible = S_AdjustSoundParams(listenmobj[i], origin, &volume, &sep, &pitch, sfx);
			}

			if (!audible)
			{
				return;
			}
		}

		// This is supposed to handle the loading/caching.
		// For some odd reason, the caching is done nearly
		// each time the sound is needed?

		// cache data if necessary
		// NOTE: set sfx->data NULL sfx->lump -1 to force a reload
		if (!sfx->data)
		{
			sfx->data = I_GetSfx(sfx);
		}

		// increase the usefulness
		if (sfx->usefulness++ < 0)
		{
			sfx->usefulness = -1;
		}

		// Avoid channel reverse if surround
		if (reverse)
		{
			sep = (~sep) & 255;
		}

		// At this point it is determined that a sound can and should be played, so find a free channel to play it on
		cnum = S_getChannel(origin, sfx);

		if (cnum < 0)
		{
			return; // If there's no free channels, there won't be any for anymore players either
		}

		// Now that we know we are going to play a sound, fill out this info
		channels[cnum].sfxinfo = sfx;
		channels[cnum].origin = origin;
		channels[cnum].volume = initial_volume;
		channels[cnum].handle = I_StartSound(sfx_id, volume, sep, pitch, priority, cnum);
	}
}

void S_StartSound(const void *origin, sfxenum_t sfx_id)
{
	if (S_SoundDisabled())
		return;

	// the volume is handled 8 bits
#ifdef HW3SOUND
	if (hws_mode != HWS_DEFAULT_MODE)
		HW3S_StartSound(origin, sfx_id);
	else
#endif
		S_StartSoundAtVolume(origin, sfx_id, 255);
}

void S_StopSound(void *origin)
{
	INT32 cnum;

	// Sounds without origin can have multiple sources, they shouldn't
	// be stopped by new sounds.
	if (!origin)
		return;

#ifdef HW3SOUND
	if (hws_mode != HWS_DEFAULT_MODE)
	{
		HW3S_StopSound(origin);
		return;
	}
#endif
	for (cnum = 0; cnum < numofchannels; cnum++)
	{
		if (channels[cnum].sfxinfo && channels[cnum].origin == origin)
		{
			S_StopChannel(cnum);
		}
	}
}

//
// Updates music & sounds
//
static INT32 actualsfxvolume; // check for change through console
static INT32 actualdigmusicvolume;
#ifndef NO_MIDI
static INT32 actualmidimusicvolume;
#endif

void S_UpdateSounds(void)
{
	INT32 cnum, volume, sep, pitch;
	boolean audible = false;
	channel_t *c;
	INT32 i;

	listener_t listener[MAXSPLITSCREENPLAYERS];
	mobj_t *listenmobj[MAXSPLITSCREENPLAYERS];

	// Update sound/music volumes, if changed manually at console
	if (actualsfxvolume != cv_soundvolume.value)
		S_SetSfxVolume (cv_soundvolume.value);
	if (actualdigmusicvolume != cv_digmusicvolume.value)
		S_SetDigMusicVolume (cv_digmusicvolume.value);
#ifndef NO_MIDI
	if (actualmidimusicvolume != cv_midimusicvolume.value)
		S_SetMIDIMusicVolume (cv_midimusicvolume.value);
#endif

	// We're done now, if we're not in a level.
	if (gamestate != GS_LEVEL)
	{
#ifndef NOMUMBLE
		// Stop Mumble cutting out. I'm sick of it.
		I_UpdateMumble(NULL, listener[0]);
#endif

		// Stop cutting FMOD out. WE'RE sick of it.
		I_UpdateSound();
		return;
	}

	if (dedicated || sound_disabled)
		return;

	for (i = 0; i <= splitscreen; i++)
	{
		player_t *player = &players[displayplayers[i]];

		memset(&listener[i], 0, sizeof (listener[i]));
		listenmobj[i] = NULL;

		if (i == 0 && democam.soundmobj)
		{
			listenmobj[i] = democam.soundmobj;
			continue;
		}

		if (player->awayviewtics)
		{
			listenmobj[i] = player->awayviewmobj;
		}
		else
		{
			listenmobj[i] = player->mo;
		}
	}

#ifndef NOMUMBLE
	I_UpdateMumble(players[consoleplayer].mo, listener[0]);
#endif

#ifdef HW3SOUND
	if (hws_mode != HWS_DEFAULT_MODE)
	{
		HW3S_UpdateSources();
		I_UpdateSound();
		return;
	}
#endif

	for (i = 0; i <= splitscreen; i++)
	{
		player_t *player = &players[displayplayers[i]];

		if (camera[i].chase && !player->awayviewtics)
		{
			listener[i].x = camera[i].x;
			listener[i].y = camera[i].y;
			listener[i].z = camera[i].z;
			listener[i].angle = camera[i].angle;
		}
		else if (listenmobj[i])
		{
			listener[i].x = listenmobj[i]->x;
			listener[i].y = listenmobj[i]->y;
			listener[i].z = listenmobj[i]->z;
			listener[i].angle = listenmobj[i]->angle;
		}
	}

	for (cnum = 0; cnum < numofchannels; cnum++)
	{
		c = &channels[cnum];

		if (c->sfxinfo)
		{
			if (I_SoundIsPlaying(c->handle))
			{
				// initialize parameters
				volume = c->volume; // 8 bits internal volume precision
				pitch = NORM_PITCH;
				sep = NORM_SEP;

				// check non-local sounds for distance clipping
				//  or modify their params
				if (c->origin)
				{
					boolean itsUs = false;

					for (i = splitscreen; i >= 0; i--)
					{
						if (c->origin != listenmobj[i])
							continue;

						itsUs = true;
					}

					if (itsUs == false)
					{
						const mobj_t *origin = c->origin;

						i = 0;

						if (splitscreen > 0)
						{
							fixed_t recdist = INT32_MAX;
							UINT8 j = 0;

							for (; j <= splitscreen; j++)
							{
								fixed_t thisdist = INT32_MAX;

								if (!listenmobj[j])
								{
									continue;
								}

								thisdist = P_AproxDistance(listener[j].x - origin->x, listener[j].y - origin->y);

								if (thisdist >= recdist)
								{
									continue;
								}

								recdist = thisdist;
								i = j;
							}
						}

						if (listenmobj[i])
						{
							audible = S_AdjustSoundParams(
								listenmobj[i], c->origin,
								&volume, &sep, &pitch,
								c->sfxinfo
							);
						}

						if (audible)
							I_UpdateSoundParams(c->handle, volume, sep, pitch);
						else
							S_StopChannel(cnum);
					}
				}
			}
			else
			{
				// if channel is allocated but sound has stopped, free it
				S_StopChannel(cnum);
			}
		}
	}

	I_UpdateSound();
}

void S_SetSfxVolume(INT32 volume)
{
	if (volume < 0 || volume > 31)
		CONS_Alert(CONS_WARNING, "sfxvolume should be between 0-31\n");

	CV_SetValue(&cv_soundvolume, volume&0x1F);
	actualsfxvolume = cv_soundvolume.value; // check for change of var

#ifdef HW3SOUND
	hws_mode == HWS_DEFAULT_MODE ? I_SetSfxVolume(volume&0x1F) : HW3S_SetSfxVolume(volume&0x1F);
#else
	// now hardware volume
	I_SetSfxVolume(volume&0x1F);
#endif
}

void S_ClearSfx(void)
{
	size_t i;
	for (i = 1; i < NUMSFX; i++)
		I_FreeSfx(S_sfx + i);
}

static void S_StopChannel(INT32 cnum)
{
	channel_t *c = &channels[cnum];

	if (c->sfxinfo)
	{
		// stop the sound playing
		if (I_SoundIsPlaying(c->handle))
			I_StopSound(c->handle);

		// degrade usefulness of sound data
		c->sfxinfo->usefulness--;
		c->sfxinfo = 0;
	}

	c->origin = NULL;
}

//
// S_CalculateSoundDistance
//
// Calculates the distance between two points for a sound.
// Clips the distance to prevent overflow.
//
fixed_t S_CalculateSoundDistance(fixed_t sx1, fixed_t sy1, fixed_t sz1, fixed_t sx2, fixed_t sy2, fixed_t sz2)
{
	fixed_t approx_dist, adx, ady;

	// calculate the distance to sound origin and clip it if necessary
	adx = abs((sx1>>FRACBITS) - (sx2>>FRACBITS));
	ady = abs((sy1>>FRACBITS) - (sy2>>FRACBITS));

	// From _GG1_ p.428. Approx. euclidian distance fast.
	// Take Z into account
	adx = adx + ady - ((adx < ady ? adx : ady)>>1);
	ady = abs((sz1>>FRACBITS) - (sz2>>FRACBITS));
	approx_dist = adx + ady - ((adx < ady ? adx : ady)>>1);

	if (approx_dist >= FRACUNIT/2)
		approx_dist = FRACUNIT/2-1;

	approx_dist <<= FRACBITS;

	return FixedDiv(approx_dist, mapobjectscale); // approx_dist
}

//
// Changes volume, stereo-separation, and pitch variables
// from the norm of a sound effect to be played.
// If the sound is not audible, returns a 0.
// Otherwise, modifies parameters and returns 1.
//
boolean S_AdjustSoundParams(const mobj_t *listener, const mobj_t *source, INT32 *vol, INT32 *sep, INT32 *pitch,
	sfxinfo_t *sfxinfo)
{
	const boolean reverse = (stereoreverse.value ^ encoremode);

	fixed_t approx_dist;

	listener_t listensource;
	INT32 i;

	(void)pitch;

	if (!listener)
		return false;

	// Init listensource with default listener
	listensource.x = listener->x;
	listensource.y = listener->y;
	listensource.z = listener->z;
	listensource.angle = listener->angle;

	for (i = 0; i <= splitscreen; i++)
	{
		// If listener is a chasecam player, use the camera instead
		if (listener == players[displayplayers[i]].mo && camera[i].chase)
		{
			listensource.x = camera[i].x;
			listensource.y = camera[i].y;
			listensource.z = camera[i].z;
			listensource.angle = camera[i].angle;
			break;
		}
	}

	if (sfxinfo->pitch & SF_OUTSIDESOUND) // Rain special case
	{
		INT64 x, y, yl, yh, xl, xh;
		fixed_t newdist;

		if (R_PointInSubsector(listensource.x, listensource.y)->sector->ceilingpic == skyflatnum)
			approx_dist = 0;
		else
		{
			// Essentially check in a 1024 unit radius of the player for an outdoor area.
			yl = listensource.y - 1024*FRACUNIT;
			yh = listensource.y + 1024*FRACUNIT;
			xl = listensource.x - 1024*FRACUNIT;
			xh = listensource.x + 1024*FRACUNIT;
			approx_dist = 1024*FRACUNIT;
			for (y = yl; y <= yh; y += FRACUNIT*64)
				for (x = xl; x <= xh; x += FRACUNIT*64)
				{
					if (R_PointInSubsector(x, y)->sector->ceilingpic == skyflatnum)
					{
						// Found the outdoors!
						newdist = S_CalculateSoundDistance(listensource.x, listensource.y, 0, x, y, 0);
						if (newdist < approx_dist)
						{
							approx_dist = newdist;
						}
					}
				}
		}
	}
	else
	{
		approx_dist = S_CalculateSoundDistance(listensource.x, listensource.y, listensource.z,
												source->x, source->y, source->z);
	}

	// Ring loss, deaths, etc, should all be heard louder.
	if (sfxinfo->pitch & SF_X8AWAYSOUND)
		approx_dist = FixedDiv(approx_dist,8*FRACUNIT);

	// Combine 8XAWAYSOUND with 4XAWAYSOUND and get.... 32XAWAYSOUND?
	if (sfxinfo->pitch & SF_X4AWAYSOUND)
		approx_dist = FixedDiv(approx_dist,4*FRACUNIT);

	if (sfxinfo->pitch & SF_X2AWAYSOUND)
		approx_dist = FixedDiv(approx_dist,2*FRACUNIT);

	if (approx_dist > S_CLIPPING_DIST)
		return false;

	if (source->x == listensource.x && source->y == listensource.y)
	{
		*sep = NORM_SEP;
	}
	else
	{
		// angle of source to listener
		angle_t angle = R_PointToAngle2(listensource.x, listensource.y, source->x, source->y);

		if (angle > listensource.angle)
			angle = angle - listensource.angle;
		else
			angle = angle + InvAngle(listensource.angle);

		if (reverse)
			angle = InvAngle(angle);

		angle >>= ANGLETOFINESHIFT;

		// stereo separation
		*sep = 128 - (FixedMul(S_STEREO_SWING, FINESINE(angle))>>FRACBITS);
	}

	// volume calculation
	/* not sure if it should be > (no =), but this matches the old behavior */
	if (approx_dist >= S_CLOSE_DIST)
	{
		// distance effect
		INT32 n = (15 * ((S_CLIPPING_DIST - approx_dist)>>FRACBITS));
		*vol = FixedMul(*vol * FRACUNIT / 255, n) / S_ATTENUATOR;
	}

	return (*vol > 0);
}

// Searches through the channels and checks if a sound is playing
// on the given origin.
INT32 S_OriginPlaying(void *origin)
{
	INT32 cnum;
	if (!origin)
		return false;

#ifdef HW3SOUND
	if (hws_mode != HWS_DEFAULT_MODE)
		return HW3S_OriginPlaying(origin);
#endif

	for (cnum = 0; cnum < numofchannels; cnum++)
		if (channels[cnum].origin == origin)
			return 1;
	return 0;
}

// Searches through the channels and checks if a given id
// is playing anywhere.
INT32 S_IdPlaying(sfxenum_t id)
{
	INT32 cnum;

#ifdef HW3SOUND
	if (hws_mode != HWS_DEFAULT_MODE)
		return HW3S_IdPlaying(id);
#endif

	for (cnum = 0; cnum < numofchannels; cnum++)
		if ((size_t)(channels[cnum].sfxinfo - S_sfx) == (size_t)id)
			return 1;
	return 0;
}

// Searches through the channels and checks for
// origin x playing sound id y.
INT32 S_SoundPlaying(void *origin, sfxenum_t id)
{
	INT32 cnum;
	if (!origin)
		return 0;

#ifdef HW3SOUND
	if (hws_mode != HWS_DEFAULT_MODE)
		return HW3S_SoundPlaying(origin, id);
#endif

	for (cnum = 0; cnum < numofchannels; cnum++)
	{
		if (channels[cnum].origin == origin
		 && (size_t)(channels[cnum].sfxinfo - S_sfx) == (size_t)id)
			return 1;
	}
	return 0;
}

//
// S_StartSoundName
// Starts a sound using the given name.
#define MAXNEWSOUNDS 10
static sfxenum_t newsounds[MAXNEWSOUNDS] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

void S_StartSoundName(void *mo, const char *soundname)
{
	INT32 i, soundnum = 0;
	// Search existing sounds...
	for (i = sfx_None + 1; i < NUMSFX; i++)
	{
		if (!S_sfx[i].name)
			continue;
		if (!stricmp(S_sfx[i].name, soundname))
		{
			soundnum = i;
			break;
		}
	}

	if (!soundnum)
	{
		for (i = 0; i < MAXNEWSOUNDS; i++)
		{
			if (newsounds[i] == 0)
				break;
			if (!S_IdPlaying(newsounds[i]))
			{
				S_RemoveSoundFx(newsounds[i]);
				break;
			}
		}

		if (i == MAXNEWSOUNDS)
		{
			CONS_Debug(DBG_GAMELOGIC, "Cannot load another extra sound!\n");
			return;
		}

		soundnum = S_AddSoundFx(soundname, false, 0, false);
		newsounds[i] = soundnum;
	}

	S_StartSound(mo, soundnum);
}

//
// Initializes sound stuff, including volume
// Sets channels, SFX volume,
//  allocates channel buffer, sets S_sfx lookup.
//
void S_InitSfxChannels(INT32 sfxVolume)
{
	INT32 i;

	if (dedicated)
		return;

	S_SetSfxVolume(sfxVolume);

	SetChannelsNum();

	// Note that sounds have not been cached (yet).
	for (i = 1; i < NUMSFX; i++)
	{
		S_sfx[i].usefulness = -1; // for I_GetSfx()
		S_sfx[i].lumpnum = LUMPERROR;
	}

	// Precache sounds if requested
	if (S_PrecacheSound())
	{
		// Initialize external data (all sounds) at start, keep static.
		CONS_Printf(M_GetText("Loading sounds... "));

			for (i = 1; i < sfx_freeslot0; i++)
				if (S_sfx[i].name && !S_sfx[i].data)
					S_sfx[i].data = I_GetSfx(&S_sfx[i]);

			for (i = sfx_freeslot0; i < NUMSFX; i++)
				if (S_sfx[i].priority && !S_sfx[i].data)
					S_sfx[i].data = I_GetSfx(&S_sfx[i]);

		CONS_Printf(M_GetText(" pre-cached all sound data\n"));
	}
}

/// ------------------------
/// Music
/// ------------------------

#ifdef MUSICSLOT_COMPATIBILITY
const char *compat_special_music_slots[16] =
{
	"titles", // 1036  title screen
	"read_m", // 1037  intro
	"lclear", // 1038  level clear
	"invinc", // 1039  invincibility
	"shoes",  // 1040  super sneakers
	"minvnc", // 1041  Mario invincibility
	"drown",  // 1042  drowning
	"gmover", // 1043  game over
	"xtlife", // 1044  extra life
	"contsc", // 1045  continue screen
	"supers", // 1046  Super Sonic
	"chrsel", // 1047  character select
	"credit", // 1048  credits
	"racent", // 1049  Race Results
	"stjr",   // 1050  Sonic Team Jr. Presents
	""
};
#endif

static char      music_name[7]; // up to 6-character name
static void      *music_data;
static UINT16    music_flags;
static boolean   music_looping;
static consvar_t *music_refade_cv;

static char      queue_name[7];
static UINT16    queue_flags;
static boolean   queue_looping;
static UINT32    queue_position;
static UINT32    queue_fadeinms;

/// ------------------------
/// Music Definitions
/// ------------------------

musicdef_t *musicdefstart = NULL; // First music definition
struct cursongcredit cursongcredit; // Currently displayed song credit info

static boolean
ReadMusicDefFields (UINT16 wadnum, int line, char *stoken, musicdef_t **defp)
{
	musicdef_t *def;

	char *value;
	char *textline;

	if (!stricmp(stoken, "lump"))
	{
		value = strtok(NULL, " ");
		if (!value)
		{
			CONS_Alert(CONS_WARNING,
					"MUSICDEF: Field '%s' is missing name. (file %s, line %d)\n",
					stoken, wadfiles[wadnum]->filename, line);
			return false;
		}
		else
		{
			def = S_FindMusicCredit(value);

			// Nothing found, add to the end.
			if (!def)
			{
				def = Z_Calloc(sizeof (musicdef_t), PU_STATIC, NULL);

				STRBUFCPY(def->name, value);
				strlwr(def->name);
				def->hash = quickncasehash (def->name, 6);

				def->next = musicdefstart;
				musicdefstart = def;
			}

			(*defp) = def;
		}
	}
	else
	{
		value = strtok(NULL, "");

		if (value)
		{
			// Find the equals sign.
			value = strchr(value, '=');
		}

		if (!value)
		{
			CONS_Alert(CONS_WARNING,
					"MUSICDEF: Field '%s' is missing value. (file %s, line %d)\n",
					stoken, wadfiles[wadnum]->filename, line);
			return false;
		}
		else
		{
			def = (*defp);

			if (!def)
			{
				CONS_Alert(CONS_ERROR,
						"MUSICDEF: No music definition before field '%s'. (file %s, line %d)\n",
						stoken, wadfiles[wadnum]->filename, line);
				return false;
			}

			// Skip the equals sign.
			value++;

			// Now skip funny whitespace.
			value += strspn(value, "\t ");

			textline = value;

// turn _ into spaces.
#define ADDDEF(field)\
	STRBUFCPY(def->field, textline);\
	for (textline = def->field; *textline; textline++)\
		if (*textline == '_') *textline = ' ';

			if (!stricmp(stoken, "usage"))
			{
				ADDDEF(usage);
			}
			else if (!stricmp(stoken, "source"))
			{
				ADDDEF(source);
			}
			else if (!stricmp(stoken, "title"))
			{
				def->use_info = true;
				ADDDEF(title);
			}
			else if (!stricmp(stoken, "alttitle"))
			{
				ADDDEF(alttitle);
			}
			else if (!stricmp(stoken, "authors"))
			{
				ADDDEF(authors);
			}
			else
				CONS_Alert(CONS_WARNING, "MUSICDEF: Invalid field '%s'. (file %s, line %d)\n", stoken, wadfiles[wadnum]->filename, line);
#undef ADDDEF
		}
	}

	return true;
}

void S_LoadMusicDefs(UINT16 wadnum)
{
	UINT16 lumpnum;
	char *lump;
	char *musdeftext;
	size_t size;

	char *lf;
	char *stoken;

	size_t nlf;
	size_t ncr;

	musicdef_t *def = NULL;
	int line = 1; // for better error msgs

	for (int k = 0; k < 2; k++)
	{
		lumpnum = W_CheckNumForNamePwad((k == 1 ? "MUSCINFO" : "MUSICDEF") , wadnum, 0); //check for MUSCINFO lump on 2nd iteration

		if (lumpnum == INT16_MAX)
			continue;

		lump = W_CacheLumpNumPwad(wadnum, lumpnum, PU_CACHE);
		size = W_LumpLengthPwad(wadnum, lumpnum);

		// Null-terminated MUSICDEF lump.
		musdeftext = malloc(size+1);
		if (!musdeftext)
			I_Error("S_LoadMusicDefs: No more free memory for the parser\n");
		M_Memcpy(musdeftext, lump, size);
		musdeftext[size] = '\0';

		// Find music def
		stoken = musdeftext;
		for (;;)
		{
			lf = strpbrk(stoken, "\r\n");
			if (lf)
			{
				if (*lf == '\n')
					nlf = 1;
				else
					nlf = 0;
				*lf++ = '\0';/* now we can delimit to here */
			}

			stoken = strtok(stoken, " ");
			if (stoken)
			{
				if (! ReadMusicDefFields(wadnum, line, stoken, &def))
					break;
			}

			if (lf)
			{
				do
				{
					line += nlf;
					ncr = strspn(lf, "\r");/* skip CR */
					lf += ncr;
					nlf = strspn(lf, "\n");
					lf += nlf;
				}
				while (nlf || ncr) ;

				stoken = lf;/* now the next nonempty line */
			}
			else
				break;/* EOF */
		}

		free(musdeftext);
	}
}

//
// S_InitMusicDefs
//
// Simply load music defs in all wads.
//
void S_InitMusicDefs(void)
{
	UINT16 i;
	for (i = 0; i < numwadfiles; i++)
		S_LoadMusicDefs(i);
}

//
// S_FindMusicCredit
//
// Returns musicdef of specified song, or null if musicdef for it doesn't exist
//
musicdef_t *S_FindMusicCredit(const char *musname)
{
	UINT32 hash = quickncasehash (musname, 6);
	musicdef_t *def;

	for (def = musicdefstart; def; def = def->next)
	{
		if (hash != def->hash)
			continue;
		if (stricmp(def->name, musname))
			continue;

		return def;
	}

	return NULL;
}

//
// S_ShowSpecifiedMusicCredit
//
// Display song's credit on screen
//
void S_ShowSpecifiedMusicCredit(const char *musname)
{
	musicdef_t *def;

	if (digital_disabled) return;

	if (!cv_songcredits.value || demo.rewinding)
		return;

	def = S_FindMusicCredit(musname);

	if (def)
	{
		cursongcredit.def = def;
		cursongcredit.anim = 5*TICRATE;
		cursongcredit.x = 0;
		cursongcredit.trans = NUMTRANSMAPS;
	}
}

//
// S_ShowMusicCredit
//
// Display current song's credit on screen
//
void S_ShowMusicCredit(void)
{
	S_ShowSpecifiedMusicCredit(music_name);
}

musicdef_t **soundtestdefs = NULL;
INT32 numsoundtestdefs = 0;

//
// S_PrepareSoundTest
//
// Prepare sound test. What am I, your butler?
//
boolean S_PrepareSoundTest(void)
{
	musicdef_t *def;
	INT32 pos = numsoundtestdefs = 0;

	for (def = musicdefstart; def; def = def->next)
	{
		numsoundtestdefs++;
	}

	if (!numsoundtestdefs)
		return false;

	if (soundtestdefs)
		Z_Free(soundtestdefs);

	if (!(soundtestdefs = Z_Malloc(numsoundtestdefs*sizeof(musicdef_t *), PU_STATIC, NULL)))
		I_Error("S_PrepareSoundTest(): could not allocate soundtestdefs.");

	for (def = musicdefstart; def /*&& i < numsoundtestdefs*/; def = def->next)
	{
		soundtestdefs[pos++] = def;
	}

	return true;
}

/// ------------------------
/// Music Status
/// ------------------------

boolean S_DigMusicDisabled(void)
{
	return digital_disabled;
}

boolean S_MIDIMusicDisabled(void)
{
	return midi_disabled; // SRB2Kart: defined as "true" w/ NO_MIDI
}

boolean S_MusicDisabled(void)
{
	return (midi_disabled && digital_disabled);
}

boolean S_MusicPlaying(void)
{
	return I_SongPlaying();
}

boolean S_MusicPaused(void)
{
	return I_SongPaused();
}

boolean S_MusicNotInFocus(void)
{
	return (
			( window_notinfocus && ! cv_playmusicifunfocused.value )
	);
}

musictype_t S_MusicType(void)
{
	return I_SongType();
}

const char *S_MusicName(void)
{
	return music_name;
}

boolean S_MusicInfo(char *mname, UINT16 *mflags, boolean *looping)
{
	if (!I_SongPlaying())
		return false;

	strncpy(mname, music_name, 7);
	mname[6] = 0;
	*mflags = music_flags;
	*looping = music_looping;

	return (boolean)mname[0];
}

boolean S_MusicExists(const char *mname, boolean checkMIDI, boolean checkDigi)
{
	return (
		(checkDigi ? W_CheckNumForName(va("O_%s", mname)) != LUMPERROR : false)
		|| (checkMIDI ? W_CheckNumForName(va("D_%s", mname)) != LUMPERROR : false)
	);
}

/// ------------------------
/// Music Effects
/// ------------------------

boolean S_SpeedMusic(float speed)
{
	return I_SetSongSpeed(speed);
}

/// ------------------------
/// Music Seeking
/// ------------------------

UINT32 S_GetMusicLength(void)
{
	return I_GetSongLength();
}

boolean S_SetMusicLoopPoint(UINT32 looppoint)
{
	return I_SetSongLoopPoint(looppoint);
}

UINT32 S_GetMusicLoopPoint(void)
{
	return I_GetSongLoopPoint();
}

boolean S_SetMusicPosition(UINT32 position)
{
	return I_SetSongPosition(position);
}

UINT32 S_GetMusicPosition(void)
{
	return I_GetSongPosition();
}

/// ------------------------
/// Music Playback
/// ------------------------

static boolean S_LoadMusic(const char *mname)
{
	lumpnum_t mlumpnum;
	void *mdata;

	if (S_MusicDisabled())
		return false;

	if (!S_DigMusicDisabled() && S_DigExists(mname))
		mlumpnum = W_GetNumForName(va("o_%s", mname));
	else if (!S_MIDIMusicDisabled() && S_MIDIExists(mname))
		mlumpnum = W_GetNumForName(va("d_%s", mname));
	else if (S_DigMusicDisabled() && S_DigExists(mname))
	{
		CONS_Alert(CONS_NOTICE, "Digital music is disabled!\n");
		return false;
	}
	else if (S_MIDIMusicDisabled() && S_MIDIExists(mname))
	{
#ifdef NO_MIDI
		CONS_Alert(CONS_ERROR, "A MIDI music lump %.6s was found,\nbut SRB2Kart does not support MIDI output.\nWe apologise for the inconvenience.\n", mname);
#else
		CONS_Alert(CONS_NOTICE, "MIDI music is disabled!\n");
#endif
		return false;
	}
	else
	{
		CONS_Alert(CONS_ERROR, M_GetText("Music lump %.6s not found!\n"), mname);
		return false;
	}

	// load & register it
	mdata = W_CacheLumpNum(mlumpnum, PU_MUSIC);

	if (I_LoadSong(mdata, W_LumpLength(mlumpnum)))
	{
		strncpy(music_name, mname, 7);
		music_name[6] = 0;
		music_data = mdata;
		return true;
	}
	else
		return false;
}

static void S_UnloadMusic(void)
{
	I_UnloadSong();

#ifndef HAVE_SDL //SDL uses RWOPS
	Z_ChangeTag(music_data, PU_CACHE);
#endif
	music_data = NULL;

	music_name[0] = 0;
	music_flags = 0;
	music_looping = false;

	music_refade_cv = 0;
}

static boolean S_PlayMusic(boolean looping, UINT32 fadeinms)
{
	if (S_MusicDisabled())
		return false;

	I_UpdateSongLagThreshold();

	if ((!fadeinms && !I_PlaySong(looping)) ||
		(fadeinms && !I_FadeInPlaySong(fadeinms, looping)))
	{
		S_UnloadMusic();
		return false;
	}

	S_InitMusicVolume(); // switch between digi and sequence volume

	if (S_MusicNotInFocus())
		I_SetMusicVolume(0);

	return true;
}

static void S_QueueMusic(const char *mmusic, UINT16 mflags, boolean looping, UINT32 position, UINT32 fadeinms)
{
	strncpy(queue_name, mmusic, 7);
	queue_flags = mflags;
	queue_looping = looping;
	queue_position = position;
	queue_fadeinms = fadeinms;
}

static void S_ClearQueue(void)
{
	queue_name[0] = queue_flags = queue_looping = queue_position = queue_fadeinms = 0;
}

static void S_ChangeMusicToQueue(void)
{
	S_ChangeMusicEx(queue_name, queue_flags, queue_looping, queue_position, 0, queue_fadeinms);
	S_ClearQueue();
}

void S_ChangeMusicEx(const char *mmusic, UINT16 mflags, boolean looping, UINT32 position, UINT32 prefadems, UINT32 fadeinms)
{
	char newmusic[7] = {0};

	if (S_MusicDisabled()
		|| demo.rewinding // Don't mess with music while rewinding!
		|| demo.title) // SRB2Kart: Demos don't interrupt title screen music
		return;

	strncpy(newmusic, mmusic, 6);

	S_CheckEventMus(newmusic);

	if (LUAh_MusicChange(music_name, newmusic, &mflags, &looping, &position, &prefadems, &fadeinms))
		return;

 	// No Music (empty string)
	if (newmusic[0] == 0)
 	{
		if (prefadems)
			I_FadeSong(0, prefadems, &S_StopMusic);
		else
			S_StopMusic();
		return;
	}

	if (prefadems && S_MusicPlaying()) // queue music change for after fade // allow even if the music is the same
	{
		CONS_Debug(DBG_DETAILED, "Now fading out song %s\n", music_name);
		S_QueueMusic(newmusic, mflags, looping, position, fadeinms);
		I_FadeSong(0, prefadems, S_ChangeMusicToQueue);
		return;
	}
	else if (strnicmp(music_name, newmusic, 6) || (mflags & MUSIC_FORCERESET))
 	{
		CONS_Debug(DBG_DETAILED, "Now playing song %s\n", newmusic);

		S_StopMusic();

		if (!S_LoadMusic(newmusic))
		{
			CONS_Alert(CONS_ERROR, "Music %.6s could not be loaded!\n", newmusic);
			return;
		}

		music_flags = mflags;
		music_looping = looping;

		if (!S_PlayMusic(looping, fadeinms))
 		{
			CONS_Alert(CONS_ERROR, "Music %.6s could not be played!\n", newmusic);
			return;
		}

		if (position)
			I_SetSongPosition(position);

		I_SetSongTrack(mflags & MUSIC_TRACKMASK);
	}
	else if (fadeinms) // let fades happen with same music
	{
		I_SetSongPosition(position);
		I_FadeSong(100, fadeinms, NULL);
 	}
	else // reset volume to 100 with same music
	{
		I_StopFadingSong();
		I_FadeSong(100, 500, NULL);
	}
}

void S_ChangeMusicSpecial (const char *mmusic)
{
	if (cv_resetspecialmusic.value)
		S_ChangeMusic(mmusic, MUSIC_FORCERESET, true);
	else
		S_ChangeMusicInternal(mmusic, true);
}

void S_StopMusic(void)
{
	if (!I_SongPlaying()
		|| demo.rewinding // Don't mess with music while rewinding!
		|| demo.title) // SRB2Kart: Demos don't interrupt title screen music
		return;

	if ((cv_birdmusic.value) && (strcasecmp(music_name, mapmusname) == 0))
		mapmusresume = I_GetSongPosition();

	if (I_SongPaused())
		I_ResumeSong();

	S_SpeedMusic(1.0f);
	I_StopSong();
	S_UnloadMusic(); // for now, stopping also means you unload the song
}

//
// Stop and resume music, during game PAUSE.
//
void S_PauseAudio(void)
{
	if (I_SongPlaying() && !I_SongPaused())
		I_PauseSong();
}

void S_ResumeAudio(void)
{
	if (S_MusicNotInFocus())
		return;

	if (I_SongPlaying() && I_SongPaused())
		I_ResumeSong();
}

void S_SetMusicVolume(INT32 digvolume, INT32 seqvolume)
{
	if (digvolume < 0)
		digvolume = cv_digmusicvolume.value;

#ifdef NO_MIDI
	(void)seqvolume;
#else
	if (seqvolume < 0)
		seqvolume = cv_midimusicvolume.value;
#endif

	if (digvolume < 0 || digvolume > 31)
		CONS_Alert(CONS_WARNING, "digmusicvolume should be between 0-31\n");
	CV_SetValue(&cv_digmusicvolume, digvolume&31);
	actualdigmusicvolume = cv_digmusicvolume.value;   //check for change of var

#ifndef NO_MIDI
	if (seqvolume < 0 || seqvolume > 31)
		CONS_Alert(CONS_WARNING, "midimusicvolume should be between 0-31\n");
	CV_SetValue(&cv_midimusicvolume, seqvolume&31);
	actualmidimusicvolume = cv_midimusicvolume.value;   //check for change of var
#endif

#ifndef NO_MIDI
	seqvolume = 31;
#endif

	switch(I_SongType())
	{
#ifndef NO_MIDI
		case MU_MID:
		//case MU_MOD:
		//case MU_GME:
			I_SetMusicVolume(seqvolume&31);
			break;
#endif
		default:
			I_SetMusicVolume(digvolume&31);
			break;
	}
}

void S_SetRestoreMusicFadeInCvar (consvar_t *cv)
{
	if (!cv_birdmusic.value)
		return;

	music_refade_cv = cv;
}

int S_GetRestoreMusicFadeIn (void)
{
	if (music_refade_cv && cv_fading.value)
		return music_refade_cv->value;
	else
		return 0;
}

/// ------------------------
/// Music Fading
/// ------------------------

void S_SetInternalMusicVolume(INT32 volume)
{
	I_SetInternalMusicVolume(min(max(volume, 0), 100));
}

void S_StopFadingMusic(void)
{
	I_StopFadingSong();
}

boolean S_FadeMusicFromVolume(UINT8 target_volume, INT16 source_volume, UINT32 ms)
{
	if (source_volume < 0)
		return I_FadeSong(target_volume, ms, NULL);
	else
		return I_FadeSongFromVolume(target_volume, source_volume, ms, NULL);
}

boolean S_FadeOutStopMusic(UINT32 ms)
{
	return I_FadeSong(0, ms, &S_StopMusic);
}

/// ------------------------
/// Init & Others
/// ------------------------

/*static boolean S_KeepMusic(void)
{
	//if (!cv_keepmusic.value)
	//return false;

	// should i compare songs or maps?
	static char oldmusname[7] = "";

	if (strcmp(music_name, mapmusname) != 0)
		return false;

	if (strcmp(oldmusname, mapmusname) == 0)
		return true;

	strncpy(oldmusname, mapmusname, 7);
	oldmusname[6] = '\0';

	return false;
}*/

static INT16 oldmap = 0;
static boolean oldencore = 0;
static boolean skipmusic = false;
boolean skipintromus = false;

static const char *musicexception_list[16] = {
	"vote", "voteea", "voteeb", "racent", "krwin",
	"krok", "krlose", "krfail", "kbwin", "kbok",
	"kblose", "kstart", "estart", "wait2j", "CHRSHP",
	"CHRSHF"
};

//checks for any kind of event music like intermission, vote etc.
//always runs when musicchange gets invoked
static void S_CheckEventMus(const char *newmus)
{
	skipmusic = false;

	if (!cv_keepmusic.value)
		return;

	for (int i = 0; i < 16; i++)
		if (stricmp(music_name, musicexception_list[i]) == 0 || stricmp(newmus, musicexception_list[i]) == 0) // weird? sure! but were lucky enough newmus reflects whats being replaced
		{
			skipmusic = true;
			break;
		}

	//CONS_Printf("musname = %s\n", music_name);
	//CONS_Printf("newmus = %s\n", newmus);
	//CONS_Printf("newmus = %d\n", skipmusic);
}

//this one compares map and encoremode instead of the music itself
//makes tunes work and stuff
void S_CheckMap(void)
{
	if (!cv_keepmusic.value)
	{
		keepmusic = false;
		return;
	}

	keepmusic = (!skipmusic && gamestate == GS_LEVEL && oldmap == gamemap && oldencore == encoremode);

	oldencore = encoremode;
	oldmap = gamemap;
}

//
// Per level startup code.
// Kills playing sounds at start of level,
//  determines music if any, changes music.
//
void S_InitMapMusic(void)
{
	char *maptitle = G_BuildMapTitle(gamemap);
	skipintromus = cv_skipintromusic.value && stricmp(maptitle, "Wandering Falls") != 0; // thanks diggle!
	if (maptitle)
		Z_Free(maptitle);

	if (mapmusflags & MUSIC_RELOADRESET)
	{
		strncpy(mapmusname, mapheaderinfo[gamemap-1]->musname, 7);
		mapmusname[6] = 0;
		mapmusflags = (mapheaderinfo[gamemap-1]->mustrack & MUSIC_TRACKMASK);
		mapmusposition = mapheaderinfo[gamemap-1]->muspos;
		mapmusresume = 0;
	}

	if (keepmusic)
		return;

	// Starting ambience should always be restarted
	// lug: but not when we keep the map music lol
	S_StopMusic();

	if (skipintromus)
		return;

	if (leveltime < MUSICSTARTTIME) // SRB2Kart
		S_ChangeMusicInternal((encoremode ? "estart" : "kstart"), false); //S_StopMusic();
	//S_ChangeMusicEx((encoremode ? "estart" : "kstart"), 0, false, mapmusposition, 0, 0);
}

void S_StartMapMusic(void)
{
	//no need to constantly run this after race has started
	if (leveltime > MUSICSTARTTIME)
		return;

	if (keepmusic)
		return;

	if (skipintromus)
	{
		if (leveltime < starttime) // dumb but i dont need this to be spammed honestly
			S_ChangeMusicEx(mapmusname, mapmusflags, true, mapmusposition, 0, 0);
		if (leveltime == MUSICSTARTTIME)
			S_ShowMusicCredit();
		return;
	}

	if (leveltime < starttime) // SRB2Kart
		S_ChangeMusicInternal((encoremode ? "estart" : "kstart"), false); // yes this will be spammed otherwise encore and some stuff WILL overwrite it
	else if (leveltime == starttime) // The GO! sound stops the level start ambience
		S_StopMusic();
	else if (leveltime == MUSICSTARTTIME) // Plays the music after the starting countdown.
	{
		S_ChangeMusicEx(mapmusname, mapmusflags, true, mapmusposition, 0, 0);
		S_ShowMusicCredit();
	}
}

void S_RestartMusic(void)
{
	S_StopMusic();
	I_ShutdownMusic();
	I_InitMusic();

#ifdef NO_MIDI
	S_SetMusicVolume(cv_digmusicvolume.value, -1);
#else
	S_SetMusicVolume(cv_digmusicvolume.value, cv_midimusicvolume.value);
#endif

	if (Playing()) // Gotta make sure the player is in a level
		P_RestoreMusic(&players[consoleplayer]);
	else
		S_ChangeMusicInternal("titles", looptitle);
}

static void Command_Tunes_f(void)
{
	const char *tunearg;
	UINT16 tunenum, track = 0;
	UINT32 position = 0;
	const size_t argc = COM_Argc();

	if (argc < 2) //tunes slot ...
	{
		CONS_Printf("tunes <name/num> [track] [speed] [position] / <-show> / <-default> / <-none>:\n");
		CONS_Printf(M_GetText("Play an arbitrary music lump. If a map number is used, 'MAP##M' is played.\n"));
		CONS_Printf(M_GetText("If the format supports multiple songs, you can specify which one to play.\n\n"));
		CONS_Printf(M_GetText("* With \"-show\", shows the currently playing tune and track.\n"));
		CONS_Printf(M_GetText("* With \"-default\", returns to the default music for the map.\n"));
		CONS_Printf(M_GetText("* With \"-none\", any music playing will be stopped.\n"));
		return;
	}

	tunearg = COM_Argv(1);
	tunenum = (UINT16)atoi(tunearg);
	track = 0;

	if (!strcasecmp(tunearg, "-show"))
	{
		CONS_Printf(M_GetText("The current tune is: %s [track %d]\n"),
			mapmusname, (mapmusflags & MUSIC_TRACKMASK));
		return;
	}
	if (!strcasecmp(tunearg, "-none"))
	{
		S_StopMusic();
		return;
	}
	else if (!strcasecmp(tunearg, "-default"))
	{
		tunearg = mapheaderinfo[gamemap-1]->musname;
		track = mapheaderinfo[gamemap-1]->mustrack;
	}
	else if (!tunearg[2] && toupper(tunearg[0]) >= 'A' && toupper(tunearg[0]) <= 'Z')
		tunenum = (UINT16)M_MapNumber(tunearg[0], tunearg[1]);

	if (tunenum && tunenum >= 1036)
	{
		CONS_Alert(CONS_NOTICE, M_GetText("Valid music slots are 1 to 1035.\n"));
		return;
	}
	if (!tunenum && strlen(tunearg) > 6) // This is automatic -- just show the error just in case
		CONS_Alert(CONS_NOTICE, M_GetText("Music name too long - truncated to six characters.\n"));

	if (argc > 2)
		track = (UINT16)atoi(COM_Argv(2))-1;

	if (tunenum)
		snprintf(mapmusname, 7, "%sM", G_BuildMapName(tunenum));
	else
		strncpy(mapmusname, tunearg, 7);

	if (argc > 4)
		position = (UINT32)atoi(COM_Argv(4));

	mapmusname[6] = 0;
	mapmusflags = (track & MUSIC_TRACKMASK);
	mapmusposition = position;
	mapmusresume = 0;

	S_ChangeMusicEx(mapmusname, mapmusflags, true, mapmusposition, 0, 0);

	if (argc > 3)
	{
		float speed = (float)atof(COM_Argv(3));
		if (speed > 0.0f)
			S_SpeedMusic(speed);
	}
}

static void Command_RestartAudio_f(void)
{
	if (dedicated)  // No point in doing anything if game is a dedicated server.
		return;

	S_StopMusic();
	S_StopSounds();
	I_ShutdownMusic();
	I_ShutdownSound();
	I_StartupSound();
	I_InitMusic();

// These must be called or no sound and music until manually set.

	I_SetSfxVolume(cv_soundvolume.value);
#ifdef NO_MIDI
	S_SetMusicVolume(cv_digmusicvolume.value, -1);
#else
	S_SetMusicVolume(cv_digmusicvolume.value, cv_midimusicvolume.value);
#endif

	S_StartSound(NULL, sfx_strpst);

	if (Playing()) // Gotta make sure the player is in a level
		P_RestoreMusic(&players[consoleplayer]);
	else
		S_ChangeMusicInternal("titles", looptitle);
}

static void Command_RestartMusic_f(void) //same as RestartAudio but only music gets restarted
{
	S_RestartMusic();
}

static void Command_ShowMusicCredit_f(void)
{
	const char *musname = music_name;

	if (COM_Argc() > 1)
	{
		musname = COM_Argv(1);
	}
	else
	{
		S_ShowMusicCredit();
	}

	musicdef_t *def = S_FindMusicCredit(musname);

	if (def)
		CONS_Printf("%.6s - %.255s\n", musname, def->source);
}

static void GameSounds_OnChange(void)
{
	if (M_CheckParm("-nosound") || M_CheckParm("-noaudio"))
		return;

	if (sound_disabled && cv_gamesounds.value)
	{
		sound_disabled = false;
		I_StartupSound(); // will return early if initialised
		S_InitSfxChannels(cv_soundvolume.value);
		S_StartSound(NULL, sfx_strpst);
	}
	else if (!sound_disabled && !cv_gamesounds.value)
	{
		sound_disabled = true;
		S_StopSounds();
	}
}

static void SoundPrecache_OnChange(void)
{
	if (S_PrecacheSound())
	{
		S_InitSfxChannels(cv_soundvolume.value);
	}
	else if (!S_PrecacheSound())
	{
		S_ClearSfx();

		if (!sound_disabled)
			S_InitSfxChannels(cv_soundvolume.value);
	}
}

static void GameDigiMusic_OnChange(void)
{
	if (M_CheckParm("-nomusic") || M_CheckParm("-noaudio"))
		return;
	else if (M_CheckParm("-nodigmusic"))
		return;

	if (digital_disabled && cv_gamedigimusic.value)
	{
		digital_disabled = false;
		I_StartupSound(); // will return early if initialised
		I_InitMusic();
		
		if (Playing())
			P_RestoreMusic(&players[consoleplayer]);
		else
			S_ChangeMusicInternal("titles", looptitle);
	}
	else if (!digital_disabled && !cv_gamedigimusic.value)
	{
		digital_disabled = true;
		if (S_MusicType() != MU_MID)
		{
			if (midi_disabled)
				S_StopMusic();
			else
			{
				char mmusic[7];
				UINT16 mflags;
				boolean looping;

				if (S_MusicInfo(mmusic, &mflags, &looping) && S_MIDIExists(mmusic))
				{
					S_StopMusic();
					S_ChangeMusic(mmusic, mflags, looping);
				}
				else
					S_StopMusic();
			}
		}
	}
}


#ifdef HAVE_OPENMPT
static void ModFilter_OnChange(void)
{
	if (openmpt_mhandle)
		openmpt_module_set_render_param(openmpt_mhandle, OPENMPT_MODULE_RENDER_INTERPOLATIONFILTER_LENGTH, cv_modfilter.value);
		
}

static void StereoSep_OnChange(void)
{
	if (openmpt_mhandle)
		openmpt_module_set_render_param(openmpt_mhandle, OPENMPT_MODULE_RENDER_STEREOSEPARATION_PERCENT, cv_stereosep.value);
		
}

static void AmigaFilter_OnChange(void)
{
	if (openmpt_mhandle)
#if OPENMPT_API_VERSION_MAJOR < 1 && OPENMPT_API_VERSION_MINOR > 4
		openmpt_module_ctl_set_boolean(openmpt_mhandle, "render.resampler.emulate_amiga", cv_amigafilter.value);
#else
		openmpt_module_ctl_set(openmpt_mhandle, "render.resampler.emulate_amiga", cv_amigafilter.value ? "1" : "0");
#endif
}


#if OPENMPT_API_VERSION_MAJOR < 1 && OPENMPT_API_VERSION_MINOR > 4
static void AmigaType_OnChange(void)
{
	if (openmpt_mhandle)
		openmpt_module_ctl_set_text(openmpt_mhandle, "render.resampler.emulate_amiga_type", cv_amigatype.string);

	if (sound_started)
        S_RestartMusic(); //need to restart the music system or else it wont work
}
#endif
#endif

static void BufferSize_OnChange(void)
{
	if (sound_started)
        COM_ImmedExecute("restartaudio");
}

#ifndef NO_MIDI
static void GameMIDIMusic_OnChange(void)
{
	if (M_CheckParm("-nomusic") || M_CheckParm("-noaudio"))
		return;
	else if (M_CheckParm("-nomidimusic"))
		return;

	if (midi_disabled && cv_gamemidimusic.value)
	{
		midi_disabled = false;
		I_InitMusic();
		if (Playing())
			P_RestoreMusic(&players[consoleplayer]);
		else
			S_ChangeMusicInternal("titles", looptitle);
	}
	else if (!midi_disabled && !cv_gamemidimusic.value)
	{
		midi_disabled = true;
		if (S_MusicType() == MU_MID)
		{
			if (digital_disabled)
				S_StopMusic();
			else
			{
				char mmusic[7];
				UINT16 mflags;
				boolean looping;

				if (S_MusicInfo(mmusic, &mflags, &looping) && S_DigExists(mmusic))
				{
					S_StopMusic();
					S_ChangeMusic(mmusic, mflags, looping);
				}
				else
					S_StopMusic();
			}
		}
	}
}
#endif
