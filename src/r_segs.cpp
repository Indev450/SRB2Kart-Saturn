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
/// \file  r_segs.c
/// \brief All the clipping: columns, horizontal spans, sky columns

#include "doomdef.h"
#include "r_local.h"
#include "r_sky.h"

#include "r_portal.h"
#include "r_splats.h"

#include "w_wad.h"
#include "z_zone.h"
#include "d_netcmd.h"
#include "m_misc.h"
#include "p_local.h" // Camera...
#include "p_setup.h"
#include "p_slopes.h"
#include "console.h" // con_clipviewtop
#include "screen.h"

#include "core/memory.h"
#include "core/thread_pool.h"

// OPTIMIZE: closed two sided lines as single sided

// True if any of the segs textures might be visible.
static boolean segtextured;
static boolean markfloor; // False if the back side is the same plane.
static boolean markceiling;

static boolean maskedtexture;
static INT32 toptexture, bottomtexture, midtexture;
static bool topremap, bottomremap, midremap;
static INT32 numthicksides, numbackffloors;

angle_t rw_normalangle;
// angle to line origin
angle_t rw_angle1;
fixed_t rw_distance;

//
// regular wall
//
static INT32 rw_x, rw_stopx;
static angle_t rw_centerangle;
static fixed_t rw_offset;
static fixed_t rw_offset2; // for splats
static fixed_t rw_scale, rw_scalestep;
static fixed_t rw_midtexturemid, rw_toptexturemid, rw_bottomtexturemid;
static INT32 worldtop, worldbottom, worldhigh, worldlow;
static INT32 worldtopslope, worldbottomslope, worldhighslope, worldlowslope; // worldtop/bottom at end of slope
static fixed_t rw_toptextureslide, rw_midtextureslide, rw_bottomtextureslide; // Defines how to adjust Y offsets along the wall for slopes
static fixed_t rw_midtextureback, rw_midtexturebackslide; // Values for masked midtexture height calculation

// Lactozilla: 3D floor clipping
static boolean rw_floormarked = false;
static boolean rw_ceilingmarked = false;

static INT32 *rw_silhouette = NULL;
static fixed_t *rw_tsilheight = NULL;
static fixed_t *rw_bsilheight = NULL;

static fixed_t pixhigh, pixlow, pixhighstep, pixlowstep;
static fixed_t topfrac, topstep;
static fixed_t bottomfrac, bottomstep;

static lighttable_t **walllights;
static INT16 *maskedtexturecol;
static fixed_t *maskedtextureheight = NULL;

// ==========================================================================
// R_RenderMaskedSegRange
// ==========================================================================

// If we have a multi-patch texture on a 2sided wall (rare) then we draw
//  it using R_DrawColumn, else we draw it using R_DrawMaskedColumn, this
//  way we don't have to store extra post_t info with each column for
//  multi-patch textures. They are not normally needed as multi-patch
//  textures don't have holes in it. At least not for now.

static void R_Render2sidedMultiPatchColumn(drawcolumndata_t* dc, column_t *column)
{
	INT32 topscreen, bottomscreen;

	topscreen = sprtopscreen; // + spryscale*column->topdelta;  topdelta is 0 for the wall
	bottomscreen = topscreen + spryscale * lengthcol;

	dc->yl = (sprtopscreen+FRACUNIT-1)>>FRACBITS;
	dc->yh = (bottomscreen-1)>>FRACBITS;

	if (windowtop != INT32_MAX && windowbottom != INT32_MAX)
	{
		dc->yl = ((windowtop + FRACUNIT)>>FRACBITS);
		dc->yh = (windowbottom - 1)>>FRACBITS;
	}

	if (dc->yh >= mfloorclip[dc->x])
		dc->yh =  mfloorclip[dc->x] - 1;
	if (dc->yl <= mceilingclip[dc->x])
		dc->yl =  mceilingclip[dc->x] + 1;

	if (dc->yl >= vid.height || dc->yh < 0)
		return;

	if (dc->yl <= dc->yh && dc->yh < vid.height && dc->yh > 0)
	{
		dc->source = (UINT8 *)column + 3;
		dc->sourcelength = lengthcol;

		// Drawn by R_DrawColumn.
		drawcolumndata_t dc_copy = *dc;
		coldrawfunc_t* colfunccopy = colfunc;

		// FIXME: do something better to look these up WITHOUT affecting global state...
		if (R_CheckColumnFunc(BASEDRAWFUNC) == true)
		{
			colfunccopy = colfuncs[COLDRAWFUNC_TWOSMULTIPATCH];
		}
		else if (R_CheckColumnFunc(COLDRAWFUNC_FUZZY) == true)
		{
			colfunccopy = colfuncs[COLDRAWFUNC_TWOSMULTIPATCHTRANS];
		}

		colfunccopy(const_cast<drawcolumndata_t*>(&dc_copy));
	}
}

transnum_t R_GetLinedefTransTable(fixed_t alpha)
{
	return static_cast<transnum_t>((20*(FRACUNIT - alpha - 1) + FRACUNIT) >> (FRACBITS+1));
}

static void R_RenderMaskedSegLoop(drawcolumndata_t* dc, drawseg_t *drawseg, INT32 x1, INT32 x2, INT32 texnum, void (*colfunc_2s)(drawcolumndata_t*, column_t *))
{
	size_t pindex;
	column_t *col;
	INT32 lightnum, i;
	fixed_t height, realbot;
	lightlist_t *light;
	r_lightlist_t *rlight;
	sector_t *front, *back;
	INT32 times, repeats;
	INT64 overflow_test;
	INT32 range;
	line_t *ldef;

	ldef = curline->linedef;

	range = std::max(drawseg->x2-drawseg->x1, 1);

	// Setup lighting based on the presence/lack-of 3D floors.
	dc->numlights = 0;

	if (frontsector->numlights)
	{
		dc->numlights = frontsector->numlights;

		if (dc->numlights >= dc->maxlights)
		{
			dc->maxlights = dc->numlights;
			dc->lightlist = static_cast<r_lightlist_s*>(Z_Realloc(dc->lightlist, sizeof (*dc->lightlist) * dc->maxlights, PU_STATIC, NULL));
		}

		for (i = 0; i < dc->numlights; i++)
		{
			fixed_t leftheight, rightheight;
			light = &frontsector->lightlist[i];
			rlight = &dc->lightlist[i];

			leftheight  = P_GetLightZAt(light, drawseg-> leftpos.x, drawseg-> leftpos.y);
			rightheight = P_GetLightZAt(light, drawseg->rightpos.x, drawseg->rightpos.y);

			leftheight  -= viewz;
			rightheight -= viewz;

			rlight->height         = (centeryfrac) - FixedMul(leftheight, drawseg->scale1);
			rlight->heightstep     = (centeryfrac) - FixedMul(rightheight, drawseg->scale2);
			rlight->heightstep     = (rlight->heightstep-rlight->height)/(range);
			rlight->startheight    = rlight->height; // keep starting value here to reset for each repeat
			rlight->lightlevel     = *light->lightlevel;
			rlight->extra_colormap = light->extra_colormap;
			rlight->flags = static_cast<ffloortype_e>(light->flags);

			if (rlight->flags & FF_FOG || (rlight->extra_colormap && rlight->extra_colormap->fog))
				lightnum = (rlight->lightlevel >> LIGHTSEGSHIFT);
			else if (R_CheckColumnFunc(COLDRAWFUNC_FUZZY))
				lightnum = LIGHTLEVELS - 1;
			else
				lightnum = (rlight->lightlevel >> LIGHTSEGSHIFT);

			if (rlight->extra_colormap && rlight->extra_colormap->fog)
				;
			else if (P_ApplyLightOffset(lightnum, frontsector))
				lightnum += curline->lightOffset;

			rlight->lightnum = lightnum;
		}
	}
	else
	{
		if ((R_CheckColumnFunc(COLDRAWFUNC_FUZZY) == false)
			|| (frontsector->extra_colormap && frontsector->extra_colormap->fog))
			lightnum = (frontsector->lightlevel >> LIGHTSEGSHIFT);
		else
			lightnum = LIGHTLEVELS - 1;

		if ((R_CheckColumnFunc(COLDRAWFUNC_FOG) == true)
			|| (frontsector->extra_colormap && frontsector->extra_colormap->fog))
			;
		else if (P_ApplyLightOffset(lightnum, frontsector))
			lightnum += curline->lightOffset;

		if (lightnum < 0)
			walllights = scalelight[0];
		else if (lightnum >= LIGHTLEVELS)
			walllights = scalelight[LIGHTLEVELS - 1];
		else
			walllights = scalelight[lightnum];
	}

	if (frontsector->heightsec != -1)
		front = &sectors[frontsector->heightsec];
	else
		front = frontsector;

	if (backsector->heightsec != -1)
		back = &sectors[backsector->heightsec];
	else
		back = backsector;

	if (drawseg->curline->sidedef->repeatcnt)
		repeats = 1 + drawseg->curline->sidedef->repeatcnt;
	else if (ldef->flags & ML_EFFECT5)
	{
		fixed_t high, low;

		if (front->ceilingheight > back->ceilingheight)
			high = back->ceilingheight;
		else
			high = front->ceilingheight;

		if (front->floorheight > back->floorheight)
			low = front->floorheight;
		else
			low = back->floorheight;

		repeats = (high - low)/textureheight[texnum];
		if ((high-low)%textureheight[texnum])
			repeats++; // tile an extra time to fill the gap -- Monster Iestyn
	}
	else
		repeats = 1;

	for (times = 0; times < repeats; times++)
	{
		if (times > 0)
		{
			rw_scalestep = drawseg->scalestep;
			spryscale = drawseg->scale1 + (x1 - drawseg->x1)*rw_scalestep;
			if (dc->numlights)
			{ // reset all lights to their starting heights
				for (i = 0; i < dc->numlights; i++)
				{
					rlight = &dc->lightlist[i];
					rlight->height = rlight->startheight;
				}
			}
		}

		dc->texheight = textureheight[texnum]>>FRACBITS;

		// draw the columns
		for (dc->x = x1; dc->x <= x2; dc->x++)
		{
			dc->texturemid = drawseg->maskedtextureheight[dc->x];

			if (!!(curline->linedef->flags & ML_DONTPEGBOTTOM) ^ !!(curline->linedef->flags & ML_EFFECT3))
				dc->texturemid += (textureheight[texnum])*times + textureheight[texnum];
			else
				dc->texturemid -= (textureheight[texnum])*times;

			// calculate lighting
			if (maskedtexturecol[dc->x] != INT16_MAX)
			{
				// Check for overflows first
				overflow_test = (INT64)centeryfrac - (((INT64)dc->texturemid*spryscale)>>FRACBITS);
				if (overflow_test < 0) overflow_test = -overflow_test;
				if ((UINT64)overflow_test&0xFFFFFFFF80000000ULL)
				{
					// Eh, no, go away, don't waste our time
					if (dc->numlights)
					{
						for (i = 0; i < dc->numlights; i++)
						{
							rlight = &dc->lightlist[i];
							rlight->height += rlight->heightstep;
						}
					}
					spryscale += rw_scalestep;
					continue;
				}

				if (dc->numlights)
				{
					lighttable_t **xwalllights;

					sprbotscreen = INT32_MAX;
					sprtopscreen = windowtop = (centeryfrac - FixedMul(dc->texturemid, spryscale));

					realbot = windowbottom = FixedMul(textureheight[texnum], spryscale) + sprtopscreen;
					dc->iscale = 0xffffffffu / (unsigned)spryscale;

					// draw the texture
					col = (column_t *)((UINT8 *)R_GetColumn(texnum, maskedtexturecol[dc->x]) - 3);

					auto set_light_vars = [&](INT32 k)
					{
						rlight = &dc->lightlist[k];

						lightnum = rlight->lightnum;

						if (lightnum < 0)
							xwalllights = scalelight[0];
						else if (lightnum >= LIGHTLEVELS)
							xwalllights = scalelight[LIGHTLEVELS-1];
						else
							xwalllights = scalelight[lightnum];

						pindex = FixedMul(spryscale, LIGHTRESOLUTIONFIX)>>LIGHTSCALESHIFT;

						if (pindex >= MAXLIGHTSCALE)
							pindex = MAXLIGHTSCALE - 1;

						if (rlight->extra_colormap)
							rlight->rcolormap = rlight->extra_colormap->colormap + (xwalllights[pindex] - colormaps);
						else
							rlight->rcolormap = xwalllights[pindex];
					};


					auto set_colormap_below_light = [&]
					{
						dc->colormap = rlight->rcolormap;
						if (encoremap && !(ldef->flags & ML_TFERLINE))
							dc->colormap += COLORMAP_REMAPOFFSET;
					};

					// Use the base sector's light level above the first FOF.
					// You can imagine it as the sky casting its light on top of the highest FOF.
					set_light_vars(0);
					set_colormap_below_light();

					for (i = 0; i < dc->numlights; i++)
					{
						if ((dc->lightlist[i].flags & FF_NOSHADE))
							continue;

						set_light_vars(i);

						height = rlight->height;
						rlight->height += rlight->heightstep;

						if (height <= windowtop)
						{
							set_colormap_below_light();
							continue;
						}

						windowbottom = height;
						if (windowbottom >= realbot)
						{
							windowbottom = realbot;
							colfunc_2s(dc, col);
							for (i++; i < dc->numlights; i++)
							{
								rlight = &dc->lightlist[i];
								rlight->height += rlight->heightstep;
							}

							continue;
						}
						colfunc_2s(dc, col);

						windowtop = windowbottom + 1;
						set_colormap_below_light();
					}
					windowbottom = realbot;
					if (windowtop < windowbottom)
						colfunc_2s(dc, col);

					spryscale += rw_scalestep;
					continue;
				}

				// calculate lighting
				pindex = FixedMul(spryscale, LIGHTRESOLUTIONFIX)>>LIGHTSCALESHIFT;

				if (pindex >= MAXLIGHTSCALE)
					pindex = MAXLIGHTSCALE - 1;

				dc->colormap = walllights[pindex];
				if (encoremap && !(ldef->flags & ML_TFERLINE))
					dc->colormap += COLORMAP_REMAPOFFSET;

				if (frontsector->extra_colormap)
					dc->colormap = frontsector->extra_colormap->colormap + (dc->colormap - colormaps);

				sprtopscreen = centeryfrac - FixedMul(dc->texturemid, spryscale);
				dc->iscale = 0xffffffffu / (unsigned)spryscale;

				// draw the texture
				col = (column_t *)((UINT8 *)R_GetColumn(texnum, maskedtexturecol[dc->x]) - 3);

				colfunc_2s(dc, col);
			}
			spryscale += rw_scalestep;
		}
	}
}

