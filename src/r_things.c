// SONIC ROBO BLAST 2
//-----------------------------------------------------------------------------
// Copyright (C) 1993-1996 by id Software, Inc.
// Copyright (C) 1998-2000 by DooM Legacy Team.
// Copyright (C) 1999-2018 by Sonic Team Junior.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  r_things.c
/// \brief Refresh of things, i.e. objects represented by sprites

#include "doomdef.h"
#include "console.h"
#include "g_game.h"
#include "r_local.h"
#include "st_stuff.h"
#include "w_wad.h"
#include "z_zone.h"
#include "m_misc.h"
#include "i_video.h" // rendermode
#include "r_fps.h"
#include "r_things.h"
#include "r_patch.h"
#include "r_plane.h"
#include "p_tick.h"
#include "p_local.h"
#include "p_slopes.h"
#include "dehacked.h" // get_number (for thok)
#include "d_netfil.h" // blargh. for nameonly().
#include "m_cheat.h" // objectplace
#include "k_kart.h" // SRB2kart
#include "p_local.h" // stplyr
#ifdef HWRENDER
#include "hardware/hw_md2.h"
#endif

#include "qs22j.h"

#ifdef PC_DOS
#include <stdio.h> // for snprintf
int	snprintf(char *str, size_t n, const char *fmt, ...);
//int	vsnprintf(char *str, size_t n, const char *fmt, va_list ap);
#endif

CV_PossibleValue_t Forceskin_cons_t[MAXSKINS+2];

static void R_InitSkins(void);

#define MINZ (FRACUNIT*4)
#define BASEYCENTER (BASEVIDHEIGHT/2)

typedef struct
{
	INT32 x1, x2;
	INT32 column;
	INT32 topclip, bottomclip;
} maskdraw_t;

//
// Sprite rotation 0 is facing the viewer,
//  rotation 1 is one angle turn CLOCKWISE around the axis.
// This is not the same as the angle,
//  which increases counter clockwise (protractor).
// There was a lot of stuff grabbed wrong, so I changed it...
//
static lighttable_t **spritelights;

// constant arrays used for psprite clipping and initializing clipping
INT16 negonearray[MAXVIDWIDTH];
INT16 screenheightarray[MAXVIDWIDTH];

spriteinfo_t spriteinfo[NUMSPRITES];

//
// INITIALIZATION FUNCTIONS
//

// variables used to look up and range check thing_t sprites patches
spritedef_t *sprites;
size_t numsprites;

static spriteframe_t sprtemp[64];
static size_t maxframe;
static const char *spritename;

//
// Clipping against drawsegs optimization, from prboom-plus
//
// TODO: This should be done with proper subsector pass through
// sprites which would ideally remove the need to do it at all.
// Unfortunately, SRB2's drawing loop has lots of annoying
// changes from Doom for portals, which make it hard to implement.

typedef struct drawseg_xrange_item_s
{
	INT16 x1, x2;
	drawseg_t *user;
} drawseg_xrange_item_t;

typedef struct drawsegs_xrange_s
{
	drawseg_xrange_item_t *items;
	INT32 count;
} drawsegs_xrange_t;

#define DS_RANGES_COUNT 3
static drawsegs_xrange_t drawsegs_xranges[DS_RANGES_COUNT];

static drawseg_xrange_item_t *drawsegs_xrange;
static size_t drawsegs_xrange_size = 0;
static INT32 drawsegs_xrange_count = 0;

// ==========================================================================
//
// Sprite loading routines: support sprites in pwad, dehacked sprite renaming,
// replacing not all frames of an existing sprite, add sprites at run-time,
// add wads at run-time.
//
// ==========================================================================

//
//
//
static void R_InstallSpriteLump(UINT16 wad,            // graphics patch
                                UINT16 lump,
                                size_t lumpid,      // identifier
                                UINT8 frame,
                                UINT8 rotation,
                                UINT8 flipped)
{
	char cn = R_Frame2Char(frame); // for debugging

	INT32 r;
	lumpnum_t lumppat = wad;
	lumppat <<= 16;
	lumppat += lump;

	if (frame >= 64 || !(R_ValidSpriteAngle(rotation)))
		I_Error("R_InstallSpriteLump: Bad frame characters in lump %s", W_CheckNameForNum(lumppat));

	if (maxframe ==(size_t)-1 || frame > maxframe)
		maxframe = frame;

// rotsprite
#ifdef ROTSPRITE
	for (r = 0; r < 16; r++)
	{
		sprtemp[frame].rotated[0][r] = NULL;
		sprtemp[frame].rotated[1][r] = NULL;
	}
#endif/*ROTSPRITE*/

	if (rotation == 0)
	{
		// the lump should be used for all rotations
		if (sprtemp[frame].rotate == SRF_SINGLE)
			CONS_Debug(DBG_SETUP, "R_InitSprites: Sprite %s frame %c has multiple rot = 0 lump\n", spritename, cn);
		else if (sprtemp[frame].rotate != SRF_NONE) // Let's complain for both 1-8 and L/R rotations.
			CONS_Debug(DBG_SETUP, "R_InitSprites: Sprite %s frame %c has rotations and a rot = 0 lump\n", spritename, cn);

		sprtemp[frame].rotate = SRF_SINGLE;
		for (r = 0; r < 8; r++)
		{
			sprtemp[frame].lumppat[r] = lumppat;
			sprtemp[frame].lumpid[r] = lumpid;
		}
		sprtemp[frame].flip = flipped ? UINT8_MAX : 0; // 11111111 in binary
		return;
	}

	if (rotation == ROT_L || rotation == ROT_R)
	{
		UINT8 rightfactor = ((rotation == ROT_R) ? 4 : 0);

		// the lump should be used for half of all rotations
		if (sprtemp[frame].rotate == SRF_SINGLE)
			CONS_Debug(DBG_SETUP, "R_InitSprites: Sprite %s frame %c has L/R rotations and a rot = 0 lump\n", spritename, cn);
		else if (sprtemp[frame].rotate == SRF_3D)
			CONS_Debug(DBG_SETUP, "R_InitSprites: Sprite %s frame %c has both L/R and 1-8 rotations\n", spritename, cn);
		// Let's not complain about multiple L/R rotations. It's not worth the effort.

		if (sprtemp[frame].rotate == SRF_NONE)
			sprtemp[frame].rotate = SRF_SINGLE;

		sprtemp[frame].rotate |= ((rotation == ROT_R) ? SRF_RIGHT : SRF_LEFT);

		if (sprtemp[frame].rotate == (SRF_3D|SRF_2D))
			sprtemp[frame].rotate = SRF_2D; // SRF_3D|SRF_2D being enabled at the same time doesn't HURT in the current sprite angle implementation, but it DOES mean more to check in some of the helper functions. Let's not allow this scenario to happen.

		for (r = 0; r < 4; r++) // Thanks to R_PrecacheLevel, we can't leave sprtemp[*].lumppat[*] == LUMPERROR... so we load into the front/back angle too.
		{
			sprtemp[frame].lumppat[r + rightfactor] = lumppat;
			sprtemp[frame].lumpid[r + rightfactor] = lumpid;
		}

		if (flipped)
			sprtemp[frame].flip |= (0x0F<<rightfactor); // 00001111 or 11110000 in binary, depending on rotation being ROT_L or ROT_R
		else
			sprtemp[frame].flip &= ~(0x0F<<rightfactor); // ditto

		return;
	}

	// the lump is only used for one rotation
	if (sprtemp[frame].rotate == SRF_SINGLE)
		CONS_Debug(DBG_SETUP, "R_InitSprites: Sprite %s frame %c has 1-8 rotations and a rot = 0 lump\n", spritename, cn);
	else if ((sprtemp[frame].rotate != SRF_3D) && (sprtemp[frame].rotate != SRF_NONE))
		CONS_Debug(DBG_SETUP, "R_InitSprites: Sprite %s frame %c has both L/R and 1-8 rotations\n", spritename, cn);

	// make 0 based
	rotation--;

	if (rotation == 0 || rotation == 4) // Front or back...
		sprtemp[frame].rotate = SRF_3D; // Prevent L and R changeover
	else if (rotation > 3) // Right side
		sprtemp[frame].rotate = (SRF_3D | (sprtemp[frame].rotate & SRF_LEFT)); // Continue allowing L frame changeover
	else // if (rotation <= 3) // Left side
		sprtemp[frame].rotate = (SRF_3D | (sprtemp[frame].rotate & SRF_RIGHT)); // Continue allowing R frame changeover

	if (sprtemp[frame].lumppat[rotation] != LUMPERROR)
		CONS_Debug(DBG_SETUP, "R_InitSprites: Sprite %s: %c%c has two lumps mapped to it\n", spritename, cn, '1'+rotation);

	// lumppat & lumpid are the same for original Doom, but different
	// when using sprites in pwad : the lumppat points the new graphics
	sprtemp[frame].lumppat[rotation] = lumppat;
	sprtemp[frame].lumpid[rotation] = lumpid;
	if (flipped)
		sprtemp[frame].flip |= (1<<rotation);
	else
		sprtemp[frame].flip &= ~(1<<rotation);
}

// Install a single sprite, given its identifying name (4 chars)
//
// (originally part of R_AddSpriteDefs)
//
// Pass: name of sprite : 4 chars
//       spritedef_t
//       wadnum         : wad number, indexes wadfiles[], where patches
//                        for frames are found
//       startlump      : first lump to search for sprite frames
//       endlump        : AFTER the last lump to search
//
// Returns true if the sprite was succesfully added
//
static boolean R_AddSingleSpriteDef(const char *sprname, spritedef_t *spritedef, UINT16 wadnum, UINT16 startlump, UINT16 endlump)
{
	UINT16 l;
	UINT8 frame;
	UINT8 rotation;
	lumpinfo_t *lumpinfo;
	patch_t patch;
	UINT8 numadded = 0;

	memset(sprtemp,0xFF, sizeof (sprtemp));
	maxframe = (size_t)-1;

	// are we 'patching' a sprite already loaded ?
	// if so, it might patch only certain frames, not all
	if (spritedef->numframes) // (then spriteframes is not null)
	{
		// copy the already defined sprite frames
		M_Memcpy(sprtemp, spritedef->spriteframes,
		 spritedef->numframes * sizeof (spriteframe_t));
		maxframe = spritedef->numframes - 1;
	}

	// scan the lumps,
	//  filling in the frames for whatever is found
	lumpinfo = wadfiles[wadnum]->lumpinfo;
	if (endlump > wadfiles[wadnum]->numlumps)
		endlump = wadfiles[wadnum]->numlumps;

	for (l = startlump; l < endlump; l++)
	{
		if (memcmp(lumpinfo[l].name,sprname,4)==0)
		{
			frame = R_Char2Frame(lumpinfo[l].name[4]);
			rotation = (UINT8)(lumpinfo[l].name[5] - '0');

			if (frame >= 64 || !(R_ValidSpriteAngle(rotation))) // Give an actual NAME error -_-...
			{
				CONS_Alert(CONS_WARNING, M_GetText("Bad sprite name: %s\n"), W_CheckNameForNumPwad(wadnum,l));
				continue;
			}

			// skip NULL sprites from very old dmadds pwads
			if (W_LumpLengthPwad(wadnum,l)<=8)
				continue;

			// store sprite info in lookup tables
			//FIXME : numspritelumps do not duplicate sprite replacements
			W_ReadLumpHeaderPwad(wadnum, l, &patch, sizeof (patch_t), 0);
			spritecachedinfo[numspritelumps].width = SHORT(patch.width)<<FRACBITS;
			spritecachedinfo[numspritelumps].offset = SHORT(patch.leftoffset)<<FRACBITS;
			spritecachedinfo[numspritelumps].topoffset = SHORT(patch.topoffset)<<FRACBITS;
			spritecachedinfo[numspritelumps].height = SHORT(patch.height)<<FRACBITS;

			//BP: we cannot use special tric in hardware mode because feet in ground caused by z-buffer
			if (rendermode != render_none) // not for psprite
				spritecachedinfo[numspritelumps].topoffset += 4<<FRACBITS;

			// Being selective with this causes bad things. :( Like the special stage tokens breaking apart.
			/*if (rendermode != render_none // not for psprite
			 && SHORT(patch.topoffset)>0 && SHORT(patch.topoffset)<SHORT(patch.height))
				// perfect is patch.height but sometime it is too high
				spritecachedinfo[numspritelumps].topoffset = min(SHORT(patch.topoffset)+4,SHORT(patch.height))<<FRACBITS;*/

			//----------------------------------------------------

			R_InstallSpriteLump(wadnum, l, numspritelumps, frame, rotation, 0);

			if (lumpinfo[l].name[6])
			{
				frame = R_Char2Frame(lumpinfo[l].name[6]);
				rotation = (UINT8)(lumpinfo[l].name[7] - '0');
				R_InstallSpriteLump(wadnum, l, numspritelumps, frame, rotation, 1);
			}

			if (++numspritelumps >= max_spritelumps)
			{
				max_spritelumps *= 2;
				Z_Realloc(spritecachedinfo, max_spritelumps*sizeof(*spritecachedinfo), PU_STATIC, &spritecachedinfo);
			}

			++numadded;
		}
	}

	//
	// if no frames found for this sprite
	//
	if (maxframe == (size_t)-1)
	{
		// the first time (which is for the original wad),
		// all sprites should have their initial frames
		// and then, patch wads can replace it
		// we will skip non-replaced sprite frames, only if
		// they have already have been initially defined (original wad)

		//check only after all initial pwads added
		//if (spritedef->numframes == 0)
		//    I_Error("R_AddSpriteDefs: no initial frames found for sprite %s\n",
		//             namelist[i]);

		// sprite already has frames, and is not replaced by this wad
		return false;
	}
	else if (!numadded)
	{
		// Nothing related to this spritedef has been changed
		// so there is no point going back through these checks again.
		return false;
	}

	maxframe++;

	//
	//  some checks to help development
	//
	for (frame = 0; frame < maxframe; frame++)
	{
		switch (sprtemp[frame].rotate)
		{
			case SRF_NONE:
			// no rotations were found for that frame at all
			I_Error("R_AddSingleSpriteDef: No patches found for %.4s frame %c", sprname, R_Frame2Char(frame));
			break;

			case SRF_SINGLE:
			// only the first rotation is needed
			break;

			case SRF_2D: // both Left and Right rotations
				// we test to see whether the left and right slots are present
				if ((sprtemp[frame].lumppat[2] == LUMPERROR) || (sprtemp[frame].lumppat[6] == LUMPERROR))
					I_Error("R_AddSingleSpriteDef: Sprite %s frame %c is missing rotations",
					        sprname, R_Frame2Char(frame));
			break;

			default:
			// must have all 8 frames
			for (rotation = 0; rotation < 8; rotation++)
				// we test the patch lump, or the id lump whatever
				// if it was not loaded the two are LUMPERROR
				if (sprtemp[frame].lumppat[rotation] == LUMPERROR)
					I_Error("R_AddSingleSpriteDef: Sprite %.4s frame %c is missing rotations",
					        sprname, R_Frame2Char(frame));
			break;
		}
	}

	// allocate space for the frames present and copy sprtemp to it
	if (spritedef->numframes &&             // has been allocated
		spritedef->numframes < maxframe)   // more frames are defined ?
	{

		Z_Free(spritedef->spriteframes);
		spritedef->spriteframes = NULL;
	}

	// allocate this sprite's frames
	if (!spritedef->spriteframes)
		spritedef->spriteframes =
		 Z_Malloc(maxframe * sizeof (*spritedef->spriteframes), PU_STATIC, NULL);

	spritedef->numframes = maxframe;
	M_Memcpy(spritedef->spriteframes, sprtemp, maxframe*sizeof (spriteframe_t));

	return true;
}

