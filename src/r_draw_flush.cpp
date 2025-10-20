// SONIC ROBO BLAST 2 KART
//-----------------------------------------------------------------------------
// Copyright (C) 2025 by Kart Krew.
// Copyright (C) 2020 by Sonic Team Junior.
// Copyright (C) 2000 by DooM Legacy Team.
// Copyright (C) 1996 by id Software, Inc.
// Copyright (C) 1999 by Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman
// Copyright (C) 1999-2000 by Jess Haas, Nicolas Kalkhof, Colin Phipps, Florian Schulze
// Copyright (C) Copyright 2005, 2006 by Florian Schulze, Colin Phipps, Neil Stevens, Andrey Budko
// Copyright (C) 2013 by James Haley, Stephen McGranahan, et al.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  r_draw_flush.cpp
/// \brief Optimized quad column buffer code. By SoM.
/// \note  no includes because this is included as part of r_draw.cpp

template<ColumnFlushType Type>
FUNCINLINE static ATTRINLINE constexpr UINT8
R_GetFlushPixelTranslated(const drawcolumndata_temp_t *t_dc, UINT8 col)
{
	if constexpr (Type & (ColumnFlushType::FLUSH_COLORMAP | ColumnFlushType::FLUSH_COLORMAP_TRANS))
	{
		col = t_dc->translation[col];
	}

	return col;
}

template<ColumnFlushType Type>
FUNCINLINE static ATTRINLINE constexpr UINT8
R_GetFlushPixelTranslucent(const drawcolumndata_temp_t *t_dc, UINT8 * restrict dest, UINT8 col)
{
	col = R_GetFlushPixelTranslated<Type>(t_dc, col);

	if constexpr (Type & (ColumnFlushType::FLUSH_TRANS | ColumnFlushType::FLUSH_COLORMAP_TRANS))
	{
		// haleyjd 09/11/04: use temptranmap here
		return *(t_dc->transmap + (col << 8) + (*dest));
	}
	else
	{
		return col;
	}
}

template<ColumnFlushType Type>
FUNCINLINE static ATTRINLINE constexpr UINT8
R_DrawFlushPixel(const drawcolumndata_temp_t *t_dc, UINT8 * restrict dest, const UINT8 * restrict source)
{
	UINT8 col = *source;
	return R_GetFlushPixelTranslucent<Type>(t_dc, dest, col);
}

//
// R_FlushWhole
//
// Flushes the entire columns in the buffer, one at a time.
// This is used when a quad flush isn't possible.
//
template<ColumnFlushType Type>
static void R_FlushWhole(void)
{
	UINT8 * restrict source;
	UINT8 * restrict dest;
	INT32 count, yl;
	const INT32 stride = vid.width;
	drawcolumndata_temp_t *t_dc = &temp_dc;
	UINT8 *restrict buf = t_dc->buf;

	while (--t_dc->x >= 0)
	{
		yl = t_dc->yl[t_dc->x];
		source = &buf[t_dc->x + (yl << 3)];
		dest = R_Address(t_dc->startx + t_dc->x, yl);
		count = t_dc->yh[t_dc->x] - yl + 1;

		while (--count >= 0)
		{
			*dest = R_DrawFlushPixel<Type>(t_dc, dest, source);
			source += 8;
			dest   += stride;
		}
	}
}

//
// R_FlushHT
//
// Flushes the head and tail of columns in the buffer in
// preparation for a quad flush.
//
template<ColumnFlushType Type>
static void R_FlushHT(void)
{
	UINT8 * restrict source;
	UINT8 * restrict dest;
	INT32 count, colnum = 0;
	INT32 yl, yh;
	const INT32 stride = vid.width;
	const drawcolumndata_temp_t *t_dc = &temp_dc;
	UINT8 *restrict buf = t_dc->buf;

	while (colnum < 8)
	{
		yl = t_dc->yl[colnum];
		yh = t_dc->yh[colnum];

		// flush column head
		if (yl < t_dc->commontop)
		{
			source = &buf[colnum + (yl << 3)];
			dest   = R_Address(t_dc->startx + colnum, yl);
			count  = t_dc->commontop - yl;

			while (--count >= 0)
			{
				*dest = R_DrawFlushPixel<Type>(t_dc, dest, source);
				source += 8;
				dest   += stride;
			}
		}

		// flush column tail
		if (yh > t_dc->commonbot)
		{
			source = &buf[colnum + ((t_dc->commonbot + 1) << 3)];
			dest   = R_Address(t_dc->startx + colnum, t_dc->commonbot + 1);
			count  = yh - t_dc->commonbot;

			while (--count >= 0)
			{
				*dest = R_DrawFlushPixel<Type>(t_dc, dest, source);
				source += 8;
				dest   += stride;
			}
		}

		++colnum;
	}
}

