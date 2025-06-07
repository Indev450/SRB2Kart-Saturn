// SONIC ROBO BLAST 2
//-----------------------------------------------------------------------------
// Copyright (C) 1998-2000 by DooM Legacy Team.
// Copyright (C) 1999-2018 by Sonic Team Junior.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  r_draw_span.c
/// \brief span drawer functions
/// \note  no includes because this is included as part of r_draw.c

// ==========================================================================
// SPANS
// ==========================================================================

#include <threads.h>

#define SPANSIZE 16
#define INVSPAN 0.0625f

// <Callum> 4194303 = (2048x2048)-1 (2048x2048 is maximum flat size)
#define MAXFLATBYTES 4194303

/**	\brief The R_DrawSpan function
	Draws the actual span.
*/
void R_DrawSpan(drawspandata_t* ds)
{
	uintptr_t xposition;
	uintptr_t yposition;
	uintptr_t xstep, ystep;
	register UINT32 bit;

	UINT8 *restrict source = ds->source;
	UINT8 *restrict colormap = ds->colormap;
	UINT8 *restrict dest = R_Address(ds->x1, ds->y);
	const UINT8 *restrict deststop = vid.screens[0] + vid.rowbytes * vid.height;

	register intptr_t count = (ds->x2 - ds->x1 + 1);

	xposition = ds->xfrac; yposition = ds->yfrac;
	xstep = ds->xstep; ystep = ds->ystep;

	// SoM: we only need 6 bits for the integer part (0 thru 63) so the rest
	// can be used for the fraction part. This allows calculation of the memory address in the
	// texture with two shifts, an OR and one AND. (see below)
	// for texture sizes > 64 the amount of precision we can allow will decrease, but only by one
	// bit per power of two (obviously)
	// Ok, because I was able to eliminate the variable spot below, this function is now FASTER
	// than the original span renderer. Whodathunkit?
	xposition <<= ds->nflatshiftup; yposition <<= ds->nflatshiftup;
	xstep <<= ds->nflatshiftup; ystep <<= ds->nflatshiftup;

	if (dest+8 > deststop)
	{
		return;
	}

	while (count >= 8)
	{
		// SoM: Why didn't I see this earlier? the spot variable is a waste now because we don't
		// have the uber complicated math to calculate it now, so that was a memory write we didn't
		// need!
		bit = (((UINT32)yposition >> ds->nflatyshift) & ds->nflatmask) | ((UINT32)xposition >> ds->nflatxshift);
		dest[0] = colormap[source[bit]];
		xposition += xstep;
		yposition += ystep;

		bit = (((UINT32)yposition >> ds->nflatyshift) & ds->nflatmask) | ((UINT32)xposition >> ds->nflatxshift);
		dest[1] = colormap[source[bit]];
		xposition += xstep;
		yposition += ystep;

		bit = (((UINT32)yposition >> ds->nflatyshift) & ds->nflatmask) | ((UINT32)xposition >> ds->nflatxshift);
		dest[2] = colormap[source[bit]];
		xposition += xstep;
		yposition += ystep;

		bit = (((UINT32)yposition >> ds->nflatyshift) & ds->nflatmask) | ((UINT32)xposition >> ds->nflatxshift);
		dest[3] = colormap[source[bit]];
		xposition += xstep;
		yposition += ystep;

		bit = (((UINT32)yposition >> ds->nflatyshift) & ds->nflatmask) | ((UINT32)xposition >> ds->nflatxshift);
		dest[4] = colormap[source[bit]];
		xposition += xstep;
		yposition += ystep;

		bit = (((UINT32)yposition >> ds->nflatyshift) & ds->nflatmask) | ((UINT32)xposition >> ds->nflatxshift);
		dest[5] = colormap[source[bit]];
		xposition += xstep;
		yposition += ystep;

		bit = (((UINT32)yposition >> ds->nflatyshift) & ds->nflatmask) | ((UINT32)xposition >> ds->nflatxshift);
		dest[6] = colormap[source[bit]];
		xposition += xstep;
		yposition += ystep;

		bit = (((UINT32)yposition >> ds->nflatyshift) & ds->nflatmask) | ((UINT32)xposition >> ds->nflatxshift);
		dest[7] = colormap[source[bit]];
		xposition += xstep;
		yposition += ystep;

		dest += 8;
		count -= 8;
	}

	while (count-- && dest <= deststop)
	{
		bit = (((UINT32)yposition >> ds->nflatyshift) & ds->nflatmask) | ((UINT32)xposition >> ds->nflatxshift);
		*dest++ = colormap[source[bit]];
		xposition += xstep;
		yposition += ystep;
	}
}

