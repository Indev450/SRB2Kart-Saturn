#include "k_stats.h"
#include "doomstat.h"
#include "g_game.h"
#include "byteptr.h"
#include "z_zone.h"

kartstats_t kartstats = {0};

void K_StatTicker(void)
{
	if (demo.playback)
		return;

	kartstats.totalplaytime++;

	if (netgame)
		kartstats.onlineplaytime++;
	else if (modeattacking)
		kartstats.raplaytime++;

	if (G_RaceGametype())
		kartstats.raceplaytime++;
	else
		kartstats.battleplaytime++;

	// Should this also track splitscreen players?
	if (!players[consoleplayer].spectator)
	{
		player_t *p = &players[consoleplayer];

		if (p->kartstuff[k_position] == spbplace)
			kartstats.spbtargettime++;

		if (max(p->kartstuff[k_spinouttimer], p->kartstuff[k_wipeoutslow]) > 0)
			kartstats.spinouttime++;
	}
}

void K_StatPlayerHit(player_t *victim, player_t *source)
{
	if (demo.playback)
		return;

	if (victim == &players[consoleplayer])
	{
		if (victim == source)
			kartstats.selfhits++;
	}
	else if (source == &players[consoleplayer])
		kartstats.hits++;
}

void K_StatPlayerSink(player_t *victim, player_t *source)
{
	if (demo.playback)
		return;

	if (victim == &players[consoleplayer])
		kartstats.sinked++;
	else if (source == &players[consoleplayer])
		kartstats.sinks++;
}

void K_StatRound(void)
{
	if (demo.playback)
		return;

	int numplayers = 0;

	for (int i = 0; i < MAXPLAYERS; ++i)
	{
		if (playeringame[i] && !players[i].spectator)
			++numplayers;
	}

	if (numplayers > 1 && !players[consoleplayer].spectator)
	{
		if (players[consoleplayer].kartstuff[k_position] == 1)
			kartstats.totalwins++;
		else if (players[consoleplayer].kartstuff[k_position] <= 3) // Should this check if there are more than 3 players in game?
			kartstats.totalpodium++;
	}
}

void K_EraseStats(void)
{
	// The only field we want to remember
	boolean vanilla = kartstats.vanilla;

	if (kartstats.copy)
		Z_Free(kartstats.copy);

	memset(&kartstats, 0, sizeof(kartstats_t));

	kartstats.vanilla = vanilla;
}

static void K_ReadStatsCustom(savebuffer_t *save)
{
	kartstats.size = READUINT32(save->p);
	kartstats.version = READUINT32(save->p);

	// So many similar-looking read's... scary...
	kartstats.raplaytime = READUINT32(save->p);
	kartstats.onlineplaytime = READUINT32(save->p);
	kartstats.raceplaytime = READUINT32(save->p);
	kartstats.battleplaytime = READUINT32(save->p);
	kartstats.spbtargettime = READUINT32(save->p);
	kartstats.spinouttime = READUINT32(save->p);
	kartstats.totalwins = READUINT32(save->p);
	kartstats.totalpodium = READUINT32(save->p);
	kartstats.hits = READUINT32(save->p);
	kartstats.selfhits = READUINT32(save->p);
	kartstats.sinks = READUINT32(save->p);
	kartstats.sinked = READUINT32(save->p);
	kartstats.respawns = READUINT32(save->p);

	// If kartstats gets updated, uncomment this and read next fields after this early return. Do same on next updates,
	// this way even data is read on different versions, it doesn't get corrupted (as long as fields aren't removed, which shouldn't happen)
	//if (kartstats.version < 2)
	//	return;
	//
	//kartstats.somenewfield = READUINT32(save->p);
}

void K_ReadStats(savebuffer_t *save, boolean vanilla)
{
	// Basically, free old kartstats.copy if needed and memset kartstats with zeros
	K_EraseStats();

	kartstats.vanilla = vanilla;

	kartstats.totalplaytime = READUINT32(save->p);
	kartstats.matchesplayed = READUINT32(save->p);

	// Vanilla only has those 2
	if (vanilla)
		return;

	// Save where block of custom stats starts
	UINT8 *customblockstart = save->p;

	K_ReadStatsCustom(save);

	// Make a copy of entire custom stats block, so unread values will still be written back
	kartstats.copy = Z_Malloc(kartstats.size, PU_STATIC, NULL);
	memcpy(kartstats.copy, customblockstart, kartstats.size);

	// If there were extra fields we couldn't read, skip them
	save->p = customblockstart + kartstats.size;
}

static void K_WriteStatsCustom(savebuffer_t *save)
{
	// Update, if needed
	kartstats.version = max(kartstats.version, KARTSTATSVERSION);

	WRITEUINT32(save->p, kartstats.size); // Note: if we will end up writing more that stored in there, this field in save file would be updated
	WRITEUINT32(save->p, kartstats.version);

	// Version 1
	WRITEUINT32(save->p, kartstats.raplaytime);
	WRITEUINT32(save->p, kartstats.onlineplaytime);
	WRITEUINT32(save->p, kartstats.raceplaytime);
	WRITEUINT32(save->p, kartstats.battleplaytime);
	WRITEUINT32(save->p, kartstats.spbtargettime);
	WRITEUINT32(save->p, kartstats.spinouttime);
	WRITEUINT32(save->p, kartstats.totalwins);
	WRITEUINT32(save->p, kartstats.totalpodium);
	WRITEUINT32(save->p, kartstats.hits);
	WRITEUINT32(save->p, kartstats.selfhits);
	WRITEUINT32(save->p, kartstats.sinks);
	WRITEUINT32(save->p, kartstats.sinked);
	WRITEUINT32(save->p, kartstats.respawns);

	// No need for early returns, but please mark each new block of write's with version it corresponds to :3

	// Version 2
	//WRITEUINT32(save->p, kartstats.somenewfield);
}

void K_WriteStats(savebuffer_t *save, boolean vanilla)
{
	WRITEUINT32(save->p, kartstats.totalplaytime);
	WRITEUINT32(save->p, kartstats.matchesplayed);

	// Vanilla only has those 2
	if (vanilla)
		return;

	// Now need to be careful... We save start of custom stats block
	UINT8 *customblockstart = save->p;

	// Write everything we know how to write
	K_WriteStatsCustom(save);

	// Calculate size of data that we wrote
	UINT32 size = (UINT32)(save->p - customblockstart);

	// Now, if we wrote less data than save originally had, that means our copy that we kept in K_ReadStats
	// has stats from newer version that we need to append
	if (size < kartstats.size)
	{
		I_Assert(kartstats.copy != NULL); // If kartstats.copy is null, kartstats.size **always** should be 0, this can only happen if we create new save
		WRITEMEM(save->p, kartstats.copy + size, kartstats.size - size);
	}
	else if (size > kartstats.size) // No need to do anything if we wrote exactly same amount...
	{
		// ...but, if we wrote more, we need to update size field in header. **It's always located at start of block**
		// So we can just:
		WRITEUINT32(customblockstart, size);
	}
}
