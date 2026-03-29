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
/// \file  r_data.c
/// \brief Preparation of data for rendering,generation of lookups, caching, retrieval by name

#include "doomdef.h"
#include "g_game.h"
#include "i_video.h"
#include "r_local.h"
#include "r_sky.h"
#include "p_local.h"
#include "m_misc.h"
#include "r_data.h"
#include "r_patch.h"
#include "w_wad.h"
#include "z_zone.h"
#include "p_setup.h" // levelflats
#include "v_video.h" // pLocalPalette
#include "dehacked.h"

#ifdef HWRENDER
#include "hardware/hw_main.h"
#endif

#ifdef _WIN32
#include <malloc.h> // alloca(sizeof)
#endif

// Not sure if this is necessary, but it was in w_wad.c, so I'm putting it here too -Shadow Hog
#include <errno.h>

//
// Graphics.
// SRB2 graphics for walls and sprites
// is stored in vertical runs of opaque pixels (posts).
// A column is composed of zero or more posts,
// a patch or sprite is composed of zero or more columns.
//

size_t numspritelumps = 0, max_spritelumps = 0;

// textures
INT32 numtextures = 0; // total number of textures found,
// size of following tables

texture_t **textures = NULL;
static UINT32 **texturecolumnofs = NULL; // column offset lookup table for each texture
UINT8 **texturecache = NULL; // graphics data for each generated full-size texture

// texture width is a power of 2, so it can easily repeat along sidedefs using a simple mask
static INT32 *texturewidth = NULL;

fixed_t *textureheight = NULL; // needed for texture pegging

INT32 *texturetranslation = NULL;

// needed for pre rendering
sprcache_t *spritecachedinfo = NULL;

lighttable_t *colormaps = NULL;
UINT8 *encoremap = NULL;
#ifdef HASINVERT
UINT8 invertmap[256];
#endif

// for debugging/info purposes
static size_t flatmemory = 0, spritememory = 0, texturememory = 0; // gotta play by 2.2 rules to get this to work

// Blends two pixels together, using the equation
// that matches the specified alpha style.
UINT32 ASTBlendPixel(RGBA_t background, RGBA_t foreground, int style, UINT8 alpha)
{
	RGBA_t output;
	INT16 fullalpha = (alpha - (0xFF - foreground.s.alpha));
	if (style == AST_TRANSLUCENT)
	{
		if (fullalpha <= 0)
			output.rgba = background.rgba;
		else
		{
			// don't go too high
			if (fullalpha >= 0xFF)
				fullalpha = 0xFF;
			alpha = (UINT8)fullalpha;

			// if the background pixel is empty,
			// match software and don't blend anything
			if (!background.s.alpha)
			{
				// ...unless the foreground pixel ISN'T actually translucent.
				if (alpha == 0xFF)
					output.rgba = foreground.rgba;
				else
					output.rgba = 0;
			}
			else
			{
				UINT8 beta = (0xFF - alpha);
				output.s.red = ((background.s.red * beta) + (foreground.s.red * alpha)) / 0xFF;
				output.s.green = ((background.s.green * beta) + (foreground.s.green * alpha)) / 0xFF;
				output.s.blue = ((background.s.blue * beta) + (foreground.s.blue * alpha)) / 0xFF;
				output.s.alpha = 0xFF;
			}
		}
		return output.rgba;
	}
#define clamp(c) max(min(c, 0xFF), 0x00);
	else
	{
		float falpha = ((float)alpha / 256.0f);
		float fr = ((float)foreground.s.red * falpha);
		float fg = ((float)foreground.s.green * falpha);
		float fb = ((float)foreground.s.blue * falpha);
		if (style == AST_ADD)
		{
			output.s.red = clamp((int)(background.s.red + fr));
			output.s.green = clamp((int)(background.s.green + fg));
			output.s.blue = clamp((int)(background.s.blue + fb));
		}
		else if (style == AST_SUBTRACT)
		{
			output.s.red = clamp((int)(background.s.red - fr));
			output.s.green = clamp((int)(background.s.green - fg));
			output.s.blue = clamp((int)(background.s.blue - fb));
		}
		else if (style == AST_REVERSESUBTRACT)
		{
			output.s.red = clamp((int)((-background.s.red) + fr));
			output.s.green = clamp((int)((-background.s.green) + fg));
			output.s.blue = clamp((int)((-background.s.blue) + fb));
		}
		else if (style == AST_MODULATE)
		{
			fr = ((float)foreground.s.red / 256.0f);
			fg = ((float)foreground.s.green / 256.0f);
			fb = ((float)foreground.s.blue / 256.0f);
			output.s.red = clamp((int)(background.s.red * fr));
			output.s.green = clamp((int)(background.s.green * fg));
			output.s.blue = clamp((int)(background.s.blue * fb));
		}
		// just copy the pixel
		else if (style == AST_COPY)
			output.rgba = foreground.rgba;

		output.s.alpha = 0xFF;
		return output.rgba;
	}
#undef clamp
	return 0;
}

INT32 ASTTextureBlendingThreshold[2] = {255/11, (10*255/11)};

// Blends a pixel for a texture patch.
UINT32 ASTBlendTexturePixel(RGBA_t background, RGBA_t foreground, int style, UINT8 alpha)
{
	// Alpha style set to translucent?
	if (style == AST_TRANSLUCENT)
	{
		// Is the alpha small enough for translucency?
		if (alpha <= ASTTextureBlendingThreshold[1])
		{
			// Is the patch way too translucent? Don't blend then.
			if (alpha < ASTTextureBlendingThreshold[0])
				return background.rgba;

			return ASTBlendPixel(background, foreground, style, alpha);
		}
		else // just copy the pixel
			return foreground.rgba;
	}
	else
		return ASTBlendPixel(background, foreground, style, alpha);
}

// Painfully simple texture id cacheing to make maps load faster. :3
static struct {
	char name[9];
	UINT32 hash;
	INT32 id;
} *tidcache = NULL;
static INT32 tidcachelen = 0;

//
// MAPTEXTURE_T CACHING
// When a texture is first needed, it counts the number of composite columns
//  required in the texture and allocates space for a column directory and
//  any new columns.
// The directory will simply point inside other patches if there is only one
//  patch in a given column, but any columns with multiple patches will have
//  new column_ts generated.
//

//
// R_DrawColumnInCache
// Clip and draw a column from a patch into a cached post.
//
static inline void R_DrawColumnInCache(column_t *patch, UINT8 *cache, texpatch_t *originPatch, INT32 cacheheight)
{
	INT32 count, position;
	UINT8 *source;
	INT32 topdelta, prevdelta = -1;
	INT32 originy = originPatch->originy;

	while (patch->topdelta != 0xff)
	{
		topdelta = patch->topdelta;
		if (topdelta <= prevdelta)
			topdelta += prevdelta;
		prevdelta = topdelta;
		source = (UINT8 *)patch + 3;
		count = patch->length;
		position = originy + topdelta;

		if (position < 0)
		{
			count += position;
			source -= position; // start further down the column
			position = 0;
		}

		if (position + count > cacheheight)
			count = cacheheight - position;

		if (count > 0)
			memcpy(cache + position, source, count);

		patch = (column_t *)((UINT8 *)patch + patch->length + 4);
	}
}

static UINT8 *R_AllocateTextureBlock(size_t blocksize, UINT8 **user)
{
	texturememory += blocksize;

	return Z_Malloc(blocksize, PU_LEVEL, user);
}

static UINT8 *R_AllocateDummyTextureBlock(size_t width, UINT8 **user)
{
	// Allocate dummy data. Keep 4-bytes aligned.
	// Column offsets will be initialized to 0, which points to the 0xff byte (empty column flag).
	size_t blocksize = 4 + (width * 4);
	UINT8 *block = R_AllocateTextureBlock(blocksize, user);

	memset(block, 0, blocksize);
	block[0] = 0xff;

	return block;
}

