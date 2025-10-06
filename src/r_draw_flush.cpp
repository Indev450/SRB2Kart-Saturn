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
/// \file  r_draw_flush.cpp
/// \brief column flush functions
/// \note  no includes because this is included as part of r_draw.cpp

template<ColumnFlushType Type>
FUNCINLINE static ATTRINLINE UINT8 R_DrawFlushColumnPixel(UINT8 restrict dest, UINT8 restrict source)
{
	if constexpr (Type & ColumnFlushType::FLUSH_OPAQUE)
	{
		return source;
	}
	else if constexpr (Type & ColumnFlushType::FLUSH_TRANS)
	{
		return temp_dc.tranmap[(source << 8) + dest];
	}
	else if constexpr (Type & ColumnFlushType::FLUSH_COLORMAP)
	{
		return temp_dc.translation[source];
	}
	else if constexpr (Type & ColumnFlushType::FLUSH_COLORMAP_TRANS)
	{
		return temp_dc.tranmap[(temp_dc.translation[source] << 8) + dest];
	}
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
	intptr_t count, yl;
	const restrict intptr_t stride = vid.width;

	while (--temp_dc.x >= 0)
	{
		yl = temp_dc.yl[temp_dc.x];
		source = &temp_dc.buf[temp_dc.x + (yl << 2)];
		dest = R_Address(temp_dc.startx + temp_dc.x, yl);
		count = temp_dc.yh[temp_dc.x] - yl + 1;

		while (--count >= 0)
		{
			*dest = R_DrawFlushColumnPixel<Type>(*dest, *source);
			source += 4;
			dest += stride;
		}
	}
}

#define DEFINE_GETFLUSHWHOLE_FUNC(name, flags) \
	FUNCINLINE static ATTRINLINE void name(void) \
	{ \
		constexpr ColumnFlushType opt = static_cast<ColumnFlushType>(flags); \
		R_FlushWhole<opt>(); \
	}

DEFINE_GETFLUSHWHOLE_FUNC(R_FlushWholeOpague, FLUSH_OPAQUE)
DEFINE_GETFLUSHWHOLE_FUNC(R_FlushWholeTrans, FLUSH_TRANS)
DEFINE_GETFLUSHWHOLE_FUNC(R_FlushWholeColormap, FLUSH_COLORMAP)
DEFINE_GETFLUSHWHOLE_FUNC(R_FlushWholeColormapTrans, FLUSH_COLORMAP_TRANS)

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
	intptr_t count, colnum = 0;
	intptr_t yl, yh;
	const restrict intptr_t stride = vid.width;

	while (colnum < 4)
	{
		yl = temp_dc.yl[colnum];
		yh = temp_dc.yh[colnum];

		// flush column head
		if (yl < temp_dc.commontop)
		{
			source = &temp_dc.buf[colnum + (yl << 2)];
			dest   = R_Address(temp_dc.startx + colnum, yl);
			count  = temp_dc.commontop - yl;

			while (--count >= 0)
			{
				*dest = R_DrawFlushColumnPixel<Type>(*dest, *source);
				source += 4;
				dest += stride;
			}
		}

		// flush column tail
		if (yh > temp_dc.commonbot)
		{
			source = &temp_dc.buf[colnum + ((temp_dc.commonbot + 1) << 2)];
			dest   = R_Address(temp_dc.startx + colnum, temp_dc.commonbot + 1);
			count   = yh - temp_dc.commonbot;

			while (--count >= 0)
			{
				*dest = R_DrawFlushColumnPixel<Type>(*dest, *source);
				source += 4;
				dest += stride;
			}
		}

		++colnum;
	}
}

#define DEFINE_GETFLUSHHT_FUNC(name, flags) \
	FUNCINLINE static ATTRINLINE void name(void) \
	{ \
		constexpr ColumnFlushType opt = static_cast<ColumnFlushType>(flags); \
		R_FlushHT<opt>(); \
	}

DEFINE_GETFLUSHHT_FUNC(R_FlushHTOpague, FLUSH_OPAQUE)
DEFINE_GETFLUSHHT_FUNC(R_FlushHTTrans, FLUSH_TRANS)
DEFINE_GETFLUSHHT_FUNC(R_FlushHTColormap, FLUSH_COLORMAP)
DEFINE_GETFLUSHHT_FUNC(R_FlushHTColormapTrans, FLUSH_COLORMAP_TRANS)

