// SONIC ROBO BLAST 2 KART
//-----------------------------------------------------------------------------
// Copyright (C) 2025 by Vivian "toastergrl" Grannell.
// Copyright (C) 2025 by Kart Krew.
// Copyright (C) 2020 by Sonic Team Junior.
// Copyright (C) 2000 by DooM Legacy Team.
// Copyright (C) 1996 by id Software, Inc.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  r_skins.c
/// \brief Loading skins

#include "dehacked.h" // get_number (for thok)
#include "doomdef.h"
#include "console.h"
#include "r_draw.h"
#include "r_things.h"
#include "r_skins.h"
#include "g_game.h"
#include "k_kart.h" // SRB2kart
#include "p_local.h"
#include "st_stuff.h"
#include "i_video.h" // rendermode
#include "w_wad.h"
#include "z_zone.h"

#ifdef HWRENDER
#include "hardware/hw_md2.h"
#endif

#include "qs22j.h"

CV_PossibleValue_t Forceskin_cons_t[MAXSKINS+2] = {}; // huehuehuehuehue

#include "discord.h"

INT32 numskins = 0;
INT32 numallskins = 0;
INT32 numlocalskins = 0;
skin_t skins[MAXSKINS];

UINT8 skinstats[9][9][MAXSKINS];
UINT8 skinstatscount[9][9] = {
	{0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 1, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0}
};

UINT8 skinsorted[MAXSKINS] = {};
skin_t localskins[MAXLOCALSKINS] = {};
skin_t allskins[MAXSKINS+MAXLOCALSKINS] = {};

// FIXTHIS: don't work because it must be inistilised before the config load
//#define SKINVALUES
#ifdef SKINVALUES
CV_PossibleValue_t skin_cons_t[MAXSKINS+1] = {};
CV_PossibleValue_t localskin_cons_t[MAXLOCALSKINS+1] = {};
#endif

static void Sk_SetDefaultValue(skin_t *skin, boolean local)
{
	INT32 i;
	//
	// set default skin values
	//
	memset(skin, 0, sizeof(skin_t));

	snprintf(skin->name, sizeof skin->name, "skin %u", K_GetMobjSkinNum(skin, local));
	skin->name[sizeof skin->name - 1] = '\0';

	skin->wadnum = INT16_MAX;

	strcpy(skin->sprite, "");

	strcpy(skin->realname, "Someone");
	strcpy(skin->hudname, "???");
	strncpy(skin->facerank, "PLAYRANK", 9);
	strncpy(skin->facewant, "PLAYWANT", 9);
	strncpy(skin->facemmap, "PLAYMMAP", 9);

	skin->starttranscolor = 160;
	skin->prefcolor = SKINCOLOR_GREEN;

	// SRB2kart
	skin->kartspeed = 5;
	skin->kartweight = 5;
	//

	skin->highresscale = FRACUNIT>>1;

	for (i = 0; i < sfx_skinsoundslot0; i++)
	{
		const INT32 skinsound = S_sfx[i].skinsound;

		if (skinsound != -1)
			skin->soundsid[skinsound] = i;
	}
}