//
// Search for sprites replacements in a wad whose names are in namelist
//
void R_AddSpriteDefs(UINT16 wadnum)
{
	size_t i, addsprites = 0;
	UINT16 start, end;
	char wadname[MAX_WADPATH];

	switch (wadfiles[wadnum]->type)
	{
	case RET_WAD:
		start = W_CheckNumForNamePwad("S_START", wadnum, 0);
		if (start == INT16_MAX)
			start = W_CheckNumForNamePwad("SS_START", wadnum, 0); //deutex compatib.
		if (start == INT16_MAX)
			start = 0; //let say S_START is lump 0
		else
			start++;   // just after S_START
		end = W_CheckNumForNamePwad("S_END",wadnum,start);
		if (end == INT16_MAX)
			end = W_CheckNumForNamePwad("SS_END",wadnum,start);     //deutex compatib.
		break;
	case RET_PK3:
		start = W_CheckNumForFolderStartPK3("Sprites/", wadnum, 0);
		end = W_CheckNumForFolderEndPK3("Sprites/", wadnum, start);
		break;
	default:
		return;
	}

	if (end == INT16_MAX)
	{
		CONS_Debug(DBG_SETUP, "no sprites in pwad %d\n", wadnum);
		return;
	}

	//
	// scan through lumps, for each sprite, find all the sprite frames
	//
	for (i = 0; i < numsprites; i++)
	{
		spritename = sprnames[i];
		if (spritename[4] && wadnum >= (UINT16)spritename[4])
			continue;

		if (R_AddSingleSpriteDef(spritename, &sprites[i], wadnum, start, end))
		{
#ifdef HWRENDER
			if (rendermode == render_opengl)
				HWR_AddSpriteMD2(i);
#endif
			// if a new sprite was added (not just replaced)
			addsprites++;
#ifndef ZDEBUG
			CONS_Debug(DBG_SETUP, "sprite %s set in pwad %d\n", spritename, wadnum);
#endif
		}
	}

	nameonly(strcpy(wadname, wadfiles[wadnum]->filename));
	CONS_Printf(M_GetText("%s added %d frames in %s sprites\n"), wadname, end-start, sizeu1(addsprites));
}


//
// GAME FUNCTIONS
//
UINT32 visspritecount, numvisiblesprites;

static UINT32 clippedvissprites;
static vissprite_t *visspritechunks[MAXVISSPRITES >> VISSPRITECHUNKBITS] = {NULL};


//
// R_InitSprites
// Called at program start.
//
void R_InitSprites(void)
{
	size_t i;
#ifdef ROTSPRITE
	INT32 angle;
	float fa;
#endif

	for (i = 0; i < MAXVIDWIDTH; i++)
	{
		negonearray[i] = -1;
	}

#ifdef ROTSPRITE
	for (angle = 1; angle < ROTANGLES; angle++)
	{
		fa = ANG2RAD(FixedAngle((ROTANGDIFF * angle)<<FRACBITS));
		rollcosang[angle] = FLOAT_TO_FIXED(cos(-fa));
		rollsinang[angle] = FLOAT_TO_FIXED(sin(-fa));
	}
#endif

	//
	// count the number of sprite names, and allocate sprites table
	//
	numsprites = 0;
	for (i = 0; i < NUMSPRITES + 1; i++)
		if (sprnames[i][0] != '\0') numsprites++;

	if (!numsprites)
		I_Error("R_AddSpriteDefs: no sprites in namelist\n");

	sprites = Z_Calloc(numsprites * sizeof (*sprites), PU_STATIC, NULL);

	// find sprites in each -file added pwad
	for (i = 0; i < numwadfiles; i++)
		R_AddSpriteDefs((UINT16)i);

	//
	// now check for skins
	//

	// it can be is do before loading config for skin cvar possible value
	R_InitSkins();
	for (i = 0; i < numwadfiles; i++) 
	{
		R_AddSkins((UINT16)i, false);
		R_LoadSpriteInfoLumps(i, wadfiles[i]->numlumps);
	}
		
	//
	// check if all sprites have frames
	//
	/*
	for (i = 0; i < numsprites; i++)
		if (sprites[i].numframes < 1)
			CONS_Debug(DBG_SETUP, "R_InitSprites: sprite %s has no frames at all\n", sprnames[i]);
	*/
}

//
// R_ClearSprites
// Called at frame start.
//
void R_ClearSprites(void)
{
	visspritecount = numvisiblesprites = clippedvissprites = 0;
}

//
// R_NewVisSprite
//
static vissprite_t overflowsprite;

static vissprite_t *R_GetVisSprite(UINT32 num)
{
		UINT32 chunk = num >> VISSPRITECHUNKBITS;

		// Allocate chunk if necessary
		if (!visspritechunks[chunk])
			Z_Malloc(sizeof(vissprite_t) * VISSPRITESPERCHUNK, PU_LEVEL, &visspritechunks[chunk]);

		return visspritechunks[chunk] + (num & VISSPRITEINDEXMASK);
}

static vissprite_t *R_NewVisSprite(void)
{
	if (visspritecount == MAXVISSPRITES)
		return &overflowsprite;

	return R_GetVisSprite(visspritecount++);
}

//
// R_DrawMaskedColumn
// Used for sprites and masked mid textures.
// Masked means: partly transparent, i.e. stored
//  in posts/runs of opaque pixels.
//
INT16 *mfloorclip;
INT16 *mceilingclip;

fixed_t spryscale = 0, sprtopscreen = 0, sprbotscreen = 0;
fixed_t windowtop = 0, windowbottom = 0;

void R_DrawMaskedColumn(column_t *column)
{
	INT32 topscreen;
	INT32 bottomscreen;
	fixed_t basetexturemid;
	INT32 topdelta, prevdelta = 0;

	basetexturemid = dc_texturemid;

	for (; column->topdelta != 0xff ;)
	{
		// calculate unclipped screen coordinates
		// for post
		topdelta = column->topdelta;
		if (topdelta <= prevdelta)
			topdelta += prevdelta;
		prevdelta = topdelta;
		topscreen = sprtopscreen + spryscale*topdelta;
		bottomscreen = topscreen + spryscale*column->length;

		dc_yl = (topscreen+FRACUNIT-1)>>FRACBITS;
		dc_yh = (bottomscreen-1)>>FRACBITS;

		if (windowtop != INT32_MAX && windowbottom != INT32_MAX)
		{
			if (windowtop > topscreen)
				dc_yl = (windowtop + FRACUNIT - 1)>>FRACBITS;
			if (windowbottom < bottomscreen)
				dc_yh = (windowbottom - 1)>>FRACBITS;
		}

		if (dc_yh >= mfloorclip[dc_x])
			dc_yh = mfloorclip[dc_x]-1;
		if (dc_yl <= mceilingclip[dc_x])
			dc_yl = mceilingclip[dc_x]+1;
		if (dc_yl < 0)
			dc_yl = 0;
		if (dc_yh >= vid.height)
			dc_yh = vid.height - 1;

		if (dc_yl <= dc_yh && dc_yl < vid.height && dc_yh > 0)
		{
			dc_source = (UINT8 *)column + 3;
			dc_texturemid = basetexturemid - (topdelta<<FRACBITS);

			// Drawn by R_DrawColumn.
			// This stuff is a likely cause of the splitscreen water crash bug.
			// FIXTHIS: Figure out what "something more proper" is and do it.
			// quick fix... something more proper should be done!!!
			if (ylookup[dc_yl])
				colfunc();
		}
		column = (column_t *)((UINT8 *)column + column->length + 4);
	}

	dc_texturemid = basetexturemid;
}

static void R_DrawFlippedMaskedColumn(column_t *column, INT32 texheight)
{
	INT32 topscreen;
	INT32 bottomscreen;
	fixed_t basetexturemid = dc_texturemid;
	INT32 topdelta, prevdelta = -1;
	UINT8 *d,*s;

	for (; column->topdelta != 0xff ;)
	{
		// calculate unclipped screen coordinates
		// for post
		topdelta = column->topdelta;
		if (topdelta <= prevdelta)
			topdelta += prevdelta;
		prevdelta = topdelta;
		topdelta = texheight-column->length-topdelta;
		topscreen = sprtopscreen + spryscale*topdelta;
		bottomscreen = sprbotscreen == INT32_MAX ? topscreen + spryscale*column->length
		                                      : sprbotscreen + spryscale*column->length;

		dc_yl = (topscreen+FRACUNIT-1)>>FRACBITS;
		dc_yh = (bottomscreen-1)>>FRACBITS;

		if (windowtop != INT32_MAX && windowbottom != INT32_MAX)
		{
			if (windowtop > topscreen)
				dc_yl = (windowtop + FRACUNIT - 1)>>FRACBITS;
			if (windowbottom < bottomscreen)
				dc_yh = (windowbottom - 1)>>FRACBITS;
		}

		if (dc_yh >= mfloorclip[dc_x])
			dc_yh = mfloorclip[dc_x]-1;
		if (dc_yl <= mceilingclip[dc_x])
			dc_yl = mceilingclip[dc_x]+1;
		if (dc_yl < 0)
			dc_yl = 0;
		if (dc_yh >= vid.height)
			dc_yh = vid.height - 1;

		if (dc_yl <= dc_yh && dc_yl < vid.height && dc_yh > 0)
		{
			dc_source = ZZ_Alloc(column->length);
			for (s = (UINT8 *)column+2+column->length, d = dc_source; d < dc_source+column->length; --s)
				*d++ = *s;
			dc_texturemid = basetexturemid - (topdelta<<FRACBITS);

			// Still drawn by R_DrawColumn.
			if (ylookup[dc_yl])
				colfunc();
			Z_Free(dc_source);
		}
		column = (column_t *)((UINT8 *)column + column->length + 4);
	}

	dc_texturemid = basetexturemid;
}

//
// R_DrawVisSprite
//  mfloorclip and mceilingclip should also be set.
//
static void R_DrawVisSprite(vissprite_t *vis)
{
	column_t *column;
	INT32 texturecolumn;
	fixed_t frac;
	patch_t *patch = vis->patch;
	fixed_t this_scale = vis->thingscale;
	INT32 x1, x2;
	INT64 overflow_test;

	if (!patch)
		return;

	// Check for overflow
	overflow_test = (INT64)centeryfrac - (((INT64)vis->texturemid*vis->scale)>>FRACBITS);
	if (overflow_test < 0) overflow_test = -overflow_test;
	if ((UINT64)overflow_test&0xFFFFFFFF80000000ULL) return; // fixed point mult would overflow

	if (vis->scalestep) // handles right edge too
	{
		overflow_test = (INT64)centeryfrac - (((INT64)vis->texturemid*(vis->scale + (vis->scalestep*(vis->x2 - vis->x1))))>>FRACBITS);
		if (overflow_test < 0) overflow_test = -overflow_test;
		if ((UINT64)overflow_test&0xFFFFFFFF80000000ULL) return; // ditto
	}
	
	// TODO This check should not be necessary. But Papersprites near to the camera will sometimes create invalid values
	// for the vissprite's startfrac. This happens because they are not depth culled like other sprites.
	// Someone who is more familiar with papersprites pls check and try to fix <3
	if (vis->startfrac < 0 || vis->startfrac > (SHORT(patch->width) << FRACBITS))
	{
		// never draw vissprites with startfrac out of patch range
		return;
	}

	colfunc = basecolfunc; // hack: this isn't resetting properly somewhere.
	dc_colormap = vis->colormap;

	// Hack: Use a special column function for drop shadows that bypasses
	// invalid memory access crashes caused by R_ProjectDropShadow putting wrong values
	// in dc_texturemid and dc_iscale when the shadow is sloped.
	if (vis->cut & SC_SHADOW)
	{
		colfunc = dropshadowcolfunc;
		dc_transmap = vis->transmap;
	}
	else if ((vis->mobj->flags & MF_BOSS) && (vis->mobj->flags2 & MF2_FRET) && (leveltime & 1)) // Bosses "flash"
	{
		// translate certain pixels to white
		colfunc = transcolfunc;
		if (vis->mobj->type == MT_CYBRAKDEMON)
			dc_translation = R_GetTranslationColormap(TC_ALLWHITE, 0, GTC_CACHE);
		else if (vis->mobj->type == MT_METALSONIC_BATTLE)
			dc_translation = R_GetTranslationColormap(TC_METALSONIC, 0, GTC_CACHE);
		else
			dc_translation = R_GetTranslationColormap(TC_BOSS, 0, GTC_CACHE);
	}
	else if (vis->mobj->color && vis->transmap) // Color mapping
	{
		colfunc = transtransfunc;
		dc_transmap = vis->transmap;
		if (vis->mobj->colorized)
			dc_translation = R_GetTranslationColormap(TC_RAINBOW, vis->mobj->color, GTC_CACHE);
		else if (vis->mobj->skin && vis->mobj->sprite == SPR_PLAY) // MT_GHOST LOOKS LIKE A PLAYER SO USE THE PLAYER TRANSLATION TABLES. >_>
			dc_translation = R_GetLocalTranslationColormap(vis->mobj->skin, vis->mobj->localskin, vis->mobj->color, GTC_CACHE, vis->mobj->skinlocal);
		else // Use the defaults
			dc_translation = R_GetTranslationColormap(TC_DEFAULT, vis->mobj->color, GTC_CACHE);
	}
	else if (vis->transmap)
	{
		colfunc = fuzzcolfunc;
		dc_transmap = vis->transmap;    //Fab : 29-04-98: translucency table
	}
	else if (vis->mobj->color)
	{
		// translate green skin to another color
		colfunc = transcolfunc;

		// New colormap stuff for skins Tails 06-07-2002
		if (vis->mobj->colorized)
			dc_translation = R_GetTranslationColormap(TC_RAINBOW, vis->mobj->color, GTC_CACHE);
		else if (vis->mobj->skin && vis->mobj->sprite == SPR_PLAY) // This thing is a player!
			dc_translation = R_GetLocalTranslationColormap(vis->mobj->skin, vis->mobj->localskin, vis->mobj->color, GTC_CACHE, vis->mobj->skinlocal);
		else // Use the defaults
			dc_translation = R_GetTranslationColormap(TC_DEFAULT, vis->mobj->color, GTC_CACHE);
	}
	else if (vis->mobj->sprite == SPR_PLAY) // Looks like a player, but doesn't have a color? Get rid of green sonic syndrome.
	{
		colfunc = transcolfunc;
		dc_translation = R_GetTranslationColormap(TC_DEFAULT, SKINCOLOR_BLUE, GTC_CACHE);
	}

	if (vis->extra_colormap)
	{
		if (!dc_colormap)
			dc_colormap = vis->extra_colormap->colormap;
		else
			dc_colormap = &vis->extra_colormap->colormap[dc_colormap - colormaps];
	}
	if (!dc_colormap)
		dc_colormap = colormaps;

	if (encoremap && !vis->mobj->color && !(vis->mobj->flags & MF_DONTENCOREMAP))
			dc_colormap += COLORMAP_REMAPOFFSET;

	dc_texturemid = vis->texturemid;
	dc_texheight = 0;

	frac = vis->startfrac;
	windowtop = windowbottom = sprbotscreen = INT32_MAX;

	if (vis->mobj->localskin && ((skin_t *)vis->mobj->localskin)->flags & SF_HIRES)
		this_scale = FixedMul(this_scale, ((skin_t *)vis->mobj->localskin)->highresscale);
	else if (vis->mobj->skin && ((skin_t *)vis->mobj->skin)->flags & SF_HIRES)
		this_scale = FixedMul(this_scale, ((skin_t *)vis->mobj->skin)->highresscale);

	if (this_scale <= 0)
		this_scale = 1;

	if (this_scale != FRACUNIT)
	{
		if (!(vis->cut & SC_ISSCALED))
		{
			vis->scale = FixedMul(vis->scale, this_scale);
			vis->scalestep = FixedMul(vis->scalestep, this_scale);
			vis->xiscale = FixedDiv(vis->xiscale,this_scale);
			vis->cut |= SC_ISSCALED;
		}
		dc_texturemid = FixedDiv(dc_texturemid, this_scale);
	}

	spryscale = vis->scale;

	if (!(vis->scalestep))
	{
		sprtopscreen = centeryfrac - FixedMul(dc_texturemid, spryscale);
		sprtopscreen += vis->shear.tan * vis->shear.offset;
		dc_iscale = FixedDiv(FRACUNIT, vis->scale);
	}

	x1 = vis->x1;
	x2 = vis->x2;

	if (vis->x1 < 0)
	{
		spryscale += vis->scalestep*(-vis->x1);
		vis->x1 = 0;
	}

	if (vis->x2 >= vid.width)
		vis->x2 = vid.width-1;

	// Something is occasionally setting 1px-wide sprites whose frac is exactly the width of the sprite, causing crashes due to
	// accessing invalid column info. Until the cause is found, let's try to correct those manually...
	{
		fixed_t temp = ((frac + vis->xiscale*(vis->x2-vis->x1))>>FRACBITS) - SHORT(patch->width);
		if (temp > 0)
			vis->x2 -= temp;
	}

	// Split drawing loops for paper and non-paper to reduce conditional checks per sprite
	if (vis->scalestep)
	{
		fixed_t horzscale = FixedMul(vis->spritexscale, this_scale);
		fixed_t scalestep = FixedMul(vis->scalestep, vis->spriteyscale);

		// Papersprite drawing loop
		for (dc_x = vis->x1; dc_x <= vis->x2; dc_x++, spryscale += scalestep)
		{
			angle_t angle = ((vis->centerangle + xtoviewangle[dc_x]) >> ANGLETOFINESHIFT) & 0xFFF;
			texturecolumn = (vis->paperoffset - FixedMul(FINETANGENT(angle), vis->paperdistance)) / horzscale;

			if (texturecolumn < 0 || texturecolumn >= SHORT(patch->width))
				continue;

			if (vis->xiscale < 0) // Flipped sprite
				texturecolumn = SHORT(patch->width) - 1 - texturecolumn;

			sprtopscreen = (centeryfrac - FixedMul(dc_texturemid, spryscale));
			dc_iscale = (0xffffffffu / (unsigned)spryscale);

			column = (column_t *)((UINT8 *)patch + LONG(patch->columnofs[texturecolumn]));

			if (vis->vflip)
				R_DrawFlippedMaskedColumn(column, patch->height);
			else
				R_DrawMaskedColumn(column);
		}
	}
	else
	{
		// Non-paper drawing loop
		for (dc_x = vis->x1; dc_x <= vis->x2; dc_x++, frac += vis->xiscale)
		{	
#define CLAMP(x, min_val, max_val) ((x) < (min_val) ? (min_val) : ((x) > (max_val) ? (max_val) : (x)))
			texturecolumn = CLAMP(frac >> FRACBITS, 0, SHORT(patch->width) - 1);
#undef CLAMP
			column = (column_t *)((UINT8 *)patch + LONG(patch->columnofs[texturecolumn]));

			if (vis->vflip)
				R_DrawFlippedMaskedColumn(column, patch->height);
			else
				R_DrawMaskedColumn(column);
		}
	}

	colfunc = basecolfunc;
	dc_hires = 0;

	vis->x1 = x1;
	vis->x2 = x2;
}

