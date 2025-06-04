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
void R_DrawColumn(void)
{
	INT32 count;
	UINT8 *restrict dest;
	intptr_t frac;
	intptr_t fracstep;

	count = dc_yh - dc_yl;

	if (count < 0) // Zero length, column does not exceed a pixel.
	{
		return;
	}


	if ((unsigned)dc_x >= (unsigned)vid.width || dc_yl < 0 || dc_yh >= vid.height)
	{
		return;
	}

	// Framebuffer destination address.
	dest = R_Address(dc_x, dc_yl);

	count++;

	// Determine scaling, which is the only mapping to be done.
	fracstep = dc_iscale;
	frac = (dc_texturemid + FixedMul((dc_yl << FRACBITS) - centeryfrac, fracstep))*(!dc_hires);

	// Inner loop that does the actual texture mapping, e.g. a DDA-like scaling.
	// This is as fast as it gets.
	const UINT8 *restrict source = dc_source;
	const lighttable_t *restrict colormap = dc_colormap;

	intptr_t heightmask = dc_sourcelength-1;

	static const INT32 npow2min = -1;
	const INT32 npow2max = dc_sourcelength;

	if (dc_sourcelength & heightmask)   // not a power of 2 -- killough
	{
		heightmask = dc_texheight << FRACBITS;

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

			dest += vid.width;


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

			dest += vid.width;
			frac += fracstep;

			*dest = colormap[source[(frac>>FRACBITS) & heightmask]];

			dest += vid.width;
			frac += fracstep;
		}

		if (count & 1)
		{
			*dest = colormap[source[(frac>>FRACBITS) & heightmask]];
		}
	}
}


void R_Draw2sMultiPatchColumn(void)
{
	INT32 count;
	UINT8 *restrict dest;
	intptr_t frac;
	intptr_t fracstep;

	count = dc_yh - dc_yl;

	if (count < 0) // Zero length, column does not exceed a pixel.
	{
		return;
	}

	if ((unsigned)dc_x >= (unsigned)vid.width || dc_yl < 0 || dc_yh >= vid.height)
	{
		return;
	}

	// Framebuffer destination address.
	dest = R_Address(dc_x, dc_yl);

	count++;

	// Determine scaling, which is the only mapping to be done.
	fracstep = dc_iscale;
	frac = (dc_texturemid + FixedMul((dc_yl << FRACBITS) - centeryfrac, fracstep))*(!dc_hires);

	// Inner loop that does the actual texture mapping, e.g. a DDA-like scaling.
	// This is as fast as it gets.
	const UINT8 *restrict source = dc_source;
	const lighttable_t *restrict colormap = dc_colormap;
	intptr_t heightmask = dc_sourcelength-1;
	UINT8 val;

	static const INT32 npow2min = -1;
	const INT32 npow2max = dc_sourcelength;

	if (dc_sourcelength & heightmask)   // not a power of 2 -- killough
	{
		heightmask = dc_texheight << FRACBITS;

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

			dest += vid.width;

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

			dest += vid.width;
			frac += fracstep;

			val = source[(frac>>FRACBITS) & heightmask];
			if (val != TRANSPARENTPIXEL)
			{
				*dest = colormap[val];
			}

			dest += vid.width;
			frac += fracstep;
		}

		if (count & 1)
		{
			val = source[(frac>>FRACBITS) & heightmask];
			if (val != TRANSPARENTPIXEL)
			{
				*dest = colormap[val];
			}
		}
	}
}

