// SONIC ROBO BLAST 2 KART
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

#include "r_main.h" // stplyr
#include "r_fps.h"
#include "r_things.h"
#include "r_plane.h"
#include "r_portal.h"
#include "r_local.h"
#include "doomdef.h"
#include "d_netfil.h" // blargh. for nameonly().
#include "console.h"
#include "g_game.h"
#include "p_tick.h"
#include "p_local.h"
#include "p_setup.h"
#include "p_slopes.h"
#include "st_stuff.h"
#include "i_video.h" // rendermode
#include "w_wad.h"
#include "z_zone.h"

#include "core/thread_pool.h"

#ifdef HWRENDER
#include "hardware/hw_md2.h"
#endif

#ifdef ROTSPRITE
#include "r_patchrotation.h"
#endif

#define MINZ (FRACUNIT*16)
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
INT16 *negonearray;
INT16 *screenheightarray;

//
// INITIALIZATION FUNCTIONS
//

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

INT32 R_ThingLightLevel(mobj_t* thing)
{
	INT32 lightlevel = thing->lightlevel;

	return lightlevel;
}

//
// Sprite rotation 0 is facing the viewer,
//  rotation 1 is one angle turn CLOCKWISE around the axis.
// This is not the same as the angle,
//  which increases counter clockwise (protractor).
// There was a lot of stuff grabbed wrong, so I changed it...
//

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
// GAME FUNCTIONS
//
UINT32 visspritecount, numvisiblesprites;

static UINT32 clippedvissprites;
static vissprite_t *visspritechunks[MAXVISSPRITES >> VISSPRITECHUNKBITS] = {NULL};


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
		sprtemp[frame].rotated[r] = NULL;
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
boolean R_AddSingleSpriteDef(const char *sprname, spritedef_t *spritedef, UINT16 wadnum, UINT16 startlump, UINT16 endlump)
{
	UINT16 l;
	UINT8 frame;
	UINT8 rotation;
	lumpinfo_t *lumpinfo;
	softwarepatch_t patch;
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
		if (memcmp(lumpinfo[l].name, sprname, 4))
			continue;

		frame = R_Char2Frame(lumpinfo[l].name[4]);
		rotation = (UINT8)(lumpinfo[l].name[5] - '0');

		if (frame >= 64 || !(R_ValidSpriteAngle(rotation))) // Give an actual NAME error -_-...
		{
			CONS_Alert(CONS_WARNING, M_GetText("Bad sprite name: %s\n"), W_CheckNameForNumPwad(wadnum,l));
			continue;
		}

		// skip NULL sprites from very old dmadds pwads
		if (W_LumpLengthPwad(wadnum, l) <= 8)
			continue;

		// store sprite info in lookup tables
		//FIXME : numspritelumps do not duplicate sprite replacements
		W_ReadLumpHeaderPwad(wadnum, l, &patch, (sizeof(INT16) *4), 0);
		spritecachedinfo[numspritelumps].width = (INT32)(SHORT(patch.width))<<FRACBITS;
		spritecachedinfo[numspritelumps].offset = (INT32)(SHORT(patch.leftoffset))<<FRACBITS;
		spritecachedinfo[numspritelumps].topoffset = (INT32)(SHORT(patch.topoffset))<<FRACBITS;
		spritecachedinfo[numspritelumps].height = (INT32)(SHORT(patch.height))<<FRACBITS;

		//BP: we cannot use special tric in hardware mode because feet in ground caused by z-buffer
		if (rendermode != render_none) // not for psprite
			spritecachedinfo[numspritelumps].topoffset += FEETADJUST;

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
		spritedef->spriteframes = static_cast<spriteframe_t*>(Z_Malloc(maxframe * sizeof (*spritedef->spriteframes), PU_STATIC, NULL));

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

	sprites = static_cast<spritedef_t*>(Z_Calloc(numsprites * sizeof (*sprites), PU_STATIC, NULL));

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
}

//
// R_ClearSprites
// Called at frame start.
//
void R_ClearSprites(void)
{
	visspritecount = numvisiblesprites = clippedvissprites = 0;
}

static INT16 *vissprite_clipbot[MAXVISSPRITES >> VISSPRITECHUNKBITS];
static INT16 *vissprite_cliptop[MAXVISSPRITES >> VISSPRITECHUNKBITS];

static void R_AllocVisSpriteChunkMemory(UINT32 chunk)
{
	vissprite_clipbot[chunk] = static_cast<INT16*>(Z_Realloc(vissprite_clipbot[chunk], sizeof(INT16) * (VISSPRITESPERCHUNK * viewwidth), PU_STATIC, NULL));
	vissprite_cliptop[chunk] = static_cast<INT16*>(Z_Realloc(vissprite_cliptop[chunk], sizeof(INT16) * (VISSPRITESPERCHUNK * viewwidth), PU_STATIC, NULL));

	for (unsigned i = 0; i < VISSPRITESPERCHUNK; i++)
	{
		vissprite_t *sprite = visspritechunks[chunk] + i;

		sprite->clipbot = vissprite_clipbot[chunk] + (viewwidth * i);
		sprite->cliptop = vissprite_cliptop[chunk] + (viewwidth * i);
	}
}

