#ifndef __M_TEXTINPUT__
#define __M_TEXTINPUT__

#include "doomtype.h"

typedef struct textinput_s {
	size_t cursor;
	size_t select;
	size_t length;

	char *buffer;
	size_t buffer_size;
} textinput_t;

void M_TextInputInit(textinput_t *input, char *buffer, size_t buffer_size);

void M_TextInputClear(textinput_t *input);

void M_TextInputSetString(textinput_t *input, const char *c);

boolean M_TextInputHandle(textinput_t *input, INT32 key);

#endif