static boolean R_CheckBlendMode(drawcolumndata_t* dc, const line_t *ldef)
{
	if (!ldef->alpha)
		return false;

	if (ldef->blendmode)
	{
		if (ldef->alpha == NUMTRANSMAPS || ldef->blendmode == AST_MODULATE)
			dc->transmap = R_GetBlendTable(ldef->blendmode, 0);
		else
			dc->transmap = R_GetBlendTable(ldef->blendmode, R_GetLinedefTransTable(ldef->alpha));

		R_SetColumnFunc(COLDRAWFUNC_FUZZY);
	}
	else if (ldef->alpha > 0 && ldef->alpha < FRACUNIT)
	{
		dc->transmap = R_GetTranslucencyTable(R_GetLinedefTransTable(ldef->alpha));
		R_SetColumnFunc(COLDRAWFUNC_FUZZY);
	}
	else if (ldef->special == 909)
	{
		R_SetColumnFunc(COLDRAWFUNC_FOG);
		windowtop = frontsector->ceilingheight;
		windowbottom = frontsector->floorheight;
	}
	else
	{
		R_SetColumnFunc(BASEDRAWFUNC);
	}

	if (curline->polyseg && curline->polyseg->translucency > 0)
	{
		if (curline->polyseg->translucency >= NUMTRANSMAPS)
			return false;

		dc->transmap = R_GetTranslucencyTable(curline->polyseg->translucency);
		R_SetColumnFunc(COLDRAWFUNC_FUZZY);
	}

	return true;
}

void R_RenderMaskedSegRange(drawseg_t *drawseg, INT32 x1, INT32 x2)
{
	INT32 texnum;
	line_t *ldef;
	void (*colfunc_2s)(drawcolumndata_t* dc, column_t *);
	drawcolumndata_t *dc = &g_dc;

	// Calculate light table.
	// Use different light tables
	//   for horizontal / vertical / diagonal. Diagonal?
	// OPTIMIZE: get rid of LIGHTSEGSHIFT globally
	curline = drawseg->curline;

	frontsector = curline->frontsector;
	backsector = curline->backsector;
	texnum = R_GetTextureNum(curline->sidedef->midtexture);
	windowbottom = windowtop = sprbotscreen = INT32_MAX;

	// hack translucent linedef types (900-909 for transtables 1-9)
	ldef = curline->linedef;

	if (R_CheckBlendMode(dc, ldef) == false)
	{
		return; // does not render
	}

	rw_scalestep = drawseg->scalestep;
	spryscale = drawseg->scale1 + (x1 - drawseg->x1)*rw_scalestep;

	// Texture must be cached before setting colfunc_2s,
	// otherwise texture[texnum]->holes may be false when it shouldn't be
	R_CheckTextureCache(texnum);

	// handle case where multipatch texture is drawn on a 2sided wall, multi-patch textures
	// are not stored per-column with post info in SRB2
	if (textures[texnum]->holes)
		colfunc_2s = R_DrawMaskedColumn; // render the usual 2sided single-patch packed texture
	else
	{
		colfunc_2s = R_Render2sidedMultiPatchColumn; // render multipatch with no holes (no post_t info)
		lengthcol = textures[texnum]->height;
	}

	maskedtexturecol = drawseg->maskedtexturecol;

	mfloorclip = drawseg->sprbottomclip;
	mceilingclip = drawseg->sprtopclip;

	R_RenderMaskedSegLoop(dc, drawseg, x1, x2, texnum, colfunc_2s);

	R_SetColumnFunc(BASEDRAWFUNC);
}

template <typename T>
static constexpr T saturating_add(T x, T y) noexcept
{
	INT64 z;
	z = static_cast<INT64>(x) + static_cast<INT64>(y);
	if (z > static_cast<INT64>(std::numeric_limits<T>::max()))
	{
		z = static_cast<INT64>(std::numeric_limits<T>::max());
	}
	else if (z < static_cast<INT64>(std::numeric_limits<T>::min()))
	{
		z = static_cast<INT64>(std::numeric_limits<T>::min());
	}
	return static_cast<T>(z);
}

template <typename T>
static constexpr T saturating_mul(T x, T y) noexcept
{
	INT64 z;
	z = static_cast<INT64>(x) * static_cast<INT64>(y);
	if (z > static_cast<INT64>(std::numeric_limits<T>::max()))
	{
		z = static_cast<INT64>(std::numeric_limits<T>::max());
	}
	else if (z < static_cast<INT64>(std::numeric_limits<T>::min()))
	{
		z = static_cast<INT64>(std::numeric_limits<T>::min());
	}
	return static_cast<T>(z);
}

// Loop through R_DrawMaskedColumn calls
static void R_DrawRepeatMaskedColumn(drawcolumndata_t* dc, column_t *col)
{
	while (sprtopscreen < sprbotscreen)
	{
		R_DrawMaskedColumn(dc, col);
		sprtopscreen = saturating_add(sprtopscreen, saturating_mul(dc->texheight, spryscale));
	}
}

// Returns true if a fake floor is translucent.
static boolean R_IsFFloorTranslucent(visffloor_t *pfloor)
{
	if (pfloor->polyobj)
		return (pfloor->polyobj->translucency > 0);

	// Polyobjects have no ffloors, and they're handled in the conditional above.
	if (pfloor->ffloor != NULL)
		return (pfloor->ffloor->flags & (FF_TRANSLUCENT|FF_FOG));

	return false;
}

