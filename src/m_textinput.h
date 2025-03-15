#ifndef __M_TEXTINPUT__
#define __M_TEXTINPUT__

#include "doomtype.h"

typedef struct textinput_s {
	size_t cursor;
	size_t select;
	size_t length;

	char *buffer;
	size_t buffer_size;

	unsigned skull;
} textinput_t;

void M_TextInputInit(textinput_t *input, char *buffer, size_t buffer_size);

void M_TextInputClear(textinput_t *input);

void M_TextInputSetString(textinput_t *input, const char *c);

boolean M_TextInputHandle(textinput_t *input, INT32 key);

#define M_DrawTextInput(x, y, input, flags) M_DrawTextInputScroll((x), (y), (input), (flags), ((input)->buffer_size-1)*8)
void M_DrawTextInputScroll(INT32 x, INT32 y, textinput_t *input, INT32 flags, INT32 MAXINPUTWIDTH);

#endif
