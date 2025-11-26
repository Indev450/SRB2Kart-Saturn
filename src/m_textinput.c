// SONIC ROBO BLAST 2 KART SATURN
//-----------------------------------------------------------------------------
// Copyright (C) 2025 by Indev.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  m_textinput.c
/// \brief User text input implementation

#include "m_textinput.h"
#include "m_menu.h" // MAXSTRINGLENGTH
#include "v_video.h"
#include "r_main.h" // renderisnewtic
#include "i_system.h"
#include "keys.h"
#include "console.h"

static void M_TextInputDel(textinput_t *input, size_t start, size_t end)
{
	size_t len;

	len = (end - start);

	if (end != input->length)
		memmove(&input->buffer[start], &input->buffer[end], input->length-end);
	memset(&input->buffer[input->length - len], 0, len);

	input->length -= len;

	if (input->select >= end)
		input->select -= len;
	else if (input->select > start)
		input->select = start;

	if (input->cursor >= end)
		input->cursor -= len;
	else if (input->cursor > start)
		input->cursor = start;
}

static void M_TextInputDelSelection(textinput_t *input)
{
	size_t start, end;

	if (input->cursor > input->select)
	{
		start = input->select;
		end = input->cursor;
	}
	else
	{
		start = input->cursor;
		end = input->select;
	}

	M_TextInputDel(input, start, end);

	input->select = input->cursor = start;
}

static void M_TextInputAddString(textinput_t *input, const char *c)
{
	size_t csize = strlen(c);

	if (input->length + csize > input->buffer_size-1)
		return;

	if (input->cursor != input->length)
		memmove(&input->buffer[input->cursor+csize], &input->buffer[input->cursor], input->length-input->cursor);
	memcpy(&input->buffer[input->cursor], c, csize);
	input->length += csize;
	input->select = (input->cursor += csize);
	input->buffer[input->length] = 0;
}

static void M_TextInputAddChar(textinput_t *input, char c)
{
	if (input->length >= input->buffer_size-1)
		return;

	if (input->cursor != input->length)
		memmove(&input->buffer[input->cursor+1], &input->buffer[input->cursor], input->length-input->cursor);
	input->buffer[input->cursor++] = c;
	input->buffer[++input->length] = 0;
	input->select = input->cursor;
}

static void M_TextInputDelChar(textinput_t *input)
{
	if (!input->cursor)
		return;

	if (input->cursor != input->length)
		memmove(&input->buffer[input->cursor-1], &input->buffer[input->cursor], input->length-input->cursor);
	input->buffer[--input->length] = 0;
	input->select = --input->cursor;
}

static void M_TextInputToWordEnd(textinput_t *input, boolean move_sel)
{
	// Skip spaces
	while (input->cursor < input->length && isspace(input->buffer[input->cursor]))
		++input->cursor;

	// Skip word
	while (input->cursor < input->length && !isspace(input->buffer[input->cursor]))
		++input->cursor;

	if (move_sel) input->select = input->cursor;
}

static void M_TextInputToWordBegin(textinput_t *input, boolean move_sel)
{
	// Hack, always move back 1 character if possible so if we press ctrl-left at a word beginning
	// we move to previous word
	if (input->cursor) --input->cursor;

	// Skip spaces
	while (input->cursor && isspace(input->buffer[input->cursor]))
		--input->cursor;

	// Skip word
	while (input->cursor && !isspace(input->buffer[input->cursor]))
		--input->cursor;

	// Unless we reached beginning of line, we're pointing at a space before word, so move cursor
	// forward to fix that
	if (input->cursor) ++input->cursor;

	if (move_sel) input->select = input->cursor;
}

static void M_TextInputPaste(textinput_t *input)
{
	const char *paste = I_ClipboardPaste();
	if (input->select != input->cursor)
		M_TextInputDelSelection(input);
	if (paste != NULL)
		M_TextInputAddString(input, paste);
}

