// SONIC ROBO BLAST 2
//-----------------------------------------------------------------------------
// Copyright (C) 2014-2025 by Sonic Team Junior.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file
/// \brief SDL interface for sound

#if defined(HAVE_SDL)

#include "../i_sound.h"
#include "../z_zone.h"
#include "../s_sound.h"
#include "../byteptr.h"
#include "../w_wad.h"

/*
Just for hu_stopped. I promise I didn't
write netcode into the sound code, OKAY?
*/
#include "../d_clisrv.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_mutex.h>
#include <sndfile.h>

#ifdef HAVE_LIBGME
#ifdef HAVE_ZLIB
#include <zlib.h>
#endif // HAVE_ZLIB
#include <gme/gme.h>
#define GME_TREBLE 5.0f
#define GME_BASS 1.0f
#endif // HAVE_LIBGME

#ifdef HAVE_OPENMPT
#include "libopenmpt/libopenmpt.h"
#endif

#ifdef HAVE_FLUIDSYNTH
#include <fluidsynth.h>
#endif

#define S_CURVE(x, s, e) sinf(M_PIf / 2.0f * (((x) - (s)) / ((e) - (s))))

/// ------------------------
/// Audio Declarations
/// ------------------------

typedef struct sample_s
{
	float *data;
	size_t len;
} sample_t;

typedef struct sound_s
{
	sample_t *sample;
	size_t pos;
	float volume[2];
	float pitch;
} sound_t;

typedef struct sndlump_s
{
	UINT8 *data;
	INT32 len;
	INT32 pos;
} sndlump_t;

#define MAXSOUNDS 256

UINT8 sound_started = false;

static SDL_AudioDeviceID audio_device;
static SDL_AudioStream *audio_stream;
static SDL_AudioSpec virtual_spec; // audio spec used internally in the engine
static SDL_AudioSpec actual_spec; // audio spec of the physical hardware
static float music_volume, sfx_volume;

static float fading_source;
static float fading_target;
static float fading_from;
static float fading_to;
static void (*fading_callback)(void);

static sound_t *sounds[MAXSOUNDS];

static SF_INFO music_info;
static SNDFILE *music_file;
static SDL_AudioStream *music_stream;
static sndlump_t music_lump;
static float loop_point;
static float song_length;
static bool song_paused;
static bool loop_song;
static float music_speed;

#ifdef HAVE_LIBGME
static Music_Emu *gme;
static UINT16 current_track;
#endif

#ifdef HAVE_OPENMPT
static int mod_err = OPENMPT_ERROR_OK;
static const char *mod_err_str;
static UINT16 current_subsong;
static size_t probesize;
static int result;
#endif

#ifdef HAVE_FLUIDSYNTH
static fluid_settings_t *synth_settings;
static fluid_synth_t *synth;
static fluid_player_t *synth_player;
static bool synth_wait;
static int total_ticks;

static void MidiSoundfontPath_Onchange(void)
{
	if (synth == NULL)
		return;

	SDL_LockAudioStream(audio_stream);
	fluid_synth_sfunload(synth, 1, 0);
	if (fluid_synth_sfload(synth, cv_midisoundfontpath.string, 1) == FLUID_FAILED)
		CONS_Alert(CONS_ERROR, "Unable to load soundfont '%s'\n", cv_midisoundfontpath.string);
	SDL_UnlockAudioStream(audio_stream);
}

static void MidiChorus_OnChange(void)
{
	if (synth_settings != NULL)
	{
		fluid_settings_setint(synth_settings, "synth.chorus.active", cv_midichorus.value > 0);
		fluid_settings_setnum(synth_settings, "synth.chorus.level", FixedToFloat(cv_midichorus.value));
	}
}

static void MidiReverb_OnChange(void)
{
	if (synth_settings != NULL)
	{
		fluid_settings_setint(synth_settings, "synth.reverb.active", cv_midireverb.value > 0);
		fluid_settings_setnum(synth_settings, "synth.reverb.level", FixedToFloat(cv_midireverb.value));
	}
}

consvar_t cv_midisoundfontpath = CVAR_INIT ("midisoundfont", "sf2/GeneralUser-GS.sf2", "Which MIDI soundfont to use", CV_CALL|CV_NOINIT|CV_SAVE, NULL, MidiSoundfontPath_Onchange);
static CV_PossibleValue_t chorus_const_t[] = {{0, "MIN"}, {2 << FRACBITS, "MAX"}, {0, NULL}};
consvar_t cv_midichorus = CVAR_INIT ("midichorus", "1", "Controls the chorus of MIDI playback; setting this too high might cause some instruments to be overexposed", CV_CALL|CV_SAVE|CV_FLOAT, chorus_const_t, MidiChorus_OnChange);
static CV_PossibleValue_t reverb_const_t[] = {{0, "MIN"}, {FRACUNIT, "MAX"}, {0, NULL}};
consvar_t cv_midireverb = CVAR_INIT ("midireverb", "0.8", "Controls the reverb of MIDI playback; setting this too high might cause notes to be drawn out", CV_CALL|CV_SAVE|CV_FLOAT, reverb_const_t, MidiReverb_OnChange);
#endif

//FIXME: this is not how it should be lol
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfloat-equal"

/// ------------------------
/// Audio System
/// ------------------------

#if defined (HAVE_LIBGME) && defined (HAVE_ZLIB)
static const char* get_zlib_error(int zErr)
{
	switch (zErr)
	{
		case Z_ERRNO:
			return "Z_ERRNO";
		case Z_STREAM_ERROR:
			return "Z_STREAM_ERROR";
		case Z_DATA_ERROR:
			return "Z_DATA_ERROR";
		case Z_MEM_ERROR:
			return "Z_MEM_ERROR";
		case Z_BUF_ERROR:
			return "Z_BUF_ERROR";
		case Z_VERSION_ERROR:
			return "Z_VERSION_ERROR";
		default:
			return "unknown error";
	}
}
#endif

static float *AdjustPitch(float *in, int size, int channels, float pitch)
{
	float *out = malloc(size * 4);
	if (out == NULL)
		return NULL;

	if (channels == 1)
	{
		for (int i = 0; i < size; i++)
		{
			float from_low = i * pitch;
			float from_high = (i + 1) * pitch;
			float delta_low = from_low - (int)from_low;
			float delta_high = from_high - (int)from_high;
			out[i] = in[(int)from_low] * (1.0f - delta_low);
			for (int j = (int)from_low + 1; j < (int)from_high; j++)
				out[i] += in[j];
			out[i] += in[(int)from_high] * delta_high;
		}
	}
	else
	{
		for (int i = 0; i < size / 2; i++)
		{
			float from_low = i * pitch;
			float from_high = (i + 1) * pitch;
			float delta_low = from_low - (int)from_low;
			float delta_high = from_high - (int)from_high;
			out[i * 2] = in[(int)from_low * 2] * (1.0f - delta_low);
			out[i * 2+1] = in[(int)from_low * 2+1] * (1.0f - delta_low);
			for (int j = (int)from_low + 1; j < (int)from_high; j++)
			{
				out[i * 2] += in[j * 2];
				out[i * 2+1] += in[j * 2+1];
			}
			out[i * 2] += in[(int)from_high * 2] * delta_high;
			out[i * 2+1] += in[(int)from_high * 2+1] * delta_high;
		}
	}

	return out;
}

static int HandleMIDIEvent(void *data, fluid_midi_event_t *event)
{
	SDL_LockAudioStream(audio_stream);
	synth_wait = false;
	SDL_UnlockAudioStream(audio_stream);
	return fluid_synth_handle_midi_event(data, event);
}

