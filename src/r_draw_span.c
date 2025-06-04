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

#define SPANSIZE 16
#define INVSPAN 0.0625f

// <Callum> 4194303 = (2048x2048)-1 (2048x2048 is maximum flat size)
#define MAXFLATBYTES 4194303

/**	\brief The R_DrawSpan function
	Draws the actual span.
*/
void R_DrawSpan(void)
{
	uintptr_t xposition;
	uintptr_t yposition;
	uintptr_t xstep, ystep;
	register UINT32 bit;

	UINT8 *restrict source = ds_source;
	UINT8 *restrict colormap = ds_colormap;
	UINT8 *restrict dest = R_Address(ds_x1, ds_y);
	const UINT8 *restrict deststop = vid.screens[0] + vid.rowbytes * vid.height;

	register intptr_t count = (ds_x2 - ds_x1 + 1);

	xposition = ds_xfrac; yposition = ds_yfrac;
	xstep = ds_xstep; ystep = ds_ystep;

	// SoM: we only need 6 bits for the integer part (0 thru 63) so the rest
	// can be used for the fraction part. This allows calculation of the memory address in the
	// texture with two shifts, an OR and one AND. (see below)
	// for texture sizes > 64 the amount of precision we can allow will decrease, but only by one
	// bit per power of two (obviously)
	// Ok, because I was able to eliminate the variable spot below, this function is now FASTER
	// than the original span renderer. Whodathunkit?
	xposition <<= nflatshiftup; yposition <<= nflatshiftup;
	xstep <<= nflatshiftup; ystep <<= nflatshiftup;

	if (dest+8 > deststop)
	{
		return;
	}
	while (count >= 8)
	{
		// SoM: Why didn't I see this earlier? the spot variable is a waste now because we don't
		// have the uber complicated math to calculate it now, so that was a memory write we didn't
		// need!
		bit = (((UINT32)yposition >> nflatyshift) & nflatmask) | ((UINT32)xposition >> nflatxshift);
		dest[0] = colormap[source[bit]];
		xposition += xstep;
		yposition += ystep;

		bit = (((UINT32)yposition >> nflatyshift) & nflatmask) | ((UINT32)xposition >> nflatxshift);
		dest[1] = colormap[source[bit]];
		xposition += xstep;
		yposition += ystep;

		bit = (((UINT32)yposition >> nflatyshift) & nflatmask) | ((UINT32)xposition >> nflatxshift);
		dest[2] = colormap[source[bit]];
		xposition += xstep;
		yposition += ystep;

		bit = (((UINT32)yposition >> nflatyshift) & nflatmask) | ((UINT32)xposition >> nflatxshift);
		dest[3] = colormap[source[bit]];
		xposition += xstep;
		yposition += ystep;

		bit = (((UINT32)yposition >> nflatyshift) & nflatmask) | ((UINT32)xposition >> nflatxshift);
		dest[4] = colormap[source[bit]];
		xposition += xstep;
		yposition += ystep;

		bit = (((UINT32)yposition >> nflatyshift) & nflatmask) | ((UINT32)xposition >> nflatxshift);
		dest[5] = colormap[source[bit]];
		xposition += xstep;
		yposition += ystep;

		bit = (((UINT32)yposition >> nflatyshift) & nflatmask) | ((UINT32)xposition >> nflatxshift);
		dest[6] = colormap[source[bit]];
		xposition += xstep;
		yposition += ystep;

		bit = (((UINT32)yposition >> nflatyshift) & nflatmask) | ((UINT32)xposition >> nflatxshift);
		dest[7] = colormap[source[bit]];
		xposition += xstep;
		yposition += ystep;

		dest += 8;
		count -= 8;
	}

	while (count-- && dest <= deststop)
	{
		bit = (((UINT32)yposition >> nflatyshift) & nflatmask) | ((UINT32)xposition >> nflatxshift);
		*dest++ = colormap[source[bit]];
		xposition += xstep;
		yposition += ystep;
	}
}