static boolean R_CheckTextureLumpLength(texture_t *texture, size_t patch)
{
	UINT16 wadnum = texture->patches[patch].wad;
	UINT16 lumpnum = texture->patches[patch].lump;
	size_t lumplength = W_LumpLengthPwad(wadnum, lumpnum);

	// The header does not exist
	if (lumplength < offsetof(softwarepatch_t, columnofs))
	{
		CONS_Alert(
			CONS_ERROR,
			 "%.8s: texture lump data is too small. Expected %s bytes, got %s. (%s)\n",
					texture->name,
					sizeu1(offsetof(softwarepatch_t, columnofs)),
					sizeu2(lumplength),
					wadfiles[wadnum]->lumpinfo[lumpnum].fullname
		);

		return false;
	}

	return true;
}

//
// R_GenerateTexture
//
// Allocate space for full size texture, either single patch or 'composite'
// Build the full textures from patches.
// The texture caching system is a little more hungry of memory, but has
// been simplified for the sake of highcolor, dynamic ligthing, & speed.
//
// This is not optimised, but it's supposed to be executed only once
// per level, when enough memory is available.
//
UINT8 *R_GenerateTexture(size_t texnum)
{
	UINT8 *block;
	UINT8 *blocktex;
	texture_t *texture;
	texpatch_t *patch;
	softwarepatch_t *realpatch;
	UINT8 *pdata;
	int x, x1, x2, i, width, height;
	size_t blocksize;
	column_t *patchcol;
	UINT8 *colofs;

	UINT16 wadnum;
	lumpnum_t lumpnum;
	size_t lumplength;

	I_Assert(texnum <= (size_t)numtextures);
	texture = textures[texnum];
	I_Assert(texture != NULL);

	// allocate texture column offset lookup

	// single-patch textures can have holes in them and may be used on
	// 2sided lines so they need to be kept in 'packed' format
	// BUT this is wrong for skies and walls with over 255 pixels,
	// so check if there's holes and if not strip the posts.
	if (texture->patchcount == 1)
	{
		boolean holey = false;
		patch = texture->patches;

		wadnum = patch->wad;
		lumpnum = patch->lump;
		lumplength = W_LumpLengthPwad(wadnum, lumpnum);

		// The header does not exist
		if (R_CheckTextureLumpLength(texture, 0) == false)
		{
			block = R_AllocateDummyTextureBlock(texture->width, &texturecache[texnum]);
			texturecolumnofs[texnum] = (UINT32*)&block[4];
			textures[texnum]->holes = true;
			return block;
		}

		pdata = (UINT8*)W_CacheLumpNumPwad(wadnum, lumpnum, PU_LEVEL);
		realpatch = (softwarepatch_t *)pdata;

		// Check the patch for holes.
		if (texture->width > SHORT(realpatch->width) || texture->height > SHORT(realpatch->height))
			holey = true;

		colofs = (UINT8 *)realpatch->columnofs;

		for (x = 0; x < texture->width && !holey; x++)
		{
			column_t *col = (column_t *)((UINT8 *)realpatch + LONG(*(UINT32 *)&colofs[x<<2]));

			INT32 topdelta, prevdelta = -1, y = 0;

			while (col->topdelta != 0xff)
			{
				topdelta = col->topdelta;
				if (topdelta <= prevdelta)
					topdelta += prevdelta;
				prevdelta = topdelta;
				if (topdelta > y)
					break;
				y = topdelta + col->length + 1;
				col = (column_t *)((UINT8 *)col + col->length + 4);
			}

			if (y < texture->height)
				holey = true; // this texture is HOLEy! D:
		}

		// If the patch uses transparency, we have to save it this way.
		if (holey)
		{
			texture->holes = true;
			blocksize = lumplength;
			block = Z_Calloc(blocksize, PU_LEVEL, &texturecache[texnum]); // will change tag at end of this function
			memcpy(block, realpatch, blocksize);
			texturememory += blocksize;

			// use the patch's column lookup
			colofs = (block + 8);
			texturecolumnofs[texnum] = (UINT32 *)colofs;
			blocktex = block;

			for (x = 0; x < texture->width; x++)
			{
				*(UINT32 *)&colofs[x<<2] = LONG(LONG(*(UINT32 *)&colofs[x<<2]) + 3);
			}

			goto done;
		}
		// Otherwise, do multipatch format.
	}

	// multi-patch textures (or 'composite')
	texture->holes = false;
	blocksize = (texture->width * 4) + (texture->width * texture->height);
	texturememory += blocksize;
	block = Z_Malloc(blocksize+1, PU_LEVEL, &texturecache[texnum]);

	memset(block, TRANSPARENTPIXEL, blocksize+1); // Transparency hack

	// columns lookup table
	colofs = block;
	texturecolumnofs[texnum] = (UINT32 *)colofs;

	// texture data after the lookup table
	blocktex = block + (texture->width*4);

	for (x = 0; x < texture->width; ++x)
	{
		// generate column ofset lookup
		*(UINT32 *)&colofs[x<<2] = LONG((x * texture->height) + (texture->width*4));
	}

	// Composite the columns together.
	for (i = 0, patch = texture->patches; i < texture->patchcount; i++, patch++)
	{
		wadnum = patch->wad;
		lumpnum = patch->lump;
		pdata = (UINT8*)W_CacheLumpNumPwad(wadnum, lumpnum, PU_LEVEL);
		realpatch = (softwarepatch_t *)pdata;

		x1 = patch->originx;
		width = SHORT(realpatch->width);
		height = SHORT(realpatch->height);
		x2 = x1 + width;

		if (x1 > texture->width || x2 < 0)
			continue; // patch not located within texture's x bounds, ignore

		if (patch->originy > texture->height || (patch->originy + height) < 0)
			continue; // patch not located within texture's y bounds, ignore

		// patch is actually inside the texture!
		// now check if texture is partly off-screen and adjust accordingly

		// left edge
		if (x1 < 0)
			x = 0;
		else
			x = x1;

		// right edge
		if (x2 > texture->width)
			x2 = texture->width;

		for (; x < x2; x++)
		{
			patchcol = (column_t *)((UINT8 *)realpatch + LONG(realpatch->columnofs[x-x1]));

			R_DrawColumnInCache(patchcol, block + LONG(*(UINT32 *)&colofs[x<<2]), patch, texture->height);
		}
	}

done:
	return blocktex;
}

//
// R_GetTextureNum
//
// Returns the actual texture id that we should use.
// This can either be texnum, the current frame for texnum's anim (if animated),
// or 0 if not valid.
//
INT32 R_GetTextureNum(INT32 texnum)
{
	if (texnum < 0 || texnum >= numtextures)
		return 0;

	return texturetranslation[texnum];
}

//
// R_CheckTextureCache
//
// Use this if you need to make sure the texture is cached before R_GetColumn calls
// e.g.: midtextures and FOF walls
//
void R_CheckTextureCache(INT32 tex)
{
	if (!texturecache[tex])
		R_GenerateTexture(tex);
}

static inline INT32 wrap_column(fixed_t tex, INT32 col)
{
	INT32 width = texturewidth[tex];

	if (width & (width - 1))
		col = (UINT32)col % width;
	else
		col &= (width - 1);

	return col;
}

//
// R_GetColumn
//
UINT8 *R_GetColumn(fixed_t tex, INT32 col)
{
	return texturecache[tex] + LONG(texturecolumnofs[tex][wrap_column(tex, col)]);
}

// convert flats to hicolor as they are requested
//
UINT8 *R_GetFlat(lumpnum_t flatlumpnum)
{
	return W_CacheLumpNum(flatlumpnum, PU_LEVEL);
}