//
// R_RenderThickSideRange
// Renders all the thick sides in the given range.
void R_RenderThickSideRange(drawseg_t *drawseg, INT32 x1, INT32 x2, ffloor_t *pfloor)
{
	size_t          pindex = 0;
	column_t *      col;
	INT32           lightnum;
	INT32           texnum;
	sector_t        tempsec;
	INT32           templight;
	INT32           i, p;
	fixed_t         bottombounds = viewheight << FRACBITS;
	fixed_t         topbounds = (con_clipviewtop - 1) << FRACBITS;
	fixed_t         offsetvalue = 0;
	lightlist_t     *light;
	r_lightlist_t   *rlight;
	INT32           range;
	line_t          *newline = NULL;
	// Render FOF sides kinda like normal sides, with the frac and step and everything
	// NOTE: INT64 instead of fixed_t because overflow concerns
	INT64         top_frac, top_step, bottom_frac, bottom_step;
	// skew FOF walls with slopes?
	boolean	      slopeskew = false;
	fixed_t       ffloortextureslide = 0;
	INT32         oldx = -1;
	fixed_t       left_top, left_bottom; // needed here for slope skewing
	pslope_t      *skewslope = NULL;

	drawcolumndata_t *dc = &g_dc;

	void (*colfunc_2s) (drawcolumndata_t* dc, column_t *);

	// Calculate light table.
	// Use different light tables
	//   for horizontal / vertical / diagonal. Diagonal?
	// OPTIMIZE: get rid of LIGHTSEGSHIFT globally

	curline = drawseg->curline;
	backsector = pfloor->target;
	frontsector = curline->frontsector == pfloor->target ? curline->backsector : curline->frontsector;
	texnum = R_GetTextureNum(sides[pfloor->master->sidenum[0]].midtexture);

	R_SetColumnFunc(BASEDRAWFUNC);

	if (pfloor->master->flags & ML_TFERLINE)
	{
		size_t linenum = std::min((size_t)(curline->linedef-backsector->lines[0]), pfloor->master->frontsector->linecount);
		newline = pfloor->master->frontsector->lines[0] + linenum;
		texnum = R_GetTextureNum(sides[newline->sidenum[0]].midtexture);
	}

	if (pfloor->flags & FF_TRANSLUCENT)
	{
		boolean fuzzy = true;

		// Hacked up support for alpha value in software mode Tails 09-24-2002
		// ...unhacked by toaster 04-01-2021, re-hacked a little by sphere 19-11-2021
		// and mercilessly shoved into saturn by chearii 02-02-2025
		{
			INT32 trans = (10*((256+12) - pfloor->alpha))/255;
			if (trans >= 10)
				return; // Don't even draw it
			if (pfloor->blend) // additive, (reverse) subtractive, modulative
				dc->transmap = R_GetBlendTable(pfloor->blend, trans);
			else if (!(dc->transmap = R_GetTranslucencyTable(trans)) || trans == 0)
				fuzzy = false; // Opaque
		}

		if (fuzzy)
		{
			R_SetColumnFunc(COLDRAWFUNC_FUZZY);
		}
	}
	else if (pfloor->flags & FF_FOG)
	{
		R_SetColumnFunc(COLDRAWFUNC_FOG);
	}

	range = std::max(drawseg->x2-drawseg->x1, 1);
	//SoM: Moved these up here so they are available for my lightlist calculations
	rw_scalestep = drawseg->scalestep;
	spryscale = drawseg->scale1 + (x1 - drawseg->x1)*rw_scalestep;

	dc->numlights = 0;
	if (frontsector->numlights)
	{
		dc->numlights = frontsector->numlights;

		if (dc->numlights >= dc->maxlights)
		{
			dc->maxlights = dc->numlights;
			dc->lightlist = static_cast<r_lightlist_s*>(Z_Realloc(dc->lightlist, sizeof (*dc->lightlist) * dc->maxlights, PU_STATIC, NULL));
		}

		for (i = p = 0; i < dc->numlights; i++)
		{
			fixed_t leftheight, rightheight;
			fixed_t pfloorleft, pfloorright;
			INT64 overflow_test;
			light = &frontsector->lightlist[i];
			rlight = &dc->lightlist[p];

#define SLOPEPARAMS(slope, end1, end2, normalheight) \
	end1 = P_GetZAt(slope, drawseg->leftpos.x, drawseg->leftpos.y, normalheight); \
	end2 = P_GetZAt(slope, drawseg->rightpos.x, drawseg->rightpos.y, normalheight);

			SLOPEPARAMS(light->slope,     leftheight, rightheight, light->height)
			SLOPEPARAMS(*pfloor->b_slope, pfloorleft, pfloorright, *pfloor->bottomheight)

			if (leftheight < pfloorleft && rightheight < pfloorright)
				continue;

			SLOPEPARAMS(*pfloor->t_slope, pfloorleft, pfloorright, *pfloor->topheight)

			if (leftheight > pfloorleft && rightheight > pfloorright && i+1 < dc->numlights)
			{
				lightlist_t *nextlight = &frontsector->lightlist[i+1];
				if ((P_GetLightZAt(nextlight, drawseg->leftpos.x, drawseg->leftpos.y) > pfloorleft)
				 && (P_GetLightZAt(nextlight, drawseg->rightpos.x, drawseg->rightpos.y) > pfloorright))
					continue;
			}

			leftheight -= viewz;
			rightheight -= viewz;

#define CLAMPMAX INT32_MAX
#define CLAMPMIN (-INT32_MAX) // This is not INT32_MIN on purpose! INT32_MIN makes the drawers freak out.
			// Monster Iestyn (25/03/18): do not skip these lights if they fail overflow test, just clamp them instead so they behave.
			overflow_test = (INT64)centeryfrac - (((INT64)leftheight*drawseg->scale1)>>FRACBITS);
			if      (overflow_test > (INT64)CLAMPMAX) rlight->height = CLAMPMAX;
			else if (overflow_test > (INT64)CLAMPMIN) rlight->height = (fixed_t)overflow_test;
			else                                      rlight->height = CLAMPMIN;

			overflow_test = (INT64)centeryfrac - (((INT64)rightheight*drawseg->scale2)>>FRACBITS);
			if      (overflow_test > (INT64)CLAMPMAX) rlight->heightstep = CLAMPMAX;
			else if (overflow_test > (INT64)CLAMPMIN) rlight->heightstep = (fixed_t)overflow_test;
			else                                      rlight->heightstep = CLAMPMIN;
			rlight->heightstep = (rlight->heightstep-rlight->height)/(range);
			rlight->flags = static_cast<ffloortype_e>(light->flags);

			if (light->flags & FF_CUTLEVEL)
			{
				SLOPEPARAMS(*light->caster->b_slope, leftheight, rightheight, *light->caster->bottomheight)
#undef SLOPEPARAMS
				leftheight -= viewz;
				rightheight -= viewz;

				// Monster Iestyn (25/03/18): do not skip these lights if they fail overflow test, just clamp them instead so they behave.
				overflow_test = (INT64)centeryfrac - (((INT64)leftheight*drawseg->scale1)>>FRACBITS);
				if      (overflow_test > (INT64)CLAMPMAX) rlight->botheight = CLAMPMAX;
				else if (overflow_test > (INT64)CLAMPMIN) rlight->botheight = (fixed_t)overflow_test;
				else                                      rlight->botheight = CLAMPMIN;

				overflow_test = (INT64)centeryfrac - (((INT64)rightheight*drawseg->scale2)>>FRACBITS);
				if      (overflow_test > (INT64)CLAMPMAX) rlight->botheightstep = CLAMPMAX;
				else if (overflow_test > (INT64)CLAMPMIN) rlight->botheightstep = (fixed_t)overflow_test;
				else                                      rlight->botheightstep = CLAMPMIN;
				rlight->botheightstep = (rlight->botheightstep-rlight->botheight)/(range);
			}

			rlight->lightlevel = *light->lightlevel;
			rlight->extra_colormap = light->extra_colormap;

			// Check if the current light effects the colormap/lightlevel
			if (pfloor->flags & FF_FOG)
				rlight->lightnum = (pfloor->master->frontsector->lightlevel >> LIGHTSEGSHIFT);
			else
				rlight->lightnum = (rlight->lightlevel >> LIGHTSEGSHIFT);

			if (pfloor->flags & FF_FOG || rlight->flags & FF_FOG || (rlight->extra_colormap && rlight->extra_colormap->fog))
				;
			else if (P_ApplyLightOffset(rlight->lightnum, frontsector))
				rlight->lightnum += curline->lightOffset;

			p++;
		}

		dc->numlights = p;
	}
	else
	{
		// Get correct light level!
		if ((frontsector->extra_colormap && frontsector->extra_colormap->fog))
			lightnum = (frontsector->lightlevel >> LIGHTSEGSHIFT);
		else if (pfloor->flags & FF_FOG)
			lightnum = (pfloor->master->frontsector->lightlevel >> LIGHTSEGSHIFT);
		else if (R_CheckColumnFunc(COLDRAWFUNC_FUZZY) == true)
			lightnum = LIGHTLEVELS-1;
		else
			lightnum = R_FakeFlat(frontsector, &tempsec, &templight, &templight, false)
				->lightlevel >> LIGHTSEGSHIFT;

		if (pfloor->flags & FF_FOG || (frontsector->extra_colormap && frontsector->extra_colormap->fog))
			;
		else if (P_ApplyLightOffset(lightnum, frontsector))
			lightnum += curline->lightOffset;

		if (lightnum < 0)
			walllights = scalelight[0];
		else if (lightnum >= LIGHTLEVELS)
			walllights = scalelight[LIGHTLEVELS-1];
		else
			walllights = scalelight[lightnum];
	}

	maskedtexturecol = drawseg->thicksidecol;

	mfloorclip = drawseg->sprbottomclip;
	mceilingclip = drawseg->sprtopclip;
	dc->texheight = textureheight[texnum]>>FRACBITS;

	// calculate both left endrawseg
	left_top    = P_GetFFloorTopZAt   (pfloor, drawseg->leftpos.x, drawseg->leftpos.y) - viewz;
	left_bottom = P_GetFFloorBottomZAt(pfloor, drawseg->leftpos.x, drawseg->leftpos.y) - viewz;

	skewslope = *pfloor->t_slope; // skew using top slope by default

	if (newline)
	{
		if (newline->flags & ML_DONTPEGTOP)
			slopeskew = true;
	}
	else if (pfloor->master->flags & ML_DONTPEGTOP)
		slopeskew = true;

	if (slopeskew)
		dc->texturemid = left_top;
	else
		dc->texturemid = *pfloor->topheight - viewz;

	if (newline)
	{
		offsetvalue = sides[newline->sidenum[0]].rowoffset;
		if (newline->flags & ML_DONTPEGBOTTOM)
		{
			skewslope = *pfloor->b_slope; // skew using bottom slope
			if (slopeskew)
				dc->texturemid = left_bottom;
			else
			offsetvalue -= *pfloor->topheight - *pfloor->bottomheight;
		}
	}
	else
	{
		offsetvalue = sides[pfloor->master->sidenum[0]].rowoffset;
		if (curline->linedef->flags & ML_DONTPEGBOTTOM)
		{
			skewslope = *pfloor->b_slope; // skew using bottom slope
			if (slopeskew)
				dc->texturemid = left_bottom;
			else
			offsetvalue -= *pfloor->topheight - *pfloor->bottomheight;
		}
	}

	if (slopeskew)
	{
		angle_t lineangle = R_PointToAngle2(curline->v1->x, curline->v1->y, curline->v2->x, curline->v2->y);

		if (skewslope)
			ffloortextureslide = FixedMul(skewslope->zdelta, FINECOSINE((lineangle-skewslope->xydirection)>>ANGLETOFINESHIFT));
	}

	dc->texturemid += offsetvalue;

	// Texture must be cached before setting colfunc_2s,
	// otherwise texture[texnum]->holes may be false when it shouldn't be
	R_CheckTextureCache(texnum);

	//faB: handle case where multipatch texture is drawn on a 2sided wall, multi-patch textures
	//     are not stored per-column with post info anymore in Doom Legacy
	if (textures[texnum]->holes)
		colfunc_2s = R_DrawRepeatMaskedColumn;              //render the usual 2sided single-patch packed texture
	else
	{
		colfunc_2s = R_Render2sidedMultiPatchColumn;        //render multipatch with no holes (no post_t info)
		lengthcol = textures[texnum]->height;
	}

	// Set heights according to plane, or slope, whichever
	{
		fixed_t right_top, right_bottom;

		// calculate right ends now
		right_top    = P_GetFFloorTopZAt   (pfloor, drawseg->rightpos.x, drawseg->rightpos.y) - viewz;
		right_bottom = P_GetFFloorBottomZAt(pfloor, drawseg->rightpos.x, drawseg->rightpos.y) - viewz;

		// using INT64 to avoid 32bit overflow
		top_frac =    (INT64)centeryfrac - (((INT64)left_top     * drawseg->scale1) >> FRACBITS);
		bottom_frac = (INT64)centeryfrac - (((INT64)left_bottom  * drawseg->scale1) >> FRACBITS);
		top_step =    (INT64)centeryfrac - (((INT64)right_top    * drawseg->scale2) >> FRACBITS);
		bottom_step = (INT64)centeryfrac - (((INT64)right_bottom * drawseg->scale2) >> FRACBITS);

		top_step = (top_step-top_frac)/(range);
		bottom_step = (bottom_step-bottom_frac)/(range);

		top_frac += top_step * (x1 - drawseg->x1);
		bottom_frac += bottom_step * (x1 - drawseg->x1);
	}

	// draw the columns
	for (dc->x = x1; dc->x <= x2; dc->x++)
	{
		if (maskedtexturecol[dc->x] == INT16_MAX)
			continue;

		// skew FOF walls
		if (ffloortextureslide)
		{
			if (oldx != -1)
				dc->texturemid += FixedMul(ffloortextureslide, (maskedtexturecol[oldx]-maskedtexturecol[dc->x])<<FRACBITS);
			oldx = dc->x;
		}
		// Calculate bounds
		// clamp the values if necessary to avoid overflows and rendering glitches caused by them

		if      (top_frac > (INT64)CLAMPMAX)    sprtopscreen = windowtop = CLAMPMAX;
		else if (top_frac > (INT64)CLAMPMIN)    sprtopscreen = windowtop = (fixed_t)top_frac;
		else                                    sprtopscreen = windowtop = CLAMPMIN;
		if      (bottom_frac > (INT64)CLAMPMAX) sprbotscreen = windowbottom = CLAMPMAX;
		else if (bottom_frac > (INT64)CLAMPMIN) sprbotscreen = windowbottom = (fixed_t)bottom_frac;
		else                                    sprbotscreen = windowbottom = CLAMPMIN;

		top_frac += top_step;
		bottom_frac += bottom_step;

		// SoM: If column is out of range, why bother with it??
		if (windowbottom < topbounds || windowtop > bottombounds)
		{
			if (dc->numlights)
			{
				for (i = 0; i < dc->numlights; i++)
				{
					rlight = &dc->lightlist[i];
					rlight->height += rlight->heightstep;
					if (rlight->flags & FF_CUTLEVEL)
						rlight->botheight += rlight->botheightstep;
				}
			}
			spryscale += rw_scalestep;
			continue;
		}

		dc->iscale = 0xffffffffu / (unsigned)spryscale;

		// Get data for the column
		col = (column_t *)((UINT8 *)R_GetColumn(texnum, maskedtexturecol[dc->x]) - 3);

		// SoM: New code does not rely on R_DrawColumnShadowed which
		// will (hopefully) put less strain on the stack.
		if (dc->numlights)
		{
			lighttable_t **xwalllights;
			fixed_t height;
			fixed_t bheight = 0;
			INT32 solid = 0;
			INT32 lighteffect = 0;

			auto set_light_vars = [&](INT32 k)
			{
				rlight = &dc->lightlist[k];
				lighteffect = !(dc->lightlist[k].flags & FF_NOSHADE);

				if (lighteffect)
				{
					lightnum = rlight->lightnum;

					if (lightnum < 0)
						xwalllights = scalelight[0];
					else if (lightnum >= LIGHTLEVELS)
						xwalllights = scalelight[LIGHTLEVELS-1];
					else
						xwalllights = scalelight[lightnum];

					pindex = FixedMul(spryscale, LIGHTRESOLUTIONFIX)>>LIGHTSCALESHIFT;

					if (pindex >= MAXLIGHTSCALE)
						pindex = MAXLIGHTSCALE-1;

					if (pfloor->flags & FF_FOG)
					{
						if (pfloor->master->frontsector->extra_colormap)
							rlight->rcolormap = pfloor->master->frontsector->extra_colormap->colormap + (xwalllights[pindex] - colormaps);
						else
							rlight->rcolormap = xwalllights[pindex];
					}
					else
					{
						if (rlight->extra_colormap)
							rlight->rcolormap = rlight->extra_colormap->colormap + (xwalllights[pindex] - colormaps);
						else
							rlight->rcolormap = xwalllights[pindex];
					}
				}
			};

			auto set_colormap_below_light = [&]
			{
				dc->colormap = rlight->rcolormap;
				if (encoremap && !(curline->linedef->flags & ML_TFERLINE))
					dc->colormap += COLORMAP_REMAPOFFSET;
			};

			// Use the base sector's light level above the first FOF.
			// You can imagine it as the sky casting its light on top of the highest FOF.
			set_light_vars(0);
			set_colormap_below_light();

			for (i = 0; i < dc->numlights; i++)
			{
				// Check if the current light effects the colormap/lightlevel
				rlight = &dc->lightlist[i];
				lighteffect = !(dc->lightlist[i].flags & FF_NOSHADE);

				if (lighteffect)
				{
					set_light_vars(i);
				}

				solid = 0; // don't carry over solid-cutting flag from the previous light

				// Check if the current light can cut the current 3D floor.
				if (rlight->flags & FF_CUTSOLIDS && !(pfloor->flags & FF_EXTRA))
					solid = 1;
				else if (rlight->flags & FF_CUTEXTRA && pfloor->flags & FF_EXTRA)
				{
					if (rlight->flags & FF_EXTRA)
					{
						// The light is from an extra 3D floor... Check the flags so
						// there are no undesired cuts.
						if ((rlight->flags & (FF_FOG|FF_SWIMMABLE)) == (pfloor->flags & (FF_FOG|FF_SWIMMABLE)))
							solid = 1;
					}
					else
						solid = 1;
				}
				else
					solid = 0;

				height = rlight->height;
				rlight->height += rlight->heightstep;

				if (solid)
				{
					bheight = rlight->botheight - (FRACUNIT >> 1);
					rlight->botheight += rlight->botheightstep;
				}

				if (height <= windowtop)
				{
					if (lighteffect)
					{
						set_colormap_below_light();
					}
					if (solid && windowtop < bheight)
						windowtop = bheight;
					continue;
				}

				windowbottom = height;
				if (windowbottom >= sprbotscreen)
				{
					windowbottom = sprbotscreen;

					// draw the texture
					colfunc_2s (dc, col);
					for (i++; i < dc->numlights; i++)
					{
						rlight = &dc->lightlist[i];
						rlight->height += rlight->heightstep;
						if (rlight->flags & FF_CUTLEVEL)
							rlight->botheight += rlight->botheightstep;
					}
					continue;
				}

				// draw the texture
				colfunc_2s (dc, col);
				if (solid)
					windowtop = bheight;
				else
					windowtop = windowbottom + 1;

				if (lighteffect)
				{
					set_colormap_below_light();
				}
			}

			windowbottom = sprbotscreen;

			// draw the texture, if there is any space left
			if (windowtop < windowbottom)
				colfunc_2s (dc, col);

			spryscale += rw_scalestep;
			continue;
		}

		// calculate lighting
		pindex = FixedMul(spryscale, LIGHTRESOLUTIONFIX)>>LIGHTSCALESHIFT;

		if (pindex >= MAXLIGHTSCALE)
			pindex = MAXLIGHTSCALE - 1;

		dc->colormap = walllights[pindex];

		if (encoremap && !(curline->linedef->flags & ML_TFERLINE))
			dc->colormap += COLORMAP_REMAPOFFSET;

		if (pfloor->flags & FF_FOG && pfloor->master->frontsector->extra_colormap)
			dc->colormap = pfloor->master->frontsector->extra_colormap->colormap + (dc->colormap - colormaps);
		else if (frontsector->extra_colormap)
			dc->colormap = frontsector->extra_colormap->colormap + (dc->colormap - colormaps);

		// draw the texture
		colfunc_2s (dc, col);
		spryscale += rw_scalestep;
	}

	R_SetColumnFunc(BASEDRAWFUNC);

#undef CLAMPMAX
#undef CLAMPMIN
}

