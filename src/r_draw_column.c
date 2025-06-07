// SONIC ROBO BLAST 2
//-----------------------------------------------------------------------------
// Copyright (C) 1998-2000 by DooM Legacy Team.
// Copyright (C) 1999-2018 by Sonic Team Junior.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  r_draw_column.c
/// \brief column drawer functions
/// \note  no includes because this is included as part of r_draw.c

// ==========================================================================
// COLUMNS
// ==========================================================================

// A column is a vertical slice/span of a wall texture that uses
// a has a constant z depth from top to bottom.
//

/**	\brief The R_DrawColumn function
	Experiment to make software go faster. Taken from the Boom source
*/
void R_DrawColumn(drawcolumndata_t* dc)
{
	INT32 count;
	UINT8 *restrict dest;
	intptr_t frac;
	intptr_t fracstep;

	count = dc->yh - dc->yl;

	if (count < 0) // Zero length, column does not exceed a pixel.
	{
		return;
	}

	if ((unsigned)dc->x >= (unsigned)vid.width || dc->yl < 0 || dc->yh >= vid.height)
	{
		return;
	}

	// Framebuffer destination address.
	dest = R_Address(dc->x, dc->yl);

	count++;

	// Determine scaling, which is the only mapping to be done.
	fracstep = dc->iscale;
	frac = (dc->texturemid + FixedMul((dc->yl << FRACBITS) - centeryfrac, fracstep));

	// Inner loop that does the actual texture mapping, e.g. a DDA-like scaling.
	// This is as fast as it gets.
	const UINT8 *restrict source = dc->source;
	const lighttable_t *restrict colormap = dc->colormap;
	intptr_t heightmask = dc->sourcelength-1;

	static const INT32 npow2min = -1;
	const INT32 npow2max = dc->sourcelength;

	register const INT32 stride = vid.width;

	if (heightmask == -1)
	{
		// texture has no height, so just go
		while (--count > 0)
		{
			*dest = colormap[source[frac>>FRACBITS]];
			dest += stride;
			frac += fracstep;
		}
	}
	else if (dc->sourcelength & heightmask)   // not a power of 2 -- killough
	{
		heightmask = dc->texheight << FRACBITS;

		if (frac < 0)
		{
			while ((frac += heightmask) < 0)
			{
				;
			}
		}
		else
		{
			while (frac >= heightmask)
			{
				frac -= heightmask;
			}
		}

		do
		{
			// Re-map color indices from wall texture column
			//  using a lighting/special effects LUT.
			// heightmask is the Tutti-Frutti fix

			// -1 is the lower clamp bound because column posts have a "safe" byte before the real data
			// and a few bytes after as well
			*dest = colormap[source[CLAMP(frac >> FRACBITS, npow2min, npow2max)]];

			dest += stride;

#if __SIZEOF_POINTER__ < 8 // 64-bit systems have large enough numbers for this to be a non-issue
			// Avoid overflow.
			if (fracstep > 0x7FFFFFFF - frac)
			{
				frac += fracstep - heightmask;
			}
			else
#endif
			{
				frac += fracstep;
			}

			while (frac >= heightmask)
			{
				frac -= heightmask;
			}
		} while (--count);
	}
	else
	{
		while ((count -= 2) >= 0) // texture height is a power of 2
		{
			*dest = colormap[source[(frac>>FRACBITS) & heightmask]];

			dest += stride;
			frac += fracstep;

			*dest = colormap[source[(frac>>FRACBITS) & heightmask]];

			dest += stride;
			frac += fracstep;
		}

		if (count & 1)
		{
			*dest = colormap[source[(frac>>FRACBITS) & heightmask]];
		}
	}
}

