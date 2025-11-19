// SONIC ROBO BLAST 2
//-----------------------------------------------------------------------------
// Copyright (C) 1993-1996 by id Software, Inc.
// Copyright (C) 2005-2009 by Andrey "entryway" Budko.
// Copyright (C) 2018-2020 by Jaime "Lactozilla" Passos.
// Copyright (C) 2019-2020 by Sonic Team Junior.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  r_patch.c
/// \brief Patch generation.

#include "byteptr.h"
#include "doomdef.h"
#include "r_patch.h"
#include "r_things.h"
#include "r_skins.h"
#include "z_zone.h"
#include "w_wad.h"
#include <vector>

#ifdef HWRENDER
#include "hardware/hw_glob.h"
#endif

//
// R_ParseSpriteInfoFrame
//
// Parse a SPRTINFO frame.
//
static void R_ParseSpriteInfoFrame(spriteinfo_t *info)
{
	char *sprinfoToken;
	size_t sprinfoTokenLength;
	char *frameChar = NULL;
	UINT8 frameFrame = 0xFF;
	INT16 frameXPivot = 0;
	INT16 frameYPivot = 0;
	rotaxis_t frameRotAxis = ROTAXIS_X;

	// Sprite identifier
	sprinfoToken = M_GetToken(NULL);
	if (sprinfoToken == NULL)
	{
		I_Error("Error parsing SPRTINFO lump: Unexpected end of file where sprite frame should be");
	}

	sprinfoTokenLength = strlen(sprinfoToken);
	if (sprinfoTokenLength != 1)
	{
		I_Error("Error parsing SPRTINFO lump: Invalid frame \"%s\"",sprinfoToken);
	}

	frameChar = sprinfoToken;

	frameFrame = R_Char2Frame(frameChar[0]);
	Z_Free(sprinfoToken);

	// Left Curly Brace
	sprinfoToken = M_GetToken(NULL);
	if (sprinfoToken == NULL)
		I_Error("Error parsing SPRTINFO lump: Missing sprite info");

	if (fastcmp(sprinfoToken, "{"))
	{
		Z_Free(sprinfoToken);
		sprinfoToken = M_GetToken(NULL);
		if (sprinfoToken == NULL)
		{
			I_Error("Error parsing SPRTINFO lump: Unexpected end of file where sprite info should be");
		}
		while (!fastcmp(sprinfoToken, "}"))
		{
			if (fasticmp(sprinfoToken, "XPIVOT"))
			{
				Z_Free(sprinfoToken);
				sprinfoToken = M_GetToken(NULL);
				frameXPivot = atoi(sprinfoToken);
			}
			else if (fasticmp(sprinfoToken, "YPIVOT"))
			{
				Z_Free(sprinfoToken);
				sprinfoToken = M_GetToken(NULL);
				frameYPivot = atoi(sprinfoToken);
			}
			else if (fasticmp(sprinfoToken, "ROTAXIS"))
			{
				Z_Free(sprinfoToken);
				sprinfoToken = M_GetToken(NULL);
				if (fasticmp(sprinfoToken, "X") || fasticmp(sprinfoToken, "XAXIS") || fasticmp(sprinfoToken, "ROLL"))
					frameRotAxis = ROTAXIS_X;
				else if (fasticmp(sprinfoToken, "Y") || fasticmp(sprinfoToken, "YAXIS") || fasticmp(sprinfoToken, "PITCH"))
					frameRotAxis = ROTAXIS_Y;
				else if (fasticmp(sprinfoToken, "Z") || fasticmp(sprinfoToken, "ZAXIS") || fasticmp(sprinfoToken, "YAW"))
					frameRotAxis = ROTAXIS_Z;
			}

			Z_Free(sprinfoToken);

			sprinfoToken = M_GetToken(NULL);
			if (sprinfoToken == NULL)
			{
				I_Error("Error parsing SPRTINFO lump: Unexpected end of file where sprite info or right curly brace should be");
			}
		}
	}

	Z_Free(sprinfoToken);

	// set fields
	info->pivot[frameFrame].x = frameXPivot;
	info->pivot[frameFrame].y = frameYPivot;
	info->pivot[frameFrame].rotaxis = frameRotAxis;
}