/**	\brief The R_DrawTranslucentSpan function
	Draws the actual span with translucent.
*/
void R_DrawTranslucentSpan(void)
{
	fixed_t xposition;
	fixed_t yposition;
	fixed_t xstep, ystep;
	register UINT32 bit;

	UINT8 *source;
	UINT8 *colormap;
	register UINT8 *dest;
	const UINT8 *deststop = vid.screens[0] + vid.rowbytes * vid.height;

	register intptr_t count = (ds_x2 - ds_x1 + 1);

	xposition = ds_xfrac; yposition = ds_yfrac;
	xstep = ds_xstep; ystep = ds_ystep;

	// SoM: we only need 6 bits for the integer part (0 thru 63) so the rest
	// can be used for the fraction part. This allows calculation of the memory address in the
	// texture with two shifts, an OR and one AND. (see below)
	// for texture sizes > 64 the amount of precision we can allow will decrease, but only by one
	// bit per power of two (obviously)
	// Ok, because I was able to eliminate the variable spot below, this function is now FASTER
	// than the original span renderer. Whodathunkit?
	xposition <<= nflatshiftup; yposition <<= nflatshiftup;
	xstep <<= nflatshiftup; ystep <<= nflatshiftup;

	source = ds_source;
	colormap = ds_colormap;
	dest = R_Address(ds_x1, ds_y);

	while (count >= 8)
	{
		// SoM: Why didn't I see this earlier? the spot variable is a waste now because we don't
		// have the uber complicated math to calculate it now, so that was a memory write we didn't
		// need!
		bit = (((UINT32)yposition >> nflatyshift) & nflatmask) | ((UINT32)xposition >> nflatxshift);
		dest[0] = *(ds_transmap + (colormap[source[bit]] << 8) + dest[0]);
		xposition += xstep;
		yposition += ystep;

		bit = (((UINT32)yposition >> nflatyshift) & nflatmask) | ((UINT32)xposition >> nflatxshift);
		dest[1] = *(ds_transmap + (colormap[source[bit]] << 8) + dest[1]);
		xposition += xstep;
		yposition += ystep;

		bit = (((UINT32)yposition >> nflatyshift) & nflatmask) | ((UINT32)xposition >> nflatxshift);
		dest[2] = *(ds_transmap + (colormap[source[bit]] << 8) + dest[2]);
		xposition += xstep;
		yposition += ystep;

		bit = (((UINT32)yposition >> nflatyshift) & nflatmask) | ((UINT32)xposition >> nflatxshift);
		dest[3] = *(ds_transmap + (colormap[source[bit]] << 8) + dest[3]);
		xposition += xstep;
		yposition += ystep;

		bit = (((UINT32)yposition >> nflatyshift) & nflatmask) | ((UINT32)xposition >> nflatxshift);
		dest[4] = *(ds_transmap + (colormap[source[bit]] << 8) + dest[4]);
		xposition += xstep;
		yposition += ystep;

		bit = (((UINT32)yposition >> nflatyshift) & nflatmask) | ((UINT32)xposition >> nflatxshift);
		dest[5] = *(ds_transmap + (colormap[source[bit]] << 8) + dest[5]);
		xposition += xstep;
		yposition += ystep;

		bit = (((UINT32)yposition >> nflatyshift) & nflatmask) | ((UINT32)xposition >> nflatxshift);
		dest[6] = *(ds_transmap + (colormap[source[bit]] << 8) + dest[6]);
		xposition += xstep;
		yposition += ystep;

		bit = (((UINT32)yposition >> nflatyshift) & nflatmask) | ((UINT32)xposition >> nflatxshift);
		dest[7] = *(ds_transmap + (colormap[source[bit]] << 8) + dest[7]);
		xposition += xstep;
		yposition += ystep;

		dest += 8;
		count -= 8;
	}
	while (count-- && dest <= deststop)
	{
		bit = (((UINT32)yposition >> nflatyshift) & nflatmask) | ((UINT32)xposition >> nflatxshift);
		*dest = *(ds_transmap + (colormap[source[bit]] << 8) + *dest);
		dest++;
		xposition += xstep;
		yposition += ystep;
	}
}