// Special precipitation drawer Tails 08-18-2002
static void R_DrawPrecipitationVisSprite(vissprite_t *vis)
{
	column_t *column;
	INT32 texturecolumn;
	fixed_t frac;
	patch_t *patch;
	INT64 overflow_test;

	//Fab : R_InitSprites now sets a wad lump number
	patch = vis->patch;
	if (!patch)
		return;

	// Check for overflow
	overflow_test = (INT64)centeryfrac - (((INT64)vis->texturemid*vis->scale)>>FRACBITS);
	if (overflow_test < 0) overflow_test = -overflow_test;
	if ((UINT64)overflow_test&0xFFFFFFFF80000000ULL) return; // fixed point mult would overflow

	if (vis->transmap)
	{
		colfunc = fuzzcolfunc;
		dc_transmap = vis->transmap;    //Fab : 29-04-98: translucency table
	}

	dc_colormap = colormaps;
	if (encoremap)
		dc_colormap += COLORMAP_REMAPOFFSET;

	dc_iscale = FixedDiv(FRACUNIT, vis->scale);
	dc_texturemid = vis->texturemid;
	dc_texheight = 0;

	frac = vis->startfrac;
	spryscale = vis->scale;
	sprtopscreen = centeryfrac - FixedMul(dc_texturemid,spryscale);
	windowtop = windowbottom = sprbotscreen = INT32_MAX;

	if (vis->x1 < 0)
		vis->x1 = 0;

	if (vis->x2 >= vid.width)
		vis->x2 = vid.width-1;

#define CLAMP(x, min_val, max_val) ((x) < (min_val) ? (min_val) : ((x) > (max_val) ? (max_val) : (x)))
	for (dc_x = vis->x1; dc_x <= vis->x2; dc_x++, frac += vis->xiscale)
	{
		texturecolumn = CLAMP(frac >> FRACBITS, 0, SHORT(patch->width) - 1);
		column = (column_t *)((UINT8 *)patch + LONG(patch->columnofs[texturecolumn]));

		R_DrawMaskedColumn(column);
	}
#undef CLAMP

	colfunc = basecolfunc;
}

//
// R_SplitSprite
// runs through a sector's lightlist and
static void R_SplitSprite(vissprite_t *sprite, mobj_t *thing)
{
	INT32 i, lightnum, lindex;
	INT16 cutfrac;
	sector_t *sector;
	vissprite_t *newsprite;

	sector = sprite->sector;

	for (i = 1; i < sector->numlights; i++)
	{
		fixed_t testheight = sector->lightlist[i].height;

		if (!(sector->lightlist[i].caster->flags & FF_CUTSPRITES))
			continue;

		if (sector->lightlist[i].slope)
			testheight = P_GetZAt(sector->lightlist[i].slope, sprite->gx, sprite->gy);

		if (testheight >= sprite->gzt)
			continue;
		if (testheight <= sprite->gz)
			return;

		cutfrac = (INT16)((centeryfrac - FixedMul(testheight - viewz, sprite->sortscale))>>FRACBITS);
		if (cutfrac < 0)
			continue;
		if (cutfrac > viewheight)
			return;

		// Found a split! Make a new sprite, copy the old sprite to it, and
		// adjust the heights.
		newsprite = M_Memcpy(R_NewVisSprite(), sprite, sizeof (vissprite_t));

		sprite->cut |= SC_BOTTOM;
		sprite->gz = testheight;

		newsprite->gzt = sprite->gz;

		sprite->sz = cutfrac;
		newsprite->szt = (INT16)(sprite->sz - 1);

		if (testheight < sprite->pzt && testheight > sprite->pz)
			sprite->pz = newsprite->pzt = testheight;
		else
		{
			newsprite->pz = newsprite->gz;
			newsprite->pzt = newsprite->gzt;
		}

		newsprite->szt -= 8;

		newsprite->cut |= SC_TOP;
		if (!(sector->lightlist[i].caster->flags & FF_NOSHADE))
		{
			lightnum = (*sector->lightlist[i].lightlevel >> LIGHTSEGSHIFT);

			if (lightnum < 0)
				spritelights = scalelight[0];
			else if (lightnum >= LIGHTLEVELS)
				spritelights = scalelight[LIGHTLEVELS-1];
			else
				spritelights = scalelight[lightnum];

			newsprite->extra_colormap = sector->lightlist[i].extra_colormap;

/*
			if (thing->frame & FF_TRANSMASK)
				;
			else if (thing->flags2 & MF2_SHADOW)
				;
			else
*/
			if (!((thing->frame & (FF_FULLBRIGHT|FF_TRANSMASK) || thing->flags2 & MF2_SHADOW)
				&& (!newsprite->extra_colormap || !(newsprite->extra_colormap->fog & 1))))
			{
				lindex = FixedMul(sprite->xscale, LIGHTRESOLUTIONFIX)>>(LIGHTSCALESHIFT);

				if (lindex >= MAXLIGHTSCALE)
					lindex = MAXLIGHTSCALE-1;
				newsprite->colormap = spritelights[lindex];
			}
		}
		sprite = newsprite;
	}
}

//
// R_GetShadowZ(thing, shadowslope)
// Get the first visible floor below the object for shadows
// shadowslope is filled with the floor's slope, if provided
//
fixed_t R_GetShadowZ(
	mobj_t *thing, pslope_t **shadowslope)
{
	fixed_t halfHeight;
	boolean isflipped = thing->eflags & MFE_VERTICALFLIP;
	fixed_t floorz;
	fixed_t ceilingz;
	fixed_t z, groundz = isflipped ? INT32_MAX : INT32_MIN;
	pslope_t *slope, *groundslope = NULL;
	msecnode_t *node;
	sector_t *sector;
	ffloor_t *rover;

	// for frame interpolation
	interpmobjstate_t interp = {0};

	if (R_UsingFrameInterpolation() && !paused)
	{
		R_InterpolateMobjState(thing, rendertimefrac, &interp);
	}
	else
	{
		R_InterpolateMobjState(thing, FRACUNIT, &interp);
	}

	halfHeight = interp.z + (thing->height >> 1);
	floorz = P_GetFloorZ(thing, interp.subsector->sector, interp.x, interp.y, NULL);
	ceilingz = P_GetCeilingZ(thing, interp.subsector->sector, interp.x, interp.y, NULL);

#define CHECKZ (isflipped ? z > halfHeight && z < groundz : z < halfHeight && z > groundz)

	for (node = thing->touching_sectorlist; node; node = node->m_sectorlist_next)
	{
		sector = node->m_sector;

		slope = sector->heightsec != -1 ? NULL : (isflipped ? sector->c_slope : sector->f_slope);

		if (sector->heightsec != -1)
			z = isflipped ? sectors[sector->heightsec].ceilingheight : sectors[sector->heightsec].floorheight;
		else
			z = isflipped ? P_GetSectorCeilingZAt(sector, interp.x, interp.y) : P_GetSectorFloorZAt(sector, interp.x, interp.y);

		if CHECKZ
		{
			groundz = z;
			groundslope = slope;
		}

		if (sector->ffloors)
			for (rover = sector->ffloors; rover; rover = rover->next)
			{
				if (!(rover->flags & FF_EXISTS) || !(rover->flags & FF_RENDERPLANES) || (rover->alpha < 90 && !(rover->flags & FF_SWIMMABLE)))
					continue;

				z = isflipped ? P_GetFFloorBottomZAt(rover, interp.x, interp.y) : P_GetFFloorTopZAt(rover, interp.x, interp.y);
				if CHECKZ
				{
					groundz = z;
					groundslope = isflipped ? *rover->b_slope : *rover->t_slope;
				}
			}
	}

	if (isflipped ? (ceilingz < groundz - (!groundslope ? 0 : FixedMul(abs(groundslope->zdelta), thing->radius*3/2)))
		: (floorz > groundz + (!groundslope ? 0 : FixedMul(abs(groundslope->zdelta), thing->radius*3/2))))
	{
		groundz = isflipped ? ceilingz : floorz;
		groundslope = NULL;
	}

	if (shadowslope != NULL)
		*shadowslope = groundslope;

#undef CHECKZ

	return groundz;
}

static void R_SkewShadowSprite(
			mobj_t *thing, pslope_t *groundslope,
			fixed_t groundz, INT32 spriteheight, fixed_t scalemul,
			fixed_t *shadowyscale, fixed_t *shadowskew)
{

	// haha let's try some dumb stuff
	fixed_t xslope, zslope;
	angle_t sloperelang;

	// for frame interpolation
	interpmobjstate_t interp = {0};

	if (R_UsingFrameInterpolation() && !paused)
	{
		R_InterpolateMobjState(thing, rendertimefrac, &interp);
	}
	else
	{
		R_InterpolateMobjState(thing, FRACUNIT, &interp);
	}

	sloperelang = (R_PointToAngle(interp.x, interp.y) - groundslope->xydirection) >> ANGLETOFINESHIFT;

	xslope = FixedMul(FINESINE(sloperelang), groundslope->zdelta);
	zslope = FixedMul(FINECOSINE(sloperelang), groundslope->zdelta);

	//CONS_Printf("Shadow is sloped by %d %d\n", xslope, zslope);

	if (viewz < groundz)
		*shadowyscale += FixedMul(FixedMul(thing->radius*2 / spriteheight, scalemul), zslope);
	else
		*shadowyscale -= FixedMul(FixedMul(thing->radius*2 / spriteheight, scalemul), zslope);

	*shadowyscale = abs((*shadowyscale));
	*shadowskew = xslope;
}

static void R_ProjectDropShadow(mobj_t *thing, vissprite_t *vis, fixed_t tx, fixed_t tz)
{
	vissprite_t *shadow;
	patch_t *patch;
	fixed_t xscale, yscale, shadowxscale, shadowyscale, shadowskew, x1, x2;
	fixed_t scalemul;
	fixed_t floordiff;
	fixed_t groundz;
	pslope_t *groundslope;
	boolean isflipped = thing->eflags & MFE_VERTICALFLIP;
	fixed_t scale = FixedDiv(4*thing->scale/3, mapobjectscale);
	interpmobjstate_t interp = {0};

	groundz = R_GetShadowZ(thing, &groundslope);

	if (abs(groundz-viewz)/tz > 4) return; // Prevent stretchy shadows and possible crashes

	if (R_UsingFrameInterpolation() && !paused)
	{
		R_InterpolateMobjState(thing, rendertimefrac, &interp);
	}
	else
	{
		R_InterpolateMobjState(thing, FRACUNIT, &interp);
	}

	floordiff = abs((isflipped ? thing->height : 0) + interp.z - groundz);

	scalemul = FixedMul(FRACUNIT - floordiff/640, scale);

	patch = W_CachePatchNum(sprites[SPR_SHAD].spriteframes[0].lumppat[0], PU_CACHE);
	xscale = FixedDiv(projection, tz);
	yscale = FixedDiv(projectiony, tz);
	shadowxscale = FixedMul(thing->radius*2, scalemul);
	shadowyscale = FixedMul(FixedMul(thing->radius*2, scalemul), FixedDiv(abs(groundz - viewz), tz));
	shadowyscale = min(shadowyscale, shadowxscale) / SHORT(patch->height);
	shadowxscale /= SHORT(patch->width);
	shadowskew = 0;
	
	//if (groundslope)
		//R_SkewShadowSprite(thing, groundslope, groundz, SHORT(patch->height), scalemul, &shadowyscale, &shadowskew); // idk whats up with this thing

	tx -= SHORT(patch->width) * shadowxscale/2;
	x1 = (centerxfrac + FixedMul(tx,xscale))>>FRACBITS;
	if (x1 >= viewwidth) return;

	tx += SHORT(patch->width) * shadowxscale;
	x2 = ((centerxfrac + FixedMul(tx,xscale))>>FRACBITS); x2--;
	if (x2 < 0 || x2 <= x1) return;

	if (shadowyscale < FRACUNIT/SHORT(patch->height)) return; // fix some crashes?

	shadow = R_NewVisSprite();
	shadow->patch = patch;
	shadow->heightsec = vis->heightsec;

	shadow->thingheight = FRACUNIT;
	shadow->pz = groundz + (isflipped ? -shadow->thingheight : 0);
	shadow->pzt = shadow->pz + shadow->thingheight;

	shadow->mobjflags = 0;
	shadow->sortscale = vis->sortscale;
	shadow->dispoffset = vis->dispoffset - 5;
	shadow->gx = interp.x;
	shadow->gy = interp.y;
	shadow->gzt = (isflipped ? shadow->pzt : shadow->pz) + SHORT(patch->height) * shadowyscale / 2;
	shadow->gz = shadow->gzt - SHORT(patch->height) * shadowyscale;
	shadow->texturemid = FixedMul(interp.scale, FixedDiv(shadow->gzt - viewz, shadowyscale));
	if (thing->skin && ((skin_t *)thing->skin)->flags & SF_HIRES)
		shadow->texturemid = FixedMul(shadow->texturemid, ((skin_t *)thing->skin)->highresscale);
	shadow->scalestep = 0;
	shadow->shear.tan = shadowskew; // repurposed variable

	shadow->mobj = thing; // Easy access! Tails 06-07-2002

	shadow->x1 = x1 < 0 ? 0 : x1;
	shadow->x2 = x2 >= viewwidth ? viewwidth-1 : x2;

	// PORTAL SEMI-CLIPPING
	if (portalrender && portalclipline)
	{
		if (shadow->x1 < portalclipstart)
			shadow->x1 = portalclipstart;
		if (shadow->x2 >= portalclipend)
			shadow->x2 = portalclipend-1;
	}

	shadow->xscale = FixedMul(xscale, shadowxscale); //SoM: 4/17/2000
	shadow->scale = FixedMul(yscale, shadowyscale);
	shadow->thingscale = interp.scale;
	shadow->sector = vis->sector;
	shadow->szt = (INT16)((centeryfrac - FixedMul(shadow->gzt - viewz, yscale))>>FRACBITS);
	shadow->sz = (INT16)((centeryfrac - FixedMul(shadow->gz - viewz, yscale))>>FRACBITS);
	shadow->cut = SC_ISSCALED|SC_SHADOW; //check this

	shadow->startfrac = 0;
	//shadow->xiscale = 0x7ffffff0 / (shadow->xscale/2);
	shadow->xiscale = (SHORT(patch->width)<<FRACBITS)/(x2-x1+1); // fuck it

	if (shadow->x1 > x1)
		shadow->startfrac += shadow->xiscale*(shadow->x1-x1);

	// reusing x1 variable
	x1 += (x2-x1)/2;
	shadow->shear.offset = shadow->x1-x1;

	shadow->extra_colormap = NULL;

	shadow->transmap = transtables + (tr_trans30<<FF_TRANSSHIFT);

	if (thing->whiteshadow)
		shadow->colormap = scalelight[LIGHTLEVELS - 1][0]; // full bright!
	else
		shadow->colormap = scalelight[0][0]; // full dark!

	objectsdrawn++;
}