// Begin: Quad column flushing functions.
template<ColumnFlushType Type>
static void R_FlushQuad(void)
{
	const INT32 stride = vid.width;
	const drawcolumndata_temp_t *t_dc = &temp_dc;
	INT32 count = t_dc->commonbot - t_dc->commontop + 1;
	const UINT8 *restrict buf = t_dc->buf;

	const UINT8 * restrict source = buf + (t_dc->commontop << 3);
	UINT8 * restrict dest = R_Address(t_dc->startx, t_dc->commontop);

	if constexpr (Type & ColumnFlushType::FLUSH_OPAQUE)
	{
		// 8 byte aligned copy -- make sure our dest ptr, source ptr AND stride are a multiple of 8!
		if ((((uintptr_t)dest | (uintptr_t)source | stride) & 7) == 0)
		{
			const INT64 *source64 = reinterpret_cast<const INT64 *>(source);
			INT64 *dest64 = reinterpret_cast<INT64 *>(dest);
			const INT32 deststep = stride / 8;

			while (--count >= 0)
			{
				*dest64 = *source64++;
				dest64 += deststep;
			}
		}
		else
		{
			while (--count >= 0)
			{
				dest[0] = source[0];
				dest[1] = source[1];
				dest[2] = source[2];
				dest[3] = source[3];
				dest[4] = source[4];
				dest[5] = source[5];
				dest[6] = source[6];
				dest[7] = source[7];
				source += 8;
				dest   += stride;
			}
		}
	}
	else
	{
		while (--count >= 0)
		{
			dest[0] = R_DrawFlushPixel<Type>(t_dc, &dest[0], &source[0]);
			dest[1] = R_DrawFlushPixel<Type>(t_dc, &dest[1], &source[1]);
			dest[2] = R_DrawFlushPixel<Type>(t_dc, &dest[2], &source[2]);
			dest[3] = R_DrawFlushPixel<Type>(t_dc, &dest[3], &source[3]);
			dest[4] = R_DrawFlushPixel<Type>(t_dc, &dest[4], &source[4]);
			dest[5] = R_DrawFlushPixel<Type>(t_dc, &dest[5], &source[5]);
			dest[6] = R_DrawFlushPixel<Type>(t_dc, &dest[6], &source[6]);
			dest[7] = R_DrawFlushPixel<Type>(t_dc, &dest[7], &source[7]);
			source += 8;
			dest   += stride;
		}
	}
}

// haleyjd 09/12/04: split up R_GetBuffer into various different
// functions to minimize the number of branches and take advantage
// of as much precalculated information as possible.
template<ColumnFlushType Type>
static UINT8 *R_GetBuffer(drawcolumndata_t *dc)
{
	drawcolumndata_temp_t *t_dc = &temp_dc;

	// haleyjd: reordered predicates
	if (t_dc->x == 8 ||
		(t_dc->x && (t_dc->type != Type || t_dc->x + t_dc->startx != dc->x)))
		R_FlushColumns();

	if (!t_dc->x)
	{
		++t_dc->x;
		t_dc->startx = dc->x;
		t_dc->yl[0]  = t_dc->commontop = dc->yl;
		t_dc->yh[0]  = t_dc->commonbot = dc->yh;
		t_dc->type   = Type;

		if constexpr (Type & (ColumnFlushType::FLUSH_TRANS | ColumnFlushType::FLUSH_COLORMAP_TRANS))
		{
			t_dc->transmap = dc->transmap;
		}

		if constexpr (Type & (ColumnFlushType::FLUSH_COLORMAP | ColumnFlushType::FLUSH_COLORMAP_TRANS))
		{
			t_dc->translation = dc->translation;
		}

		R_FlushWholeColumns = R_FlushWhole<Type>;
		R_FlushHTColumns    = R_FlushHT<Type>;
		R_FlushQuadColumn   = R_FlushQuad<Type>;

		return &t_dc->buf[dc->yl << 3];
	}

	t_dc->yl[t_dc->x] = dc->yl;
	t_dc->yh[t_dc->x] = dc->yh;

	if (dc->yl > t_dc->commontop)
		t_dc->commontop = dc->yl;
	if (dc->yh < t_dc->commonbot)
		t_dc->commonbot = dc->yh;

	return &t_dc->buf[(dc->yl << 3) + t_dc->x++];
}

#define DEFINE_GETBUF_FUNC(name, flags) \
	FUNCINLINE static ATTRINLINE UINT8 *name(drawcolumndata_t *dc) \
	{ \
		constexpr ColumnFlushType opt = static_cast<ColumnFlushType>(flags); \
		return R_GetBuffer<opt>(dc); \
	}

DEFINE_GETBUF_FUNC(R_GetBufferOpaque, FLUSH_OPAQUE)
DEFINE_GETBUF_FUNC(R_GetBufferTrans, FLUSH_TRANS)
DEFINE_GETBUF_FUNC(R_GetBufferColormap, FLUSH_COLORMAP)
DEFINE_GETBUF_FUNC(R_GetBufferColormapTrans, FLUSH_COLORMAP_TRANS)

