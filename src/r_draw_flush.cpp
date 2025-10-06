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

//
// R_FlushWholeOpaque
//
// Flushes the entire columns in the buffer, one at a time.
// This is used when a quad flush isn't possible.
// Opaque version -- no remapping whatsoever.
//
static void R_FlushWholeOpaque(void)
{
	UINT8 *source;
	UINT8 *dest;
	INT32 count, yl;
	const restrict INT32 stride = vid.width;

	while (--temp_dc.x >= 0)
	{
		yl = temp_dc.yl[temp_dc.x];
		source = &temp_dc.buf[temp_dc.x + (yl << 2)];
		dest = R_Address(temp_dc.startx + temp_dc.x, yl);
		count = temp_dc.yh[temp_dc.x] - yl + 1;

		while (--count >= 0)
		{
			*dest = *source;
			source += 4;
			dest += stride;
		}
	}
}

//
// R_FlushHTOpaque
//
// Flushes the head and tail of columns in the buffer in
// preparation for a quad flush.
// Opaque version -- no remapping whatsoever.
//
static void R_FlushHTOpaque(void)
{
	UINT8 *source;
	UINT8 *dest;
	INT32 count, colnum = 0;
	INT32 yl, yh;
	const restrict INT32 stride = vid.width;

	while (colnum < 4)
	{
		yl = temp_dc.yl[colnum];
		yh = temp_dc.yh[colnum];

		// flush column head
		if (yl < temp_dc.commontop)
		{
			source = &temp_dc.buf[colnum + (yl << 2)];
			dest = R_Address(temp_dc.startx + colnum, yl);
			count = temp_dc.commontop - yl;

			while (--count >= 0)
			{
				*dest = *source;
				source += 4;
				dest += stride;
			}
		}

		// flush column tail
		if (yh > temp_dc.commonbot)
		{
			source = &temp_dc.buf[colnum + ((temp_dc.commonbot + 1) << 2)];
			dest = R_Address(temp_dc.startx + colnum, temp_dc.commonbot + 1);
			count = yh - temp_dc.commonbot;

			while (--count >= 0)
			{
				*dest = *source;
				source += 4;
				dest += stride;
			}
		}
		++colnum;
	}
}

