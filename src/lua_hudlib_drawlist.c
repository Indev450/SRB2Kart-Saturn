// SONIC ROBO BLAST 2
//-----------------------------------------------------------------------------
// Copyright (C) 2014-2016 by John "JTE" Muniz.
// Copyright (C) 2014-2022 by Sonic Team Junior.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  lua_hudlib_drawlist.c
/// \brief a data structure for managing cached drawlists for the Lua hud lib

#include "lua_hudlib_drawlist.h"
#include "lua_hud.h"
#include "blua/lstate.h" // shhhhhh
#include "lua_libs.h"

#include <string.h>

#include "v_video.h"
#include "z_zone.h"
#include "r_main.h"
#include "r_fps.h"

enum drawitem_e {
	DI_Draw = 0,
	DI_DrawScaled,
	DI_DrawStretched,
	DI_DrawNum,
	DI_DrawPaddedNum,
	DI_DrawPingNum,
	DI_DrawFill,
	DI_DrawString,
	DI_DrawKartString,
	DI_DrawLevelTitle,
	DI_FadeScreen,
	DI_MAX,
};

// A single draw item with all possible arguments needed for a draw call.
typedef struct drawitem_s {
	UINT64 id;
	enum drawitem_e type;
	fixed_t x;
	fixed_t y;
	union {
		INT32 flags;
		INT32 c;
	};

	union {
		fixed_t scale;  // drawScaled
		fixed_t hscale; // drawStretched
		fixed_t w;      // drawFill
		INT32 align;    // drawString
		INT32 num;      // drawNum, drawPaddedNum, drawPingNum
		UINT16 color;   // fadeScreen
	};
	union {
		fixed_t vscale; // drawStretched
		fixed_t h;      // drawFill
		INT32 digits;   // drawPaddedNum
		UINT8 strength; // fadeScreen
	};

	// pointers (and size_t's) last, for potentially better packing
	union {
		patch_t *patch;
		size_t stroffset; // offset into strbuf to get str
	};
	const UINT8 *colormap;
	INT32 blend;
} drawitem_t;

// The internal structure of a drawlist.
struct huddrawlist_s {
	drawitem_t *items;
	size_t items_len;
	drawitem_t *olditems;
	size_t olditems_len;
	size_t capacity;
	char *strbuf;
	size_t strbuf_capacity;
	size_t strbuf_len;
};

// alignment types for v.drawString
enum align {
	align_left = 0,
	align_center,
	align_right,
	align_fixed,
	align_small,
	align_smallright,
	align_thin,
	align_thinright
};

huddrawlist_h LUA_HUD_CreateDrawList(void)
{
	huddrawlist_h drawlist;

	drawlist = (huddrawlist_h) Z_Calloc(sizeof(struct huddrawlist_s), PU_STATIC, NULL);
	drawlist->items = NULL;
	drawlist->items_len = 0;
	drawlist->olditems = NULL;
	drawlist->olditems_len = 0;
	drawlist->capacity = 0;
	drawlist->strbuf = NULL;
	drawlist->strbuf_capacity = 0;
	drawlist->strbuf_len = 0;

	return drawlist;
}

void LUA_HUD_ClearDrawList(huddrawlist_h list)
{
	// swap the old and new lists
	void *tmp = list->olditems;
	list->olditems = list->items;
	list->items = tmp;

	// rather than deallocate, we'll just save the existing allocation and empty
	// it out for reuse
	list->olditems_len = list->items_len;
	list->items_len = 0;

	if (list->strbuf)
	{
		list->strbuf[0] = 0;
	}
	list->strbuf_len = 0;
}

void LUA_HUD_DestroyDrawList(huddrawlist_h list)
{
	if (list == NULL) return;

	if (list->items)
	{
		Z_Free(list->items);
	}
	if (list->olditems)
	{
		Z_Free(list->olditems);
	}
	if (list->strbuf)
	{
		Z_Free(list->strbuf);
	}
	Z_Free(list);
}

boolean LUA_HUD_IsDrawListValid(huddrawlist_h list)
{
	if (!list) return false;

	// that's all we can really do to check the validity of the handle right now
	return true;
}

static size_t AllocateDrawItem(huddrawlist_h list)
{
	if (!list) I_Error("can't allocate draw item: invalid list");
	if (list->capacity <= list->items_len)
	{
		list->capacity = list->capacity == 0 ? 128 : list->capacity * 2;
		list->items = Z_Realloc(list->items, sizeof(struct drawitem_s) * list->capacity, PU_STATIC, NULL);
		list->olditems = Z_Realloc(list->olditems, sizeof(struct drawitem_s) * list->capacity, PU_STATIC, NULL);
	}

	return list->items_len++;
}

