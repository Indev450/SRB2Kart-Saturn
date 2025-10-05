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
/// \file  r_draw_span.cpp
/// \brief span drawer functions
/// \note  no includes because this is included as part of r_draw.cpp

#ifdef HAVE_THREADS
#ifdef _WIN32
#include <windows.h>
#define local_for_thread static __thread
#else
#include <threads.h>
#define local_for_thread thread_local static
#endif
#else
#define local_for_thread static
#endif

#include <vector>
#include <algorithm>

// ==========================================================================
// SPANS
// ==========================================================================

#define SPANSIZE 16
#define INVSPAN 0.0625f

// <Callum> 4194303 = (2048x2048)-1 (2048x2048 is maximum flat size)
#define MAXFLATBYTES 4194303

#define PLANELIGHTFLOAT ((float)BASEVIDWIDTH * BASEVIDWIDTH / stride / ds->zeroheight / 21.0f * FIXED_TO_FLOAT(fovtan))

enum DrawSpanType
{
	DS_BASIC			= 0x0000,
	DS_COLORMAP			= 0x0001,
	DS_TRANSMAP			= 0x0002,
	DS_HOLES			= 0x0004,
	DS_RIPPLE			= 0x0008,
};

template<DrawSpanType Type>
static constexpr UINT8 R_GetSpanTranslated(drawspandata_t* ds, UINT8 col)
{
	if constexpr (Type & DrawSpanType::DS_COLORMAP)
	{
		return ds->translation[col];
	}
	else
	{
		return col;
	}
}

template<DrawSpanType Type>
static constexpr UINT8 R_GetSpanTranslucent(drawspandata_t* ds, UINT8 *dsrc, const UINT8 *colormap, UINT8 col)
{
	col = colormap[R_GetSpanTranslated<Type>(ds, col)];

	if constexpr (Type & DrawSpanType::DS_TRANSMAP)
	{
		return *(ds->transmap + (col << 8) + (*dsrc));
	}
	else
	{
		return col;
	}
}

template<DrawSpanType Type>
static constexpr UINT8 R_DrawSpanPixel(drawspandata_t* ds, UINT8 *dsrc, const UINT8 *colormap, UINT32 bit, const UINT8 *source)
{
	UINT8 col = source[bit];

	if constexpr (Type & DrawSpanType::DS_HOLES)
	{
		if (col == TRANSPARENTPIXEL)
		{
			return *dsrc;
		}
	}

	return R_GetSpanTranslucent<Type>(ds, dsrc, colormap, col);
}

/**	\brief The R_DrawSpan_8 function
	Draws the actual span.
*/
template<DrawSpanType Type>
static void R_DrawSpanTemplate(drawspandata_t* ds)
{
	UINT32 xposition;
	UINT32 yposition;
	UINT32 xstep, ystep;
	UINT32 bit;

	UINT8 * restrict dest = R_Address(ds->x1, ds->y);
	UINT8 * restrict dsrc;

	const UINT8 * restrict deststop = vid.screens[0] + vid.width * vid.height;

	if (dest+8 > deststop)
	{
		return;
	}

	intptr_t count = (ds->x2 - ds->x1 + 1);
	size_t i;

	xposition = ds->xfrac; yposition = ds->yfrac;
	xstep = ds->xstep; ystep = ds->ystep;

	const UINT8 * restrict source = ds->source;
	const UINT8 * restrict colormap = ds->colormap;

	if constexpr (Type & DS_RIPPLE)
	{
		yposition += ds->waterofs;
	}

	// SoM: we only need 6 bits for the integer part (0 thru 63) so the rest
	// can be used for the fraction part. This allows calculation of the memory address in the
	// texture with two shifts, an OR and one AND. (see below)
	// for texture sizes > 64 the amount of precision we can allow will decrease, but only by one
	// bit per power of two (obviously)
	// Ok, because I was able to eliminate the variable spot below, this function is now FASTER
	// than the original span renderer. Whodathunkit?
	xposition <<= ds->nflatshiftup; yposition <<= ds->nflatshiftup;
	xstep <<= ds->nflatshiftup; ystep <<= ds->nflatshiftup;

	if constexpr (Type & DS_RIPPLE)
	{
		dsrc = vid.screens[1] + (ds->y + ds->bgofs) * vid.width + ds->x1;
	}
	else
	{
		dsrc = dest;
	}

	while (count >= 8)
	{
		// SoM: Why didn't I see this earlier? the spot variable is a waste now because we don't
		// have the uber complicated math to calculate it now, so that was a memory write we didn't
		// need!

		for (i = 0; i < 8; i++)
		{
			bit = ((yposition >> ds->nflatyshift) & ds->nflatmask) | (xposition >> ds->nflatxshift);

			dest[i] = R_DrawSpanPixel<Type>(ds, &dsrc[i], colormap, bit, source);

			xposition += xstep;
			yposition += ystep;
		}

		dest += 8;
		dsrc += 8;

		count -= 8;
	}

	while (count-- && dest <= deststop)
	{
		bit = ((yposition >> ds->nflatyshift) & ds->nflatmask) | (xposition >> ds->nflatxshift);

		*dest = R_DrawSpanPixel<Type>(ds, dsrc, colormap, bit, source);

		dest++;
		dsrc++;

		xposition += xstep;
		yposition += ystep;
	}
}

