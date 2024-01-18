// SONIC ROBO BLAST 2
//-----------------------------------------------------------------------------
// Copyright (C) 1993-1996 by id Software, Inc.
// Copyright (C) 1998-2000 by DooM Legacy Team.
// Copyright (C) 1999-2019 by Sonic Team Junior.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file
/// \brief load and convert graphics to the hardware format

#include "../doomdef.h"

#ifdef HWRENDER
#include "hw_main.h"
#include "hw_glob.h"
#include "hw_drv.h"

#include "../doomstat.h"    //gamemode
#include "../i_video.h"     //rendermode
#include "../r_data.h"
#include "../w_wad.h"
#include "../z_zone.h"
#include "../v_video.h"
#include "../r_draw.h"
#include "../r_patch.h"    // patch rotation

// Values set after a call to HWR_ResizeBlock()
static INT32 blocksize, blockwidth, blockheight;

INT32 patchformat = GR_TEXFMT_AP_88; // use alpha for holes
INT32 textureformat = GR_TEXFMT_P_8; // use chromakey for hole

RGBA_t mapPalette[256] = {0}; // the palette for the currently loaded level or menu etc.

// Returns a pointer to the palette which should be used for caching textures.
RGBA_t *HWR_GetTexturePalette(void)
{
	return HWR_ShouldUsePaletteRendering() ? mapPalette : pLocalPalette;
}

// sprite, use alpha and chroma key for hole
static void HWR_DrawPatchInCache(GLMipmap_t *mipmap,
	INT32 pblockwidth, INT32 pblockheight, INT32 blockmodulo,
	INT32 ptexturewidth, INT32 ptextureheight,
	INT32 originx, INT32 originy, // where to draw patch in surface block
	const patch_t *realpatch, INT32 bpp, RGBA_t *palette)
{
	INT32 x, x1, x2;
	INT32 col, ncols;
	fixed_t xfrac, xfracstep;
	fixed_t yfrac, yfracstep, position, count;
	fixed_t scale_y;
	RGBA_t colortemp;
	UINT8 *dest;
	const UINT8 *source;
	const column_t *patchcol;
	UINT8 alpha;
	UINT8 *block = mipmap->grInfo.data;
	UINT8 texel;
	UINT16 texelu16;

	x1 = originx;
	x2 = x1 + SHORT(realpatch->width);

	if (x1 < 0)
		x = 0;
	else
		x = x1;

	if (x2 > ptexturewidth)
		x2 = ptexturewidth;

	if (!ptexturewidth)
		return;
	
	palette = HWR_GetTexturePalette();

	col = x * pblockwidth / ptexturewidth;
	ncols = ((x2 - x) * pblockwidth) / ptexturewidth;

/*
	CONS_Debug(DBG_RENDER, "patch %dx%d texture %dx%d block %dx%d\n", SHORT(realpatch->width),
															SHORT(realpatch->height),
															ptexturewidth,
															textureheight,
															pblockwidth,pblockheight);
	CONS_Debug(DBG_RENDER, "      col %d ncols %d x %d\n", col, ncols, x);
*/

	// source advance
	xfrac = 0;
	if (x1 < 0)
		xfrac = -x1<<FRACBITS;

	xfracstep = (ptexturewidth << FRACBITS) / pblockwidth;
	yfracstep = (ptextureheight<< FRACBITS) / pblockheight;
	if (bpp < 1 || bpp > 4)
		I_Error("HWR_DrawPatchInCache: no drawer defined for this bpp (%d)\n",bpp);

	for (block += col*bpp; ncols--; block += bpp, xfrac += xfracstep)
	{
		INT32 topdelta, prevdelta = -1;
		patchcol = (const column_t *)((const UINT8 *)realpatch
		 + LONG(realpatch->columnofs[xfrac>>FRACBITS]));

		scale_y = (pblockheight << FRACBITS) / ptextureheight;

		while (patchcol->topdelta != 0xff)
		{
			topdelta = patchcol->topdelta;
			if (topdelta <= prevdelta)
				topdelta += prevdelta;
			prevdelta = topdelta;
			source = (const UINT8 *)patchcol + 3;
			count  = ((patchcol->length * scale_y) + (FRACUNIT/2)) >> FRACBITS;
			position = originy + topdelta;

			yfrac = 0;
			//yfracstep = (patchcol->length << FRACBITS) / count;
			if (position < 0)
			{
				yfrac = -position<<FRACBITS;
				count += (((position * scale_y) + (FRACUNIT/2)) >> FRACBITS);
				position = 0;
			}

			position = ((position * scale_y) + (FRACUNIT/2)) >> FRACBITS;

			if (position < 0)
				position = 0;

			if (position + count >= pblockheight)
				count = pblockheight - position;

			dest = block + (position*blockmodulo);
			while (count > 0)
			{
				count--;

				texel = source[yfrac>>FRACBITS];
				alpha = 0xff;

				//Hurdler: not perfect, but better than holes
				if (texel == HWR_PATCHES_CHROMAKEY_COLORINDEX && (mipmap->flags & TF_CHROMAKEYED))
					alpha = 0x00;

					//texel = HWR_CHROMAKEY_EQUIVALENTCOLORINDEX;
				// Lat:  Don't do that, some weirdos still use CYAN on their WALLTEXTURES for translucency :V

				//Hurdler: 25/04/2000: now support colormap in hardware mode
				if (mipmap->colormap)
					texel = mipmap->colormap[texel];

				// hope compiler will get this switch out of the loops (dreams...)
				// gcc do it ! but vcc not ! (why don't use cygwin gcc for win32 ?)
				// Alam: SRB2 uses Mingw, HUGS
				switch (bpp)
				{
					case 2 : texelu16 = (UINT16)((alpha<<8) | texel);
					         memcpy(dest, &texelu16, sizeof(UINT16));
					         break;
					case 3 : colortemp = palette[texel];
					         memcpy(dest, &colortemp, sizeof(RGBA_t)-sizeof(UINT8));
					         break;
					case 4 : colortemp = palette[texel];
					         colortemp.s.alpha = alpha;
					         memcpy(dest, &colortemp, sizeof(RGBA_t));
					         break;
					// default is 1
					default: *dest = texel;
					         break;
				}

				dest += blockmodulo;
				yfrac += yfracstep;
			}
			patchcol = (const column_t *)((const UINT8 *)patchcol + patchcol->length + 4);
		}
	}
}