/**	\brief The R_DrawTranslucentSpan function
	Draws the actual span with translucent.
*/
void R_DrawTranslucentSpan(drawspandata_t* ds)
{
	fixed_t xposition;
	fixed_t yposition;
	fixed_t xstep, ystep;
	register UINT32 bit;

	UINT8 *source;
	UINT8 *colormap;
	register UINT8 *dest;
	const UINT8 *deststop = vid.screens[0] + vid.rowbytes * vid.height;

	register intptr_t count = (ds->x2 - ds->x1 + 1);

	xposition = ds->xfrac; yposition = ds->yfrac;
	xstep = ds->xstep; ystep = ds->ystep;

	// SoM: we only need 6 bits for the integer part (0 thru 63) so the rest
	// can be used for the fraction part. This allows calculation of the memory address in the
	// texture with two shifts, an OR and one AND. (see below)
	// for texture sizes > 64 the amount of precision we can allow will decrease, but only by one
	// bit per power of two (obviously)
	// Ok, because I was able to eliminate the variable spot below, this function is now FASTER
	// than the original span renderer. Whodathunkit?
	xposition <<= ds->nflatshiftup; yposition <<= ds->nflatshiftup;
	xstep <<= ds->nflatshiftup; ystep <<= ds->nflatshiftup;

	source = ds->source;
	colormap = ds->colormap;
	dest = R_Address(ds->x1, ds->y);

	register const UINT8 *tranmap = ds->transmap;

	while (count >= 8)
	{
		// SoM: Why didn't I see this earlier? the spot variable is a waste now because we don't
		// have the uber complicated math to calculate it now, so that was a memory write we didn't
		// need!
		bit = (((UINT32)yposition >> ds->nflatyshift) & ds->nflatmask) | ((UINT32)xposition >> ds->nflatxshift);
		dest[0] = *(tranmap + (colormap[source[bit]] << 8) + dest[0]);
		xposition += xstep;
		yposition += ystep;

		bit = (((UINT32)yposition >> ds->nflatyshift) & ds->nflatmask) | ((UINT32)xposition >> ds->nflatxshift);
		dest[1] = *(tranmap + (colormap[source[bit]] << 8) + dest[1]);
		xposition += xstep;
		yposition += ystep;

		bit = (((UINT32)yposition >> ds->nflatyshift) & ds->nflatmask) | ((UINT32)xposition >> ds->nflatxshift);
		dest[2] = *(tranmap + (colormap[source[bit]] << 8) + dest[2]);
		xposition += xstep;
		yposition += ystep;

		bit = (((UINT32)yposition >> ds->nflatyshift) & ds->nflatmask) | ((UINT32)xposition >> ds->nflatxshift);
		dest[3] = *(tranmap + (colormap[source[bit]] << 8) + dest[3]);
		xposition += xstep;
		yposition += ystep;

		bit = (((UINT32)yposition >> ds->nflatyshift) & ds->nflatmask) | ((UINT32)xposition >> ds->nflatxshift);
		dest[4] = *(tranmap + (colormap[source[bit]] << 8) + dest[4]);
		xposition += xstep;
		yposition += ystep;

		bit = (((UINT32)yposition >> ds->nflatyshift) & ds->nflatmask) | ((UINT32)xposition >> ds->nflatxshift);
		dest[5] = *(tranmap + (colormap[source[bit]] << 8) + dest[5]);
		xposition += xstep;
		yposition += ystep;

		bit = (((UINT32)yposition >> ds->nflatyshift) & ds->nflatmask) | ((UINT32)xposition >> ds->nflatxshift);
		dest[6] = *(tranmap + (colormap[source[bit]] << 8) + dest[6]);
		xposition += xstep;
		yposition += ystep;

		bit = (((UINT32)yposition >> ds->nflatyshift) & ds->nflatmask) | ((UINT32)xposition >> ds->nflatxshift);
		dest[7] = *(tranmap + (colormap[source[bit]] << 8) + dest[7]);
		xposition += xstep;
		yposition += ystep;

		dest += 8;
		count -= 8;
	}
	while (count-- && dest <= deststop)
	{
		bit = (((UINT32)yposition >> ds->nflatyshift) & ds->nflatmask) | ((UINT32)xposition >> ds->nflatxshift);
		*dest = *(tranmap + (colormap[source[bit]] << 8) + *dest);
		dest++;
		xposition += xstep;
		yposition += ystep;
	}
}

