// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright (C) 1998-2000 by DooM Legacy Team.
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//-----------------------------------------------------------------------------
/// \file
/// \brief 3D hardware renderer definitions

#ifndef _HWR_DEFS_
#define _HWR_DEFS_
#include "../doomtype.h"
#include "../r_defs.h"

#define ZCLIP_PLANE 4.0f // Used for the actual game drawing
#define NZCLIP_PLANE 0.9f // Seems to be only used for the HUD and screen textures
#define FAR_ZCLIP_DEFAULT 32768.0f

// The width/height/depth of the palette lookup table used by palette rendering.
// Changing this also requires changing the shader code!
// Also assumed to be a power of two in some parts of the code.
// 64 seems to work perfectly for the vanilla palette.
#define HWR_PALETTE_LUT_SIZE 64

// ==========================================================================
//                                                               SIMPLE TYPES
// ==========================================================================

typedef long            FINT;
typedef unsigned long   FUINT;
typedef unsigned char   FUBYTE;
typedef unsigned long   FBITFIELD;
#ifndef __MINGW32__
typedef float           FLOAT;
#endif
typedef unsigned char   FBOOLEAN;

// ==========================================================================
//                                                                     COLORS
// ==========================================================================

// byte value for paletted graphics, which represent the transparent color
#define HWR_PATCHES_CHROMAKEY_COLORINDEX   247
#define HWR_CHROMAKEY_EQUIVALENTCOLORINDEX 220

// RGBA Color components with float type ranging [ 0 ... 1 ]
struct FRGBAFloat
{
	FLOAT   red;
	FLOAT   green;
	FLOAT   blue;
	FLOAT   alpha;
};
typedef struct FRGBAFloat FRGBAFloat;

struct FColorARGB
{
	FUBYTE  alpha;
	FUBYTE  red;
	FUBYTE  green;
	FUBYTE  blue;
};
typedef struct FColorARGB ARGB_t;
typedef struct FColorARGB FColorARGB;

// ==========================================================================
//                                                                    VECTORS
// ==========================================================================

// Simple 2D coordinate
typedef struct
{
	FLOAT x,y;
} F2DCoord, v2d_t;

// ======================
//      wallVert3D
// ----------------------
// :crab: IS GONE! :crab:
// ======================

// -----------
// structures
// -----------

// a vertex of a Doom 'plane' polygon
typedef struct
{
	float x;
	float y;
	float z;
} polyvertex_t;

#ifdef _MSC_VER
#pragma warning(disable :  4200)
#endif

// a convex 'plane' polygon, clockwise order
typedef struct
{
	INT32 numpts;
	polyvertex_t pts[0];
} poly_t;

#ifdef _MSC_VER
#pragma warning(default :  4200)
#endif

// holds extra info for 3D render, for each subsector in subsectors[]
typedef struct
{
	poly_t *planepoly;  // the generated convex polygon
} extrasubsector_t;

// Kart features
#define USE_FTRANSFORM_ANGLEZ
#define USE_FTRANSFORM_MIRROR

// Vanilla features
//#define USE_MODEL_NEXTFRAME

typedef struct
{
	FLOAT       x,y,z;           // position
#ifdef USE_FTRANSFORM_ANGLEZ
	FLOAT       anglex,angley,anglez;   // aimingangle / viewangle
	FLOAT       anglex2,anglez2;        // secondaries
#else
	FLOAT       anglex,angley;   // aimingangle / viewangle
	FLOAT       anglex2;         // secondaries
#endif
	FLOAT       scalex,scaley,scalez;
	FLOAT       spritexscale,spriteyscale;
	FLOAT       fovxangle, fovyangle;
	UINT8       splitscreen;
	boolean     flip;            // screenflip
	boolean     roll;
	boolean     rollmodel;
	SINT8       rollflip;
	FLOAT       rollangle;
	UINT8       rotaxis;
	FLOAT       centerx, centery;
#ifdef USE_FTRANSFORM_MIRROR
	boolean     mirror;          // SRB2Kart: Encore Mode
	boolean     mirrorflip;      // Encore Mode with Flipcam
#endif
	boolean     shearing;        // 14042019
	float       viewaiming;      // 17052019
} FTransform;

// Transformed vector, as passed to HWR API
typedef struct
{
	FLOAT       x,y,z;
	FLOAT       s,t;
} FOutVector;

// Shader targets used to render specific types of geometry.
// A shader target is resolved to an actual shader with HWR_GetShaderFromTarget.
// The shader returned may be a base shader or a custom shader.
enum
{
	SHADER_NONE = -1,
	SHADER_FLOOR = 0,
	SHADER_SHADOW,
	SHADER_WALL,
	SHADER_SPRITE,
	SHADER_MODEL,
	SHADER_WATER,
	SHADER_FOG,
	SHADER_SKY,
	SHADER_PALETTE_POSTPROCESS,
	SHADER_UI_COLORMAP_FADE,

	NUMSHADERTARGETS
};

// Must be at least NUMSHADERTARGETS*2 to fit base and custom shaders for each shader target.
#define HWR_MAXSHADERS NUMSHADERTARGETS*2

// Custom shader reference table
typedef struct
{
	const char *type;
	INT32 id;
} customshaderxlat_t;

enum hwdshaderstage
{
	HWD_SHADERSTAGE_VERTEX,
	HWD_SHADERSTAGE_FRAGMENT,
};

typedef enum hwdshaderstage hwdshaderstage_t;


// ==========================================================================
//                                                               RENDER MODES
// ==========================================================================

