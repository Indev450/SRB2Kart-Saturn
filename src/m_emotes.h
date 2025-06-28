#ifndef __M_EMOTES__
#define __M_EMOTES__

#ifdef __cplusplus
extern "C" {
#endif

#include "doomdef.h"
#include "command.h"

extern consvar_t cv_emotes;

#define MAXEMOTENAME 32
#define MAXEMOTEFRAMES 32

typedef struct emote_s {
	char frames[MAXEMOTEFRAMES][9];
	UINT8 numframes;
	tic_t timeperframe;
} emote_t;

// Reads and adds emotes from EMOTES lump, with syntax similar to soc
void M_LoadEmotes(UINT16 wadnum);

// Load emotes from all base wads
void M_InitEmotes(void);

// Finds first matching emote, ignoring first few matches
emote_t *M_FindEmote(const char *name, int skips);

// Starts from ':' character, goes until it finds ':' or end of string
// If end of string is found, or closing ':' is found but resulting emote name doesn't exist,
// it returns null, otherwise it returns emote and stores into *emotelen how much characters
// needs to be skipped to get past emote name
emote_t *M_VerifyEmote(const char *name, int *emotelen);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // __M_EMOTES__
