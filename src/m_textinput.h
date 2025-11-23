// SONIC ROBO BLAST 2 KART SATURN
//-----------------------------------------------------------------------------
// Copyright (C) 2025 by Indev.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  m_textinput.h
/// \brief User text input implementation

#ifndef __M_TEXTINPUT__
#define __M_TEXTINPUT__

#include "doomtype.h"
#include "m_emotes.h" // M_VerifyEmote, M_FindEmote

typedef struct textinput_s {
	size_t cursor;
	size_t select;
	size_t length;

	char *buffer;
	size_t buffer_size;

	unsigned skull;
} textinput_t;

typedef struct emote_autocomplete_s {
	char complete[MAXEMOTENAME+1];
	int skip;
	int emotestart;
} emote_autocomplete_t;

void M_TextInputInit(textinput_t *input, char *buffer, size_t buffer_size);

void M_TextInputClear(textinput_t *input);

void M_TextInputSetString(textinput_t *input, const char *c);

boolean M_TextInputHandle(textinput_t *input, INT32 key);

// Does everything same as M_TextInputHandle, but also handles emotes in input string
// If currently typing something that looks like an existing emote, return suggestions for it
boolean M_TextInputHandleEmotes(textinput_t *input, INT32 key, emote_autocomplete_t *autocomplete);

#define M_DrawTextInput(x, y, input, flags) M_DrawTextInputScroll((x), (y), (input), (flags), ((input)->buffer_size-1)*8)
void M_DrawTextInputScroll(INT32 x, INT32 y, textinput_t *input, INT32 flags, INT32 MAXINPUTWIDTH);

#endif