// resize the patch
// set : blocksize = blockwidth * blockheight  (no bpp used)
//       blockwidth
//       blockheight
//note :  8bit (1 byte per pixel) palettized format
static void HWR_ResizeBlock(INT32 originalwidth, INT32 originalheight,
	GLTexInfo *grInfo)
{
	//   Build the full textures from patches.
	static const GLlod_t gr_lods[9] =
	{
		GR_LOD_LOG2_256,
		GR_LOD_LOG2_128,
		GR_LOD_LOG2_64,
		GR_LOD_LOG2_32,
		GR_LOD_LOG2_16,
		GR_LOD_LOG2_8,
		GR_LOD_LOG2_4,
		GR_LOD_LOG2_2,
		GR_LOD_LOG2_1
	};

	typedef struct
	{
		GLAspectRatio_t aspect;
		float           max_s;
		float           max_t;
	} booring_aspect_t;

	static const booring_aspect_t gr_aspects[8] =
	{
		{GR_ASPECT_LOG2_1x1, 255, 255},
		{GR_ASPECT_LOG2_2x1, 255, 127},
		{GR_ASPECT_LOG2_4x1, 255,  63},
		{GR_ASPECT_LOG2_8x1, 255,  31},

		{GR_ASPECT_LOG2_1x1, 255, 255},
		{GR_ASPECT_LOG2_1x2, 127, 255},
		{GR_ASPECT_LOG2_1x4,  63, 255},
		{GR_ASPECT_LOG2_1x8,  31, 255}
	};

	INT32     j,k;
	INT32     max,min;

	// size up to nearest power of 2
	blockwidth = 1;
	while (blockwidth < originalwidth)
		blockwidth <<= 1;
	// scale down the original graphics to fit in 2048
	if (blockwidth > 2048)
		blockwidth = 2048;

	// size up to nearest power of 2
	blockheight = 1;
	while (blockheight < originalheight)
		blockheight <<= 1;
	// scale down the original graphics to fit in 2048
	if (blockheight > 2048)
		blockheight = 2048;

	// do the boring LOD stuff.. blech!
	if (blockwidth >= blockheight)
	{
		max = blockwidth;
		min = blockheight;
	}
	else
	{
		max = blockheight;
		min = blockwidth;
	}

	for (k = 2048, j = 0; k > max && j < 8; j++)
		k>>=1;
	grInfo->smallLodLog2 = gr_lods[j];
	grInfo->largeLodLog2 = gr_lods[j];

	for (k = max, j = 0; k > min && j < 4; j++)
		k>>=1;
	// aspect ratio too small for 3Dfx (eg: 8x128 is 1x16 : use 1x8)
	if (j == 4)
	{
		j = 3;
		//CONS_Debug(DBG_RENDER, "HWR_ResizeBlock : bad aspect ratio %dx%d\n", blockwidth,blockheight);
		if (blockwidth < blockheight)
			blockwidth = max>>3;
		else
			blockheight = max>>3;
	}
	if (blockwidth < blockheight)
		j += 4;
	grInfo->aspectRatioLog2 = gr_aspects[j].aspect;

	blocksize = blockwidth * blockheight;

	//CONS_Debug(DBG_RENDER, "Width is %d, Height is %d\n", blockwidth, blockheight);
}