//
// Empty the texture cache (used for load wad at runtime)
//
void R_FlushTextureCache(void)
{
	INT32 i;

	if (numtextures)
		for (i = 0; i < numtextures; i++)
			Z_Free(texturecache[i]);
}

// Need these prototypes for later; defining them here instead of r_data.h so they're "private"
int R_CountTexturesInTEXTURESLump(UINT16 wadNum, UINT16 lumpNum);
void R_ParseTEXTURESLump(UINT16 wadNum, UINT16 lumpNum, INT32 *index);

#define TX_START "TX_START"
#define TX_END "TX_END"

static INT32
Rloadtextures (INT32 i, INT32 w)
{
	UINT16 j, numlumps = 0;
	UINT16 texstart, texend, texturesLumpPos;
	texpatch_t *patch;
	texture_t *texture;
	softwarepatch_t patchlump;

	// Get the lump numbers for the markers in the WAD, if they exist.
	if (W_FileHasFolders(wadfiles[w]))
	{
		texstart = W_CheckNumForFolderStartPK3("textures/", (UINT16)w, 0);
		texend = W_CheckNumForFolderEndPK3("textures/", (UINT16)w, texstart);
		texturesLumpPos = W_CheckNumForNamePwad("TEXTURES", (UINT16)w, 0);
		while (texturesLumpPos != INT16_MAX)
		{
			R_ParseTEXTURESLump(w, texturesLumpPos, &i);
			texturesLumpPos = W_CheckNumForNamePwad("TEXTURES", (UINT16)w, texturesLumpPos + 1);
		}
	}
	else
	{
		texstart = W_CheckNumForMarkerStartPwad(TX_START, (UINT16)w, 0);
		texend = W_CheckNumForNamePwad(TX_END, (UINT16)w, 0);
		texturesLumpPos = W_CheckNumForNamePwad("TEXTURES", (UINT16)w, 0);
		if (texturesLumpPos != INT16_MAX)
			R_ParseTEXTURESLump(w, texturesLumpPos, &i);
	}

	if (texstart == INT16_MAX || texend == INT16_MAX)
		return i;

	numlumps = texend - texstart;

	// Work through each lump between the markers in the WAD.
	for (j = 0; j < numlumps; j++)
	{
		UINT16 wadnum = (UINT16)w;
		lumpnum_t lumpnum = texstart + j;

		if (W_FileHasFolders(wadfiles[w]))
		{
			if (W_IsLumpFolder(wadnum, lumpnum)) // Check if lump is a folder
				continue; // If it is then SKIP IT
		}

		W_ReadLumpHeaderPwad(wadnum, lumpnum, &patchlump, PNG_HEADER_SIZE, 0);

		//CONS_Printf("\n\"%s\" is a single patch, dimensions %d x %d",W_CheckNameForNumPwad((wadnum, lumpnum), patchlump->width, patchlump->height);
		texture = textures[i] = Z_Calloc(sizeof(texture_t) + sizeof(texpatch_t), PU_STATIC, NULL);

		// Set texture properties.
		memcpy(texture->name, W_CheckNameForNumPwad(wadnum, lumpnum), sizeof(texture->name));
		texture->hash = FNV1a_QuickCaseHash(texture->name, 8);

		texture->width = SHORT(patchlump.width);
		texture->height = SHORT(patchlump.height);

		texture->type = TEXTURETYPE_SINGLEPATCH;
		texture->patchcount = 1;
		texture->holes = false;

		// Allocate information for the texture's patches.
		patch = &texture->patches[0];

		patch->originx = patch->originy = 0;
		patch->wad = wadnum;
		patch->lump = texstart + j;

		texturewidth[i] = texture->width;
		textureheight[i] = texture->height << FRACBITS;
		i++;
	}

	return i;
}

static INT32
count_range
(		const char * marker_start,
		const char * marker_end,
		const char * folder,
		UINT16 wadnum)
{
	UINT16 j;
	UINT16 texstart, texend;
	INT32 count = 0;

	// Count flats
	if (W_FileHasFolders(wadfiles[wadnum]))
	{
		texstart = W_CheckNumForFolderStartPK3(folder, wadnum, 0);
		texend = W_CheckNumForFolderEndPK3(folder, wadnum, texstart);
	}
	else
	{
		texstart = W_CheckNumForMarkerStartPwad(marker_start, wadnum, 0);
		texend = W_CheckNumForNamePwad(marker_end, wadnum, texstart);
	}

	if (texstart != INT16_MAX && texend != INT16_MAX)
	{
		// PK3s have subfolders, so we can't just make a simple sum
		if (W_FileHasFolders(wadfiles[wadnum]))
		{
			for (j = texstart; j < texend; j++)
			{
				if (!W_IsLumpFolder(wadnum, j)) // Check if lump is a folder; if not, then count it
					count++;
			}
		}
		else // Add all the textures between markers
		{
			count += (texend - texstart);
		}
	}

	return count;
}

static INT32 R_CountTextures(UINT16 wadnum)
{
	UINT16 texturesLumpPos;
	INT32 count = 0;

	// Load patches and textures.

	// Get the number of textures to check.
	// NOTE: Make SURE the system does not process
	// the markers.
	// This system will allocate memory for all duplicate/patched textures even if it never uses them,
	// but the alternative is to spend a ton of time checking and re-checking all previous entries just to skip any potentially patched textures.

	// Count the textures from TEXTURES lumps
	texturesLumpPos = W_CheckNumForNamePwad("TEXTURES", wadnum, 0);

	while (texturesLumpPos != INT16_MAX)
	{
		count += R_CountTexturesInTEXTURESLump(wadnum, texturesLumpPos);
		texturesLumpPos = W_CheckNumForNamePwad("TEXTURES", wadnum, texturesLumpPos + 1);
	}

	// Count single-patch textures
	count += count_range(TX_START, TX_END, "textures/", wadnum);

	return count;
}

static void R_AllocateTextures(INT32 add)
{
	const INT32 newtextures = (numtextures + add);
	const size_t newsize = newtextures * sizeof (void*);
	//const size_t oldsize = numtextures * sizeof (void*);

	INT32 i;

	// Allocate memory and initialize to 0 for all the textures we are initialising.
	Z_Realloc(textures, newsize, PU_STATIC, &textures);

	// Allocate texture column offset table.
	Z_Realloc(texturecolumnofs, newsize, PU_STATIC, &texturecolumnofs);
	// Allocate texture referencing cache.
	Z_Realloc(texturecache, newsize, PU_STATIC, &texturecache);
	// Allocate texture width table.
	Z_Realloc(texturewidth, newsize, PU_STATIC, &texturewidth);
	// Allocate texture height table.
	Z_Realloc(textureheight, newsize, PU_STATIC, &textureheight);
	// Create translation table for global animation.
	Z_Realloc(texturetranslation, (newtextures + 1) * sizeof(*texturetranslation), PU_STATIC, &texturetranslation);

	for (i = 0; i < numtextures; ++i)
	{
		// R_FlushTextureCache relies on the user for
		// Z_Free, texturecache has been reallocated so the
		// user is now garbage memory.
		Z_SetUser(texturecache[i], (void**)&texturecache[i]);
	}

	while (i < newtextures)
	{
		texturetranslation[i] = i;
		i++;
	}
}

static INT32 R_DefineTextures(INT32 i, UINT16 w)
{
	return Rloadtextures(i, w);
}

static void R_FinishLoadingTextures(INT32 add)
{
	numtextures += add;

#ifdef HWRENDER
	if (rendermode == render_opengl)
		HWR_LoadMapTextures(numtextures);
#endif
}

