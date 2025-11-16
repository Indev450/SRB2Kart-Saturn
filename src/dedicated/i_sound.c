// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//-----------------------------------------------------------------------------
/// \file
/// \brief SDL interface for sound

#include <math.h>
#include "../doomdef.h"
#include "../i_sound.h"
#include "../s_sound.h"

/**	\brief Sound subsystem runing and waiting
*/
UINT8 sound_started = false;

/**	\brief	The I_GetSfx function

	\param	sfx	sfx to setup

	\return	data for sfx
*/
void *I_GetSfx(sfxinfo_t *sfx)
{
	(void)sfx;
	return NULL;
}

/**	\brief	The I_FreeSfx function

	\param	sfx	sfx to be freed up

	\return	void
*/
void I_FreeSfx(sfxinfo_t *sfx)
{
	(void)sfx;
}

/**	\brief Init at program start...
*/
void I_StartupSound(void){}

/**	\brief ... shut down and relase at program termination.
*/
void I_ShutdownSound(void){}

/// ------------------------
///  SFX I/O
/// ------------------------

/**	\brief	Starts a sound in a particular sound channel.
	\param	id	sfxid
	\param	vol	volume for sound
	\param	sep	left-right balancle
	\param	pitch	not used
	\param	priority	not used

	\return	sfx handle
*/
INT32 I_StartSound(sfxenum_t id, UINT8 vol, UINT8 sep, INT32 channel)
{
	(void)id;
	(void)vol;
	(void)sep;
	(void)channel;
	return 0;
}

/**	\brief	Stops a sound channel.

	\param	handle	stop sfx handle

	\return	void
*/
void I_StopSound(INT32 handle)
{
	(void)handle;
}

/**	\brief Some digital sound drivers need this.
*/
void I_UpdateSound(void){}

/**	\brief	Called by S_*() functions to see if a channel is still playing.

	\param	handle	sfx handle

	\return	0 if no longer playing, 1 if playing.
*/
boolean I_SoundIsPlaying(INT32 handle)
{
	(void)handle;
	return false;
}

/**	\brief	Updates the sfx handle

	\param	handle	sfx handle
	\param	vol	volume
	\param	sep	separation
	\param	pitch	ptich

	\return	void
*/
void I_UpdateSoundParams(INT32 handle, UINT8 vol, UINT8 sep)
{
	(void)handle;
	(void)vol;
	(void)sep;
}

/**	\brief	The I_SetSfxVolume function

	\param	volume	volume to set at

	\return	void
*/
void I_SetSfxVolume(UINT8 volume)
{
	(void)volume;
}

/// ------------------------
//  MUSIC SYSTEM
/// ------------------------

/** \brief Init the music systems
*/
void I_InitMusic(void){}

/** \brief Shutdown the music systems
*/
void I_ShutdownMusic(void){}

/// ------------------------
//  MUSIC PROPERTIES
/// ------------------------

musictype_t I_SongType(void)
{
	return MU_NONE;
}

boolean I_SongPlaying(void)
{
	return false;
}

boolean I_SongPaused(void)
{
	return false;
}

/// ------------------------
//  MUSIC EFFECTS
/// ------------------------

boolean I_SetSongSpeed(float speed)
{
	(void)speed;
	return false;
}

/// ------------------------
//  MUSIC SEEKING
/// ------------------------

UINT32 I_GetSongLength(void)
{
	return 0;
}

boolean I_SetSongLoopPoint(UINT32 looppoint)
{
	(void)looppoint;
	return false;
}

UINT32 I_GetSongLoopPoint(void)
{
	return 0;
}

boolean I_SetSongPosition(UINT32 position)
{
	(void)position;
	return false;
}

UINT32 I_GetSongPosition(void)
{
	return 0;
}

/// ------------------------
//  MUSIC PLAYBACK
/// ------------------------

/**	\brief	Registers a song handle to song data.

	\param	data	pointer to song data
	\param	len	len of data

	\return	song handle

	\todo Remove this
*/
boolean I_LoadSong(char *data, size_t len)
{
	(void)data;
	(void)len;
	return false;
}

/**	\brief	See ::I_LoadSong, then think backwards

	\param	handle	song handle

	\sa I_LoadSong
	\todo remove midi handle
*/
void I_UnloadSong(void){}

/**	\brief	Called by anything that wishes to start music

	\param	handle	Song handle
	\param	looping	looping it if true

	\return	if true, it's playing the song

	\todo pass music name, not handle
*/
boolean I_PlaySong(boolean looping)
{
	(void)looping;
	return false;
}

/**	\brief	Stops a song over 3 seconds

	\param	handle	Song handle
	\return	void

	/todo drop handle
*/
void I_StopSong(void){}

/**	\brief	PAUSE game handling.

	\param	handle	song handle

	\return	void
*/
void I_PauseSong(void){}

/**	\brief	RESUME game handling

	\param	handle	song handle

	\return	void
*/
void I_ResumeSong(void){}

/**	\brief	The I_SetMusicVolume function

	\param	volume	volume to set at

	\return	void
*/
void I_SetMusicVolume(UINT8 volume)
{
	(void)volume;
}

boolean I_SetSongTrack(INT32 track)
{
	(void)track;
	return false;
}

/// ------------------------
/// MUSIC FADING
/// ------------------------

void I_SetInternalMusicVolume(UINT8 volume)
{
	(void)volume;
}

void I_StopFadingSong(void){}

boolean I_FadeSongFromVolume(UINT8 target_volume, UINT8 source_volume, UINT32 ms, void (*callback)(void))
{
	(void)target_volume;
	(void)source_volume;
	(void)ms;
	(void)callback;
	return false;
}

boolean I_FadeSong(UINT8 target_volume, UINT32 ms, void (*callback)(void))
{
	(void)target_volume;
	(void)ms;
	(void)callback;
	return false;
}

boolean I_FadeOutStopSong(UINT32 ms)
{
	(void)ms;
	return false;
}

boolean I_FadeInPlaySong(UINT32 ms, boolean looping)
{
	(void)ms;
	(void)looping;
	return false;
}

void I_UpdateSongLagThreshold(void)
{
}