static void MusicCallback(void *userdata, SDL_AudioStream *stream, int additional_amount, int total_amount)
{
	(void)userdata;
	(void)total_amount;

	int amount = additional_amount / 4.0f * music_speed;

	if (music_info.channels == 2 && (amount & 1))
		amount++;
	float *needed = malloc(amount * 4);
	if (needed == NULL)
		return;

	sf_count_t count = sf_read_float(music_file, needed, amount);
	if (count < amount)
	{
		if (loop_song)
		{
			sf_seek(music_file, loop_point * music_info.samplerate, SEEK_SET);
			sf_read_float(music_file, &needed[count], amount - count);
		}
		else
		{
			song_paused = true;
			memset(&needed[count], 0, (amount - count) * 4);
		}
	}
	for (int i = 0; i < amount; i++)
		needed[i] *= music_volume;

	if (music_speed != 1.0f)
	{
		float *adjusted = AdjustPitch(needed, additional_amount / 4, music_info.channels, music_speed);
		if (adjusted != NULL)
		{
			free(needed);
			needed = adjusted;
		}
	}

	SDL_PutAudioStreamData(stream, needed, additional_amount);
	free(needed);
}

// gme and openmpt are both quite quiet...
#define seq_volume (music_volume * 1.5f)

static void StreamCallback(void *userdata, SDL_AudioStream *stream, int additional_amount, int total_amount)
{
	(void)userdata;
	(void)total_amount;
	additional_amount /= 2;

	union
	{
		float *f;
		INT16 *i16;
	} sample_buffer;
	sample_buffer.f = malloc(additional_amount * 4);
	if (sample_buffer.f == NULL)
		return;

	float pos = 0.0f;
	if (fading_from != fading_to)
		pos = I_GetSongPosition();

	if (music_stream != NULL && !song_paused)
	{
		int avail = SDL_GetAudioStreamData(music_stream, sample_buffer.f, additional_amount * 4);
		if (avail == -1)
			memset(sample_buffer.f, 0, additional_amount * 4);
		else if (avail < additional_amount)
			memset(&sample_buffer.f[avail], 0, additional_amount * 4 - avail);
	}
#ifdef HAVE_LIBGME
	else if (gme != NULL && !gme_track_ended(gme) && !song_paused)
	{
		INT16 *buf = malloc(additional_amount * 2);

		if (buf != NULL)
		{
			gme_play(gme, additional_amount, buf);
			for (int i = 0; i < additional_amount; i++)
				sample_buffer.f[i] = buf[i] * seq_volume / 32767.0f;
			free(buf);
		}
		else
		{
			memset(sample_buffer.f, 0, additional_amount * 4);
		}
	}
#endif
#ifdef HAVE_OPENMPT
	else if (openmpt_mhandle != NULL && !song_paused)
	{
		size_t total;
		if (virtual_spec.channels == 1)
			total = openmpt_module_read_float_mono(openmpt_mhandle, virtual_spec.freq, additional_amount, sample_buffer.f);
		else
			total = openmpt_module_read_interleaved_float_stereo(openmpt_mhandle, virtual_spec.freq, additional_amount / 2, sample_buffer.f);

		size_t i = 0;
		while (i < total * 2)
			sample_buffer.f[i++] *= seq_volume;
		memset(&sample_buffer.f[i], 0, (additional_amount - i) * 4);
		while (i < (size_t)additional_amount)
			sample_buffer.f[i++] = 0.0f;
	}
#endif
#ifdef HAVE_FLUIDSYNTH
	else if (synth_player != NULL && !song_paused)
	{
		fluid_synth_write_float(synth, additional_amount / 2, sample_buffer.f, 0, 2, sample_buffer.f, 1, 2);

		for (int i = 0; i < additional_amount; i++)
			sample_buffer.f[i] *= music_volume;
	}
#endif
	else
	{
		memset(sample_buffer.f, 0, additional_amount * 4);
	}

	if (fading_from != fading_to)
	{
		float new_pos = I_GetSongPosition();
		if (new_pos < pos)
		{
			// we looped, compensate
			float len = I_GetSongLength();
			if (len > 0.0f)
			{
				pos -= len - loop_point * 1000.0f;
				fading_from -= len - loop_point * 1000.0f;
				fading_to -= len - loop_point * 1000.0f;
			}
		}

		if (virtual_spec.channels == 1)
		{
			for (int i = 0; i < additional_amount; i++)
			{
				float pos_fine = pos + (new_pos - pos) * i / additional_amount;
				float fade = fading_source + (fading_target - fading_source) * S_CURVE(pos_fine, fading_from, fading_to);
				if (fade < 0.0f)
					fade = 0.0f;
				if (fade > 1.0f)
					fade = 1.0f;
				sample_buffer.f[i] *= music_volume * fade;
			}
		}
		else
		{
			for (int i = 0; i < additional_amount; i += 2)
			{
				float pos_fine = pos + (new_pos - pos) * i / additional_amount;
				float fade = fading_source + (fading_target - fading_source) * S_CURVE(pos_fine, fading_from, fading_to);
				if (fade < 0.0f)
					fade = 0.0f;
				if (fade > 1.0f)
					fade = 1.0f;
				sample_buffer.f[i] *= music_volume * fade;
				sample_buffer.f[i+1] *= music_volume * fade;
			}
		}

		if (pos >= fading_to)
		{
			fading_from = 0.0f;
			fading_to = 0.0f;
		}
	}
	else if (fading_target != 1.0f)
	{
		for (int i = 0; i < additional_amount; i++)
			sample_buffer.f[i] *= fading_target;
	}

	for (size_t i = 0; i < MAXSOUNDS; i++)
	{
		if (sounds[i] == NULL)
			continue;

		float *sound_data = sounds[i]->sample->data;
		size_t sound_len = sounds[i]->sample->len;
		if (sounds[i]->pitch != 1.0f)
		{
			sound_len = additional_amount * sounds[i]->pitch;
			if (virtual_spec.channels == 2 && (sound_len & 1))
				sound_len++;
			sound_data = AdjustPitch(sound_data, sound_len, virtual_spec.channels, sounds[i]->pitch);
			if (sound_data == NULL)
				continue;
		}

		if (sound_len - sounds[i]->pos <= (size_t)additional_amount)
		{
			for (size_t j = 0; j < sound_len - sounds[i]->pos; j++)
				sample_buffer.f[j] += sound_data[j + sounds[i]->pos] * sounds[i]->volume[j & 1] * sfx_volume;

			if (sounds[i]->pitch != 1.0f)
				free(sound_data);

			free(sounds[i]);
			sounds[i] = NULL;
		}
		else
		{
			for (int j = 0; j < additional_amount; j++)
				sample_buffer.f[j] += sound_data[j + sounds[i]->pos] * sounds[i]->volume[j & 1] * sfx_volume;

			if (sounds[i]->pitch != 1.0f)
				free(sound_data);
			sounds[i]->pos += additional_amount;
		}
	}

	for (int i = 0; i < additional_amount; i++)
	{
		if (sample_buffer.f[i] > 1.0f)
			sample_buffer.i16[i] = 32767;
		else if (sample_buffer.f[i] < -1.0f)
			sample_buffer.i16[i] = -32767;
		else
			sample_buffer.i16[i] = sample_buffer.f[i] * 32767;
	}

	SDL_PutAudioStreamData(stream, sample_buffer.i16, additional_amount * 2);
	free(sample_buffer.f);
}