//
// R_LoadTextures
// Initializes the texture list with the textures from the world map.
//
void R_LoadTextures(void)
{
	INT32 i, w;
	INT32 newtextures = 0;

	for (w = 0; w < numwadfiles; w++)
	{
		newtextures += R_CountTextures((UINT16)w);
	}

	// If no textures found by this point, bomb out
	if (!newtextures)
		I_Error("No textures detected in any WADs!\n");

	R_AllocateTextures(newtextures);

	for (i = 0, w = 0; w < numwadfiles; w++)
	{
		i = R_DefineTextures(i, w);
	}

	R_FinishLoadingTextures(i);
}

void R_LoadTexturesPwad(UINT16 wadnum)
{
	INT32 newtextures = R_CountTextures(wadnum);

	if (!newtextures)
		return;

	R_AllocateTextures(newtextures);
	newtextures = R_DefineTextures(numtextures, wadnum) - numtextures;
	R_FinishLoadingTextures(newtextures);
}

static texpatch_t *R_ParsePatch(boolean actuallyLoadPatch)
{
	char *texturesToken;
	size_t texturesTokenLength;
	char *endPos;
	char *patchName = NULL;
	INT16 patchXPos;
	INT16 patchYPos;
	texpatch_t *resultPatch = NULL;
	lumpnum_t patchLumpNum;

	// Patch identifier
	texturesToken = M_GetToken(NULL);

	if (texturesToken == NULL)
	{
		I_Error("Error parsing TEXTURES lump: Unexpected end of file where patch name should be");
	}

	texturesTokenLength = strlen(texturesToken);

	if (texturesTokenLength > 8)
	{
		I_Error("Error parsing TEXTURES lump: Patch name \"%s\" exceeds 8 characters",texturesToken);
	}
	else
	{
		Z_Free(patchName);
		patchName = (char *)Z_Malloc((texturesTokenLength+1)*sizeof(char),PU_STATIC,NULL);
		memcpy(patchName,texturesToken,texturesTokenLength*sizeof(char));
		patchName[texturesTokenLength] = '\0';
	}

	// Comma 1
	Z_Free(texturesToken);
	texturesToken = M_GetToken(NULL);

	if (texturesToken == NULL)
	{
		I_Error("Error parsing TEXTURES lump: Unexpected end of file where comma after \"%s\"'s patch name should be",patchName);
	}

	if (!fastcmp(texturesToken,","))
	{
		I_Error("Error parsing TEXTURES lump: Expected \",\" after %s's patch name, got \"%s\"",patchName,texturesToken);
	}

	// XPos
	Z_Free(texturesToken);
	texturesToken = M_GetToken(NULL);

	if (texturesToken == NULL)
	{
		I_Error("Error parsing TEXTURES lump: Unexpected end of file where patch \"%s\"'s x coordinate should be",patchName);
	}

	endPos = NULL;
#ifndef AVOID_ERRNO
	errno = 0;
#endif

	patchXPos = strtol(texturesToken,&endPos,10);
	(void)patchXPos; //unused for now
	if (endPos == texturesToken // Empty string
		|| *endPos != '\0' // Not end of string
#ifndef AVOID_ERRNO
		|| errno == ERANGE // Number out-of-range
#endif
		)
	{
		I_Error("Error parsing TEXTURES lump: Expected an integer for patch \"%s\"'s x coordinate, got \"%s\"",patchName,texturesToken);
	}

	// Comma 2
	Z_Free(texturesToken);
	texturesToken = M_GetToken(NULL);

	if (texturesToken == NULL)
	{
		I_Error("Error parsing TEXTURES lump: Unexpected end of file where comma after patch \"%s\"'s x coordinate should be",patchName);
	}

	if (!fastcmp(texturesToken,","))
	{
		I_Error("Error parsing TEXTURES lump: Expected \",\" after patch \"%s\"'s x coordinate, got \"%s\"",patchName,texturesToken);
	}

	// YPos
	Z_Free(texturesToken);
	texturesToken = M_GetToken(NULL);

	if (texturesToken == NULL)
	{
		I_Error("Error parsing TEXTURES lump: Unexpected end of file where patch \"%s\"'s y coordinate should be",patchName);
	}
	endPos = NULL;
#ifndef AVOID_ERRNO
	errno = 0;
#endif
	patchYPos = strtol(texturesToken,&endPos,10);
	(void)patchYPos; //unused for now
	if (endPos == texturesToken // Empty string
		|| *endPos != '\0' // Not end of string
#ifndef AVOID_ERRNO
		|| errno == ERANGE // Number out-of-range
#endif
		)
	{
		I_Error("Error parsing TEXTURES lump: Expected an integer for patch \"%s\"'s y coordinate, got \"%s\"",patchName,texturesToken);
	}
	Z_Free(texturesToken);

	if (actuallyLoadPatch == true)
	{
		// Check lump exists
		patchLumpNum = W_GetNumForName(patchName);
		// If so, allocate memory for texpatch_t and fill 'er up
		resultPatch = (texpatch_t *)Z_Malloc(sizeof(texpatch_t),PU_STATIC,NULL);
		resultPatch->originx = patchXPos;
		resultPatch->originy = patchYPos;
		resultPatch->lump = patchLumpNum & 65535;
		resultPatch->wad = patchLumpNum>>16;
		// Clean up a little after ourselves
		Z_Free(patchName);
		// Then return it
		return resultPatch;
	}
	else
	{
		Z_Free(patchName);
		return NULL;
	}
}