static const INT32 format2bpp[16] =
{
	0, //0
	0, //1
	1, //2  GR_TEXFMT_ALPHA_8
	1, //3  GR_TEXFMT_INTENSITY_8
	1, //4  GR_TEXFMT_ALPHA_INTENSITY_44
	1, //5  GR_TEXFMT_P_8
	4, //6  GR_RGBA
	0, //7
	0, //8
	0, //9
	2, //10 GR_TEXFMT_RGB_565
	2, //11 GR_TEXFMT_ARGB_1555
	2, //12 GR_TEXFMT_ARGB_4444
	2, //13 GR_TEXFMT_ALPHA_INTENSITY_88
	2, //14 GR_TEXFMT_AP_88
};

static UINT8 *MakeBlock(GLMipmap_t *grMipmap)
{
	UINT8 *block;
	INT32 bpp, i;
	UINT16 bu16 = ((0x00 <<8) | HWR_PATCHES_CHROMAKEY_COLORINDEX);

	bpp =  format2bpp[grMipmap->grInfo.format];
	block = Z_Malloc(blocksize*bpp, PU_HWRCACHE, &(grMipmap->grInfo.data));

	switch (bpp)
	{
		case 1: memset(block, HWR_PATCHES_CHROMAKEY_COLORINDEX, blocksize); break;
		case 2:
				// fill background with chromakey, alpha = 0
				for (i = 0; i < blocksize; i++)
					memcpy(block+i*sizeof(UINT16), &bu16, sizeof(UINT16));

				break;
		case 4: memset(block, 0x00, blocksize*sizeof(UINT32)); break;
	}

	return block;
}

//
// Create a composite texture from patches, adapt the texture size to a power of 2
// height and width for the hardware texture cache.
//
static void HWR_GenerateTexture(INT32 texnum, GLTexture_t *grtex)
{
	UINT8 *block;
	texture_t *texture;
	texpatch_t *patch;
	patch_t *realpatch;

	INT32 i;
	boolean skyspecial = false; //poor hack for Legacy large skies..
	
	RGBA_t *palette;
	palette = HWR_GetTexturePalette();

	texture = textures[texnum];

	// hack the Legacy skies..
	if (texture->name[0] == 'S' &&
	    texture->name[1] == 'K' &&
	    texture->name[2] == 'Y' &&
	    (texture->name[4] == 0 ||
	     texture->name[5] == 0)
	   )
	{
		skyspecial = true;
		grtex->mipmap.flags = TF_WRAPXY; // don't use the chromakey for sky
	}
	else
		grtex->mipmap.flags = TF_CHROMAKEYED | TF_WRAPXY;

	HWR_ResizeBlock (texture->width, texture->height, &grtex->mipmap.grInfo);
	grtex->mipmap.width = (UINT16)blockwidth;
	grtex->mipmap.height = (UINT16)blockheight;
	//grtex->mipmap.grInfo.format = textureformat;
	
	if (skyspecial)
		grtex->mipmap.grInfo.format = GR_RGBA; // that skyspecial code below assumes this format ...
	else
		grtex->mipmap.grInfo.format = textureformat;

	grtex->mipmap.colormap = colormaps;

#ifdef GLENCORE
	if (encoremap)
		grtex->mipmap.colormap += (256*32);
#endif

	block = MakeBlock(&grtex->mipmap);

	if (skyspecial) //Hurdler: not efficient, but better than holes in the sky (and it's done only at level loading)
	{
		INT32 j;
		RGBA_t col;

		col = palette[HWR_CHROMAKEY_EQUIVALENTCOLORINDEX];
		for (j = 0; j < blockheight; j++)
		{
			for (i = 0; i < blockwidth; i++)
			{
				block[4*(j*blockwidth+i)+0] = col.s.red;
				block[4*(j*blockwidth+i)+1] = col.s.green;
				block[4*(j*blockwidth+i)+2] = col.s.blue;
				block[4*(j*blockwidth+i)+3] = 0xff;
			}
		}
	}

	// Composite the columns together.
	for (i = 0, patch = texture->patches; i < texture->patchcount; i++, patch++)
	{
		realpatch = W_CacheLumpNumPwad(patch->wad, patch->lump, PU_CACHE);
		HWR_DrawPatchInCache(&grtex->mipmap,
		                     blockwidth, blockheight,
		                     blockwidth*format2bpp[grtex->mipmap.grInfo.format],
		                     texture->width, texture->height,
		                     patch->originx, patch->originy,
		                     realpatch,
		                     format2bpp[grtex->mipmap.grInfo.format], palette);
		Z_Unlock(realpatch);
	}
	//Hurdler: not efficient at all but I don't remember exactly how HWR_DrawPatchInCache works :(
	if (format2bpp[grtex->mipmap.grInfo.format]==4)
	{
		for (i = 3; i < blocksize*4; i += 4) // blocksize*4 because blocksize doesn't include the bpp
		{
			if (block[i] == 0)
			{
				grtex->mipmap.flags |= TF_TRANSPARENT;
				break;
			}
		}
	}

	grtex->scaleX = 1.0f/(texture->width*FRACUNIT);
	grtex->scaleY = 1.0f/(texture->height*FRACUNIT);
}