void I_StartupSound(void)
{
#ifdef _WIN32
	// Force DirectSound instead of WASAPI
	// SDL 2.0.6+ defaults to the latter and it screws up our sound effects
	SDL_setenv_unsafe("SDL_AUDIODRIVER", "directsound", 1);
#endif

	if (cv_audbuffersize.string != NULL)
		SDL_setenv_unsafe("SDL_AUDIO_DEVICE_SAMPLE_FRAMES", cv_audbuffersize.string, 1);

	// EE inits audio first so we're following along.
	if (SDL_WasInit(SDL_INIT_AUDIO) == SDL_INIT_AUDIO)
	{
		CONS_Debug(DBG_DETAILED, "SDL Audio already started\n");
		return;
	}
	else if (!SDL_InitSubSystem(SDL_INIT_AUDIO))
	{
		CONS_Alert(CONS_ERROR, "Error initializing SDL Audio: %s\n", SDL_GetError());
		// call to start audio failed -- we do not have it
		return;
	}

	// supposedly you would be able to get this with SDL_GetAudioDeviceFormat before opening the audio device
	// but that doesent work for some strange reason
	// assume 16bit, stereo device and force 44.1khz since it sounds the best to me
	actual_spec.format = SDL_AUDIO_S16;
	actual_spec.channels = 2;
	actual_spec.freq = 44100;

	audio_device = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &actual_spec);
	if (audio_device == 0)
	{
		CONS_Alert(CONS_ERROR, "Error opening audio device: %s\n", SDL_GetError());
		return;
	}

	/*if (!SDL_GetAudioDeviceFormat(audio_device, &actual_spec, NULL))
	 {  *
	 CONS_Alert(CONS_ERROR, "Error retrieving audio format: %s\n", SDL_GetError());
	 return;
	}*/

	audio_stream = SDL_OpenAudioDeviceStream(audio_device, &actual_spec, StreamCallback, NULL);
	if (audio_stream == NULL)
	{
		CONS_Alert(CONS_ERROR, "Error opening audio stream: %s\n", SDL_GetError());
		return;
	}

	SDL_ResumeAudioDevice(audio_device);
	SDL_ResumeAudioStreamDevice(audio_stream);

	virtual_spec.format = SDL_AUDIO_F32LE;
	virtual_spec.channels = actual_spec.channels;
	virtual_spec.freq = actual_spec.freq;

	music_volume = sfx_volume = 0;
	sound_started = true;

#ifdef HAVE_OPENMPT
	CONS_Printf("libopenmpt version: %s\n", openmpt_get_string("library_version"));
	CONS_Printf("libopenmpt build date: %s\n", openmpt_get_string("build"));
#endif
}

void I_ShutdownSound(void)
{
	SDL_DestroyAudioStream(audio_stream);
	SDL_CloseAudioDevice(audio_device);
	SDL_QuitSubSystem(SDL_INIT_AUDIO);
	sound_started = false;
}

void I_UpdateSound(void)
{
	if (fading_callback != NULL)
	{
		if (fading_from == fading_to)
		{
			fading_callback();
			fading_callback = NULL;
		}
	}
}

/// ------------------------
/// SFX
/// ------------------------

static sample_t *CreateSample(float *data, size_t len)
{
	sample_t *sample = malloc(sizeof(sample_t));
	if (sample == NULL)
	{
		free(data);
		return NULL;
	}
	sample->data = data;
	sample->len = len / 4;
	return sample;
}

// TODO: make this toggable, maybe someone prefers linear interpolation...
/*
static sample_t *ConvertDOOMSample(const void *stream)
{
	UINT16 ver, freq;
	UINT32 samples;

	// lump header
	ver = READUINT16(stream); // sound version format?
	if (ver != 3) // It should be 3 if it's a doomsound...
		return NULL; // onos! it's not a doomsound!
	freq = READUINT16(stream);

	// dont divide by 0!
	if (freq == 0)
		return NULL;

	samples = READUINT32(stream);

	SDL_AudioSpec srcspec = {
		.format = SDL_AUDIO_U8,
		.channels = 1,
		.freq = freq,
	};

	float *data;
	int len;
	if (!SDL_ConvertAudioSamples(&srcspec, (const void *)(((const char *)stream)+16), samples-32, &virtual_spec, (void *)&data, &len))
	{
		CONS_Alert(CONS_ERROR, "Failed to convert audio: %s\n", SDL_GetError());
		return NULL;
	}
	return CreateSample(data, len);
}
*/

static sample_t *ds2chunk(const void *stream)
{
	UINT16 ver, freq;
	UINT32 samples, i, newsamples;
	CLEANUP(Z_Pfree)UINT8 *sound = NULL;

	const SINT8 *s;
	INT16 *d;
	INT16 o;
	fixed_t step, frac;

	// lump header
	ver = READUINT16(stream); // sound version format?
	if (ver != 3) // It should be 3 if it's a doomsound...
		return NULL; // onos! it's not a doomsound!

	freq = READUINT16(stream);
	samples = READUINT32(stream);

	if (freq == 0)
		return NULL; // division by zero

	switch (freq)
	{
		case 44100:
			if (samples >= UINT32_MAX>>2)
				return NULL; // would wrap, can't store.

			newsamples = samples;
			break;
		case 22050:
			if (samples >= UINT32_MAX>>3)
				return NULL; // would wrap, can't store.

			newsamples = samples<<1;
			break;
		case 11025:
			if (samples >= UINT32_MAX>>4)
				return NULL; // would wrap, can't store.

			newsamples = samples<<2;
			break;
		default:
			frac = (virtual_spec.freq << FRACBITS) / (UINT32)freq;

			if (!(frac & 0xFFFF)) // other solid multiples (change if FRACBITS != 16)
				newsamples = samples * (frac >> FRACBITS);
			else // strange and unusual fractional frequency steps, plus anything higher than 44100hz.
				newsamples = FixedMul(FixedDiv(samples, freq), virtual_spec.freq) + 1; // add 1 to counter truncation.

			if (newsamples >= UINT32_MAX>>2)
				return NULL; // would and/or did wrap, can't store.

			break;
	}

	sound = Z_Malloc(newsamples<<2, PU_SOUND, NULL); // samples * frequency shift * bytes per sample * channels

	s = (const SINT8 *)stream;
	d = (INT16 *)sound;

	i = 0;

	i = 0;

	switch(freq)
	{
		case 44100: // already at the same rate? well that makes it simple.
			while(i++ < samples)
			{
				o = ((INT16)(*s++)+0x80)<<8; // changed signedness and shift up to 16 bits
				*d++ = o; // left channel
				*d++ = o; // right channel
			}
			break;
		case 22050: // unwrap 2x
			while(i++ < samples)
			{
				o = ((INT16)(*s++)+0x80)<<8; // changed signedness and shift up to 16 bits
				*d++ = o; // left channel
				*d++ = o; // right channel
				*d++ = o; // left channel
				*d++ = o; // right channel
			}
			break;
		case 11025: // unwrap 4x
			while(i++ < samples)
			{
				o = ((INT16)(*s++)+0x80)<<8; // changed signedness and shift up to 16 bits
				*d++ = o; // left channel
				*d++ = o; // right channel
				*d++ = o; // left channel
				*d++ = o; // right channel
				*d++ = o; // left channel
				*d++ = o; // right channel
				*d++ = o; // left channel
				*d++ = o; // right channel
			}
			break;
		default: // convert arbitrary hz to 44100.
			step = 0;
			frac = ((UINT32)freq << FRACBITS) / virtual_spec.freq + 1; //Add 1 to counter truncation.

			while (i < samples)
			{
				o = (INT16)(*s+0x80)<<8; // changed signedness and shift up to 16 bits
				while (step < FRACUNIT) // this is as fast as I can make it.
				{
					*d++ = o; // left channel
					*d++ = o; // right channel
					step += frac;
				}
				do {
					i++; s++;
					step -= FRACUNIT;
				} while (step >= FRACUNIT);
			}
			break;
	}

	size_t num_samples = ((UINT8*)d - sound) / 2;
	float *fdata = malloc(num_samples * sizeof(float));

	d = (INT16 *)sound;

	for (i = 0; i < num_samples; i++)
	{
		// convert and normalise to -1.0 - 1.0
		fdata[i] = d[i] / 32768.0f;
	}

	return CreateSample(fdata, num_samples * sizeof(float));
}