void R_DrawTranslucentWaterSpan(drawspandata_t* ds)
{
	UINT32 xposition;
	UINT32 yposition;
	UINT32 xstep, ystep;
	register UINT32 bit;

	UINT8 *source;
	UINT8 *colormap;
	register UINT8 *dest;
	UINT8 *dsrc;

	register intptr_t count;

	// SoM: we only need 6 bits for the integer part (0 thru 63) so the rest
	// can be used for the fraction part. This allows calculation of the memory address in the
	// texture with two shifts, an OR and one AND. (see below)
	// for texture sizes > 64 the amount of precision we can allow will decrease, but only by one
	// bit per power of two (obviously)
	// Ok, because I was able to eliminate the variable spot below, this function is now FASTER
	// than the original span renderer. Whodathunkit?
	xposition = ds->xfrac << ds->nflatshiftup; yposition = (ds->yfrac + ds->waterofs) << ds->nflatshiftup;
	xstep = ds->xstep << ds->nflatshiftup; ystep = ds->ystep << ds->nflatshiftup;

	source = ds->source;
	colormap = ds->colormap;
	dest = R_Address(ds->x1, ds->y);
	dsrc = vid.screens[1] + (ds->y+ds->bgofs)*vid.width + ds->x1;
	count = ds->x2 - ds->x1 + 1;

	register const UINT8 *tranmap = ds->transmap;

	while (count >= 8)
	{
		// SoM: Why didn't I see this earlier? the spot variable is a waste now because we don't
		// have the uber complicated math to calculate it now, so that was a memory write we didn't
		// need!
		bit = ((yposition >> ds->nflatyshift) & ds->nflatmask) | (xposition >> ds->nflatxshift);
		dest[0] = colormap[*(tranmap + (source[bit] << 8) + *dsrc++)];
		xposition += xstep;
		yposition += ystep;

		bit = ((yposition >> ds->nflatyshift) & ds->nflatmask) | (xposition >> ds->nflatxshift);
		dest[1] = colormap[*(tranmap + (source[bit] << 8) + *dsrc++)];
		xposition += xstep;
		yposition += ystep;

		bit = ((yposition >> ds->nflatyshift) & ds->nflatmask) | (xposition >> ds->nflatxshift);
		dest[2] = colormap[*(tranmap + (source[bit] << 8) + *dsrc++)];
		xposition += xstep;
		yposition += ystep;

		bit = ((yposition >> ds->nflatyshift) & ds->nflatmask) | (xposition >> ds->nflatxshift);
		dest[3] = colormap[*(tranmap + (source[bit] << 8) + *dsrc++)];
		xposition += xstep;
		yposition += ystep;

		bit = ((yposition >> ds->nflatyshift) & ds->nflatmask) | (xposition >> ds->nflatxshift);
		dest[4] = colormap[*(tranmap + (source[bit] << 8) + *dsrc++)];
		xposition += xstep;
		yposition += ystep;

		bit = ((yposition >> ds->nflatyshift) & ds->nflatmask) | (xposition >> ds->nflatxshift);
		dest[5] = colormap[*(tranmap + (source[bit] << 8) + *dsrc++)];
		xposition += xstep;
		yposition += ystep;

		bit = ((yposition >> ds->nflatyshift) & ds->nflatmask) | (xposition >> ds->nflatxshift);
		dest[6] = colormap[*(tranmap + (source[bit] << 8) + *dsrc++)];
		xposition += xstep;
		yposition += ystep;

		bit = ((yposition >> ds->nflatyshift) & ds->nflatmask) | (xposition >> ds->nflatxshift);
		dest[7] = colormap[*(tranmap + (source[bit] << 8) + *dsrc++)];
		xposition += xstep;
		yposition += ystep;

		dest += 8;
		count -= 8;
	}
	while (count--)
	{
		bit = ((yposition >> ds->nflatyshift) & ds->nflatmask) | (xposition >> ds->nflatxshift);
		*dest++ = colormap[*(tranmap + (source[bit] << 8) + *dsrc++)];

		xposition += xstep;
		yposition += ystep;
	}
}