//
// R_ProjectSprite
// Generates a vissprite for a thing
// if it might be visible.
//
static void R_ProjectSprite(mobj_t *thing)
{
	mobj_t *oldthing = thing;

	fixed_t tr_x, tr_y;
	fixed_t gxt, gyt;
	fixed_t tx, tz;
	fixed_t xscale, yscale, sortscale; //added : 02-02-98 : aaargll..if I were a math-guy!!!

	INT32 x1, x2;

	spritedef_t *sprdef;
	spriteframe_t *sprframe;
#ifdef ROTSPRITE
	spriteinfo_t *sprinfo;
#endif
	size_t lump;

	size_t rot;
	UINT8 flip;

	boolean mirrored = thing->mirrored;
	boolean hflip = (!(thing->frame & FF_HORIZONTALFLIP) != !mirrored);

	INT32 lindex;

	vissprite_t *vis;

	angle_t ang = 0; // gcc 4.6 and lower fix
	angle_t camang = 0;
	fixed_t iscale;
	fixed_t scalestep; // toast '16
	fixed_t offset, offset2;

	fixed_t basetx, basetz; // drop shadows

	boolean papersprite = !!(thing->frame & FF_PAPERSPRITE);
	fixed_t paperoffset = 0, paperdistance = 0; angle_t centerangle = 0;

	//SoM: 3/17/2000
	fixed_t gz, gzt;
	INT32 heightsec, phs;
	INT32 light = 0;
	fixed_t this_scale;
	fixed_t spritexscale, spriteyscale;

	// rotsprite
	fixed_t spr_width, spr_height;
	fixed_t spr_offset, spr_topoffset;

#ifdef ROTSPRITE
	patch_t *rotsprite = NULL;
	INT32 rollangle = 0;
	angle_t rollsum = 0;
	angle_t pitchnroll = 0;
	angle_t sliptiderollangle = 0;
#endif
	
	INT32 dist = -1;

	if (cv_grmaxinterpdist.value)
		dist = R_QuickCamDist(thing->x, thing->y);

	// uncapped/interpolation
	interpmobjstate_t interp = {0};

	// do interpolation
	if (R_UsingFrameInterpolation() && !paused && (!cv_grmaxinterpdist.value || dist < cv_grmaxinterpdist.value))
	{
		R_InterpolateMobjState(oldthing, rendertimefrac, &interp);
	}
	else
	{
		R_InterpolateMobjState(oldthing, FRACUNIT, &interp);
	}

	this_scale = interp.scale;

	// transform the origin point
	tr_x = interp.x - viewx;
	tr_y = interp.y - viewy;

	gxt = FixedMul(tr_x, viewcos);
	gyt = -FixedMul(tr_y, viewsin);

	basetz = tz = gxt-gyt;

	// thing is behind view plane?
	if (!papersprite && (tz < FixedMul(MINZ, this_scale))) // papersprite clipping is handled later
		return;

	gxt = -FixedMul(tr_x, viewsin);
	gyt = FixedMul(tr_y, viewcos);
	basetx = tx = -(gyt + gxt);

	// too far off the side?
	if (!papersprite && abs(tx) > tz<<2) // papersprite clipping is handled later
		return;

	// aspect ratio stuff
	xscale = FixedDiv(projection, tz);
	sortscale = FixedDiv(projectiony, tz);

	// decide which patch to use for sprite relative to player
#ifdef RANGECHECK
	if ((size_t)(thing->sprite) >= numsprites)
		I_Error("R_ProjectSprite: invalid sprite number %d ", thing->sprite);
#endif

	rot = thing->frame&FF_FRAMEMASK;

	//Fab : 02-08-98: 'skin' override spritedef currently used for skin
	if ((thing->skin || thing->localskin) && thing->sprite == SPR_PLAY)
	{
		sprdef = &((skin_t *)( (thing->localskin) ? thing->localskin : thing->skin ))->spritedef;
#ifdef ROTSPRITE
		sprinfo = &((skin_t *)( (thing->localskin) ? thing->localskin : thing->skin ))->sprinfo;
#endif

		if (rot >= sprdef->numframes)
			sprdef = &sprites[thing->sprite];
	}
	else
	{
		sprdef = &sprites[thing->sprite];
#ifdef ROTSPRITE
		sprinfo = &spriteinfo[thing->sprite];
#endif
	}

	if (rot >= sprdef->numframes)
	{
		CONS_Alert(CONS_ERROR, M_GetText("R_ProjectSprite: invalid sprite frame %s/%s for %s\n"),
			sizeu1(rot), sizeu2(sprdef->numframes), sprnames[thing->sprite]);
		thing->sprite = states[S_UNKNOWN].sprite;
		thing->frame = states[S_UNKNOWN].frame;
		sprdef = &sprites[thing->sprite];
#ifdef ROTSPRITE
		sprinfo = &spriteinfo[thing->sprite];
#endif
		rot = thing->frame&FF_FRAMEMASK;
		if (!thing->skin)
		{
			thing->state->sprite = thing->sprite;
			thing->state->frame = thing->frame;
		}
	}

	sprframe = &sprdef->spriteframes[rot];

#ifdef PARANOIA
	if (!sprframe)
		I_Error("R_ProjectSprite: sprframes NULL for sprite %d\n", thing->sprite);
#endif

	if (sprframe->rotate != SRF_SINGLE || papersprite || (cv_sloperoll.value == 2 && cv_spriteroll.value))
	{
		ang = R_PointToAngle (interp.x, interp.y) - interp.angle;
		camang = R_PointToAngle (interp.x, interp.y);
		
		if (mirrored)
			ang = InvAngle(ang);
	}

	if (sprframe->rotate == SRF_SINGLE)
	{
		// use single rotation for all views
		rot = 0;                        //Fab: for vis->patch below
		lump = sprframe->lumpid[0];     //Fab: see note above
		flip = sprframe->flip; // Will only be 0x00 or 0xFF
	}
	else
	{
		// choose a different rotation based on player view
		//ang = R_PointToAngle (interp.x, interp.y) - interpangle;

		if ((ang < ANGLE_180) && (sprframe->rotate & SRF_RIGHT)) // See from right
			rot = 6; // F7 slot
		else if ((ang >= ANGLE_180) && (sprframe->rotate & SRF_LEFT)) // See from left
			rot = 2; // F3 slot
		else // Normal behaviour
			rot = (ang+ANGLE_202h)>>29;

		//Fab: lumpid is the index for spritewidth,spriteoffset... tables
		lump = sprframe->lumpid[rot];
		flip = sprframe->flip & (1<<rot);
	}

	I_Assert(lump < max_spritelumps);
	
	if (thing->localskin && ((skin_t *)thing->localskin)->flags & SF_HIRES)
		this_scale = FixedMul(this_scale, ((skin_t *)thing->localskin)->highresscale);
	else if (thing->skin && ((skin_t *)thing->skin)->flags & SF_HIRES)
		this_scale = FixedMul(this_scale, ((skin_t *)thing->skin)->highresscale);

	spr_width = spritecachedinfo[lump].width;
	spr_height = spritecachedinfo[lump].height;
	spr_offset = spritecachedinfo[lump].offset;
	spr_topoffset = spritecachedinfo[lump].topoffset;

#ifdef ROTSPRITE
    pitchnroll = 0;  // set this to 0, non-paper sprites will affect this value
	
	if (cv_spriteroll.value)
	{
		if (papersprite)
		{
			if (ang >= ANGLE_180)
			{
				// Makes Software act much more sane like OpenGL
				rollangle = InvAngle(thing->rollangle);
			}
			else
			{
				rollangle = thing->rollangle;
			}
		}
		else
		{
			// this is very messy, but it on-the-fly calculates rotations for all the
			// pitch and roll variables
			pitchnroll = FixedMul(FINECOSINE((ang) >> ANGLETOFINESHIFT), interp.roll) +
						 FixedMul(FINESINE((ang) >> ANGLETOFINESHIFT), interp.pitch) +
						 FixedMul(FINECOSINE((camang) >> ANGLETOFINESHIFT), interp.sloperoll) +
						 FixedMul(FINESINE((camang) >> ANGLETOFINESHIFT), interp.slopepitch);

			rollangle = thing->rollangle;
		}

		if ((rollangle) || (pitchnroll) || (thing->player && thing->player->sliproll))
		{
			rollsum = pitchnroll;

			if (thing->player)
			{
				sliptiderollangle =
					cv_sliptideroll.value ? thing->player->sliproll * (thing->player->sliptidemem) : 0;
				rollsum += thing->rollangle +
						   FixedMul(FINECOSINE((ang) >> ANGLETOFINESHIFT), sliptiderollangle);
			}
			else
				rollsum += thing->rollangle;

			rollangle = R_GetRollAngle(rollsum);
			rotsprite = Patch_GetRotatedSprite(sprframe, (thing->frame & FF_FRAMEMASK), rot, flip, false, sprinfo, rollangle);

			if (rotsprite != NULL)
			{
				spr_width = SHORT(rotsprite->width) << FRACBITS;
				spr_height = SHORT(rotsprite->height) << FRACBITS;
				spr_offset = SHORT(rotsprite->leftoffset) << FRACBITS;
				spr_topoffset = SHORT(rotsprite->topoffset) << FRACBITS;
				spr_topoffset += FEETADJUST;

				// flip -> rotate, not rotate -> flip
				flip = 0;
			}
		}
	}
#endif

	flip = !flip != !hflip;

	// calculate edges of the shape
	spritexscale = interp.spritexscale;
	spriteyscale = interp.spriteyscale;
	if (spritexscale < 1 || spriteyscale < 1)
		return;

	if (thing->renderflags & RF_ABSOLUTEOFFSETS)
	{
		spr_offset = interp.spritexoffset;
		spr_topoffset = interp.spriteyoffset;
	}
	else
	{
		SINT8 flipoffset = 1;

		if ((thing->renderflags & RF_FLIPOFFSETS) && flip)
			flipoffset = -1;

		spr_offset += interp.spritexoffset * flipoffset;
		spr_topoffset += interp.spriteyoffset * flipoffset;
	}

	if (flip)
		offset = spr_offset - spr_width;
	else
		offset = -spr_offset;

	offset = FixedMul(offset, FixedMul(spritexscale, this_scale));
	offset2 = FixedMul(spr_width, FixedMul(spritexscale, this_scale));

	if (papersprite)
	{
		fixed_t xscale2, yscale2, cosmul, sinmul, tx2, tz2;
		INT32 range;

		if (ang >= ANGLE_180)
		{
			offset *= -1;
			offset2 *= -1;
		}

		cosmul = FINECOSINE(interp.angle >> ANGLETOFINESHIFT);
		sinmul = FINESINE(interp.angle >> ANGLETOFINESHIFT);

		tr_x += FixedMul(offset, cosmul);
		tr_y += FixedMul(offset, sinmul);
		gxt = FixedMul(tr_x, viewcos);
		gyt = -FixedMul(tr_y, viewsin);
		tz = gxt-gyt;
		yscale = FixedDiv(projectiony, tz);

		gxt = -FixedMul(tr_x, viewsin);
		gyt = FixedMul(tr_y, viewcos);
		tx = -(gyt + gxt);
		xscale = FixedDiv(projection, tz);
		x1 = (centerxfrac + FixedMul(tx,xscale))>>FRACBITS;

		// Get paperoffset (offset) and paperoffset (distance)
		paperoffset = -FixedMul(tr_x, cosmul) - FixedMul(tr_y, sinmul);
		paperdistance = -FixedMul(tr_x, sinmul) + FixedMul(tr_y, cosmul);
		if (paperdistance < 0)
		{
			paperoffset = -paperoffset;
			paperdistance = -paperdistance;
		}
		centerangle = viewangle - interp.angle;

		tr_x += FixedMul(offset2, cosmul);
		tr_y += FixedMul(offset2, sinmul);
		gxt = FixedMul(tr_x, viewcos);
		gyt = -FixedMul(tr_y, viewsin);
		tz2 = gxt-gyt;
		yscale2 = FixedDiv(projectiony, tz2);
		//if (yscale2 < 64) return; // ditto

		gxt = -FixedMul(tr_x, viewsin);
		gyt = FixedMul(tr_y, viewcos);
		tx2 = -(gyt + gxt);
		xscale2 = FixedDiv(projection, tz2);
		x2 = ((centerxfrac + FixedMul(tx2,xscale2))>>FRACBITS);

		if (max(tz, tz2) < FixedMul(MINZ, this_scale)) // non-papersprite clipping is handled earlier
			return;

		// Needs partially clipped
		if (tz < FixedMul(MINZ, this_scale))
		{
			fixed_t div = FixedDiv(tz2-tz, FixedMul(MINZ, this_scale)-tz);
			tx += FixedDiv(tx2-tx, div);
			tz = FixedMul(MINZ, this_scale);
		}
		else if (tz2 < FixedMul(MINZ, this_scale))
		{
			fixed_t div = FixedDiv(tz-tz2, FixedMul(MINZ, this_scale)-tz2);
			tx2 += FixedDiv(tx-tx2, div);
			tz2 = FixedMul(MINZ, this_scale);
		}
		
		if ((tx2 / 4) < -(FixedMul(tz2, fovtan)) || (tx / 4) > FixedMul(tz, fovtan)) // too far off the side?
			return;

		yscale = FixedDiv(projectiony, tz);
		xscale = FixedDiv(projection, tz);

		x1 = (centerxfrac + FixedMul(tx,xscale))>>FRACBITS;

		// off the right side?
		if (x1 > viewwidth)
			return;

		yscale2 = FixedDiv(projectiony, tz2);
		xscale2 = FixedDiv(projection, tz2);

		x2 = (centerxfrac + FixedMul(tx2,xscale2))>>FRACBITS;

		// off the left side
		if (x2 < 0)
			return;

		range = x2 - x1;

		if (range < 0)
			return;

		range++; // fencepost problem
		
		if (range > 32767)
		{
			// If the range happens to be too large for fixed_t,
			// abort the draw to avoid xscale becoming negative due to arithmetic overflow.
			return;
		}

		scalestep = ((yscale2 - yscale)/range) ?: 1;

		if (scalestep == 0)
			scalestep = 1;

		xscale = FixedDiv(range<<FRACBITS, abs(offset2));

		// The following two are alternate sorting methods which might be more applicable in some circumstances. TODO - maybe enable via MF2?
		// sortscale = max(yscale, yscale2);
		// sortscale = min(yscale, yscale2);
	}
	else
	{
		scalestep = 0;
		yscale = sortscale;
		tx += offset;
		//x1 = (centerxfrac + FixedMul(tx,xscale))>>FRACBITS;
		x1 = centerx + (FixedMul(tx,xscale) / FRACUNIT);

		// off the right side?
		if (x1 > viewwidth)
			return;

		tx += offset2;
		//x2 = ((centerxfrac + FixedMul(tx,xscale))>>FRACBITS); x2--;
		x2 = (centerx + (FixedMul(tx,xscale) / FRACUNIT)) - 1;

		// off the left side
		if (x2 < 0)
			return;
	}

	// PORTAL SPRITE CLIPPING
	if (portalrender && portalclipline)
	{
		if (x2 < portalclipstart || x1 > portalclipend)
			return;

		if (P_PointOnLineSide(interp.x, interp.y, portalclipline) != 0)
			return;
	}

	//SoM: 3/17/2000: Disregard sprites that are out of view..
	if (thing->eflags & MFE_VERTICALFLIP)
	{
		// When vertical flipped, draw sprites from the top down, at least as far as offsets are concerned.
		// sprite height - sprite topoffset is the proper inverse of the vertical offset, of course.
		// remember gz and gzt should be seperated by sprite height, not thing height - thing height can be shorter than the sprite itself sometimes!
		gz = interp.z + oldthing->height - FixedMul(spr_topoffset, FixedMul(spriteyscale, this_scale));
		gzt = gz + FixedMul(spr_height, FixedMul(spriteyscale, this_scale));
	}
	else
	{
		gzt = interp.z + FixedMul(spr_topoffset, FixedMul(spriteyscale, this_scale));
		gz = gzt - FixedMul(spr_height, FixedMul(spriteyscale, this_scale));
	}

	if (thing->subsector->sector->cullheight)
	{
		if (R_DoCulling(thing->subsector->sector->cullheight, viewsector->cullheight, viewz, gz, gzt))
			return;
	}

	if (thing->subsector->sector->numlights)
	{
		INT32 lightnum;
		light = thing->subsector->sector->numlights - 1;

		for (lightnum = 1; lightnum < thing->subsector->sector->numlights; lightnum++) {
			fixed_t h = thing->subsector->sector->lightlist[lightnum].slope ? P_GetZAt(thing->subsector->sector->lightlist[lightnum].slope, interp.x, interp.y)
			            : thing->subsector->sector->lightlist[lightnum].height;
			if (h <= gzt) {
				light = lightnum - 1;
				break;
			}
		}
		lightnum = (*thing->subsector->sector->lightlist[light].lightlevel >> LIGHTSEGSHIFT);

		if (lightnum < 0)
			spritelights = scalelight[0];
		else if (lightnum >= LIGHTLEVELS)
			spritelights = scalelight[LIGHTLEVELS-1];
		else
			spritelights = scalelight[lightnum];
	}

	heightsec = thing->subsector->sector->heightsec;
	if (viewplayer->mo && viewplayer->mo->subsector)
		phs = viewplayer->mo->subsector->sector->heightsec;
	else
		phs = -1;

	if (heightsec != -1 && phs != -1) // only clip things which are in special sectors
	{
		if (viewz < sectors[phs].floorheight ?
		interp.z >= sectors[heightsec].floorheight :
		gzt < sectors[heightsec].floorheight)
			return;
		if (viewz > sectors[phs].ceilingheight ?
		gzt < sectors[heightsec].ceilingheight && viewz >= sectors[heightsec].ceilingheight :
		interp.z >= sectors[heightsec].ceilingheight)
			return;
	}

	// store information in a vissprite
	vis = R_NewVisSprite();
	vis->renderflags = thing->renderflags;
	vis->heightsec = heightsec; //SoM: 3/17/2000
	vis->mobjflags = thing->flags;
	vis->scale = yscale; //<<detailshift;
	vis->sortscale = sortscale;
	vis->dispoffset = thing->info->dispoffset; // Monster Iestyn: 23/11/15
	vis->gx = interp.x;
	vis->gy = interp.y;
	vis->gz = gz;
	vis->gzt = gzt;
	vis->thingheight = thing->height;
	vis->pz = interp.z;
	vis->pzt = vis->pz + vis->thingheight;
	vis->texturemid = FixedDiv(gzt - viewz, spriteyscale);
	vis->scalestep = scalestep;
	vis->paperoffset = paperoffset;
	vis->paperdistance = paperdistance;
	vis->centerangle = centerangle;
	vis->shear.tan = 0;
	vis->shear.offset = 0;

	vis->mobj = thing; // Easy access! Tails 06-07-2002

	vis->x1 = x1 < 0 ? 0 : x1;
	vis->x2 = x2 >= viewwidth ? viewwidth-1 : x2;

	// PORTAL SEMI-CLIPPING
	if (portalrender && portalclipline)
	{
		if (vis->x1 < portalclipstart)
			vis->x1 = portalclipstart;
		if (vis->x2 > portalclipend)
			vis->x2 = portalclipend;
	}

	vis->sector = thing->subsector->sector;
	vis->szt = (INT16)((centeryfrac - FixedMul(vis->gzt - viewz, sortscale))>>FRACBITS);
	vis->sz = (INT16)((centeryfrac - FixedMul(vis->gz - viewz, sortscale))>>FRACBITS);
	vis->cut = SC_NONE;

	if (thing->subsector->sector->numlights)
		vis->extra_colormap = thing->subsector->sector->lightlist[light].extra_colormap;
	else
		vis->extra_colormap = thing->subsector->sector->extra_colormap;

	vis->xscale = FixedMul(spritexscale, xscale); //SoM: 4/17/2000
	vis->scale = FixedMul(spriteyscale, yscale); //<<detailshift;
	vis->thingscale = interp.scale;

	vis->spritexscale = spritexscale;
	vis->spriteyscale = spriteyscale;
	vis->spritexoffset = spr_offset;
	vis->spriteyoffset = spr_topoffset;

	iscale = FixedDiv(FRACUNIT, vis->xscale);

	if (flip)
	{
		vis->startfrac = spr_width-1;
		vis->xiscale = -iscale;
	}
	else
	{
		vis->startfrac = 0;
		vis->xiscale = iscale;
	}

	if (vis->x1 > x1)
	{
		vis->startfrac += FixedDiv(vis->xiscale, this_scale) * (vis->x1 - x1);
		vis->scale += FixedMul(scalestep, spriteyscale) * (vis->x1 - x1);
	}

	//Fab: lumppat is the lump number of the patch to use, this is different
	//     than lumpid for sprites-in-pwad : the graphics are patched
#ifdef ROTSPRITE
	if ((rotsprite != NULL) && (cv_spriteroll.value))
		vis->patch = rotsprite;
	else
#endif
		vis->patch = W_CachePatchNum(sprframe->lumppat[rot], PU_CACHE);

//
// determine the colormap (lightlevel & special effects)
//
	vis->transmap = NULL;

	// specific translucency
	if (!cv_translucency.value)
		; // no translucency
	else if (thing->flags2 & MF2_SHADOW) // actually only the player should use this (temporary invisibility)
		vis->transmap = transtables + ((tr_trans80-1)<<FF_TRANSSHIFT); // because now the translucency is set through FF_TRANSMASK
	else if (thing->frame & FF_TRANSMASK)
		vis->transmap = transtables + (thing->frame & FF_TRANSMASK) - 0x10000;

	if (((thing->frame & FF_FULLBRIGHT) || (thing->flags2 & MF2_SHADOW))
		&& (!vis->extra_colormap || !(vis->extra_colormap->fog & 1)))
	{
		// full bright: goggles
		vis->colormap = colormaps;
	}
	else
	{
		// diminished light
		lindex = FixedMul(xscale, LIGHTRESOLUTIONFIX)>>(LIGHTSCALESHIFT);

		if (lindex >= MAXLIGHTSCALE)
			lindex = MAXLIGHTSCALE-1;

		vis->colormap = spritelights[lindex];
	}

	vis->precip = false;

	if (thing->eflags & MFE_VERTICALFLIP)
		vis->vflip = true;
	else
		vis->vflip = false;

	if (thing->subsector->sector->numlights)
		R_SplitSprite(vis, thing);

	if (cv_shadow.value)
	{
		if (oldthing->haveshadow)
			R_ProjectDropShadow(oldthing, vis, basetx, basetz);
	}

	// Debug
	++objectsdrawn;
}