static sf_count_t SF_GetFilelen(void *userdata)
{
	sndlump_t *snd = userdata;
	return snd->len;
}

static sf_count_t SF_Seek(sf_count_t offset, int whence, void *userdata)
{
	sndlump_t *snd = userdata;
	switch (whence)
	{
		case SEEK_SET:
			snd->pos = offset;
			break;
		case SEEK_CUR:
			snd->pos += offset;
			break;
		case SEEK_END:
			snd->pos = snd->len + offset;
			break;
	}
	return snd->pos;
}

static sf_count_t SF_Read(void *ptr, sf_count_t count, void *userdata)
{
	sndlump_t *snd = userdata;
	if (count > snd->len - snd->pos)
		count = snd->len - snd->pos;

	memcpy(ptr, &snd->data[snd->pos], count);
	snd->pos += count;
	return count;
}

static sf_count_t SF_Write(const void *ptr, sf_count_t count, void *userdata)
{
	(void)ptr;
	(void)count;
	(void)userdata;
	// no writing
	return 0;
}

static sf_count_t SF_Tell(void *userdata)
{
	sndlump_t *snd = userdata;
	return snd->pos;
}

void *I_GetSfx(sfxinfo_t *sfx)
{
	void *lump;
	sample_t *chunk;
#ifdef HAVE_LIBGME
	Music_Emu *emu;
	gme_info_t *info;
#endif

	if (sfx->lumpnum == LUMPERROR)
		sfx->lumpnum = S_GetSfxLumpNum(sfx);
	sfx->length = W_LumpLength(sfx->lumpnum);

	lump = W_CacheLumpNum(sfx->lumpnum, PU_SOUND);

	// convert from standard DoomSample format.
	//chunk = ConvertDOOMSample(lump);
	chunk = ds2chunk(lump);
	if (chunk)
	{
		Z_Free(lump);
		return chunk;
	}

	// Not a doom sound? Try something else.
#ifdef HAVE_LIBGME
	// VGZ format
	if (((UINT8 *)lump)[0] == 0x1F
		&& ((UINT8 *)lump)[1] == 0x8B)
	{
#ifdef HAVE_ZLIB
		UINT8 *inflatedData;
		size_t inflatedLen;
		z_stream stream;
		int zErr; // Somewhere to handle any error messages zlib tosses out

		memset(&stream, 0x00, sizeof (z_stream)); // Init zlib stream

		// Begin the inflation process
		inflatedLen = *(UINT32 *)lump + (sfx->length-4); // Last 4 bytes are the decompressed size, typically
		inflatedData = (UINT8 *)malloc(inflatedLen); // Make room for the decompressed data
		stream.total_in = stream.avail_in = sfx->length;
		stream.total_out = stream.avail_out = inflatedLen;
		stream.next_in = (UINT8 *)lump;
		stream.next_out = inflatedData;

		zErr = inflateInit2(&stream, 32 + MAX_WBITS);
		if (zErr == Z_OK) // We're good to go
		{
			zErr = inflate(&stream, Z_FINISH);
			if (zErr == Z_STREAM_END)
			{
				// Run GME on new data
				if (!gme_open_data(inflatedData, inflatedLen, &emu, virtual_spec.freq))
				{
					short *mem;
					UINT32 len;
					gme_equalizer_t eq = {GME_TREBLE, GME_BASS, 0,0,0,0,0,0,0,0};

					inflateEnd(&stream);
					free(inflatedData); // GME supposedly makes a copy for itself, so we don't need this lying around
					free(lump); // We're done with the uninflated lump now, too.

					gme_start_track(emu, 0);
					gme_set_equalizer(emu, &eq);
					gme_track_info(emu, &info, 0);

					len = (info->play_length * 441 / 10) << 2;
					mem = malloc(len);
					gme_play(emu, len >> 1, mem);
					gme_free_info(info);
					gme_delete(emu);

					return CreateSample((void *)mem, len);
				}
			}
			else
				CONS_Alert(CONS_ERROR,"Encountered %s when running inflate: %s\n", get_zlib_error(zErr), stream.msg);
			inflateEnd(&stream);
		}
		else // Hold up, zlib's got a problem
			CONS_Alert(CONS_ERROR,"Encountered %s when running inflateInit: %s\n", get_zlib_error(zErr), stream.msg);
		free(inflatedData); // GME didn't open jack, but don't let that stop us from freeing this up
#else
		return NULL; // No zlib support
#endif
	}
	// Try to read it as a GME sound
	else if (gme_identify_header(lump)[0] != '\0')
	{
		const char *err = gme_open_data(lump, sfx->length, &emu, virtual_spec.freq);
		if (err == NULL)
		{
			short *mem;
			UINT32 len;
			gme_equalizer_t eq = {GME_TREBLE, GME_BASS, 0,0,0,0,0,0,0,0};

			free(lump);

			gme_start_track(emu, 0);
			gme_set_equalizer(emu, &eq);
			gme_track_info(emu, &info, 0);

			len = (info->play_length * 441 / 10) << 2;
			mem = malloc(len);
			gme_play(emu, len >> 1, mem);
			gme_free_info(info);
			gme_delete(emu);

			return CreateSample((void *)mem, len);
		}
		else
		{
			CONS_Alert(CONS_ERROR, "Failed to open GME sound: %s\n", err);
		}
	}
#endif

	sndlump_t sndlump = {
		.data = lump,
		.len = sfx->length,
		.pos = 0,
	};
	SF_VIRTUAL_IO virtual = {
		.get_filelen = SF_GetFilelen,
		.seek = SF_Seek,
		.read = SF_Read,
		.write = SF_Write,
		.tell = SF_Tell,
	};
	SF_INFO sfinfo = {
		.format = 0,
		.seekable = 1,
	};
	SNDFILE *sndfile = sf_open_virtual(&virtual, SFM_READ, &sfinfo, &sndlump);
	if (sndfile == NULL)
	{
		CONS_Alert(CONS_ERROR, "Failed to open audio lump: %s\n", sf_strerror(sndfile));
		return NULL;
	}

	SDL_AudioSpec srcspec = {
		.format = SDL_AUDIO_F32LE,
		.channels = sfinfo.channels,
		.freq = sfinfo.samplerate,
	};
	float *indata = malloc(sfinfo.frames * sfinfo.channels * sizeof(float));
	if (indata == NULL)
	{
		sf_close(sndfile);
		CONS_Alert(CONS_ERROR, "Failed to convert audio: %s\n", SDL_GetError());
		return NULL;
	}

	sf_readf_float(sndfile, indata, sfinfo.frames);
	float *data;
	int len;
	if (!SDL_ConvertAudioSamples(&srcspec, (void *)indata, sfinfo.frames * sfinfo.channels * sizeof(float), &virtual_spec, (void *)&data, &len))
	{
		free(indata);
		sf_close(sndfile);
		CONS_Alert(CONS_ERROR, "Failed to convert audio: %s\n", SDL_GetError());
		return NULL;
	}
	free(indata);
	sf_close(sndfile);
	return CreateSample(data, len);
}

void I_FreeSfx(sfxinfo_t *sfx)
{
	if (sfx->data != NULL)
	{
		sample_t *sample = (sample_t *)sfx->data;

		SDL_LockAudioStream(audio_stream);
		// stop any sound that is playing the sfx we're about to remove
		for (size_t i = 0; i < MAXSOUNDS; i++)
		{
			if (sounds[i] == NULL)
				continue;

			if (sounds[i]->sample == sample)
			{
				free(sounds[i]);
				sounds[i] = NULL;
			}
		}
		SDL_UnlockAudioStream(audio_stream);

		free(sample->data);
		free(sample);
	}

	sfx->data = NULL;
	sfx->lumpnum = LUMPERROR;
}