// copy string to list's internal string buffer
// lua can deallocate the string before we get to use it, so it's important to
// keep our own copy
static size_t CopyString(huddrawlist_h list, const char* str)
{
	size_t lenstr;

	if (!list) I_Error("can't allocate string; invalid list");
	lenstr = strlen(str);
	if (list->strbuf_capacity <= list->strbuf_len + lenstr + 1)
	{
		if (list->strbuf_capacity == 0) list->strbuf_capacity = 256;
		while (list->strbuf_capacity <= list->strbuf_len + lenstr + 1)
			list->strbuf_capacity *= 2;
		list->strbuf = (char*) Z_Realloc(list->strbuf, sizeof(char) * list->strbuf_capacity, PU_STATIC, NULL);
	}

	{
		size_t old_len = list->strbuf_len;
		strncpy(&list->strbuf[old_len], str, lenstr + 1);
		list->strbuf_len += lenstr + 1;
		return old_len;
	}
}

static void CalcStringCoords(drawitem_t *item, const char *string)
{
	if (!(item->flags & V_NOSCALESTART) && cv_uncappedhud.value)
	{
		INT32 dupx = vid.dupx;
		INT32 dupy = vid.dupy;

		item->flags |= V_NOSCALESTART;

		if (item->flags & V_SCALEPATCHMASK) switch ((item->flags & V_SCALEPATCHMASK) >> V_SCALEPATCHSHIFT)
		{
			case 1: // V_NOSCALEPATCH
				dupx = dupy = 1;
				break;
			case 2: // V_SMALLSCALEPATCH
				dupx = vid.smalldupx;
				dupy = vid.smalldupy;
				break;
			case 3: // V_MEDSCALEPATCH
				dupx = vid.meddupx;
				dupy = vid.meddupy;
				break;
			default:
				break;
		}

		// only use one dup, to avoid stretching (har har)
		dupx = dupy = (dupx < dupy ? dupx : dupy);

		INT32 x = item->x * dupx;
		INT32 y = item->y * dupy;

		if (item->flags & V_SPLITSCREEN)
			y += (BASEVIDHEIGHT * (dupy - 1))/2;

		if (item->flags & V_HORZSCREEN)
			x += (BASEVIDWIDTH * (dupx - 1))/2;

		if (vid.width != BASEVIDWIDTH * dupx)
		{
			// dupx adjustments pretend that screen width is BASEVIDWIDTH * dupx,
			// so center this imaginary screen
			if ((item->flags & (V_HORZSCREEN|V_SNAPTOLEFT)) == (V_HORZSCREEN|V_SNAPTOLEFT))
				x += (vid.width/2 - (BASEVIDWIDTH/2 * dupx));
			else if (item->flags & V_SNAPTORIGHT)
				x += (vid.width - (BASEVIDWIDTH * dupx));
			else if (!(item->flags & V_SNAPTOLEFT))
				x += (vid.width - (BASEVIDWIDTH * dupx)) / 2;
		}
		if (vid.height != BASEVIDHEIGHT * dupy)
		{
			// same thing here
			if ((item->flags & (V_SPLITSCREEN|V_SNAPTOTOP)) == (V_SPLITSCREEN|V_SNAPTOTOP))
				y += (vid.height/2 - (BASEVIDHEIGHT/2 * dupy));
			else if (item->flags & V_SNAPTOBOTTOM)
				y += (vid.height - (BASEVIDHEIGHT * dupy));
			else if (!(item->flags & V_SNAPTOTOP))
				y += (vid.height - (BASEVIDHEIGHT * dupy)) / 2;
		}

		// need to compensate for dup here
		if (item->type == DI_DrawString)
		{
			if (item->align == align_center)
				x -= (V_StringWidth(string, item->flags) * (dupx - 1))/2;
			if (item->align == align_right)
				x -= V_StringWidth(string, item->flags) * (dupx - 1);
			if (item->align == align_smallright)
				x -= V_SmallStringWidth(string, item->flags) * (dupx - 1);
			if (item->align == align_thinright)
				x -= V_ThinStringWidth(string, item->flags) * (dupx - 1);
		}

		item->x = x;
		item->y = y;
	}
}

