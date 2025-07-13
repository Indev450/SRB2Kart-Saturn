// SONIC ROBO BLAST 2 KART
//-----------------------------------------------------------------------------
// Copyright (C) 1993-1996 by id Software, Inc.
// Copyright (C) 1998-2000 by DooM Legacy Team.
// Copyright (C) 1999-2018 by Sonic Team Junior.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  r_things.h
/// \brief Rendering of moving objects, sprites

#ifndef __R_THINGS__
#define __R_THINGS__

#ifdef __cplusplus
extern "C" {
#endif

#include "r_plane.h"
#include "r_portal.h"

// number of sprite lumps for spritewidth,offset,topoffset lookup tables
// Fab: this is a hack : should allocate the lookup tables per sprite
#if defined(__x86_64__) || defined(__amd64__) || defined(__aarch64__) || defined(__arm64__) // only for 64bit (idk how else to proper check lmao)
#define MAXVISSPRITES 4096
#else
#define MAXVISSPRITES 2048 // added 2-2-98 was 128
#endif

#define VISSPRITECHUNKBITS 6	// 2^6 = 64 sprites per chunk
#define VISSPRITESPERCHUNK (1 << VISSPRITECHUNKBITS)
#define VISSPRITEINDEXMASK (VISSPRITESPERCHUNK - 1)

#define FEETADJUST (4<<FRACBITS) // R_AddSingleSpriteDef

// Takes 2 fixed-point coordinates, returns "distance" between them and camera,
// as an non-fixed-point integer.
// It is very rough, tho it is used only for optimizing out unnecessary
// interpolation, so it is kinda ok on big distances.

#ifdef __cplusplus
#define R_QuickCamDist(x, y) std::max(std::abs(((x)>>FRACBITS) - (viewx>>FRACBITS)), std::abs(((y)>>FRACBITS) - (viewy>>FRACBITS)))
#else
#define R_QuickCamDist(x, y) max(abs(((x)>>FRACBITS) - (viewx>>FRACBITS)), abs(((y)>>FRACBITS) - (viewy>>FRACBITS)))
#endif

// Constant arrays used for psprite clipping
//  and initializing clipping.
extern INT16 *negonearray;
extern INT16 *screenheightarray;

// vars for R_DrawMaskedColumn
extern INT16 *mfloorclip;
extern INT16 *mceilingclip;
extern fixed_t spryscale;
extern fixed_t sprtopscreen;
extern fixed_t sprbotscreen;
extern fixed_t windowtop;
extern fixed_t windowbottom;
extern INT32 lengthcol;

INT32 R_ThingLightLevel(mobj_t *thing);
fixed_t R_GetSpriteDirectionalLighting(angle_t angle);

fixed_t R_GetShadowZ(mobj_t *thing, pslope_t **shadowslope);

//SoM: 6/5/2000: Light sprites correctly!
boolean R_AddSingleSpriteDef(const char *sprname, spritedef_t *spritedef, UINT16 wadnum, UINT16 startlump, UINT16 endlump);
void R_AddSprites(sector_t *sec, INT32 lightlevel);
void R_AddPrecipitationSprites(void);
void R_InitSprites(void);
void R_ClearSprites(void);

/** Used to count the amount of masked elements
 * per portal to later group them in separate
 * drawnode lists.
 */
typedef struct
{
	size_t drawsegs[2];
	size_t vissprites[2];
	fixed_t viewx, viewy, viewz;			/**< View z stored at the time of the BSP traversal for the view/portal. Masked sorting/drawing needs it. */
	sector_t* viewsector;
} maskcount_t;

void R_DrawMasked(maskcount_t* masks, INT32 nummasks);

transnum_t R_GetThingTransTable(fixed_t alpha, transnum_t transmap);

boolean R_ThingVisible(mobj_t *thing);
boolean R_ThingWithinDist(mobj_t *thing, INT32 limit_dist);
boolean R_CheckInterpDist(mobj_t *thing);
fixed_t R_GetThingFade(mobj_t *thing);

boolean R_ThingIsFullBright(mobj_t *thing);
boolean R_ThingIsSemiBright(mobj_t *thing);
boolean R_ThingIsFullDark(mobj_t *thing);

// -----------
// NOT SKINS STUFF !
// -----------
typedef enum
{
	SC_NONE = 0,
	SC_TOP = 1,
	SC_BOTTOM = 2,
	SC_VFLIP = 3,
	SC_NOTVISIBLE = 4,
	SC_FULLBRIGHT = 8,
	SC_SEMIBRIGHT = 16,
	SC_FULLDARK   = 32,
	SC_CUTMASK    = SC_TOP|SC_BOTTOM|SC_NOTVISIBLE,
	SC_FLAGMASK   = ~SC_CUTMASK
} spritecut_e;

// A vissprite_t is a thing that will be drawn during a refresh,
// i.e. a sprite object that is partly visible.
typedef struct vissprite_s
{
	// Doubly linked list.
	struct vissprite_s *prev;
	struct vissprite_s *next;

	mobj_t *mobj; // for easy access

	INT32 x1, x2;

	fixed_t gx, gy; // for line side calculation
	fixed_t gz, gzt; // global bottom/top for silhouette clipping
	fixed_t pz, pzt; // physical bottom/top for sorting with 3D floors

	fixed_t startfrac; // horizontal position of x1
	fixed_t xscale, scale; // projected horizontal and vertical scales
	fixed_t thingscale; // the object's scale
	fixed_t sortscale; // sortscale only differs from scale for flat sprites
	fixed_t scalestep; // only for flat sprites, 0 otherwise
	fixed_t paperoffset, paperdistance; // for paper sprites, offset/dist relative to the angle
	fixed_t xiscale; // negative if flipped

	angle_t centerangle; // for paper sprites

	fixed_t texturemid;
	patch_t *patch;

	lighttable_t *colormap; // for color translation and shadow draw
	                        // maxbright frames as well

	UINT8 *transmap; // for MF2_SHADOW sprites, which translucency table to use

	INT32 mobjflags;

	INT32 heightsec; // height sector for underwater/fake ceiling support

	extracolormap_t *extra_colormap; // global colormaps

	// Precalculated top and bottom screen coords for the sprite.
	fixed_t thingheight; // The actual height of the thing (for 3D floors)
	sector_t *sector; // The sector containing the thing.
	INT16 sz, szt;

	spritecut_e cut;
	UINT32 renderflags;

	fixed_t spritexscale, spriteyscale;
	fixed_t spritexoffset, spriteyoffset;

	INT16 *clipbot, *cliptop;

	boolean precip;
	boolean vflip; // Flip vertically
	boolean isScaled;
	INT32 dispoffset; // copy of info->dispoffset, affects ordering but not drawing
} vissprite_t;

extern UINT32 visspritecount, numvisiblesprites;

void R_ClipSprites(drawseg_t* dsstart, portal_t* portal);

void R_AllocVisSpriteMemory(void);

UINT8 *R_GetSpriteTranslation(vissprite_t *vis);

void R_InitDrawNodes(void);

// ----------
// DRAW NODES
// ----------

// A drawnode is something that points to a 3D floor, 3D side, or masked
// middle texture. This is used for sorting with sprites.
typedef struct drawnode_s
{
	visplane_t *plane;
	drawseg_t *seg;
	drawseg_t *thickseg;
	ffloor_t *ffloor;
	vissprite_t *sprite;

	struct drawnode_s *next;
	struct drawnode_s *prev;
} drawnode_t;

#ifdef __cplusplus
} // extern "C"
#endif

#endif //__R_THINGS__