INT32 I_StartSound(sfxenum_t id, UINT8 vol, UINT8 sep, /*UINT8 pitch, UINT8 priority,*/ INT32 channel)
{
	(void)channel; // ignore this - we do mixing ourselves so we don't need it
	//(void)priority; // priority and channel management is handled by SRB2...
	//(void)pitch; // TODO (we can do this now with SDL3)
	if (S_sfx[id].data == NULL)
		return -1;

	SDL_LockAudioStream(audio_stream);
	for (size_t i = 0; i < MAXSOUNDS; i++)
	{
		if (sounds[i] == NULL)
		{
			// we can't use zone allocation here since it's not thread-safe
			sound_t *sound = malloc(sizeof(sound_t));
			if (sound == NULL)
			{
				SDL_UnlockAudioStream(audio_stream);
				return -1;
			}

			sound->sample = S_sfx[id].data;
			sound->pos = 0;
			//sound->pitch = pitch / 128.0f;
			sound->pitch = 1.0f;
			sound->volume[0] = (float)vol / 255;
			sound->volume[1] = (float)vol / 255;
			if (sep >= 128)
				sound->volume[0] *= 1.0f - ((float)sep-128) / 128.0f;
			else
				sound->volume[1] *= (float)sep / 128.0f;
			sounds[i] = sound;
			SDL_UnlockAudioStream(audio_stream);
			return i;
		}
	}

	SDL_UnlockAudioStream(audio_stream);
	return -1;
}

void I_StopSound(INT32 handle)
{
	if (handle == -1)
		return;

	SDL_LockAudioStream(audio_stream);
	if (sounds[handle] != NULL)
	{
		free(sounds[handle]);
		sounds[handle] = NULL;
	}
	SDL_UnlockAudioStream(audio_stream);
}

boolean I_SoundIsPlaying(INT32 handle)
{
	if (handle == -1)
		return false;

	SDL_LockAudioStream(audio_stream);
	bool playing = sounds[handle] != NULL;
	SDL_UnlockAudioStream(audio_stream);
	return playing;
}

void I_UpdateSoundParams(INT32 handle, UINT8 vol, UINT8 sep/*, UINT8 pitch*/)
{
	//(void)pitch; // TODO
	if (handle == -1)
		return;

	SDL_LockAudioStream(audio_stream);
	if (sounds[handle] == NULL)
	{
		SDL_UnlockAudioStream(audio_stream);
		return;
	}

	sounds[handle]->volume[0] = (float)vol / 255;
	sounds[handle]->volume[1] = (float)vol / 255;
	if (virtual_spec.channels > 1)
	{
		if (sep >= 128)
			sounds[handle]->volume[0] *= 1.0f - ((float)sep-128) / 128.0f;
		else
			sounds[handle]->volume[1] *= (float)sep / 128.0f;
	}
	//sounds[handle]->pitch = pitch / 128.0f;
	sounds[handle]->pitch = 1.0f;
	SDL_UnlockAudioStream(audio_stream);
}

void I_SetSfxVolume(UINT8 volume)
{
	sfx_volume = powf(2.0f, (float)volume / 16) - 1.0f;
}

/// ------------------------
/// Music System
/// ------------------------

static void LogFluidMessage(int level, const char *message, void *data)
{
	(void)data;
	switch (level)
	{
		case FLUID_WARN:
			CONS_Alert(CONS_WARNING, "%s\n", message);
			break;
		case FLUID_ERR:
		case FLUID_PANIC:
			CONS_Alert(CONS_ERROR, "%s\n", message);
			break;
	}
}

void I_InitMusic(void)
{
#ifdef HAVE_FLUIDSYNTH
	if (synth == NULL)
	{
		fluid_set_log_function(FLUID_DBG, NULL, NULL);
		fluid_set_log_function(FLUID_INFO, NULL, NULL);
		fluid_set_log_function(FLUID_WARN, LogFluidMessage, NULL);
		fluid_set_log_function(FLUID_ERR, LogFluidMessage, NULL);
		fluid_set_log_function(FLUID_PANIC, LogFluidMessage, NULL);
		synth_settings = new_fluid_settings();
		fluid_settings_setnum(synth_settings, "synth.gain", 1.0f);
		fluid_settings_setnum(synth_settings, "synth.sample-rate", virtual_spec.freq);
		fluid_settings_setint(synth_settings, "synth.chorus.active", cv_midichorus.value > 0);
		fluid_settings_setnum(synth_settings, "synth.chorus.level", FixedToFloat(cv_midichorus.value));
		fluid_settings_setint(synth_settings, "synth.reverb.active", cv_midireverb.value > 0);
		fluid_settings_setnum(synth_settings, "synth.reverb.level", FixedToFloat(cv_midireverb.value));
		synth = new_fluid_synth(synth_settings);
		if (synth == NULL)
		{
			CONS_Alert(CONS_ERROR, "Failed to initialize FluidSynth\n");
			delete_fluid_settings(synth_settings);
			synth_settings = NULL;
			return;
		}

		if (fluid_synth_sfload(synth, cv_midisoundfontpath.string, 1) == FLUID_FAILED)
			CONS_Alert(CONS_ERROR, "Unable to load soundfont '%s'\n", cv_midisoundfontpath.string);
	}
#endif
}

void I_ShutdownMusic(void)
{
	I_UnloadSong();

#ifdef HAVE_FLUIDSYNTH
	SDL_LockAudioStream(audio_stream);
	delete_fluid_synth(synth);
	delete_fluid_settings(synth_settings);
	synth = NULL;
	synth_settings = NULL;
	SDL_UnlockAudioStream(audio_stream);
#endif
}

/// ------------------------
/// Music Properties
/// ------------------------

musictype_t I_SongType(void)
{
#ifdef HAVE_LIBGME
	if (gme)
		return MU_GME;
#endif
#ifdef HAVE_OPENMPT
	if (openmpt_mhandle)
		return MU_MOD_EX;
#endif
#ifdef HAVE_FLUIDSYNTH
	if (synth_player)
		return MU_MID_EX;
#endif
	if (music_stream)
		return MU_OGG; // the actual format doesn't really matter
	return MU_NONE;
}

boolean I_SongPlaying(void)
{
	return I_SongType() != MU_NONE;
}

boolean I_SongPaused(void)
{
	return song_paused && I_SongType() != MU_NONE;
}

/// ------------------------
/// Music Effects
/// ------------------------

static void SyncMIDI(void)
{
	synth_wait = true;
	for (;;)
	{
		// keep unlocking the stream in a loop, so fluidsynth gets a chance to handle the event.
		SDL_UnlockAudioStream(audio_stream);
		SDL_LockAudioStream(audio_stream);
		if (!synth_wait)
			return;
	}
}