void R_AllocVisSpriteMemory(void)
{
	unsigned numchunks = MAXVISSPRITES >> VISSPRITECHUNKBITS;

	for (unsigned i = 0; i < numchunks; i++)
	{
		if (visspritechunks[i])
			R_AllocVisSpriteChunkMemory(i);
	}
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
		{
			Z_Malloc(sizeof(vissprite_t) * VISSPRITESPERCHUNK, PU_LEVEL, &visspritechunks[chunk]);
			R_AllocVisSpriteChunkMemory(chunk);
		}

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

void R_DrawMaskedColumn(drawcolumndata_t* dc, column_t *column)
{
	INT32 topscreen;
	INT32 bottomscreen;
	fixed_t basetexturemid;
	INT32 topdelta, prevdelta = 0;

	basetexturemid = dc->texturemid;

	while (column->topdelta != 0xff)
	{
		// calculate unclipped screen coordinates
		// for post
		topdelta = column->topdelta;
		if (topdelta <= prevdelta)
			topdelta += prevdelta;
		prevdelta = topdelta;
		topscreen = sprtopscreen + spryscale*topdelta;
		bottomscreen = topscreen + spryscale*column->length;

		dc->yl = (topscreen+FRACUNIT-1)>>FRACBITS;
		dc->yh = (bottomscreen-1)>>FRACBITS;

		if (windowtop != INT32_MAX && windowbottom != INT32_MAX)
		{
			if (windowtop > topscreen)
				dc->yl = (windowtop + FRACUNIT - 1)>>FRACBITS;
			if (windowbottom < bottomscreen)
				dc->yh = (windowbottom - 1)>>FRACBITS;
		}

		if (dc->yh >= mfloorclip[dc->x])
			dc->yh = mfloorclip[dc->x]-1;
		if (dc->yl <= mceilingclip[dc->x])
			dc->yl = mceilingclip[dc->x]+1;

		if (dc->yl < 0)
			dc->yl = 0;
		if (dc->yh >= vid.height)
			dc->yh = vid.height - 1;

		if (dc->yl <= dc->yh && dc->yh > 0 && column->length != 0)
		{
			dc->source = (UINT8 *)column + 3;
			dc->sourcelength = column->length;
			dc->texturemid = basetexturemid - (topdelta<<FRACBITS);

			// Drawn by R_DrawColumn.
			drawcolumndata_t dc_copy = *dc;
			void (*colfunccopy)(drawcolumndata_t*);
			colfunccopy = colfunc;
			colfunccopy(const_cast<drawcolumndata_t*>(&dc_copy));
		}
		column = (column_t *)((UINT8 *)column + column->length + 4);
	}

	dc->texturemid = basetexturemid;
}

INT32 lengthcol; // column->length : for flipped column function pointers and multi-patch on 2sided wall = texture->height

static void R_DrawFlippedMaskedColumn(drawcolumndata_t* dc, column_t *column)
{
	INT32 topscreen;
	INT32 bottomscreen;
	fixed_t basetexturemid = dc->texturemid;
	INT32 topdelta, prevdelta = -1;
	UINT8 *d,*s;

	while (column->topdelta != 0xff)
	{
		// calculate unclipped screen coordinates
		// for post
		topdelta = column->topdelta;
		if (topdelta <= prevdelta)
			topdelta += prevdelta;
		prevdelta = topdelta;
		topdelta = lengthcol-column->length-topdelta;
		topscreen = sprtopscreen + spryscale*topdelta;
		bottomscreen = sprbotscreen == INT32_MAX ? topscreen + spryscale*column->length
		                                      : sprbotscreen + spryscale*column->length;
		dc->yl = (topscreen+FRACUNIT-1)>>FRACBITS;
		dc->yh = (bottomscreen-1)>>FRACBITS;

		if (windowtop != INT32_MAX && windowbottom != INT32_MAX)
		{
			if (windowtop > topscreen)
				dc->yl = (windowtop + FRACUNIT - 1)>>FRACBITS;
			if (windowbottom < bottomscreen)
				dc->yh = (windowbottom - 1)>>FRACBITS;
		}

		if (dc->yh >= mfloorclip[dc->x])
			dc->yh = mfloorclip[dc->x]-1;
		if (dc->yl <= mceilingclip[dc->x])
			dc->yl = mceilingclip[dc->x]+1;
		if (dc->yl < 0)
			dc->yl = 0;
		if (dc->yh >= vid.height) // dc->yl must be < vid.height, so reduces number of checks in tight loop
			dc->yh = vid.height - 1;

		if (dc->yl <= dc->yh && dc->yh > 0 && column->length != 0)
		{
			dc->source = static_cast<UINT8*>(ZZ_Alloc(column->length));
			dc->sourcelength = column->length;

			for (s = (UINT8 *)column+2+column->length, d = dc->source; d < dc->source+column->length; --s)
				*d++ = *s;
			dc->texturemid = basetexturemid - (topdelta<<FRACBITS);

			// Still drawn by R_DrawColumn.
			drawcolumndata_t dc_copy = *dc;
			void (*colfunccopy)(drawcolumndata_t*);
			colfunccopy = colfunc;
			colfunccopy(const_cast<drawcolumndata_t*>(&dc_copy));

			Z_Free(dc->source);
		}
		column = (column_t *)((UINT8 *)column + column->length + 4);
	}

	dc->texturemid = basetexturemid;
}


// Based off of R_GetLinedefTransTable
transnum_t R_GetThingTransTable(fixed_t alpha, transnum_t transmap)
{
	return static_cast<transnum_t>((20*(FRACUNIT - ((alpha * (10 - transmap))/10) - 1) + FRACUNIT) >> (FRACBITS+1));
}

//
// R_DrawVisSprite
//  mfloorclip and mceilingclip should also be set.
//
static void R_DrawVisSprite(vissprite_t *vis)
{
	column_t *column;
	void (*localcolfunc)(drawcolumndata_t*, column_t *);
	INT32 texturecolumn;
	INT32 pwidth;
	fixed_t frac;
	patch_t *patch = vis->patch;
	fixed_t this_scale = vis->thingscale;
	INT32 x1, x2;
	INT64 overflow_test;
	drawcolumndata_t dc = {};

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
	if (vis->startfrac < 0 || vis->startfrac > (patch->width << FRACBITS))
	{
		// never draw vissprites with startfrac out of patch range
		return;
	}

	R_SetColumnFunc(BASEDRAWFUNC); // hack: this isn't resetting properly somewhere.
	dc.colormap = vis->colormap;
	if ((vis->mobj->flags & MF_BOSS) && (vis->mobj->flags2 & MF2_FRET) && (leveltime & 1)) // Bosses "flash"
	{
		R_SetColumnFunc(COLDRAWFUNC_TRANS); // translate certain pixels to white
		if (vis->mobj->type == MT_CYBRAKDEMON)
			dc.translation = R_GetTranslationColormap(TC_ALLWHITE, SKINCOLOR_NONE, GTC_CACHE);
		else if (vis->mobj->type == MT_METALSONIC_BATTLE)
			dc.translation = R_GetTranslationColormap(TC_METALSONIC, SKINCOLOR_NONE, GTC_CACHE);
		else
			dc.translation = R_GetTranslationColormap(TC_BOSS, SKINCOLOR_NONE, GTC_CACHE);
	}
	else if (vis->mobj->color && vis->transmap) // Color mapping
	{
		R_SetColumnFunc(COLDRAWFUNC_TRANSTRANS);
		dc.transmap = vis->transmap;
		if (vis->mobj->colorized)
			dc.translation = R_GetTranslationColormap(TC_RAINBOW, static_cast<skincolors_t>(vis->mobj->color), GTC_CACHE);
		else if (vis->mobj->skin && vis->mobj->sprite == SPR_PLAY) // MT_GHOST LOOKS LIKE A PLAYER SO USE THE PLAYER TRANSLATION TABLES. >_>
			dc.translation = R_GetLocalTranslationColormap(static_cast<skin_t*>(vis->mobj->skin), static_cast<skin_t*>(vis->mobj->localskin), static_cast<skincolors_t>(vis->mobj->color), GTC_CACHE, vis->mobj->skinlocal);
		else // Use the defaults
			dc.translation = R_GetTranslationColormap(TC_DEFAULT, static_cast<skincolors_t>(vis->mobj->color), GTC_CACHE);
	}
	else if (vis->transmap)
	{
		R_SetColumnFunc(COLDRAWFUNC_FUZZY);
		dc.transmap = vis->transmap;    //Fab : 29-04-98: translucency table
	}
	else if (vis->mobj->color)
	{
		// translate green skin to another color
		R_SetColumnFunc(COLDRAWFUNC_TRANS);

		// New colormap stuff for skins Tails 06-07-2002
		if (vis->mobj->colorized)
			dc.translation = R_GetTranslationColormap(TC_RAINBOW, static_cast<skincolors_t>(vis->mobj->color), GTC_CACHE);
		else if (vis->mobj->skin && vis->mobj->sprite == SPR_PLAY) // This thing is a player!
			dc.translation = R_GetLocalTranslationColormap(static_cast<skin_t*>(vis->mobj->skin), static_cast<skin_t*>(vis->mobj->localskin), static_cast<skincolors_t>(vis->mobj->color), GTC_CACHE, vis->mobj->skinlocal);
		else // Use the defaults
			dc.translation = R_GetTranslationColormap(TC_DEFAULT, static_cast<skincolors_t>(vis->mobj->color), GTC_CACHE);
	}
	else if (vis->mobj->sprite == SPR_PLAY) // Looks like a player, but doesn't have a color? Get rid of green sonic syndrome.
	{
		R_SetColumnFunc(COLDRAWFUNC_TRANS);
		dc.translation = R_GetTranslationColormap(TC_DEFAULT, SKINCOLOR_BLUE, GTC_CACHE);
	}

	if (vis->extra_colormap)
	{
		if (!dc.colormap)
			dc.colormap = vis->extra_colormap->colormap;
		else
			dc.colormap = &vis->extra_colormap->colormap[dc.colormap - colormaps];
	}
	if (!dc.colormap)
		dc.colormap = colormaps;

	if (encoremap && !vis->mobj->color && !(vis->mobj->flags & MF_DONTENCOREMAP))
		dc.colormap += COLORMAP_REMAPOFFSET;

	dc.texturemid = vis->texturemid;
	dc.texheight = patch->height;

	frac = vis->startfrac;
	windowtop = windowbottom = sprbotscreen = INT32_MAX;

	if (this_scale <= 0)
		this_scale = 1;

	if (this_scale != FRACUNIT)
	{
		if (!vis->isScaled)
		{
			vis->scale = FixedMul(vis->scale, this_scale);
			vis->scalestep = FixedMul(vis->scalestep, this_scale);
			vis->xiscale = FixedDiv(vis->xiscale, this_scale);
			vis->isScaled = true;
		}
		dc.texturemid = FixedDiv(dc.texturemid, this_scale);
	}

	spryscale = vis->scale;

	if (!(vis->scalestep))
	{
		sprtopscreen = centeryfrac - FixedMul(dc.texturemid, spryscale);
		dc.iscale = FixedDiv(FRACUNIT, vis->scale);
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

	localcolfunc = (vis->vflip) ? R_DrawFlippedMaskedColumn : R_DrawMaskedColumn;
	lengthcol = patch->height;
	pwidth = patch->width;

	// Split drawing loops for paper and non-paper to reduce conditional checks per sprite
	if (vis->scalestep)
	{
		fixed_t horzscale = FixedMul(vis->spritexscale, this_scale);
		fixed_t scalestep = FixedMul(vis->scalestep, vis->spriteyscale);

		// Papersprite drawing loop
		for (dc.x = vis->x1; dc.x <= vis->x2; dc.x++, spryscale += scalestep)
		{
			angle_t angle = ((vis->centerangle + xtoviewangle[dc.x]) >> ANGLETOFINESHIFT) & 0xFFF;
			texturecolumn = (vis->paperoffset - FixedMul(FINETANGENT(angle), vis->paperdistance)) / horzscale;

			if (texturecolumn < 0 || texturecolumn >= pwidth)
				continue;

			if (vis->xiscale < 0) // Flipped sprite
				texturecolumn = pwidth - 1 - texturecolumn;

			sprtopscreen = (centeryfrac - FixedMul(dc.texturemid, spryscale));
			dc.iscale = (0xffffffffu / (unsigned)spryscale);

			column = (column_t *)((UINT8 *)patch->columns + (patch->columnofs[texturecolumn]));

			localcolfunc(&dc, column);
		}
	}
	else
	{
		// Non-paper drawing loop
		for (dc.x = vis->x1; dc.x <= vis->x2; dc.x++, frac += vis->xiscale)
		{
			texturecolumn = CLAMP(frac >> FRACBITS, 0, pwidth - 1);
			column = (column_t *)((UINT8 *)patch->columns + (patch->columnofs[texturecolumn]));
			localcolfunc(&dc, column);
		}
	}

	R_SetColumnFunc(BASEDRAWFUNC);

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
	fixed_t this_scale = vis->thingscale;
	INT64 overflow_test;
	drawcolumndata_t dc = {};

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
		R_SetColumnFunc(COLDRAWFUNC_FUZZY);
		dc.transmap = vis->transmap;    //Fab : 29-04-98: translucency table
	}

	dc.colormap = colormaps;
	if (encoremap)
		dc.colormap += COLORMAP_REMAPOFFSET;

	dc.iscale = FixedDiv(FRACUNIT, vis->scale);
	dc.texturemid = FixedDiv(vis->texturemid, this_scale);
	dc.texheight = patch->height;

	frac = vis->startfrac;
	spryscale = vis->scale;
	sprtopscreen = centeryfrac - FixedMul(dc.texturemid,spryscale);
	windowtop = windowbottom = sprbotscreen = INT32_MAX;

	if (vis->x1 < 0)
		vis->x1 = 0;

	if (vis->x2 >= vid.width)
		vis->x2 = vid.width-1;

	for (dc.x = vis->x1; dc.x <= vis->x2; dc.x++, frac += vis->xiscale)
	{
		texturecolumn = frac>>FRACBITS;

		if (texturecolumn < 0 || texturecolumn >= patch->width)
		{
			CONS_Debug(DBG_RENDER, "R_DrawPrecipitationSpriteRange: bad texturecolumn\n");
			break;
		}

		column = (column_t *)((UINT8 *)patch->columns + (patch->columnofs[texturecolumn]));

		R_DrawMaskedColumn(&dc, column);
	}

	R_SetColumnFunc(BASEDRAWFUNC);
}

//
// R_SplitSprite
// runs through a sector's lightlist and splits the sprite according to the heights
//
static void R_SplitSprite(vissprite_t *sprite)
{
	sector_t *sector = sprite->sector;

	for (INT32 i = 1; i < sector->numlights; i++)
	{
		fixed_t testheight;

		if (!(sector->lightlist[i].caster->flags & FF_CUTSPRITES))
			continue;

		testheight = P_GetLightZAt(&sector->lightlist[i], sprite->gx, sprite->gy);

		if (testheight >= sprite->gzt)
			continue;
		if (testheight <= sprite->gz)
			return;

		INT16 cutfrac = (INT16)((centeryfrac - FixedMul(testheight - viewz, sprite->sortscale))>>FRACBITS);
		if (cutfrac < 0)
			continue;
		if (cutfrac > viewheight)
			return;

		// Found a split! Make a new sprite, copy the old sprite to it, and
		// adjust the heights.
		vissprite_t *newsprite = R_NewVisSprite();

		// Needs to keep the new sprite's clipping tables
		INT16 *cliptop = newsprite->cliptop;
		INT16 *clipbot = newsprite->clipbot;

		M_Memcpy(newsprite, sprite, sizeof (vissprite_t));

		newsprite->cliptop = cliptop;
		newsprite->clipbot = clipbot;

		sprite->cut = static_cast<spritecut_e>(sprite->cut | SC_BOTTOM);
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

		sprite->cut = static_cast<spritecut_e>(sprite->cut | SC_TOP);

		if (!(sector->lightlist[i].caster->flags & FF_NOSHADE))
		{
			INT32 lightnum = R_GetSoftLightlevel(*sector->lightlist[i].lightlevel);

			if (lightnum < 0)
				spritelights = scalelight[0];
			else if (lightnum >= LIGHTLEVELS)
				spritelights = scalelight[LIGHTLEVELS-1];
			else
				spritelights = scalelight[lightnum];

			newsprite->extra_colormap = sector->lightlist[i].extra_colormap;

			if (!((newsprite->cut & SC_FULLBRIGHT)
				&& (!newsprite->extra_colormap || !(newsprite->extra_colormap->fog & 1))))
			{
				INT32 lindex = FixedMul(sprite->xscale, LIGHTRESOLUTIONFIX)>>(LIGHTSCALESHIFT);

				// Mitigate against negative xscale and arithmetic overflow
				lindex = CLAMP(lindex, 0, MAXLIGHTSCALE - 1);

				if (newsprite->cut & SC_SEMIBRIGHT)
					lindex = (MAXLIGHTSCALE/2) + (lindex >>1);

				newsprite->colormap = spritelights[lindex];
			}
		}
		sprite = newsprite;
	}
}

// R_GetShadowZ(thing, shadowslope)
// Get the first visible floor below the object for shadows
// shadowslope is filled with the floor's slope, if provided
//
fixed_t R_GetShadowZ(mobj_t *thing, pslope_t **shadowslope)
{
	boolean isflipped = thing->eflags & MFE_VERTICALFLIP;
	fixed_t z, groundz = isflipped ? INT32_MAX : INT32_MIN;
	pslope_t *slope, *groundslope = NULL;
	msecnode_t *node;
	sector_t *sector;
	ffloor_t *rover;

#define CHECKZ (isflipped ? z > thing->z+thing->height/2 && z < groundz : z < thing->z+thing->height/2 && z > groundz)

	for (node = thing->touching_sectorlist; node; node = node->m_sectorlist_next)
	{
		sector = node->m_sector;

		slope = sector->heightsec != -1 ? NULL : (isflipped ? sector->c_slope : sector->f_slope);

		if (sector->heightsec != -1)
			z = isflipped ? sectors[sector->heightsec].ceilingheight : sectors[sector->heightsec].floorheight;
		else
			z = isflipped ? P_GetSectorCeilingZAt(sector, thing->x, thing->y) : P_GetSectorFloorZAt(sector, thing->x, thing->y);

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

				z = isflipped ? P_GetFFloorBottomZAt(rover, thing->x, thing->y) : P_GetFFloorTopZAt(rover, thing->x, thing->y);
				if CHECKZ
				{
					groundz = z;
					groundslope = isflipped ? *rover->b_slope : *rover->t_slope;
				}
			}
	}

	if (isflipped ? (thing->ceilingz < groundz - (!groundslope ? 0 : FixedMul(abs(groundslope->zdelta), thing->radius*3/2)))
		: (thing->floorz > groundz + (!groundslope ? 0 : FixedMul(abs(groundslope->zdelta), thing->radius*3/2))))
	{
		groundz = isflipped ? thing->ceilingz : thing->floorz;
		groundslope = NULL;
	}

	if (shadowslope != NULL)
		*shadowslope = groundslope;

	return groundz;
#undef CHECKZ
}