// patch may be NULL if grMipmap has been initialised already and makebitmap is false
void HWR_MakePatch (patch_t *patch, GLPatch_t *grPatch, GLMipmap_t *grMipmap, boolean makebitmap)
{
	INT32 newwidth, newheight;
	
	RGBA_t *palette = HWR_GetTexturePalette();

	// don't do it twice (like a cache)
	if (grMipmap->width == 0)
	{
		// save the original patch header so that the GLPatch can be casted
		// into a standard patch_t struct and the existing code can get the
		// orginal patch dimensions and offsets.
		grPatch->width = SHORT(patch->width);
		grPatch->height = SHORT(patch->height);
		grPatch->leftoffset = SHORT(patch->leftoffset);
		grPatch->topoffset = SHORT(patch->topoffset);

		// resize patch
		HWR_ResizeBlock(SHORT(patch->width), SHORT(patch->height), &grMipmap->grInfo);
		grMipmap->width = (UINT16)blockwidth;
		grMipmap->height = (UINT16)blockheight;

		// no wrap around, no chroma key
		grMipmap->flags = 0;
		// setup the texture info
		grMipmap->grInfo.format = patchformat;
	}
	else
	{
		blockwidth = grMipmap->width;
		blockheight = grMipmap->height;
		blocksize = blockwidth * blockheight;
	}

	Z_Free(grMipmap->grInfo.data);
	grMipmap->grInfo.data = NULL;

	newwidth  = min(grPatch->width, blockwidth);
	newheight = min(grPatch->height, blockheight);

	if (makebitmap)
	{
		MakeBlock(grMipmap);
		HWR_DrawPatchInCache(grMipmap,
			newwidth, newheight,
			blockwidth*format2bpp[grMipmap->grInfo.format],
			grPatch->width, grPatch->height,
			0, 0,
			patch,
			format2bpp[grMipmap->grInfo.format], palette);
	}

	grPatch->max_s = (float)newwidth / (float)blockwidth;
	grPatch->max_t = (float)newheight / (float)blockheight;
}

// =================================================
//             CACHING HANDLING
// =================================================

static size_t gr_numtextures;
static GLTexture_t *gr_textures; // for ALL Doom textures

void HWR_InitTextureCache(void)
{
	gr_numtextures = 0;
	gr_textures = NULL;
}

// Callback function for HWR_FreeTextureCache.
static void FreeMipmapColormap(INT32 patchnum, void *patch)
{
	GLPatch_t* const pat = patch;
	(void)patchnum; //unused

	// The patch must be valid, obviously
	if (!pat)
		return;

	// The mipmap must be valid, obviously
	while (pat->mipmap)
	{
		// Confusing at first, but pat->mipmap->nextcolormap
		// at the beginning of the loop is the first colormap
		// from the linked list of colormaps.
		GLMipmap_t *next = NULL;

		// No mipmap in this patch, break out of the loop.
		if (!pat->mipmap)
			break;

		// No colormap mipmaps either.
		if (!pat->mipmap->nextcolormap)
			break;

		// Set the first colormap to the one that comes after it.
		next = pat->mipmap->nextcolormap;
		pat->mipmap->nextcolormap = next->nextcolormap;

		// Free image data from memory.
		if (next->grInfo.data)
			Z_Free(next->grInfo.data);
		next->grInfo.data = NULL;

		// Free the old colormap from memory.
		free(next);
	}
}

void HWR_FreeTextureCache(void)
{
	INT32 i;
	// free references to the textures
	HWD.pfnClearMipMapCache();

	// free all hardware-converted graphics cached in the heap
	// our gool is only the textures since user of the texture is the texture cache
	Z_FreeTags(PU_HWRCACHE, PU_HWRCACHE);
	Z_FreeTags(PU_HWRCACHE_UNLOCKED, PU_HWRCACHE_UNLOCKED);

	// Alam: free the Z_Blocks before freeing it's users

	// free all patch colormaps after each level: must be done after ClearMipMapCache!
	for (i = 0; i < numwadfiles; i++)
		M_AATreeIterate(wadfiles[i]->hwrcache, FreeMipmapColormap);

	// now the heap don't have any 'user' pointing to our
	// texturecache info, we can free it
	if (gr_textures)
		free(gr_textures);
	gr_textures = NULL;
	gr_numtextures = 0;
}

void HWR_PrepLevelCache(size_t pnumtextures)
{
	// problem: the mipmap cache management hold a list of mipmaps.. but they are
	//           reallocated on each level..
	//sub-optimal, but 1) just need re-download stuff in hardware cache VERY fast
	//   2) sprite/menu stuff mixed with level textures so can't do anything else

	// we must free it since numtextures changed
	HWR_FreeTextureCache();

	gr_numtextures = pnumtextures;
	gr_textures = calloc(pnumtextures, sizeof (*gr_textures));
	if (gr_textures == NULL)
		I_Error("HWR_PrepLevelCache: can't alloc gr_textures");
}