void R_DrawTranslucentWaterSpan(void)
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
	xposition = ds_xfrac << nflatshiftup; yposition = (ds_yfrac + ds_waterofs) << nflatshiftup;
	xstep = ds_xstep << nflatshiftup; ystep = ds_ystep << nflatshiftup;

	source = ds_source;
	colormap = ds_colormap;
	dest = R_Address(ds_x1, ds_y);
	dsrc = vid.screens[1] + (ds_y+ds_bgofs)*vid.width + ds_x1;
	count = ds_x2 - ds_x1 + 1;

	while (count >= 8)
	{
		// SoM: Why didn't I see this earlier? the spot variable is a waste now because we don't
		// have the uber complicated math to calculate it now, so that was a memory write we didn't
		// need!
		bit = ((yposition >> nflatyshift) & nflatmask) | (xposition >> nflatxshift);
		dest[0] = colormap[*(ds_transmap + (source[bit] << 8) + *dsrc++)];
		xposition += xstep;
		yposition += ystep;

		bit = ((yposition >> nflatyshift) & nflatmask) | (xposition >> nflatxshift);
		dest[1] = colormap[*(ds_transmap + (source[bit] << 8) + *dsrc++)];
		xposition += xstep;
		yposition += ystep;

		bit = ((yposition >> nflatyshift) & nflatmask) | (xposition >> nflatxshift);
		dest[2] = colormap[*(ds_transmap + (source[bit] << 8) + *dsrc++)];
		xposition += xstep;
		yposition += ystep;

		bit = ((yposition >> nflatyshift) & nflatmask) | (xposition >> nflatxshift);
		dest[3] = colormap[*(ds_transmap + (source[bit] << 8) + *dsrc++)];
		xposition += xstep;
		yposition += ystep;

		bit = ((yposition >> nflatyshift) & nflatmask) | (xposition >> nflatxshift);
		dest[4] = colormap[*(ds_transmap + (source[bit] << 8) + *dsrc++)];
		xposition += xstep;
		yposition += ystep;

		bit = ((yposition >> nflatyshift) & nflatmask) | (xposition >> nflatxshift);
		dest[5] = colormap[*(ds_transmap + (source[bit] << 8) + *dsrc++)];
		xposition += xstep;
		yposition += ystep;

		bit = ((yposition >> nflatyshift) & nflatmask) | (xposition >> nflatxshift);
		dest[6] = colormap[*(ds_transmap + (source[bit] << 8) + *dsrc++)];
		xposition += xstep;
		yposition += ystep;

		bit = ((yposition >> nflatyshift) & nflatmask) | (xposition >> nflatxshift);
		dest[7] = colormap[*(ds_transmap + (source[bit] << 8) + *dsrc++)];
		xposition += xstep;
		yposition += ystep;

		dest += 8;
		count -= 8;
	}
	while (count--)
	{
		bit = ((yposition >> nflatyshift) & nflatmask) | (xposition >> nflatxshift);
		*dest++ = colormap[*(ds_transmap + (source[bit] << 8) + *dsrc++)];

		xposition += xstep;
		yposition += ystep;
	}
}