static void CalcFillCoords(drawitem_t *item)
{
	if (!(item->flags & V_NOSCALESTART) && cv_uncappedhud.value)
	{
		INT32 dupx = vid.dupx;
		INT32 dupy = vid.dupy;
		INT32 x = item->x * dupx;
		INT32 y = item->y * dupy;
		INT32 c = item->c;

		item->flags |= V_NOSCALESTART;
		item->w *= dupx;
		item->h *= dupy;

		// Center it if necessary
		if (vid.width != BASEVIDWIDTH * dupx)
		{
			// dupx adjustments pretend that screen width is BASEVIDWIDTH * dupx,
			// so center this imaginary screen
			if (c & V_SNAPTORIGHT)
				x += (vid.width - (BASEVIDWIDTH * dupx));
			else if (!(c & V_SNAPTOLEFT))
				x += (vid.width - (BASEVIDWIDTH * dupx)) / 2;
		}
		if (vid.height != BASEVIDHEIGHT * dupy)
		{
			// same thing here
			if (c & V_SNAPTOBOTTOM)
				y += (vid.height - (BASEVIDHEIGHT * dupy));
			else if (!(c & V_SNAPTOTOP))
				y += (vid.height - (BASEVIDHEIGHT * dupy)) / 2;
		}
		if (c & V_SPLITSCREEN)
			y += (BASEVIDHEIGHT * dupy)/2;
		if (c & V_HORZSCREEN)
			x += (BASEVIDWIDTH * dupx)/2;

		item->x = x;
		item->y = y;
	}
}

#define INTERP_LATCH 1
#define INTERP_STRING 2
#define INTERP_FLAGMASK 0x3

static UINT64 GetItemId(void)
{
	if (!hud_interpolate)
		return 0;

	// leave bits 0 and 1 free for the string mode
	UINT64 id = ((UINT64)(uintptr_t)gL->savedpc << 32) | (hud_interpcounter << 10) | (hud_interptag << 2);

	if (hud_interplatch)
	{
		id |= INTERP_LATCH;
		hud_interplatch = false;
	}
	if (hud_interpstring)
		id |= INTERP_STRING;

	return id;
}

void LUA_HUD_AddDraw(
	huddrawlist_h list,
	INT32 x,
	INT32 y,
	patch_t *patch,
	INT32 flags,
	UINT8 *colormap,
	INT32 blend
)
{
	size_t i = AllocateDrawItem(list);
	drawitem_t *item = &list->items[i];
	item->id = GetItemId();
	item->type = DI_Draw;
	item->x = x << FRACBITS;
	item->y = y << FRACBITS;
	item->patch = patch;
	item->flags = flags;
	item->colormap = colormap;
	item->blend = blend;
}

void LUA_HUD_AddDrawScaled(
	huddrawlist_h list,
	fixed_t x,
	fixed_t y,
	fixed_t scale,
	patch_t *patch,
	INT32 flags,
	UINT8 *colormap,
	INT32 blend
)
{
	size_t i = AllocateDrawItem(list);
	drawitem_t *item = &list->items[i];
	item->id = GetItemId();
	item->type = DI_DrawScaled;
	item->x = x;
	item->y = y;
	item->scale = scale;
	item->patch = patch;
	item->flags = flags;
	item->colormap = colormap;
	item->blend = blend;
}

void LUA_HUD_AddDrawStretched(
	huddrawlist_h list,
	fixed_t x,
	fixed_t y,
	fixed_t hscale,
	fixed_t vscale,
	patch_t *patch,
	INT32 flags,
	UINT8 *colormap,
	INT32 blend
)
{
	size_t i = AllocateDrawItem(list);
	drawitem_t *item = &list->items[i];
	item->id = GetItemId();
	item->type = DI_DrawStretched;
	item->x = x;
	item->y = y;
	item->hscale = hscale;
	item->vscale = vscale;
	item->patch = patch;
	item->flags = flags;
	item->colormap = colormap;
	item->blend = blend;
}

void LUA_HUD_AddDrawNum(
	huddrawlist_h list,
	INT32 x,
	INT32 y,
	INT32 num,
	INT32 flags
)
{
	size_t i = AllocateDrawItem(list);
	drawitem_t *item = &list->items[i];
	item->id = GetItemId();
	item->type = DI_DrawNum;
	item->x = x;
	item->y = y;
	item->num = num;
	item->flags = flags;
	CalcStringCoords(item, NULL);
}