// --------------------------------------------------------------------------
// Make sure texture is downloaded and set it as the source
// --------------------------------------------------------------------------
GLTexture_t *HWR_GetTexture(INT32 tex)
{
	GLTexture_t *grtex;
#ifdef PARANOIA
	if ((unsigned)tex >= gr_numtextures)
		I_Error("HWR_GetTexture: tex >= numtextures\n");
#endif
	grtex = &gr_textures[tex];

	if (!grtex->mipmap.grInfo.data && !grtex->mipmap.downloaded)
		HWR_GenerateTexture(tex, grtex);

	HWD.pfnSetTexture(&grtex->mipmap);

	// The system-memory data can be purged now.
	Z_ChangeTag(grtex->mipmap.grInfo.data, PU_HWRCACHE_UNLOCKED);

	return grtex;
}


static void HWR_CacheFlat(GLMipmap_t *grMipmap, lumpnum_t flatlumpnum)
{
#ifdef GLENCORE
	UINT8 *flat;
	size_t steppy;
#endif
	size_t size, pflatsize;


	// setup the texture info
	grMipmap->grInfo.smallLodLog2 = GR_LOD_LOG2_64;
	grMipmap->grInfo.largeLodLog2 = GR_LOD_LOG2_64;
	grMipmap->grInfo.aspectRatioLog2 = GR_ASPECT_LOG2_1x1;
	grMipmap->grInfo.format = GR_TEXFMT_P_8;
	grMipmap->flags = TF_WRAPXY|TF_CHROMAKEYED;

	size = W_LumpLength(flatlumpnum);

	switch (size)
	{
		case 4194304: // 2048x2048 lump
			pflatsize = 2048;
			break;
		case 1048576: // 1024x1024 lump
			pflatsize = 1024;
			break;
		case 262144:// 512x512 lump
			pflatsize = 512;
			break;
		case 65536: // 256x256 lump
			pflatsize = 256;
			break;
		case 16384: // 128x128 lump
			pflatsize = 128;
			break;
		case 1024: // 32x32 lump
			pflatsize = 32;
			break;
		default: // 64x64 lump
			pflatsize = 64;
			break;
	}
	grMipmap->width  = (UINT16)pflatsize;
	grMipmap->height = (UINT16)pflatsize;

	// the flat raw data needn't be converted with palettized textures
	W_ReadLump(flatlumpnum, Z_Malloc(W_LumpLength(flatlumpnum),
		PU_HWRCACHE, &grMipmap->grInfo.data));

#ifdef GLENCORE
	flat = grMipmap->grInfo.data;
	for (steppy = 0; steppy < size; steppy++)
		if (flat[steppy] != HWR_PATCHES_CHROMAKEY_COLORINDEX)
			flat[steppy] = grMipmap->colormap[flat[steppy]];
#endif
}


// Download a Doom 'flat' to the hardware cache and make it ready for use
void HWR_GetFlat(lumpnum_t flatlumpnum, boolean noencoremap)
{
	GLMipmap_t *grmip;

	grmip = HWR_GetCachedGLPatch(flatlumpnum)->mipmap;

	grmip->colormap = colormaps;

#ifdef GLENCORE
	if (!noencoremap && encoremap)
		grmip->colormap += (256*32);
#endif

	if (!grmip->downloaded && !grmip->grInfo.data)
		HWR_CacheFlat(grmip, flatlumpnum);

	HWD.pfnSetTexture(grmip);

	// The system-memory data can be purged now.
	Z_ChangeTag(grmip->grInfo.data, PU_HWRCACHE_UNLOCKED);
}

//
// HWR_LoadMappedPatch(): replace the skin color of the sprite in cache
//                          : load it first in doom cache if not already
//
static void HWR_LoadMappedPatch(GLMipmap_t *grmip, GLPatch_t *gpatch)
{
	if (!grmip->downloaded && !grmip->grInfo.data)
	{
		patch_t *patch = gpatch->rawpatch;
		if (!patch)
				patch = W_CacheLumpNumPwad(gpatch->wadnum, gpatch->lumpnum, PU_STATIC);
		HWR_MakePatch(patch, gpatch, grmip, true);

		// You can't free rawpatch for some reason?
		// (Obviously I can't, sprite rotation needs that...)

		if (!gpatch->rawpatch)
			Z_Free(patch);
	}


	HWD.pfnSetTexture(grmip);

	// The system-memory data can be purged now.
	Z_ChangeTag(grmip->grInfo.data, PU_HWRCACHE_UNLOCKED);
}

