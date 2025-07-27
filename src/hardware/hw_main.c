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

#ifdef HWRENDER

#include "hw_main.h"
#include "hw_glob.h"
#include "hw_gl.h"
#include "hw_batching.h"
#include "hw_md2.h"
#include "hw_clip.h"
#include "hw_portal.h"

#include "r_opengl/r_opengl.h"

#include "../d_clisrv.h"
#include "../g_game.h"
#include "../m_argv.h"		// parm functions for msaa
#include "../m_cheat.h"
#include "../m_menu.h"		// ogl menu updating

#include "../r_bsp.h"		// R_NoEncore
#include "../r_data.h"
#include "../r_fps.h"
#include "../r_local.h"
#include "../r_main.h"		// cv_fov
#include "../r_portal.h"
#ifdef WALLSPLATS
#include "../r_splats.h"
#endif
#include "../r_state.h"

#include "../i_system.h"
#include "../i_video.h"
#include "../p_local.h"
#include "../p_setup.h"
#include "../p_slopes.h"
#include "../st_stuff.h"
#include "../v_video.h"
#include "../w_wad.h"
#include "../z_zone.h"

#ifdef ROTSPRITE
#include "../r_patchrotation.h"		// a mystery as to what this is for
#endif

#include "../qs22j.h" 		// fast qsort

// ==========================================================================
// Globals
// ==========================================================================

#define ABS(x) ((x) < 0 ? -(x) : (x))
#define SOFTLIGHT(llevel) HWR_ShouldUsePaletteRendering() ? ((llevel) >> LIGHTSEGSHIFT) << LIGHTSEGSHIFT : (llevel)

// false if shaders have not been initialized yet, or if shaders are not available
boolean gl_shadersavailable = false;

// Whether the internal state is set to palette rendering or not.
static boolean gl_palette_rendering_state = false;

boolean gl_drawing_stencil = false;

static INT32 current_bsp_culling_distance = 0;

// base values set at SetViewSize
float gl_baseviewwindowy, gl_baseviewwindowx;
static float gl_viewwindowy, gl_viewwindowx; // top left corner of view window
float gl_viewwidth, gl_viewheight; // viewport clipping boundaries (screen coords)

FTransform atransform;

// Float variants of viewx, viewy, viewz, etc.
static float gl_viewx, gl_viewy, gl_viewz;
float gl_viewsin, gl_viewcos;
static float gl_viewludsin, gl_viewludcos;
static angle_t gl_aimingangle;

seg_t *gl_curline;
side_t *gl_sidedef;
line_t *gl_linedef;
sector_t *gl_frontsector;
sector_t *gl_backsector;

static boolean gl_maphashorizonlines = false;

// values for the far clipping plane
static float clipping_distances[] = {1024.0f, 2048.0f, 4096.0f, 6144.0f, 8192.0f, 12288.0f, 16384.0f};
// values for bsp culling
// slightly higher than the far clipping plane to compensate for impreciseness
static INT32 bsp_culling_distances[] = {(1024+512)*FRACUNIT, (2048+512)*FRACUNIT, (4096+512)*FRACUNIT,
	(6144+512)*FRACUNIT, (8192+512)*FRACUNIT, (12288+512)*FRACUNIT, (16384+512)*FRACUNIT};

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

static void HWR_SplitWall(sector_t *sector, FOutVector *wallVerts, INT32 texnum, boolean noencore, FSurfaceInfo* Surf, INT32 cutflag, ffloor_t *pfloor, FBITFIELD polyflags);
static void HWR_RenderWall(FOutVector *wallVerts, FSurfaceInfo *pSurf, FBITFIELD blend, boolean fogwall, INT32 lightlevel, extracolormap_t *wallcolormap);

static void HWR_AddTransparentFloor(lumpnum_t lumpnum, extrasubsector_t *xsub, boolean isceiling, fixed_t fixedheight, INT32 lightlevel, INT32 alpha, sector_t *FOFSector, FBITFIELD blend, boolean fogplane, extracolormap_t *planecolormap);
static void HWR_AddTransparentWall(FOutVector *wallVerts, FSurfaceInfo *pSurf, INT32 texnum, boolean noencore, FBITFIELD blend, boolean fogwall, INT32 lightlevel, extracolormap_t *wallcolormap);
static void HWR_AddTransparentPolyobjectFloor(lumpnum_t lumpnum, polyobj_t *polysector, boolean isceiling, fixed_t fixedheight, INT32 lightlevel, INT32 alpha, sector_t *FOFSector, FBITFIELD blend, extracolormap_t *planecolormap);

static void HWR_AddSprites(sector_t *sec);
static void HWR_ProjectSprite(mobj_t *thing);
static void HWR_AddPrecipitationSprites(void);
static void HWR_ProjectPrecipitationSprite(precipmobj_t *thing);

static void HWR_SetTransformAiming(FTransform *trans);
static void HWR_RollTransform(FTransform *tr, angle_t roll);

static void HWR_DoPostProcessor(player_t *player);

static void HWR_SetShaderState(void);
static void HWR_TogglePaletteRendering(void);

// ==========================================================================
// Commands and console variables
// ==========================================================================
static void HWR_RegisterCommands(void);
static void COM_HWR_glinfo(void);

//
// Onchanges
//

static void CV_screentextures_OnChange(void);
#ifdef USE_FBO_OGL
static void CV_glframebuffer_OnChange(void);
#endif
static void CV_glshaders_OnChange(void);
static void CV_gllightdithering_OnChange(void);
static void CV_filtermode_OnChange(void);
static void CV_anisotropic_OnChange(void);
static void CV_gltextureformat_OnChange(void);
static void CV_glpaletterendering_OnChange(void);
static void CV_glpalettedepth_OnChange(void);

//
// CV_PossibleValue_t
//

static CV_PossibleValue_t glscreentextures_cons_t[] = {{0, "Off"}, {1, "Wipes Only"}, {2, "All"}, {0, NULL}};
static CV_PossibleValue_t glfakecontrast_cons_t[] = {{0, "Off"}, {1, "Standard"}, {2, "Smooth"}, {0, NULL}};
static CV_PossibleValue_t glshaders_cons_t[] = {{0, "Off"}, {1, "On"}, {2, "Ignore custom shaders"}, {0, NULL}};
static CV_PossibleValue_t secbright_cons_t[] = {{0, "MIN"}, {255, "MAX"}, {0, NULL}};

static CV_PossibleValue_t glfiltermode_cons_t[]= {{HWD_SET_TEXTUREFILTER_POINTSAMPLED, "Nearest"},
	{HWD_SET_TEXTUREFILTER_BILINEAR, "Bilinear"}, {HWD_SET_TEXTUREFILTER_TRILINEAR, "Trilinear"},
	{HWD_SET_TEXTUREFILTER_MIXED1, "Linear_Nearest"}, {HWD_SET_TEXTUREFILTER_MIXED2, "Nearest_Linear"},
	{HWD_SET_TEXTUREFILTER_MIXED3, "Nearest_Mipmap"}, {0, NULL}};
CV_PossibleValue_t glanisotropicmode_cons_t[] = {{1, "MIN"}, {16, "MAX"}, {0, NULL}};

static CV_PossibleValue_t glrenderdistance_cons_t[] = {
	{0, "Max"}, {1, "1024"}, {2, "2048"}, {3, "4096"}, {4, "6144"}, {5, "8192"},
	{6, "12288"}, {7, "16384"}, {0, NULL}};

static CV_PossibleValue_t gltexdepth_cons_t[] = {{16, "16 bits"}, {32, "32 bits"}, {0, NULL}};

static CV_PossibleValue_t glpalettedepth_cons_t[] = {{16, "16 bits"}, {24, "24 bits"}, {0, NULL}};

//
// console variables
//

// The current screen texture implementation is inefficient and disabling it can result in significant
// performance gains on lower end hardware. The game is still quite playable without this functionality.
// Features that break when disabling this:
//  - water and heat wave effects
//  - intermission background
//  - full screen scaling (use native resolution or windowed mode to avoid this)
consvar_t cv_glscreentextures = {"gr_screentextures", "All", CV_CALL|CV_SAVE, glscreentextures_cons_t, CV_screentextures_OnChange, 0, NULL, NULL, 0, 0, NULL};

#ifdef USE_FBO_OGL
consvar_t cv_glframebuffer = {"gr_framebuffer", "Off", CV_SAVE|CV_CALL|CV_NOINIT, CV_OnOff, CV_glframebuffer_OnChange, 0, NULL, NULL, 0, 0, NULL};
#endif

consvar_t cv_glmdls = {"gr_mdls", "Off", CV_SAVE|CV_CALL, CV_OnOff, M_UpdateOGLMenu, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_glfallbackplayermodel = {"gr_fallbackplayermodel", "Off", CV_SAVE, CV_OnOff, NULL, 0, NULL, NULL, 0, 0, NULL};

consvar_t cv_glspritebillboarding = {"gr_spritebillboarding", "On", CV_SAVE, CV_OnOff, NULL, 0, NULL, NULL, 0, 0, NULL};

consvar_t cv_glshearing = {"gr_shearing", "Off", CV_SAVE, CV_OnOff, NULL, 0, NULL, NULL, 0, 0, NULL};

consvar_t cv_glfakecontrast = {"gr_fakecontrast", "Standard", CV_SAVE, glfakecontrast_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_glslopecontrast = {"gr_slopecontrast", "Off", CV_SAVE, CV_OnOff, NULL, 0, NULL, NULL, 0, 0, NULL};

consvar_t cv_glshaders = {"gr_shaders", "On", CV_CALL|CV_SAVE, glshaders_cons_t, CV_glshaders_OnChange, 0, NULL, NULL, 0, 0, NULL};

consvar_t cv_gllightdither = {"gr_lightdithering", "Off", CV_CALL|CV_SAVE, CV_OnOff, CV_gllightdithering_OnChange, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_glsecbright = {"secbright", "0", CV_SAVE, secbright_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL};

consvar_t cv_glfiltermode = {"gr_filtermode", "Nearest", CV_CALL|CV_SAVE, glfiltermode_cons_t, CV_filtermode_OnChange, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_glanisotropicmode = {"gr_anisotropicmode", "1", CV_CALL|CV_SAVE, glanisotropicmode_cons_t, CV_anisotropic_OnChange, 0, NULL, NULL, 0, 0, NULL};

consvar_t cv_glsolvetjoin = {"gr_solvetjoin", "On", 0, CV_OnOff, NULL, 0, NULL, NULL, 0, 0, NULL};

consvar_t cv_glbatching = {"gr_batching", "On", 0, CV_OnOff, NULL, 0, NULL, NULL, 0, 0, NULL};

consvar_t cv_glrenderdistance = {"gr_renderdistance", "Max", CV_SAVE, glrenderdistance_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL};

consvar_t cv_glhorizonlines = {"gr_horizonlines", "On", CV_SAVE, CV_OnOff, NULL, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_glportals = {"gr_portals", "On", CV_SAVE, CV_OnOff, NULL, 0, NULL, NULL, 0, 0, NULL};

consvar_t cv_glpaletterendering = {"gr_paletteshader", "Off", CV_CALL|CV_SAVE, CV_OnOff, CV_glpaletterendering_OnChange, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_glpalettedepth = {"gr_palettedepth", "16 bits", CV_SAVE|CV_CALL, glpalettedepth_cons_t, CV_glpalettedepth_OnChange, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_glflashpal = {"gr_flashpal", "On", CV_CALL|CV_SAVE, CV_OnOff, CV_glpaletterendering_OnChange, 0, NULL, NULL, 0, 0, NULL};

consvar_t cv_gltexturedepth = {"gr_texturedepth", "32 bits", CV_CALL|CV_SAVE, gltexdepth_cons_t, CV_gltextureformat_OnChange, 0, NULL, NULL, 0, 0, NULL};

#define ONLY_IF_GL_LOADED if (vid.glstate != VID_GL_LIBRARY_LOADED) return;

static void CV_screentextures_OnChange(void)
{
	ONLY_IF_GL_LOADED
	if (cv_glscreentextures.value != 2)
	{
		if (cv_glpaletterendering.value != 0)
			CV_SetValue(&cv_glpaletterendering, 0);

#ifdef USE_FBO_OGL
		if (cv_glframebuffer.value != 0)
			CV_SetValue(&cv_glframebuffer, 0);
#endif
	}
	GL_SetSpecialState(HWD_SET_SCREEN_TEXTURES, cv_glscreentextures.value);
	M_UpdateOGLMenu();
}

#ifdef USE_FBO_OGL
static void CV_glframebuffer_OnChange(void)
{
	ONLY_IF_GL_LOADED
	if ((cv_glframebuffer.value != 0 && cv_glscreentextures.value != 2) || (!supportFBO && cv_glframebuffer.value != 0)) // screen FBO needs screen textures
		CV_SetValue(&cv_glframebuffer, 0);

	I_DownSample();
	RefreshOGLSDLSurface();
	M_UpdateOGLMenu();
}
#endif

static void CV_glshaders_OnChange(void)
{
	ONLY_IF_GL_LOADED
	HWR_SetShaderState();
	if (cv_glpaletterendering.value)
	{
		// can't do palette rendering without shaders, so update the state if needed
		HWR_TogglePaletteRendering();
	}
	M_UpdateOGLMenu();
}

static void CV_gllightdithering_OnChange(void)
{
	ONLY_IF_GL_LOADED
	if (gl_shadersavailable)
	{
		HWR_CompileShaders();
	}
}

static void CV_gltextureformat_OnChange(void)
{
	ONLY_IF_GL_LOADED
	GL_SetSpecialState(HWD_SET_TEXTURE_FORMAT, cv_gltexturedepth.value);
}

static void CV_filtermode_OnChange(void)
{
	ONLY_IF_GL_LOADED
	GL_SetSpecialState(HWD_SET_TEXTUREFILTERMODE, cv_glfiltermode.value);
}

static void CV_anisotropic_OnChange(void)
{
	ONLY_IF_GL_LOADED
	GL_SetSpecialState(HWD_SET_TEXTUREANISOTROPICMODE, cv_glanisotropicmode.value);
}

static void CV_glpaletterendering_OnChange(void)
{
	ONLY_IF_GL_LOADED
	if (cv_glpaletterendering.value != 0 && cv_glscreentextures.value != 2) // can't do palette rendering without screen textures
		CV_SetValue(&cv_glpaletterendering, 0);

	if (gl_shadersavailable)
	{
		HWR_CompileShaders();
		HWR_TogglePaletteRendering();
	}
	M_UpdateOGLMenu();
}

static void CV_glpalettedepth_OnChange(void)
{
	ONLY_IF_GL_LOADED
	if (HWR_ShouldUsePaletteRendering())
		HWR_SetPalette(pLocalPalette);
}

// ==========================================================================
// Lighting
// ==========================================================================

static void HWR_SetShaderState(void)
{
	GL_SetSpecialState(HWD_SET_SHADERS, HWR_UseShader() ? 1 : 0);
}

static boolean HWR_OverrideObjectLightLevel(mobj_t *thing, INT32 *lightlevel)
{
	if (R_ThingIsFullBright(thing))
		*lightlevel = 255;
	else if (R_ThingIsFullDark(thing))
		*lightlevel = 0;
	else if (thing->frame & FF_ABSOLUTELIGHTLEVEL)
		*lightlevel = R_ThingLightLevel(thing);
	else
		return false;

	return true;
}

void HWR_ObjectLightLevelPost(gl_vissprite_t *spr, const sector_t *sector, INT32 *lightlevel, boolean model)
{
	const boolean semibright = R_ThingIsSemiBright(spr->mobj);

	*lightlevel += R_ThingLightLevel(spr->mobj);

	if (maplighting.directional == true && P_SectorUsesDirectionalLighting(sector))
	{
		if (model == false) // this is implemented by shader
		{
			fixed_t extralight = R_GetSpriteDirectionalLighting(R_PointToAngle(spr->mobj->x, spr->mobj->y));

			// Less change in contrast in dark sectors
			extralight = FixedMul(extralight, min(max(0, *lightlevel), 255) * FRACUNIT / 255);

			// simple OGL approximation
			fixed_t tr = R_PointToDist(spr->mobj->x, spr->mobj->y);
			fixed_t xscale = FixedDiv((vid.width / 2) << FRACBITS, tr);

			// Less change in contrast at further distances, to counteract DOOM diminished light
			fixed_t n = FixedDiv(FixedMul(xscale, LIGHTRESOLUTIONFIX), ((MAXLIGHTSCALE-1) << LIGHTSCALESHIFT));
			extralight = FixedMul(extralight, min(n, FRACUNIT));

			// Contrast is stronger for normal sprites, stronger than wall lighting is at the same distance
			*lightlevel += FixedFloor((extralight * 2) + (FRACUNIT / 2)) / FRACUNIT;
		}

		// Semibright objects will be made slightly brighter to compensate contrast
		if (semibright)
		{
			*lightlevel += 16;
		}
	}

	if (semibright)
	{
		*lightlevel = 128 + (*lightlevel >> 1);
	}
}

void HWR_Lighting(FSurfaceInfo *Surface, INT32 light_level, extracolormap_t *colormap, const boolean directional)
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
	light_level = CLAMP(SOFTLIGHT(light_level), cv_glsecbright.value, 255);

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
	Surface->LightInfo.directional = (maplighting.directional == true && directional == true);
	Surface->LightTableId = HWR_ShouldUsePaletteRendering() ? HWR_GetLightTableID(colormap) : 0;
}

static UINT8 HWR_FogBlockAlpha(INT32 light, extracolormap_t *colormap) // Let's see if this can work
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
		// Don't go out of bounds
		light = CLAMP(light - (255 - light), 0, 255);

		alpha = (realcolor.s.alpha*255)/25;

		// at 255 brightness, alpha is between 0 and 127, at 0 brightness alpha will always be 255
		surfcolor.s.alpha = (alpha*light) / (2*256) + 255-light;
	}

	return surfcolor.s.alpha;
}

static FUINT HWR_CalcWallLight(FUINT lightnum, seg_t *seg, extracolormap_t *colormap)
{
	INT16 finallight = lightnum;

	if (cv_glfakecontrast.value == 0 || (HWR_ShouldUsePaletteRendering() && colormap))
		return (FUINT)finallight;

	if (seg != NULL && P_ApplyLightOffsetFine(lightnum, seg->frontsector))
	{
		INT16 offset = (cv_glfakecontrast.value == 2) ? seg->hwLightOffset : ((INT16)seg->lightOffset * 8);

		finallight += offset;
		finallight = CLAMP(finallight, 0 , 255);
	}

	return (FUINT)finallight;
}

static FUINT HWR_CalcSlopeLight(FUINT lightnum, pslope_t *slope, const sector_t *sector, const boolean fof)
{
	INT16 finallight = lightnum;

	if (cv_glfakecontrast.value == 0 || cv_glslopecontrast.value == 0)
		return (FUINT)finallight;

	if (slope != NULL && sector != NULL && P_ApplyLightOffsetFine(lightnum, sector))
	{
		INT16 offset = (cv_glfakecontrast.value == 2) ? slope->hwLightOffset : ((INT16)slope->lightOffset * 8);

		finallight += (fof ? -offset : offset);
		finallight = CLAMP(finallight, 0 , 255);
	}

	return (FUINT)finallight;
}

// ==========================================================================
// Floor and ceiling generation from subsectors
// ==========================================================================

// HWR_RenderPlane
// Render a floor or ceiling convex polygon
static void HWR_RenderPlane(subsector_t *subsector, extrasubsector_t *xsub, boolean isceiling, fixed_t fixedheight, FBITFIELD PolyFlags, INT32 lightlevel, lumpnum_t lumpnum, sector_t *FOFsector, UINT8 alpha, extracolormap_t *planecolormap)
{
	FSurfaceInfo Surf;
	FOutVector *v3d;
	polyvertex_t *pv;
	pslope_t *slope = NULL;
	INT32 shader = SHADER_NONE;

	size_t nrPlaneVerts;
	INT32 i;

	float height; // constant y for all points on the convex flat polygon
	float flatxref, flatyref = 0.0f;
	float fflatsize = 64.0f;
	INT32 flatflag = 63;
	size_t len;

	float tempxsow, tempytow;
	float scrollx = 0.0f, scrolly = 0.0f;
	angle_t angle = 0;

	static FOutVector *planeVerts = NULL;
	static UINT16 numAllocedPlaneVerts = 0;

	poly_t *planepoly = xsub->planepoly;

	// no convex poly were generated for this subsector
	if (!planepoly)
		return;

	nrPlaneVerts = planepoly->numpts;

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
		if (gl_frontsector->f_slope && !isceiling)
			slope = gl_frontsector->f_slope;
		else if (gl_frontsector->c_slope && isceiling)
			slope = gl_frontsector->c_slope;
	}

	// Set fixedheight to the slope's height from our viewpoint, if we have a slope
	if (slope)
		fixedheight = P_GetSlopeZAt(slope, viewx, viewy);

	height = FixedToFloat(fixedheight);

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

	pv = planepoly->pts;

	// reference point for flat texture coord for each vertex around the polygon
	flatxref = (float)(((fixed_t)pv->x & (~flatflag)) / fflatsize);
	flatyref = (float)(((fixed_t)pv->y & (~flatflag)) / fflatsize);

	if (FOFsector != NULL)
	{
		if (!isceiling) // it's a floor
		{
			scrollx = FixedToFloat(FOFsector->floor_xoffs)/fflatsize;
			scrolly = FixedToFloat(FOFsector->floor_yoffs)/fflatsize;
			angle = FOFsector->floorpic_angle;
		}
		else // it's a ceiling
		{
			scrollx = FixedToFloat(FOFsector->ceiling_xoffs)/fflatsize;
			scrolly = FixedToFloat(FOFsector->ceiling_yoffs)/fflatsize;
			angle = FOFsector->ceilingpic_angle;
		}
	}
	else if (gl_frontsector)
	{
		if (!isceiling) // it's a floor
		{
			scrollx = FixedToFloat(gl_frontsector->floor_xoffs)/fflatsize;
			scrolly = FixedToFloat(gl_frontsector->floor_yoffs)/fflatsize;
			angle = gl_frontsector->floorpic_angle;
		}
		else // it's a ceiling
		{
			scrollx = FixedToFloat(gl_frontsector->ceiling_xoffs)/fflatsize;
			scrolly = FixedToFloat(gl_frontsector->ceiling_yoffs)/fflatsize;
			angle = gl_frontsector->ceilingpic_angle;
		}
	}

	if (angle) // Only needs to be done if there's an altered angle
	{
		angle = InvAngle(angle)>>ANGLETOFINESHIFT;

		// This needs to be done so everything aligns after rotation
		// It would be done so that rotation is done, THEN the translation, but I couldn't get it to rotate AND scroll like software does
		tempxsow = FloatToFixed(flatxref);
		tempytow = FloatToFixed(flatyref);
		flatxref = (FixedToFloat(FixedMul(tempxsow, FINECOSINE(angle)) - FixedMul(tempytow, FINESINE(angle))));
		flatyref = (FixedToFloat(FixedMul(tempxsow, FINESINE(angle)) + FixedMul(tempytow, FINECOSINE(angle))));
	}

#define SETUP3DVERT(vert, vx, vy) {\
		/* Hurdler: add scrolling texture on floor/ceiling */\
		vert->s = (float)(((vx) / fflatsize) - flatxref + scrollx);\
		vert->t = (float)(flatyref - ((vy) / fflatsize) + scrolly);\
\
		/* Need to rotate before translate */\
		if (angle) /* Only needs to be done if there's an altered angle */\
		{\
			tempxsow = FloatToFixed(vert->s);\
			tempytow = FloatToFixed(vert->t);\
			vert->s = (FixedToFloat(FixedMul(tempxsow, FINECOSINE(angle)) - FixedMul(tempytow, FINESINE(angle))));\
			vert->t = (FixedToFloat(FixedMul(tempxsow, FINESINE(angle)) + FixedMul(tempytow, FINECOSINE(angle))));\
		}\
\
		vert->x = (vx);\
		vert->y = height;\
		vert->z = (vy);\
\
		if (slope)\
		{\
			fixedheight = P_GetSlopeZAt(slope, FloatToFixed((vx)), FloatToFixed((vy)));\
			vert->y = FixedToFloat(fixedheight);\
		}\
}
	for (i = 0, v3d = planeVerts; i < (INT32)nrPlaneVerts; i++,v3d++,pv++)
		SETUP3DVERT(v3d, pv->x, pv->y);

	lightlevel = HWR_CalcSlopeLight(lightlevel, slope, gl_frontsector, (FOFsector != NULL));

	HWR_Lighting(&Surf, lightlevel, planecolormap, P_SectorUsesDirectionalLighting(gl_frontsector));

	if (PolyFlags & (PF_Translucent|PF_Fog|PF_Additive|PF_Subtractive|PF_ReverseSubtract|PF_Multiplicative|PF_Environment))
	{
		Surf.PolyColor.s.alpha = (UINT8)alpha;
		PolyFlags |= PF_Modulated;
	}
	else
		PolyFlags |= PF_Masked|PF_Modulated;

	if (HWR_UseShader())
	{
		if (PolyFlags & PF_Fog)
			shader = SHADER_FOG;
		else if (cv_ripplewater.value && (PolyFlags & PF_Ripple))
			shader = SHADER_WATER;
		else
			shader = SHADER_FLOOR;

		PolyFlags |= PF_ColorMapped;
	}

	HWR_ProcessPolygon(&Surf, planeVerts, nrPlaneVerts, PolyFlags, shader, false);

	if (gl_maphashorizonlines && subsector && cv_glhorizonlines.value)
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
			if (line->linedef->special != HORIZONSPECIAL)
				continue;

			if (R_PointOnSegSide(viewx, viewy, line) != 0)
				continue;

			P_ClosestPointOnLine(viewx, viewy, line->linedef, &v);
			dist = FixedToFloat(R_PointToDist(v.x, v.y));

			if (line->pv1)
			{
				x1 = ((polyvertex_t *)line->pv1)->x;
				y1 = ((polyvertex_t *)line->pv1)->y;
			}
			else
			{
				x1 = FixedToFloat(line->v1->x);
				y1 = FixedToFloat(line->v1->x);
			}

			if (line->pv2)
			{
				xd = ((polyvertex_t *)line->pv2)->x - x1;
				yd = ((polyvertex_t *)line->pv2)->y - y1;
			}
			else
			{
				xd = FixedToFloat(line->v2->x) - x1;
				yd = FixedToFloat(line->v2->y) - y1;
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

				dist = sqrtf(powf(vx - gl_viewx, 2) + powf(vy - gl_viewy, 2));
				vx = (vx - gl_viewx) * renderdist / dist + gl_viewx;
				vy = (vy - gl_viewy) * renderdist / dist + gl_viewy;
				SETUP3DVERT((&horizonpts[0]), vx, vy);

				// Right side
				vx = x1 + xd * (j+1) / numplanes;
				vy = y1 + yd * (j+1) / numplanes;
				SETUP3DVERT((&horizonpts[2]), vx, vy);

				dist = sqrtf(powf(vx - gl_viewx, 2) + powf(vy - gl_viewy, 2));
				vx = (vx - gl_viewx) * renderdist / dist + gl_viewx;
				vy = (vy - gl_viewy) * renderdist / dist + gl_viewy;
				SETUP3DVERT((&horizonpts[3]), vx, vy);

				// Horizon fills
				vx = (horizonpts[0].x - gl_viewx) * farrenderdist / renderdist + gl_viewx;
				vy = (horizonpts[0].z - gl_viewy) * farrenderdist / renderdist + gl_viewy;
				SETUP3DVERT((&horizonpts[5]), vx, vy);
				horizonpts[5].y = gl_viewz;

				vx = (horizonpts[3].x - gl_viewx) * farrenderdist / renderdist + gl_viewx;
				vy = (horizonpts[3].z - gl_viewy) * farrenderdist / renderdist + gl_viewy;
				SETUP3DVERT((&horizonpts[4]), vx, vy);
				horizonpts[4].y = gl_viewz;

				// Draw
				HWR_ProcessPolygon(&Surf, horizonpts, 6, PolyFlags, shader, true);
			}
		}
	}
}