fixed_t R_GetSpriteDirectionalLighting(angle_t angle)
{
	// Copied from P_UpdateSegLightOffset
	const UINT8 contrast = std::min(std::max(0, maplighting.contrast - maplighting.backlight), UINT8_MAX);
	const fixed_t contrastFixed = ((fixed_t)contrast) * FRACUNIT;

	fixed_t light = FRACUNIT;
	fixed_t extralight = 0;

	light = FixedMul(FINECOSINE(angle >> ANGLETOFINESHIFT), FINECOSINE(maplighting.angle >> ANGLETOFINESHIFT))
		+ FixedMul(FINESINE(angle >> ANGLETOFINESHIFT), FINESINE(maplighting.angle >> ANGLETOFINESHIFT));
	light = (light + FRACUNIT) / 2;

	light = FixedMul(light, FRACUNIT - FSIN(abs(AngleDeltaSigned(angle, maplighting.angle)) / 2));

	extralight = -contrastFixed + FixedMul(light, contrastFixed * 2);

	return extralight;
}

//
// R_ProjectSprite
// Generates a vissprite for a thing
// if it might be visible.
//
static void R_ProjectSprite(mobj_t *thing)
{
	fixed_t tr_x, tr_y;
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

	INT32 lindex;
	INT32 trans;

	vissprite_t *vis;

	angle_t ang = 0; // gcc 4.6 and lower fix
#ifdef ROTSPRITE
	angle_t camang = 0;
#endif
	fixed_t iscale;
	fixed_t scalestep; // toast '16
	fixed_t offset, offset2;
	fixed_t paperoffset = 0, paperdistance = 0; angle_t centerangle = 0;

	//SoM: 3/17/2000
	fixed_t gz, gzt;
	INT32 heightsec, phs;
	INT32 light = 0;
	lighttable_t **lights_array = spritelights;
	fixed_t this_scale;
	fixed_t spritexscale, spriteyscale;

	// rotsprite
	fixed_t spr_width, spr_height;
	fixed_t spr_offset, spr_topoffset;

#ifdef ROTSPRITE
	patch_t *rotsprite = NULL;
	INT32 rollangle = 0;
	angle_t pitchnroll = 0;
	angle_t sliptiderollangle = 0;
#endif

	if (!thing || thing->subsector == NULL)
		return;

	mobj_t *oldthing = thing;

	const boolean mirrored = thing->mirrored;
	const boolean vflip = (thing->eflags & MFE_VERTICALFLIP);
	const boolean hflip = (!(thing->frame & FF_HORIZONTALFLIP) != !mirrored);
	const boolean papersprite = (thing->frame & FF_PAPERSPRITE);

	// uncapped/interpolation
	interpmobjstate_t interp = {};

	// do interpolation
	if (R_UsingFrameInterpolation() && !paused && R_CheckInterpDist(oldthing))
	{
		R_InterpolateMobjState(oldthing, rendertimefrac, &interp);
	}
	else
	{
		R_InterpolateMobjState(oldthing, FRACUNIT, &interp);
	}

	this_scale = interp.scale;

	if (this_scale < 1)
		return;

	// transform the origin point
	tr_x = interp.x - viewx;
	tr_y = interp.y - viewy;

	tz = FixedMul(tr_x, viewcos) + FixedMul(tr_y, viewsin); // near/far distance

	// thing is behind view plane?
	if (!papersprite && (tz < FixedMul(MINZ, this_scale))) // papersprite clipping is handled later
		return;

	tx = FixedMul(tr_x, viewsin) - FixedMul(tr_y, viewcos); // sideways distance

	// too far off the side?
	if (!papersprite && abs(tx) > (INT64)FixedMul(tz, fovtan)<<2) // papersprite clipping is handled later
		return;

	// aspect ratio stuff
	xscale = FixedDiv(projection, tz);
	sortscale = FixedDiv(projectiony, tz);

	// decide which patch to use for sprite relative to player
#ifdef RANGECHECK
	if ((size_t)(thing->sprite) >= numsprites)
		I_Error("R_ProjectSprite: invalid sprite number %d ", thing->sprite);
#endif

	rot = (thing->frame & FF_FRAMEMASK);

#ifdef ROTSPRITE
	// determine here if sprite should rotate for optimization
	const boolean sliprollrotate = (cv_sliptideroll.value && (thing->player && thing->player->sliproll));
	const boolean shouldrotate = (interp.sloperoll || interp.slopepitch || interp.roll || interp.pitch || thing->rollangle || sliprollrotate);
#endif

	//Fab : 02-08-98: 'skin' override spritedef currently used for skin
	if ((thing->skin || thing->localskin) && thing->sprite == SPR_PLAY)
	{
		sprdef = &K_GetMobjSkin(thing)->spritedef;
#ifdef ROTSPRITE
		sprinfo = &K_GetMobjSkin(thing)->sprinfo;
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
		rot = (thing->frame & FF_FRAMEMASK);
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

	if (sprframe->rotate != SRF_SINGLE || papersprite
#ifdef ROTSPRITE
		|| (shouldrotate)
#endif
	)
	{
		ang = R_PointToAngle(interp.x, interp.y);
#ifdef ROTSPRITE
		camang = ang;
#endif
		ang -= interp.angle;

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

		if ((sprframe->rotate & SRF_RIGHT) && (ang < ANGLE_180)) // See from right
			rot = 6; // F7 slot
		else if ((sprframe->rotate & SRF_LEFT) && (ang >= ANGLE_180)) // See from left
			rot = 2; // F3 slot
		else // Normal behaviour
			rot = (ang+ANGLE_202h)>>29;

		//Fab: lumpid is the index for spritewidth,spriteoffset... tables
		lump = sprframe->lumpid[rot];
		flip = sprframe->flip & (1<<rot);
	}

	I_Assert(lump < max_spritelumps);

	spr_width = spritecachedinfo[lump].width;
	spr_height = spritecachedinfo[lump].height;
	spr_offset = spritecachedinfo[lump].offset;
	spr_topoffset = spritecachedinfo[lump].topoffset;

#ifdef ROTSPRITE
	if (shouldrotate)
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
			pitchnroll = R_RotationAngle(ang, camang, &interp);
			rollangle = thing->rollangle;
		}

		if (rollangle || pitchnroll || sliprollrotate)
		{
			if (sliprollrotate)
			{
				sliptiderollangle = thing->player->sliproll * thing->player->kartstuff[k_aizdriftstrat];
				pitchnroll += rollangle + FixedMul(FINECOSINE((ang) >> ANGLETOFINESHIFT), sliptiderollangle);
			}
			else
				pitchnroll += rollangle;

			rollangle = R_GetRollAngle(pitchnroll);
			rotsprite = Patch_GetRotatedSprite(sprframe, (thing->frame & FF_FRAMEMASK), rot, flip, sprinfo, rollangle);

			if (rotsprite != NULL)
			{
				spr_width = rotsprite->width << FRACBITS;
				spr_height = rotsprite->height << FRACBITS;
				spr_offset = rotsprite->leftoffset << FRACBITS;
				spr_topoffset = rotsprite->topoffset << FRACBITS;
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

	if ((thing->skin || thing->localskin) && K_GetMobjSkin(thing)->flags & SF_HIRES)
	{
		fixed_t highresscale = ((skin_t *)thing->skin)->highresscale;
		spritexscale = FixedMul(spritexscale, highresscale);
		spriteyscale = FixedMul(spriteyscale, highresscale);
	}

	if (spritexscale < 1 || spriteyscale < 1)
		return;

	spr_offset += interp.spritexoffset;
	spr_topoffset += interp.spriteyoffset;

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
		tz = FixedMul(tr_x, viewcos) + FixedMul(tr_y, viewsin);

		tx = FixedMul(tr_x, viewsin) - FixedMul(tr_y, viewcos);

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
		tz2 = FixedMul(tr_x, viewcos) + FixedMul(tr_y, viewsin);

		tx2 = FixedMul(tr_x, viewsin) - FixedMul(tr_y, viewcos);

		if (std::max(tz, tz2) < FixedMul(MINZ, this_scale)) // non-papersprite clipping is handled earlier
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

		if ((range = x2 - x1) <= 0)
			return;

		range++; // fencepost problem

		if (range > INT16_MAX)
		{
			// If the range happens to be too large for fixed_t,
			// abort the draw to avoid xscale becoming negative due to arithmetic overflow.
			return;
		}

		// Compatibility with MSVC - SSNTails
		scalestep = ((yscale2 - yscale)/range);

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
		x1 = centerx + (FixedMul(tx,xscale) / FRACUNIT);

		// off the right side?
		if (x1 > viewwidth)
			return;

		tx += offset2;
		x2 = (centerx + (FixedMul(tx,xscale) / FRACUNIT)) - 1;

		// off the left side
		if (x2 < 0)
			return;
	}

	// PORTAL SPRITE CLIPPING
	if (portalrender && portalclipline)
	{
		if (x2 < portalclipstart || x1 >= portalclipend)
			return;

		if (P_PointOnLineSide(interp.x, interp.y, portalclipline) != 0)
			return;
	}

	// Determine the blendmode and translucency value
	INT32 blendmode;
	if (oldthing->frame & FF_BLENDMASK)
		blendmode = ((oldthing->frame & FF_BLENDMASK) >> FF_BLENDSHIFT) + 1;
	else
		blendmode = oldthing->blendmode;

	if (oldthing->flags2 & MF2_SHADOW || thing->flags2 & MF2_SHADOW) // actually only the player should use this (temporary invisibility)
		trans = tr_trans80; // because now the translucency is set through FF_TRANSMASK
	else if (oldthing->frame & FF_TRANSMASK)
	{
		trans = (oldthing->frame & FF_TRANSMASK) >> FF_TRANSSHIFT;
		if (!R_BlendLevelVisible(blendmode, trans))
			return;
	}
	else
		trans = 0;

	if (cv_playerfade.value && oldthing->player)
		trans = static_cast<INT32>(R_GetThingTransTable(R_DoPlayerFade(oldthing), static_cast<transnum_t>(trans)));

	//SoM: 3/17/2000: Disregard sprites that are out of view..
	if (vflip)
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

	if (oldthing->frame & FF_ABSOLUTELIGHTLEVEL)
	{
		const UINT8 n = R_ThingLightLevel(oldthing);
		// n = uint8 aka 0 - 255, so the shift will always be 0 - LIGHTLEVELS - 1
		lights_array = scalelight[n >> LIGHTSEGSHIFT];
	}
	else
	{
		INT32 lightnum;

		if (thing->subsector->sector->numlights)
		{
			light = thing->subsector->sector->numlights - 1;

			// R_GetPlaneLight won't work on sloped lights!
			for (lightnum = 1; lightnum < thing->subsector->sector->numlights; lightnum++)
			{
				fixed_t h = P_GetLightZAt(&thing->subsector->sector->lightlist[lightnum], interp.x, interp.y);

				if (h <= gzt)
				{
					light = lightnum - 1;
					break;
				}
			}

			lightnum = *thing->subsector->sector->lightlist[light].lightlevel;
		}
		else
		{
			lightnum = thing->subsector->sector->lightlevel;
		}

		lightnum = R_GetSoftLightlevel(lightnum + R_ThingLightLevel(thing));

		if (maplighting.directional && P_SectorUsesDirectionalLighting(thing->subsector->sector))
		{
			fixed_t extralight = R_GetSpriteDirectionalLighting(papersprite
					? interp.angle + (ang >= ANGLE_180 ? -ANGLE_90 : ANGLE_90)
					: R_PointToAngle(interp.x, interp.y));

			// Krangle contrast in 3P/4P because scalelight
			// scales differently depending on the screen
			// width (which is halved in 3P/4P).
			if (splitscreen > 1)
			{
				extralight *= 2;
			}

			// Less change in contrast in dark sectors
			extralight = FixedMul(extralight, std::min(std::max(0, lightnum), LIGHTLEVELS - 1) * FRACUNIT / (LIGHTLEVELS - 1));

			if (papersprite)
			{
				// Papersprite contrast should match walls
				lightnum += FixedFloor((extralight / 8) + (FRACUNIT / 2)) / FRACUNIT;
			}
			else
			{
				fixed_t n = FixedDiv(FixedMul(xscale, LIGHTRESOLUTIONFIX), ((MAXLIGHTSCALE-1) << LIGHTSCALESHIFT));

				// Less change in contrast at further distances, to counteract DOOM diminished light
				extralight = FixedMul(extralight, std::min(n, FRACUNIT));

				// Contrast is stronger for normal sprites, stronger than wall lighting is at the same distance
				lightnum += FixedFloor((extralight / 4) + (FRACUNIT / 2)) / FRACUNIT;
			}
		}

		if (lightnum < 0)
			lights_array = scalelight[0];
		else if (lightnum >= LIGHTLEVELS)
			lights_array = scalelight[LIGHTLEVELS-1];
		else
			lights_array = scalelight[lightnum];
	}

	heightsec = thing->subsector->sector->heightsec;
	if (viewplayer && viewplayer->mo && viewplayer->mo->subsector)
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

	vis->mobj = thing; // Easy access! Tails 06-07-2002

	vis->x1 = x1 < portalclipstart ? portalclipstart : x1;
	vis->x2 = x2 >= portalclipend ? portalclipend-1 : x2;

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

	if ((blendmode != AST_COPY) && cv_translucency.value)
		vis->transmap = R_GetBlendTable(blendmode, trans);
	else
		vis->transmap = NULL;

	if (R_ThingIsFullBright(oldthing) || oldthing->flags2 & MF2_SHADOW || thing->flags2 & MF2_SHADOW)
		vis->cut = static_cast<spritecut_e>(vis->cut | SC_FULLBRIGHT);
	else if (R_ThingIsSemiBright(oldthing))
		vis->cut = static_cast<spritecut_e>(vis->cut | SC_SEMIBRIGHT);
	else if (R_ThingIsFullDark(oldthing))
		vis->cut = static_cast<spritecut_e>(vis->cut | SC_FULLDARK);

	//
	// determine the colormap (lightlevel & special effects)
	//

	if ((vis->cut & SC_FULLBRIGHT)
		&& (!vis->extra_colormap || !(vis->extra_colormap->fog & 1)))
	{
		// full bright: goggles
		vis->colormap = colormaps;
	}
	else if (vis->cut & SC_FULLDARK)
		vis->colormap = scalelight[0][0];
	else
	{
		// diminished light
		lindex = FixedMul(xscale, LIGHTRESOLUTIONFIX)>>(LIGHTSCALESHIFT);

		// Mitigate against negative xscale and arithmetic overflow
		lindex = CLAMP(lindex, 0, MAXLIGHTSCALE - 1);

		if (vis->cut & SC_SEMIBRIGHT)
			lindex = (MAXLIGHTSCALE/2) + (lindex >> 1);

		vis->colormap = lights_array[lindex];
	}

	//Fab: lumppat is the lump number of the patch to use, this is different
	//     than lumpid for sprites-in-pwad : the graphics are patched
#ifdef ROTSPRITE
	if (rotsprite != NULL)
		vis->patch = rotsprite;
	else
#endif
		vis->patch = static_cast<patch_t*>(W_CachePatchNum(sprframe->lumppat[rot], PU_SPRITE));

	vis->precip = false;

	vis->vflip = vflip;

	vis->isScaled = false;

	if (thing->subsector->sector->numlights)
		R_SplitSprite(vis);

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
	fixed_t gz, gzt;
	fixed_t this_scale;

	if (!thing || thing->subsector == NULL)
		return;

	// okay... this is a hack, but weather isn't networked, so it should be ok
	if (!P_PrecipThinker(thing))
	{
		return;
	}

	// uncapped/interpolation
	interpmobjstate_t interp = {};

	// do interpolation
	if (R_UsingFrameInterpolation() && !paused && R_CheckInterpDist((mobj_t*)thing))
	{
		R_InterpolatePrecipMobjState(thing, rendertimefrac, &interp);
	}
	else
	{
		R_InterpolatePrecipMobjState(thing, FRACUNIT, &interp);
	}

	this_scale = interp.scale;

	// transform the origin point
	tr_x = interp.x - viewx;
	tr_y = interp.y - viewy;

	gxt = FixedMul(tr_x, viewcos);
	gyt = -FixedMul(tr_y, viewsin);

	tz = gxt - gyt;

	// thing is behind view plane?
	if (tz < FixedMul(MINZ, this_scale))
		return;

	gxt = -FixedMul(tr_x, viewsin);
	gyt = FixedMul(tr_y, viewcos);
	tx = -(gyt + gxt);

	// too far off the side?
	if (abs(tx) > tz<<2)
		return;

	// aspect ratio stuff :
	xscale = FixedDiv(projection, tz);

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
	tx -= FixedMul(spritecachedinfo[lump].offset, this_scale);
	x1 = (centerxfrac + FixedMul (tx,xscale)) >>FRACBITS;

	// off the right side?
	if (x1 > viewwidth)
		return;

	tx += FixedMul(spritecachedinfo[lump].width, this_scale);
	x2 = ((centerxfrac + FixedMul (tx,xscale)) >>FRACBITS) - 1;

	// off the left side
	if (x2 < 0)
		return;

	// PORTAL SPRITE CLIPPING
	if (portalrender && portalclipline)
	{
		if (x2 < portalclipstart || x1 >= portalclipend)
			return;

		if (P_PointOnLineSide(interp.x, interp.y, portalclipline) != 0)
			return;
	}

	//SoM: 3/17/2000: Disregard sprites that are out of view..
	gzt = interp.z + FixedMul(spritecachedinfo[lump].topoffset, this_scale);
	gz = gzt - FixedMul(spritecachedinfo[lump].height, this_scale);

	if (thing->subsector->sector->cullheight)
	{
		if (R_DoCulling(thing->subsector->sector->cullheight, viewsector->cullheight, viewz, gz, gzt))
			return;
	}

	// aspect ratio stuff :
	yscale = FixedDiv(projectiony, tz);

	// store information in a vissprite
	vis = R_NewVisSprite();
	vis->scale = FixedMul(yscale, this_scale);
	vis->sortscale = yscale; //<<detailshift;
	vis->thingscale = interp.scale;
	vis->dispoffset = 0; // Monster Iestyn: 23/11/15
	vis->gx = interp.x;
	vis->gy = interp.y;
	vis->gz = gz;
	vis->gzt = gzt;
	vis->thingheight = 4 << FRACBITS;
	vis->pz = interp.z;
	vis->pzt = vis->pz + vis->thingheight;
	vis->texturemid = vis->gzt - viewz;
	vis->scalestep = 0;
	vis->paperdistance = 0;

	vis->x1 = x1 < portalclipstart ? portalclipstart : x1;
	vis->x2 = x2 >= portalclipend ? portalclipend-1 : x2;

	vis->xscale = xscale; //SoM: 4/17/2000
	vis->sector = thing->subsector->sector;
	vis->szt = (INT16)((centeryfrac - FixedMul(vis->gzt - viewz, yscale))>>FRACBITS);
	vis->sz = (INT16)((centeryfrac - FixedMul(vis->gz - viewz, yscale))>>FRACBITS);

	iscale = FixedDiv(FRACUNIT, xscale);

	vis->startfrac = 0;
	vis->xiscale = FixedDiv(iscale, this_scale);

	if (vis->x1 > x1)
		vis->startfrac += vis->xiscale*(vis->x1-x1);

	//Fab: lumppat is the lump number of the patch to use, this is different
	//     than lumpid for sprites-in-pwad : the graphics are patched
	vis->patch = static_cast<patch_t*>(W_CachePatchNum(sprframe->lumppat[0], PU_SPRITE));

	// specific translucency
	if ((thing->blendmode != AST_COPY) && cv_translucency.value)
		vis->transmap = R_GetTranslucencyTable((thing->frame & FF_TRANSMASK) >> FF_TRANSSHIFT);
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
	vis->isScaled = false;
}

// R_AddSprites
// During BSP traversal, this adds sprites by sector.
//
void R_AddSprites(sector_t *sec, INT32 lightlevel)
{
	mobj_t *thing;
	INT32 lightnum;

	if (rendermode != render_soft)
		return;

	// BSP is traversed by subsector.
	// A sector might have been split into several
	// subsectors during BSP building.
	// Thus we check whether its already added.
	if (sec->validcount == validcount)
		return;

	// Well, now it will be done.
	sec->validcount = validcount;

	if (!sec->numlights)
	{
		if (sec->heightsec == -1) lightlevel = sec->lightlevel;

		lightnum = R_GetSoftLightlevel(lightlevel);

		if (lightnum < 0)
			spritelights = scalelight[0];
		else if (lightnum >= LIGHTLEVELS)
			spritelights = scalelight[LIGHTLEVELS-1];
		else
			spritelights = scalelight[lightnum];
	}

	// Handle all things in sector.
	// If a limit exists, handle things a tiny bit different.
	const INT32 limit_dist = cv_drawdist.value;

	for (thing = sec->thinglist; thing; thing = thing->snext)
	{
		if (!R_ThingWithinDist(thing, limit_dist))
			continue;

		if (!R_ThingVisible(thing))
			continue;

		R_ProjectSprite(thing);
	}
}

// R_AddPrecipitationSprites
// This renders through the blockmap instead of BSP to avoid
// iterating a huge amount of precipitation sprites in sectors
// that are beyond drawdist.
//
void R_AddPrecipitationSprites(void)
{
	INT32 xl, xh, yl, yh, bx, by;
	precipmobj_t *th, *next;

	// save a little time if theres no or invisible weather
	if (curWeather == PRECIP_NONE || curWeather == PRECIP_BLANK || curWeather == PRECIP_STORM_NORAIN)
	{
		return;
	}

	const fixed_t precipscale = (cv_mobjscaleprecip.value ? mapobjectscale : FRACUNIT);
	const fixed_t drawdist = ((fixed_t)(cv_drawdist_precip.value) * precipscale);

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
				// Store this beforehand because R_ProjectPrecipitationSprite may free th (see P_PrecipThinker)
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
static void R_SortVisSprites(vissprite_t* vsprsortedhead, UINT32 start, UINT32 end)
{
	UINT32       i;
	vissprite_t *ds, *dsprev, *dsnext, *dsfirst;
	vissprite_t *best = NULL;
	vissprite_t  unsorted;
	fixed_t      bestscale;
	INT32        bestdispoffset;

	unsorted.next = unsorted.prev = &unsorted;

	dsfirst = R_GetVisSprite(start);

	// The first's prev and last's next will be set to
	// nonsense, but are fixed in a moment
	for (i = start, dsnext = dsfirst, ds = NULL; i < end; i++)
	{
		dsprev = ds;
		ds = dsnext;
		if (i < end - 1)
			dsnext = R_GetVisSprite(i + 1);

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
	vsprsortedhead->next = vsprsortedhead->prev = vsprsortedhead;
	for (i = start; i < end; i++)
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
			best->next = vsprsortedhead;
			best->prev = vsprsortedhead->prev;
			vsprsortedhead->prev->next = best;
			vsprsortedhead->prev = best;
		}
	}
}

//
// R_CreateDrawNodes
// Creates and sorts a list of drawnodes for the scene being rendered.
static drawnode_t *R_CreateDrawNode(drawnode_t *link);

static drawnode_t nodebankhead;

static void R_CreateDrawNodes(maskcount_t* mask, drawnode_t* head, boolean tempskip)
{
	drawnode_t *entry;
	drawseg_t *ds;
	INT32 i, p, best, x1, x2;
	fixed_t bestdelta, delta;
	vissprite_t *rover;
	static vissprite_t vsprsortedhead;
	drawnode_t *r2;
	visplane_t *plane;
	INT32 sintersect;
	fixed_t scale = 0;
	const INT32 vidheight = vid.height;

	// Add the 3D floors, thicksides, and masked textures...
	for (ds = drawsegs + mask->drawsegs[1]; ds-- > drawsegs + mask->drawsegs[0];)
	{
		if (ds->numthicksides)
		{
			for (i = 0; i < ds->numthicksides; i++)
			{
				entry = R_CreateDrawNode(head);
				entry->thickseg = ds;
				entry->ffloor = ds->thicksides[i];
			}
		}
		// Check for a polyobject plane, but only if this is a front line
		if (ds->curline->polyseg && ds->curline->polyseg->visplane && !ds->curline->side)
		{
			plane = ds->curline->polyseg->visplane;
			R_PlaneBounds(plane);

			if (plane->low < 0 || plane->high > vidheight || plane->high > plane->low)
				;
			else
			{
				// Put it in!
				entry = R_CreateDrawNode(head);
				entry->plane = plane;
				entry->seg = ds;
			}
			ds->curline->polyseg->visplane = NULL;
		}
		if (ds->maskedtexturecol)
		{
			entry = R_CreateDrawNode(head);
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

					if (plane->low < 0 || plane->high > vidheight || plane->high > plane->low || plane->polyobj)
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
					entry = R_CreateDrawNode(head);
					entry->plane = ds->ffloorplanes[best];
					entry->seg = ds;
					ds->ffloorplanes[best] = NULL;
				}
				else
					break;
			}
		}
	}

	if (tempskip)
		return;

	// find all the remaining polyobject planes and add them on the end of the list
	// probably this is a terrible idea if we wanted them to be sorted properly
	// but it works getting them in for now
	for (i = 0; i < numPolyObjects; i++)
	{
		if (!PolyObjects[i].visplane)
			continue;

		plane = PolyObjects[i].visplane;
		R_PlaneBounds(plane);

		if (plane->low < 0 || plane->high > vidheight || plane->high > plane->low)
		{
			PolyObjects[i].visplane = NULL;
			continue;
		}

		entry = R_CreateDrawNode(head);
		entry->plane = plane;
		// note: no seg is set, for what should be obvious reasons
		PolyObjects[i].visplane = NULL;
	}

	// No vissprites in this mask?
	if (mask->vissprites[1] - mask->vissprites[0] == 0)
		return;

	R_SortVisSprites(&vsprsortedhead, mask->vissprites[0], mask->vissprites[1]);

	for (rover = vsprsortedhead.prev; rover != &vsprsortedhead; rover = rover->prev)
	{
		if (rover->szt > vidheight || rover->sz < 0)
			continue;

		sintersect = (rover->x1 + rover->x2) / 2;

		for (r2 = head->next; r2 != head; r2 = r2->next)
		{
			if (r2->plane)
			{
				fixed_t planeobjectz, planecameraz;
				if (r2->plane->minx > rover->x2 || r2->plane->maxx < rover->x1)
					continue;
				if (rover->szt > r2->plane->low || rover->sz < r2->plane->high)
					continue;

				// Effective height may be different for each comparison in the case of slopes
				planeobjectz = P_GetZAt(r2->plane->slope, rover->gx, rover->gy, r2->plane->height);
				planecameraz = P_GetZAt(r2->plane->slope,     viewx,     viewy, r2->plane->height);

				// bird: if any part of the sprite peeks in front the plane
				if (planecameraz < viewz)
				{
					if (rover->gzt >= planeobjectz)
						continue;
				}
				else if (planecameraz > viewz)
				{
					if (rover->gz <= planeobjectz)
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
				//fixed_t topplaneobjectz, topplanecameraz, botplaneobjectz, botplanecameraz;

				if (rover->x1 > r2->thickseg->x2 || rover->x2 < r2->thickseg->x1)
					continue;

				scale = r2->thickseg->scale1 > r2->thickseg->scale2 ? r2->thickseg->scale1 : r2->thickseg->scale2;
				if (scale <= rover->sortscale)
					continue;

				scale = r2->thickseg->scale1 + (r2->thickseg->scalestep * (sintersect - r2->thickseg->x1));
				if (scale <= rover->sortscale)
					continue;

				// bird: Always sort sprites behind segs. This helps the plane
				// sorting above too. Basically if the sprite gets sorted behind
				// the seg here, it will be behind the plane too, since planes
				// are added after segs in the list.
#if 0
				topplaneobjectz = P_GetFFloorTopZAt   (r2->ffloor, rover->gx, rover->gy);
				topplanecameraz = P_GetFFloorTopZAt   (r2->ffloor,     viewx,     viewy);
				botplaneobjectz = P_GetFFloorBottomZAt(r2->ffloor, rover->gx, rover->gy);
				botplanecameraz = P_GetFFloorBottomZAt(r2->ffloor,     viewx,     viewy);

				if ((topplanecameraz > viewz && botplanecameraz < viewz) ||
				    (topplanecameraz < viewz && rover->gzt < topplaneobjectz) ||
				    (botplanecameraz > viewz && rover->gz > botplaneobjectz))
#endif
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
		if (r2 == head)
		{
			entry = R_CreateDrawNode(head);
			entry->sprite = rover;
		}
	}
}

static drawnode_t *R_CreateDrawNode(drawnode_t *link)
{
	drawnode_t *node = nodebankhead.next;

	if (node == &nodebankhead)
	{
		node = static_cast<drawnode_t*>(malloc(sizeof (*node)));
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

static void R_ClearDrawNodes(drawnode_t* head)
{
	drawnode_t *rover;
	drawnode_t *next;

	for (rover = head->next; rover != head;)
	{
		next = rover->next;
		R_DoneWithNode(rover);
		rover = next;
	}

	head->next = head->prev = head;
}

void R_InitDrawNodes(void)
{
	nodebankhead.next = nodebankhead.prev = &nodebankhead;
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

	fixed_t texturemid = 0, yscale = 0, scalestep = spr->scalestep; // "= 0" pleases the compiler
	INT32 height = 0;

	if (scalestep)
	{
		height = spr->patch->height;
		yscale = spr->scale;
		scalestep = FixedMul(scalestep, spr->spriteyscale);

		if (spr->thingscale != FRACUNIT)
			texturemid = FixedDiv(spr->texturemid, std::max(spr->thingscale, 1));
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
static void R_ClipVisSprite(vissprite_t *spr, INT32 x1, INT32 x2, portal_t* portal)
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
			__builtin_prefetch(curr + 1, 0, 3);

			// determine if the drawseg obscures the sprite
			if (curr->x1 > x2 || curr->x2 < x1)
			{
				// does not cover sprite
				continue;
			}

			ds = curr->user;

			if (ds->portalpass != 66) // unused?
			{
				if (ds->portalpass > 0 && ds->portalpass <= portalrender)
					continue; // is a portal

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
					!R_PointOnSegSide(spr->gx, spr->gy, ds->curline)))
				{
					// seg is behind sprite
					continue;
				}
			}

			r1 = ds->x1 < x1 ? x1 : ds->x1;
			r2 = ds->x2 > x2 ? x2 : ds->x2;

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
		const INT32 phs = viewplayer->mo->subsector->sector->heightsec;

		if ((mh = sectors[spr->heightsec].floorheight) > spr->gz &&
			(h = centeryfrac - FixedMul(mh -= viewz, spr->sortscale)) >= 0 &&
			(h >>= FRACBITS) < viewheight)
		{
			if (mh <= 0 || (phs != -1 && viewz > sectors[phs].floorheight))
			{                          // clip bottom
				for (x = x1; x <= x2; x++)
					if (spr->clipbot[x] == -2 || h < spr->clipbot[x])
						spr->clipbot[x] = (INT16)h;
			}
			else						// clip top
			{
				for (x = x1; x <= x2; x++)
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
				for (x = x1; x <= x2; x++)
					if (spr->clipbot[x] == -2 || h < spr->clipbot[x])
						spr->clipbot[x] = (INT16)h;
			}
			else                       // clip top
			{
				for (x = x1; x <= x2; x++)
					if (spr->cliptop[x] == -2 || h > spr->cliptop[x])
						spr->cliptop[x] = (INT16)h;
			}
		}
	}

	if (spr->cut & SC_TOP && spr->cut & SC_BOTTOM)
	{
		for (x = x1; x <= x2; x++)
		{
			if (spr->cliptop[x] == -2 || spr->szt > spr->cliptop[x])
				spr->cliptop[x] = spr->szt;

			if (spr->clipbot[x] == -2 || spr->sz < spr->clipbot[x])
				spr->clipbot[x] = spr->sz;
		}
	}
	else if (spr->cut & SC_TOP)
	{
		for (x = x1; x <= x2; x++)
		{
			if (spr->cliptop[x] == -2 || spr->szt > spr->cliptop[x])
				spr->cliptop[x] = spr->szt;
		}
	}
	else if (spr->cut & SC_BOTTOM)
	{
		for (x = x1; x <= x2; x++)
		{
			if (spr->clipbot[x] == -2 || spr->sz < spr->clipbot[x])
				spr->clipbot[x] = spr->sz;
		}
	}

	// all clipping has been performed, so store the values - what, did you think we were drawing them NOW?

	// check for unclipped columns
	for (x = x1; x <= x2; x++)
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
			spr->cut = static_cast<spritecut_e>(spr->cut | SC_NOTVISIBLE);
	}

	if (portal)
	{
		INT32 start_index = std::max(portal->start, x1);
		INT32 end_index = std::min(portal->start + portal->end - portal->start, x2);

		for (x = x1; x < start_index; x++)
		{
			spr->clipbot[x] = -1;
			spr->cliptop[x] = -1;
		}
		for (x = start_index; x <= end_index; x++)
		{
			if (spr->clipbot[x] > portal->floorclip[x - portal->start])
				spr->clipbot[x] = portal->floorclip[x - portal->start];
			if (spr->cliptop[x] < portal->ceilingclip[x - portal->start])
				spr->cliptop[x] = portal->ceilingclip[x - portal->start];
		}
		for (x = end_index + 1; x <= x2; x++)
		{
			spr->clipbot[x] = -1;
			spr->cliptop[x] = -1;
		}
	}
}

void R_ClipSprites(drawseg_t* dsstart, portal_t* portal)
{
	const size_t maxdrawsegs = ds_p - dsstart;
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
			drawsegs_xranges[i].items = static_cast<drawseg_xrange_item_t*>(Z_Realloc(
				drawsegs_xranges[i].items,
				drawsegs_xrange_size * sizeof(drawsegs_xranges[i].items[0]),
				PU_STATIC, NULL
			));
		}
	}

	for (ds = ds_p; ds-- > dsstart;)
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
			spr->cut = static_cast<spritecut_e>(spr->cut | SC_NOTVISIBLE);
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

		R_ClipVisSprite(spr, spr->x1, spr->x2, portal);

		if ((spr->cut & SC_NOTVISIBLE) == 0)
			numvisiblesprites++;
	}
}