/**	\brief The R_DrawFogSpan function
	Draws the actual span with fogging.
*/
void R_DrawFogSpan(drawspandata_t* ds)
{
	UINT8 *colormap;
	register UINT8 *dest;

	register intptr_t count;

	colormap = ds->colormap;
	dest = R_Address(ds->x1, ds->y);

	count = ds->x2 - ds->x1 + 1;

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

// R_CalcTiltedLighting
// Exactly what it says on the tin. I wish I wasn't too lazy to explain things properly.
static void R_CalcTiltedLighting(INT32 *lightbuffer, INT32 x1, INT32 x2, fixed_t start, fixed_t end)
{
	// ZDoom uses a different lighting setup to us, and I couldn't figure out how to adapt their version
	// of this function. Here's my own.
	INT32 left = x1, right = x2;
	fixed_t step = (end-start)/(x2 - x1 + 1);
	INT32 i;

	// I wanna do some optimizing by checking for out-of-range segments on either side to fill in all at once,
	// but I'm too bad at coding to not crash the game trying to do that. I guess this is fast enough for now...

	for (i = left; i <= right; i++)
	{
		lightbuffer[i] = (start += step) >> FRACBITS;

		if (lightbuffer[i] < 0)
		{
			lightbuffer[i] = 0;
		}
		else if (lightbuffer[i] >= MAXLIGHTSCALE)
		{
			lightbuffer[i] = MAXLIGHTSCALE-1;
		}
	}
}

#define PLANELIGHTFLOAT ((float)BASEVIDWIDTH * BASEVIDWIDTH / vid.width / ds->zeroheight / 21.0f * FIXED_TO_FLOAT(fovtan))

/**	\brief The R_DrawTiltedSpan function
	Draw slopes! Holy sheit!
*/
void R_DrawSpan_Tilted(drawspandata_t* ds)
{
	int width = ds->x2 - ds->x1;
	float iz, uz, vz;
	UINT32 u, v;
	int i;

	UINT8 *source;
	UINT8 *colormap;
	register UINT8 *dest;

	float startz, startu, startv;
	float izstep, uzstep, vzstep;
	float endz, endu, endv;
	UINT32 stepu, stepv;
	register UINT32 bit;
	thread_local static INT32 *tiltlighting = NULL;
	thread_local static INT32 oldviewwidth = 0;

	// dont realloc every frame pls thx
	if (tiltlighting == NULL || oldviewwidth != viewwidth)
	{
		tiltlighting = Z_Realloc(tiltlighting, sizeof(*tiltlighting) * viewwidth, PU_STATIC, NULL);
		oldviewwidth = viewwidth;
	}

	iz = ds->szp.z + ds->szp.y*(centery-ds->y) + ds->szp.x*(ds->x1-centerx);

	// Lighting is simple. It's just linear interpolation from start to end
	{
		float planelightfloat = PLANELIGHTFLOAT;
		float lightstart, lightend;

		lightend = (iz + ds->szp.x*width) * planelightfloat;
		lightstart = iz * planelightfloat;

		R_CalcTiltedLighting(tiltlighting, ds->x1, ds->x2, FLOAT_TO_FIXED(lightstart), FLOAT_TO_FIXED(lightend));
		//CONS_Printf("tilted lighting %f to %f (foc %f)\n", lightstart, lightend, focallengthf);
	}

	uz = ds->sup.z + ds->sup.y*(centery-ds->y) + ds->sup.x*(ds->x1-centerx);
	vz = ds->svp.z + ds->svp.y*(centery-ds->y) + ds->svp.x*(ds->x1-centerx);

	dest = R_Address(ds->x1, ds->y);

	source = ds->source;

#if 0	// The "perfect" reference version of this routine. Pretty slow.
		// Use it only to see how things are supposed to look.
	i = 0;
	do
	{
		double z = 1.f/iz;
		u = (INT64)(uz*z);
		v = (INT64)(vz*z);

		bit = ((v >> ds->nflatyshift) & ds->nflatmask) | (u >> ds->nflatxshift);
		colormap = ds->planezlight[tiltlighting[ds->x1++]] + (ds->colormap - colormaps);
		*dest = colormap[source[bit]];

		dest++;
		iz += ds->szp.x;
		uz += ds->sup.x;
		vz += ds->svp.x;
	} while (--width >= 0);
#else
	startz = 1.f/iz;
	startu = uz*startz;
	startv = vz*startz;

	izstep = ds->szp.x * SPANSIZE;
	uzstep = ds->sup.x * SPANSIZE;
	vzstep = ds->svp.x * SPANSIZE;
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

		for (i = SPANSIZE-1; i >= 0; i--)
		{
			bit = ((v >> ds->nflatyshift) & ds->nflatmask) | (u >> ds->nflatxshift);
			colormap = ds->planezlight[tiltlighting[ds->x1++]] + (ds->colormap - colormaps);
			*dest = colormap[source[bit]];

			dest++;
			u += stepu;
			v += stepv;
		}
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
			bit = ((v >> ds->nflatyshift) & ds->nflatmask) | (u >> ds->nflatxshift);
			colormap = ds->planezlight[tiltlighting[ds->x1++]] + (ds->colormap - colormaps);
			*dest = colormap[source[bit]];
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
				colormap = ds->planezlight[tiltlighting[ds->x1++]] + (ds->colormap - colormaps);
				*dest = colormap[source[bit]];

				dest++;
				u += stepu;
				v += stepv;
			}
		}
	}