// R_FFloorCanClip
//
// Returns true if a fake floor can clip a column away.
static boolean R_FFloorCanClip(visffloor_t *pfloor)
{
	return (cv_ffloorclip.value && !R_IsFFloorTranslucent(pfloor) && !pfloor->polyobj);
}

// R_ExpandPlaneY
//
// A simple function to modify a vsplane's top and bottom for a particular column
// Sort of like R_ExpandPlane in r_plane.c, except this is vertical expansion
static inline void R_ExpandPlaneY(visplane_t *pl, INT32 x, INT16 top, INT16 bottom)
{
	// Expand the plane, don't shrink it!
	// note: top and bottom default to 0xFFFF and 0x0000 respectively, which is totally compatible with this
	if (pl->top[x] > top)       pl->top[x] = top;
	if (pl->bottom[x] < bottom) pl->bottom[x] = bottom;
}

//
// R_RenderSegLoop
// Draws zero, one, or two textures (and possibly a masked
//  texture) for walls.
// Can draw or mark the starting pixel of floor and ceiling
//  textures.
// CALLED: CORE LOOPING ROUTINE.
//
#define HEIGHTBITS              12
#define HEIGHTUNIT              (1<<HEIGHTBITS)

static void R_DrawWallColumn(drawcolumndata_t* dc, INT32 yl, INT32 yh, fixed_t mid, fixed_t texturecolumn, INT32 texture, boolean remap)
{
	dc->yl = yl;
	dc->yh = yh;
	dc->texturemid = mid;
	dc->source = R_GetColumn(texture, texturecolumn);
	dc->texheight = textureheight[texture] >> FRACBITS;
	dc->sourcelength = dc->texheight;
	R_SetColumnFunc(colfunctype);
	coldrawfunc_t* colfunccopy = colfunc;
	drawcolumndata_t dc_copy = *dc;
	if (remap)
	{
		dc_copy.colormap += COLORMAP_REMAPOFFSET;
	}
	colfunccopy(const_cast<drawcolumndata_t*>(&dc_copy));
}

static boolean didsolidcol; // True if at least one column was marked solid

static void R_RenderSegLoop(drawcolumndata_t* dc)
{
	angle_t angle;
	size_t  pindex;
	INT32     yl;
	INT32     yh;

	INT32     mid;
	fixed_t texturecolumn = 0;
	fixed_t oldtexturecolumn = -1;
	INT32     top;
	INT32     bottom;
	INT32     i;

	for (; rw_x < rw_stopx; rw_x++)
	{
		// mark floor / ceiling areas
		yl = (topfrac+HEIGHTUNIT-1)>>HEIGHTBITS;

		// no space above wall?
		top = ceilingclip[rw_x]+1;

		// no space above wall?
		if (yl < top)
			yl = top;

		if (markceiling)
		{
			bottom = yl > floorclip[rw_x] ? floorclip[rw_x] : yl;

			if (top <= --bottom && ceilingplane)
			{
				R_ExpandPlaneY(ceilingplane, rw_x, top, bottom);
			}
		}

		yh = bottomfrac>>HEIGHTBITS;

		bottom = floorclip[rw_x]-1;

		if (yh > bottom)
			yh = bottom;

		if (markfloor)
		{
			top = yh < ceilingclip[rw_x] ? ceilingclip[rw_x] : yh;

			if (++top <= bottom && floorplane)
			{
				R_ExpandPlaneY(floorplane, rw_x, top, bottom);
			}
		}

		rw_floormarked = false;
		rw_ceilingmarked = false;

		if (numffloors)
		{
			INT16 fftop, ffbottom;

			firstseg->frontscale[rw_x] = frontscale[rw_x];
			top = ceilingclip[rw_x]+1; // PRBoom
			bottom = floorclip[rw_x]-1; // PRBoom

			for (i = 0; i < numffloors; i++)
			{
				if (ffloor[i].polyobj && (!curline->polyseg || ffloor[i].polyobj != curline->polyseg))
					continue;

				if (ffloor[i].height < viewz)
				{
					INT32 top_w = (ffloor[i].f_frac >> HEIGHTBITS) + 1;
					INT32 bottom_w = ffloor[i].f_clip[rw_x];

					if (top_w < top)
						top_w = top;

					if (bottom_w > bottom)
						bottom_w = bottom;

					// Polyobject-specific hack to fix plane leaking -Red
					if (ffloor[i].polyobj && top_w >= bottom_w)
					{
						ffloor[i].plane->top[rw_x] = 0xFFFF;
						ffloor[i].plane->bottom[rw_x] = 0x0000; // fix for sky plane drawing crashes - Monster Iestyn 25/05/18
					}
					else if (top_w <= bottom_w)
					{
						fftop = (INT16)top_w;
						ffbottom = (INT16)bottom_w;

						ffloor[i].plane->top[rw_x] = fftop;
						ffloor[i].plane->bottom[rw_x] = ffbottom;

						// Lactozilla: Cull part of the column by the 3D floor if it can't be seen
						// "bottom" is the top pixel of the floor column
						if (ffbottom >= bottom-1 && R_FFloorCanClip(&ffloor[i]) && !curline->polyseg)
						{
							rw_floormarked = true;
							floorclip[rw_x] = fftop;

							if (yh > fftop)
								yh = fftop;

							if (markfloor && floorplane)
								floorplane->top[rw_x] = bottom;

							if (rw_silhouette)
							{
								(*rw_silhouette) |= SIL_BOTTOM;
								(*rw_bsilheight) = INT32_MAX;
							}
						}
					}
				}
				else if (ffloor[i].height > viewz)
				{
					INT32 top_w = ffloor[i].c_clip[rw_x] + 1;
					INT32 bottom_w = (ffloor[i].f_frac >> HEIGHTBITS);

					if (top_w < top)
						top_w = top;

					if (bottom_w > bottom)
						bottom_w = bottom;

					// Polyobject-specific hack to fix plane leaking -Red
					if (ffloor[i].polyobj && top_w >= bottom_w)
					{
						ffloor[i].plane->top[rw_x] = 0xFFFF;
						ffloor[i].plane->bottom[rw_x] = 0x0000; // fix for sky plane drawing crashes - Monster Iestyn 25/05/18
					}
					else if (top_w <= bottom_w)
					{
						fftop = (INT16)top_w;
						ffbottom = (INT16)bottom_w;

						ffloor[i].plane->top[rw_x] = fftop;
						ffloor[i].plane->bottom[rw_x] = ffbottom;

						// Lactozilla: Cull part of the column by the 3D floor if it can't be seen
						// "top" is the height of the ceiling column
						if (fftop <= top+1 && R_FFloorCanClip(&ffloor[i]) && !curline->polyseg)
						{
							rw_ceilingmarked = true;
							ceilingclip[rw_x] = ffbottom;

							if (yl < ffbottom)
								yl = ffbottom;

							if (markceiling && ceilingplane)
								ceilingplane->bottom[rw_x] = top;

							if (rw_silhouette)
							{
								(*rw_silhouette) |= SIL_TOP;
								(*rw_tsilheight) = INT32_MIN;
							}
						}
					}
				}
			}
		}

		//SoM: Calculate offsets for Thick fake floors.
		// calculate texture offset
		angle = (rw_centerangle + xtoviewangle[rw_x])>>ANGLETOFINESHIFT;
		// Mask 4095 to guarantee this index is within bounds
		texturecolumn = rw_offset-FixedMul(FINETANGENT(angle & 4095),rw_distance);

		if (oldtexturecolumn != -1)
		{
			rw_bottomtexturemid += FixedMul(rw_bottomtextureslide,  oldtexturecolumn-texturecolumn);
			rw_midtexturemid    += FixedMul(rw_midtextureslide,     oldtexturecolumn-texturecolumn);
			rw_toptexturemid    += FixedMul(rw_toptextureslide,     oldtexturecolumn-texturecolumn);
			rw_midtextureback   += FixedMul(rw_midtexturebackslide, oldtexturecolumn-texturecolumn);
		}
		oldtexturecolumn = texturecolumn;

		texturecolumn >>= FRACBITS;

		// texturecolumn and lighting are independent of wall tiers
		if (segtextured)
		{
			// calculate lighting
			pindex = FixedMul(rw_scale, LIGHTRESOLUTIONFIX)>>LIGHTSCALESHIFT;

			if (pindex >=  MAXLIGHTSCALE)
				pindex = MAXLIGHTSCALE-1;

			dc->colormap = walllights[pindex];

			dc->x = rw_x;
			dc->iscale = 0xffffffffu / (unsigned)rw_scale;

			if (frontsector->extra_colormap)
				dc->colormap = frontsector->extra_colormap->colormap + (dc->colormap - colormaps);
		}

		if (dc->numlights)
		{
			lighttable_t **xwalllights;

			for (i = 0; i < dc->numlights; i++)
			{
				INT32 lightnum;
				lightnum = (dc->lightlist[i].lightlevel >> LIGHTSEGSHIFT);

				if (dc->lightlist[i].extra_colormap)
					;
				else if (P_ApplyLightOffset(lightnum, curline->frontsector))
					lightnum += curline->lightOffset;

				if (lightnum < 0)
					xwalllights = scalelight[0];
				else if (lightnum >= LIGHTLEVELS)
					xwalllights = scalelight[LIGHTLEVELS-1];
				else
					xwalllights = scalelight[lightnum];

				pindex = FixedMul(rw_scale, LIGHTRESOLUTIONFIX)>>LIGHTSCALESHIFT;

				if (pindex >=  MAXLIGHTSCALE)
					pindex = MAXLIGHTSCALE-1;

				if (dc->lightlist[i].extra_colormap)
					dc->lightlist[i].rcolormap = dc->lightlist[i].extra_colormap->colormap + (xwalllights[pindex] - colormaps);
				else
					dc->lightlist[i].rcolormap = xwalllights[pindex];

				R_SetColumnFunc(COLDRAWFUNC_SHADOWED);
			}
		}

		frontscale[rw_x] = rw_scale;

		const INT16 topclip = (yl >= 0) ? ((yl > viewheight) ? (INT16)viewheight : (INT16)((INT16)yl - 1)) : -1;
		const INT16 bottomclip = (yh < viewheight) ? ((yh < -1) ? -1 : (INT16)((INT16)yh + 1)) : (INT16)viewheight;

		// draw the wall tiers
		if (midtexture)
		{
			// single sided line
			if (yl <= yh && yh >= 0 && yl < viewheight)
			{
				R_DrawWallColumn(dc, yl, yh, rw_midtexturemid, texturecolumn, midtexture, (encoremap && !(curline->linedef->flags & ML_TFERLINE)));

				// dont draw anything more for this column, since
				// a midtexture blocks the view
				if (!rw_ceilingmarked)
					ceilingclip[rw_x] = (INT16)viewheight;
				if (!rw_floormarked)
					floorclip[rw_x] = -1;
			}
			else
			{
				// note: don't use min/max macros, since casting from INT32 to INT16 is involved here
				if (markceiling && (!rw_ceilingmarked))
					ceilingclip[rw_x] = topclip;
				if (markfloor && (!rw_floormarked))
					floorclip[rw_x] = bottomclip;
			}
		}
		else
		{
			// two sided line
			if (toptexture)
			{
				// top wall
				mid = pixhigh>>HEIGHTBITS;
				pixhigh += pixhighstep;

				if (mid >= floorclip[rw_x])
					mid = floorclip[rw_x]-1;

				if (mid >= yl) // back ceiling lower than front ceiling ?
				{
					if (yl >= viewheight) // entirely off bottom of screen
					{
						if (!rw_ceilingmarked)
							ceilingclip[rw_x] = (INT16)viewheight;
					}
					else if (mid >= 0) // safe to draw top texture
					{
						R_DrawWallColumn(dc, yl, mid, rw_toptexturemid, texturecolumn, toptexture, topremap);
						ceilingclip[rw_x] = (INT16)mid;
					}
					else if (!rw_ceilingmarked) // entirely off top of screen
						ceilingclip[rw_x] = -1;
				}
				else if (!rw_ceilingmarked)
					ceilingclip[rw_x] = topclip;
			}
			else if (markceiling && (!rw_ceilingmarked)) // no top wall
				ceilingclip[rw_x] = topclip;

			if (bottomtexture)
			{
				// bottom wall
				mid = (pixlow+HEIGHTUNIT-1)>>HEIGHTBITS;
				pixlow += pixlowstep;

				// no space above wall?
				if (mid <= ceilingclip[rw_x])
					mid = ceilingclip[rw_x]+1;

				if (mid <= yh) // back floor higher than front floor ?
				{
					if (yh < 0) // entirely off top of screen
					{
						if (!rw_floormarked)
							floorclip[rw_x] = -1;
					}
					else if (mid < viewheight) // safe to draw bottom texture
					{
						R_DrawWallColumn(dc, mid, yh, rw_bottomtexturemid, texturecolumn, bottomtexture, bottomremap);
						floorclip[rw_x] = (INT16)mid;
					}
					else if (!rw_floormarked)  // entirely off bottom of screen
						floorclip[rw_x] = (INT16)viewheight;
				}
				else if (!rw_floormarked)
					floorclip[rw_x] = bottomclip;
			}
			else if (markfloor && (!rw_floormarked)) // no bottom wall
				floorclip[rw_x] = bottomclip;
		}

		if ((markceiling || markfloor) && (floorclip[rw_x] <= ceilingclip[rw_x] + 1))
		{
			solidcol[rw_x] = 1;
			didsolidcol = true;
		}

		if (maskedtexture || numthicksides)
		{
			// save texturecol for backdrawing of masked mid texture
			maskedtexturecol[rw_x] = (INT16)texturecolumn;

			if (maskedtextureheight != NULL)
			{
				maskedtextureheight[rw_x] = (!!(curline->linedef->flags & ML_DONTPEGBOTTOM) ^ !!(curline->linedef->flags & ML_EFFECT3) ?
											std::max(rw_midtexturemid, rw_midtextureback) :
											std::min(rw_midtexturemid, rw_midtextureback));
			}
		}

		if (dc->numlights)
		{
			for (i = 0; i < dc->numlights; i++)
			{
				dc->lightlist[i].height += dc->lightlist[i].heightstep;

				if (dc->lightlist[i].flags & FF_CUTSOLIDS)
					dc->lightlist[i].botheight += dc->lightlist[i].botheightstep;
			}
		}

		for (i = 0; i < numffloors; i++)
		{
			if (curline->polyseg && (ffloor[i].polyobj != curline->polyseg))
				continue;

			ffloor[i].f_frac += ffloor[i].f_step;
		}

		for (i = 0; i < numbackffloors; i++)
		{
			if (curline->polyseg && (ffloor[i].polyobj != curline->polyseg))
				continue;

			ffloor[i].f_clip[rw_x] = ffloor[i].c_clip[rw_x] = (INT16)((ffloor[i].b_frac >> HEIGHTBITS) & 0xFFFF);
			ffloor[i].b_frac += ffloor[i].b_step;
		}

		rw_scale += rw_scalestep;
		topfrac += topstep;
		bottomfrac += bottomstep;
	}
}