static void M_TextInputLeft(textinput_t *input, boolean emotes)
{
	// Just do it simple way if possible
	if (!emotes || input->cursor < 2 || input->buffer[input->cursor-1] != ':')
	{
		if (input->cursor != 0)
			--input->cursor;
		return;
	}

	// Otherwise we need to check if we have to skip emote
	size_t start = input->cursor-2;

	while (input->buffer[start] != ':')
	{
		// We reached start of string and didn't find ':' symbol, thats definitely not an emote
		// so just do it simple way
		if (start == 0)
		{
			M_TextInputLeft(input, false);
			return;
		}

		--start;
	}

	int emotelen = 0;
	if (M_VerifyEmote(input->buffer+start, &emotelen))
		input->cursor -= emotelen; // also skip the :
	else
		M_TextInputLeft(input, false); // Not a valid emote, do the simple thing
}

static void M_TextInputRight(textinput_t *input, boolean emotes)
{
	// Just do it simple way if possible
	if (!emotes || input->cursor == input->length || input->buffer[input->cursor] != ':')
	{
		if (input->cursor < input->length)
			++input->cursor;
		return;
	}

	int emotelen = 0;
	if (M_VerifyEmote(input->buffer+input->cursor, &emotelen))
		input->cursor += emotelen; // Also skip the :
	else
		M_TextInputRight(input, false); // Not a valid emote, do the simple thing
}

// Check if we're inside a valid emote
static boolean M_TextInputCheckEmote(textinput_t *input)
{
	int start = input->cursor;

	// Definitely not an emote
	if (start == 0)
		return false;

	// Might be closing :, need to skip it just in case
	if (input->buffer[start] == ':')
		--start;

	while (input->buffer[start] != ':')
	{
		// No ':' found, definitely not inside emote
		if (start == 0)
			return false;

		--start;
	}

	// Found what may be emote start, now we can check
	return M_VerifyEmote(input->buffer+start, NULL) != NULL;
}

void M_TextInputInit(textinput_t *input, char *buffer, size_t buffer_size)
{
	input->buffer = buffer;
	input->buffer_size = buffer_size;

	input->skull = 0;

	M_TextInputClear(input);
}

void M_TextInputClear(textinput_t *input)
{
	input->cursor = 0;
	input->select = 0;
	input->length = 0;

	input->buffer[0] = 0;
}

void M_TextInputSetString(textinput_t *input, const char *c)
{
	memset(input->buffer, 0, input->buffer_size);
	strncpy(input->buffer, c, input->buffer_size);
	input->cursor = input->select = input->length = strlen(input->buffer);
}