boolean I_SetSongSpeed(float speed)
{
	if (speed > 250.0f)
		speed = 250.0f; //limit speed up to 250x
#ifdef HAVE_LIBGME
	if (gme)
	{
		SDL_LockAudioStream(audio_stream);
		gme_set_tempo(gme, speed);
		SDL_UnlockAudioStream(audio_stream);
		return true;
	}
	else
#endif
#ifdef HAVE_OPENMPT
	if (openmpt_mhandle)
	{
		if (speed > 4.0f)
			speed = 4.0f; // Limit this to 4x to prevent crashing, stupid fix but... ~SteelT 27/9/19

		SDL_LockAudioStream(audio_stream);
#if OPENMPT_API_VERSION_MAJOR < 1 && OPENMPT_API_VERSION_MINOR < 5
		{
			// deprecated in 0.5.0
			char modspd[13];
			sprintf(modspd, "%g", speed);
			openmpt_module_ctl_set(openmpt_mhandle, "play.tempo_factor", modspd);
		}
#else
		openmpt_module_ctl_set_floatingpoint(openmpt_mhandle, "play.tempo_factor", (double)speed);
#endif
		SDL_UnlockAudioStream(audio_stream);
		return true;
	}
#endif
#ifdef HAVE_FLUIDSYNTH
	if (synth_player)
	{
		SDL_LockAudioStream(audio_stream);
#if FLUIDSYNTH_VERSION_MINOR < 2
		fluid_player_set_bpm(synth_player, speed);
#else
		fluid_player_set_tempo(synth_player, FLUID_PLAYER_TEMPO_INTERNAL, speed);
#endif
		SDL_UnlockAudioStream(audio_stream);
		return true;
	}
#endif
	music_speed = speed;
	return true;
}

/// ------------------------
///  MUSIC SEEKING
/// ------------------------

UINT32 I_GetSongLength(void)
{
	INT32 length;

#ifdef HAVE_LIBGME
	if (gme)
	{
		gme_info_t *info;
		gme_err_t gme_e = gme_track_info(gme, &info, current_track);

		if (gme_e != NULL)
		{
			CONS_Alert(CONS_ERROR, "GME error: %s\n", gme_e);
			length = 0;
		}
		else
		{
			// reconstruct info->play_length, from GME source
			// we only want intro + 1 loop, not 2
			length = info->length;
			if (length <= 0)
			{
				length = info->intro_length + info->loop_length; // intro + 1 loop
				if (length <= 0)
					length = 150 * 1000; // 2.5 minutes
			}
		}

		gme_free_info(info);
		return max(length, 0);
	}
#endif
#ifdef HAVE_OPENMPT
	if (openmpt_mhandle)
		return (UINT32)(openmpt_module_get_duration_seconds(openmpt_mhandle) * 1000.0f);
#endif
#ifdef HAVE_FLUIDSYNTH
	if (synth_player)
	{
		SDL_LockAudioStream(audio_stream);
		int bpm = fluid_player_get_bpm(synth_player);
		SDL_UnlockAudioStream(audio_stream);
		return total_ticks * 60 / bpm;
	}
#endif
	if (music_stream)
		return song_length / 1000.0f;

	return 0;
}

boolean I_SetSongLoopPoint(UINT32 looppoint)
{
	if (!music_stream)
		return false;

	UINT32 length = I_GetSongLength();
	if (length > 0)
		looppoint %= length;

	SDL_LockAudioStream(audio_stream);
	loop_point = max((float)(looppoint / 1000.0f), 0);
	SDL_UnlockAudioStream(audio_stream);
	return true;
}

UINT32 I_GetSongLoopPoint(void)
{
#ifdef HAVE_LIBGME
	if (gme)
	{
		INT32 looppoint;
		gme_info_t *info;
		gme_err_t gme_e = gme_track_info(gme, &info, current_track);

		if (gme_e != NULL)
		{
			CONS_Alert(CONS_ERROR, "GME error: %s\n", gme_e);
			looppoint = 0;
		}
		else
			looppoint = info->intro_length > 0 ? info->intro_length : 0;

		gme_free_info(info);
		return max(looppoint, 0);
	}
#endif
	if (music_stream)
		return loop_point * 1000;
	return 0;
}

static UINT32 get_adjusted_position(UINT32 position)
{
	UINT32 length = I_GetSongLength();
	UINT32 looppoint = I_GetSongLoopPoint();
	if (length)
		return position >= length ? (position % (length-looppoint)) : position;
	else
		return position;
}

boolean I_SetSongPosition(UINT32 position)
{
#ifdef HAVE_LIBGME
	if (gme)
	{
		SDL_LockAudioStream(audio_stream);
		position = get_adjusted_position(position);
		gme_err_t gme_e = gme_seek(gme, position);
		SDL_UnlockAudioStream(audio_stream);
		if (gme_e != NULL)
		{
			CONS_Alert(CONS_ERROR, "GME error: %s\n", gme_e);
			return false;
		}
		else
			return true;
	}
#endif
#ifdef HAVE_OPENMPT
	if (openmpt_mhandle)
	{
		SDL_LockAudioStream(audio_stream);
		// This isn't 100% correct because we don't account for loop points because we can't get them.
		// But if you seek past end of song, OpenMPT seeks to 0. So adjust the position anyway.
		openmpt_module_set_position_seconds(openmpt_mhandle, get_adjusted_position(position)/1000.0); // returns new position
		SDL_UnlockAudioStream(audio_stream);
		return true;
	}
#endif
#ifdef HAVE_FLUIDSYNTH
	if (synth_player)
	{
		SDL_LockAudioStream(audio_stream);
		int bpm = fluid_player_get_bpm(synth_player);
		position %= total_ticks * 60 / bpm;
		bool status = fluid_player_seek(synth_player, position * bpm / 60) == FLUID_OK;
		if (status == FLUID_OK)
			SyncMIDI();
		SDL_UnlockAudioStream(audio_stream);
		return status;
	}
#endif

	if (music_stream)
	{
		SDL_LockAudioStream(audio_stream);
		position %= (int)(song_length * music_info.samplerate * music_info.channels);
		bool status = sf_seek(music_file, position, SEEK_SET) != -1;
		SDL_UnlockAudioStream(audio_stream);
		return status;
	}
	return false;
}

UINT32 I_GetSongPosition(void)
{
#ifdef HAVE_LIBGME
	if (gme)
	{
		INT32 position = gme_tell(gme);

		gme_info_t *info;
		gme_err_t gme_e = gme_track_info(gme, &info, current_track);

		if (gme_e != NULL)
		{
			CONS_Alert(CONS_ERROR, "GME error: %s\n", gme_e);
			return position;
		}
		else
		{
			// adjust position, since GME's counter keeps going past loop
			if (info->length > 0)
				position %= info->length;
			else if (info->intro_length + info->loop_length > 0)
				position = position >= (info->intro_length + info->loop_length) ? (position % info->loop_length) : position;
			else
				position %= 150 * 1000; // 2.5 minutes
		}

		gme_free_info(info);
		return max(position, 0);
	}
#endif
#ifdef HAVE_OPENMPT
	if (openmpt_mhandle)
		// This will be incorrect if we adjust for length because we can't get loop points.
		// So return unadjusted. See note in SetMusicPosition: we adjust for that.
		return (UINT32)(openmpt_module_get_position_seconds(openmpt_mhandle)*1000.);
#endif
#ifdef HAVE_FLUIDSYNTH
	if (synth_player)
	{
		SDL_LockAudioStream(audio_stream);
		int bpm = fluid_player_get_bpm(synth_player);
		int ticks = fluid_player_get_current_tick(synth_player);
		ticks %= total_ticks;
		SDL_UnlockAudioStream(audio_stream);
		return ticks * 60 / bpm;
	}
#endif

	if (!music_stream)
		return 0;

	return sf_seek(music_file, 0, SEEK_CUR) * 1000 / music_info.samplerate;
}

/// ------------------------
/// Music Playback
/// ------------------------

static const char loop_prefix[] = "LOOP";
static const char looppoint_key[] = "POINT=";
static const char loopms_key[] = "MS=";
static const size_t loop_prefix_len = sizeof(loop_prefix)-1;
static const size_t looppoint_key_len = sizeof(looppoint_key)-1;
static const size_t loopms_key_len = sizeof(loopms_key)-1;