/**	\brief The R_DrawFogSpan function
	Draws the actual span with fogging.
*/
void R_DrawFogSpan(void)
{
	UINT8 *colormap;
	register UINT8 *dest;

	register intptr_t count;

	colormap = ds_colormap;
	dest = R_Address(dc_x, dc_yl);

	count = ds_x2 - ds_x1 + 1;

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
void R_CalcTiltedLighting(fixed_t start, fixed_t end)
{
	// ZDoom uses a different lighting setup to us, and I couldn't figure out how to adapt their version
	// of this function. Here's my own.
	INT32 left = ds_x1, right = ds_x2;
	fixed_t step = (end-start)/(ds_x2-ds_x1+1);
	INT32 i;

	// I wanna do some optimizing by checking for out-of-range segments on either side to fill in all at once,
	// but I'm too bad at coding to not crash the game trying to do that. I guess this is fast enough for now...

	for (i = left; i <= right; i++)
	{
		tiltlighting[i] = (start += step) >> FRACBITS;

		if (tiltlighting[i] < 0)
		{
			tiltlighting[i] = 0;
		}
		else if (tiltlighting[i] >= MAXLIGHTSCALE)
		{
			tiltlighting[i] = MAXLIGHTSCALE-1;
		}
	}
}

#define PLANELIGHTFLOAT ((float)BASEVIDWIDTH * BASEVIDWIDTH / vid.width / (zeroheight - FIXED_TO_FLOAT(viewz)) / 21.0f * FIXED_TO_FLOAT(fovtan))

/**	\brief The R_DrawTiltedSpan function
	Draw slopes! Holy sheit!
*/
void R_DrawTiltedSpan(void)
{
	int width = ds_x2 - ds_x1;
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

	iz = ds_szp->z + ds_szp->y*(centery-ds_y) + ds_szp->x*(ds_x1-centerx);

	// Lighting is simple. It's just linear interpolation from start to end
	{
		float planelightfloat = PLANELIGHTFLOAT;
		float lightstart, lightend;

		lightend = (iz + ds_szp->x*width) * planelightfloat;
		lightstart = iz * planelightfloat;

		R_CalcTiltedLighting(FLOAT_TO_FIXED(lightstart), FLOAT_TO_FIXED(lightend));
		//CONS_Printf("tilted lighting %f to %f (foc %f)\n", lightstart, lightend, focallengthf);
	}

	uz = ds_sup->z + ds_sup->y*(centery-ds_y) + ds_sup->x*(ds_x1-centerx);
	vz = ds_svp->z + ds_svp->y*(centery-ds_y) + ds_svp->x*(ds_x1-centerx);

	dest = R_Address(ds_x1, ds_y);

	source = ds_source;

#if 0	// The "perfect" reference version of this routine. Pretty slow.
		// Use it only to see how things are supposed to look.
	i = 0;
	do
	{
		double z = 1.f/iz;
		u = (INT64)(uz*z) + viewx;
		v = (INT64)(vz*z) + viewy;

		bit = ((v >> nflatyshift) & nflatmask) | (u >> nflatxshift);
		colormap = planezlight[tiltlighting[ds_x1++]] + (ds_colormap - colormaps);
		*dest = colormap[source[bit]];

		dest++;
		iz += ds_szp->x;
		uz += ds_sup->x;
		vz += ds_svp->x;
	} while (--width >= 0);
#else
	startz = 1.f/iz;
	startu = uz*startz;
	startv = vz*startz;

	izstep = ds_szp->x * SPANSIZE;
	uzstep = ds_sup->x * SPANSIZE;
	vzstep = ds_svp->x * SPANSIZE;
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
		u = (INT64)(startu) + viewx;
		v = (INT64)(startv) + viewy;

		for (i = SPANSIZE-1; i >= 0; i--)
		{
			bit = ((v >> nflatyshift) & nflatmask) | (u >> nflatxshift);
			colormap = planezlight[tiltlighting[ds_x1++]] + (ds_colormap - colormaps);
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
			bit = ((v >> nflatyshift) & nflatmask) | (u >> nflatxshift);
			colormap = planezlight[tiltlighting[ds_x1++]] + (ds_colormap - colormaps);
			*dest = colormap[source[bit]];
		}
		else
		{
			float left = width;
			iz += ds_szp->x * left;
			uz += ds_sup->x * left;
			vz += ds_svp->x * left;

			endz = 1.f/iz;
			endu = uz*endz;
			endv = vz*endz;
			left = 1.f/left;
			stepu = (INT64)((endu - startu) * left);
			stepv = (INT64)((endv - startv) * left);
			u = (INT64)(startu) + viewx;
			v = (INT64)(startv) + viewy;

			for (; width != 0; width--)
			{
				bit = ((v >> nflatyshift) & nflatmask) | (u >> nflatxshift);
				colormap = planezlight[tiltlighting[ds_x1++]] + (ds_colormap - colormaps);
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
void R_DrawTiltedTranslucentSpan(void)
{
	int width = ds_x2 - ds_x1;
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

	iz = ds_szp->z + ds_szp->y*(centery-ds_y) + ds_szp->x*(ds_x1-centerx);

	// Lighting is simple. It's just linear interpolation from start to end
	{
		float planelightfloat = PLANELIGHTFLOAT;
		float lightstart, lightend;

		lightend = (iz + ds_szp->x*width) * planelightfloat;
		lightstart = iz * planelightfloat;

		R_CalcTiltedLighting(FLOAT_TO_FIXED(lightstart), FLOAT_TO_FIXED(lightend));
		//CONS_Printf("tilted lighting %f to %f (foc %f)\n", lightstart, lightend, focallengthf);
	}

	uz = ds_sup->z + ds_sup->y*(centery-ds_y) + ds_sup->x*(ds_x1-centerx);
	vz = ds_svp->z + ds_svp->y*(centery-ds_y) + ds_svp->x*(ds_x1-centerx);

	dest = R_Address(ds_x1, ds_y);

	source = ds_source;

#if 0	// The "perfect" reference version of this routine. Pretty slow.
		// Use it only to see how things are supposed to look.
	i = 0;
	do
	{
		double z = 1.f/iz;
		u = (INT64)(uz*z) + viewx;
		v = (INT64)(vz*z) + viewy;

		bit = ((v >> nflatyshift) & nflatmask) | (u >> nflatxshift);
		colormap = planezlight[tiltlighting[ds_x1++]] + (ds_colormap - colormaps);
		*dest = *(ds_transmap + (colormap[source[bit]] << 8) + *dest);

		dest++;
		iz += ds_szp->x;
		uz += ds_sup->x;
		vz += ds_svp->x;
	} while (--width >= 0);
#else
	startz = 1.f/iz;
	startu = uz*startz;
	startv = vz*startz;

	izstep = ds_szp->x * SPANSIZE;
	uzstep = ds_sup->x * SPANSIZE;
	vzstep = ds_svp->x * SPANSIZE;
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
		u = (INT64)(startu) + viewx;
		v = (INT64)(startv) + viewy;

		for (i = SPANSIZE-1; i >= 0; i--)
		{
			bit = ((v >> nflatyshift) & nflatmask) | (u >> nflatxshift);
			colormap = planezlight[tiltlighting[ds_x1++]] + (ds_colormap - colormaps);
			*dest = *(ds_transmap + (colormap[source[bit]] << 8) + *dest);

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
			bit = ((v >> nflatyshift) & nflatmask) | (u >> nflatxshift);
			colormap = planezlight[tiltlighting[ds_x1++]] + (ds_colormap - colormaps);
			*dest = *(ds_transmap + (colormap[source[bit]] << 8) + *dest);
		}
		else
		{
			float left = width;
			iz += ds_szp->x * left;
			uz += ds_sup->x * left;
			vz += ds_svp->x * left;

			endz = 1.f/iz;
			endu = uz*endz;
			endv = vz*endz;
			left = 1.f/left;
			stepu = (INT64)((endu - startu) * left);
			stepv = (INT64)((endv - startv) * left);
			u = (INT64)(startu) + viewx;
			v = (INT64)(startv) + viewy;

			for (; width != 0; width--)
			{
				bit = ((v >> nflatyshift) & nflatmask) | (u >> nflatxshift);
				colormap = planezlight[tiltlighting[ds_x1++]] + (ds_colormap - colormaps);
				*dest = *(ds_transmap + (colormap[source[bit]] << 8) + *dest);

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
void R_DrawTiltedTranslucentWaterSpan(void)
{
	int width = ds_x2 - ds_x1;
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

	iz = ds_szp->z + ds_szp->y*(centery-ds_y) + ds_szp->x*(ds_x1-centerx);

	// Lighting is simple. It's just linear interpolation from start to end
	{
		float planelightfloat = PLANELIGHTFLOAT;
		float lightstart, lightend;

		lightend = (iz + ds_szp->x*width) * planelightfloat;
		lightstart = iz * planelightfloat;

		R_CalcTiltedLighting(FLOAT_TO_FIXED(lightstart), FLOAT_TO_FIXED(lightend));
		//CONS_Printf("tilted lighting %f to %f (foc %f)\n", lightstart, lightend, focallengthf);
	}

	uz = ds_sup->z + ds_sup->y*(centery-ds_y) + ds_sup->x*(ds_x1-centerx);
	vz = ds_svp->z + ds_svp->y*(centery-ds_y) + ds_svp->x*(ds_x1-centerx);

	dest = R_Address(ds_x1, ds_y);
	dsrc = vid.screens[1] + (ds_y+ds_bgofs)*vid.width + ds_x1;

	source = ds_source;

#if 0	// The "perfect" reference version of this routine. Pretty slow.
		// Use it only to see how things are supposed to look.
	i = 0;
	do
	{
		double z = 1.f/iz;
		u = (INT64)(uz*z) + viewx;
		v = (INT64)(vz*z) + viewy;

		bit = ((v >> nflatyshift) & nflatmask) | (u >> nflatxshift);
		colormap = planezlight[tiltlighting[ds_x1++]] + (ds_colormap - colormaps);
		*dest = *(ds_transmap + (colormap[source[bit]] << 8) + *dsrc++);

		dest++;
		iz += ds_szp->x;
		uz += ds_sup->x;
		vz += ds_svp->x;
	} while (--width >= 0);
#else
	startz = 1.f/iz;
	startu = uz*startz;
	startv = vz*startz;

	izstep = ds_szp->x * SPANSIZE;
	uzstep = ds_sup->x * SPANSIZE;
	vzstep = ds_svp->x * SPANSIZE;
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
		u = (INT64)(startu) + viewx;
		v = (INT64)(startv) + viewy;

		for (i = SPANSIZE-1; i >= 0; i--)
		{
			bit = ((v >> nflatyshift) & nflatmask) | (u >> nflatxshift);
			colormap = planezlight[tiltlighting[ds_x1++]] + (ds_colormap - colormaps);
			*dest = *(ds_transmap + (colormap[source[bit]] << 8) + *dsrc++);

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
			bit = ((v >> nflatyshift) & nflatmask) | (u >> nflatxshift);
			colormap = planezlight[tiltlighting[ds_x1++]] + (ds_colormap - colormaps);
			*dest = *(ds_transmap + (colormap[source[bit]] << 8) + *dsrc++);
		}
		else
		{
			float left = width;
			iz += ds_szp->x * left;
			uz += ds_sup->x * left;
			vz += ds_svp->x * left;

			endz = 1.f/iz;
			endu = uz*endz;
			endv = vz*endz;
			left = 1.f/left;
			stepu = (INT64)((endu - startu) * left);
			stepv = (INT64)((endv - startv) * left);
			u = (INT64)(startu) + viewx;
			v = (INT64)(startv) + viewy;

			for (; width != 0; width--)
			{
				bit = ((v >> nflatyshift) & nflatmask) | (u >> nflatxshift);
				colormap = planezlight[tiltlighting[ds_x1++]] + (ds_colormap - colormaps);
				*dest = *(ds_transmap + (colormap[source[bit]] << 8) + *dsrc++);

				dest++;
				u += stepu;
				v += stepv;
			}
		}
	}
#endif
}

void R_DrawTiltedSplat(void)
{
	// x1, x2 = ds_x1, ds_x2
	int width = ds_x2 - ds_x1;
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

	iz = ds_szp->z + ds_szp->y*(centery-ds_y) + ds_szp->x*(ds_x1-centerx);

	// Lighting is simple. It's just linear interpolation from start to end
	{
		float planelightfloat = PLANELIGHTFLOAT;
		float lightstart, lightend;

		lightend = (iz + ds_szp->x*width) * planelightfloat;
		lightstart = iz * planelightfloat;

		R_CalcTiltedLighting(FLOAT_TO_FIXED(lightstart), FLOAT_TO_FIXED(lightend));
		//CONS_Printf("tilted lighting %f to %f (foc %f)\n", lightstart, lightend, focallengthf);
	}

	uz = ds_sup->z + ds_sup->y*(centery-ds_y) + ds_sup->x*(ds_x1-centerx);
	vz = ds_svp->z + ds_svp->y*(centery-ds_y) + ds_svp->x*(ds_x1-centerx);

	dest = R_Address(ds_x1, ds_y);

	source = ds_source;
	//colormap = ds_colormap;

#if 0	// The "perfect" reference version of this routine. Pretty slow.
		// Use it only to see how things are supposed to look.
	i = 0;
	do
	{
		double z = 1.f/iz;
		u = (INT64)(uz*z) + viewx;
		v = (INT64)(vz*z) + viewy;

		bit = ((v >> nflatyshift) & nflatmask) | (u >> nflatxshift);
		val = source[bit];

		if (val != TRANSPARENTPIXEL)
		{
			colormap = planezlight[tiltlighting[ds_x1++]] + (ds_colormap - colormaps);
			*dest = colormap[val];
		}
		dest++;
		iz += ds_szp->x;
		uz += ds_sup->x;
		vz += ds_svp->x;
	} while (--width >= 0);
#else
	startz = 1.f/iz;
	startu = uz*startz;
	startv = vz*startz;

	izstep = ds_szp->x * SPANSIZE;
	uzstep = ds_sup->x * SPANSIZE;
	vzstep = ds_svp->x * SPANSIZE;
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
		u = (INT64)(startu) + viewx;
		v = (INT64)(startv) + viewy;

		for (i = SPANSIZE-1; i >= 0; i--)
		{
			bit = ((v >> nflatyshift) & nflatmask) | (u >> nflatxshift);
			val = source[bit];
			if (val != TRANSPARENTPIXEL)
			{
				colormap = planezlight[tiltlighting[ds_x1++]] + (ds_colormap - colormaps);
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
			bit = ((v >> nflatyshift) & nflatmask) | (u >> nflatxshift);
			val = source[bit];
			if (val != TRANSPARENTPIXEL)
			{
				colormap = planezlight[tiltlighting[ds_x1++]] + (ds_colormap - colormaps);
				*dest = colormap[val];
			}
		}
		else
		{
			float left = width;
			iz += ds_szp->x * left;
			uz += ds_sup->x * left;
			vz += ds_svp->x * left;

			endz = 1.f/iz;
			endu = uz*endz;
			endv = vz*endz;
			left = 1.f/left;
			stepu = (INT64)((endu - startu) * left);
			stepv = (INT64)((endv - startv) * left);
			u = (INT64)(startu) + viewx;
			v = (INT64)(startv) + viewy;

			for (; width != 0; width--)
			{
				bit = ((v >> nflatyshift) & nflatmask) | (u >> nflatxshift);
				val = source[bit];
				if (val != TRANSPARENTPIXEL)
				{
					colormap = planezlight[tiltlighting[ds_x1++]] + (ds_colormap - colormaps);
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
void R_DrawSplat(void)
{
	UINT32 xposition;
	UINT32 yposition;
	UINT32 xstep, ystep;
	register UINT32 bit;

	UINT8 *source;
	UINT8 *colormap;
	register UINT8 *dest;

	register size_t count;
	size_t i;
	UINT32 val;

	// SoM: we only need 6 bits for the integer part (0 thru 63) so the rest
	// can be used for the fraction part. This allows calculation of the memory address in the
	// texture with two shifts, an OR and one AND. (see below)
	// for texture sizes > 64 the amount of precision we can allow will decrease, but only by one
	// bit per power of two (obviously)
	// Ok, because I was able to eliminate the variable spot below, this function is now FASTER
	// than the original span renderer. Whodathunkit?
	xposition = ds_xfrac << nflatshiftup; yposition = ds_yfrac << nflatshiftup;
	xstep = ds_xstep << nflatshiftup; ystep = ds_ystep << nflatshiftup;

	source = ds_source;
	colormap = ds_colormap;
	dest = R_Address(ds_x1, ds_y);
	count = ds_x2 - ds_x1 + 1;

	while (count >= 8)
	{
		// SoM: Why didn't I see this earlier? the spot variable is a waste now because we don't
		// have the uber complicated math to calculate it now, so that was a memory write we didn't
		// need!
		//
		// <Callum> 4194303 = (2048x2048)-1 (2048x2048 is maximum flat size)
		for (i = 0; i < 8; i++)
		{
			bit = (((UINT32)yposition >> nflatyshift) & nflatmask) | ((UINT32)xposition >> nflatxshift);
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
		bit = (((UINT32)yposition >> nflatyshift) & nflatmask) | ((UINT32)xposition >> nflatxshift);
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
void R_DrawTranslucentSplat(void)
{
	UINT32 xposition;
	UINT32 yposition;
	UINT32 xstep, ystep;
	register UINT32 bit;

	UINT8 *source;
	UINT8 *colormap;
	register UINT8 *dest;

	register size_t count;
	size_t i;
	UINT8 val;

	// SoM: we only need 6 bits for the integer part (0 thru 63) so the rest
	// can be used for the fraction part. This allows calculation of the memory address in the
	// texture with two shifts, an OR and one AND. (see below)
	// for texture sizes > 64 the amount of precision we can allow will decrease, but only by one
	// bit per power of two (obviously)
	// Ok, because I was able to eliminate the variable spot below, this function is now FASTER
	// than the original span renderer. Whodathunkit?
	xposition = ds_xfrac << nflatshiftup; yposition = ds_yfrac << nflatshiftup;
	xstep = ds_xstep << nflatshiftup; ystep = ds_ystep << nflatshiftup;

	source = ds_source;
	colormap = ds_colormap;
	dest = R_Address(ds_x1, ds_y);
	count = ds_x2 - ds_x1 + 1;

	while (count >= 8)
	{
		// SoM: Why didn't I see this earlier? the spot variable is a waste now because we don't
		// have the uber complicated math to calculate it now, so that was a memory write we didn't
		// need!
		for (i = 0; i < 8; i++)
		{
			bit = (((UINT32)yposition >> nflatyshift) & nflatmask) | ((UINT32)xposition >> nflatxshift);
			val = source[bit];
			if (val != TRANSPARENTPIXEL)
				dest[i] = *(ds_transmap + (colormap[val] << 8) + dest[i]);

			xposition += xstep;
			yposition += ystep;
		}

		dest += 8;
		count -= 8;
	}
	while (count--)
	{
		bit = (((UINT32)yposition >> nflatyshift) & nflatmask) | ((UINT32)xposition >> nflatxshift);
		val = source[bit];
		if (val != TRANSPARENTPIXEL)
			*dest = *(ds_transmap + (colormap[val] << 8) + *dest);

		dest++;
		xposition += xstep;
		yposition += ystep;
	}
}