//
// R_ParseSpriteInfo
//
// Parse a SPRTINFO lump.
//
static void R_ParseSpriteInfo(boolean spr2)
{
	spriteinfo_t *info;
	char *sprinfoToken;
	size_t sprinfoTokenLength;
	char newSpriteName[5]; // no longer dynamically allocated
	spritenum_t sprnum = NUMSPRITES;
	//playersprite_t spr2num = NUMPLAYERSPRITES;
	INT32 i;
	INT32 skinnumbers[MAXSKINS]; // >variable 'skinnumbers' set but not used >compile error when commented out
	INT32 foundskins = 0;

	// Sprite name
	sprinfoToken = M_GetToken(NULL);
	if (sprinfoToken == NULL)
	{
		I_Error("Error parsing SPRTINFO lump: Unexpected end of file where sprite name should be");
	}

	sprinfoTokenLength = strlen(sprinfoToken);
	if (sprinfoTokenLength != 4)
	{
		I_Error("Error parsing SPRTINFO lump: Sprite name \"%s\" isn't 4 characters long",sprinfoToken);
	}
	else
	{
		memset(&newSpriteName, 0, 5);
		memcpy(newSpriteName, sprinfoToken, sprinfoTokenLength);
		// ^^ we've confirmed that the token is == 4 characters so it will never overflow a 5 byte char buffer
		strupr(newSpriteName); // Just do this now so we don't have to worry about it
	}

	Z_Free(sprinfoToken);

	if (!spr2)
	{
		for (i = 0; i <= NUMSPRITES; i++)
		{
			if (i == NUMSPRITES)
				I_Error("Error parsing SPRTINFO lump: Unknown sprite name \"%s\"", newSpriteName);

			if (!memcmp(newSpriteName,sprnames[i], 4))
			{
				sprnum = static_cast<spritenum_t>(i);
				break;
			}
		}
	}

	// allocate a spriteinfo
	info = static_cast<spriteinfo_t*>(Z_Calloc(sizeof(spriteinfo_t), PU_STATIC, NULL));
	info->available = true;

	// Left Curly Brace
	sprinfoToken = M_GetToken(NULL);
	if (sprinfoToken == NULL)
	{
		I_Error("Error parsing SPRTINFO lump: Unexpected end of file where open curly brace for sprite \"%s\" should be",newSpriteName);
	}

	if (fastcmp(sprinfoToken, "{"))
	{
		Z_Free(sprinfoToken);

		sprinfoToken = M_GetToken(NULL);
		if (sprinfoToken == NULL)
		{
			I_Error("Error parsing SPRTINFO lump: Unexpected end of file where definition for sprite \"%s\" should be",newSpriteName);
		}

		while (!fastcmp(sprinfoToken, "}"))
		{
			if (fasticmp(sprinfoToken, "SKIN"))
			{
				INT32 skinnum;
				char *skinName = NULL;
				if (!spr2)
					I_Error("Error parsing SPRTINFO lump: \"SKIN\" token found outside of a sprite2 definition");

				Z_Free(sprinfoToken);

				// Skin name
				sprinfoToken = M_GetToken(NULL);
				if (sprinfoToken == NULL)
				{
					I_Error("Error parsing SPRTINFO lump: Unexpected end of file where skin frame should be");
				}

				// copy skin name yada yada
				sprinfoTokenLength = strlen(sprinfoToken);
				skinName = (char *)Z_Malloc((sprinfoTokenLength+1)*sizeof(char),PU_STATIC,NULL);
				memcpy(skinName,sprinfoToken,sprinfoTokenLength*sizeof(char));
				skinName[sprinfoTokenLength] = '\0';
				strlwr(skinName);
				Z_Free(sprinfoToken);

				skinnum = R_AnySkinAvailable(skinName);
				if (skinnum == -1)
					I_Error("Error parsing SPRTINFO lump: Unknown skin \"%s\"", skinName);

				skinnumbers[foundskins] = skinnum;
				foundskins++;
			}
			else if (fasticmp(sprinfoToken, "FRAME"))
			{
				R_ParseSpriteInfoFrame(info);
				Z_Free(sprinfoToken);

				if (spr2)
				{
					if (!foundskins)
						I_Error("Error parsing SPRTINFO lump: No skins specified in this sprite2 definition");
					for (i = 0; i < foundskins; i++)
					{
						size_t skinnum = skinnumbers[i];
						skin_t *skin;
						if (allskins[skinnum].localskin)
							skin = &localskins[allskins[skinnum].localnum];
						else
							skin = &skins[allskins[skinnum].localnum];
						memcpy(&skin->sprinfo, info, sizeof(spriteinfo_t));
					}
				}
				else
					memcpy(&spriteinfo[sprnum], info, sizeof(spriteinfo_t));
			}
			else
			{
				I_Error("Error parsing SPRTINFO lump: Unknown keyword \"%s\" in sprite %s",sprinfoToken,newSpriteName);
			}

			sprinfoToken = M_GetToken(NULL);
			if (sprinfoToken == NULL)
			{
				I_Error("Error parsing SPRTINFO lump: Unexpected end of file where sprite info or right curly brace for sprite \"%s\" should be",newSpriteName);
			}
		}
	}
	else
	{
		I_Error("Error parsing SPRTINFO lump: Expected \"{\" for sprite \"%s\", got \"%s\"",newSpriteName,sprinfoToken);
	}

	Z_Free(sprinfoToken);
	Z_Free(info);
}