static texture_t *R_ParseTexture(boolean actuallyLoadTexture)
{
	char *texturesToken;
	size_t texturesTokenLength;
	char *endPos;
	INT32 newTextureWidth;
	INT32 newTextureHeight;
	texture_t *resultTexture = NULL;
	texpatch_t *newPatch;
	char newTextureName[9]; // no longer dynamically allocated

	// Texture name
	texturesToken = M_GetToken(NULL);
	if (texturesToken == NULL)
	{
		I_Error("Error parsing TEXTURES lump: Unexpected end of file where texture name should be");
	}

	texturesTokenLength = strlen(texturesToken);
	if (texturesTokenLength > 8)
	{
		I_Error("Error parsing TEXTURES lump: Texture name \"%s\" exceeds 8 characters",texturesToken);
	}
	else
	{
		memset(&newTextureName, 0, 9);
		memcpy(newTextureName, texturesToken, texturesTokenLength);
		// ^^ we've confirmed that the token is <= 8 characters so it will never overflow a 9 byte char buffer
		strupr(newTextureName); // Just do this now so we don't have to worry about it
	}

	Z_Free(texturesToken);

	// Comma 1
	texturesToken = M_GetToken(NULL);
	if (texturesToken == NULL)
	{
		I_Error("Error parsing TEXTURES lump: Unexpected end of file where comma after texture \"%s\"'s name should be",newTextureName);
	}
	else if (!fastcmp(texturesToken, ","))
	{
		I_Error("Error parsing TEXTURES lump: Expected \",\" after texture \"%s\"'s name, got \"%s\"",newTextureName,texturesToken);
	}

	Z_Free(texturesToken);

	// Width
	texturesToken = M_GetToken(NULL);
	if (texturesToken == NULL)
	{
		I_Error("Error parsing TEXTURES lump: Unexpected end of file where texture \"%s\"'s width should be",newTextureName);
	}
	endPos = NULL;
#ifndef AVOID_ERRNO
	errno = 0;
#endif
	newTextureWidth = strtol(texturesToken,&endPos,10);
	if (endPos == texturesToken // Empty string
		|| *endPos != '\0' // Not end of string
#ifndef AVOID_ERRNO
		|| errno == ERANGE // Number out-of-range
#endif
		|| newTextureWidth < 0) // Number is not positive
	{
		I_Error("Error parsing TEXTURES lump: Expected a positive integer for texture \"%s\"'s width, got \"%s\"",newTextureName,texturesToken);
	}
	Z_Free(texturesToken);

	// Comma 2
	texturesToken = M_GetToken(NULL);
	if (texturesToken == NULL)
	{
		I_Error("Error parsing TEXTURES lump: Unexpected end of file where comma after texture \"%s\"'s width should be",newTextureName);
	}

	if (!fastcmp(texturesToken, ","))
	{
		I_Error("Error parsing TEXTURES lump: Expected \",\" after texture \"%s\"'s width, got \"%s\"",newTextureName,texturesToken);
	}

	Z_Free(texturesToken);

	// Height
	texturesToken = M_GetToken(NULL);
	if (texturesToken == NULL)
	{
		I_Error("Error parsing TEXTURES lump: Unexpected end of file where texture \"%s\"'s height should be",newTextureName);
	}
	endPos = NULL;
#ifndef AVOID_ERRNO
	errno = 0;
#endif
	newTextureHeight = strtol(texturesToken,&endPos,10);
	if (endPos == texturesToken // Empty string
		|| *endPos != '\0' // Not end of string
#ifndef AVOID_ERRNO
		|| errno == ERANGE // Number out-of-range
#endif
		|| newTextureHeight < 0) // Number is not positive
	{
		I_Error("Error parsing TEXTURES lump: Expected a positive integer for texture \"%s\"'s height, got \"%s\"",newTextureName,texturesToken);
	}
	Z_Free(texturesToken);

	// Left Curly Brace
	texturesToken = M_GetToken(NULL);

	if (texturesToken == NULL)
	{
		I_Error("Error parsing TEXTURES lump: Unexpected end of file where open curly brace for texture \"%s\" should be",newTextureName);
	}

	if (fastcmp(texturesToken, "{"))
	{
		if (actuallyLoadTexture)
		{
			// Allocate memory for a zero-patch texture. Obviously, we'll be adding patches momentarily.
			resultTexture = (texture_t *)Z_Calloc(sizeof(texture_t), PU_STATIC, NULL);
			memcpy(resultTexture->name, newTextureName, 8);
			resultTexture->hash = FNV1a_QuickCaseHash(newTextureName, 8);
			resultTexture->width = newTextureWidth;
			resultTexture->height = newTextureHeight;
			resultTexture->type = TEXTURETYPE_COMPOSITE;
		}

		Z_Free(texturesToken);

		texturesToken = M_GetToken(NULL);
		if (texturesToken == NULL)
		{
			I_Error("Error parsing TEXTURES lump: Unexpected end of file where patch definition for texture \"%s\" should be",newTextureName);
		}

		while (!fastcmp(texturesToken, "}"))
		{
			if (fasticmp(texturesToken, "PATCH"))
			{
				Z_Free(texturesToken);
				if (resultTexture)
				{
					// Get that new patch
					newPatch = R_ParsePatch(true);
					// Make room for the new patch
					resultTexture = Z_Realloc(resultTexture, sizeof(texture_t) + (resultTexture->patchcount+1)*sizeof(texpatch_t), PU_STATIC, NULL);
					// Populate the uninitialized values in the new patch entry of our array
					memcpy(&resultTexture->patches[resultTexture->patchcount], newPatch, sizeof(texpatch_t));
					// Account for the new number of patches in the texture
					resultTexture->patchcount++;
					// Then free up the memory assigned to R_ParsePatch, as it's unneeded now
					Z_Free(newPatch);
				}
				else
				{
					R_ParsePatch(false);
				}
			}
			else
			{
				I_Error("Error parsing TEXTURES lump: Expected \"PATCH\" in texture \"%s\", got \"%s\"",newTextureName,texturesToken);
			}

			texturesToken = M_GetToken(NULL);
			if (texturesToken == NULL)
			{
				I_Error("Error parsing TEXTURES lump: Unexpected end of file where patch declaration or right curly brace for texture \"%s\" should be",newTextureName);
			}
		}

		if (resultTexture && resultTexture->patchcount == 0)
		{
			I_Error("Error parsing TEXTURES lump: Texture \"%s\" must have at least one patch",newTextureName);
		}
	}
	else
	{
		I_Error("Error parsing TEXTURES lump: Expected \"{\" for texture \"%s\", got \"%s\"",newTextureName,texturesToken);
	}

	Z_Free(texturesToken);

	if (actuallyLoadTexture)
		return resultTexture;

	return NULL;
}

// Parses the TEXTURES lump... but just to count the number of textures.
int R_CountTexturesInTEXTURESLump(UINT16 wadNum, UINT16 lumpNum)
{
	char *texturesLump;
	size_t texturesLumpLength;
	char *texturesText;
	UINT32 numTexturesInLump = 0;
	char *texturesToken;

	// Since lumps AREN'T \0-terminated like I'd assumed they should be, I'll
	// need to make a space of memory where I can ensure that it will terminate
	// correctly. Start by loading the relevant data from the WAD.
	texturesLump = (char *)W_CacheLumpNumPwad(wadNum, lumpNum, PU_STATIC);
	// If that didn't exist, we have nothing to do here.
	if (texturesLump == NULL) return 0;
	// If we're still here, then it DOES exist; figure out how long it is, and allot memory accordingly.
	texturesLumpLength = W_LumpLengthPwad(wadNum, lumpNum);
	texturesText = (char *)Z_Malloc((texturesLumpLength+1)*sizeof(char),PU_STATIC,NULL);
	// Now move the contents of the lump into this new location.
	memmove(texturesText,texturesLump,texturesLumpLength);
	// Make damn well sure the last character in our new memory location is \0.
	texturesText[texturesLumpLength] = '\0';
	// Finally, free up the memory from the first data load, because we really
	// don't need it.
	Z_Free(texturesLump);

	texturesToken = M_GetToken(texturesText);
	while (texturesToken != NULL)
	{
		if (fasticmp(texturesToken, "WALLTEXTURE"))
		{
			numTexturesInLump++;
			Z_Free(texturesToken);
			R_ParseTexture(false);
		}
		else
		{
			I_Error("Error parsing TEXTURES lump: Expected \"WALLTEXTURE\", got \"%s\"",texturesToken);
		}

		texturesToken = M_GetToken(NULL);
	}

	Z_Free(texturesToken);
	Z_Free((void *)texturesText);

	return numTexturesInLump;
}

// Parses the TEXTURES lump... for real, this time.
void R_ParseTEXTURESLump(UINT16 wadNum, UINT16 lumpNum, INT32 *texindex)
{
	char *texturesLump;
	size_t texturesLumpLength;
	char *texturesText;
	char *texturesToken;
	texture_t *newTexture;

	I_Assert(texindex != NULL);

	// Since lumps AREN'T \0-terminated like I'd assumed they should be, I'll
	// need to make a space of memory where I can ensure that it will terminate
	// correctly. Start by loading the relevant data from the WAD.
	texturesLump = (char *)W_CacheLumpNumPwad(wadNum, lumpNum, PU_STATIC);
	// If that didn't exist, we have nothing to do here.
	if (texturesLump == NULL) return;
	// If we're still here, then it DOES exist; figure out how long it is, and allot memory accordingly.
	texturesLumpLength = W_LumpLengthPwad(wadNum, lumpNum);
	texturesText = (char *)Z_Malloc((texturesLumpLength+1)*sizeof(char),PU_STATIC,NULL);
	// Now move the contents of the lump into this new location.
	memmove(texturesText,texturesLump,texturesLumpLength);
	// Make damn well sure the last character in our new memory location is \0.
	texturesText[texturesLumpLength] = '\0';
	// Finally, free up the memory from the first data load, because we really
	// don't need it.
	Z_Free(texturesLump);

	texturesToken = M_GetToken(texturesText);
	while (texturesToken != NULL)
	{
		if (fasticmp(texturesToken, "WALLTEXTURE"))
		{
			Z_Free(texturesToken);
			// Get the new texture
			newTexture = R_ParseTexture(true);
			// Store the new texture
			textures[*texindex] = newTexture;
			texturewidth[*texindex] = newTexture->width;
			textureheight[*texindex] = newTexture->height << FRACBITS;
			// Increment i back in R_LoadTextures()
			(*texindex)++;
		}
		else
		{
			I_Error("Error parsing TEXTURES lump: Expected \"WALLTEXTURE\", got \"%s\"",texturesToken);
		}

		texturesToken = M_GetToken(NULL);
	}

	Z_Free(texturesToken);
	Z_Free((void *)texturesText);
}