//
// Initialize the basic skins
//
void R_InitSkins(void)
{
	skin_t *skin;
#ifdef SKINVALUES
	INT32 i;

	for (i = 0; i <= MAXSKINS; i++)
	{
		skin_cons_t[i].value = 0;
		skin_cons_t[i].strvalue = NULL;
	}
	for (i = 0; i <= MAXLOCALSKINS; i++)
	{
		localskin_cons_t[i].value = 0;
		localskin_cons_t[i].strvalue = NULL;
	}
#endif

	// skin[0] = Sonic skin
	skin = &skins[0];
	numskins = 1;
	Sk_SetDefaultValue(skin, false);

	memset(skinstats, 0, sizeof(skinstats));
	memset(skinsorted, 0, sizeof(skinsorted));

	// Hardcoded S_SKIN customizations for Sonic.
	strcpy(skin->name,       DEFAULTSKIN);
#ifdef SKINVALUES
	skin_cons_t[0].strvalue = skins[0].name;
#endif

	strcpy(skin->realname,   "Sonic");
	strcpy(skin->hudname,    "SONIC");

	strncpy(skin->facerank, "PLAYRANK", 9);
	strncpy(skin->facewant, "PLAYWANT", 9);
	strncpy(skin->facemmap, "PLAYMMAP", 9);

	skin->flags = 0;

	skin->wadnum = 0; // god what have you brought to this world
	skin->prefcolor = SKINCOLOR_BLUE;
	skin->localskin = false;
	skin->localnum = 0;

	// SRB2kart
	skin->kartspeed = 8;
	skin->kartweight = 2;
	//

	skin->spritedef.numframes = sprites[SPR_PLAY].numframes;
	skin->spritedef.spriteframes = sprites[SPR_PLAY].spriteframes;
	skin->sprinfo = spriteinfo[SPR_PLAY];
	ST_LoadFaceGraphics(skin->facerank, skin->facewant, skin->facemmap, 0);

	// Set values for Sonic skin
	Forceskin_cons_t[1].value = 0;
	Forceskin_cons_t[1].strvalue = skin->name;

	//MD2 for sonic doesn't want to load in Linux.
#ifdef HWRENDER
	if (rendermode == render_opengl)
		HWR_AddPlayerMD2(0, false);
#endif

	// lets set it
	allskins[0] = skins[0];
	numallskins = 1;
}

// returns true if the skin name is found (loaded from pwad)
// warning return -1 if not found
INT32 R_SkinAvailable(const char *name)
{
	INT32 i;

	for (i = 0; i < numskins; i++)
	{
		if (fasticmp(skins[i].name, name))
			return i;
	}

	return -1;
}

// returns true if the skin name is found (loaded from pwad)
// warning return -1 if not found
INT32 R_AnySkinAvailable(const char *name)
{
	INT32 i;

	for (i = 0; i < numallskins; i++)
	{
		if (fasticmp(allskins[i].name, name))
			return i;
	}

	return -1;
}

INT32 R_LocalSkinAvailable(const char *name, boolean local)
{
	INT32 i;

	if (local)
	{
		for (i = 0; i < numlocalskins; i++)
		{
			if (fasticmp(localskins[i].name, name))
				return i;
		}

		return -1;
	}

	return R_SkinAvailable(name);
}

// network code calls this when a 'skin change' is received
boolean SetPlayerSkin(INT32 playernum, const char *skinname)
{
	INT32 i;

	for (i = 0; i < numskins; i++)
	{
		// search in the skin list
		if (fasticmp(skins[i].name, skinname))
		{
			SetPlayerSkinByNum(playernum, i);
			return true;
		}
	}

	if (P_IsLocalPlayer(&players[playernum]))
		CONS_Alert(CONS_WARNING, M_GetText("Skin '%s' not found.\n"), skinname);
	else if (server || IsPlayerAdmin(consoleplayer))
		CONS_Alert(CONS_WARNING, M_GetText("Player %d (%s) skin '%s' not found\n"), playernum, player_names[playernum], skinname);

	SetPlayerSkinByNum(playernum, 0);
	return false;
}

void SetLocalPlayerSkin(INT32 playernum, const char *skinname, consvar_t *cvar)
{
	INT32 i;
	player_t *player = &players[playernum];

	if (!fasticmp(skinname, "none"))
	{
		for (i = 0; i < numlocalskins; i++)
		{
			// search in the localskin list
			if (fasticmp(localskins[i].name, skinname))
			{
				player->localskin = 1 + i;
				player->skinlocal = true;

				if (player->mo)
				{
					player->mo->localskin = &localskins[i];
					player->mo->skinlocal = true;
				}

				goto setcvar;
			}
		}

		for (i = 0; i < numskins; i++)
		{
			// search in the skin list
			if (fasticmp(skins[i].name, skinname))
			{
				player->localskin = 1 + i;
				player->skinlocal = false;

				if (player->mo)
				{
					player->mo->localskin = &skins[i];
					player->mo->skinlocal = false;
				}

				goto setcvar;
			}
		}
	}
	else
	{
		player->localskin = 0;
		player->skinlocal = false;

		if (player->mo)
		{
			player->mo->localskin = NULL;
			player->mo->skinlocal = false;
		}
	}

setcvar:
	if (cvar != NULL)
	{
		if (player->localskin > 0)
		{
			CV_StealthSet(&cv_fakelocalskin, K_GetSkinArray(player->skinlocal)[player->localskin-1].name);
			CV_StealthSet(cvar, K_GetSkinArray(player->skinlocal)[player->localskin-1].name);

		}
		else
		{
			CV_StealthSet(&cv_fakelocalskin, "none");
			CV_StealthSet(cvar, "none");
		}
	}
}