static void R_ProjectPrecipitationSprite(precipmobj_t *thing)
{
	fixed_t tr_x, tr_y;
	fixed_t gxt, gyt;
	fixed_t tx, tz;
	fixed_t xscale, yscale; //added : 02-02-98 : aaargll..if I were a math-guy!!!

	INT32 x1, x2;

	spritedef_t *sprdef;
	spriteframe_t *sprframe;
	size_t lump;

	vissprite_t *vis;

	fixed_t iscale;

	//SoM: 3/17/2000
	fixed_t gz ,gzt;

	INT32 dist = 1;

	if (cv_grmaxinterpdist.value)
		dist = R_QuickCamDist(thing->x, thing->y);

	// uncapped/interpolation
	interpmobjstate_t interp = {0};

	// do interpolation
	if (R_UsingFrameInterpolation() && !paused && (!cv_grmaxinterpdist.value || dist < cv_grmaxinterpdist.value))
	{
		R_InterpolatePrecipMobjState(thing, rendertimefrac, &interp);
	}
	else
	{
		R_InterpolatePrecipMobjState(thing, FRACUNIT, &interp);
	}

	// transform the origin point
	tr_x = interp.x - viewx;
	tr_y = interp.y - viewy;

	gxt = FixedMul(tr_x, viewcos);
	gyt = -FixedMul(tr_y, viewsin);

	tz = gxt - gyt;

	// thing is behind view plane?
	if (tz < MINZ)
		return;

	gxt = -FixedMul(tr_x, viewsin);
	gyt = FixedMul(tr_y, viewcos);
	tx = -(gyt + gxt);

	// too far off the side?
	if (abs(tx) > FixedMul(tz, fovtan)<<2)
		return;

	// aspect ratio stuff :
	xscale = FixedDiv(projection, tz);
	yscale = FixedDiv(projectiony, tz);

	// decide which patch to use for sprite relative to player
#ifdef RANGECHECK
	if ((unsigned)thing->sprite >= numsprites)
		I_Error("R_ProjectPrecipitationSprite: invalid sprite number %d ",
			thing->sprite);
#endif

	sprdef = &sprites[thing->sprite];

#ifdef RANGECHECK
	if ((UINT8)(thing->frame&FF_FRAMEMASK) >= sprdef->numframes)
		I_Error("R_ProjectPrecipitationSprite: invalid sprite frame %d : %d for %s",
			thing->sprite, thing->frame, sprnames[thing->sprite]);
#endif

	sprframe = &sprdef->spriteframes[thing->frame & FF_FRAMEMASK];

#ifdef PARANOIA
	if (!sprframe)
		I_Error("R_ProjectPrecipitationSprite: sprframes NULL for sprite %d\n", thing->sprite);
#endif

	// use single rotation for all views
	lump = sprframe->lumpid[0];     //Fab: see note above

	// calculate edges of the shape
	tx -= spritecachedinfo[lump].offset;
	x1 = (centerxfrac + FixedMul (tx,xscale)) >>FRACBITS;

	// off the right side?
	if (x1 > viewwidth)
		return;

	tx += spritecachedinfo[lump].width;
	x2 = ((centerxfrac + FixedMul (tx,xscale)) >>FRACBITS) - 1;

	// off the left side
	if (x2 < 0)
		return;

	// PORTAL SPRITE CLIPPING
	if (portalrender && portalclipline)
	{
		if (x2 < portalclipstart || x1 > portalclipend)
			return;

		if (P_PointOnLineSide(interp.x, interp.y, portalclipline) != 0)
			return;
	}

	//SoM: 3/17/2000: Disregard sprites that are out of view..
	gzt = interp.z + spritecachedinfo[lump].topoffset;
	gz = gzt - spritecachedinfo[lump].height;

	if (thing->subsector->sector->cullheight)
	{
		if (R_DoCulling(thing->subsector->sector->cullheight, viewsector->cullheight, viewz, gz, gzt))
			goto weatherthink;
	}

	// store information in a vissprite
	vis = R_NewVisSprite();
	vis->scale = vis->sortscale = yscale; //<<detailshift;
	vis->dispoffset = 0; // Monster Iestyn: 23/11/15
	vis->gx = interp.x;
	vis->gy = interp.y;
	vis->gz = gz;
	vis->gzt = gzt;
	vis->thingheight = 4*FRACUNIT;
	vis->pz = interp.z;
	vis->pzt = vis->pz + vis->thingheight;
	vis->texturemid = vis->gzt - viewz;
	vis->scalestep = 0;
	vis->paperdistance = 0;
	vis->shear.tan = 0;
	vis->shear.offset = 0;

	vis->x1 = x1 < 0 ? 0 : x1;
	vis->x2 = x2 >= viewwidth ? viewwidth-1 : x2;

	// PORTAL SEMI-CLIPPING
	if (portalrender && portalclipline)
	{
		if (vis->x1 < portalclipstart)
			vis->x1 = portalclipstart;
		if (vis->x2 > portalclipend)
			vis->x2 = portalclipend;
	}

	vis->xscale = xscale; //SoM: 4/17/2000
	vis->sector = thing->subsector->sector;
	vis->szt = (INT16)((centeryfrac - FixedMul(vis->gzt - viewz, yscale))>>FRACBITS);
	vis->sz = (INT16)((centeryfrac - FixedMul(vis->gz - viewz, yscale))>>FRACBITS);

	iscale = FixedDiv(FRACUNIT, xscale);

	vis->startfrac = 0;
	vis->xiscale = iscale;

	vis->thingscale = interp.scale;

	if (vis->x1 > x1)
		vis->startfrac += vis->xiscale*(vis->x1-x1);

	//Fab: lumppat is the lump number of the patch to use, this is different
	//     than lumpid for sprites-in-pwad : the graphics are patched
	vis->patch = W_CachePatchNum(sprframe->lumppat[0], PU_CACHE);

	// specific translucency
	if (thing->frame & FF_TRANSMASK)
		vis->transmap = (thing->frame & FF_TRANSMASK) - 0x10000 + transtables;
	else
		vis->transmap = NULL;

	vis->mobjflags = 0;
	vis->cut = SC_NONE;
	vis->extra_colormap = thing->subsector->sector->extra_colormap;
	vis->heightsec = thing->subsector->sector->heightsec;

	// Fullbright
	vis->colormap = colormaps;
	vis->precip = true;
	vis->vflip = false;
	
weatherthink:
	// okay... this is a hack, but weather isn't networked, so it should be ok
	if (!(thing->precipflags & PCF_THUNK))
	{
		if (thing->precipflags & PCF_RAIN)
			P_RainThinker(thing);
		else
			P_SnowThinker(thing);
		thing->precipflags |= PCF_THUNK;
	}

}