// R_CalcTiltedLighting
// Exactly what it says on the tin. I wish I wasn't too lazy to explain things properly.
static void R_CalcTiltedLighting(std::vector<INT32>& lightbuffer, INT32 x1, INT32 x2, fixed_t start, fixed_t end)
{
	// ZDoom uses a different lighting setup to us, and I couldn't figure out how to adapt their version
	// of this function. Here's my own.
	INT32 i;
	const fixed_t step = (end-start)/(x2 - x1 + 1);

	// I wanna do some optimizing by checking for out-of-range segments on either side to fill in all at once,
	// but I'm too bad at coding to not crash the game trying to do that. I guess this is fast enough for now...

	for (i = x1; i <= x2; i++)
	{
		fixed_t light = start >> FRACBITS;
		lightbuffer[i] = CLAMP(light, 0, MAXLIGHTSCALE - 1);
		start += step;
	}
}

static void R_GetTiltedLighting(std::vector<INT32>& tiltlighting, const drawspandata_t* ds, const float iz, const int width, const INT32 stride)
{
	float planelightfloat = PLANELIGHTFLOAT;
	const fixed_t lightstart = FloatToFixed(iz * planelightfloat);
	const fixed_t lightend   = FloatToFixed((iz + ds->szp.x * width) * planelightfloat);

	if (tiltlighting.size() != (size_t)viewwidth)
	{
		tiltlighting.resize(viewwidth);
	}

	R_CalcTiltedLighting(tiltlighting, ds->x1, ds->x2, lightstart, lightend);
	//CONS_Printf("tilted lighting %f to %f (foc %f)\n", FixedToFloat(lightstart), FixedToFloat(lightend), focallengthf);
}

template<DrawSpanType Type>
static void R_DrawTiltedSpanTemplate(drawspandata_t* ds)
{
	int width = ds->x2 - ds->x1;
	float iz, uz, vz;
	UINT32 u, v;
	int i;

	UINT8 * restrict dest = R_Address(ds->x1, ds->y);
	UINT8 * restrict dsrc;

	float startz, startu, startv;
	float izstep, uzstep, vzstep;
	float endz, endu, endv;
	UINT32 stepu, stepv;
	UINT32 bit;

	INT32 x1 = ds->x1;
	const INT32 nflatxshift = ds->nflatxshift;
	const INT32 nflatyshift = ds->nflatyshift;
	const INT32 nflatmask = ds->nflatmask;
	const INT32 stride = vid.width;
	local_for_thread std::vector<INT32> tiltlighting;

	iz = ds->szp.z + ds->szp.y*(centery-ds->y) + ds->szp.x*(ds->x1-centerx);

	// Lighting is simple. It's just linear interpolation from start to end
	R_GetTiltedLighting(tiltlighting, ds, iz, width, stride);

	uz = ds->sup.z + ds->sup.y*(centery-ds->y) + ds->sup.x*(ds->x1-centerx);
	vz = ds->svp.z + ds->svp.y*(centery-ds->y) + ds->svp.x*(ds->x1-centerx);

	const UINT8 *source = ds->source;
	const UINT8 *colormap = ds->colormap;

	if constexpr (Type & DS_RIPPLE)
	{
		dsrc = vid.screens[1] + (ds->y + ds->bgofs) * stride + ds->x1;
	}
	else
	{
		dsrc = dest;
	}

#if 0	// The "perfect" reference version of this routine. Pretty slow.
		// Use it only to see how things are supposed to look.
	i = 0;
	do
	{
		double z = 1.f/iz;
		u = (INT64)(uz*z);
		v = (INT64)(vz*z);

		bit = ((v >> nflatyshift) & nflatmask) | (u >> nflatxshift);
		colormap = planezlight[tiltlighting[ds->x1]] + (ds->colormap - colormaps);

		*dest = R_DrawSpanPixel<Type>(ds, dsrc, colormap, bit);
		dest++;
		ds->x1++;
		dsrc++;
		iz += ds_szp->x;
		uz += ds_sup->x;
		vz += ds_svp->x;
	} while (--width >= 0);
#else
	startz = 1.f/iz;
	startu = uz*startz;
	startv = vz*startz;

	izstep = ds->szp.x * SPANSIZE;
	uzstep = ds->sup.x * SPANSIZE;
	vzstep = ds->svp.x * SPANSIZE;
	//x1 = 0;
	width++;

	while (width >= SPANSIZE)
	{
		iz += izstep;
		uz += uzstep;
		vz += vzstep;

		endz = 1.f/iz;
		endu = uz*endz;
		endv = vz*endz;
		stepu = (INT64)((endu - startu) * INVSPAN);
		stepv = (INT64)((endv - startv) * INVSPAN);
		u = (INT64)(startu);
		v = (INT64)(startv);

		x1 = ds->x1;

		for (i = 0; i < SPANSIZE; i++)
		{
			bit = (((v + stepv * i) >> nflatyshift) & nflatmask) | ((u + stepu * i) >> nflatxshift);
			colormap = ds->planezlight[tiltlighting[x1 + i]] + (ds->colormap - colormaps);
			dest[i] = R_DrawSpanPixel<Type>(ds, &dsrc[i], colormap, bit, source);
		}

		ds->x1 += SPANSIZE;
		dest += SPANSIZE;
		dsrc += SPANSIZE;
		startu = endu;
		startv = endv;
		width -= SPANSIZE;
	}

	if (width > 0)
	{
		if (width == 1)
		{
			u = (INT64)(startu);
			v = (INT64)(startv);
			bit = ((v >> nflatyshift) & nflatmask) | (u >> nflatxshift);
			colormap = ds->planezlight[tiltlighting[ds->x1]] + (ds->colormap - colormaps);
			*dest = R_DrawSpanPixel<Type>(ds, dsrc, colormap, bit, source);
			ds->x1++;
		}
		else
		{
			float left = width;
			iz += ds->szp.x * left;
			uz += ds->sup.x * left;
			vz += ds->svp.x * left;

			endz = 1.f/iz;
			endu = uz*endz;
			endv = vz*endz;
			left = 1.f/left;
			stepu = (INT64)((endu - startu) * left);
			stepv = (INT64)((endv - startv) * left);
			u = (INT64)(startu);
			v = (INT64)(startv);

			for (; width != 0; width--)
			{
				bit = ((v >> ds->nflatyshift) & ds->nflatmask) | (u >> ds->nflatxshift);
				colormap = ds->planezlight[tiltlighting[ds->x1]] + (ds->colormap - colormaps);
				*dest = R_DrawSpanPixel<Type>(ds, dsrc, colormap, bit, source);
				dest++;
				ds->x1++;
				dsrc++;
				u += stepu;
				v += stepv;
			}
		}
	}
#endif
}