#ifdef WALLSPLATS
static void HWR_DrawSegsSplats(FSurfaceInfo * pSurf)
{
	FOutVector wallVerts[4];
	wallsplat_t *splat;
	patch_t *gpatch;
	fixed_t i;
	// seg bbox
	fixed_t segbbox[4];

	INT32 shader = SHADER_NONE;

	M_ClearBox(segbbox);
	M_AddToBox(segbbox,
		FloatToFixed(((polyvertex_t *)gl_curline->pv1)->x),
		FloatToFixed(((polyvertex_t *)gl_curline->pv1)->y));
	M_AddToBox(segbbox,
		FloatToFixed(((polyvertex_t *)gl_curline->pv2)->x),
		FloatToFixed(((polyvertex_t *)gl_curline->pv2)->y));

	splat = (wallsplat_t *)gl_curline->linedef->splats;
	for (; splat; splat = splat->next)
	{
		//BP: don't draw splat extern to this seg
		//    this is quick fix best is explain in logboris.txt at 12-4-2000
		if (!M_PointInBox(segbbox,splat->v1.x,splat->v1.y) && !M_PointInBox(segbbox,splat->v2.x,splat->v2.y))
			continue;

		gpatch = W_CachePatchNum(splat->patch, PU_SPRITE);
		HWR_GetPatch(gpatch);

		wallVerts[0].x = wallVerts[3].x = FixedToFloat(splat->v1.x);
		wallVerts[0].z = wallVerts[3].z = FixedToFloat(splat->v1.y);
		wallVerts[2].x = wallVerts[1].x = FixedToFloat(splat->v2.x);
		wallVerts[2].z = wallVerts[1].z = FixedToFloat(splat->v2.y);

		i = splat->top;
		if (splat->yoffset)
			i += *splat->yoffset;

		wallVerts[2].y = wallVerts[3].y = FixedToFloat(i)+(gpatch->height>>1);
		wallVerts[0].y = wallVerts[1].y = FixedToFloat(i)-(gpatch->height>>1);

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
			shader = SHADER_WALL;

		HWR_ProcessPolygon(&pSurf, wallVerts, 4, i|PF_Modulated|PF_Decal, shader, false);
	}
}
#endif

FBITFIELD HWR_GetBlendModeFlag(INT32 style)
{
	switch (style)
	{
		case AST_TRANSLUCENT:
			return PF_Translucent;
		case AST_ADD:
			return PF_Additive;
		case AST_SUBTRACT:
			return PF_Subtractive;
		case AST_REVERSESUBTRACT:
			return PF_ReverseSubtract;
		case AST_MODULATE:
			return PF_Multiplicative;
		default:
			return PF_Masked;
	}
}

UINT8 HWR_GetTranstableAlpha(INT32 transtablenum)
{
	switch (transtablenum)
	{
		case 0          : return 0xff;
		case tr_trans10 : return 0xe6;
		case tr_trans20 : return 0xcc;
		case tr_trans30 : return 0xb3;
		case tr_trans40 : return 0x99;
		case tr_trans50 : return 0x80;
		case tr_trans60 : return 0x66;
		case tr_trans70 : return 0x4c;
		case tr_trans80 : return 0x33;
		case tr_trans90 : return 0x19;
	}

	return 0xff;
}

FBITFIELD HWR_SurfaceBlend(INT32 style, INT32 transtablenum, FSurfaceInfo *pSurf)
{
	if (!transtablenum || style <= AST_COPY || style >= AST_OVERLAY)
	{
		pSurf->PolyColor.s.alpha = 0xff;
		return PF_Masked;
	}

	pSurf->PolyColor.s.alpha = HWR_GetTranstableAlpha(transtablenum);
	return HWR_GetBlendModeFlag(style);
}

FBITFIELD HWR_TranstableToAlpha(INT32 transtablenum, FSurfaceInfo *pSurf)
{
	if (!transtablenum)
	{
		pSurf->PolyColor.s.alpha = 0x00;
		return PF_Masked;
	}

	pSurf->PolyColor.s.alpha = HWR_GetTranstableAlpha(transtablenum);
	return PF_Translucent;
}

// ==========================================================================
// Wall generation from subsector segs
// ==========================================================================