//
// R_ParseSPRTINFOLump
//
// Read a SPRTINFO lump.
//
void R_ParseSPRTINFOLump(UINT16 wadNum, UINT16 lumpNum)
{
	char *sprinfoLump;
	size_t sprinfoLumpLength;
	char *sprinfoText;
	char *sprinfoToken;

	// Since lumps AREN'T \0-terminated like I'd assumed they should be, I'll
	// need to make a space of memory where I can ensure that it will terminate
	// correctly. Start by loading the relevant data from the WAD.
	sprinfoLump = (char *)W_CacheLumpNumPwad(wadNum, lumpNum, PU_STATIC);
	// If that didn't exist, we have nothing to do here.
	if (sprinfoLump == NULL) return;
	// If we're still here, then it DOES exist; figure out how long it is, and allot memory accordingly.
	sprinfoLumpLength = W_LumpLengthPwad(wadNum, lumpNum);
	sprinfoText = (char *)Z_Malloc((sprinfoLumpLength+1)*sizeof(char),PU_STATIC,NULL);
	// Now move the contents of the lump into this new location.
	memmove(sprinfoText,sprinfoLump,sprinfoLumpLength);
	// Make damn well sure the last character in our new memory location is \0.
	sprinfoText[sprinfoLumpLength] = '\0';
	// Finally, free up the memory from the first data load, because we really
	// don't need it.
	Z_Free(sprinfoLump);

	sprinfoToken = M_GetToken(sprinfoText);
	while (sprinfoToken != NULL)
	{
		CONS_Printf("Attempting to parse le SPRTINFO lump for ya..\n");

		if (fasticmp(sprinfoToken, "SPRITE"))
			R_ParseSpriteInfo(false);
		else if (fasticmp(sprinfoToken, "SPRITE2"))
			R_ParseSpriteInfo(true);
		else
			I_Error("Error parsing SPRTINFO lump: Unknown keyword \"%s\"", sprinfoToken);

		Z_Free(sprinfoToken);
		sprinfoToken = M_GetToken(NULL);
	}

	Z_Free((void *)sprinfoText);
}

//
// R_LoadSpriteInfoLumps
//
// Load and read every SPRTINFO lump from the specified file.
//
void R_LoadSpriteInfoLumps(UINT16 wadnum, UINT16 numlumps)
{
	lumpinfo_t *lumpinfo = wadfiles[wadnum]->lumpinfo;
	UINT16 i;
	char *name;

	for (i = 0; i < numlumps; i++, lumpinfo++)
	{
		name = lumpinfo->name;
		// Load SPRTINFO and SPR_ lumps as SpriteInfo
		if (!memcmp(name, "SPRTINFO", 8) || !memcmp(name, "SPR_", 4))
			R_ParseSPRTINFOLump(wadnum, i);
	}
}

//
// Creates a patch.
// Assumes a PU_PATCH zone memory tag and no user, but can always be set later
//