#endif
}

/**	\brief The R_DrawTiltedTranslucentSpan function
	Like DrawTiltedSpan, but translucent
*/
void R_DrawTranslucentSpan_Tilted(drawspandata_t* ds)
{
	int width = ds->x2 - ds->x1;
	float iz, uz, vz;
	UINT32 u, v;
	int i;

	UINT8 *source;
	UINT8 *colormap;
	register UINT8 *dest;

	float startz, startu, startv;
	float izstep, uzstep, vzstep;
	float endz, endu, endv;
	UINT32 stepu, stepv;
	register UINT32 bit;
	thread_local static INT32 *tiltlighting = NULL;
	thread_local static INT32 oldviewwidth = 0;

	// dont realloc every frame pls thx
	if (tiltlighting == NULL || oldviewwidth != viewwidth)
	{
		tiltlighting = Z_Realloc(tiltlighting, sizeof(*tiltlighting) * viewwidth, PU_STATIC, NULL);
		oldviewwidth = viewwidth;
	}

	iz = ds->szp.z + ds->szp.y*(centery-ds->y) + ds->szp.x*(ds->x1-centerx);

	// Lighting is simple. It's just linear interpolation from start to end
	{
		float planelightfloat = PLANELIGHTFLOAT;
		float lightstart, lightend;

		lightend = (iz + ds->szp.x*width) * planelightfloat;
		lightstart = iz * planelightfloat;

		R_CalcTiltedLighting(tiltlighting, ds->x1, ds->x2, FLOAT_TO_FIXED(lightstart), FLOAT_TO_FIXED(lightend));
		//CONS_Printf("tilted lighting %f to %f (foc %f)\n", lightstart, lightend, focallengthf);
	}

	uz = ds->sup.z + ds->sup.y*(centery-ds->y) + ds->sup.x*(ds->x1-centerx);
	vz = ds->svp.z + ds->svp.y*(centery-ds->y) + ds->svp.x*(ds->x1-centerx);

	dest = R_Address(ds->x1, ds->y);

	source = ds->source;

#if 0	// The "perfect" reference version of this routine. Pretty slow.
		// Use it only to see how things are supposed to look.
	i = 0;
	do
	{
		double z = 1.f/iz;
		u = (INT64)(uz*z);
		v = (INT64)(vz*z);

		bit = ((v >> ds->nflatyshift) & ds->nflatmask) | (u >> ds->nflatxshift);
		colormap = ds->planezlight[tiltlighting[ds->x1++]] + (ds->colormap - colormaps);
		*dest = *(ds->transmap + (colormap[source[bit]] << 8) + *dest);

		dest++;
		iz += ds->szp.x;
		uz += ds->sup.x;
		vz += ds->svp.x;
	} while (--width >= 0);
#else
	startz = 1.f/iz;
	startu = uz*startz;
	startv = vz*startz;

	izstep = ds->szp.x * SPANSIZE;
	uzstep = ds->sup.x * SPANSIZE;
	vzstep = ds->svp.x * SPANSIZE;
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

		for (i = SPANSIZE-1; i >= 0; i--)
		{
			bit = ((v >> ds->nflatyshift) & ds->nflatmask) | (u >> ds->nflatxshift);
			colormap = ds->planezlight[tiltlighting[ds->x1++]] + (ds->colormap - colormaps);
			*dest = *(ds->transmap + (colormap[source[bit]] << 8) + *dest);

			dest++;
			u += stepu;
			v += stepv;
		}
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
			bit = ((v >> ds->nflatyshift) & ds->nflatmask) | (u >> ds->nflatxshift);
			colormap = ds->planezlight[tiltlighting[ds->x1++]] + (ds->colormap - colormaps);
			*dest = *(ds->transmap + (colormap[source[bit]] << 8) + *dest);
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
				colormap = ds->planezlight[tiltlighting[ds->x1++]] + (ds->colormap - colormaps);
				*dest = *(ds->transmap + (colormap[source[bit]] << 8) + *dest);

				dest++;
				u += stepu;
				v += stepv;
			}
		}
	}