// Same as SetPlayerSkin, but uses the skin #.
// network code calls this when a 'skin change' is received
void SetPlayerSkinByNum(INT32 playernum, INT32 skinnum)
{
	player_t *player = &players[playernum];
	skin_t *skin = &skins[skinnum];

	if (skinnum >= 0 && skinnum < numskins) // Make sure it exists!
	{
		player->skin = skinnum;

		if (player->mo)
			player->mo->skin = skin;

		player->charflags = (UINT32)skin->flags;

		// SRB2kart
		player->kartspeed = skin->kartspeed;
		player->kartweight = skin->kartweight;

		if (player->mo)
			P_SetScale(player->mo, player->mo->scale);

		// for replays: We have changed our skin mid-game; let the game know so it can do the same in the replay!
		demo_extradata[playernum] |= DXD_SKIN;

#ifdef HAVE_DISCORDRPC
		if (player - players == consoleplayer)
			DRPC_UpdatePresence();
#endif

		return;
	}

	if (P_IsLocalPlayer(player))
		CONS_Alert(CONS_WARNING, M_GetText("Skin %d not found\n"), skinnum);
	else if (server || IsPlayerAdmin(consoleplayer))
		CONS_Alert(CONS_WARNING, "Player %d (%s) skin %d not found\n", playernum, player_names[playernum], skinnum);

	SetPlayerSkinByNum(playernum, 0); // not found put the sonic skin
}

//
// Add skins from a pwad, each skin preceded by 'S_SKIN' marker
//

// Does the same is in w_wad, but check only for
// the first 6 characters (this is so we can have S_SKIN1, S_SKIN2..
// for wad editors that don't like multiple resources of the same name)
//
static UINT16 W_CheckForSkinMarkerInPwad(UINT16 wadid, UINT16 startlump)
{
	UINT16 i;
	lumpinfo_t *lump_p;

	// scan forward, start at <startlump>
	if (startlump < wadfiles[wadid]->numlumps)
	{
		lump_p = wadfiles[wadid]->lumpinfo + startlump;

		for (i = startlump; i < wadfiles[wadid]->numlumps; i++, lump_p++)
		{
			if (memcmp(lump_p->name, "S_SKIN", 6) == 0)
				return i;
		}
	}

	return INT16_MAX; // not found
}

//sort function for sorting skin names
static int skinSortFunc(const void *a, const void *b) // tbh i have no clue what the naming conventions for local functions are
{
	int diff = 0;
	const UINT8 val_a = *(const UINT8 *)a;
	const UINT8 val_b = *(const UINT8 *)b;
	const skin_t *in1 = &skins[val_a];
	const skin_t *in2 = &skins[val_b];

	// return (strcmp(in1->realname, in2->realname) < 0) || (strcmp(in1->realname, in2->realname) ==);

	switch (cv_skinselectgridsort.value)
	{
		case SKINMENUSORT_REALNAME:
			// CONS_Printf("Sorting by realname\n");
			// check name
			diff = strcmp(in1->realname, in2->realname);
			if (diff != 0) return diff;

			break;
		case SKINMENUSORT_SPEED:
			// CONS_Printf("Sorting by speed\n");
			// check speed
			diff = in1->kartspeed - in2->kartspeed;
			if (diff != 0) return diff;

			// then check weight
			diff = in1->kartweight - in2->kartweight;
			if (diff != 0) return diff;

			// then check name
			diff = strcmp(in1->realname, in2->realname);
			if (diff != 0) return diff;

			break;
		case SKINMENUSORT_WEIGHT:
			// CONS_Printf("Sorting by weight\n");
			// check weight
			diff = in1->kartweight - in2->kartweight;
			if (diff != 0) return diff;

			// then check speed
			diff = in1->kartspeed - in2->kartspeed;
			if (diff != 0) return diff;

			// then check name
			diff = strcmp(in1->realname, in2->realname);
			if (diff != 0) return diff;

			break;
		case SKINMENUSORT_PREFCOLOR:
			// CONS_Printf("Sorting by prefcolor\n");
			// check prefcolor
			diff = in1->prefcolor - in2->prefcolor;
			if (diff != 0) return diff;

			// then check name
			diff = strcmp(in1->realname, in2->realname);
			if (diff != 0) return diff;

			break;
		case SKINMENUSORT_ID:
			// CONS_Printf("Sorting by id\n");
			// how do i do by ID?????
			// wait why dont i just convert the inputs to UINT32s
			// please tell me im allowed to define variables in here since its a block
			return (int)val_a - (int)val_b;
		case SKINMENUSORT_NAME:
		default:
			break;
	}

	// im scared this somehow will sometimes end up here so im gonna add this here just to be safe
	// now it will often end up here >:3
	return strcmp(in1->name, in2->name);
}

