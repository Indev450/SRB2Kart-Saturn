// SONIC ROBO BLAST 2 KART
//-----------------------------------------------------------------------------
// Copyright (C) 2025 by Kart Krew.
// Copyright (C) 2020 by Sonic Team Junior.
// Copyright (C) 2000 by DooM Legacy Team.
// Copyright (C) 1996 by id Software, Inc.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  r_draw_column.cpp
/// \brief column drawer functions
/// \note  no includes because this is included as part of r_draw.cpp

// ==========================================================================
// COLUMNS
// ==========================================================================

// A column is a vertical slice/span of a wall texture that uses
// a has a constant z depth from top to bottom.
//

#include "r_draw_flush.cpp"

enum DrawColumnType
{
	DC_BASIC			= 0x0000,
	DC_COLORMAP			= 0x0001,
	DC_TRANSMAP			= 0x0002,
	DC_HOLES			= 0x0004,
	DC_LIGHTLIST		= 0x0008,
};

template<DrawColumnType Type>
static constexpr UINT8 R_GetColumnTranslated(drawcolumndata_t* dc, UINT8 col, const UINT8 * restrict colormap)
{
	if constexpr (Type & DrawColumnType::DC_COLORMAP)
	{
		col = dc->translation[col];
	}

	return colormap[col];
}

template<DrawColumnType Type>
static constexpr UINT8 R_GetColumnTranslucent(drawcolumndata_t* dc, UINT8 col, const UINT8 * restrict colormap)
{
	col = R_GetColumnTranslated<Type>(dc, col, colormap);

	/*if constexpr (Type & DrawColumnType::DC_TRANSMAP)
	{
		return *(dc->transmap + (col << 8) + (*dest));
	}
	else*/
	{
		return col;
	}
}

template<DrawColumnType Type>
static constexpr UINT8 R_DrawColumnPixel(drawcolumndata_t* dc, UINT8 * restrict dest, UINT32 bit, const UINT8 * restrict source, const UINT8 * restrict colormap)
{
	UINT8 col = source[bit];

	if constexpr (Type & DrawColumnType::DC_HOLES)
	{
		if (col == TRANSPARENTPIXEL)
		{
			return *dest;
		}
	}

	return R_GetColumnTranslucent<Type>(dc, col, colormap);
}