#endif
}

/**	\brief The R_DrawTiltedTranslucentWaterSpan function
	Like DrawTiltedTranslucentSpan, but for water
*/
void R_DrawTranslucentWaterSpan_Tilted(drawspandata_t* ds)
{
	int width = ds->x2 - ds->x1;
	float iz, uz, vz;
	UINT32 u, v;
	int i;

	UINT8 *source;
	UINT8 *colormap;
	register UINT8 *dest;
	UINT8 *dsrc;

	float startz, startu, startv;
	float izstep, uzstep, vzstep;
	float endz, endu, endv;
	UINT32 stepu, stepv;
	register UINT32 bit;
	thread_local static INT32 *tiltlighting = NULL;
	thread_local static INT32 oldviewwidth = 0;

	// dont realloc every frame pls thx
	if (tiltlighting == NULL || oldviewwidth != viewwidth)
	{
		tiltlighting = Z_Realloc(tiltlighting, sizeof(*tiltlighting) * viewwidth, PU_STATIC, NULL);
		oldviewwidth = viewwidth;
	}

	iz = ds->szp.z + ds->szp.y*(centery-ds->y) + ds->szp.x*(ds->x1-centerx);

	// Lighting is simple. It's just linear interpolation from start to end
	{
		float planelightfloat = PLANELIGHTFLOAT;
		float lightstart, lightend;

		lightend = (iz + ds->szp.x*width) * planelightfloat;
		lightstart = iz * planelightfloat;

		R_CalcTiltedLighting(tiltlighting, ds->x1, ds->x2, FLOAT_TO_FIXED(lightstart), FLOAT_TO_FIXED(lightend));
		//CONS_Printf("tilted lighting %f to %f (foc %f)\n", lightstart, lightend, focallengthf);
	}

	uz = ds->sup.z + ds->sup.y*(centery-ds->y) + ds->sup.x*(ds->x1-centerx);
	vz = ds->svp.z + ds->svp.y*(centery-ds->y) + ds->svp.x*(ds->x1-centerx);

	dest = R_Address(ds->x1, ds->y);
	dsrc = vid.screens[1] + (ds->y+ds->bgofs)*vid.width + ds->x1;

	source = ds->source;

#if 0	// The "perfect" reference version of this routine. Pretty slow.
		// Use it only to see how things are supposed to look.
	i = 0;
	do
	{
		double z = 1.f/iz;
		u = (INT64)(uz*z);
		v = (INT64)(vz*z);

		bit = ((v >> ds->nflatyshift) & ds->nflatmask) | (u >> ds->nflatxshift);
		colormap = ds->planezlight[tiltlighting[ds->x1++]] + (ds->colormap - colormaps);
		*dest = *(ds->transmap + (colormap[source[bit]] << 8) + *dsrc++);

		dest++;
		iz += ds->szp.x;
		uz += ds->sup.x;
		vz += ds->svp.x;
	} while (--width >= 0);
#else
	startz = 1.f/iz;
	startu = uz*startz;
	startv = vz*startz;

	izstep = ds->szp.x * SPANSIZE;
	uzstep = ds->sup.x * SPANSIZE;
	vzstep = ds->svp.x * SPANSIZE;
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

		for (i = SPANSIZE-1; i >= 0; i--)
		{
			bit = ((v >> ds->nflatyshift) & ds->nflatmask) | (u >> ds->nflatxshift);
			colormap = ds->planezlight[tiltlighting[ds->x1++]] + (ds->colormap - colormaps);
			*dest = *(ds->transmap + (colormap[source[bit]] << 8) + *dsrc++);

			dest++;
			u += stepu;
			v += stepv;
		}
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
			bit = ((v >> ds->nflatyshift) & ds->nflatmask) | (u >> ds->nflatxshift);
			colormap = ds->planezlight[tiltlighting[ds->x1++]] + (ds->colormap - colormaps);
			*dest = *(ds->transmap + (colormap[source[bit]] << 8) + *dsrc++);
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
				colormap = ds->planezlight[tiltlighting[ds->x1++]] + (ds->colormap - colormaps);
				*dest = *(ds->transmap + (colormap[source[bit]] << 8) + *dsrc++);

				dest++;
				u += stepu;
				v += stepv;
			}
		}
	}