// -----------------+
// HWR_GetPatch     : Download a patch to the hardware cache and make it ready for use
// -----------------+
void HWR_GetPatch(GLPatch_t *gpatch)
{
		/*if (needpatchflush)
			W_FlushCachedPatches();*/

	// is it in hardware cache
	if (!gpatch->mipmap->downloaded && !gpatch->mipmap->grInfo.data)
	{
		// load the software patch, PU_STATIC or the Z_Malloc for hardware patch will
		// flush the software patch before the conversion! oh yeah I suffered
		patch_t *ptr = gpatch->rawpatch;
		if (!ptr)
			ptr = W_CacheLumpNumPwad(gpatch->wadnum, gpatch->lumpnum, PU_STATIC);
		HWR_MakePatch(ptr, gpatch, gpatch->mipmap, true);

		// this is inefficient.. but the hardware patch in heap is purgeable so it should
		// not fragment memory, and besides the REAL cache here is the hardware memory
		if (!gpatch->rawpatch)
			Z_Free(ptr);
	}

	HWD.pfnSetTexture(gpatch->mipmap);

	// The system-memory patch data can be purged now.
	Z_ChangeTag(gpatch->mipmap->grInfo.data, PU_HWRCACHE_UNLOCKED);
}


// -------------------+
// HWR_GetMappedPatch : Same as HWR_GetPatch for sprite color
// -------------------+
void HWR_GetMappedPatch(GLPatch_t *gpatch, const UINT8 *colormap)
{
	GLMipmap_t *grmip, *newmip;

	if (colormap == colormaps || colormap == NULL)
	{
		// Load the default (green) color in doom cache (temporary?) AND hardware cache
		HWR_GetPatch(gpatch);
		return;
	}

	// search for the mimmap
	// skip the first (no colormap translated)
	for (grmip = gpatch->mipmap; grmip->nextcolormap; )
	{
		grmip = grmip->nextcolormap;
		if (grmip->colormap == colormap)
		{
			HWR_LoadMappedPatch(grmip, gpatch);
			return;
		}
	}
	// not found, create it!
	// If we are here, the sprite with the current colormap is not already in hardware memory

	//BP: WARNING: don't free it manually without clearing the cache of harware renderer
	//              (it have a liste of mipmap)
	//    this malloc is cleared in HWR_FreeTextureCache
	//    (...) unfortunately z_malloc fragment alot the memory :(so malloc is better
	newmip = calloc(1, sizeof (*newmip));
	if (newmip == NULL)
		I_Error("%s: Out of memory", "HWR_GetMappedPatch");
	grmip->nextcolormap = newmip;

	newmip->colormap = colormap;
	HWR_LoadMappedPatch(newmip, gpatch);
}

void HWR_UnlockCachedPatch(GLPatch_t *gpatch)
{
	if (!gpatch)
		return;

	Z_ChangeTag(gpatch->mipmap->grInfo.data, PU_HWRCACHE_UNLOCKED);
	Z_ChangeTag(gpatch, PU_HWRPATCHINFO_UNLOCKED);
}

GLPatch_t *HWR_GetCachedGLPatchPwad(UINT16 wadnum, UINT16 lumpnum)
{
	aatree_t *hwrcache = wadfiles[wadnum]->hwrcache;
	GLPatch_t *grpatch;

	if (!(grpatch = M_AATreeGet(hwrcache, lumpnum)))
	{
		grpatch = Z_Calloc(sizeof(GLPatch_t), PU_HWRPATCHINFO, NULL);
		grpatch->wadnum = wadnum;
		grpatch->lumpnum = lumpnum;
		grpatch->mipmap = Z_Calloc(sizeof(GLMipmap_t), PU_HWRPATCHINFO, NULL);
		M_AATreeSet(hwrcache, lumpnum, grpatch);
	}

	return grpatch;
}

GLPatch_t *HWR_GetCachedGLPatch(lumpnum_t lumpnum)
{
	return HWR_GetCachedGLPatchPwad(WADFILENUM(lumpnum),LUMPNUM(lumpnum));
}

// Need to do this because they aren't powers of 2
static void HWR_DrawFadeMaskInCache(GLMipmap_t *mipmap, INT32 pblockwidth, INT32 pblockheight,
	lumpnum_t fademasklumpnum, UINT16 fmwidth, UINT16 fmheight)
{
	INT32 i,j;
	fixed_t posx, posy, stepx, stepy;
	UINT8 *block = mipmap->grInfo.data; // places the data directly into here, it already has the space allocated from HWR_ResizeBlock
	UINT8 *flat;
	UINT8 *dest, *src, texel;
	RGBA_t col;
	RGBA_t *palette = HWR_GetTexturePalette();

	// Place the flats data into flat
	W_ReadLump(fademasklumpnum, Z_Malloc(W_LumpLength(fademasklumpnum),
		PU_HWRCACHE, &flat));

	stepy = ((INT32)SHORT(fmheight)<<FRACBITS)/pblockheight;
	stepx = ((INT32)SHORT(fmwidth)<<FRACBITS)/pblockwidth;
	posy = 0;
	for (j = 0; j < pblockheight; j++)
	{
		posx = 0;
		dest = &block[j*blockwidth]; // 1bpp
		src = &flat[(posy>>FRACBITS)*SHORT(fmwidth)];
		for (i = 0; i < pblockwidth;i++)
		{
			// fademask bpp is always 1, and is used just for alpha
			texel = src[(posx)>>FRACBITS];
			col = palette[texel];
			*dest = col.s.red; // take the red level of the colour and use it for alpha, as fademasks do

			dest++;
			posx += stepx;
		}
		posy += stepy;
	}

	Z_Free(flat);
}