void LUA_HUD_AddDrawPaddedNum(
	huddrawlist_h list,
	INT32 x,
	INT32 y,
	INT32 num,
	INT32 digits,
	INT32 flags
)
{
	size_t i = AllocateDrawItem(list);
	drawitem_t *item = &list->items[i];
	item->id = GetItemId();
	item->type = DI_DrawPaddedNum;
	item->x = x;
	item->y = y;
	item->num = num;
	item->digits = digits;
	item->flags = flags;
	CalcStringCoords(item, NULL);
}

void LUA_HUD_AddDrawPingNum(
	huddrawlist_h list,
	INT32 x,
	INT32 y,
	INT32 num,
	INT32 flags,
	const UINT8 *colormap
)
{
	size_t i = AllocateDrawItem(list);
	drawitem_t *item = &list->items[i];
	item->id = GetItemId();
	item->type = DI_DrawPingNum;
	item->x = x;
	item->y = y;
	item->num = num;
	item->flags = flags;
	item->colormap = colormap;
	CalcStringCoords(item, NULL);
}

void LUA_HUD_AddDrawFill(
	huddrawlist_h list,
	INT32 x,
	INT32 y,
	INT32 w,
	INT32 h,
	INT32 c
)
{
	size_t i = AllocateDrawItem(list);
	drawitem_t *item = &list->items[i];
	item->id = GetItemId();
	item->type = DI_DrawFill;
	item->x = x;
	item->y = y;
	item->w = w;
	item->h = h;
	item->c = c;
	CalcFillCoords(item);
}

void LUA_HUD_AddDrawString(
	huddrawlist_h list,
	fixed_t x,
	fixed_t y,
	const char *str,
	INT32 flags,
	INT32 align
)
{
	size_t i = AllocateDrawItem(list);
	drawitem_t *item = &list->items[i];
	item->id = GetItemId();
	item->type = DI_DrawString;
	item->x = x;
	item->y = y;
	item->stroffset = CopyString(list, str);
	item->flags = flags;
	item->align = align;
	if (item->align != align_fixed)
		CalcStringCoords(item, str);
}

void LUA_HUD_AddDrawKartString(
	huddrawlist_h list,
	fixed_t x,
	fixed_t y,
	const char *str,
	INT32 flags
)
{
	size_t i = AllocateDrawItem(list);
	drawitem_t *item = &list->items[i];
	item->id = GetItemId();
	item->type = DI_DrawKartString;
	item->x = x;
	item->y = y;
	item->stroffset = CopyString(list, str);
	item->flags = flags;
	CalcStringCoords(item, str);
}

void LUA_HUD_AddDrawLevelTitle(
	huddrawlist_h list,
	INT32 x,
	INT32 y,
	const char *str,
	INT32 flags
)
{
	size_t i = AllocateDrawItem(list);
	drawitem_t *item = &list->items[i];
	item->id = GetItemId();
	item->type = DI_DrawLevelTitle;
	item->x = x;
	item->y = y;
	item->stroffset = CopyString(list, str);
	item->flags = flags;
	CalcStringCoords(item, str);
}

void LUA_HUD_AddFadeScreen(
	huddrawlist_h list,
	UINT16 color,
	UINT8 strength
)
{
	size_t i = AllocateDrawItem(list);
	drawitem_t *item = &list->items[i];
	// nothing to interpolate here
	item->id = 0;
	item->type = DI_FadeScreen;
	item->color = color;
	item->strength = strength;
}