static boolean M_TextInputHandleBase(textinput_t *input, INT32 key, boolean emotes)
{
	if (key == KEY_LSHIFT || key == KEY_RSHIFT
	 || key == KEY_LCTRL || key == KEY_RCTRL
	 || key == KEY_LALT || key == KEY_RALT)
		return false;

	if (shiftdown && key == KEY_INS)
	{
		M_TextInputPaste(input);
		return true;
	}

	if ((cv_keyboardlayout.value != 3 && ctrldown) || (cv_keyboardlayout.value == 3 && ctrldown && !altdown))
	{
		if (key == 'x' || key == 'X')
		{
			if (input->select > input->cursor)
				I_ClipboardCopy(&input->buffer[input->cursor], input->select-input->cursor);
			else
				I_ClipboardCopy(&input->buffer[input->select], input->cursor-input->select);
			M_TextInputDelSelection(input);
			return true;
		}
		else if (key == 'c' || key == 'C')
		{
			if (input->select > input->cursor)
				I_ClipboardCopy(&input->buffer[input->cursor], input->select-input->cursor);
			else
				I_ClipboardCopy(&input->buffer[input->select], input->cursor-input->select);
			return true;
		}
		else if (key == 'v' || key == 'V')
		{
			M_TextInputPaste(input);
			return true;
		}
		else if (key == 'w' || key == 'W')
		{
			size_t word_start, word_end, i;
			word_end = i = input->cursor;

			// Unless we're pointing at the beginning of line, decrement i so we only start
			// removing symbols that come before the cursor
			if (i) --i;

			// We might be pointing to spaces, skip them first
			while (i && isspace(input->buffer[i]))
				--i;

			// Now skip the "word"
			while (i && !isspace(input->buffer[i]))
				--i;

			// Unless we reached beginning of line, i is pointing at first space that was found
			// before word start, and we don't want to remove it
			if (i) ++i;

			word_start = i;

			if (word_start != word_end)
				M_TextInputDel(input, word_start, word_end);

			return true;
		}
		else if (key == KEY_RIGHTARROW)
			M_TextInputToWordEnd(input, !shiftdown);
		else if (key == KEY_LEFTARROW)
			M_TextInputToWordBegin(input, !shiftdown);

		// Select all
		if (key == 'a' || key == 'A')
		{
			input->select = 0;
			input->cursor = input->length;
			return true;
		}

		// ...why shouldn't it eat the key? if it doesn't, it just means you
		// can control Sonic from the console, which is silly
		return true;
	}

	if (key == KEY_LEFTARROW)
	{
		M_TextInputLeft(input, emotes);
		if (!shiftdown)
			input->select = input->cursor;
		return true;
	}
	else if (key == KEY_RIGHTARROW)
	{
		M_TextInputRight(input, emotes);
		if (!shiftdown)
			input->select = input->cursor;
		return true;
	}
	else if (key == KEY_HOME)
	{
		input->cursor = 0;
		if (!shiftdown)
			input->select = input->cursor;
		return true;
	}
	else if (key == KEY_END)
	{
		input->cursor = input->length;
		if (!shiftdown)
			input->select = input->cursor;
		return true;
	}

	// backspace and delete command prompt
	if (input->select != input->cursor)
	{
		if (key == KEY_BACKSPACE || key == KEY_DEL)
		{
			M_TextInputDelSelection(input);
			return true;
		}
	}
	else if (key == KEY_BACKSPACE)
	{
		M_TextInputDelChar(input);
		return true;
	}
	else if (key == KEY_DEL)
	{
		if (input->cursor == input->length)
			return true;
		++input->cursor;
		M_TextInputDelChar(input);
		return true;
	}

	// allow people to use keypad in console (good for typing IP addresses) - Calum
	if (key >= KEY_KEYPAD7 && key <= KEY_KPADDEL)
	{
		char keypad_translation[] = {'7','8','9','-',
		                             '4','5','6','+',
		                             '1','2','3',
		                             '0','.'};

		key = keypad_translation[key - KEY_KEYPAD7];
	}
	else if (key == KEY_KPADSLASH)
		key = '/';

	// same capslock code as hu_stuff.c's HU_responder. Check there for details.
	key = cv_keyboardlayout.value == 3 ? CON_ShitAndAltGrChar(key) : CON_ShiftChar(key);

	// enter a char into the command prompt
	if (key < 32 || key > 127)
		return false;

	if (input->select != input->cursor)
		M_TextInputDelSelection(input);
	M_TextInputAddChar(input, key);

	return true;
}

// Just an alias, more or less
boolean M_TextInputHandle(textinput_t *input, INT32 key)
{
	return M_TextInputHandleBase(input, key, false);
}

static int M_TextInputEmoteStart(textinput_t *input)
{
	if (input->cursor == 0)
		return -1;

	int emotestart = -1;

	for (int i = input->cursor-1; i >= 0 && input->cursor-i <= MAXEMOTENAME; --i)
	{
		// Space can't be part of emote name
		if (isspace(input->buffer[i]))
			break;

		// Found a :, try suggest emotes
		if (input->buffer[i] == ':')
		{
			emotestart = i+1;
			break;
		}
	}

	return emotestart;
}

static boolean M_TextInputCompleteEmote(textinput_t *input, emote_autocomplete_t *autocomplete)
{
	if (input->cursor == 0)
		return false;

	// If we're in "autocomplete" state, delete ':' for latest autocompleted emote
	if (autocomplete->complete[0])
	{
		if (input->buffer[input->cursor-1] == ':')
		{
			M_TextInputDelChar(input);
		}

		autocomplete->skip++;
	}
	else
	{
		// Try to find emote to autocomplete

		int emotestart = autocomplete->emotestart;

		// No emote - no autocomplete
		if (emotestart == -1 || input->cursor - emotestart < 2)
			return false;

		strlcpy(autocomplete->complete, &input->buffer[emotestart], input->cursor-emotestart+1);
	}

	emote_t *completed = M_FindEmote(autocomplete->complete, strlen(autocomplete->complete), autocomplete->skip);

	// We hit end of list of suggested emotes
	if (completed == NULL)
	{
		autocomplete->skip = 0;
		completed = M_FindEmote(autocomplete->complete, strlen(autocomplete->complete), autocomplete->skip);

		// NULL again, list was empty to begin with
		if (completed == NULL)
		{
			// Clear autocompletion state
			autocomplete->complete[0] = 0;
			return false;
		}
	}

	// Erase previously typed emote. Not really efficent but too lazy to count chars and do M_TextInputDel()
	while (input->buffer[input->cursor-1] != ':')
	{
		M_TextInputDelChar(input);
	}

	M_TextInputAddString(input, completed->name);
	M_TextInputAddChar(input, ':');

	return true;
}