static void HWR_CacheFadeMask(GLMipmap_t *grMipmap, lumpnum_t fademasklumpnum)
{
	size_t size;
	UINT16 fmheight = 0, fmwidth = 0;

	// setup the texture info
	grMipmap->grInfo.format = GR_TEXFMT_ALPHA_8; // put the correct alpha levels straight in so I don't need to convert it later
	grMipmap->flags = 0;

	size = W_LumpLength(fademasklumpnum);

	switch (size)
	{
		// None of these are powers of 2, so I'll need to do what is done for textures and make them powers of 2 before they can be used
		case 256000: // 640x400
			fmwidth = 640;
			fmheight = 400;
			break;
		case 64000: // 320x200
			fmwidth = 320;
			fmheight = 200;
			break;
		case 16000: // 160x100
			fmwidth = 160;
			fmheight = 100;
			break;
		case 4000: // 80x50 (minimum)
			fmwidth = 80;
			fmheight = 50;
			break;
		default: // Bad lump
			CONS_Alert(CONS_WARNING, "Fade mask lump of incorrect size, ignored\n"); // I should avoid this by checking the lumpnum in HWR_RunWipe
			break;
	}

	// Thankfully, this will still work for this scenario
	HWR_ResizeBlock(fmwidth, fmheight, &grMipmap->grInfo);

	grMipmap->width  = blockwidth;
	grMipmap->height = blockheight;

	MakeBlock(grMipmap);

	HWR_DrawFadeMaskInCache(grMipmap, blockwidth, blockheight, fademasklumpnum, fmwidth, fmheight);

	// I DO need to convert this because it isn't power of 2 and we need the alpha
}


void HWR_GetFadeMask(lumpnum_t fademasklumpnum)
{
	GLMipmap_t *grmip;

	grmip = HWR_GetCachedGLPatch(fademasklumpnum)->mipmap;

	if (!grmip->downloaded && !grmip->grInfo.data)
		HWR_CacheFadeMask(grmip, fademasklumpnum);

	HWD.pfnSetTexture(grmip);

	// The system-memory data can be purged now.
	Z_ChangeTag(grmip->grInfo.data, PU_HWRCACHE_UNLOCKED);
}

// =================================================
//             PALETTE HANDLING
// =================================================

void HWR_SetPalette(RGBA_t *palette)
{
	if (HWR_ShouldUsePaletteRendering())
	{
		// set the palette for palette postprocessing
		if (cv_grpalettedepth.value == 16)
		{
			// crush to 16-bit rgb565, like software currently does in the standard configuration
			// Note: Software's screenshots have the 24-bit palette, but the screen gets
			// the 16-bit version! For making comparison screenshots either use an external screenshot
			// tool or set the palette depth to 24 bits.
			RGBA_t crushed_palette[256];
			int i;
			for (i = 0; i < 256; i++)
			{
				float fred = (float)(palette[i].s.red >> 3);
				float fgreen = (float)(palette[i].s.green >> 2);
				float fblue = (float)(palette[i].s.blue >> 3);
				crushed_palette[i].s.red = (UINT8)(fred / 31.0f * 255.0f);
				crushed_palette[i].s.green = (UINT8)(fgreen / 63.0f * 255.0f);
				crushed_palette[i].s.blue = (UINT8)(fblue / 31.0f * 255.0f);
				crushed_palette[i].s.alpha = 255;
			}
			HWD.pfnSetScreenPalette(crushed_palette);
		}
		else
		{
			HWD.pfnSetScreenPalette(palette);
		}

		// this part is responsible for keeping track of the palette OUTSIDE of a level.
		if ((!(gamestate == GS_LEVEL)) || (gamestate == GS_TITLESCREEN))
			HWR_SetMapPalette();
	}
	else
	{
		// set the palette for the textures
		HWD.pfnSetTexturePalette(palette);
		// reset mapPalette so next call to HWR_SetMapPalette will update everything correctly
		memset(mapPalette, 0, sizeof(mapPalette));
		// hardware driver will flush there own cache if cache is non paletized
		// now flush data texture cache so 32 bit texture are recomputed

		//if (patchformat == GL_TEXFMT_RGBA || textureformat == GL_TEXFMT_RGBA)
		if (patchformat == GR_RGBA || textureformat == GR_RGBA)
		{
			Z_FreeTag(PU_HWRCACHE);
			Z_FreeTag(PU_HWRCACHE_UNLOCKED);
		}
	}
}