// R_AddSprites
// During BSP traversal, this adds sprites by sector.
//
void R_AddSprites(sector_t *sec, INT32 lightlevel)
{
	mobj_t *thing;
	INT32 lightnum;
	fixed_t approx_dist, limit_dist;

	INT32 splitflags;			// check if a mobj has spliscreen flags
	boolean split_drawsprite;	// used for splitscreen flags

	if (rendermode != render_soft)
		return;

	// BSP is traversed by subsector.
	// A sector might have been split into several
	//  subsectors during BSP building.
	// Thus we check whether its already added.
	if (sec->validcount == validcount)
		return;

	// Well, now it will be done.
	sec->validcount = validcount;

	if (!sec->numlights)
	{
		if (sec->heightsec == -1) lightlevel = sec->lightlevel;

		lightnum = (lightlevel >> LIGHTSEGSHIFT);

		if (lightnum < 0)
			spritelights = scalelight[0];
		else if (lightnum >= LIGHTLEVELS)
			spritelights = scalelight[LIGHTLEVELS-1];
		else
			spritelights = scalelight[lightnum];
	}

	// Handle all things in sector.
	// If a limit exists, handle things a tiny bit different.
	if ((limit_dist = (fixed_t)(cv_drawdist.value) * mapobjectscale))
	{
		for (thing = sec->thinglist; thing; thing = thing->snext)
		{
			split_drawsprite = false;

			if (thing->sprite == SPR_NULL || thing->flags2 & MF2_DONTDRAW)
				continue;

			splitflags = thing->eflags & (MFE_DRAWONLYFORP1|MFE_DRAWONLYFORP2|MFE_DRAWONLYFORP3|MFE_DRAWONLYFORP4);

			if (splitscreen && splitflags)
			{
				if (thing->eflags & MFE_DRAWONLYFORP1)
					if (viewssnum == 0)
						split_drawsprite = true;

				if (thing->eflags & MFE_DRAWONLYFORP2)
					if (viewssnum == 1)
						split_drawsprite = true;

				if (thing->eflags & MFE_DRAWONLYFORP3 && splitscreen > 1)
					if (viewssnum == 2)
						split_drawsprite = true;

				if (thing->eflags & MFE_DRAWONLYFORP4 && splitscreen > 2)
					if (viewssnum == 3)
						split_drawsprite = true;
			}
			else
				split_drawsprite = true;

			if (!split_drawsprite)
				continue;

			approx_dist = P_AproxDistance(viewx-thing->x, viewy-thing->y);

			if (approx_dist > limit_dist)
				continue;

			R_ProjectSprite(thing);
		}
	}
	else
	{
		// Draw everything in sector, no checks
		for (thing = sec->thinglist; thing; thing = thing->snext)
		{

			split_drawsprite = false;

			if (thing->sprite == SPR_NULL || thing->flags2 & MF2_DONTDRAW)
				continue;

			splitflags = thing->eflags & (MFE_DRAWONLYFORP1|MFE_DRAWONLYFORP2|MFE_DRAWONLYFORP3|MFE_DRAWONLYFORP4);

			if (splitscreen && splitflags)
			{
				if (thing->eflags & MFE_DRAWONLYFORP1)
					if (viewssnum == 0)
						split_drawsprite = true;

				if (thing->eflags & MFE_DRAWONLYFORP2)
					if (viewssnum == 1)
						split_drawsprite = true;

				if (thing->eflags & MFE_DRAWONLYFORP3 && splitscreen > 1)
					if (viewssnum == 2)
						split_drawsprite = true;

				if (thing->eflags & MFE_DRAWONLYFORP4 && splitscreen > 2)
					if (viewssnum == 3)
						split_drawsprite = true;
			}
			else
				split_drawsprite = true;

			if (!split_drawsprite)
				continue;

			R_ProjectSprite(thing);
		}
	}
}

// R_AddPrecipitationSprites
// This renders through the blockmap instead of BSP to avoid
// iterating a huge amount of precipitation sprites in sectors
// that are beyond drawdist.
//
void R_AddPrecipitationSprites(void)
{
	const fixed_t drawdist = (fixed_t)(cv_drawdist_precip.value) * mapobjectscale;

	INT32 xl, xh, yl, yh, bx, by;
	precipmobj_t *th, *next;

	// no, no infinite draw distance for precipitation. this option at zero is supposed to turn it off
	if (drawdist == 0)
	{
		return;
	}

	R_GetRenderBlockMapDimensions(drawdist, &xl, &xh, &yl, &yh);

	for (bx = xl; bx <= xh; bx++)
	{
		for (by = yl; by <= yh; by++)
		{
			for (th = precipblocklinks[(by * bmapwidth) + bx]; th; th = next)
			{
				// Store this beforehand because R_ProjectPrecipitionSprite may free th (see P_PrecipThinker)
				next = th->bnext;

				if (th->precipflags & PCF_INVISIBLE)
					continue;

				R_ProjectPrecipitationSprite(th);
			}
		}
	}
}

//
// R_SortVisSprites
//
static vissprite_t vsprsortedhead;

void R_SortVisSprites(void)
{
	UINT32       i;
	vissprite_t *ds, *dsprev, *dsnext, *dsfirst;
	vissprite_t *best = NULL;
	vissprite_t  unsorted;
	fixed_t      bestscale;
	INT32        bestdispoffset;

	if (!visspritecount)
		return;

	unsorted.next = unsorted.prev = &unsorted;

	dsfirst = R_GetVisSprite(0);

	// The first's prev and last's next will be set to
	// nonsense, but are fixed in a moment
	for (i = 0, dsnext = dsfirst, ds = NULL; i < visspritecount; i++)
	{
		dsprev = ds;
		ds = dsnext;
		if (i < visspritecount - 1) dsnext = R_GetVisSprite(i + 1);

		ds->next = dsnext;
		ds->prev = dsprev;
	}

	// Fix first and last. ds still points to the last one after the loop
	dsfirst->prev = &unsorted;
	unsorted.next = dsfirst;
	if (ds)
		ds->next = &unsorted;
	unsorted.prev = ds;

	// pull the vissprites out by scale
	vsprsortedhead.next = vsprsortedhead.prev = &vsprsortedhead;
	for (i = 0; i < visspritecount; i++)
	{
		bestscale = bestdispoffset = INT32_MAX;
		for (ds = unsorted.next; ds != &unsorted; ds = ds->next)
		{
			// Remove this sprite if it was determined to not be visible
			if (ds->cut & SC_NOTVISIBLE)
			{
				ds->next->prev = ds->prev;
				ds->prev->next = ds->next;
				continue;
			}

			if (ds->sortscale < bestscale)
			{
				bestscale = ds->sortscale;
				bestdispoffset = ds->dispoffset;
				best = ds;
			}
			// order visprites of same scale by dispoffset, smallest first
			else if (ds->sortscale == bestscale && ds->dispoffset < bestdispoffset)
			{
				bestdispoffset = ds->dispoffset;
				best = ds;
			}
		}
		if (best)
		{
			best->next->prev = best->prev;
			best->prev->next = best->next;
			best->next = &vsprsortedhead;
			best->prev = vsprsortedhead.prev;
			vsprsortedhead.prev->next = best;
			vsprsortedhead.prev = best;
		}
	}
}

//
// R_CreateDrawNodes
// Creates and sorts a list of drawnodes for the scene being rendered.
static drawnode_t *R_CreateDrawNode(drawnode_t *link);

static drawnode_t nodebankhead;
static drawnode_t nodehead;

static void R_CreateDrawNodes(void)
{
	drawnode_t *entry;
	drawseg_t *ds;
	INT32 i, p, best, x1, x2;
	fixed_t bestdelta, delta;
	vissprite_t *rover;
	drawnode_t *r2;
	visplane_t *plane;
	INT32 sintersect;
	fixed_t scale = 0;

	// Add the 3D floors, thicksides, and masked textures...
	for (ds = ds_p; ds-- > drawsegs ;)
	{
		if (ds->numthicksides)
		{
			for (i = 0; i < ds->numthicksides; i++)
			{
				entry = R_CreateDrawNode(&nodehead);
				entry->thickseg = ds;
				entry->ffloor = ds->thicksides[i];
			}
		}
		// Check for a polyobject plane, but only if this is a front line
		if (ds->curline->polyseg && ds->curline->polyseg->visplane && !ds->curline->side) {
			plane = ds->curline->polyseg->visplane;
			R_PlaneBounds(plane);

			if (plane->low < 0 || plane->high > vid.height || plane->high > plane->low)
				;
			else {
				// Put it in!
				entry = R_CreateDrawNode(&nodehead);
				entry->plane = plane;
				entry->seg = ds;
			}
			ds->curline->polyseg->visplane = NULL;
		}
		if (ds->maskedtexturecol)
		{
			entry = R_CreateDrawNode(&nodehead);
			entry->seg = ds;
		}
		if (ds->numffloorplanes)
		{
			for (i = 0; i < ds->numffloorplanes; i++)
			{
				best = -1;
				bestdelta = 0;
				for (p = 0; p < ds->numffloorplanes; p++)
				{
					if (!ds->ffloorplanes[p])
						continue;
					plane = ds->ffloorplanes[p];
					R_PlaneBounds(plane);

					if (plane->low < 0 || plane->high > vid.height || plane->high > plane->low || plane->polyobj)
					{
						ds->ffloorplanes[p] = NULL;
						continue;
					}

					delta = abs(plane->height - viewz);
					if (delta > bestdelta)
					{
						best = p;
						bestdelta = delta;
					}
				}
				if (best != -1)
				{
					entry = R_CreateDrawNode(&nodehead);
					entry->plane = ds->ffloorplanes[best];
					entry->seg = ds;
					ds->ffloorplanes[best] = NULL;
				}
				else
					break;
			}
		}
	}

	// find all the remaining polyobject planes and add them on the end of the list
	// probably this is a terrible idea if we wanted them to be sorted properly
	// but it works getting them in for now
	for (i = 0; i < numPolyObjects; i++)
	{
		if (!PolyObjects[i].visplane)
			continue;
		plane = PolyObjects[i].visplane;
		R_PlaneBounds(plane);

		if (plane->low < 0 || plane->high > vid.height || plane->high > plane->low)
		{
			PolyObjects[i].visplane = NULL;
			continue;
		}
		entry = R_CreateDrawNode(&nodehead);
		entry->plane = plane;
		// note: no seg is set, for what should be obvious reasons
		PolyObjects[i].visplane = NULL;
	}

	if (visspritecount == 0)
		return;

	R_SortVisSprites();
	for (rover = vsprsortedhead.prev; rover != &vsprsortedhead; rover = rover->prev)
	{
		if (rover->szt > vid.height || rover->sz < 0)
			continue;

		sintersect = (rover->x1 + rover->x2) / 2;

		for (r2 = nodehead.next; r2 != &nodehead; r2 = r2->next)
		{
			if (r2->plane)
			{
				fixed_t planeobjectz, planecameraz;
				if (r2->plane->minx > rover->x2 || r2->plane->maxx < rover->x1)
					continue;
				if (rover->szt > r2->plane->low || rover->sz < r2->plane->high)
					continue;

				// Effective height may be different for each comparison in the case of slopes
				if (r2->plane->slope) {
					planeobjectz = P_GetZAt(r2->plane->slope, rover->gx, rover->gy);
					planecameraz = P_GetZAt(r2->plane->slope, viewx, viewy);
				} else
					planeobjectz = planecameraz = r2->plane->height;

				if (rover->mobjflags & MF_NOCLIPHEIGHT)
				{
					//Objects with NOCLIPHEIGHT can appear halfway in.
					if (planecameraz < viewz && rover->pz+(rover->thingheight/2) >= planeobjectz)
						continue;
					if (planecameraz > viewz && rover->pzt-(rover->thingheight/2) <= planeobjectz)
						continue;
				}
				else
				{
					if (planecameraz < viewz && rover->pz >= planeobjectz)
						continue;
					if (planecameraz > viewz && rover->pzt <= planeobjectz)
						continue;
				}

				// SoM: NOTE: Because a visplane's shape and scale is not directly
				// bound to any single linedef, a simple poll of it's frontscale is
				// not adequate. We must check the entire frontscale array for any
				// part that is in front of the sprite.

				x1 = rover->x1;
				x2 = rover->x2;
				if (x1 < r2->plane->minx) x1 = r2->plane->minx;
				if (x2 > r2->plane->maxx) x2 = r2->plane->maxx;

				if (r2->seg) // if no seg set, assume the whole thing is in front or something stupid
				{
					for (i = x1; i <= x2; i++)
					{
						if (r2->seg->frontscale[i] > rover->sortscale)
							break;
					}
					if (i > x2)
						continue;
				}

				entry = R_CreateDrawNode(NULL);
				(entry->prev = r2->prev)->next = entry;
				(entry->next = r2)->prev = entry;
				entry->sprite = rover;
				break;
			}
			else if (r2->thickseg)
			{
				fixed_t topplaneobjectz, topplanecameraz, botplaneobjectz, botplanecameraz;
				if (rover->x1 > r2->thickseg->x2 || rover->x2 < r2->thickseg->x1)
					continue;

				scale = r2->thickseg->scale1 > r2->thickseg->scale2 ? r2->thickseg->scale1 : r2->thickseg->scale2;
				if (scale <= rover->sortscale)
					continue;
				scale = r2->thickseg->scale1 + (r2->thickseg->scalestep * (sintersect - r2->thickseg->x1));
				if (scale <= rover->sortscale)
					continue;

				if (*r2->ffloor->t_slope) {
					topplaneobjectz = P_GetZAt(*r2->ffloor->t_slope, rover->gx, rover->gy);
					topplanecameraz = P_GetZAt(*r2->ffloor->t_slope, viewx, viewy);
				} else
					topplaneobjectz = topplanecameraz = *r2->ffloor->topheight;

				if (*r2->ffloor->b_slope) {
					botplaneobjectz = P_GetZAt(*r2->ffloor->b_slope, rover->gx, rover->gy);
					botplanecameraz = P_GetZAt(*r2->ffloor->b_slope, viewx, viewy);
				} else
					botplaneobjectz = botplanecameraz = *r2->ffloor->bottomheight;

				if ((topplanecameraz > viewz && botplanecameraz < viewz) ||
				    (topplanecameraz < viewz && rover->gzt < topplaneobjectz) ||
				    (botplanecameraz > viewz && rover->gz > botplaneobjectz))
				{
					entry = R_CreateDrawNode(NULL);
					(entry->prev = r2->prev)->next = entry;
					(entry->next = r2)->prev = entry;
					entry->sprite = rover;
					break;
				}
			}
			else if (r2->seg)
			{
				if (rover->x1 > r2->seg->x2 || rover->x2 < r2->seg->x1)
					continue;

				scale = r2->seg->scale1 > r2->seg->scale2 ? r2->seg->scale1 : r2->seg->scale2;
				if (scale <= rover->sortscale)
					continue;
				scale = r2->seg->scale1 + (r2->seg->scalestep * (sintersect - r2->seg->x1));

				if (rover->sortscale < scale)
				{
					entry = R_CreateDrawNode(NULL);
					(entry->prev = r2->prev)->next = entry;
					(entry->next = r2)->prev = entry;
					entry->sprite = rover;
					break;
				}
			}
			else if (r2->sprite)
			{
				if (r2->sprite->x1 > rover->x2 || r2->sprite->x2 < rover->x1)
					continue;
				if (r2->sprite->szt > rover->sz || r2->sprite->sz < rover->szt)
					continue;

				if (r2->sprite->sortscale > rover->sortscale
				 || (r2->sprite->sortscale == rover->sortscale && r2->sprite->dispoffset > rover->dispoffset))
				{
					entry = R_CreateDrawNode(NULL);
					(entry->prev = r2->prev)->next = entry;
					(entry->next = r2)->prev = entry;
					entry->sprite = rover;
					break;
				}
			}
		}
		if (r2 == &nodehead)
		{
			entry = R_CreateDrawNode(&nodehead);
			entry->sprite = rover;
		}
	}
}