boolean M_TextInputHandleEmotes(textinput_t *input, INT32 key, emote_autocomplete_t *autocomplete)
{
	boolean ret = M_TextInputHandleBase(input, key, true);

	// After this handled key we ended up inside an emote, lets fix that
	if (M_TextInputCheckEmote(input))
		M_TextInputToWordEnd(input, !shiftdown);

	// Always update it
	autocomplete->emotestart = M_TextInputEmoteStart(input);

	if (key == '\t' || (autocomplete->complete[0] == 0 && key == KEY_ENTER))
	{
		if (M_TextInputCompleteEmote(input, autocomplete) && key == KEY_ENTER) // Don't send message if we've autocompleted an emote
			ret = true;
	}
	else
	{
		// Reset state just in case
		autocomplete->complete[0] = 0;
		autocomplete->skip = 0;
	}

	return ret;
}

void M_DrawTextInputScroll(INT32 x, INT32 y, textinput_t *input, INT32 flags, INT32 MAXINPUTWIDTH)
{
	if (renderisnewtic)
		input->skull++;
	input->skull %= 8;

	char nametodraw[MAXSTRINGLENGTH*2+1] = {0};

	size_t drawstart = 0;
	size_t drawend = 0; // Only used for selection

	INT32 skullx = x;

	while (V_SubStringWidth(input->buffer+drawstart, input->cursor-drawstart, V_ALLOWLOWERCASE) > MAXINPUTWIDTH)
		++drawstart;

	size_t drawlength = V_SubStringLengthToFit(input->buffer+drawstart, MAXINPUTWIDTH+8, V_ALLOWLOWERCASE)+1;
	drawend = drawstart + drawlength;

	memcpy(nametodraw, input->buffer+drawstart, drawlength);

	if (input->length)
		skullx += V_SubStringWidth(nametodraw, input->cursor-drawstart, V_ALLOWLOWERCASE);

	V_DrawString(x, y, V_ALLOWLOWERCASE|flags, nametodraw);

	// draw text cursor for name
	if (input->skull < 4) // blink cursor
		V_DrawCharacter(skullx, y+3, '_'|flags, false);

	// draw selection
	if (input->select != input->cursor)
	{
		size_t start = min(input->select, input->cursor);
		size_t end =   max(input->select, input->cursor);

		INT32 startx = 0;
		INT32 width = 0;

		// I couldn't figure out one formula so here's bunch of separate cases
		if (start < drawstart && end > drawend) // Selection covers whole visible portion of demo name
		{
			startx = -2;
			width = V_StringWidth(nametodraw, V_ALLOWLOWERCASE)+4;
		}
		else if (start < drawstart) // Only left side of selection is off visible part
		{
			startx = -2;
			size_t len = (end - start) - (drawstart - start);
			width = V_SubStringWidth(nametodraw, len, V_ALLOWLOWERCASE)+2;
		}
		else if (end > drawend) // Only right side of selection is off visible part
		{
			startx = V_SubStringWidth(nametodraw, start-drawstart, V_ALLOWLOWERCASE);
			width = V_StringWidth(nametodraw+(start-drawstart), V_ALLOWLOWERCASE)+2;
		}
		else // All selection is on visible part
		{
			startx = V_SubStringWidth(nametodraw, start-drawstart, V_ALLOWLOWERCASE);
			width = V_SubStringWidth(nametodraw+(start-drawstart), end-start, V_ALLOWLOWERCASE);
		}

		V_DrawFill(x+startx, y, width, 8, 103|V_TRANSLUCENT|flags);
	}
}