patch_t *Patch_Create(softwarepatch_t *source, size_t srcsize, void *dest)
{
	patch_t *patch = static_cast<patch_t*>((dest == NULL) ? Z_Calloc(sizeof(patch_t), PU_PATCH, NULL) : dest);

	if (source)
	{
		INT32 col, colsize;
		size_t size = sizeof(INT32) * SHORT(source->width);
		size_t offs = (sizeof(INT16) * 4) + size;

		patch->width      = SHORT(source->width);
		patch->height     = SHORT(source->height);
		patch->leftoffset = SHORT(source->leftoffset);
		patch->topoffset  = SHORT(source->topoffset);
		patch->columnofs  = static_cast<INT32*>(Z_Calloc(size, PU_PATCH_DATA, NULL));

		for (col = 0; col < source->width; col++)
		{
			// This makes the column offsets relative to the column data itself,
			// instead of the entire patch data
			patch->columnofs[col] = LONG(source->columnofs[col]) - offs;
		}

		if (!srcsize)
			I_Error("Patch_Create: no source size!");

		colsize = (INT32)(srcsize) - (INT32)offs;
		if (colsize <= 0)
			I_Error("Patch_Create: no column data!");

		patch->columns = static_cast<UINT8*>(Z_Calloc(colsize, PU_PATCH_DATA, NULL));
		memcpy(patch->columns, ((UINT8 *)source + LONG(source->columnofs[0])), colsize);
	}

	return patch;
}

//
// Frees a patch from memory.
//

static void Patch_FreeData(patch_t *patch)
{
#ifdef HWRENDER
	if (patch->hardware)
		HWR_FreeTexture(patch);
#endif

#ifdef ROTSPRITE
	/*if (patch->rotated)
	{
		rotsprite_t *rotsprite = patch->rotated;
		INT32 i = 0;

		for (; i < rotsprite->angles; i++)
		{
			if (rotsprite->patches[i])
				Patch_Free(rotsprite->patches[i]);
		}

		Z_Free(rotsprite->patches);
		Z_Free(rotsprite);
	}*/
#endif

	if (patch->columnofs)
		Z_Free(patch->columnofs);
	if (patch->columns)
		Z_Free(patch->columns);
}

void Patch_Free(patch_t *patch)
{
	Patch_FreeData(patch);
	Z_Free(patch);
}

//
// Frees patches with a tag range.
//

static boolean Patch_FreeTagsCallback(void *mem)
{
	patch_t *patch = (patch_t *)mem;
	Patch_FreeData(patch);
	return true;
}

void Patch_FreeTags(INT32 lowtag, INT32 hightag)
{
	Z_IterateTags(lowtag, hightag, Patch_FreeTagsCallback);
}

#ifdef HWRENDER
//
// Allocates a hardware patch.
//

void *Patch_AllocateHardwarePatch(patch_t *patch)
{
	if (!patch->hardware)
	{
		GLPatch_t *glPatch = static_cast<GLPatch_t*>(Z_Calloc(sizeof(GLPatch_t), PU_HWRPATCHINFO, &patch->hardware));
		glPatch->mipmap = static_cast<GLMipmap_t*>(Z_Calloc(sizeof(GLMipmap_t), PU_HWRPATCHINFO, &glPatch->mipmap));
	}
	return (void *)(patch->hardware);
}

//
// Creates a hardware patch.
//

void *Patch_CreateGL(patch_t *patch)
{
	GLPatch_t *glPatch = (GLPatch_t *)Patch_AllocateHardwarePatch(patch);
	if (!glPatch->mipmap->data) // Run HWR_MakePatch in all cases, to recalculate some things
		HWR_MakePatch(patch, glPatch, glPatch->mipmap, false);
	return glPatch;
}
#endif // HWRENDER

// 1 MB should be enough for most cases
static std::vector<UINT8> imgbuf(1024 * 1024);

