// SONIC ROBO BLAST 2
//-----------------------------------------------------------------------------
// Copyright (C) 2021 by Sonic Team Junior.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file hw_shaders.h
/// \brief Handles the shaders used by the game.

#ifndef _HW_SHADERS_H_
#define _HW_SHADERS_H_

#include "../doomtype.h"

// ================
//  Vertex shaders
// ================

//
// Generic vertex shader
//

#define GLSL_DEFAULT_VERTEX_SHADER \
	"void main()\n" \
	"{\n" \
		"gl_Position = gl_ProjectionMatrix * gl_ModelViewMatrix * gl_Vertex;\n" \
		"gl_FrontColor = gl_Color;\n" \
		"gl_TexCoord[0].xy = gl_MultiTexCoord0.xy;\n" \
		"gl_ClipVertex = gl_ModelViewMatrix * gl_Vertex;\n" \
	"}\0"

// ==================
//  Fragment shaders
// ==================

//
// Generic fragment shader
//

#define GLSL_DEFAULT_FRAGMENT_SHADER \
	"uniform sampler2D tex;\n" \
	"uniform vec4 poly_color;\n" \
	"void main(void) {\n" \
		"gl_FragColor = texture2D(tex, gl_TexCoord[0].st) * poly_color;\n" \
	"}\0"

//
// Software fragment shader
//

// Include GLSL_FLOOR_FUDGES or GLSL_WALL_FUDGES or define the fudges in shaders that use this macro.
#define GLSL_DOOM_COLORMAP_DITHER \
	"float baseValue = max(startmap * STARTMAP_FUDGE - scale * 0.5 * SCALE_FUDGE, cap);\n" \
	"float halfResolutionx = scr_resolution.x;\n" \
	"float halfResolutiony = scr_resolution.y;\n" \
	"if (scr_resolution.x > 1280.0) {\n" \
		"halfResolutionx = scr_resolution.x * 0.5;\n" \
	"}\n" \
	"if (scr_resolution.y > 720.0) {\n" \
		"halfResolutiony = scr_resolution.y * 0.5;\n" \
	"}\n" \
	"float zFactor = clamp((z - 768.0) / (6144.0 - 768.0), 0.0, 1.0);\n" \
	"halfResolutionx = mix(halfResolutionx, halfResolutionx * 2.0, zFactor);\n" \
	"halfResolutiony = mix(halfResolutiony, halfResolutiony * 2.0, zFactor);\n" \
	"vec2 normalizedPosition = position * vec2(halfResolutionx / scr_resolution.x, halfResolutiony / scr_resolution.y);\n" \
	"int x = int(mod(normalizedPosition.x, 4.0));\n" \
	"int y = int(mod(normalizedPosition.y, 4.0));\n" \
	"float bayerMatrix[4*4] = float[4*4](\n" \
		"0.0, 8.0, 2.0, 10.0,\n" \
		"12.0, 4.0, 14.0, 6.0,\n" \
		"3.0, 11.0, 1.0, 9.0,\n" \
		"15.0, 7.0, 13.0, 5.0\n" \
	");\n" \
	"#ifdef SRB2_PALETTE_RENDERING\n" \
	"float threshold = (1.0 - pow(zFactor, 2.0)) * (bayerMatrix[y*4 + x] / 11.0);\n" \
	"#else\n" \
	"float threshold = (1.0 - pow(zFactor, 2.0)) * (bayerMatrix[y*4 + x] / 16.0);\n" \
	"#endif\n" \
	"return mix(baseValue + threshold - 0.5 / 16.0, baseValue, zFactor);\n"

#define GLSL_DOOM_COLORMAP_NODITHER \
	"return max(startmap * STARTMAP_FUDGE - scale * 0.5 * SCALE_FUDGE, cap);\n" \

#define GLSL_DOOM_COLORMAP \
	"uniform vec2 scr_resolution;\n" \
	"float R_DoomColormap(float light, float z, vec2 position)\n" \
	"{\n" \
		"float lightnum = clamp(light / 17.0, 0.0, 15.0);\n" \
		"float lightz = clamp(z / 16.0, 0.0, 127.0);\n" \
		"float startmap = (15.0 - lightnum) * 4.0;\n" \
		"float scale = 160.0 / (lightz + 1.0);\n" \
		"float cap = (155.0 - light) * 0.26;\n" \
		"#ifdef SRB2_LIGHT_DITHER\n" \
		GLSL_DOOM_COLORMAP_DITHER \
		"#else\n" \
		GLSL_DOOM_COLORMAP_NODITHER \
		"#endif\n" \
	"}\n"