void sortSkinGrid(void)
{
	//CONS_Printf("Sorting skin list (%d)...\n", cv_skinselectgridsort.value);
	qs22j(skinsorted, numskins, sizeof(UINT8), skinSortFunc);
}

//
// Find skin sprites, sounds & optional status bar face, & add them
//
void R_AddSkins(UINT16 wadnum, boolean local)
{
	UINT16 lump, lastlump = 0;
	char *buf;
	char *buf2;
	char *stoken;
	char *value;
	size_t size;
	skin_t *skin;
	boolean hudname, realname;

#define lnumskins (local ? numlocalskins : numskins)

	//
	// search for all skin markers in pwad
	//

	while ((lump = W_CheckForSkinMarkerInPwad(wadnum, lastlump)) != INT16_MAX)
	{
		// advance by default
		lastlump = lump + 1;

		if (!local && numskins >= MAXSKINS)
		{
			CONS_Alert(CONS_WARNING, M_GetText("Unable to add skin, too many characters are loaded (%d maximum)\n"), MAXSKINS);
			continue; // so we know how many skins couldn't be added
		}
		if (local && numlocalskins >= MAXLOCALSKINS)
		{
			CONS_Alert(CONS_WARNING, M_GetText("Unable to add localskin, too many localskins are loaded (%d maximum)\n"), MAXLOCALSKINS);
			continue; // so we know how many skins couldn't be added
		}

		buf = W_CacheLumpNumPwad(wadnum, lump, PU_CACHE);
		size = W_LumpLengthPwad(wadnum, lump);

		// for strtok
		buf2 = malloc(size+1);
		if (!buf2)
			I_Error("R_AddSkins: No more free memory\n");
		memcpy(buf2,buf,size);
		buf2[size] = '\0';

		// set defaults
		skin = &K_GetSkinArray(local)[lnumskins];
		Sk_SetDefaultValue(skin, local);
		skin->wadnum = wadnum;
		hudname = realname = false;

		// parse
		stoken = strtok(buf2, "\r\n= ");
		while (stoken)
		{
			if ((stoken[0] == '/' && stoken[1] == '/')
				|| (stoken[0] == '#'))// skip comments
			{
				strtok(NULL, "\r\n"); // skip end of line
				stoken = strtok(NULL, "\r\n= ");
				continue; // find the real next token
			}

			value = strtok(NULL, "\r\n= ");

			if (!value)
				I_Error("R_AddSkins: syntax error in S_SKIN lump# %d(%s) in WAD %s\n", lump, W_CheckNameForNumPwad(wadnum,lump), wadfiles[wadnum]->filename);

			if (fasticmp(stoken, "name"))
			{
				// the skin name must uniquely identify a single skin
				// I'm lazy so if name is already used I leave the 'skin x'
				// default skin name set in Sk_SetDefaultValue
				if (R_LocalSkinAvailable(value, local) == -1)
				{
					STRBUFCPY(skin->name, value);
					strlwr(skin->name);
				}
				// I'm not lazy, so if the name is already used I make the name 'namex'
				// using the default skin name's number set above
				else
				{
					const size_t stringspace = strlen(value) + sizeof lnumskins + 1;
					char *value2 = Z_Malloc(stringspace, PU_STATIC, NULL);

					snprintf(value2, stringspace, "%s%d", value, lnumskins);
					value2[stringspace - 1] = '\0';

					if (R_LocalSkinAvailable(value2, local) == -1)
					{
						STRBUFCPY(skin->name, value2);
						strlwr(skin->name);
					}

					Z_Free(value2);
				}

				// copy to hudname and fullname as a default.
				if (!realname)
				{
					STRBUFCPY(skin->realname, skin->name);
					for (value = skin->realname; *value; value++)
						if (*value == '_') *value = ' '; // turn _ into spaces.
				}

				if (!hudname)
				{
					STRBUFCPY(skin->hudname, skin->name);
					strupr(skin->hudname);

					for (value = skin->hudname; *value; value++)
						if (*value == '_') *value = ' '; // turn _ into spaces.
				}
			}
			else if (fasticmp(stoken, "realname"))
			{ // Display name (eg. "Knuckles")
				realname = true;
				STRBUFCPY(skin->realname, value);

				for (value = skin->realname; *value; value++)
					if (*value == '_') *value = ' '; // turn _ into spaces.

				if (!hudname)
					STRBUFCPY(skin->hudname, skin->realname);
			}
			else if (fasticmp(stoken, "hudname"))
			{ // Life icon name (eg. "K.T.E")
				hudname = true;
				STRBUFCPY(skin->hudname, value);

				for (value = skin->hudname; *value; value++)
					if (*value == '_') *value = ' '; // turn _ into spaces.

				if (!realname)
					STRBUFCPY(skin->realname, skin->hudname);
			}
			else if (fasticmp(stoken, "sprite"))
			{
				strupr(value);
				memcpy(skin->sprite, value, sizeof skin->sprite);
			}
			else if (fasticmp(stoken, "facerank"))
			{
				strupr(value);
				memcpy(skin->facerank, value, sizeof(skin->facerank)-1);
				skin->facerank[sizeof(skin->facerank)-1] = '\0';
			}
			else if (fasticmp(stoken, "facewant"))
			{
				strupr(value);
				memcpy(skin->facewant, value, sizeof(skin->facewant)-1);
				skin->facewant[sizeof(skin->facewant)-1] = '\0';
			}
			else if (fasticmp(stoken, "facemmap"))
			{
				strupr(value);
				strncpy(skin->facemmap, value, sizeof(skin->facemmap)-1);
				skin->facemmap[sizeof(skin->facemmap)-1] = '\0';
			}
			else if (fasticmp(stoken, "flags")) // character type identification
			{
				skin->flags = get_number(value);
			}
			else if (fasticmp(stoken, "kartspeed"))
			{
				skin->kartspeed = atoi(value);

				if (skin->kartspeed < 1)
					skin->kartspeed = 1;
				if (skin->kartspeed > 9)
					skin->kartspeed = 9;
			}
			else if (fasticmp(stoken, "kartweight"))
			{
				skin->kartweight = atoi(value);

				if (skin->kartweight < 1)
					skin->kartweight = 1;
				if (skin->kartweight > 9)
					skin->kartweight = 9;
			}
			// custom translation table
			else if (fasticmp(stoken, "startcolor"))
			{
				skin->starttranscolor = atoi(value);
			}
			else if (fasticmp(stoken, "prefcolor"))
			{
				skin->prefcolor = K_GetKartColorByName(value);
			}
			else if (fasticmp(stoken, "highresscale"))
			{
				skin->highresscale = FLOAT_TO_FIXED(atof(value));
			}
			else
			{
				INT32 found = false;
				sfxenum_t i;

				// copy name of sounds that are remapped
				// for this skin
				for (i = 0; i < sfx_skinsoundslot0; i++)
				{
					const sfxinfo_t *sfx = &S_sfx[i];

					if (!sfx->name)
						continue;

					if (sfx->skinsound != -1
					&& fasticmp(sfx->name, stoken + 2))
					{
						skin->soundsid[sfx->skinsound] = S_AddSoundFx(value+2, sfx->singularity, sfx->pitch, true);
						found = true;
					}
				}

				if (!found)
					CONS_Debug(DBG_SETUP, "R_AddSkins: Unknown keyword '%s' in S_SKIN lump# %d (WAD %s)\n", stoken, lump, wadfiles[wadnum]->filename);
			}

			stoken = strtok(NULL, "\r\n= ");
		}

		free(buf2);

		lump++; // if no sprite defined use spirte just after this one

		if (skin->sprite[0] == '\0')
		{
			const char *csprname = W_CheckNameForNumPwad(wadnum, lump);

			// skip to end of this skin's frames
			lastlump = lump;

			while (W_CheckNameForNumPwad(wadnum, lastlump) && memcmp(W_CheckNameForNumPwad(wadnum, lastlump), csprname, 4) == 0)
				lastlump++;

			// allocate (or replace) sprite frames, and set spritedef
			R_AddSingleSpriteDef(csprname, &skin->spritedef, wadnum, lump, lastlump);
		}
		else
		{
			// search in the normal sprite tables
			size_t name;
			boolean found = false;
			const char *sprname = skin->sprite;

			for (name = 0; sprnames[name][0] != '\0'; name++)
			{
				if (strncmp(sprnames[name], sprname, 4) == 0)
				{
					skin->spritedef = sprites[name];
					found = true;
				}
			}

			// not found so make a new one
			// go through the entire current wad looking for our sprite
			// don't just mass add anything beginning with our four letters.
			// "HOODFACE" is not a sprite name.
			if (!found)
			{
				UINT16 localllump = 0, lstart = UINT16_MAX, lend = UINT16_MAX;
				const char *lname;

				while ((lname = W_CheckNameForNumPwad(wadnum, localllump)))
				{
					// If this is a valid sprite...
					if (!memcmp(lname, sprname, 4) && lname[4] && lname[5] && lname[5] >= '0' && lname[5] <= '8')
					{
						if (lstart == UINT16_MAX)
							lstart = localllump;
						// If already set do nothing
					}
					else
					{
						if (lstart != UINT16_MAX)
						{
							lend = localllump;
							break;
						}
						// If not already set do nothing
					}

					++localllump;
				}

				R_AddSingleSpriteDef(sprname, &skin->spritedef, wadnum, lstart, lend);
			}

			// I don't particularly care about skipping to the end of the used frames.
			// We could be using frames from ANYWHERE in the current WAD file, including
			// right before us, which is a terrible idea.
			// So just let the function in the while loop take care of it for us.
		}

		//R_FlushTranslationColormapCache();

		CONS_Printf(M_GetText("Added skin '%s'\n"), skin->name);
#ifdef SKINVALUES
		(local ? localskin_cons_t : skin_cons_t)[lnumskins].value = lnumskins;
		(local ? localskin_cons_t : skin_cons_t)[lnumskins].strvalue = skin->name;
#endif
		if (!local)
		{
			// Update the forceskin possiblevalues
			Forceskin_cons_t[numskins+1].value = numskins;
			Forceskin_cons_t[numskins+1].strvalue = skins[numskins].name;
			skin->localskin = false;
			skin->localnum = numskins;
			ST_LoadFaceGraphics(skin->facerank, skin->facewant, skin->facemmap, numskins);
		}
		else
		{
			skin->localskin = true;
			skin->localnum = numlocalskins;
			ST_LoadLocalFaceGraphics(skin->facerank, skin->facewant, skin->facemmap, numlocalskins);
		}

#ifdef HWRENDER
		if (rendermode == render_opengl)
			HWR_AddPlayerMD2(lnumskins, local);
#endif
		if (!local)
		{
			skinstats[skin->kartspeed-1][skin->kartweight-1][skinstatscount[skin->kartspeed-1][skin->kartweight-1]] = numskins;
			CONS_Debug(DBG_SETUP, M_GetText("Added %d to %d, %d\n"), numskins, skin->kartweight, skin->kartweight);
			skinstatscount[skin->kartspeed-1][skin->kartweight-1]++;
			CONS_Debug(DBG_SETUP, M_GetText("Incremented %d, %d to %d\n"), skin->kartspeed, skin->kartweight, skinstatscount[skin->kartspeed - 1][skin->kartweight - 1]);
			skinsorted[numskins] = numskins;
		}

		allskins[numallskins] = K_GetSkinArray(local)[lnumskins];

		local ? numlocalskins++ : numskins++;
		numallskins++;
	}

#undef lnumskins

	return;
}