// Search for flat name.
lumpnum_t R_GetFlatNumForName(const char *name)
{
	INT32 i;
	lumpnum_t lump = LUMPERROR;
	lumpnum_t start = LUMPERROR;
	lumpnum_t end = LUMPERROR;

	// Scan wad files backwards so patched flats take preference.
	for (i = numwadfiles - 1; i >= 0; i--)
	{
		switch (wadfiles[i]->type)
		{
			case RET_WAD:
				if ((start = W_CheckNumForNamePwad("F_START", (UINT16)i, 0)) == INT16_MAX)
				{
					if ((start = W_CheckNumForNamePwad("FF_START", (UINT16)i, 0)) == INT16_MAX)
						continue;
					else if ((end = W_CheckNumForNamePwad("FF_END", (UINT16)i, start)) == INT16_MAX)
						continue;
				}
				else
					if ((end = W_CheckNumForNamePwad("F_END", (UINT16)i, start)) == INT16_MAX)
						continue;
				break;
			case RET_PK3:
				if ((start = W_CheckNumForFolderStartPK3("Flats/", i, 0)) == INT16_MAX)
					continue;
				if ((end = W_CheckNumForFolderEndPK3("Flats/", i, start)) == INT16_MAX)
					continue;
				break;
			default:
				continue;
		}

		// Now find lump with specified name in that range.
		lump = W_CheckNumForNamePwad(name, (UINT16)i, start);
		if (lump < end)
		{
			lump += (i<<16); // found it, in our constraints
			break;
		}
		lump = LUMPERROR;
	}

	if (lump == LUMPERROR)
	{
		if (!fastcmp(name, SKYFLATNAME))
			CONS_Debug(DBG_SETUP, "R_GetFlatNumForName: Could not find flat %.8s\n", name);

		lump = W_CheckNumForName("REDFLR");
	}

	return lump;
}

//
// R_InitSpriteLumps
// Finds the width and hoffset of all sprites in the wad, so the sprite does not need to be
// cached completely, just for having the header info ready during rendering.
//

//
// allocate sprite lookup tables
//
static void R_InitSpriteLumps(void)
{
	numspritelumps = 0;
	max_spritelumps = 8192;

	Z_Malloc(max_spritelumps*sizeof(*spritecachedinfo), PU_STATIC, &spritecachedinfo);
}

//
// R_InitColormaps
//
static void R_InitColormaps(void)
{
	lumpnum_t lump;

	// Load in the light tables
	lump = W_GetNumForName("COLORMAP");
	colormaps = Z_Malloc((256 * 64), PU_STATIC, NULL);
	W_ReadLump(lump, colormaps);
	// no need to init encoremap at this stage

	// Init Boom colormaps.
	R_ClearColormaps();
	//R_InitExtraColormaps();
#ifdef HASINVERT
	R_MakeInvertmap(); // this isn't the BEST place to do it the first time, but whatever
#endif
}

void R_ReInitColormaps(UINT16 num, lumpnum_t newencoremap)
{
	char colormap[9] = "COLORMAP";
	lumpnum_t lump;
	const lumpnum_t basecolormaplump = W_GetNumForName(colormap);

	if (num > 0 && num <= 10000)
		snprintf(colormap, 8, "CLM%04u", num-1);

	// Load in the light tables, now 64k aligned for smokie...
	lump = W_GetNumForName(colormap);

	if (lump == LUMPERROR)
		lump = basecolormaplump;
	else
	{
		if (W_LumpLength(lump) != W_LumpLength(basecolormaplump))
		{
			CONS_Alert(CONS_WARNING, "%s lump size does not match COLORMAP, results may be unexpected.\n", colormap);
		}
	}

	W_ReadLumpHeader(lump, colormaps, W_LumpLength(basecolormaplump), 0U);

	// Encore mode.
	if (newencoremap != LUMPERROR)
	{
		lighttable_t *colormap_p, *colormap_p2;
		size_t p, i;

		encoremap = Z_Malloc(256 + 10, PU_LEVEL, NULL);
		W_ReadLump(newencoremap, encoremap);
		colormap_p = colormap_p2 = colormaps;
		colormap_p += COLORMAP_REMAPOFFSET;

		for (p = 0; p < LIGHTLEVELS; p++)
		{
			for (i = 0; i < 256; i++)
			{
				*colormap_p = colormap_p2[encoremap[i]];
				colormap_p++;
			}

			colormap_p2 += 256;
		}
	}
	else
		encoremap = NULL;

	// Init Boom colormaps.
	R_ClearColormaps();
}

static lumpnum_t foundcolormaps[MAXCOLORMAPS];

//
// R_ClearColormaps
//
// Clears out extra colormaps between levels.
//
void R_ClearColormaps(void)
{
	size_t i;

	num_extra_colormaps = 0;

	for (i = 0; i < MAXCOLORMAPS; i++)
		foundcolormaps[i] = LUMPERROR;

	memset(extra_colormaps, 0, sizeof(extra_colormaps));
}

//
// R_CreateColormap
//
// This is a more GL friendly way of doing colormaps: Specify colormap
// data in a special linedef's texture areas and use that to generate
// custom colormaps at runtime. NOTE: For GL mode, we only need to color
// data and not the colormap data.
//
static double deltas[256][3], map[256][3];

static int RoundUp(double number);

#ifdef HASINVERT
void R_MakeInvertmap(void)
{
	size_t i;

	for (i = 0; i < 256; i++)
		invertmap[i] = NearestColor(256 - pLocalPalette[i].s.red, 256 - pLocalPalette[i].s.green, 256 - pLocalPalette[i].s.blue);
}
#endif