static void HWR_SetPaletteLookup(RGBA_t *palette)
{
	int r, g, b;
	UINT8 *lut = Z_Malloc(
		HWR_PALETTE_LUT_SIZE*HWR_PALETTE_LUT_SIZE*HWR_PALETTE_LUT_SIZE*sizeof(UINT8),
		PU_STATIC, NULL);
#define STEP_SIZE (256/HWR_PALETTE_LUT_SIZE)
	for (b = 0; b < HWR_PALETTE_LUT_SIZE; b++)
	{
		for (g = 0; g < HWR_PALETTE_LUT_SIZE; g++)
		{
			for (r = 0; r < HWR_PALETTE_LUT_SIZE; r++)
			{
				lut[b*HWR_PALETTE_LUT_SIZE*HWR_PALETTE_LUT_SIZE+g*HWR_PALETTE_LUT_SIZE+r] =
					NearestPaletteColor(r*STEP_SIZE, g*STEP_SIZE, b*STEP_SIZE, palette);
			}
		}
	}
#undef STEP_SIZE
	HWD.pfnSetPaletteLookup(lut);
	Z_Free(lut);
}

// Updates mapPalette to reflect the loaded level or other game state.
// Textures are flushed if needed.
// Call this function only in palette rendering mode.
void HWR_SetMapPalette(void)
{
	RGBA_t RGBA_converted[256];
	RGBA_t *palette;
	int i;

	if ((!(gamestate == GS_LEVEL)) || (gamestate == GS_TITLESCREEN))
	{
		// outside of a level, pMasterPalette should have PLAYPAL ready for us
		//palette = pMasterPalette; // we dont have this in kart so just use pLocalPalette as before

		palette = pLocalPalette;
	}
	else
	{
		// in a level pLocalPalette might have a flash palette, but we
		// want the map's original palette.
		lumpnum_t lumpnum = W_GetNumForName(GetPalette());
		size_t palsize = W_LumpLength(lumpnum);
		UINT8 *RGB_data;
		if (palsize < 768) // 256 * 3
			I_Error("HWR_SetMapPalette: A programmer assumed palette lumps are at least 768 bytes long, but apparently this was a wrong assumption!\n");
		RGB_data = W_CacheLumpNum(lumpnum, PU_CACHE);
		// we got the RGB palette now, but we need it in RGBA format.
		for (i = 0; i < 256; i++)
		{
			RGBA_converted[i].s.red = *(RGB_data++);
			RGBA_converted[i].s.green = *(RGB_data++);
			RGBA_converted[i].s.blue = *(RGB_data++);
			RGBA_converted[i].s.alpha = 255;
		}
		palette = RGBA_converted;
	}

	// check if the palette has changed from the previous one
	if (memcmp(mapPalette, palette, sizeof(mapPalette)))
	{
		memcpy(mapPalette, palette, sizeof(mapPalette));
		// in palette rendering mode, this means that all rgba textures now have wrong colors
		// and the lookup table is outdated
		HWR_SetPaletteLookup(mapPalette);
		HWD.pfnSetTexturePalette(mapPalette);

		//if (patchformat == GL_TEXFMT_RGBA || textureformat == GL_TEXFMT_RGBA)
		if (patchformat == GR_RGBA || textureformat == GR_RGBA)
		{
			Z_FreeTag(PU_HWRCACHE);
			Z_FreeTag(PU_HWRCACHE_UNLOCKED);
		}
	}
}

// Creates a hardware lighttable from the supplied lighttable.
// Returns the id of the hw lighttable, usable in FSurfaceInfo.
UINT32 HWR_CreateLightTable(UINT8 *lighttable)
{
	UINT32 i, id;
	RGBA_t *palette = HWR_GetTexturePalette();
	RGBA_t *hw_lighttable = Z_Malloc(256 * 32 * sizeof(RGBA_t), PU_STATIC, NULL);

	// To make the palette index -> RGBA mapping easier for the shader,
	// the hardware lighttable is composed of RGBA colors instead of palette indices.
	for (i = 0; i < 256 * 32; i++)
		hw_lighttable[i] = palette[lighttable[i]];

	id = HWD.pfnCreateLightTable(hw_lighttable);
	Z_Free(hw_lighttable);
	return id;
}


// get hwr lighttable id for colormap, create it if it doesn't already exist
UINT32 HWR_GetLightTableID(extracolormap_t *colormap)
{
	boolean default_colormap = false;
	if (!colormap)
	{
		colormap = &extra_colormaps[num_extra_colormaps]; // a place to store the hw lighttable id
		// alternatively could just store the id in a global variable if there are issues
		default_colormap = true;
	}

	// create hw lighttable if there isn't one
	if (!colormap->gl_lighttable_id)
	{
		UINT8 *colormap_pointer;

		if (default_colormap)
			colormap_pointer = colormaps; // don't actually use the data from the "default colormap"
		else
			colormap_pointer = colormap->colormap;
		colormap->gl_lighttable_id = HWR_CreateLightTable(colormap_pointer);
	}

	return colormap->gl_lighttable_id;
}

// Note: all hardware lighttable ids assigned before this
// call become invalid and must not be used.
void HWR_ClearLightTables(void)
{
	//if (vid.glstate == VID_GL_LIBRARY_LOADED) // we have pfnInit which is essentially the same, but it doesent work here for whatever reason, since this should not be done in software renderer ever!
	if (rendermode == render_opengl)
		HWD.pfnClearLightTables();
}

#endif //HWRENDER