#endif
}

void R_DrawSplat_Tilted(drawspandata_t* ds)
{
	// x1, x2 = ds->x1, ds->x2
	int width = ds->x2 - ds->x1;
	float iz, uz, vz;
	UINT32 u, v;
	int i;

	UINT8 *source;
	UINT8 *colormap;
	register UINT8 *dest;

	UINT8 val;

	float startz, startu, startv;
	float izstep, uzstep, vzstep;
	float endz, endu, endv;
	UINT32 stepu, stepv;
	register UINT32 bit;
	thread_local static INT32 *tiltlighting = NULL;
	thread_local static INT32 oldviewwidth = 0;

	// dont realloc every frame pls thx
	if (tiltlighting == NULL || oldviewwidth != viewwidth)
	{
		tiltlighting = Z_Realloc(tiltlighting, sizeof(*tiltlighting) * viewwidth, PU_STATIC, NULL);
		oldviewwidth = viewwidth;
	}

	iz = ds->szp.z + ds->szp.y*(centery-ds->y) + ds->szp.x*(ds->x1-centerx);

	// Lighting is simple. It's just linear interpolation from start to end
	{
		float planelightfloat = PLANELIGHTFLOAT;
		float lightstart, lightend;

		lightend = (iz + ds->szp.x*width) * planelightfloat;
		lightstart = iz * planelightfloat;

		R_CalcTiltedLighting(tiltlighting, ds->x1, ds->x2, FLOAT_TO_FIXED(lightstart), FLOAT_TO_FIXED(lightend));
		//CONS_Printf("tilted lighting %f to %f (foc %f)\n", lightstart, lightend, focallengthf);
	}

	uz = ds->sup.z + ds->sup.y*(centery-ds->y) + ds->sup.x*(ds->x1-centerx);
	vz = ds->svp.z + ds->svp.y*(centery-ds->y) + ds->svp.x*(ds->x1-centerx);

	dest = R_Address(ds->x1, ds->y);

	source = ds->source;
	//colormap = ds->colormap;

#if 0	// The "perfect" reference version of this routine. Pretty slow.
		// Use it only to see how things are supposed to look.
	i = 0;
	do
	{
		double z = 1.f/iz;
		u = (INT64)(uz*z);
		v = (INT64)(vz*z);

		bit = ((v >> ds->nflatyshift) & ds->nflatmask) | (u >> ds->nflatxshift);
		val = source[bit];

		if (val != TRANSPARENTPIXEL)
		{
			colormap = ds->planezlight[tiltlighting[ds->x1++]] + (ds->colormap - colormaps);
			*dest = colormap[val];
		}
		dest++;
		iz += ds->szp.x;
		uz += ds->sup.x;
		vz += ds->svp.x;
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

		for (i = SPANSIZE-1; i >= 0; i--)
		{
			bit = ((v >> ds->nflatyshift) & ds->nflatmask) | (u >> ds->nflatxshift);
			val = source[bit];
			if (val != TRANSPARENTPIXEL)
			{
				colormap = ds->planezlight[tiltlighting[ds->x1++]] + (ds->colormap - colormaps);
				*dest = colormap[val];
			}
			dest++;
			u += stepu;
			v += stepv;
		}
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
			bit = ((v >> ds->nflatyshift) & ds->nflatmask) | (u >> ds->nflatxshift);
			val = source[bit];
			if (val != TRANSPARENTPIXEL)
			{
				colormap = ds->planezlight[tiltlighting[ds->x1++]] + (ds->colormap - colormaps);
				*dest = colormap[val];
			}
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
				val = source[bit];
				if (val != TRANSPARENTPIXEL)
				{
					colormap = ds->planezlight[tiltlighting[ds->x1++]] + (ds->colormap - colormaps);
					*dest = colormap[val];
				}
				dest++;
				u += stepu;
				v += stepv;
			}
		}
	}
#endif
}