/* Check if thing may be drawn from our current view. */
boolean R_ThingVisible(mobj_t *thing)
{
	if (UNLIKELY((thing->sprite == SPR_NULL) || (thing->flags2 & MF2_DONTDRAW)))
		return false;

	if (UNLIKELY(splitscreen))
	{
		if    ((viewssnum == 0 && (thing->eflags & MFE_DRAWONLYFORP1))
			|| (viewssnum == 1 && (thing->eflags & MFE_DRAWONLYFORP2))
			|| (viewssnum == 2 && (thing->eflags & MFE_DRAWONLYFORP2))
			|| (viewssnum == 3 && (thing->eflags & MFE_DRAWONLYFORP4)))
			return true;
	}

	return true;
}

boolean R_ThingWithinDist(mobj_t *thing, INT32 limit_dist)
{
	if (limit_dist)
	{
		if ((R_QuickCamDist(thing->x, thing->y) << FRACBITS)/mapobjectscale > limit_dist)
		{
			return false;
		}
	}

	return true;
}

fixed_t R_DoPlayerFade(mobj_t *thing)
{
	fixed_t fadealpha = FRACUNIT;
	static constexpr tic_t countdownstarttime = (15 * TICRATE) / 4; // starttime - (3*TICRATE)

	if (thing->player == viewplayer || viewplayer->exiting || camera[R_GetViewNumber()].freecam || leveltime < countdownstarttime)
		return fadealpha;

	const INT32 playerdist     = (FixedMul((thing->x - viewx), viewcos) + FixedMul((thing->y - viewy), viewsin)) >> FRACBITS;
	const INT32 viewplayerdist = (FixedMul((viewplayer->mo->x - viewx), viewcos) + FixedMul((viewplayer->mo->y - viewy), viewsin)) >> FRACBITS;

	if (playerdist < viewplayerdist)
	{
		if (playerdist < viewplayerdist / 2) // stronger fade when very close
		{
			fadealpha = (playerdist * FRACUNIT) / (viewplayerdist / 2) / 3;
		}
		else
		{
			fadealpha = (playerdist * FRACUNIT) / viewplayerdist;
		}
	}

	return fadealpha;
}

