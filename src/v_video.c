// SONIC ROBO BLAST 2
//-----------------------------------------------------------------------------
// Copyright (C) 1993-1996 by id Software, Inc.
// Copyright (C) 1998-2000 by DooM Legacy Team.
// Copyright (C) 1999-2018 by Sonic Team Junior.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  v_video.c
/// \brief Gamma correction LUT stuff
///        Functions to draw patches (by post) directly to screen.
///        Functions to blit a block to the screen.

#include "doomdef.h"
#include "d_main.h"
#include "g_game.h"
#include "r_local.h"
#include "p_local.h"
#include "v_video.h"
#include "hu_stuff.h"
#include "r_draw.h"
#include "r_main.h"
#include "r_fps.h"
#include "console.h"
#include "i_video.h" // rendermode
#include "z_zone.h"
#include "m_misc.h"
#include "m_random.h"
#include "doomstat.h"

#ifdef HWRENDER
#include "hardware/hw_main.h"
#include "hardware/r_opengl/r_opengl.h"
#endif

static CV_PossibleValue_t fps_cons_t[] = {{0, "No"}, {1, "Normal"}, {2, "Compact"}, {3, "Old"}, {4, "Old Compact"}, {0, NULL}};
consvar_t cv_ticrate = {"showfps", "No", CV_SAVE, fps_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL};

static void CV_palette_OnChange(void);