/**	\brief The R_DrawColumn function
	Experiment to make software go faster. Taken from the Boom source
*/
template<DrawColumnType Type>
static void R_DrawColumnTemplate(drawcolumndata_t *dc)
{
	INT32 count;

	// leban 1/17/99:
	// removed the + 1 here, adjusted the if test, and added an increment
	// later.  this helps a compiler pipeline a bit better.  the x86
	// assembler also does this.
	count = dc->yh - dc->yl;

	// leban 1/17/99:
	// this case isn't executed too often.  depending on how many instructions
	// there are between here and the second if test below, this case could
	// be moved down and might save instructions overall.  since there are
	// probably different wads that favor one way or the other, i'll leave
	// this alone for now.
	if (count < 0) // Zero length, column does not exceed a pixel.
	{
		return;
	}

	if ((unsigned)dc->x >= (unsigned)vid.width || dc->yl < 0 || dc->yh >= vid.height)
	{
		return;
	}

	if constexpr (Type & DrawColumnType::DC_LIGHTLIST)
	{
		constexpr DrawColumnType NewType = static_cast<DrawColumnType>(Type & ~DC_LIGHTLIST);
		INT32 i, height, bheight = 0, solid = 0;
		drawcolumndata_t dc_copy = *dc;

		const INT32 realyh = dc_copy.yh;

		// This runs through the lightlist from top to bottom and cuts up the column accordingly.
		for (i = 0; i < dc_copy.numlights; i++)
		{
			// If the height of the light is above the column, get the colormap
			// anyway because the lighting of the top should be affected.
			solid = dc_copy.lightlist[i].flags & FF_CUTSOLIDS;
			height = dc_copy.lightlist[i].height >> LIGHTSCALESHIFT;

			if (solid)
			{
				bheight = dc_copy.lightlist[i].botheight >> LIGHTSCALESHIFT;

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

			if (height <= dc_copy.yl)
			{
				dc_copy.colormap = dc_copy.lightlist[i].rcolormap;

				if (encoremap)
				{
					dc_copy.colormap += COLORMAP_REMAPOFFSET;
				}

				if (solid && dc_copy.yl < bheight)
				{
					dc_copy.yl = bheight;
				}

				continue;
			}

			// Found a break in the column!
			dc_copy.yh = height;

			if (dc_copy.yh > realyh)
			{
				dc_copy.yh = realyh;
			}

			R_DrawColumnTemplate<NewType>(&dc_copy);

			if (solid)
			{
				dc_copy.yl = bheight;
			}
			else
			{
				dc_copy.yl = dc_copy.yh + 1;
			}

			dc_copy.colormap = dc_copy.lightlist[i].rcolormap;

			if (encoremap)
			{
				dc_copy.colormap += COLORMAP_REMAPOFFSET;
			}
		}

		dc_copy.yh = realyh;

		if (dc_copy.yl <= realyh)
		{
			R_DrawColumnTemplate<NewType>(&dc_copy);
		}
	}
	else
	{
		// Inner loop that does the actual texture mapping,
		//  e.g. a DDA-lile scaling.
		// This is as fast as it gets.       (Yeah, right!!! -- killough)
		//
		// killough 2/1/98: more performance tuning

		intptr_t frac;
		const intptr_t fracstep = dc->iscale;
		const intptr_t heightmask = dc->sourcelength-1; // CPhipps - specify type
		constexpr INT32 npow2min = -1;
		const INT32 npow2max = dc->sourcelength;

		const UINT8 * restrict source = dc->source;
		const lighttable_t * restrict colormap = dc->colormap;

		// Framebuffer destination address.
		// SoM: MAGIC
		UINT8 * restrict dest;

		if constexpr ((Type & (DrawColumnType::DC_COLORMAP | DrawColumnType::DC_TRANSMAP))
						   == (DrawColumnType::DC_COLORMAP | DrawColumnType::DC_TRANSMAP))
			dest = R_GetBufferColormapTrans(dc);
		else if constexpr (Type & DrawColumnType::DC_TRANSMAP)
			dest = R_GetBufferTrans(dc);
		else if constexpr (Type & DrawColumnType::DC_COLORMAP)
			dest = R_GetBufferColormap(dc);
		else
			dest = R_GetBufferOpaque(dc);

		count++;

		// Determine scaling, which is the only mapping to be done.
		frac = (dc->texturemid + FixedMul((dc->yl << FRACBITS) - centeryfrac, fracstep));

		switch (heightmask)
		{
			case 255:
			case 127:
				{
					while (count--)
					{
						*dest = R_DrawColumnPixel<Type>(dc, dest, (frac>>FRACBITS) & heightmask, source, colormap);
						dest += 4;
						frac += fracstep;
					}
				}
				break;
			case npow2min:
				{
					if (frac < 0)
						// adjust in case we underread
						frac += fracstep;

					// texture has no height, so just go
					while (--count >= 0)
					{
						*dest = R_DrawColumnPixel<Type>(dc, dest, frac>>FRACBITS, source, colormap);
						dest += 4;
						frac += fracstep;
					}
				}
				break;
			default:
				{
					if (!(dc->sourcelength & heightmask))   // power of 2 -- killough
					{
						while ((count -= 2) >= 0) // texture height is a power of 2 -- killough
						{
							*dest = R_DrawColumnPixel<Type>(dc, dest, (frac>>FRACBITS) & heightmask, source, colormap);
							dest += 4;
							frac += fracstep;

							*dest = R_DrawColumnPixel<Type>(dc, dest, (frac>>FRACBITS) & heightmask, source, colormap);
							dest += 4;
							frac += fracstep;
						}

						if (count & 1)
						{
							*dest = R_DrawColumnPixel<Type>(dc, dest, (frac>>FRACBITS) & heightmask, source, colormap);
						}
					}
					else
					{
						const intptr_t fixed_heightmask = dc->texheight << FRACBITS;

						if (frac < 0)
						{
							while ((frac += fixed_heightmask) < 0)
							{
								;
							}
						}
						else
						{
							while (frac >= fixed_heightmask)
							{
								frac -= fixed_heightmask;
							}
						}

						do
						{
							// Re-map color indices from wall texture column
							//  using a lighting/special effects LUT.
							// heightmask is the Tutti-Frutti fix -- killough

							// -1 is the lower clamp bound because column posts have a "safe" byte before the real data
							// and a few bytes after as well
							*dest = R_DrawColumnPixel<Type>(dc, dest, CLAMP((frac >> FRACBITS), npow2min, npow2max), source, colormap);

							dest += 4;

#if __SIZEOF_POINTER__ < 8  // 64-bit systems have large enough numbers for this to be a non-issue
							// Avoid overflow.
							if (fracstep > 0x7FFFFFFF - frac)
							{
								frac += fracstep - fixed_heightmask;
							}
							else
#endif
							{
								frac += fracstep;
							}

							while (frac >= fixed_heightmask)
							{
								frac -= fixed_heightmask;
							}
						}
						while (--count);
					}
				}
				break;
		}
	}
}

#define DEFINE_COLUMN_FUNC(name, flags) \
	void name(drawcolumndata_t *dc) \
	{ \
		constexpr DrawColumnType opt = static_cast<DrawColumnType>(flags); \
		R_DrawColumnTemplate<opt>(dc); \
	}

DEFINE_COLUMN_FUNC(R_DrawColumn, DC_BASIC)
DEFINE_COLUMN_FUNC(R_DrawTranslucentColumn, DC_TRANSMAP)
DEFINE_COLUMN_FUNC(R_DrawTranslatedColumn, DC_COLORMAP)
DEFINE_COLUMN_FUNC(R_DrawColumnShadowed, DC_LIGHTLIST)
DEFINE_COLUMN_FUNC(R_DrawTranslatedTranslucentColumn, DC_COLORMAP|DC_TRANSMAP)
DEFINE_COLUMN_FUNC(R_Draw2sMultiPatchColumn, DC_HOLES)
DEFINE_COLUMN_FUNC(R_Draw2sMultiPatchTranslucentColumn, DC_HOLES|DC_TRANSMAP)

/**	\brief The R_DrawFogColumn function
	Fog wall.
*/
void R_DrawFogColumn(drawcolumndata_t* dc)
{
	INT32 count;
	UINT8 * restrict dest;

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

	const INT32 stride = vid.width;
	const lighttable_t * restrict colormap = dc->colormap;

	// Determine scaling, which is the only mapping to be done.
	do
	{
		// Simple. Apply the colormap to what's already on the screen.
		*dest = colormap[*dest];
		dest += stride;
	} while (count--);
}

// blunt copy paste for now
// cant share a global buffer between threads for obvious reasons lmao
void R_DrawSkyColumn(drawcolumndata_t *dc)
{
	INT32 count;

	count = dc->yh - dc->yl;

	if (count < 0) // Zero length, column does not exceed a pixel.
	{
		return;
	}

	if ((unsigned)dc->x >= (unsigned)vid.width || dc->yl < 0 || dc->yh >= vid.height)
	{
		return;
	}

	{
		// Inner loop that does the actual texture mapping,
		//  e.g. a DDA-lile scaling.
		// This is as fast as it gets.       (Yeah, right!!! -- killough)
		//
		// killough 2/1/98: more performance tuning

		intptr_t frac;
		const intptr_t fracstep = dc->iscale;
		const intptr_t heightmask = dc->sourcelength-1;
		constexpr INT32 npow2min = -1;
		const INT32 npow2max = dc->sourcelength;
		const INT32 stride = vid.width;

		const UINT8 * restrict source = dc->source;
		const lighttable_t * restrict colormap = dc->colormap;

		// Framebuffer destination address.
		UINT8 * restrict dest = R_Address(dc->x, dc->yl);

		count++;

		// Determine scaling, which is the only mapping to be done.
		frac = (dc->texturemid + FixedMul((dc->yl << FRACBITS) - centeryfrac, fracstep));

		switch (heightmask)
		{
			case 255:
			case 127:
				{
					while(count--)
					{
						*dest = R_DrawColumnPixel<DC_BASIC>(dc, dest, (frac>>FRACBITS) & heightmask, source, colormap);
						dest += stride;
						frac += fracstep;
					}
				}
				break;
			case -1:
				{
					if (frac < 0)
						// adjust in case we underread
						frac += fracstep;

					// texture has no height, so just go
					while (--count >= 0)
					{
						*dest = R_DrawColumnPixel<DC_BASIC>(dc, dest, frac>>FRACBITS, source, colormap);
						dest += stride;
						frac += fracstep;
					}
				}
				break;
			default:
				{
					if (!(dc->sourcelength & heightmask))   // power of 2 -- killough
					{
						while ((count -= 2) >= 0) // texture height is a power of 2 -- killough
						{
							*dest = R_DrawColumnPixel<DC_BASIC>(dc, dest, (frac>>FRACBITS) & heightmask, source, colormap);
							dest += stride;
							frac += fracstep;

							*dest = R_DrawColumnPixel<DC_BASIC>(dc, dest, (frac>>FRACBITS) & heightmask, source, colormap);
							dest += stride;
							frac += fracstep;
						}

						if (count & 1)
						{
							*dest = R_DrawColumnPixel<DC_BASIC>(dc, dest, (frac>>FRACBITS) & heightmask, source, colormap);
						}
					}
					else
					{
						const intptr_t fixed_heightmask = dc->texheight << FRACBITS;

						if (frac < 0)
						{
							while ((frac += fixed_heightmask) < 0)
							{
								;
							}
						}
						else
						{
							while (frac >= fixed_heightmask)
							{
								frac -= fixed_heightmask;
							}
						}

						do
						{
							// Re-map color indices from wall texture column
							//  using a lighting/special effects LUT.
							// heightmask is the Tutti-Frutti fix

							// -1 is the lower clamp bound because column posts have a "safe" byte before the real data
							// and a few bytes after as well
							*dest = R_DrawColumnPixel<DC_BASIC>(dc, dest, CLAMP((frac >> FRACBITS), npow2min, npow2max), source, colormap);

							dest += stride;

#if __SIZEOF_POINTER__ < 8  // 64-bit systems have large enough numbers for this to be a non-issue
							// Avoid overflow.
							if (fracstep > 0x7FFFFFFF - frac)
							{
								frac += fracstep - fixed_heightmask;
							}
							else
#endif
							{
								frac += fracstep;
							}

							while (frac >= fixed_heightmask)
							{
								frac -= fixed_heightmask;
							}
						}
						while (--count);
					}
				}
				break;
		}
	}
}