//
// R_MaskedFlatToPatch
//
// Convert raw pixels to a patch.
//
void *R_PixelsToPatch(UINT8 *raw, INT16 width, INT16 height, INT16 leftoffset, INT16 topoffset, size_t *destsize)
{
	INT16 x, y;
	UINT8 *img, *imgptr;
	UINT8 *colpointers, *startofspan;
	size_t size = 0;

	if (!raw)
		return NULL;

	// Allocate a staging buffer with the maximum size needed for a patch of the same size as the input.

	// round up to nearest multiple of 254-pixel posts, plus 1 more 254-pixel post for paranoia reasons
	size_t maxcolumnsize = (2 + (height - 1) / 256) * 256;
	// the patch header, and width columns of the max column size
	size_t maxoutsize = maxcolumnsize * width + (8 + 4 * width);
	// so, a 512x512 flat should maximally need 393,760 (384.53 KiB) bytes.
	// quite a bit smaller than 64 megabytes, and much less annoying to the windows debug allocator!

	imgbuf.assign(maxoutsize, 0);

	imgptr = imgbuf.data();

	// Write image size and offset
	WRITEINT16(imgptr, width);
	WRITEINT16(imgptr, height);
	WRITEINT16(imgptr, leftoffset);
	WRITEINT16(imgptr, topoffset);

	// Leave placeholder to column pointers
	colpointers = imgptr;
	imgptr += width*4;

	// Write columns
	for (x = 0; x < width; x++)
	{
		int lastStartY = 0;
		int spanSize = 0;
		startofspan = NULL;

		// Write column pointer
		WRITEINT32(colpointers, imgptr - imgbuf.data());

		// Write pixels
		for (y = 0; y < height; y++)
		{
			UINT8 pixel = raw[((y * width) + x)];
			UINT8 opaque = (pixel != 0xf7); // If not 247 (0xf7), we have a pixel

			// End span if we have a transparent pixel
			if (!opaque)
			{
				if (startofspan)
					WRITEUINT8(imgptr, 0);
				startofspan = NULL;
				continue;
			}

			// Start new column if we need to
			if (!startofspan || spanSize == 255)
			{
				int writeY = y;

				// If we reached the span size limit, finish the previous span
				if (startofspan)
					WRITEUINT8(imgptr, 0);

				if (y > 254)
				{
					// Make sure we're aligned to 254
					if (lastStartY < 254)
					{
						WRITEUINT8(imgptr, 254);
						WRITEUINT8(imgptr, 0);
						imgptr += 2;
						lastStartY = 254;
					}

					// Write stopgap empty spans if needed
					writeY = y - lastStartY;

					while (writeY > 254)
					{
						WRITEUINT8(imgptr, 254);
						WRITEUINT8(imgptr, 0);
						imgptr += 2;
						writeY -= 254;
					}
				}

				startofspan = imgptr;
				WRITEUINT8(imgptr, writeY);
				imgptr += 2;
				spanSize = 0;

				lastStartY = y;
			}

			// Write the pixel
			WRITEUINT8(imgptr, pixel);
			spanSize++;
			startofspan[1] = spanSize;
		}

		if (startofspan)
			WRITEUINT8(imgptr, 0);

		WRITEUINT8(imgptr, 0xFF);
	}

	size = imgptr-imgbuf.data();
	img = static_cast<UINT8*>(Z_Malloc(size, PU_STATIC, NULL));
	memcpy(img, imgbuf.data(), size);

	if (destsize != NULL)
		*destsize = size;

	patch_t *converted = Patch_Create((softwarepatch_t *)img, size, NULL);
#ifdef HWRENDER
	Patch_CreateGL(converted);
#endif
	Z_Free(img);

	return converted;
}