void R_Draw2sMultiPatchTranslucentColumn(void)
{
	INT32 count;
	UINT8 *restrict dest;
	intptr_t frac;
	intptr_t fracstep;

	count = dc_yh - dc_yl;

	if (count < 0) // Zero length, column does not exceed a pixel.
	{
		return;
	}

	if ((unsigned)dc_x >= (unsigned)vid.width || dc_yl < 0 || dc_yh >= vid.height)
	{
		return;
	}

	// Framebuffer destination address.
	dest = R_Address(dc_x, dc_yl);

	count++;

	// Determine scaling, which is the only mapping to be done.
	fracstep = dc_iscale;
	frac = (dc_texturemid + FixedMul((dc_yl << FRACBITS) - centeryfrac, fracstep))*(!dc_hires);

	// Inner loop that does the actual texture mapping, e.g. a DDA-like scaling.
	// This is as fast as it gets.
	const UINT8 *restrict source = dc_source;
	const UINT8 *transmap = dc_transmap;
	const lighttable_t *restrict colormap = dc_colormap;
	intptr_t heightmask = dc_sourcelength-1;
	register UINT8 val;

	static const INT32 npow2min = -1;
	const INT32 npow2max = dc_sourcelength;

	if (dc_sourcelength & heightmask)   // not a power of 2 -- killough
	{
		heightmask = dc_texheight << FRACBITS;

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

			dest += vid.width;

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

			dest += vid.width;
			frac += fracstep;

			val = source[(frac>>FRACBITS) & heightmask];
			if (val != TRANSPARENTPIXEL)
			{
				*dest = *(transmap + (colormap[val]<<8) + (*dest));
			}

			dest += vid.width;
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

/**	\brief The R_DrawShadeColumn function
	Experiment to make software go faster. Taken from the Boom source
*/
void R_DrawShadeColumn(void)
{
	register INT32 count;
	register UINT8 *dest;
	register fixed_t frac, fracstep;

	// check out coords for src*
	if ((dc_yl < 0) || (dc_x >= vid.width))
		return;

	count = dc_yh - dc_yl;

	if (count < 0) // Zero length, column does not exceed a pixel.
	{
		return;
	}

	if ((unsigned)dc_x >= (unsigned)vid.width || dc_yl < 0 || dc_yh >= vid.height)
	{
		return;
	}

	// Framebuffer destination address.
	dest = R_Address(dc_x, dc_yl);

	// Looks familiar.
	fracstep = dc_iscale;
	frac = (dc_texturemid + FixedMul((dc_yl << FRACBITS) - centeryfrac, fracstep))*(!dc_hires);

	// Here we do an additional index re-mapping.
	do
	{
		*dest = colormaps[(dc_source[frac>>FRACBITS] <<8) + (*dest)];
		dest += vid.width;
		frac += fracstep;
	} while (count--);
}

/**	\brief The R_DrawTranslucentColumn function
	I've made an asm routine for the transparency, because it slows down
	a lot in 640x480 with big sprites (bfg on all screen, or transparent
	walls on fullscreen)
*/
void R_DrawTranslucentColumn(void)
{
	register INT32 count;
	UINT8 *restrict dest;
	intptr_t frac;
	intptr_t fracstep;

	count = dc_yh - dc_yl + 1;

	if (count <= 0) // Zero length, column does not exceed a pixel.
	{
		return;
	}

	if ((unsigned)dc_x >= (unsigned)vid.width || dc_yl < 0 || dc_yh >= vid.height)
	{
		return;
	}

	// Framebuffer destination address.
	dest = R_Address(dc_x, dc_yl);

	// Looks familiar.
	fracstep = dc_iscale;
	frac = (dc_texturemid + FixedMul((dc_yl << FRACBITS) - centeryfrac, fracstep))*(!dc_hires);

	// Inner loop that does the actual texture mapping, e.g. a DDA-like scaling.
	// This is as fast as it gets.
	const UINT8 *restrict source = dc_source;
	const UINT8 *transmap = dc_transmap;
	const lighttable_t *restrict colormap = dc_colormap;
	intptr_t heightmask = dc_sourcelength-1;

	static const INT32 npow2min = -1;
	const INT32 npow2max = dc_sourcelength;

	if (dc_sourcelength & heightmask)
	{
		heightmask = dc_texheight << FRACBITS;

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

			dest += vid.width;

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
			dest += vid.width;
			frac += fracstep;

			*dest = *(transmap + (colormap[source[(frac>>FRACBITS)&heightmask]]<<8) + (*dest));
			dest += vid.width;
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
void R_DrawTranslatedTranslucentColumn(void)
{
	register INT32 count;
	UINT8 *restrict dest;
	intptr_t frac;
	intptr_t fracstep;

	count = dc_yh - dc_yl + 1;

	if (count <= 0) // Zero length, column does not exceed a pixel.
	{
		return;
	}

	if ((unsigned)dc_x >= (unsigned)vid.width || dc_yl < 0 || dc_yh >= vid.height)
	{
		return;
	}

	// Framebuffer destination address.
	dest = R_Address(dc_x, dc_yl);

	// Looks familiar.
	fracstep = dc_iscale;
	frac = (dc_texturemid + FixedMul((dc_yl << FRACBITS) - centeryfrac, fracstep))*(!dc_hires);

	// Inner loop that does the actual texture mapping, e.g. a DDA-like scaling.
	// This is as fast as it gets.
	intptr_t heightmask = dc_sourcelength-1;

	static const INT32 npow2min = -1;
	const INT32 npow2max = dc_sourcelength;

	if (dc_sourcelength & heightmask)
	{
		heightmask = dc_texheight << FRACBITS;

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
			*dest = *(dc_transmap + (dc_colormap[dc_translation[dc_source[CLAMP(frac >> FRACBITS, npow2min, npow2max)]]]<<8) + (*dest));

			dest += vid.width;

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
			*dest = *(dc_transmap + (dc_colormap[dc_translation[dc_source[(frac>>FRACBITS)&heightmask]]]<<8) + (*dest));
			dest += vid.width;
			frac += fracstep;

			*dest = *(dc_transmap + (dc_colormap[dc_translation[dc_source[(frac>>FRACBITS)&heightmask]]]<<8) + (*dest));
			dest += vid.width;
			frac += fracstep;
		}

		if (count & 1)
		{
			*dest = *(dc_transmap + (dc_colormap[dc_translation[dc_source[(frac>>FRACBITS)&heightmask]]]<<8) + (*dest));
		}
	}
}

/**	\brief The R_DrawTranslatedColumn function
	Draw columns up to 128 high but remap the green ramp to other colors

  \warning STILL NOT IN ASM, TO DO..
*/
void R_DrawTranslatedColumn(void)
{
	register INT32 count;
	register UINT8 *dest;
	register fixed_t frac, fracstep;

	count = dc_yh - dc_yl;

	if (count < 0)
	{
		return;
	}

	if ((unsigned)dc_x >= (unsigned)vid.width || dc_yl < 0 || dc_yh >= vid.height)
	{
		return;
	}

	// Framebuffer destination address.
	dest = R_Address(dc_x, dc_yl);

	// Looks familiar.
	fracstep = dc_iscale;
	frac = (dc_texturemid + FixedMul((dc_yl << FRACBITS) - centeryfrac, fracstep))*(!dc_hires);

	// Here we do an additional index re-mapping.
	do
	{
		// Translation tables are used
		//  to map certain colorramps to other ones,
		//  used with PLAY sprites.
		// Thus the "green" ramp of the player 0 sprite
		//  is mapped to gray, red, black/indigo.
		*dest = dc_colormap[dc_translation[dc_source[frac>>FRACBITS]]];

		dest += vid.width;

		frac += fracstep;
	} while (count--);
}

/**	\brief The R_DrawFogColumn function
	Fog wall.
*/
void R_DrawFogColumn(void)
{
	register INT32 count;
	register UINT8 *dest;

	count = dc_yh - dc_yl;

	// Zero length, column does not exceed a pixel.
	if (count < 0)
		return;

#ifdef RANGECHECK
	if ((unsigned)dc_x >= (unsigned)vid.width || dc_yl < 0 || dc_yh >= vid.height)
		I_Error("R_DrawFogColumn: %d to %d at %d", dc_yl, dc_yh, dc_x);
#endif

	// Framebuffer destination address.
	dest = R_Address(dc_x, dc_yl);

	// Determine scaling, which is the only mapping to be done.
	do
	{
		// Simple. Apply the colormap to what's already on the screen.
		*dest = dc_colormap[*dest];
		dest += vid.width;
	} while (count--);
}

/**	\brief The R_DrawShadeColumn function
	This is for 3D floors that cast shadows on walls.

	This function just cuts the column up into sections and calls R_DrawColumn
*/
void R_DrawColumnShadowed(void)
{
	register INT32 count;
	INT32 realyh, i, height, bheight = 0, solid = 0;

	realyh = dc_yh;

	count = dc_yh - dc_yl;

	// Zero length, column does not exceed a pixel.
	if (count < 0)
		return;

#ifdef RANGECHECK
	if ((unsigned)dc_x >= (unsigned)vid.width || dc_yl < 0 || dc_yh >= vid.height)
		I_Error("R_DrawColumnShadowed: %d to %d at %d", dc_yl, dc_yh, dc_x);
#endif

	// This runs through the lightlist from top to bottom and cuts up the column accordingly.
	for (i = 0; i < dc_numlights; i++)
	{
		// If the height of the light is above the column, get the colormap
		// anyway because the lighting of the top should be affected.
		solid = dc_lightlist[i].flags & FF_CUTSOLIDS;

		height = dc_lightlist[i].height >> LIGHTSCALESHIFT;

		if (solid)
		{
			bheight = dc_lightlist[i].botheight >> LIGHTSCALESHIFT;
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

		if (height <= dc_yl)
		{
			dc_colormap = dc_lightlist[i].rcolormap;
			if (encoremap)
				dc_colormap += COLORMAP_REMAPOFFSET;
			if (solid && dc_yl < bheight)
				dc_yl = bheight;
			continue;
		}

		// Found a break in the column!
		dc_yh = height;

		if (dc_yh > realyh)
			dc_yh = realyh;
		basecolfunc();		// R_DrawColumn for the appropriate architecture
		if (solid)
			dc_yl = bheight;
		else
			dc_yl = dc_yh + 1;

		dc_colormap = dc_lightlist[i].rcolormap;
		if (encoremap)
			dc_colormap += COLORMAP_REMAPOFFSET;
	}
	dc_yh = realyh;
	if (dc_yl <= realyh)
		walldrawerfunc();		// R_DrawWallColumn for the appropriate architecture
}