static void ResetMusic(void)
{
	loop_point = 0.0f;
	song_length = 0.0f;
	song_paused = false;
	fading_target = 1.0f;
	fading_source = fading_from = fading_to = 0.0f;
	fading_callback = NULL;
	music_speed = 1.0f;
}

boolean I_LoadSong(char *data, size_t len)
{
	char *p = data;

	if (!sound_started)
	{
		CONS_Alert(CONS_NOTICE, "Tried to play music before audio was initialized, this is a bug!\n");
		return false;
	}

	if (music_stream
#ifdef HAVE_LIBGME
		|| gme
#endif
#ifdef HAVE_OPENMPT
		|| openmpt_mhandle
#endif
	)
		I_UnloadSong();

	ResetMusic();

	SDL_LockAudioStream(audio_stream);
#ifdef HAVE_LIBGME
	if ((UINT8)data[0] == 0x1F
		&& (UINT8)data[1] == 0x8B)
	{
#ifdef HAVE_ZLIB
		UINT8 *inflatedData;
		size_t inflatedLen;
		z_stream stream;
		int zErr; // Somewhere to handle any error messages zlib tosses out

		memset(&stream, 0x00, sizeof (z_stream)); // Init zlib stream
		// Begin the inflation process
		inflatedLen = *(UINT32 *)(data + (len-4)); // Last 4 bytes are the decompressed size, typically
		inflatedData = (UINT8 *)Z_Calloc(inflatedLen, PU_MUSIC, NULL); // Make room for the decompressed data
		stream.total_in = stream.avail_in = len;
		stream.total_out = stream.avail_out = inflatedLen;
		stream.next_in = (UINT8 *)data;
		stream.next_out = inflatedData;

		zErr = inflateInit2(&stream, 32 + MAX_WBITS);

		if (zErr == Z_OK) // We're good to go
		{
			zErr = inflate(&stream, Z_FINISH);
			if (zErr == Z_STREAM_END)
			{
				// Run GME on new data
				const char *err = gme_open_data(inflatedData, inflatedLen, &gme, virtual_spec.freq);
				if (err == NULL)
				{
					inflateEnd(&stream);
					Z_Free(inflatedData); // GME supposedly makes a copy for itself, so we don't need this lying around

					SDL_UnlockAudioStream(audio_stream);
					return true;
				}

				CONS_Alert(CONS_ERROR, "Failed to open GME soundtrack: %s\n", err);
			}
			else
				CONS_Alert(CONS_ERROR, "Encountered %s when running inflate: %s\n", get_zlib_error(zErr), stream.msg);
			inflateEnd(&stream);
		}
		else // Hold up, zlib's got a problem
			CONS_Alert(CONS_ERROR, "Encountered %s when running inflateInit: %s\n", get_zlib_error(zErr), stream.msg);

		Z_Free(inflatedData); // GME didn't open jack, but don't let that stop us from freeing this up
		SDL_UnlockAudioStream(audio_stream);
		return false;
#else
		SDL_UnlockAudioStream(audio_stream);
		CONS_Alert(CONS_ERROR, "Cannot decompress VGZ; no zlib support\n");
		return false;
#endif
	}
	else if (gme_identify_header(data)[0] != '\0')
	{
		const char *err = gme_open_data(data, len, &gme, virtual_spec.freq);
		SDL_UnlockAudioStream(audio_stream);
		if (err == NULL)
			return true;

		CONS_Alert(CONS_ERROR, "Failed to open GME soundtrack: %s\n", err);
		return false;
	}
#endif

#ifdef HAVE_OPENMPT
	/*
		If the size of the data to be checked is bigger than the recommended size (> 2048 bytes)
		Let's just set the probe size to the recommended size
		Otherwise let's give it the full data size
	*/

	if (len > openmpt_probe_file_header_get_recommended_size())
		probesize = openmpt_probe_file_header_get_recommended_size();
	else
		probesize = len;

	result = openmpt_probe_file_header(OPENMPT_PROBE_FILE_HEADER_FLAGS_DEFAULT, data, probesize, len, NULL, NULL, NULL, NULL, NULL, NULL);

	if (result == OPENMPT_PROBE_FILE_HEADER_RESULT_SUCCESS) // We only cared if it succeeded, continue on if not.
	{
		openmpt_mhandle = openmpt_module_create_from_memory2(data, len, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
		if (!openmpt_mhandle) // Failed to create module handle? Show error and return!
		{
			mod_err = openmpt_module_error_get_last(openmpt_mhandle);
			mod_err_str = openmpt_error_string(mod_err);
			CONS_Alert(CONS_ERROR, "openmpt_module_create_from_memory2: %s\n", mod_err_str);
			SDL_UnlockAudioStream(audio_stream);
			return false;
		}
		else
		{
			SDL_UnlockAudioStream(audio_stream);
			return true; // All good and we're ready for music playback!
		}
	}
#endif

	if (memcmp(data, "MThd", 4) == 0)
	{
#ifdef HAVE_FLUIDSYNTH
		synth_player = new_fluid_player(synth);
		fluid_player_set_playback_callback(synth_player, HandleMIDIEvent, synth);
		if (fluid_player_add_mem(synth_player, data, len) == FLUID_FAILED)
		{
			CONS_Alert(CONS_ERROR, "Cannot play MIDI file: MIDI is invalid or corrupted\n");
			SDL_UnlockAudioStream(audio_stream);
			return false;
		}

		fluid_player_play(synth_player);
		SyncMIDI();
		total_ticks = fluid_player_get_total_ticks(synth_player);

		SDL_UnlockAudioStream(audio_stream);
		return true;
#else
		CONS_Alert(CONS_ERROR, "FluidSynth is not available on this build, MIDI files cannot be played\n");
		SDL_UnlockAudioStream(audio_stream);
		return false;
#endif
	}

	music_lump.data = (UINT8 *)data;
	music_lump.len = len;
	music_lump.pos = 0;
	SF_VIRTUAL_IO virtual = {
		.get_filelen = SF_GetFilelen,
		.seek = SF_Seek,
		.read = SF_Read,
		.write = SF_Write,
		.tell = SF_Tell,
	};
	music_info.format = 0;
	music_info.seekable = 1;
	music_file = sf_open_virtual(&virtual, SFM_READ, &music_info, &music_lump);
	if (music_file == NULL)
	{
		SDL_UnlockAudioStream(audio_stream);
		CONS_Alert(CONS_ERROR, "Failed to create virtual music stream: %s\n", sf_strerror(music_file));
		return false;
	}

	SDL_AudioSpec music_spec = {
		.format = SDL_AUDIO_F32LE,
		.channels = music_info.channels,
		.freq = music_info.samplerate,
	};
	music_stream = SDL_CreateAudioStream(&music_spec, &virtual_spec);
	SDL_UnlockAudioStream(audio_stream);
	if (music_stream == NULL)
	{
		CONS_Alert(CONS_ERROR, "Failed to open music stream: %s\n", SDL_GetError());
		return false;
	}

	SDL_SetAudioStreamGetCallback(music_stream, MusicCallback, NULL);

	// Find the OGG loop point.
	loop_point = 0.0f;
	song_length = (float)music_info.frames / music_info.samplerate;

	while ((UINT32)(p - data) < len)
	{
		if (fpclassify(loop_point) == FP_ZERO && strncmp(p, loop_prefix, loop_prefix_len) == 0)
		{
			p += loop_prefix_len; // skip LOOP
			if (strncmp(p, looppoint_key, looppoint_key_len) == 0) // is it LOOPPOINT=?
			{
				p += looppoint_key_len; // skip POINT=
				loop_point = (float)((44.1f+atoi(p)) / 44100.0f); // LOOPPOINT works by sample count.
				// because SDL_Mixer is USELESS and can't even tell us
				// something simple like the frequency of the streaming music,
				// we are unfortunately forced to assume that ALL MUSIC is 44100hz.
				// This means a lot of tracks that are only 22050hz for a reasonable downloadable file size will loop VERY badly.
			}
			else if (strncmp(p, loopms_key, loopms_key_len) == 0) // is it LOOPMS=?
			{
				p += loopms_key_len; // skip MS=
				loop_point = (float)(atof(p) / 1000.0); // LOOPMS works by real time, as miliseconds.
				// Everything that uses LOOPMS will work perfectly with SDL_Mixer.
			}
		}

		if (fpclassify(loop_point) != FP_ZERO) // Got what we needed
			break;
		else // continue searching
			p++;
	}

	return true;
}

void I_UnloadSong(void)
{
	SDL_LockAudioStream(audio_stream);
#ifdef HAVE_LIBGME
	if (gme)
	{
		gme_delete(gme);
		gme = NULL;
	}
#endif
#ifdef HAVE_OPENMPT
	if (openmpt_mhandle)
	{
		openmpt_module_destroy(openmpt_mhandle);
		openmpt_mhandle = NULL;
	}
#endif
#ifdef HAVE_FLUIDSYNTH
	if (synth_player)
	{
		delete_fluid_player(synth_player);
		synth_player = NULL;
	}
#endif
	if (music_stream)
	{
		SDL_DestroyAudioStream(music_stream);
		sf_close(music_file);
		music_stream = NULL;
		music_file = NULL;
	}
	SDL_UnlockAudioStream(audio_stream);
}

boolean I_PlaySong(boolean looping)
{
	SDL_LockAudioStream(audio_stream);
#ifdef HAVE_LIBGME
	if (gme)
	{
		gme_equalizer_t eq = {GME_TREBLE, GME_BASS, 0,0,0,0,0,0,0,0};
#if defined (GME_VERSION) && GME_VERSION >= 0x000603
		if (looping)
			gme_set_autoload_playback_limit(gme, 0);
#endif
		gme_set_equalizer(gme, &eq);
		gme_start_track(gme, 0);
		current_track = 0;
	}
	else
#endif
#ifdef HAVE_OPENMPT
	if (openmpt_mhandle)
	{
		openmpt_module_select_subsong(openmpt_mhandle, 0);
		openmpt_module_set_render_param(openmpt_mhandle, OPENMPT_MODULE_RENDER_STEREOSEPARATION_PERCENT, cv_stereosep.value); //have a feeling some might like it
#if OPENMPT_API_VERSION_MAJOR < 1 && OPENMPT_API_VERSION_MINOR > 4
		openmpt_module_ctl_set_text(openmpt_mhandle, "dither", "1");
		openmpt_module_ctl_set_boolean(openmpt_mhandle, "render.resampler.emulate_amiga", cv_amigafilter.value);
		openmpt_module_ctl_set_text(openmpt_mhandle, "render.resampler.emulate_amiga_type", cv_amigatype.string);
#else
		openmpt_module_ctl_set(openmpt_mhandle, "dither", "1");
		openmpt_module_ctl_set(openmpt_mhandle, "render.resampler.emulate_amiga", cv_amigafilter.value ? "1" : "0");
#endif
		openmpt_module_set_render_param(openmpt_mhandle, OPENMPT_MODULE_RENDER_INTERPOLATIONFILTER_LENGTH, cv_modfilter.value);
		if (looping)
			openmpt_module_set_repeat_count(openmpt_mhandle, -1); // Always repeat
		current_subsong = 0;
	}
	else
#endif
#ifdef HAVE_FLUIDSYNTH
	if (synth_player)
	{
		fluid_player_set_loop(synth_player, looping ? -1 : 0);
		fluid_player_play(synth_player);
	}
	else
#endif
	if (music_stream == NULL)
	{
		SDL_UnlockAudioStream(audio_stream);
		return false;
	}

	loop_song = looping;
	song_paused = false;
	SDL_UnlockAudioStream(audio_stream);
	return true;
}

void I_StopSong(void)
{
	I_PauseSong();
	I_SetSongPosition(0);
}

void I_PauseSong(void)
{
	song_paused = true;
#ifdef HAVE_FLUIDSYNTH
	if (synth_player)
	{
		SDL_LockAudioStream(audio_stream);
		fluid_synth_all_sounds_off(synth, -1);
		SDL_UnlockAudioStream(audio_stream);
	}
#endif
}

void I_ResumeSong(void)
{
	song_paused = false;
}

void I_SetMusicVolume(UINT8 volume)
{
	music_volume = powf(2.0f, (float)volume / 16) - 1.0f;
}

boolean I_SetSongTrack(INT32 track)
{
#ifdef HAVE_LIBGME
	// If the specified track is within the number of tracks playing, then change it
	if (gme)
	{
		if (current_track == track)
			return false;
		SDL_LockAudioStream(audio_stream);
		if (track >= 0 && track < gme_track_count(gme)-1)
		{
			gme_err_t gme_e = gme_start_track(gme, track);
			if (gme_e != NULL)
			{
				CONS_Alert(CONS_ERROR, "GME error: %s\n", gme_e);
				SDL_UnlockAudioStream(audio_stream);
				return false;
			}
			current_track = track;
			SDL_UnlockAudioStream(audio_stream);
			return true;
		}
		SDL_UnlockAudioStream(audio_stream);
		return false;
	}
	else
#endif
#ifdef HAVE_OPENMPT
	if (openmpt_mhandle)
	{
		if (current_subsong == track)
			return false;
		SDL_LockAudioStream(audio_stream);
		if (track >= 0 && track < openmpt_module_get_num_subsongs(openmpt_mhandle))
		{
			openmpt_module_select_subsong(openmpt_mhandle, track);
			current_subsong = track;
			SDL_UnlockAudioStream(audio_stream);
			return true;
		}
		SDL_UnlockAudioStream(audio_stream);

		return false;
	}
#endif
	(void)track;
	return false;
}

/// ------------------------
/// MUSIC FADING
/// ------------------------

void I_SetInternalMusicVolume(UINT8 volume)
{
	I_StopFadingSong();
	fading_target = powf(2.0f, volume / 100.0f) - 1.0f;
}

void I_StopFadingSong(void)
{
	SDL_LockAudioStream(audio_stream);
	fading_source = fading_from = fading_to = 0.0f;
	SDL_UnlockAudioStream(audio_stream);
}

boolean I_FadeSongFromVolume(UINT8 target_volume, UINT8 source_volume, UINT32 ms, void (*callback)(void))
{
	if (ms == 0 || target_volume == source_volume)
	{
		I_StopFadingSong();
		fading_target = powf(2.0f, target_volume / 100.0f) - 1.0f;
		if (callback)
			(*callback)();
		return true;
	}

	SDL_LockAudioStream(audio_stream);
	fading_source = powf(2.0f, source_volume / 100.0f) - 1.0f;
	fading_target = powf(2.0f, target_volume / 100.0f) - 1.0f;
	fading_from = I_GetSongPosition();
	fading_to = fading_from + ms;
	fading_callback = callback;
	SDL_UnlockAudioStream(audio_stream);
	return true;
}

boolean I_FadeSong(UINT8 target_volume, UINT32 ms, void (*callback)(void))
{
	return I_FadeSongFromVolume(target_volume, fading_target * 100.0f, ms, callback);
}

boolean I_FadeOutStopSong(UINT32 ms)
{
	return I_FadeSongFromVolume(0, fading_target * 100.0f, ms, I_StopSong);
}

boolean I_FadeInPlaySong(UINT32 ms, boolean looping)
{
	if (I_PlaySong(looping))
		return I_FadeSongFromVolume(100, 0, ms, NULL);
	else
		return false;
}

#pragma GCC diagnostic pop
#endif