boolean R_CheckInterpDist(mobj_t *thing)
{
	if (!cv_maxinterpdist.value)
		return true;

	const INT32 dist = R_QuickCamDist(thing->x, thing->y);

	return (dist < cv_maxinterpdist.value);
}

boolean R_ThingIsFullBright(mobj_t *thing)
{
	return ((thing->frame & FF_BRIGHTMASK) == FF_FULLBRIGHT);
}

boolean R_ThingIsSemiBright(mobj_t *thing)
{
	return ((thing->frame & FF_BRIGHTMASK) == FF_SEMIBRIGHT);
}

boolean R_ThingIsFullDark(mobj_t *thing)
{
	return ((thing->frame & FF_BRIGHTMASK) == FF_FULLDARK);
}

//
// R_DrawMasked
//
static void R_DrawMaskedList(drawnode_t* head)
{
	drawnode_t *r2;
	drawnode_t *next;

	for (r2 = head->next; r2 != head; r2 = r2->next)
	{
		if (r2->plane)
		{
			drawspandata_t ds = {};
			next = r2->prev;
			boolean parallel = cv_parallelsoftware.value && cv_paralleldrawmasked.value;
#ifdef HAVE_THREADS
			srb2::ThreadPool::Sema tp_sema;
			if (parallel)
				srb2::g_main_threadpool->begin_sema();
#endif
			R_DrawSinglePlane(&ds, r2->plane, parallel);
#ifdef HAVE_THREADS
			if (parallel)
			{
				tp_sema = srb2::g_main_threadpool->end_sema();
				srb2::g_main_threadpool->notify_sema(tp_sema);
				srb2::g_main_threadpool->wait_sema(tp_sema);
			}
#endif
			R_DoneWithNode(r2);
			r2 = next;
		}
		else if (r2->seg && r2->seg->maskedtexturecol != NULL)
		{
			next = r2->prev;
			R_RenderMaskedSegRange( r2->seg, r2->seg->x1, r2->seg->x2);
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
			if (r2->sprite->precip)
			{
				R_DrawPrecipitationSprite(r2->sprite);
			}
			else
			{
				R_DrawSprite(r2->sprite);
			}

			R_DoneWithNode(r2);
			r2 = next;
		}
	}
}

void R_DrawMasked(maskcount_t* masks, INT32 nummasks)
{
	INT32 i;
	drawnode_t *heads;	/**< Drawnode lists; as many as number of views/portals. */

	heads = static_cast<drawnode_t*>(calloc(nummasks, sizeof(drawnode_t)));

	for (i = 0; i < nummasks; i++)
	{
		heads[i].next = heads[i].prev = &heads[i];

		viewx = masks[i].viewx;
		viewy = masks[i].viewy;
		viewz = masks[i].viewz;
		viewsector = masks[i].viewsector;

		R_CreateDrawNodes(&masks[i], &heads[i], false);
	}

	for (; nummasks > 0; nummasks--)
	{
		viewx = masks[nummasks - 1].viewx;
		viewy = masks[nummasks - 1].viewy;
		viewz = masks[nummasks - 1].viewz;
		viewsector = masks[nummasks - 1].viewsector;

		R_DrawMaskedList(&heads[nummasks - 1]);
		R_ClearDrawNodes(&heads[nummasks - 1]);
	}

	free(heads);
}