// Uses precalculated seg->length
static INT64 R_CalcSegDist(seg_t* seg, INT64 x2, INT64 y2)
{
	if (!seg->linedef->dy)
		return llabs(y2 - seg->v1->y);
	else if (!seg->linedef->dx)
		return llabs(x2 - seg->v1->x);
	else
	{
		INT64 dx = (seg->v2->x)-(seg->v1->x);
		INT64 dy = (seg->v2->y)-(seg->v1->y);
		INT64 vdx = x2-(seg->v1->x);
		INT64 vdy = y2-(seg->v1->y);
		return ((dy*vdx)-(dx*vdy))/(seg->length);
	}
}

static size_t maxdrawsegs = 0;

static fixed_t *frontscaletable = NULL;
static fixed_t *maskedheighttable = NULL;

void R_AllocSegMemory(void)
{
	if (!maxdrawsegs)
		return;

	frontscaletable = static_cast<fixed_t*>(Z_Realloc(frontscaletable, sizeof(*frontscaletable) * (maxdrawsegs * viewwidth), PU_STATIC, NULL));
	maskedheighttable = static_cast<fixed_t*>(Z_Realloc(maskedheighttable, sizeof(*maskedheighttable) * (maxdrawsegs * viewwidth), PU_STATIC, NULL));

	drawseg_t *lastseg = drawsegs + maxdrawsegs;

	fixed_t *frontscale_p = frontscaletable;
	fixed_t *maskedheight_p = maskedheighttable;

	for (drawseg_t *ds = drawsegs; ds < lastseg; ds++)
	{
		ds->frontscale = frontscale_p;
		ds->maskedtextureheight = maskedheight_p;
		frontscale_p += viewwidth;
		maskedheight_p += viewwidth;
	}
}