static void R_FlushQuadOpaque(void)
{
	UINT8 *source = &temp_dc.buf[temp_dc.commontop << 2];
	UINT8 *dest = R_Address(temp_dc.startx, temp_dc.commontop);
	INT32 count;
	const restrict INT32 stride = vid.width;

	count = temp_dc.commonbot - temp_dc.commontop + 1;

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

static void R_FlushWholeTrans(void)
{
	UINT8 *source;
	UINT8 *dest;
	INT32 count, yl;
	const restrict INT32 stride = vid.width;

	while (--temp_dc.x >= 0)
	{
		yl = temp_dc.yl[temp_dc.x];
		source = &temp_dc.buf[temp_dc.x + (yl << 2)];
		dest = R_Address(temp_dc.startx + temp_dc.x, yl);
		count = temp_dc.yh[temp_dc.x] - yl + 1;

		while (--count >= 0)
		{
			*dest = temp_dc.tranmap[(*dest << 8) + *source];
			source += 4;
			dest += stride;
		}
	}
}

static void R_FlushHTTrans(void)
{
	UINT8 *source;
	UINT8 *dest;
	INT32 count;
	INT32 colnum = 0, yl, yh;
	const restrict INT32 stride = vid.width;

	while (colnum < 4)
	{
		yl = temp_dc.yl[colnum];
		yh = temp_dc.yh[colnum];

		// flush column head
		if (yl < temp_dc.commontop)
		{
			source = &temp_dc.buf[colnum + (yl << 2)];
			dest = R_Address(temp_dc.startx + colnum, yl);
			count = temp_dc.commontop - yl;

			while (--count >= 0)
			{
				*dest = temp_dc.tranmap[(*dest << 8) + *source];
				source += 4;
				dest += stride;
			}
		}

		// flush column tail
		if (yh > temp_dc.commonbot)
		{
			source = &temp_dc.buf[colnum + ((temp_dc.commonbot + 1) << 2)];
			dest = R_Address(temp_dc.startx + colnum, temp_dc.commonbot + 1);
			count = yh - temp_dc.commonbot;

			while (--count >= 0)
			{
				*dest = temp_dc.tranmap[(*dest << 8) + *source];
				source += 4;
				dest += stride;
			}
		}

		++colnum;
	}
}

static void R_FlushQuadTrans(void)
{
	UINT8 *source = &temp_dc.buf[temp_dc.commontop << 2];
	UINT8 *dest = R_Address(temp_dc.startx, temp_dc.commontop);
	INT32 count;
	const restrict INT32 stride = vid.width;

	count = temp_dc.commonbot - temp_dc.commontop + 1;

	while (--count >= 0)
	{
		dest[0] = temp_dc.tranmap[(dest[0] << 8) + source[0]];
		dest[1] = temp_dc.tranmap[(dest[1] << 8) + source[1]];
		dest[2] = temp_dc.tranmap[(dest[2] << 8) + source[2]];
		dest[3] = temp_dc.tranmap[(dest[3] << 8) + source[3]];
		source += 4;
		dest += stride;
	}
}

static void R_FlushWholeColormap(void)
{
	UINT8 *source;
	UINT8 *dest;
	INT32 count, yl;
	const restrict INT32 stride = vid.width;

	while (--temp_dc.x >= 0)
	{
		yl = temp_dc.yl[temp_dc.x];
		source = &temp_dc.buf[temp_dc.x + (yl << 2)];
		dest = R_Address(temp_dc.startx + temp_dc.x, yl);
		count = temp_dc.yh[temp_dc.x] - yl + 1;

		while (--count >= 0)
		{
			*dest = temp_dc.translation[*source];
			source += 4;
			dest += stride;
		}
	}
}

static void R_FlushHTColormap(void)
{
	UINT8 *source;
	UINT8 *dest;
	INT32 count;
	INT32 colnum = 0, yl, yh;
	const restrict INT32 stride = vid.width;

	while (colnum < 4)
	{
		yl = temp_dc.yl[colnum];
		yh = temp_dc.yh[colnum];

		// flush column head
		if (yl < temp_dc.commontop)
		{
			source = &temp_dc.buf[colnum + (yl << 2)];
			dest = R_Address(temp_dc.startx + colnum, yl);
			count = temp_dc.commontop - yl;

			while (--count >= 0)
			{
				*dest = temp_dc.translation[*source];
				source += 4;
				dest += stride;
			}
		}

		// flush column tail
		if (yh > temp_dc.commonbot)
		{
			source = &temp_dc.buf[colnum + ((temp_dc.commonbot + 1) << 2)];
			dest = R_Address(temp_dc.startx + colnum, temp_dc.commonbot + 1);
			count = yh - temp_dc.commonbot;

			while (--count >= 0)
			{
				*dest = temp_dc.translation[*source];
				source += 4;
				dest += stride;
			}
		}

		++colnum;
	}
}

static void R_FlushQuadColormap(void)
{
	UINT8 *source = &temp_dc.buf[temp_dc.commontop << 2];
	UINT8 *dest = R_Address(temp_dc.startx, temp_dc.commontop);
	INT32 count;
	const restrict INT32 stride = vid.width;

	count = temp_dc.commonbot - temp_dc.commontop + 1;

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

static void R_FlushWholeColormapTrans(void)
{
	UINT8 *source;
	UINT8 *dest;
	INT32 count, yl;
	const restrict INT32 stride = vid.width;

	while (--temp_dc.x >= 0)
	{
		yl = temp_dc.yl[temp_dc.x];
		source = &temp_dc.buf[temp_dc.x + (yl << 2)];
		dest = R_Address(temp_dc.startx + temp_dc.x, yl);
		count = temp_dc.yh[temp_dc.x] - yl + 1;

		while (--count >= 0)
		{
			*dest = temp_dc.tranmap[(*dest << 8) + temp_dc.translation[*source]];
			source += 4;
			dest += stride;
		}
	}
}

static void R_FlushHTColormapTrans(void)
{
	UINT8 *source;
	UINT8 *dest;
	INT32 count;
	INT32 colnum = 0, yl, yh;
	const restrict INT32 stride = vid.width;

	while (colnum < 4)
	{
		yl = temp_dc.yl[colnum];
		yh = temp_dc.yh[colnum];

		// flush column head
		if (yl < temp_dc.commontop)
		{
			source = &temp_dc.buf[colnum + (yl << 2)];
			dest = R_Address(temp_dc.startx + colnum, yl);
			count = temp_dc.commontop - yl;

			while (--count >= 0)
			{
				*dest = temp_dc.tranmap[(*dest << 8) + temp_dc.translation[*source]];
				source += 4;
				dest += stride;
			}
		}

		// flush column tail
		if (yh > temp_dc.commonbot)
		{
			source = &temp_dc.buf[colnum + ((temp_dc.commonbot + 1) << 2)];
			dest = R_Address(temp_dc.startx + colnum, temp_dc.commonbot + 1);
			count = yh - temp_dc.commonbot;

			while (--count >= 0)
			{
				*dest = temp_dc.tranmap[(*dest << 8) + temp_dc.translation[*source]];
				source += 4;
				dest += stride;
			}
		}

		++colnum;
	}
}

static void R_FlushQuadColormapTrans(void)
{
	UINT8 *source = &temp_dc.buf[temp_dc.commontop << 2];
	UINT8 *dest = R_Address(temp_dc.startx, temp_dc.commontop);
	INT32 count;
	const restrict INT32 stride = vid.width;

	count = temp_dc.commonbot - temp_dc.commontop + 1;

	while (--count >= 0)
	{
		dest[0] = temp_dc.tranmap[(dest[0] << 8) + temp_dc.translation[source[0]]];
		dest[1] = temp_dc.tranmap[(dest[1] << 8) + temp_dc.translation[source[1]]];
		dest[2] = temp_dc.tranmap[(dest[2] << 8) + temp_dc.translation[source[2]]];
		dest[3] = temp_dc.tranmap[(dest[3] << 8) + temp_dc.translation[source[3]]];
		source += 4;
		dest += stride;
	}
}

static UINT8 *R_GetBufferOpaque(drawcolumndata_t *dc)
{
	if (temp_dc.x == 4 ||
		(temp_dc.x && (temp_dc.type != FLUSH_OPAQUE || temp_dc.x + temp_dc.startx != dc->x)))
		R_FlushColumns();

	if (!temp_dc.x)
	{
		temp_dc.startx = dc->x;
		temp_dc.yl[0] = temp_dc.commontop = dc->yl;
		temp_dc.yh[0] = temp_dc.commonbot = dc->yh;
		temp_dc.type = FLUSH_OPAQUE;
		R_FlushWholeColumns = R_FlushWholeOpaque;
		R_FlushHTColumns = R_FlushHTOpaque;
		R_FlushQuadColumn = R_FlushQuadOpaque;
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

static UINT8 *R_GetBufferTrans(drawcolumndata_t *dc)
{
	if (temp_dc.x == 4 || dc->transmap != temp_dc.tranmap ||
		(temp_dc.x && (temp_dc.type != FLUSH_TRANS || temp_dc.x + temp_dc.startx != dc->x)))
		R_FlushColumns();

	if (!temp_dc.x)
	{
		temp_dc.startx = dc->x;
		temp_dc.yl[0] = temp_dc.commontop = dc->yl;
		temp_dc.yh[0] = temp_dc.commonbot = dc->yh;
		temp_dc.type = FLUSH_TRANS;
		temp_dc.tranmap = dc->transmap;
		R_FlushWholeColumns = R_FlushWholeTrans;
		R_FlushHTColumns = R_FlushHTTrans;
		R_FlushQuadColumn = R_FlushQuadTrans;
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

static UINT8 *R_GetBufferColormap(drawcolumndata_t *dc)
{
	if (temp_dc.x == 4 || dc->translation != temp_dc.translation ||
		(temp_dc.x && (temp_dc.type != FLUSH_COLORMAP || temp_dc.x + temp_dc.startx != dc->x)))
		R_FlushColumns();

	if (!temp_dc.x)
	{
		temp_dc.startx = dc->x;
		temp_dc.yl[0] = temp_dc.commontop = dc->yl;
		temp_dc.yh[0] = temp_dc.commonbot = dc->yh;
		temp_dc.type = FLUSH_COLORMAP;
		temp_dc.translation = dc->translation;
		R_FlushWholeColumns = R_FlushWholeColormap;
		R_FlushHTColumns = R_FlushHTColormap;
		R_FlushQuadColumn = R_FlushQuadColormap;
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

static UINT8 *R_GetBufferColormapTrans(drawcolumndata_t *dc)
{
	if (temp_dc.x == 4 || dc->translation != temp_dc.translation || dc->transmap != temp_dc.tranmap ||
		(temp_dc.x && (temp_dc.type != FLUSH_COLORMAP_TRANS || temp_dc.x + temp_dc.startx != dc->x)))
		R_FlushColumns();

	if (!temp_dc.x)
	{
		temp_dc.startx = dc->x;
		temp_dc.yl[0] = temp_dc.commontop = dc->yl;
		temp_dc.yh[0] = temp_dc.commonbot = dc->yh;
		temp_dc.type = FLUSH_COLORMAP_TRANS;
		temp_dc.translation = dc->translation;
		temp_dc.tranmap = dc->transmap;
		R_FlushWholeColumns = R_FlushWholeColormapTrans;
		R_FlushHTColumns = R_FlushHTColormapTrans;
		R_FlushQuadColumn = R_FlushQuadColormapTrans;
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
