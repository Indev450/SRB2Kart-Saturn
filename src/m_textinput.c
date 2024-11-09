#include "m_textinput.h"
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

void M_TextInputInit(textinput_t *input, char *buffer, size_t buffer_size)
{
	input->buffer = buffer;
	input->buffer_size = buffer_size;

	M_TextInputClear(input);
}

void M_TextInputClear(textinput_t *input)
{
	input->cursor = 0;
	input->select = 0;
	input->length = 0;
}

void M_TextInputSetString(textinput_t *input, const char *c)
{
	memset(input->buffer, 0, input->buffer_size);
	strcpy(input->buffer, c);
	input->cursor = input->select = input->length = strlen(c);
}

boolean M_TextInputHandle(textinput_t *input, INT32 key)
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
		if (input->cursor != 0)
			--input->cursor;
		if (!shiftdown)
			input->select = input->cursor;
		return true;
	}
	else if (key == KEY_RIGHTARROW)
	{
		if (input->cursor < input->length)
			++input->cursor;
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