void R_Draw2sMultiPatchColumn(drawcolumndata_t* dc)
{
	INT32 count;
	UINT8 *restrict dest;
	intptr_t frac;
	intptr_t fracstep;

	count = dc->yh - dc->yl;

	if (count < 0) // Zero length, column does not exceed a pixel.
	{
		return;
	}

	if ((unsigned)dc->x >= (unsigned)vid.width || dc->yl < 0 || dc->yh >= vid.height)
	{
		return;
	}

	// Framebuffer destination address.
	dest = R_Address(dc->x, dc->yl);

	count++;

	// Determine scaling, which is the only mapping to be done.
	fracstep = dc->iscale;
	frac = (dc->texturemid + FixedMul((dc->yl << FRACBITS) - centeryfrac, fracstep));

	// Inner loop that does the actual texture mapping, e.g. a DDA-like scaling.
	// This is as fast as it gets.
	const UINT8 *restrict source = dc->source;
	const lighttable_t *restrict colormap = dc->colormap;
	intptr_t heightmask = dc->sourcelength-1;
	UINT8 val;

	static const INT32 npow2min = -1;
	const INT32 npow2max = dc->sourcelength;

	register const INT32 stride = vid.width;

	if (heightmask == -1)
	{
		// texture has no height, so just go
		while (--count > 0)
		{
			*dest = colormap[source[frac>>FRACBITS]];
			dest += stride;
			frac += fracstep;
		}
	}
	else if (dc->sourcelength & heightmask)   // not a power of 2 -- killough
	{
		heightmask = dc->texheight << FRACBITS;

		if (frac < 0)
		{
			while ((frac += heightmask) < 0)
			{
				;
			}
		}
		else
		{
			while (frac >= heightmask)
			{
				frac -= heightmask;
			}
		}

		do
		{
			// Re-map color indices from wall texture column
			//  using a lighting/special effects LUT.
			// heightmask is the Tutti-Frutti fix

			// -1 is the lower clamp bound because column posts have a "safe" byte before the real data
			// and a few bytes after as well
			val = source[CLAMP(frac >> FRACBITS, npow2min, npow2max)];

			if (val != TRANSPARENTPIXEL)
			{
				*dest = colormap[val];
			}

			dest += stride;

			// Avoid overflow.
#if __SIZEOF_POINTER__ < 8
			if (fracstep > 0x7FFFFFFF - frac)
			{
				frac += fracstep - heightmask;
			}
			else
#endif
			{
				frac += fracstep;
			}

			while (frac >= heightmask)
			{
				frac -= heightmask;
			}
		} while (--count);
	}
	else
	{
		while ((count -= 2) >= 0) // texture height is a power of 2
		{
			val = source[(frac>>FRACBITS) & heightmask];

			if (val != TRANSPARENTPIXEL)
			{
				*dest = colormap[val];
			}

			dest += stride;
			frac += fracstep;

			val = source[(frac>>FRACBITS) & heightmask];
			if (val != TRANSPARENTPIXEL)
			{
				*dest = colormap[val];
			}

			dest += stride;
			frac += fracstep;
		}

		if (count & 1)
		{
			val = source[CLAMP(frac >> FRACBITS, -1, dc->sourcelength)];
			if (val != TRANSPARENTPIXEL)
			{
				*dest = colormap[val];
			}
		}
	}
}

