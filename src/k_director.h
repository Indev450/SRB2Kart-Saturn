// SONIC ROBO BLAST 2 KART
//-----------------------------------------------------------------------------
// Copyright (C) 2024 by AJ "Tyron" Martinez.
// Copyright (C) 2024 by James Robert Roman.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  k_director.h
/// \brief SRB2kart automatic spectator camera.

extern struct directorinfo
{
    tic_t cooldown; // how long has it been since we last switched?
    tic_t freeze;   // when nonzero, fixed switch pending, freeze logic!
    INT32 attacker; // who to switch to when freeze delay elapses
    INT32 maxdist;  // how far is the closest player from finishing?

    INT32 sortedplayers[MAXPLAYERS]; // position-1 goes in, player index comes out.
    INT32 gap[MAXPLAYERS];           // gap between a given position and their closest pursuer
    INT32 boredom[MAXPLAYERS];       // how long has a given position had no credible attackers?
} directorinfo;

void K_InitDirector(void);
void K_UpdateDirector(void);
void K_DrawDirectorDebugger(void);
void K_DirectorFollowAttack(player_t *player, mobj_t *inflictor, mobj_t *source);
void K_ToggleDirector(void);
