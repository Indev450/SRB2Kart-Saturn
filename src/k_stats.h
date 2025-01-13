#ifndef __K_STATS__
#define __K_STATS__

#include "doomtype.h"
#include "d_player.h"
#include "p_saveg.h"

typedef struct kartstats_s {
	// Remember if stats we loaded are vanilla ones or not
	boolean vanilla;

	// Vanilla
	tic_t totalplaytime;
	UINT32 matchesplayed;

	tic_t raplaytime;
	tic_t onlineplaytime;
	tic_t raceplaytime;
	tic_t battleplaytime;
	tic_t spbtargettime;
	tic_t spinouttime;
	UINT32 totalwins; // 1st place
	UINT32 totalpodium; // 2nd and 3rd place, *but not 1st*
	UINT32 hits;
	UINT32 selfhits;
	UINT32 sinks; // Times hitting *others* by kitchen sink
	UINT32 sinked; // Times *being hit* by kitchen sink
	UINT32 respawns;
} kartstats_t;

extern kartstats_t kartstats;

// Note: All stat-tracking functions check for demo.playback and early return if its true

// Update global stats such as total play time, etc
void K_StatTicker(void);

// Update hit-related stats (PlayerSpin, PlayerSquish, PlayerExplode)
void K_StatPlayerHit(player_t *victim, player_t *source);

// Separate from stuff above
void K_StatPlayerSink(player_t *victim, player_t *source);

// Update round-related stats (such as matchesplayed, totalwins, etc)
void K_StatRound(void);

void K_EraseStats(void);

// If vanilla is true, only read vanilla-supported fields. Everything else is initialized to 0
void K_ReadStats(savebuffer_t *save, boolean vanilla);

// Same as above, except it just doesn't write non-vanilla fields if vanilla is true
void K_WriteStats(savebuffer_t *save, boolean vanilla);


#endif