void R_Draw2sMultiPatchTranslucentColumn(drawcolumndata_t* dc)
{
	INT32 count;
	UINT8 *restrict dest;
	intptr_t frac;
	intptr_t fracstep;

	count = dc->yh - dc->yl;

	if (count < 0) // Zero length, column does not exceed a pixel.
	{
		return;
	}

	if ((unsigned)dc->x >= (unsigned)vid.width || dc->yl < 0 || dc->yh >= vid.height)
	{
		return;
	}

	// Framebuffer destination address.
	dest = R_Address(dc->x, dc->yl);

	count++;

	// Determine scaling, which is the only mapping to be done.
	fracstep = dc->iscale;
	frac = (dc->texturemid + FixedMul((dc->yl << FRACBITS) - centeryfrac, fracstep));

	// Inner loop that does the actual texture mapping, e.g. a DDA-like scaling.
	// This is as fast as it gets.
	const UINT8 *restrict source = dc->source;
	const UINT8 *transmap = dc->transmap;
	const lighttable_t *restrict colormap = dc->colormap;
	intptr_t heightmask = dc->sourcelength-1;
	register UINT8 val;

	static const INT32 npow2min = -1;
	const INT32 npow2max = dc->sourcelength;

	register const INT32 stride = vid.width;

	if (heightmask == -1)
	{
		// texture has no height, so just go
		while (--count > 0)
		{
			*dest = colormap[source[frac>>FRACBITS]];
			dest += stride;
			frac += fracstep;
		}
	}
	else if (dc->sourcelength & heightmask)   // not a power of 2 -- killough
	{
		heightmask = dc->texheight << FRACBITS;

		if (frac < 0)
		{
			while ((frac += heightmask) < 0)
			{
				;
			}
		}
		else
		{
			while (frac >= heightmask)
			{
				frac -= heightmask;
			}
		}

		do
		{
			// Re-map color indices from wall texture column
			//  using a lighting/special effects LUT.
			// heightmask is the Tutti-Frutti fix

			// -1 is the lower clamp bound because column posts have a "safe" byte before the real data
			// and a few bytes after as well
			val = source[CLAMP(frac >> FRACBITS, npow2min, npow2max)];

			if (val != TRANSPARENTPIXEL)
			{
				*dest = *(transmap + (colormap[val]<<8) + (*dest));
			}

			dest += stride;

			// Avoid overflow.
#if __SIZEOF_POINTER__ < 8
			if (fracstep > 0x7FFFFFFF - frac)
			{
				frac += fracstep - heightmask;
			}
			else
#endif
			{
				frac += fracstep;
			}

			while (frac >= heightmask)
			{
				frac -= heightmask;
			}
		} while (--count);
	}
	else
	{
		while ((count -= 2) >= 0) // texture height is a power of 2
		{
			val = source[(frac>>FRACBITS) & heightmask];
			if (val != TRANSPARENTPIXEL)
			{
				*dest = *(transmap + (colormap[val]<<8) + (*dest));
			}

			dest += stride;
			frac += fracstep;

			val = source[(frac>>FRACBITS) & heightmask];
			if (val != TRANSPARENTPIXEL)
			{
				*dest = *(transmap + (colormap[val]<<8) + (*dest));
			}

			dest += stride;
			frac += fracstep;
		}

		if (count & 1)
		{
			val = source[(frac>>FRACBITS) & heightmask];
			if (val != TRANSPARENTPIXEL)
			{
				*dest = *(transmap + (colormap[val]<<8) + (*dest));
			}
		}
	}
}

/**	\brief The R_DrawTranslucentColumn function
	I've made an asm routine for the transparency, because it slows down
	a lot in 640x480 with big sprites (bfg on all screen, or transparent
	walls on fullscreen)
*/
void R_DrawTranslucentColumn(drawcolumndata_t* dc)
{
	register INT32 count;
	UINT8 *restrict dest;
	intptr_t frac;
	intptr_t fracstep;

	count = dc->yh - dc->yl + 1;

	if (count <= 0) // Zero length, column does not exceed a pixel.
	{
		return;
	}

	if ((unsigned)dc->x >= (unsigned)vid.width || dc->yl < 0 || dc->yh >= vid.height)
	{
		return;
	}

	// Framebuffer destination address.
	dest = R_Address(dc->x, dc->yl);

	// Looks familiar.
	fracstep = dc->iscale;
	frac = (dc->texturemid + FixedMul((dc->yl << FRACBITS) - centeryfrac, fracstep));

	// Inner loop that does the actual texture mapping, e.g. a DDA-like scaling.
	// This is as fast as it gets.
	const UINT8 *restrict source = dc->source;
	const UINT8 *transmap = dc->transmap;
	const lighttable_t *restrict colormap = dc->colormap;
	intptr_t heightmask = dc->sourcelength-1;

	static const INT32 npow2min = -1;
	const INT32 npow2max = dc->sourcelength;

	register const INT32 stride = vid.width;

	if (dc->sourcelength & heightmask)
	{
		heightmask = dc->texheight << FRACBITS;

		if (frac < 0)
		{
			while ((frac += heightmask) < 0)
			{
				;
			}
		}
		else
		{
			while (frac >= heightmask)
			{
				frac -= heightmask;
			}
		}

		do
		{
			// Re-map color indices from wall texture column
			// using a lighting/special effects LUT.
			// heightmask is the Tutti-Frutti fix

			// -1 is the lower clamp bound because column posts have a "safe" byte before the real data
			// and a few bytes after as well
			*dest = *(transmap + (colormap[source[CLAMP(frac >> FRACBITS, npow2min, npow2max)]]<<8) + (*dest));

			dest += stride;

			if ((frac += fracstep) >= heightmask)
			{
				frac -= heightmask;
			}
		}
		while (--count);
	}
	else
	{
		while ((count -= 2) >= 0) // texture height is a power of 2
		{
			*dest = *(transmap + (colormap[source[(frac>>FRACBITS)&heightmask]]<<8) + (*dest));
			dest += stride;
			frac += fracstep;

			*dest = *(transmap + (colormap[source[(frac>>FRACBITS)&heightmask]]<<8) + (*dest));
			dest += stride;
			frac += fracstep;
		}

		if (count & 1)
		{
			*dest = *(transmap + (colormap[source[(frac>>FRACBITS)&heightmask]]<<8) + (*dest));
		}
	}
}