// lighting cap adjustment:
// first num (155.0), increase to make it start to go dark sooner
// second num (0.26), increase to make it go dark faster

#define GLSL_DOOM_LIGHT_EQUATION \
	"float R_DoomLightingEquation(float light)\n" \
	"{\n" \
		"float z = gl_FragCoord.z / gl_FragCoord.w;\n" \
		"float colormap = floor(R_DoomColormap(light, z, gl_FragCoord.xy)) + 0.5;\n" \
		"return clamp(colormap, 0.0, 31.0) / 32.0;\n" \
	"}\n"

#define GLSL_SOFTWARE_TINT_EQUATION \
	"if (tint_color.a > 0.0) {\n" \
		"float color_bright = sqrt((base_color.r * base_color.r) + (base_color.g * base_color.g) + (base_color.b * base_color.b));\n" \
		"float strength = sqrt(9.0 * tint_color.a);\n" \
		"final_color.r = clamp((color_bright * (tint_color.r * strength)) + (base_color.r * (1.0 - strength)), 0.0, 1.0);\n" \
		"final_color.g = clamp((color_bright * (tint_color.g * strength)) + (base_color.g * (1.0 - strength)), 0.0, 1.0);\n" \
		"final_color.b = clamp((color_bright * (tint_color.b * strength)) + (base_color.b * (1.0 - strength)), 0.0, 1.0);\n" \
	"}\n"

#define GLSL_SOFTWARE_FADE_EQUATION \
	"float darkness = R_DoomLightingEquation(lighting);\n" \
	"if (fade_start != 0.0 || fade_end != 31.0) {\n" \
		"float fs = fade_start / 31.0;\n" \
		"float fe = fade_end / 31.0;\n" \
		"float fd = fe - fs;\n" \
		"darkness = clamp((darkness - fs) * (1.0 / fd), 0.0, 1.0);\n" \
	"}\n" \
	"final_color = mix(final_color, fade_color, darkness);\n"

#define GLSL_PALETTE_RENDERING \
	"float tex_pal_idx = texture3D(palette_lookup_tex, vec3((texel * 63.0 + 0.5) / 64.0))[0] * 255.0;\n" \
	"float z = gl_FragCoord.z / gl_FragCoord.w;\n" \
	"float light_y = clamp(floor(R_DoomColormap(lighting, z, gl_FragCoord.xy)), 0.0, 31.0);\n" \
	"vec2 lighttable_coord = vec2((tex_pal_idx + 0.5) / 256.0, (light_y + 0.5) / 32.0);\n" \
	"vec4 final_color = texture2D(lighttable_tex, lighttable_coord);\n" \
	"final_color.a = texel.a * poly_color.a;\n" \
	"gl_FragColor = final_color;\n" \

#define GLSL_SOFTWARE_FRAGMENT_SHADER \
	"#ifdef SRB2_PALETTE_RENDERING\n" \
	"uniform sampler2D tex;\n" \
	"uniform sampler3D palette_lookup_tex;\n" \
	"uniform sampler2D lighttable_tex;\n" \
	"uniform vec4 poly_color;\n" \
	"uniform float lighting;\n" \
	GLSL_DOOM_COLORMAP \
	"void main(void) {\n" \
		"vec4 texel = texture2D(tex, gl_TexCoord[0].st);\n" \
		GLSL_PALETTE_RENDERING \
	"}\n" \
	"#else\n" \
	"uniform sampler2D tex;\n" \
	"uniform vec4 poly_color;\n" \
	"uniform vec4 tint_color;\n" \
	"uniform vec4 fade_color;\n" \
	"uniform float lighting;\n" \
	"uniform float fade_start;\n" \
	"uniform float fade_end;\n" \
	GLSL_DOOM_COLORMAP \
	GLSL_DOOM_LIGHT_EQUATION \
	"void main(void) {\n" \
		"vec4 texel = texture2D(tex, gl_TexCoord[0].st);\n" \
		"vec4 base_color = texel * poly_color;\n" \
		"vec4 final_color = base_color;\n" \
		GLSL_SOFTWARE_TINT_EQUATION \
		GLSL_SOFTWARE_FADE_EQUATION \
		"final_color.a = texel.a * poly_color.a;\n" \
		"gl_FragColor = final_color;\n" \
	"}\n" \
	"#endif\0"
	
