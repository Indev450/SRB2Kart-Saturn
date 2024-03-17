// SONIC ROBO BLAST 2
//-----------------------------------------------------------------------------
// Copyright (C) 1993-1996 by id Software, Inc.
// Copyright (C) 1998-2000 by DooM Legacy Team.
// Copyright (C) 1999-2019 by Sonic Team Junior.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file
/// \brief hardware renderer, using the standard HardWareRender driver DLL for SRB2

#include <math.h>

#include "../doomstat.h"
#include "../doomdef.h"

#include "../qs22j.h"

#ifdef HWRENDER
#include "hw_main.h"
#include "hw_glob.h"
#include "hw_drv.h"
#include "hw_batching.h"
#include "hw_md2.h"
#include "hw_clip.h"
#include "hw_light.h"

#include "../i_video.h" // for rendermode == render_glide
#include "../v_video.h"
#include "../p_local.h"
#include "../p_setup.h"
#include "../r_fps.h"
#include "../r_state.h"
#include "../r_local.h"
#include "../r_data.h"
#include "../r_patch.h" // a mystery as to what this is for
#include "../r_bsp.h"	// R_NoEncore
#include "../r_main.h"	// cv_fov
#include "../d_clisrv.h"
#include "../w_wad.h"
#include "../z_zone.h"
#include "../r_splats.h"
#include "../g_game.h"
#include "../st_stuff.h"
#include "../i_system.h"
#include "../m_cheat.h"
#include "../m_argv.h" // parm functions for msaa
#include "../p_slopes.h"

#include <stdlib.h> // qsort

#define ABS(x) ((x) < 0 ? -(x) : (x))

#define SOFTLIGHT(llevel) HWR_ShouldUsePaletteRendering() ? ((llevel) >> LIGHTSEGSHIFT) << LIGHTSEGSHIFT : (llevel)

// ==========================================================================
// the hardware driver object
// ==========================================================================
struct hwdriver_s hwdriver;

// false if shaders have not been initialized yet, or if shaders are not available
boolean gr_shadersavailable = false;

// Whether the internal state is set to palette rendering or not.
static boolean gr_palette_rendering_state = false;

// ==========================================================================
// Commands and console variables
// ==========================================================================

static void CV_grframebuffer_OnChange(void);
static void CV_filtermode_ONChange(void);
static void CV_anisotropic_ONChange(void);
static void CV_screentextures_ONChange(void);
static void CV_grshaders_OnChange(void);
static void CV_grpaletterendering_OnChange(void);
static void CV_grpalettedepth_OnChange(void);

static CV_PossibleValue_t grfakecontrast_cons_t[] = {{0, "Off"}, {1, "Standard"}, {2, "Smooth"}, {0, NULL}};

static CV_PossibleValue_t grfiltermode_cons_t[]= {{HWD_SET_TEXTUREFILTER_POINTSAMPLED, "Nearest"},
	{HWD_SET_TEXTUREFILTER_BILINEAR, "Bilinear"}, {HWD_SET_TEXTUREFILTER_TRILINEAR, "Trilinear"},
	{HWD_SET_TEXTUREFILTER_MIXED1, "Linear_Nearest"},
	{HWD_SET_TEXTUREFILTER_MIXED2, "Nearest_Linear"},
	{HWD_SET_TEXTUREFILTER_MIXED3, "Nearest_Mipmap"},
	{0, NULL}};
CV_PossibleValue_t granisotropicmode_cons_t[] = {{1, "MIN"}, {16, "MAX"}, {0, NULL}};

consvar_t cv_grfiltermode = {"gr_filtermode", "Nearest", CV_CALL|CV_SAVE, grfiltermode_cons_t,
                             CV_filtermode_ONChange, 0, NULL, NULL, 0, 0, NULL};

consvar_t cv_granisotropicmode = {"gr_anisotropicmode", "1", CV_CALL|CV_SAVE, granisotropicmode_cons_t,
                             CV_anisotropic_ONChange, 0, NULL, NULL, 0, 0, NULL};

consvar_t cv_grsolvetjoin = {"gr_solvetjoin", "On", 0, CV_OnOff, NULL, 0, NULL, NULL, 0, 0, NULL};

consvar_t cv_grbatching = {"gr_batching", "On", 0, CV_OnOff, NULL, 0, NULL, NULL, 0, 0, NULL};

consvar_t cv_grframebuffer = {"gr_framebuffer", "Off", CV_SAVE|CV_CALL|CV_NOINIT, CV_OnOff, CV_grframebuffer_OnChange, 0, NULL, NULL, 0, 0, NULL};

consvar_t cv_grfofcut = {"gr_fofcut", "On", CV_SAVE, CV_OnOff, NULL, 0, NULL, NULL, 0, 0, NULL};

consvar_t cv_fofzfightfix = {"fofzfightfix", "On", CV_SAVE, CV_OnOff, NULL, 0, NULL, NULL, 0, 0, NULL}; 

consvar_t cv_splitwallfix = {"splitwallfix", "On", CV_SAVE, CV_OnOff, NULL, 0, NULL, NULL, 0, 0, NULL};

consvar_t cv_slopepegfix = {"slopepegfix", "On", CV_SAVE, CV_OnOff, NULL, 0, NULL, NULL, 0, 0, NULL}; 

static CV_PossibleValue_t grrenderdistance_cons_t[] = {
	{0, "Max"}, {1, "1024"}, {2, "2048"}, {3, "4096"}, {4, "6144"}, {5, "8192"},
	{6, "12288"}, {7, "16384"}, {0, NULL}};
consvar_t cv_grrenderdistance = {"gr_renderdistance", "Max", CV_SAVE, grrenderdistance_cons_t,
							NULL, 0, NULL, NULL, 0, 0, NULL};