/**	\brief The R_DrawTranslatedTranslucentColumn function
	Spiffy function. Not only does it colormap a sprite, but does translucency as well.
	Uber-kudos to Cyan Helkaraxe
*/
void R_DrawTranslatedTranslucentColumn(drawcolumndata_t* dc)
{
	register INT32 count;
	UINT8 *restrict dest;
	intptr_t frac;
	intptr_t fracstep;

	count = dc->yh - dc->yl + 1;

	if (count <= 0) // Zero length, column does not exceed a pixel.
	{
		return;
	}

	if ((unsigned)dc->x >= (unsigned)vid.width || dc->yl < 0 || dc->yh >= vid.height)
	{
		return;
	}

	// Framebuffer destination address.
	dest = R_Address(dc->x, dc->yl);

	// Looks familiar.
	fracstep = dc->iscale;
	frac = (dc->texturemid + FixedMul((dc->yl << FRACBITS) - centeryfrac, fracstep));

	// Inner loop that does the actual texture mapping, e.g. a DDA-like scaling.
	// This is as fast as it gets.
	intptr_t heightmask = dc->sourcelength-1;

	static const INT32 npow2min = -1;
	const INT32 npow2max = dc->sourcelength;

	register const INT32 stride = vid.width;

	if (dc->sourcelength & heightmask)
	{
		heightmask = dc->texheight << FRACBITS;

		if (frac < 0)
		{
			while ((frac += heightmask) < 0)
			{
				;
			}
		}
		else
		{
			while (frac >= heightmask)
			{
				frac -= heightmask;
			}
		}

		do
		{
			// Re-map color indices from wall texture column
			//  using a lighting/special effects LUT.
			// heightmask is the Tutti-Frutti fix

			// -1 is the lower clamp bound because column posts have a "safe" byte before the real data
			// and a few bytes after as well
			*dest = *(dc->transmap + (dc->colormap[dc->translation[dc->source[CLAMP(frac >> FRACBITS, npow2min, npow2max)]]]<<8) + (*dest));

			dest += stride;

			if ((frac += fracstep) >= heightmask)
			{
				frac -= heightmask;
			}
		}
		while (--count);
	}
	else
	{
		while ((count -= 2) >= 0) // texture height is a power of 2
		{
			*dest = *(dc->transmap + (dc->colormap[dc->translation[dc->source[(frac>>FRACBITS)&heightmask]]]<<8) + (*dest));
			dest += stride;
			frac += fracstep;

			*dest = *(dc->transmap + (dc->colormap[dc->translation[dc->source[(frac>>FRACBITS)&heightmask]]]<<8) + (*dest));
			dest += stride;
			frac += fracstep;
		}

		if (count & 1)
		{
			*dest = *(dc->transmap + (dc->colormap[dc->translation[dc->source[(frac>>FRACBITS)&heightmask]]]<<8) + (*dest));
		}
	}
}

