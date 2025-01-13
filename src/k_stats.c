#include "k_stats.h"
#include "doomstat.h"
#include "g_game.h"
#include "byteptr.h"

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

	if (numplayers > 1)
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

	memset(&kartstats, 0, sizeof(kartstats_t));

	kartstats.vanilla = vanilla;
}

void K_ReadStats(savebuffer_t *save, boolean vanilla)
{
	memset(&kartstats, 0, sizeof(kartstats_t));

	kartstats.vanilla = vanilla;

	kartstats.totalplaytime = READUINT32(save->p);
	kartstats.matchesplayed = READUINT32(save->p);

	// Vanilla only has those 2
	if (vanilla)
		return;

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
}

void K_WriteStats(savebuffer_t *save, boolean vanilla)
{
	WRITEUINT32(save->p, kartstats.totalplaytime);
	WRITEUINT32(save->p, kartstats.matchesplayed);

	// Vanilla only has those 2
	if (vanilla)
		return;

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
}