INT32 R_CreateColormap(char *p1, char *p2, char *p3)
{
	double cmaskr, cmaskg, cmaskb, cdestr, cdestg, cdestb;
	double maskamt = 0, othermask = 0;
	int mask, fog = 0;
	size_t mapnum = num_extra_colormaps;
	size_t i;
	UINT32 cr, cg, cb, maskcolor, fadecolor;
	UINT32 fadestart = 0, fadeend = 31, fadedist = 31;

#define HEX2INT(x) (UINT32)(x >= '0' && x <= '9' ? x - '0' : x >= 'a' && x <= 'f' ? x - 'a' + 10 : x >= 'A' && x <= 'F' ? x - 'A' + 10 : 0)
	if (p1[0] == '#')
	{
		cr = ((HEX2INT(p1[1]) * 16) + HEX2INT(p1[2]));
		cg = ((HEX2INT(p1[3]) * 16) + HEX2INT(p1[4]));
		cb = ((HEX2INT(p1[5]) * 16) + HEX2INT(p1[6]));

		if (encoremap)
		{
			i = encoremap[NearestColor((UINT8)cr, (UINT8)cg, (UINT8)cb)];
			//CONS_Printf("R_CreateColormap: encoremap[%d] = %d\n", i, encoremap[i]); -- moved encoremap upwards for optimisation
			cr = pLocalPalette[i].s.red;
			cg = pLocalPalette[i].s.green;
			cb = pLocalPalette[i].s.blue;
		}

		cmaskr = cr;
		cmaskg = cg;
		cmaskb = cb;
		// Create a rough approximation of the color (a 16 bit color)
		maskcolor = ((cb) >> 3) + (((cg) >> 2) << 5) + (((cr) >> 3) << 11);
		if (p1[7] >= 'a' && p1[7] <= 'z')
			mask = (p1[7] - 'a');
		else if (p1[7] >= 'A' && p1[7] <= 'Z')
			mask = (p1[7] - 'A');
		else
			mask = 24;

		maskamt = (double)(mask/24.0l);

		othermask = 1 - maskamt;
		maskamt /= 0xff;
		cmaskr *= maskamt;
		cmaskg *= maskamt;
		cmaskb *= maskamt;
	}
	else
	{
		cmaskr = cmaskg = cmaskb = 0xff;
		maskamt = 0;
		maskcolor = ((0xff) >> 3) + (((0xff) >> 2) << 5) + (((0xff) >> 3) << 11);
	}

#define NUMFROMCHAR(c) (c >= '0' && c <= '9' ? c - '0' : 0)
	if (p2[0] == '#')
	{
		// Get parameters like fadestart, fadeend, and the fogflag
		fadestart = NUMFROMCHAR(p2[3]) + (NUMFROMCHAR(p2[2]) * 10);
		fadeend = NUMFROMCHAR(p2[5]) + (NUMFROMCHAR(p2[4]) * 10);
		if (fadestart > 30)
			fadestart = 0;
		if (fadeend > 31 || fadeend < 1)
			fadeend = 31;
		fadedist = fadeend - fadestart;
		fog = NUMFROMCHAR(p2[1]);
	}
#undef NUMFROMCHAR

	if (p3[0] == '#')
	{
		cr = ((HEX2INT(p3[1]) * 16) + HEX2INT(p3[2]));
		cg = ((HEX2INT(p3[3]) * 16) + HEX2INT(p3[4]));
		cb = ((HEX2INT(p3[5]) * 16) + HEX2INT(p3[6]));

		if (encoremap)
		{
			i = encoremap[NearestColor((UINT8)cr, (UINT8)cg, (UINT8)cb)];
			cr = pLocalPalette[i].s.red;
			cg = pLocalPalette[i].s.green;
			cb = pLocalPalette[i].s.blue;
		}

		cdestr = cr;
		cdestg = cg;
		cdestb = cb;
		fadecolor = (((cb) >> 3) + (((cg) >> 2) << 5) + (((cr) >> 3) << 11));
	}
	else
		cdestr = cdestg = cdestb = fadecolor = 0;
#undef HEX2INT

	for (i = 0; i < num_extra_colormaps; i++)
	{
		if (foundcolormaps[i] != LUMPERROR)
			continue;

		if (maskcolor == extra_colormaps[i].maskcolor
			&& fadecolor == extra_colormaps[i].fadecolor
			&& fabs(maskamt - extra_colormaps[i].maskamt) < DBL_EPSILON
			&& fadestart == extra_colormaps[i].fadestart
			&& fadeend == extra_colormaps[i].fadeend
			&& fog == extra_colormaps[i].fog)
		{
			return (INT32)i;
		}
	}

	if (num_extra_colormaps == MAXCOLORMAPS)
		I_Error("R_CreateColormap: Too many colormaps! the limit is %d\n", MAXCOLORMAPS);

	num_extra_colormaps++;

	foundcolormaps[mapnum] = LUMPERROR;

	// aligned on 8 bit for asm code
	extra_colormaps[mapnum].colormap = NULL;
	extra_colormaps[mapnum].maskcolor = (UINT16)maskcolor;
	extra_colormaps[mapnum].fadecolor = (UINT16)fadecolor;
	extra_colormaps[mapnum].maskamt = maskamt;
	extra_colormaps[mapnum].fadestart = (UINT16)fadestart;
	extra_colormaps[mapnum].fadeend = (UINT16)fadeend;
	extra_colormaps[mapnum].fog = fog;

	if (rendermode != render_none)
	{
		double r, g, b, cbrightness;
		int p;
		lighttable_t *colormap_p;

		// Initialise the map and delta arrays
		// map[i] stores an RGB color (as double) for index i,
		//  which is then converted to SRB2's palette later
		// deltas[i] stores a corresponding fade delta between the RGB color and the final fade color;
		//  map[i]'s values are decremented by after each use
		for (i = 0; i < 256; i++)
		{
			r = pLocalPalette[i].s.red;
			g = pLocalPalette[i].s.green;
			b = pLocalPalette[i].s.blue;
			cbrightness = sqrt((r*r) + (g*g) + (b*b));

			map[i][0] = (cbrightness * cmaskr) + (r * othermask);
			if (map[i][0] > 255.0l)
				map[i][0] = 255.0l;
			deltas[i][0] = (map[i][0] - cdestr) / (double)fadedist;

			map[i][1] = (cbrightness * cmaskg) + (g * othermask);
			if (map[i][1] > 255.0l)
				map[i][1] = 255.0l;
			deltas[i][1] = (map[i][1] - cdestg) / (double)fadedist;

			map[i][2] = (cbrightness * cmaskb) + (b * othermask);
			if (map[i][2] > 255.0l)
				map[i][2] = 255.0l;
			deltas[i][2] = (map[i][2] - cdestb) / (double)fadedist;
		}

		// Now allocate memory for the actual colormap array itself!
		colormap_p = Z_Malloc((256 * (encoremap ? 64 : 32)) + 10, PU_LEVEL, NULL);
		extra_colormaps[mapnum].colormap = (UINT8 *)colormap_p;

		// Calculate the palette index for each palette index, for each light level
		// (as well as the two unused colormap lines we inherited from Doom)
		for (p = 0; p < LIGHTLEVELS; p++)
		{
			for (i = 0; i < 256; i++)
			{
				*colormap_p = NearestColor((UINT8)RoundUp(map[i][0]),
					(UINT8)RoundUp(map[i][1]),
					(UINT8)RoundUp(map[i][2]));
				colormap_p++;

				if ((UINT32)p < fadestart)
					continue;
#define ABS2(x) ((x) < 0 ? -(x) : (x))
				if (ABS2(map[i][0] - cdestr) > ABS2(deltas[i][0]))
					map[i][0] -= deltas[i][0];
				else
					map[i][0] = cdestr;

				if (ABS2(map[i][1] - cdestg) > ABS2(deltas[i][1]))
					map[i][1] -= deltas[i][1];
				else
					map[i][1] = cdestg;

				if (ABS2(map[i][2] - cdestb) > ABS2(deltas[i][1]))
					map[i][2] -= deltas[i][2];
				else
					map[i][2] = cdestb;
#undef ABS2
			}
		}

		if (encoremap)
		{
			lighttable_t *colormap_p2 = extra_colormaps[mapnum].colormap;

			for (p = 0; p < LIGHTLEVELS; p++)
			{
				for (i = 0; i < 256; i++)
				{
					*colormap_p = colormap_p2[encoremap[i]];
					colormap_p++;
				}
				colormap_p2 += 256;
			}
		}
	}

	return (INT32)mapnum;
}

// Thanks to quake2 source!
// utils3/qdata/images.c
UINT8 NearestPaletteColor(UINT8 r, UINT8 g, UINT8 b, RGBA_t *palette)
{
	int dr, dg, db;
	int distortion, bestdistortion = 256 * 256 * 4, bestcolor = 0, i;

	// Use local palette if none specified
	if (palette == NULL)
		palette = pLocalPalette;

	for (i = 0; i < 256; i++)
	{
		dr = r - palette[i].s.red;
		dg = g - palette[i].s.green;
		db = b - palette[i].s.blue;
		distortion = dr*dr + dg*dg + db*db;
		if (distortion < bestdistortion)
		{
			if (!distortion)
				return (UINT8)i;

			bestdistortion = distortion;
			bestcolor = i;
		}
	}

	return (UINT8)bestcolor;
}