/**	\brief The R_DrawTranslatedColumn function
	Draw columns up to 128 high but remap the green ramp to other colors

  \warning STILL NOT IN ASM, TO DO..
*/
void R_DrawTranslatedColumn(drawcolumndata_t* dc)
{
	register INT32 count;
	register UINT8 *dest;
	register fixed_t frac, fracstep;

	count = dc->yh - dc->yl;

	if (count < 0)
	{
		return;
	}

	if ((unsigned)dc->x >= (unsigned)vid.width || dc->yl < 0 || dc->yh >= vid.height)
	{
		return;
	}

	// Framebuffer destination address.
	dest = R_Address(dc->x, dc->yl);

	// Looks familiar.
	fracstep = dc->iscale;
	frac = (dc->texturemid + FixedMul((dc->yl << FRACBITS) - centeryfrac, fracstep));

	register const INT32 stride = vid.width;

	// Here we do an additional index re-mapping.
	do
	{
		// Translation tables are used
		//  to map certain colorramps to other ones,
		//  used with PLAY sprites.
		// Thus the "green" ramp of the player 0 sprite
		//  is mapped to gray, red, black/indigo.
		*dest = dc->colormap[dc->translation[dc->source[frac>>FRACBITS]]];

		dest += stride;

		frac += fracstep;
	} while (count--);
}

/**	\brief The R_DrawFogColumn function
	Fog wall.
*/
void R_DrawFogColumn(drawcolumndata_t* dc)
{
	register INT32 count;
	register UINT8 *dest;

	count = dc->yh - dc->yl;

	// Zero length, column does not exceed a pixel.
	if (count < 0)
	{
		return;
	}

	if ((unsigned)dc->x >= (unsigned)vid.width || dc->yl < 0 || dc->yh >= vid.height)
	{
		return;
	}

	// Framebuffer destination address.
	dest = R_Address(dc->x, dc->yl);

	register const INT32 stride = vid.width;

	// Determine scaling, which is the only mapping to be done.
	do
	{
		// Simple. Apply the colormap to what's already on the screen.
		*dest = dc->colormap[*dest];
		dest += stride;
	} while (count--);
}

/**	\brief The R_DrawShadeColumn function
	This is for 3D floors that cast shadows on walls.

	This function just cuts the column up into sections and calls R_DrawColumn
*/
void R_DrawColumnShadowed(drawcolumndata_t* dc)
{
	register INT32 count;
	INT32 realyh, i, height, bheight = 0, solid = 0;

	count = dc->yh - dc->yl;

	// Zero length, column does not exceed a pixel.
	if (count < 0)
	{
		return;
	}

	if ((unsigned)dc->x >= (unsigned)vid.width || dc->yl < 0 || dc->yh >= vid.height)
	{
		return;
	}

	realyh = dc->yh;

	// This runs through the lightlist from top to bottom and cuts up the column accordingly.
	for (i = 0; i < dc->numlights; i++)
	{
		// If the height of the light is above the column, get the colormap
		// anyway because the lighting of the top should be affected.
		solid = dc->lightlist[i].flags & FF_CUTSOLIDS;

		height = dc->lightlist[i].height >> LIGHTSCALESHIFT;

		if (solid)
		{
			bheight = dc->lightlist[i].botheight >> LIGHTSCALESHIFT;
			if (bheight < height)
			{
				// confounded slopes sometimes allow partial invertedness,
				// even including cases where the top and bottom heights
				// should actually be the same!
				// swap the height values as a workaround for this quirk
				INT32 temp = height;
				height = bheight;
				bheight = temp;
			}
		}

		if (height <= dc->yl)
		{
			dc->colormap = dc->lightlist[i].rcolormap;
			if (encoremap)
				dc->colormap += COLORMAP_REMAPOFFSET;
			if (solid && dc->yl < bheight)
				dc->yl = bheight;
			continue;
		}

		// Found a break in the column!
		dc->yh = height;

		if (dc->yh > realyh)
			dc->yh = realyh;

		(colfuncs[BASEDRAWFUNC])(dc);		// R_DrawColumn_8 for the appropriate architecture

		if (solid)
			dc->yl = bheight;
		else
			dc->yl = dc->yh + 1;

		dc->colormap = dc->lightlist[i].rcolormap;
		if (encoremap)
			dc->colormap += COLORMAP_REMAPOFFSET;
	}

	dc->yh = realyh;

	if (dc->yl <= realyh)
		(colfuncs[BASEDRAWFUNC])(dc);		// R_DrawWallColumn_8 for the appropriate architecture
}