void LUA_HUD_DrawList(huddrawlist_h list)
{
	size_t i;
	size_t j = 0;
	fixed_t frac = R_UsingFrameInterpolation() ? rendertimefrac : FRACUNIT;
	fixed_t lerpx = 0, lerpy = 0;
	drawitem_t *latchitem = NULL;
	drawitem_t *oldlatchitem = NULL;

	if (!list) I_Error("HUD drawlist invalid");
	if (list->items_len <= 0) return;
	if (!list->items) I_Error("HUD drawlist->items invalid");

	for (i = 0; i < list->items_len; i++)
	{
		drawitem_t *item = &list->items[i];
		drawitem_t *olditem = NULL;
		const char *itemstr = &list->strbuf[item->stroffset];

		if (item->id)
		{
			// find the old one too
			// this is kinda cursed... we need to check every item
			// but stop when the first-checked item is reached again
			size_t stop = j;
			do
			{
				if (j == list->olditems_len)
				{
					j = 0;
					continue;
				}
				drawitem_t *old = &list->olditems[j++];
				if ((old->id & ~INTERP_FLAGMASK) == (item->id & ~INTERP_FLAGMASK))
				{
					// gotcha!
					olditem = old;
					if (item->id & INTERP_LATCH)
					{
						lerpx = FixedMul(frac, item->x - olditem->x);
						lerpy = FixedMul(frac, item->y - olditem->y);
						latchitem = item;
						oldlatchitem = olditem;
					}
					else if (!(item->id & INTERP_STRING))
					{
						lerpx = FixedMul(frac, item->x - olditem->x);
						lerpy = FixedMul(frac, item->y - olditem->y);
						latchitem = NULL;
					}
					break;
				}
			}
			while (j != stop);
		}
		else
		{
			lerpx = lerpy = 0;
			latchitem = NULL;
		}

		#define LERP(it) (olditem ? olditem->it + FixedMul(frac, item->it - olditem->it) : item->it)

		// half of this could be done when the coordinates are latched... zzzzzz
		#define LERPS(it) (latchitem ? ((latchitem->it + (item->it - latchitem->it)) - (latchitem->it - oldlatchitem->it)) + lerp##it : (olditem ? olditem->it + lerp##it : item->it))
		//define LERPS(it) (olditem ? (latchitem ? (latchitem->it + (item->it - latchitem->it)) - (latchitem->it - oldlatchitem->it) : olditem->it) + lerp##it : item->it)

		switch (item->type)
		{
			case DI_Draw:
				if (!item->patch || item->patch == NULL)
					return;
				V_DrawBlendingFixedPatch(LERPS(x), LERPS(y), FRACUNIT, item->flags, item->patch, item->colormap, item->blend);
				break;
			case DI_DrawScaled:
				if (!item->patch || item->patch == NULL)
					return;
				V_DrawBlendingFixedPatch(LERPS(x), LERPS(y), LERP(scale), item->flags, item->patch, item->colormap, item->blend);
				break;
			case DI_DrawStretched:
				if (!item->patch || item->patch == NULL)
					return;
				V_DrawStretchyFixedPatch(LERPS(x), LERPS(y), LERP(hscale), LERP(vscale), item->flags, item->patch, item->colormap, item->blend);
				break;
			case DI_DrawNum:
				V_DrawTallNum(LERPS(x), LERPS(y), item->flags, item->num);
				break;
			case DI_DrawPaddedNum:
				V_DrawPaddedTallNum(LERPS(x), LERPS(y), item->flags, item->num, item->digits);
				break;
			case DI_DrawPingNum:
				V_DrawPingNum(LERPS(x), LERPS(y), item->flags, item->num, item->colormap);
				break;
			case DI_DrawFill:
				V_DrawFill(LERPS(x), LERPS(y), LERP(w), LERP(h), item->c);
				break;
			case DI_DrawString:
				switch(item->align)
				{
				// hu_font
				case align_left:
					V_DrawString(LERPS(x), LERPS(y), item->flags, itemstr);
					break;
				case align_center:
					V_DrawCenteredString(LERPS(x), LERPS(y), item->flags, itemstr);
					break;
				case align_right:
					V_DrawRightAlignedString(LERPS(x), LERPS(y), item->flags, itemstr);
					break;
				case align_fixed:
					V_DrawStringAtFixed(LERPS(x), LERPS(y), item->flags, itemstr);
					break;
				// hu_font, 0.5x scale
				case align_small:
					V_DrawSmallString(LERPS(x), LERPS(y), item->flags, itemstr);
					break;
				case align_smallright:
					V_DrawRightAlignedSmallString(LERPS(x), LERPS(y), item->flags, itemstr);
					break;
				// tny_font
				case align_thin:
					V_DrawThinString(LERPS(x), LERPS(y), item->flags, itemstr);
					break;
				case align_thinright:
					V_DrawRightAlignedThinString(LERPS(x), LERPS(y), item->flags, itemstr);
					break;
				}
				break;
			case DI_DrawKartString:
				V_DrawKartString(LERPS(x), LERPS(y), item->flags, itemstr);
				break;
			case DI_DrawLevelTitle:
				V_DrawLevelTitle(LERPS(x), LERPS(y), item->flags, itemstr);
				break;
			case DI_FadeScreen:
				V_DrawFadeScreen(item->color, item->strength);
				break;
			default:
				I_Error("can't draw draw list item: invalid draw list item type");
				continue;
		}
	}
}