static drawnode_t *R_CreateDrawNode(drawnode_t *link)
{
	drawnode_t *node = nodebankhead.next;

	if (node == &nodebankhead)
	{
		node = malloc(sizeof (*node));
		if (!node)
			I_Error("No more free memory to CreateDrawNode");
	}
	else
		(nodebankhead.next = node->next)->prev = &nodebankhead;

	if (link)
	{
		node->next = link;
		node->prev = link->prev;
		link->prev->next = node;
		link->prev = node;
	}

	node->plane = NULL;
	node->seg = NULL;
	node->thickseg = NULL;
	node->ffloor = NULL;
	node->sprite = NULL;
	
	ps_numdrawnodes.value.i++;
	return node;
}

static void R_DoneWithNode(drawnode_t *node)
{
	(node->next->prev = node->prev)->next = node->next;
	(node->next = nodebankhead.next)->prev = node;
	(node->prev = &nodebankhead)->next = node;
}

static void R_ClearDrawNodes(void)
{
	drawnode_t *rover;
	drawnode_t *next;

	for (rover = nodehead.next; rover != &nodehead ;)
	{
		next = rover->next;
		R_DoneWithNode(rover);
		rover = next;
	}

	nodehead.next = nodehead.prev = &nodehead;
}

void R_InitDrawNodes(void)
{
	nodebankhead.next = nodebankhead.prev = &nodebankhead;
	nodehead.next = nodehead.prev = &nodehead;
}

//
// R_DrawSprite
//
//Fab : 26-04-98:
// NOTE : uses con_clipviewtop, so that when console is on,
//        don't draw the part of sprites hidden under the console
static void R_DrawSprite(vissprite_t *spr)
{
	mfloorclip = spr->clipbot;
	mceilingclip = spr->cliptop;
	R_DrawVisSprite(spr);
}

// Special drawer for precipitation sprites Tails 08-18-2002
static void R_DrawPrecipitationSprite(vissprite_t *spr)
{
	mfloorclip = spr->clipbot;
	mceilingclip = spr->cliptop;
	R_DrawPrecipitationVisSprite(spr);
}

static boolean R_CheckSpriteVisible(vissprite_t *spr, INT32 x1, INT32 x2)
{
	INT16 sz = spr->sz;
	INT16 szt = spr->szt;

	fixed_t texturemid, yscale, scalestep = spr->scalestep;
	INT32 height;

	if (scalestep)
	{
		height = spr->patch->height;
		yscale = spr->scale;
		scalestep = FixedMul(scalestep, spr->spriteyscale);

		if (spr->thingscale != FRACUNIT)
			texturemid = FixedDiv(spr->texturemid, max(spr->thingscale, 1));
		else
			texturemid = spr->texturemid;
	}

	for (INT32 x = x1; x <= x2; x++)
	{
		if (scalestep)
		{
			fixed_t top = centeryfrac - FixedMul(texturemid, yscale);
			fixed_t bottom = top + (height * yscale);
			szt = (INT16)(top >> FRACBITS);
			sz = (INT16)(bottom >> FRACBITS);
			yscale += scalestep;
		}

		if (spr->cliptop[x] < spr->clipbot[x] && sz > spr->cliptop[x] && szt < spr->clipbot[x])
			return true;
	}

	return false;
}

// R_ClipVisSprite
// Clips vissprites without drawing, so that portals can work. -Red
static void R_ClipVisSprite(vissprite_t *spr, INT32 x1, INT32 x2)
{
	drawseg_t *ds;
	INT32		x;
	INT32		r1;
	INT32		r2;
	fixed_t		scale;
	fixed_t		lowscale;
	INT32		silhouette;

	for (x = x1; x <= x2; x++)
	{
		spr->clipbot[x] = spr->cliptop[x] = -2;
	}

	// Scan drawsegs from end to start for obscuring segs.
	// The first drawseg that has a greater scale
	//  is the clip seg.
	//SoM: 4/8/2000:
	// Pointer check was originally nonportable
	// and buggy, by going past LEFT end of array:

	// e6y: optimization
	if (drawsegs_xrange_size)
	{
		const drawseg_xrange_item_t *last = &drawsegs_xrange[drawsegs_xrange_count - 1];
		drawseg_xrange_item_t *curr = &drawsegs_xrange[-1];

		while (++curr <= last)
		{
			// determine if the drawseg obscures the sprite
			if (curr->x1 > spr->x2 || curr->x2 < spr->x1)
			{
				// does not cover sprite
				continue;
			}

			ds = curr->user;

			if (ds->portalpass > 0 && ds->portalpass <= portalrender)
				continue; // is a portal

			r1 = ds->x1 < spr->x1 ? spr->x1 : ds->x1;
			r2 = ds->x2 > spr->x2 ? spr->x2 : ds->x2;

			if (ds->scale1 > ds->scale2)
			{
				lowscale = ds->scale2;
				scale = ds->scale1;
			}
			else
			{
				lowscale = ds->scale1;
				scale = ds->scale2;
			}

			if (scale < spr->sortscale ||
			    (lowscale < spr->sortscale &&
			     !R_PointOnSegSide (spr->gx, spr->gy, ds->curline)))
			{
				// masked mid texture?
				/*if (ds->maskedtexturecol)
					R_RenderMaskedSegRange (ds, r1, r2);*/
				// seg is behind sprite
				continue;
			}

			// clip this piece of the sprite
			silhouette = ds->silhouette;

			if (spr->gz >= ds->bsilheight)
				silhouette &= ~SIL_BOTTOM;

			if (spr->gzt <= ds->tsilheight)
				silhouette &= ~SIL_TOP;

			if (silhouette == SIL_BOTTOM)
			{
				// bottom sil
				for (x = r1; x <= r2; x++)
					if (spr->clipbot[x] == -2)
						spr->clipbot[x] = ds->sprbottomclip[x];
			}
			else if (silhouette == SIL_TOP)
			{
				// top sil
				for (x = r1; x <= r2; x++)
					if (spr->cliptop[x] == -2)
						spr->cliptop[x] = ds->sprtopclip[x];
			}
			else if (silhouette == (SIL_TOP|SIL_BOTTOM))
			{
				// both
				for (x = r1; x <= r2; x++)
				{
					if (spr->clipbot[x] == -2)
						spr->clipbot[x] = ds->sprbottomclip[x];
					if (spr->cliptop[x] == -2)
						spr->cliptop[x] = ds->sprtopclip[x];
				}
			}
		}
	}
	//SoM: 3/17/2000: Clip sprites in water.
	if (spr->heightsec != -1)  // only things in specially marked sectors
	{
		fixed_t mh, h;
		INT32 phs = viewplayer->mo->subsector->sector->heightsec;
		if ((mh = sectors[spr->heightsec].floorheight) > spr->gz &&
			(h = centeryfrac - FixedMul(mh -= viewz, spr->sortscale)) >= 0 &&
			(h >>= FRACBITS) < viewheight)
		{
			if (mh <= 0 || (phs != -1 && viewz > sectors[phs].floorheight))
			{                          // clip bottom
				for (x = spr->x1; x <= spr->x2; x++)
					if (spr->clipbot[x] == -2 || h < spr->clipbot[x])
						spr->clipbot[x] = (INT16)h;
			}
			else						// clip top
			{
				for (x = spr->x1; x <= spr->x2; x++)
					if (spr->cliptop[x] == -2 || h > spr->cliptop[x])
						spr->cliptop[x] = (INT16)h;
			}
		}

		if ((mh = sectors[spr->heightsec].ceilingheight) < spr->gzt &&
		    (h = centeryfrac - FixedMul(mh-viewz, spr->sortscale)) >= 0 &&
		    (h >>= FRACBITS) < viewheight)
		{
			if (phs != -1 && viewz >= sectors[phs].ceilingheight)
			{                         // clip bottom
				for (x = spr->x1; x <= spr->x2; x++)
					if (spr->clipbot[x] == -2 || h < spr->clipbot[x])
						spr->clipbot[x] = (INT16)h;
			}
			else                       // clip top
			{
				for (x = spr->x1; x <= spr->x2; x++)
					if (spr->cliptop[x] == -2 || h > spr->cliptop[x])
						spr->cliptop[x] = (INT16)h;
			}
		}
	}
	if (spr->cut & SC_TOP && spr->cut & SC_BOTTOM)
	{
		for (x = spr->x1; x <= spr->x2; x++)
		{
			if (spr->cliptop[x] == -2 || spr->szt > spr->cliptop[x])
				spr->cliptop[x] = spr->szt;

			if (spr->clipbot[x] == -2 || spr->sz < spr->clipbot[x])
				spr->clipbot[x] = spr->sz;
		}
	}
	else if (spr->cut & SC_TOP)
	{
		for (x = spr->x1; x <= spr->x2; x++)
		{
			if (spr->cliptop[x] == -2 || spr->szt > spr->cliptop[x])
				spr->cliptop[x] = spr->szt;
		}
	}
	else if (spr->cut & SC_BOTTOM)
	{
		for (x = spr->x1; x <= spr->x2; x++)
		{
			if (spr->clipbot[x] == -2 || spr->sz < spr->clipbot[x])
				spr->clipbot[x] = spr->sz;
		}
	}

	// all clipping has been performed, so store the values - what, did you think we were drawing them NOW?

	// check for unclipped columns
	for (x = spr->x1; x <= spr->x2; x++)
	{
		if (spr->clipbot[x] == -2)
			spr->clipbot[x] = (INT16)viewheight;

		if (spr->cliptop[x] == -2)
			spr->cliptop[x] = -1;
	}
	
	// Check if it'll be visible
	// Not done for floorsprites.
	if (cv_spriteclip.value)
	{
		if (!R_CheckSpriteVisible(spr, x1, x2))
			spr->cut |= SC_NOTVISIBLE;
	}
}

void R_ClipSprites(void)
{
	const size_t maxdrawsegs = ds_p - drawsegs;
	const INT32 cx = viewwidth / 2;
	drawseg_t* ds;
	INT32 i;

	// e6y
	// Reducing of cache misses in the following R_DrawSprite()
	// Makes sense for scenes with huge amount of drawsegs.
	// ~12% of speed improvement on epic.wad map05
	for (i = 0; i < DS_RANGES_COUNT; i++)
	{
		drawsegs_xranges[i].count = 0;
	}

	if (visspritecount - clippedvissprites <= 0)
	{
		return;
	}

	if (drawsegs_xrange_size < maxdrawsegs)
	{
		drawsegs_xrange_size = 2 * maxdrawsegs;

		for (i = 0; i < DS_RANGES_COUNT; i++)
		{
			drawsegs_xranges[i].items = Z_Realloc(
				drawsegs_xranges[i].items,
				drawsegs_xrange_size * sizeof(drawsegs_xranges[i].items[0]),
				PU_STATIC, NULL
			);
		}
	}

	for (ds = ds_p; ds-- > drawsegs;)
	{
		if (ds->silhouette || ds->maskedtexturecol)
		{
			drawsegs_xranges[0].items[drawsegs_xranges[0].count].x1 = ds->x1;
			drawsegs_xranges[0].items[drawsegs_xranges[0].count].x2 = ds->x2;
			drawsegs_xranges[0].items[drawsegs_xranges[0].count].user = ds;

			// e6y: ~13% of speed improvement on sunder.wad map10
			if (ds->x1 < cx)
			{
				drawsegs_xranges[1].items[drawsegs_xranges[1].count] =
					drawsegs_xranges[0].items[drawsegs_xranges[0].count];
				drawsegs_xranges[1].count++;
			}

			if (ds->x2 >= cx)
			{
				drawsegs_xranges[2].items[drawsegs_xranges[2].count] =
					drawsegs_xranges[0].items[drawsegs_xranges[0].count];
				drawsegs_xranges[2].count++;
			}

			drawsegs_xranges[0].count++;
		}
	}

	for (; clippedvissprites < visspritecount; clippedvissprites++)
	{
		vissprite_t *spr = R_GetVisSprite(clippedvissprites);
		
		if (cv_spriteclip.value
		&& (spr->szt > vid.height || spr->sz < 0))
		{
			spr->cut |= SC_NOTVISIBLE;
			continue;
		}

		if (spr->x2 < cx)
		{
			drawsegs_xrange = drawsegs_xranges[1].items;
			drawsegs_xrange_count = drawsegs_xranges[1].count;
		}
		else if (spr->x1 >= cx)
		{
			drawsegs_xrange = drawsegs_xranges[2].items;
			drawsegs_xrange_count = drawsegs_xranges[2].count;
		}
		else
		{
			drawsegs_xrange = drawsegs_xranges[0].items;
			drawsegs_xrange_count = drawsegs_xranges[0].count;
		}

		R_ClipVisSprite(spr, spr->x1, spr->x2);
		
		if ((spr->cut & SC_NOTVISIBLE) == 0)
			numvisiblesprites++;
	}
}

//
// R_DrawMasked
//
void R_DrawMasked(void)
{
	drawnode_t *r2;
	drawnode_t *next;

	R_CreateDrawNodes();

	for (r2 = nodehead.next; r2 != &nodehead; r2 = r2->next)
	{
		if (r2->plane)
		{
			next = r2->prev;
			R_DrawSinglePlane(r2->plane);
			R_DoneWithNode(r2);
			r2 = next;
		}
		else if (r2->seg && r2->seg->maskedtexturecol != NULL)
		{
			next = r2->prev;
			R_RenderMaskedSegRange(r2->seg, r2->seg->x1, r2->seg->x2);
			r2->seg->maskedtexturecol = NULL;
			R_DoneWithNode(r2);
			r2 = next;
		}
		else if (r2->thickseg)
		{
			next = r2->prev;
			R_RenderThickSideRange(r2->thickseg, r2->thickseg->x1, r2->thickseg->x2, r2->ffloor);
			R_DoneWithNode(r2);
			r2 = next;
		}
		else if (r2->sprite)
		{
			next = r2->prev;

			// Tails 08-18-2002
			if (r2->sprite->precip == true)
				R_DrawPrecipitationSprite(r2->sprite);
			else
				R_DrawSprite(r2->sprite);

			R_DoneWithNode(r2);
			r2 = next;
		}
	}
	R_ClearDrawNodes();
}

// ==========================================================================
//
//                              SKINS CODE
//
// ==========================================================================

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
UINT8 skinsorted[MAXSKINS];
skin_t localskins[MAXSKINS];
skin_t allskins[MAXSKINS*2];

// FIXTHIS: don't work because it must be inistilised before the config load
//#define SKINVALUES
#ifdef SKINVALUES
CV_PossibleValue_t skin_cons_t[MAXSKINS+1];
CV_PossibleValue_t localskin_cons_t[MAXSKINS+1];
#endif

static void Sk_SetDefaultValue(skin_t *skin, boolean local)
{
	INT32 i;
	//
	// set default skin values
	//
	memset(skin, 0, sizeof (skin_t));
	snprintf(skin->name,
		sizeof skin->name, "skin %u", (UINT32)(skin-( (local) ? localskins : skins )));
	skin->name[sizeof skin->name - 1] = '\0';
	skin->wadnum = INT16_MAX;
	strcpy(skin->sprite, "");

	skin->flags = 0;

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
		if (S_sfx[i].skinsound != -1)
			skin->soundsid[S_sfx[i].skinsound] = i;
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
	skin->flags = 0;
	strcpy(skin->realname,   "Sonic");
	strcpy(skin->hudname,    "SONIC");

	strncpy(skin->facerank, "PLAYRANK", 9);
	strncpy(skin->facewant, "PLAYWANT", 9);
	strncpy(skin->facemmap, "PLAYMMAP", 9);
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
		if (stricmp(skins[i].name,name)==0)
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
		if (stricmp(allskins[i].name,name)==0)
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
			if (stricmp(localskins[i].name,name)==0)
				return i;
		}
		return -1;
	}
	else
		return R_SkinAvailable(name);
}