// Rounds off floating numbers and checks for 0 - 255 bounds
static int RoundUp(double number)
{
	if (number > 255.0l)
		return 255;
	if (number < 0.0l)
		return 0;

	if ((int)number <= (int)(number - 0.5f))
		return (int)number + 1;

	return (int)number;
}

const char *R_ColormapNameForNum(INT32 num)
{
	if (num == -1)
		return "NONE";

	if (num < 0 || num >= MAXCOLORMAPS)
		I_Error("R_ColormapNameForNum: num %d is invalid!\n", num);

	if (foundcolormaps[num] == LUMPERROR)
		return "INLEVEL";

	return W_CheckNameForNum(foundcolormaps[num]);
}


//
// R_InitData
//
// Locates all the lumps that will be used by all views
// Must be called after W_Init.
//
void R_InitData(void)
{
	CONS_Printf("R_LoadTextures()...\n");
	R_LoadTextures();

	CONS_Printf("P_InitPicAnims()...\n");
	P_InitPicAnims();

	CONS_Printf("R_InitSprites()...\n");
	R_InitSpriteLumps();
	R_InitSprites();

	CONS_Printf("R_InitColormaps()...\n");
	R_InitColormaps();
}

void R_ClearTextureNumCache(boolean btell)
{
	Z_Free(tidcache);
	tidcache = NULL;
	if (btell)
		CONS_Debug(DBG_SETUP, "Fun Fact: There are %d textures used in this map.\n", tidcachelen);
	tidcachelen = 0;
}

static void AddTextureToCache(const char *name, UINT32 hash, INT32 id)
{
	tidcachelen++;
	Z_Realloc(tidcache, tidcachelen * sizeof(*tidcache), PU_STATIC, &tidcache);
	strncpy(tidcache[tidcachelen-1].name, name, 8);
	tidcache[tidcachelen-1].name[8] = '\0';
#ifndef ZDEBUG
	CONS_Debug(DBG_SETUP, "texture #%s: %s\n", sizeu1(tidcachelen), tidcache[tidcachelen-1].name);
#endif
	tidcache[tidcachelen-1].hash = hash;
	tidcache[tidcachelen-1].id = id;
}


//
// R_CheckTextureNumForName
//
// Check whether texture is available. Filter out NoTexture indicator.
//
INT32 R_CheckTextureNumForName(const char *name)
{
	INT32 i;
	UINT32 hash;

	// "NoTexture" marker.
	if (name[0] == '-')
		return 0;

	hash = FNV1a_QuickCaseHash(name, 8);

	for (i = 0; i < tidcachelen; i++)
		if (tidcache[i].hash == hash && !strncasecmp(tidcache[i].name, name, 8))
			return tidcache[i].id;

	// Need to parse the list backwards, so textures loaded more recently are used in lieu of ones loaded earlier
	for (i = (numtextures - 1); i >= 0; i--)
		if (textures[i]->hash == hash && !strncasecmp(textures[i]->name, name, 8))
		{
			AddTextureToCache(name, hash, i);
			return i;
		}

	return -1;
}

//
// R_TextureNumForName
//
// Calls R_CheckTextureNumForName, aborts with error message.
//
INT32 R_TextureNumForName(const char *name)
{
	const INT32 i = R_CheckTextureNumForName(name);

	if (i == -1)
	{
		static INT32 redwall = -2;
		CONS_Debug(DBG_SETUP, "WARNING: R_TextureNumForName: %.8s not found\n", name);
		if (redwall == -2)
			redwall = R_CheckTextureNumForName("REDWALL");
		if (redwall != -1)
			return redwall;
		return 1;
	}
	return i;
}

static void R_PrecacheLevelTextures(void)
{
	char *texturepresent;
	anim_t *anim;
	size_t j;
	INT32 h;

	// no need to precache all software textures in 3D mode
	// (note they are still used with the reference software view)
	texturepresent = calloc(numtextures, sizeof (*texturepresent));
	if (texturepresent == NULL) I_Error("%s: Out of memory looking up textures", "R_PrecacheLevel");

	for (j = 0; j < numsides; j++)
	{
		// huh, a potential bug here????
		if (sides[j].toptexture >= 0 && sides[j].toptexture < numtextures)
			texturepresent[sides[j].toptexture] = 1;
		if (sides[j].midtexture >= 0 && sides[j].midtexture < numtextures)
			texturepresent[sides[j].midtexture] = 1;
		if (sides[j].bottomtexture >= 0 && sides[j].bottomtexture < numtextures)
			texturepresent[sides[j].bottomtexture] = 1;
	}

	// check for animated textures
	for (anim = anims; anim < lastanim; anim++)
	{
		if (!anim->istexture)
			continue;

		if (!texturepresent[anim->basepic])
			continue;

		for (h = 1; h < anim->numpics; h++)
		{
			if (!texturecache[anim->basepic+h])
				R_GenerateTexture(anim->basepic+h);
		}
	}

	// Sky texture is always present.
	// Note that F_SKY1 is the name used to indicate a sky floor/ceiling as a flat,
	// while the sky texture is stored like a wall texture, with a skynum dependent name.
	texturepresent[skytexture] = 1;

	texturememory = 0;
	for (j = 0; j < (unsigned)numtextures; j++)
	{
		if (!texturepresent[j])
			continue;

		if (!texturecache[j])
			R_GenerateTexture(j);
		// pre-caching individual patches that compose textures became obsolete,
		// since we cache entire composite textures
	}
	free(texturepresent);
}

static void R_PrecacheLevelSprites(void)
{
	char *spritepresent;
	size_t i, j, k;
	lumpnum_t lump;

	thinker_t *th;
	spriteframe_t *sf;

	spritepresent = calloc(numsprites, sizeof (*spritepresent));
	if (spritepresent == NULL) I_Error("%s: Out of memory looking up sprites", "R_PrecacheLevel");

	for (th = thinkercap.next; th != &thinkercap; th = th->next)
	{
		if (th->function != (actionf_p1)P_MobjThinker)
			continue;

		spritepresent[((mobj_t *)th)->sprite] = 1;
	}

	spritememory = 0;
	for (i = 0; i < numsprites; i++)
	{
		if (!spritepresent[i])
			continue;

		for (j = 0; j < sprites[i].numframes; j++)
		{
			sf = &sprites[i].spriteframes[j];
#define cacheang(a) {\
				lump = sf->lumppat[a];\
				if (devparm)\
					spritememory += W_LumpLength(lump);\
				W_CachePatchNum(lump, PU_SPRITE);\
			}
			// see R_InitSprites for more about lumppat,lumpid
			switch (sf->rotate)
			{
				case SRF_SINGLE:
					cacheang(0);
					break;
				case SRF_2D:
					cacheang(2);
					cacheang(6);
					break;
				default:
					k = 8;
					while (k--)
						cacheang(k);
					break;
			}
#undef cacheang
		}
	}

	free(spritepresent);
}

//
// R_PrecacheLevel
//
// Preloads all relevant graphics for the level.
//
void R_PrecacheLevel(void)
{
	// do not flush the memory, Z_Malloc twice with same user will cause error in Z_CheckHeap()
	if (rendermode == render_none)
		return;

	if (demo.playback)
		return;

#ifdef HWRENDER
	if (rendermode == render_opengl)
	{
		HWR_PrecacheLevel();
		return;
	}
#endif

	// Precache flats.
	flatmemory = P_PrecacheLevelFlats();

	// Precache textures.
	R_PrecacheLevelTextures();

	// Precache sprites.
	R_PrecacheLevelSprites();

	// FIXME: this is no longer correct with OpenGL render mode
	CONS_Debug(DBG_SETUP, "Precache level done:\n"
			"flatmemory:    %s k\n"
			"texturememory: %s k\n"
			"spritememory:  %s k\n", sizeu1(flatmemory>>10), sizeu2(texturememory>>10), sizeu3(spritememory>>10));
}