// Flags describing how to render a polygon
// You pass a combination of these flags to DrawPolygon()
enum EPolyFlags
{
	// the first 5 are mutually exclusive

	PF_Masked           = 0x00000001,   // Poly is alpha scaled and 0 alpha pels are discarded (holes in texture)
	PF_Translucent      = 0x00000002,   // Poly is transparent, alpha = level of transparency
	PF_Additive         = 0x00000004,   // Poly is added to the frame buffer
	PF_Environment      = 0x00000008,   // Poly should be drawn environment mapped.
	                                    // Hurdler: used for text drawing
	PF_Substractive     = 0x00000010,   // for splat
	PF_NoAlphaTest      = 0x00000020,   // hiden param
	PF_Fog              = 0x00000040,   // Fog blocks
	PF_Blending         = (PF_Environment|PF_Additive|PF_Translucent|PF_Masked|PF_Substractive|PF_Fog)&~PF_NoAlphaTest,

	// other flag bits

	PF_Occlude          = 0x00000100,   // Update the depth buffer
	PF_NoDepthTest      = 0x00000200,   // Disable the depth test mode
	PF_Invisible        = 0x00000400,   // Disable write to color buffer
	PF_Decal            = 0x00000800,   // Enable polygon offset
	PF_Modulated        = 0x00001000,   // Modulation (multiply output with constant RGBA)
	                                    // When set, pass the color constant into the FSurfaceInfo -> FlatColor
	PF_NoTexture        = 0x00002000,   // Disable texture
	PF_ColorMapped      = 0x00008000,   // Surface has "tint" and "fade" colors, which are sent as uniforms to a shader.
	//                    0x00008000
	PF_RemoveYWrap      = 0x00010000,   // Force clamp texture on Y
	PF_ForceWrapX       = 0x00020000,   // Force repeat texture on X
	PF_ForceWrapY       = 0x00040000,   // Forces repeat texture on Y
	PF_Ripple           = 0x00100000    // Water ripple effect. The current backend doesn't use it for anything.
	//                    0x20000000
	//                    0x40000000
	//                    0x80000000

};

enum ETextureFlags
{
	TF_WRAPX       = 0x00000001,        // wrap around X
	TF_WRAPY       = 0x00000002,        // wrap around Y
	TF_WRAPXY      = TF_WRAPY|TF_WRAPX, // very common so use alias is more easy
	TF_CHROMAKEYED = 0x00000010,
	TF_TRANSPARENT = 0x00000040,        // texture with some alpha == 0
};

struct FTextureInfo
{
	UINT32 width, height;
	UINT32 downloaded;
	UINT32 format;

	struct GLMipmap_s *texture;
	struct FTextureInfo *prev, *next;
};
typedef struct FTextureInfo FTextureInfo;


struct FLightInfo
{
	FUINT			light_level;
	FUINT			fade_start;
	FUINT			fade_end;
};
typedef struct FLightInfo FLightInfo;

// Description of a renderable surface
struct FSurfaceInfo
{
	FUINT			PolyFlags;
	RGBA_t			PolyColor;
	RGBA_t			TintColor;
	RGBA_t			FadeColor;
	UINT32			LightTableId;
	FLightInfo		LightInfo;
};
typedef struct FSurfaceInfo FSurfaceInfo;

enum hwdsetspecialstate
{
	HWD_SET_SHADERS,

	HWD_SET_TEXTUREFILTERMODE,
	HWD_SET_TEXTUREANISOTROPICMODE,
	
	HWD_SET_MSAA,

	HWD_SET_SCREEN_TEXTURES,

	HWD_SET_PORTAL_MODE,// new portal thing
	HWD_SET_STENCIL_LEVEL,

	HWD_NUMSTATE
};
typedef enum hwdsetspecialstate hwdspecialstate_t;

// Lactozilla: Shader info
// Generally set at the start of the frame.
enum hwdshaderinfo
{
	HWD_SHADERINFO_LEVELTIME = 1,
};

typedef enum hwdshaderinfo hwdshaderinfo_t;

enum hwdportalstate
{
	HWD_PORTAL_NORMAL,
	HWD_PORTAL_STENCIL_SEGS,
	HWD_PORTAL_STENCIL_REVERSE_SEGS,
	HWD_PORTAL_DEPTH_SEGS,
	HWD_PORTAL_SKY_STENCIL_SEGS,
};

#define GL_DEFAULTMIX 0x00000000
#define GL_DEFAULTFOG 0xFF000000

enum hwdfiltermode
{
	HWD_SET_TEXTUREFILTER_POINTSAMPLED,
	HWD_SET_TEXTUREFILTER_BILINEAR,
	HWD_SET_TEXTUREFILTER_TRILINEAR,
	HWD_SET_TEXTUREFILTER_MIXED1,
	HWD_SET_TEXTUREFILTER_MIXED2,
	HWD_SET_TEXTUREFILTER_MIXED3,
};

// Screen texture slots
enum hwdscreentexture
{
	HWD_SCREENTEXTURE_WIPE_START, // source image for the wipe/fade effect
	HWD_SCREENTEXTURE_WIPE_END,   // destination image for the wipe/fade effect
	HWD_SCREENTEXTURE_GENERIC1,   // underwater/heat effect, intermission background
	HWD_SCREENTEXTURE_GENERIC2,   // palette-based colormap fade, final screen texture
	NUMSCREENTEXTURES,            // (generic3 is unused if palette rendering is disabled)
};
typedef enum hwdscreentexture hwdscreentexture_t;

#endif //_HWR_DEFS_