consvar_t cv_palette = {"palette", "", CV_CALL|CV_NOINIT, NULL, CV_palette_OnChange, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_palettenum = {"palettenum", "0", CV_CALL|CV_NOINIT, CV_Unsigned, CV_palette_OnChange, 0, NULL, NULL, 0, 0, NULL};

#ifdef BACKWARDSCOMPATCORRECTION
static CV_PossibleValue_t gamma_cons_t[] = {{0, "MIN"}, {4, "MAX"}, {0, NULL}};
consvar_t cv_globalgamma = {"gamma", "0", CV_SAVE|CV_CALL, gamma_cons_t, CV_palette_OnChange, 0, NULL, NULL, 0, 0, NULL};
#endif

static CV_PossibleValue_t brightness_cons_t[] = {{-15, "MIN"}, {5, "MAX"}, {0, NULL}};
consvar_t cv_globalbrightness = {"brightness", "0", CV_SAVE|CV_CALL, brightness_cons_t, CV_palette_OnChange, 0, NULL, NULL, 0, 0, NULL};

static CV_PossibleValue_t saturation_cons_t[] = {{0, "MIN"}, {10, "MAX"}, {0, NULL}};
consvar_t cv_globalsaturation = {"saturation", "10", CV_SAVE|CV_CALL, saturation_cons_t, CV_palette_OnChange, 0, NULL, NULL, 0, 0, NULL};

#define huecoloursteps 4

static CV_PossibleValue_t hue_cons_t[] = {{0, "MIN"}, {(huecoloursteps*6)-1, "MAX"}, {0, NULL}};
consvar_t cv_rhue = {"rhue",  "0", CV_SAVE|CV_CALL, hue_cons_t, CV_palette_OnChange, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_yhue = {"yhue",  "4", CV_SAVE|CV_CALL, hue_cons_t, CV_palette_OnChange, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_ghue = {"ghue",  "8", CV_SAVE|CV_CALL, hue_cons_t, CV_palette_OnChange, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_chue = {"chue", "12", CV_SAVE|CV_CALL, hue_cons_t, CV_palette_OnChange, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_bhue = {"bhue", "16", CV_SAVE|CV_CALL, hue_cons_t, CV_palette_OnChange, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_mhue = {"mhue", "20", CV_SAVE|CV_CALL, hue_cons_t, CV_palette_OnChange, 0, NULL, NULL, 0, 0, NULL};

consvar_t cv_rbrightness = {"rbrightness", "0", CV_SAVE|CV_CALL, brightness_cons_t, CV_palette_OnChange, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_ybrightness = {"ybrightness", "0", CV_SAVE|CV_CALL, brightness_cons_t, CV_palette_OnChange, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_gbrightness = {"gbrightness", "0", CV_SAVE|CV_CALL, brightness_cons_t, CV_palette_OnChange, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_cbrightness = {"cbrightness", "0", CV_SAVE|CV_CALL, brightness_cons_t, CV_palette_OnChange, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_bbrightness = {"bbrightness", "0", CV_SAVE|CV_CALL, brightness_cons_t, CV_palette_OnChange, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_mbrightness = {"mbrightness", "0", CV_SAVE|CV_CALL, brightness_cons_t, CV_palette_OnChange, 0, NULL, NULL, 0, 0, NULL};

consvar_t cv_rsaturation = {"rsaturation", "10", CV_SAVE|CV_CALL, saturation_cons_t, CV_palette_OnChange, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_ysaturation = {"ysaturation", "10", CV_SAVE|CV_CALL, saturation_cons_t, CV_palette_OnChange, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_gsaturation = {"gsaturation", "10", CV_SAVE|CV_CALL, saturation_cons_t, CV_palette_OnChange, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_csaturation = {"csaturation", "10", CV_SAVE|CV_CALL, saturation_cons_t, CV_palette_OnChange, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_bsaturation = {"bsaturation", "10", CV_SAVE|CV_CALL, saturation_cons_t, CV_palette_OnChange, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_msaturation = {"msaturation", "10", CV_SAVE|CV_CALL, saturation_cons_t, CV_palette_OnChange, 0, NULL, NULL, 0, 0, NULL};

consvar_t cv_allcaps = {"allcaps", "Off", 0, CV_OnOff, NULL, 0, NULL, NULL, 0, 0, NULL};

static CV_PossibleValue_t constextsize_cons_t[] = {
	{V_NOSCALEPATCH, "Small"}, {V_SMALLSCALEPATCH, "Medium"}, {V_MEDSCALEPATCH, "Large"}, {0, "Huge"},
	{0, NULL}};
static void CV_constextsize_OnChange(void);
consvar_t cv_constextsize = {"con_textsize", "Medium", CV_SAVE|CV_CALL, constextsize_cons_t, CV_constextsize_OnChange, 0, NULL, NULL, 0, 0, NULL};

consvar_t cv_menucaps = {"menucaps", "On", CV_SAVE, CV_OnOff, NULL, 0, NULL, NULL, 0, 0, NULL};

// local copy of the palette for V_GetColor()
RGBA_t *pLocalPalette = NULL;
RGBA_t *pGammaCorrectedPalette = NULL;

static size_t currentPaletteSize;

/*
The following was an extremely helpful resource when developing my Colour Cube LUT.
http://http.developer.nvidia.com/GPUGems2/gpugems2_chapter24.html
Please check it out if you're trying to maintain this.
toast 18/04/17
*/

float Cubepal[2][2][2][3] = {};
boolean Cubeapply = false;

// returns whether to apply cube, selectively avoiding expensive operations
static boolean InitCube(void)
{
	boolean apply = false;
	UINT8 q;
	float working[2][2][2][3] = // the initial positions of the corners of the colour cube!
	{
		{
			{
				{0.0f, 0.0f, 0.0f}, // black corner
				{0.0f, 0.0f, 1.0f}  // blue corner
			},
			{
				{0.0f, 1.0f, 0.0f}, // green corner
				{0.0f, 1.0f, 1.0f}  // cyan corner
			}
		},
		{
			{
				{1.0f, 0.0f, 0.0f}, // red corner
				{1.0f, 0.0f, 1.0f}  // magenta corner
			},
			{
				{1.0f, 1.0f, 0.0f}, // yellow corner
				{1.0f, 1.0f, 1.0f}  // white corner
			}
		}
	};

	float desatur[3]; // grey
	float globalbrightnessmul, globalbrightnessoffs;
	boolean doingbrightness;

	if (!loaded_config)
		return false;

#define diffcons(cv) (!fastcmp(cv.string, cv.defaultvalue))
#define diffconsbrightness(cv) (cv.value != 0)
#define diffconssat(cv) (cv.value != 10)

	doingbrightness = diffcons(cv_globalbrightness);

#define brightnessscale 8
	globalbrightnessmul = (cv_globalbrightness.value ? ((255.0f - (brightnessscale*abs(cv_globalbrightness.value))) / 255.0f) : 1.0f);
	globalbrightnessoffs = ((cv_globalbrightness.value > 0) ? ((brightnessscale*cv_globalbrightness.value) / 255.0f) : 0.0f);
	desatur[0] = desatur[1] = desatur[2] = globalbrightnessoffs + (0.33f * globalbrightnessmul);

	if (doingbrightness
		|| diffcons(cv_rhue)
		|| diffcons(cv_yhue)
		|| diffcons(cv_ghue)
		|| diffcons(cv_chue)
		|| diffcons(cv_bhue)
		|| diffcons(cv_mhue)
		|| diffconsbrightness(cv_rbrightness)
		|| diffconsbrightness(cv_ybrightness)
		|| diffconsbrightness(cv_gbrightness)
		|| diffconsbrightness(cv_cbrightness)
		|| diffconsbrightness(cv_bbrightness)
		|| diffconsbrightness(cv_mbrightness)) // set the brightness'd/hued positions (saturation is done later)
	{
		float mod, tempbrightnessmul, tempbrightnessoffs;

		apply = true;

		working[0][0][0][0] = working[0][0][0][1] = working[0][0][0][2] = globalbrightnessoffs;
		working[1][1][1][0] = working[1][1][1][1] = working[1][1][1][2] = globalbrightnessoffs+globalbrightnessmul;

#define dohue(hue, brightness, loc) \
		tempbrightnessmul = (brightness ? ((255.0f - (brightnessscale*abs(brightness)))/255.0f)*globalbrightnessmul : globalbrightnessmul);\
		tempbrightnessoffs = ((brightness > 0) ? ((brightnessscale*brightness)/255.0f) + globalbrightnessoffs : globalbrightnessoffs);\
		mod = ((hue % huecoloursteps)*(tempbrightnessmul)/huecoloursteps);\
		switch (hue/huecoloursteps)\
		{\
			case 0:\
			default:\
				loc[0] = tempbrightnessoffs+tempbrightnessmul;\
				loc[1] = tempbrightnessoffs+mod;\
				loc[2] = tempbrightnessoffs;\
				break;\
			case 1:\
				loc[0] = tempbrightnessoffs+tempbrightnessmul-mod;\
				loc[1] = tempbrightnessoffs+tempbrightnessmul;\
				loc[2] = tempbrightnessoffs;\
				break;\
			case 2:\
				loc[0] = tempbrightnessoffs;\
				loc[1] = tempbrightnessoffs+tempbrightnessmul;\
				loc[2] = tempbrightnessoffs+mod;\
				break;\
			case 3:\
				loc[0] = tempbrightnessoffs;\
				loc[1] = tempbrightnessoffs+tempbrightnessmul-mod;\
				loc[2] = tempbrightnessoffs+tempbrightnessmul;\
				break;\
			case 4:\
				loc[0] = tempbrightnessoffs+mod;\
				loc[1] = tempbrightnessoffs;\
				loc[2] = tempbrightnessoffs+tempbrightnessmul;\
				break;\
			case 5:\
				loc[0] = tempbrightnessoffs+tempbrightnessmul;\
				loc[1] = tempbrightnessoffs;\
				loc[2] = tempbrightnessoffs+tempbrightnessmul-mod;\
				break;\
		}
		dohue(cv_rhue.value, cv_rbrightness.value, working[1][0][0]);
		dohue(cv_yhue.value, cv_ybrightness.value, working[1][1][0]);
		dohue(cv_ghue.value, cv_gbrightness.value, working[0][1][0]);
		dohue(cv_chue.value, cv_cbrightness.value, working[0][1][1]);
		dohue(cv_bhue.value, cv_bbrightness.value, working[0][0][1]);
		dohue(cv_mhue.value, cv_mbrightness.value, working[1][0][1]);
#undef dohue
	}

#define dosaturation(a, e) a = ((1 - work)*e + work*a)
#define docvsat(cv_sat, hue, brightness, r, g, b) \
	if diffconssat(cv_sat)\
	{\
		float work, mod, tempbrightnessmul, tempbrightnessoffs;\
		apply = true;\
		work = (cv_sat.value/10.0f);\
		mod = ((hue % huecoloursteps)*(1.0f)/huecoloursteps);\
		if (hue & huecoloursteps)\
			mod = 2-mod;\
		else\
			mod += 1;\
		tempbrightnessmul = (brightness ? ((255.0f - (brightnessscale*abs(brightness)))/255.0f)*globalbrightnessmul : globalbrightnessmul);\
		tempbrightnessoffs = ((brightness > 0) ? ((brightnessscale*brightness)/255.0f) + globalbrightnessoffs : globalbrightnessoffs);\
		for (q = 0; q < 3; q++)\
			dosaturation(working[r][g][b][q], (tempbrightnessoffs+(desatur[q]*mod*tempbrightnessmul)));\
	}

	docvsat(cv_rsaturation, cv_rhue.value, cv_rbrightness.value, 1, 0, 0);
	docvsat(cv_ysaturation, cv_yhue.value, cv_ybrightness.value, 1, 1, 0);
	docvsat(cv_gsaturation, cv_ghue.value, cv_gbrightness.value, 0, 1, 0);
	docvsat(cv_csaturation, cv_chue.value, cv_cbrightness.value, 0, 1, 1);
	docvsat(cv_bsaturation, cv_bhue.value, cv_bbrightness.value, 0, 0, 1);
	docvsat(cv_msaturation, cv_mhue.value, cv_mbrightness.value, 1, 0, 1);

#undef brightnessscale

	if diffconssat(cv_globalsaturation)
	{
		float work = (cv_globalsaturation.value/10.0f);

		apply = true;

		for (q = 0; q < 3; q++)
		{
			dosaturation(working[1][0][0][q], desatur[q]);
			dosaturation(working[0][1][0][q], desatur[q]);
			dosaturation(working[0][0][1][q], desatur[q]);

			dosaturation(working[1][1][0][q], 2*desatur[q]);
			dosaturation(working[0][1][1][q], 2*desatur[q]);
			dosaturation(working[1][0][1][q], 2*desatur[q]);
		}
	}

#undef dosaturation

#undef diffcons
#undef diffconsbrightness
#undef diffconssat

	if (!apply)
		return false;

#define dowork(i, j, k, l) \
	if (working[i][j][k][l] > 1.0f)\
		working[i][j][k][l] = 1.0f;\
	else if (working[i][j][k][l] < 0.0f)\
		working[i][j][k][l] = 0.0f;\
	Cubepal[i][j][k][l] = working[i][j][k][l]
	for (q = 0; q < 3; q++)
	{
		dowork(0, 0, 0, q);
		dowork(1, 0, 0, q);
		dowork(0, 1, 0, q);
		dowork(1, 1, 0, q);
		dowork(0, 0, 1, q);
		dowork(1, 0, 1, q);
		dowork(0, 1, 1, q);
		dowork(1, 1, 1, q);
	}
#undef dowork

	return true;
}

#ifdef BACKWARDSCOMPATCORRECTION
/*
So it turns out that the way gamma was implemented previously, the default
colour profile of the game was messed up. Since this bad decision has been
around for a long time, and the intent is to keep the base game looking the
same, I'm not gonna be the one to remove this base modification.
toast 20/04/17
... welp yes i am (27/07/19, see the ifdef around it)
*/
const UINT8 gammatable[5][256] =
{
	{1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,
	17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,
	33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,
	49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,64,
	65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,80,
	81,82,83,84,85,86,87,88,89,90,91,92,93,94,95,96,
	97,98,99,100,101,102,103,104,105,106,107,108,109,110,111,112,
	113,114,115,116,117,118,119,120,121,122,123,124,125,126,127,128,
	128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,
	144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,
	160,161,162,163,164,165,166,167,168,169,170,171,172,173,174,175,
	176,177,178,179,180,181,182,183,184,185,186,187,188,189,190,191,
	192,193,194,195,196,197,198,199,200,201,202,203,204,205,206,207,
	208,209,210,211,212,213,214,215,216,217,218,219,220,221,222,223,
	224,225,226,227,228,229,230,231,232,233,234,235,236,237,238,239,
	240,241,242,243,244,245,246,247,248,249,250,251,252,253,254,255},

	{2,4,5,7,8,10,11,12,14,15,16,18,19,20,21,23,24,25,26,27,29,30,31,
	32,33,34,36,37,38,39,40,41,42,44,45,46,47,48,49,50,51,52,54,55,
	56,57,58,59,60,61,62,63,64,65,66,67,69,70,71,72,73,74,75,76,77,
	78,79,80,81,82,83,84,85,86,87,88,89,90,91,92,93,94,95,96,97,98,
	99,100,101,102,103,104,105,106,107,108,109,110,111,112,113,114,
	115,116,117,118,119,120,121,122,123,124,125,126,127,128,129,129,
	130,131,132,133,134,135,136,137,138,139,140,141,142,143,144,145,
	146,147,148,148,149,150,151,152,153,154,155,156,157,158,159,160,
	161,162,163,163,164,165,166,167,168,169,170,171,172,173,174,175,
	175,176,177,178,179,180,181,182,183,184,185,186,186,187,188,189,
	190,191,192,193,194,195,196,196,197,198,199,200,201,202,203,204,
	205,205,206,207,208,209,210,211,212,213,214,214,215,216,217,218,
	219,220,221,222,222,223,224,225,226,227,228,229,230,230,231,232,
	233,234,235,236,237,237,238,239,240,241,242,243,244,245,245,246,
	247,248,249,250,251,252,252,253,254,255},

	{4,7,9,11,13,15,17,19,21,22,24,26,27,29,30,32,33,35,36,38,39,40,42,
	43,45,46,47,48,50,51,52,54,55,56,57,59,60,61,62,63,65,66,67,68,69,
	70,72,73,74,75,76,77,78,79,80,82,83,84,85,86,87,88,89,90,91,92,93,
	94,95,96,97,98,100,101,102,103,104,105,106,107,108,109,110,111,112,
	113,114,114,115,116,117,118,119,120,121,122,123,124,125,126,127,128,
	129,130,131,132,133,133,134,135,136,137,138,139,140,141,142,143,144,
	144,145,146,147,148,149,150,151,152,153,153,154,155,156,157,158,159,
	160,160,161,162,163,164,165,166,166,167,168,169,170,171,172,172,173,
	174,175,176,177,178,178,179,180,181,182,183,183,184,185,186,187,188,
	188,189,190,191,192,193,193,194,195,196,197,197,198,199,200,201,201,
	202,203,204,205,206,206,207,208,209,210,210,211,212,213,213,214,215,
	216,217,217,218,219,220,221,221,222,223,224,224,225,226,227,228,228,
	229,230,231,231,232,233,234,235,235,236,237,238,238,239,240,241,241,
	242,243,244,244,245,246,247,247,248,249,250,251,251,252,253,254,254,
	255},

	{8,12,16,19,22,24,27,29,31,34,36,38,40,41,43,45,47,49,50,52,53,55,
	57,58,60,61,63,64,65,67,68,70,71,72,74,75,76,77,79,80,81,82,84,85,
	86,87,88,90,91,92,93,94,95,96,98,99,100,101,102,103,104,105,106,107,
	108,109,110,111,112,113,114,115,116,117,118,119,120,121,122,123,124,
	125,126,127,128,129,130,131,132,133,134,135,135,136,137,138,139,140,
	141,142,143,143,144,145,146,147,148,149,150,150,151,152,153,154,155,
	155,156,157,158,159,160,160,161,162,163,164,165,165,166,167,168,169,
	169,170,171,172,173,173,174,175,176,176,177,178,179,180,180,181,182,
	183,183,184,185,186,186,187,188,189,189,190,191,192,192,193,194,195,
	195,196,197,197,198,199,200,200,201,202,202,203,204,205,205,206,207,
	207,208,209,210,210,211,212,212,213,214,214,215,216,216,217,218,219,
	219,220,221,221,222,223,223,224,225,225,226,227,227,228,229,229,230,
	231,231,232,233,233,234,235,235,236,237,237,238,238,239,240,240,241,
	242,242,243,244,244,245,246,246,247,247,248,249,249,250,251,251,252,
	253,253,254,254,255},

	{16,23,28,32,36,39,42,45,48,50,53,55,57,60,62,64,66,68,69,71,73,75,76,
	78,80,81,83,84,86,87,89,90,92,93,94,96,97,98,100,101,102,103,105,106,
	107,108,109,110,112,113,114,115,116,117,118,119,120,121,122,123,124,
	125,126,128,128,129,130,131,132,133,134,135,136,137,138,139,140,141,
	142,143,143,144,145,146,147,148,149,150,150,151,152,153,154,155,155,
	156,157,158,159,159,160,161,162,163,163,164,165,166,166,167,168,169,
	169,170,171,172,172,173,174,175,175,176,177,177,178,179,180,180,181,
	182,182,183,184,184,185,186,187,187,188,189,189,190,191,191,192,193,
	193,194,195,195,196,196,197,198,198,199,200,200,201,202,202,203,203,
	204,205,205,206,207,207,208,208,209,210,210,211,211,212,213,213,214,
	214,215,216,216,217,217,218,219,219,220,220,221,221,222,223,223,224,
	224,225,225,226,227,227,228,228,229,229,230,230,231,232,232,233,233,
	234,234,235,235,236,236,237,237,238,239,239,240,240,241,241,242,242,
	243,243,244,244,245,245,246,246,247,247,248,248,249,249,250,250,251,
	251,252,252,253,254,254,255,255}
};
#endif

UINT32 V_GammaCorrect(UINT32 input, double power)
{
	RGBA_t result;
	double linear;

	result.rgba = input;

	linear = ((double)result.s.red)/255.0f;
	linear = pow(linear, power)*255.0f;
	result.s.red = (UINT8)(linear);
	linear = ((double)result.s.green)/255.0f;
	linear = pow(linear, power)*255.0f;
	result.s.green = (UINT8)(linear);
	linear = ((double)result.s.blue)/255.0f;
	linear = pow(linear, power)*255.0f;
	result.s.blue = (UINT8)(linear);

	return result.rgba;
}

// keep a copy of the palette so that we can get the RGB value for a color index at any time.
static void LoadPalette(const char *lumpname)
{
	UINT8 *pal;
	size_t i, palsize;
	lumpnum_t lumpnum;

#ifdef BACKWARDSCOMPATCORRECTION
	const UINT8 *usegamma = gammatable[cv_globalgamma.value];
#endif
	Cubeapply = InitCube();

	lumpnum = W_GetNumForName(lumpname);

	currentPaletteSize = W_LumpLength(lumpnum);
	palsize = currentPaletteSize / 3;

	Z_Free(pLocalPalette);
	Z_Free(pGammaCorrectedPalette);

	pLocalPalette = Z_Malloc(sizeof (*pLocalPalette)*palsize, PU_STATIC, NULL);
	pGammaCorrectedPalette = Z_Malloc(sizeof (*pGammaCorrectedPalette)*palsize, PU_STATIC, NULL);

	pal = W_CacheLumpNum(lumpnum, PU_CACHE);
	for (i = 0; i < palsize; i++)
	{
#ifdef BACKWARDSCOMPATCORRECTION
		pLocalPalette[i].s.red = usegamma[*pal++];
		pLocalPalette[i].s.green = usegamma[*pal++];
		pLocalPalette[i].s.blue = usegamma[*pal++];
#else
		pLocalPalette[i].s.red = *pal++;
		pLocalPalette[i].s.green = *pal++;
		pLocalPalette[i].s.blue = *pal++;
#endif
		pLocalPalette[i].s.alpha = 0xFF;

		pGammaCorrectedPalette[i].rgba = V_GammaDecode(pLocalPalette[i].rgba);

		if (!Cubeapply)
			continue;

		V_CubeApply(&pGammaCorrectedPalette[i]);
		pLocalPalette[i].rgba = V_GammaEncode(pGammaCorrectedPalette[i].rgba);
	}
}

void V_CubeApply(RGBA_t *input)
{
	float working[4][3];
	float linear;
	UINT8 q;

	if (!Cubeapply)
		return;

	linear = ((*input).s.red/255.0f);
#define dolerp(e1, e2) ((1 - linear)*e1 + linear*e2)
	for (q = 0; q < 3; q++)
	{
		working[0][q] = dolerp(Cubepal[0][0][0][q], Cubepal[1][0][0][q]);
		working[1][q] = dolerp(Cubepal[0][1][0][q], Cubepal[1][1][0][q]);
		working[2][q] = dolerp(Cubepal[0][0][1][q], Cubepal[1][0][1][q]);
		working[3][q] = dolerp(Cubepal[0][1][1][q], Cubepal[1][1][1][q]);
	}

	linear = ((*input).s.green/255.0f);
	for (q = 0; q < 3; q++)
	{
		working[0][q] = dolerp(working[0][q], working[1][q]);
		working[1][q] = dolerp(working[2][q], working[3][q]);
	}

	linear = ((*input).s.blue/255.0f);
	for (q = 0; q < 3; q++)
	{
		working[0][q] = 255*dolerp(working[0][q], working[1][q]);
		if (working[0][q] > 255.0f)
			working[0][q] = 255.0f;
		else if (working[0][q] < 0.0f)
			working[0][q] = 0.0f;
	}
#undef dolerp

	(*input).s.red = (UINT8)(working[0][0]);
	(*input).s.green = (UINT8)(working[0][1]);
	(*input).s.blue = (UINT8)(working[0][2]);
}

const char *R_GetPalname(UINT16 num)
{
	static char palname[9];
	char newpal[9] = "PLAYPAL";

	if (num > 0 && num <= 10000)
		snprintf(newpal, 8, "PAL%04u", num-1);

	memcpy(palname, newpal, 9);
	return palname;
}

const char *GetPalette(void)
{
	const char *user = cv_palette.string;

	if (user && user[0])
	{
		if (W_CheckNumForName(user) == LUMPERROR)
		{
			CONS_Alert(CONS_WARNING, "cv_palette %s lump does not exist\n", user);
		}
		else
		{
			return cv_palette.string;
		}
	}

	if (gamestate == GS_LEVEL)
		return R_GetPalname((encoremode ? mapheaderinfo[gamemap-1]->encorepal : mapheaderinfo[gamemap-1]->palette));

	return "PLAYPAL";
}

void V_ReloadPalette(void)
{
	LoadPalette(GetPalette());
}

// -------------+
// V_SetPalette : Set the current palette to use for palettized graphics
//              :
// -------------+
void V_SetPalette(INT32 palettenum)
{
	if (!pLocalPalette)
		V_ReloadPalette();

#ifdef HWRENDER
	if (rendermode == render_soft ||
	   (rendermode == render_opengl && HWR_ShouldUsePaletteRendering())) // opengl without paletterendering hates subpalettes
#endif
	{
		if (palettenum == 0)
		{
			palettenum = cv_palettenum.value;

			if (palettenum * 256U > currentPaletteSize - 256)
			{
				CONS_Alert(CONS_WARNING, "cv_palettenum %d out of range\n", palettenum);
				palettenum = 0;
			}
		}
	}

#ifdef HWRENDER
	if (rendermode == render_opengl)
		HWR_SetPalette(&pLocalPalette[palettenum*256]);
#if defined (__unix__) || defined (UNIXCOMMON) || defined (HAVE_SDL)
	else
#endif
#endif
	if (rendermode != render_none)
		I_SetPalette(&pLocalPalette[palettenum*256]);
}

void V_SetPaletteLump(const char *pal)
{
	LoadPalette(pal);
	V_SetPalette(0);
#ifdef HASINVERT
	R_MakeInvertmap();
#endif
}

static void CV_palette_OnChange(void)
{
	if (!loaded_config)
		return;
	// reload palette
	// recalculate Color Cube
	V_ReloadPalette();
	V_SetPalette(0);
}

void V_ResetPaletteCVars(void)
{
	if (!loaded_config)
		return;

#define diffcons(cv) (!fastcmp(cv.string, cv.defaultvalue))
	if diffcons(cv_palette)
		CV_StealthSetValue(&cv_palette, atoi(cv_palette.defaultvalue));
	if diffcons(cv_palettenum)
		CV_StealthSetValue(&cv_palettenum, atoi(cv_palettenum.defaultvalue));
#undef diffcons
}

static void CV_constextsize_OnChange(void)
{
	con_recalc = true;
}

// --------------------------------------------------------------------------
// Copy a rectangular area from one bitmap to another (8bpp)
// --------------------------------------------------------------------------
void VID_BlitLinearScreen(const UINT8 *restrict srcptr, UINT8 *restrict destptr, INT32 width, INT32 height, size_t srcrowbytes, size_t destrowbytes)
{
	if (srcrowbytes == destrowbytes && srcrowbytes == (size_t)width)
	{
		size_t i = srcrowbytes * height;
#if defined(__SSE__)
		while (i >= 16)
		{
			// TODO: find where the buffer is misaligned at times and align it
			_mm_storeu_ps((void *)destptr, _mm_loadu_ps((const void *)srcptr));
			srcptr += 16;
			destptr += 16;
			i -= 16;
		}
#endif
		memcpy(destptr, srcptr, i);
	}
	else
	{
		while (height--)
		{
			memcpy(destptr, srcptr, width);

			destptr += destrowbytes;
			srcptr += srcrowbytes;
		}
	}
}

static const UINT8 hudplusalpha[11]  = { 10,  8,  6,  4,  2,  0,  0,  0,  0,  0,  0};
static const UINT8 hudminusalpha[11] = { 10,  9,  9,  8,  8,  7,  7,  6,  6,  5,  5};
UINT8 hudtrans = 0;

static const UINT8 *v_colormap = NULL;
static const UINT8 *v_translevel = NULL;

#define STANDARDDRAW 1
#define MAPPEDDRAW 2
#define TRANSLUCENTDRAW 3
#define TRANSMAPPEDDRAW 4

// Draws a patch scaled to arbitrary size.
void V_DrawStretchyFixedPatch(fixed_t x, fixed_t y, fixed_t pscale, fixed_t vscale, INT32 scrn, patch_t *patch, const UINT8 *colormap, INT32 bflags)
{
	UINT8 patchdrawtype;
	UINT32 alphalevel;
	UINT32 blendmode;

	fixed_t col, ofs, colfrac, rowfrac, fdup, vdup;
	INT32 dup;
	const column_t *column;
	UINT8 *desttop, *dest, *deststart, *destend;
	const UINT8 *source, *deststop;
	fixed_t pwidth; // patch width
	fixed_t offx = 0; // x offset

	if (rendermode == render_none || !patch)
		return;

#ifdef HWRENDER
	//if (rendermode != render_soft && !con_startup)		// Why?
	if (rendermode == render_opengl)
	{
		HWR_DrawStretchyFixedPatch(patch, x, y, pscale, vscale, scrn, colormap, bflags);
		return;
	}
#endif

	alphalevel = ((scrn & V_ALPHAMASK) >> V_ALPHASHIFT);
	blendmode = ((bflags & V_BLENDMASK) >> V_BLENDSHIFT);

	patchdrawtype = STANDARDDRAW;

	v_translevel = NULL;
	if (alphalevel || blendmode)
	{
		switch (alphalevel)
		{
			case 13:
				alphalevel = hudminusalpha[hudtrans];
				break;
			case 14:
				alphalevel = (10 - hudtrans);
				break;
			case 15:
				alphalevel = hudplusalpha[hudtrans];
				break;
		}

		if (alphalevel >= 10)
			return; // invis

		if (alphalevel || blendmode)
		{
			v_translevel = R_GetBlendTable(blendmode+1, alphalevel);
			patchdrawtype = TRANSLUCENTDRAW;
		}
	}

	v_colormap = NULL;

	if (colormap)
	{
		v_colormap = colormap;
		patchdrawtype = (v_translevel) ? TRANSMAPPEDDRAW : MAPPEDDRAW;
	}

	dup = vid.dup;

	if (scrn & V_SCALEPATCHMASK)
	{
		switch ((scrn & V_SCALEPATCHMASK) >> V_SCALEPATCHSHIFT)
		{
			case 1: // V_NOSCALEPATCH
				dup = 1;
				break;
			case 2: // V_SMALLSCALEPATCH
				dup = vid.smalldup;
				break;
			case 3: // V_MEDSCALEPATCH
				dup = vid.meddup;
				break;
			default:
				break;
		}
	}

	fdup = vdup = FixedMul(dup<<FRACBITS, pscale);

	if (vscale != pscale)
	{
		vdup = FixedMul(dup<<FRACBITS, vscale);
		colfrac = FixedDiv(FRACUNIT, fdup);
		rowfrac = FixedDiv(FRACUNIT, vdup);
	}
	else
	{
		colfrac = rowfrac = FixedDiv(FRACUNIT, fdup);
	}

	// So it turns out offsets aren't scaled in V_NOSCALESTART unless V_OFFSET is applied ...poo, that's terrible
	// For now let's just at least give V_OFFSET the ability to support V_FLIP
	// I'll probably make a better fix for 2.2 where I don't have to worry about breaking existing support for stuff
	// -- Monster Iestyn 29/10/18
	{
		fixed_t offsetx = 0, offsety = 0;

		// left offset
		if (scrn & V_FLIP)
			offsetx = FixedMul((patch->width - patch->leftoffset)<<FRACBITS, pscale) + 1;
		else
			offsetx = FixedMul(patch->leftoffset<<FRACBITS, pscale);

		// top offset
		// TODO: make some kind of vertical version of V_FLIP, maybe by deprecating V_OFFSET in future?!?
		offsety = FixedMul(patch->topoffset<<FRACBITS, vscale);

		if ((scrn & (V_NOSCALESTART|V_OFFSET)) == (V_NOSCALESTART|V_OFFSET)) // Multiply by dup
		{
			offsetx = FixedMul(offsetx, dup<<FRACBITS);
			offsety = FixedMul(offsety, dup<<FRACBITS);
		}

		// Subtract the offsets from x/y positions
		x -= offsetx;
		y -= offsety;
	}

	if (scrn & V_SPLITSCREEN)
		y += (BASEVIDHEIGHT/2)<<FRACBITS;

	if (scrn & V_HORZSCREEN)
		x += (BASEVIDWIDTH/2)<<FRACBITS;

	desttop = vid.screens[scrn&V_PARAMMASK];

	if (!desttop)
		return;

	deststop = desttop + vid.width * vid.height;

	if (scrn & V_NOSCALESTART)
	{
		x >>= FRACBITS;
		y >>= FRACBITS;
		desttop += (y*vid.width) + x;
	}
	else
	{
		x = FixedMul(x,dup<<FRACBITS);
		y = FixedMul(y,dup<<FRACBITS);
		x >>= FRACBITS;
		y >>= FRACBITS;

		// Center it if necessary
		if (!(scrn & V_SCALEPATCHMASK))
		{
			// if it's meant to cover the whole screen, black out the rest (ONLY IF TOP LEFT ISN'T TRANSPARENT)
			if (x == 0 && patch->width == BASEVIDWIDTH && y == 0 && patch->height == BASEVIDHEIGHT)
			{
				column = (const column_t *)((const UINT8 *)(patch->columns) + (patch->columnofs[0]));
				if (!column->topdelta)
				{
					source = (const UINT8 *)(column) + 3;
					V_DrawFill(0, 0, BASEVIDWIDTH, BASEVIDHEIGHT, source[0]);
				}
			}

			if (vid.width != BASEVIDWIDTH * dup)
			{
				// dup adjustments pretend that screen width is BASEVIDWIDTH * dup,
				// so center this imaginary screen
				if ((scrn & (V_HORZSCREEN|V_SNAPTOLEFT)) == (V_HORZSCREEN|V_SNAPTOLEFT))
					x += (vid.width/2 - (BASEVIDWIDTH/2 * dup));
				else if (scrn & V_SNAPTORIGHT)
					x += (vid.width - (BASEVIDWIDTH * dup));
				else if (!(scrn & V_SNAPTOLEFT))
					x += (vid.width - (BASEVIDWIDTH * dup)) / 2;
			}

			if (vid.height != BASEVIDHEIGHT * dup)
			{
				// same thing here
				if ((scrn & (V_SPLITSCREEN|V_SNAPTOTOP)) == (V_SPLITSCREEN|V_SNAPTOTOP))
					y += (vid.height/2 - (BASEVIDHEIGHT/2 * dup));
				else if (scrn & V_SNAPTOBOTTOM)
					y += (vid.height - (BASEVIDHEIGHT * dup));
				else if (!(scrn & V_SNAPTOTOP))
					y += (vid.height - (BASEVIDHEIGHT * dup)) / 2;
			}
		}

		desttop += (y*vid.width) + x;
	}

	if (pscale != FRACUNIT) // scale width properly
	{
		pwidth = patch->width<<FRACBITS;
		pwidth = FixedMul(pwidth, pscale);
		pwidth = FixedMul(pwidth, dup<<FRACBITS);
		pwidth >>= FRACBITS;
	}
	else
		pwidth = patch->width * dup;

	deststart = desttop;
	destend = desttop + pwidth;

	const INT32 stride = vid.width;

	for (col = 0; (col>>FRACBITS) < patch->width; col += colfrac, ++offx, desttop++)
	{
		INT32 topdelta, prevdelta = -1;
		if (scrn & V_FLIP) // offx is measured from right edge instead of left
		{
			if (x+pwidth-offx < 0) // don't draw off the left of the screen (WRAP PREVENTION)
				break;
			if (x+pwidth-offx >= stride) // don't draw off the right of the screen (WRAP PREVENTION)
				continue;
		}
		else
		{
			if (x+offx < 0) // don't draw off the left of the screen (WRAP PREVENTION)
				continue;
			if (x+offx >= stride) // don't draw off the right of the screen (WRAP PREVENTION)
				break;
		}
		column = (const column_t *)((const UINT8 *)(patch->columns) + (patch->columnofs[col>>FRACBITS]));

		switch (patchdrawtype)
		{
			case STANDARDDRAW:
				while (column->topdelta != 0xff)
				{
					topdelta = column->topdelta;
					if (topdelta <= prevdelta)
						topdelta += prevdelta;
					prevdelta = topdelta;
					source = (const UINT8 *)(column) + 3;
					dest = desttop;
					if (scrn & V_FLIP)
						dest = deststart + (destend - desttop);
					dest += FixedInt(FixedMul(topdelta<<FRACBITS,vdup))*stride;

					for (ofs = 0; dest < deststop && (ofs>>FRACBITS) < column->length; ofs += rowfrac)
					{
						if (dest >= vid.screens[scrn&V_PARAMMASK]) // don't draw off the top of the screen (CRASH PREVENTION)
							*dest = source[ofs>>FRACBITS];
						dest += stride;
					}
					column = (const column_t *)((const UINT8 *)column + column->length + 4);
				}
				break;

			case MAPPEDDRAW:
				while (column->topdelta != 0xff)
				{
					topdelta = column->topdelta;
					if (topdelta <= prevdelta)
						topdelta += prevdelta;
					prevdelta = topdelta;
					source = (const UINT8 *)(column) + 3;
					dest = desttop;
					if (scrn & V_FLIP)
						dest = deststart + (destend - desttop);
					dest += FixedInt(FixedMul(topdelta<<FRACBITS,vdup))*stride;

					for (ofs = 0; dest < deststop && (ofs>>FRACBITS) < column->length; ofs += rowfrac)
					{
						if (dest >= vid.screens[scrn&V_PARAMMASK]) // don't draw off the top of the screen (CRASH PREVENTION)
							*dest = *(v_colormap + source[ofs>>FRACBITS]);
						dest += stride;
					}
					column = (const column_t *)((const UINT8 *)column + column->length + 4);
				}
				break;

			case TRANSLUCENTDRAW:
				while (column->topdelta != 0xff)
				{
					topdelta = column->topdelta;
					if (topdelta <= prevdelta)
						topdelta += prevdelta;
					prevdelta = topdelta;
					source = (const UINT8 *)(column) + 3;
					dest = desttop;
					if (scrn & V_FLIP)
						dest = deststart + (destend - desttop);
					dest += FixedInt(FixedMul(topdelta<<FRACBITS,vdup))*stride;

					for (ofs = 0; dest < deststop && (ofs>>FRACBITS) < column->length; ofs += rowfrac)
					{
						if (dest >= vid.screens[scrn&V_PARAMMASK]) // don't draw off the top of the screen (CRASH PREVENTION)
							*dest = *(v_translevel + ((source[ofs>>FRACBITS]<<8)&0xff00) + (*dest&0xff));
						dest += stride;
					}
					column = (const column_t *)((const UINT8 *)column + column->length + 4);
				}
				break;

			case TRANSMAPPEDDRAW:
				while (column->topdelta != 0xff)
				{
					topdelta = column->topdelta;
					if (topdelta <= prevdelta)
						topdelta += prevdelta;
					prevdelta = topdelta;
					source = (const UINT8 *)(column) + 3;
					dest = desttop;
					if (scrn & V_FLIP)
						dest = deststart + (destend - desttop);
					dest += FixedInt(FixedMul(topdelta<<FRACBITS,vdup))*stride;

					for (ofs = 0; dest < deststop && (ofs>>FRACBITS) < column->length; ofs += rowfrac)
					{
						if (dest >= vid.screens[scrn&V_PARAMMASK]) // don't draw off the top of the screen (CRASH PREVENTION)
							*dest = *(v_translevel + (((*(v_colormap + source[ofs>>FRACBITS]))<<8)&0xff00) + (*dest&0xff));
						dest += stride;
					}
					column = (const column_t *)((const UINT8 *)column + column->length + 4);
				}
				break;
		}
	}
}

// Draws a patch cropped and scaled to arbitrary size.
void V_DrawCroppedPatch(fixed_t x, fixed_t y, fixed_t pscale, INT32 scrn, patch_t *patch, fixed_t sx, fixed_t sy, fixed_t w, fixed_t h)
{
	UINT8 patchdrawtype;
	UINT32 alphalevel = 0;

	fixed_t col, ofs, colfrac, rowfrac, fdup;
	const column_t *column;
	UINT8 *desttop, *dest;
	const UINT8 *source, *deststop;

	if (rendermode == render_none || !patch)
		return;

#ifdef HWRENDER
	if (rendermode == render_opengl)
	{
		HWR_DrawCroppedPatch(patch, x, y, pscale, scrn, sx, sy, w, h);
		return;
	}
#endif

	patchdrawtype = STANDARDDRAW;

	v_translevel = NULL;
	if ((alphalevel = ((scrn & V_ALPHAMASK) >> V_ALPHASHIFT)))
	{
		switch (alphalevel)
		{
			case 13:
				alphalevel = hudminusalpha[hudtrans];
				break;
			case 14:
				alphalevel = (10 - hudtrans);
				break;
			case 15:
				alphalevel = hudplusalpha[hudtrans];
				break;
		}

		if (alphalevel >= 10)
			return; // invis

		if (alphalevel)
		{
			v_translevel = transtables + ((alphalevel-1)<<FF_TRANSSHIFT);
			patchdrawtype = TRANSLUCENTDRAW;
		}
	}

	fdup = FixedMul(vid.dup<<FRACBITS, pscale);
	colfrac = rowfrac = FixedDiv(FRACUNIT, fdup);

	y -= FixedMul(patch->topoffset<<FRACBITS, pscale);
	x -= FixedMul(patch->leftoffset<<FRACBITS, pscale);

	desttop = vid.screens[scrn&V_PARAMMASK];

	if (!desttop)
		return;

	deststop = desttop + vid.width * vid.height;

	if (scrn & V_NOSCALESTART)
	{
		x >>= FRACBITS;
		y >>= FRACBITS;
		desttop += (y*vid.width) + x;
	}
	else
	{
		x = FixedMul(x, vid.dup<<FRACBITS);
		y = FixedMul(y, vid.dup<<FRACBITS);
		x >>= FRACBITS;
		y >>= FRACBITS;

		// Center it if necessary
		if (!(scrn & V_SCALEPATCHMASK))
		{
			// if it's meant to cover the whole screen, black out the rest
			// no the patch is cropped do not do this ever

			if (vid.width != BASEVIDWIDTH * vid.dup)
			{
				// vid.dup adjustments pretend that screen width is BASEVIDWIDTH * vid.dup,
				// so center this imaginary screen
				if (scrn & V_SNAPTORIGHT)
					x += (vid.width - (BASEVIDWIDTH * vid.dup));
				else if (!(scrn & V_SNAPTOLEFT))
					x += (vid.width - (BASEVIDWIDTH * vid.dup)) / 2;
			}

			if (vid.height != BASEVIDHEIGHT * vid.dup)
			{
				// same thing here
				if (scrn & V_SNAPTOBOTTOM)
					y += (vid.height - (BASEVIDHEIGHT * vid.dup));
				else if (!(scrn & V_SNAPTOTOP))
					y += (vid.height - (BASEVIDHEIGHT * vid.dup)) / 2;
			}
		}

		desttop += (y*vid.width) + x;
	}

	const INT32 stride = vid.width;

	for (col = sx<<FRACBITS; (col>>FRACBITS) < patch->width && ((col>>FRACBITS) - sx) < w; col += colfrac, ++x, desttop++)
	{
		INT32 topdelta, prevdelta = -1;

		if (x < 0) // don't draw off the left of the screen (WRAP PREVENTION)
			continue;

		if (x >= stride) // don't draw off the right of the screen (WRAP PREVENTION)
			break;

		column = (const column_t *)((const UINT8 *)(patch->columns) + (patch->columnofs[col>>FRACBITS]));

		switch (patchdrawtype)
		{
			case STANDARDDRAW:
				while (column->topdelta != 0xff)
				{
					topdelta = column->topdelta;
					if (topdelta <= prevdelta)
						topdelta += prevdelta;
					prevdelta = topdelta;
					source = (const UINT8 *)(column) + 3;
					dest = desttop;
					if (topdelta-sy > 0)
					{
						dest += FixedInt(FixedMul((topdelta-sy)<<FRACBITS,fdup))*stride;
						ofs = 0;
					}
					else
						ofs = (sy-topdelta)<<FRACBITS;

					for (; dest < deststop && (ofs>>FRACBITS) < column->length && (((ofs>>FRACBITS) - sy) + topdelta) < h; ofs += rowfrac)
					{
						if (dest >= vid.screens[scrn&V_PARAMMASK]) // don't draw off the top of the screen (CRASH PREVENTION)
							*dest = source[ofs>>FRACBITS];
						dest += stride;
					}
					column = (const column_t *)((const UINT8 *)column + column->length + 4);
				}
				break;

			case TRANSLUCENTDRAW:
				while (column->topdelta != 0xff)
				{
					topdelta = column->topdelta;
					if (topdelta <= prevdelta)
						topdelta += prevdelta;
					prevdelta = topdelta;
					source = (const UINT8 *)(column) + 3;
					dest = desttop;
					if (topdelta-sy > 0)
					{
						dest += FixedInt(FixedMul((topdelta-sy)<<FRACBITS,fdup))*stride;
						ofs = 0;
					}
					else
						ofs = (sy-topdelta)<<FRACBITS;

					for (; dest < deststop && (ofs>>FRACBITS) < column->length && (((ofs>>FRACBITS) - sy) + topdelta) < h; ofs += rowfrac)
					{
						if (dest >= vid.screens[scrn&V_PARAMMASK]) // don't draw off the top of the screen (CRASH PREVENTION)
							*dest = *(v_translevel + ((source[ofs>>FRACBITS]<<8)&0xff00) + (*dest&0xff));
						dest += stride;
					}
					column = (const column_t *)((const UINT8 *)column + column->length + 4);
				}
				break;
		}
	}
}

//
// V_DrawContinueIcon
// Draw a mini player!  If we can, that is.  Otherwise we draw a star.
//
void V_DrawContinueIcon(INT32 x, INT32 y, INT32 flags, INT32 skinnum, UINT8 skincolor)
{
	if (skinnum < 0 || skinnum >= numskins || (skins[skinnum].flags & SF_HIRES))
		V_DrawScaledPatch(x - 10, y - 14, flags, W_CachePatchName("CONTINS", PU_PATCH)); // Draw a star
	else
	{ // Find front angle of the first waiting frame of the character's actual sprites
		spriteframe_t *sprframe = &skins[skinnum].spritedef.spriteframes[2 & FF_FRAMEMASK];
		patch_t *patch = W_CachePatchNum(sprframe->lumppat[0], PU_PATCH);
		const UINT8 *colormap = R_GetTranslationColormap(skinnum, skincolor, GTC_CACHE);

		// No variant for translucency
		V_DrawTinyMappedPatch(x, y, flags, patch, colormap);
	}
}

//
// V_DrawBlock
// Draw a linear block of pixels into the view buffer.
//
void V_DrawBlock(INT32 x, INT32 y, INT32 scrn, INT32 width, INT32 height, const UINT8 *src)
{
	UINT8 *dest;
	const UINT8 *deststop;

#ifdef RANGECHECK
	if (x < 0 || x + width > vid.width || y < 0 || y + height > vid.height || (unsigned)scrn > 4)
		I_Error("Bad V_DrawBlock");
#endif

	dest = vid.screens[scrn] + y*vid.width + x;
	deststop = vid.screens[scrn] + vid.width * vid.height;

	while (height--)
	{
		memcpy(dest, src, width);

		src += width;
		dest += vid.width;
		if (dest > deststop)
			return;
	}
}

//
// Fills a box of pixels with a single color, NOTE: scaled to screen size
//
// alug: now with translucency support X)
void V_DrawFill(INT32 x, INT32 y, INT32 w, INT32 h, INT32 c)
{
	UINT8 *dest;
	const UINT8 *deststop;
	UINT32 alphalevel = ((c & V_ALPHAMASK) >> V_ALPHASHIFT);

	if (rendermode == render_none)
		return;

#ifdef HWRENDER
	if (rendermode == render_opengl)
	{
		HWR_DrawFill(x, y, w, h, c);
		return;
	}
#endif

	if (!(c & V_NOSCALESTART))
	{
		if (x == 0 && y == 0 && w == BASEVIDWIDTH && h == BASEVIDHEIGHT)
		{ // Clear the entire screen, from dest to deststop. Yes, this really works.
			memset(vid.screens[0], (c&255), vid.width * vid.height);
			return;
		}

		x *= vid.dup;
		y *= vid.dup;
		w *= vid.dup;
		h *= vid.dup;

		// Center it if necessary
		if (vid.width != BASEVIDWIDTH * vid.dup)
		{
			// vid.dup adjustments pretend that screen width is BASEVIDWIDTH * vid.dup,
			// so center this imaginary screen
			if (c & V_SNAPTORIGHT)
				x += (vid.width - (BASEVIDWIDTH * vid.dup));
			else if (!(c & V_SNAPTOLEFT))
				x += (vid.width - (BASEVIDWIDTH * vid.dup)) / 2;
		}

		if (vid.height != BASEVIDHEIGHT * vid.dup)
		{
			// same thing here
			if (c & V_SNAPTOBOTTOM)
				y += (vid.height - (BASEVIDHEIGHT * vid.dup));
			else if (!(c & V_SNAPTOTOP))
				y += (vid.height - (BASEVIDHEIGHT * vid.dup)) / 2;
		}

		if (c & V_SPLITSCREEN)
			y += (BASEVIDHEIGHT * vid.dup)/2;
		if (c & V_HORZSCREEN)
			x += (BASEVIDWIDTH * vid.dup)/2;
	}

	if (x >= vid.width || y >= vid.height)
		return; // off the screen

	if (x < 0)
	{
		w += x;
		x = 0;
	}
	if (y < 0)
	{
		h += y;
		y = 0;
	}

	if (w <= 0 || h <= 0)
		return; // zero width/height wouldn't draw anything

	if (x + w > vid.width)
		w = vid.width-x;
	if (y + h > vid.height)
		h = vid.height-y;

	dest = vid.screens[0] + y*vid.width + x;
	deststop = vid.screens[0] + vid.width * vid.height;

	if (alphalevel)
	{
		switch (alphalevel)
		{
			case 13:
				alphalevel = hudminusalpha[hudtrans];
				break;
			case 14:
				alphalevel = (10 - hudtrans);
				break;
			case 15:
				alphalevel = hudplusalpha[hudtrans];
				break;
		}

		if (alphalevel >= 10)
			return; // invis
	}

	c &= 255;

	if (alphalevel)
	{
		const UINT8 *fadetable = ((UINT8 *)transtables + ((alphalevel-1)<<FF_TRANSSHIFT) + (c*256));
		for (;(--h >= 0) && dest < deststop; dest += vid.width)
		{
			for (x = 0; x < w; x++)
				dest[x] = fadetable[dest[x]];
		}
	}
	else
	{
		for (;(--h >= 0) && dest < deststop; dest += vid.width)
			memset(dest, c, w);
	}
}

#ifdef HWRENDER
// This is now a function since it's otherwise repeated 2 times and honestly looks retarded:
static UINT32 V_GetHWConsBackColor(void)
{
	RGBA_t output;

	switch (cons_backcolor.value)
	{
		case 0:		output.s.red = 0xff; output.s.green = 0xff; output.s.blue = 0xff;	break; 	// White
		case 1:		output.s.red = 0x80; output.s.green = 0x80; output.s.blue = 0x80;	break; 	// Black
		case 2:		output.s.red = 0xde; output.s.green = 0xb8; output.s.blue = 0x87;	break;	// Sepia
		case 3:		output.s.red = 0x40; output.s.green = 0x20; output.s.blue = 0x10;	break; 	// Brown
		case 4:		output.s.red = 0xfa; output.s.green = 0x80; output.s.blue = 0x72;	break; 	// Pink
		case 5:		output.s.red = 0xff; output.s.green = 0x69; output.s.blue = 0xb4;	break; 	// Raspberry
		case 6:		output.s.red = 0xff; output.s.green = 0x00; output.s.blue = 0x00;	break; 	// Red
		case 7:		output.s.red = 0xff; output.s.green = 0xd6; output.s.blue = 0x83;	break;	// Creamsicle
		case 8:		output.s.red = 0xff; output.s.green = 0x80; output.s.blue = 0x00;	break; 	// Orange
		case 9:		output.s.red = 0xda; output.s.green = 0xa5; output.s.blue = 0x20;	break; 	// Gold
		case 10:	output.s.red = 0x80; output.s.green = 0x80; output.s.blue = 0x00;	break; 	// Yellow
		case 11:	output.s.red = 0x00; output.s.green = 0xff; output.s.blue = 0x00;	break; 	// Emerald
		case 12:	output.s.red = 0x00; output.s.green = 0x80; output.s.blue = 0x00;	break; 	// Green
		case 13:	output.s.red = 0x40; output.s.green = 0x80; output.s.blue = 0xff;	break; 	// Cyan
		case 14:	output.s.red = 0x46; output.s.green = 0x82; output.s.blue = 0xb4;	break; 	// Steel
		case 15:	output.s.red = 0x1e; output.s.green = 0x90; output.s.blue = 0xff;	break;	// Periwinkle
		case 16:	output.s.red = 0x00; output.s.green = 0x00; output.s.blue = 0xff;	break; 	// Blue
		case 17:	output.s.red = 0xff; output.s.green = 0x00; output.s.blue = 0xff;	break; 	// Purple
		case 18:	output.s.red = 0xee; output.s.green = 0x82; output.s.blue = 0xee;	break; 	// Lavender
		// Default green
		default:	output.s.red = 0x00; output.s.green = 0x80; output.s.blue = 0x00;	break;
	}

	if (!HWR_ShouldUsePaletteRendering())
		V_CubeApply(&output);

	return (output.s.red << 24) | (output.s.green << 16) | (output.s.blue << 8);
}
#endif

// THANK YOU MPC!!!

void V_DrawFillConsoleMap(INT32 x, INT32 y, INT32 w, INT32 h, INT32 c)
{
	UINT8 *dest;
	INT32 u, v;
	UINT32 alphalevel = 0;

	if (rendermode == render_none)
		return;

#ifdef HWRENDER
	if (rendermode == render_opengl)
	{
		UINT32 hwcolor = V_GetHWConsBackColor();
		HWR_DrawConsoleFill(x, y, w, h, hwcolor, c); // we still use the regular color stuff but only for flags. actual draw color is "hwcolor" for this.
		return;
	}
#endif

	if (!(c & V_NOSCALESTART))
	{
		if (x == 0 && y == 0 && w == BASEVIDWIDTH && h == BASEVIDHEIGHT)
		{ // Clear the entire screen, from dest to deststop. Yes, this really works.
			memset(vid.screens[0], (UINT8)(c&255), vid.width * vid.height);
			return;
		}

		x *= vid.dup;
		y *= vid.dup;
		w *= vid.dup;
		h *= vid.dup;

		// Center it if necessary
		if (vid.width != BASEVIDWIDTH * vid.dup)
		{
			// vid.dup adjustments pretend that screen width is BASEVIDWIDTH * vid.dup,
			// so center this imaginary screen
			if (c & V_SNAPTORIGHT)
				x += (vid.width - (BASEVIDWIDTH * vid.dup));
			else if (!(c & V_SNAPTOLEFT))
				x += (vid.width - (BASEVIDWIDTH * vid.dup)) / 2;
		}

		if (vid.height != BASEVIDHEIGHT * vid.dup)
		{
			// same thing here
			if (c & V_SNAPTOBOTTOM)
				y += (vid.height - (BASEVIDHEIGHT * vid.dup));
			else if (!(c & V_SNAPTOTOP))
				y += (vid.height - (BASEVIDHEIGHT * vid.dup)) / 2;
		}
	}

	if (x >= vid.width || y >= vid.height)
		return; // off the screen
	if (x < 0) {
		w += x;
		x = 0;
	}
	if (y < 0) {
		h += y;
		y = 0;
	}

	if (w <= 0 || h <= 0)
		return; // zero width/height wouldn't draw anything
	if (x + w > vid.width)
		w = vid.width-x;
	if (y + h > vid.height)
		h = vid.height-y;

	dest = vid.screens[0] + y*vid.width + x;

	if ((alphalevel = ((c & V_ALPHAMASK) >> V_ALPHASHIFT)))
	{
		switch (alphalevel)
		{
			case 13:
				alphalevel = hudminusalpha[hudtrans];
				break;
			case 14:
				alphalevel = (10 - hudtrans);
				break;
			case 15:
				alphalevel = hudplusalpha[hudtrans];
				break;
		}

		if (alphalevel >= 10)
			return; // invis
	}

	c &= 255;

	if (!alphalevel)
	{
		for (v = 0; v < h; v++, dest += vid.width)
		{
			for (u = 0; u < w; u++)
			{
				dest[u] = consolebgmap[dest[u]];
			}
		}
	}
	else
	{ // mpc 12-04-2018
		const UINT8 *fadetable = ((UINT8 *)transtables + ((alphalevel-1)<<FF_TRANSSHIFT) + (c*256));
#define clip(x,y) (x>y) ? y : x
		w = clip(w,vid.width);
		h = clip(h,vid.height);
#undef clip
		for (v = 0; v < h; v++, dest += vid.width)
		{
			for (u = 0; u < w; u++)
			{
				dest[u] = fadetable[consolebgmap[dest[u]]];
			}
		}
	}
}

//
// Fills a triangle of pixels with a single color, NOTE: scaled to screen size
//
// ...
// ..  <-- this shape only for now, i'm afraid
// .
//
void V_DrawDiag(INT32 x, INT32 y, INT32 wh, INT32 c)
{
	UINT8 *dest;
	const UINT8 *deststop;
	INT32 w, h, wait = 0;

	if (rendermode == render_none)
		return;

#ifdef HWRENDER
	if (rendermode != render_soft && !con_startup)
	{
		HWR_DrawDiag(x, y, wh, c);
		return;
	}
#endif

	if (!(c & V_NOSCALESTART))
	{
		x  *= vid.dup;
		y  *= vid.dup;
		wh *= vid.dup;

		// Center it if necessary
		if (vid.width != BASEVIDWIDTH * vid.dup)
		{
			// vid.dup adjustments pretend that screen width is BASEVIDWIDTH * vid.dup,
			// so center this imaginary screen
			if (c & V_SNAPTORIGHT)
				x += (vid.width - (BASEVIDWIDTH * vid.dup));
			else if (!(c & V_SNAPTOLEFT))
				x += (vid.width - (BASEVIDWIDTH * vid.dup)) / 2;
		}
		if (vid.height != BASEVIDHEIGHT * vid.dup)
		{
			// same thing here
			if (c & V_SNAPTOBOTTOM)
				y += (vid.height - (BASEVIDHEIGHT * vid.dup));
			else if (!(c & V_SNAPTOTOP))
				y += (vid.height - (BASEVIDHEIGHT * vid.dup)) / 2;
		}
		if (c & V_SPLITSCREEN)
			y += (BASEVIDHEIGHT * vid.dup)/2;
		if (c & V_HORZSCREEN)
			x += (BASEVIDWIDTH * vid.dup)/2;
	}

	if (x >= vid.width || y >= vid.height)
		return; // off the screen

	if (y < 0)
	{
		wh += y;
		y = 0;
	}

	w = h = wh;

	if (x < 0)
	{
		w += x;
		x = 0;
	}

	if (w <= 0 || h <= 0)
		return; // zero width/height wouldn't draw anything
	if (x + w > vid.width)
	{
		wait = w - (vid.width - x);
		w = vid.width - x;
	}
	if (y + w > vid.height)
		h = vid.height - y;

	if (h > w)
		h = w;

	dest = vid.screens[0] + y*vid.width + x;
	deststop = vid.screens[0] + vid.width * vid.height;

	c &= 255;

	for (;(--h >= 0) && dest < deststop; dest += vid.width)
	{
		memset(dest, c, w);
		if (wait)
			wait--;
		else
			w--;
	}
}

//
// Fills a box of pixels using a flat texture as a pattern, scaled to screen size.
//
void V_DrawFlatFill(INT32 x, INT32 y, INT32 w, INT32 h, lumpnum_t flatnum)
{
	INT32 u, v;
	fixed_t dx, dy, xfrac, yfrac;
	const UINT8 *src, *deststop;
	UINT8 *flat, *dest;
	size_t size, lflatsize, flatshift;

#ifdef HWRENDER
	if (rendermode == render_opengl)
	{
		HWR_DrawFlatFill(x, y, w, h, flatnum);
		return;
	}
#endif

	size = W_LumpLength(flatnum);

	switch (size)
	{
		case 4194304: // 2048x2048 lump
			lflatsize = 2048;
			flatshift = 10;
			break;
		case 1048576: // 1024x1024 lump
			lflatsize = 1024;
			flatshift = 9;
			break;
		case 262144:// 512x512 lump
			lflatsize = 512;
			flatshift = 8;
			break;
		case 65536: // 256x256 lump
			lflatsize = 256;
			flatshift = 7;
			break;
		case 16384: // 128x128 lump
			lflatsize = 128;
			flatshift = 7;
			break;
		case 1024: // 32x32 lump
			lflatsize = 32;
			flatshift = 5;
			break;
		default: // 64x64 lump
			lflatsize = 64;
			flatshift = 6;
			break;
	}

	flat = W_CacheLumpNum(flatnum, PU_CACHE);

	dest = vid.screens[0] + y*vid.dup*vid.width + x*vid.dup;
	deststop = vid.screens[0] + vid.width * vid.height;

	// from V_DrawScaledPatch
	if (vid.width != BASEVIDWIDTH * vid.dup)
	{
		// vid.dup adjustments pretend that screen width is BASEVIDWIDTH * vid.dup,
		// so center this imaginary screen
		dest += (vid.width - (BASEVIDWIDTH * vid.dup)) / 2;
	}

	if (vid.height != BASEVIDHEIGHT * vid.dup)
	{
		// same thing here
		dest += (vid.height - (BASEVIDHEIGHT * vid.dup)) * vid.width / 2;
	}

	w *= vid.dup;
	h *= vid.dup;

	dx = FixedDiv(FRACUNIT, vid.dup<<(FRACBITS-2));
	dy = FixedDiv(FRACUNIT, vid.dup<<(FRACBITS-2));

	yfrac = 0;
	for (v = 0; v < h; v++, dest += vid.width)
	{
		xfrac = 0;
		src = flat + (((yfrac>>FRACBITS) & (lflatsize - 1)) << flatshift);
		for (u = 0; u < w; u++)
		{
			if (&dest[u] > deststop)
				return;
			dest[u] = src[(xfrac>>FRACBITS)&(lflatsize-1)];
			xfrac += dx;
		}
		yfrac += dy;
	}
}

//
// V_DrawPatchFill
//
void V_DrawPatchFill(patch_t *pat)
{
	INT32 x, y, pw = pat->width * vid.dup, ph = pat->height * vid.dup;

	for (x = 0; x < vid.width; x += pw)
	{
		for (y = 0; y < vid.height; y += ph)
			V_DrawScaledPatch(x, y, V_NOSCALESTART, pat);
	}
}

// Draws a patch and tries to always fill the screen with the patch
void V_DrawAdaptiveScaledFullScreenPatch(patch_t *patch)
{
	fixed_t x = 0, y = 0;
	fixed_t scale = ((vid.width * FRACUNIT) / patch->width); // fit the screen horizontally
	fixed_t scaled_height = FixedMul(patch->height << FRACBITS, scale);

	// however, if this means the patch doesent fill out the screen vertically then
	if (scaled_height < (vid.height << FRACBITS))
	{
		scale = ((vid.height * FRACUNIT) / patch->height); // scale it to fit the screen vertically
		x = ((vid.width << FRACBITS) - FixedMul(patch->width << FRACBITS, scale)) / 2;
	}
	else
		y = (vid.height << FRACBITS) - scaled_height;

	V_DrawFixedPatch(x, y, scale, V_NOSCALEPATCH, patch, NULL);
}

// Draws a patch and scales it to fill out the screen vertically while remaining centered
void V_DrawVerticallyScaledFullScreenPatch(patch_t *patch)
{
	fixed_t scale = ((vid.height * FRACUNIT) / patch->height);
	fixed_t x = ((vid.width << FRACBITS) - FixedMul(patch->width << FRACBITS, scale)) / 2; // i fucking hate maths

	V_DrawFixedPatch(x, 0, scale, V_NOSCALEPATCH, patch, NULL);
}

// Draws a patch and scales it to fill out the screen horizontally
// centers the patch when its too small to fit the screen vertically
void V_DrawHorizontallyScaledFullScreenPatch(patch_t *patch)
{
	fixed_t scale = ((vid.width * FRACUNIT) / patch->width);
	fixed_t scaled_height = FixedMul(patch->height << FRACBITS, scale);
	fixed_t y = (vid.height << FRACBITS) - scaled_height;

	// center it if it does not fit the screen vertically
	if (scaled_height < (vid.height << FRACBITS))
	{
		y /= 2;
	}

	V_DrawFixedPatch(0, y, scale, V_NOSCALEPATCH, patch, NULL);
}

void V_DrawVhsEffect(boolean rewind)
{
	fixed_t uby, dby;
	// upbary is the bar going from top to bottom for some reason
	static fixed_t upbary = 100*FRACUNIT, downbary = 150*FRACUNIT;

	UINT8 barsize, updistort, downdistort;

	UINT16 y;
	UINT32 x, pos;

	UINT8 *buf, *tmp;
	UINT8 *normalmapstart, *thismapstart;
#ifdef HQ_VHS
	UINT8 *tmapstart;
#endif
	SINT8 offs;

	if (cv_reducevfx.value)
		return;

	barsize = vid.udup << 5;
	updistort = vid.udup << (rewind ? 5 : 3);
	downdistort = updistort >> 1;

	if (rewind)
		V_DrawVhsEffect(false); // experimentation

	upbary -= renderdeltatics * (fixed_t)(vid.udup * (rewind ? 3 : 1.8f));
	downbary += renderdeltatics * (vid.udup * (rewind ? 2 : 1));

	if (upbary < -barsize*FRACUNIT)
		upbary = vid.height << FRACBITS;
	if (upbary > vid.height << FRACBITS)
		upbary = -barsize*FRACUNIT;
	if (downbary > vid.height << FRACBITS)
		downbary = -barsize*FRACUNIT;
	if (downbary < -barsize*FRACUNIT)
		downbary = vid.height << FRACBITS;

	uby = upbary >> FRACBITS;
	dby = downbary >> FRACBITS;

#ifdef HWRENDER
	if (rendermode == render_opengl)
	{
		HWR_RenderVhsEffect(uby, dby, updistort, downdistort, barsize);
		return;
	}
#endif

	buf = vid.screens[0];
	tmp = vid.screens[4];

	normalmapstart = ((UINT8 *)transtables + (8<<FF_TRANSSHIFT|(19<<8)));
#ifdef HQ_VHS
	tmapstart = ((UINT8 *)transtables + (6<<FF_TRANSSHIFT));
#endif

	pos = 0;

	for (y = 0; y < vid.height; y+=2)
	{
		thismapstart = normalmapstart;
		offs = 0;

		if (y >= uby && y < uby+barsize)
		{
			thismapstart -= (2<<FF_TRANSSHIFT) - (5<<8);
			offs += updistort * 2.0f * min(y-uby, uby+barsize-y) / barsize;
		}

		if (y >= dby && y < dby+barsize)
		{
			thismapstart -= (2<<FF_TRANSSHIFT) - (5<<8);
			offs -= downdistort * 2.0f * min(y-dby, dby+barsize-y) / barsize;
		}

		offs += M_RandomKey(vid.dup<<1);

		// lazy way to avoid crashes
		if ((y == 0 && offs < 0) || (y >= vid.height-2 && offs > 0))
			offs = 0;

		for (x = min(pos+(size_t)vid.width*2, (size_t)vid.width*vid.height); pos < x; pos++)
		{
			tmp[pos] = thismapstart[buf[pos+offs]];
#ifdef HQ_VHS
			tmp[pos] = tmapstart[buf[pos]<<8 | tmp[pos]];
#endif
		}
	}

	memcpy(buf, tmp, vid.width*vid.height);
}

//
// Fade all the screen buffer, so that the menu is more readable,
// especially now that we use the small hufont in the menus...
// If color is 0x00 to 0xFF, draw transtable (strength range 0-9).
// Else, use COLORMAP lump (strength range 0-31).
// IF YOU ARE NOT CAREFUL, THIS CAN AND WILL CRASH!
// I have kept the safety checks out of this function;
// the v.fadeScreen Lua interface handles those.
//
void V_DrawFadeScreen(UINT16 color, UINT8 strength)
{
#ifdef HWRENDER
	if (rendermode == render_opengl)
	{
		HWR_FadeScreenMenuBack(color, strength);
		return;
	}
#endif

	const UINT8 *fadetable =
		(color > 0xFFF0) // Grab a specific colormap palette?
		? R_GetTranslationColormap(color | 0xFFFF0000, strength, GTC_CACHE)
		: ((color & 0xFF00) // Color is not palette index?
		? ((UINT8 *)colormaps + strength*256) // Do COLORMAP fade.
		: ((UINT8 *)transtables + ((9-strength)<<FF_TRANSSHIFT) + color*256)); // Else, do TRANSMAP** fade.
	const UINT8 *deststop = vid.screens[0] + vid.width * vid.height;
	UINT8 *buf = vid.screens[0];

	// heavily simplified -- we don't need to know x or y
	// position when we're doing a full screen fade
	for (; buf < deststop; ++buf)
		*buf = fadetable[*buf];
}

// Simple translucency with one color, over a set number of lines starting from the top.
void V_DrawFadeConsBack(INT32 plines)
{
	UINT8 *deststop, *buf;

#ifdef HWRENDER // not win32 only 19990829 by Kin
	if (rendermode == render_opengl)
	{
		UINT32 hwcolor = V_GetHWConsBackColor();
		HWR_DrawConsoleBack(hwcolor, plines);
		return;
	}
#endif

	// heavily simplified -- we don't need to know x or y position,
	// just the stop position
	deststop = vid.screens[0] + vid.width * min(plines, vid.height);
	for (buf = vid.screens[0]; buf < deststop; ++buf)
		*buf = consolebgmap[*buf];
}

// Gets string colormap, used for 0x80 color codes
//
UINT8 *V_GetStringColormap(INT32 colorflags)
{
#if 0 // perfect
	switch ((colorflags & V_CHARCOLORMASK) >> V_CHARCOLORSHIFT)
	{
	case  1: // 0x81, purple
		return purplemap;
	case  2: // 0x82, yellow
		return yellowmap;
	case  3: // 0x83, green
		return greenmap;
	case  4: // 0x84, blue
		return bluemap;
	case  5: // 0x85, red
		return redmap;
	case  6: // 0x86, gray
		return graymap;
	case  7: // 0x87, orange
		return orangemap;
	case  8: // 0x88, sky
		return skymap;
	case  9: // 0x89, lavender
		return lavendermap;
	case 10: // 0x8A, gold
		return goldmap;
	case 11: // 0x8B, tea-green
		return teamap;
	case 12: // 0x8C, steel
		return steelmap;
	case 13: // 0x8D, pink
		return pinkmap;
	case 14: // 0x8E, brown
		return brownmap;
	case 15: // 0x8F, peach
		return peachmap;
	default: // reset
		return NULL;
	}
#else // optimised
	colorflags = ((colorflags & V_CHARCOLORMASK) >> V_CHARCOLORSHIFT);

	if (!colorflags || colorflags > 15) // INT32 is signed, but V_CHARCOLORMASK is a very restrictive mask.
		return NULL;

	return (purplemap+((colorflags-1)<<8));
#endif
}

// Writes a single character (draw WHITE if bit 7 set)
//
void V_DrawCharacter(INT32 x, INT32 y, INT32 c, boolean lowercaseallowed)
{
	INT32 w, flags;
	const UINT8 *colormap = V_GetStringColormap(c);

	flags = c & ~(V_CHARCOLORMASK | V_PARAMMASK);
	c &= 0x7f;
	if (lowercaseallowed)
		c -= HU_FONTSTART;
	else
		c = toupper(c) - HU_FONTSTART;

	if (c < 0 || c >= HU_FONTSIZE || !hu_font[c])
		return;

	w = hu_font[c]->width;
	if (x + w > vid.width)
		return;

	if (colormap != NULL)
		V_DrawMappedPatch(x, y, flags, hu_font[c], colormap);
	else
		V_DrawScaledPatch(x, y, flags, hu_font[c]);
}

// Writes a single character for the chat (half scaled). (draw WHITE if bit 7 set)
// 16/02/19: Scratch the scaling thing, chat doesn't work anymore under 2x res -Lat'
//
void V_DrawChatCharacter(INT32 x, INT32 y, INT32 c, boolean lowercaseallowed, UINT8 *colormap)
{
	INT32 w, flags;
	//const UINT8 *colormap = V_GetStringColormap(c);

	flags = c & ~(V_CHARCOLORMASK | V_PARAMMASK);
	c &= 0x7f;
	if (lowercaseallowed)
		c -= HU_FONTSTART;
	else
		c = toupper(c) - HU_FONTSTART;
	if (c < 0 || c >= HU_FONTSIZE || !hu_font[c])
		return;

	w = hu_font[c]->width/2;
	if (x + w > vid.width)
		return;

	V_DrawFixedPatch(x*FRACUNIT, y*FRACUNIT, FRACUNIT/2, flags, hu_font[c], colormap);
}

// Precompile a wordwrapped string to any given width.
// This is a muuuch better method than V_WORDWRAP.
char *V_WordWrap(INT32 x, INT32 w, INT32 option, const char *string)
{
	int c;
	size_t chw, i, lastusablespace = 0;
	size_t slen;
	char *newstring = Z_StrDup(string);
	INT32 spacewidth = 4, charwidth = 0;

	slen = strlen(string);

	if (w == 0)
		w = BASEVIDWIDTH;
	w -= x;
	x = 0;

	switch (option & V_SPACINGMASK)
	{
		case V_MONOSPACE:
			spacewidth = 8;
			/* FALLTHRU */
		case V_OLDSPACING:
			charwidth = 8;
			break;
		case V_6WIDTHSPACE:
			spacewidth = 6;
		default:
			break;
	}

	for (i = 0; i < slen; ++i)
	{
		c = newstring[i];

		if ((UINT8)c >= 0x80 && (UINT8)c <= 0x8F) //color parsing! -Inuyasha 2.16.09
			continue;

		if (c == '\n')
		{
			x = 0;
			lastusablespace = 0;
			continue;
		}

		if (!(option & V_ALLOWLOWERCASE))
			c = toupper(c);
		c -= HU_FONTSTART;

		if (c < 0 || c >= HU_FONTSIZE || !hu_font[c])
		{
			chw = spacewidth;
			lastusablespace = i;
		}
		else
			chw = (charwidth ? charwidth : hu_font[c]->width);

		x += chw;

		if (lastusablespace != 0 && x > w)
		{
			newstring[lastusablespace] = '\n';
			i = lastusablespace;
			lastusablespace = 0;
			x = 0;
		}
	}
	return newstring;
}

//
// Write a string using the hu_font
// NOTE: the text is centered for screens larger than the base width
//
void V_DrawString(INT32 x, INT32 y, INT32 option, const char *string)
{
	INT32 w, c, cx = x, cy = y, dup, scrwidth, center = 0, left = 0;
	const char *ch = string;
	INT32 charflags = 0;
	const UINT8 *colormap = NULL;
	INT32 spacewidth = 4, charwidth = 0;

	INT32 lowercase = (option & V_ALLOWLOWERCASE);
	option &= ~V_FLIP; // which is also shared with V_ALLOWLOWERCASE...

	if (option & V_NOSCALESTART)
	{
		dup = vid.dup;
		scrwidth = vid.width;
	}
	else
	{
		dup = 1;
		scrwidth = vid.scaledwidth;

		if (!(option & V_SNAPTOLEFT))
		{
			left = (scrwidth - BASEVIDWIDTH)/2;
			scrwidth -= left;
		}
	}

	charflags = (option & V_CHARCOLORMASK);
	colormap = V_GetStringColormap(charflags);

	switch (option & V_SPACINGMASK)
	{
		case V_MONOSPACE:
			spacewidth = 8;
			/* FALLTHRU */
		case V_OLDSPACING:
			charwidth = 8;
			break;
		case V_6WIDTHSPACE:
			spacewidth = 6;
		default:
			break;
	}

	for (;;ch++)
	{
		if (!*ch)
			break;
		if (*ch & 0x80) //color parsing -x 2.16.09
		{
			// manually set flags override color codes
			if (!(option & V_CHARCOLORMASK))
			{
				charflags = ((*ch & 0x7f) << V_CHARCOLORSHIFT) & V_CHARCOLORMASK;
				colormap = V_GetStringColormap(charflags);
			}
			continue;
		}
		if (*ch == '\n')
		{
			cx = x;

			if (option & V_RETURN8)
				cy += 8*dup;
			else
				cy += 12*dup;

			continue;
		}

		c = *ch;
		if (!lowercase)
			c = toupper(c);
		c -= HU_FONTSTART;

		// character does not exist or is a space
		if (c < 0 || c >= HU_FONTSIZE || !hu_font[c])
		{
			cx += spacewidth * dup;
			continue;
		}

		if (charwidth)
		{
			w = charwidth * dup;
			center = w/2 - hu_font[c]->width*dup/2;
		}
		else
			w = hu_font[c]->width * dup;

		if (cx > scrwidth)
			break;
		if (cx+left + w < 0) //left boundary check
		{
			cx += w;
			continue;
		}

		V_DrawFixedPatch((cx + center)<<FRACBITS, cy<<FRACBITS, FRACUNIT, option, hu_font[c], colormap);

		cx += w;
	}
}

// SRB2kart
void V_DrawKartString(INT32 x, INT32 y, INT32 option, const char *string)
{
	INT32 w, c, cx = x, cy = y, dup, scrwidth, center = 0, left = 0;
	const char *ch = string;
	INT32 charflags = 0;
	const UINT8 *colormap = NULL;
	INT32 spacewidth = 12, charwidth = 0;

	INT32 lowercase = (option & V_ALLOWLOWERCASE);
	option &= ~V_FLIP; // which is also shared with V_ALLOWLOWERCASE...

	if (option & V_NOSCALESTART)
	{
		dup = vid.dup;
		scrwidth = vid.width;
	}
	else
	{
		dup = 1;
		scrwidth = vid.scaledwidth;
		left = (scrwidth - BASEVIDWIDTH)/2;
	}

	charflags = (option & V_CHARCOLORMASK);
	colormap = V_GetStringColormap(charflags);

	switch (option & V_SPACINGMASK)
	{
		case V_MONOSPACE:
			spacewidth = 12;
			/* FALLTHRU */
		case V_OLDSPACING:
			charwidth = 12;
			break;
		case V_6WIDTHSPACE:
			spacewidth = 6;
		default:
			break;
	}

	for (;;ch++)
	{
		if (!*ch)
			break;
		if (*ch & 0x80) //color parsing -x 2.16.09
		{
			// manually set flags override color codes
			if (!(option & V_CHARCOLORMASK))
			{
				charflags = ((*ch & 0x7f) << V_CHARCOLORSHIFT) & V_CHARCOLORMASK;
				colormap = V_GetStringColormap(charflags);
			}
			continue;
		}
		if (*ch == '\n')
		{
			cx = x;

			if (option & V_RETURN8)
				cy += 8*dup;
			else
				cy += 12*dup;

			continue;
		}

		c = *ch;
		if (!lowercase)
			c = toupper(c);
		c -= KART_FONTSTART;

		// character does not exist or is a space
		if (c < 0 || c >= KART_FONTSIZE || !kart_font[c])
		{
			cx += spacewidth * dup;
			continue;
		}

		if (charwidth)
		{
			w = charwidth * dup;
			center = w/2 - kart_font[c]->width*dup/2;
		}
		else
			w = kart_font[c]->width * dup;

		if (cx > scrwidth)
			break;
		if (cx+left + w < 0) //left boundary check
		{
			cx += w;
			continue;
		}

		V_DrawFixedPatch((cx + center)<<FRACBITS, cy<<FRACBITS, FRACUNIT, option, kart_font[c], colormap);

		cx += w;
	}
}
//

void V_DrawCenteredString(INT32 x, INT32 y, INT32 option, const char *string)
{
	x -= V_StringWidth(string, option)/2;
	V_DrawString(x, y, option, string);
}

void V_DrawRightAlignedString(INT32 x, INT32 y, INT32 option, const char *string)
{
	x -= V_StringWidth(string, option);
	V_DrawString(x, y, option, string);
}

//
// Write a string using the hu_font, 0.5x scale
// NOTE: the text is centered for screens larger than the base width
//
void V_DrawSmallString(INT32 x, INT32 y, INT32 option, const char *string)
{
	INT32 w, c, cx = x, cy = y, dup, scrwidth, center = 0, left = 0;
	const char *ch = string;
	INT32 charflags = 0;
	const UINT8 *colormap = NULL;
	INT32 spacewidth = 2, charwidth = 0;

	INT32 lowercase = (option & V_ALLOWLOWERCASE);
	option &= ~V_FLIP; // which is also shared with V_ALLOWLOWERCASE...

	if (option & V_NOSCALESTART)
	{
		dup = vid.dup;
		scrwidth = vid.width;
	}
	else
	{
		dup = 1;
		scrwidth = vid.scaledwidth;
		left = (scrwidth - BASEVIDWIDTH)/2;
		scrwidth -= left;
	}

	charflags = (option & V_CHARCOLORMASK);
	colormap = V_GetStringColormap(charflags);

	switch (option & V_SPACINGMASK)
	{
		case V_MONOSPACE:
			spacewidth = 4;
			/* FALLTHRU */
		case V_OLDSPACING:
			charwidth = 4;
			break;
		case V_6WIDTHSPACE:
			spacewidth = 3;
		default:
			break;
	}

	for (;;ch++)
	{
		if (!*ch)
			break;
		if (*ch & 0x80) //color parsing -x 2.16.09
		{
			// manually set flags override color codes
			if (!(option & V_CHARCOLORMASK))
			{
				charflags = ((*ch & 0x7f) << V_CHARCOLORSHIFT) & V_CHARCOLORMASK;
				colormap = V_GetStringColormap(charflags);
			}
			continue;
		}
		if (*ch == '\n')
		{
			cx = x;

			if (option & V_RETURN8)
				cy += 4*dup;
			else
				cy += 6*dup;

			continue;
		}

		c = *ch;
		if (!lowercase)
			c = toupper(c);
		c -= HU_FONTSTART;

		if (c < 0 || c >= HU_FONTSIZE || !hu_font[c])
		{
			cx += spacewidth * dup;
			continue;
		}

		if (charwidth)
		{
			w = charwidth * dup;
			center = w/2 - hu_font[c]->width*dup/4;
		}
		else
			w = hu_font[c]->width * dup / 2;

		if (cx > scrwidth)
			break;

		if (cx+left + w < 0) //left boundary check
		{
			cx += w;
			continue;
		}

		V_DrawFixedPatch((cx + center)<<FRACBITS, cy<<FRACBITS, FRACUNIT/2, option, hu_font[c], colormap);

		cx += w;
	}
}

void V_DrawCenteredSmallString(INT32 x, INT32 y, INT32 option, const char *string)
{
	x -= V_SmallStringWidth(string, option)/2;
	V_DrawSmallString(x, y, option, string);
}

void V_DrawRightAlignedSmallString(INT32 x, INT32 y, INT32 option, const char *string)
{
	x -= V_SmallStringWidth(string, option);
	V_DrawSmallString(x, y, option, string);
}

//
// Write a string using the tny_font
// NOTE: the text is centered for screens larger than the base width
//
void V_DrawThinString(INT32 x, INT32 y, INT32 option, const char *string)
{
	INT32 w, c, cx = x, cy = y, dup, scrwidth, left = 0;
	const char *ch = string;
	INT32 charflags = 0;
	const UINT8 *colormap = NULL;
	INT32 spacewidth = 2, charwidth = 0;

	INT32 lowercase = (option & V_ALLOWLOWERCASE);
	option &= ~V_FLIP; // which is also shared with V_ALLOWLOWERCASE...

	if (option & V_NOSCALESTART)
	{
		dup = vid.dup;
		scrwidth = vid.width;
	}
	else
	{
		dup = 1;
		scrwidth = vid.scaledwidth;
		left = (scrwidth - BASEVIDWIDTH)/2;
		scrwidth -= left;
	}

	charflags = (option & V_CHARCOLORMASK);
	colormap = V_GetStringColormap(charflags);

	switch (option & V_SPACINGMASK)
	{
		case V_MONOSPACE:
			spacewidth = 5;
			/* FALLTHRU */
		case V_OLDSPACING:
			charwidth = 5;
			break;
		// Out of video flags, so we're reusing this for alternate charwidth instead
		/*case V_6WIDTHSPACE:
			spacewidth = 3;*/
		default:
			break;
	}

	for (;;ch++)
	{
		if (!*ch)
			break;
		if (*ch & 0x80) //color parsing -x 2.16.09
		{
			// manually set flags override color codes
			if (!(option & V_CHARCOLORMASK))
			{
				charflags = ((*ch & 0x7f) << V_CHARCOLORSHIFT) & V_CHARCOLORMASK;
				colormap = V_GetStringColormap(charflags);
			}
			continue;
		}
		if (*ch == '\n')
		{
			cx = x;

			if (option & V_RETURN8)
				cy += 8*dup;
			else
				cy += 12*dup;

			continue;
		}

		c = *ch;
		if (!lowercase || !tny_font[c-HU_FONTSTART])
			c = toupper(c);
		c -= HU_FONTSTART;

		if (c < 0 || c >= HU_FONTSIZE || !tny_font[c])
		{
			cx += spacewidth * dup;
			continue;
		}

		if (charwidth)
			w = charwidth * dup;
		else
			w = ((option & V_6WIDTHSPACE ? max(1, tny_font[c]->width-1) // Reuse this flag for the alternate bunched-up spacing
				: tny_font[c]->width) * dup);

		if (cx > scrwidth)
			break;

		if (cx+left + w < 0) //left boundary check
		{
			cx += w;
			continue;
		}

		V_DrawFixedPatch(cx<<FRACBITS, cy<<FRACBITS, FRACUNIT, option, tny_font[c], colormap);

		cx += w;
	}
}

void V_DrawCenteredThinString(INT32 x, INT32 y, INT32 option, const char *string)
{
	x -= V_ThinStringWidth(string, option)/2;
	V_DrawThinString(x, y, option, string);
}

void V_DrawRightAlignedThinString(INT32 x, INT32 y, INT32 option, const char *string)
{
	x -= V_ThinStringWidth(string, option);
	V_DrawThinString(x, y, option, string);
}

// Draws a string at a fixed_t location.
void V_DrawStringAtFixed(fixed_t x, fixed_t y, INT32 option, const char *string)
{
	fixed_t cx = x, cy = y;
	INT32 w, c, dup, scrwidth, center = 0, left = 0;
	const char *ch = string;
	INT32 spacewidth = 4, charwidth = 0;

	INT32 lowercase = (option & V_ALLOWLOWERCASE);
	option &= ~V_FLIP; // which is also shared with V_ALLOWLOWERCASE...

	if (option & V_NOSCALESTART)
	{
		dup = vid.dup;
		scrwidth = vid.width;
	}
	else
	{
		dup = 1;
		scrwidth = vid.scaledwidth;
		left = (scrwidth - BASEVIDWIDTH)/2;
		scrwidth -= left;
	}

	switch (option & V_SPACINGMASK)
	{
		case V_MONOSPACE:
			spacewidth = 8;
			/* FALLTHRU */
		case V_OLDSPACING:
			charwidth = 8;
			break;
		case V_6WIDTHSPACE:
			spacewidth = 6;
		default:
			break;
	}

	for (;;ch++)
	{
		if (!*ch)
			break;

		if (*ch & 0x80) //color ignoring
			continue;

		if (*ch == '\n')
		{
			cx = x;

			if (option & V_RETURN8)
				cy += (8*dup)<<FRACBITS;
			else
				cy += (12*dup)<<FRACBITS;

			continue;
		}

		c = *ch;
		if (!lowercase)
			c = toupper(c);
		c -= HU_FONTSTART;

		// character does not exist or is a space
		if (c < 0 || c >= HU_FONTSIZE || !hu_font[c])
		{
			cx += (spacewidth * dup)<<FRACBITS;
			continue;
		}

		if (charwidth)
		{
			w = charwidth * dup;
			center = w/2 - hu_font[c]->width*(dup/2);
		}
		else
			w = hu_font[c]->width * dup;

		if ((cx>>FRACBITS) > scrwidth)
			break;

		if ((cx>>FRACBITS)+left + w < 0) //left boundary check
		{
			cx += w<<FRACBITS;
			continue;
		}

		V_DrawSciencePatch(cx + (center<<FRACBITS), cy, option, hu_font[c], FRACUNIT);

		cx += w<<FRACBITS;
	}
}

// Jaden: awesome!
void V_DrawSmallStringAtFixed(fixed_t x, fixed_t y, INT32 option, const char *string)
{
	fixed_t cx = x, cy = y;
	INT32 w, c, dup, scrwidth, center = 0, left = 0;
	const char *ch = string;
	INT32 charflags = 0;
	const UINT8 *colormap = NULL;
	INT32 spacewidth = 2, charwidth = 0;
	INT32 lowercase = (option & V_ALLOWLOWERCASE);
	option &= ~V_FLIP; // which is also shared with V_ALLOWLOWERCASE...

	if (option & V_NOSCALESTART)
	{
		dup = vid.dup;
		scrwidth = vid.width;
	}
	else
	{
		dup = 1;
		scrwidth = vid.scaledwidth;
		left = (scrwidth - BASEVIDWIDTH)/2;
		scrwidth -= left;
	}

	if (option & V_NOSCALEPATCH)
		scrwidth *= vid.dup;

	charflags = (option & V_CHARCOLORMASK);

	switch (option & V_SPACINGMASK)
	{
		case V_MONOSPACE:
			spacewidth = 4;
			/* FALLTHRU */
		case V_OLDSPACING:
			charwidth = 4;
			break;
		case V_6WIDTHSPACE:
			spacewidth = 3;
		default:
			break;
	}

	for (;;ch++)
	{
		if (!*ch)
			break;

		if (*ch & 0x80) //color parsing -x 2.16.09
		{
			// manually set flags override color codes
			if (!(option & V_CHARCOLORMASK))
				charflags = ((*ch & 0x7f) << V_CHARCOLORSHIFT) & V_CHARCOLORMASK;
			continue;
		}

		if (*ch == '\n')
		{
			cx = x;
			if (option & V_RETURN8)
				cy += (4*dup)<<FRACBITS;
			else
				cy += (6*dup)<<FRACBITS;
			continue;
		}

		c = *ch;

		if (!lowercase)
			c = toupper(c);
		c -= HU_FONTSTART;

		// character does not exist or is a space
		if (c < 0 || c >= HU_FONTSIZE || !hu_font[c])
		{
			cx += (spacewidth * dup)<<FRACBITS;
			continue;
		}

		if (charwidth)
		{
			w = charwidth * dup;
			center = w/2 - hu_font[c]->width*(dup/4);
		}
		else
			w = hu_font[c]->width * dup / 2;

		if ((cx>>FRACBITS) > scrwidth)
			break;

		if ((cx>>FRACBITS)+left + w < 0) //left boundary check
		{
			cx += w<<FRACBITS;
			continue;
		}

		colormap = V_GetStringColormap(charflags);
		V_DrawFixedPatch(cx + (center<<FRACBITS), cy, FRACUNIT/2, option, hu_font[c], colormap);
		cx += w<<FRACBITS;
	}
}

void V_DrawCenteredSmallStringAtFixed(fixed_t x, fixed_t y, INT32 option, const char *string)
{
	x -= (V_SmallStringWidth(string, option) / 2)<<FRACBITS;
	V_DrawSmallStringAtFixed(x, y, option, string);
}

void V_DrawThinStringAtFixed(fixed_t x, fixed_t y, INT32 option, const char *string)
{
	fixed_t cx = x, cy = y;
	INT32 w, c, dup, scrwidth, center = 0, left = 0;
	const char *ch = string;
	INT32 spacewidth = 4, charwidth = 0;
	INT32 charflags = 0;
	const UINT8 *colormap = NULL;
	INT32 lowercase = (option & V_ALLOWLOWERCASE);
	option &= ~V_FLIP; // which is also shared with V_ALLOWLOWERCASE...

	if (option & V_NOSCALESTART)
	{
		dup = vid.dup;
		scrwidth = vid.width;
	}
	else
	{
		dup = 1;
		scrwidth = vid.scaledwidth;
		left = (scrwidth - BASEVIDWIDTH)/2;
		scrwidth -= left;
	}

	switch (option & V_SPACINGMASK)
	{
		case V_MONOSPACE:
			spacewidth = 8;
			/* FALLTHRU */
		case V_OLDSPACING:
			charwidth = 8;
			break;
		case V_6WIDTHSPACE:
			spacewidth = 6;
		default:
			break;
	}

	charflags = (option & V_CHARCOLORMASK);
	colormap = V_GetStringColormap(charflags);

	for (;;ch++)
	{
		if (!*ch)
			break;

		if (*ch & 0x80) //color parsing -x 2.16.09
		{
			// manually set flags override color codes
			if (!(option & V_CHARCOLORMASK))
			{
				charflags = ((*ch & 0x7f) << V_CHARCOLORSHIFT) & V_CHARCOLORMASK;
				colormap = V_GetStringColormap(charflags);
			}
			continue;
		}

		if (*ch == '\n')
		{
			cx = x;

			if (option & V_RETURN8)
				cy += (8*dup)<<FRACBITS;
			else
				cy += (12*dup)<<FRACBITS;

			continue;
		}

		c = *ch;
		if (!lowercase)
			c = toupper(c);
		c -= HU_FONTSTART;

		// character does not exist or is a space
		if (c < 0 || c >= HU_FONTSIZE || !tny_font[c])
		{
			cx += (spacewidth * dup)<<FRACBITS;
			continue;
		}

		if (charwidth)
		{
			w = charwidth * dup;
			center = w/2 - tny_font[c]->width*(dup/2);
		}
		else
			w = tny_font[c]->width * dup;

		if ((cx>>FRACBITS) > scrwidth)
			break;

		if ((cx>>FRACBITS)+left + w < 0) //left boundary check
		{
			cx += w<<FRACBITS;
			continue;
		}

		//V_DrawSciencePatch(cx + (center<<FRACBITS), cy, option, tny_font[c], FRACUNIT);
		V_DrawFixedPatch(cx + (center<<FRACBITS), cy, FRACUNIT, option, tny_font[c], colormap);

		cx += w<<FRACBITS;
	}
}

// Draws a tallnum.  Replaces two functions in y_inter and st_stuff
void V_DrawTallNum(INT32 x, INT32 y, INT32 flags, INT32 num)
{
	INT32 w = tallnum[0]->width;
	boolean neg;

	if (flags & V_NOSCALESTART)
		w *= vid.dup;

	if ((neg = num < 0))
		num = -num;

	// draw the number
	do
	{
		x -= w;
		V_DrawScaledPatch(x, y, flags, tallnum[num % 10]);
		num /= 10;
	} while (num);

	// draw a minus sign if necessary
	if (neg)
		V_DrawScaledPatch(x - w, y, flags, tallminus); // Tails
}

// Draws a number with a set number of digits.
// Does not handle negative numbers in a special way, don't try to feed it any.
void V_DrawPaddedTallNum(INT32 x, INT32 y, INT32 flags, INT32 num, INT32 digits)
{
	INT32 w = tallnum[0]->width;

	if (flags & V_NOSCALESTART)
		w *= vid.dup;

	if (num < 0)
		num = -num;

	// draw the number
	do
	{
		x -= w;
		V_DrawScaledPatch(x, y, flags, tallnum[num % 10]);
		num /= 10;
	} while (--digits);
}

// Draws a number with a set number of digits.
// Does not handle negative numbers in a special way, don't try to feed it any.
void V_DrawPaddedTallColorNum(INT32 x, INT32 y, INT32 flags, INT32 num, INT32 digits, const UINT8 *colormap)
{
	INT32 w = tallnum[0]->width;

	if (flags & V_NOSCALESTART)
		w *= vid.dup;

	if (num < 0)
		num = -num;

	// draw the number
	do
	{
		x -= w;
		V_DrawFixedPatch(x<<FRACBITS, y<<FRACBITS, FRACUNIT, flags, tallnum[num % 10], colormap);
		num /= 10;
	} while (--digits);
}

// Draws a number using the PING font thingy.
// TODO: Merge number drawing functions into one with "font name" selection.

INT32 V_DrawPingNum(INT32 x, INT32 y, INT32 flags, INT32 num, const UINT8 *colormap)
{
	INT32 w = pingnum[0]->width;	// this SHOULD always be 5 but I guess custom graphics exist.

	if (flags & V_NOSCALESTART)
		w *= vid.dup;

	if (num < 0)
		num = -num;

	// draw the number
	do
	{
		x -= (w-1);	// Oni wanted their outline to intersect.
		V_DrawFixedPatch(x<<FRACBITS, y<<FRACBITS, FRACUNIT, flags, pingnum[num%10], colormap);
		num /= 10;
	} while (num);

	return x;
}

// Jaden: Draw a number using the position numbers.
//
void V_DrawRankNum(INT32 x, INT32 y, INT32 flags, INT32 num, INT32 digits, const UINT8 *colormap)
{
	INT32 w = ranknum[0]->width - 1;

	if (flags & V_NOSCALESTART)
		w *= vid.dup;

	if (num < 0)
		num = -num;

	// draw the number
	do
	{
		x -= (w - 1);

		V_DrawFixedPatch(x << FRACBITS, y << FRACBITS, FRACUNIT, flags, ranknum[num % 10], colormap);
		num /= 10;
	} while (--digits);
}

// Write a string using the credit font
// NOTE: the text is centered for screens larger than the base width
//
void V_DrawCreditString(fixed_t x, fixed_t y, INT32 option, const char *string)
{
	INT32 w, c, dup, scrwidth = BASEVIDWIDTH;
	fixed_t cx = x, cy = y;
	const char *ch = string;

	// It's possible for string to be a null pointer
	if (!string)
		return;

	if (option & V_NOSCALESTART)
	{
		dup = vid.dup;
		scrwidth = vid.width;
	}
	else
		dup = 1;

	for (;;)
	{
		c = *ch++;

		if (!c)
			break;

		if (c == '\n')
		{
			cx = x;
			cy += (12*dup)<<FRACBITS;
			continue;
		}

		c = toupper(c) - CRED_FONTSTART;

		if (c < 0 || c >= CRED_FONTSIZE)
		{
			cx += (16*dup)<<FRACBITS;
			continue;
		}

		w = cred_font[c]->width * dup;

		if ((cx>>FRACBITS) > scrwidth)
			break;

		V_DrawSciencePatch(cx, cy, option, cred_font[c], FRACUNIT);
		cx += w<<FRACBITS;
	}
}

// Find string width from cred_font chars
//
INT32 V_CreditStringWidth(const char *string)
{
	INT32 c, w = 0;
	size_t i;

	// It's possible for string to be a null pointer
	if (!string)
		return 0;

	const size_t strlength = strlen(string);

	for (i = 0; i < strlength; i++)
	{
		c = toupper(string[i]) - CRED_FONTSTART;
		if (c < 0 || c >= CRED_FONTSIZE)
			w += 16;
		else
			w += cred_font[c]->width;
	}

	return w;
}

// Write a string using the level title font
// NOTE: the text is centered for screens larger than the base width
//
void V_DrawLevelTitle(INT32 x, INT32 y, INT32 option, const char *string)
{
	INT32 w, c, cx = x, cy = y, dup, scrwidth, left = 0;
	const char *ch = string;

	if (option & V_NOSCALESTART)
	{
		dup = vid.dup;
		scrwidth = vid.width;
	}
	else
	{
		dup = 1;
		scrwidth = vid.scaledwidth;
		left = (scrwidth - BASEVIDWIDTH)/2;
		scrwidth -= left;
	}

	for (;;)
	{
		c = *ch++;

		if (!c)
			break;

		if (c == '\n')
		{
			cx = x;
			cy += 12*dup;
			continue;
		}

		c = toupper(c) - LT_FONTSTART;

		if (c < 0 || c >= LT_FONTSIZE || !lt_font[c])
		{
			cx += 12*dup;
			continue;
		}

		w = lt_font[c]->width * dup;

		if (cx > scrwidth)
			break;

		if (cx+left + w < 0) //left boundary check
		{
			cx += w;
			continue;
		}

		V_DrawScaledPatch(cx, cy, option, lt_font[c]);
		cx += w;
	}
}

// Find string width from lt_font chars
//
INT32 V_LevelNameWidth(const char *string)
{
	INT32 c, w = 0;
	size_t i;

	const size_t strlength = strlen(string);

	for (i = 0; i < strlength; i++)
	{
		c = toupper(string[i]) - LT_FONTSTART;
		if (c < 0 || c >= LT_FONTSIZE || !lt_font[c])
			w += 12;
		else
			w += lt_font[c]->width;
	}

	return w;
}

// Find max height of the string
//
INT32 V_LevelNameHeight(const char *string)
{
	INT32 c, w = 0;
	size_t i;

	const size_t strlength = strlen(string);

	for (i = 0; i < strlength; i++)
	{
		c = toupper(string[i]) - LT_FONTSTART;
		if (c < 0 || c >= LT_FONTSIZE || !lt_font[c])
			continue;

		if (lt_font[c]->height > w)
			w = lt_font[c]->height;
	}

	return w;
}

//
// Find string width from hu_font chars
//
INT32 V_SubStringWidth(const char *string, INT32 length, INT32 option)
{
	INT32 c, w = 0;
	INT32 spacewidth = 4, charwidth = 0;
	ssize_t i;

	if (!string)
		return 0;

	if (length < 0)
		length = strlen(string);

	switch (option & V_SPACINGMASK)
	{
		case V_MONOSPACE:
			spacewidth = 8;
			/* FALLTHRU */
		case V_OLDSPACING:
			charwidth = 8;
			break;
		case V_6WIDTHSPACE:
			spacewidth = 6;
		default:
			break;
	}

	for (i = 0; string[i] && i < length; i++)
	{
		c = string[i];
		if ((UINT8)c >= 0x80 && (UINT8)c <= 0x8F) //color parsing! -Inuyasha 2.16.09
			continue;

		c = toupper(c) - HU_FONTSTART;
		if (c < 0 || c >= HU_FONTSIZE || !hu_font[c])
			w += spacewidth;
		else
			w += (charwidth ? charwidth : hu_font[c]->width);
	}

	return w;
}

//
// Find string width from hu_font chars, 0.5x scale
//
INT32 V_SmallSubStringWidth(const char *string, INT32 length, INT32 option)
{
	INT32 c, w = 0;
	INT32 spacewidth = 2, charwidth = 0;
	ssize_t i;

	if (!string)
		return 0;

	if (length < 0)
		length = strlen(string);

	switch (option & V_SPACINGMASK)
	{
		case V_MONOSPACE:
			spacewidth = 4;
			/* FALLTHRU */
		case V_OLDSPACING:
			charwidth = 4;
			break;
		case V_6WIDTHSPACE:
			spacewidth = 3;
		default:
			break;
	}

	for (i = 0; string[i] && i < length; i++)
	{
		c = string[i];
		if ((UINT8)c >= 0x80 && (UINT8)c <= 0x8F) //color parsing! -Inuyasha 2.16.09
			continue;

		c = toupper(c) - HU_FONTSTART;
		if (c < 0 || c >= HU_FONTSIZE || !hu_font[c])
			w += spacewidth;
		else
			w += (charwidth ? charwidth : hu_font[c]->width/2);
	}

	return w;
}

//
// Find string width from tny_font chars
//
INT32 V_ThinSubStringWidth(const char *string, INT32 length, INT32 option)
{
	INT32 c, w = 0;
	INT32 spacewidth = 2, charwidth = 0;
	boolean lowercase;
	ssize_t i;

	if (!string)
		return 0;

	lowercase = (option & V_ALLOWLOWERCASE);

	if (length < 0)
		length = strlen(string);

	switch (option & V_SPACINGMASK)
	{
		case V_MONOSPACE:
			spacewidth = 5;
			/* FALLTHRU */
		case V_OLDSPACING:
			charwidth = 5;
			break;
		// Out of video flags, so we're reusing this for alternate charwidth instead
		/*case V_6WIDTHSPACE:
			spacewidth = 3;*/
		default:
			break;
	}

	for (i = 0; string[i] && i < length; i++)
	{
		c = string[i];
		if ((UINT8)c >= 0x80 && (UINT8)c <= 0x8F) //color parsing! -Inuyasha 2.16.09
			continue;

		if (c < HU_FONTSTART)
			continue;

		if (!lowercase || !tny_font[c-HU_FONTSTART])
			c = toupper(c);
		c -= HU_FONTSTART;

		if (c < 0 || c >= HU_FONTSIZE || !tny_font[c])
			w += spacewidth;
		else
		{
			w += (charwidth ? charwidth
				: ((option & V_6WIDTHSPACE && i < length-1) ? max(1, tny_font[c]->width-1) // Reuse this flag for the alternate bunched-up spacing
				: tny_font[c]->width));
		}
	}


	return w;
}

//
// Find maximum length for substring taken from current string to fit into given width
//
INT32 V_SubStringLengthToFit(const char *string, INT32 width, INT32 option)
{
	INT32 c, w = 0;
	INT32 spacewidth = 4, charwidth = 0;
	INT32 i;

	if (!string)
		return 0;

	switch (option & V_SPACINGMASK)
	{
		case V_MONOSPACE:
			spacewidth = 8;
			/* FALLTHRU */
		case V_OLDSPACING:
			charwidth = 8;
			break;
		case V_6WIDTHSPACE:
			spacewidth = 6;
		default:
			break;
	}

	for (i = 0; string[i] && w < width; i++)
	{
		c = string[i];
		if ((UINT8)c >= 0x80 && (UINT8)c <= 0x8F) //color parsing! -Inuyasha 2.16.09
			continue;

		c = toupper(c) - HU_FONTSTART;
		if (c < 0 || c >= HU_FONTSIZE || !hu_font[c])
			w += spacewidth;
		else
			w += (charwidth ? charwidth : hu_font[c]->width);
	}

	return max(i-1, 0);
}

char V_GetSkincolorChar(INT32 color)
{
	char cstart = 0x80;

	switch (color)
	{
		case SKINCOLOR_WHITE:
		case SKINCOLOR_SILVER:
		case SKINCOLOR_SLATE:
			cstart = 0x80; // White
			break;

		case SKINCOLOR_GREY:
		case SKINCOLOR_NICKEL:
		case SKINCOLOR_BLACK:
		case SKINCOLOR_SKUNK:
		case SKINCOLOR_JET:
			cstart = 0x86; // V_GRAYMAP
			break;

		case SKINCOLOR_SEPIA:
		case SKINCOLOR_BEIGE:
		case SKINCOLOR_WALNUT:
		case SKINCOLOR_BROWN:
		case SKINCOLOR_LEATHER:
		case SKINCOLOR_RUST:
		case SKINCOLOR_WRISTWATCH:
			cstart = 0x8e; // V_BROWNMAP
			break;

		case SKINCOLOR_FAIRY:
		case SKINCOLOR_SALMON:
		case SKINCOLOR_PINK:
		case SKINCOLOR_ROSE:
		case SKINCOLOR_BRICK:
		case SKINCOLOR_LEMONADE:
		case SKINCOLOR_BUBBLEGUM:
		case SKINCOLOR_LILAC:
			cstart = 0x8d; // V_PINKMAP
			break;

		case SKINCOLOR_CINNAMON:
		case SKINCOLOR_RUBY:
		case SKINCOLOR_RASPBERRY:
		case SKINCOLOR_CHERRY:
		case SKINCOLOR_RED:
		case SKINCOLOR_CRIMSON:
		case SKINCOLOR_MAROON:
		case SKINCOLOR_FLAME:
		case SKINCOLOR_SCARLET:
		case SKINCOLOR_KETCHUP:
			cstart = 0x85; // V_REDMAP
			break;

		case SKINCOLOR_DAWN:
		case SKINCOLOR_SUNSET:
		case SKINCOLOR_CREAMSICLE:
		case SKINCOLOR_ORANGE:
		case SKINCOLOR_PUMPKIN:
		case SKINCOLOR_ROSEWOOD:
		case SKINCOLOR_BURGUNDY:
		case SKINCOLOR_TANGERINE:
			cstart = 0x87; // V_ORANGEMAP
			break;

		case SKINCOLOR_PEACH:
		case SKINCOLOR_CARAMEL:
		case SKINCOLOR_CREAM:
			cstart = 0x8f; // V_PEACHMAP
			break;

		case SKINCOLOR_GOLD:
		case SKINCOLOR_ROYAL:
		case SKINCOLOR_BRONZE:
		case SKINCOLOR_COPPER:
		case SKINCOLOR_THUNDER:
			cstart = 0x8a; // V_GOLDMAP
			break;

		case SKINCOLOR_POPCORN:
		case SKINCOLOR_QUARRY:
		case SKINCOLOR_YELLOW:
		case SKINCOLOR_MUSTARD:
		case SKINCOLOR_CROCODILE:
		case SKINCOLOR_OLIVE:
			cstart = 0x82; // V_YELLOWMAP
			break;

		case SKINCOLOR_ARTICHOKE:
		case SKINCOLOR_VOMIT:
		case SKINCOLOR_GARDEN:
		case SKINCOLOR_TEA:
		case SKINCOLOR_PISTACHIO:
			cstart = 0x8b; // V_TEAMAP
			break;

		case SKINCOLOR_LIME:
		case SKINCOLOR_HANDHELD:
		case SKINCOLOR_MOSS:
		case SKINCOLOR_CAMOUFLAGE:
		case SKINCOLOR_ROBOHOOD:
		case SKINCOLOR_MINT:
		case SKINCOLOR_GREEN:
		case SKINCOLOR_PINETREE:
		case SKINCOLOR_EMERALD:
		case SKINCOLOR_SWAMP:
		case SKINCOLOR_DREAM:
		case SKINCOLOR_PLAGUE:
		case SKINCOLOR_ALGAE:
			cstart = 0x83; // V_GREENMAP
			break;

		case SKINCOLOR_CARIBBEAN:
		case SKINCOLOR_AZURE:
		case SKINCOLOR_AQUA:
		case SKINCOLOR_TEAL:
		case SKINCOLOR_CYAN:
		case SKINCOLOR_JAWZ:
		case SKINCOLOR_CERULEAN:
		case SKINCOLOR_NAVY:
		case SKINCOLOR_SAPPHIRE:
			cstart = 0x88; // V_SKYMAP
			break;

		case SKINCOLOR_PIGEON:
		case SKINCOLOR_PLATINUM:
		case SKINCOLOR_STEEL:
			cstart = 0x8c; // V_STEELMAP
			break;

		case SKINCOLOR_PERIWINKLE:
		case SKINCOLOR_BLUE:
		case SKINCOLOR_BLUEBERRY:
		case SKINCOLOR_NOVA:
			cstart = 0x84; // V_BLUEMAP
			break;

		case SKINCOLOR_ULTRAVIOLET:
		case SKINCOLOR_PURPLE:
		case SKINCOLOR_FUCHSIA:
			cstart = 0x81; // V_PURPLEMAP
			break;

		case SKINCOLOR_PASTEL:
		case SKINCOLOR_MOONSLAM:
		case SKINCOLOR_DUSK:
		case SKINCOLOR_TOXIC:
		case SKINCOLOR_MAUVE:
		case SKINCOLOR_LAVENDER:
		case SKINCOLOR_BYZANTIUM:
		case SKINCOLOR_POMEGRANATE:
			cstart = 0x89; // V_LAVENDERMAP
			break;

		default:
			break;
	}

	return cstart;
}

INT32 V_SkinColorToHighlightcolor(skincolors_t color)
{
	switch (color)
	{
		case SKINCOLOR_WHITE:
		case SKINCOLOR_SILVER:
		case SKINCOLOR_SLATE:
			return V_STEELMAP;
		case SKINCOLOR_GREY:
		case SKINCOLOR_NICKEL:
		case SKINCOLOR_BLACK:
		case SKINCOLOR_SKUNK:
		case SKINCOLOR_JET:
			return V_GRAYMAP;
		case SKINCOLOR_SEPIA:
		case SKINCOLOR_BEIGE:
		case SKINCOLOR_WALNUT:
		case SKINCOLOR_BROWN:
		case SKINCOLOR_LEATHER:
		case SKINCOLOR_RUST:
		case SKINCOLOR_WRISTWATCH:
			return V_BROWNMAP;
		case SKINCOLOR_FAIRY:
		case SKINCOLOR_SALMON:
		case SKINCOLOR_PINK:
		case SKINCOLOR_ROSE:
		case SKINCOLOR_BRICK:
		case SKINCOLOR_LEMONADE:
		case SKINCOLOR_BUBBLEGUM:
		case SKINCOLOR_LILAC:
			return V_PINKMAP;
		case SKINCOLOR_CINNAMON:
		case SKINCOLOR_RUBY:
		case SKINCOLOR_RASPBERRY:
		case SKINCOLOR_CHERRY:
		case SKINCOLOR_RED:
		case SKINCOLOR_CRIMSON:
		case SKINCOLOR_MAROON:
		case SKINCOLOR_FLAME:
		case SKINCOLOR_SCARLET:
		case SKINCOLOR_KETCHUP:
			return V_REDMAP;
		case SKINCOLOR_DAWN:
		case SKINCOLOR_SUNSET:
		case SKINCOLOR_CREAMSICLE:
		case SKINCOLOR_ORANGE:
		case SKINCOLOR_PUMPKIN:
		case SKINCOLOR_ROSEWOOD:
		case SKINCOLOR_BURGUNDY:
		case SKINCOLOR_TANGERINE:
			return V_ORANGEMAP;
		case SKINCOLOR_PEACH:
		case SKINCOLOR_CARAMEL:
		case SKINCOLOR_CREAM:
			return V_PEACHMAP;
		case SKINCOLOR_GOLD:
		case SKINCOLOR_ROYAL:
		case SKINCOLOR_BRONZE:
		case SKINCOLOR_COPPER:
		case SKINCOLOR_THUNDER:
			return V_GOLDMAP;
		case SKINCOLOR_POPCORN:
		case SKINCOLOR_QUARRY:
		case SKINCOLOR_YELLOW:
		case SKINCOLOR_MUSTARD:
		case SKINCOLOR_CROCODILE:
		case SKINCOLOR_OLIVE:
			return V_YELLOWMAP;
		case SKINCOLOR_ARTICHOKE:
		case SKINCOLOR_VOMIT:
		case SKINCOLOR_GARDEN:
		case SKINCOLOR_TEA:
		case SKINCOLOR_PISTACHIO:
			return V_TEAMAP;
		case SKINCOLOR_LIME:
		case SKINCOLOR_HANDHELD:
		case SKINCOLOR_MOSS:
		case SKINCOLOR_CAMOUFLAGE:
		case SKINCOLOR_ROBOHOOD:
		case SKINCOLOR_MINT:
		case SKINCOLOR_GREEN:
		case SKINCOLOR_PINETREE:
		case SKINCOLOR_EMERALD:
		case SKINCOLOR_SWAMP:
		case SKINCOLOR_DREAM:
		case SKINCOLOR_PLAGUE:
		case SKINCOLOR_ALGAE:
			return V_GREENMAP;
		case SKINCOLOR_CARIBBEAN:
		case SKINCOLOR_AZURE:
		case SKINCOLOR_AQUA:
		case SKINCOLOR_TEAL:
		case SKINCOLOR_CYAN:
		case SKINCOLOR_JAWZ:
		case SKINCOLOR_CERULEAN:
		case SKINCOLOR_NAVY:
		case SKINCOLOR_SAPPHIRE:
			return V_SKYMAP;
		case SKINCOLOR_PIGEON:
		case SKINCOLOR_PLATINUM:
		case SKINCOLOR_STEEL:
			return V_STEELMAP;
		case SKINCOLOR_PERIWINKLE:
		case SKINCOLOR_BLUE:
		case SKINCOLOR_BLUEBERRY:
		case SKINCOLOR_NOVA:
			return V_BLUEMAP;
		case SKINCOLOR_ULTRAVIOLET:
		case SKINCOLOR_PURPLE:
		case SKINCOLOR_FUCHSIA:
			return V_PURPLEMAP;
		case SKINCOLOR_PASTEL:
		case SKINCOLOR_MOONSLAM:
		case SKINCOLOR_DUSK:
		case SKINCOLOR_TOXIC:
		case SKINCOLOR_MAUVE:
		case SKINCOLOR_LAVENDER:
		case SKINCOLOR_BYZANTIUM:
		case SKINCOLOR_POMEGRANATE:
			return V_LAVENDERMAP;

		default:
			return 0;
	}
}

boolean *heatshifter = NULL;
INT32 lastheight = 0;
INT32 heatindex[MAXSPLITSCREENPLAYERS] = {0, 0, 0, 0};

// unused motion blur effect, probably non functional
//#define MOTIONBLUR

//
// V_DoPostProcessor
//
// Perform a particular image postprocessing function.
//

void V_DoPostProcessor(INT32 view, INT32 param)
{
#if NUMSCREENS < 5
	// do not enable image post processing for ARM, SH and MIPS CPUs
	(void)view;
	(void)type;
	(void)param;
#else
#ifndef MOTIONBLUR
	(void)param; // unused motion blur stuff
#endif
	INT32 yoffset, xoffset;

#ifdef HWRENDER
	if (rendermode != render_soft)
		return;
#endif

	if (view < 0 || view > 3 || view > splitscreen)
		return;

	camera_t *thiscam = &camera[view];

	if (!thiscam->postimg)
		return;

	if ((view == 1 && splitscreen == 1) || view >= 2)
		yoffset = viewheight;
	else
		yoffset = 0;

	if ((view == 1 || view == 3) && splitscreen > 1)
		xoffset = viewwidth;
	else
		xoffset = 0;

	UINT8 *tmpscr = vid.screens[4];
	UINT8 *srcscr = vid.screens[0];

	if (!cv_reducevfx.value)
	{
		if (thiscam->postimg & POSTIMG_WATER)
		{
			INT32 y;
			// Set disStart to a range from 0 to FINEANGLE, incrementing by 128 per tic
			angle_t disStart = (((leveltime-1)*128) + (R_GetTimeFrac(RTF_LEVEL) / (FRACUNIT/128))) & FINEMASK;
			INT32 newpix;
			INT32 sine;
			//UINT8 *transme = transtables + ((tr_trans50-1)<<FF_TRANSSHIFT);

			for (y = yoffset; y < yoffset+viewheight; y++)
			{
				sine = (FINESINE(disStart)*5)>>FRACBITS;
				newpix = abs(sine);

				if (sine < 0)
				{
					memcpy(&tmpscr[(y*vid.width)+xoffset+newpix], &srcscr[(y*vid.width)+xoffset], viewwidth-newpix);

					// Cleanup edge
					while (newpix)
					{
						tmpscr[(y*vid.width)+xoffset+newpix] = srcscr[(y*vid.width)+xoffset];
						newpix--;
					}
				}
				else
				{
					memcpy(&tmpscr[(y*vid.width)+xoffset+0], &srcscr[(y*vid.width)+xoffset+sine], viewwidth-newpix);

					// Cleanup edge
					while (newpix)
					{
						tmpscr[(y*vid.width)+xoffset+viewwidth-newpix] = srcscr[(y*vid.width)+xoffset+(viewwidth-1)];
						newpix--;
					}
				}

				/*
				Unoptimized version
				for (x = 0; x < vid.width; x++)
				{
					newpix = (x + sine);

					if (newpix < 0)
						newpix = 0;
					else if (newpix >= vid.width)
						newpix = vid.width-1;

					tmpscr[y*vid.width + x] = srcscr[y*vid.width+newpix]; // *(transme + (srcscr[y*vid.width+x]<<8) + srcscr[y*vid.width+newpix]);
				}*/

				disStart += 22;//the offset into the displacement map, increment each game loop
				disStart &= FINEMASK; //clip it to FINEMASK
			}

			UINT8 *tmp = tmpscr;
			tmpscr = srcscr;
			srcscr = tmp;
		}
		else if (thiscam->postimg & POSTIMG_HEAT) // Heat wave
		{
			INT32 y;

			// Make sure table is built
			if (heatshifter == NULL || lastheight != viewheight)
			{
				Z_Free(heatshifter);
				heatshifter = Z_Calloc(viewheight * sizeof(boolean), PU_STATIC, NULL);

				for (y = 0; y < viewheight; y++)
				{
					if (M_RandomChance(FRACUNIT/8)) // 12.5%
						heatshifter[y] = true;
				}

				heatindex[0] = heatindex[1] = heatindex[2] = heatindex[3] = 0;
				lastheight = viewheight;
			}

			for (y = yoffset; y < yoffset+viewheight; y++)
			{
				if (heatshifter[heatindex[view]++])
				{
					// Shift this row of pixels to the right by 2
					tmpscr[(y*vid.width)+xoffset] = srcscr[(y*vid.width)+xoffset];
					memcpy(&tmpscr[(y*vid.width)+xoffset], &srcscr[(y*vid.width)+xoffset+vid.dup], viewwidth-vid.dup);
				}
				else
					memcpy(&tmpscr[(y*vid.width)+xoffset], &srcscr[(y*vid.width)+xoffset], viewwidth);

				heatindex[view] %= viewheight;
			}

			if (renderisnewtic) // This isn't interpolated... but how do you interpolate a one-pixel shift?
			{
				heatindex[view]++;
				heatindex[view] %= vid.height;
			}

			UINT8 *tmp = tmpscr;
			tmpscr = srcscr;
			srcscr = tmp;
		}

#ifdef MOTIONBLUR
		if (thiscam->postimg & POSTIMG_MOTION) // Motion Blur!
		{
			INT32 x, y;

			// TODO: Add a postimg_param so that we can pick the translucency level...
			UINT8 *transme = transtables + ((param-1)<<FF_TRANSSHIFT);

			for (y = yoffset; y < yoffset+viewheight; y++)
			{
				for (x = xoffset; x < xoffset+viewwidth; x++)
					tmpscr[y*vid.width + x] =     colormaps[*(transme     + (srcscr   [(y*vid.width)+x ] <<8) + (tmpscr[(y*vid.width)+x]))];
			}
		}
#endif
	}

	if ((thiscam->postimg & POSTIMG_FLIP) && !(thiscam->postimg & POSTIMG_MIRROR)) // Flip the screen upside-down
	{
		INT32 y, y2;

		for (y = yoffset, y2 = yoffset+viewheight - 1; y < yoffset+viewheight; y++, y2--)
			memcpy(&tmpscr[(y2*vid.width)+xoffset], &srcscr[(y*vid.width)+xoffset], viewwidth);

		UINT8 *tmp = tmpscr;
		tmpscr = srcscr;
		srcscr = tmp;
	}
	else if ((thiscam->postimg & POSTIMG_MIRROR) && !(thiscam->postimg & POSTIMG_FLIP)) // Flip the screen on the x axis
	{
		INT32 y, x, x2;

		for (y = yoffset; y < yoffset+viewheight; y++)
			for (x = xoffset, x2 = xoffset+(viewwidth-1); x < xoffset+viewwidth; x++, x2--)
				tmpscr[y*vid.width + x2] = srcscr[y*vid.width + x];

		UINT8 *tmp = tmpscr;
		tmpscr = srcscr;
		srcscr = tmp;
	}
	else if ((thiscam->postimg & POSTIMG_MIRROR) && (thiscam->postimg & POSTIMG_FLIP)) // Flip the screen upside-down and on the x axis
	{
		INT32 y, x;

		for (y = yoffset; y < yoffset + viewheight; y++)
			for (x = xoffset; x < xoffset + viewwidth; x++)
				tmpscr[((yoffset + viewheight - 1 - y) * vid.width) + xoffset + viewwidth - (x - xoffset) - 1] = srcscr[(y * vid.width) + x];

		UINT8 *tmp = tmpscr;
		tmpscr = srcscr;
		srcscr = tmp;
	}

	VID_BlitLinearScreen(srcscr+vid.width*yoffset+xoffset, tmpscr+vid.width*yoffset+xoffset,
						 viewwidth, viewheight, vid.width, vid.width);
#endif
}

// Generates a RGB565 color look-up table
void InitColorLUT(colorlookup_t *lut, RGBA_t *palette, boolean makecolors)
{
	size_t palsize = (sizeof(RGBA_t) * 256);

	if (!lut->init || memcmp(lut->palette, palette, palsize))
	{
		INT32 i;

		lut->init = true;
		memcpy(lut->palette, palette, palsize);

		for (i = 0; i < 0x10000; i++)
			lut->table[i] = 0xFFFF;

		if (makecolors)
		{
			UINT8 r, g, b;

			for (r = 0; r < 0xFF; r++)
			{
				for (g = 0; g < 0xFF; g++)
				{
					for (b = 0; b < 0xFF; b++)
					{
						i = CLUTINDEX(r, g, b);
						if (lut->table[i] == 0xFFFF)
							lut->table[i] = NearestPaletteColor(r, g, b, palette);
					}
				}
			}
		}
	}
}

UINT8 GetColorLUT(colorlookup_t *lut, UINT8 r, UINT8 g, UINT8 b)
{
	INT32 i = CLUTINDEX(r, g, b);
	if (lut->table[i] == 0xFFFF)
		lut->table[i] = NearestPaletteColor(r, g, b, lut->palette);
	return lut->table[i];
}

UINT8 GetColorLUTDirect(colorlookup_t *lut, UINT8 r, UINT8 g, UINT8 b)
{
	INT32 i = CLUTINDEX(r, g, b);
	return lut->table[i];
}

// V_Init
// old software stuff, buffers are allocated at video mode setup
// here we set the screens[x] pointers accordingly
// WARNING: called at runtime (don't init cvar here)
void V_Init(void)
{
	INT32 i;
	INT32 screensize = vid.width * vid.height;

	for (i = 0; i < NUMSCREENS; i++)
	{
		if (vid.screens[i])
		{
#if defined(__SSE__)
			aligned_free(vid.screens[i]);
#else
			free(vid.screens[i]);
#endif
		}

		vid.screens[i] = NULL;
	}

	// start address of NUMSCREENS * width*height vidbuffers
	if (screensize > 0)
	{
		for (i = 0; i < NUMSCREENS; i++)
		{
			// we need to allocate these relative to their cpu restrictions to not trigger segfaults
			// TODO: add support for sve and neon
#if defined(__SSE__)
			while (screensize & 15)
				screensize++;
			vid.screens[i] = aligned_alloc(16, screensize);
#else
			vid.screens[i] = malloc(screensize);
#endif
			memset(vid.screens[i], 0, screensize);
		}
	}

#ifdef DEBUG
	CONS_Debug(DBG_RENDER, "V_Init done:\n");
	for (i = 0; i < NUMSCREENS; i++)
		CONS_Debug(DBG_RENDER, " screens[%d] = %x\n", i, screens[i]);
#endif
}

void V_Recalc(void)
{
	// scale 1,2,3 times in x and y the patches for the menus and overlays...
	// calculated once and for all, used by routines in v_video.c and v_draw.c

	// Set dup based on width or height, whichever is less
	if (((vid.width*FRACUNIT) / BASEVIDWIDTH) < ((vid.height*FRACUNIT) / BASEVIDHEIGHT))
	{
		vid.dup = vid.width / BASEVIDWIDTH;
		vid.fdup = (vid.width*FRACUNIT) / BASEVIDWIDTH;
	}
	else
	{
		vid.dup = vid.height / BASEVIDHEIGHT;
		vid.fdup = (vid.height*FRACUNIT) / BASEVIDHEIGHT;
	}

	vid.udup = vid.dup;

	if (loaded_config // this could use a better name, since it is more and indicator that early startup is done and its safe to do sketchy shit now :chaosleep:
	&& (vid.width > 720) && (vid.height > 1280)) // ehhhh well this thing has so many issues, so ill lock it to higher resolutions instead
	{
		vid.dup = FixedDiv(vid.dup, cv_highreshudscale.value);
		vid.fdup = FixedDiv(vid.fdup, cv_highreshudscale.value);
	}

	vid.scaledwidth = (vid.width/vid.dup);

	vid.meddup = (UINT8)(vid.dup >> 1) + 1;
	vid.smalldup = (UINT8)(vid.dup / 3) + 1;
}