// network code calls this when a 'skin change' is received
boolean SetPlayerSkin(INT32 playernum, const char *skinname)
{
	INT32 i;
	player_t *player = &players[playernum];

	for (i = 0; i < numskins; i++)
	{
		// search in the skin list
		if (stricmp(skins[i].name, skinname) == 0)
		{
			SetPlayerSkinByNum(playernum, i);
			return true;
		}
	}

	if (P_IsLocalPlayer(player))
		CONS_Alert(CONS_WARNING, M_GetText("Skin '%s' not found.\n"), skinname);
	else if(server || IsPlayerAdmin(consoleplayer))
		CONS_Alert(CONS_WARNING, M_GetText("Player %d (%s) skin '%s' not found\n"), playernum, player_names[playernum], skinname);

	SetPlayerSkinByNum(playernum, 0);
	return false;
}

void SetLocalPlayerSkin(INT32 playernum, const char *skinname, consvar_t *cvar)
{
	player_t *player = &players[playernum];
	INT32 i;

	if (strcasecmp(skinname, "none"))
	{
		for (i = 0; i < numlocalskins; i++)
		{
			// search in the skin list
			if (stricmp(localskins[i].name, skinname) == 0)
			{
				player->localskin = 1 + i;
				player->skinlocal = true;
				if (player->mo)
				{
					player->mo->localskin = &localskins[i];
					player->mo->skinlocal = true;
				}
				break;
			}
		}
		for (i = 0; i < numskins; i++)
		{
			// search in the skin list
			if (stricmp(skins[i].name, skinname) == 0)
			{
				player->localskin = 1 + i;
				player->skinlocal = false;
				if (player->mo)
				{
					player->mo->localskin = &skins[i];
					player->mo->skinlocal = false;
				}
				break;
			}
		}
	}
	else
	{
		player->localskin = 0;
		player->skinlocal = false;
		if (player->mo)
		{
			player->mo->localskin = 0;
			player->mo->skinlocal = false;
		}
	}

	if (cvar != NULL) {
		if (player->localskin > 0) {
			CV_StealthSet(&cv_fakelocalskin, ( (player->skinlocal) ? localskins : skins )[player->localskin - 1].name);
			CV_StealthSet(cvar, ( (player->skinlocal) ? localskins : skins )[player->localskin - 1].name);
		}
		else {
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

		/*if (!(cv_debug || devparm) && !(netgame || multiplayer || demo.playback || modeattacking))
		{
			if (playernum == consoleplayer)
				CV_StealthSetValue(&cv_playercolor, skin->prefcolor);
			else if (playernum == displayplayers[1])
				CV_StealthSetValue(&cv_playercolor2, skin->prefcolor);
			else if (playernum == displayplayers[2])
				CV_StealthSetValue(&cv_playercolor3, skin->prefcolor);
			else if (playernum == displayplayers[3])
				CV_StealthSetValue(&cv_playercolor4, skin->prefcolor);
			player->skincolor = skin->prefcolor;
			if (player->mo)
				player->mo->color = player->skincolor;
		}*/

		if (player->mo)
			P_SetScale(player->mo, player->mo->scale);

		demo_extradata[playernum] |= DXD_SKIN;

		return;
	}

	if (P_IsLocalPlayer(player))
		CONS_Alert(CONS_WARNING, M_GetText("Skin %d not found\n"), skinnum);
	else if(server || IsPlayerAdmin(consoleplayer))
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
	const char *S_SKIN = "S_SKIN";
	lumpinfo_t *lump_p;

	// scan forward, start at <startlump>
	if (startlump < wadfiles[wadid]->numlumps)
	{
		lump_p = wadfiles[wadid]->lumpinfo + startlump;
		for (i = startlump; i < wadfiles[wadid]->numlumps; i++, lump_p++)
			if (memcmp(lump_p->name,S_SKIN,6)==0)
				return i;
	}
	return INT16_MAX; // not found
}

//sort function for sorting skin names
static int skinSortFunc(const void *a, const void *b) //tbh i have no clue what the naming conventions for local functions are
{
	const skin_t *in1 = &skins[*(const UINT8 *)a];
	const skin_t *in2 = &skins[*(const UINT8 *)b];
	INT32 temp = 0;
	const UINT8 val_a = *((const UINT8 *)a);
	const UINT8 val_b = *((const UINT8 *)b);

	//return (strcmp(in1->realname, in2->realname) < 0) || (strcmp(in1->realname, in2->realname) ==);

	switch (cv_skinselectgridsort.value)
	{
		case SKINMENUSORT_REALNAME:
			//CONS_Printf("Sorting by realname\n");
			// check name
			if ((temp = strcmp(in1->realname, in2->realname)))
				return temp;
			// sort by internal name
			return strcmp(in1->name, in2->name);
			break;

		case SKINMENUSORT_NAME:
			//CONS_Printf("Sorting by name\n");
			return strcmp(in1->name, in2->name);
			break;

		case SKINMENUSORT_SPEED:
			//CONS_Printf("Sorting by speed\n");
			// check speed
			if (in1->kartspeed < in2->kartspeed)
				return -1;
			else if (in2->kartspeed < in1->kartspeed)
				return 1;
			// then check weight
			if (in1->kartweight < in2->kartweight)
				return -1;
			else if (in2->kartweight < in1->kartweight)
				return 1;
			// then check name
			if ((temp = strcmp(in1->realname, in2->realname)))
				return temp;
			// sort by internal name
			return strcmp(in1->name, in2->name);
			break;

		case SKINMENUSORT_WEIGHT:
			//CONS_Printf("Sorting by weight\n");
			// check weight
			if (in1->kartweight < in2->kartweight)
				return -1;
			else if (in2->kartweight < in1->kartweight)
				return 1;
			// then check speed
			if (in1->kartspeed < in2->kartspeed)
				return -1;
			else if (in2->kartspeed < in1->kartspeed)
				return 1;
			// then check name
			if ((temp = strcmp(in1->realname, in2->realname)))
				return temp;
			// sort by internal name
			return strcmp(in1->name, in2->name);
			break;

		case SKINMENUSORT_PREFCOLOR:
			//CONS_Printf("Sorting by prefcolor\n");
			// check prefcolor
			if (in1->prefcolor < in2->prefcolor)
				return -1;
			else if (in2->prefcolor < in1->prefcolor)
				return 1;
			// then check name
			if ((temp = strcmp(in1->realname, in2->realname)))
				return temp;
			// sort by internal name
			return strcmp(in1->name, in2->name);
			break;

		case SKINMENUSORT_ID:
			//CONS_Printf("Sorting by id\n");
			//how do i do by ID?????
			//wait why dont i just convert the inputs to UINT32s
			//please tell me im allowed to define variables in here since its a block

			if (val_a == val_b)
				return 0;
			else if (val_a < val_b)
				return -1;
			else
				return 1;

		default:
			return strcmp(in1->name, in2->name);
			break;
	}
	//im scared this somehow will sometimes end up here so im gonna add this here just to be safe
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

	//
	// search for all skin markers in pwad
	//

	while ((lump = W_CheckForSkinMarkerInPwad(wadnum, lastlump)) != INT16_MAX)
	{
		// advance by default
		lastlump = lump + 1;

		if (( (local) ? numlocalskins : numskins ) >= MAXSKINS)
		{
			CONS_Alert(CONS_WARNING, M_GetText("Unable to add skin, too many characters are loaded (%d maximum)\n"), MAXSKINS);
			continue; // so we know how many skins couldn't be added
		}
		buf = W_CacheLumpNumPwad(wadnum, lump, PU_CACHE);
		size = W_LumpLengthPwad(wadnum, lump);

		// for strtok
		buf2 = malloc(size+1);
		if (!buf2)
			I_Error("R_AddSkins: No more free memory\n");
		M_Memcpy(buf2,buf,size);
		buf2[size] = '\0';

		// set defaults
		skin = &( (local) ? localskins : skins )[( (local) ? numlocalskins : numskins )];
		Sk_SetDefaultValue(skin, local);
		skin->wadnum = wadnum;
		hudname = realname = false;
		// parse
		stoken = strtok (buf2, "\r\n= ");
		while (stoken)
		{
			if ((stoken[0] == '/' && stoken[1] == '/')
				|| (stoken[0] == '#'))// skip comments
			{
				stoken = strtok(NULL, "\r\n"); // skip end of line
				goto next_token;              // find the real next token
			}

			value = strtok(NULL, "\r\n= ");

			if (!value)
				I_Error("R_AddSkins: syntax error in S_SKIN lump# %d(%s) in WAD %s\n", lump, W_CheckNameForNumPwad(wadnum,lump), wadfiles[wadnum]->filename);

			if (!stricmp(stoken, "name"))
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
					const size_t stringspace =
						strlen(value) + sizeof (( (local) ? numlocalskins : numskins )) + 1;
					char *value2 = Z_Malloc(stringspace, PU_STATIC, NULL);
					snprintf(value2, stringspace,
						"%s%d", value, ( (local) ? numlocalskins : numskins ));
					value2[stringspace - 1] = '\0';
					if (R_LocalSkinAvailable(value2, local) == -1)
					{
						STRBUFCPY(skin->name,
							value2);
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
			else if (!stricmp(stoken, "realname"))
			{ // Display name (eg. "Knuckles")
				realname = true;
				STRBUFCPY(skin->realname, value);
				for (value = skin->realname; *value; value++)
					if (*value == '_') *value = ' '; // turn _ into spaces.
				if (!hudname)
					STRBUFCPY(skin->hudname, skin->realname);
			}
			else if (!stricmp(stoken, "hudname"))
			{ // Life icon name (eg. "K.T.E")
				hudname = true;
				STRBUFCPY(skin->hudname, value);
				for (value = skin->hudname; *value; value++)
					if (*value == '_') *value = ' '; // turn _ into spaces.
				if (!realname)
					STRBUFCPY(skin->realname, skin->hudname);
			}

			else if (!stricmp(stoken, "sprite"))
			{
				strupr(value);
				memcpy(skin->sprite, value, sizeof skin->sprite);
			}
			else if (!stricmp(stoken, "facerank"))
			{
				strupr(value);
				memcpy(skin->facerank, value, sizeof(skin->facerank)-1);
				skin->facerank[sizeof(skin->facerank)-1] = '\0';
			}
			else if (!stricmp(stoken, "facewant"))
			{
				strupr(value);
				memcpy(skin->facewant, value, sizeof(skin->facewant)-1);
				skin->facewant[sizeof(skin->facewant)-1] = '\0';
			}
			else if (!stricmp(stoken, "facemmap"))
			{
				strupr(value);
				strncpy(skin->facemmap, value, sizeof(skin->facemmap)-1);
				skin->facemmap[sizeof(skin->facemmap)-1] = '\0';
			}

#define FULLPROCESS(field) else if (!stricmp(stoken, #field)) skin->field = get_number(value);
			// character type identification
			FULLPROCESS(flags)
#undef FULLPROCESS

#define GETKARTSTAT(field) \
	else if (!stricmp(stoken, #field)) \
	{ \
		skin->field = atoi(value); \
		if (skin->field < 1) skin->field = 1; \
		if (skin->field > 9) skin->field = 9; \
	}
			GETKARTSTAT(kartspeed)
			GETKARTSTAT(kartweight)
#undef GETKARTSTAT

			// custom translation table
			else if (!stricmp(stoken, "startcolor"))
				skin->starttranscolor = atoi(value);

			else if (!stricmp(stoken, "prefcolor"))
				skin->prefcolor = K_GetKartColorByName(value);
			else if (!stricmp(stoken, "highresscale"))
				skin->highresscale = FLOAT_TO_FIXED(atof(value));
			else
			{
				INT32 found = false;
				sfxenum_t i;
				// copy name of sounds that are remapped
				// for this skin
				for (i = 0; i < sfx_skinsoundslot0; i++)
				{
					if (!S_sfx[i].name)
						continue;
					if (S_sfx[i].skinsound != -1
						&& !stricmp(S_sfx[i].name,
							stoken + 2))
					{
						skin->soundsid[S_sfx[i].skinsound] =
							S_AddSoundFx(value+2, S_sfx[i].singularity, S_sfx[i].pitch, true);
						found = true;
					}
				}
				if (!found)
					CONS_Debug(DBG_SETUP, "R_AddSkins: Unknown keyword '%s' in S_SKIN lump# %d (WAD %s)\n", stoken, lump, wadfiles[wadnum]->filename);
			}
next_token:
			stoken = strtok(NULL, "\r\n= ");
		}
		free(buf2);

		lump++; // if no sprite defined use spirte just after this one
		if (skin->sprite[0] == '\0')
		{
			const char *csprname = W_CheckNameForNumPwad(wadnum, lump);

			// skip to end of this skin's frames
			lastlump = lump;
			while (W_CheckNameForNumPwad(wadnum,lastlump) && memcmp(W_CheckNameForNumPwad(wadnum, lastlump),csprname,4)==0)
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
			for (name = 0;sprnames[name][0] != '\0';name++)
				if (strncmp(sprnames[name], sprname, 4) == 0)
				{
					found = true;
					skin->spritedef = sprites[name];
				}

			// not found so make a new one
			// go through the entire current wad looking for our sprite
			// don't just mass add anything beginning with our four letters.
			// "HOODFACE" is not a sprite name.
			if (!found)
			{
				UINT16 localllump = 0, lstart = UINT16_MAX, lend = UINT16_MAX;
				const char *lname;

				while ((lname = W_CheckNameForNumPwad(wadnum,localllump)))
				{
					// If this is a valid sprite...
					if (!memcmp(lname,sprname,4) && lname[4] && lname[5] && lname[5] >= '0' && lname[5] <= '8')
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

		R_FlushTranslationColormapCache();

		CONS_Printf(M_GetText("Added skin '%s'\n"), skin->name);
#ifdef SKINVALUES
		( (local) ? localskin_cons_t : skin_cons_t )[( (local) ? numlocalskins : numskins )].value = ( (local) ? numlocalskins : numskins );
		( (local) ? localskin_cons_t : skin_cons_t )[( (local) ? numlocalskins : numskins )].strvalue = skin->name;
#endif

		// Update the forceskin possiblevalues
		if (!local)
		{
			Forceskin_cons_t[numskins+1].value = numskins;
			Forceskin_cons_t[numskins+1].strvalue = skins[numskins].name;
		}
		
		skin->localskin = local;
		// so we dont have to guess
		if (local)
			skin->localnum = numlocalskins;
		else
			skin->localnum = numskins;

		// add face graphics
		if (local) {
			ST_LoadLocalFaceGraphics(skin->facerank, skin->facewant, skin->facemmap, numlocalskins);
		} else {
			ST_LoadFaceGraphics(skin->facerank, skin->facewant, skin->facemmap, numskins);
		}

#ifdef HWRENDER
		if (rendermode == render_opengl)
			HWR_AddPlayerMD2(( (local) ? numlocalskins : numskins ), local);
#endif
		if (!local) {
			skinstats[skin->kartspeed-1][skin->kartweight-1][skinstatscount[skin->kartspeed-1][skin->kartweight-1]] = numskins;
			CONS_Debug(DBG_SETUP, M_GetText("Added %d to %d, %d\n"), numskins, skin->kartweight, skin->kartweight);
			skinstatscount[skin->kartspeed-1][skin->kartweight-1]++;
			CONS_Debug(DBG_SETUP, M_GetText("Incremented %d, %d to %d\n"), skin->kartspeed, skin->kartweight, skinstatscount[skin->kartspeed - 1][skin->kartweight - 1]);
			
			skinsorted[numskins] = numskins;
		}
		
		allskins[numallskins] = ( (local) ? localskins : skins )[( (local) ? numlocalskins : numskins )];

		( (local) ? numlocalskins++ : numskins++ );
		numallskins++;
	}

	//sortSkinGrid();

	return;
}