//
// R_MaskedFlatToPatch
//
// Convert a masked flat to a patch.
// Explanation of "masked" flats in R_PatchToMaskedFlat.
//
void *R_MaskedFlatToPatch(UINT16 *raw, INT16 width, INT16 height, INT16 leftoffset, INT16 topoffset, size_t *destsize)
{
	INT16 x, y;
	UINT8 *img, *imgptr;
	UINT8 *colpointers, *startofspan;
	size_t size = 0;

	if (!raw)
		return NULL;

	// Allocate a staging buffer with the maximum size needed for a patch of the same size as the input.

	// round up to nearest multiple of 254-pixel posts, plus 1 more 254-pixel post for paranoia reasons
	size_t maxcolumnsize = (2 + (height - 1) / 256) * 256;
	// the patch header, and width columns of the max column size
	size_t maxoutsize = maxcolumnsize * width + (8 + 4 * width);
	// so, a 512x512 flat should maximally need 393,760 (384.53 KiB) bytes.
	// quite a bit smaller than 64 megabytes, and much less annoying to the windows debug allocator!

	imgbuf.assign(maxoutsize, 0);

	imgptr = imgbuf.data();

	// Write image size and offset
	WRITEINT16(imgptr, width);
	WRITEINT16(imgptr, height);
	WRITEINT16(imgptr, leftoffset);
	WRITEINT16(imgptr, topoffset);

	// Leave placeholder to column pointers
	colpointers = imgptr;
	imgptr += width*4;

	// Write columns
	for (x = 0; x < width; x++)
	{
		int lastStartY = 0;
		int spanSize = 0;
		startofspan = NULL;

		// Write column pointer
		WRITEINT32(colpointers, imgptr - imgbuf.data());

		// Write pixels
		for (y = 0; y < height; y++)
		{
			UINT16 pixel = raw[((y * width) + x)];
			UINT8 paletteIndex = (pixel & 0xFF);
			UINT8 opaque = (pixel != 0xFF00); // If 1, we have a pixel

			// End span if we have a transparent pixel
			if (!opaque)
			{
				if (startofspan)
					WRITEUINT8(imgptr, 0);
				startofspan = NULL;
				continue;
			}

			// Start new column if we need to
			if (!startofspan || spanSize == 255)
			{
				int writeY = y;

				// If we reached the span size limit, finish the previous span
				if (startofspan)
					WRITEUINT8(imgptr, 0);

				if (y > 254)
				{
					// Make sure we're aligned to 254
					if (lastStartY < 254)
					{
						WRITEUINT8(imgptr, 254);
						WRITEUINT8(imgptr, 0);
						imgptr += 2;
						lastStartY = 254;
					}

					// Write stopgap empty spans if needed
					writeY = y - lastStartY;

					while (writeY > 254)
					{
						WRITEUINT8(imgptr, 254);
						WRITEUINT8(imgptr, 0);
						imgptr += 2;
						writeY -= 254;
					}
				}

				startofspan = imgptr;
				WRITEUINT8(imgptr, writeY);
				imgptr += 2;
				spanSize = 0;

				lastStartY = y;
			}

			// Write the pixel
			WRITEUINT8(imgptr, paletteIndex);
			spanSize++;
			startofspan[1] = spanSize;
		}

		if (startofspan)
			WRITEUINT8(imgptr, 0);

		WRITEUINT8(imgptr, 0xFF);
	}

	size = imgptr-imgbuf.data();
	img = static_cast<UINT8*>(Z_Malloc(size, PU_STATIC, NULL));
	memcpy(img, imgbuf.data(), size);

	if (destsize != NULL)
		*destsize = size;

	patch_t *converted = Patch_Create((softwarepatch_t *)img, size, NULL);
#ifdef HWRENDER
	Patch_CreateGL(converted);
#endif
	Z_Free(img);

	return converted;
}

UINT16 R_GetPatchPixel(patch_t *patch, INT32 x, INT32 y, boolean flip)
{
	fixed_t ofs;
	column_t *column;
	UINT8 *source = NULL;
	INT16 width;

	if (patch == NULL)
		I_Error("GetPatchPixel: patch == NULL");

	width = patch->width;

	if (x >= 0 && x < width)
	{
		INT32 colx = flip ? (width-1)-x : x;
		INT32 topdelta, prevdelta = -1;

		column = (column_t *)((UINT8 *)patch->columns + (patch->columnofs[colx]));

		while (column->topdelta != 0xff)
		{
			topdelta = column->topdelta;

			if (topdelta <= prevdelta)
				topdelta += prevdelta;

			prevdelta = topdelta;

			ofs = (y - topdelta);

			if (y >= topdelta && ofs < column->length)
			{
				source = (UINT8 *)(column) + 3;
				return source[ofs];
			}

			column = (column_t *)((UINT8 *)column + column->length + 4);
		}
	}

	return 0xFF00;
}

void R_PatchToPixels(patch_t *patch, UINT8 *dst)
{
	const INT16 width = patch->width;

	// init out destination to cyan so we can easily check for it when we convert it back to a patch
	memset(dst, 0xf7, (width * patch->height)); // should this be here or should this be taken care of before passing it?

	for (INT32 x = 0; x < width; ++x)
	{
		INT32 y = 0;
		INT32 prevdelta = -1;

		column_t *column = (column_t *)((UINT8 *)patch->columns + (patch->columnofs[x]));

		while (column->topdelta != 0xff)
		{
			INT32 topdelta = column->topdelta;

			if (topdelta <= prevdelta)
				topdelta += prevdelta;

			prevdelta = topdelta;

			for (; y-topdelta < column->length; ++y)
			{
				UINT8 pixel = 0;

				if (y >= topdelta)
				{
					UINT8 *source = (UINT8*)(column) + 3;
					pixel = source[y-topdelta];
				}

				dst[(y*width) + x] = pixel;
			}

			column = (column_t *)((UINT8 *)column + column->length + 4);
		}
	}
}