//
// HWR_ProjectWall
//
static void HWR_ProjectWall(FOutVector *wallVerts, FSurfaceInfo *pSurf, FBITFIELD blendmode, INT32 lightlevel, extracolormap_t *wallcolormap)
{
	INT32 shader = SHADER_NONE;

	HWR_Lighting(pSurf, lightlevel, wallcolormap, P_SectorUsesDirectionalLighting(gl_frontsector));

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
	if (gl_curline->linedef->splats && cv_splats.value)
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

	float diff;

	fixed_t v1x = FloatToFixed(wallVerts[0].x);
	fixed_t v1y = FloatToFixed(wallVerts[0].z);
	fixed_t v2x = FloatToFixed(wallVerts[1].x);
	fixed_t v2y = FloatToFixed(wallVerts[1].z);

	const UINT8 alpha = Surf->PolyColor.s.alpha;
	FUINT lightnum = HWR_CalcWallLight(sector->lightlevel, gl_curline, NULL);
	extracolormap_t *colormap = NULL;

	realtop = top = wallVerts[3].y;
	realbot = bot = wallVerts[0].y;
	diff = top - bot;

	pegt = wallVerts[3].t;
	pegb = wallVerts[0].t;

	// Lactozilla: If both heights of a side lay on the same position, then this wall is a triangle.
	// To avoid division by zero, which would result in a NaN, we check if the vertical difference
	// between the two vertices is not zero.
	if (fpclassify(diff) == FP_ZERO)
		pegmul = 0.0;
	else
		pegmul = (pegb - pegt) / diff;

	endrealtop = endtop = wallVerts[2].y;
	endrealbot = endbot = wallVerts[1].y;
	diff = endtop - endbot;

	endpegt = wallVerts[2].t;
	endpegb = wallVerts[1].t;

	if (fpclassify(diff) == FP_ZERO)
		endpegmul = 0.0;
	else
		endpegmul = (endpegb - endpegt) / diff;

	for (INT32 i = 0; i < sector->numlights; i++)
	{
		if ((endtop < endrealbot) && (top < realbot))
			return;

		lightlist_t *list = sector->lightlist;

		if (!(list[i].flags & FF_NOSHADE))
		{
			if (pfloor && (pfloor->flags & FF_FOG))
			{
				lightnum = pfloor->master->frontsector->lightlevel;
				colormap = pfloor->master->frontsector->extra_colormap;
			}
			else
			{
				lightnum = *list[i].lightlevel;
				colormap = list[i].extra_colormap;
			}

			lightnum = HWR_CalcWallLight(lightnum, gl_curline, colormap);
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

		if (endbheight >= endtop)
			continue;

		if (bheight >= top)
			continue;

		// Found a break
		// The heights are clamped to ensure the polygon doesn't cross itself.
		bot = CLAMP(bheight, realbot, top);
		endbot = CLAMP(endbheight, endrealbot, endtop);

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
		else if (polyflags & (PF_Translucent|PF_Additive|PF_Subtractive|PF_ReverseSubtract|PF_Multiplicative|PF_Environment))
			HWR_AddTransparentWall(wallVerts, Surf, texnum, noencore, polyflags, false, lightnum, colormap);
		else
			HWR_ProjectWall(wallVerts, Surf, PF_Masked|polyflags, lightnum, colormap);

		top = bot;
		endtop = endbot;
	}

	bot = realbot;
	endbot = endrealbot;
	if ((endtop <= endrealbot) && (top <= realbot))
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
	else if (polyflags & (PF_Translucent|PF_Additive|PF_Subtractive|PF_ReverseSubtract|PF_Multiplicative|PF_Environment))
		HWR_AddTransparentWall(wallVerts, Surf, texnum, noencore, polyflags, false, lightnum, colormap);
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

boolean gl_collect_skywalls = false;

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

	memcpy(skyWallVertexArray + skyWallVertexArraySize * 4, wallVerts, sizeof(FOutVector) * 4);
	skyWallVertexArraySize++;
}

static void HWR_DrawSkyWallList(void)
{
	int i;
	FSurfaceInfo surf;

	surf.PolyColor.rgba = 0xFFFFFFFF;

	HWR_SetCurrentTexture(NULL);
	GL_UnSetShader();
	for (i = 0; i < skyWallVertexArraySize; i++)
	{
		GL_DrawPolygon(&surf, skyWallVertexArray + i * 4, 4, PF_Occlude|PF_Invisible|PF_NoTexture|PF_Skydecal);
	}
}

// HWR_DrawSkyWalls
// Draw walls into the depth buffer so that anything behind is culled properly
static void HWR_DrawSkyWall(FOutVector *wallVerts, FSurfaceInfo *Surf)
{
	//HWR_SetCurrentTexture(NULL);
	// no texture
	wallVerts[3].t = wallVerts[2].t = 0;
	wallVerts[0].t = wallVerts[1].t = 0;
	wallVerts[0].s = wallVerts[3].s = 0;
	wallVerts[2].s = wallVerts[1].s = 0;

	if (UNLIKELY(gl_collect_skywalls))
	{
		HWR_SkyWallList_Add(wallVerts);
	}
	else
	{
		HWR_SetCurrentTexture(NULL);
		HWR_ProjectWall(wallVerts, Surf, PF_Invisible|PF_NoTexture|PF_Skydecal, 255, NULL);
	}
	// PF_Invisible so it's not drawn into the colour buffer
	// PF_NoTexture for no texture
	// PF_Occlude is set in HWR_ProjectWall to draw into the depth buffer
}

// Returns true if the midtexture is visible, and false if... it isn't...
static inline boolean HWR_BlendMidtextureSurface(FSurfaceInfo *pSurf)
{
	FUINT blendmode = PF_Masked;

	pSurf->PolyColor.s.alpha = 0xFF;

	if (LIKELY(!gl_curline->polyseg))
	{
		// set alpha for transparent walls
		switch (gl_linedef->special)
		{
			// Translucent linedef types
			case 102:
			case 121 ... 125:
			case 141 ... 145:
			case 174:
			case 175:
			case 192:
			case 195:
			case 221:
			case 253:
			case 256:
				if (gl_linedef->blendmode)
					blendmode = HWR_SurfaceBlend(gl_linedef->blendmode, R_GetLinedefTransTable(gl_linedef->alpha), pSurf);
				else
					blendmode = PF_Translucent;
				break;
			default:
				if (gl_linedef->blendmode)
				{
					if (gl_linedef->alpha >= 0 && gl_linedef->alpha < FRACUNIT)
						blendmode = HWR_SurfaceBlend(gl_linedef->blendmode, R_GetLinedefTransTable(gl_linedef->alpha), pSurf);
					else
						blendmode = HWR_GetBlendModeFlag(gl_linedef->blendmode);
				}
				else if (gl_linedef->alpha >= 0 && gl_linedef->alpha < FRACUNIT)
					blendmode = HWR_TranstableToAlpha(R_GetLinedefTransTable(gl_linedef->alpha), pSurf);
				else
					blendmode = PF_Masked;
				break;
		}
	}
	else if (gl_curline->polyseg->translucency > 0)
	{
		// Polyobject translucency is shared between all of its lines
		if (gl_curline->polyseg->translucency >= NUMTRANSMAPS) // wall not drawn
		{
			pSurf->PolyColor.s.alpha = 0x00; // This shouldn't draw anything regardless of blendmode
			return false;
		}
		else
			blendmode = HWR_TranstableToAlpha(gl_curline->polyseg->translucency, pSurf);
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

	fixed_t h, l; // 3D sides and 2s middle textures
	fixed_t hS, lS;

	gl_sidedef = gl_curline->sidedef;
	gl_linedef = gl_curline->linedef;

	const boolean noencore = (gl_linedef->flags & ML_TFERLINE);

	if (LIKELY(gl_curline->pv1))
	{
		vs.x = ((polyvertex_t *)gl_curline->pv1)->x;
		vs.y = ((polyvertex_t *)gl_curline->pv1)->y;
		v1x = FloatToFixed(vs.x);
		v1y = FloatToFixed(vs.y);
	}
	else
	{
		vs.x = FixedToFloat(gl_curline->v1->x);
		vs.y = FixedToFloat(gl_curline->v1->y);
		v1x = gl_curline->v1->x;
		v1y = gl_curline->v1->y;
	}

	if (LIKELY(gl_curline->pv2))
	{
		ve.x = ((polyvertex_t *)gl_curline->pv2)->x;
		ve.y = ((polyvertex_t *)gl_curline->pv2)->y;
		v2x = FloatToFixed(ve.x);
		v2y = FloatToFixed(ve.y);
	}
	else
	{
		ve.x = FixedToFloat(gl_curline->v2->x);
		ve.y = FixedToFloat(gl_curline->v2->y);
		v2x = gl_curline->v2->x;
		v2y = gl_curline->v2->y;
	}

#define SLOPEPARAMS(slope, end1, end2, normalheight) \
	end1 = P_GetZAt(slope, v1x, v1y, normalheight);  \
	end2 = P_GetZAt(slope, v2x, v2y, normalheight);

	SLOPEPARAMS(gl_frontsector->c_slope, worldtop,    worldtopslope,    gl_frontsector->ceilingheight)
	SLOPEPARAMS(gl_frontsector->f_slope, worldbottom, worldbottomslope, gl_frontsector->floorheight)

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
	fixed_t texturehpeg = gl_sidedef->textureoffset + gl_curline->offset;
	float cliplow = (float)texturehpeg;
	float cliphigh = (float)(texturehpeg + (gl_curline->flength*FRACUNIT));

	FUINT lightnum = gl_frontsector->lightlevel;
	extracolormap_t *colormap = gl_frontsector->extra_colormap;
	lightnum = HWR_CalcWallLight(lightnum, gl_curline, colormap);

	FSurfaceInfo Surf;

	if (gl_frontsector)
		Surf.PolyColor.s.alpha = 255;

	INT32 gl_midtexture = R_GetTextureNum(gl_sidedef->midtexture);
	GLMapTexture_t *glTex = NULL;

	// two sided line
	if (gl_backsector)
	{
		INT32 gl_toptexture = 0, gl_bottomtexture = 0;

		fixed_t texturevpeg;

		SLOPEPARAMS(gl_backsector->c_slope, worldhigh, worldhighslope, gl_backsector->ceilingheight)
		SLOPEPARAMS(gl_backsector->f_slope, worldlow,  worldlowslope,  gl_backsector->floorheight)

		// Sky culling
		if (!gl_curline->polyseg) // Don't do it for polyobjects
		{
			// Sky Ceilings
			wallVerts[3].y = wallVerts[2].y = FixedToFloat(INT32_MAX);

			if (gl_frontsector->ceilingpic == skyflatnum)
			{
				if (gl_backsector->ceilingpic == skyflatnum)
				{
					// Both front and back sectors are sky, needs skywall from the frontsector's ceiling, but only if the
					// backsector is lower
					if ((worldhigh <= worldtop && worldhighslope <= worldtopslope) // Assuming ESLOPE is always on with my changes
						&& (worldhigh != worldtop || worldhighslope != worldtopslope))
					// Removing the second line above will render more rarely visible skywalls. Example: Cave garden ceiling in Dark race
					{
						wallVerts[0].y = FixedToFloat(worldhigh);
						wallVerts[1].y = FixedToFloat(worldhighslope);
						HWR_DrawSkyWall(wallVerts, &Surf);
					}
				}
				else
				{
					// Only the frontsector is sky, just draw a skywall from the front ceiling
					wallVerts[0].y = FixedToFloat(worldtop);
					wallVerts[1].y = FixedToFloat(worldtopslope);
					HWR_DrawSkyWall(wallVerts, &Surf);
				}
			}
			else if (gl_backsector->ceilingpic == skyflatnum)
			{
				// Only the backsector is sky, just draw a skywall from the front ceiling
				wallVerts[0].y = FixedToFloat(worldtop);
				wallVerts[1].y = FixedToFloat(worldtopslope);
				HWR_DrawSkyWall(wallVerts, &Surf);
			}

			// Sky Floors
			wallVerts[0].y = wallVerts[1].y = FixedToFloat(INT32_MIN);

			if (gl_frontsector->floorpic == skyflatnum)
			{
				if (gl_backsector->floorpic == skyflatnum)
				{
					// Both front and back sectors are sky, needs skywall from the backsector's floor, but only if the
					// it's higher, also needs to check for bottomtexture as the floors don't usually move down
					// when both sides are sky floors
					if ((worldlow >= worldbottom && worldlowslope >= worldbottomslope)
					&& (worldlow != worldbottom || worldlowslope != worldbottomslope)
					// Removing the second line above will render more rarely visible skywalls. Example: Cave garden ceiling in Dark race
					&& !(gl_sidedef->bottomtexture))
					{
						wallVerts[3].y = FixedToFloat(worldlow);
						wallVerts[2].y = FixedToFloat(worldlowslope);
						HWR_DrawSkyWall(wallVerts, &Surf);
					}
				}
				else
				{
					// Only the backsector has sky, just draw a skywall from the back floor
					wallVerts[3].y = FixedToFloat(worldbottom);
					wallVerts[2].y = FixedToFloat(worldbottomslope);
					HWR_DrawSkyWall(wallVerts, &Surf);
				}
			}
			else if ((gl_backsector->floorpic == skyflatnum) && !(gl_sidedef->bottomtexture))
			{
				// Only the backsector has sky, just draw a skywall from the back floor if there's no bottomtexture
				wallVerts[3].y = FixedToFloat(worldlow);
				wallVerts[2].y = FixedToFloat(worldlowslope);
				HWR_DrawSkyWall(wallVerts, &Surf);
			}
		}

		// hack to allow height changes in outdoor areas
		// This is what gets rid of the upper textures if there should be sky
		if (gl_frontsector->ceilingpic == skyflatnum
			&& gl_backsector->ceilingpic  == skyflatnum)
		{
			worldtop = worldhigh;
			worldtopslope = worldhighslope;
		}

		gl_toptexture = R_GetTextureNum(gl_sidedef->toptexture);

		// check TOP TEXTURE
		if (gl_toptexture && (worldhighslope < worldtopslope || worldhigh < worldtop))
		{
			// PEGGING
			if (gl_linedef->flags & ML_DONTPEGTOP)
				texturevpeg = 0;
			else if (gl_linedef->flags & ML_EFFECT1)
				texturevpeg = worldhigh + textureheight[gl_toptexture] - worldtop;
			else
				texturevpeg = gl_backsector->ceilingheight + textureheight[gl_toptexture] - gl_frontsector->ceilingheight;

			texturevpeg += gl_sidedef->rowoffset;

			// This is so that it doesn't overflow and screw up the wall, it doesn't need to go higher than the texture's height anyway
			texturevpeg %= textureheight[gl_toptexture];

			glTex = HWR_GetTexture(gl_toptexture, noencore);

			wallVerts[3].t = wallVerts[2].t = texturevpeg * glTex->scaleY;
			wallVerts[0].t = wallVerts[1].t = (texturevpeg + gl_frontsector->ceilingheight - gl_backsector->ceilingheight) * glTex->scaleY;
			wallVerts[0].s = wallVerts[3].s = cliplow * glTex->scaleX;
			wallVerts[2].s = wallVerts[1].s = cliphigh * glTex->scaleX;

			// Adjust t value for sloped walls
			if (!(gl_linedef->flags & ML_EFFECT1))
			{
				// Unskewed
				wallVerts[3].t -= (worldtop - gl_frontsector->ceilingheight) * glTex->scaleY;
				wallVerts[2].t -= (worldtopslope - gl_frontsector->ceilingheight) * glTex->scaleY;
				wallVerts[0].t -= (worldhigh - gl_backsector->ceilingheight) * glTex->scaleY;
				wallVerts[1].t -= (worldhighslope - gl_backsector->ceilingheight) * glTex->scaleY;
			}
			else if (gl_linedef->flags & ML_DONTPEGTOP)
			{
				// Skewed by top
				wallVerts[0].t = (texturevpeg + worldtop - worldhigh) * glTex->scaleY;
				wallVerts[1].t = (texturevpeg + worldtopslope - worldhighslope) * glTex->scaleY;
			}
			else
			{
				// Skewed by bottom
				wallVerts[0].t = wallVerts[1].t = (texturevpeg + worldtop - worldhigh) * glTex->scaleY;
				wallVerts[3].t = wallVerts[0].t - (worldtop - worldhigh) * glTex->scaleY;
				wallVerts[2].t = wallVerts[1].t - (worldtopslope - worldhighslope) * glTex->scaleY;
			}

			// set top/bottom coords
			wallVerts[3].y = FixedToFloat(worldtop);
			wallVerts[0].y = FixedToFloat(worldhigh);
			wallVerts[2].y = FixedToFloat(worldtopslope);
			wallVerts[1].y = FixedToFloat(worldhighslope);

			if (!gl_drawing_stencil && gl_frontsector->numlights)
				HWR_SplitWall(gl_frontsector, wallVerts, gl_toptexture, noencore, &Surf, FF_CUTLEVEL, NULL, 0);
			else if (!gl_drawing_stencil && glTex->mipmap.flags & TF_TRANSPARENT)
				HWR_AddTransparentWall(wallVerts, &Surf, gl_toptexture, noencore, PF_Environment, false, lightnum, colormap);
			else
				HWR_ProjectWall(wallVerts, &Surf, PF_Masked, lightnum, colormap);
		}

		gl_bottomtexture = R_GetTextureNum(gl_sidedef->bottomtexture);

		// check BOTTOM TEXTURE
		if (gl_bottomtexture && (worldlowslope > worldbottomslope || worldlow > worldbottom))
		{
			// PEGGING
			if (!(gl_linedef->flags & ML_DONTPEGBOTTOM))
				texturevpeg = 0;
			else if (gl_linedef->flags & ML_EFFECT1)
				texturevpeg = worldbottom - worldlow;
			else
				texturevpeg = gl_frontsector->floorheight - gl_backsector->floorheight;

			texturevpeg += gl_sidedef->rowoffset;

			// This is so that it doesn't overflow and screw up the wall, it doesn't need to go higher than the texture's height anyway
			texturevpeg %= textureheight[gl_bottomtexture];

			glTex = HWR_GetTexture(gl_bottomtexture, noencore);

			wallVerts[3].t = wallVerts[2].t = texturevpeg * glTex->scaleY;
			wallVerts[0].t = wallVerts[1].t = (texturevpeg + gl_backsector->floorheight - gl_frontsector->floorheight) * glTex->scaleY;
			wallVerts[0].s = wallVerts[3].s = cliplow * glTex->scaleX;
			wallVerts[2].s = wallVerts[1].s = cliphigh * glTex->scaleX;

			// Adjust t value for sloped walls
			if (!(gl_linedef->flags & ML_EFFECT1))
			{
				// Unskewed
				wallVerts[0].t -= (worldbottom - gl_frontsector->floorheight) * glTex->scaleY;
				wallVerts[1].t -= (worldbottomslope - gl_frontsector->floorheight) * glTex->scaleY;
				wallVerts[3].t -= (worldlow - gl_backsector->floorheight) * glTex->scaleY;
				wallVerts[2].t -= (worldlowslope - gl_backsector->floorheight) * glTex->scaleY;
			}
			else if (gl_linedef->flags & ML_DONTPEGBOTTOM)
			{
				// Skewed by bottom
				wallVerts[0].t = wallVerts[1].t = (texturevpeg + worldlow - worldbottom) * glTex->scaleY;
				wallVerts[2].t = wallVerts[1].t - (worldlowslope - worldbottomslope) * glTex->scaleY;
			}
			else
			{
				// Skewed by top
				wallVerts[0].t = (texturevpeg + worldlow - worldbottom) * glTex->scaleY;
				wallVerts[1].t = (texturevpeg + worldlowslope - worldbottomslope) * glTex->scaleY;
			}

			// set top/bottom coords
			wallVerts[3].y = FixedToFloat(worldlow);
			wallVerts[0].y = FixedToFloat(worldbottom);
			wallVerts[2].y = FixedToFloat(worldlowslope);
			wallVerts[1].y = FixedToFloat(worldbottomslope);

			if (!gl_drawing_stencil && gl_frontsector->numlights)
				HWR_SplitWall(gl_frontsector, wallVerts, gl_bottomtexture, noencore, &Surf, FF_CUTLEVEL, NULL, 0);
			else if (!gl_drawing_stencil && glTex->mipmap.flags & TF_TRANSPARENT)
				HWR_AddTransparentWall(wallVerts, &Surf, gl_bottomtexture, noencore, PF_Environment, false, lightnum, colormap);
			else
				HWR_ProjectWall(wallVerts, &Surf, PF_Masked, lightnum, colormap);
		}

		if ((gl_midtexture && HWR_BlendMidtextureSurface(&Surf)) || gl_portal_state == GLPORTAL_STENCIL || gl_portal_state == GLPORTAL_DEPTH || gl_drawing_stencil)
		{
			sector_t *front, *back;
			INT32 repeats;

			if (gl_linedef->frontsector->heightsec != -1)
				front = &sectors[gl_linedef->frontsector->heightsec];
			else
				front = gl_linedef->frontsector;

			if (gl_linedef->backsector->heightsec != -1)
				back = &sectors[gl_linedef->backsector->heightsec];
			else
				back = gl_linedef->backsector;

			if (gl_sidedef->repeatcnt)
				repeats = 1 + gl_sidedef->repeatcnt;
			else if (gl_linedef->flags & ML_EFFECT5 || gl_portal_state == GLPORTAL_STENCIL || gl_portal_state == GLPORTAL_DEPTH)
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

				repeats = (high - low) / textureheight[gl_midtexture];
				if ((high - low) % textureheight[gl_midtexture])
					repeats++; // tile an extra time to fill the gap -- Monster Iestyn
			}
			else
				repeats = 1;

			// SoM: a little note: This code re-arranging will
			// fix the bug in Nimrod map02. popentop and popenbottom
			// record the limits the texture can be displayed in.
			// polytop and polybottom, are the ideal (i.e. unclipped)
			// heights of the polygon, and h & l, are the final (clipped)
			// poly coords.
			fixed_t popentop, popenbottom, polytop, polybottom, lowcut, highcut;
			fixed_t popentopslope, popenbottomslope, polytopslope, polybottomslope, lowcutslope, highcutslope;

			// NOTE: With polyobjects, whenever you need to check the properties of the polyobject sector it belongs to,
			// you must use the linedef's backsector to be correct
			// From CB
			if (gl_curline->polyseg)
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

			// Find the wall's coordinates
			fixed_t midtexheight = textureheight[gl_midtexture] * repeats;

			if (gl_linedef->flags & ML_EFFECT2)
			{
				if (!!(gl_linedef->flags & ML_DONTPEGBOTTOM) ^ !!(gl_linedef->flags & ML_EFFECT3)) // Peg it to the floor
				{
					polybottom = max(front->floorheight, back->floorheight) + gl_sidedef->rowoffset;
					polytop = polybottom + midtexheight;
				}
				else // Peg it to the ceiling
				{
					polytop = min(front->ceilingheight, back->ceilingheight) + gl_sidedef->rowoffset;
					polybottom = polytop - midtexheight;
				}

				// The right side's coordinates are the the same as the left side
				polytopslope = polytop;
				polybottomslope = polybottom;
			}
			else if (!!(gl_linedef->flags & ML_DONTPEGBOTTOM) ^ !!(gl_linedef->flags & ML_EFFECT3)) // Skew the texture, but peg it to the floor
			{
				polybottom = popenbottom + gl_sidedef->rowoffset;
				polytop = polybottom + midtexheight;
				polybottomslope = popenbottomslope + gl_sidedef->rowoffset;
				polytopslope = polybottomslope + midtexheight;
			}
			else // Skew it according to the ceiling's slope
			{
				polytop = popentop + gl_sidedef->rowoffset;
				polybottom = polytop - midtexheight;
				polytopslope = popentopslope + gl_sidedef->rowoffset;
				polybottomslope = polytopslope - midtexheight;
			}

			// CB
			// NOTE: With polyobjects, whenever you need to check the properties of the polyobject sector it belongs to,
			// you must use the linedef's backsector to be correct
			if (gl_curline->polyseg)
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
			fixed_t texturevpegslope;

			if (!!(gl_linedef->flags & ML_DONTPEGBOTTOM) ^ !!(gl_linedef->flags & ML_EFFECT3))
			{
				texturevpeg = midtexheight - h + polybottom;
				texturevpegslope = midtexheight - hS + polybottomslope;
			}
			else
			{
				texturevpeg = polytop - h;
				texturevpegslope = polytopslope - hS;
			}

			glTex = HWR_GetTexture(gl_midtexture, noencore);

			// Left side
			wallVerts[3].t = texturevpeg * glTex->scaleY;
			wallVerts[0].t = (h - l + texturevpeg) * glTex->scaleY;
			wallVerts[0].s = wallVerts[3].s = cliplow * glTex->scaleX;

			// Right side
			wallVerts[2].t = texturevpegslope * glTex->scaleY;
			wallVerts[1].t = (hS - lS + texturevpegslope) * glTex->scaleY;
			wallVerts[2].s = wallVerts[1].s = cliphigh * glTex->scaleX;

			// set top/bottom coords
			// Take the texture peg into account, rather than changing the offsets past
			// where the polygon might not be.
			wallVerts[3].y = FixedToFloat(h);
			wallVerts[0].y = FixedToFloat(l);
			wallVerts[2].y = FixedToFloat(hS);
			wallVerts[1].y = FixedToFloat(lS);

			// TODO: Actually use the surface's flags so that I don't have to do this
			FUINT blendmode = Surf.PolyFlags;

			// Render midtextures on two-sided lines with a z-buffer offset.
			// This will cause the midtexture appear on top, if a FOF overlaps with it.
			blendmode |= PF_Decal;

			if (!gl_drawing_stencil && gl_frontsector->numlights)
				HWR_SplitWall(gl_frontsector, wallVerts, gl_midtexture, noencore, &Surf, (!(blendmode & PF_Masked)) ? FF_TRANSLUCENT : FF_CUTLEVEL, NULL, blendmode);
			else if (!gl_drawing_stencil && !(blendmode & PF_Masked))
				HWR_AddTransparentWall(wallVerts, &Surf, gl_midtexture, noencore, blendmode, false, lightnum, colormap);
			else
				HWR_ProjectWall(wallVerts, &Surf, blendmode, lightnum, colormap);
		}
	}
	else
	{
		// Single sided line... Deal only with the middletexture (if one exists)
		if (gl_midtexture && gl_linedef->special != HORIZONSPECIAL) // Ignore horizon line for OGL
		{
			glTex = HWR_GetTexture(gl_midtexture, noencore);

			fixed_t     texturevpeg;

			// PEGGING
			if ((gl_linedef->flags & (ML_DONTPEGBOTTOM|ML_EFFECT2)) == (ML_DONTPEGBOTTOM|ML_EFFECT2))
				texturevpeg = gl_frontsector->floorheight + textureheight[gl_sidedef->midtexture] - gl_frontsector->ceilingheight + gl_sidedef->rowoffset;
			else if (gl_linedef->flags & ML_DONTPEGBOTTOM)
				texturevpeg = worldbottom + textureheight[gl_sidedef->midtexture] - worldtop + gl_sidedef->rowoffset;
			else
				texturevpeg = gl_sidedef->rowoffset; // top of texture at top

			wallVerts[3].t = wallVerts[2].t = texturevpeg * glTex->scaleY;
			wallVerts[0].t = wallVerts[1].t = (texturevpeg + gl_frontsector->ceilingheight - gl_frontsector->floorheight) * glTex->scaleY;
			wallVerts[0].s = wallVerts[3].s = cliplow * glTex->scaleX;
			wallVerts[2].s = wallVerts[1].s = cliphigh * glTex->scaleX;

			// Texture correction for slopes
			if (gl_linedef->flags & ML_EFFECT2)
			{
				wallVerts[3].t += (gl_frontsector->ceilingheight - worldtop) * glTex->scaleY;
				wallVerts[2].t += (gl_frontsector->ceilingheight - worldtopslope) * glTex->scaleY;
				wallVerts[0].t += (gl_frontsector->floorheight - worldbottom) * glTex->scaleY;
				wallVerts[1].t += (gl_frontsector->floorheight - worldbottomslope) * glTex->scaleY;
			}
			else if (gl_linedef->flags & ML_DONTPEGBOTTOM)
			{
				wallVerts[3].t = wallVerts[0].t + (worldbottom-worldtop) * glTex->scaleY;
				wallVerts[2].t = wallVerts[1].t + (worldbottomslope-worldtopslope) * glTex->scaleY;
			}
			else
			{
				wallVerts[0].t = wallVerts[3].t - (worldbottom-worldtop) * glTex->scaleY;
				wallVerts[1].t = wallVerts[2].t - (worldbottomslope-worldtopslope) * glTex->scaleY;
			}

			//Set textures properly on single sided walls that are sloped
			wallVerts[3].y = FixedToFloat(worldtop);
			wallVerts[0].y = FixedToFloat(worldbottom);
			wallVerts[2].y = FixedToFloat(worldtopslope);
			wallVerts[1].y = FixedToFloat(worldbottomslope);

			if (gl_frontsector->numlights)
			{
				HWR_SplitWall(gl_frontsector, wallVerts, gl_midtexture, noencore, &Surf, FF_CUTLEVEL, NULL, 0);
			}
			else  // I don't think that solid walls can use translucent linedef types...
			{
				if (glTex->mipmap.flags & TF_TRANSPARENT)
					HWR_AddTransparentWall(wallVerts, &Surf, gl_midtexture, noencore, PF_Environment, false, lightnum, colormap);
				else
					HWR_ProjectWall(wallVerts, &Surf, PF_Masked, lightnum, colormap);
			}
		}
		else
		{
			//Set textures properly on single sided walls that are sloped
			wallVerts[3].y = FixedToFloat(worldtop);
			wallVerts[0].y = FixedToFloat(worldbottom);
			wallVerts[2].y = FixedToFloat(worldtopslope);
			wallVerts[1].y = FixedToFloat(worldbottomslope);

			// When there's no midtexture, draw a skywall to prevent rendering behind it
			HWR_DrawSkyWall(wallVerts, &Surf);
		}

		if (!gl_curline->polyseg)
		{
			if (gl_frontsector->ceilingpic == skyflatnum) // It's a single-sided line with sky for its sector
			{
				wallVerts[2].y = wallVerts[3].y = FixedToFloat(INT32_MAX); // draw to top of map space
				wallVerts[0].y = FixedToFloat(worldtop);
				wallVerts[1].y = FixedToFloat(worldtopslope);

				HWR_DrawSkyWall(wallVerts, &Surf);
			}
			if (gl_frontsector->floorpic == skyflatnum)
			{
				wallVerts[3].y = FixedToFloat(worldbottom);
				wallVerts[2].y = FixedToFloat(worldbottomslope);
				wallVerts[0].y = wallVerts[1].y = FixedToFloat(INT32_MIN); // draw to bottom of map space

				HWR_DrawSkyWall(wallVerts, &Surf);
			}
		}
	}

	//Hurdler: 3d-floors test
	if (!gl_drawing_stencil && gl_backsector && gl_frontsector->tag != gl_backsector->tag && (gl_backsector->ffloors || gl_frontsector->ffloors))
	{
		ffloor_t * rover;
		fixed_t    highcut = 0, lowcut = 0;
		fixed_t lowcutslope = 0, highcutslope = 0;

		// Used for height comparisons and etc across FOFs and slopes
		fixed_t high1, highslope1, low1, lowslope1;

		INT32 texnum;
		line_t * newline = NULL; // Multi-Property FOF

		lowcut = max(worldbottom, worldlow);
		highcut = min(worldtop, worldhigh);
		lowcutslope = max(worldbottomslope, worldlowslope);
		highcutslope = min(worldtopslope, worldhighslope);

		if (gl_backsector->ffloors)
		{
			for (rover = gl_backsector->ffloors; rover; rover = rover->next)
			{
				const ffloortype_e roverflags = rover->flags;

				if (!(roverflags & FF_EXISTS) || !(roverflags & FF_RENDERSIDES) || (roverflags & FF_INVERTSIDES))
					continue;

				SLOPEPARAMS(*rover->t_slope, high1, highslope1, *rover->topheight)
				SLOPEPARAMS(*rover->b_slope, low1,  lowslope1,  *rover->bottomheight)

				if ((high1 < lowcut || highslope1 < lowcutslope) || (low1 > highcut || lowslope1 > highcutslope))
					continue;

				texnum = R_GetTextureNum(sides[rover->master->sidenum[0]].midtexture);

				if (rover->master->flags & ML_TFERLINE)
				{
					size_t linenum = min((size_t)(gl_curline->linedef-gl_backsector->lines[0]), rover->master->frontsector->linecount);
					newline = rover->master->frontsector->lines[0] + linenum;
					texnum = R_GetTextureNum(sides[newline->sidenum[0]].midtexture);
				}

				h  = P_GetFFloorTopZAt   (rover, v1x, v1y);
				hS = P_GetFFloorTopZAt   (rover, v2x, v2y);
				l  = P_GetFFloorBottomZAt(rover, v1x, v1y);
				lS = P_GetFFloorBottomZAt(rover, v2x, v2y);

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

				//Hurdler: HW code starts here
				//FIXME: check if peging is correct
				// set top/bottom coords

				wallVerts[3].y = FixedToFloat(h);
				wallVerts[2].y = FixedToFloat(hS);
				wallVerts[0].y = FixedToFloat(l);
				wallVerts[1].y = FixedToFloat(lS);

				if (roverflags & FF_FOG)
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
						attachtobottom = !!(gl_linedef->flags & ML_DONTPEGBOTTOM);
						slopeskew = !!(rover->master->flags & ML_DONTPEGTOP);
					}

					glTex = HWR_GetTexture(texnum, noencore);

					if (!slopeskew) // no skewing
					{
						if (attachtobottom)
							texturevpeg -= *rover->topheight - *rover->bottomheight;
						wallVerts[3].t = (*rover->topheight - h + texturevpeg) * glTex->scaleY;
						wallVerts[2].t = (*rover->topheight - hS + texturevpeg) * glTex->scaleY;
						wallVerts[0].t = (*rover->topheight - l + texturevpeg) * glTex->scaleY;
						wallVerts[1].t = (*rover->topheight - lS + texturevpeg) * glTex->scaleY;
					}
					else
					{
						if (!attachtobottom) // skew by top
						{
							wallVerts[3].t = wallVerts[2].t = texturevpeg * glTex->scaleY;
							wallVerts[0].t = (h - l + texturevpeg) * glTex->scaleY;
							wallVerts[1].t = (hS - lS + texturevpeg) * glTex->scaleY;
						}
						else // skew by bottom
						{
							wallVerts[0].t = wallVerts[1].t = texturevpeg * glTex->scaleY;
							wallVerts[3].t = wallVerts[0].t - (h - l) * glTex->scaleY;
							wallVerts[2].t = wallVerts[1].t - (hS - lS) * glTex->scaleY;
						}
					}

					wallVerts[0].s = wallVerts[3].s = cliplow * glTex->scaleX;
					wallVerts[2].s = wallVerts[1].s = cliphigh * glTex->scaleX;
				}
				FBITFIELD blendmode;

				if (roverflags & FF_FOG)
				{
					blendmode = PF_Fog|PF_NoTexture;

					lightnum = rover->master->frontsector->lightlevel;
					colormap = rover->master->frontsector->extra_colormap;

					Surf.PolyColor.s.alpha = HWR_FogBlockAlpha(lightnum, colormap);

					lightnum = HWR_CalcWallLight(lightnum, gl_curline, colormap);

					if (gl_frontsector->numlights)
						HWR_SplitWall(gl_frontsector, wallVerts, 0, false, &Surf, roverflags, rover, blendmode);
					else
						HWR_AddTransparentWall(wallVerts, &Surf, 0, false, blendmode, true, lightnum, colormap);
				}
				else
				{
					blendmode = PF_Masked;

					if ((roverflags & FF_TRANSLUCENT && rover->alpha < 256) || rover->blend)
					{
						blendmode = rover->blend ? HWR_GetBlendModeFlag(rover->blend) : PF_Translucent;
						Surf.PolyColor.s.alpha = CLAMP(rover->alpha, 0, 255);
					}

					if (gl_frontsector->numlights)
						HWR_SplitWall(gl_frontsector, wallVerts, texnum, noencore, &Surf, roverflags, rover, blendmode);
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

		if (gl_frontsector->ffloors) // Putting this seperate should allow 2 FOF sectors to be connected without too many errors? I think?
		{
			for (rover = gl_frontsector->ffloors; rover; rover = rover->next)
			{
				const ffloortype_e roverflags = rover->flags;

				if (!(roverflags & FF_EXISTS) || !(roverflags & FF_RENDERSIDES) || !(roverflags & FF_ALLSIDES))
					continue;

				SLOPEPARAMS(*rover->t_slope, high1, highslope1, *rover->topheight)
				SLOPEPARAMS(*rover->b_slope, low1,  lowslope1,  *rover->bottomheight)

				if ((high1 < lowcut || highslope1 < lowcutslope) || (low1 > highcut || lowslope1 > highcutslope))
					continue;

				texnum = R_GetTextureNum(sides[rover->master->sidenum[0]].midtexture);

				if (rover->master->flags & ML_TFERLINE)
				{
					size_t linenum = min((size_t)(gl_curline->linedef-gl_backsector->lines[0]), rover->master->frontsector->linecount);
					newline = rover->master->frontsector->lines[0] + linenum;
					texnum = R_GetTextureNum(sides[newline->sidenum[0]].midtexture);
				}

				h  = P_GetFFloorTopZAt   (rover, v1x, v1y);
				hS = P_GetFFloorTopZAt   (rover, v2x, v2y);
				l  = P_GetFFloorBottomZAt(rover, v1x, v1y);
				lS = P_GetFFloorBottomZAt(rover, v2x, v2y);

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

				//Hurdler: HW code starts here
				//FIXME: check if peging is correct
				// set top/bottom coords

				wallVerts[3].y = FixedToFloat(h);
				wallVerts[2].y = FixedToFloat(hS);
				wallVerts[0].y = FixedToFloat(l);
				wallVerts[1].y = FixedToFloat(lS);

				if (roverflags & FF_FOG)
				{
					wallVerts[3].t = wallVerts[2].t = 0;
					wallVerts[0].t = wallVerts[1].t = 0;
					wallVerts[0].s = wallVerts[3].s = 0;
					wallVerts[2].s = wallVerts[1].s = 0;
				}
				else
				{
					glTex = HWR_GetTexture(texnum, noencore);

					if (newline)
					{
						wallVerts[3].t = wallVerts[2].t = (*rover->topheight - h + sides[newline->sidenum[0]].rowoffset) * glTex->scaleY;
						wallVerts[0].t = wallVerts[1].t = (h - l + (*rover->topheight - h + sides[newline->sidenum[0]].rowoffset)) * glTex->scaleY;
					}
					else
					{
						wallVerts[3].t = wallVerts[2].t = (*rover->topheight - h + sides[rover->master->sidenum[0]].rowoffset) * glTex->scaleY;
						wallVerts[0].t = wallVerts[1].t = (h - l + (*rover->topheight - h + sides[rover->master->sidenum[0]].rowoffset)) * glTex->scaleY;
					}

					wallVerts[0].s = wallVerts[3].s = cliplow * glTex->scaleX;
					wallVerts[2].s = wallVerts[1].s = cliphigh * glTex->scaleX;
				}

				FBITFIELD blendmode;

				if (roverflags & FF_FOG)
				{
					blendmode = PF_Fog|PF_NoTexture;

					lightnum = rover->master->frontsector->lightlevel;
					colormap = rover->master->frontsector->extra_colormap;

					Surf.PolyColor.s.alpha = HWR_FogBlockAlpha(lightnum, colormap);

					lightnum = HWR_CalcWallLight(lightnum, gl_curline, colormap);

					if (gl_backsector->numlights)
						HWR_SplitWall(gl_backsector, wallVerts, 0, false, &Surf, roverflags, rover, blendmode);
					else
						HWR_AddTransparentWall(wallVerts, &Surf, 0, false, blendmode, true, lightnum, colormap);
				}
				else
				{
					blendmode = PF_Masked;

					if ((roverflags & FF_TRANSLUCENT && rover->alpha < 256) || rover->blend)
					{
						blendmode = rover->blend ? HWR_GetBlendModeFlag(rover->blend) : PF_Translucent;
						Surf.PolyColor.s.alpha = CLAMP(rover->alpha, 0, 255);
					}

					if (gl_backsector->numlights)
						HWR_SplitWall(gl_backsector, wallVerts, texnum, noencore, &Surf, roverflags, rover, blendmode);
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

static inline boolean HWR_UsePortals(void)
{
	return cv_glportals.value && gl_maphasportals;
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

		if (LIKELY(gl_curline->pv1))
		{
			v1x = FloatToFixed(((polyvertex_t *)gl_curline->pv1)->x);
			v1y = FloatToFixed(((polyvertex_t *)gl_curline->pv1)->y);
		}
		else
		{
			v1x = gl_curline->v1->x;
			v1y = gl_curline->v1->y;
		}

		if (LIKELY(gl_curline->pv2))
		{
			v2x = FloatToFixed(((polyvertex_t *)gl_curline->pv2)->x);
			v2y = FloatToFixed(((polyvertex_t *)gl_curline->pv2)->y);
		}
		else
		{
			v2x = gl_curline->v2->x;
			v2y = gl_curline->v2->y;
		}

#define SLOPEPARAMS(slope, end1, end2, normalheight) \
		end1 = P_GetZAt(slope, v1x, v1y, normalheight); \
		end2 = P_GetZAt(slope, v2x, v2y, normalheight);

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
	if (LIKELY(!portalclipline) &&
	(afrontsector == viewsector || abacksector == viewsector))
	{
		fixed_t viewf1, viewf2, viewc1, viewc2;
		if (afrontsector == viewsector)
		{
			viewf1 = frontf1;
			viewf2 = frontf2;
			viewc1 = frontc1;
			viewc2 = frontc2;
		}
		else
		{
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
		return (!portalclipline); // during portal rendering view position may cause undesired culling and the above code has some wrong side effects
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

	gl_curline = line;

	if (LIKELY(gl_curline->pv1))
	{
		v1x = FloatToFixed(((polyvertex_t *)gl_curline->pv1)->x);
		v1y = FloatToFixed(((polyvertex_t *)gl_curline->pv1)->y);
	}
	else
	{
		v1x = gl_curline->v1->x;
		v1y = gl_curline->v1->y;
	}

	if (LIKELY(gl_curline->pv2))
	{
		v2x = FloatToFixed(((polyvertex_t *)gl_curline->pv2)->x);
		v2y = FloatToFixed(((polyvertex_t *)gl_curline->pv2)->y);
	}
	else
	{
		v2x = gl_curline->v2->x;
		v2y = gl_curline->v2->y;
	}

	// OPTIMIZE: quickly reject orthogonal back sides.
	angle1 = R_PointToAngle64(v1x, v1y);
	angle2 = R_PointToAngle64(v2x, v2y);

	 // PrBoom: Back side, i.e. backface culling - read: endAngle >= startAngle!
	if (angle2 - angle1 < ANGLE_180)
		return;

	// PrBoom: use REAL clipping math YAYYYYYYY!!!
	if (!gld_clipper_SafeCheckRange(angle2, angle1))
		return;

	checkforemptylines = true;

	gl_backsector = line->backsector;

	if (LIKELY(!HWR_UsePortals()))
	{
doaddline:
		if (!line->backsector)
		{
			gld_clipper_SafeAddClipRange(angle2, angle1);
		}
		else
		{
			gl_backsector = R_FakeFlat(gl_backsector, &tempsec, NULL, NULL, true);

			if (CheckClip(gl_frontsector, gl_backsector))
			{
				gld_clipper_SafeAddClipRange(angle2, angle1);
				checkforemptylines = false;
			}

			// Reject empty lines used for triggers and special events.
			// Identical floor and ceiling on both sides,
			//  identical light levels on both sides,
			//  and no middle texture.
			if (checkforemptylines && R_IsEmptyLine(line, gl_frontsector, gl_backsector))
				return;
		}

		if (LIKELY(gl_portal_state != GLPORTAL_SEARCH && !dont_draw))// no need to do this during the portal check
			HWR_ProcessSeg(); // Doesn't need arguments because they're defined globally :D

		return;
	}

	// do extra checks on the seg when rendering portals:
	// don't render segs that are behind the portal destination line
	if (portalclipline &&
		!HWR_PortalCheckPointSide(line->v1->x, line->v1->y) &&
		!HWR_PortalCheckPointSide(line->v2->x, line->v2->y))
	{
		return;
	}

	if (gl_portal_state == GLPORTAL_STENCIL || gl_portal_state == GLPORTAL_DEPTH)
		goto doaddline;

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
				if (gl_portal_state == GLPORTAL_SEARCH)
					HWR_Portal_Add2Lines(line->linedef-lines, line2, line);
				else if (gl_portal_state == GLPORTAL_INSIDE)
					dont_draw = true;
			}
		}
		else
			return;// dont do anything with the other side i guess?
	}

	goto doaddline;
}

// HWR_CheckBBox
// Checks BSP node/subtree bounding box.
// Returns true
// if some part of the bbox might be visible.
//
// modified to use local variables

static boolean HWR_CheckBBox(const fixed_t *bspcoord)
{
	fixed_t px1, py1, px2, py2;
	angle_t angle1, angle2;

	// Find the corners of the box
	// that define the edges from current viewpoint.
	const INT32 boxpos = (viewx <= bspcoord[BOXLEFT] ? 0 : viewx < bspcoord[BOXRIGHT] ? 1 : 2) +
	(viewy >= bspcoord[BOXTOP] ? 0 : viewy > bspcoord[BOXBOTTOM] ? 4 : 8);

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

	angle1 = R_PointToAngle64(px1, py1);
	angle2 = R_PointToAngle64(px2, py2);

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

	// Sort through all the polyobjects
	for (i = 0; i < numpolys; ++i)
	{
		// Render the polyobject's lines
		for (j = 0; j < po_ptrs[i]->segCount; ++j)
		{
			HWR_AddLine(po_ptrs[i]->segs[j]);
		}
	}
}

static void HWR_RenderPolyObjectPlane(polyobj_t *polysector, boolean isceiling, fixed_t fixedheight, FBITFIELD blendmode, UINT8 lightlevel, lumpnum_t lumpnum, sector_t *FOFsector, UINT8 alpha, extracolormap_t *planecolormap)
{
	float           height; //constant y for all points on the convex flat polygon
	FOutVector      *v3d;
	INT32           i;
	float           flatxref,flatyref;
	float           fflatsize;
	INT32           flatflag;
	size_t          len;
	float           scrollx = 0.0f, scrolly = 0.0f;
	angle_t         angle = 0;
	FSurfaceInfo    Surf;
	fixed_t         tempxsow, tempytow;
	size_t          nrPlaneVerts;

	static FOutVector *planeVerts = NULL;
	static UINT16 numAllocedPlaneVerts = 0;

	INT32 shader = SHADER_NONE;

	nrPlaneVerts = polysector->numVertices;

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

	height = FixedToFloat(fixedheight);

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
	flatxref = FixedToFloat(polysector->origVerts[0].x);
	flatyref = FixedToFloat(polysector->origVerts[0].y);

	flatxref = (float)(((fixed_t)flatxref & (~flatflag)) / fflatsize);
	flatyref = (float)(((fixed_t)flatyref & (~flatflag)) / fflatsize);

	// transform
	v3d = planeVerts;

	if (FOFsector != NULL)
	{
		if (!isceiling) // it's a floor
		{
			scrollx = FixedToFloat(FOFsector->floor_xoffs)/fflatsize;
			scrolly = FixedToFloat(FOFsector->floor_yoffs)/fflatsize;
			angle = FOFsector->floorpic_angle>>ANGLETOFINESHIFT;
		}
		else // it's a ceiling
		{
			scrollx = FixedToFloat(FOFsector->ceiling_xoffs)/fflatsize;
			scrolly = FixedToFloat(FOFsector->ceiling_yoffs)/fflatsize;
			angle = FOFsector->ceilingpic_angle>>ANGLETOFINESHIFT;
		}
	}
	else if (gl_frontsector)
	{
		if (!isceiling) // it's a floor
		{
			scrollx = FixedToFloat(gl_frontsector->floor_xoffs)/fflatsize;
			scrolly = FixedToFloat(gl_frontsector->floor_yoffs)/fflatsize;
			angle = gl_frontsector->floorpic_angle>>ANGLETOFINESHIFT;
		}
		else // it's a ceiling
		{
			scrollx = FixedToFloat(gl_frontsector->ceiling_xoffs)/fflatsize;
			scrolly = FixedToFloat(gl_frontsector->ceiling_yoffs)/fflatsize;
			angle = gl_frontsector->ceilingpic_angle>>ANGLETOFINESHIFT;
		}
	}

	if (angle) // Only needs to be done if there's an altered angle
	{
		// This needs to be done so that it scrolls in a different direction after rotation like software
		tempxsow = FloatToFixed(scrollx);
		tempytow = FloatToFixed(scrolly);
		scrollx = (FixedToFloat(FixedMul(tempxsow, FINECOSINE(angle)) - FixedMul(tempytow, FINESINE(angle))));
		scrolly = (FixedToFloat(FixedMul(tempxsow, FINESINE(angle)) + FixedMul(tempytow, FINECOSINE(angle))));

		// This needs to be done so everything aligns after rotation
		// It would be done so that rotation is done, THEN the translation, but I couldn't get it to rotate AND scroll like software does
		tempxsow = FloatToFixed(flatxref);
		tempytow = FloatToFixed(flatyref);
		flatxref = (FixedToFloat(FixedMul(tempxsow, FINECOSINE(angle)) - FixedMul(tempytow, FINESINE(angle))));
		flatyref = (FixedToFloat(FixedMul(tempxsow, FINESINE(angle)) + FixedMul(tempytow, FINECOSINE(angle))));
	}

	for (i = 0; i < (INT32)nrPlaneVerts; i++,v3d++)
	{
		// Hurdler: add scrolling texture on floor/ceiling
		v3d->s = (float)((FixedToFloat(polysector->origVerts[i].x) / fflatsize) - flatxref + scrollx); // Go from the polysector's original vertex locations
		v3d->t = (float)(flatyref - (FixedToFloat(polysector->origVerts[i].y) / fflatsize) + scrolly); // Means the flat is offset based on the original vertex locations

		// Need to rotate before translate
		if (angle) // Only needs to be done if there's an altered angle
		{
			tempxsow = FloatToFixed(v3d->s);
			tempytow = FloatToFixed(v3d->t);
			v3d->s = (FixedToFloat(FixedMul(tempxsow, FINECOSINE(angle)) - FixedMul(tempytow, FINESINE(angle))));
			v3d->t = (FixedToFloat(-FixedMul(tempxsow, FINESINE(angle)) - FixedMul(tempytow, FINECOSINE(angle))));
		}

		v3d->x = FixedToFloat(polysector->vertices[i]->x);
		v3d->y = height;
		v3d->z = FixedToFloat(polysector->vertices[i]->y);
	}

	HWR_Lighting(&Surf, lightlevel, planecolormap, P_SectorUsesDirectionalLighting((FOFsector != NULL) ? FOFsector : gl_frontsector));

	if (blendmode & PF_Translucent)
	{
		Surf.PolyColor.s.alpha = (UINT8)alpha;
		blendmode |= PF_Modulated|PF_Occlude;
	}
	else
		blendmode |= PF_Masked|PF_Modulated;

	if (HWR_UseShader())
	{
		shader = SHADER_FLOOR;
		blendmode |= PF_ColorMapped;
	}

	HWR_ProcessPolygon(&Surf, planeVerts, nrPlaneVerts, blendmode, shader, false);
}

static void HWR_AddPolyObjectPlanes(void)
{
	size_t i;
	sector_t *polyobjsector;
	INT32 light = 0;

	// Polyobject Planes need their own function for drawing because they don't have extrasubsectors by themselves
	// It should be okay because polyobjects should always be convex anyway

	for (i  = 0; i < numpolys; i++)
	{
		polyobjsector = po_ptrs[i]->lines[0]->backsector; // the in-level polyobject sector

		if (!(po_ptrs[i]->flags & POF_RENDERPLANES)) // Only render planes when you should
			continue;

		if (po_ptrs[i]->translucency >= NUMTRANSMAPS)
			continue;

		if (polyobjsector->floorheight <= gl_frontsector->ceilingheight
			&& polyobjsector->floorheight >= gl_frontsector->floorheight
			&& (viewz < polyobjsector->floorheight))
		{
			light = R_GetPlaneLight(gl_frontsector, polyobjsector->floorheight, true);
			if (po_ptrs[i]->translucency > 0)
			{
				FSurfaceInfo Surf;
				FBITFIELD blendmode;
				memset(&Surf, 0x00, sizeof(Surf));
				blendmode = HWR_TranstableToAlpha(po_ptrs[i]->translucency, &Surf);
				HWR_AddTransparentPolyobjectFloor(levelflats[polyobjsector->floorpic].lumpnum, po_ptrs[i], false, polyobjsector->floorheight,
												(light == -1 ? gl_frontsector->lightlevel : *gl_frontsector->lightlist[light].lightlevel), Surf.PolyColor.s.alpha, polyobjsector, blendmode, (light == -1 ? gl_frontsector->extra_colormap : gl_frontsector->lightlist[light].extra_colormap));
			}
			else
			{
				HWR_GetFlat(levelflats[polyobjsector->floorpic].lumpnum, R_NoEncore(polyobjsector, false));
				HWR_RenderPolyObjectPlane(po_ptrs[i], false, polyobjsector->floorheight, PF_Occlude,
											(light == -1 ? gl_frontsector->lightlevel : *gl_frontsector->lightlist[light].lightlevel), levelflats[polyobjsector->floorpic].lumpnum,
											polyobjsector, 255, (light == -1 ? gl_frontsector->extra_colormap : gl_frontsector->lightlist[light].extra_colormap));
			}
		}

		if (polyobjsector->ceilingheight >= gl_frontsector->floorheight
			&& polyobjsector->ceilingheight <= gl_frontsector->ceilingheight
			&& (viewz > polyobjsector->ceilingheight))
		{
			light = R_GetPlaneLight(gl_frontsector, polyobjsector->ceilingheight, true);
			if (po_ptrs[i]->translucency > 0)
			{
				FSurfaceInfo Surf;
				FBITFIELD blendmode;
				memset(&Surf, 0x00, sizeof(Surf));
				blendmode = HWR_TranstableToAlpha(po_ptrs[i]->translucency, &Surf);
				HWR_AddTransparentPolyobjectFloor(levelflats[polyobjsector->ceilingpic].lumpnum, po_ptrs[i], true, polyobjsector->ceilingheight,
												(light == -1 ? gl_frontsector->lightlevel : *gl_frontsector->lightlist[light].lightlevel), Surf.PolyColor.s.alpha, polyobjsector, blendmode, (light == -1 ? gl_frontsector->extra_colormap : gl_frontsector->lightlist[light].extra_colormap));
			}
			else
			{
				HWR_GetFlat(levelflats[polyobjsector->ceilingpic].lumpnum, R_NoEncore(polyobjsector, true));
				HWR_RenderPolyObjectPlane(po_ptrs[i], true, polyobjsector->ceilingheight, PF_Occlude,
										(light == -1 ? gl_frontsector->lightlevel : *gl_frontsector->lightlist[light].lightlevel), levelflats[polyobjsector->ceilingpic].lumpnum,
										polyobjsector, 255, (light == -1 ? gl_frontsector->extra_colormap : gl_frontsector->lightlist[light].extra_colormap));
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

	cullplane = FixedToFloat(cullheight->frontsector->floorheight);

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

static FBITFIELD HWR_RippleBlend(sector_t *sector, ffloor_t *rover, boolean ceiling)
{
	(void)sector;
	(void)ceiling;
	return /*R_IsRipplePlane(sector, rover, ceiling)*/ (rover->flags & FF_RIPPLE) ? PF_Ripple : 0;
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
		gl_frontsector = sub->sector;
		// how many linedefs
		count = sub->numlines;
		// first line seg
		line = &segs[sub->firstline];
	}
	else
	{
		// there are no segs but only planes
		sub = &subsectors[0];
		gl_frontsector = sub->sector;
		count = 0;
		line = NULL;
	}

	//SoM: 4/7/2000: Test to make Boom water work in Hardware mode.
	gl_frontsector = R_FakeFlat(gl_frontsector, &tempsec, &floorlightlevel,
								&ceilinglightlevel, false);

	if (UNLIKELY(gl_portal_state == GLPORTAL_SEARCH))
	{
		goto doaddline;
	}

	floorcolormap = ceilingcolormap = gl_frontsector->extra_colormap;

	cullFloorHeight   = P_GetSectorFloorZAt  (gl_frontsector, viewx, viewy);
	cullCeilingHeight = P_GetSectorCeilingZAt(gl_frontsector, viewx, viewy);
	locFloorHeight    = P_GetSectorFloorZAt  (gl_frontsector, gl_frontsector->soundorg.x, gl_frontsector->soundorg.y);
	locCeilingHeight  = P_GetSectorCeilingZAt(gl_frontsector, gl_frontsector->soundorg.x, gl_frontsector->soundorg.y);

	if (gl_frontsector->ffloors)
	{
		boolean anyMoved = gl_frontsector->moved;

		if (anyMoved == false)
		{
			for (rover = gl_frontsector->ffloors; rover; rover = rover->next)
			{
				if (!(rover->flags & FF_EXISTS) || !(rover->flags & FF_RENDERPLANES))
					continue;

				if (sub->validcount == validcount)
					continue;

				sector_t *controlSec = &sectors[rover->secnum];

				if (!controlSec->moved)
					continue;

				anyMoved = true;
				break;
			}
		}

		if (anyMoved == true)
		{
			gl_frontsector->numlights = sub->sector->numlights = 0;
			R_Prep3DFloors(gl_frontsector);
			sub->sector->lightlist = gl_frontsector->lightlist;
			sub->sector->numlights = gl_frontsector->numlights;
			sub->sector->moved = gl_frontsector->moved = false;
		}

		light = R_GetPlaneLight(gl_frontsector, locFloorHeight, false);
		if (gl_frontsector->floorlightsec == -1)
			floorlightlevel = *gl_frontsector->lightlist[light].lightlevel;
		floorcolormap = gl_frontsector->lightlist[light].extra_colormap;

		light = R_GetPlaneLight(gl_frontsector, locCeilingHeight, false);
		if (gl_frontsector->ceilinglightsec == -1)
			ceilinglightlevel = *gl_frontsector->lightlist[light].lightlevel;
		ceilingcolormap = gl_frontsector->lightlist[light].extra_colormap;
	}

	sub->sector->extra_colormap = gl_frontsector->extra_colormap;

	// render floor ?
	// yeah, easy backface cull! :)
	if (cullFloorHeight < viewz)
	{
		if (gl_frontsector->floorpic != skyflatnum)
		{
			if (sub->validcount != validcount)
			{
				HWR_GetFlat(levelflats[gl_frontsector->floorpic].lumpnum, R_NoEncore(gl_frontsector, false));
				HWR_RenderPlane(sub, &extrasubsectors[num], false,
					// Hack to make things continue to work around slopes.
					locFloorHeight == cullFloorHeight ? locFloorHeight : gl_frontsector->floorheight,
					// We now return you to your regularly scheduled rendering.
					PF_Occlude, floorlightlevel, levelflats[gl_frontsector->floorpic].lumpnum, NULL, 255, floorcolormap);
			}
		}
	}

	if (cullCeilingHeight > viewz)
	{
		if (gl_frontsector->ceilingpic != skyflatnum)
		{
			if (sub->validcount != validcount)
			{
				HWR_GetFlat(levelflats[gl_frontsector->ceilingpic].lumpnum, R_NoEncore(gl_frontsector, true));
				HWR_RenderPlane(sub, &extrasubsectors[num], true,
					// Hack to make things continue to work around slopes.
					locCeilingHeight == cullCeilingHeight ? locCeilingHeight : gl_frontsector->ceilingheight,
					// We now return you to your regularly scheduled rendering.
					PF_Occlude, ceilinglightlevel, levelflats[gl_frontsector->ceilingpic].lumpnum,NULL, 255, ceilingcolormap);
			}
		}
	}

	if (gl_frontsector->ffloors)
	{
		/// \todo fix light, xoffs, yoffs, extracolormap ?
		for (rover = gl_frontsector->ffloors; rover; rover = rover->next)
		{
			fixed_t bottomCullHeight, topCullHeight, centerHeight;

			if (!(rover->flags & FF_EXISTS) || !(rover->flags & FF_RENDERPLANES) || !(rover->flags & FF_RENDERALL))
				continue;
			if (sub->validcount == validcount)
				continue;

			// rendering heights for bottom and top planes
			// yes there were functions for this stuff, no idea why it wasnt used but bleh
			bottomCullHeight = P_GetFFloorBottomZAt(rover, viewx, viewy);
			topCullHeight = P_GetFFloorTopZAt(rover, viewx, viewy);

			if (gl_frontsector->cullheight)
			{
				if (HWR_DoCulling(gl_frontsector->cullheight, viewsector->cullheight, gl_viewz, FixedToFloat(*rover->bottomheight), FixedToFloat(*rover->topheight)))
					continue;
			}

			// bottom plane
			centerHeight = P_GetFFloorBottomZAt(rover, gl_frontsector->soundorg.x, gl_frontsector->soundorg.y);

			if (centerHeight <= locCeilingHeight && centerHeight >= locFloorHeight &&
			    ((viewz < bottomCullHeight && !(rover->flags & FF_INVERTPLANES)) ||
			     (viewz > bottomCullHeight && (rover->flags & FF_BOTHPLANES || rover->flags & FF_INVERTPLANES))))
			{
				if (rover->flags & FF_FOG)
				{
					UINT8 alpha;

					light = R_GetPlaneLight(gl_frontsector, centerHeight, (viewz < bottomCullHeight));

					alpha = HWR_FogBlockAlpha(*gl_frontsector->lightlist[light].lightlevel, rover->master->frontsector->extra_colormap);

					HWR_AddTransparentFloor(0,
											&extrasubsectors[num],
											false,
											*rover->bottomheight,
											*gl_frontsector->lightlist[light].lightlevel,
											alpha, rover->master->frontsector, PF_Fog|PF_NoTexture,
											true, rover->master->frontsector->extra_colormap);
				}
				else if ((rover->flags & FF_TRANSLUCENT && rover->alpha < 256) || rover->blend) // SoM: Flags are more efficient
				{
					light = R_GetPlaneLight(gl_frontsector, centerHeight, (viewz < bottomCullHeight));

					HWR_AddTransparentFloor(levelflats[*rover->bottompic].lumpnum,
											&extrasubsectors[num],
											false,
											*rover->bottomheight,
											*gl_frontsector->lightlist[light].lightlevel,
											CLAMP(rover->alpha, 0 ,255), rover->master->frontsector, HWR_RippleBlend(gl_frontsector, rover, false) | (rover->blend ? HWR_GetBlendModeFlag(rover->blend) : PF_Translucent),
											false, gl_frontsector->lightlist[light].extra_colormap);
				}
				else
				{
					HWR_GetFlat(levelflats[*rover->bottompic].lumpnum, R_NoEncore(gl_frontsector, false));
					light = R_GetPlaneLight(gl_frontsector, centerHeight, (viewz < bottomCullHeight));

					HWR_RenderPlane(sub, &extrasubsectors[num], false, *rover->bottomheight, HWR_RippleBlend(gl_frontsector, rover, false)|PF_Occlude, *gl_frontsector->lightlist[light].lightlevel, levelflats[*rover->bottompic].lumpnum,
									rover->master->frontsector, 255, gl_frontsector->lightlist[light].extra_colormap);
				}
			}

			// top plane
			centerHeight = P_GetFFloorTopZAt(rover, gl_frontsector->soundorg.x, gl_frontsector->soundorg.y);

			if (centerHeight >= locFloorHeight &&
			    centerHeight <= locCeilingHeight &&
			    ((viewz > topCullHeight && !(rover->flags & FF_INVERTPLANES)) ||
			     (viewz < topCullHeight && (rover->flags & FF_BOTHPLANES || rover->flags & FF_INVERTPLANES))))
			{
				if (rover->flags & FF_FOG)
				{
					UINT8 alpha;

					light = R_GetPlaneLight(gl_frontsector, centerHeight, (viewz < topCullHeight));

					alpha = HWR_FogBlockAlpha(*gl_frontsector->lightlist[light].lightlevel, rover->master->frontsector->extra_colormap);

					HWR_AddTransparentFloor(0,
											&extrasubsectors[num],
											true,
											*rover->topheight,
											*gl_frontsector->lightlist[light].lightlevel,
											alpha, rover->master->frontsector, PF_Fog|PF_NoTexture,
											true, rover->master->frontsector->extra_colormap);
				}
				else if ((rover->flags & FF_TRANSLUCENT && rover->alpha < 256) || rover->blend)
				{
					light = R_GetPlaneLight(gl_frontsector, centerHeight, (viewz < topCullHeight));

					HWR_AddTransparentFloor(levelflats[*rover->toppic].lumpnum,
											&extrasubsectors[num],
											true,
											*rover->topheight,
											*gl_frontsector->lightlist[light].lightlevel,
											CLAMP(rover->alpha, 0 ,255), rover->master->frontsector, HWR_RippleBlend(gl_frontsector, rover, false) | (rover->blend ? HWR_GetBlendModeFlag(rover->blend) : PF_Translucent),
											false, gl_frontsector->lightlist[light].extra_colormap);
				}
				else
				{
					HWR_GetFlat(levelflats[*rover->toppic].lumpnum, R_NoEncore(gl_frontsector, true));
					light = R_GetPlaneLight(gl_frontsector, centerHeight, (viewz < topCullHeight));

					HWR_RenderPlane(sub, &extrasubsectors[num], true, *rover->topheight, HWR_RippleBlend(gl_frontsector, rover, false)|PF_Occlude, *gl_frontsector->lightlist[light].lightlevel, levelflats[*rover->toppic].lumpnum,
									rover->master->frontsector, 255, gl_frontsector->lightlist[light].extra_colormap);
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

doaddline:
	// Hurdler: here interesting things are happening!
	// we have just drawn the floor and ceiling
	// we now draw the sprites first and then the walls
	// hurdler: false: we only add the sprites, the walls are drawn first
	if (line)
	{
		// draw sprites first, coz they are clipped to the solidsegs of
		// subsectors more 'in front'
		HWR_AddSprites(gl_frontsector);

		//Hurdler: at this point validcount must be the same, but is not because
		//         gl_frontsector doesn't point anymore to sub->sector due to
		//         the call gl_frontsector = R_FakeFlat(...)
		//         if it's not done, the sprite is drawn more than once,
		//         what looks really bad with translucency or dynamic light,
		//         without talking about the overdraw of course.
		sub->sector->validcount = validcount;/// \todo fix that in a better way

		if (UNLIKELY(numPolyObjects))
		{
			while (count--)
			{
				if (LIKELY(!line->polyseg)) // ignore segs that belong to polyobjects
					HWR_AddLine(line);
				line++;
			}
		}
		else
		{
			while (count--)
			{
				HWR_AddLine(line);
				line++;
			}
		}
	}

	sub->validcount = validcount;
}

//
// Renders all subsectors below a given node,
//  traversing subtree recursively.
// Just call with BSP root.
//

static void HWR_RenderBSPNode(INT32 bspnum)
{
	register const node_t *bsp;
	register INT32 side;
	ps_numbspcalls.value.i++;

	while (!(bspnum & NF_SUBSECTOR))  // Keep going until found a subsector
	{
		bsp = &nodes[bspnum];

		// Decide which side the view point is on.
		side = R_PointOnSideFast(viewx, viewy, bsp);

		// Recursively divide front space (toward the viewer).
		HWR_RenderBSPNode(bsp->children[side]);

		// Possibly divide back space (away from the viewer).
		if (!(HWR_CheckBBox(bsp->bbox[side^1])))
			return;

		bspnum = bsp->children[side^1];
	}

	// e6y: support for extended nodes
	HWR_Subsector(bspnum == -1 ? 0 : bspnum & ~NF_SUBSECTOR);
}

//
// Same thing but with extra portal checks
//
static void HWR_RenderPortalBSPNode(INT32 bspnum)
{
	register const node_t *bsp;
	register INT32 side;
	ps_numbspcalls.value.i++;

	while (!(bspnum & NF_SUBSECTOR))  // Keep going until found a subsector
	{
		bsp = &nodes[bspnum];

		// Decide which side the view point is on.
		side = R_PointOnSideFast(viewx, viewy, bsp);

		// Recursively divide front space (toward the viewer).
		if (HWR_PortalCheckBBox(bsp->bbox[side]))
			HWR_RenderPortalBSPNode(bsp->children[side]);

		// Possibly divide back space (away from the viewer).
		if (!(HWR_CheckBBox(bsp->bbox[side^1]) && HWR_PortalCheckBBox(bsp->bbox[side^1])))
			return;

		bspnum = bsp->children[side^1];
	}

	// PORTAL CULLING
	if (portalcullsector)
	{
		// skip all subsectors encountered before the portal
		// destination's front sector
		if (portalcullsector != subsectors[bspnum & ~NF_SUBSECTOR].sector)
			return;
		else
			portalcullsector = NULL;
	}

	// e6y: support for extended nodes
	HWR_Subsector(bspnum == -1 ? 0 : bspnum & ~NF_SUBSECTOR);
}

// ==========================================================================
// gl_things.c
// ==========================================================================

// sprites are drawn after all wall and planes are rendered, so that
// sprite translucency effects apply on the rendered view (instead of the background sky!!)

static UINT32 gl_visspritecount;
static gl_vissprite_t *gl_visspritechunks[MAXVISSPRITES >> VISSPRITECHUNKBITS] = {NULL};

// --------------------------------------------------------------------------
// HWR_ClearSprites
// Called at frame start.
// --------------------------------------------------------------------------
static inline void HWR_ClearSprites(void)
{
	gl_visspritecount = 0;
}

// --------------------------------------------------------------------------
// HWR_NewVisSprite
// --------------------------------------------------------------------------
static gl_vissprite_t gl_overflowsprite;

static gl_vissprite_t *HWR_GetVisSprite(UINT32 num)
{
	UINT32 chunk = num >> VISSPRITECHUNKBITS;

	// Allocate chunk if necessary
	if (!gl_visspritechunks[chunk])
		Z_Malloc(sizeof(gl_vissprite_t) * VISSPRITESPERCHUNK, PU_LEVEL, &gl_visspritechunks[chunk]);

	return gl_visspritechunks[chunk] + (num & VISSPRITEINDEXMASK);
}

static gl_vissprite_t *HWR_NewVisSprite(void)
{
	if (gl_visspritecount == MAXVISSPRITES)
		return &gl_overflowsprite;

	return HWR_GetVisSprite(gl_visspritecount++);
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

			if (!(*rover->topheight > floorz && abs(delta1) < abs(delta2)))
				continue;

			floorz = *rover->topheight;
		}
	}

	return floorz;
}

static void HWR_DrawSpriteShadow(gl_vissprite_t *spr, patch_t *gpatch, GLPatch_t *hwrpatch)
{
	float this_scale = 1.0f;
	FOutVector swallVerts[4];
	FSurfaceInfo sSurf;
	FBITFIELD blendmode = 0;
	fixed_t floorheight, mobjfloor;
	pslope_t *floorslope;
	fixed_t slopez;
	float offset = 0;

	// technically this_scale gets multiplied and added to sprite y/x scale, but this thing needs it for some crap so ill just throw it in here again
	const boolean hires = (spr->mobj && spr->mobj->skin && K_GetMobjSkin(spr->mobj)->flags & SF_HIRES);
	if (spr->mobj)
		this_scale = FixedToFloat(spr->mobj->scale);
	if (hires)
		this_scale = this_scale * FixedToFloat(K_GetMobjSkin(spr->mobj)->highresscale);

	R_GetShadowZ(spr->mobj, &floorslope);

	mobjfloor = HWR_OpaqueFloorAtPos(
		spr->mobj->x, spr->mobj->y,
		spr->mobj->z, spr->mobj->height);

	if (cv_shadowoffs.value)
	{
		angle_t shadowdir;

		// Set direction
		if (splitscreen && R_GetViewNumber() == 1)
			shadowdir = localangle[1] + FixedAngle(cv_cam_rotate[1].value);
		else if (splitscreen > 1 && R_GetViewNumber() == 2)
			shadowdir = localangle[2] + FixedAngle(cv_cam_rotate[2].value);
		else if (splitscreen > 2 && R_GetViewNumber() == 3)
			shadowdir = localangle[3] + FixedAngle(cv_cam_rotate[3].value);
		else
			shadowdir = localangle[0] + FixedAngle(cv_cam_rotate[0].value);

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
		swallVerts[0].y = swallVerts[1].y = swallVerts[2].y = swallVerts[3].y = spr->gzt - gpatch->topoffset * this_scale - (floorheight+3);

		// Now transform the TOP vertices along the floor in the direction of the camera
		swallVerts[3].x = spr->x1 + ((gpatch->height * this_scale) + offset) * gl_viewcos;
		swallVerts[2].x = spr->x2 + ((gpatch->height * this_scale) + offset) * gl_viewcos;
		swallVerts[3].z = spr->z1 + ((gpatch->height * this_scale) + offset) * gl_viewsin;
		swallVerts[2].z = spr->z2 + ((gpatch->height * this_scale) + offset) * gl_viewsin;
	}
	else
	{
		// Always a pixel above the floor, perfectly flat.
		swallVerts[0].y = swallVerts[1].y = swallVerts[2].y = swallVerts[3].y = spr->gzt - gpatch->topoffset - (floorheight+3);

		// Now transform the TOP vertices along the floor in the direction of the camera
		swallVerts[3].x = spr->x1 + (gpatch->height + offset) * gl_viewcos;
		swallVerts[2].x = spr->x2 + (gpatch->height + offset) * gl_viewcos;
		swallVerts[3].z = spr->z1 + (gpatch->height + offset) * gl_viewsin;
		swallVerts[2].z = spr->z2 + (gpatch->height + offset) * gl_viewsin;
	}

	// We also need to move the bottom ones away when shadowoffs is on
	if (cv_shadowoffs.value)
	{
		swallVerts[0].x = spr->x1 + offset * gl_viewcos;
		swallVerts[1].x = spr->x2 + offset * gl_viewcos;
		swallVerts[0].z = spr->z1 + offset * gl_viewsin;
		swallVerts[1].z = spr->z2 + offset * gl_viewsin;
	}

	if (floorslope)
	{
		for (int i = 0; i < 4; i++)
		{
			slopez = P_GetSlopeZAt(floorslope, FloatToFixed(swallVerts[i].x), FloatToFixed(swallVerts[i].z));
			swallVerts[i].y = FixedToFloat(slopez) + 0.05f;
		}
	}

	if (spr->flip)
	{
		swallVerts[0].s = swallVerts[3].s = hwrpatch->max_s;
		swallVerts[2].s = swallVerts[1].s = 0;
	}
	else
	{
		swallVerts[0].s = swallVerts[3].s = 0;
		swallVerts[2].s = swallVerts[1].s = hwrpatch->max_s;
	}

	// flip the texture coords (look familiar?)
	if (spr->vflip)
	{
		swallVerts[3].t = swallVerts[2].t = hwrpatch->max_t;
		swallVerts[0].t = swallVerts[1].t = 0;
	}
	else
	{
		swallVerts[3].t = swallVerts[2].t = 0;
		swallVerts[0].t = swallVerts[1].t = hwrpatch->max_t;
	}

	sSurf.PolyColor.s.red = 0x01;
	sSurf.PolyColor.s.blue = 0x01;
	sSurf.PolyColor.s.green = 0x01;

	// shadow is always half as translucent as the sprite itself
	if (UNLIKELY(!cv_translucency.value)) // use default translucency (main sprite won't have any translucency)
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
		HWR_ProcessPolygon(&sSurf, swallVerts, 4, blendmode|PF_Translucent|PF_Modulated, SHADER_NONE, false);
	}
}

// This is expecting a pointer to an array containing 4 wallVerts for a sprite
static void HWR_RotateSpritePolyToAim(gl_vissprite_t *spr, FOutVector *wallVerts, const boolean precip, const boolean papersprite)
{
	if (!cv_glspritebillboarding.value || !spr || !spr->mobj || !wallVerts || papersprite)
	{
		return;
	}

	// uncapped/interpolation
	interpmobjstate_t interp = {0};
	float basey, lowy;
	INT32 dist = -1;

	if (cv_maxinterpdist.value)
		dist = R_QuickCamDist(spr->mobj->x, spr->mobj->y);

	// do interpolation
	if (R_UsingFrameInterpolation() && !paused && (!cv_maxinterpdist.value || dist < cv_maxinterpdist.value))
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

	if (!precip && P_MobjFlip(spr->mobj) == -1) // precip doesn't have eflags so they can't flip
	{
		basey = FixedToFloat(interp.z + spr->mobj->height);
	}
	else
	{
		basey = FixedToFloat(interp.z);
	}

	lowy = wallVerts[0].y;

	// Rotate sprites to fully billboard with the camera
	// X, Y, AND Z need to be manipulated for the polys to rotate around the
	// origin, because of how the origin setting works I believe that should
	// be mobj->z or mobj->z + mobj->height
	wallVerts[2].y = wallVerts[3].y = (spr->gzt - basey) * gl_viewludsin + basey;
	wallVerts[0].y = wallVerts[1].y = (lowy - basey) * gl_viewludsin + basey;

	// translate back to be around 0 before translating back
	wallVerts[3].x += ((spr->gzt - basey) * gl_viewludcos) * gl_viewcos;
	wallVerts[2].x += ((spr->gzt - basey) * gl_viewludcos) * gl_viewcos;

	wallVerts[0].x += ((lowy - basey) * gl_viewludcos) * gl_viewcos;
	wallVerts[1].x += ((lowy - basey) * gl_viewludcos) * gl_viewcos;

	wallVerts[3].z += ((spr->gzt - basey) * gl_viewludcos) * gl_viewsin;
	wallVerts[2].z += ((spr->gzt - basey) * gl_viewludcos) * gl_viewsin;

	wallVerts[0].z += ((lowy - basey) * gl_viewludcos) * gl_viewsin;
	wallVerts[1].z += ((lowy - basey) * gl_viewludcos) * gl_viewsin;
}

static inline void HWR_ApplyDispoffset(gl_vissprite_t *spr, FOutVector *wallVerts, const boolean papersprite)
{
	HWR_RotateSpritePolyToAim(spr, wallVerts, false, papersprite);

	float sprdist = sqrtf((spr->x1 - gl_viewx)*(spr->x1 - gl_viewx) + (spr->z1 - gl_viewy)*(spr->z1 - gl_viewy) + (spr->gzt - gl_viewz)*(spr->gzt - gl_viewz));
	float distfact = ((2.0f*spr->dispoffset) + 20.0f) / sprdist;

	for (size_t i = 0; i < 4; i++)
	{
		wallVerts[i].x += (gl_viewx - wallVerts[i].x)*distfact;
		wallVerts[i].z += (gl_viewy - wallVerts[i].z)*distfact;
		wallVerts[i].y += (gl_viewz - wallVerts[i].y)*distfact;
	}
}

static void HWR_SplitSprite(gl_vissprite_t *spr, const boolean papersprite)
{
	FOutVector wallVerts[4];
	FOutVector baseWallVerts[4]; // This is what the verts should end up as
	patch_t *gpatch;
	GLPatch_t *hwrpatch;
	FSurfaceInfo Surf;
	extracolormap_t *colormap;
	INT32 lightlevel;
	boolean lightset = true;
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

	gpatch = spr->gpatch;

	// cache the patch in the graphics card memory
	//12/12/99: Hurdler: same comment as above (for md2)
	//Hurdler: 25/04/2000: now support colormap in hardware mode
	HWR_GetMappedPatch(gpatch, spr->colormap);

	hwrpatch = ((GLPatch_t *)gpatch->hardware);

	// Draw shadow BEFORE sprite
	if (cv_shadow.value // Shadows enabled
		&& (spr->mobj->flags & (MF_SCENERY|MF_SPAWNCEILING|MF_NOGRAVITY)) != (MF_SCENERY|MF_SPAWNCEILING|MF_NOGRAVITY) // Ceiling scenery have no shadow.
		&& !(spr->mobj->flags2 & MF2_DEBRIS) // Debris have no corona or shadow.
		&& (spr->mobj->z >= spr->mobj->floorz)) // Without this, your shadow shows on the floor, even after you die and fall through the ground.
	{
		////////////////////
		// SHADOW SPRITE! //
		////////////////////
		HWR_DrawSpriteShadow(spr, gpatch, hwrpatch);
	}

	baseWallVerts[0].x = baseWallVerts[3].x = spr->x1;
	baseWallVerts[2].x = baseWallVerts[1].x = spr->x2;
	baseWallVerts[0].z = baseWallVerts[3].z = spr->z1;
	baseWallVerts[1].z = baseWallVerts[2].z = spr->z2;

	baseWallVerts[2].y = baseWallVerts[3].y = spr->gzt;
	baseWallVerts[0].y = baseWallVerts[1].y = spr->gz;

	v1x = FloatToFixed(spr->x1);
	v1y = FloatToFixed(spr->z1);
	v2x = FloatToFixed(spr->x2);
	v2y = FloatToFixed(spr->z2);

	if (spr->flip)
	{
		baseWallVerts[0].s = baseWallVerts[3].s = hwrpatch->max_s;
		baseWallVerts[2].s = baseWallVerts[1].s = 0;
	}
	else
	{
		baseWallVerts[0].s = baseWallVerts[3].s = 0;
		baseWallVerts[2].s = baseWallVerts[1].s = hwrpatch->max_s;
	}

	// flip the texture coords (look familiar?)
	if (spr->vflip)
	{
		baseWallVerts[3].t = baseWallVerts[2].t = hwrpatch->max_t;
		baseWallVerts[0].t = baseWallVerts[1].t = 0;
	}
	else
	{
		baseWallVerts[3].t = baseWallVerts[2].t = 0;
		baseWallVerts[0].t = baseWallVerts[1].t = hwrpatch->max_t;
	}

	// push it toward the camera to mitigate floor-clipping sprites
	HWR_ApplyDispoffset(spr, baseWallVerts, papersprite);

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
	memcpy(wallVerts, baseWallVerts, sizeof(baseWallVerts));

	INT32 blendmode;
	if (spr->mobj->frame & FF_BLENDMASK)
		blendmode = ((spr->mobj->frame & FF_BLENDMASK) >> FF_BLENDSHIFT) + 1;
	else
		blendmode = spr->mobj->blendmode;

	if (UNLIKELY(!cv_translucency.value)) // translucency disabled
	{
		Surf.PolyColor.s.alpha = 0xFF;
		blend = PF_Translucent|PF_Occlude;
	}
	else if (spr->mobj->flags2 & MF2_SHADOW)
	{
		Surf.PolyColor.s.alpha = 0x40;
		blend = HWR_GetBlendModeFlag(blendmode);
	}
	else if (spr->mobj->frame & FF_TRANSMASK)
	{
		INT32 trans = (spr->mobj->frame & FF_TRANSMASK)>>FF_TRANSSHIFT;
		blend = HWR_SurfaceBlend(blendmode, trans, &Surf);
	}
	else
	{
		// BP: i agree that is little better in environement but it don't
		//     work properly under glide nor with fogcolor to ffffff :(
		// Hurdler: PF_Environement would be cool, but we need to fix
		//          the issue with the fog before
		Surf.PolyColor.s.alpha = 0xFF;
		blend = HWR_GetBlendModeFlag(blendmode)|PF_Occlude;
	}

	if (cv_playerfade.value && spr->mobj->player)
		Surf.PolyColor.s.alpha = FixedMul(R_DoPlayerFade(spr->mobj), Surf.PolyColor.s.alpha);

	if (HWR_UseShader())
	{
		shader = SHADER_SPRITE;
		blend |= PF_ColorMapped;
	}

	alpha = Surf.PolyColor.s.alpha;

	// Start with the lightlevel and colormap from the top of the sprite
	lightlevel = *list[sector->numlights - 1].lightlevel;

	colormap = list[sector->numlights - 1].extra_colormap;
	i = 0;
	temp = FloatToFixed(realtop);

	lightset = HWR_OverrideObjectLightLevel(spr->mobj, &lightlevel);

	for (i = 1; i < sector->numlights; i++)
	{
		fixed_t h = P_GetLightZAt(&sector->lightlist[i], spr->mobj->x, spr->mobj->y);
		if (!(h <= temp))
			continue;

		if (!lightset)
			lightlevel = min(*list[i-1].lightlevel, 255);
		colormap = list[i-1].extra_colormap;
		break;
	}

	if (!lightset)
		HWR_ObjectLightLevelPost(spr, sector, &lightlevel, false);

	for (i = 0; i < sector->numlights; i++)
	{
		if ((endtop < endrealbot) && (top < realbot))
			return;

		// even if we aren't changing colormap or lightlevel, we still need to continue drawing down the sprite
		if (!(list[i].flags & FF_NOSHADE) && (list[i].flags & FF_CUTSPRITES))
		{
			if (!lightset)
			{
				lightlevel = min(*list[i].lightlevel, 255);
				HWR_ObjectLightLevelPost(spr, sector, &lightlevel, false);
			}

			colormap = list[i].extra_colormap;
		}

		if (i + 1 < sector->numlights)
		{
			temp = P_GetLightZAt(&list[i+1], v1x, v1y);
			bheight = FixedToFloat(temp);
			temp = P_GetLightZAt(&list[i+1], v2x, v2y);
			endbheight = FixedToFloat(temp);
		}
		else
		{
			bheight = realbot;
			endbheight = endrealbot;
		}

		if (endbheight >= endtop)
			continue;

		if (bheight >= top)
			continue;

		// Found a break
		// The heights are clamped to ensure the polygon doesn't cross itself.
		bot = CLAMP(bheight, realbot, top);
		endbot = CLAMP(endbheight, endrealbot, endtop);

		wallVerts[3].t = towtop + ((realtop - top) * towmult);
		wallVerts[2].t = towtop + ((endrealtop - endtop) * towmult);
		wallVerts[0].t = towtop + ((realtop - bot) * towmult);
		wallVerts[1].t = towtop + ((endrealtop - endbot) * towmult);

		wallVerts[3].y = top;
		wallVerts[2].y = endtop;
		wallVerts[0].y = bot;
		wallVerts[1].y = endbot;

		// The x and y only need to be adjusted in the case that it's not a papersprite
		if (cv_glspritebillboarding.value && spr->mobj && !papersprite)
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

		HWR_Lighting(&Surf, lightlevel, colormap, P_SectorUsesDirectionalLighting(sector) && !(spr->mobj->frame & FF_FULLBRIGHT));

		Surf.PolyColor.s.alpha = alpha;

		HWR_ProcessPolygon(&Surf, wallVerts, 4, blend|PF_Modulated, shader, false); // sprite shader

		top = bot;
		endtop = endbot;
	}

	bot = realbot;
	endbot = endrealbot;

	if ((endtop <= endrealbot) && (top <= realbot))
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

	HWR_Lighting(&Surf, lightlevel, colormap, P_SectorUsesDirectionalLighting(sector));

	Surf.PolyColor.s.alpha = alpha;

	HWR_ProcessPolygon(&Surf, wallVerts, 4, blend|PF_Modulated, shader, false); // sprite shader
}

// -----------------+
// HWR_DrawSprite   : Draw flat sprites
//                  : (monsters, bonuses, weapons, lights, ...)
// Returns          :
// -----------------+
static void HWR_DrawSprite(gl_vissprite_t *spr)
{
	FOutVector wallVerts[4];
	patch_t *gpatch; // sprite patch converted to hardware
	GLPatch_t *hwrpatch;
	FSurfaceInfo Surf;
	FBITFIELD blend = 0;

	INT32 shader = SHADER_NONE;

	if (!spr->mobj || !spr->mobj->subsector)
		return;

	const boolean papersprite = (spr->mobj->frame & FF_PAPERSPRITE);
	sector_t *sector = spr->mobj->subsector->sector;

	if (sector->numlights)
	{
		HWR_SplitSprite(spr, papersprite);
		return;
	}

	// cache sprite graphics
	//12/12/99: Hurdler:
	//          OK, I don't change anything for MD2 support because I want to be
	//          sure to do it the right way. So actually, we keep normal sprite
	//          in memory and we add the md2 model if it exists for that sprite

	gpatch = spr->gpatch;

	// cache the patch in the graphics card memory
	//12/12/99: Hurdler: same comment as above (for md2)
	//Hurdler: 25/04/2000: now support colormap in hardware mode
	HWR_GetMappedPatch(gpatch, spr->colormap);

	hwrpatch = ((GLPatch_t *)gpatch->hardware);

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
		wallVerts[0].s = wallVerts[3].s = hwrpatch->max_s;
		wallVerts[2].s = wallVerts[1].s = 0;
	}
	else
	{
		wallVerts[0].s = wallVerts[3].s = 0;
		wallVerts[2].s = wallVerts[1].s = hwrpatch->max_s;
	}

	// flip the texture coords (look familiar?)
	if (spr->vflip)
	{
		wallVerts[3].t = wallVerts[2].t = hwrpatch->max_t;
		wallVerts[0].t = wallVerts[1].t = 0;
	}
	else
	{
		wallVerts[3].t = wallVerts[2].t = 0;
		wallVerts[0].t = wallVerts[1].t = hwrpatch->max_t;
	}

	// Draw shadow BEFORE sprite
	if (cv_shadow.value // Shadows enabled
		&& (spr->mobj->flags & (MF_SCENERY|MF_SPAWNCEILING|MF_NOGRAVITY)) != (MF_SCENERY|MF_SPAWNCEILING|MF_NOGRAVITY) // Ceiling scenery have no shadow.
		&& !(spr->mobj->flags2 & MF2_DEBRIS) // Debris have no corona or shadow.
		&& (spr->mobj->z >= spr->mobj->floorz)) // Without this, your shadow shows on the floor, even after you die and fall through the ground.
	{
		////////////////////
		// SHADOW SPRITE! //
		////////////////////
		HWR_DrawSpriteShadow(spr, gpatch, hwrpatch);
	}

	// push it toward the camera to mitigate floor-clipping sprites
	HWR_ApplyDispoffset(spr, wallVerts, papersprite);

	// This needs to be AFTER the shadows so that the regular sprites aren't drawn completely black.
	// sprite lighting by modulating the RGB components
	/// \todo coloured

	// colormap test
	INT32 lightlevel = 255;
	boolean lightset = HWR_OverrideObjectLightLevel(spr->mobj, &lightlevel);
	extracolormap_t *colormap = sector->extra_colormap;
	const boolean fullbright = R_ThingIsFullBright(spr->mobj);

	if (!lightset)
	{
		lightlevel = min(sector->lightlevel, 255);
		HWR_ObjectLightLevelPost(spr, sector, &lightlevel, false);
	}

	HWR_Lighting(&Surf, lightlevel, colormap, P_SectorUsesDirectionalLighting(sector) && !fullbright);

	INT32 blendmode;
	if (spr->mobj->frame & FF_BLENDMASK)
		blendmode = ((spr->mobj->frame & FF_BLENDMASK) >> FF_BLENDSHIFT) + 1;
	else
		blendmode = spr->mobj->blendmode;

	if (UNLIKELY(!cv_translucency.value)) // translucency disabled
	{
		Surf.PolyColor.s.alpha = 0xFF;
		blend = PF_Translucent|PF_Occlude;
	}
	else if (spr->mobj->flags2 & MF2_SHADOW)
	{
		Surf.PolyColor.s.alpha = 0x40;
		blend = HWR_GetBlendModeFlag(blendmode);
	}
	else if (spr->mobj->frame & FF_TRANSMASK)
	{
		INT32 trans = (spr->mobj->frame & FF_TRANSMASK)>>FF_TRANSSHIFT;
		blend = HWR_SurfaceBlend(blendmode, trans, &Surf);
	}
	else
	{
		// BP: i agree that is little better in environement but it don't
		//     work properly under glide nor with fogcolor to ffffff :(
		// Hurdler: PF_Environement would be cool, but we need to fix
		//          the issue with the fog before
		Surf.PolyColor.s.alpha = 0xFF;
		blend = HWR_GetBlendModeFlag(blendmode)|PF_Occlude;
	}

	if (cv_playerfade.value && spr->mobj->player)
		Surf.PolyColor.s.alpha = FixedMul(R_DoPlayerFade(spr->mobj), Surf.PolyColor.s.alpha);

	if (HWR_UseShader())
	{
		shader = SHADER_SPRITE;
		blend |= PF_ColorMapped;
	}

	HWR_ProcessPolygon(&Surf, wallVerts, 4, blend|PF_Modulated, shader, false);
}

// Sprite drawer for precipitation
static void HWR_DrawPrecipitationSprite(gl_vissprite_t *spr)
{
	FBITFIELD blend = 0;
	FOutVector wallVerts[4];
	patch_t *gpatch; // sprite patch converted to hardware
	GLPatch_t *hwrpatch;
	FSurfaceInfo Surf;

	INT32 shader = SHADER_NONE;

	if (!spr->mobj || !spr->mobj->subsector)
		return;

	// cache sprite graphics
	gpatch = spr->gpatch;

	// cache the patch in the graphics card memory
	//12/12/99: Hurdler: same comment as above (for md2)
	//Hurdler: 25/04/2000: now support colormap in hardware mode
	HWR_GetMappedPatch(gpatch, spr->colormap);

	hwrpatch = ((GLPatch_t *)gpatch->hardware);

	// create the sprite billboard
	//
	//  3--2
	//  | /|
	//  |/ |
	//  0--1
	wallVerts[0].x = wallVerts[3].x = spr->x1;
	wallVerts[2].x = wallVerts[1].x = spr->x2;
	wallVerts[2].y = wallVerts[3].y = spr->gzt;
	wallVerts[0].y = wallVerts[1].y = spr->gz;

	// make a wall polygon (with 2 triangles), using the floor/ceiling heights,
	// and the 2d map coords of start/end vertices
	wallVerts[0].z = wallVerts[3].z = spr->z1;
	wallVerts[1].z = wallVerts[2].z = spr->z2;

	HWR_RotateSpritePolyToAim(spr, wallVerts, true, false);

	wallVerts[0].s = wallVerts[3].s = 0;
	wallVerts[2].s = wallVerts[1].s = hwrpatch->max_s;

	wallVerts[3].t = wallVerts[2].t = 0;
	wallVerts[0].t = wallVerts[1].t = hwrpatch->max_t;

	// colormap test
	sector_t *sector = spr->mobj->subsector->sector;
	UINT8 lightlevel = 255;
	extracolormap_t *colormap = sector->extra_colormap;

	if (sector->numlights)
	{
		INT32 light;

		light = R_GetPlaneLight(sector, spr->mobj->z + spr->mobj->height, false); // Always use the light at the top instead of whatever I was doing before

		if (!(spr->mobj->frame & FF_FULLBRIGHT))
			lightlevel = min(*sector->lightlist[light].lightlevel, 255);

		if (sector->lightlist[light].extra_colormap)
			colormap = sector->lightlist[light].extra_colormap;
	}
	else
	{
		if (!(spr->mobj->frame & FF_FULLBRIGHT))
			lightlevel = min(sector->lightlevel, 255);

		if (sector->extra_colormap)
			colormap = sector->extra_colormap;
	}

	HWR_Lighting(&Surf, lightlevel, colormap, P_SectorUsesDirectionalLighting(sector));

	if (spr->mobj->frame & FF_TRANSMASK)
	{
		INT32 trans = (spr->mobj->frame & FF_TRANSMASK)>>FF_TRANSSHIFT;
		blend = HWR_SurfaceBlend(AST_TRANSLUCENT, trans, &Surf);
	}
	else
	{
		// BP: i agree that is little better in environement but it don't
		//     work properly under glide nor with fogcolor to ffffff :(
		// Hurdler: PF_Environement would be cool, but we need to fix
		//          the issue with the fog before
		Surf.PolyColor.s.alpha = 0xFF;
		blend = HWR_GetBlendModeFlag(AST_TRANSLUCENT)|PF_Occlude;
	}

	if (HWR_UseShader())
	{
		shader = SHADER_SPRITE;
		blend |= PF_ColorMapped;
	}

	HWR_ProcessPolygon(&Surf, wallVerts, 4, blend|PF_Modulated, shader, false);
}

// --------------------------------------------------------------------------
// Sort vissprites by distance
// --------------------------------------------------------------------------

gl_vissprite_t* gl_vsprorder[MAXVISSPRITES];

// For more correct transparency the transparent sprites would need to be
// sorted and drawn together with transparent surfaces.
static int CompareVisSprites(const void *p1, const void *p2)
{
	gl_vissprite_t* spr1 = *(gl_vissprite_t*const*)p1;
	gl_vissprite_t* spr2 = *(gl_vissprite_t*const*)p2;
	int idiff;
	float fdiff;

	// make transparent sprites last
	// "boolean to int"

	// check for precip first, because then sprX->mobj is actually a precipmobj_t and does not have flags2 or tracer
	int transparency1 = (!spr1->precip && (spr1->mobj->flags2 & MF2_SHADOW)) || (spr1->mobj->frame & FF_TRANSMASK);
	int transparency2 = (!spr2->precip && (spr2->mobj->flags2 & MF2_SHADOW)) || (spr2->mobj->frame & FF_TRANSMASK);

	idiff = transparency1 - transparency2;
	if (idiff != 0) return idiff;

	fdiff = spr2->tz - spr1->tz;// this order seems correct when checking with apitrace. Back to front.
	if (fabsf(fdiff) < 1.0E-36f)
		return spr1->dispoffset - spr2->dispoffset;// smallest dispoffset first if sprites are at (almost) same location.
	else if (fdiff > 0)
		return 1;
	else
		return -1;
}

static void HWR_SortVisSprites(void)
{
	UINT32 i;
	for (i = 0; i < gl_visspritecount; i++)
	{
		gl_vsprorder[i] = HWR_GetVisSprite(i);
	}

	qs22j(gl_vsprorder, gl_visspritecount, sizeof(gl_vissprite_t*), CompareVisSprites);
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
} gl_drawnode_type_t;

typedef struct
{
	gl_drawnode_type_t type;
	union {
		planeinfo_t plane;
		polyplaneinfo_t polyplane;
		wallinfo_t wall;
	} u;
} gl_drawnode_t;

// initial size of drawnode array
#define DRAWNODES_INIT_SIZE 64
gl_drawnode_t *drawnodes = NULL;
INT32 numdrawnodes = 0;
INT32 alloceddrawnodes = 0;

static void *HWR_CreateDrawNode(gl_drawnode_type_t type)
{
	gl_drawnode_t *drawnode;

	if (!drawnodes)
	{
		alloceddrawnodes = DRAWNODES_INIT_SIZE;
		drawnodes = Z_Malloc(alloceddrawnodes * sizeof(gl_drawnode_t), PU_LEVEL, &drawnodes);
	}
	else if (numdrawnodes >= alloceddrawnodes)
	{
		alloceddrawnodes *= 2;
		Z_Realloc(drawnodes, alloceddrawnodes * sizeof(gl_drawnode_t), PU_LEVEL, &drawnodes);
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

static void HWR_AddTransparentWall(FOutVector *wallVerts, FSurfaceInfo *pSurf, INT32 texnum, boolean noencore, FBITFIELD blend, boolean fogwall, INT32 lightlevel, extracolormap_t *wallcolormap)
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

static void HWR_AddTransparentFloor(lumpnum_t lumpnum, extrasubsector_t *xsub, boolean isceiling, fixed_t fixedheight, INT32 lightlevel, INT32 alpha, sector_t *FOFSector, FBITFIELD blend, boolean fogplane, extracolormap_t *planecolormap)
{
	planeinfo_t *planeinfo = HWR_CreateDrawNode(DRAWNODE_PLANE);

	planeinfo->isceiling = isceiling;
	planeinfo->fixedheight = fixedheight;
	planeinfo->lightlevel = (HWR_ShouldUsePaletteRendering() && (planecolormap && (planecolormap->fog & 2))) ? 255 : lightlevel;
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
static void HWR_AddTransparentPolyobjectFloor(lumpnum_t lumpnum, polyobj_t *polysector, boolean isceiling, fixed_t fixedheight, INT32 lightlevel, INT32 alpha, sector_t *FOFSector, FBITFIELD blend, extracolormap_t *planecolormap)
{
	polyplaneinfo_t *polyplaneinfo = HWR_CreateDrawNode(DRAWNODE_POLYOBJECT_PLANE);

	polyplaneinfo->isceiling = isceiling;
	polyplaneinfo->fixedheight = fixedheight;
	polyplaneinfo->lightlevel = (HWR_ShouldUsePaletteRendering() && (planecolormap && (planecolormap->fog & 2))) ? 255 : lightlevel;
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
	GL_SetTransform(&atransform);

	for (i = 0; i < numdrawnodes; i++)
	{
		gl_drawnode_t *drawnode = &drawnodes[sortindex[i]];

		switch (drawnode->type)
		{
			case DRAWNODE_PLANE:
				{
					planeinfo_t *plane = &drawnode->u.plane;

					// We aren't traversing the BSP tree, so make gl_frontsector null to avoid crashes.
					gl_frontsector = NULL;

					if (!(plane->blend & PF_NoTexture))
						HWR_GetFlat(plane->lumpnum,  R_NoEncore(plane->FOFSector, plane->isceiling));

					HWR_RenderPlane(NULL, plane->xsub, plane->isceiling, plane->fixedheight, plane->blend, plane->lightlevel,
									plane->lumpnum, plane->FOFSector, plane->alpha, plane->planecolormap);
				}
				break;
			case DRAWNODE_POLYOBJECT_PLANE:
				{
					polyplaneinfo_t *polyplane = &drawnode->u.polyplane;

					// We aren't traversing the BSP tree, so make gl_frontsector null to avoid crashes.
					gl_frontsector = NULL;

					if (!(polyplane->blend & PF_NoTexture))
						HWR_GetFlat(polyplane->lumpnum,  R_NoEncore(polyplane->FOFSector, polyplane->isceiling));

					HWR_RenderPolyObjectPlane(polyplane->polysector, polyplane->isceiling, polyplane->fixedheight, polyplane->blend, polyplane->lightlevel,
											polyplane->lumpnum, polyplane->FOFSector, polyplane->alpha, polyplane->planecolormap);

				}
				break;
			case DRAWNODE_WALL:
				{
					wallinfo_t *wall = &drawnode->u.wall;

					if (!(wall->blend & PF_NoTexture))
						HWR_GetTexture(wall->texnum, wall->noencore);

					HWR_RenderWall(wall->wallVerts, &wall->Surf, wall->blend, wall->fogwall,
								wall->lightlevel, wall->wallcolormap);
				}
				break;
			default:
				break;
		}
	}

	PS_STOP_TIMING(ps_hw_nodedrawtime);

	numdrawnodes = 0;

	Z_Free(sortindex);
}


// --------------------------------------------------------------------------
//  Draw all vissprites
// --------------------------------------------------------------------------
static void HWR_DrawSprites(void)
{
	UINT32 i;

	for (i = 0; i < gl_visspritecount; i++)
	{
		gl_vissprite_t *spr = gl_vsprorder[i];

		if (spr->precip)
		{
			HWR_DrawPrecipitationSprite(spr);
			continue;
		}

		HWR_DrawSprite(spr);
	}
}

static void HWR_DrawModels(void)
{
	UINT32 i;

	for (i = 0; i < gl_visspritecount; i++)
	{
		gl_vissprite_t *spr = gl_vsprorder[i];

		if (spr->precip)
		{
			HWR_DrawPrecipitationSprite(spr);
			continue;
		}

		if (!spr->mobj)
			continue;

		if (spr->mobj->skin && spr->mobj->sprite == SPR_PLAY)
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
			if (((md2->notfound || md2->scale < 0.0f) && ((!cv_glfallbackplayermodel.value) || md2_models[SPR_PLAY].notfound || md2_models[SPR_PLAY].scale < 0.0f)) || spr->mobj->state == &states[S_PLAY_SIGN])
				HWR_DrawSprite(spr);
			else
				HWR_DrawMD2(spr);
		}
		else
		{
			if (md2_models[spr->mobj->sprite].notfound || md2_models[spr->mobj->sprite].scale < 0.0f)
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
static void HWR_AddSprites(sector_t *sec)
{
	mobj_t *thing;
	INT32 limit_dist;

	// BSP is traversed by subsector.
	// A sector might have been split into several
	// subsectors during BSP building.
	// Thus we check whether its already added.
	if (sec->validcount == validcount)
		return;

	// Well, now it will be done.
	sec->validcount = validcount;

	limit_dist = cv_drawdist.value;

	if (current_bsp_culling_distance)
	{
		// Use the smaller setting
		if (limit_dist)
			limit_dist = min(current_bsp_culling_distance/mapobjectscale, limit_dist);
		else
			limit_dist = current_bsp_culling_distance/mapobjectscale;
	}

	// Handle all things in sector.
	for (thing = sec->thinglist; thing; thing = thing->snext)
	{
		if (!R_ThingWithinDist(thing, limit_dist))
			continue;

		if (!R_ThingVisible(thing))
			continue;

		HWR_ProjectSprite(thing);
	}
}

// --------------------------------------------------------------------------
// HWR_AddPrecipitationSprites
// This renders through the blockmap instead of BSP to avoid
// iterating a huge amount of precipitation sprites in sectors
// that are beyond drawdist.
// --------------------------------------------------------------------------
static void HWR_AddPrecipitationSprites(void)
{
	INT32 xl, xh, yl, yh, bx, by;
	precipmobj_t *th, *next;

	fixed_t drawdist;

	// save a little time if theres no or invisible weather
	if (curWeather == PRECIP_NONE || curWeather == PRECIP_BLANK || curWeather == PRECIP_STORM_NORAIN)
	{
		return;
	}

	drawdist = ((fixed_t)(cv_drawdist_precip.value) * (cv_mobjscaleprecip.value ? mapobjectscale : FRACUNIT));

	// No to infinite precipitation draw distance.
	if (drawdist == 0)
	{
		return;
	}

	if (current_bsp_culling_distance)
		drawdist = min((fixed_t)current_bsp_culling_distance, drawdist);

	R_GetRenderBlockMapDimensions(drawdist, &xl, &xh, &yl, &yh);

	for (bx = xl; bx <= xh; bx++)
	{
		for (by = yl; by <= yh; by++)
		{
			for (th = precipblocklinks[(by * bmapwidth) + bx]; th; th = next)
			{
				// Store this beforehand because HWR_ProjectPrecipitationSprite may free th (see P_PrecipThinker)
				next = th->bnext;

				if (th->precipflags & PCF_INVISIBLE)
					continue;

				HWR_ProjectPrecipitationSprite(th);
			}
		}
	}
}

// --------------------------------------------------------------------------
// HWR_ProjectSprite
//  Generates a vissprite for a thing if it might be visible.
// --------------------------------------------------------------------------
// BP why not use xtoviexangle/viewangletox like in bsp ?....
static void HWR_ProjectSprite(mobj_t *thing)
{
	gl_vissprite_t *vis;
	float tr_x, tr_y;
	float tz;
	float x1, x2;
	float z1, z2;
	float rightsin, rightcos;
	float this_scale;
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

	angle_t ang = 0;
#ifdef ROTSPRITE
	angle_t camang = 0;
#endif
	INT32 heightsec, phs;
	INT32 dist = -1;

	fixed_t spr_width, spr_height;
	fixed_t spr_offset, spr_topoffset;
#ifdef ROTSPRITE
	patch_t *rotsprite = NULL;
	INT32 rollangle = 0;
	angle_t pitchnroll = 0;
	angle_t sliptiderollangle = 0;
#endif

	if (!thing || thing->subsector == NULL)
		return;

	// uncapped/interpolation
	interpmobjstate_t interp = {0};

	if (cv_maxinterpdist.value)
		dist = R_QuickCamDist(thing->x, thing->y);

	if (R_UsingFrameInterpolation() && !paused && (!cv_maxinterpdist.value || dist < cv_maxinterpdist.value))
	{
		R_InterpolateMobjState(thing, rendertimefrac, &interp);
	}
	else
	{
		R_InterpolateMobjState(thing, FRACUNIT, &interp);
	}

	if (interp.spritexscale < 1 || interp.spriteyscale < 1)
		return;

	const boolean papersprite = (thing->frame & FF_PAPERSPRITE);

	INT32 blendmode;
	if (thing->frame & FF_BLENDMASK)
		blendmode = ((thing->frame & FF_BLENDMASK) >> FF_BLENDSHIFT) + 1;
	else
		blendmode = thing->blendmode;

	// Visibility check by the blend mode.
	if (thing->frame & FF_TRANSMASK)
	{
		if (!R_BlendLevelVisible(blendmode, (thing->frame & FF_TRANSMASK)>>FF_TRANSSHIFT))
			return;
	}

	// transform the origin point
	tr_x = FixedToFloat(interp.x);
	tr_y = FixedToFloat(interp.y);

	// rotation around vertical axis
	tz = ((tr_x - gl_viewx) * gl_viewcos) + ((tr_y - gl_viewy) * gl_viewsin);

	// thing is behind view plane?
	if (tz < ZCLIP_PLANE && !papersprite && (!cv_glmdls.value || md2_models[thing->sprite].notfound == true)) // Yellow: Only MD2's dont disappear
		return;

	const boolean mirrored = thing->mirrored;
	const boolean vflip = (thing->eflags & MFE_VERTICALFLIP);
	const boolean hflip = (!(thing->frame & FF_HORIZONTALFLIP) != !mirrored);

	this_scale   = FixedToFloat(interp.scale);
	spritexscale = FixedToFloat(interp.spritexscale);
	spriteyscale = FixedToFloat(interp.spriteyscale);

	// decide which patch to use for sprite relative to player
#ifdef RANGECHECK
	if ((unsigned)thing->sprite >= numsprites)
		I_Error("HWR_ProjectSprite: invalid sprite number %i ", thing->sprite);
#endif

	rot = (thing->frame & FF_FRAMEMASK);

#ifdef ROTSPRITE
	// determine here if sprite should rotate for optimization
	const boolean sliprollrotate = (cv_sliptideroll.value && (thing->player && thing->player->sliproll));
	const boolean shouldrotate = (interp.sloperoll || interp.slopepitch || interp.roll || interp.pitch || thing->rollangle || sliprollrotate);
#endif

	//Fab : 02-08-98: 'skin' override spritedef currently used for skin
	if ((thing->skin || thing->localskin) && thing->sprite == SPR_PLAY)
	{
		sprdef = &K_GetMobjSkin(thing)->spritedef;
#ifdef ROTSPRITE
		sprinfo = &K_GetMobjSkin(thing)->sprinfo;
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
		rot = (thing->frame & FF_FRAMEMASK);
		thing->state->sprite = thing->sprite;
		thing->state->frame = thing->frame;
	}

	sprframe = &sprdef->spriteframes[rot];

#ifdef PARANOIA
	if (!sprframe)
		I_Error("sprframes NULL for sprite %d\n", thing->sprite);
#endif

	if (sprframe->rotate != SRF_SINGLE || papersprite
#ifdef ROTSPRITE
		|| (shouldrotate)
#endif
	)
	{
		ang = R_PointToAngle(interp.x, interp.y);

#ifdef ROTSPRITE
		camang = ang;
#endif

		ang -= interp.angle;

		if (mirrored)
			ang = InvAngle(ang);
	}

	if (sprframe->rotate == SRF_SINGLE)
	{
		// use single rotation for all views
		rot = 0;                        //Fab: for vis->patch below
		lumpoff = sprframe->lumpid[0];     //Fab: see note above
		flip = sprframe->flip; // Will only be 0x00 or 0xFF

		if (papersprite && ang < ANGLE_180)
			flip ^= 0xFFFF;
	}
	else
	{
		// choose a different rotation based on player view
		if ((sprframe->rotate & SRF_RIGHT) && (ang < ANGLE_180)) // See from right
			rot = 6; // F7 slot
		else if ((sprframe->rotate & SRF_LEFT) && (ang >= ANGLE_180)) // See from left
			rot = 2; // F3 slot
		else // Normal behaviour
			rot = (ang+ANGLE_202h)>>29;

		//Fab: lumpid is the index for spritewidth,spriteoffset... tables
		lumpoff = sprframe->lumpid[rot];
		flip = sprframe->flip & (1<<rot);

		if (papersprite && ang < ANGLE_180)
			flip ^= (1<<rot);
	}

	if ((thing->skin || thing->localskin) && K_GetMobjSkin(thing)->flags & SF_HIRES)
		this_scale *= FixedToFloat(K_GetMobjSkin(thing)->highresscale);

	spr_width = spritecachedinfo[lumpoff].width;
	spr_height = spritecachedinfo[lumpoff].height;
	spr_offset = spritecachedinfo[lumpoff].offset;
	spr_topoffset = spritecachedinfo[lumpoff].topoffset;

#ifdef ROTSPRITE
	if (shouldrotate)
	{
		// this is very messy, but it on-the-fly calculates rotations for all the
		// pitch and roll variables
		pitchnroll = R_RotationAngle(ang, camang, &interp);
		rollangle = thing->rollangle;

		if (rollangle || pitchnroll || sliprollrotate)
		{
			if (sliprollrotate)
			{
				sliptiderollangle = thing->player->sliproll * thing->player->kartstuff[k_aizdriftstrat];
				pitchnroll += rollangle + FixedMul(FINECOSINE((ang) >> ANGLETOFINESHIFT), sliptiderollangle);
			}
			else
				pitchnroll += rollangle;

			rollangle = R_GetRollAngle(pitchnroll);
			rotsprite = Patch_GetRotatedSprite(sprframe, (thing->frame & FF_FRAMEMASK), rot, flip, sprinfo, rollangle);

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

	spr_offset += interp.spritexoffset;
	spr_topoffset += interp.spriteyoffset;

	if (papersprite)
	{
		rightsin = FixedToFloat(FINESINE(interp.angle >> ANGLETOFINESHIFT));
		rightcos = FixedToFloat(FINECOSINE(interp.angle >> ANGLETOFINESHIFT));
	}
	else
	{
		rightsin = FixedToFloat(FINESINE((viewangle + ANGLE_90)>>ANGLETOFINESHIFT));
		rightcos = FixedToFloat(FINECOSINE((viewangle + ANGLE_90)>>ANGLETOFINESHIFT));
	}

	spritexscale *= this_scale;
	spriteyscale *= this_scale;

	flip = !flip != !hflip;

	if (flip)
	{
		x1 = (FixedToFloat(spr_width - spr_offset) * spritexscale);
		x2 = (FixedToFloat(spr_offset) * spritexscale);
	}
	else
	{
		x1 = (FixedToFloat(spr_offset) * spritexscale);
		x2 = (FixedToFloat(spr_width - spr_offset) * spritexscale);
	}

	z1 = tr_y + x1 * rightsin;
	z2 = tr_y - x2 * rightsin;
	x1 = tr_x + x1 * rightcos;
	x2 = tr_x - x2 * rightcos;

	if (vflip)
	{
		gz = FixedToFloat(interp.z + thing->height) - (FixedToFloat(spr_topoffset) * spriteyscale);
		gzt = gz + (FixedToFloat(spr_height) * spriteyscale);
	}
	else
	{
		gzt = FixedToFloat(interp.z) + (FixedToFloat(spr_topoffset) * spriteyscale);
		gz = gzt - (FixedToFloat(spr_height) * spriteyscale);
	}

	if (thing->subsector->sector->cullheight)
	{
		if (HWR_DoCulling(thing->subsector->sector->cullheight, viewsector->cullheight, gl_viewz, gz, gzt))
			return;
	}

	heightsec = thing->subsector->sector->heightsec;
	if (viewplayer && viewplayer->mo && viewplayer->mo->subsector)
		phs = viewplayer->mo->subsector->sector->heightsec;
	else
		phs = -1;

	if (heightsec != -1 && phs != -1) // only clip things which are in special sectors
	{
		if (gl_viewz < FixedToFloat(sectors[phs].floorheight) ?
			FixedToFloat(interp.z) >= FixedToFloat(sectors[heightsec].floorheight) :
			gzt < FixedToFloat(sectors[heightsec].floorheight))
			return;
		if (gl_viewz > FixedToFloat(sectors[phs].ceilingheight) ?
			gzt < FixedToFloat(sectors[heightsec].ceilingheight) && gl_viewz >= FixedToFloat(sectors[heightsec].ceilingheight) :
			FixedToFloat(interp.z) >= FixedToFloat(sectors[heightsec].ceilingheight))
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
	vis->spritexoffset = FixedToFloat(spr_offset);
	vis->spriteyoffset = FixedToFloat(spr_topoffset);

#ifdef ROTSPRITE
	if (rotsprite != NULL)
		vis->gpatch = (patch_t *)rotsprite;
	else
#endif
		vis->gpatch = (patch_t *)W_CachePatchNum(sprframe->lumppat[rot], PU_SPRITE);

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
			vis->colormap += COLORMAP_REMAPOFFSET;
#endif
	}

	// set top/bottom coords
	vis->gzt = gzt;
	vis->gz = gz;

	//CONS_Debug(DBG_RENDER, "------------------\nH: sprite  : %d\nH: frame   : %x\nH: type    : %d\nH: sname   : %s\n\n",
	//            thing->sprite, thing->frame, thing->type, sprnames[thing->sprite]);

	vis->vflip = vflip;

	vis->precip = false;
}

// Precipitation projector for hardware mode
static void HWR_ProjectPrecipitationSprite(precipmobj_t *thing)
{
	gl_vissprite_t *vis;
	float tr_x, tr_y;
	float tz;
	float x1, x2;
	float z1, z2;
	float rightsin, rightcos;
	float this_scale;
	spritedef_t *sprdef;
	spriteframe_t *sprframe;
	size_t lumpoff;
	unsigned rot = 0;
	UINT8 flip;
	INT32 dist = -1;

	if (!thing)
		return;

	// okay... this is a hack, but weather isn't networked, so it should be ok
	if (!P_PrecipThinker(thing))
	{
		return;
	}

	// uncapped/interpolation
	interpmobjstate_t interp = {0};

	if (cv_maxinterpdist.value)
		dist = R_QuickCamDist(thing->x, thing->y);

	// do interpolation
	if (R_UsingFrameInterpolation() && !paused && (!cv_maxinterpdist.value || dist < cv_maxinterpdist.value))
	{
		R_InterpolatePrecipMobjState(thing, rendertimefrac, &interp);
	}
	else
	{
		R_InterpolatePrecipMobjState(thing, FRACUNIT, &interp);
	}

	// Visibility check by the blend mode.
	if (thing->frame & FF_TRANSMASK)
	{
		if (!R_BlendLevelVisible(thing->blendmode, (thing->frame & FF_TRANSMASK)>>FF_TRANSSHIFT))
			return;
	}

	// transform the origin point
	tr_x = FixedToFloat(interp.x);
	tr_y = FixedToFloat(interp.y);

	// rotation around vertical axis
	tz = ((tr_x - gl_viewx) * gl_viewcos) + ((tr_y - gl_viewy) * gl_viewsin);

	// thing is behind view plane?
	if (tz < ZCLIP_PLANE)
		return;

	// decide which patch to use for sprite relative to player
	if ((unsigned)thing->sprite >= numsprites)
	{
#ifdef RANGECHECK
		I_Error("HWR_ProjectPrecipitationSprite: invalid sprite number %i ",
		        thing->sprite);
#else
		return;
#endif
	}

	sprdef = &sprites[thing->sprite];

	if ((size_t)(thing->frame&FF_FRAMEMASK) >= sprdef->numframes)
	{
#ifdef RANGECHECK
		I_Error("HWR_ProjectPrecipitationSprite: invalid sprite frame %i : %i for %s",
		        thing->sprite, thing->frame, sprnames[thing->sprite]);
#else
		return;
#endif
	}

	this_scale = FixedToFloat(interp.scale);

	sprframe = &sprdef->spriteframes[thing->frame & FF_FRAMEMASK];

	// use single rotation for all views
	lumpoff = sprframe->lumpid[0];
	flip = sprframe->flip; // Will only be 0x00 or 0xFF

	rightsin = FixedToFloat(FINESINE((viewangle + ANGLE_90)>>ANGLETOFINESHIFT));
	rightcos = FixedToFloat(FINECOSINE((viewangle + ANGLE_90)>>ANGLETOFINESHIFT));

	if (flip)
	{
		x1 = FixedToFloat(spritecachedinfo[lumpoff].width - spritecachedinfo[lumpoff].offset);
		x2 = FixedToFloat(spritecachedinfo[lumpoff].offset);
	}
	else
	{
		x1 = FixedToFloat(spritecachedinfo[lumpoff].offset);
		x2 = FixedToFloat(spritecachedinfo[lumpoff].width - spritecachedinfo[lumpoff].offset);
	}

	x1 *= this_scale;
	x2 *= this_scale;

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
	vis->gpatch = (patch_t *)W_CachePatchNum(sprframe->lumppat[rot], PU_SPRITE);
	vis->flip = flip;
	vis->mobj = (mobj_t *)thing;

	vis->colormap = NULL;

#ifdef GLENCORE
	//if (encoremap && !(thing->flags & MF_DONTENCOREMAP))
		//vis->colormap += COLORMAP_REMAPOFFSET;
#endif

	// set top/bottom coords
	vis->gzt = FixedToFloat(interp.z) + (FixedToFloat(spritecachedinfo[lumpoff].topoffset) * this_scale);
	vis->gz = vis->gzt - (FixedToFloat(spritecachedinfo[lumpoff].height) * this_scale);

	vis->precip = true;
}


// ==========================================================================
// Sky dome rendering, ported from PrBoom+
// ==========================================================================

static gl_sky_t gl_sky;

#define DEG2RADGL(a) ((a * M_PIl) / 180.0f)

static void HWR_SkyDomeVertex(gl_sky_t *sky, gl_skyvertex_t *vbo, int r, int c, signed char yflip, float delta, boolean foglayer)
{
	static const float scale = 10000.0f;
	static const float maxSideAngle = DEG2RADGL(60.0f);

	float topAngle = DEG2RADGL(c / (float)sky->columns * 360.0f);
	float sideAngle = (maxSideAngle * (float)(sky->rows - r) / (float)sky->rows);
	float height = (float)(sin(sideAngle));
	float realRadius = (scale * (float)cos(sideAngle));
	float x = (realRadius * (float)cos(topAngle));
	float y = (!yflip) ? scale * height : -scale * height;
	float z = (realRadius * (float)sin(topAngle));
	float timesRepeat = (4 * (256.0f / sky->width));

	if (fpclassify(timesRepeat) == FP_ZERO)
		timesRepeat = 1.0f;

	if (!foglayer)
	{
		vbo->r = 255;
		vbo->g = 255;
		vbo->b = 255;
		vbo->a = (r == 0 ? 0 : 255);

		// And the texture coordinates.
		vbo->u = (-timesRepeat * c / (float)sky->columns);
		if (!yflip)	// Flipped Y is for the lower hemisphere.
			vbo->v = (r / (float)sky->rows) + 0.5f;
		else
			vbo->v = 1.0f + ((sky->rows - r) / (float)sky->rows) + 0.5f;
	}

	if (r != 4)
		y += 300.0f;

	// And finally the vertex.
	vbo->x = x;
	vbo->y = y + delta;
	vbo->z = z;
}

// Clears the sky dome.
void HWR_ClearSkyDome(void)
{
	gl_sky_t *sky = &gl_sky;

	if (sky->loops)
		free(sky->loops);
	if (sky->data)
		free(sky->data);

	sky->loops = NULL;
	sky->data = NULL;

	sky->vbo = 0;
	sky->rows = sky->columns = 0;
	sky->loopcount = 0;

	sky->detail = 0;
	sky->texture = -1;
	sky->width = sky->height = 0;

	sky->rebuild = true;
}

void HWR_BuildSkyDome(void)
{
	int c, r;
	signed char yflip;
	int row_count = 4;
	int col_count = 4;
	float delta;

	gl_sky_t *sky = &gl_sky;
	gl_skyvertex_t *vertex_p;
	texture_t *texture = textures[texturetranslation[skytexture]];

	sky->detail = 16;
	col_count *= sky->detail;

	if ((sky->columns != col_count) || (sky->rows != row_count))
		HWR_ClearSkyDome();

	sky->columns = col_count;
	sky->rows = row_count;
	sky->vertex_count = 2 * sky->rows * (sky->columns * 2 + 2) + sky->columns * 2;

	if (!sky->loops)
		sky->loops = malloc((sky->rows * 2 + 2) * sizeof(sky->loops[0]));

	// create vertex array
	if (!sky->data)
		sky->data = malloc(sky->vertex_count * sizeof(sky->data[0]));

	sky->texture = texturetranslation[skytexture];
	sky->width = texture->width;
	sky->height = texture->height;

	vertex_p = &sky->data[0];
	sky->loopcount = 0;

	for (yflip = 0; yflip < 2; yflip++)
	{
		sky->loops[sky->loopcount].mode = HWD_SKYLOOP_FAN;
		sky->loops[sky->loopcount].vertexindex = vertex_p - &sky->data[0];
		sky->loops[sky->loopcount].vertexcount = col_count;
		sky->loops[sky->loopcount].use_texture = false;
		sky->loopcount++;

		delta = 0.0f;

		for (c = 0; c < col_count; c++)
		{
			HWR_SkyDomeVertex(sky, vertex_p, 1, c, yflip, 0.0f, true);
			vertex_p->r = 255;
			vertex_p->g = 255;
			vertex_p->b = 255;
			vertex_p->a = 255;
			vertex_p++;
		}

		delta = (yflip ? 5.0f : -5.0f) / 128.0f;

		for (r = 0; r < row_count; r++)
		{
			sky->loops[sky->loopcount].mode = HWD_SKYLOOP_STRIP;
			sky->loops[sky->loopcount].vertexindex = vertex_p - &sky->data[0];
			sky->loops[sky->loopcount].vertexcount = 2 * col_count + 2;
			sky->loops[sky->loopcount].use_texture = true;
			sky->loopcount++;

			for (c = 0; c <= col_count; c++)
			{
				HWR_SkyDomeVertex(sky, vertex_p++, r + (yflip ? 1 : 0), (c ? c : 0), yflip, delta, false);
				HWR_SkyDomeVertex(sky, vertex_p++, r + (yflip ? 0 : 1), (c ? c : 0), yflip, delta, false);
			}
		}
	}
}

static boolean drewsky = false;

// precompute to save a bit of division
static const float FINEDEGREE = (360.0f/(float)FINEANGLES);

static void HWR_DrawSkyBackground(void)
{
	FTransform dometransform;

	if (drewsky)
		return;

	GL_SetBlend(PF_Translucent|PF_NoDepthTest|PF_Modulated);

	memcpy(&dometransform, &atransform, sizeof(FTransform));

	dometransform.x      = 0.0;
	dometransform.y      = 0.0;
	dometransform.z      = 0.0;

	//04/01/2000: Hurdler: added for T&L
	//                     It should replace all other gl_viewxxx when finished
	HWR_SetTransformAiming(&dometransform);
	dometransform.angley = (float)((viewangle-ANGLE_270)>>ANGLETOFINESHIFT)*(FINEDEGREE);

	HWR_GetTexture(texturetranslation[skytexture], false);

	if (gl_sky.texture != texturetranslation[skytexture])
	{
		HWR_ClearSkyDome();
		HWR_BuildSkyDome();
	}

	if (HWR_UseShader())
		GL_SetShader(HWR_GetShaderFromTarget(SHADER_SKY));
	GL_SetTransform(&dometransform);
	GL_RenderSkyDome(&gl_sky);
}

// -----------------+
// HWR_ClearView : clear the viewwindow, with maximum z value. also clears stencil buffer.
// -----------------+
static inline void HWR_ClearView(void)
{
	GL_GClipRect((INT32)gl_viewwindowx,
				(INT32)gl_viewwindowy,
				(INT32)(gl_viewwindowx + gl_viewwidth),
				(INT32)(gl_viewwindowy + gl_viewheight),
						ZCLIP_PLANE, FAR_ZCLIP_DEFAULT);
	GL_ClearBuffer(false, true, true, NULL);
}


// -----------------+
// HWR_SetViewSize  : set projection and scaling values
// -----------------+
void HWR_SetViewSize(void)
{
	// setup view size
	gl_viewwidth = (float)vid.width;
	gl_viewheight = (float)vid.height;

	if (splitscreen)
		gl_viewheight /= 2;

	if (splitscreen > 1)
		gl_viewwidth /= 2;

	gl_baseviewwindowy = 0;
	gl_baseviewwindowx = 0;

	GL_FlushScreenTextures();
}

// Set view aiming, for the sky dome, the skybox,
// and the normal view, all with a single function.
static void HWR_SetTransformAiming(FTransform *trans)
{
	if (cv_glshearing.value)
	{
		fixed_t fixedaiming = AIMINGTODY(aimingangle);
		trans->viewaiming = FixedToFloat(fixedaiming);
		trans->shearing = true;
		gl_aimingangle = 0;
	}
	else
	{
		trans->shearing = false;
		gl_aimingangle = aimingangle;
	}

	trans->anglex = (float)(gl_aimingangle>>ANGLETOFINESHIFT)*(FINEDEGREE);
}

void HWR_SetTransform(float fpov)
{
	gl_viewx = FixedToFloat(viewx);
	gl_viewy = FixedToFloat(viewy);
	gl_viewz = FixedToFloat(viewz);
	gl_viewsin = FixedToFloat(viewsin);
	gl_viewcos = FixedToFloat(viewcos);

	memset(&atransform, 0x00, sizeof(FTransform));

	// Set T&L transform
	atransform.x = gl_viewx;
	atransform.y = gl_viewy;
	atransform.z = gl_viewz;

	atransform.scalex = 1;
	atransform.scaley = (float)vid.width/vid.height;
	atransform.scalez = 1;

	HWR_SetTransformAiming(&atransform);
	atransform.angley = (float)(viewangle>>ANGLETOFINESHIFT)*(FINEDEGREE);

	gl_viewludsin = FixedToFloat(FINECOSINE(gl_aimingangle>>ANGLETOFINESHIFT));
	gl_viewludcos = FixedToFloat(-FINESINE(gl_aimingangle>>ANGLETOFINESHIFT));

	atransform.fovangle = fpov; // Tails
	HWR_RollTransform(&atransform, viewroll);
	atransform.splitscreen = splitscreen;

	const UINT8 postimg = camera[R_GetViewNumber()].postimg;

	if (postimg & POSTIMG_FLIP)
	{
		if (postimg & POSTIMG_MIRROR)
			atransform.fliptype = TRANSFORM_MIRRORFLIP;
		else
			atransform.fliptype = TRANSFORM_FLIP;
	}
	else if (postimg & POSTIMG_MIRROR)
		atransform.fliptype = TRANSFORM_MIRROR;

	// Set transform.
	GL_SetTransform(&atransform);
}

void HWR_ClearClipper(void)
{
	angle_t a1 = gld_FrustumAngle(gl_aimingangle);
	gld_clipper_Clear();
	gld_clipper_SafeAddClipRange(viewangle + a1, viewangle - a1);
#ifdef HAVE_SPHEREFRUSTRUM
	gld_FrustumSetup();
#endif
}

// Changes the current stencil state.
void HWR_SetStencilState(int state, int level)
{
	if (level > -1)
		GL_SetSpecialState(HWD_SET_STENCIL_LEVEL, level);
	GL_SetSpecialState(HWD_SET_PORTAL_MODE, state);
}

typedef void (*bspfunc)(INT32 bspnum);

// Renders the current viewpoint, though takes portal arguments for recursive portals.
void HWR_RenderViewpoint(gl_portal_t *rootportal, const float fpov, player_t *player, int stencil_level, boolean allow_portals)
{
	gl_portallist_t portallist;

	const boolean skybox = (skyboxmo[0] && cv_skybox.value);
	const boolean useportals = HWR_UsePortals() && allow_portals;
	bspfunc bspFunc = (useportals && portalclipline) ? HWR_RenderPortalBSPNode : HWR_RenderBSPNode;

	portallist.base = portallist.cap = NULL;

	if (useportals && stencil_level < cv_maxportals.value) // if recursion limit is not reached
	{
		gl_portal_t *portal;

		// search for portals in current frame
		currentportallist = &portallist;
		HWR_SetPortalState(GLPORTAL_SEARCH);

		HWR_ClearClipper();

		if (rootportal)
			HWR_PortalClipping(rootportal);

		validcount++;

		// no actual rendering happens
		bspFunc((INT32)numnodes-1);

		// for each found portal:
		// note: if necessary, could sort the portals here?
		for (portal = portallist.base; portal; portal = portal->next)
		{
			HWR_RenderPortal(portal, rootportal, fpov, player, stencil_level);
		}

		HWR_SetPortalState(GLPORTAL_INSIDE); // when portal walls are encountered in following bsp traversal, nothing should be drawn
	}
	else
		HWR_SetPortalState(GLPORTAL_OFF); // there may be portals and they need to be drawn as regural walls

	// draw normal things in current frame in current incremented stencil buffer area
	HWR_SetStencilState(HWR_STENCIL_NORMAL, stencil_level);

	HWR_SetTransform(fpov);

	HWR_ClearSprites();
	HWR_ClearClipper();

	if (useportals && rootportal)
	{
		HWR_PortalFrame(rootportal);// for portalclipsector, it could have gone null from search
		HWR_PortalClipping(rootportal);
	}

	// Set transform.
	GL_SetTransform(&atransform);

	ps_numbspcalls.value.i = 0;
	ps_numpolyobjects.value.i = 0;
	PS_START_TIMING(ps_bsptime);

	validcount++;

	if (LIKELY(cv_glbatching.value))
		HWR_StartBatching();

	if (useportals && !rootportal && portallist.base && !skybox) // if portals have been drawn in the main view, then render skywalls differently
		gl_collect_skywalls = true;

	// HAYA: Save the old portal state, and turn portals off while normally rendering the BSP tree.
	// This fixes specific effects not working, such as horizon lines.
	SINT8 oldgl_portal_state = gl_portal_state;

	if (gl_portal_state != GLPORTAL_OFF) // if we already haven't hit the recursion limit or we already ended our portal shenanigans
		HWR_SetPortalState(GLPORTAL_INSIDE); // TURN IT OFF

	// Recursively "render" the BSP tree.
	bspFunc((INT32)numnodes-1);

	// woo we back
	HWR_SetPortalState(oldgl_portal_state);

	if (allow_portals) // looks weird, but this is only true when its not skybox rendering skipping precip in skyboxes like software does
		HWR_AddPrecipitationSprites();

	PS_STOP_TIMING(ps_bsptime);

	if (LIKELY(cv_glbatching.value))
		HWR_RenderBatches();

	if (skyWallVertexArraySize) // if there are skywalls to draw using the alternate method
	{
		HWR_SetStencilState(HWR_STENCIL_SKY, -1);
		HWR_DrawSkyWallList();
		HWR_SkyWallList_Clear();
		HWR_SetStencilState(HWR_STENCIL_NORMAL, 1);
		drewsky = false;
		HWR_DrawSkyBackground();
		HWR_SetStencilState(HWR_STENCIL_NORMAL, 0);
		GL_ClearBuffer(false, false, true, 0);// clear skywall markings from the stencil buffer
		HWR_SetTransform(fpov);// restore transform
	}
	gl_collect_skywalls = false;

	ps_numsprites.value.i = gl_visspritecount;
	PS_START_TIMING(ps_hw_spritesorttime);
	HWR_SortVisSprites();
	PS_STOP_TIMING(ps_hw_spritesorttime);
	PS_START_TIMING(ps_hw_spritedrawtime);
	if (LIKELY(!cv_glmdls.value))
		HWR_DrawSprites();
	else
		HWR_DrawModels();
	PS_STOP_TIMING(ps_hw_spritedrawtime);

	ps_numdrawnodes.value.i = 0;
	ps_hw_nodesorttime.value.p = 0;
	ps_hw_nodedrawtime.value.p = 0;
	HWR_RenderDrawNodes();

	HWR_FreePortalList(portallist);
}


// ==========================================================================
// Render the current frame.
// ==========================================================================
static void HWR_RenderFrame(player_t *player, boolean skybox)
{
	const float fpov = FixedToFloat(R_GetPlayerFov(player));

	// set window position
	gl_viewwindowx = gl_baseviewwindowx;
	gl_viewwindowy = gl_baseviewwindowy;

	if ((splitscreen == 1 && viewssnum == 1) || (splitscreen > 1 && viewssnum > 1))
	{
		gl_viewwindowy += gl_viewheight;
	}

	if (splitscreen > 1 && viewssnum & 1)
	{
		gl_viewwindowx += gl_viewwidth;
	}

	if (splitscreen == 2 && player == &players[displayplayers[2]])
	{
		// V_DrawPatchFill, but for the fourth screen only
		patch_t *gpatch = W_CachePatchName("SRB2BACK", PU_PATCH);
		INT32 dupz = (vid.dupx < vid.dupy ? vid.dupx : vid.dupy);
		INT32 x, y, pw = (gpatch->width * dupz), ph = (gpatch->height * dupz);

		for (x = vid.width >> 1; x < vid.width; x += pw)
		{
			for (y = vid.height >> 1; y < vid.height; y += ph)
				HWR_DrawStretchyFixedPatch(gpatch, (x)<<FRACBITS, (y)<<FRACBITS, FRACUNIT, FRACUNIT, V_NOSCALESTART, NULL, 0);
		}
	}

	// check for new console commands.
	NetUpdate();

	// Clear view, set viewport (glViewport), set perspective...
	HWR_ClearView();

	ST_doPaletteStuff();

	// Draw the sky background.
	HWR_DrawSkyBackground();
	if (skybox)
		drewsky = true;

	current_bsp_culling_distance = 0;

	if (!skybox && cv_glrenderdistance.value)
	{
		GL_GClipRect((INT32)gl_viewwindowx,
					(INT32)gl_viewwindowy,
					(INT32)(gl_viewwindowx + gl_viewwidth),
					(INT32)(gl_viewwindowy + gl_viewheight),
					ZCLIP_PLANE, clipping_distances[cv_glrenderdistance.value - 1]);
		current_bsp_culling_distance = bsp_culling_distances[cv_glrenderdistance.value - 1];
	}

	portalclipline = NULL;
	HWR_RenderViewpoint(NULL, fpov, player, 0, !skybox);

	// Unset transform and shader
	GL_SetTransform(NULL);
	GL_UnSetShader();

	// Run post processor effects
	if (!skybox)
		HWR_DoPostProcessor(player);

	// Check for new console commands.
	NetUpdate();

	// added by Hurdler for correct splitscreen
	// moved here by hurdler so it works with the new near clipping plane
	GL_GClipRect(0, 0, vid.width, vid.height, NZCLIP_PLANE, FAR_ZCLIP_DEFAULT);
}

// ==========================================================================
// Render the player view.
// ==========================================================================

static void HWR_RollTransform(FTransform *tr, angle_t roll)
{
	if (roll != 0)
	{
		tr->rollangle = roll / (float)ANG1;
		tr->roll = true;
		tr->rollx = 1.0f;
		tr->rollz = 0.0f;
	}
}

void HWR_RenderPlayerView(void)
{
	player_t * player = &players[displayplayers[viewssnum]];

	const boolean skybox = (skyboxmo[0] && cv_skybox.value); // True if there's a skybox object and skyboxes are on

	// Clear the color buffer, stops HOMs. Also seems to fix the skybox issue on Intel GPUs.
	if (viewssnum == 0) // Only do it if it's the first screen being rendered
	{
		FRGBAFloat ClearColor;

		ClearColor.red = 0.0f;
		ClearColor.green = 0.0f;
		ClearColor.blue = 0.0f;
		ClearColor.alpha = 1.0f;

		GL_ClearBuffer(true, false, false, &ClearColor);
	}

	if (HWR_UseShader())
	{
		if (cv_ripplewater.value)
			GL_SetShaderInfo(HWD_SHADERINFO_LEVELTIME, (INT32)leveltime); // The water surface shader needs the leveltime.

		const angle_t light_angle = maplighting.angle - viewangle + ANGLE_90; // I fucking hate OGL's coordinate system
		GL_SetShaderInfo(HWD_SHADERINFO_LIGHT_X, FINECOSINE(light_angle >> ANGLETOFINESHIFT));
		GL_SetShaderInfo(HWD_SHADERINFO_LIGHT_Y, 0);
		GL_SetShaderInfo(HWD_SHADERINFO_LIGHT_Z,  -FINESINE(light_angle >> ANGLETOFINESHIFT));

		GL_SetShaderInfo(HWD_SHADERINFO_LIGHT_CONTRAST, maplighting.contrast);
		GL_SetShaderInfo(HWD_SHADERINFO_LIGHT_BACKLIGHT, maplighting.backlight);
	}

	if (viewssnum > 3)
		return;

	// Render the skybox if there is one.
	PS_START_TIMING(ps_skyboxtime);
	drewsky = false;
	if (skybox)
	{
		R_SkyboxFrame(viewssnum);
		HWR_RenderFrame(player, true);
	}
	PS_STOP_TIMING(ps_skyboxtime);

	R_SetupFrame(viewssnum, false); // This can stay false because it is only used to set viewsky in r_main.c, which isn't used here
	framecount++; // for timedemo
	HWR_RenderFrame(player, false);
}

static void HWR_CheckForHorizonLines(void)
{
	size_t i;
	INT32 h;

	gl_maphashorizonlines = false;

	if (!cv_glhorizonlines.value)
		return;

	for (i = 0; i < numsubsectors; i++)
	{
		subsector_t *subsec = &subsectors[i];

		// sector checked already?
		if (subsec->validcount == validcount)
			continue;

		subsec->validcount = validcount;

		seg_t *line = &segs[subsec->firstline];

		for (h = 0; h < subsec->numlines; h++, line++)
		{
			if (line->linedef->special != HORIZONSPECIAL)
				continue;

			gl_maphashorizonlines = true;
			break;
		}
	}
}

void HWR_LoadLevel(boolean reloadinggamestate)
{
	HWR_CreatePlanePolygons((INT32)numnodes - 1);

	// Build the sky dome
	HWR_ClearSkyDome();
	HWR_BuildSkyDome();

	if (!reloadinggamestate)
		HWR_CheckForHorizonLines();

	if (HWR_ShouldUsePaletteRendering())
		HWR_SetMapPalette();
}

// enable or disable palette rendering state depending on settings and availability
// called when relevant settings change
// shader recompilation is done in the cvar callback
static void HWR_TogglePaletteRendering(void)
{
	V_ResetPaletteCVars(); // dont carry over changed palettes

	// which state should we go to?
	if (HWR_ShouldUsePaletteRendering())
	{
		// are we not in that state already?
		if (!gl_palette_rendering_state)
		{
			gl_palette_rendering_state = true;

			// The textures will still be converted to RGBA by r_opengl.
			// This however makes hw_cache use paletted blending for composite textures!
			// (patchformat is not touched)
			textureformat = GL_TEXFMT_P_8;

			HWR_SetMapPalette();
			HWR_SetPalette(pLocalPalette);

			// If the r_opengl "texture palette" stays the same during this switch, these textures
			// will not be cleared out. However they are still out of date since the
			// composite texture blending method has changed. Therefore they need to be cleared.
			HWR_LoadMapTextures(numtextures);
		}
	}
	else
	{
		// are we not in that state already?
		if (gl_palette_rendering_state)
		{
			gl_palette_rendering_state = false;
			textureformat = GL_TEXFMT_RGBA;
			HWR_SetPalette(pLocalPalette);
			// If the r_opengl "texture palette" stays the same during this switch, these textures
			// will not be cleared out. However they are still out of date since the
			// composite texture blending method has changed. Therefore they need to be cleared.
			HWR_LoadMapTextures(numtextures);
		}
	}
}

//added by Hurdler: console varibale that are saved
void HWR_AddCommands(void)
{
	CV_RegisterVar(&cv_gltexturedepth);

	CV_RegisterVar(&cv_glscreentextures);

#ifdef USE_FBO_OGL
	CV_RegisterVar(&cv_glframebuffer);
#endif

	CV_RegisterVar(&cv_glmdls);
	CV_RegisterVar(&cv_glfallbackplayermodel);

	CV_RegisterVar(&cv_glspritebillboarding);
	CV_RegisterVar(&cv_glshearing);

	CV_RegisterVar(&cv_glfakecontrast);
	CV_RegisterVar(&cv_glslopecontrast);

	CV_RegisterVar(&cv_glshaders);

	CV_RegisterVar(&cv_gllightdither);
	CV_RegisterVar(&cv_glsecbright);

	CV_RegisterVar(&cv_glfiltermode);
	CV_RegisterVar(&cv_glanisotropicmode);

	CV_RegisterVar(&cv_glsolvetjoin);

	CV_RegisterVar(&cv_glbatching);

	CV_RegisterVar(&cv_glrenderdistance);

	CV_RegisterVar(&cv_glhorizonlines);
	CV_RegisterVar(&cv_glportals);

	CV_RegisterVar(&cv_glpaletterendering);
	CV_RegisterVar(&cv_glpalettedepth);
	CV_RegisterVar(&cv_glflashpal);
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

		HWR_InitMapTextures();
		HWR_InitMD2();

		gl_shadersavailable = HWR_InitShaders();
		HWR_SetShaderState();
		HWR_LoadAllCustomShaders();

		HWR_TogglePaletteRendering();

		if (msaa)
			GL_SetSpecialState(HWD_SET_MSAA, a2c ? 2 : 1);

		HWR_RegisterCommands();
	}
	startupdone = true;
}

/**
 * Register renderer commands.
 */
static void HWR_RegisterCommands(void)
{
	COM_AddCommand("gr_glinfo", COM_HWR_glinfo);
}

static void COM_HWR_glinfo(void)
{
	if (vid.glstate != VID_GL_LIBRARY_LOADED)
	{
		CONS_Printf("Currently not using the OpenGL renderer.\n");
		return;
	}

	int list_extensions = 0;
	size_t argc = COM_Argc();
	const char *argv;
	for (size_t i = 1; i < argc; i++)
	{
		argv = COM_Argv(i);

		if (strcmp(argv, "--list-extensions") == 0 || strcmp(argv, "-l") == 0)
		{
			list_extensions = 1;
		}
		else
		{
			CONS_Printf("Unrecognized argument: %s\n", argv);
			return;
		}

	}

	CONS_Printf("\x88OpenGL %s\x80\n", gl_version);
	CONS_Printf("Renderer: %s\n", gl_renderer);
	CONS_Printf("Vendor: %s\n", gl_vendor);

	CONS_Printf("%u GL extensions present.\n", gl_num_extensions);

	if (list_extensions)
	{
		// We need this strtok loop because we cannot write the extensions list directly
		// to the output buffer, because it will overflow the output buffer of CONS_Printf
		// if the GPU is super new and supports a bajillion extensions - xyzzy

		char *copy = strdup((const char*)gl_extensions);
		char *ext = strtok(copy, " ");

		if (copy == NULL)
		{
			CONS_Printf("Ran out of memory listing extensions?!?!");
			return;
		}

		do
		{
			CONS_Printf(" - %s\n", ext);
		} while ((ext = strtok(NULL, " ")) != NULL);

		free(copy);
	}
	else
	{
		CONS_Printf("Use --list-extensions to view the list of extensions.\n");
	}
}

// --------------------------------------------------------------------------
// Free resources allocated by the hardware renderer
// --------------------------------------------------------------------------
void HWR_Shutdown(void)
{
	CONS_Printf("HWR_Shutdown()\n");
	HWR_FreeExtraSubsectors();
	HWR_FreeMapTextures();
	GL_FlushScreenTextures();
#ifdef USE_FBO_OGL
	GL_Framebuffer_Disable();
#endif
}

static void HWR_RenderWall(FOutVector *wallVerts, FSurfaceInfo *pSurf, FBITFIELD blend, boolean fogwall, INT32 lightlevel, extracolormap_t *wallcolormap)
{
	FBITFIELD blendmode = blend;
	UINT8 alpha = pSurf->PolyColor.s.alpha; // retain the alpha

	INT32 shader = SHADER_NONE;

	// Lighting is done here instead so that fog isn't drawn incorrectly on transparent walls after sorting
	HWR_Lighting(pSurf, lightlevel, wallcolormap, P_SectorUsesDirectionalLighting(gl_frontsector));

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
	if (gl_curline->linedef->splats && cv_splats.value)
		HWR_DrawSegsSplats(pSurf);
#endif
}

static void HWR_DoPostProcessor(player_t *player)
{
	GL_UnSetShader();

	// Armageddon Blast Flash!
	// Could this even be considered postprocessor?
	if (!HWR_PalRenderFlashpal() && player->flashcount)
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

		GL_DrawPolygon(&Surf, v, 4, PF_Modulated|PF_Translucent|PF_NoTexture|PF_NoDepthTest);
	}

	if (cv_glscreentextures.value != 2) // screen textures are needed for the rest of the effects
		return;

	// Capture the screen for intermission and screen waving
	if (gamestate != GS_INTERMISSION)
		GL_MakeScreenTexture(HWD_SCREENTEXTURE_GENERIC1);

	if (splitscreen) // Not supported in splitscreen - someone want to add support?
		return;

	//UINT8 viewnum = R_GetViewNumber(); // see above
	//camera_t *thiscam = &camera[viewnum];
	camera_t *thiscam = &camera[0];

	// Drunken vision! WooOOooo~
	if (thiscam->postimg & POSTIMG_WATER || thiscam->postimg & POSTIMG_HEAT)
	{
		// 10 by 10 grid. 2 coordinates (xy)
		float v[SCREENVERTS][SCREENVERTS][2];
		float disStart = (leveltime-1) + FixedToFloat(rendertimefrac);

		UINT8 x, y;
		INT32 WAVELENGTH;
		INT32 AMPLITUDE;
		INT32 FREQUENCY;

		// Modifies the wave.
		if (thiscam->postimg & POSTIMG_WATER)
		{
			WAVELENGTH = 5;
			AMPLITUDE = 40;
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
		GL_PostImgRedraw(v);

		// Capture the screen again for screen waving on the intermission
		if (gamestate != GS_INTERMISSION)
			GL_MakeScreenTexture(HWD_SCREENTEXTURE_GENERIC1);
	}
	// Flipping of the screen isn't done here anymore
}

void HWR_DoWipe(UINT8 wipenum, UINT8 scrnnum)
{
	static char lumpname[9] = "FADEmmss";
	lumpnum_t lumpnum;
	size_t lsize;

	if (cv_glscreentextures.value == 0)
	{
		V_DrawFill(0, 0, BASEVIDWIDTH, BASEVIDHEIGHT, 31); // just draw a black screen instead of flashing and crap
		return;
	}

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
	GL_DoScreenWipe(HWD_SCREENTEXTURE_WIPE_START, HWD_SCREENTEXTURE_WIPE_END);
}

#endif // HWRENDER