template<ColumnFlushType Type>
static void R_FlushQuad(void)
{
	UINT8 * restrict source = &temp_dc.buf[temp_dc.commontop << 2];
	UINT8 * restrict dest = R_Address(temp_dc.startx, temp_dc.commontop);
	intptr_t count;
	const restrict intptr_t stride = vid.width;

	count = temp_dc.commonbot - temp_dc.commontop + 1;

	if constexpr (Type & ColumnFlushType::FLUSH_OPAQUE)
	{
		if ((sizeof(int) == 4) && (((intptr_t)source % 4) == 0) && (((intptr_t)dest % 4) == 0))
		{
			while(--count >= 0)
			{
				*(int *)dest =   *(int *)source;
				source += 4      * sizeof(UINT8);
				dest   += stride * sizeof(UINT8);
			}
		}
		else
		{
			while(--count >= 0)
			{
				dest[0] = source[0];
				dest[1] = source[1];
				dest[2] = source[2];
				dest[3] = source[3];
				source += 4      * sizeof(UINT8);
				dest   += stride * sizeof(UINT8);
			}
		}
	}
	else if constexpr (Type & ColumnFlushType::FLUSH_TRANS)
	{
		while (--count >= 0)
		{
			dest[0] = temp_dc.tranmap[(source[0] << 8) + dest[0]];
			dest[1] = temp_dc.tranmap[(source[1] << 8) + dest[1]];
			dest[2] = temp_dc.tranmap[(source[2] << 8) + dest[2]];
			dest[3] = temp_dc.tranmap[(source[3] << 8) + dest[3]];
			source += 4;
			dest += stride;
		}
	}
	else if constexpr (Type & ColumnFlushType::FLUSH_COLORMAP)
	{
		while (--count >= 0)
		{
			dest[0] = temp_dc.translation[source[0]];
			dest[1] = temp_dc.translation[source[1]];
			dest[2] = temp_dc.translation[source[2]];
			dest[3] = temp_dc.translation[source[3]];
			source += 4;
			dest += stride;
		}
	}
	else if constexpr (Type & ColumnFlushType::FLUSH_COLORMAP_TRANS)
	{
		while (--count >= 0)
		{
			dest[0] = temp_dc.tranmap[(temp_dc.translation[source[0]] << 8) + dest[0]];
			dest[1] = temp_dc.tranmap[(temp_dc.translation[source[1]] << 8) + dest[1]];
			dest[2] = temp_dc.tranmap[(temp_dc.translation[source[2]] << 8) + dest[2]];
			dest[3] = temp_dc.tranmap[(temp_dc.translation[source[3]] << 8) + dest[3]];
			source += 4;
			dest += stride;
		}
	}
}

#define DEFINE_GETFLUSHQUAD_FUNC(name, flags) \
	FUNCINLINE static ATTRINLINE void name(void) \
	{ \
		constexpr ColumnFlushType opt = static_cast<ColumnFlushType>(flags); \
		R_FlushQuad<opt>(); \
	}

DEFINE_GETFLUSHQUAD_FUNC(R_FlushQuadOpague, FLUSH_OPAQUE)
DEFINE_GETFLUSHQUAD_FUNC(R_FlushQuadTrans, FLUSH_TRANS)
DEFINE_GETFLUSHQUAD_FUNC(R_FlushQuadColormap, FLUSH_COLORMAP)
DEFINE_GETFLUSHQUAD_FUNC(R_FlushQuadColormapTrans, FLUSH_COLORMAP_TRANS)

template<ColumnFlushType Type>
static UINT8 *R_GetBuffer(drawcolumndata_t *dc)
{
	if (temp_dc.x == 4 ||
		(temp_dc.x && (temp_dc.type != Type || temp_dc.x + temp_dc.startx != dc->x)))
		R_FlushColumns();

	if (!temp_dc.x)
	{
		temp_dc.startx = dc->x;
		temp_dc.yl[0] = temp_dc.commontop = dc->yl;
		temp_dc.yh[0] = temp_dc.commonbot = dc->yh;
		temp_dc.type = Type;

		if constexpr (Type & ColumnFlushType::FLUSH_OPAQUE)
		{
			R_FlushWholeColumns = R_FlushWholeOpague;
			R_FlushHTColumns = R_FlushHTOpague;
			R_FlushQuadColumn = R_FlushQuadOpague;
		}
		else if constexpr (Type & ColumnFlushType::FLUSH_TRANS)
		{
			temp_dc.tranmap = dc->transmap;
			R_FlushWholeColumns = R_FlushWholeTrans;
			R_FlushHTColumns = R_FlushHTTrans;
			R_FlushQuadColumn = R_FlushQuadTrans;
		}
		else if constexpr (Type & ColumnFlushType::FLUSH_COLORMAP)
		{
			temp_dc.translation = dc->translation;
			R_FlushWholeColumns = R_FlushWholeColormap;
			R_FlushHTColumns = R_FlushHTColormap;
			R_FlushQuadColumn = R_FlushQuadColormap;
		}
		else if constexpr (Type & ColumnFlushType::FLUSH_COLORMAP_TRANS)
		{
			temp_dc.tranmap = dc->transmap;
			temp_dc.translation = dc->translation;
			R_FlushWholeColumns = R_FlushWholeColormapTrans;
			R_FlushHTColumns = R_FlushHTColormapTrans;
			R_FlushQuadColumn = R_FlushQuadColormapTrans;
		}

		temp_dc.x += 1;
		return &temp_dc.buf[dc->yl << 2];
	}

	temp_dc.yl[temp_dc.x] = dc->yl;
	temp_dc.yh[temp_dc.x] = dc->yh;

	if (dc->yl > temp_dc.commontop)
		temp_dc.commontop = dc->yl;
	if (dc->yh < temp_dc.commonbot)
		temp_dc.commonbot = dc->yh;

	return &temp_dc.buf[(dc->yl << 2) + temp_dc.x++];
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