#define DEFINE_SPAN_FUNC(name, flags, template) \
void name(drawspandata_t* ds) \
{ \
	constexpr DrawSpanType opt = static_cast<DrawSpanType>(flags); \
	template<opt>(ds); \
}

#define DEFINE_SPAN_COMBO(name, flags) \
DEFINE_SPAN_FUNC(name, flags, R_DrawSpanTemplate) \
DEFINE_SPAN_FUNC(name ## _Tilted, flags, R_DrawTiltedSpanTemplate) \

DEFINE_SPAN_COMBO(R_DrawSpan, DS_BASIC)
DEFINE_SPAN_COMBO(R_DrawTranslucentSpan, DS_TRANSMAP)
DEFINE_SPAN_COMBO(R_DrawSplat, DS_HOLES)
DEFINE_SPAN_COMBO(R_DrawTranslucentSplat, DS_TRANSMAP|DS_HOLES)
DEFINE_SPAN_COMBO(R_DrawTranslucentWaterSpan, DS_TRANSMAP|DS_RIPPLE)

/**	\brief The R_DrawFogSpan function
	Draws the actual span with fogging.
*/
void R_DrawFogSpan(drawspandata_t* ds)
{
	const UINT8 * restrict colormap = ds->colormap;
	UINT8 * restrict dest = R_Address(ds->x1, ds->y);

	intptr_t count = ds->x2 - ds->x1 + 1;

	while (count >= 4)
	{
		dest[0] = colormap[dest[0]];
		dest[1] = colormap[dest[1]];
		dest[2] = colormap[dest[2]];
		dest[3] = colormap[dest[3]];

		dest += 4;
		count -= 4;
	}

	while (count--)
	{
		*dest = colormap[*dest];
		dest++;
	}
}

void R_DrawFogSpan_Tilted(drawspandata_t* ds)
{
	int width = ds->x2 - ds->x1;
	float iz = ds->szp.z + ds->szp.y*(centery-ds->y) + ds->szp.x*(ds->x1-centerx);
	UINT8 * restrict dest;
	local_for_thread std::vector<INT32> tiltlighting;

	dest = R_Address(ds->x1, ds->y);
	const INT32 stride = vid.width;

	// Lighting is simple. It's just linear interpolation from start to end
	R_GetTiltedLighting(tiltlighting, ds, iz, width, stride);

	do
	{
		UINT8 *colormap = ds->planezlight[tiltlighting[ds->x1++]] + (ds->colormap - colormaps);
		*dest = colormap[*dest];
		dest++;
	}
	while (--width >= 0);
}
