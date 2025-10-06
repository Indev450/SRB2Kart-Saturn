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

enum ColumnFlushType
{
	FLUSH_NONE,
	FLUSH_OPAQUE,
	FLUSH_TRANS,
	FLUSH_COLORMAP,
	FLUSH_COLORMAP_TRANS
};

static int temp_x = 0;
static int tempyl[4], tempyh[4];
static UINT8 tempbuf[MAXVIDWIDTH * 4];
static int startx = 0;
static int commontop, commonbot;
static ColumnFlushType temptype = FLUSH_NONE;
static const UINT8 *temptranmap = NULL;
static const UINT8 *temptranslation = NULL;

static void R_FlushColumns(void);

static void R_FlushWholeError(void)
{
	I_Error("R_FlushWholeColumns called without being initialized.\n");
}

static void R_FlushHTError(void)
{
	I_Error("R_FlushHTColumns called without being initialized.\n");
}

static void R_QuadFlushError(void)
{
	I_Error("R_FlushQuadColumn called without being initialized.\n");
}

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

	while (--temp_x >= 0)
	{
		yl = tempyl[temp_x];
		source = &tempbuf[temp_x + (yl << 2)];
		dest = R_Address(startx + temp_x, yl);
		count = tempyh[temp_x] - yl + 1;

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
		yl = tempyl[colnum];
		yh = tempyh[colnum];

		// flush column head
		if (yl < commontop)
		{
			source = &tempbuf[colnum + (yl << 2)];
			dest = R_Address(startx + colnum, yl);
			count = commontop - yl;

			while (--count >= 0)
			{
				*dest = *source;
				source += 4;
				dest += stride;
			}
		}

		// flush column tail
		if (yh > commonbot)
		{
			source = &tempbuf[colnum + ((commonbot + 1) << 2)];
			dest = R_Address(startx + colnum, commonbot + 1);
			count = yh - commonbot;

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
	UINT8 *source = &tempbuf[commontop << 2];
	UINT8 *dest = R_Address(startx, commontop);
	INT32 count;
	const restrict INT32 stride = vid.width;

	count = commonbot - commontop + 1;

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

	while (--temp_x >= 0)
	{
		yl = tempyl[temp_x];
		source = &tempbuf[temp_x + (yl << 2)];
		dest = R_Address(startx + temp_x, yl);
		count = tempyh[temp_x] - yl + 1;

		while (--count >= 0)
		{
			*dest = temptranmap[(*dest << 8) + *source];
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
		yl = tempyl[colnum];
		yh = tempyh[colnum];

		// flush column head
		if (yl < commontop)
		{
			source = &tempbuf[colnum + (yl << 2)];
			dest = R_Address(startx + colnum, yl);
			count = commontop - yl;

			while (--count >= 0)
			{
				*dest = temptranmap[(*dest << 8) + *source];
				source += 4;
				dest += stride;
			}
		}

		// flush column tail
		if (yh > commonbot)
		{
			source = &tempbuf[colnum + ((commonbot + 1) << 2)];
			dest = R_Address(startx + colnum, commonbot + 1);
			count = yh - commonbot;

			while (--count >= 0)
			{
				*dest = temptranmap[(*dest << 8) + *source];
				source += 4;
				dest += stride;
			}
		}

		++colnum;
	}
}

static void R_FlushQuadTrans(void)
{
	UINT8 *source = &tempbuf[commontop << 2];
	UINT8 *dest = R_Address(startx, commontop);
	INT32 count;
	const restrict INT32 stride = vid.width;

	count = commonbot - commontop + 1;

	while (--count >= 0)
	{
		dest[0] = temptranmap[(dest[0] << 8) + source[0]];
		dest[1] = temptranmap[(dest[1] << 8) + source[1]];
		dest[2] = temptranmap[(dest[2] << 8) + source[2]];
		dest[3] = temptranmap[(dest[3] << 8) + source[3]];
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

	while (--temp_x >= 0)
	{
		yl = tempyl[temp_x];
		source = &tempbuf[temp_x + (yl << 2)];
		dest = R_Address(startx + temp_x, yl);
		count = tempyh[temp_x] - yl + 1;

		while (--count >= 0)
		{
			*dest = temptranslation[*source];
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
		yl = tempyl[colnum];
		yh = tempyh[colnum];

		// flush column head
		if (yl < commontop)
		{
			source = &tempbuf[colnum + (yl << 2)];
			dest = R_Address(startx + colnum, yl);
			count = commontop - yl;

			while (--count >= 0)
			{
				*dest = temptranslation[*source];
				source += 4;
				dest += stride;
			}
		}

		// flush column tail
		if (yh > commonbot)
		{
			source = &tempbuf[colnum + ((commonbot + 1) << 2)];
			dest = R_Address(startx + colnum, commonbot + 1);
			count = yh - commonbot;

			while (--count >= 0)
			{
				*dest = temptranslation[*source];
				source += 4;
				dest += stride;
			}
		}

		++colnum;
	}
}

static void R_FlushQuadColormap(void)
{
	UINT8 *source = &tempbuf[commontop << 2];
	UINT8 *dest = R_Address(startx, commontop);
	INT32 count;
	const restrict INT32 stride = vid.width;

	count = commonbot - commontop + 1;

	while (--count >= 0)
	{
		dest[0] = temptranslation[source[0]];
		dest[1] = temptranslation[source[1]];
		dest[2] = temptranslation[source[2]];
		dest[3] = temptranslation[source[3]];
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

	while (--temp_x >= 0)
	{
		yl = tempyl[temp_x];
		source = &tempbuf[temp_x + (yl << 2)];
		dest = R_Address(startx + temp_x, yl);
		count = tempyh[temp_x] - yl + 1;

		while (--count >= 0)
		{
			*dest = temptranmap[(*dest << 8) + temptranslation[*source]];
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
		yl = tempyl[colnum];
		yh = tempyh[colnum];

		// flush column head
		if (yl < commontop)
		{
			source = &tempbuf[colnum + (yl << 2)];
			dest = R_Address(startx + colnum, yl);
			count = commontop - yl;

			while (--count >= 0)
			{
				*dest = temptranmap[(*dest << 8) + temptranslation[*source]];
				source += 4;
				dest += stride;
			}
		}

		// flush column tail
		if (yh > commonbot)
		{
			source = &tempbuf[colnum + ((commonbot + 1) << 2)];
			dest = R_Address(startx + colnum, commonbot + 1);
			count = yh - commonbot;

			while (--count >= 0)
			{
				*dest = temptranmap[(*dest << 8) + temptranslation[*source]];
				source += 4;
				dest += stride;
			}
		}

		++colnum;
	}
}

static void R_FlushQuadColormapTrans(void)
{
	UINT8 *source = &tempbuf[commontop << 2];
	UINT8 *dest = R_Address(startx, commontop);
	INT32 count;
	const restrict INT32 stride = vid.width;

	count = commonbot - commontop + 1;

	while (--count >= 0)
	{
		dest[0] = temptranmap[(dest[0] << 8) + temptranslation[source[0]]];
		dest[1] = temptranmap[(dest[1] << 8) + temptranslation[source[1]]];
		dest[2] = temptranmap[(dest[2] << 8) + temptranslation[source[2]]];
		dest[3] = temptranmap[(dest[3] << 8) + temptranslation[source[3]]];
		source += 4;
		dest += stride;
	}
}

static void (*R_FlushWholeColumns)(void) = R_FlushWholeError;
static void (*R_FlushHTColumns)(void) = R_FlushHTError;
static void (*R_FlushQuadColumn)(void) = R_QuadFlushError;

static void R_FlushColumns(void)
{
	if (temp_x != 4 || commontop >= commonbot)
		R_FlushWholeColumns();
	else
	{
		R_FlushHTColumns();
		R_FlushQuadColumn();
	}

	temp_x = 0;
}

//
// R_ResetColumnBuffer
//
void R_ResetColumnBuffer(void)
{
	if (temp_x)
		R_FlushColumns();

	temptype = FLUSH_NONE;
	R_FlushWholeColumns = R_FlushWholeError;
	R_FlushHTColumns = R_FlushHTError;
	R_FlushQuadColumn = R_QuadFlushError;
}

static UINT8 *R_GetBufferOpaque(drawcolumndata_t *dc)
{
	if (temp_x == 4 ||
		(temp_x && (temptype != FLUSH_OPAQUE || temp_x + startx != dc->x)))
		R_FlushColumns();

	if (!temp_x)
	{
		startx = dc->x;
		tempyl[0] = commontop = dc->yl;
		tempyh[0] = commonbot = dc->yh;
		temptype = FLUSH_OPAQUE;
		R_FlushWholeColumns = R_FlushWholeOpaque;
		R_FlushHTColumns = R_FlushHTOpaque;
		R_FlushQuadColumn = R_FlushQuadOpaque;
		temp_x += 1;
		return &tempbuf[dc->yl << 2];
	}

	tempyl[temp_x] = dc->yl;
	tempyh[temp_x] = dc->yh;

	if (dc->yl > commontop)
		commontop = dc->yl;
	if (dc->yh < commonbot)
		commonbot = dc->yh;

	return &tempbuf[(dc->yl << 2) + temp_x++];
}

static UINT8 *R_GetBufferTrans(drawcolumndata_t *dc)
{
	if (temp_x == 4 || dc->transmap != temptranmap ||
		(temp_x && (temptype != FLUSH_TRANS || temp_x + startx != dc->x)))
		R_FlushColumns();

	if (!temp_x)
	{
		startx = dc->x;
		tempyl[0] = commontop = dc->yl;
		tempyh[0] = commonbot = dc->yh;
		temptype = FLUSH_TRANS;
		temptranmap = dc->transmap;
		R_FlushWholeColumns = R_FlushWholeTrans;
		R_FlushHTColumns = R_FlushHTTrans;
		R_FlushQuadColumn = R_FlushQuadTrans;
		temp_x += 1;
		return &tempbuf[dc->yl << 2];
	}

	tempyl[temp_x] = dc->yl;
	tempyh[temp_x] = dc->yh;

	if (dc->yl > commontop)
		commontop = dc->yl;
	if (dc->yh < commonbot)
		commonbot = dc->yh;

	return &tempbuf[(dc->yl << 2) + temp_x++];
}

static UINT8 *R_GetBufferColormap(drawcolumndata_t *dc)
{
	if (temp_x == 4 || dc->translation != temptranslation ||
		(temp_x && (temptype != FLUSH_COLORMAP || temp_x + startx != dc->x)))
		R_FlushColumns();

	if (!temp_x)
	{
		startx = dc->x;
		tempyl[0] = commontop = dc->yl;
		tempyh[0] = commonbot = dc->yh;
		temptype = FLUSH_COLORMAP;
		temptranslation = dc->translation;
		R_FlushWholeColumns = R_FlushWholeColormap;
		R_FlushHTColumns = R_FlushHTColormap;
		R_FlushQuadColumn = R_FlushQuadColormap;
		temp_x += 1;
		return &tempbuf[dc->yl << 2];
	}

	tempyl[temp_x] = dc->yl;
	tempyh[temp_x] = dc->yh;

	if (dc->yl > commontop)
		commontop = dc->yl;
	if (dc->yh < commonbot)
		commonbot = dc->yh;

	return &tempbuf[(dc->yl << 2) + temp_x++];
}

static UINT8 *R_GetBufferColormapTrans(drawcolumndata_t *dc)
{
	if (temp_x == 4 || dc->translation != temptranslation || dc->transmap != temptranmap ||
		(temp_x && (temptype != FLUSH_COLORMAP_TRANS || temp_x + startx != dc->x)))
		R_FlushColumns();

	if (!temp_x)
	{
		startx = dc->x;
		tempyl[0] = commontop = dc->yl;
		tempyh[0] = commonbot = dc->yh;
		temptype = FLUSH_COLORMAP_TRANS;
		temptranslation = dc->translation;
		temptranmap = dc->transmap;
		R_FlushWholeColumns = R_FlushWholeColormapTrans;
		R_FlushHTColumns = R_FlushHTColormapTrans;
		R_FlushQuadColumn = R_FlushQuadColormapTrans;
		temp_x += 1;
		return &tempbuf[dc->yl << 2];
	}

	tempyl[temp_x] = dc->yl;
	tempyh[temp_x] = dc->yh;

	if (dc->yl > commontop)
		commontop = dc->yl;
	if (dc->yh < commonbot)
		commonbot = dc->yh;

	return &tempbuf[(dc->yl << 2) + temp_x++];
}