#define GLSL_SOFTWARE_FRAGMENT_SHADER_NOPAL \
	"uniform sampler2D tex;\n" \
	"uniform vec4 poly_color;\n" \
	"uniform vec4 tint_color;\n" \
	"uniform vec4 fade_color;\n" \
	"uniform float lighting;\n" \
	"uniform float fade_start;\n" \
	"uniform float fade_end;\n" \
	GLSL_DOOM_COLORMAP \
	GLSL_DOOM_LIGHT_EQUATION \
	"void main(void) {\n" \
		"vec4 texel = texture2D(tex, gl_TexCoord[0].st);\n" \
		"vec4 base_color = texel * poly_color;\n" \
		"vec4 final_color = base_color;\n" \
		GLSL_SOFTWARE_TINT_EQUATION \
		GLSL_SOFTWARE_FADE_EQUATION \
		"final_color.a = texel.a * poly_color.a;\n" \
		"gl_FragColor = final_color;\n" \
	"}\n" \

// hand tuned adjustments for light level calculation
#define GLSL_FLOOR_FUDGES \
	"#version 120\n" \
	"#define STARTMAP_FUDGE 1.06\n" \
	"#define SCALE_FUDGE 1.15\n"

#define GLSL_WALL_FUDGES \
	"#version 120\n" \
	"#define STARTMAP_FUDGE 1.05\n" \
	"#define SCALE_FUDGE 2.2\n"

#define GLSL_FLOOR_FRAGMENT_SHADER \
	GLSL_FLOOR_FUDGES \
	GLSL_SOFTWARE_FRAGMENT_SHADER

#define GLSL_WALL_FRAGMENT_SHADER \
	GLSL_WALL_FUDGES \
	GLSL_SOFTWARE_FRAGMENT_SHADER
	
#define GLSL_SHADOW_FRAGMENT_SHADER \
	GLSL_FLOOR_FUDGES \
	GLSL_SOFTWARE_FRAGMENT_SHADER_NOPAL

//
// Water surface shader
//
// Mostly guesstimated, rather than the rest being built off Software science.
// Still needs to distort things underneath/around the water...
//

#define GLSL_WATER_TEXEL \
	"float water_z = (gl_FragCoord.z / gl_FragCoord.w) / 2.0;\n" \
	"float a = -pi * (water_z * freq) + (leveltime * speed);\n" \
	"float sdistort = sin(a) * amp;\n" \
	"float cdistort = cos(a) * amp;\n" \
	"vec4 texel = texture2D(tex, vec2(gl_TexCoord[0].s - sdistort, gl_TexCoord[0].t - cdistort));\n"

#define GLSL_WATER_FRAGMENT_SHADER \
	GLSL_FLOOR_FUDGES \
	"const float freq = 0.025;\n" \
	"const float amp = 0.025;\n" \
	"const float speed = 2.0;\n" \
	"const float pi = 3.14159;\n" \
	"#ifdef SRB2_PALETTE_RENDERING\n" \
	"uniform sampler2D tex;\n" \
	"uniform sampler3D palette_lookup_tex;\n" \
	"uniform sampler2D lighttable_tex;\n" \
	"uniform vec4 poly_color;\n" \
	"uniform float lighting;\n" \
	"uniform float leveltime;\n" \
	GLSL_DOOM_COLORMAP \
	"void main(void) {\n" \
		GLSL_WATER_TEXEL \
		GLSL_PALETTE_RENDERING \
	"}\n" \
	"#else\n" \
	"uniform sampler2D tex;\n" \
	"uniform vec4 poly_color;\n" \
	"uniform vec4 tint_color;\n" \
	"uniform vec4 fade_color;\n" \
	"uniform float lighting;\n" \
	"uniform float fade_start;\n" \
	"uniform float fade_end;\n" \
	"uniform float leveltime;\n" \
	GLSL_DOOM_COLORMAP \
	GLSL_DOOM_LIGHT_EQUATION \
	"void main(void) {\n" \
		GLSL_WATER_TEXEL \
		"vec4 base_color = texel * poly_color;\n" \
		"vec4 final_color = base_color;\n" \
		GLSL_SOFTWARE_TINT_EQUATION \
		GLSL_SOFTWARE_FADE_EQUATION \
		"final_color.a = texel.a * poly_color.a;\n" \
		"gl_FragColor = final_color;\n" \
	"}\n" \
	"#endif\0"