/**	\brief The R_DrawSplat function
	Just like R_DrawSpan, but skips transparent pixels.
*/
void R_DrawSplat(drawspandata_t* ds)
{
	uintptr_t xposition;
	uintptr_t yposition;
	uintptr_t xstep, ystep;
	register UINT32 bit;

	UINT8 *restrict source = ds->source;
	UINT8 *restrict colormap = ds->colormap;
	UINT8 *restrict dest = R_Address(ds->x1, ds->y);

	register intptr_t count = (ds->x2 - ds->x1 + 1);
	size_t i;
	UINT32 val;

	xposition = ds->xfrac; yposition = ds->yfrac;
	xstep = ds->xstep; ystep = ds->ystep;

	// SoM: we only need 6 bits for the integer part (0 thru 63) so the rest
	// can be used for the fraction part. This allows calculation of the memory address in the
	// texture with two shifts, an OR and one AND. (see below)
	// for texture sizes > 64 the amount of precision we can allow will decrease, but only by one
	// bit per power of two (obviously)
	// Ok, because I was able to eliminate the variable spot below, this function is now FASTER
	// than the original span renderer. Whodathunkit?
	xposition <<= ds->nflatshiftup; yposition <<= ds->nflatshiftup;
	xstep <<= ds->nflatshiftup; ystep <<= ds->nflatshiftup;

	while (count >= 8)
	{
		// SoM: Why didn't I see this earlier? the spot variable is a waste now because we don't
		// have the uber complicated math to calculate it now, so that was a memory write we didn't
		// need!
		//
		// <Callum> 4194303 = (2048x2048)-1 (2048x2048 is maximum flat size)
		for (i = 0; i < 8; i++)
		{
			bit = (((UINT32)yposition >> ds->nflatyshift) & ds->nflatmask) | ((UINT32)xposition >> ds->nflatxshift);
			bit &= MAXFLATBYTES;
			val = source[bit];
			if (val != TRANSPARENTPIXEL)
				dest[i] = colormap[val];

			xposition += xstep;
			yposition += ystep;
		}

		dest += 8;
		count -= 8;
	}
	while (count--)
	{
		bit = (((UINT32)yposition >> ds->nflatyshift) & ds->nflatmask) | ((UINT32)xposition >> ds->nflatxshift);
		val = source[bit];
		if (val != TRANSPARENTPIXEL)
		{
			*dest = colormap[val];
		}

		dest++;
		xposition += xstep;
		yposition += ystep;
	}
}

/**	\brief The R_DrawTranslucentSplat function
	Just like R_DrawSplat, but is translucent!
*/
void R_DrawTranslucentSplat(drawspandata_t* ds)
{
	uintptr_t xposition;
	uintptr_t yposition;
	uintptr_t xstep, ystep;
	register UINT32 bit;

	UINT8 *restrict source = ds->source;
	UINT8 *restrict colormap = ds->colormap;
	UINT8 *restrict dest = R_Address(ds->x1, ds->y);

	register intptr_t count = (ds->x2 - ds->x1 + 1);
	size_t i;
	UINT8 val;

	xposition = ds->xfrac; yposition = ds->yfrac;
	xstep = ds->xstep; ystep = ds->ystep;

	// SoM: we only need 6 bits for the integer part (0 thru 63) so the rest
	// can be used for the fraction part. This allows calculation of the memory address in the
	// texture with two shifts, an OR and one AND. (see below)
	// for texture sizes > 64 the amount of precision we can allow will decrease, but only by one
	// bit per power of two (obviously)
	// Ok, because I was able to eliminate the variable spot below, this function is now FASTER
	// than the original span renderer. Whodathunkit?
	xposition <<= ds->nflatshiftup; yposition <<= ds->nflatshiftup;
	xstep <<= ds->nflatshiftup; ystep <<= ds->nflatshiftup;

	while (count >= 8)
	{
		// SoM: Why didn't I see this earlier? the spot variable is a waste now because we don't
		// have the uber complicated math to calculate it now, so that was a memory write we didn't
		// need!
		for (i = 0; i < 8; i++)
		{
			bit = (((UINT32)yposition >> ds->nflatyshift) & ds->nflatmask) | ((UINT32)xposition >> ds->nflatxshift);
			val = source[bit];
			if (val != TRANSPARENTPIXEL)
				dest[i] = *(ds->transmap + (colormap[val] << 8) + dest[i]);

			xposition += xstep;
			yposition += ystep;
		}

		dest += 8;
		count -= 8;
	}
	while (count--)
	{
		bit = (((UINT32)yposition >> ds->nflatyshift) & ds->nflatmask) | ((UINT32)xposition >> ds->nflatxshift);
		val = source[bit];
		if (val != TRANSPARENTPIXEL)
			*dest = *(ds->transmap + (colormap[val] << 8) + *dest);

		dest++;
		xposition += xstep;
		yposition += ystep;
	}
}