//
// R_StoreWallRange
// A wall segment will be drawn
//  between start and stop pixels (inclusive).
//
void R_StoreWallRange(INT32 start, INT32 stop)
{
	fixed_t       hyp;
	fixed_t       sineval;
	angle_t       distangle, offsetangle;
	boolean longboi;
	INT32           lightnum;
	INT32           i, p;
	lightlist_t   *light;
	r_lightlist_t *rlight;
	INT32 range;
	vertex_t segleft, segright;
	fixed_t ceilingfrontslide, floorfrontslide, ceilingbackslide, floorbackslide;
	drawcolumndata_t dc = {};

	maskedtextureheight = NULL;
	//initialize segleft and segright
	memset(&segleft, 0x00, sizeof(segleft));
	memset(&segright, 0x00, sizeof(segright));

	if (ds_p == drawsegs+maxdrawsegs)
	{
		size_t curpos = curdrawsegs - drawsegs;
		size_t pos = ds_p - drawsegs;
		size_t newmax = maxdrawsegs ? maxdrawsegs*2 : 128;

		if (firstseg)
			firstseg = (drawseg_t *)(firstseg - drawsegs);

		drawsegs = static_cast<drawseg_t*>(Z_Realloc(drawsegs, newmax*sizeof (*drawsegs), PU_STATIC, NULL));

		ds_p = drawsegs + pos;

		maxdrawsegs = newmax;
		curdrawsegs = drawsegs + curpos;

		if (firstseg)
			firstseg = drawsegs + (size_t)firstseg;

		R_AllocSegMemory();
	}

	sidedef = curline->sidedef;
	linedef = curline->linedef;

	// calculate rw_distance for scale calculation
	rw_normalangle = curline->angle + ANGLE_90;
	offsetangle = abs((INT32)(rw_normalangle-rw_angle1));

	if (offsetangle > ANGLE_90)
		offsetangle = ANGLE_90;

	distangle = ANGLE_90 - offsetangle;
	sineval = FINESINE(distangle>>ANGLETOFINESHIFT);

	hyp = R_PointToDist(curline->v1->x, curline->v1->y);
	rw_distance = FixedMul(hyp, sineval);
	longboi = (hyp >= INT32_MAX);

	// big room fix
	if (longboi)
		rw_distance = (fixed_t)R_CalcSegDist(curline,viewx,viewy);

	ds_p->x1 = rw_x = start;
	ds_p->x2 = stop;
	ds_p->curline = curline;
	rw_stopx = stop+1;

	//SoM: Code to remove limits on openings.
	{
		size_t pos = lastopening - openings;
		size_t need = (rw_stopx - start)*4 + pos;
		if (need > maxopenings)
		{
			drawseg_t *ds;  //needed for fix from *cough* zdoom *cough*
			INT16 *oldopenings = openings;
			INT16 *oldlast = lastopening;

			do
				maxopenings = maxopenings ? maxopenings*2 : 16384;
			while (need > maxopenings);
			openings = static_cast<INT16*>(Z_Realloc(openings, maxopenings * sizeof (*openings), PU_STATIC, NULL));
			lastopening = openings + pos;

			if (oldopenings == NULL)
				return;

			// borrowed fix from *cough* zdoom *cough*
			// [RH] We also need to adjust the openings pointers that
			//    were already stored in drawsegs.
			for (ds = curdrawsegs; ds < ds_p; ds++)
			{
#define ADJUST(p) if (ds->p + ds->x1 >= oldopenings && ds->p + ds->x1 <= oldlast) ds->p = ds->p - oldopenings + openings;
				ADJUST(maskedtexturecol);
				ADJUST(sprtopclip);
				ADJUST(sprbottomclip);
				ADJUST(thicksidecol);
#undef ADJUST
			}
		}
	}  // end of code to remove limits on openings

	// calculate scale at both ends and step
	ds_p->scale1 = rw_scale = R_ScaleFromGlobalAngle(viewangle + xtoviewangle[start]);

	if (stop > start)
	{
		ds_p->scale2 = R_ScaleFromGlobalAngle(viewangle + xtoviewangle[stop]);
		range = stop-start;
	}
	else
	{
		// UNUSED: try to fix the stretched line bug
#if 0
		if (rw_distance < FRACUNIT/2)
		{
			fixed_t         tr_x,tr_y;
			fixed_t         gxt,gyt;
			CONS_Debug(DBG_RENDER, "TRYING TO FIX THE STRETCHED ETC\n");

			tr_x = curline->v1->x - viewx;
			tr_y = curline->v1->y - viewy;

			gxt = FixedMul(tr_x, viewcos);
			gyt = -FixedMul(tr_y, viewsin);
			ds_p->scale1 = FixedDiv(projection, gxt - gyt);
		}
#endif
		ds_p->scale2 = ds_p->scale1;
		range = 1;
	}

	ds_p->scalestep = rw_scalestep = (ds_p->scale2 - rw_scale) / (range);

	// calculate texture boundaries
	//  and decide if floor / ceiling marks are needed
	// Figure out map coordinates of where start and end are mapping to on seg, so we can clip right for slope bullshit
	if (frontsector->hasslope || (backsector && backsector->hasslope)) // Commenting this out for FOFslop. -Red
	{
		angle_t temp;
		double a1, b1, c1, a2, b2, c2, det; // 1 is the seg, 2 is the view angle vector...

		// left
		temp = xtoviewangle[start]+viewangle;

		a1 = FixedToDouble(curline->v2->y-curline->v1->y);
		b1 = FixedToDouble(curline->v1->x-curline->v2->x);
		c1 = a1*FixedToDouble(curline->v1->x) + b1*FixedToDouble(curline->v1->y);

		// Both lines can be written in slope-intercept form, so figure out line intersection

		// left
		a2 = -FixedToDouble(FINESINE(temp>>ANGLETOFINESHIFT));
		b2 = FixedToDouble(FINECOSINE(temp>>ANGLETOFINESHIFT));
		c2 = a2*FixedToDouble(viewx) + b2*FixedToDouble(viewy);

		det = a1*b2 - a2*b1;

		ds_p->leftpos.x = segleft.x = DoubleToFixed((b2*c1 - b1*c2)/det);
		ds_p->leftpos.y = segleft.y = DoubleToFixed((a1*c2 - a2*c1)/det);

		// right
		temp = xtoviewangle[stop]+viewangle;

		a2 = -FixedToDouble(FINESINE(temp>>ANGLETOFINESHIFT));
		b2 = FixedToDouble(FINECOSINE(temp>>ANGLETOFINESHIFT));
		c2 = a2*FixedToDouble(viewx) + b2*FixedToDouble(viewy);

		det = a1*b2 - a2*b1;

		ds_p->rightpos.x = segright.x = DoubleToFixed((b2*c1 - b1*c2)/det);
		ds_p->rightpos.y = segright.y = DoubleToFixed((a1*c2 - a2*c1)/det);
	}

#define SLOPEPARAMS(slope, end1, end2, normalheight) \
	end1 = P_GetZAt(slope,  segleft.x,  segleft.y, normalheight); \
	end2 = P_GetZAt(slope, segright.x, segright.y, normalheight);

	SLOPEPARAMS(frontsector->c_slope, worldtop,    worldtopslope,    frontsector->ceilingheight)
	SLOPEPARAMS(frontsector->f_slope, worldbottom, worldbottomslope, frontsector->floorheight)
	// subtract viewz from these to turn them into
	// positions relative to the camera's z position
	worldtop -= viewz;
	worldtopslope -= viewz;
	worldbottom -= viewz;
	worldbottomslope -= viewz;

	midtexture = toptexture = bottomtexture = maskedtexture = 0;
	midremap = topremap = bottomremap = false;
	ds_p->maskedtexturecol = NULL;
	ds_p->numthicksides = numthicksides = 0;
	ds_p->thicksidecol = NULL;
	ds_p->tsilheight = 0;

	numbackffloors = 0;

	for (i = 0; i < MAXFFLOORS; i++)
		ds_p->thicksides[i] = NULL;

	if (numffloors)
	{
		for (i = 0; i < numffloors; i++)
		{
			if (ffloor[i].polyobj && (!ds_p->curline->polyseg || ffloor[i].polyobj != ds_p->curline->polyseg))
				continue;

			ffloor[i].f_pos       = P_GetZAt(ffloor[i].slope, segleft .x, segleft .y, ffloor[i].height) - viewz;
			ffloor[i].f_pos_slope = P_GetZAt(ffloor[i].slope, segright.x, segright.y, ffloor[i].height) - viewz;
		}
	}

	// Set up texture Y offset slides for sloped walls
	rw_toptextureslide = rw_midtextureslide = rw_bottomtextureslide = 0;
	ceilingfrontslide = floorfrontslide = ceilingbackslide = floorbackslide = 0;

	angle_t lineangle = R_PointToAngle2(curline->v1->x, curline->v1->y, curline->v2->x, curline->v2->y);

	if (frontsector->f_slope)
		floorfrontslide = FixedMul(frontsector->f_slope->zdelta, FINECOSINE((lineangle-frontsector->f_slope->xydirection)>>ANGLETOFINESHIFT));

	if (frontsector->c_slope)
		ceilingfrontslide = FixedMul(frontsector->c_slope->zdelta, FINECOSINE((lineangle-frontsector->c_slope->xydirection)>>ANGLETOFINESHIFT));

	if (backsector && backsector->f_slope)
		floorbackslide = FixedMul(backsector->f_slope->zdelta, FINECOSINE((lineangle-backsector->f_slope->xydirection)>>ANGLETOFINESHIFT));

	if (backsector && backsector->c_slope)
		ceilingbackslide = FixedMul(backsector->c_slope->zdelta, FINECOSINE((lineangle-backsector->c_slope->xydirection)>>ANGLETOFINESHIFT));

	if (!backsector)
	{
		fixed_t texheight;

		// single sided line
		midtexture = R_GetTextureNum(sidedef->midtexture);
		midremap = (encoremap && !(curline->linedef->flags & ML_TFERLINE));
		texheight = textureheight[midtexture];

		// a single sided line is terminal, so it must mark ends
		markfloor = markceiling = true;

		if (linedef->flags & ML_EFFECT2)
		{
			if (linedef->flags & ML_DONTPEGBOTTOM)
				rw_midtexturemid = frontsector->floorheight + texheight - viewz;
			else
				rw_midtexturemid = frontsector->ceilingheight - viewz;
		}
		else if (linedef->flags & ML_DONTPEGBOTTOM)
		{
			rw_midtexturemid = worldbottom + texheight;
			rw_midtextureslide = floorfrontslide;
		}
		else
		{
			// top of texture at top
			rw_midtexturemid = worldtop;
			rw_midtextureslide = ceilingfrontslide;
		}
		rw_midtexturemid += sidedef->rowoffset;

		ds_p->silhouette = SIL_BOTH;
		ds_p->sprtopclip = screenheightarray;
		ds_p->sprbottomclip = negonearray;
		ds_p->bsilheight = INT32_MAX;
		ds_p->tsilheight = INT32_MIN;
	}
	else
	{
		// two sided line
		SLOPEPARAMS(backsector->c_slope, worldhigh, worldhighslope, backsector->ceilingheight)
		SLOPEPARAMS(backsector->f_slope, worldlow,  worldlowslope,  backsector->floorheight)
		worldhigh -= viewz;
		worldhighslope -= viewz;
		worldlow -= viewz;
		worldlowslope -= viewz;

		// hack to allow height changes in outdoor areas
		// This is what gets rid of the upper textures if there should be sky
		if (frontsector->ceilingpic == skyflatnum
			&& backsector->ceilingpic == skyflatnum)
		{
			worldtopslope = worldhighslope =
			worldtop = worldhigh;
		}

		ds_p->sprtopclip = ds_p->sprbottomclip = NULL;
		ds_p->silhouette = 0;

		if (worldbottomslope > worldlowslope || worldbottom > worldlow)
		{
			ds_p->silhouette = SIL_BOTTOM;
			if (P_GetSectorFloorZAt(backsector, viewx, viewy) > viewz)
				ds_p->bsilheight = INT32_MAX;
			else
				ds_p->bsilheight = (frontsector->f_slope ? INT32_MAX : frontsector->floorheight);
		}
		else if (P_GetSectorFloorZAt(backsector, viewx, viewy) > viewz)
		{
			ds_p->silhouette = SIL_BOTTOM;
			ds_p->bsilheight = INT32_MAX;
			// ds_p->sprbottomclip = negonearray;
		}

		if (worldtopslope < worldhighslope || worldtop < worldhigh)
		{
			ds_p->silhouette |= SIL_TOP;
			if (P_GetSectorCeilingZAt(backsector, viewx, viewy) < viewz)
				ds_p->tsilheight = INT32_MIN;
			else
				ds_p->tsilheight = (frontsector->c_slope ? INT32_MIN : frontsector->ceilingheight);
		}
		else if (P_GetSectorCeilingZAt(backsector, viewx, viewy) < viewz)
		{
			ds_p->silhouette |= SIL_TOP;
			ds_p->tsilheight = INT32_MIN;
		}

		if (viewsector != frontsector && viewsector != backsector)
		{
			//SoM: 3/25/2000: This code fixes an automap bug that didn't check
			// frontsector->ceiling and backsector->floor to see if a door was closed.
			// Without the following code, sprites get displayed behind closed doors.
			if (doorclosed || (worldhigh <= worldbottom && worldhighslope <= worldbottomslope))
			{
				ds_p->sprbottomclip = negonearray;
				ds_p->bsilheight = INT32_MAX;
				ds_p->silhouette |= SIL_BOTTOM;
			}
			if (doorclosed || (worldlow >= worldtop && worldlowslope >= worldtopslope))
			{                   // killough 1/17/98, 2/8/98
				ds_p->sprtopclip = screenheightarray;
				ds_p->tsilheight = INT32_MIN;
				ds_p->silhouette |= SIL_TOP;
			}
		}

		if (worldlow != worldbottom
			|| worldlowslope != worldbottomslope
			|| backsector->f_slope != frontsector->f_slope
			|| backsector->floorpic != frontsector->floorpic
			|| backsector->lightlevel != frontsector->lightlevel
			//SoM: 3/22/2000: Check floor x and y offsets.
			|| backsector->floor_xoffs != frontsector->floor_xoffs
			|| backsector->floor_yoffs != frontsector->floor_yoffs
			|| backsector->floorpic_angle != frontsector->floorpic_angle
			//SoM: 3/22/2000: Prevents bleeding.
			|| frontsector->heightsec != -1
			|| backsector->floorlightsec != frontsector->floorlightsec
			//SoM: 4/3/2000: Check for colormaps
			|| frontsector->extra_colormap != backsector->extra_colormap
			|| (frontsector->ffloors != backsector->ffloors && frontsector->tag != backsector->tag))
		{
			markfloor = true;
		}
		else
		{
			// same plane on both sides
			markfloor = false;
		}

		if (worldhigh != worldtop
			|| worldhighslope != worldtopslope
			|| backsector->c_slope != frontsector->c_slope
			|| backsector->ceilingpic != frontsector->ceilingpic
			|| backsector->lightlevel != frontsector->lightlevel
			//SoM: 3/22/2000: Check floor x and y offsets.
			|| backsector->ceiling_xoffs != frontsector->ceiling_xoffs
			|| backsector->ceiling_yoffs != frontsector->ceiling_yoffs
			|| backsector->ceilingpic_angle != frontsector->ceilingpic_angle
			//SoM: 3/22/2000: Prevents bleeding.
			|| (frontsector->heightsec != -1 && frontsector->ceilingpic != skyflatnum)
			|| backsector->ceilinglightsec != frontsector->ceilinglightsec
			//SoM: 4/3/2000: Check for colormaps
			|| frontsector->extra_colormap != backsector->extra_colormap
			|| (frontsector->ffloors != backsector->ffloors && frontsector->tag != backsector->tag))
		{
			markceiling = true;
		}
		else
		{
			// same plane on both sides
			markceiling = false;
		}

		if (backsector->ceilingheight <= frontsector->floorheight ||
			backsector->floorheight >= frontsector->ceilingheight)
		{
			// closed door
			markceiling = markfloor = true;
		}

		// check TOP TEXTURE
		if (worldhigh < worldtop || worldhighslope < worldtopslope)
		{
			fixed_t texheight;
			// top texture
			topremap = (encoremap && !(curline->linedef->flags & ML_TFERLINE));

			if ((linedef->flags & (ML_DONTPEGTOP) && (linedef->flags & ML_DONTPEGBOTTOM))
				&& linedef->sidenum[1] != 0xffff)
			{
				// Special case... use offsets from 2nd side but only if it has a texture.
				side_t *def = &sides[linedef->sidenum[1]];
				toptexture = R_GetTextureNum(def->toptexture);

				if (!toptexture) //Second side has no texture, use the first side's instead.
					toptexture = R_GetTextureNum(sidedef->toptexture);
				texheight = textureheight[toptexture];
			}
			else
			{
				toptexture = R_GetTextureNum(sidedef->toptexture);
				texheight = textureheight[toptexture];
			}

			if (!(linedef->flags & ML_EFFECT1)) // Ignore slopes for lower/upper textures unless flag is checked
			{
				if (linedef->flags & ML_DONTPEGTOP)
					rw_toptexturemid = frontsector->ceilingheight - viewz;
				else
					rw_toptexturemid = backsector->ceilingheight - viewz;
			}
			else if (linedef->flags & ML_DONTPEGTOP)
			{
				// top of texture at top
				rw_toptexturemid = worldtop;
				rw_toptextureslide = ceilingfrontslide;
			}
			else
			{
				rw_toptexturemid = worldhigh + texheight;
				rw_toptextureslide = ceilingbackslide;
			}
		}
		// check BOTTOM TEXTURE
		if (worldlow > worldbottom || worldlowslope > worldbottomslope) // Only if VISIBLE!!!
		{
			// bottom texture
			bottomtexture = R_GetTextureNum(sidedef->bottomtexture);
			bottomremap = (encoremap && !(curline->linedef->flags & ML_TFERLINE));

			if (!(linedef->flags & ML_EFFECT1)) // Ignore slopes for lower/upper textures unless flag is checked
			{
				if (linedef->flags & ML_DONTPEGBOTTOM)
					rw_bottomtexturemid = frontsector->floorheight - viewz;
				else
					rw_bottomtexturemid = backsector->floorheight - viewz;
			}
			else if (linedef->flags & ML_DONTPEGBOTTOM)
			{
				// bottom of texture at bottom
				// top of texture at top
				rw_bottomtexturemid = worldbottom;
				rw_bottomtextureslide = floorfrontslide;
			}
			else // top of texture at top
			{
				rw_bottomtexturemid = worldlow;
				rw_bottomtextureslide = floorbackslide;
			}
		}

		rw_toptexturemid += sidedef->rowoffset;
		rw_bottomtexturemid += sidedef->rowoffset;

		// allocate space for masked texture tables
		if (frontsector && backsector && frontsector->tag != backsector->tag && (backsector->ffloors || frontsector->ffloors))
		{
			ffloor_t *rover;
			ffloor_t *r2;
			fixed_t   lowcut, highcut;
			fixed_t lowcutslope, highcutslope;

			// Used for height comparisons and etc across FOFs and slopes
			fixed_t high1, highslope1, low1, lowslope1, high2, highslope2, low2, lowslope2;

			maskedtexture = true;

			ds_p->thicksidecol = maskedtexturecol = lastopening - rw_x;
			lastopening += rw_stopx - rw_x;

			lowcut       = std::max(worldbottom, worldlow) + viewz;
			highcut      = std::min(worldtop, worldhigh) + viewz;
			lowcutslope  = std::max(worldbottomslope, worldlowslope) + viewz;
			highcutslope = std::min(worldtopslope, worldhighslope) + viewz;

			if (frontsector->ffloors && backsector->ffloors)
			{
				i = 0;

				for (rover = backsector->ffloors; rover && i < MAXFFLOORS; rover = rover->next)
				{
					if (!(rover->flags & FF_RENDERSIDES) || !(rover->flags & FF_EXISTS))
						continue;
					if (rover->flags & FF_INVERTSIDES)
						continue;

					if (rover->norender == leveltime)
						continue;

					SLOPEPARAMS(*rover->t_slope, high1, highslope1, *rover->topheight)
					SLOPEPARAMS(*rover->b_slope, low1,  lowslope1,  *rover->bottomheight)

					if ((high1 < lowcut && highslope1 < lowcutslope) || (low1 > highcut && lowslope1 > highcutslope))
						continue;

					for (r2 = frontsector->ffloors; r2; r2 = r2->next)
					{
						if (!(r2->flags & FF_EXISTS) || !(r2->flags & FF_RENDERSIDES))
							continue;

						if (r2->norender == leveltime)
							continue;

						if (rover->flags & FF_EXTRA)
						{
							if (!(r2->flags & FF_CUTEXTRA))
								continue;

							if (r2->flags & FF_EXTRA && (r2->flags & (FF_TRANSLUCENT|FF_FOG)) != (rover->flags & (FF_TRANSLUCENT|FF_FOG)))
								continue;
						}
						else
						{
							if (!(r2->flags & FF_CUTSOLIDS))
								continue;
						}

						SLOPEPARAMS(*r2->t_slope, high2, highslope2, *r2->topheight)
						SLOPEPARAMS(*r2->b_slope, low2,  lowslope2,  *r2->bottomheight)

						if ((high2 < lowcut || highslope2 < lowcutslope) || (low2 > highcut || lowslope2 > highcutslope))
							continue;
						if ((high1 > high2 || highslope1 > highslope2) || (low1 < low2 || lowslope1 < lowslope2))
							continue;

						break;
					}

					if (r2)
						continue;

					ds_p->thicksides[i] = rover;
					i++;
				}

				for (rover = frontsector->ffloors; rover && i < MAXFFLOORS; rover = rover->next)
				{
					if (!(rover->flags & FF_RENDERSIDES) || !(rover->flags & FF_EXISTS))
						continue;
					if (!(rover->flags & FF_ALLSIDES))
						continue;

					if (rover->norender == leveltime)
						continue;

					SLOPEPARAMS(*rover->t_slope, high1, highslope1, *rover->topheight)
					SLOPEPARAMS(*rover->b_slope, low1,  lowslope1,  *rover->bottomheight)

					if ((high1 < lowcut && highslope1 < lowcutslope) || (low1 > highcut && lowslope1 > highcutslope))
						continue;

					for (r2 = backsector->ffloors; r2; r2 = r2->next)
					{
						if (!(r2->flags & FF_EXISTS) || !(r2->flags & FF_RENDERSIDES))
							continue;

						if (r2->norender == leveltime)
							continue;

						if (rover->flags & FF_EXTRA)
						{
							if (!(r2->flags & FF_CUTEXTRA))
								continue;

							if (r2->flags & FF_EXTRA && (r2->flags & (FF_TRANSLUCENT|FF_FOG)) != (rover->flags & (FF_TRANSLUCENT|FF_FOG)))
								continue;
						}
						else
						{
							if (!(r2->flags & FF_CUTSOLIDS))
								continue;
						}

						SLOPEPARAMS(*r2->t_slope, high2, highslope2, *r2->topheight)
						SLOPEPARAMS(*r2->b_slope, low2,  lowslope2,  *r2->bottomheight)
#undef SLOPEPARAMS
						if ((high2 < lowcut || highslope2 < lowcutslope) || (low2 > highcut || lowslope2 > highcutslope))
							continue;
						if ((high1 > high2 || highslope1 > highslope2) || (low1 < low2 || lowslope1 < lowslope2))
							continue;

						break;
					}
					if (r2)
						continue;

					ds_p->thicksides[i] = rover;
					i++;
				}
			}
			else if (backsector->ffloors)
			{
				for (rover = backsector->ffloors, i = 0; rover && i < MAXFFLOORS; rover = rover->next)
				{
					if (!(rover->flags & FF_RENDERSIDES) || !(rover->flags & FF_EXISTS) || rover->flags & FF_INVERTSIDES)
						continue;
					if (rover->norender == leveltime)
						continue;

					// Oy vey.
					if (      ((P_GetFFloorTopZAt   (rover, segleft .x, segleft .y)) <= worldbottom      + viewz
							&& (P_GetFFloorTopZAt   (rover, segright.x, segright.y)) <= worldbottomslope + viewz)
							||((P_GetFFloorBottomZAt(rover, segleft .x, segleft .y)) >= worldtop         + viewz
							&& (P_GetFFloorBottomZAt(rover, segright.x, segright.y)) >= worldtopslope    + viewz))
						continue;

					ds_p->thicksides[i] = rover;
					i++;
				}
			}
			else if (frontsector->ffloors)
			{
				for (rover = frontsector->ffloors, i = 0; rover && i < MAXFFLOORS; rover = rover->next)
				{
					if (!(rover->flags & FF_RENDERSIDES) || !(rover->flags & FF_EXISTS) || !(rover->flags & FF_ALLSIDES))
						continue;
					if (rover->norender == leveltime)
						continue;

					// Oy vey.
					if (      (P_GetFFloorTopZAt   (rover, segleft .x, segleft .y) <= worldbottom      + viewz
							&& P_GetFFloorTopZAt   (rover, segright.x, segright.y) <= worldbottomslope + viewz)
							||(P_GetFFloorBottomZAt(rover, segleft .x, segleft .y) >= worldtop         + viewz
							&& P_GetFFloorBottomZAt(rover, segright.x, segright.y) >= worldtopslope    + viewz))
						continue;

					if (      (P_GetFFloorTopZAt   (rover, segleft .x, segleft .y) <= worldlow       + viewz
							&& P_GetFFloorTopZAt   (rover, segright.x, segright.y) <= worldlowslope  + viewz)
							||(P_GetFFloorBottomZAt(rover, segleft .x, segleft .y) >= worldhigh      + viewz
							&& P_GetFFloorBottomZAt(rover, segright.x, segright.y) >= worldhighslope + viewz))
						continue;

					ds_p->thicksides[i] = rover;
					i++;
				}
			}

			ds_p->numthicksides = numthicksides = i;
		}

		if (sidedef->midtexture > 0 && sidedef->midtexture < numtextures)
		{
			// masked midtexture
			if (!ds_p->thicksidecol)
			{
				ds_p->maskedtexturecol = maskedtexturecol = lastopening - rw_x;
				lastopening += rw_stopx - rw_x;
			}
			else
				ds_p->maskedtexturecol = ds_p->thicksidecol;

			maskedtextureheight = ds_p->maskedtextureheight; // note to red, this == &(ds_p->maskedtextureheight[0])

			if (curline->polyseg) // use REAL front and back floors please, so midtexture rendering isn't mucked up
			{
				rw_midtextureslide = rw_midtexturebackslide = 0;

				if (!!(linedef->flags & ML_DONTPEGBOTTOM) ^ !!(linedef->flags & ML_EFFECT3))
					rw_midtexturemid = rw_midtextureback = std::max(curline->frontsector->floorheight, curline->backsector->floorheight) - viewz;
				else
					rw_midtexturemid = rw_midtextureback = std::min(curline->frontsector->ceilingheight, curline->backsector->ceilingheight) - viewz;
			}
			else
			{
				// Set midtexture starting height
				if (linedef->flags & ML_EFFECT2) // Ignore slopes when texturing
				{
					rw_midtextureslide = rw_midtexturebackslide = 0;

					if (!!(linedef->flags & ML_DONTPEGBOTTOM) ^ !!(linedef->flags & ML_EFFECT3))
						rw_midtexturemid = rw_midtextureback = std::max(frontsector->floorheight, backsector->floorheight) - viewz;
					else
						rw_midtexturemid = rw_midtextureback = std::min(frontsector->ceilingheight, backsector->ceilingheight) - viewz;

				}
				else if (!!(linedef->flags & ML_DONTPEGBOTTOM) ^ !!(linedef->flags & ML_EFFECT3))
				{
					rw_midtexturemid = worldbottom;
					rw_midtextureslide = floorfrontslide;
					rw_midtextureback = worldlow;
					rw_midtexturebackslide = floorbackslide;
				}
				else
				{
					rw_midtexturemid = worldtop;
					rw_midtextureslide = ceilingfrontslide;
					rw_midtextureback = worldhigh;
					rw_midtexturebackslide = ceilingbackslide;
				}
			}
			rw_midtexturemid += sidedef->rowoffset;
			rw_midtextureback += sidedef->rowoffset;

			maskedtexture = true;
		}
	}

	// calculate rw_offset (only needed for textured lines)
	segtextured = midtexture || toptexture || bottomtexture || maskedtexture || (numthicksides > 0);

	if (segtextured)
	{
		offsetangle = rw_normalangle-rw_angle1;

		if (offsetangle > ANGLE_180)
			offsetangle = -(signed)offsetangle;

		if (offsetangle > ANGLE_90)
			offsetangle = ANGLE_90;

		sineval = FINESINE(offsetangle>>ANGLETOFINESHIFT);
		rw_offset = FixedMul(hyp, sineval);

		// big room fix
		if (longboi)
		{
			INT64 dx = (curline->v2->x)-(curline->v1->x);
			INT64 dy = (curline->v2->y)-(curline->v1->y);
			INT64 vdx = viewx-(curline->v1->x);
			INT64 vdy = viewy-(curline->v1->y);
			rw_offset = ((dx*vdx-dy*vdy))/(curline->length);
		}

		if (rw_normalangle-rw_angle1 < ANGLE_180)
			rw_offset = -rw_offset;

		/// don't use texture offset for splats
		rw_offset2 = rw_offset + curline->offset;
		rw_offset += sidedef->textureoffset + curline->offset;
		rw_centerangle = ANGLE_90 + viewangle - rw_normalangle;

		// calculate light table
		//  use different light tables
		//  for horizontal / vertical / diagonal
		// OPTIMIZE: get rid of LIGHTSEGSHIFT globally
		lightnum = (frontsector->lightlevel >> LIGHTSEGSHIFT);

		if (P_ApplyLightOffset(lightnum, frontsector))
			lightnum += curline->lightOffset;

		if (lightnum < 0)
			walllights = scalelight[0];
		else if (lightnum >= LIGHTLEVELS)
			walllights = scalelight[LIGHTLEVELS - 1];
		else
			walllights = scalelight[lightnum];
	}

	// if a floor / ceiling plane is on the wrong side
	//  of the view plane, it is definitely invisible
	//  and doesn't need to be marked.
	if (frontsector->heightsec == -1)
	{
		if (P_GetSectorFloorZAt(frontsector, viewx, viewy) >= viewz)
		{
			// above view plane
			markfloor = false;
		}

		if (frontsector->ceilingpic != skyflatnum && P_GetSectorCeilingZAt(frontsector, viewx, viewy) <= viewz)
		{
			// below view plane
			markceiling = false;
		}
	}

	// calculate incremental stepping values for texture edges
	worldtop >>= 4;
	worldbottom >>= 4;
	worldtopslope >>= 4;
	worldbottomslope >>= 4;

	if (linedef->special == HORIZONSPECIAL) // HORIZON LINES
	{
		topstep = bottomstep = 0;
		topfrac = bottomfrac = (centeryfrac>>4);
		topfrac++; // Prevent 1px HOM
	}
	else
	{
		topstep = -FixedMul (rw_scalestep, worldtop);
		topfrac = (centeryfrac>>4) - FixedMul (worldtop, rw_scale);

		bottomstep = -FixedMul (rw_scalestep,worldbottom);
		bottomfrac = (centeryfrac>>4) - FixedMul (worldbottom, rw_scale);

		if (frontsector->c_slope)
		{
			fixed_t topfracend = (centeryfrac>>4) - FixedMul (worldtopslope, ds_p->scale2);
			topstep = (topfracend-topfrac)/(range);
		}
		if (frontsector->f_slope)
		{
			fixed_t bottomfracend = (centeryfrac>>4) - FixedMul (worldbottomslope, ds_p->scale2);
			bottomstep = (bottomfracend-bottomfrac)/(range);
		}
	}

	dc.numlights = 0;

	if (frontsector->numlights)
	{
		dc.numlights = frontsector->numlights;
		dc.maxlights = dc.numlights;

		if (dc.numlights >= dc.maxlights)
		{
			dc.maxlights = dc.numlights;
			dc.lightlist = static_cast<r_lightlist_s*>(Z_Realloc(dc.lightlist, sizeof (*dc.lightlist) * dc.maxlights, PU_STATIC, NULL));
		}

		for (i = p = 0; i < dc.numlights; i++)
		{
			fixed_t leftheight, rightheight;

			light = &frontsector->lightlist[i];
			rlight = &dc.lightlist[p];

			leftheight  = P_GetLightZAt(light,  segleft.x,  segleft.y);
			rightheight = P_GetLightZAt(light, segright.x, segright.y);

			// Flag sector as having slopes
			if (light->slope)
				frontsector->hasslope = true;

			leftheight -= viewz;
			rightheight -= viewz;

			leftheight >>= 4;
			rightheight >>= 4;

			if (i != 0)
			{
				if (leftheight < worldbottom && rightheight < worldbottomslope)
					continue;

				if (leftheight > worldtop && rightheight > worldtopslope && i+1 < dc.numlights && frontsector->lightlist[i+1].height > frontsector->ceilingheight)
					continue;
			}

			rlight->height = (centeryfrac>>4) - FixedMul(leftheight, rw_scale);
			rlight->heightstep = (centeryfrac>>4) - FixedMul(rightheight, ds_p->scale2);
			rlight->heightstep = (rlight->heightstep-rlight->height)/(range);
			rlight->flags = static_cast<ffloortype_e>(light->flags);

			if (light->caster && light->caster->flags & FF_CUTSOLIDS)
			{
				leftheight  = P_GetFFloorBottomZAt(light->caster,  segleft.x,  segleft.y);
				rightheight = P_GetFFloorBottomZAt(light->caster, segright.x, segright.y);

				// Flag sector as having slopes
				if (*light->caster->b_slope)
					frontsector->hasslope = true;

				leftheight -= viewz;
				rightheight -= viewz;

				leftheight >>= 4;
				rightheight >>= 4;

				rlight->botheight = (centeryfrac>>4) - FixedMul(leftheight, rw_scale);
				rlight->botheightstep = (centeryfrac>>4) - FixedMul(rightheight, ds_p->scale2);
				rlight->botheightstep = (rlight->botheightstep-rlight->botheight)/(range);

			}

			rlight->lightlevel = *light->lightlevel;
			rlight->extra_colormap = light->extra_colormap;
			p++;
		}

		dc.numlights = p;
	}

	if (numffloors)
	{
		for (i = 0; i < numffloors; i++)
		{
			ffloor[i].f_pos >>= 4;
			ffloor[i].f_pos_slope >>= 4;

			if (linedef->special == HORIZONSPECIAL) // Horizon lines extend FOFs in contact with them too.
			{
				ffloor[i].f_step = 0;
				ffloor[i].f_frac = (centeryfrac>>4);
				topfrac++; // Prevent 1px HOM
			}
			else
			{
				ffloor[i].f_frac = (centeryfrac>>4) - FixedMul(ffloor[i].f_pos, rw_scale);
				ffloor[i].f_step = ((centeryfrac>>4) - FixedMul(ffloor[i].f_pos_slope, ds_p->scale2) - ffloor[i].f_frac)/(range);
			}
		}
	}

	if (backsector)
	{
		worldhigh >>= 4;
		worldlow >>= 4;
		worldhighslope >>= 4;
		worldlowslope >>= 4;

		if (toptexture)
		{
			fixed_t topfracend = (centeryfrac>>4) - FixedMul (worldhighslope, ds_p->scale2);

			pixhigh = (centeryfrac>>4) - FixedMul (worldhigh, rw_scale);
			pixhighstep = (topfracend-pixhigh)/(range);

			// If the lowest part of a ceiling stretching down covers the entire screen
			if (std::min(pixhigh, topfracend)>>HEIGHTBITS >= viewheight-1)
				g_walloffscreen = true;
		}

		if (bottomtexture)
		{
			fixed_t bottomfracend = (centeryfrac>>4) - FixedMul (worldlowslope, ds_p->scale2);

			pixlow = (centeryfrac>>4) - FixedMul (worldlow, rw_scale);
			pixlowstep = (bottomfracend-pixlow)/(range);

			// If the highest part of a floor stretching up covers the entire screen
			if ((std::max(pixlow, bottomfracend)+HEIGHTUNIT-1)>>HEIGHTBITS <= 0)
				g_walloffscreen = true;
		}

		{
			ffloor_t * rover;
			fixed_t roverleft, roverright;
			fixed_t planevistest;
			i = 0;

			if (backsector->ffloors)
			{
				for (rover = backsector->ffloors; rover && i < MAXFFLOORS; rover = rover->next)
				{
					if (!(rover->flags & FF_EXISTS) || !(rover->flags & FF_RENDERPLANES))
						continue;
					if (rover->norender == leveltime)
						continue;

					// Let the renderer know this sector is sloped.
					if (*rover->b_slope || *rover->t_slope)
						backsector->hasslope = true;

					roverleft    = P_GetFFloorBottomZAt(rover, segleft .x, segleft .y) - viewz;
					roverright   = P_GetFFloorBottomZAt(rover, segright.x, segright.y) - viewz;
					planevistest = P_GetFFloorBottomZAt(rover, viewx, viewy);

					if ((roverleft>>4 <= worldhigh || roverright>>4 <= worldhighslope) &&
						(roverleft>>4 >= worldlow || roverright>>4 >= worldlowslope) &&
						((viewz < planevistest && !(rover->flags & FF_INVERTPLANES)) ||
						 (viewz > planevistest && (rover->flags & FF_BOTHPLANES))))
					{
						//ffloor[i].slope = *rover->b_slope;
						ffloor[i].b_pos = roverleft;
						ffloor[i].b_pos_slope = roverright;
						ffloor[i].b_pos >>= 4;
						ffloor[i].b_pos_slope >>= 4;
						ffloor[i].b_frac = (centeryfrac >> 4) - FixedMul(ffloor[i].b_pos, rw_scale);
						ffloor[i].b_step = (centeryfrac >> 4) - FixedMul(ffloor[i].b_pos_slope, ds_p->scale2);
						ffloor[i].b_step = (ffloor[i].b_step-ffloor[i].b_frac)/(range);
						i++;
					}

					if (i >= MAXFFLOORS)
						break;

					roverleft    = P_GetFFloorTopZAt(rover, segleft .x, segleft .y) - viewz;
					roverright   = P_GetFFloorTopZAt(rover, segright.x, segright.y) - viewz;
					planevistest = P_GetFFloorTopZAt(rover, viewx, viewy);

					if ((roverleft>>4 <= worldhigh || roverright>>4 <= worldhighslope) &&
						(roverleft>>4 >= worldlow || roverright>>4 >= worldlowslope) &&
						((viewz > planevistest && !(rover->flags & FF_INVERTPLANES)) ||
						 (viewz < planevistest && (rover->flags & FF_BOTHPLANES))))
					{
						//ffloor[i].slope = *rover->t_slope;
						ffloor[i].b_pos = roverleft;
						ffloor[i].b_pos_slope = roverright;
						ffloor[i].b_pos >>= 4;
						ffloor[i].b_pos_slope >>= 4;
						ffloor[i].b_frac = (centeryfrac >> 4) - FixedMul(ffloor[i].b_pos, rw_scale);
						ffloor[i].b_step = (centeryfrac >> 4) - FixedMul(ffloor[i].b_pos_slope, ds_p->scale2);
						ffloor[i].b_step = (ffloor[i].b_step-ffloor[i].b_frac)/(range);
						i++;
					}
				}
			}
			else if (frontsector && frontsector->ffloors)
			{
				for (rover = frontsector->ffloors; rover && i < MAXFFLOORS; rover = rover->next)
				{
					if (!(rover->flags & FF_EXISTS) || !(rover->flags & FF_RENDERPLANES))
						continue;
					if (rover->norender == leveltime)
						continue;

					// Let the renderer know this sector is sloped.
					if (*rover->b_slope || *rover->t_slope)
						frontsector->hasslope = true;

					roverleft    = P_GetFFloorBottomZAt(rover, segleft .x, segleft .y) - viewz;
					roverright   = P_GetFFloorBottomZAt(rover, segright.x, segright.y) - viewz;
					planevistest = P_GetFFloorBottomZAt(rover, viewx, viewy);

					if ((roverleft>>4 <= worldhigh || roverright>>4 <= worldhighslope) &&
						(roverleft>>4 >= worldlow || roverright>>4 >= worldlowslope) &&
						((viewz < planevistest && !(rover->flags & FF_INVERTPLANES)) ||
						 (viewz > planevistest && (rover->flags & FF_BOTHPLANES))))
					{
						//ffloor[i].slope = *rover->b_slope;
						ffloor[i].b_pos = roverleft;
						ffloor[i].b_pos_slope = roverright;
						ffloor[i].b_pos >>= 4;
						ffloor[i].b_pos_slope >>= 4;
						ffloor[i].b_frac = (centeryfrac >> 4) - FixedMul(ffloor[i].b_pos, rw_scale);
						ffloor[i].b_step = (centeryfrac >> 4) - FixedMul(ffloor[i].b_pos_slope, ds_p->scale2);
						ffloor[i].b_step = (ffloor[i].b_step-ffloor[i].b_frac)/(range);
						i++;
					}

					if (i >= MAXFFLOORS)
						break;

					roverleft    = P_GetFFloorTopZAt(rover, segleft .x, segleft .y) - viewz;
					roverright   = P_GetFFloorTopZAt(rover, segright.x, segright.y) - viewz;
					planevistest = P_GetFFloorTopZAt(rover, viewx, viewy);

					if ((roverleft>>4 <= worldhigh || roverright>>4 <= worldhighslope) &&
						(roverleft>>4 >= worldlow || roverright>>4 >= worldlowslope) &&
						((viewz > planevistest && !(rover->flags & FF_INVERTPLANES)) ||
						 (viewz < planevistest && (rover->flags & FF_BOTHPLANES))))
					{
						//ffloor[i].slope = *rover->t_slope;
						ffloor[i].b_pos = roverleft;
						ffloor[i].b_pos_slope = roverright;
						ffloor[i].b_pos >>= 4;
						ffloor[i].b_pos_slope >>= 4;
						ffloor[i].b_frac = (centeryfrac >> 4) - FixedMul(ffloor[i].b_pos, rw_scale);
						ffloor[i].b_step = (centeryfrac >> 4) - FixedMul(ffloor[i].b_pos_slope, ds_p->scale2);
						ffloor[i].b_step = (ffloor[i].b_step-ffloor[i].b_frac)/(range);
						i++;
					}
				}
			}

			if (curline->polyseg && frontsector && (curline->polyseg->flags & POF_RENDERPLANES))
			{
				while (i < numffloors && ffloor[i].polyobj != curline->polyseg)
					i++;

				if (i < numffloors && backsector->floorheight <= frontsector->ceilingheight &&
					backsector->floorheight >= frontsector->floorheight &&
					(viewz < backsector->floorheight))
				{
					if (ffloor[i].plane->minx > ds_p->x1)
						ffloor[i].plane->minx = ds_p->x1;

					if (ffloor[i].plane->maxx < ds_p->x2)
						ffloor[i].plane->maxx = ds_p->x2;

					ffloor[i].slope = NULL;
					ffloor[i].b_pos = backsector->floorheight;
					ffloor[i].b_pos = (ffloor[i].b_pos - viewz) >> 4;
					ffloor[i].b_step = FixedMul(-rw_scalestep, ffloor[i].b_pos);
					ffloor[i].b_frac = (centeryfrac >> 4) - FixedMul(ffloor[i].b_pos, rw_scale);
					i++;
				}

				if (i < numffloors && backsector->ceilingheight >= frontsector->floorheight &&
					backsector->ceilingheight <= frontsector->ceilingheight &&
					(viewz > backsector->ceilingheight))
				{
					if (ffloor[i].plane->minx > ds_p->x1)
						ffloor[i].plane->minx = ds_p->x1;

					if (ffloor[i].plane->maxx < ds_p->x2)
						ffloor[i].plane->maxx = ds_p->x2;

					ffloor[i].slope = NULL;
					ffloor[i].b_pos = backsector->ceilingheight;
					ffloor[i].b_pos = (ffloor[i].b_pos - viewz) >> 4;
					ffloor[i].b_step = FixedMul(-rw_scalestep, ffloor[i].b_pos);
					ffloor[i].b_frac = (centeryfrac >> 4) - FixedMul(ffloor[i].b_pos, rw_scale);
					i++;
				}
			}

			numbackffloors = i;
		}
	}

	// get a new or use the same visplane
	if (markceiling)
	{
		if (ceilingplane) //SoM: 3/29/2000: Check for null ceiling planes
			ceilingplane = R_CheckPlane (ceilingplane, rw_x, rw_stopx-1);
		else
			markceiling = false;

		// Don't mark ceiling flat lines for polys unless this line has an upper texture, otherwise we get flat leakage pulling downward
		// (If it DOES have an upper texture and we do this, the ceiling won't render at all)
		if (curline->polyseg && !curline->sidedef->toptexture)
			markceiling = false;
	}

	// get a new or use the same visplane
	if (markfloor)
	{
		if (floorplane) //SoM: 3/29/2000: Check for null planes
			floorplane = R_CheckPlane (floorplane, rw_x, rw_stopx-1);
		else
			markfloor = false;

		// Don't mark floor flat lines for polys unless this line has a lower texture, otherwise we get flat leakage pulling upward
		// (If it DOES have a lower texture and we do this, the floor won't render at all)
		if (curline->polyseg && !curline->sidedef->bottomtexture)
			markfloor = false;
	}

	ds_p->numffloorplanes = 0;
	if (numffloors)
	{
		if (!firstseg)
		{
			ds_p->numffloorplanes = numffloors;

			for (i = 0; i < numffloors; i++)
			{
				ds_p->ffloorplanes[i] = ffloor[i].plane =
					R_CheckPlane(ffloor[i].plane, rw_x, rw_stopx - 1);
			}

			firstseg = ds_p;
		}
		else
		{
			for (i = 0; i < numffloors; i++)
				R_ExpandPlane(ffloor[i].plane, rw_x, rw_stopx - 1);
		}

		// FIXME hack to fix planes disappearing when a seg goes behind the camera. This NEEDS to be changed to be done properly. -Red
		if (curline->polyseg)
		{
			for (i = 0; i < numffloors; i++)
			{
				if (!ffloor[i].polyobj || ffloor[i].polyobj != curline->polyseg)
					continue;
				if (ffloor[i].plane->minx > rw_x)
					ffloor[i].plane->minx = rw_x;

				if (ffloor[i].plane->maxx < rw_stopx - 1)
					ffloor[i].plane->maxx = rw_stopx - 1;
			}
		}

	}

	rw_silhouette = &(ds_p->silhouette);
	rw_tsilheight = &(ds_p->tsilheight);
	rw_bsilheight = &(ds_p->bsilheight);

	didsolidcol = false;

	R_RenderSegLoop(&dc);
	R_SetColumnFunc(BASEDRAWFUNC);

	if (g_portal) // if curline is a portal, set portalrender for drawseg
		ds_p->portalpass = portalrender+1;
	else
		ds_p->portalpass = 0;

	// cph - if a column was made solid by this wall, we _must_ save full clipping info
	if (backsector && didsolidcol)
	{
		if (!(ds_p->silhouette & SIL_BOTTOM))
		{
			ds_p->silhouette |= SIL_BOTTOM;
			ds_p->bsilheight = backsector->f_slope ? INT32_MAX : backsector->floorheight;
		}
		if (!(ds_p->silhouette & SIL_TOP))
		{
			ds_p->silhouette |= SIL_TOP;
			ds_p->tsilheight = backsector->c_slope ? INT32_MIN : backsector->ceilingheight;
		}
	}

	// save sprite clipping info
	if (((ds_p->silhouette & SIL_TOP) || maskedtexture) && !ds_p->sprtopclip)
	{
		M_Memcpy(lastopening, ceilingclip+start, 2*(rw_stopx - start));
		ds_p->sprtopclip = lastopening - start;
		lastopening += rw_stopx - start;
	}

	if (((ds_p->silhouette & SIL_BOTTOM) || maskedtexture) && !ds_p->sprbottomclip)
	{
		M_Memcpy(lastopening, floorclip + start, 2*(rw_stopx-start));
		ds_p->sprbottomclip = lastopening - start;
		lastopening += rw_stopx - start;
	}

	if (maskedtexture && !(ds_p->silhouette & SIL_TOP))
	{
		ds_p->silhouette |= SIL_TOP;
		ds_p->tsilheight = (sidedef->midtexture > 0 && sidedef->midtexture < numtextures) ? INT32_MIN: INT32_MAX;
	}
	if (maskedtexture && !(ds_p->silhouette & SIL_BOTTOM))
	{
		ds_p->silhouette |= SIL_BOTTOM;
		ds_p->bsilheight = (sidedef->midtexture > 0 && sidedef->midtexture < numtextures) ? INT32_MAX: INT32_MIN;
	}

	ds_p++;
}