//
// Fog block shader
//
// Alpha of the planes themselves are still slightly off -- see HWR_FogBlockAlpha
//

// The floor fudges are used, but should the wall fudges be used instead? or something inbetween?
// or separate values for floors and walls? (need to change more than this shader for that)
#define GLSL_FOG_FRAGMENT_SHADER \
	GLSL_FLOOR_FUDGES \
	"uniform vec4 tint_color;\n" \
	"uniform vec4 fade_color;\n" \
	"uniform float lighting;\n" \
	"uniform float fade_start;\n" \
	"uniform float fade_end;\n" \
	GLSL_DOOM_COLORMAP \
	GLSL_DOOM_LIGHT_EQUATION \
	"void main(void) {\n" \
		"vec4 base_color = gl_Color;\n" \
		"vec4 final_color = base_color;\n" \
		GLSL_SOFTWARE_TINT_EQUATION \
		GLSL_SOFTWARE_FADE_EQUATION \
		"gl_FragColor = final_color;\n" \
	"}\0"

//
// Sky fragment shader
// Modulates poly_color with gl_Color
//
#define GLSL_SKY_FRAGMENT_SHADER \
	"uniform sampler2D tex;\n" \
	"uniform vec4 poly_color;\n" \
	"void main(void) {\n" \
		"gl_FragColor = texture2D(tex, gl_TexCoord[0].st) * gl_Color * poly_color;\n" \
	"}\0"

// Shader for the palette rendering postprocess step
#define GLSL_PALETTE_POSTPROCESS_FRAGMENT_SHADER \
	"uniform sampler2D tex;\n" \
	"uniform sampler3D palette_lookup_tex;\n" \
	"uniform sampler1D palette_tex;\n" \
	"void main(void) {\n" \
		"vec4 texel = texture2D(tex, gl_TexCoord[0].st);\n" \
		"float tex_pal_idx = texture3D(palette_lookup_tex, vec3((texel * 63.0 + 0.5) / 64.0))[0] * 255.0;\n" \
		"float palette_coord = (tex_pal_idx + 0.5) / 256.0;\n" \
		"vec4 final_color = texture1D(palette_tex, palette_coord);\n" \
		"gl_FragColor = final_color;\n" \
	"}\0"

// Applies a palettized colormap fade to tex
#define GLSL_UI_COLORMAP_FADE_FRAGMENT_SHADER \
	"uniform sampler2D tex;\n" \
	"uniform float lighting;\n" \
	"uniform sampler3D palette_lookup_tex;\n" \
	"uniform sampler2D lighttable_tex;\n" \
	"void main(void) {\n" \
		"vec4 texel = texture2D(tex, gl_TexCoord[0].st);\n" \
		"float tex_pal_idx = texture3D(palette_lookup_tex, vec3((texel * 63.0 + 0.5) / 64.0))[0] * 255.0;\n" \
		"vec2 lighttable_coord = vec2((tex_pal_idx + 0.5) / 256.0, (lighting + 0.5) / 32.0);\n" \
		"gl_FragColor = texture2D(lighttable_tex, lighttable_coord);\n" \
	"}\0" \

// ================
//  Fallback shaders
// ================

//
// Generic vertex shader
//

#define GLSL_FALLBACK_VERTEX_SHADER \
	"void main()\n" \
	"{\n" \
		"gl_Position = gl_ProjectionMatrix * gl_ModelViewMatrix * gl_Vertex;\n" \
		"gl_FrontColor = gl_Color;\n" \
		"gl_TexCoord[0].xy = gl_MultiTexCoord0.xy;\n" \
		"gl_ClipVertex = gl_ModelViewMatrix * gl_Vertex;\n" \
	"}\0"

//
// Generic fragment shader
//

#define GLSL_FALLBACK_FRAGMENT_SHADER \
	"uniform sampler2D tex;\n" \
	"uniform vec4 poly_color;\n" \
	"void main(void) {\n" \
		"gl_FragColor = texture2D(tex, gl_TexCoord[0].st) * poly_color;\n" \
	"}\0"

#endif