consvar_t cv_grportals = {"gr_portals", "On", CV_SAVE, CV_OnOff, NULL, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_nostencil = {"nostencil", "Off", 0, CV_OnOff, NULL, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_portalline = {"portalline", "0", 0, CV_Unsigned, NULL, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_portalonly = {"portalonly", "Off", 0, CV_OnOff, NULL, 0, NULL, NULL, 0, 0, NULL};
CV_PossibleValue_t secbright_cons_t[] = {{0, "MIN"}, {255, "MAX"}, {0, NULL}};

consvar_t cv_secbright = {"secbright", "0", CV_SAVE, secbright_cons_t,
							NULL, 0, NULL, NULL, 0, 0, NULL};

consvar_t cv_grfovchange = {"gr_fovchange", "Off", CV_SAVE, CV_OnOff, NULL, 0, NULL, NULL, 0, 0, NULL};

// values for the far clipping plane
static float clipping_distances[] = {1024.0f, 2048.0f, 4096.0f, 6144.0f, 8192.0f, 12288.0f, 16384.0f};
// values for bsp culling
// slightly higher than the far clipping plane to compensate for impreciseness
static INT32 bsp_culling_distances[] = {(1024+512)*FRACUNIT, (2048+512)*FRACUNIT, (4096+512)*FRACUNIT,
	(6144+512)*FRACUNIT, (8192+512)*FRACUNIT, (12288+512)*FRACUNIT, (16384+512)*FRACUNIT};

static INT32 current_bsp_culling_distance = 0;

// The current screen texture implementation is inefficient and disabling it can result in significant
// performance gains on lower end hardware. The game is still quite playable without this functionality.
// Features that break when disabling this:
//  - water and heat wave effects
//  - intermission background
//  - full screen scaling (use native resolution or windowed mode to avoid this)
consvar_t cv_grscreentextures = {"gr_screentextures", "On", CV_CALL|CV_SAVE, CV_OnOff,
                                 CV_screentextures_ONChange, 0, NULL, NULL, 0, 0, NULL};

static CV_PossibleValue_t grshaders_cons_t[] = {{0, "Off"}, {1, "On"}, {2, "Ignore custom shaders"}, {0, NULL}};
consvar_t cv_grshaders = {"gr_shaders", "On", CV_CALL|CV_SAVE, grshaders_cons_t, CV_grshaders_OnChange, 0, NULL, NULL, 0, 0, NULL};

consvar_t cv_grpaletterendering = {"gr_paletteshader", "Off", CV_CALL|CV_SAVE, CV_OnOff, CV_grpaletterendering_OnChange, 0, NULL, NULL, 0, 0, NULL};

static CV_PossibleValue_t glpalettedepth_cons_t[] = {{16, "16 bits"}, {24, "24 bits"}, {0, NULL}};
consvar_t cv_grpalettedepth = {"gr_palettedepth", "16 bits", CV_SAVE|CV_CALL, glpalettedepth_cons_t, CV_grpalettedepth_OnChange, 0, NULL, NULL, 0, 0, NULL};

consvar_t cv_grflashpal = {"gr_flashpal", "On", CV_CALL|CV_SAVE, CV_OnOff, CV_grpaletterendering_OnChange, 0, NULL, NULL, 0, 0, NULL};

consvar_t cv_grmdls = {"gr_mdls", "Off", CV_SAVE, CV_OnOff, NULL, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_grfallbackplayermodel = {"gr_fallbackplayermodel", "Off", CV_SAVE, CV_OnOff, NULL, 0, NULL, NULL, 0, 0, NULL};

consvar_t cv_grshearing = {"gr_shearing", "Off", CV_SAVE, CV_OnOff, NULL, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_grspritebillboarding = {"gr_spritebillboarding", "On", CV_SAVE, CV_OnOff, NULL, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_grfakecontrast = {"gr_fakecontrast", "Standard", CV_SAVE, grfakecontrast_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_grslopecontrast = {"gr_slopecontrast", "Off", CV_SAVE, CV_OnOff, NULL, 0, NULL, NULL, 0, 0, NULL};

consvar_t cv_grhorizonlines = {"gr_horizonlines", "On", CV_SAVE, CV_OnOff, NULL, 0, NULL, NULL, 0, 0, NULL};

#define ONLY_IF_GL_LOADED if (vid.glstate != VID_GL_LIBRARY_LOADED) return;

static void CV_grframebuffer_OnChange(void)
{
	ONLY_IF_GL_LOADED
	HWD.pfnSetSpecialState(HWD_SET_FRAMEBUFFER, cv_grframebuffer.value);
	RefreshSDLSurface();
}

static void CV_filtermode_ONChange(void)
{
	ONLY_IF_GL_LOADED
	HWD.pfnSetSpecialState(HWD_SET_TEXTUREFILTERMODE, cv_grfiltermode.value);
}

static void CV_anisotropic_ONChange(void)
{
	ONLY_IF_GL_LOADED
	HWD.pfnSetSpecialState(HWD_SET_TEXTUREANISOTROPICMODE, cv_granisotropicmode.value);
}

static void CV_screentextures_ONChange(void)
{
	ONLY_IF_GL_LOADED
	HWD.pfnSetSpecialState(HWD_SET_SCREEN_TEXTURES, cv_grscreentextures.value);
}

static void CV_grshaders_OnChange(void)
{
	ONLY_IF_GL_LOADED
	HWR_SetShaderState();
	if ((cv_grpaletterendering.value))
	{
		// can't do palette rendering without shaders, so update the state if needed
		HWR_TogglePaletteRendering();
	}
}

static void CV_grpaletterendering_OnChange(void)
{
	ONLY_IF_GL_LOADED
	if (gr_shadersavailable)
	{
		HWR_CompileShaders();
		HWR_TogglePaletteRendering();
	}
}

static void CV_grpalettedepth_OnChange(void)
{
	ONLY_IF_GL_LOADED
	if (HWR_ShouldUsePaletteRendering())
		HWR_SetPalette(pLocalPalette);
}

//
// Sets the shader state.
//
void HWR_SetShaderState(void)
{
	HWD.pfnSetSpecialState(HWD_SET_SHADERS, HWR_UseShader() ? 1 : 0);
}

// ==========================================================================
// Globals
// ==========================================================================

// base values set at SetViewSize
static float gr_basecentery;
static float gr_basecenterx;

float gr_baseviewwindowy, gr_basewindowcentery;
float gr_baseviewwindowx, gr_basewindowcenterx;
float gr_viewwidth, gr_viewheight; // viewport clipping boundaries (screen coords)

static float gr_centerx;
static float gr_viewwindowx;
static float gr_windowcenterx; // center of view window, for projection

static float gr_centery;
static float gr_viewwindowy; // top left corner of view window
static float gr_windowcentery;

static float gr_pspritexscale, gr_pspriteyscale;

static seg_t *gr_curline;
static side_t *gr_sidedef;
static line_t *gr_linedef;
static sector_t *gr_frontsector;
static sector_t *gr_backsector;

static void HWR_SplitWall(sector_t *sector, FOutVector *wallVerts, INT32 texnum, boolean noencore, FSurfaceInfo* Surf, INT32 cutflag, ffloor_t *pfloor, FBITFIELD polyflags);

// Performance stats
ps_metric_t ps_hw_nodesorttime = {0};
ps_metric_t ps_hw_nodedrawtime = {0};
ps_metric_t ps_hw_spritesorttime = {0};
ps_metric_t ps_hw_spritedrawtime = {0};

// Performance stats for batching
ps_metric_t ps_hw_numpolys = {0};
ps_metric_t ps_hw_numverts = {0};
ps_metric_t ps_hw_numcalls = {0};
ps_metric_t ps_hw_numshaders = {0};
ps_metric_t ps_hw_numtextures = {0};
ps_metric_t ps_hw_numpolyflags = {0};
ps_metric_t ps_hw_numcolors = {0};
ps_metric_t ps_hw_batchsorttime = {0};
ps_metric_t ps_hw_batchdrawtime = {0};

// ==========================================================================
// View position
// ==========================================================================

FTransform atransform;

// Float variants of viewx, viewy, viewz, etc.
static float gr_viewx, gr_viewy, gr_viewz;
float gr_viewsin, gr_viewcos;

static angle_t gr_aimingangle;
static float gr_viewludsin, gr_viewludcos;

// ==========================================================================
// Visual portals (attr. Hannu Hanhi)
// ==========================================================================

enum
{
	GRPORTAL_OFF,
	GRPORTAL_SEARCH,
	GRPORTAL_STENCIL,
	GRPORTAL_DEPTH,
	GRPORTAL_INSIDE,
};

static int gr_portal = GRPORTAL_OFF;
static boolean gl_drawing_stencil = false;
typedef struct gl_portal_s
{
	struct gl_portal_s *next;

	// Viewport.
	fixed_t viewx;
	fixed_t viewy;
	fixed_t viewz;
	angle_t viewangle;

	UINT8 pass;			/**< Keeps track of the portal's recursion depth. */
	INT32 startline;
	INT32 clipline;		/**< Optional clipline for line-based portals. */
	INT32 drawcount;	/**< For OpenGL. */

	seg_t *seg;
} gl_portal_t;

typedef struct gl_portallist_s
{
	gl_portal_t *base;
	gl_portal_t *cap;
} gl_portallist_t;

// new thing
gl_portallist_t *currentportallist;

INT32 portalviewside;

// Linked list for portals.
gl_portal_t *portal_base_gl, *portal_cap_gl;

/* For now, these aren't used.
// maybe at some point these could be organized better
static void HWR_Portal_InitList (void)
{
	portalrender = 0;
	portal_base_gl = portal_cap_gl = NULL;
}

static void HWR_Portal_Remove(gl_portal_t* portal)
{
	portal_base_gl = portal->next;
	Z_Free(portal);
}
*/

static void HWR_Portal_Add2Lines(const INT32 line1, const INT32 line2, seg_t *seg)
{
	line_t *start, *dest;

	angle_t dangle;

	fixed_t disttopoint;
	angle_t angtopoint;

	vertex_t dest_c, start_c;

	gl_portal_t *portal;

	portal = Z_Malloc(sizeof(gl_portal_t), PU_STATIC, NULL);

	// Linked list.
	if (!currentportallist->base)
	{
		currentportallist->base	= portal;
		currentportallist->cap	= portal;
	}
	else
	{
		currentportallist->cap->next = portal;
		currentportallist->cap = portal;
	}
	portal->next = NULL;

	// Increase recursion level.
	portal->pass = portalrender+1;

	portal->seg = seg;

	// Offset the portal view by the linedef centers
	start	= &lines[line1];
	dest	= &lines[line2];
	dangle	= R_PointToAngle2(0,0,dest->dx,dest->dy) - R_PointToAngle2(start->dx,start->dy,0,0);

	// looking glass center
	start_c.x = (start->v1->x + start->v2->x) / 2;
	start_c.y = (start->v1->y + start->v2->y) / 2;

	// other side center
	dest_c.x = (dest->v1->x + dest->v2->x) / 2;
	dest_c.y = (dest->v1->y + dest->v2->y) / 2;

	disttopoint = R_PointToDist2(start_c.x, start_c.y, viewx, viewy);
	angtopoint = R_PointToAngle2(start_c.x, start_c.y, viewx, viewy);
	angtopoint += dangle;

	if (dangle == 0)
	{
		portal->viewx = viewx + dest_c.x - start_c.x;
		portal->viewy = viewy + dest_c.y - start_c.y;
	}
	else
	{
		portal->viewx = dest_c.x + FixedMul(FINECOSINE(angtopoint>>ANGLETOFINESHIFT), disttopoint);
		portal->viewy = dest_c.y + FixedMul(FINESINE(angtopoint>>ANGLETOFINESHIFT), disttopoint);
	}
	portal->viewz = viewz + dest->frontsector->floorheight - start->frontsector->floorheight;
	portal->viewangle = viewangle + dangle;

	portal->startline = line1;
	portal->clipline = line2;
}

static void HWR_PortalFrame(gl_portal_t* portal)
{
	viewx = portal->viewx;
	viewy = portal->viewy;
	viewz = portal->viewz;

	viewangle = portal->viewangle;
	viewsin = FINESINE(viewangle>>ANGLETOFINESHIFT);
	viewcos = FINECOSINE(viewangle>>ANGLETOFINESHIFT);

	if (portal->clipline != -1)
	{
		portalclipline = &lines[portal->clipline];
		portalcullsector = portalclipline->frontsector;
		viewsector = portalclipline->frontsector;
		portalviewside = P_PointOnLineSide(viewx, viewy, portalclipline);
	}
	else
	{
		portalclipline = NULL;
		portalcullsector = NULL;
		viewsector = R_PointInSubsector(viewx, viewy)->sector;
	}
}

// ==========================================================================
// Lighting
// ==========================================================================

boolean HWR_UseShader(void)
{
	return (cv_grshaders.value && gr_shadersavailable);
}

boolean HWR_ShouldUsePaletteRendering(void)
{
	return (cv_grpaletterendering.value && HWR_UseShader());
}

boolean HWR_PalRenderFlashpal(void)
{
	return (HWR_ShouldUsePaletteRendering() && cv_grflashpal.value);
}

void HWR_Lighting(FSurfaceInfo *Surface, INT32 light_level, extracolormap_t *colormap)
{
	RGBA_t poly_color, tint_color, fade_color;

	poly_color.rgba = 0xFFFFFFFF;
	tint_color.rgba = (colormap != NULL) ? (UINT32)colormap->rgba : GL_DEFAULTMIX;
	fade_color.rgba = (colormap != NULL) ? (UINT32)colormap->fadergba : GL_DEFAULTFOG;

	// Crappy backup coloring if you can't do shaders
	if (!HWR_UseShader())
	{
		// be careful, this may get negative for high lightlevel values.
		float tint_alpha, fade_alpha;
		float red, green, blue;

		red = (float)poly_color.s.red;
		green = (float)poly_color.s.green;
		blue = (float)poly_color.s.blue;

		// 48 is just an arbritrary value that looked relatively okay.
		tint_alpha = (float)(sqrt(tint_color.s.alpha) * 48) / 255.0f;

		// 8 is roughly the brightness of the "close" color in Software, and 16 the brightness of the "far" color.
		// 8 is too bright for dark levels, and 16 is too dark for bright levels.
		// 12 is the compromise value. It doesn't look especially good anywhere, but it's the most balanced.
		// (Also, as far as I can tell, fade_color's alpha is actually not used in Software, so we only use light level.)
		fade_alpha = (float)(sqrt(255-light_level) * 12) / 255.0f;

		// Clamp the alpha values
		tint_alpha = min(max(tint_alpha, 0.0f), 1.0f);
		fade_alpha = min(max(fade_alpha, 0.0f), 1.0f);

		red = (tint_color.s.red * tint_alpha) + (red * (1.0f - tint_alpha));
		green = (tint_color.s.green * tint_alpha) + (green * (1.0f - tint_alpha));
		blue = (tint_color.s.blue * tint_alpha) + (blue * (1.0f - tint_alpha));

		red = (fade_color.s.red * fade_alpha) + (red * (1.0f - fade_alpha));
		green = (fade_color.s.green * fade_alpha) + (green * (1.0f - fade_alpha));
		blue = (fade_color.s.blue * fade_alpha) + (blue * (1.0f - fade_alpha));

		poly_color.s.red = (UINT8)red;
		poly_color.s.green = (UINT8)green;
		poly_color.s.blue = (UINT8)blue;
	}

	// Clamp the light level, since it can sometimes go out of the 0-255 range from animations
	light_level = min(max(light_level, cv_secbright.value), 255);

	// in palette rendering mode, this is not needed since it properly takes the changes to the palette itself
	if (!HWR_ShouldUsePaletteRendering())
	{
		V_CubeApply(&tint_color.s.red, &tint_color.s.green, &tint_color.s.blue);
		V_CubeApply(&fade_color.s.red, &fade_color.s.green, &fade_color.s.blue);
	}
	Surface->PolyColor.rgba = poly_color.rgba;
	Surface->TintColor.rgba = tint_color.rgba;
	Surface->FadeColor.rgba = fade_color.rgba;
	Surface->LightInfo.light_level = light_level;
	Surface->LightInfo.fade_start = (colormap != NULL) ? colormap->fadestart : 0;
	Surface->LightInfo.fade_end = (colormap != NULL) ? colormap->fadeend : 31;
	
	if (HWR_ShouldUsePaletteRendering())
		Surface->LightTableId = HWR_GetLightTableID(colormap);
	else
		Surface->LightTableId = 0;
}

UINT8 HWR_FogBlockAlpha(INT32 light, extracolormap_t *colormap) // Let's see if this can work
{
	RGBA_t realcolor, surfcolor;
	INT32 alpha;

	realcolor.rgba = (colormap != NULL) ? colormap->rgba : GL_DEFAULTMIX;

	if (HWR_UseShader())
	{
		surfcolor.s.alpha = (255 - light);
	}
	else
	{
		light = light - (255 - light);

		// Don't go out of bounds
		if (light < 0)
			light = 0;
		else if (light > 255)
			light = 255;

		alpha = (realcolor.s.alpha*255)/25;

		// at 255 brightness, alpha is between 0 and 127, at 0 brightness alpha will always be 255
		surfcolor.s.alpha = (alpha*light) / (2*256) + 255-light;
	}

	return surfcolor.s.alpha;
}

static FUINT HWR_CalcWallLight(FUINT lightnum, fixed_t v1x, fixed_t v1y, fixed_t v2x, fixed_t v2y)
{
	INT16 finallight = lightnum;

	if (cv_grfakecontrast.value != 0)
	{
		const UINT8 contrast = 8;
		fixed_t extralight = 0;

		if (cv_grfakecontrast.value == 2) // Smooth setting
		{
			extralight = (-(contrast<<FRACBITS) +
			FixedDiv(AngleFixed(R_PointToAngle2(0, 0,
				abs(v1x - v2x),
				abs(v1y - v2y))), 90<<FRACBITS)
			* (contrast * 2)) >> FRACBITS;
		}
		else
		{
			if (v1y == v2y)
				extralight = -contrast;
			else if (v1x == v2x)
				extralight = contrast;
		}

		if (extralight != 0)
		{
			finallight += extralight;

			if (finallight < 0)
				finallight = 0;
			if (finallight > 255)
				finallight = 255;
		}
	}

	return (FUINT)finallight;
}

static FUINT HWR_CalcSlopeLight(FUINT lightnum, angle_t dir, fixed_t delta)
{
	INT16 finallight = lightnum;

	if (cv_grfakecontrast.value != 0 && cv_grslopecontrast.value != 0)
	{
		const UINT8 contrast = 8;
		fixed_t extralight = 0;

		if (cv_grfakecontrast.value == 2) // Smooth setting
		{
			fixed_t dirmul = abs(FixedDiv(AngleFixed(dir) - (180<<FRACBITS), 180<<FRACBITS));

			extralight = -(contrast<<FRACBITS) + (dirmul * (contrast * 2));

			extralight = FixedMul(extralight, delta*4) >> FRACBITS;
		}
		else
		{
			dir = ((dir + ANGLE_45) / ANGLE_90) * ANGLE_90;

			if (dir == ANGLE_180)
				extralight = -contrast;
			else if (dir == 0)
				extralight = contrast;

			if (delta >= FRACUNIT/2)
				extralight *= 2;
		}

		if (extralight != 0)
		{
			finallight += extralight;

			if (finallight < 0)
				finallight = 0;
			if (finallight > 255)
				finallight = 255;
		}
	}

	return (FUINT)finallight;
}

// ==========================================================================
// Floor and ceiling generation from subsectors
// ==========================================================================

// HWR_RenderPlane
// Render a floor or ceiling convex polygon
void HWR_RenderPlane(subsector_t *subsector, extrasubsector_t *xsub, boolean isceiling, fixed_t fixedheight, FBITFIELD PolyFlags, INT32 lightlevel, lumpnum_t lumpnum, sector_t *FOFsector, UINT8 alpha, extracolormap_t *planecolormap)
{
	polyvertex_t *  pv;
	float           height; //constant y for all points on the convex flat polygon
	FOutVector      *v3d;
	INT32             nrPlaneVerts;   //verts original define of convex flat polygon
	INT32             i;
	float           flatxref,flatyref;
	float fflatsize;
	INT32 flatflag;
	size_t len;
	float scrollx = 0.0f, scrolly = 0.0f;
	angle_t angle = 0;
	FSurfaceInfo    Surf;
	fixed_t tempxsow, tempytow;
	pslope_t *slope = NULL;

	static FOutVector *planeVerts = NULL;
	static UINT16 numAllocedPlaneVerts = 0;

	INT32 shader = SHADER_NONE;

	// no convex poly were generated for this subsector
	if (!xsub->planepoly)
		return;
	
	pv  = xsub->planepoly->pts;
	nrPlaneVerts = xsub->planepoly->numpts;

	if (nrPlaneVerts < 3)   //not even a triangle ?
		return;

	// Get the slope pointer to simplify future code
	if (FOFsector)
	{
		if (FOFsector->f_slope && !isceiling)
			slope = FOFsector->f_slope;
		else if (FOFsector->c_slope && isceiling)
			slope = FOFsector->c_slope;
	}
	else
	{
		if (gr_frontsector->f_slope && !isceiling)
			slope = gr_frontsector->f_slope;
		else if (gr_frontsector->c_slope && isceiling)
			slope = gr_frontsector->c_slope;
	}

	// Set fixedheight to the slope's height from our viewpoint, if we have a slope
	if (slope)
		fixedheight = P_GetZAt(slope, viewx, viewy);

	height = FIXED_TO_FLOAT(fixedheight);

	// Allocate plane-vertex buffer if we need to
	if (!planeVerts || nrPlaneVerts > numAllocedPlaneVerts)
	{
		numAllocedPlaneVerts = (UINT16)nrPlaneVerts;
		Z_Free(planeVerts);
		Z_Malloc(numAllocedPlaneVerts * sizeof (FOutVector), PU_LEVEL, &planeVerts);
	}

	len = W_LumpLength(lumpnum);

	switch (len)
	{
		case 4194304: // 2048x2048 lump
			fflatsize = 2048.0f;
			flatflag = 2047;
			break;
		case 1048576: // 1024x1024 lump
			fflatsize = 1024.0f;
			flatflag = 1023;
			break;
		case 262144:// 512x512 lump
			fflatsize = 512.0f;
			flatflag = 511;
			break;
		case 65536: // 256x256 lump
			fflatsize = 256.0f;
			flatflag = 255;
			break;
		case 16384: // 128x128 lump
			fflatsize = 128.0f;
			flatflag = 127;
			break;
		case 1024: // 32x32 lump
			fflatsize = 32.0f;
			flatflag = 31;
			break;
		default: // 64x64 lump
			fflatsize = 64.0f;
			flatflag = 63;
			break;
	}

	// reference point for flat texture coord for each vertex around the polygon
	flatxref = (float)(((fixed_t)pv->x & (~flatflag)) / fflatsize);
	flatyref = (float)(((fixed_t)pv->y & (~flatflag)) / fflatsize);

	if (FOFsector != NULL)
	{
		if (!isceiling) // it's a floor
		{
			scrollx = FIXED_TO_FLOAT(FOFsector->floor_xoffs)/fflatsize;
			scrolly = FIXED_TO_FLOAT(FOFsector->floor_yoffs)/fflatsize;
			angle = FOFsector->floorpic_angle;
		}
		else // it's a ceiling
		{
			scrollx = FIXED_TO_FLOAT(FOFsector->ceiling_xoffs)/fflatsize;
			scrolly = FIXED_TO_FLOAT(FOFsector->ceiling_yoffs)/fflatsize;
			angle = FOFsector->ceilingpic_angle;
		}
	}
	else if (gr_frontsector)
	{
		if (!isceiling) // it's a floor
		{
			scrollx = FIXED_TO_FLOAT(gr_frontsector->floor_xoffs)/fflatsize;
			scrolly = FIXED_TO_FLOAT(gr_frontsector->floor_yoffs)/fflatsize;
			angle = gr_frontsector->floorpic_angle;
		}
		else // it's a ceiling
		{
			scrollx = FIXED_TO_FLOAT(gr_frontsector->ceiling_xoffs)/fflatsize;
			scrolly = FIXED_TO_FLOAT(gr_frontsector->ceiling_yoffs)/fflatsize;
			angle = gr_frontsector->ceilingpic_angle;
		}
	}

	if (angle) // Only needs to be done if there's an altered angle
	{
		angle = InvAngle(angle)>>ANGLETOFINESHIFT;

		// This needs to be done so that it scrolls in a different direction after rotation like software
		/*tempxsow = FLOAT_TO_FIXED(scrollx);
		tempytow = FLOAT_TO_FIXED(scrolly);
		scrollx = (FIXED_TO_FLOAT(FixedMul(tempxsow, FINECOSINE(angle)) - FixedMul(tempytow, FINESINE(angle))));
		scrolly = (FIXED_TO_FLOAT(FixedMul(tempxsow, FINESINE(angle)) + FixedMul(tempytow, FINECOSINE(angle))));*/

		// This needs to be done so everything aligns after rotation
		// It would be done so that rotation is done, THEN the translation, but I couldn't get it to rotate AND scroll like software does
		tempxsow = FLOAT_TO_FIXED(flatxref);
		tempytow = FLOAT_TO_FIXED(flatyref);
		flatxref = (FIXED_TO_FLOAT(FixedMul(tempxsow, FINECOSINE(angle)) - FixedMul(tempytow, FINESINE(angle))));
		flatyref = (FIXED_TO_FLOAT(FixedMul(tempxsow, FINESINE(angle)) + FixedMul(tempytow, FINECOSINE(angle))));
	}

#define SETUP3DVERT(vert, vx, vy) {\
		/* Hurdler: add scrolling texture on floor/ceiling */\
		vert->s = (float)(((vx) / fflatsize) - flatxref + scrollx);\
		vert->t = (float)(flatyref - ((vy) / fflatsize) + scrolly);\
\
		/* Need to rotate before translate */\
		if (angle) /* Only needs to be done if there's an altered angle */\
		{\
			tempxsow = FLOAT_TO_FIXED(vert->s);\
			tempytow = FLOAT_TO_FIXED(vert->t);\
			vert->s = (FIXED_TO_FLOAT(FixedMul(tempxsow, FINECOSINE(angle)) - FixedMul(tempytow, FINESINE(angle))));\
			vert->t = (FIXED_TO_FLOAT(FixedMul(tempxsow, FINESINE(angle)) + FixedMul(tempytow, FINECOSINE(angle))));\
		}\
\
		vert->x = (vx);\
		vert->y = height;\
		vert->z = (vy);\
\
		if (slope)\
		{\
			fixedheight = P_GetZAt(slope, FLOAT_TO_FIXED((vx)), FLOAT_TO_FIXED((vy)));\
			vert->y = FIXED_TO_FLOAT(fixedheight);\
		}\
}
	for (i = 0, v3d = planeVerts; i < nrPlaneVerts; i++,v3d++,pv++)
		SETUP3DVERT(v3d, pv->x, pv->y);

	if (slope)
		lightlevel = HWR_CalcSlopeLight(lightlevel, R_PointToAngle2(0, 0, slope->normal.x, slope->normal.y), abs(slope->zdelta));

	HWR_Lighting(&Surf, lightlevel, planecolormap);

	if (PolyFlags & (PF_Translucent|PF_Fog))
	{
		Surf.PolyColor.s.alpha = (UINT8)alpha;
		PolyFlags |= PF_Modulated;
	}
	else
		PolyFlags |= PF_Masked|PF_Modulated;

	if (HWR_UseShader())
	{	
		if (PolyFlags & PF_Fog)
			shader = SHADER_FOG;	// fog shader
		else if (PolyFlags & PF_Ripple)
			shader = SHADER_WATER; // water shader
		else
			shader = SHADER_FLOOR;	// floor shader
		
		PolyFlags |= PF_ColorMapped;
	}

	HWR_ProcessPolygon(&Surf, planeVerts, nrPlaneVerts, PolyFlags, shader, false);

	if (subsector && cv_grhorizonlines.value)
	{
		// Horizon lines
		FOutVector horizonpts[6];
		float dist, vx, vy;
		float x1, y1, xd, yd;
		UINT8 numplanes, j;
		vertex_t v; // For determining the closest distance from the line to the camera, to split render planes for minimum distortion;

		const float renderdist = 27000.0f; // How far out to properly render the plane
		const float farrenderdist = 32768.0f; // From here, raise plane to horizon level to fill in the line with some texture distortion

		seg_t *line = &segs[subsector->firstline];

		for (i = 0; i < subsector->numlines; i++, line++)
		{
			if (line->linedef->special == HORIZONSPECIAL && R_PointOnSegSide(viewx, viewy, line) == 0)
			{
				P_ClosestPointOnLine(viewx, viewy, line->linedef, &v);
				dist = FIXED_TO_FLOAT(R_PointToDist(v.x, v.y));
				
				if (line->pv1)
				{
					x1 = ((polyvertex_t *)line->pv1)->x;
					y1 = ((polyvertex_t *)line->pv1)->y;
				}
				else
				{
					x1 = FIXED_TO_FLOAT(line->v1->x);
					y1 = FIXED_TO_FLOAT(line->v1->x);
				}
				if (line->pv2)
				{
					xd = ((polyvertex_t *)line->pv2)->x - x1;
					yd = ((polyvertex_t *)line->pv2)->y - y1;
				}
				else
				{
					xd = FIXED_TO_FLOAT(line->v2->x) - x1;
					yd = FIXED_TO_FLOAT(line->v2->y) - y1;
				}

				// Based on the seg length and the distance from the line, split horizon into multiple poly sets to reduce distortion
				dist = sqrtf((xd*xd) + (yd*yd)) / dist / 16.0f;
				if (dist > 100.0f)
					numplanes = 100;
				else
					numplanes = (UINT8)dist + 1;

				for (j = 0; j < numplanes; j++)
				{
					// Left side
					vx = x1 + xd * j / numplanes;
					vy = y1 + yd * j / numplanes;
					SETUP3DVERT((&horizonpts[1]), vx, vy);

					dist = sqrtf(powf(vx - gr_viewx, 2) + powf(vy - gr_viewy, 2));
					vx = (vx - gr_viewx) * renderdist / dist + gr_viewx;
					vy = (vy - gr_viewy) * renderdist / dist + gr_viewy;
					SETUP3DVERT((&horizonpts[0]), vx, vy);

					// Right side
					vx = x1 + xd * (j+1) / numplanes;
					vy = y1 + yd * (j+1) / numplanes;
					SETUP3DVERT((&horizonpts[2]), vx, vy);

					dist = sqrtf(powf(vx - gr_viewx, 2) + powf(vy - gr_viewy, 2));
					vx = (vx - gr_viewx) * renderdist / dist + gr_viewx;
					vy = (vy - gr_viewy) * renderdist / dist + gr_viewy;
					SETUP3DVERT((&horizonpts[3]), vx, vy);

					// Horizon fills
					vx = (horizonpts[0].x - gr_viewx) * farrenderdist / renderdist + gr_viewx;
					vy = (horizonpts[0].z - gr_viewy) * farrenderdist / renderdist + gr_viewy;
					SETUP3DVERT((&horizonpts[5]), vx, vy);
					horizonpts[5].y = gr_viewz;

					vx = (horizonpts[3].x - gr_viewx) * farrenderdist / renderdist + gr_viewx;
					vy = (horizonpts[3].z - gr_viewy) * farrenderdist / renderdist + gr_viewy;
					SETUP3DVERT((&horizonpts[4]), vx, vy);
					horizonpts[4].y = gr_viewz;

					// Draw
					HWR_ProcessPolygon(&Surf, horizonpts, 6, PolyFlags, shader, true);
				}
			}
		}
	}
}

#ifdef WALLSPLATS
static void HWR_DrawSegsSplats(FSurfaceInfo * pSurf)
{
	FOutVector wallVerts[4];
	wallsplat_t *splat;
	GLPatch_t *gpatch;
	fixed_t i;
	// seg bbox
	fixed_t segbbox[4];
	
	INT32 shader = SHADER_NONE;

	M_ClearBox(segbbox);
	M_AddToBox(segbbox,
		FLOAT_TO_FIXED(((polyvertex_t *)gr_curline->pv1)->x),
		FLOAT_TO_FIXED(((polyvertex_t *)gr_curline->pv1)->y));
	M_AddToBox(segbbox,
		FLOAT_TO_FIXED(((polyvertex_t *)gr_curline->pv2)->x),
		FLOAT_TO_FIXED(((polyvertex_t *)gr_curline->pv2)->y));

	splat = (wallsplat_t *)gr_curline->linedef->splats;
	for (; splat; splat = splat->next)
	{
		//BP: don't draw splat extern to this seg
		//    this is quick fix best is explain in logboris.txt at 12-4-2000
		if (!M_PointInBox(segbbox,splat->v1.x,splat->v1.y) && !M_PointInBox(segbbox,splat->v2.x,splat->v2.y))
			continue;

		gpatch = W_CachePatchNum(splat->patch, PU_CACHE);
		HWR_GetPatch(gpatch);

		wallVerts[0].x = wallVerts[3].x = FIXED_TO_FLOAT(splat->v1.x);
		wallVerts[0].z = wallVerts[3].z = FIXED_TO_FLOAT(splat->v1.y);
		wallVerts[2].x = wallVerts[1].x = FIXED_TO_FLOAT(splat->v2.x);
		wallVerts[2].z = wallVerts[1].z = FIXED_TO_FLOAT(splat->v2.y);

		i = splat->top;
		if (splat->yoffset)
			i += *splat->yoffset;

		wallVerts[2].y = wallVerts[3].y = FIXED_TO_FLOAT(i)+(gpatch->height>>1);
		wallVerts[0].y = wallVerts[1].y = FIXED_TO_FLOAT(i)-(gpatch->height>>1);

		wallVerts[3].s = wallVerts[3].t = wallVerts[2].s = wallVerts[0].t = 0.0f;
		wallVerts[1].s = wallVerts[1].t = wallVerts[2].t = wallVerts[0].s = 1.0f;

		switch (splat->flags & SPLATDRAWMODE_MASK)
		{
			case SPLATDRAWMODE_OPAQUE :
				pSurf.PolyColor.s.alpha = 0xff;
				i = PF_Translucent;
				break;
			case SPLATDRAWMODE_TRANS :
				pSurf.PolyColor.s.alpha = 128;
				i = PF_Translucent;
				break;
			case SPLATDRAWMODE_SHADE :
				pSurf.PolyColor.s.alpha = 0xff;
				i = PF_Substractive;
				break;
		}

		if (HWR_UseShader())
			shader = SHADER_WALL;	// wall shader
		
		HWR_ProcessPolygon(&pSurf, wallVerts, 4, i|PF_Modulated|PF_Decal, shader, false);
	}
}
#endif

FBITFIELD HWR_TranstableToAlpha(INT32 transtablenum, FSurfaceInfo *pSurf)
{
	switch (transtablenum)
	{
		case tr_trans10 : pSurf->PolyColor.s.alpha = 0xe6;return  PF_Translucent;
		case tr_trans20 : pSurf->PolyColor.s.alpha = 0xcc;return  PF_Translucent;
		case tr_trans30 : pSurf->PolyColor.s.alpha = 0xb3;return  PF_Translucent;
		case tr_trans40 : pSurf->PolyColor.s.alpha = 0x99;return  PF_Translucent;
		case tr_trans50 : pSurf->PolyColor.s.alpha = 0x80;return  PF_Translucent;
		case tr_trans60 : pSurf->PolyColor.s.alpha = 0x66;return  PF_Translucent;
		case tr_trans70 : pSurf->PolyColor.s.alpha = 0x4c;return  PF_Translucent;
		case tr_trans80 : pSurf->PolyColor.s.alpha = 0x33;return  PF_Translucent;
		case tr_trans90 : pSurf->PolyColor.s.alpha = 0x19;return  PF_Translucent;
	}
	return PF_Translucent;
}

// ==========================================================================
// Wall generation from subsector segs
// ==========================================================================

//
// HWR_ProjectWall
//
void HWR_ProjectWall(FOutVector *wallVerts, FSurfaceInfo *pSurf, FBITFIELD blendmode, INT32 lightlevel, extracolormap_t *wallcolormap)
{
	INT32 shader = SHADER_NONE;
	
	HWR_Lighting(pSurf, lightlevel, wallcolormap);

	if (HWR_UseShader())
	{
		shader = SHADER_WALL;
		blendmode |= PF_ColorMapped;
	}

	// don't draw to color buffer when drawing to stencil
	if (gl_drawing_stencil)
	{
		blendmode |= PF_Invisible|PF_NoAlphaTest; // TODO not sure if any others than PF_Invisible are needed??
		blendmode &= ~PF_Masked;
	}

	HWR_ProcessPolygon(pSurf, wallVerts, 4, blendmode|PF_Modulated|PF_Occlude, shader, false);

#ifdef WALLSPLATS
	if (gr_curline->linedef->splats && cv_splats.value)
		HWR_DrawSegsSplats(pSurf);
#endif
}

//
// HWR_SplitWall
//
// SoM: split up and light walls according to the lightlist.
// This may also include leaving out parts of the wall that can't be seen
static void HWR_SplitWall(sector_t *sector, FOutVector *wallVerts, INT32 texnum, boolean noencore, FSurfaceInfo* Surf, INT32 cutflag, ffloor_t *pfloor, FBITFIELD polyflags)
{
	float realtop, realbot, top, bot;
	float pegt, pegb, pegmul;
	float height = 0.0f, bheight = 0.0f;

	float endrealtop, endrealbot, endtop, endbot;
	float endpegt, endpegb, endpegmul;
	float endheight = 0.0f, endbheight = 0.0f;

	fixed_t v1x = FloatToFixed(wallVerts[0].x);
	fixed_t v1y = FloatToFixed(wallVerts[0].z);
	fixed_t v2x = FloatToFixed(wallVerts[1].x);
	fixed_t v2y = FloatToFixed(wallVerts[1].z);

	const UINT8 alpha = Surf->PolyColor.s.alpha;
	FUINT lightnum = HWR_CalcWallLight(sector->lightlevel, v1x, v1y, v2x, v2y);
	extracolormap_t *colormap = NULL;

	realtop = top = wallVerts[3].y;
	realbot = bot = wallVerts[0].y;
	pegt = wallVerts[3].t;
	pegb = wallVerts[0].t;

	// Lactozilla: If both heights of a side lay on the same position, then this wall is a triangle.
	// To avoid division by zero, which would result in a NaN, we check if the vertical difference
	// between the two vertices is not zero.
	if (fpclassify(top - bot) == FP_ZERO && cv_splitwallfix.value)
		pegmul = 0.0;
	else
		pegmul = (pegb - pegt) / (top - bot);

	endrealtop = endtop = wallVerts[2].y;
	endrealbot = endbot = wallVerts[1].y;
	endpegt = wallVerts[2].t;
	endpegb = wallVerts[1].t;

	if (fpclassify(endtop - endbot) == FP_ZERO && cv_splitwallfix.value)
		endpegmul = 0.0;
	else
		endpegmul = (endpegb - endpegt) / (endtop - endbot);

	for (INT32 i = 0; i < sector->numlights; i++)
	{
		if (endtop < endrealbot && top < realbot)
			return;
		
		lightlist_t *list = sector->lightlist;

		if (!(list[i].flags & FF_NOSHADE))
		{
			if (pfloor && (pfloor->flags & FF_FOG))
			{
				if (HWR_ShouldUsePaletteRendering())
				{
					lightnum = SOFTLIGHT(pfloor->master->frontsector->lightlevel);
					colormap = pfloor->master->frontsector->extra_colormap;
					lightnum = colormap ? lightnum : HWR_CalcWallLight(lightnum, v1x, v1y, v2x, v2y);
				}else{
					lightnum = HWR_CalcWallLight(pfloor->master->frontsector->lightlevel, v1x, v1y, v2x, v2y);
					colormap = pfloor->master->frontsector->extra_colormap;
				}
			}
			else
			{
				if (HWR_ShouldUsePaletteRendering())
				{
					lightnum = SOFTLIGHT(*list[i].lightlevel);
					colormap = list[i].extra_colormap;
					lightnum = colormap ? lightnum : HWR_CalcWallLight(lightnum, v1x, v1y, v2x, v2y);
				}else{
					lightnum = HWR_CalcWallLight(*list[i].lightlevel, v1x, v1y, v2x, v2y);
					colormap = list[i].extra_colormap;
				}
			}
		}

		boolean solid = false;

		if ((sector->lightlist[i].flags & FF_CUTSOLIDS) && !(cutflag & FF_EXTRA))
			solid = true;
		else if ((sector->lightlist[i].flags & FF_CUTEXTRA) && (cutflag & FF_EXTRA))
		{
			if (sector->lightlist[i].flags & FF_EXTRA)
			{
				if ((sector->lightlist[i].flags & (FF_FOG|FF_SWIMMABLE)) == (cutflag & (FF_FOG|FF_SWIMMABLE))) // Only merge with your own types
					solid = true;
			}
			else
				solid = true;
		}
		else
			solid = false;

		height = FixedToFloat(P_GetLightZAt(&list[i], v1x, v1y));
		endheight = FixedToFloat(P_GetLightZAt(&list[i], v2x, v2y));

		if (solid)
		{
			bheight = FixedToFloat(P_GetFFloorBottomZAt(list[i].caster, v1x, v1y));
			endbheight = FixedToFloat(P_GetFFloorBottomZAt(list[i].caster, v2x, v2y));
		}

		if (endheight >= endtop && height >= top)
		{
			if (solid && top > bheight)
				top = bheight;
			if (solid && endtop > endbheight)
				endtop = endbheight;
		}

		if (i + 1 < sector->numlights)
		{
			bheight = FixedToFloat(P_GetLightZAt(&list[i+1], v1x, v1y));
			endbheight = FixedToFloat(P_GetLightZAt(&list[i+1], v2x, v2y));
		}
		else
		{
			bheight = realbot;
			endbheight = endrealbot;
		}
		
		if (cv_splitwallfix.value)
		{
			if (endbheight > endtop)
				endbot = endtop;
		}

		if ((endbheight >= endtop && bheight >= top && !cv_splitwallfix.value) || (bheight >= top && cv_splitwallfix.value))
			continue;

		// Found a break
		// The heights are clamped to ensure the polygon doesn't cross itself.
		bot = bheight;

		if (bot < realbot)
			bot = realbot;

		if (cv_splitwallfix.value)
			endbot = min(max(endbheight, endrealbot), endtop);
		else
		{
			endbot = endbheight;

			if (endbot < endrealbot)
				endbot = endrealbot;
		}

		Surf->PolyColor.s.alpha = alpha;

		wallVerts[3].t = pegt + ((realtop - top) * pegmul);
		wallVerts[2].t = endpegt + ((endrealtop - endtop) * endpegmul);
		wallVerts[0].t = pegt + ((realtop - bot) * pegmul);
		wallVerts[1].t = endpegt + ((endrealtop - endbot) * endpegmul);

		// set top/bottom coords
		wallVerts[3].y = top;
		wallVerts[2].y = endtop;
		wallVerts[0].y = bot;
		wallVerts[1].y = endbot;

		if (cutflag & FF_FOG)
			HWR_AddTransparentWall(wallVerts, Surf, texnum, noencore, PF_Fog|PF_NoTexture|polyflags, true, lightnum, colormap);
		else if (cutflag & FF_TRANSLUCENT)
			HWR_AddTransparentWall(wallVerts, Surf, texnum, noencore, PF_Translucent|polyflags, false, lightnum, colormap);
		else
			HWR_ProjectWall(wallVerts, Surf, PF_Masked|polyflags, lightnum, colormap);

		top = bot;
		endtop = endbot;
	}

	bot = realbot;
	endbot = endrealbot;
	if (endtop <= endrealbot && top <= realbot)
		return;

	Surf->PolyColor.s.alpha = alpha;

	wallVerts[3].t = pegt + ((realtop - top) * pegmul);
	wallVerts[2].t = endpegt + ((endrealtop - endtop) * endpegmul);
	wallVerts[0].t = pegt + ((realtop - bot) * pegmul);
	wallVerts[1].t = endpegt + ((endrealtop - endbot) * endpegmul);

	// set top/bottom coords
	wallVerts[3].y = top;
	wallVerts[2].y = endtop;
	wallVerts[0].y = bot;
	wallVerts[1].y = endbot;

	if (cutflag & FF_FOG)
		HWR_AddTransparentWall(wallVerts, Surf, texnum, noencore, PF_Fog|PF_NoTexture|polyflags, true, lightnum, colormap);
	else if (cutflag & FF_TRANSLUCENT)
		HWR_AddTransparentWall(wallVerts, Surf, texnum, noencore, PF_Translucent|polyflags, false, lightnum, colormap);
	else
		HWR_ProjectWall(wallVerts, Surf, PF_Masked|polyflags, lightnum, colormap);
}

// skywall list system for fixing portal issues by postponing skywall rendering (and using stencil buffer for them)
// ideally this will be a temporary implementation.
// a better and more efficient way to do this would be to sort skywalls to the end of the draw call list inside RenderBatches.
// Additionally, to remove the need to draw the sky twice, drawing a plane at the far clip boundary to the stencil buffer after other
// rendering will also allow stencil sky rendering to fill in any untouched pixels too.

FOutVector* skyWallVertexArray = NULL;
int skyWallVertexArraySize = 0;
int skyWallVertexArrayAllocSize = 65536;// what a mouthful

boolean gr_collect_skywalls = false;

static void HWR_SkyWallList_Clear(void)
{
	skyWallVertexArraySize = 0;
}

static void HWR_SkyWallList_Add(FOutVector *wallVerts)
{
	if (!skyWallVertexArray)
	{
		// array has not been allocated yet. allocate it now
		skyWallVertexArray = Z_Malloc(sizeof(FOutVector) * 4 * skyWallVertexArrayAllocSize, PU_STATIC, NULL);
	}

	if (skyWallVertexArraySize == skyWallVertexArrayAllocSize)
	{
		// allocated array got full, allocate more space
		skyWallVertexArrayAllocSize *= 2;
		skyWallVertexArray = Z_Realloc(skyWallVertexArray, sizeof(FOutVector) * 4 * skyWallVertexArrayAllocSize, PU_STATIC, NULL);
	}

	memcpy_fast(skyWallVertexArray + skyWallVertexArraySize * 4, wallVerts, sizeof(FOutVector) * 4);
	skyWallVertexArraySize++;
}

static void HWR_DrawSkyWallList(void)
{
	int i;
	FSurfaceInfo surf;

	surf.PolyColor.rgba = 0xFFFFFFFF;

	HWR_SetCurrentTexture(NULL);
	HWD.pfnUnSetShader();
	for (i = 0; i < skyWallVertexArraySize; i++)
	{
		HWD.pfnDrawPolygon(&surf, skyWallVertexArray + i * 4, 4, PF_Occlude|PF_Invisible|PF_NoTexture);
	}
}

// HWR_DrawSkyWalls
// Draw walls into the depth buffer so that anything behind is culled properly
void HWR_DrawSkyWall(FOutVector *wallVerts, FSurfaceInfo *Surf)
{
	//HWD.pfnSetTexture(NULL);
	// no texture
	wallVerts[3].t = wallVerts[2].t = 0;
	wallVerts[0].t = wallVerts[1].t = 0;
	wallVerts[0].s = wallVerts[3].s = 0;
	wallVerts[2].s = wallVerts[1].s = 0;

	if (gr_collect_skywalls)
	{
		HWR_SkyWallList_Add(wallVerts);
	}
	else
	{
		HWR_SetCurrentTexture(NULL);
		HWR_ProjectWall(wallVerts, Surf, PF_Invisible|PF_NoTexture, 255, NULL);
	}
	// PF_Invisible so it's not drawn into the colour buffer
	// PF_NoTexture for no texture
	// PF_Occlude is set in HWR_ProjectWall to draw into the depth buffer
}

// Returns true if the midtexture is visible, and false if... it isn't...
static boolean HWR_BlendMidtextureSurface(FSurfaceInfo *pSurf)
{
	FUINT blendmode = PF_Masked;

	pSurf->PolyColor.s.alpha = 0xFF;

	if (!gr_curline->polyseg)
	{
		// set alpha for transparent walls
		switch (gr_linedef->special)
		{
			case 900:
				blendmode = HWR_TranstableToAlpha(tr_trans10, pSurf);
				break;
			case 901:
				blendmode = HWR_TranstableToAlpha(tr_trans20, pSurf);
				break;
			case 902:
				blendmode = HWR_TranstableToAlpha(tr_trans30, pSurf);
				break;
			case 903:
				blendmode = HWR_TranstableToAlpha(tr_trans40, pSurf);
				break;
			case 904:
				blendmode = HWR_TranstableToAlpha(tr_trans50, pSurf);
				break;
			case 905:
				blendmode = HWR_TranstableToAlpha(tr_trans60, pSurf);
				break;
			case 906:
				blendmode = HWR_TranstableToAlpha(tr_trans70, pSurf);
				break;
			case 907:
				blendmode = HWR_TranstableToAlpha(tr_trans80, pSurf);
				break;
			case 908:
				blendmode = HWR_TranstableToAlpha(tr_trans90, pSurf);
				break;
			//  Translucent
			case 102:
			case 121:
			case 123:
			case 124:
			case 125:
			case 141:
			case 142:
			case 144:
			case 145:
			case 174:
			case 175:
			case 192:
			case 195:
			case 221:
			case 253:
			case 256:
				blendmode = PF_Translucent;
				break;
			default:
				blendmode = PF_Masked;
				break;
		}
	}
	else if (gr_curline->polyseg->translucency > 0)
	{
		// Polyobject translucency is shared between all of its lines
		if (gr_curline->polyseg->translucency >= NUMTRANSMAPS) // wall not drawn
			return false;
			//Surf.PolyColor.s.alpha = 0x00; // This shouldn't draw anything regardless of blendmode
			//blendmode = PF_Masked;
		else
			blendmode = HWR_TranstableToAlpha(gr_curline->polyseg->translucency, pSurf);
	}

	if (blendmode != PF_Masked && pSurf->PolyColor.s.alpha == 0x00)
		return false;

	pSurf->PolyFlags = blendmode;

	return true;
}

//
// HWR_ProcessSeg
// A portion or all of a wall segment will be drawn, from startfrac to endfrac,
//  where 0 is the start of the segment, 1 the end of the segment
// Anything between means the wall segment has been clipped with solidsegs,
//  reducing wall overdraw to a minimum
//
void HWR_ProcessSeg(void) // Sort of like GLWall::Process in GZDoom
{
	FOutVector wallVerts[4];
	v2d_t vs, ve; // start, end vertices of 2d line (view from above)

	fixed_t worldtop, worldbottom;
	fixed_t worldhigh = 0, worldlow = 0;
	fixed_t worldtopslope, worldbottomslope;
	fixed_t worldhighslope = 0, worldlowslope = 0;
	fixed_t v1x, v1y, v2x, v2y;

	float cliplow = 0.0f, cliphigh = 0.0f;
	fixed_t h, l; // 3D sides and 2s middle textures
	fixed_t hS, lS;

	FUINT lightnum = 0; // shut up compiler
	extracolormap_t *colormap;
	FSurfaceInfo Surf;

	boolean noencore = false;

	gr_sidedef = gr_curline->sidedef;
	gr_linedef = gr_curline->linedef;

	if (gr_curline->pv1)
	{
		vs.x = ((polyvertex_t *)gr_curline->pv1)->x;
		vs.y = ((polyvertex_t *)gr_curline->pv1)->y;
	}
	else
	{
		vs.x = FIXED_TO_FLOAT(gr_curline->v1->x);
		vs.y = FIXED_TO_FLOAT(gr_curline->v1->y);
	}
	if (gr_curline->pv2)
	{
		ve.x = ((polyvertex_t *)gr_curline->pv2)->x;
		ve.y = ((polyvertex_t *)gr_curline->pv2)->y;
	}
	else
	{
		ve.x = FIXED_TO_FLOAT(gr_curline->v2->x);
		ve.y = FIXED_TO_FLOAT(gr_curline->v2->y);
	}

	v1x = FLOAT_TO_FIXED(vs.x);
	v1y = FLOAT_TO_FIXED(vs.y);
	v2x = FLOAT_TO_FIXED(ve.x);
	v2y = FLOAT_TO_FIXED(ve.y);

#define SLOPEPARAMS(slope, end1, end2, normalheight) \
	if (slope) { \
		end1 = P_GetZAt(slope, v1x, v1y); \
		end2 = P_GetZAt(slope, v2x, v2y); \
	} else \
		end1 = end2 = normalheight;

	SLOPEPARAMS(gr_frontsector->c_slope, worldtop,    worldtopslope,    gr_frontsector->ceilingheight)
	SLOPEPARAMS(gr_frontsector->f_slope, worldbottom, worldbottomslope, gr_frontsector->floorheight)

	// remember vertices ordering
	//  3--2
	//  | /|
	//  |/ |
	//  0--1
	// make a wall polygon (with 2 triangles), using the floor/ceiling heights,
	// and the 2d map coords of start/end vertices
	wallVerts[0].x = wallVerts[3].x = vs.x;
	wallVerts[0].z = wallVerts[3].z = vs.y;
	wallVerts[2].x = wallVerts[1].x = ve.x;
	wallVerts[2].z = wallVerts[1].z = ve.y;

	// x offset the texture
	fixed_t texturehpeg = gr_sidedef->textureoffset + gr_curline->offset;
	cliplow = (float)texturehpeg;
	cliphigh = (float)(texturehpeg + (gr_curline->flength*FRACUNIT));

	if (HWR_ShouldUsePaletteRendering())
	{
		lightnum = SOFTLIGHT(gr_frontsector->lightlevel);
		colormap = gr_frontsector->extra_colormap;
		lightnum = colormap ? lightnum : HWR_CalcWallLight(lightnum, vs.x, vs.y, ve.x, ve.y);
	}
	else
	{
		lightnum = HWR_CalcWallLight(gr_frontsector->lightlevel, vs.x, vs.y, ve.x, ve.y);
		colormap = gr_frontsector->extra_colormap;
	}

	if (gr_linedef->flags & ML_TFERLINE)
		noencore = true;

	Surf.PolyColor.s.alpha = 255;
	
	INT32 gr_midtexture = R_GetTextureNum(gr_sidedef->midtexture);
	GLMapTexture_t *grTex = NULL;

	// two sided line
	if (gr_backsector)
	{
		INT32 gr_toptexture, gr_bottomtexture;

		SLOPEPARAMS(gr_backsector->c_slope, worldhigh, worldhighslope, gr_backsector->ceilingheight)
		SLOPEPARAMS(gr_backsector->f_slope, worldlow,  worldlowslope,  gr_backsector->floorheight)

		// Sky culling
		if (!gr_curline->polyseg) // Don't do it for polyobjects
		{
			// Sky Ceilings
			wallVerts[3].y = wallVerts[2].y = FIXED_TO_FLOAT(INT32_MAX);

			if (gr_frontsector->ceilingpic == skyflatnum)
			{
				if (gr_backsector->ceilingpic == skyflatnum)
				{
					// Both front and back sectors are sky, needs skywall from the frontsector's ceiling, but only if the
					// backsector is lower
					if ((worldhigh <= worldtop && worldhighslope <= worldtopslope)// Assuming ESLOPE is always on with my changes
					&& (worldhigh != worldtop || worldhighslope != worldtopslope))
					// Removing the second line above will render more rarely visible skywalls. Example: Cave garden ceiling in Dark race
					{
						wallVerts[0].y = FIXED_TO_FLOAT(worldhigh);
						wallVerts[1].y = FIXED_TO_FLOAT(worldhighslope);
						HWR_DrawSkyWall(wallVerts, &Surf);
					}
				}
				else
				{
					// Only the frontsector is sky, just draw a skywall from the front ceiling
					wallVerts[0].y = FIXED_TO_FLOAT(worldtop);
					wallVerts[1].y = FIXED_TO_FLOAT(worldtopslope);
					HWR_DrawSkyWall(wallVerts, &Surf);
				}
			}
			else if (gr_backsector->ceilingpic == skyflatnum)
			{
				// Only the backsector is sky, just draw a skywall from the front ceiling
				wallVerts[0].y = FIXED_TO_FLOAT(worldtop);
				wallVerts[1].y = FIXED_TO_FLOAT(worldtopslope);
				HWR_DrawSkyWall(wallVerts, &Surf);
			}


			// Sky Floors
			wallVerts[0].y = wallVerts[1].y = FIXED_TO_FLOAT(INT32_MIN);

			if (gr_frontsector->floorpic == skyflatnum)
			{
				if (gr_backsector->floorpic == skyflatnum)
				{
					// Both front and back sectors are sky, needs skywall from the backsector's floor, but only if the
					// it's higher, also needs to check for bottomtexture as the floors don't usually move down
					// when both sides are sky floors
					if ((worldlow >= worldbottom && worldlowslope >= worldbottomslope)
					&& (worldlow != worldbottom || worldlowslope != worldbottomslope)
					// Removing the second line above will render more rarely visible skywalls. Example: Cave garden ceiling in Dark race
					&& !(gr_sidedef->bottomtexture))
					{
						wallVerts[3].y = FIXED_TO_FLOAT(worldlow);
						wallVerts[2].y = FIXED_TO_FLOAT(worldlowslope);
						HWR_DrawSkyWall(wallVerts, &Surf);
					}
				}
				else
				{
					// Only the backsector has sky, just draw a skywall from the back floor
					wallVerts[3].y = FIXED_TO_FLOAT(worldbottom);
					wallVerts[2].y = FIXED_TO_FLOAT(worldbottomslope);
					HWR_DrawSkyWall(wallVerts, &Surf);
				}
			}
			else if ((gr_backsector->floorpic == skyflatnum) && !(gr_sidedef->bottomtexture))
			{
				// Only the backsector has sky, just draw a skywall from the back floor if there's no bottomtexture
				wallVerts[3].y = FIXED_TO_FLOAT(worldlow);
				wallVerts[2].y = FIXED_TO_FLOAT(worldlowslope);
				HWR_DrawSkyWall(wallVerts, &Surf);
			}
		}

		// hack to allow height changes in outdoor areas
		// This is what gets rid of the upper textures if there should be sky
		if (gr_frontsector->ceilingpic == skyflatnum
			&& gr_backsector->ceilingpic  == skyflatnum)
		{
			worldtop = worldhigh;
			worldtopslope = worldhighslope;
		}

		gr_toptexture = R_GetTextureNum(gr_sidedef->toptexture);
		gr_bottomtexture = R_GetTextureNum(gr_sidedef->bottomtexture);

		// check TOP TEXTURE
		if ((worldhighslope < worldtopslope || worldhigh < worldtop) && gr_toptexture)
		{
			{
				fixed_t texturevpegtop; // top

				grTex = HWR_GetTexture(gr_toptexture, gr_linedef->flags & ML_TFERLINE);

				// PEGGING
				if (gr_linedef->flags & ML_DONTPEGTOP)
					texturevpegtop = 0;
				else if (gr_linedef->flags & ML_EFFECT1)
					texturevpegtop = worldhigh + textureheight[gr_sidedef->toptexture] - worldtop;
				else
					texturevpegtop = gr_backsector->ceilingheight + textureheight[gr_sidedef->toptexture] - gr_frontsector->ceilingheight;

				texturevpegtop += gr_sidedef->rowoffset;

				// This is so that it doesn't overflow and screw up the wall, it doesn't need to go higher than the texture's height anyway
				texturevpegtop %= SHORT(textures[gr_toptexture]->height)<<FRACBITS;

				wallVerts[3].t = wallVerts[2].t = texturevpegtop * grTex->scaleY;
				wallVerts[0].t = wallVerts[1].t = (texturevpegtop + gr_frontsector->ceilingheight - gr_backsector->ceilingheight) * grTex->scaleY;
				wallVerts[0].s = wallVerts[3].s = cliplow * grTex->scaleX;
				wallVerts[2].s = wallVerts[1].s = cliphigh * grTex->scaleX;

				// Adjust t value for sloped walls
				if (!(gr_linedef->flags & ML_EFFECT1))
				{
					// Unskewed
					wallVerts[3].t -= (worldtop - gr_frontsector->ceilingheight) * grTex->scaleY;
					wallVerts[2].t -= (worldtopslope - gr_frontsector->ceilingheight) * grTex->scaleY;
					wallVerts[0].t -= (worldhigh - gr_backsector->ceilingheight) * grTex->scaleY;
					wallVerts[1].t -= (worldhighslope - gr_backsector->ceilingheight) * grTex->scaleY;
				}
				else if (gr_linedef->flags & ML_DONTPEGTOP)
				{
					// Skewed by top
					wallVerts[0].t = (texturevpegtop + worldtop - worldhigh) * grTex->scaleY;
					wallVerts[1].t = (texturevpegtop + worldtopslope - worldhighslope) * grTex->scaleY;
				}
				else
				{
					// Skewed by bottom
					wallVerts[0].t = wallVerts[1].t = (texturevpegtop + worldtop - worldhigh) * grTex->scaleY;
					wallVerts[3].t = wallVerts[0].t - (worldtop - worldhigh) * grTex->scaleY;
					wallVerts[2].t = wallVerts[1].t - (worldtopslope - worldhighslope) * grTex->scaleY;
				}
			}

			// set top/bottom coords
			wallVerts[3].y = FIXED_TO_FLOAT(worldtop);
			wallVerts[0].y = FIXED_TO_FLOAT(worldhigh);
			wallVerts[2].y = FIXED_TO_FLOAT(worldtopslope);
			wallVerts[1].y = FIXED_TO_FLOAT(worldhighslope);

			if (!gl_drawing_stencil && gr_frontsector->numlights)
				HWR_SplitWall(gr_frontsector, wallVerts, gr_toptexture, gr_linedef->flags & ML_TFERLINE, &Surf, FF_CUTLEVEL, NULL, 0);
			else if (!gl_drawing_stencil && grTex->mipmap.flags & TF_TRANSPARENT)
				HWR_AddTransparentWall(wallVerts, &Surf, gr_toptexture, gr_linedef->flags & ML_TFERLINE, PF_Environment, false, lightnum, colormap);
			else
				HWR_ProjectWall(wallVerts, &Surf, PF_Masked, lightnum, colormap);
		}

		// check BOTTOM TEXTURE
		if ((worldlowslope > worldbottomslope || worldlow > worldbottom) && gr_bottomtexture) //only if VISIBLE!!!
		{
			{
				fixed_t texturevpegbottom = 0; // bottom

				grTex = HWR_GetTexture(gr_bottomtexture, gr_linedef->flags & ML_TFERLINE);

				// PEGGING
				if (!(gr_linedef->flags & ML_DONTPEGBOTTOM))
					texturevpegbottom = 0;
				else if (gr_linedef->flags & ML_EFFECT1)
					texturevpegbottom = worldbottom - worldlow;
				else
					texturevpegbottom = gr_frontsector->floorheight - gr_backsector->floorheight;

				texturevpegbottom += gr_sidedef->rowoffset;

				// This is so that it doesn't overflow and screw up the wall, it doesn't need to go higher than the texture's height anyway
				texturevpegbottom %= SHORT(textures[gr_bottomtexture]->height)<<FRACBITS;

				wallVerts[3].t = wallVerts[2].t = texturevpegbottom * grTex->scaleY;
				wallVerts[0].t = wallVerts[1].t = (texturevpegbottom + gr_backsector->floorheight - gr_frontsector->floorheight) * grTex->scaleY;
				wallVerts[0].s = wallVerts[3].s = cliplow * grTex->scaleX;
				wallVerts[2].s = wallVerts[1].s = cliphigh * grTex->scaleX;

				// Adjust t value for sloped walls
				if (!(gr_linedef->flags & ML_EFFECT1))
				{
					// Unskewed
					wallVerts[0].t -= (worldbottom - gr_frontsector->floorheight) * grTex->scaleY;
					wallVerts[1].t -= (worldbottomslope - gr_frontsector->floorheight) * grTex->scaleY;
					wallVerts[3].t -= (worldlow - gr_backsector->floorheight) * grTex->scaleY;
					wallVerts[2].t -= (worldlowslope - gr_backsector->floorheight) * grTex->scaleY;
				}
				else if (gr_linedef->flags & ML_DONTPEGBOTTOM)
				{
					// Skewed by bottom
					wallVerts[0].t = wallVerts[1].t = (texturevpegbottom + worldlow - worldbottom) * grTex->scaleY;
					//wallVerts[3].t = wallVerts[0].t - (worldlow - worldbottom) * grTex->scaleY; // no need, [3] is already this
					wallVerts[2].t = wallVerts[1].t - (worldlowslope - worldbottomslope) * grTex->scaleY;
				}
				else
				{
					// Skewed by top
					wallVerts[0].t = (texturevpegbottom + worldlow - worldbottom) * grTex->scaleY;
					wallVerts[1].t = (texturevpegbottom + worldlowslope - worldbottomslope) * grTex->scaleY;
				}
			}

			// set top/bottom coords
			wallVerts[3].y = FIXED_TO_FLOAT(worldlow);
			wallVerts[0].y = FIXED_TO_FLOAT(worldbottom);
			wallVerts[2].y = FIXED_TO_FLOAT(worldlowslope);
			wallVerts[1].y = FIXED_TO_FLOAT(worldbottomslope);

			if (!gl_drawing_stencil && gr_frontsector->numlights)
				HWR_SplitWall(gr_frontsector, wallVerts, gr_bottomtexture, gr_linedef->flags & ML_TFERLINE, &Surf, FF_CUTLEVEL, NULL, 0);
			else if (!gl_drawing_stencil && grTex->mipmap.flags & TF_TRANSPARENT)
				HWR_AddTransparentWall(wallVerts, &Surf, gr_bottomtexture, gr_linedef->flags & ML_TFERLINE, PF_Environment, false, lightnum, colormap);
			else
				HWR_ProjectWall(wallVerts, &Surf, PF_Masked, lightnum, colormap);
		}

		if ((gr_midtexture && HWR_BlendMidtextureSurface(&Surf)) || gr_portal == GRPORTAL_STENCIL || gr_portal == GRPORTAL_DEPTH || gl_drawing_stencil)
		{
			sector_t *front, *back;
			INT32 repeats;

			if (gr_linedef->frontsector->heightsec != -1)
				front = &sectors[gr_linedef->frontsector->heightsec];
			else
				front = gr_linedef->frontsector;

			if (gr_linedef->backsector->heightsec != -1)
				back = &sectors[gr_linedef->backsector->heightsec];
			else
				back = gr_linedef->backsector;

			if (gr_sidedef->repeatcnt)
				repeats = 1 + gr_sidedef->repeatcnt;
			else if (gr_linedef->flags & ML_EFFECT5 || gr_portal == GRPORTAL_STENCIL || gr_portal == GRPORTAL_DEPTH)
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

				repeats = (high - low)/textureheight[gr_sidedef->midtexture];
				if ((high-low)%textureheight[gr_sidedef->midtexture])
					repeats++; // tile an extra time to fill the gap -- Monster Iestyn
			}
			else
				repeats = 1;

			fixed_t midtexheight = textureheight[gr_midtexture] * repeats;
			fixed_t popentop, popenbottom, polytop, polybottom, lowcut, highcut;
			fixed_t popentopslope, popenbottomslope, polytopslope, polybottomslope, lowcutslope, highcutslope;

			// SoM: a little note: This code re-arranging will
			// fix the bug in Nimrod map02. popentop and popenbottom
			// record the limits the texture can be displayed in.
			// polytop and polybottom, are the ideal (i.e. unclipped)
			// heights of the polygon, and h & l, are the final (clipped)
			// poly coords.

			// NOTE: With polyobjects, whenever you need to check the properties of the polyobject sector it belongs to,
			// you must use the linedef's backsector to be correct
			// From CB
			if (gr_curline->polyseg)
			{
				popentop = popentopslope = back->ceilingheight;
				popenbottom = popenbottomslope = back->floorheight;
			}
			else
            {
				popentop = min(worldtop, worldhigh);
				popenbottom = max(worldbottom, worldlow);

				popentopslope = min(worldtopslope, worldhighslope);
				popenbottomslope = max(worldbottomslope, worldlowslope);
			}

			if (gr_linedef->flags & ML_EFFECT2)
			{
				if (!!(gr_linedef->flags & ML_DONTPEGBOTTOM) ^ !!(gr_linedef->flags & ML_EFFECT3))
				{
					polybottom = max(front->floorheight, back->floorheight) + gr_sidedef->rowoffset;
					polybottomslope = polybottom;
					polytop = polybottom + midtexheight;
					polytopslope = polytop;
				}
				else
				{
					polytop = min(front->ceilingheight, back->ceilingheight) + gr_sidedef->rowoffset;
					polytopslope = polytop;
					polybottom = polytop - midtexheight;
					polybottomslope = polybottom;
				}
			}
			else if (!!(gr_linedef->flags & ML_DONTPEGBOTTOM) ^ !!(gr_linedef->flags & ML_EFFECT3))
			{
				polybottom = popenbottom + gr_sidedef->rowoffset;
				polytop = polybottom + midtexheight;
				polybottomslope = popenbottomslope + gr_sidedef->rowoffset;
				polytopslope = polybottomslope + midtexheight;
			}
			else
			{
				polytop = popentop + gr_sidedef->rowoffset;
				polybottom = polytop - textureheight[gr_midtexture]*repeats;
				polybottom = polytop - midtexheight;
				polytopslope = popentopslope + gr_sidedef->rowoffset;
				polybottomslope = polytopslope - midtexheight;
			}

			// CB
			// NOTE: With polyobjects, whenever you need to check the properties of the polyobject sector it belongs to,
			// you must use the linedef's backsector to be correct
			if (gr_curline->polyseg)
			{
				lowcut = polybottom;
				highcut = polytop;
				lowcutslope = polybottomslope;
				highcutslope = polytopslope;
			}
			else
			{
				// The cut-off values of a linedef can always be constant, since every line has an absoulute front and or back sector
				lowcut = popenbottom;
				highcut = popentop;
				lowcutslope = popenbottomslope;
				highcutslope = popentopslope;
			}

			h = min(highcut, polytop);
			l = max(polybottom, lowcut);
			hS = min(highcutslope, polytopslope);
			lS = max(polybottomslope, lowcutslope);

			// PEGGING
			fixed_t texturevpeg, texturevpegslope;

			grTex = HWR_GetTexture(gr_midtexture, gr_linedef->flags & ML_TFERLINE);

			if (cv_slopepegfix.value)
			{
				if (!!(gr_linedef->flags & ML_DONTPEGBOTTOM) ^ !!(gr_linedef->flags & ML_EFFECT3))
				{
					texturevpeg = midtexheight - h + polybottom;
					texturevpegslope = midtexheight - hS + polybottomslope;
				}
				else
				{
					texturevpeg = polytop - h;
					texturevpegslope = polytopslope - hS;
				}

				wallVerts[3].t = texturevpeg * grTex->scaleY;
				wallVerts[0].t = (h - l + texturevpeg) * grTex->scaleY;
				wallVerts[2].t = texturevpegslope * grTex->scaleY;
				wallVerts[1].t = (hS - lS + texturevpegslope) * grTex->scaleY;
				wallVerts[0].s = wallVerts[3].s = cliplow * grTex->scaleX;
				wallVerts[2].s = wallVerts[1].s = cliphigh * grTex->scaleX;

				// set top/bottom coords
				// Take the texture peg into account, rather than changing the offsets past
				// where the polygon might not be.
				wallVerts[3].y = FIXED_TO_FLOAT(h);
				wallVerts[0].y = FIXED_TO_FLOAT(l);
				wallVerts[2].y = FIXED_TO_FLOAT(hS);
				wallVerts[1].y = FIXED_TO_FLOAT(lS);
			}
			else
			{
				// PEGGING
				if (!!(gr_linedef->flags & ML_DONTPEGBOTTOM) ^ !!(gr_linedef->flags & ML_EFFECT3))
					texturevpeg = textureheight[gr_sidedef->midtexture]*repeats - h + polybottom;
				else
					texturevpeg = polytop - h;

				wallVerts[3].t = wallVerts[2].t = texturevpeg * grTex->scaleY;
				wallVerts[0].t = wallVerts[1].t = (h - l + texturevpeg) * grTex->scaleY;
				wallVerts[0].s = wallVerts[3].s = cliplow * grTex->scaleX;
				wallVerts[2].s = wallVerts[1].s = cliphigh * grTex->scaleX;
				
				// set top/bottom coords
				// Take the texture peg into account, rather than changing the offsets past
				// where the polygon might not be.
				wallVerts[2].y = wallVerts[3].y = FIXED_TO_FLOAT(h);
				wallVerts[0].y = wallVerts[1].y = FIXED_TO_FLOAT(l);			

				// Correct to account for slopes
				{
					fixed_t midtextureslant;

					if (gr_linedef->flags & ML_EFFECT2)
						midtextureslant = 0;
					else if (!!(gr_linedef->flags & ML_DONTPEGBOTTOM) ^ !!(gr_linedef->flags & ML_EFFECT3))
						midtextureslant = worldlow < worldbottom
								  ? worldbottomslope-worldbottom
								  : worldlowslope-worldlow;
					else
						midtextureslant = worldtop < worldhigh
								  ? worldtopslope-worldtop
								  : worldhighslope-worldhigh;

					polytop += midtextureslant;
					polybottom += midtextureslant;

					highcut += worldtop < worldhigh
							 ? worldtopslope-worldtop
							 : worldhighslope-worldhigh;
					lowcut += worldlow < worldbottom
							? worldbottomslope-worldbottom
							: worldlowslope-worldlow;

					// Texture stuff
					h = min(highcut, polytop);
					l = max(polybottom, lowcut);

					{
						// PEGGING
						if (!!(gr_linedef->flags & ML_DONTPEGBOTTOM) ^ !!(gr_linedef->flags & ML_EFFECT3))
							texturevpeg = textureheight[gr_sidedef->midtexture]*repeats - h + polybottom;
						else
							texturevpeg = polytop - h;
						wallVerts[2].t = texturevpeg * grTex->scaleY;
						wallVerts[1].t = (h - l + texturevpeg) * grTex->scaleY;
					}

					wallVerts[2].y = FIXED_TO_FLOAT(h);
					wallVerts[1].y = FIXED_TO_FLOAT(l);
				}
			}

			// TODO: Actually use the surface's flags so that I don't have to do this
			FUINT blendmode = Surf.PolyFlags;

			// Render midtextures on two-sided lines with a z-buffer offset.
			// This will cause the midtexture appear on top, if a FOF overlaps with it.
			if (cv_fofzfightfix.value)
				blendmode |= PF_Decal;

			if (!gl_drawing_stencil && gr_frontsector->numlights)
			{
				if (!(blendmode & PF_Masked))
					HWR_SplitWall(gr_frontsector, wallVerts, gr_midtexture, gr_linedef->flags & ML_TFERLINE, &Surf, FF_TRANSLUCENT, NULL, cv_fofzfightfix.value ? PF_Decal : 0);
				else
					HWR_SplitWall(gr_frontsector, wallVerts, gr_midtexture, gr_linedef->flags & ML_TFERLINE, &Surf, FF_CUTLEVEL, NULL, cv_fofzfightfix.value ? PF_Decal : 0);
			}
			else if (!gl_drawing_stencil && !(blendmode & PF_Masked))
				HWR_AddTransparentWall(wallVerts, &Surf, gr_midtexture, gr_linedef->flags & ML_TFERLINE, blendmode, false, lightnum, colormap);
			else
				HWR_ProjectWall(wallVerts, &Surf, blendmode, lightnum, colormap);
		}
	}
	else
	{
		// Single sided line... Deal only with the middletexture (if one exists)

		if (gr_midtexture && gr_linedef->special != HORIZONSPECIAL) // Ignore horizon line for OGL
		{
			{
				fixed_t     texturevpeg;
				// PEGGING
				if ((gr_linedef->flags & (ML_DONTPEGBOTTOM|ML_EFFECT2)) == (ML_DONTPEGBOTTOM|ML_EFFECT2))
					texturevpeg = gr_frontsector->floorheight + textureheight[gr_sidedef->midtexture] - gr_frontsector->ceilingheight + gr_sidedef->rowoffset;
				else if (gr_linedef->flags & ML_DONTPEGBOTTOM)
					texturevpeg = worldbottom + textureheight[gr_sidedef->midtexture] - worldtop + gr_sidedef->rowoffset;
				else
					// top of texture at top
					texturevpeg = gr_sidedef->rowoffset;

				grTex = HWR_GetTexture(gr_midtexture, gr_linedef->flags & ML_TFERLINE);

				wallVerts[3].t = wallVerts[2].t = texturevpeg * grTex->scaleY;
				wallVerts[0].t = wallVerts[1].t = (texturevpeg + gr_frontsector->ceilingheight - gr_frontsector->floorheight) * grTex->scaleY;
				wallVerts[0].s = wallVerts[3].s = cliplow * grTex->scaleX;
				wallVerts[2].s = wallVerts[1].s = cliphigh * grTex->scaleX;

				// Texture correction for slopes
				if (gr_linedef->flags & ML_EFFECT2) {
					wallVerts[3].t += (gr_frontsector->ceilingheight - worldtop) * grTex->scaleY;
					wallVerts[2].t += (gr_frontsector->ceilingheight - worldtopslope) * grTex->scaleY;
					wallVerts[0].t += (gr_frontsector->floorheight - worldbottom) * grTex->scaleY;
					wallVerts[1].t += (gr_frontsector->floorheight - worldbottomslope) * grTex->scaleY;
				} else if (gr_linedef->flags & ML_DONTPEGBOTTOM) {
					wallVerts[3].t = wallVerts[0].t + (worldbottom-worldtop) * grTex->scaleY;
					wallVerts[2].t = wallVerts[1].t + (worldbottomslope-worldtopslope) * grTex->scaleY;
				} else {
					wallVerts[0].t = wallVerts[3].t - (worldbottom-worldtop) * grTex->scaleY;
					wallVerts[1].t = wallVerts[2].t - (worldbottomslope-worldtopslope) * grTex->scaleY;
				}
			}
			//Set textures properly on single sided walls that are sloped
			wallVerts[3].y = FIXED_TO_FLOAT(worldtop);
			wallVerts[0].y = FIXED_TO_FLOAT(worldbottom);
			wallVerts[2].y = FIXED_TO_FLOAT(worldtopslope);
			wallVerts[1].y = FIXED_TO_FLOAT(worldbottomslope);

			if (gr_frontsector->numlights)
				HWR_SplitWall(gr_frontsector, wallVerts, gr_midtexture, gr_linedef->flags & ML_TFERLINE, &Surf, FF_CUTLEVEL, NULL, 0);
			// I don't think that solid walls can use translucent linedef types...
			else
			{
				if (grTex->mipmap.flags & TF_TRANSPARENT)
					HWR_AddTransparentWall(wallVerts, &Surf, gr_midtexture, gr_linedef->flags & ML_TFERLINE, PF_Environment, false, lightnum, colormap);
				else
					HWR_ProjectWall(wallVerts, &Surf, PF_Masked, lightnum, colormap);
			}
		}
		else
		{
			//Set textures properly on single sided walls that are sloped
			wallVerts[3].y = FIXED_TO_FLOAT(worldtop);
			wallVerts[0].y = FIXED_TO_FLOAT(worldbottom);
			wallVerts[2].y = FIXED_TO_FLOAT(worldtopslope);
			wallVerts[1].y = FIXED_TO_FLOAT(worldbottomslope);

			// When there's no midtexture, draw a skywall to prevent rendering behind it
			HWR_DrawSkyWall(wallVerts, &Surf);
		}

		// Single sided lines are simple for skywalls, just need to draw from the top or bottom of the sector if there's
		// a sky flat
		if (!gr_curline->polyseg)
		{
			if (gr_frontsector->ceilingpic == skyflatnum) // It's a single-sided line with sky for its sector
			{
				wallVerts[3].y = wallVerts[2].y = FIXED_TO_FLOAT(INT32_MAX);
				wallVerts[0].y = FIXED_TO_FLOAT(worldtop);
				wallVerts[1].y = FIXED_TO_FLOAT(worldtopslope);
				HWR_DrawSkyWall(wallVerts, &Surf);
			}
			if (gr_frontsector->floorpic == skyflatnum)
			{
				wallVerts[3].y = FIXED_TO_FLOAT(worldbottom);
				wallVerts[2].y = FIXED_TO_FLOAT(worldbottomslope);
				wallVerts[0].y = wallVerts[1].y = FIXED_TO_FLOAT(INT32_MIN);

				HWR_DrawSkyWall(wallVerts, &Surf);
			}
		}
	}

	//Hurdler: 3d-floors test
	if (!gl_drawing_stencil && gr_frontsector && gr_backsector && gr_frontsector->tag != gr_backsector->tag && (gr_backsector->ffloors || gr_frontsector->ffloors))
	{
		ffloor_t * rover;
		fixed_t    highcut = 0, lowcut = 0;
		fixed_t lowcutslope = 0, highcutslope = 0;

		// Used for height comparisons and etc across FOFs and slopes
		fixed_t high1, highslope1, low1, lowslope1;

		INT32 texnum;
		line_t * newline = NULL; // Multi-Property FOF

		if (!cv_grfofcut.value)
		{
			///TODO add slope support (fixing cutoffs, proper wall clipping) - maybe just disable highcut/lowcut if either sector or FOF has a slope
			///     to allow fun plane intersecting in OGL? But then people would abuse that and make software look bad. :C
			highcut = gr_frontsector->ceilingheight < gr_backsector->ceilingheight ? gr_frontsector->ceilingheight : gr_backsector->ceilingheight;
			lowcut = gr_frontsector->floorheight > gr_backsector->floorheight ? gr_frontsector->floorheight : gr_backsector->floorheight;
		}
		else
		{
			lowcut = max(worldbottom, worldlow);
			highcut = min(worldtop, worldhigh);
			lowcutslope = max(worldbottomslope, worldlowslope);
			highcutslope = min(worldtopslope, worldhighslope);
		}

		if (gr_backsector->ffloors)
		{
			for (rover = gr_backsector->ffloors; rover; rover = rover->next)
			{
				if (!(rover->flags & FF_EXISTS) || !(rover->flags & FF_RENDERSIDES) || (rover->flags & FF_INVERTSIDES))
					continue;

				if (!cv_grfofcut.value)
				{
					if (*rover->topheight < lowcut || *rover->bottomheight > highcut)
						continue;
				}
				else
				{
					SLOPEPARAMS(*rover->t_slope, high1, highslope1, *rover->topheight)
					SLOPEPARAMS(*rover->b_slope, low1,  lowslope1,  *rover->bottomheight)

					if ((high1 < lowcut && highslope1 < lowcutslope) || (low1 > highcut && lowslope1 > highcutslope))
						continue;
				}

				texnum = R_GetTextureNum(sides[rover->master->sidenum[0]].midtexture);

				if (rover->master->flags & ML_TFERLINE)
				{
					size_t linenum = min((size_t)(gr_curline->linedef-gr_backsector->lines[0]), rover->master->frontsector->linecount);
					newline = rover->master->frontsector->lines[0] + linenum;
					texnum = R_GetTextureNum(sides[newline->sidenum[0]].midtexture);
				}

				h  = P_GetFFloorTopZAt   (rover, v1x, v1y);
				hS = P_GetFFloorTopZAt   (rover, v2x, v2y);
				l  = P_GetFFloorBottomZAt(rover, v1x, v1y);
				lS = P_GetFFloorBottomZAt(rover, v2x, v2y);

				if (!cv_grfofcut.value)
				{
					if (!(*rover->t_slope) && !gr_frontsector->c_slope && !gr_backsector->c_slope && h > highcut)
						h = hS = highcut;
					if (!(*rover->b_slope) && !gr_frontsector->f_slope && !gr_backsector->f_slope && l < lowcut)
						l = lS = lowcut;
				}
				else
				{
					// Adjust the heights so the FOF does not overlap with top and bottom textures.
					if (h >= highcut && hS >= highcutslope)
					{
						h = highcut;
						hS = highcutslope;
					}
					if (l <= lowcut && lS <= lowcutslope)
					{
						l = lowcut;
						lS = lowcutslope;
					}
				}

				//Hurdler: HW code starts here
				//FIXME: check if peging is correct
				// set top/bottom coords

				wallVerts[3].y = FIXED_TO_FLOAT(h);
				wallVerts[2].y = FIXED_TO_FLOAT(hS);
				wallVerts[0].y = FIXED_TO_FLOAT(l);
				wallVerts[1].y = FIXED_TO_FLOAT(lS);

				if (rover->flags & FF_FOG)
				{
					wallVerts[3].t = wallVerts[2].t = 0;
					wallVerts[0].t = wallVerts[1].t = 0;
					wallVerts[0].s = wallVerts[3].s = 0;
					wallVerts[2].s = wallVerts[1].s = 0;
				}
				else
				{
					fixed_t texturevpeg;
					boolean attachtobottom = false;
					boolean slopeskew = false; // skew FOF walls with slopes?

					// Wow, how was this missing from OpenGL for so long?
					// ...Oh well, anyway, Lower Unpegged now changes pegging of FOFs like in software
					// -- Monster Iestyn 26/06/18
					if (newline)
					{
						texturevpeg = sides[newline->sidenum[0]].rowoffset;
						attachtobottom = !!(newline->flags & ML_DONTPEGBOTTOM);
						slopeskew = !!(newline->flags & ML_DONTPEGTOP);
					}
					else
					{
						texturevpeg = sides[rover->master->sidenum[0]].rowoffset;
						attachtobottom = !!(gr_linedef->flags & ML_DONTPEGBOTTOM);
						slopeskew = !!(rover->master->flags & ML_DONTPEGTOP);
					}

					grTex = HWR_GetTexture(texnum, noencore);

					if (!slopeskew) // no skewing
					{
						if (attachtobottom)
							texturevpeg -= *rover->topheight - *rover->bottomheight;
						wallVerts[3].t = (*rover->topheight - h + texturevpeg) * grTex->scaleY;
						wallVerts[2].t = (*rover->topheight - hS + texturevpeg) * grTex->scaleY;
						wallVerts[0].t = (*rover->topheight - l + texturevpeg) * grTex->scaleY;
						wallVerts[1].t = (*rover->topheight - lS + texturevpeg) * grTex->scaleY;
					}
					else
					{
						if (!attachtobottom) // skew by top
						{
							wallVerts[3].t = wallVerts[2].t = texturevpeg * grTex->scaleY;
							wallVerts[0].t = (h - l + texturevpeg) * grTex->scaleY;
							wallVerts[1].t = (hS - lS + texturevpeg) * grTex->scaleY;
						}
						else // skew by bottom
						{
							wallVerts[0].t = wallVerts[1].t = texturevpeg * grTex->scaleY;
							wallVerts[3].t = wallVerts[0].t - (h - l) * grTex->scaleY;
							wallVerts[2].t = wallVerts[1].t - (hS - lS) * grTex->scaleY;
						}
					}

					wallVerts[0].s = wallVerts[3].s = cliplow * grTex->scaleX;
					wallVerts[2].s = wallVerts[1].s = cliphigh * grTex->scaleX;
				}
				if (rover->flags & FF_FOG)
				{
					FBITFIELD blendmode;

					blendmode = PF_Fog|PF_NoTexture;

					if (HWR_ShouldUsePaletteRendering())
					{
						lightnum = SOFTLIGHT(rover->master->frontsector->lightlevel);
						colormap = rover->master->frontsector->extra_colormap;
						lightnum = colormap ? lightnum : HWR_CalcWallLight(lightnum, vs.x, vs.y, ve.x, ve.y);
					}else{
						lightnum = HWR_CalcWallLight(rover->master->frontsector->lightlevel, vs.x, vs.y, ve.x, ve.y);
						colormap = rover->master->frontsector->extra_colormap;
					}
					
					Surf.PolyColor.s.alpha = HWR_FogBlockAlpha(SOFTLIGHT(rover->master->frontsector->lightlevel), rover->master->frontsector->extra_colormap);

					if (gr_frontsector->numlights)
						HWR_SplitWall(gr_frontsector, wallVerts, 0, false, &Surf, rover->flags, rover, 0);
					else
						HWR_AddTransparentWall(wallVerts, &Surf, 0, false, blendmode, true, lightnum, colormap);
				}
				else
				{
					FBITFIELD blendmode = PF_Masked;

					if (rover->flags & FF_TRANSLUCENT && rover->alpha < 256)
					{
						blendmode = PF_Translucent;
						Surf.PolyColor.s.alpha = max(0, min(rover->alpha, 255));
					}

					if (gr_frontsector->numlights)
						HWR_SplitWall(gr_frontsector, wallVerts, texnum, noencore, &Surf, rover->flags, rover, 0);
					else
					{
						if (blendmode != PF_Masked)
							HWR_AddTransparentWall(wallVerts, &Surf, texnum, gr_linedef->flags & ML_TFERLINE, blendmode, false, lightnum, colormap);
						else
							HWR_ProjectWall(wallVerts, &Surf, PF_Masked, lightnum, colormap);
					}
				}
			}
		}

		if (gr_frontsector->ffloors) // Putting this seperate should allow 2 FOF sectors to be connected without too many errors? I think?
		{
			for (rover = gr_frontsector->ffloors; rover; rover = rover->next)
			{
				if (!(rover->flags & FF_EXISTS) || !(rover->flags & FF_RENDERSIDES) || !(rover->flags & FF_ALLSIDES))
					continue;

				if (!cv_grfofcut.value)
				{
					if (*rover->topheight < lowcut || *rover->bottomheight > highcut)
						continue;
				}
				else
				{
					SLOPEPARAMS(*rover->t_slope, high1, highslope1, *rover->topheight)
					SLOPEPARAMS(*rover->b_slope, low1,  lowslope1,  *rover->bottomheight)

					if ((high1 < lowcut && highslope1 < lowcutslope) || (low1 > highcut && lowslope1 > highcutslope))
						continue;
				}


				texnum = R_GetTextureNum(sides[rover->master->sidenum[0]].midtexture);

				if (rover->master->flags & ML_TFERLINE)
				{
					size_t linenum = min((size_t)(gr_curline->linedef-gr_backsector->lines[0]), rover->master->frontsector->linecount);
					newline = rover->master->frontsector->lines[0] + linenum;
					texnum = R_GetTextureNum(sides[newline->sidenum[0]].midtexture);
				}

				h  = P_GetFFloorTopZAt   (rover, v1x, v1y);
				hS = P_GetFFloorTopZAt   (rover, v2x, v2y);
				l  = P_GetFFloorBottomZAt(rover, v1x, v1y);
				lS = P_GetFFloorBottomZAt(rover, v2x, v2y);

				if (!cv_grfofcut.value)
				{
					if (!(*rover->t_slope) && !gr_frontsector->c_slope && !gr_backsector->c_slope && h > highcut)
						h = hS = highcut;
					if (!(*rover->b_slope) && !gr_frontsector->f_slope && !gr_backsector->f_slope && l < lowcut)
						l = lS = lowcut;
				}
				else
				{
					// Adjust the heights so the FOF does not overlap with top and bottom textures.
					if (h >= highcut && hS >= highcutslope)
					{
						h = highcut;
						hS = highcutslope;
					}
					if (l <= lowcut && lS <= lowcutslope)
					{
						l = lowcut;
						lS = lowcutslope;
					}
				}
				
				//Hurdler: HW code starts here
				//FIXME: check if peging is correct
				// set top/bottom coords

				wallVerts[3].y = FIXED_TO_FLOAT(h);
				wallVerts[2].y = FIXED_TO_FLOAT(hS);
				wallVerts[0].y = FIXED_TO_FLOAT(l);
				wallVerts[1].y = FIXED_TO_FLOAT(lS);

				if (rover->flags & FF_FOG)
				{
					wallVerts[3].t = wallVerts[2].t = 0;
					wallVerts[0].t = wallVerts[1].t = 0;
					wallVerts[0].s = wallVerts[3].s = 0;
					wallVerts[2].s = wallVerts[1].s = 0;
				}
				else
				{
					grTex = HWR_GetTexture(texnum, noencore);

					if (newline)
					{
						wallVerts[3].t = wallVerts[2].t = (*rover->topheight - h + sides[newline->sidenum[0]].rowoffset) * grTex->scaleY;
						wallVerts[0].t = wallVerts[1].t = (h - l + (*rover->topheight - h + sides[newline->sidenum[0]].rowoffset)) * grTex->scaleY;
					}
					else
					{
						wallVerts[3].t = wallVerts[2].t = (*rover->topheight - h + sides[rover->master->sidenum[0]].rowoffset) * grTex->scaleY;
						wallVerts[0].t = wallVerts[1].t = (h - l + (*rover->topheight - h + sides[rover->master->sidenum[0]].rowoffset)) * grTex->scaleY;
					}

					wallVerts[0].s = wallVerts[3].s = cliplow * grTex->scaleX;
					wallVerts[2].s = wallVerts[1].s = cliphigh * grTex->scaleX;
				}

				if (rover->flags & FF_FOG)
				{
					FBITFIELD blendmode;

					blendmode = PF_Fog|PF_NoTexture;

					if (HWR_ShouldUsePaletteRendering())
					{
						lightnum = SOFTLIGHT(rover->master->frontsector->lightlevel);
						colormap = rover->master->frontsector->extra_colormap;
						lightnum = colormap ? lightnum : HWR_CalcWallLight(lightnum, vs.x, vs.y, ve.x, ve.y);
					}else{
						lightnum = HWR_CalcWallLight(rover->master->frontsector->lightlevel, vs.x, vs.y, ve.x, ve.y);
						colormap = rover->master->frontsector->extra_colormap;
					}

					Surf.PolyColor.s.alpha = HWR_FogBlockAlpha(SOFTLIGHT(rover->master->frontsector->lightlevel), rover->master->frontsector->extra_colormap);

					if (gr_backsector->numlights)
						HWR_SplitWall(gr_backsector, wallVerts, 0, false, &Surf, rover->flags, rover, 0);
					else
						HWR_AddTransparentWall(wallVerts, &Surf, 0, false, blendmode, true, lightnum, colormap);
				}
				else
				{
					FBITFIELD blendmode = PF_Masked;

					if (rover->flags & FF_TRANSLUCENT && rover->alpha < 256)
					{
						blendmode = PF_Translucent;
						Surf.PolyColor.s.alpha = max(0, min(rover->alpha, 255));
					}

					if (gr_backsector->numlights)
						HWR_SplitWall(gr_backsector, wallVerts, texnum, noencore, &Surf, rover->flags, rover, 0);
					else
					{
						if (blendmode != PF_Masked)
							HWR_AddTransparentWall(wallVerts, &Surf, texnum, noencore, blendmode, false, lightnum, colormap);
						else
							HWR_ProjectWall(wallVerts, &Surf, PF_Masked, lightnum, colormap);
					}
				}
			}
		}
	}

#undef SLOPEPARAMS
//Hurdler: end of 3d-floors test
}

// From PrBoom:
//
// e6y: Check whether the player can look beyond this line, returns true if we can't
//

boolean checkforemptylines = true;
// Don't modify anything here, just check
// Kalaron: Modified for sloped linedefs
static boolean CheckClip(sector_t * afrontsector, sector_t * abacksector)
{
	fixed_t frontf1,frontf2, frontc1, frontc2; // front floor/ceiling ends
	fixed_t backf1, backf2, backc1, backc2; // back floor ceiling ends

	// GZDoom method of sloped line clipping

	if (afrontsector->f_slope || afrontsector->c_slope || abacksector->f_slope || abacksector->c_slope)
	{
		fixed_t v1x, v1y, v2x, v2y; // the seg's vertexes as fixed_t
		
		if (gr_curline->pv1)
		{
			v1x = FLOAT_TO_FIXED(((polyvertex_t *)gr_curline->pv1)->x);
			v1y = FLOAT_TO_FIXED(((polyvertex_t *)gr_curline->pv1)->y);
		}
		else
		{
			v1x = gr_curline->v1->x;
			v1y = gr_curline->v1->y;
		}
		if (gr_curline->pv2)
		{
			v2x = FLOAT_TO_FIXED(((polyvertex_t *)gr_curline->pv2)->x);
			v2y = FLOAT_TO_FIXED(((polyvertex_t *)gr_curline->pv2)->y);
		}
		else
		{
			v2x = gr_curline->v2->x;
			v2y = gr_curline->v2->y;
		}
		
#define SLOPEPARAMS(slope, end1, end2, normalheight) \
		if (slope) { \
			end1 = P_GetZAt(slope, v1x, v1y); \
			end2 = P_GetZAt(slope, v2x, v2y); \
		} else \
			end1 = end2 = normalheight;

		SLOPEPARAMS(afrontsector->f_slope, frontf1, frontf2, afrontsector->floorheight)
		SLOPEPARAMS(afrontsector->c_slope, frontc1, frontc2, afrontsector->ceilingheight)
		SLOPEPARAMS( abacksector->f_slope, backf1,  backf2,  abacksector->floorheight)
		SLOPEPARAMS( abacksector->c_slope, backc1,  backc2,  abacksector->ceilingheight)
#undef SLOPEPARAMS
	}
	else
	{
		frontf1 = frontf2 = afrontsector->floorheight;
		frontc1 = frontc2 = afrontsector->ceilingheight;
		backf1 = backf2 = abacksector->floorheight;
		backc1 = backc2 = abacksector->ceilingheight;
	}

	// using this check with portals causes weird culling issues on ante-station
	//if (!portalclipline && (afrontsector == viewsector || abacksector == viewsector))
	if (afrontsector == viewsector || abacksector == viewsector)
	{
		fixed_t viewf1, viewf2, viewc1, viewc2;
		if (afrontsector == viewsector)
		{
			//if (printportals)
			//	CONS_Printf("CheckClip frontsector is viewsector\n");
			viewf1 = frontf1;
			viewf2 = frontf2;
			viewc1 = frontc1;
			viewc2 = frontc2;
		}
		else
		{
			//if (printportals)
			//	CONS_Printf("CheckClip backsector is viewsector\n");
			viewf1 = backf1;
			viewf2 = backf2;
			viewc1 = backc1;
			viewc2 = backc2;
		}

		// check if camera is outside the bounds of the floor and the ceiling (noclipping)
		// either above the ceiling or below the floor
		if ((viewz > viewc1 && viewz > viewc2) || (viewz < viewf1 && viewz < viewf2))
			return false;
	}

	// now check for closed sectors!

	// here we're talking about a CEILING lower than a floor. ...yeah we don't even need to bother.
	if (backc1 <= frontf1 && backc2 <= frontf2)
	{
		checkforemptylines = false;
		//if (portalclipline)// during portal rendering view position may cause undesired culling and the above code has some wrong side effects
			//return false;
		//else
			return true;
	}

	// here we're talking about floors higher than ceilings, don't even bother either.
	if (backf1 >= frontc1 && backf2 >= frontc2)
	{
		checkforemptylines = false;
		return true;
	}

	// Lat: Ok, here's what we need to do, we want to draw thok barriers. Let's define what a thok barrier is;
	// -Must have ceilheight <= floorheight
	// -ceilpic must be skyflatnum
	// -an adjacant sector needs to have a ceilingheight or a floor height different than the one we have, otherwise, it's just a huge ass wall, we shouldn't render past it.
	// -said adjacant sector cannot also be a thok barrier, because that's also dumb and we could render far more than we need to as a result :V

	if (backc1 <= backf1 && backc2 <= backf2)
	{
		checkforemptylines = false;

		// before we do anything, if both sectors are thok barriers, GET ME OUT OF HERE!
		if (frontc1 <= backc1 && frontc2 <= backc2)
			return true;	// STOP RENDERING.

		// draw floors at the top of thok barriers:
		if (backc1 < frontc1 || backc2 < frontc2)
			return false;

		if (backf1 > frontf1 || backf2 > frontf2)
			return false;

		return true;
	}

	// Window.
	// We know it's a window when the above isn't true and the back and front sectors don't match
	if (backc1 != frontc1 || backc2 != frontc2
		|| backf1 != frontf1 || backf2 != frontf2)
	{
		checkforemptylines = false;
		return false;
	}

	// In this case we just need to check whether there is actually a need to render any lines, so checkforempty lines
	// stays true
	return false;
}

// HWR_AddLine
// Clips the given segment and adds any visible pieces to the line list.
static void HWR_AddLine(seg_t *line)
{
	angle_t angle1, angle2;

	// SoM: Backsector needs to be run through R_FakeFlat
	static sector_t tempsec;

	fixed_t v1x, v1y, v2x, v2y; // the seg's vertexes as fixed_t

	boolean dont_draw = false;

	if (line->polyseg && !(line->polyseg->flags & POF_RENDERSIDES))
		return;

	gr_curline = line;

	if (gr_curline->pv1)
	{
		v1x = FLOAT_TO_FIXED(((polyvertex_t *)gr_curline->pv1)->x);
		v1y = FLOAT_TO_FIXED(((polyvertex_t *)gr_curline->pv1)->y);
	}
	else
	{
		v1x = gr_curline->v1->x;
		v1y = gr_curline->v1->y;
	}
	if (gr_curline->pv2)
	{
		v2x = FLOAT_TO_FIXED(((polyvertex_t *)gr_curline->pv2)->x);
		v2y = FLOAT_TO_FIXED(((polyvertex_t *)gr_curline->pv2)->y);
	}
	else
	{
		v2x = gr_curline->v2->x;
		v2y = gr_curline->v2->y;
	}

	// OPTIMIZE: quickly reject orthogonal back sides.
	if (cv_pointoangleexor64.value)
	{
		angle1 = R_PointToAngle64(v1x, v1y);
		angle2 = R_PointToAngle64(v2x, v2y);
	}
	else
	{
		angle1 = R_PointToAngleEx(viewx, viewy, v1x, v1y);
		angle2 = R_PointToAngleEx(viewx, viewy, v2x, v2y);
	}

	 // PrBoom: Back side, i.e. backface culling - read: endAngle >= startAngle!
	if (angle2 - angle1 < ANGLE_180 || !gr_curline->linedef)
		return;

	// PrBoom: use REAL clipping math YAYYYYYYY!!!
	if (!gld_clipper_SafeCheckRange(angle2, angle1))
		return;

	checkforemptylines = true;

	gr_backsector = line->backsector;

	// do an extra culling check when rendering portals
	// check if any line vertex is on the viewable side of the portal target line
	// if not, the line can be culled.
	if (portalclipline)// portalclipline should be NULL when we are not rendering portal contents
	{
		vertex_t closest_point;
		boolean pass = false;

		// similar idea than in PortalCheckBBox, but checking the line vertices instead
		// TODO could make the check more efficient and not check anything if pass==true from earlier or something
		P_ClosestPointOnLine(line->v1->x, line->v1->y, portalclipline, &closest_point);
		if (closest_point.x != line->v1->x || closest_point.y != line->v1->y)
		{
			if (P_PointOnLineSide(line->v1->x, line->v1->y, portalclipline) != portalviewside)
				pass = true;
		}
		P_ClosestPointOnLine(line->v2->x, line->v2->y, portalclipline, &closest_point);
		if (closest_point.x != line->v2->x || closest_point.y != line->v2->y)
		{
			if (P_PointOnLineSide(line->v2->x, line->v2->y, portalclipline) != portalviewside)
				pass = true;
		}
		if (!pass)
			return;
	}

	if (gr_portal == GRPORTAL_STENCIL || gr_portal == GRPORTAL_DEPTH)
	{
		gr_backsector = line->backsector;
		goto doaddline;
	}

	if (line->linedef->special == 40)
	{
		if (line->side == 0)
		{
			// Find the other side!
			INT32 line2 = P_FindSpecialLineFromTag(40, line->linedef->tag, -1);
			if (line->linedef == &lines[line2])
				line2 = P_FindSpecialLineFromTag(40, line->linedef->tag, line2);
			if (line2 >= 0) // found it!
			{
				if (gr_portal == GRPORTAL_SEARCH)
					HWR_Portal_Add2Lines(line->linedef-lines, line2, line);
				else if (gr_portal == GRPORTAL_INSIDE)
					dont_draw = true;
			}
		}
		else
			return;// dont do anything with the other side i guess?
	}

doaddline:

	if (!line->backsector)
	{
		gld_clipper_SafeAddClipRange(angle2, angle1);
	}
	else
	{
		gr_backsector = R_FakeFlat(gr_backsector, &tempsec, NULL, NULL, true);

		if (CheckClip(gr_frontsector, gr_backsector))
		{
			gld_clipper_SafeAddClipRange(angle2, angle1);
			checkforemptylines = false;
		}
		// Reject empty lines used for triggers and special events.
		// Identical floor and ceiling on both sides,
		//  identical light levels on both sides,
		//  and no middle texture.
		if (checkforemptylines && R_IsEmptyLine(line, gr_frontsector, gr_backsector))
			return;
    }

	if (gr_portal != GRPORTAL_SEARCH && !dont_draw)// no need to do this during the portal check
		HWR_ProcessSeg(); // Doesn't need arguments because they're defined globally :D

	return;
}

// HWR_CheckBBox
// Checks BSP node/subtree bounding box.
// Returns true
//  if some part of the bbox might be visible.
//
// modified to use local variables

static boolean HWR_CheckBBox(const fixed_t *bspcoord)
{
	INT32 boxpos;
	fixed_t px1, py1, px2, py2;
	angle_t angle1, angle2;

	// Find the corners of the box
	// that define the edges from current viewpoint.
	if (viewx <= bspcoord[BOXLEFT])
		boxpos = 0;
	else if (viewx < bspcoord[BOXRIGHT])
		boxpos = 1;
	else
		boxpos = 2;

	if (viewy >= bspcoord[BOXTOP])
		boxpos |= 0;
	else if (viewy > bspcoord[BOXBOTTOM])
		boxpos |= 4;
	else
		boxpos |= 8;

	if (boxpos == 5)
		return true;

	px1 = bspcoord[checkcoord[boxpos][0]];
	py1 = bspcoord[checkcoord[boxpos][1]];
	px2 = bspcoord[checkcoord[boxpos][2]];
	py2 = bspcoord[checkcoord[boxpos][3]];

	if (current_bsp_culling_distance)
	{
		//fixed_t midx = (px1 >> 1) + (px2 >> 1);
		//fixed_t midy = (py1 >> 1) + (py2 >> 1);
		//fixed_t mindist = min(min(R_PointToDist(px1, py1), R_PointToDist(px2, py2)), R_PointToDist(midx, midy));

		//fixed_t mindist = ClosestPointOnLineDistance(px1, py1, px2, py2);

		//fixed_t mindist1 = ClosestPointOnLineDistance(bspcoord[BOXLEFT], bspcoord[BOXTOP], bspcoord[BOXRIGHT], bspcoord[BOXTOP]); // top line
		//fixed_t mindist2 = ClosestPointOnLineDistance(bspcoord[BOXLEFT], bspcoord[BOXTOP], bspcoord[BOXLEFT], bspcoord[BOXBOTTOM]); // left line
		//fixed_t mindist3 = ClosestPointOnLineDistance(bspcoord[BOXLEFT], bspcoord[BOXBOTTOM], bspcoord[BOXRIGHT], bspcoord[BOXBOTTOM]); // bottom line
		//fixed_t mindist4 = ClosestPointOnLineDistance(bspcoord[BOXRIGHT], bspcoord[BOXTOP], bspcoord[BOXRIGHT], bspcoord[BOXBOTTOM]); // right line
		// this one seems too lax.. maybe closestpointonlinedistance is glitchy and returns points that are not on the line segment?
		// could try building an if-else structure that determines what point or line is closest
		// 1  | 2  | 3
		//--------------
		// 4  |node| 5
		//--------------
		// 6  | 7  | 8
		// y
		// ^
		// |
		// -----> x
		// inside node: always inside draw distance. the above boxpos thing might have returned true already?
		// 1. check top left corner     2. check top line     3. check top right corner
		// 4. check left line                                 5. check right line
		// 6. check bottom left corner  7. check bottom line  8. check bottom right corner
		// one if statement will split the space in two for one coordinate
		// for example:
		// x < BOXLEFT   || BOXLEFT ||   !(x < BOXLEFT)   <-- (same as x >= BOXLEFT)
		fixed_t mindist;// = min(min(mindist1, mindist2), min(mindist3, mindist4));

		// new thing
		// calculate distance to axis aligned bounding box.
		if (viewx < bspcoord[BOXLEFT]) // 1,4,6
		{
			if (viewy > bspcoord[BOXTOP]) // 1
				mindist = R_PointToDist(bspcoord[BOXLEFT], bspcoord[BOXTOP]);
			else if (viewy < bspcoord[BOXBOTTOM]) // 6
				mindist = R_PointToDist(bspcoord[BOXLEFT], bspcoord[BOXBOTTOM]);
			else // 4
				mindist = bspcoord[BOXLEFT] - viewx;
		}
		else if (viewx > bspcoord[BOXRIGHT]) // 3,5,8
		{
			if (viewy > bspcoord[BOXTOP]) // 3
				mindist = R_PointToDist(bspcoord[BOXRIGHT], bspcoord[BOXTOP]);
			else if (viewy < bspcoord[BOXBOTTOM]) // 8
				mindist = R_PointToDist(bspcoord[BOXRIGHT], bspcoord[BOXBOTTOM]);
			else // 5
				mindist = viewx - bspcoord[BOXRIGHT];
		}
		else // 2,node,7
		{
			if (viewy > bspcoord[BOXTOP]) // 2
				mindist = viewy - bspcoord[BOXTOP];
			else if (viewy < bspcoord[BOXBOTTOM]) // 7
				mindist = bspcoord[BOXBOTTOM] - viewy;
			else // node
				mindist = 0;
		}
		if (mindist > current_bsp_culling_distance) return false;
	}

	if (cv_pointoangleexor64.value)
	{
		angle1 = R_PointToAngle64(px1, py1);
		angle2 = R_PointToAngle64(px2, py2);
	}
	else
	{
		angle1 = R_PointToAngleEx(viewx, viewy, px1, py1);
		angle2 = R_PointToAngleEx(viewx, viewy, px2, py2);
	}

	return gld_clipper_SafeCheckRange(angle2, angle1);
}


//
// HWR_AddPolyObjectSegs
//
// haleyjd 02/19/06
// Adds all segs in all polyobjects in the given subsector.
// Modified for hardware rendering.
//
static inline void HWR_AddPolyObjectSegs(void)
{
	size_t i, j;
	seg_t gr_fakeline;
	polyvertex_t pv1;
	polyvertex_t pv2;

	// Sort through all the polyobjects
	for (i = 0; i < numpolys; ++i)
	{
		// Render the polyobject's lines
		for (j = 0; j < po_ptrs[i]->segCount; ++j)
		{
			// Copy the info of a polyobject's seg, then convert it to OpenGL floating point
			M_Memcpy(&gr_fakeline, po_ptrs[i]->segs[j], sizeof(seg_t));

			// Now convert the line to float and add it to be rendered
			pv1.x = FIXED_TO_FLOAT(gr_fakeline.v1->x);
			pv1.y = FIXED_TO_FLOAT(gr_fakeline.v1->y);
			pv2.x = FIXED_TO_FLOAT(gr_fakeline.v2->x);
			pv2.y = FIXED_TO_FLOAT(gr_fakeline.v2->y);

			gr_fakeline.pv1 = &pv1;
			gr_fakeline.pv2 = &pv2;

			HWR_AddLine(&gr_fakeline);
		}
	}
}

void HWR_RenderPolyObjectPlane(polyobj_t *polysector, boolean isceiling, fixed_t fixedheight, FBITFIELD blendmode, UINT8 lightlevel, lumpnum_t lumpnum, sector_t *FOFsector, UINT8 alpha, extracolormap_t *planecolormap)
{
	float           height; //constant y for all points on the convex flat polygon
	FOutVector      *v3d;
	INT32             i;
	float           flatxref,flatyref;
	float fflatsize;
	INT32 flatflag;
	size_t len;
	float scrollx = 0.0f, scrolly = 0.0f;
	angle_t angle = 0;
	FSurfaceInfo    Surf;
	fixed_t tempxsow, tempytow;
	size_t nrPlaneVerts;

	static FOutVector *planeVerts = NULL;
	static UINT16 numAllocedPlaneVerts = 0;
	
	INT32 shader = SHADER_NONE;

	nrPlaneVerts = polysector->numVertices;

	height = FIXED_TO_FLOAT(fixedheight);

	if (nrPlaneVerts < 3)   //not even a triangle ?
		return;

	if (nrPlaneVerts > INT16_MAX) // FIXME: exceeds plVerts size
	{
		CONS_Debug(DBG_RENDER, "polygon size of %s exceeds max value of %d vertices\n", sizeu1(nrPlaneVerts), UINT16_MAX);
		return;
	}

	// Allocate plane-vertex buffer if we need to
	if (!planeVerts || nrPlaneVerts > numAllocedPlaneVerts)
	{
		numAllocedPlaneVerts = (UINT16)nrPlaneVerts;
		Z_Free(planeVerts);
		Z_Malloc(numAllocedPlaneVerts * sizeof (FOutVector), PU_LEVEL, &planeVerts);
	}

	len = W_LumpLength(lumpnum);

	switch (len)
	{
		case 4194304: // 2048x2048 lump
			fflatsize = 2048.0f;
			flatflag = 2047;
			break;
		case 1048576: // 1024x1024 lump
			fflatsize = 1024.0f;
			flatflag = 1023;
			break;
		case 262144:// 512x512 lump
			fflatsize = 512.0f;
			flatflag = 511;
			break;
		case 65536: // 256x256 lump
			fflatsize = 256.0f;
			flatflag = 255;
			break;
		case 16384: // 128x128 lump
			fflatsize = 128.0f;
			flatflag = 127;
			break;
		case 1024: // 32x32 lump
			fflatsize = 32.0f;
			flatflag = 31;
			break;
		default: // 64x64 lump
			fflatsize = 64.0f;
			flatflag = 63;
			break;
	}

	// reference point for flat texture coord for each vertex around the polygon
	flatxref = FIXED_TO_FLOAT(polysector->origVerts[0].x);
	flatyref = FIXED_TO_FLOAT(polysector->origVerts[0].y);

	flatxref = (float)(((fixed_t)flatxref & (~flatflag)) / fflatsize);
	flatyref = (float)(((fixed_t)flatyref & (~flatflag)) / fflatsize);

	// transform
	v3d = planeVerts;

	if (FOFsector != NULL)
	{
		if (!isceiling) // it's a floor
		{
			scrollx = FIXED_TO_FLOAT(FOFsector->floor_xoffs)/fflatsize;
			scrolly = FIXED_TO_FLOAT(FOFsector->floor_yoffs)/fflatsize;
			angle = FOFsector->floorpic_angle>>ANGLETOFINESHIFT;
		}
		else // it's a ceiling
		{
			scrollx = FIXED_TO_FLOAT(FOFsector->ceiling_xoffs)/fflatsize;
			scrolly = FIXED_TO_FLOAT(FOFsector->ceiling_yoffs)/fflatsize;
			angle = FOFsector->ceilingpic_angle>>ANGLETOFINESHIFT;
		}
	}
	else if (gr_frontsector)
	{
		if (!isceiling) // it's a floor
		{
			scrollx = FIXED_TO_FLOAT(gr_frontsector->floor_xoffs)/fflatsize;
			scrolly = FIXED_TO_FLOAT(gr_frontsector->floor_yoffs)/fflatsize;
			angle = gr_frontsector->floorpic_angle>>ANGLETOFINESHIFT;
		}
		else // it's a ceiling
		{
			scrollx = FIXED_TO_FLOAT(gr_frontsector->ceiling_xoffs)/fflatsize;
			scrolly = FIXED_TO_FLOAT(gr_frontsector->ceiling_yoffs)/fflatsize;
			angle = gr_frontsector->ceilingpic_angle>>ANGLETOFINESHIFT;
		}
	}

	if (angle) // Only needs to be done if there's an altered angle
	{
		// This needs to be done so that it scrolls in a different direction after rotation like software
		tempxsow = FLOAT_TO_FIXED(scrollx);
		tempytow = FLOAT_TO_FIXED(scrolly);
		scrollx = (FIXED_TO_FLOAT(FixedMul(tempxsow, FINECOSINE(angle)) - FixedMul(tempytow, FINESINE(angle))));
		scrolly = (FIXED_TO_FLOAT(FixedMul(tempxsow, FINESINE(angle)) + FixedMul(tempytow, FINECOSINE(angle))));

		// This needs to be done so everything aligns after rotation
		// It would be done so that rotation is done, THEN the translation, but I couldn't get it to rotate AND scroll like software does
		tempxsow = FLOAT_TO_FIXED(flatxref);
		tempytow = FLOAT_TO_FIXED(flatyref);
		flatxref = (FIXED_TO_FLOAT(FixedMul(tempxsow, FINECOSINE(angle)) - FixedMul(tempytow, FINESINE(angle))));
		flatyref = (FIXED_TO_FLOAT(FixedMul(tempxsow, FINESINE(angle)) + FixedMul(tempytow, FINECOSINE(angle))));
	}

	for (i = 0; i < (INT32)nrPlaneVerts; i++,v3d++)
	{
		// Hurdler: add scrolling texture on floor/ceiling
		v3d->s = (float)((FIXED_TO_FLOAT(polysector->origVerts[i].x) / fflatsize) - flatxref + scrollx); // Go from the polysector's original vertex locations
		v3d->t = (float)(flatyref - (FIXED_TO_FLOAT(polysector->origVerts[i].y) / fflatsize) + scrolly); // Means the flat is offset based on the original vertex locations

		// Need to rotate before translate
		if (angle) // Only needs to be done if there's an altered angle
		{
			tempxsow = FLOAT_TO_FIXED(v3d->s);
			tempytow = FLOAT_TO_FIXED(v3d->t);
			v3d->s = (FIXED_TO_FLOAT(FixedMul(tempxsow, FINECOSINE(angle)) - FixedMul(tempytow, FINESINE(angle))));
			v3d->t = (FIXED_TO_FLOAT(-FixedMul(tempxsow, FINESINE(angle)) - FixedMul(tempytow, FINECOSINE(angle))));
		}

		v3d->x = FIXED_TO_FLOAT(polysector->vertices[i]->x);
		v3d->y = height;
		v3d->z = FIXED_TO_FLOAT(polysector->vertices[i]->y);
	}

	HWR_Lighting(&Surf, lightlevel, planecolormap);

	if (blendmode & PF_Translucent)
	{
		Surf.PolyColor.s.alpha = (UINT8)alpha;
		blendmode |= PF_Modulated|PF_Occlude;
	}
	else
		blendmode |= PF_Masked|PF_Modulated;

	if (HWR_UseShader())
	{		
		shader = SHADER_FLOOR;	// floor shader
		blendmode |= PF_ColorMapped;
	}
		
	HWR_ProcessPolygon(&Surf, planeVerts, nrPlaneVerts, blendmode, shader, false); // floor shader
}

void HWR_AddPolyObjectPlanes(void)
{
	size_t i;
	sector_t *polyobjsector;

	// Polyobject Planes need their own function for drawing because they don't have extrasubsectors by themselves
	// It should be okay because polyobjects should always be convex anyway

	for (i  = 0; i < numpolys; i++)
	{
		polyobjsector = po_ptrs[i]->lines[0]->backsector; // the in-level polyobject sector

		if (!(po_ptrs[i]->flags & POF_RENDERPLANES)) // Only render planes when you should
			continue;

		if (po_ptrs[i]->translucency >= NUMTRANSMAPS)
			continue;

		if (polyobjsector->floorheight <= gr_frontsector->ceilingheight
			&& polyobjsector->floorheight >= gr_frontsector->floorheight
			&& (viewz < polyobjsector->floorheight))
		{
			if (po_ptrs[i]->translucency > 0)
			{
				FSurfaceInfo Surf;
				FBITFIELD blendmode = HWR_TranstableToAlpha(po_ptrs[i]->translucency, &Surf);
				HWR_AddTransparentPolyobjectFloor(levelflats[polyobjsector->floorpic].lumpnum, po_ptrs[i], false, polyobjsector->floorheight,
													SOFTLIGHT(polyobjsector->lightlevel), Surf.PolyColor.s.alpha, polyobjsector, blendmode, NULL);
			}
			else
			{
				HWR_GetFlat(levelflats[polyobjsector->floorpic].lumpnum, R_NoEncore(polyobjsector, false));
				HWR_RenderPolyObjectPlane(po_ptrs[i], false, polyobjsector->floorheight, PF_Occlude,
										SOFTLIGHT(polyobjsector->lightlevel), levelflats[polyobjsector->floorpic].lumpnum,
										polyobjsector, 255, NULL);
			}
		}

		if (polyobjsector->ceilingheight >= gr_frontsector->floorheight
			&& polyobjsector->ceilingheight <= gr_frontsector->ceilingheight
			&& (viewz > polyobjsector->ceilingheight))
		{
			if (po_ptrs[i]->translucency > 0)
			{
				FSurfaceInfo Surf;
				FBITFIELD blendmode;
				memset(&Surf, 0x00, sizeof(Surf));
				blendmode = HWR_TranstableToAlpha(po_ptrs[i]->translucency, &Surf);
				HWR_AddTransparentPolyobjectFloor(levelflats[polyobjsector->ceilingpic].lumpnum, po_ptrs[i], true, polyobjsector->ceilingheight,
				                                  SOFTLIGHT(polyobjsector->lightlevel), Surf.PolyColor.s.alpha, polyobjsector, blendmode, NULL);
			}
			else
			{
				HWR_GetFlat(levelflats[polyobjsector->ceilingpic].lumpnum, R_NoEncore(polyobjsector, true));
				HWR_RenderPolyObjectPlane(po_ptrs[i], true, polyobjsector->ceilingheight, PF_Occlude,
				                          SOFTLIGHT(polyobjsector->lightlevel), levelflats[polyobjsector->ceilingpic].lumpnum,
				                          polyobjsector, 255, NULL);
			}
		}
	}
}


//
// HWR_DoCulling
// Hardware version of R_DoCulling
// (see r_main.c)
static boolean HWR_DoCulling(line_t *cullheight, line_t *viewcullheight, float vz, float bottomh, float toph)
{
	float cullplane;

	if (!cullheight)
		return false;

	cullplane = FIXED_TO_FLOAT(cullheight->frontsector->floorheight);
	if (cullheight->flags & ML_NOCLIMB) // Group culling
	{
		if (!viewcullheight)
			return false;

		// Make sure this is part of the same group
		if (viewcullheight->frontsector == cullheight->frontsector)
		{
			// OK, we can cull
			if (vz > cullplane && toph < cullplane) // Cull if below plane
				return true;

			if (bottomh > cullplane && vz <= cullplane) // Cull if above plane
				return true;
		}
	}
	else // Quick culling
	{
		if (vz > cullplane && toph < cullplane) // Cull if below plane
			return true;

		if (bottomh > cullplane && vz <= cullplane) // Cull if above plane
			return true;
	}

	return false;
}

// -----------------+
// HWR_Subsector    : Determine floor/ceiling planes.
//                  : Add sprites of things in sector.
//                  : Draw one or more line segments.
// -----------------+
static void HWR_Subsector(size_t num)
{
	INT16 count;
	seg_t *line;
	subsector_t *sub;
	static sector_t tempsec; //SoM: 4/7/2000
	INT32 floorlightlevel;
	INT32 ceilinglightlevel;
	INT32 locFloorHeight, locCeilingHeight;
	INT32 cullFloorHeight, cullCeilingHeight;
	INT32 light = 0;
	extracolormap_t *floorcolormap;
	extracolormap_t *ceilingcolormap;
	ffloor_t *rover;
	boolean skipSprites = false;

#ifdef PARANOIA //no risk while developing, enough debugging nights!
	if (num >= addsubsector)
		I_Error("HWR_Subsector: ss %s with numss = %s, addss = %s\n",
			sizeu1(num), sizeu2(numsubsectors), sizeu3(addsubsector));
#endif

	if (num < numsubsectors)
	{
		// subsector
		sub = &subsectors[num];
		// sector
		gr_frontsector = sub->sector;
		// how many linedefs
		count = sub->numlines;
		// first line seg
		line = &segs[sub->firstline];
	}
	else
	{
		// there are no segs but only planes
		sub = &subsectors[0];
		gr_frontsector = sub->sector;
		count = 0;
		line = NULL;
	}

	//SoM: 4/7/2000: Test to make Boom water work in Hardware mode.
	gr_frontsector = R_FakeFlat(gr_frontsector, &tempsec, &floorlightlevel,
								&ceilinglightlevel, false);
	//FIXME: Use floorlightlevel and ceilinglightlevel insted of lightlevel.

	if (gr_portal == GRPORTAL_SEARCH)
	{
		skipSprites = true;
		goto skip_stuff_for_portals;// hopefully this goto is okay
	}

	floorcolormap = ceilingcolormap = gr_frontsector->extra_colormap;

	cullFloorHeight   = P_GetSectorFloorZAt  (gr_frontsector, viewx, viewy);
	cullCeilingHeight = P_GetSectorCeilingZAt(gr_frontsector, viewx, viewy);
	locFloorHeight    = P_GetSectorFloorZAt  (gr_frontsector, gr_frontsector->soundorg.x, gr_frontsector->soundorg.y);
	locCeilingHeight  = P_GetSectorCeilingZAt(gr_frontsector, gr_frontsector->soundorg.x, gr_frontsector->soundorg.y);

	if (gr_frontsector->ffloors)
	{
		boolean anyMoved = gr_frontsector->moved;

		if (anyMoved == false)
		{
			for (rover = gr_frontsector->ffloors; rover; rover = rover->next)
			{
				sector_t *controlSec = &sectors[rover->secnum];

				if (controlSec->moved == true)
				{
					anyMoved = true;
					break;
				}
			}
		}

		if (anyMoved == true)
		{
			gr_frontsector->numlights = sub->sector->numlights = 0;
			R_Prep3DFloors(gr_frontsector);
			sub->sector->lightlist = gr_frontsector->lightlist;
			sub->sector->numlights = gr_frontsector->numlights;
			sub->sector->moved = gr_frontsector->moved = false;
		}

		light = R_GetPlaneLight(gr_frontsector, locFloorHeight, false);
		if (gr_frontsector->floorlightsec == -1)
			floorlightlevel = SOFTLIGHT(*gr_frontsector->lightlist[light].lightlevel);
		floorcolormap = gr_frontsector->lightlist[light].extra_colormap;

		light = R_GetPlaneLight(gr_frontsector, locCeilingHeight, false);
		if (gr_frontsector->ceilinglightsec == -1)
			ceilinglightlevel = SOFTLIGHT(*gr_frontsector->lightlist[light].lightlevel);
		ceilingcolormap = gr_frontsector->lightlist[light].extra_colormap;
	}

	sub->sector->extra_colormap = gr_frontsector->extra_colormap;

	// render floor ?
	// yeah, easy backface cull! :)
	if (cullFloorHeight < viewz)
	{
		if (gr_frontsector->floorpic != skyflatnum)
		{
			if (sub->validcount != validcount)
			{
				HWR_GetFlat(levelflats[gr_frontsector->floorpic].lumpnum, R_NoEncore(gr_frontsector, false));
				HWR_RenderPlane(sub, &extrasubsectors[num], false,
								locFloorHeight == cullFloorHeight ? locFloorHeight : gr_frontsector->floorheight,
								PF_Occlude, floorlightlevel, levelflats[gr_frontsector->floorpic].lumpnum, NULL, 255, floorcolormap);
			}
		}
	}

	if (cullCeilingHeight > viewz)
	{
		if (gr_frontsector->ceilingpic != skyflatnum)
		{
			if (sub->validcount != validcount)
			{
				HWR_GetFlat(levelflats[gr_frontsector->ceilingpic].lumpnum, R_NoEncore(gr_frontsector, true));
				HWR_RenderPlane(sub, &extrasubsectors[num], true,
					// Hack to make things continue to work around slopes.
					locCeilingHeight == cullCeilingHeight ? locCeilingHeight : gr_frontsector->ceilingheight,
					// We now return you to your regularly scheduled rendering.
					PF_Occlude, ceilinglightlevel, levelflats[gr_frontsector->ceilingpic].lumpnum,NULL, 255, ceilingcolormap);
			}
		}
	}

	if (gr_frontsector->ffloors)
	{
		/// \todo fix light, xoffs, yoffs, extracolormap ?
		for (rover = gr_frontsector->ffloors;
			rover; rover = rover->next)
		{
			fixed_t bottomCullHeight, topCullHeight, centerHeight;

			if (!(rover->flags & FF_EXISTS) || !(rover->flags & FF_RENDERPLANES))
				continue;
			if (sub->validcount == validcount)
				continue;
			
			// rendering heights for bottom and top planes
			// yes there were functions for this stuff, no idea why it wasnt used but bleh
			bottomCullHeight = P_GetFFloorBottomZAt(rover, viewx, viewy);
			topCullHeight = P_GetFFloorTopZAt(rover, viewx, viewy);

			if (gr_frontsector->cullheight)
			{
				if (HWR_DoCulling(gr_frontsector->cullheight, viewsector->cullheight, gr_viewz, FIXED_TO_FLOAT(*rover->bottomheight), FIXED_TO_FLOAT(*rover->topheight)))
					continue;
			}

			// bottom plane
			centerHeight = P_GetFFloorBottomZAt(rover, gr_frontsector->soundorg.x, gr_frontsector->soundorg.y);

			if (centerHeight <= locCeilingHeight && centerHeight >= locFloorHeight &&
			    ((viewz < bottomCullHeight && (rover->flags & FF_BOTHPLANES || !(rover->flags & FF_INVERTPLANES))) ||
			     (viewz > bottomCullHeight && (rover->flags & FF_BOTHPLANES || rover->flags & FF_INVERTPLANES))))
			{
				if (rover->flags & FF_FOG)
				{
					UINT8 alpha;

					light = R_GetPlaneLight(gr_frontsector, centerHeight, viewz < bottomCullHeight  ? true : false);

					alpha = HWR_FogBlockAlpha(SOFTLIGHT(*gr_frontsector->lightlist[light].lightlevel), rover->master->frontsector->extra_colormap);

					HWR_AddTransparentFloor(0,
					                       &extrasubsectors[num],
										   false,
					                       *rover->bottomheight,
					                       SOFTLIGHT(*gr_frontsector->lightlist[light].lightlevel),
					                       alpha, rover->master->frontsector, PF_Fog|PF_NoTexture,
										   true, rover->master->frontsector->extra_colormap);
				}
				else if (rover->flags & FF_TRANSLUCENT && rover->alpha < 256) // SoM: Flags are more efficient
				{
					light = R_GetPlaneLight(gr_frontsector, centerHeight, viewz < bottomCullHeight  ? true : false);
					HWR_AddTransparentFloor(levelflats[*rover->bottompic].lumpnum,
					                       &extrasubsectors[num],
										   false,
					                       *rover->bottomheight,
					                       SOFTLIGHT(*gr_frontsector->lightlist[light].lightlevel),
					                       max(0, min(rover->alpha, 255)), rover->master->frontsector, (rover->flags & FF_RIPPLE ? PF_Ripple : 0)|PF_Translucent,
					                       false, gr_frontsector->lightlist[light].extra_colormap);
				}
				else
				{
					HWR_GetFlat(levelflats[*rover->bottompic].lumpnum, R_NoEncore(gr_frontsector, false));
					light = R_GetPlaneLight(gr_frontsector, centerHeight, viewz < bottomCullHeight  ? true : false);
					HWR_RenderPlane(sub, &extrasubsectors[num], false, *rover->bottomheight, (rover->flags & FF_RIPPLE ? PF_Ripple : 0)|PF_Occlude, SOFTLIGHT(*gr_frontsector->lightlist[light].lightlevel), levelflats[*rover->bottompic].lumpnum,
					                rover->master->frontsector, 255, gr_frontsector->lightlist[light].extra_colormap);
				}
			}

			// top plane
			centerHeight = P_GetFFloorTopZAt(rover, gr_frontsector->soundorg.x, gr_frontsector->soundorg.y);

			if (centerHeight >= locFloorHeight &&
			    centerHeight <= locCeilingHeight &&
			    ((viewz > topCullHeight && (rover->flags & FF_BOTHPLANES || !(rover->flags & FF_INVERTPLANES))) ||
			     (viewz < topCullHeight && (rover->flags & FF_BOTHPLANES || rover->flags & FF_INVERTPLANES))))
			{
				if (rover->flags & FF_FOG)
				{
					UINT8 alpha;

					light = R_GetPlaneLight(gr_frontsector, centerHeight, viewz < topCullHeight  ? true : false);

					alpha = HWR_FogBlockAlpha(SOFTLIGHT(*gr_frontsector->lightlist[light].lightlevel), rover->master->frontsector->extra_colormap);

					HWR_AddTransparentFloor(0,
					                       &extrasubsectors[num],
										   true,
					                       *rover->topheight,
					                       SOFTLIGHT(*gr_frontsector->lightlist[light].lightlevel),
					                       alpha, rover->master->frontsector, PF_Fog|PF_NoTexture,
										   true, rover->master->frontsector->extra_colormap);
				}
				else if (rover->flags & FF_TRANSLUCENT && rover->alpha < 256)
				{
					light = R_GetPlaneLight(gr_frontsector, centerHeight, viewz < topCullHeight  ? true : false);
					HWR_AddTransparentFloor(levelflats[*rover->toppic].lumpnum,
											&extrasubsectors[num],
											true,
											*rover->topheight,
											SOFTLIGHT(*gr_frontsector->lightlist[light].lightlevel),
											max(0, min(rover->alpha, 255)), rover->master->frontsector, (rover->flags & FF_RIPPLE ? PF_Ripple : 0)|PF_Translucent,
											false, gr_frontsector->lightlist[light].extra_colormap);
				}
				else
				{
					HWR_GetFlat(levelflats[*rover->toppic].lumpnum, R_NoEncore(gr_frontsector, true));
					light = R_GetPlaneLight(gr_frontsector, centerHeight, viewz < topCullHeight  ? true : false);
					HWR_RenderPlane(sub, &extrasubsectors[num], true, *rover->topheight, (rover->flags & FF_RIPPLE ? PF_Ripple : 0)|PF_Occlude, SOFTLIGHT(*gr_frontsector->lightlist[light].lightlevel), levelflats[*rover->toppic].lumpnum,
									  rover->master->frontsector, 255, gr_frontsector->lightlist[light].extra_colormap);
				}
			}
		}
	}

	// Draw all the polyobjects in this subsector
	if (sub->polyList)
	{
		polyobj_t *po = sub->polyList;

		numpolys = 0;

		// Count all the polyobjects, reset the list, and recount them
		while (po)
		{
			++numpolys;
			po = (polyobj_t *)(po->link.next);
		}

		// for performance stats
		ps_numpolyobjects.value.i += numpolys;

		// Sort polyobjects
		R_SortPolyObjects(sub);

		// Draw polyobject lines.
		HWR_AddPolyObjectSegs();

		if (sub->validcount != validcount) // This validcount situation seems to let us know that the floors have already been drawn.
		{
			// Draw polyobject planes
			HWR_AddPolyObjectPlanes();
		}
	}

skip_stuff_for_portals:

// Hurder ici se passe les choses INT32�essantes!
// on vient de tracer le sol et le plafond
// on trace �pr�ent d'abord les sprites et ensuite les murs
// hurdler: faux: on ajoute seulement les sprites, le murs sont trac� d'abord
	if (line)
	{
		// draw sprites first, coz they are clipped to the solidsegs of
		// subsectors more 'in front'
		if (!skipSprites)
			HWR_AddSprites(gr_frontsector);

		//Hurdler: at this point validcount must be the same, but is not because
		//         gr_frontsector doesn't point anymore to sub->sector due to
		//         the call gr_frontsector = R_FakeFlat(...)
		//         if it's not done, the sprite is drawn more than once,
		//         what looks really bad with translucency or dynamic light,
		//         without talking about the overdraw of course.
		sub->sector->validcount = validcount;/// \todo fix that in a better way

		while (count--)
		{
				if (!line->polyseg) // ignore segs that belong to polyobjects
				HWR_AddLine(line);
				line++;
		}
	}

	sub->validcount = validcount;
}

// idea for fixing fakery map: one portal pillar works, 2 pillars have left/right bug wall, 1 pillar has both sides bugged.
// bounding box is probably right on the edge, maybe could check for this with P_ClosestPointOnLine
// so: for each side check, if it passes then also check distance to line,
// if its zero (or very close?) then dont return true, instead continue to next side check
// it helped with center pillars! but other parts still have issues, probably because some of bounding box is on correct side.


// idea for further clipping improvement:
// have a separate xyz coordinate for portal view side checking: one that is derived by moving the viewxyz forward
// the new coords would be at the intersection of line_a and line_b, where
// line_a = line of view, pointing forward from the center of the camera
// line_b = a line orthogonal to line_a, defined so that the nearest vertex of portalclipline lies within it
// maybe if the seg to be drawn has these new coords on one side and the normal viewxyz on the other side then it can be culled?

// looks like P_Thrust in p_user.c has code for moving point forward towards a direction
// maybe P_InterceptVector could be used for intersect point
// use returned value as multiplier for the added values from p_thrust thing
// P_InterceptVector needs divlines which need dx and dy, dx=x2-x1 dy=y2-y1

static boolean HWR_PortalCheckBBox(const fixed_t *bspcoord)
{
	vertex_t closest_point;
	if (!portalclipline)
		return true;
	
	// we are looking for a bounding box corner that is on the viewable side of the portal exit.
	// being exactly on the portal exit line is not enough to pass the test.
	// P_PointOnLineSide could behave differently from this expectation on this case,
	// so first check if the point is precisely on the line, and then if not, check the side.

	P_ClosestPointOnLine(bspcoord[BOXLEFT], bspcoord[BOXTOP], portalclipline, &closest_point);
	if (closest_point.x != bspcoord[BOXLEFT] || closest_point.y != bspcoord[BOXTOP])
	{
		if (P_PointOnLineSide(bspcoord[BOXLEFT], bspcoord[BOXTOP], portalclipline) != portalviewside)
			return true;
	}
	P_ClosestPointOnLine(bspcoord[BOXLEFT], bspcoord[BOXBOTTOM], portalclipline, &closest_point);
	if (closest_point.x != bspcoord[BOXLEFT] || closest_point.y != bspcoord[BOXBOTTOM])
	{
		if (P_PointOnLineSide(bspcoord[BOXLEFT], bspcoord[BOXBOTTOM], portalclipline) != portalviewside)
			return true;
	}
	P_ClosestPointOnLine(bspcoord[BOXRIGHT], bspcoord[BOXTOP], portalclipline, &closest_point);
	if (closest_point.x != bspcoord[BOXRIGHT] || closest_point.y != bspcoord[BOXTOP])
	{
		if (P_PointOnLineSide(bspcoord[BOXRIGHT], bspcoord[BOXTOP], portalclipline) != portalviewside)
			return true;
	}
	P_ClosestPointOnLine(bspcoord[BOXRIGHT], bspcoord[BOXBOTTOM], portalclipline, &closest_point);
	if (closest_point.x != bspcoord[BOXRIGHT] || closest_point.y != bspcoord[BOXBOTTOM])
	{
		if (P_PointOnLineSide(bspcoord[BOXRIGHT], bspcoord[BOXBOTTOM], portalclipline) != portalviewside)
			return true;
	}

	// we did not find any reason to pass the check, so return failure
	return false;
}

//
// Renders all subsectors below a given node,
//  traversing subtree recursively.
// Just call with BSP root.

static void HWR_RenderBSPNode(INT32 bspnum)
{
	ps_numbspcalls.value.i++;

	while (!(bspnum & NF_SUBSECTOR))  // Found a subsector?
	{
		const node_t *bsp = &nodes[bspnum];

		// Decide which side the view point is on.
		INT32 side = R_PointOnSide(viewx, viewy, bsp);

		// Recursively divide front space.
		if (HWR_PortalCheckBBox(bsp->bbox[side]))
			HWR_RenderBSPNode(bsp->children[side]);

        // Possibly divide back space
        if (!(HWR_CheckBBox(bsp->bbox[side^1]) && HWR_PortalCheckBBox(bsp->bbox[side^1])))
            return;

		bspnum = bsp->children[side^1];
	}

	// PORTAL CULLING
	if (portalclipline && portalcullsector)
	{
		if (portalcullsector != subsectors[bspnum & ~NF_SUBSECTOR].sector)
			return;
		portalcullsector = NULL;
	}
	// e6y: support for extended nodes
	HWR_Subsector(bspnum == -1 ? 0 : bspnum & ~NF_SUBSECTOR);
}

// ==========================================================================
// gr_things.c
// ==========================================================================

// sprites are drawn after all wall and planes are rendered, so that
// sprite translucency effects apply on the rendered view (instead of the background sky!!)

static UINT32 gr_visspritecount;
static gr_vissprite_t *gr_visspritechunks[MAXVISSPRITES >> VISSPRITECHUNKBITS] = {NULL};

// --------------------------------------------------------------------------
// HWR_ClearSprites
// Called at frame start.
// --------------------------------------------------------------------------
static void HWR_ClearSprites(void)
{
	gr_visspritecount = 0;
}

// --------------------------------------------------------------------------
// HWR_NewVisSprite
// --------------------------------------------------------------------------
static gr_vissprite_t gr_overflowsprite;

static gr_vissprite_t *HWR_GetVisSprite(UINT32 num)
{
		UINT32 chunk = num >> VISSPRITECHUNKBITS;

		// Allocate chunk if necessary
		if (!gr_visspritechunks[chunk])
			Z_Malloc(sizeof(gr_vissprite_t) * VISSPRITESPERCHUNK, PU_LEVEL, &gr_visspritechunks[chunk]);

		return gr_visspritechunks[chunk] + (num & VISSPRITEINDEXMASK);
}

static gr_vissprite_t *HWR_NewVisSprite(void)
{
	if (gr_visspritecount == MAXVISSPRITES)
		return &gr_overflowsprite;

	return HWR_GetVisSprite(gr_visspritecount++);
}

// Finds a floor through which light does not pass.
static fixed_t HWR_OpaqueFloorAtPos(fixed_t x, fixed_t y, fixed_t z, fixed_t height)
{
	const sector_t *sec = R_PointInSubsector(x, y)->sector;
	fixed_t floorz = sec->floorheight;

	if (sec->ffloors)
	{
		ffloor_t *rover;
		fixed_t delta1, delta2;
		const fixed_t thingtop = z + height;

		for (rover = sec->ffloors; rover; rover = rover->next)
		{
			if (!(rover->flags & FF_EXISTS)
			|| !(rover->flags & FF_RENDERPLANES)
			|| rover->flags & FF_TRANSLUCENT
			|| rover->flags & FF_FOG
			|| rover->flags & FF_INVERTPLANES)
				continue;

			delta1 = z - (*rover->bottomheight + ((*rover->topheight - *rover->bottomheight)/2));
			delta2 = thingtop - (*rover->bottomheight + ((*rover->topheight - *rover->bottomheight)/2));
			if (*rover->topheight > floorz && abs(delta1) < abs(delta2))
				floorz = *rover->topheight;
		}
	}

	return floorz;
}

static void HWR_DrawSpriteShadow(gr_vissprite_t *spr, GLPatch_t *gpatch, float this_scale)
{
	FOutVector swallVerts[4];
	FSurfaceInfo sSurf;
	fixed_t floorheight, mobjfloor;
	pslope_t *floorslope;
	fixed_t slopez;
	float offset = 0;
	
	INT32 shader = SHADER_NONE;
	
	R_GetShadowZ(spr->mobj, &floorslope);

	mobjfloor = HWR_OpaqueFloorAtPos(
		spr->mobj->x, spr->mobj->y,
		spr->mobj->z, spr->mobj->height);

	if (cv_shadowoffs.value)
	{
		angle_t shadowdir;

		// Set direction
		if (splitscreen && stplyr == &players[displayplayers[1]])
			shadowdir = localangle[1] + FixedAngle(cv_cam2_rotate.value);
		else if (splitscreen > 1 && stplyr == &players[displayplayers[2]])
			shadowdir = localangle[2] + FixedAngle(cv_cam3_rotate.value);
		else if (splitscreen > 2 && stplyr == &players[displayplayers[3]])
			shadowdir = localangle[3] + FixedAngle(cv_cam4_rotate.value);
		else
			shadowdir = localangle[0] + FixedAngle(cv_cam_rotate.value);

		// Find floorheight
		floorheight = HWR_OpaqueFloorAtPos(
			spr->mobj->x + P_ReturnThrustX(spr->mobj, shadowdir, spr->mobj->z - mobjfloor),
			spr->mobj->y + P_ReturnThrustY(spr->mobj, shadowdir, spr->mobj->z - mobjfloor),
			spr->mobj->z, spr->mobj->height);

		// The shadow is falling ABOVE it's mobj?
		// Don't draw it, then!
		if (spr->mobj->z < floorheight)
			return;
		else
		{
			fixed_t floorz;
			floorz = HWR_OpaqueFloorAtPos(
				spr->mobj->x + P_ReturnThrustX(spr->mobj, shadowdir, spr->mobj->z - floorheight),
				spr->mobj->y + P_ReturnThrustY(spr->mobj, shadowdir, spr->mobj->z - floorheight),
				spr->mobj->z, spr->mobj->height);
			// The shadow would be falling on a wall? Don't draw it, then.
			// Would draw midair otherwise.
			if (floorz < floorheight)
				return;
		}

		floorheight = FixedInt(spr->mobj->z - floorheight);

		offset = floorheight;
	}
	else
		floorheight = FixedInt(spr->mobj->z - mobjfloor);

	// create the sprite billboard
	//
	//  3--2
	//  | /|
	//  |/ |
	//  0--1

	// x1/x2 were already scaled in HWR_ProjectSprite
	// First match the normal sprite
	swallVerts[0].x = swallVerts[3].x = spr->x1;
	swallVerts[2].x = swallVerts[1].x = spr->x2;
	swallVerts[0].z = swallVerts[3].z = spr->z1;
	swallVerts[2].z = swallVerts[1].z = spr->z2;

	if (spr->mobj && fabsf(this_scale - 1.0f) > 1.0E-36f)
	{
		// Always a pixel above the floor, perfectly flat.
		swallVerts[0].y = swallVerts[1].y = swallVerts[2].y = swallVerts[3].y = spr->ty - gpatch->topoffset * this_scale - (floorheight+3);

		// Now transform the TOP vertices along the floor in the direction of the camera
		swallVerts[3].x = spr->x1 + ((gpatch->height * this_scale) + offset) * gr_viewcos;
		swallVerts[2].x = spr->x2 + ((gpatch->height * this_scale) + offset) * gr_viewcos;
		swallVerts[3].z = spr->z1 + ((gpatch->height * this_scale) + offset) * gr_viewsin;
		swallVerts[2].z = spr->z2 + ((gpatch->height * this_scale) + offset) * gr_viewsin;
	}
	else
	{
		// Always a pixel above the floor, perfectly flat.
		swallVerts[0].y = swallVerts[1].y = swallVerts[2].y = swallVerts[3].y = spr->ty - gpatch->topoffset - (floorheight+3);

		// Now transform the TOP vertices along the floor in the direction of the camera
		swallVerts[3].x = spr->x1 + (gpatch->height + offset) * gr_viewcos;
		swallVerts[2].x = spr->x2 + (gpatch->height + offset) * gr_viewcos;
		swallVerts[3].z = spr->z1 + (gpatch->height + offset) * gr_viewsin;
		swallVerts[2].z = spr->z2 + (gpatch->height + offset) * gr_viewsin;
	}

	// We also need to move the bottom ones away when shadowoffs is on
	if (cv_shadowoffs.value)
	{
		swallVerts[0].x = spr->x1 + offset * gr_viewcos;
		swallVerts[1].x = spr->x2 + offset * gr_viewcos;
		swallVerts[0].z = spr->z1 + offset * gr_viewsin;
		swallVerts[1].z = spr->z2 + offset * gr_viewsin;
	}
	
	if (floorslope)
	{
		for (int i = 0; i < 4; i++)
		{
			slopez = P_GetZAt(floorslope, FLOAT_TO_FIXED(swallVerts[i].x), FLOAT_TO_FIXED(swallVerts[i].z));
			swallVerts[i].y = FIXED_TO_FLOAT(slopez) + 0.05f;
		}
	}

	if (spr->flip)
	{
		swallVerts[0].s = swallVerts[3].s = gpatch->max_s;
		swallVerts[2].s = swallVerts[1].s = 0;
	}
	else
	{
		swallVerts[0].s = swallVerts[3].s = 0;
		swallVerts[2].s = swallVerts[1].s = gpatch->max_s;
	}

	// flip the texture coords (look familiar?)
	if (spr->vflip)
	{
		swallVerts[3].t = swallVerts[2].t = gpatch->max_t;
		swallVerts[0].t = swallVerts[1].t = 0;
	}
	else
	{
		swallVerts[3].t = swallVerts[2].t = 0;
		swallVerts[0].t = swallVerts[1].t = gpatch->max_t;
	}

	sSurf.PolyColor.s.red = 0x01;
	sSurf.PolyColor.s.blue = 0x01;
	sSurf.PolyColor.s.green = 0x01;

	// shadow is always half as translucent as the sprite itself
	if (!cv_translucency.value) // use default translucency (main sprite won't have any translucency)
		sSurf.PolyColor.s.alpha = 0x80; // default
	else if (spr->mobj->flags2 & MF2_SHADOW)
		sSurf.PolyColor.s.alpha = 0x20;
	else if (spr->mobj->frame & FF_TRANSMASK)
	{
		HWR_TranstableToAlpha((spr->mobj->frame & FF_TRANSMASK)>>FF_TRANSSHIFT, &sSurf);
		sSurf.PolyColor.s.alpha /= 2; //cut alpha in half!
	}
	else
		sSurf.PolyColor.s.alpha = 0x80; // default

	if (sSurf.PolyColor.s.alpha > floorheight/4)
	{
		sSurf.PolyColor.s.alpha = (UINT8)(sSurf.PolyColor.s.alpha - floorheight/4);

		if (HWR_UseShader())
		{
			shader = SHADER_FLOOR;	// floor shader
		}

		HWR_ProcessPolygon(&sSurf, swallVerts, 4, PF_Translucent|PF_Modulated, shader, false);
	}
}

// This is expecting a pointer to an array containing 4 wallVerts for a sprite
static void HWR_RotateSpritePolyToAim(gr_vissprite_t *spr, FOutVector *wallVerts, const boolean precip)
{
	if (cv_grspritebillboarding.value && spr && spr->mobj && !(spr->mobj->frame & FF_PAPERSPRITE) && wallVerts)
	{
		// uncapped/interpolation
		interpmobjstate_t interp = {0};
		float basey, lowy;
		INT32 dist = -1;

		if (cv_grmaxinterpdist.value)
			dist = R_QuickCamDist(spr->mobj->x, spr->mobj->y);

		// do interpolation
		if (R_UsingFrameInterpolation() && !paused && (!cv_grmaxinterpdist.value || dist < cv_grmaxinterpdist.value))
		{
			if (precip)
			{
				R_InterpolatePrecipMobjState((precipmobj_t *)spr->mobj, rendertimefrac, &interp);
			}
			else
			{
				R_InterpolateMobjState(spr->mobj, rendertimefrac, &interp);
			}
		}
		else
		{
			if (precip)
			{
				R_InterpolatePrecipMobjState((precipmobj_t *)spr->mobj, FRACUNIT, &interp);
			}
			else
			{
				R_InterpolateMobjState(spr->mobj, FRACUNIT, &interp);
			}
		}

		if (P_MobjFlip(spr->mobj) == -1)
		{
			basey = FIXED_TO_FLOAT(interp.z + spr->mobj->height);
		}
		else
		{
			basey = FIXED_TO_FLOAT(interp.z);
		}
		lowy = wallVerts[0].y;

		// Rotate sprites to fully billboard with the camera
		// X, Y, AND Z need to be manipulated for the polys to rotate around the
		// origin, because of how the origin setting works I believe that should
		// be mobj->z or mobj->z + mobj->height
		wallVerts[2].y = wallVerts[3].y = (spr->gzt - basey) * gr_viewludsin + basey;
		wallVerts[0].y = wallVerts[1].y = (lowy - basey) * gr_viewludsin + basey;
		// translate back to be around 0 before translating back
		wallVerts[3].x += ((spr->gzt - basey) * gr_viewludcos) * gr_viewcos;
		wallVerts[2].x += ((spr->gzt - basey) * gr_viewludcos) * gr_viewcos;

		wallVerts[0].x += ((lowy - basey) * gr_viewludcos) * gr_viewcos;
		wallVerts[1].x += ((lowy - basey) * gr_viewludcos) * gr_viewcos;

		wallVerts[3].z += ((spr->gzt - basey) * gr_viewludcos) * gr_viewsin;
		wallVerts[2].z += ((spr->gzt - basey) * gr_viewludcos) * gr_viewsin;

		wallVerts[0].z += ((lowy - basey) * gr_viewludcos) * gr_viewsin;
		wallVerts[1].z += ((lowy - basey) * gr_viewludcos) * gr_viewsin;
	}
}

static void HWR_SplitSprite(gr_vissprite_t *spr)
{
	float this_scale = 1.0f;
	FOutVector wallVerts[4];
	FOutVector baseWallVerts[4]; // This is what the verts should end up as
	GLPatch_t *gpatch;
	FSurfaceInfo Surf;
	const boolean hires = (spr->mobj && spr->mobj->skin && ((skin_t *)( (spr->mobj->localskin) ? spr->mobj->localskin : spr->mobj->skin ))->flags & SF_HIRES);
	extracolormap_t *colormap;
	FUINT lightlevel;
	FBITFIELD blend = 0;
	UINT8 alpha;

	INT32 i;
	float realtop, realbot, top, bot;
	float towtop, towbot, towmult;
	float bheight;
	float realheight, heightmult;
	const sector_t *sector = spr->mobj->subsector->sector;
	const lightlist_t *list = sector->lightlist;
	float endrealtop, endrealbot, endtop, endbot;
	float endbheight;
	float endrealheight;
	fixed_t temp;
	fixed_t v1x, v1y, v2x, v2y;
	INT32 shader = SHADER_NONE;

	this_scale = FIXED_TO_FLOAT(spr->mobj->scale);

	if (hires)
		this_scale = this_scale * FIXED_TO_FLOAT(((skin_t *)( (spr->mobj->localskin) ? spr->mobj->localskin : spr->mobj->skin ))->highresscale);

	gpatch = spr->gpatch; //W_CachePatchNum(spr->patchlumpnum, PU_CACHE);

	// cache the patch in the graphics card memory
	//12/12/99: Hurdler: same comment as above (for md2)
	//Hurdler: 25/04/2000: now support colormap in hardware mode
	HWR_GetMappedPatch(gpatch, spr->colormap);

	// Draw shadow BEFORE sprite
	if (cv_shadow.value // Shadows enabled
		&& (spr->mobj->flags & (MF_SCENERY|MF_SPAWNCEILING|MF_NOGRAVITY)) != (MF_SCENERY|MF_SPAWNCEILING|MF_NOGRAVITY) // Ceiling scenery have no shadow.
		&& !(spr->mobj->flags2 & MF2_DEBRIS) // Debris have no corona or shadow.
		&& (spr->mobj->z >= spr->mobj->floorz)) // Without this, your shadow shows on the floor, even after you die and fall through the ground.
	{
		////////////////////
		// SHADOW SPRITE! //
		////////////////////
		HWR_DrawSpriteShadow(spr, gpatch, this_scale);
	}

	baseWallVerts[0].x = baseWallVerts[3].x = spr->x1;
	baseWallVerts[2].x = baseWallVerts[1].x = spr->x2;
	baseWallVerts[0].z = baseWallVerts[3].z = spr->z1;
	baseWallVerts[1].z = baseWallVerts[2].z = spr->z2;

	baseWallVerts[2].y = baseWallVerts[3].y = spr->gzt;
	baseWallVerts[0].y = baseWallVerts[1].y = spr->gz;

	v1x = FLOAT_TO_FIXED(spr->x1);
	v1y = FLOAT_TO_FIXED(spr->z1);
	v2x = FLOAT_TO_FIXED(spr->x2);
	v2y = FLOAT_TO_FIXED(spr->z2);

	if (spr->flip)
	{
		baseWallVerts[0].s = baseWallVerts[3].s = gpatch->max_s;
		baseWallVerts[2].s = baseWallVerts[1].s = 0;
	}
	else
	{
		baseWallVerts[0].s = baseWallVerts[3].s = 0;
		baseWallVerts[2].s = baseWallVerts[1].s = gpatch->max_s;
	}

	// flip the texture coords (look familiar?)
	if (spr->vflip)
	{
		baseWallVerts[3].t = baseWallVerts[2].t = gpatch->max_t;
		baseWallVerts[0].t = baseWallVerts[1].t = 0;
	}
	else
	{
		baseWallVerts[3].t = baseWallVerts[2].t = 0;
		baseWallVerts[0].t = baseWallVerts[1].t = gpatch->max_t;
	}

	// Let dispoffset work first since this adjust each vertex
	HWR_RotateSpritePolyToAim(spr, baseWallVerts, false);

	// push it toward the camera to mitigate floor-clipping sprites
	{
		float sprdist = sqrtf((spr->x1 - gr_viewx)*(spr->x1 - gr_viewx) + (spr->z1 - gr_viewy)*(spr->z1 - gr_viewy) + (spr->ty - gr_viewz)*(spr->ty - gr_viewz));
		float distfact = ((2.0f*spr->dispoffset) + 20.0f) / sprdist;
		for (i = 0; i < 4; i++)
		{
			baseWallVerts[i].x += (gr_viewx - baseWallVerts[i].x)*distfact;
			baseWallVerts[i].z += (gr_viewy - baseWallVerts[i].z)*distfact;
			baseWallVerts[i].y += (gr_viewz - baseWallVerts[i].y)*distfact;
		}
	}

	realtop = top = baseWallVerts[3].y;
	realbot = bot = baseWallVerts[0].y;
	towtop = baseWallVerts[3].t;
	towbot = baseWallVerts[0].t;
	towmult = (towbot - towtop) / (top - bot);

	endrealtop = endtop = baseWallVerts[2].y;
	endrealbot = endbot = baseWallVerts[1].y;

	// copy the contents of baseWallVerts into the drawn wallVerts array
	// baseWallVerts is used to know the final shape to easily get the vertex
	// co-ordinates
	memcpy_fast(wallVerts, baseWallVerts, sizeof(baseWallVerts));

	if (!cv_translucency.value) // translucency disabled
	{
		Surf.PolyColor.s.alpha = 0xFF;
		blend = PF_Translucent|PF_Occlude;
	}
	else if (spr->mobj->flags2 & MF2_SHADOW)
	{
		Surf.PolyColor.s.alpha = 0x40;
		blend = PF_Translucent;
	}
	else if (spr->mobj->frame & FF_TRANSMASK)
		blend = HWR_TranstableToAlpha((spr->mobj->frame & FF_TRANSMASK)>>FF_TRANSSHIFT, &Surf);
	else
	{
		// BP: i agree that is little better in environement but it don't
		//     work properly under glide nor with fogcolor to ffffff :(
		// Hurdler: PF_Environement would be cool, but we need to fix
		//          the issue with the fog before
		Surf.PolyColor.s.alpha = 0xFF;
		blend = PF_Translucent|PF_Occlude;
	}

	if (HWR_UseShader())
	{
		shader = SHADER_SPRITE;
		blend |= PF_ColorMapped;
	}

	alpha = Surf.PolyColor.s.alpha;
	
	// Start with the lightlevel and colormap from the top of the sprite
	lightlevel = SOFTLIGHT(*list[sector->numlights - 1].lightlevel);

	colormap = list[sector->numlights - 1].extra_colormap;
	i = 0;
	temp = FLOAT_TO_FIXED(realtop);

	if (spr->mobj->frame & FF_FULLBRIGHT)
		lightlevel = 255;

	for (i = 1; i < sector->numlights; i++)
	{
		fixed_t h = P_GetLightZAt(&sector->lightlist[i], spr->mobj->x, spr->mobj->y);
		if (h <= temp)
		{
			if (!(spr->mobj->frame & FF_FULLBRIGHT))
				lightlevel = *list[i-1].lightlevel > 255 ? 255 : SOFTLIGHT(*list[i-1].lightlevel);
			colormap = list[i-1].extra_colormap;
			break;
		}
	}

	for (i = 0; i < sector->numlights; i++)
	{
		if (endtop < endrealbot && top < realbot)
			return;

		// even if we aren't changing colormap or lightlevel, we still need to continue drawing down the sprite
		if (!(list[i].flags & FF_NOSHADE) && (list[i].flags & FF_CUTSPRITES))
		{
			if (!(spr->mobj->frame & FF_FULLBRIGHT))
				lightlevel = *list[i].lightlevel > 255 ? 255 : SOFTLIGHT(*list[i].lightlevel);
			colormap = list[i].extra_colormap;
		}

		if (i + 1 < sector->numlights)
		{
			temp = P_GetLightZAt(&list[i+1], v1x, v1y);
			bheight = FIXED_TO_FLOAT(temp);
			temp = P_GetLightZAt(&list[i+1], v2x, v2y);
			endbheight = FIXED_TO_FLOAT(temp);
		}
		else
		{
			bheight = realbot;
			endbheight = endrealbot;
		}

		if (endbheight >= endtop && bheight >= top)
			continue;

		bot = bheight;

		if (bot < realbot)
			bot = realbot;

		endbot = endbheight;

		if (endbot < endrealbot)
			endbot = endrealbot;

		wallVerts[3].t = towtop + ((realtop - top) * towmult);
		wallVerts[2].t = towtop + ((endrealtop - endtop) * towmult);
		wallVerts[0].t = towtop + ((realtop - bot) * towmult);
		wallVerts[1].t = towtop + ((endrealtop - endbot) * towmult);

		wallVerts[3].y = top;
		wallVerts[2].y = endtop;
		wallVerts[0].y = bot;
		wallVerts[1].y = endbot;

		// The x and y only need to be adjusted in the case that it's not a papersprite
		if (cv_grspritebillboarding.value && spr->mobj && !(spr->mobj->frame & FF_PAPERSPRITE))
		{
			// Get the x and z of the vertices so billboarding draws correctly
			realheight = realbot - realtop;
			endrealheight = endrealbot - endrealtop;
			heightmult = (realtop - top) / realheight;
			wallVerts[3].x = baseWallVerts[3].x + (baseWallVerts[3].x - baseWallVerts[0].x) * heightmult;
			wallVerts[3].z = baseWallVerts[3].z + (baseWallVerts[3].z - baseWallVerts[0].z) * heightmult;

			heightmult = (endrealtop - endtop) / endrealheight;
			wallVerts[2].x = baseWallVerts[2].x + (baseWallVerts[2].x - baseWallVerts[1].x) * heightmult;
			wallVerts[2].z = baseWallVerts[2].z + (baseWallVerts[2].z - baseWallVerts[1].z) * heightmult;

			heightmult = (realtop - bot) / realheight;
			wallVerts[0].x = baseWallVerts[3].x + (baseWallVerts[3].x - baseWallVerts[0].x) * heightmult;
			wallVerts[0].z = baseWallVerts[3].z + (baseWallVerts[3].z - baseWallVerts[0].z) * heightmult;

			heightmult = (endrealtop - endbot) / endrealheight;
			wallVerts[1].x = baseWallVerts[2].x + (baseWallVerts[2].x - baseWallVerts[1].x) * heightmult;
			wallVerts[1].z = baseWallVerts[2].z + (baseWallVerts[2].z - baseWallVerts[1].z) * heightmult;
		}

		HWR_Lighting(&Surf, lightlevel, colormap);

		Surf.PolyColor.s.alpha = alpha;

		HWR_ProcessPolygon(&Surf, wallVerts, 4, blend|PF_Modulated, shader, false); // sprite shader

		top = bot;
		endtop = endbot;
	}

	bot = realbot;
	endbot = endrealbot;

	if (endtop <= endrealbot && top <= realbot)
		return;

	// If we're ever down here, somehow the above loop hasn't draw all the light levels of sprite
	wallVerts[3].t = towtop + ((realtop - top) * towmult);
	wallVerts[2].t = towtop + ((endrealtop - endtop) * towmult);
	wallVerts[0].t = towtop + ((realtop - bot) * towmult);
	wallVerts[1].t = towtop + ((endrealtop - endbot) * towmult);

	wallVerts[3].y = top;
	wallVerts[2].y = endtop;
	wallVerts[0].y = bot;
	wallVerts[1].y = endbot;

	HWR_Lighting(&Surf, lightlevel, colormap);

	Surf.PolyColor.s.alpha = alpha;

	HWR_ProcessPolygon(&Surf, wallVerts, 4, blend|PF_Modulated, shader, false); // sprite shader
}

// -----------------+
// HWR_DrawSprite   : Draw flat sprites
//                  : (monsters, bonuses, weapons, lights, ...)
// Returns          :
// -----------------+
static void HWR_DrawSprite(gr_vissprite_t *spr)
{
	float this_scale = 1.0f;
	FOutVector wallVerts[4];
	GLPatch_t *gpatch; // sprite patch converted to hardware
	FSurfaceInfo Surf;
	
	INT32 shader = SHADER_NONE;

	if (!spr->mobj)
		return;

	if (!spr->mobj->subsector)
		return;

	if (spr->mobj->subsector->sector->numlights)
	{
		HWR_SplitSprite(spr);
		return;
	}

	const boolean hires = (spr->mobj && spr->mobj->skin && ((skin_t *)( (spr->mobj->localskin) ? spr->mobj->localskin : spr->mobj->skin ))->flags & SF_HIRES);
	if (spr->mobj)
		this_scale = FIXED_TO_FLOAT(spr->mobj->scale);
	if (hires)
		this_scale = this_scale * FIXED_TO_FLOAT(((skin_t *)( (spr->mobj->localskin) ? spr->mobj->localskin : spr->mobj->skin ))->highresscale);

	// cache sprite graphics
	//12/12/99: Hurdler:
	//          OK, I don't change anything for MD2 support because I want to be
	//          sure to do it the right way. So actually, we keep normal sprite
	//          in memory and we add the md2 model if it exists for that sprite

	gpatch = spr->gpatch; //W_CachePatchNum(spr->patchlumpnum, PU_CACHE);

	// create the sprite billboard
	//
	//  3--2
	//  | /|
	//  |/ |
	//  0--1

	// these were already scaled in HWR_ProjectSprite
	wallVerts[0].x = wallVerts[3].x = spr->x1;
	wallVerts[2].x = wallVerts[1].x = spr->x2;
	wallVerts[2].y = wallVerts[3].y = spr->gzt;
	wallVerts[0].y = wallVerts[1].y = spr->gz;

	// make a wall polygon (with 2 triangles), using the floor/ceiling heights,
	// and the 2d map coords of start/end vertices
	wallVerts[0].z = wallVerts[3].z = spr->z1;
	wallVerts[1].z = wallVerts[2].z = spr->z2;

	if (spr->flip)
	{
		wallVerts[0].s = wallVerts[3].s = gpatch->max_s;
		wallVerts[2].s = wallVerts[1].s = 0;
	}else{
		wallVerts[0].s = wallVerts[3].s = 0;
		wallVerts[2].s = wallVerts[1].s = gpatch->max_s;
	}

	// flip the texture coords (look familiar?)
	if (spr->vflip)
	{
		wallVerts[3].t = wallVerts[2].t = gpatch->max_t;
		wallVerts[0].t = wallVerts[1].t = 0;
	}else{
		wallVerts[3].t = wallVerts[2].t = 0;
		wallVerts[0].t = wallVerts[1].t = gpatch->max_t;
	}

	// cache the patch in the graphics card memory
	//12/12/99: Hurdler: same comment as above (for md2)
	//Hurdler: 25/04/2000: now support colormap in hardware mode
	HWR_GetMappedPatch(gpatch, spr->colormap);

	// Draw shadow BEFORE sprite
	if (cv_shadow.value // Shadows enabled
		&& (spr->mobj->flags & (MF_SCENERY|MF_SPAWNCEILING|MF_NOGRAVITY)) != (MF_SCENERY|MF_SPAWNCEILING|MF_NOGRAVITY) // Ceiling scenery have no shadow.
		&& !(spr->mobj->flags2 & MF2_DEBRIS) // Debris have no corona or shadow.
		&& (spr->mobj->z >= spr->mobj->floorz)) // Without this, your shadow shows on the floor, even after you die and fall through the ground.
	{
		////////////////////
		// SHADOW SPRITE! //
		////////////////////
		HWR_DrawSpriteShadow(spr, gpatch, this_scale);
	}

	// Let dispoffset work first since this adjust each vertex
	// ...nah
	HWR_RotateSpritePolyToAim(spr, wallVerts, false);

	// push it toward the camera to mitigate floor-clipping sprites
	{
		float sprdist = sqrtf((spr->x1 - gr_viewx)*(spr->x1 - gr_viewx) + (spr->z1 - gr_viewy)*(spr->z1 - gr_viewy) + (spr->ty - gr_viewz)*(spr->ty - gr_viewz));
		float distfact = ((2.0f*spr->dispoffset) + 20.0f) / sprdist;
		size_t i;
		for (i = 0; i < 4; i++)
		{
			wallVerts[i].x += (gr_viewx - wallVerts[i].x)*distfact;
			wallVerts[i].z += (gr_viewy - wallVerts[i].z)*distfact;
			wallVerts[i].y += (gr_viewz - wallVerts[i].y)*distfact;
		}
	}

	// This needs to be AFTER the shadows so that the regular sprites aren't drawn completely black.
	// sprite lighting by modulating the RGB components
	/// \todo coloured

	// colormap test
	{
		sector_t *sector = spr->mobj->subsector->sector;
		UINT8 lightlevel = 255;
		extracolormap_t *colormap = sector->extra_colormap;

		if (!(spr->mobj->frame & FF_FULLBRIGHT))
			lightlevel = sector->lightlevel > 255 ? 255 : SOFTLIGHT(sector->lightlevel);

		HWR_Lighting(&Surf, lightlevel, colormap);
	}

	{
		FBITFIELD blend = 0;
		if (!cv_translucency.value) // translucency disabled
		{
			Surf.PolyColor.s.alpha = 0xFF;
			blend = PF_Translucent|PF_Occlude;
		}
		else if (spr->mobj->flags2 & MF2_SHADOW)
		{
			Surf.PolyColor.s.alpha = 0x40;
			blend = PF_Translucent;
		}
		else if (spr->mobj->frame & FF_TRANSMASK)
			blend = HWR_TranstableToAlpha((spr->mobj->frame & FF_TRANSMASK)>>FF_TRANSSHIFT, &Surf);
		else
		{
			// BP: i agree that is little better in environement but it don't
			//     work properly under glide nor with fogcolor to ffffff :(
			// Hurdler: PF_Environement would be cool, but we need to fix
			//          the issue with the fog before
			Surf.PolyColor.s.alpha = 0xFF;
			blend = PF_Translucent|PF_Occlude;
		}

		if (HWR_UseShader())
		{
			shader = SHADER_SPRITE;	// sprite shader
			blend |= PF_ColorMapped;
		}

		HWR_ProcessPolygon(&Surf, wallVerts, 4, blend|PF_Modulated, shader, false);
	}
}

// Sprite drawer for precipitation
static inline void HWR_DrawPrecipitationSprite(gr_vissprite_t *spr)
{
	FBITFIELD blend = 0;
	FOutVector wallVerts[4];
	GLPatch_t *gpatch; // sprite patch converted to hardware
	FSurfaceInfo Surf;

	INT32 shader = SHADER_NONE;

	if (!spr->mobj)
		return;

	if (!spr->mobj->subsector)
		return;

	// cache sprite graphics
	gpatch = spr->gpatch; //W_CachePatchNum(spr->patchlumpnum, PU_CACHE);

	// create the sprite billboard
	//
	//  3--2
	//  | /|
	//  |/ |
	//  0--1
	wallVerts[0].x = wallVerts[3].x = spr->x1;
	wallVerts[2].x = wallVerts[1].x = spr->x2;
	wallVerts[2].y = wallVerts[3].y = spr->ty;
	wallVerts[0].y = wallVerts[1].y = spr->ty - gpatch->height;

	// make a wall polygon (with 2 triangles), using the floor/ceiling heights,
	// and the 2d map coords of start/end vertices
	wallVerts[0].z = wallVerts[3].z = spr->z1;
	wallVerts[1].z = wallVerts[2].z = spr->z2;

	// Let dispoffset work first since this adjust each vertex
	HWR_RotateSpritePolyToAim(spr, wallVerts, true);

	wallVerts[0].s = wallVerts[3].s = 0;
	wallVerts[2].s = wallVerts[1].s = gpatch->max_s;

	wallVerts[3].t = wallVerts[2].t = 0;
	wallVerts[0].t = wallVerts[1].t = gpatch->max_t;

	// cache the patch in the graphics card memory
	//12/12/99: Hurdler: same comment as above (for md2)
	//Hurdler: 25/04/2000: now support colormap in hardware mode
	HWR_GetMappedPatch(gpatch, spr->colormap);

	// colormap test
	{
		sector_t *sector = spr->mobj->subsector->sector;
		UINT8 lightlevel = 255;
		extracolormap_t *colormap = sector->extra_colormap;

		if (sector->numlights)
		{
			INT32 light;

			light = R_GetPlaneLight(sector, spr->mobj->z + spr->mobj->height, false); // Always use the light at the top instead of whatever I was doing before

			if (!(spr->mobj->frame & FF_FULLBRIGHT))
				lightlevel = *sector->lightlist[light].lightlevel > 255 ? 255 : SOFTLIGHT(*sector->lightlist[light].lightlevel);

			if (sector->lightlist[light].extra_colormap)
				colormap = sector->lightlist[light].extra_colormap;
		}
		else
		{
			if (!(spr->mobj->frame & FF_FULLBRIGHT))
			lightlevel = sector->lightlevel > 255 ? 255 : SOFTLIGHT(sector->lightlevel);

			if (sector->extra_colormap)
				colormap = sector->extra_colormap;
		}

		HWR_Lighting(&Surf, lightlevel, colormap);
	}

	if (spr->mobj->frame & FF_TRANSMASK)
		blend = HWR_TranstableToAlpha((spr->mobj->frame & FF_TRANSMASK)>>FF_TRANSSHIFT, &Surf);
	else
	{
		// BP: i agree that is little better in environement but it don't
		//     work properly under glide nor with fogcolor to ffffff :(
		// Hurdler: PF_Environement would be cool, but we need to fix
		//          the issue with the fog before
		Surf.PolyColor.s.alpha = 0xFF;
		blend = PF_Translucent|PF_Occlude;
	}

	if (HWR_UseShader())
	{
		shader = SHADER_SPRITE;	// sprite shader
		blend |= PF_ColorMapped;
	}

	HWR_ProcessPolygon(&Surf, wallVerts, 4, blend|PF_Modulated, shader, false);
}

// --------------------------------------------------------------------------
// Sort vissprites by distance
// --------------------------------------------------------------------------

gr_vissprite_t* gr_vsprorder[MAXVISSPRITES];

// For more correct transparency the transparent sprites would need to be
// sorted and drawn together with transparent surfaces.
static int CompareVisSprites(const void *p1, const void *p2)
{
	gr_vissprite_t* spr1 = *(gr_vissprite_t*const*)p1;
	gr_vissprite_t* spr2 = *(gr_vissprite_t*const*)p2;
	int idiff;
	float fdiff;
	float tz1, tz2;
	
	int transparency1;
	int transparency2;

	// make transparent sprites last
	// "boolean to int"

	// check for precip first, because then sprX->mobj is actually a precipmobj_t and does not have flags2 or tracer
	tz1 = spr1->tz;
	transparency1 = (!spr1->precip && (spr1->mobj->flags2 & MF2_SHADOW)) || (spr1->mobj->frame & FF_TRANSMASK);
	tz2 = spr2->tz;
	transparency2 = (!spr2->precip && (spr2->mobj->flags2 & MF2_SHADOW)) || (spr2->mobj->frame & FF_TRANSMASK);
	
	// first compare transparency flags, then compare tz, then compare dispoffset

	idiff = transparency1 - transparency2;
	if (idiff != 0) return idiff;

	fdiff = tz2 - tz1; // this order seems correct when checking with apitrace. Back to front.
	if (fabsf(fdiff) < 1.0E-36f)
		return spr1->dispoffset - spr2->dispoffset; // smallest dispoffset first if sprites are at (almost) same location.
	else if (fdiff > 0)
		return 1;
	else
		return -1;
}

#undef SOFTLIGHT

static void HWR_SortVisSprites(void)
{
	UINT32 i;
	for (i = 0; i < gr_visspritecount; i++)
	{
		gr_vsprorder[i] = HWR_GetVisSprite(i);
	}
	qs22j(gr_vsprorder, gr_visspritecount, sizeof(gr_vissprite_t*), CompareVisSprites);
}

// A drawnode is something that points to a 3D floor, 3D side, or masked
// middle texture. This is used for sorting with sprites.
typedef struct
{
	FOutVector    wallVerts[4];
	FSurfaceInfo  Surf;
	INT32         texnum;
	boolean		  noencore;
	FBITFIELD     blend;
	boolean fogwall;
	INT32 lightlevel;
	extracolormap_t *wallcolormap; // Doing the lighting in HWR_RenderWall now for correct fog after sorting
} wallinfo_t;

typedef struct
{
	extrasubsector_t *xsub;
	boolean isceiling;
	fixed_t fixedheight;
	INT32 lightlevel;
	lumpnum_t lumpnum;
	INT32 alpha;
	sector_t *FOFSector;
	FBITFIELD blend;
	boolean fogplane;
	extracolormap_t *planecolormap;
} planeinfo_t;

typedef struct
{
	polyobj_t *polysector;
	boolean isceiling;
	fixed_t fixedheight;
	INT32 lightlevel;
	lumpnum_t lumpnum;
	INT32 alpha;
	sector_t *FOFSector;
	FBITFIELD blend;
	extracolormap_t *planecolormap;
} polyplaneinfo_t;

typedef enum
{
	DRAWNODE_PLANE,
	DRAWNODE_POLYOBJECT_PLANE,
	DRAWNODE_WALL
} gr_drawnode_type_t;

typedef struct
{
	gr_drawnode_type_t type;
	union {
		planeinfo_t plane;
		polyplaneinfo_t polyplane;
		wallinfo_t wall;
	} u;
} gr_drawnode_t;

// initial size of drawnode array
#define DRAWNODES_INIT_SIZE 64
gr_drawnode_t *drawnodes = NULL;
INT32 numdrawnodes = 0;
INT32 alloceddrawnodes = 0;

static void *HWR_CreateDrawNode(gr_drawnode_type_t type)
{
	gr_drawnode_t *drawnode;

	if (!drawnodes)
	{
		alloceddrawnodes = DRAWNODES_INIT_SIZE;
		drawnodes = Z_Malloc(alloceddrawnodes * sizeof(gr_drawnode_t), PU_LEVEL, &drawnodes);
	}
	else if (numdrawnodes >= alloceddrawnodes)
	{
		alloceddrawnodes *= 2;
		Z_Realloc(drawnodes, alloceddrawnodes * sizeof(gr_drawnode_t), PU_LEVEL, &drawnodes);
	}


	drawnode = &drawnodes[numdrawnodes++];
	drawnode->type = type;

	// not sure if returning different pointers to a union is necessary
	switch (type)
	{
		case DRAWNODE_PLANE:
			return &drawnode->u.plane;
		case DRAWNODE_POLYOBJECT_PLANE:
			return &drawnode->u.polyplane;
		case DRAWNODE_WALL:
			return &drawnode->u.wall;
	}
	return NULL;
}

void HWR_AddTransparentWall(FOutVector *wallVerts, FSurfaceInfo *pSurf, INT32 texnum, boolean noencore, FBITFIELD blend, boolean fogwall, INT32 lightlevel, extracolormap_t *wallcolormap)
{
	wallinfo_t *wallinfo = HWR_CreateDrawNode(DRAWNODE_WALL);

	M_Memcpy(wallinfo->wallVerts, wallVerts, sizeof (wallinfo->wallVerts));
	M_Memcpy(&wallinfo->Surf, pSurf, sizeof (FSurfaceInfo));
	wallinfo->texnum = texnum;
	wallinfo->noencore = noencore;
	wallinfo->blend = blend;
	wallinfo->fogwall = fogwall;
	wallinfo->lightlevel = lightlevel;
	wallinfo->wallcolormap = wallcolormap;
}

void HWR_AddTransparentFloor(lumpnum_t lumpnum, extrasubsector_t *xsub, boolean isceiling, fixed_t fixedheight, INT32 lightlevel, INT32 alpha, sector_t *FOFSector, FBITFIELD blend, boolean fogplane, extracolormap_t *planecolormap)
{
	planeinfo_t *planeinfo = HWR_CreateDrawNode(DRAWNODE_PLANE);

	planeinfo->isceiling = isceiling;
	planeinfo->fixedheight = fixedheight;

	if (HWR_ShouldUsePaletteRendering())
		planeinfo->lightlevel = (planecolormap && (planecolormap->fog & 1)) ? 255 : lightlevel;
	else
		planeinfo->lightlevel = lightlevel;
	
	planeinfo->lumpnum = lumpnum;
	planeinfo->xsub = xsub;
	planeinfo->alpha = alpha;
	planeinfo->FOFSector = FOFSector;
	planeinfo->blend = blend;
	planeinfo->fogplane = fogplane;
	planeinfo->planecolormap = planecolormap;
}

// Adding this for now until I can create extrasubsector info for polyobjects
// When that happens it'll just be done through HWR_AddTransparentFloor and HWR_RenderPlane
void HWR_AddTransparentPolyobjectFloor(lumpnum_t lumpnum, polyobj_t *polysector, boolean isceiling, fixed_t fixedheight, INT32 lightlevel, INT32 alpha, sector_t *FOFSector, FBITFIELD blend, extracolormap_t *planecolormap)
{
	polyplaneinfo_t *polyplaneinfo = HWR_CreateDrawNode(DRAWNODE_POLYOBJECT_PLANE);

	polyplaneinfo->isceiling = isceiling;
	polyplaneinfo->fixedheight = fixedheight;
	
	if (HWR_ShouldUsePaletteRendering())
		polyplaneinfo->lightlevel = (planecolormap && (planecolormap->fog & 1)) ? 255 : lightlevel;
	else
		polyplaneinfo->lightlevel = lightlevel;
	
	polyplaneinfo->lumpnum = lumpnum;
	polyplaneinfo->polysector = polysector;
	polyplaneinfo->alpha = alpha;
	polyplaneinfo->FOFSector = FOFSector;
	polyplaneinfo->blend = blend;
	polyplaneinfo->planecolormap = planecolormap;
}

static int CompareDrawNodePlanes(const void *p1, const void *p2)
{
	INT32 n1 = *(const INT32*)p1;
	INT32 n2 = *(const INT32*)p2;

	return ABS(drawnodes[n2].u.plane.fixedheight - viewz) - ABS(drawnodes[n1].u.plane.fixedheight - viewz);
}

//
// HWR_RenderDrawNodes
// Sorts and renders the list of drawnodes for the scene being rendered.
static void HWR_RenderDrawNodes(void)
{
	INT32 i = 0, run_start = 0;

	// Array for storing the rendering order.
	// A list of indices into the drawnodes array.
	INT32 *sortindex;

	if (!numdrawnodes)
		return;

	ps_numdrawnodes.value.i = numdrawnodes;

	PS_START_TIMING(ps_hw_nodesorttime);

	sortindex = Z_Malloc(sizeof(INT32) * numdrawnodes, PU_STATIC, NULL);

	// Reversed order
	for (i = 0; i < numdrawnodes; i++)
		sortindex[i] = numdrawnodes - i - 1;

	// The order is correct apart from planes in the same subsector.
	// So scan the list and sort out these cases.
	// For each consecutive run of planes in the list, sort that run based on
	// plane height and view height.
	while (run_start < numdrawnodes-1) // numdrawnodes-1 because a 1 plane run at the end of the list does not count
	{
		// locate run start
		if (drawnodes[sortindex[run_start]].type == DRAWNODE_PLANE)
		{
			// found it, now look for run end
			INT32 run_end; // (inclusive)

			for (i = run_start+1; i < numdrawnodes; i++)
			{
				if (drawnodes[sortindex[i]].type != DRAWNODE_PLANE) break;
			}
			run_end = i-1;
			if (run_end > run_start) // if there are multiple consecutive planes, not just one
			{
				// consecutive run of planes found, now sort it
				qs22j(sortindex + run_start, run_end - run_start + 1, sizeof(INT32), CompareDrawNodePlanes);
			}
			run_start = run_end + 1; // continue looking for runs coming right after this one
		}
		else
		{
			// this wasnt the run start, try next one
			run_start++;
		}
	}

	PS_STOP_TIMING(ps_hw_nodesorttime);

	PS_START_TIMING(ps_hw_nodedrawtime);

	// Okay! Let's draw it all! Woo!
	HWD.pfnSetTransform(&atransform);

	for (i = 0; i < numdrawnodes; i++)
	{
		gr_drawnode_t *drawnode = &drawnodes[sortindex[i]];

		if (drawnode->type == DRAWNODE_PLANE)
		{
			planeinfo_t *plane = &drawnode->u.plane;

			// We aren't traversing the BSP tree, so make gr_frontsector null to avoid crashes.
			gr_frontsector = NULL;

			if (!(plane->blend & PF_NoTexture))
				HWR_GetFlat(plane->lumpnum,  R_NoEncore(plane->FOFSector, plane->isceiling));
			HWR_RenderPlane(NULL, plane->xsub, plane->isceiling, plane->fixedheight, plane->blend, plane->lightlevel,
				plane->lumpnum, plane->FOFSector, plane->alpha, plane->planecolormap);
		}
		else if (drawnode->type == DRAWNODE_POLYOBJECT_PLANE)
		{
			polyplaneinfo_t *polyplane = &drawnode->u.polyplane;

			// We aren't traversing the BSP tree, so make gr_frontsector null to avoid crashes.
			gr_frontsector = NULL;

			if (!(polyplane->blend & PF_NoTexture))
				HWR_GetFlat(polyplane->lumpnum,  R_NoEncore(polyplane->FOFSector, polyplane->isceiling));
			HWR_RenderPolyObjectPlane(polyplane->polysector, polyplane->isceiling, polyplane->fixedheight, polyplane->blend, polyplane->lightlevel,
				polyplane->lumpnum, polyplane->FOFSector, polyplane->alpha, polyplane->planecolormap);
		}
		else if (drawnode->type == DRAWNODE_WALL)
		{
			wallinfo_t *wall = &drawnode->u.wall;

			if (!(wall->blend & PF_NoTexture))
				HWR_GetTexture(wall->texnum, wall->noencore);
			HWR_RenderWall(wall->wallVerts, &wall->Surf, wall->blend, wall->fogwall,
				wall->lightlevel, wall->wallcolormap);
		}
	}
		
	PS_STOP_TIMING(ps_hw_nodedrawtime);

	numdrawnodes = 0;

	Z_Free(sortindex);
}


// --------------------------------------------------------------------------
//  Draw all vissprites
// --------------------------------------------------------------------------
void HWR_DrawSprites(void)
{
	UINT32 i;
	for (i = 0; i < gr_visspritecount; i++)
	{
		gr_vissprite_t *spr = gr_vsprorder[i];
		if (spr->precip)
			HWR_DrawPrecipitationSprite(spr);
		else
			if (spr->mobj && spr->mobj->skin && spr->mobj->sprite == SPR_PLAY)
			{
				md2_t *md2;
				if (spr->mobj->localskin)
				{
					if (spr->mobj->skinlocal)
						md2 = &md2_localplayermodels[(skin_t *)spr->mobj->localskin - localskins];
					else
						md2 = &md2_playermodels     [(skin_t *)spr->mobj->localskin -      skins];
				}
				else
					md2 = &md2_playermodels[(skin_t *)spr->mobj->skin - skins];

				// 8/1/19: Only don't display player models if no default SPR_PLAY is found.
				if (!cv_grmdls.value || ((md2->notfound || md2->scale < 0.0f) && ((!cv_grfallbackplayermodel.value) || md2_models[SPR_PLAY].notfound || md2_models[SPR_PLAY].scale < 0.0f)) || spr->mobj->state == &states[S_PLAY_SIGN])
					HWR_DrawSprite(spr);
				else
					HWR_DrawMD2(spr);
			}
			else
			{
				if (!cv_grmdls.value || md2_models[spr->mobj->sprite].notfound || md2_models[spr->mobj->sprite].scale < 0.0f)
					HWR_DrawSprite(spr);
				else
					HWR_DrawMD2(spr);
			}
	}
}

// --------------------------------------------------------------------------
// HWR_AddSprites
// During BSP traversal, this adds sprites by sector.
// --------------------------------------------------------------------------
void HWR_AddSprites(sector_t *sec)
{
	mobj_t *thing;
	precipmobj_t *precipthing;
	fixed_t approx_dist, limit_dist, precip_limit_dist;

	INT32 splitflags;
	boolean split_drawsprite;	// drawing with splitscreen flags

	// BSP is traversed by subsector.
	// A sector might have been split into several
	//  subsectors during BSP building.
	// Thus we check whether its already added.
	if (sec->validcount == validcount)
		return;

	// Well, now it will be done.
	sec->validcount = validcount;

	if (current_bsp_culling_distance)
	{
		// Use the smaller setting
		if (cv_drawdist.value)
			limit_dist = min(current_bsp_culling_distance, cv_drawdist.value << FRACBITS);
		else
			limit_dist = current_bsp_culling_distance;
		precip_limit_dist = min(current_bsp_culling_distance, cv_drawdist_precip.value << FRACBITS);
	}
	else
	{
		limit_dist = cv_drawdist.value << FRACBITS;
		precip_limit_dist = cv_drawdist_precip.value << FRACBITS;
	}

	// Handle all things in sector.
	// If a limit exists, handle things a tiny bit different.
	if (limit_dist)
	{
		for (thing = sec->thinglist; thing; thing = thing->snext)
		{

			split_drawsprite = false;

			if (thing->sprite == SPR_NULL || thing->flags2 & MF2_DONTDRAW)
				continue;

			splitflags = thing->eflags & (MFE_DRAWONLYFORP1|MFE_DRAWONLYFORP2|MFE_DRAWONLYFORP3|MFE_DRAWONLYFORP4);

			if (splitscreen && splitflags)
			{
				if (thing->eflags & MFE_DRAWONLYFORP1)
					if (viewssnum == 0)
						split_drawsprite = true;

				if (thing->eflags & MFE_DRAWONLYFORP2)
					if (viewssnum == 1)
						split_drawsprite = true;

				if (thing->eflags & MFE_DRAWONLYFORP3 && splitscreen > 1)
					if (viewssnum == 2)
						split_drawsprite = true;

				if (thing->eflags & MFE_DRAWONLYFORP4 && splitscreen > 2)
					if (viewssnum == 3)
						split_drawsprite = true;
			}
			else
				split_drawsprite = true;

			if (!split_drawsprite)
				continue;

			approx_dist = P_AproxDistance(viewx-thing->x, viewy-thing->y);

			if (approx_dist > limit_dist)
				continue;

			HWR_ProjectSprite(thing);
		}
	}
	else
	{
		// Draw everything in sector, no checks
		for (thing = sec->thinglist; thing; thing = thing->snext)
		{

			split_drawsprite = false;

			if (thing->sprite == SPR_NULL || thing->flags2 & MF2_DONTDRAW)
				continue;

			splitflags = thing->eflags & (MFE_DRAWONLYFORP1|MFE_DRAWONLYFORP2|MFE_DRAWONLYFORP3|MFE_DRAWONLYFORP4);

			if (splitscreen && splitflags)
			{
				if (thing->eflags & MFE_DRAWONLYFORP1)
					if (viewssnum == 0)
						split_drawsprite = true;

				if (thing->eflags & MFE_DRAWONLYFORP2)
					if (viewssnum == 1)
						split_drawsprite = true;

				if (thing->eflags & MFE_DRAWONLYFORP3 && splitscreen > 1)
					if (viewssnum == 2)
						split_drawsprite = true;

				if (thing->eflags & MFE_DRAWONLYFORP4 && splitscreen > 2)
					if (viewssnum == 3)
						split_drawsprite = true;
			}
			else
				split_drawsprite = true;

			if (!split_drawsprite)
				continue;

			HWR_ProjectSprite(thing);
		}
	}

	// No to infinite precipitation draw distance.
	if (precip_limit_dist)
	{
		for (precipthing = sec->preciplist; precipthing; precipthing = precipthing->snext)
		{
			if (precipthing->precipflags & PCF_INVISIBLE)
				continue;

			approx_dist = P_AproxDistance(viewx-precipthing->x, viewy-precipthing->y);

			if (approx_dist > precip_limit_dist)
				continue;

			HWR_ProjectPrecipitationSprite(precipthing);
		}
	}
}

// --------------------------------------------------------------------------
// HWR_ProjectSprite
//  Generates a vissprite for a thing if it might be visible.
// --------------------------------------------------------------------------
// BP why not use xtoviexangle/viewangletox like in bsp ?....
void HWR_ProjectSprite(mobj_t *thing)
{
	gr_vissprite_t *vis;
	float tr_x, tr_y;
	float tz;
	float x1, x2;
	float z1, z2;
	float rightsin, rightcos;
	float this_scale, this_xscale, this_yscale;
	float spritexscale, spriteyscale;
	float gz, gzt;
	spritedef_t *sprdef;
	spriteframe_t *sprframe;
#ifdef ROTSPRITE
	spriteinfo_t *sprinfo;
#endif
	size_t lumpoff;
	unsigned rot;
	UINT8 flip;
	boolean mirrored = thing->mirrored;
	boolean hflip = (!(thing->frame & FF_HORIZONTALFLIP) != !mirrored);

	angle_t ang, camang;
	const boolean papersprite = (thing->frame & FF_PAPERSPRITE);
	INT32 heightsec, phs;
	INT32 dist = -1;

	fixed_t spr_width, spr_height;
	fixed_t spr_offset, spr_topoffset;
#ifdef ROTSPRITE
	patch_t *rotsprite = NULL;
	INT32 rollangle = 0;
	angle_t rollsum = 0;
	angle_t sliptiderollangle = 0;
#endif

	// uncapped/interpolation
	interpmobjstate_t interp = {0};

	if (!thing)
		return;

	if (cv_grmaxinterpdist.value)
		dist = R_QuickCamDist(thing->x, thing->y);

	if (R_UsingFrameInterpolation() && !paused && (!cv_grmaxinterpdist.value || dist < cv_grmaxinterpdist.value))
	{
		R_InterpolateMobjState(thing, rendertimefrac, &interp);
	}
	else
	{
		R_InterpolateMobjState(thing, FRACUNIT, &interp);
	}

	if (interp.spritexscale < 1 || interp.spriteyscale < 1)
		return;

	this_scale = FIXED_TO_FLOAT(interp.scale);
	spritexscale = FIXED_TO_FLOAT(interp.spritexscale);
	spriteyscale = FIXED_TO_FLOAT(interp.spriteyscale);

	// transform the origin point
	tr_x = FIXED_TO_FLOAT(interp.x) - gr_viewx;
	tr_y = FIXED_TO_FLOAT(interp.y) - gr_viewy;

	// rotation around vertical axis
	tz = (tr_x * gr_viewcos) + (tr_y * gr_viewsin);

	// thing is behind view plane?
	if (tz < ZCLIP_PLANE && !papersprite && (!cv_grmdls.value || md2_models[thing->sprite].notfound == true)) //Yellow: Only MD2's dont disappear
		return;

	// The above can stay as it works for cutting sprites that are too close
	tr_x = FIXED_TO_FLOAT(interp.x);
	tr_y = FIXED_TO_FLOAT(interp.y);

	// decide which patch to use for sprite relative to player
#ifdef RANGECHECK
	if ((unsigned)thing->sprite >= numsprites)
		I_Error("HWR_ProjectSprite: invalid sprite number %i ", thing->sprite);
#endif

	rot = thing->frame&FF_FRAMEMASK;

	//Fab : 02-08-98: 'skin' override spritedef currently used for skin
	if ((thing->skin || thing->localskin) && thing->sprite == SPR_PLAY)
	{
		sprdef = &((skin_t *)( (thing->localskin) ? thing->localskin : thing->skin ))->spritedef;
#ifdef ROTSPRITE
		sprinfo = &((skin_t *)( (thing->localskin) ? thing->localskin : thing->skin ))->sprinfo;
#endif
	}
	else
	{
		sprdef = &sprites[thing->sprite];
#ifdef ROTSPRITE
		sprinfo = &spriteinfo[thing->sprite];
#endif
	}

	if (rot >= sprdef->numframes)
	{
		CONS_Alert(CONS_ERROR, M_GetText("HWR_ProjectSprite: invalid sprite frame %s/%s for %s\n"),
			sizeu1(rot), sizeu2(sprdef->numframes), sprnames[thing->sprite]);
		thing->sprite = states[S_UNKNOWN].sprite;
		thing->frame = states[S_UNKNOWN].frame;
		sprdef = &sprites[thing->sprite];
#ifdef ROTSPRITE
		sprinfo = &spriteinfo[thing->sprite];
#endif
		rot = thing->frame&FF_FRAMEMASK;
		thing->state->sprite = thing->sprite;
		thing->state->frame = thing->frame;
	}

	sprframe = &sprdef->spriteframes[rot];

#ifdef PARANOIA
	if (!sprframe)
		I_Error("sprframes NULL for sprite %d\n", thing->sprite);
#endif

	ang = R_PointToAngle(interp.x, interp.y) - interp.angle;
	camang = R_PointToAngle(interp.x, interp.y);

	if (mirrored)
		ang = InvAngle(ang);

	if (sprframe->rotate == SRF_SINGLE)
	{
		// use single rotation for all views
		rot = 0;                        //Fab: for vis->patch below
		lumpoff = sprframe->lumpid[0];     //Fab: see note above
		flip = sprframe->flip; // Will only be 0x00 or 0xFF

		if (papersprite && ang < ANGLE_180)
		{
			if (flip)
				flip = 0;
			else
				flip = 255;
		}
	}
	else
	{
		// choose a different rotation based on player view
		if ((ang < ANGLE_180) && (sprframe->rotate & SRF_RIGHT)) // See from right
			rot = 6; // F7 slot
		else if ((ang >= ANGLE_180) && (sprframe->rotate & SRF_LEFT)) // See from left
			rot = 2; // F3 slot
		else // Normal behaviour
			rot = (ang+ANGLE_202h)>>29;

		//Fab: lumpid is the index for spritewidth,spriteoffset... tables
		lumpoff = sprframe->lumpid[rot];
		flip = sprframe->flip & (1<<rot);

		if (papersprite && ang < ANGLE_180)
		{
			if (flip)
				flip = 0;
			else
				flip = 1<<rot;
		}
	}

	if (thing->skin && ((skin_t *)( (thing->localskin) ? thing->localskin : thing->skin ))->flags & SF_HIRES)
		this_scale = this_scale * FIXED_TO_FLOAT(((skin_t *)( (thing->localskin) ? thing->localskin : thing->skin ))->highresscale);

	spr_width = spritecachedinfo[lumpoff].width;
	spr_height = spritecachedinfo[lumpoff].height;
	spr_offset = spritecachedinfo[lumpoff].offset;
	spr_topoffset = spritecachedinfo[lumpoff].topoffset;

#ifdef ROTSPRITE
	if (cv_spriteroll.value)
	{
		rollangle = FixedMul(FINECOSINE((ang) >> ANGLETOFINESHIFT), interp.roll) 
			+ FixedMul(FINESINE((ang) >> ANGLETOFINESHIFT), interp.pitch)
			+ FixedMul(FINECOSINE((camang) >> ANGLETOFINESHIFT), interp.sloperoll) 
			+ FixedMul(FINESINE((camang) >> ANGLETOFINESHIFT), interp.slopepitch)
			+ thing->rollangle;

		if ((rollangle)||(thing->player && thing->player->sliproll))
		{
			if (thing->player)
			{
				sliptiderollangle = cv_sliptideroll.value ? thing->player->sliproll*(thing->player->sliptidemem) : 0;
				rollsum = rollangle+FixedMul(FINECOSINE((ang) >> ANGLETOFINESHIFT), sliptiderollangle);
			}
			else
				rollsum = rollangle;

			rollangle = R_GetRollAngle(rollsum);
			rotsprite = Patch_GetRotatedSprite(sprframe, (thing->frame & FF_FRAMEMASK), rot, flip, false, sprinfo, rollangle);

			if (rotsprite != NULL)
			{
				spr_width = rotsprite->width << FRACBITS;
				spr_height = rotsprite->height << FRACBITS;
				spr_offset = rotsprite->leftoffset << FRACBITS;
				spr_topoffset = rotsprite->topoffset << FRACBITS;
				spr_topoffset += FEETADJUST;
				
				// flip -> rotate, not rotate -> flip
				flip = 0;
			}
		}
	}
#endif

	if (thing->renderflags & RF_ABSOLUTEOFFSETS)
	{
		spr_offset = interp.spritexoffset;
		spr_topoffset = interp.spriteyoffset;
	}
	else
	{
		SINT8 flipoffset = 1;

		if ((thing->renderflags & RF_FLIPOFFSETS) && flip)
			flipoffset = -1;

		spr_offset += interp.spritexoffset * flipoffset;
		spr_topoffset += interp.spriteyoffset * flipoffset;
	}

	if (papersprite)
	{
		rightsin = FIXED_TO_FLOAT(FINESINE(interp.angle >> ANGLETOFINESHIFT));
		rightcos = FIXED_TO_FLOAT(FINECOSINE(interp.angle >> ANGLETOFINESHIFT));
	}
	else
	{
		rightsin = FIXED_TO_FLOAT(FINESINE((viewangle + ANGLE_90)>>ANGLETOFINESHIFT));
		rightcos = FIXED_TO_FLOAT(FINECOSINE((viewangle + ANGLE_90)>>ANGLETOFINESHIFT));
	}

	this_xscale = spritexscale * this_scale;
	this_yscale = spriteyscale * this_scale;
	
	flip = !flip != !hflip;

	if (flip)
	{
		x1 = (FIXED_TO_FLOAT(spr_width - spr_offset) * this_xscale);
		x2 = (FIXED_TO_FLOAT(spr_offset) * this_xscale);
	}
	else
	{
		x1 = (FIXED_TO_FLOAT(spr_offset) * this_xscale);
		x2 = (FIXED_TO_FLOAT(spr_width - spr_offset) * this_xscale);
	}

	z1 = tr_y + x1 * rightsin;
	z2 = tr_y - x2 * rightsin;
	x1 = tr_x + x1 * rightcos;
	x2 = tr_x - x2 * rightcos;


	if (thing->eflags & MFE_VERTICALFLIP)
	{
		gz = FIXED_TO_FLOAT(interp.z + thing->height) - (FIXED_TO_FLOAT(spr_topoffset) * this_yscale);
		gzt = gz + (FIXED_TO_FLOAT(spr_height) * this_yscale);
	}
	else
	{
		gzt = FIXED_TO_FLOAT(interp.z) + (FIXED_TO_FLOAT(spr_topoffset) * this_yscale);
		gz = gzt - (FIXED_TO_FLOAT(spr_height) * this_yscale);
	}

	if (thing->subsector->sector->cullheight)
	{
		if (HWR_DoCulling(thing->subsector->sector->cullheight, viewsector->cullheight, gr_viewz, gz, gzt))
			return;
	}

	heightsec = thing->subsector->sector->heightsec;
	if (viewplayer->mo && viewplayer->mo->subsector)
		phs = viewplayer->mo->subsector->sector->heightsec;
	else
		phs = -1;

	if (heightsec != -1 && phs != -1) // only clip things which are in special sectors
	{
		if (gr_viewz < FIXED_TO_FLOAT(sectors[phs].floorheight) ?
		FIXED_TO_FLOAT(interp.z) >= FIXED_TO_FLOAT(sectors[heightsec].floorheight) :
		gzt < FIXED_TO_FLOAT(sectors[heightsec].floorheight))
			return;
		if (gr_viewz > FIXED_TO_FLOAT(sectors[phs].ceilingheight) ?
		gzt < FIXED_TO_FLOAT(sectors[heightsec].ceilingheight) && gr_viewz >= FIXED_TO_FLOAT(sectors[heightsec].ceilingheight) :
		FIXED_TO_FLOAT(interp.z) >= FIXED_TO_FLOAT(sectors[heightsec].ceilingheight))
			return;
	}

	// store information in a vissprite
	vis = HWR_NewVisSprite();
	vis->x1 = x1;
	vis->x2 = x2;
	vis->z1 = z1;
	vis->z2 = z2;
	vis->tz = tz; // Keep tz for the simple sprite sorting that happens
	vis->dispoffset = thing->info->dispoffset; // Monster Iestyn: 23/11/15: HARDWARE SUPPORT AT LAST
	vis->flip = flip;

	vis->scale = this_scale;
	vis->spritexscale = spritexscale;
	vis->spriteyscale = spriteyscale;
	vis->spritexoffset = FIXED_TO_FLOAT(spr_offset);
	vis->spriteyoffset = FIXED_TO_FLOAT(spr_topoffset);

#ifdef ROTSPRITE
	if ((rotsprite) && (cv_spriteroll.value))
		vis->gpatch = (GLPatch_t *)rotsprite;
	else
#endif
			vis->gpatch = (GLPatch_t *)W_CachePatchNum(sprframe->lumppat[rot], PU_CACHE);

	vis->mobj = thing;


	//Hurdler: 25/04/2000: now support colormap in hardware mode
	if ((vis->mobj->flags & MF_BOSS) && (vis->mobj->flags2 & MF2_FRET) && (leveltime & 1)) // Bosses "flash"
	{
		if (vis->mobj->type == MT_CYBRAKDEMON)
			vis->colormap = R_GetTranslationColormap(TC_ALLWHITE, 0, GTC_CACHE);
		else if (vis->mobj->type == MT_METALSONIC_BATTLE)
			vis->colormap = R_GetTranslationColormap(TC_METALSONIC, 0, GTC_CACHE);
		else
			vis->colormap = R_GetTranslationColormap(TC_BOSS, 0, GTC_CACHE);
	}
	else if (thing->color)
	{
		// New colormap stuff for skins Tails 06-07-2002
		if (thing->colorized)
			vis->colormap = R_GetTranslationColormap(TC_RAINBOW, thing->color, GTC_CACHE);
		else if (thing->skin && thing->sprite == SPR_PLAY) // This thing is a player!
			vis->colormap = R_GetLocalTranslationColormap(thing->skin, thing->localskin, thing->color, GTC_CACHE, thing->skinlocal);
		else
			vis->colormap = R_GetTranslationColormap(TC_DEFAULT, thing->color, GTC_CACHE);
	}
	else
	{
		vis->colormap = colormaps;
#ifdef GLENCORE
		if (encoremap && !(thing->flags & MF_DONTENCOREMAP))
			vis->colormap += (256*32);
#endif
	}

	// set top/bottom coords
	vis->ty = gzt;

	vis->gzt = gzt;
	vis->gz = gz;

	//CONS_Debug(DBG_RENDER, "------------------\nH: sprite  : %d\nH: frame   : %x\nH: type    : %d\nH: sname   : %s\n\n",
	//            thing->sprite, thing->frame, thing->type, sprnames[thing->sprite]);

	if (thing->eflags & MFE_VERTICALFLIP)
		vis->vflip = true;
	else
		vis->vflip = false;

	vis->precip = false;
}

// Precipitation projector for hardware mode
void HWR_ProjectPrecipitationSprite(precipmobj_t *thing)
{
	gr_vissprite_t *vis;
	float tr_x, tr_y;
	float tz;
	float x1, x2;
	float z1, z2;
	float rightsin, rightcos;
	spritedef_t *sprdef;
	spriteframe_t *sprframe;
	size_t lumpoff;
	unsigned rot = 0;
	UINT8 flip;
	INT32 dist = -1;

	if (!thing)
		return;

	if (cv_grmaxinterpdist.value)
		dist = R_QuickCamDist(thing->x, thing->y);

	// uncapped/interpolation
	interpmobjstate_t interp = {0};

	// do interpolation
	if (R_UsingFrameInterpolation() && !paused && (!cv_grmaxinterpdist.value || dist < cv_grmaxinterpdist.value))
	{
		R_InterpolatePrecipMobjState(thing, rendertimefrac, &interp);
	}
	else
	{
		R_InterpolatePrecipMobjState(thing, FRACUNIT, &interp);
	}

	// transform the origin point
	tr_x = FIXED_TO_FLOAT(interp.x) - gr_viewx;
	tr_y = FIXED_TO_FLOAT(interp.y) - gr_viewy;

	// rotation around vertical axis
	tz = (tr_x * gr_viewcos) + (tr_y * gr_viewsin);

	// thing is behind view plane?
	if (tz < ZCLIP_PLANE)
		return;

	tr_x = FIXED_TO_FLOAT(interp.x);
	tr_y = FIXED_TO_FLOAT(interp.y);

	// decide which patch to use for sprite relative to player
	if ((unsigned)thing->sprite >= numsprites)
#ifdef RANGECHECK
		I_Error("HWR_ProjectPrecipitationSprite: invalid sprite number %i ",
		        thing->sprite);
#else
		return;
#endif

	sprdef = &sprites[thing->sprite];

	if ((size_t)(thing->frame&FF_FRAMEMASK) >= sprdef->numframes)
#ifdef RANGECHECK
		I_Error("HWR_ProjectPrecipitationSprite: invalid sprite frame %i : %i for %s",
		        thing->sprite, thing->frame, sprnames[thing->sprite]);
#else
		return;
#endif

	sprframe = &sprdef->spriteframes[ thing->frame & FF_FRAMEMASK];

	// use single rotation for all views
	lumpoff = sprframe->lumpid[0];
	flip = sprframe->flip; // Will only be 0x00 or 0xFF

	rightsin = FIXED_TO_FLOAT(FINESINE((viewangle + ANGLE_90)>>ANGLETOFINESHIFT));
	rightcos = FIXED_TO_FLOAT(FINECOSINE((viewangle + ANGLE_90)>>ANGLETOFINESHIFT));
	if (flip)
	{
		x1 = FIXED_TO_FLOAT(spritecachedinfo[lumpoff].width - spritecachedinfo[lumpoff].offset);
		x2 = FIXED_TO_FLOAT(spritecachedinfo[lumpoff].offset);
	}
	else
	{
		x1 = FIXED_TO_FLOAT(spritecachedinfo[lumpoff].offset);
		x2 = FIXED_TO_FLOAT(spritecachedinfo[lumpoff].width - spritecachedinfo[lumpoff].offset);
	}

	z1 = tr_y + x1 * rightsin;
	z2 = tr_y - x2 * rightsin;
	x1 = tr_x + x1 * rightcos;
	x2 = tr_x - x2 * rightcos;

	//
	// store information in a vissprite
	//
	vis = HWR_NewVisSprite();
	vis->x1 = x1;
	vis->x2 = x2;
	vis->z1 = z1;
	vis->z2 = z2;
	vis->tz = tz;
	vis->dispoffset = 0; // Monster Iestyn: 23/11/15: HARDWARE SUPPORT AT LAST
	vis->gpatch = (GLPatch_t *)W_CachePatchNum(sprframe->lumppat[rot], PU_CACHE);
	vis->flip = flip;
	vis->mobj = (mobj_t *)thing;

	vis->colormap = colormaps;

#ifdef GLENCORE
	if (encoremap && !(thing->flags & MF_DONTENCOREMAP))
		vis->colormap += (256*32);
#endif

	// set top/bottom coords
	vis->ty = FIXED_TO_FLOAT(interp.z + spritecachedinfo[lumpoff].topoffset);

	vis->gzt = FIXED_TO_FLOAT(interp.z + spritecachedinfo[lumpoff].topoffset);
	vis->gz = vis->gzt - FIXED_TO_FLOAT(spritecachedinfo[lumpoff].height);

	vis->precip = true;
	
	// okay... this is a hack, but weather isn't networked, so it should be ok
	if (!(thing->precipflags & PCF_THUNK))
	{
		if (thing->precipflags & PCF_RAIN)
			P_RainThinker(thing);
		else
			P_SnowThinker(thing);
		thing->precipflags |= PCF_THUNK;
	}
}

static boolean drewsky = false;

void HWR_DrawSkyBackground(float fpov)
{
	FTransform dometransform;

	if (drewsky)
		return;

	memset(&dometransform, 0x00, sizeof(FTransform));

	//04/01/2000: Hurdler: added for T&L
	//                     It should replace all other gr_viewxxx when finished
	if (!atransform.shearing)
		dometransform.anglex = (float)(aimingangle>>ANGLETOFINESHIFT)*(360.0f/(float)FINEANGLES);
	dometransform.angley = (float)((viewangle-ANGLE_270)>>ANGLETOFINESHIFT)*(360.0f/(float)FINEANGLES);

	dometransform.flip = atransform.flip;
	dometransform.mirror = atransform.mirror;
	dometransform.shearing = atransform.shearing;
	dometransform.viewaiming = atransform.viewaiming;

	dometransform.scalex = 1;
	dometransform.scaley = (float)vid.width/vid.height;
	dometransform.scalez = 1;
	dometransform.fovxangle = fpov; // Tails
	dometransform.fovyangle = fpov; // Tails
	dometransform.rollangle = atransform.rollangle;
	dometransform.roll = atransform.roll;
	dometransform.splitscreen = splitscreen;

	HWR_GetTexture(texturetranslation[skytexture], false);
	if (HWR_UseShader())
		HWD.pfnSetShader(HWR_GetShaderFromTarget(SHADER_SKY));// sky shader
	HWD.pfnRenderSkyDome(skytexture, textures[skytexture]->width, textures[skytexture]->height, dometransform);
}


// -----------------+
// HWR_ClearView : clear the viewwindow, with maximum z value. also clears stencil buffer.
// -----------------+
static inline void HWR_ClearView(void)
{
	HWD.pfnGClipRect((INT32)gr_viewwindowx,
	                 (INT32)gr_viewwindowy,
	                 (INT32)(gr_viewwindowx + gr_viewwidth),
	                 (INT32)(gr_viewwindowy + gr_viewheight),
	                 ZCLIP_PLANE, FAR_ZCLIP_DEFAULT);
	HWD.pfnClearBuffer(false, true, true, NULL);
}


// -----------------+
// HWR_SetViewSize  : set projection and scaling values
// -----------------+
void HWR_SetViewSize(void)
{
	// setup view size
	gr_viewwidth = (float)vid.width;
	gr_viewheight = (float)vid.height;

	if (splitscreen)
		gr_viewheight /= 2;

	if (splitscreen > 1)
		gr_viewwidth /= 2;

	gr_basecenterx = gr_viewwidth / 2;
	gr_basecentery = gr_viewheight / 2;

	gr_baseviewwindowy = 0;
	gr_basewindowcentery = (float)(gr_viewheight / 2);

	gr_baseviewwindowx = 0;
	gr_basewindowcenterx = (float)(gr_viewwidth / 2);

	gr_pspritexscale = ((vid.width*gr_pspriteyscale*BASEVIDHEIGHT)/BASEVIDWIDTH)/vid.height;
	gr_pspriteyscale = ((vid.height*gr_pspritexscale*BASEVIDWIDTH)/BASEVIDHEIGHT)/vid.width;

	HWD.pfnFlushScreenTextures();
}

void HWR_SetTransform(float fpov, player_t *player)
{
	postimg_t *postprocessor = &postimgtype[0];
	INT32 i;

	gr_viewx = FIXED_TO_FLOAT(viewx);
	gr_viewy = FIXED_TO_FLOAT(viewy);
	gr_viewz = FIXED_TO_FLOAT(viewz);
	gr_viewsin = FIXED_TO_FLOAT(viewsin);
	gr_viewcos = FIXED_TO_FLOAT(viewcos);

	memset(&atransform, 0x00, sizeof(FTransform));

	// Set T&L transform
	atransform.x = gr_viewx;
	atransform.y = gr_viewy;
	atransform.z = gr_viewz;

	atransform.scalex = 1;
	atransform.scaley = (float)vid.width/vid.height;
	atransform.scalez = 1;

	// 14042019
	gr_aimingangle = aimingangle;
	atransform.shearing = false;
	atransform.viewaiming = aimingangle;

	if (cv_grshearing.value)
	{
		gr_aimingangle = 0;
		atransform.shearing = true;
	}

	gr_viewludsin = FIXED_TO_FLOAT(FINECOSINE(gr_aimingangle>>ANGLETOFINESHIFT));
	gr_viewludcos = FIXED_TO_FLOAT(-FINESINE(gr_aimingangle>>ANGLETOFINESHIFT));

	atransform.anglex = (float)(gr_aimingangle>>ANGLETOFINESHIFT)*(360.0f/(float)FINEANGLES);
	atransform.angley = (float)(viewangle>>ANGLETOFINESHIFT)*(360.0f/(float)FINEANGLES);

	atransform.fovxangle = fpov; // Tails
	atransform.fovyangle = fpov; // Tails
	if (player->viewrollangle != 0)
	{
		fixed_t rol = AngleFixed(player->viewrollangle);
		atransform.rollangle = FIXED_TO_FLOAT(rol);
		atransform.roll = true;
	}
	atransform.splitscreen = splitscreen;

	for (i = 0; i <= splitscreen; i++)
	{
		if (player == &players[displayplayers[i]])
			postprocessor = &postimgtype[i];
	}

	atransform.flip = false;
	if (*postprocessor == postimg_flip)
		atransform.flip = true;

	atransform.mirror = false;
	if (*postprocessor == postimg_mirror)
		atransform.mirror = true;

	// Set transform.
	HWD.pfnSetTransform(&atransform);
}

void HWR_ClearClipper(void)
{
	angle_t a1 = gld_FrustumAngle(gr_aimingangle);
	gld_clipper_Clear();
	gld_clipper_SafeAddClipRange(viewangle + a1, viewangle - a1);
#ifdef HAVE_SPHEREFRUSTRUM
	gld_FrustumSetup();
#endif
}

// Adds an entry to the clipper for portal rendering
static void HWR_PortalClipping(gl_portal_t *portal)
{
	angle_t angle1, angle2;

	line_t *line = &lines[portal->clipline];

	if (cv_pointoangleexor64.value)
	{
		angle1 = R_PointToAngle64(line->v1->x, line->v1->y);
		angle2 = R_PointToAngle64(line->v2->x, line->v2->y);
	}
	else
	{
		angle1 = R_PointToAngleEx(viewx, viewy, line->v1->x, line->v1->y);
		angle2 = R_PointToAngleEx(viewx, viewy, line->v2->x, line->v2->y);
	}

	// clip things that are not inside the portal window from our viewpoint
	gld_clipper_SafeAddClipRange(angle2, angle1);
}

enum 
{
	HWR_STENCIL_NORMAL,
	HWR_STENCIL_BEGIN,
	HWR_STENCIL_REVERSE,
	HWR_STENCIL_DEPTH,
	HWR_STENCIL_SKY
};

// Changes the current stencil state.
static void HWR_SetStencilState(int state, int level)
{
	if (!cv_nostencil.value && level > -1)
		HWD.pfnSetSpecialState(HWD_SET_STENCIL_LEVEL, level);
	HWD.pfnSetSpecialState(HWD_SET_PORTAL_MODE, state);
}

// Renders a portal segment.
static void HWR_RenderPortalSeg(gl_portal_t* portal, int state)
{
	gl_drawing_stencil = true; // do not draw outside of the stencil buffer, idiot.
	// set our portal state and prepare to render the seg
	gr_portal = state;
	gr_curline = portal->seg;
	gr_frontsector = portal->seg->frontsector;
	gr_backsector = portal->seg->backsector;
	HWR_ProcessSeg();
	gl_drawing_stencil = false;
	// need to work around the r_opengl PF_Invisible bug with this call
	// similarly as in the linkdraw hack in HWR_DrawSprites
	HWD.pfnSetBlend(PF_Translucent|PF_Occlude|PF_Masked);
}

// Renders the current viewpoint, though takes portal arguments for recursive portals.
static void HWR_RenderViewpoint(gl_portal_t *rootportal, const float fpov, player_t *player, int stencil_level, boolean allow_portals);

// Renders a single portal from the current viewpoint.
static void HWR_RenderPortal(gl_portal_t* portal, gl_portal_t* rootportal, const float fpov, player_t *player, int stencil_level)
{
	// draw portal seg to stencil buffer with increment
	HWR_SetTransform(fpov, player);
	HWR_ClearClipper();

	HWR_SetStencilState(HWR_STENCIL_BEGIN, stencil_level);
	HWR_RenderPortalSeg(portal, GRPORTAL_STENCIL);

	// go to portal frame lmao
	HWR_PortalFrame(portal);
	// call HWR_RenderViewpoint
	HWR_RenderViewpoint(portal, fpov, player, stencil_level + 1, true);
	// return to current frame
	if (rootportal)
		HWR_PortalFrame(rootportal);
	else // current frame is not a portal frame but the main view!
	{
		R_SetupFrame(player, false);
		portalclipline = NULL;
	}

	// remove portal seg from stencil buffer
	HWR_SetTransform(fpov, player);
	HWR_ClearClipper();

	HWR_SetStencilState(HWR_STENCIL_REVERSE, stencil_level);
	HWR_RenderPortalSeg(portal, GRPORTAL_STENCIL);

	// draw portal seg to depth buffer
	HWR_ClearClipper();
	HWR_SetStencilState(HWR_STENCIL_DEPTH, stencil_level);
	HWR_RenderPortalSeg(portal, GRPORTAL_DEPTH);
}

// Renders the current viewpoint, though takes portal arguments for recursive portals.
static void HWR_RenderViewpoint(gl_portal_t *rootportal, const float fpov, player_t *player, int stencil_level, boolean allow_portals)
{
	gl_portallist_t portallist;
	gl_portal_t *portal;
	gl_portal_t *gl_portal_temp;
	portallist.base = portallist.cap = NULL;
	const boolean skybox = (skyboxmo[0] && cv_skybox.value);

	if (gr_maphasportals && allow_portals && cv_grportals.value && stencil_level < cv_maxportals.value)// if recursion limit is not reached
	{
		// search for portals in current frame
		currentportallist = &portallist;
		gr_portal = GRPORTAL_SEARCH;

		HWR_ClearClipper();
		if (!rootportal)
			portalclipline = NULL;
		else
			HWR_PortalClipping(rootportal);

		validcount++;

		HWR_RenderBSPNode((INT32)numnodes-1);// no actual rendering happens

		// for each found portal:
		// note: if necessary, could sort the portals here?
		for (portal = portallist.base; portal; portal = portal->next)
		{
			if (cv_portalline.value && cv_portalline.value != portal->startline)
				continue;
			HWR_RenderPortal(portal, rootportal, fpov, player, stencil_level);
		}
		gr_portal = GRPORTAL_INSIDE;// when portal walls are encountered in following bsp traversal, nothing should be drawn
	}
	else
		gr_portal = GRPORTAL_OFF;// there may be portals and they need to be drawn as regural walls
	// draw normal things in current frame in current incremented stencil buffer area
	HWR_SetStencilState(HWR_STENCIL_NORMAL, stencil_level);
	if (!cv_portalonly.value || rootportal)
	{	
		HWR_SetTransform(fpov, player);

		HWR_ClearClipper();
		HWR_ClearSprites();

		if (!rootportal)
			portalclipline = NULL;
		else
		{
			HWR_PortalFrame(rootportal);// for portalclipsector, it could have gone null from search
			HWR_PortalClipping(rootportal);
		}
		
		// Set transform.
		HWD.pfnSetTransform(&atransform);

		validcount++;
		
		if (cv_grbatching.value)
			HWR_StartBatching();

		ps_numbspcalls.value.i = 0;
		ps_numpolyobjects.value.i = 0;
		PS_START_TIMING(ps_bsptime);

		if (!rootportal && portallist.base && !skybox)// if portals have been drawn in the main view, then render skywalls differently
			gr_collect_skywalls = true;

		// HAYA: Save the old portal state, and turn portals off while normally rendering the BSP tree.
		// This fixes specific effects not working, such as horizon lines.
		int oldgl_portal_state = gr_portal;

		if (gr_portal != GRPORTAL_OFF) // if we already haven't hit the recursion limit or we already ended our portal shenanigans
			gr_portal = GRPORTAL_INSIDE; // TURN IT OFF
		// Recursively "render" the BSP tree.
		HWR_RenderBSPNode((INT32)numnodes-1);
		// woo we back
		gr_portal = oldgl_portal_state;

		PS_STOP_TIMING(ps_bsptime);
		
		if (cv_grbatching.value)
			HWR_RenderBatches();

		if (skyWallVertexArraySize)// if there are skywalls to draw using the alternate method
		{
			HWR_SetStencilState(HWR_STENCIL_SKY, -1);
			HWR_DrawSkyWallList();
			HWR_SkyWallList_Clear();
			HWR_SetStencilState(HWR_STENCIL_NORMAL, 1);
			drewsky = false;
			HWR_DrawSkyBackground(fpov);
			HWR_SetStencilState(HWR_STENCIL_NORMAL, 0);
			HWD.pfnClearBuffer(false, false, true, 0);// clear skywall markings from the stencil buffer
			HWR_SetTransform(fpov, player);// restore transform
		}
		gr_collect_skywalls = false;
		
		ps_numsprites.value.i = gr_visspritecount;
		PS_START_TIMING(ps_hw_spritesorttime);
		HWR_SortVisSprites();
		PS_STOP_TIMING(ps_hw_spritesorttime);
		PS_START_TIMING(ps_hw_spritedrawtime);
		HWR_DrawSprites();
		PS_STOP_TIMING(ps_hw_spritedrawtime);

		ps_numdrawnodes.value.i = 0;
		ps_hw_nodesorttime.value.p = 0;
		ps_hw_nodedrawtime.value.p = 0;
		HWR_RenderDrawNodes();
	}
	// free memory from portal list allocated by calls to Add2Lines
	gl_portal_temp = portallist.base;
	while (gl_portal_temp)
	{
		gl_portal_t *nextportal = gl_portal_temp->next;
		Z_Free(gl_portal_temp);
		gl_portal_temp = nextportal;
	}

	// TODO: batching at some point
	// TODO: is it okay if stencil test is on all the time even when its not needed?
}


// ==========================================================================
// Render the current frame.
// ==========================================================================
void HWR_RenderFrame(INT32 viewnumber, player_t *player, boolean skybox)
{
	const float fpov = FIXED_TO_FLOAT(cv_fov.value+player->fovadd);

	// set window position
	gr_centerx = gr_basecenterx;
	gr_viewwindowx = gr_baseviewwindowx;
	gr_windowcenterx = gr_basewindowcenterx;
	gr_centery = gr_basecentery;
	gr_viewwindowy = gr_baseviewwindowy;
	gr_windowcentery = gr_basewindowcentery;

	if ((splitscreen == 1 && viewnumber == 1) || (splitscreen > 1 && viewnumber > 1))
	{
		gr_viewwindowy += gr_viewheight;
		gr_windowcentery += gr_viewheight;
	}

	if (splitscreen > 1 && viewnumber & 1)
	{
		gr_viewwindowx += gr_viewwidth;
		gr_windowcenterx += gr_viewwidth;
	}

	if (splitscreen == 2 && player == &players[displayplayers[2]])
	{
		// V_DrawPatchFill, but for the fourth screen only
		GLPatch_t *gpatch = W_CachePatchName("SRB2BACK", PU_CACHE);
		INT32 dupz = (vid.dupx < vid.dupy ? vid.dupx : vid.dupy);
		INT32 x, y, pw = SHORT(gpatch->width) * dupz, ph = SHORT(gpatch->height) * dupz;

		for (x = vid.width>>1; x < vid.width; x += pw)
		{
			for (y = vid.height>>1; y < vid.height; y += ph)
				HWR_DrawStretchyFixedPatch(gpatch, (x)<<FRACBITS, (y)<<FRACBITS, FRACUNIT, FRACUNIT, V_NOSCALESTART, NULL);
		}
	}

	// check for new console commands.
	NetUpdate();

	// Clear view, set viewport (glViewport), set perspective...
	HWR_ClearView();

	ST_doPaletteStuff();

	// Draw the sky background.
	HWR_DrawSkyBackground(fpov);
	if (skybox)
		drewsky = true;

	current_bsp_culling_distance = 0;

	if (!skybox && cv_grrenderdistance.value)
	{
		HWD.pfnGClipRect((INT32)gr_viewwindowx,
	                 (INT32)gr_viewwindowy,
	                 (INT32)(gr_viewwindowx + gr_viewwidth),
	                 (INT32)(gr_viewwindowy + gr_viewheight),
	                 ZCLIP_PLANE, clipping_distances[cv_grrenderdistance.value - 1]);
		current_bsp_culling_distance = bsp_culling_distances[cv_grrenderdistance.value - 1];
	}

	portalclipline = NULL;
	HWR_RenderViewpoint(NULL, fpov, player, 0, !skybox);

	// Unset transform and shader
	HWD.pfnSetTransform(NULL);
	HWD.pfnUnSetShader();

	// Run post processor effects
	if (!skybox)
		HWR_DoPostProcessor(player);

	// Check for new console commands.
	NetUpdate();

	// added by Hurdler for correct splitscreen
	// moved here by hurdler so it works with the new near clipping plane
	HWD.pfnGClipRect(0, 0, vid.width, vid.height, NZCLIP_PLANE, FAR_ZCLIP_DEFAULT);
}

// ==========================================================================
// Render the player view.
// ==========================================================================
void HWR_RenderPlayerView(INT32 viewnumber, player_t *player)
{
	const boolean skybox = (skyboxmo[0] && cv_skybox.value); // True if there's a skybox object and skyboxes are on

	// Clear the color buffer, stops HOMs. Also seems to fix the skybox issue on Intel GPUs.
	if (viewnumber == 0) // Only do it if it's the first screen being rendered
	{
		FRGBAFloat ClearColor;

		ClearColor.red = 0.0f;
		ClearColor.green = 0.0f;
		ClearColor.blue = 0.0f;
		ClearColor.alpha = 1.0f;
		HWD.pfnClearBuffer(true, false, false, &ClearColor);
	}

	if (cv_grshaders.value)
		HWD.pfnSetShaderInfo(HWD_SHADERINFO_LEVELTIME, (INT32)leveltime); // The water surface shader needs the leveltime.

	if (viewnumber > 3)
		return;

	// Render the skybox if there is one.
	PS_START_TIMING(ps_skyboxtime);
	drewsky = false;
	if (skybox)
	{
		R_SkyboxFrame(player);
		HWR_RenderFrame(viewnumber, player, true);
	}
	PS_STOP_TIMING(ps_skyboxtime);

	R_SetupFrame(player, false); // This can stay false because it is only used to set viewsky in r_main.c, which isn't used here
	framecount++; // for timedemo
	HWR_RenderFrame(viewnumber, player, false);
}

// enable or disable palette rendering state depending on settings and availability
// called when relevant settings change
// shader recompilation is done in the cvar callback
void HWR_TogglePaletteRendering(void)
{
	// which state should we go to?
	if (HWR_ShouldUsePaletteRendering())
	{
		// are we not in that state already?
		if (!gr_palette_rendering_state)
		{
			gr_palette_rendering_state = true;

			// The textures will still be converted to RGBA by r_opengl.
			// This however makes hw_cache use paletted blending for composite textures!
			// (patchformat is not touched)
			textureformat = GL_TEXFMT_P_8;

			HWR_SetMapPalette();
			HWR_SetPalette(pLocalPalette);

			// If the r_opengl "texture palette" stays the same during this switch, these textures
			// will not be cleared out. However they are still out of date since the
			// composite texture blending method has changed. Therefore they need to be cleared.
			HWD.pfnClearMipMapCache();
		}
	}
	else
	{
		// are we not in that state already?
		if (gr_palette_rendering_state)
		{
			gr_palette_rendering_state = false;
			textureformat = GL_TEXFMT_RGBA;
			HWR_SetPalette(pLocalPalette);
			// If the r_opengl "texture palette" stays the same during this switch, these textures
			// will not be cleared out. However they are still out of date since the
			// composite texture blending method has changed. Therefore they need to be cleared.
			HWD.pfnClearMipMapCache();
		}
	}
}

//added by Hurdler: console varibale that are saved
void HWR_AddCommands(void)
{
	CV_RegisterVar(&cv_grframebuffer);
	CV_RegisterVar(&cv_grfiltermode);
	CV_RegisterVar(&cv_granisotropicmode);
	CV_RegisterVar(&cv_grsolvetjoin);

	CV_RegisterVar(&cv_grbatching);
	CV_RegisterVar(&cv_grfofcut);
	CV_RegisterVar(&cv_fofzfightfix);
	CV_RegisterVar(&cv_splitwallfix);
	CV_RegisterVar(&cv_slopepegfix);

	CV_RegisterVar(&cv_grscreentextures);

	CV_RegisterVar(&cv_grrenderdistance);

	CV_RegisterVar(&cv_grfakecontrast);
	CV_RegisterVar(&cv_grslopecontrast);
	CV_RegisterVar(&cv_grhorizonlines);

	CV_RegisterVar(&cv_grfovchange);

	CV_RegisterVar(&cv_grmdls);
	CV_RegisterVar(&cv_grfallbackplayermodel);

	CV_RegisterVar(&cv_grspritebillboarding);

	CV_RegisterVar(&cv_grshearing);

	CV_RegisterVar(&cv_grshaders);

	CV_RegisterVar(&cv_grportals);
	CV_RegisterVar(&cv_nostencil);
	CV_RegisterVar(&cv_portalline);
	CV_RegisterVar(&cv_portalonly);

	CV_RegisterVar(&cv_secbright);

	CV_RegisterVar(&cv_grpaletterendering);
	CV_RegisterVar(&cv_grpalettedepth);
	CV_RegisterVar(&cv_grflashpal);
}

// --------------------------------------------------------------------------
// Setup the hardware renderer
// --------------------------------------------------------------------------
void HWR_Startup(void)
{
	static boolean startupdone = false;

	// do this once
	if (!startupdone)
	{
		CONS_Printf("HWR_Startup()...\n");
		textureformat = patchformat = GL_TEXFMT_RGBA;
		
		HWR_InitTextureCache();
		HWR_InitMD2();

		gr_shadersavailable = HWR_InitShaders();
		HWR_SetShaderState();
		HWR_LoadAllCustomShaders();

		HWR_TogglePaletteRendering();
		
		HWD.pfnSetSpecialState(HWD_SET_FRAMEBUFFER, cv_grframebuffer.value);

		if (msaa)
		{
			if (a2c)
				HWD.pfnSetSpecialState(HWD_SET_MSAA, 2);
			else
				HWD.pfnSetSpecialState(HWD_SET_MSAA, 1);
		}
	}
	startupdone = true;
}

// --------------------------------------------------------------------------
// Free resources allocated by the hardware renderer
// --------------------------------------------------------------------------
void HWR_Shutdown(void)
{
	CONS_Printf("HWR_Shutdown()\n");
	HWR_FreeExtraSubsectors();
	HWR_FreeMipmapCache();
	HWR_FreeTextureCache();
	HWD.pfnFlushScreenTextures();
}

void HWR_RenderWall(FOutVector *wallVerts, FSurfaceInfo *pSurf, FBITFIELD blend, boolean fogwall, INT32 lightlevel, extracolormap_t *wallcolormap)
{
	FBITFIELD blendmode = blend;
	UINT8 alpha = pSurf->PolyColor.s.alpha; // retain the alpha
	
	INT32 shader = SHADER_NONE;

	// Lighting is done here instead so that fog isn't drawn incorrectly on transparent walls after sorting
	HWR_Lighting(pSurf, lightlevel, wallcolormap);

	pSurf->PolyColor.s.alpha = alpha; // put the alpha back after lighting

	if (blend & PF_Environment)
		blendmode |= PF_Occlude;	// PF_Occlude must be used for solid objects

	if (HWR_UseShader())
	{
		if (fogwall)
			shader = SHADER_FOG;
		else
			shader = SHADER_WALL;

		blendmode |= PF_ColorMapped;
	}
	
	if (fogwall)
		blendmode |= PF_Fog;
	
	blendmode |= PF_Modulated;	// No PF_Occlude means overlapping (incorrect) transparency
	HWR_ProcessPolygon(pSurf, wallVerts, 4, blendmode, shader, false);

#ifdef WALLSPLATS
	if (gr_curline->linedef->splats && cv_splats.value)
		HWR_DrawSegsSplats(pSurf);
#endif
}

INT32 HWR_GetTextureUsed(void)
{
	return HWD.pfnGetTextureUsed();
}

void HWR_DoPostProcessor(player_t *player)
{
	postimg_t *type = &postimgtype[0];
	UINT8 i;

	HWD.pfnUnSetShader();

	for (i = splitscreen; i > 0; i--)
	{
		if (player == &players[displayplayers[i]])
		{
			type = &postimgtype[i];
			break;
		}
	}

	// Armageddon Blast Flash!
	// Could this even be considered postprocessor?
	if ((player->flashcount) && (!HWR_PalRenderFlashpal()))
	{
		FOutVector      v[4];
		FSurfaceInfo Surf;

		v[0].x = v[2].y = v[3].x = v[3].y = -4.0f;
		v[0].y = v[1].x = v[1].y = v[2].x = 4.0f;
		v[0].z = v[1].z = v[2].z = v[3].z = 4.0f; // 4.0 because of the same reason as with the sky, just after the screen is cleared so near clipping plane is 3.99

		// This won't change if the flash palettes are changed unfortunately, but it works for its purpose
		if (player->flashpal == PAL_NUKE)
		{
			Surf.PolyColor.s.red = 0xff;
			Surf.PolyColor.s.green = Surf.PolyColor.s.blue = 0x7F; // The nuke palette is kind of pink-ish
		}
		else
			Surf.PolyColor.s.red = Surf.PolyColor.s.green = Surf.PolyColor.s.blue = 0xff;

		Surf.PolyColor.s.alpha = 0xc0; // match software mode

		HWD.pfnDrawPolygon(&Surf, v, 4, PF_Modulated|PF_Translucent|PF_NoTexture|PF_NoDepthTest);
	}

	if (!cv_grscreentextures.value) // screen textures are needed for the rest of the effects
		return;

	// Capture the screen for intermission and screen waving
	if(gamestate != GS_INTERMISSION)
		HWD.pfnMakeScreenTexture(HWD_SCREENTEXTURE_GENERIC1);

	if (splitscreen) // Not supported in splitscreen - someone want to add support?
		return;

	// Drunken vision! WooOOooo~
	if (*type == postimg_water || *type == postimg_heat)
	{
		// 10 by 10 grid. 2 coordinates (xy)
		float v[SCREENVERTS][SCREENVERTS][2];
		float disStart = (leveltime-1) + FIXED_TO_FLOAT(rendertimefrac);

		UINT8 x, y;
		INT32 WAVELENGTH;
		INT32 AMPLITUDE;
		INT32 FREQUENCY;

		// Modifies the wave.
		if (*type == postimg_water)
		{
			WAVELENGTH = 5;
			AMPLITUDE = 20;
			FREQUENCY = 8;
		}
		else
		{
			WAVELENGTH = 10;
			AMPLITUDE = 60;
			FREQUENCY = 4;
		}

		for (x = 0; x < SCREENVERTS; x++)
		{
			for (y = 0; y < SCREENVERTS; y++)
			{
				// Change X position based on its Y position.
				v[x][y][0] = (x/((float)(SCREENVERTS-1.0f)/9.0f))-4.5f + (float)sin((disStart+(y*WAVELENGTH))/FREQUENCY)/AMPLITUDE;
				v[x][y][1] = (y/((float)(SCREENVERTS-1.0f)/9.0f))-4.5f;
			}
		}
		HWD.pfnPostImgRedraw(v);

		// Capture the screen again for screen waving on the intermission
		if(gamestate != GS_INTERMISSION)
			HWD.pfnMakeScreenTexture(HWD_SCREENTEXTURE_GENERIC1);
	}
	// Flipping of the screen isn't done here anymore
}

void HWR_StartScreenWipe(void)
{
	HWD.pfnMakeScreenTexture(HWD_SCREENTEXTURE_WIPE_START);
}

void HWR_EndScreenWipe(void)
{
	HWD.pfnMakeScreenTexture(HWD_SCREENTEXTURE_WIPE_END);
}

void HWR_DrawIntermissionBG(void)
{
	HWD.pfnDrawScreenTexture(HWD_SCREENTEXTURE_GENERIC1, NULL, 0);
}

void HWR_DoWipe(UINT8 wipenum, UINT8 scrnnum)
{
	static char lumpname[9] = "FADEmmss";
	lumpnum_t lumpnum;
	size_t lsize;

	if (wipenum > 99 || scrnnum > 99) // not a valid wipe number
		return; // shouldn't end up here really, the loop should've stopped running beforehand

	// puts the numbers into the lumpname
	sprintf(&lumpname[4], "%.2hu%.2hu", (UINT16)wipenum, (UINT16)scrnnum);
	lumpnum = W_CheckNumForName(lumpname);

	if (lumpnum == LUMPERROR) // again, shouldn't be here really
		return;

	lsize = W_LumpLength(lumpnum);

	if (!(lsize == 256000 || lsize == 64000 || lsize == 16000 || lsize == 4000))
	{
		CONS_Alert(CONS_WARNING, "Fade mask lump %s of incorrect size, ignored\n", lumpname);
		return; // again, shouldn't get here if it is a bad size
	}

	HWR_GetFadeMask(lumpnum);
	HWD.pfnDoScreenWipe(HWD_SCREENTEXTURE_WIPE_START, HWD_SCREENTEXTURE_WIPE_END);
}

void HWR_RenderVhsEffect(fixed_t upbary, fixed_t downbary, UINT8 updistort, UINT8 downdistort, UINT8 barsize)
{
	HWD.pfnRenderVhsEffect(upbary, downbary, updistort, downdistort, barsize);
}

void HWR_MakeScreenFinalTexture(void)
{
	HWD.pfnMakeScreenTexture(HWD_SCREENTEXTURE_GENERIC2);
}

void HWR_DrawScreenFinalTexture(INT32 width, INT32 height)
{
	HWD.pfnDrawScreenFinalTexture(HWD_SCREENTEXTURE_GENERIC2, width, height);
}
#endif // HWRENDER
