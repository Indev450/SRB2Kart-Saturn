// SONIC ROBO BLAST 2 KART
//-----------------------------------------------------------------------------
// Copyright (C) 2018-2020 by Kart Krew
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  k_hud.c
/// \brief HUD drawing functions exclusive to Kart

#include "doomdef.h"
#include "doomstat.h"
#include "doomtype.h"
#include "command.h"
#include "d_netcmd.h"
#include "d_player.h"
#include "hu_stuff.h"
#include "g_game.h"
#include "i_joy.h"
#include "m_fixed.h"
#include "m_menu.h" // ffdhidshfuisduifigergho9igj89dgodhfih AAAAAAAAAA
#include "p_local.h"
#include "p_setup.h"
#include "r_defs.h"
#include "r_draw.h"
#include "r_state.h"
#include "r_fps.h"
#include "s_sound.h"
#include "screen.h"
#include "st_stuff.h"
#include "tables.h"
#include "v_video.h"
#include "z_zone.h"
#include "m_cond.h"
#include "k_director.h"
#include "k_kart.h"
#include "lua_hud.h"	// For Lua hud checks
#include "d_main.h"		// found_extra_kart
#include "i_video.h"

#include "k_hud.h"

#ifdef ROTSPRITE
#include "r_patchrotation.h"
#endif

#ifdef HWRENDER
#include "hardware/hw_main.h"
#endif

#define XTRA3PSCALE 50135 //0.765
#define XTRA3VSCALE 35344 //0.55

// Hud offset cvars
#define IMPL_HUD_OFFSET_X(name)\
consvar_t cv_##name##_xoffset = {"hud_" #name "_xoffset", "0", CV_SAVE, NULL, NULL, 0, NULL, NULL, 0, 0, NULL};

#define IMPL_HUD_OFFSET_Y(name)\
consvar_t cv_##name##_yoffset = {"hud_" #name "_yoffset", "0", CV_SAVE, NULL, NULL, 0, NULL, NULL, 0, 0, NULL};

#define IMPL_HUD_OFFSET(name)\
IMPL_HUD_OFFSET_X(name)\
IMPL_HUD_OFFSET_Y(name)

IMPL_HUD_OFFSET(item);   // Item box
IMPL_HUD_OFFSET(time);   // Time
IMPL_HUD_OFFSET(laps);   // Number of laps
IMPL_HUD_OFFSET(dnft);   // Countdown (did not finish timer)
IMPL_HUD_OFFSET(speed);  // Speedometer
IMPL_HUD_OFFSET(posi);   // Position in race
IMPL_HUD_OFFSET(wheel);  // RA Wheel
IMPL_HUD_OFFSET(face);   // Mini rankings
IMPL_HUD_OFFSET(stcd);   // Starting countdown
IMPL_HUD_OFFSET_Y(chek); // Check gfx
IMPL_HUD_OFFSET(mini);   // Minimap
IMPL_HUD_OFFSET(want);   // Wanted
IMPL_HUD_OFFSET(stat);   // Stats

#undef IMPL_HUD_OFFSET
#undef IMPL_HUD_OFFSET_X
#undef IMPL_HUD_OFFSET_Y

// extra hud things
consvar_t cv_showstats = {"showstats", "Off", CV_SAVE, CV_OnOff, NULL, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_showstats_skinname = {"showstats_skinname", "On", CV_SAVE, CV_OnOff, NULL, 0, NULL, NULL, 0, 0, NULL};

consvar_t cv_highresportrait = {"highresportrait", "Off", CV_SAVE, CV_OnOff, NULL, 0, NULL, NULL, 0, 0, NULL}; // make char potraits use their high-res version instead

CV_PossibleValue_t inputdisplay_cons_t[NUMINPUTDISPLAYSTUFF];
consvar_t cv_showinput = {"showinput", "Off", CV_SAVE, inputdisplay_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL};

static CV_PossibleValue_t minihead_cons_t[] = {{0, "Off"}, {1, "On"}, {2, "Others"}, {0, NULL}};
consvar_t cv_minihead         = {"smallminimapplayers", "Off", CV_SAVE, minihead_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_showminimapnames = {"showminimapnames", "Off", CV_SAVE, CV_OnOff, NULL, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_showminimapfinished = {"showminimapfinished", "On", CV_SAVE, CV_OnOff, NULL, 0, NULL, NULL, 0, 0, NULL};

CV_PossibleValue_t minimapdot_cons_t[NUMMINIMAPDOTSTUFF];
consvar_t cv_showminimapangle = {"showminimapangle", "Off", CV_SAVE, minimapdot_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL};

static CV_PossibleValue_t posanim_cons_t[] = {{0, "Off"}, {1, "On"}, {2, "Smooth"}, {0, NULL}};
consvar_t cv_posanim        = {"postitionanimation", "On", CV_SAVE, posanim_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_smallposnum    = {"smallpositionnumber", "Off", CV_SAVE, CV_OnOff, NULL, 0, NULL, NULL, 0, 0, NULL};

consvar_t cv_showlapemblem = {"showlapemblem", "On", CV_SAVE, CV_OnOff, NULL, 0, NULL, NULL, 0, 0, NULL};

static CV_PossibleValue_t huditemamount_cons_t[] = {{0, "Vanilla"}, {1, "Multiple"}, {2, "Always"},{0, NULL}};
consvar_t cv_huditemamount = {"showitemamountnumber", "Vanilla", CV_SAVE, huditemamount_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_fancyroulette = {"animatedroulette", "Off", CV_SAVE, CV_OnOff, NULL, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_darkitembox   = {"darkitembox", "On", CV_SAVE, CV_OnOff, NULL, 0, NULL, NULL, 0, 0, NULL}; // itembox gets a dark border with specific items
consvar_t cv_multiitemicon = {"multiitemicon", "Off", CV_SAVE, CV_OnOff, NULL, 0, NULL, NULL, 0, 0, NULL};

consvar_t cv_showlaptimes = {"showlaptimes", "Off", CV_SAVE, CV_OnOff, NULL, 0, NULL, NULL, 0, 0, NULL};

consvar_t cv_biglaps = {"biglaphud", "On", CV_SAVE, CV_OnOff, NULL, 0, NULL, NULL, 0, 0, NULL}; // here for ppl who dont want to make 2 more patches for their custom hud

// Speedometer
CV_PossibleValue_t speedo_cons_t[NUMSPEEDOSTUFF];
consvar_t cv_newspeedometer = {"newspeedometer", "Default", CV_SAVE, speedo_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_battlespeedo   = {"battlespeedo", "Off", CV_SAVE, CV_OnOff, NULL, 0, NULL, NULL, 0, 0, NULL}; // toggle for showing the speedometer in battlemode

// Colourized HUD
consvar_t cv_colorizedhud     = {"colorizedhud", "Off", CV_SAVE|CV_CALL, CV_OnOff, SaturnHud_menu_Onchange, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_colorizeditembox = {"colorizeditembox", "Off", CV_SAVE, CV_OnOff, NULL, 0, NULL, NULL, 0, 0, NULL};

static CV_PossibleValue_t HudColor_cons_t[MAXSKINCOLORS+1];
consvar_t cv_colorizedhudcolor = {"colorizedhudcolor", "Skin Color", CV_SAVE, HudColor_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL};

// Driftgauge
static CV_PossibleValue_t driftgaugeoffset_cons_t[] = {
	{-FRACUNIT*128, "MIN"}, {FRACUNIT*128, "MAX"}, {0, NULL}};
CV_PossibleValue_t driftgaugestyle_cons_t[NUMDGAUGESTUFF];

consvar_t cv_driftgauge      = {"kartdriftgauge", "Off", CV_SAVE, CV_OnOff, NULL, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_driftgaugeofs   = {"kartdriftgaugeoffset", "-20", CV_FLOAT|CV_SAVE, driftgaugeoffset_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_driftgaugetrans = {"kartdriftgaugetransparency", "On", CV_SAVE, CV_OnOff, NULL, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_driftgaugestyle = {"kartdriftgaugestyle", "1", CV_SAVE, driftgaugestyle_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL};

// Nametags
static CV_PossibleValue_t nametagtrans_cons_t[] = {
	{0, "Never"}, {1, "Far only"}, {2, "Dynamic"}, {3, "Always"}, {4, "HUD Trans."}, {0, NULL}};
static CV_PossibleValue_t nametagdistance_cons_t[] = {
	{0, "MIN"},  {640, "MAX"}, {0, NULL}};
static CV_PossibleValue_t nametagmaxplayer_cons_t[] = {
    {1, "MIN"}, {MAXPLAYERS, "MAX"}, {0, NULL}};
static CV_PossibleValue_t nametagsize_cons_t[] = {
	{0, "Off"}, {1, "Small"}, {2, "Minimal"}, {0, NULL}};
static CV_PossibleValue_t nametagrestat_cons_t[] = {
	{0, "Off"}, {1, "Restat"}, {2, "Always"}, {0, NULL}};

consvar_t cv_nametag              = {"kartnametag", "Off", CV_SAVE, CV_OnOff, NULL, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_nametagtrans         = {"kartnametagtransparency", "Dynamic", CV_SAVE, nametagtrans_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_nametagfacerank      = {"kartnametagfacerank", "Off", CV_SAVE, CV_OnOff, NULL, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_nametagrestat        = {"kartnametagrestat", "Restat", CV_SAVE, nametagrestat_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_nametagdist          = {"kartnametagdist", "320", CV_SAVE, nametagdistance_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_nametagmaxplayers    = {"kartnametagmaxplayers", "3", CV_SAVE, nametagmaxplayer_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_nametagmaxlenght     = {"kartnametagmaxlenght", "12", CV_SAVE, CV_Unsigned, NULL, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_showownnametag       = {"kartnametagshowown", "Off", CV_SAVE, CV_OnOff, NULL, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_smallnametags        = {"kartnametagsmall", "Off", CV_SAVE, nametagsize_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_nametagscore         = {"kartnametagscore", "Off", CV_SAVE, CV_OnOff, NULL, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_shownametagspectator = {"kartshownametagspectator", "No", CV_SAVE, CV_YesNo, NULL, 0, NULL, NULL, 0, 0, NULL};

// Registers kart client commands and variables.
// Nothing needed for a dedicated server should be registered here.
void K_RegisterKartHudStuff(void)
{
	HudColor_cons_t[0].value = 0;
	HudColor_cons_t[0].strvalue = "Skin Color";

	for (INT32 i = 1; i < MAXSKINCOLORS; i++)
	{
		HudColor_cons_t[i].value = i;
		HudColor_cons_t[i].strvalue = KartColor_Names[i]; // SRB2kart
	}
	HudColor_cons_t[MAXSKINCOLORS].value = 0;
	HudColor_cons_t[MAXSKINCOLORS].strvalue = NULL;

#define REG_HUD_OFFSET_X(name)\
	CV_RegisterVar(&cv_##name##_xoffset);

#define REG_HUD_OFFSET_Y(name)\
	CV_RegisterVar(&cv_##name##_yoffset);

#define REG_HUD_OFFSET(name)\
	REG_HUD_OFFSET_X(name)\
	REG_HUD_OFFSET_Y(name)

	REG_HUD_OFFSET(item);   // Item box
	REG_HUD_OFFSET(time);   // Time
	REG_HUD_OFFSET(laps);   // Number of laps
	REG_HUD_OFFSET(dnft);   // Countdown (did not finish timer)
	REG_HUD_OFFSET(speed);  // Speedometer
	REG_HUD_OFFSET(posi);   // Position in race
	REG_HUD_OFFSET(wheel);  // Position in race
	REG_HUD_OFFSET(face);   // Mini rankings
	REG_HUD_OFFSET(stcd);   // Starting countdown
	REG_HUD_OFFSET_Y(chek); // Check gfx
	REG_HUD_OFFSET(mini);   // Minimap
	REG_HUD_OFFSET(want);   // Wanted
	REG_HUD_OFFSET(stat);   // Stats

#undef REG_HUD_OFFSET
#undef REG_HUD_OFFSET_X
#undef REG_HUD_OFFSET_Y

	CV_RegisterVar(&cv_kartminimap);
	CV_RegisterVar(&cv_kartcheck);

	CV_RegisterVar(&cv_kartspeedometer);

	CV_RegisterVar(&cv_showstats);
	CV_RegisterVar(&cv_showstats_skinname);
	CV_RegisterVar(&cv_showinput);

	CV_RegisterVar(&cv_posanim);
	CV_RegisterVar(&cv_smallposnum);

	CV_RegisterVar(&cv_fancyroulette);

	CV_RegisterVar(&cv_showlaptimes);
	CV_RegisterVar(&cv_newspeedometer);

	CV_RegisterVar(&cv_battlespeedo);

	CV_RegisterVar(&cv_minihead);
	CV_RegisterVar(&cv_showminimapnames);
	CV_RegisterVar(&cv_showminimapfinished);
	CV_RegisterVar(&cv_showminimapangle);

	CV_RegisterVar(&cv_showlapemblem);

	CV_RegisterVar(&cv_stagetitle);

	// Colourized HUD
	CV_RegisterVar(&cv_colorizedhud);
	CV_RegisterVar(&cv_colorizedhudcolor);
	CV_RegisterVar(&cv_colorizeditembox);

	CV_RegisterVar(&cv_biglaps);

	CV_RegisterVar(&cv_darkitembox);

	CV_RegisterVar(&cv_multiitemicon);
	CV_RegisterVar(&cv_huditemamount);

	CV_RegisterVar(&cv_highresportrait);

	CV_RegisterVar(&cv_nametag);
	CV_RegisterVar(&cv_nametagtrans);
	CV_RegisterVar(&cv_nametagfacerank);
	CV_RegisterVar(&cv_nametagmaxplayers);
	CV_RegisterVar(&cv_nametagmaxlenght);
	CV_RegisterVar(&cv_nametagdist);
	CV_RegisterVar(&cv_showownnametag);
	CV_RegisterVar(&cv_smallnametags);
	CV_RegisterVar(&cv_nametagrestat);
	CV_RegisterVar(&cv_nametagscore);
	CV_RegisterVar(&cv_shownametagspectator);

	CV_RegisterVar(&cv_driftgauge);
	CV_RegisterVar(&cv_driftgaugeofs);
	CV_RegisterVar(&cv_driftgaugetrans);
	CV_RegisterVar(&cv_driftgaugestyle);

	CV_RegisterVar(&cv_lessflicker);
}

//{ Patch Definitions

// --Saturn patches--
// dont leave those unitialized incase we missed a check somewhere

// -Speedometers-

// Smol speedo
static patch_t *skp_smallsticker    = NULL;
static patch_t *skp_smallsticker3   = NULL;

static patch_t *skp_speedpatches[5] = {NULL};

// Achii speedo
static patch_t *skp_smallstickerachi       =  NULL;
static patch_t *skp_speedpatchesachi[5]    = {NULL};
static patch_t *skp_smallstickerachiclr    =  NULL;
static patch_t *skp_speedpatchesachiclr[5] = {NULL};

// Kartz speedo
static patch_t *kp_kartzspeedo[25]      = {NULL};
static patch_t *kp_kartzspeedo_smol[25] = {NULL};

// dial speedometer
static patch_t *skp_rankfinish             =  NULL;
static patch_t *skp_dialbase[2]            = {NULL};
static patch_t *skp_speedpatchesdial[5]    = {NULL};
static patch_t *skp_dialnum[10]            = {NULL};
//static patch_t *skp_dialclr              =  NULL;
static patch_t *skp_dialbaseclr[2]         = {NULL};
static patch_t *skp_speedpatchesdialclr[5] = {NULL};
static patch_t *skp_dialnumclr[10]         = {NULL};

// -Colourized hud-

static patch_t *kp_timestickerclr        =  NULL;
static patch_t *kp_timestickerwideclr    =  NULL;
static patch_t *kp_lapstickerclr         =  NULL;
static patch_t *kp_lapstickerbigclr      =  NULL;
static patch_t *kp_lapstickerbig2clr     =  NULL;
static patch_t *kp_lapstickerwideclr     =  NULL;
//static patch_t *kp_lapstickernarrowclr =  NULL;
static patch_t *kp_bumperstickerclr      =  NULL;
static patch_t *kp_bumperstickerwideclr  =  NULL;
static patch_t *kp_karmastickerclr       =  NULL;
static patch_t *kp_timeoutstickerclr     =  NULL;
static patch_t *skp_smallstickerclr      =  NULL;
static patch_t *skp_smallstickerclr3     =  NULL;
static patch_t *kp_itemmulstickerclr[2]  = {NULL};
static patch_t *kp_itembgclr[4]          = {NULL};

// -Misc.-

static patch_t *kp_lapstickerbig  = NULL;
static patch_t *kp_lapstickerbig2 = NULL;

static patch_t *nametagpic    = NULL;
static patch_t *nametagline   = NULL;
static patch_t *nametagspeed  = NULL;
static patch_t *nametagweight = NULL;

static patch_t *driftgauge           = NULL;
static patch_t *driftgaugecolor      = NULL;
static patch_t *driftgaugesmall      = NULL;
static patch_t *driftgaugesmallcolor = NULL;

static patch_t *kp_minimapdot = NULL;

static patch_t *joybacking = NULL;
static patch_t *joyknob    = NULL;
static patch_t *joyshadow  = NULL;

static patch_t *kp_multjawz       =  NULL;
static patch_t *kp_multsneaker[2] = {NULL};
static patch_t *kp_multbanana[3]  = {NULL};

static void K_LoadSaturnHUDGraphics(void)
{
	INT32 i;
	char buffer[9];

	if (found_extra_kart)
	{
		if (xtra_speedo) // smol speedometer
		{
			skp_smallsticker    = (patch_t *)W_CachePatchName("SP_SMSTC", PU_HUDGFX);
			skp_speedpatches[0] = (patch_t *)W_CachePatchName("K_TRNULL", PU_HUDGFX); // lolxd
			skp_speedpatches[1] = (patch_t *)W_CachePatchName("SP_MKMH",  PU_HUDGFX);
			skp_speedpatches[2] = (patch_t *)W_CachePatchName("SP_MMPH",  PU_HUDGFX);
			skp_speedpatches[3] = (patch_t *)W_CachePatchName("SP_MFRAC", PU_HUDGFX);
			skp_speedpatches[4] = (patch_t *)W_CachePatchName("SP_MPERC", PU_HUDGFX);
		}

		if (achi_speedo)
		{
			skp_smallstickerachi    = (patch_t *)W_CachePatchName("SP_AMSTC", PU_HUDGFX);
			skp_speedpatchesachi[0] = (patch_t *)W_CachePatchName("K_TRNULL", PU_HUDGFX); // lolxd
			skp_speedpatchesachi[1] = (patch_t *)W_CachePatchName("SP_AKMH",  PU_HUDGFX);
			skp_speedpatchesachi[2] = (patch_t *)W_CachePatchName("SP_AMPH",  PU_HUDGFX);
			skp_speedpatchesachi[3] = (patch_t *)W_CachePatchName("SP_AFRAC", PU_HUDGFX);
			skp_speedpatchesachi[4] = (patch_t *)W_CachePatchName("SP_APERC", PU_HUDGFX);
		}

		if (dial_speedo)
		{
			skp_rankfinish  = (patch_t *)W_CachePatchName("RANKFIN",  PU_HUDGFX);
			skp_dialbase[0] = (patch_t *)W_CachePatchName("K_DSPBS1", PU_HUDGFX);
			skp_dialbase[1] = (patch_t *)W_CachePatchName("K_DSPBS2", PU_HUDGFX);

			skp_speedpatchesdial[0] = (patch_t *)W_CachePatchName("K_TRNULL", PU_HUDGFX); // lolxd
			skp_speedpatchesdial[1] = (patch_t *)W_CachePatchName("SP_DKMH",  PU_HUDGFX);
			skp_speedpatchesdial[2] = (patch_t *)W_CachePatchName("SP_DMPH",  PU_HUDGFX);
			skp_speedpatchesdial[3] = (patch_t *)W_CachePatchName("SP_DFRAC", PU_HUDGFX);
			skp_speedpatchesdial[4] = (patch_t *)W_CachePatchName("SP_DPERC", PU_HUDGFX);

			sprintf(buffer, "K_DSPNMx");
			for (i = 0; i < 10; i++)
			{
				buffer[7] = '0'+(i%10);
				skp_dialnum[i] = (patch_t *)W_CachePatchName(buffer, PU_HUDGFX);
			}
		}

		// KartZ speedo
		if (kartz_speedo)
		{
			const char* patchNames[] = {
				"K_KZSP1", "K_KZSP2", "K_KZSP3", "K_KZSP4", "K_KZSP5",
				"K_KZSP6", "K_KZSP7", "K_KZSP8", "K_KZSP9", "K_KZSP10",
				"K_KZSP11", "K_KZSP12", "K_KZSP13", "K_KZSP14", "K_KZSP15",
				"K_KZSP16", "K_KZSP17", "K_KZSP18", "K_KZSP19", "K_KZSP20",
				"K_KZSP21", "K_KZSP22", "K_KZSP23", "K_KZSP24", "K_KZSP25"
			};

			for (size_t m = 0; m < sizeof(patchNames) / sizeof(patchNames[0]); ++m)
			{
				kp_kartzspeedo[m] = (patch_t *)W_CachePatchName(patchNames[m], PU_HUDGFX);
			}
		}

		if (kartz_speedo_smol)
		{
			const char* patchNames[] = {
				"K_KZSS1", "K_KZSS2", "K_KZSS3", "K_KZSS4", "K_KZSS5",
				"K_KZSS6", "K_KZSS7", "K_KZSS8", "K_KZSS9", "K_KZSS10",
				"K_KZSS11", "K_KZSS12", "K_KZSS13", "K_KZSS14", "K_KZSS15",
				"K_KZSS16", "K_KZSS17", "K_KZSS18", "K_KZSS19", "K_KZSS20",
				"K_KZSS21", "K_KZSS22", "K_KZSS23", "K_KZSS24", "K_KZSS25"
			};

			for (size_t m = 0; m < sizeof(patchNames) / sizeof(patchNames[0]); ++m)
			{
				kp_kartzspeedo_smol[m] = (patch_t *)W_CachePatchName(patchNames[m], PU_HUDGFX);
			}
		}

		if (big_lap)
		{
			kp_lapstickerbig  = (patch_t *)W_CachePatchName("K_STLAPB", PU_HUDGFX);
			kp_lapstickerbig2 = (patch_t *)W_CachePatchName("K_STLA2B", PU_HUDGFX);
		}

		// Nametags
		if (nametaggfx)
		{
			nametagpic    = (patch_t *)W_CachePatchName("NTLINE", PU_HUDGFX);
			nametagline   = (patch_t *)W_CachePatchName("NTLINEV", PU_HUDGFX);
			nametagspeed  = (patch_t *)W_CachePatchName("NTSP", PU_HUDGFX);
			nametagweight = (patch_t *)W_CachePatchName("NTWH", PU_HUDGFX);
		}

		if (driftgaugegfx)
		{
			driftgauge      =  (patch_t *)W_CachePatchName("K_DGAU", PU_HUDGFX);
			driftgaugesmall =  (patch_t *)W_CachePatchName("K_DGSU", PU_HUDGFX);

			if (driftgaugegfx_clr)
			{
				driftgaugecolor      =  (patch_t *)W_CachePatchName("K_DCAU", PU_HUDGFX);
				driftgaugesmallcolor =  (patch_t *)W_CachePatchName("K_DCSU", PU_HUDGFX);
			}
		}

		if (joystickicon)
		{
			joybacking = (patch_t *)W_CachePatchName("JOYBCK", PU_HUDGFX);
			joyknob    = (patch_t *)W_CachePatchName("JOYKNB", PU_HUDGFX);
			joyshadow  = (patch_t *)W_CachePatchName("JOYSHD", PU_HUDGFX);
		}

		if (minidoticon)
		{
			kp_minimapdot = (patch_t *)W_CachePatchName("MMAPDOT", PU_HUDGFX);
		}

		if (multiitem_icon)
		{
			kp_multsneaker[0] = (patch_t *)W_CachePatchName("K_ITSHO2", PU_HUDGFX);
			kp_multsneaker[1] = (patch_t *)W_CachePatchName("K_ITSHO3", PU_HUDGFX);
			kp_multbanana[0]  = (patch_t *)W_CachePatchName("K_ITBAN2", PU_HUDGFX);
			kp_multbanana[1]  = (patch_t *)W_CachePatchName("K_ITBAN3", PU_HUDGFX);
			kp_multbanana[2]  = (patch_t *)W_CachePatchName("K_ITBAN4", PU_HUDGFX);
			kp_multjawz       = (patch_t *)W_CachePatchName("K_ITJAW2", PU_HUDGFX);
		}
	}

	if (found_extra2_kart)
	{
		if (xtra_speedo_clr)
		{
			skp_smallstickerclr = (patch_t *)W_CachePatchName("SC_SMSTC", PU_HUDGFX);
		}

		if (xtra_speedo_clr3)
		{
			skp_smallstickerclr3 = (patch_t *)W_CachePatchName("SC_SM3TC", PU_HUDGFX);
		}

		if (achi_speedo_clr)
		{
			skp_smallstickerachiclr    = (patch_t *)W_CachePatchName("SC_AMSTC", PU_HUDGFX);
			skp_speedpatchesachiclr[0] = (patch_t *)W_CachePatchName("K_TRNULL", PU_HUDGFX); // lolxd
			skp_speedpatchesachiclr[1] = (patch_t *)W_CachePatchName("SC_AKMH",  PU_HUDGFX);
			skp_speedpatchesachiclr[2] = (patch_t *)W_CachePatchName("SC_AMPH",  PU_HUDGFX);
			skp_speedpatchesachiclr[3] = (patch_t *)W_CachePatchName("SC_AFRAC", PU_HUDGFX);
			skp_speedpatchesachiclr[4] = (patch_t *)W_CachePatchName("SC_APERC", PU_HUDGFX);
		}

		if (dial_speedo_clr)
		{
			skp_dialbaseclr[0] = (patch_t *)W_CachePatchName("K_DSPBC1", PU_HUDGFX);
			skp_dialbaseclr[1] = (patch_t *)W_CachePatchName("K_DSPBC2", PU_HUDGFX);

			skp_speedpatchesdialclr[0] = (patch_t *)W_CachePatchName("K_TRNULL", PU_HUDGFX); // lolxd
			skp_speedpatchesdialclr[1] = (patch_t *)W_CachePatchName("SC_DKMH",  PU_HUDGFX);
			skp_speedpatchesdialclr[2] = (patch_t *)W_CachePatchName("SC_DMPH",  PU_HUDGFX);
			skp_speedpatchesdialclr[3] = (patch_t *)W_CachePatchName("SC_DFRAC", PU_HUDGFX);
			skp_speedpatchesdialclr[4] = (patch_t *)W_CachePatchName("SC_DPERC", PU_HUDGFX);

			sprintf(buffer, "K_DSPNCx");
			for (i = 0; i < 10; i++)
			{
				buffer[7] = '0'+(i%10);
				skp_dialnumclr[i] = (patch_t *)W_CachePatchName(buffer, PU_HUDGFX);
			}
		}

		if (big_lap_color)
		{
			kp_lapstickerbigclr  = (patch_t *)W_CachePatchName("K_SCLAPB", PU_HUDGFX);
			kp_lapstickerbig2clr = (patch_t *)W_CachePatchName("K_SCLA2B", PU_HUDGFX);
		}

		// Colourized hud
		if (clr_hud)
		{
			kp_timestickerclr       = (patch_t *)W_CachePatchName("K_SCTIME", PU_HUDGFX);
			kp_timestickerwideclr   = (patch_t *)W_CachePatchName("K_SCTIMW", PU_HUDGFX);
			kp_lapstickerclr        = (patch_t *)W_CachePatchName("K_SCLAPS", PU_HUDGFX);
			kp_lapstickerwideclr    = (patch_t *)W_CachePatchName("K_SCLAPW", PU_HUDGFX);
			kp_bumperstickerclr     = (patch_t *)W_CachePatchName("K_SCBALN", PU_HUDGFX);
			kp_bumperstickerwideclr = (patch_t *)W_CachePatchName("K_SCBALW", PU_HUDGFX);
			kp_karmastickerclr      = (patch_t *)W_CachePatchName("K_SCKARM", PU_HUDGFX);
			kp_timeoutstickerclr    = (patch_t *)W_CachePatchName("K_SCTOUT", PU_HUDGFX);
			kp_itembgclr[0]         = (patch_t *)W_CachePatchName("K_ITBC"  , PU_HUDGFX);
			kp_itembgclr[1]         = (patch_t *)W_CachePatchName("K_ITBCD" , PU_HUDGFX);
			kp_itembgclr[2]         = (patch_t *)W_CachePatchName("K_ISBC"  , PU_HUDGFX);
			kp_itembgclr[3]         = (patch_t *)W_CachePatchName("K_ISBCD" , PU_HUDGFX);
			kp_itemmulstickerclr[1] = (patch_t *)W_CachePatchName("K_ISMULC", PU_HUDGFX);
			kp_itemmulstickerclr[0] = (patch_t *)W_CachePatchName("K_ITMULC", PU_HUDGFX);
		}
	}

	if (found_extra3_kart)
	{
		if (xtra_speedo3) // 80x11 patch scaled to size
		{
			skp_smallsticker3 = (patch_t *)W_CachePatchName("SP_SM3TC", PU_HUDGFX);
		}
	}
}

// -- Kart patches --

#define NUMPOSNUMS 10
#define NUMPOSFRAMES 7 // White, three blues, three reds
#define NUMWINFRAMES 6 // Red, yellow, green, cyan, blue, purple

static patch_t *kp_nodraw;
static patch_t *kp_timesticker;
static patch_t *kp_timestickerwide;
static patch_t *kp_lapsticker;
static patch_t *kp_lapstickerwide;
//static patch_t *kp_lapstickernarrow;
static patch_t *kp_splitlapflag;
static patch_t *kp_bumpersticker;
static patch_t *kp_bumperstickerwide;
static patch_t *kp_karmasticker;
static patch_t *kp_splitkarmabomb;
static patch_t *kp_timeoutsticker;

static patch_t *kp_startcountdown[16];
static patch_t *kp_racefinish[6];

static patch_t *kp_positionnum[NUMPOSNUMS][NUMPOSFRAMES];
static patch_t *kp_winnernum[NUMPOSFRAMES];

patch_t *kp_facenum[MAXPLAYERS+1] = {};
static patch_t *kp_facehighlight[8];

static patch_t *kp_rankbumper;
static patch_t *kp_tinybumper[2];
static patch_t *kp_ranknobumpers;

static patch_t *kp_battlewin;
static patch_t *kp_battlecool;
static patch_t *kp_battlelose;
static patch_t *kp_battlewait;
static patch_t *kp_battleinfo;
static patch_t *kp_wanted;
static patch_t *kp_wantedsplit;
static patch_t *kp_wantedreticle;

static patch_t *kp_itembg[4];
static patch_t *kp_itemtimer[2];
static patch_t *kp_itemmulsticker[2];
static patch_t *kp_itemx;

static patch_t *kp_sneaker[2];
static patch_t *kp_rocketsneaker[2];
static patch_t *kp_invincibility[13];
static patch_t *kp_banana[2];
static patch_t *kp_eggman[2];
static patch_t *kp_orbinaut[5];
static patch_t *kp_jawz[2];
static patch_t *kp_mine[2];
static patch_t *kp_ballhog[2];
static patch_t *kp_selfpropelledbomb[2];
static patch_t *kp_grow[2];
static patch_t *kp_shrink[2];
static patch_t *kp_thundershield[2];
static patch_t *kp_hyudoro[2];
static patch_t *kp_pogospring[2];
static patch_t *kp_kitchensink[2];
static patch_t *kp_sadface[2];

static patch_t *kp_check[6];

static patch_t *kp_eggnum[4];

static patch_t *kp_fpview[3];
static patch_t *kp_inputwheel[5];

static patch_t *kp_challenger[25];

static patch_t *kp_lapanim_lap[7];
static patch_t *kp_lapanim_final[11];
static patch_t *kp_lapanim_number[10][3];
static patch_t *kp_lapanim_emblem[2];
static patch_t *kp_lapanim_hand[3];

static patch_t *kp_yougotem;

void K_LoadKartHUDGraphics(void)
{
	INT32 i, j;
	char buffer[9];

	// Null Stuff
	kp_nodraw = (patch_t *)W_CachePatchName("K_TRNULL", PU_HUDGFX);

	// Stickers
	kp_timesticker        = (patch_t *)W_CachePatchName("K_STTIME", PU_HUDGFX);
	kp_timestickerwide    = (patch_t *)W_CachePatchName("K_STTIMW", PU_HUDGFX);
	kp_lapsticker         = (patch_t *)W_CachePatchName("K_STLAPS", PU_HUDGFX);
	kp_lapstickerwide     = (patch_t *)W_CachePatchName("K_STLAPW", PU_HUDGFX);
	//kp_lapstickernarrow = (patch_t *)W_CachePatchName("K_STLAPN", PU_HUDGFX);
	kp_splitlapflag       = (patch_t *)W_CachePatchName("K_SPTLAP", PU_HUDGFX);
	kp_bumpersticker      = (patch_t *)W_CachePatchName("K_STBALN", PU_HUDGFX);
	kp_bumperstickerwide  = (patch_t *)W_CachePatchName("K_STBALW", PU_HUDGFX);
	kp_karmasticker       = (patch_t *)W_CachePatchName("K_STKARM", PU_HUDGFX);
	kp_splitkarmabomb     = (patch_t *)W_CachePatchName("K_SPTKRM", PU_HUDGFX);
	kp_timeoutsticker     = (patch_t *)W_CachePatchName("K_STTOUT", PU_HUDGFX);

	// Starting countdown
	kp_startcountdown[0] = (patch_t *)W_CachePatchName("K_CNT3A", PU_HUDGFX);
	kp_startcountdown[1] = (patch_t *)W_CachePatchName("K_CNT2A", PU_HUDGFX);
	kp_startcountdown[2] = (patch_t *)W_CachePatchName("K_CNT1A", PU_HUDGFX);
	kp_startcountdown[3] = (patch_t *)W_CachePatchName("K_CNTGOA", PU_HUDGFX);
	kp_startcountdown[4] = (patch_t *)W_CachePatchName("K_CNT3B", PU_HUDGFX);
	kp_startcountdown[5] = (patch_t *)W_CachePatchName("K_CNT2B", PU_HUDGFX);
	kp_startcountdown[6] = (patch_t *)W_CachePatchName("K_CNT1B", PU_HUDGFX);
	kp_startcountdown[7] = (patch_t *)W_CachePatchName("K_CNTGOB", PU_HUDGFX);

	// Splitscreen
	kp_startcountdown[8]  = (patch_t *)W_CachePatchName("K_SMC3A", PU_HUDGFX);
	kp_startcountdown[9]  = (patch_t *)W_CachePatchName("K_SMC2A", PU_HUDGFX);
	kp_startcountdown[10] = (patch_t *)W_CachePatchName("K_SMC1A", PU_HUDGFX);
	kp_startcountdown[11] = (patch_t *)W_CachePatchName("K_SMCGOA", PU_HUDGFX);
	kp_startcountdown[12] = (patch_t *)W_CachePatchName("K_SMC3B", PU_HUDGFX);
	kp_startcountdown[13] = (patch_t *)W_CachePatchName("K_SMC2B", PU_HUDGFX);
	kp_startcountdown[14] = (patch_t *)W_CachePatchName("K_SMC1B", PU_HUDGFX);
	kp_startcountdown[15] = (patch_t *)W_CachePatchName("K_SMCGOB", PU_HUDGFX);

	// Finish
	kp_racefinish[0] = (patch_t *)W_CachePatchName("K_FINA", PU_HUDGFX);
	kp_racefinish[1] = (patch_t *)W_CachePatchName("K_FINB", PU_HUDGFX);

	// Splitscreen
	kp_racefinish[2] = (patch_t *)W_CachePatchName("K_SMFINA", PU_HUDGFX);
	kp_racefinish[3] = (patch_t *)W_CachePatchName("K_SMFINB", PU_HUDGFX);

	// 2P splitscreen
	kp_racefinish[4] = (patch_t *)W_CachePatchName("K_2PFINA", PU_HUDGFX);
	kp_racefinish[5] = (patch_t *)W_CachePatchName("K_2PFINB", PU_HUDGFX);

	// Position numbers
	sprintf(buffer, "K_POSNxx");
	for (i = 0; i < NUMPOSNUMS; i++)
	{
		buffer[6] = '0'+i;
		for (j = 0; j < NUMPOSFRAMES; j++)
		{
			buffer[7] = '0'+j;
			kp_positionnum[i][j] = (patch_t *)W_CachePatchName(buffer, PU_HUDGFX);
		}
	}

	sprintf(buffer, "K_POSNWx");
	for (i = 0; i < NUMWINFRAMES; i++)
	{
		buffer[7] = '0'+i;
		kp_winnernum[i] = (patch_t *)W_CachePatchName(buffer, PU_HUDGFX);
	}

	sprintf(buffer, "OPPRNKxx");
	for (i = 0; i <= MAXPLAYERS; i++)
	{
		buffer[6] = '0'+(i/10);
		buffer[7] = '0'+(i%10);
		kp_facenum[i] = (patch_t *)W_CachePatchName(buffer, PU_HUDGFX);
	}

	sprintf(buffer, "K_CHILIx");
	for (i = 0; i < 8; i++)
	{
		buffer[7] = '0'+(i+1);
		kp_facehighlight[i] = (patch_t *)W_CachePatchName(buffer, PU_HUDGFX);
	}

	// Extra ranking icons
	kp_rankbumper    = (patch_t *)W_CachePatchName("K_BLNICO", PU_HUDGFX);
	kp_tinybumper[0] = (patch_t *)W_CachePatchName("K_BLNA", PU_HUDGFX);
	kp_tinybumper[1] = (patch_t *)W_CachePatchName("K_BLNB", PU_HUDGFX);
	kp_ranknobumpers = (patch_t *)W_CachePatchName("K_NOBLNS", PU_HUDGFX);

	// Battle graphics
	kp_battlewin     = (patch_t *)W_CachePatchName("K_BWIN", PU_HUDGFX);
	kp_battlecool    = (patch_t *)W_CachePatchName("K_BCOOL", PU_HUDGFX);
	kp_battlelose    = (patch_t *)W_CachePatchName("K_BLOSE", PU_HUDGFX);
	kp_battlewait    = (patch_t *)W_CachePatchName("K_BWAIT", PU_HUDGFX);
	kp_battleinfo    = (patch_t *)W_CachePatchName("K_BINFO", PU_HUDGFX);
	kp_wanted        = (patch_t *)W_CachePatchName("K_WANTED", PU_HUDGFX);
	kp_wantedsplit   = (patch_t *)W_CachePatchName("4PWANTED", PU_HUDGFX);
	kp_wantedreticle = (patch_t *)W_CachePatchName("MMAPWANT", PU_HUDGFX);

	// Kart Item Windows
	kp_itembg[0]         = (patch_t *)W_CachePatchName("K_ITBG", PU_HUDGFX);
	kp_itembg[1]         = (patch_t *)W_CachePatchName("K_ITBGD", PU_HUDGFX);
	kp_itemtimer[0]      = (patch_t *)W_CachePatchName("K_ITIMER", PU_HUDGFX);
	kp_itemmulsticker[0] = (patch_t *)W_CachePatchName("K_ITMUL", PU_HUDGFX);
	kp_itemx             = (patch_t *)W_CachePatchName("K_ITX", PU_HUDGFX);
	// Splitscreen
	kp_itembg[2]         = (patch_t *)W_CachePatchName("K_ISBG", PU_HUDGFX);
	kp_itembg[3]         = (patch_t *)W_CachePatchName("K_ISBGD", PU_HUDGFX);
	kp_itemtimer[1]      = (patch_t *)W_CachePatchName("K_ISIMER", PU_HUDGFX);
	kp_itemmulsticker[1] = (patch_t *)W_CachePatchName("K_ISMUL", PU_HUDGFX);

	// Kart Items
	kp_sneaker[0]           = (patch_t *)W_CachePatchName("K_ITSHOE", PU_HUDGFX);
	kp_rocketsneaker[0]     = (patch_t *)W_CachePatchName("K_ITRSHE", PU_HUDGFX);
	kp_banana[0]            = (patch_t *)W_CachePatchName("K_ITBANA", PU_HUDGFX);
	kp_eggman[0]            = (patch_t *)W_CachePatchName("K_ITEGGM", PU_HUDGFX);
	kp_jawz[0]              = (patch_t *)W_CachePatchName("K_ITJAWZ", PU_HUDGFX);
	kp_mine[0]              = (patch_t *)W_CachePatchName("K_ITMINE", PU_HUDGFX);
	kp_ballhog[0]           = (patch_t *)W_CachePatchName("K_ITBHOG", PU_HUDGFX);
	kp_selfpropelledbomb[0] = (patch_t *)W_CachePatchName("K_ITSPB", PU_HUDGFX);
	kp_grow[0]              = (patch_t *)W_CachePatchName("K_ITGROW", PU_HUDGFX);
	kp_shrink[0]            = (patch_t *)W_CachePatchName("K_ITSHRK", PU_HUDGFX);
	kp_thundershield[0]     = (patch_t *)W_CachePatchName("K_ITTHNS", PU_HUDGFX);
	kp_hyudoro[0]           = (patch_t *)W_CachePatchName("K_ITHYUD", PU_HUDGFX);
	kp_pogospring[0]        = (patch_t *)W_CachePatchName("K_ITPOGO", PU_HUDGFX);
	kp_kitchensink[0]       = (patch_t *)W_CachePatchName("K_ITSINK", PU_HUDGFX);
	kp_sadface[0]           = (patch_t *)W_CachePatchName("K_ITSAD", PU_HUDGFX);
	// Splitscreen
	kp_sneaker[1]           = (patch_t *)W_CachePatchName("K_ISSHOE", PU_HUDGFX);
	kp_rocketsneaker[1]     = (patch_t *)W_CachePatchName("K_ISRSHE", PU_HUDGFX);
	kp_banana[1]            = (patch_t *)W_CachePatchName("K_ISBANA", PU_HUDGFX);
	kp_eggman[1]            = (patch_t *)W_CachePatchName("K_ISEGGM", PU_HUDGFX);
	kp_orbinaut[4]          = (patch_t *)W_CachePatchName("K_ISORBN", PU_HUDGFX);
	kp_jawz[1]              = (patch_t *)W_CachePatchName("K_ISJAWZ", PU_HUDGFX);
	kp_mine[1]              = (patch_t *)W_CachePatchName("K_ISMINE", PU_HUDGFX);
	kp_ballhog[1]           = (patch_t *)W_CachePatchName("K_ISBHOG", PU_HUDGFX);
	kp_selfpropelledbomb[1] = (patch_t *)W_CachePatchName("K_ISSPB", PU_HUDGFX);
	kp_grow[1]              = (patch_t *)W_CachePatchName("K_ISGROW", PU_HUDGFX);
	kp_shrink[1]            = (patch_t *)W_CachePatchName("K_ISSHRK", PU_HUDGFX);
	kp_thundershield[1]     = (patch_t *)W_CachePatchName("K_ISTHNS", PU_HUDGFX);
	kp_hyudoro[1]           = (patch_t *)W_CachePatchName("K_ISHYUD", PU_HUDGFX);
	kp_pogospring[1]        = (patch_t *)W_CachePatchName("K_ISPOGO", PU_HUDGFX);
	kp_kitchensink[1]       = (patch_t *)W_CachePatchName("K_ISSINK", PU_HUDGFX);
	kp_sadface[1]           = (patch_t *)W_CachePatchName("K_ISSAD", PU_HUDGFX);

	sprintf(buffer, "K_ITINVx");
	for (i = 0; i < 7; i++)
	{
		buffer[7] = '1'+i;
		kp_invincibility[i] = (patch_t *)W_CachePatchName(buffer, PU_HUDGFX);
	}

	sprintf(buffer, "K_ITORBx");
	for (i = 0; i < 4; i++)
	{
		buffer[7] = '1'+i;
		kp_orbinaut[i] = (patch_t *)W_CachePatchName(buffer, PU_HUDGFX);
	}

	sprintf(buffer, "K_ISINVx");
	for (i = 0; i < 6; i++)
	{
		buffer[7] = '1'+i;
		kp_invincibility[i+7] = (patch_t *)W_CachePatchName(buffer, PU_HUDGFX);
	}

	// CHECK indicators
	sprintf(buffer, "K_CHECKx");
	for (i = 0; i < 6; i++)
	{
		buffer[7] = '1'+i;
		kp_check[i] = (patch_t *)W_CachePatchName(buffer, PU_HUDGFX);
	}

	// Eggman warning numbers
	sprintf(buffer, "K_EGGNx");
	for (i = 0; i < 4; i++)
	{
		buffer[6] = '0'+i;
		kp_eggnum[i] = (patch_t *)W_CachePatchName(buffer, PU_HUDGFX);
	}

	// First person mode
	kp_fpview[0] = (patch_t *)W_CachePatchName("VIEWA0", PU_HUDGFX);
	kp_fpview[1] = (patch_t *)W_CachePatchName("VIEWB0D0", PU_HUDGFX);
	kp_fpview[2] = (patch_t *)W_CachePatchName("VIEWC0E0", PU_HUDGFX);

	// Input UI Wheel
	sprintf(buffer, "K_WHEELx");
	for (i = 0; i < 5; i++)
	{
		buffer[7] = '0'+i;
		kp_inputwheel[i] = (patch_t *)W_CachePatchName(buffer, PU_HUDGFX);
	}

	// HERE COMES A NEW CHALLENGER
	sprintf(buffer, "K_CHALxx");
	for (i = 0; i < 25; i++)
	{
		buffer[6] = '0'+((i+1)/10);
		buffer[7] = '0'+((i+1)%10);
		kp_challenger[i] = (patch_t *)W_CachePatchName(buffer, PU_HUDGFX);
	}

	// Lap start animation
	sprintf(buffer, "K_LAP0x");
	for (i = 0; i < 7; i++)
	{
		buffer[6] = '0'+(i+1);
		kp_lapanim_lap[i] = (patch_t *)W_CachePatchName(buffer, PU_HUDGFX);
	}

	sprintf(buffer, "K_LAPFxx");
	for (i = 0; i < 11; i++)
	{
		buffer[6] = '0'+((i+1)/10);
		buffer[7] = '0'+((i+1)%10);
		kp_lapanim_final[i] = (patch_t *)W_CachePatchName(buffer, PU_HUDGFX);
	}

	sprintf(buffer, "K_LAPNxx");
	for (i = 0; i < 10; i++)
	{
		buffer[6] = '0'+i;
		for (j = 0; j < 3; j++)
		{
			buffer[7] = '0'+(j+1);
			kp_lapanim_number[i][j] = (patch_t *)W_CachePatchName(buffer, PU_HUDGFX);
		}
	}

	sprintf(buffer, "K_LAPE0x");
	for (i = 0; i < 2; i++)
	{
		buffer[7] = '0'+(i+1);
		kp_lapanim_emblem[i] = (patch_t *)W_CachePatchName(buffer, PU_HUDGFX);
	}

	sprintf(buffer, "K_LAPH0x");
	for (i = 0; i < 3; i++)
	{
		buffer[7] = '0'+(i+1);
		kp_lapanim_hand[i] = (patch_t *)W_CachePatchName(buffer, PU_HUDGFX);
	}

	kp_yougotem = (patch_t *)W_CachePatchName("YOUGOTEM", PU_HUDGFX);

	if (found_extra_kart || found_extra2_kart || found_extra3_kart)
		K_LoadSaturnHUDGraphics();
}

// For the item toggle menu
const char *K_GetItemPatch(UINT8 item, boolean tiny)
{
	switch (item)
	{
		case KITEM_SNEAKER:
		case KRITEM_TRIPLESNEAKER:
			return (tiny ? "K_ISSHOE" : "K_ITSHOE");
		case KITEM_ROCKETSNEAKER:
			return (tiny ? "K_ISRSHE" : "K_ITRSHE");
		case KITEM_INVINCIBILITY:
			return (tiny ? "K_ISINV1" : "K_ITINV1");
		case KITEM_BANANA:
		case KRITEM_TRIPLEBANANA:
		case KRITEM_TENFOLDBANANA:
			return (tiny ? "K_ISBANA" : "K_ITBANA");
		case KITEM_EGGMAN:
			return (tiny ? "K_ISEGGM" : "K_ITEGGM");
		case KITEM_ORBINAUT:
			return (tiny ? "K_ISORBN" : "K_ITORB1");
		case KITEM_JAWZ:
		case KRITEM_DUALJAWZ:
			return (tiny ? "K_ISJAWZ" : "K_ITJAWZ");
		case KITEM_MINE:
			return (tiny ? "K_ISMINE" : "K_ITMINE");
		case KITEM_BALLHOG:
			return (tiny ? "K_ISBHOG" : "K_ITBHOG");
		case KITEM_SPB:
			return (tiny ? "K_ISSPB" : "K_ITSPB");
		case KITEM_GROW:
			return (tiny ? "K_ISGROW" : "K_ITGROW");
		case KITEM_SHRINK:
			return (tiny ? "K_ISSHRK" : "K_ITSHRK");
		case KITEM_THUNDERSHIELD:
			return (tiny ? "K_ISTHNS" : "K_ITTHNS");
		case KITEM_HYUDORO:
			return (tiny ? "K_ISHYUD" : "K_ITHYUD");
		case KITEM_POGOSPRING:
			return (tiny ? "K_ISPOGO" : "K_ITPOGO");
		case KITEM_KITCHENSINK:
			return (tiny ? "K_ISSINK" : "K_ITSINK");
		case KRITEM_TRIPLEORBINAUT:
			return (tiny ? "K_ISORBN" : "K_ITORB3");
		case KRITEM_QUADORBINAUT:
			return (tiny ? "K_ISORBN" : "K_ITORB4");
		default:
			return (tiny ? "K_ISSAD" : "K_ITSAD");
	}
}

//}

static INT32 ITEM_X, ITEM_Y;	// Item Window
static INT32 TIME_X, TIME_Y;	// Time Sticker
static INT32 LAPS_X, LAPS_Y;	// Lap Sticker
static INT32 SPDM_X, SPDM_Y;	// Speedometer
static INT32 POSI_X, POSI_Y;	// Position Number
static INT32 FACE_X, FACE_Y;	// Top-four Faces
static INT32 STCD_X, STCD_Y;	// Starting countdown
static INT32 CHEK_Y;			// CHECK graphic
static INT32 MINI_X, MINI_Y;	// Minimap
static INT32 WANT_X, WANT_Y;	// Battle WANTED poster

// This is for the P2 and P4 side of splitscreen. Then we'll flip P1's and P2's to the bottom with V_SPLITSCREEN.
static INT32 ITEM2_X, ITEM2_Y;
static INT32 LAPS2_X, LAPS2_Y;
static INT32 POSI2_X, POSI2_Y;

static void K_initKartHUD(void)
{
	/*
		BASEVIDWIDTH  = 320
		BASEVIDHEIGHT = 200

		Item window graphic is 41 x 33

		Time Sticker graphic is 116 x 11
		Time Font is a solid block of (8 x [12) x 14], equal to 96 x 14
		Therefore, timestamp is 116 x 14 altogether

		Lap Sticker is 80 x 11
		Lap flag is 22 x 20
		Lap Font is a solid block of (3 x [12) x 14], equal to 36 x 14
		Therefore, lapstamp is 80 x 20 altogether

		Position numbers are 43 x 53

		Faces are 32 x 32
		Faces draw downscaled at 16 x 16
		Therefore, the allocated space for them is 16 x 67 altogether

		----

		ORIGINAL CZ64 SPLITSCREEN:

		Item window:
		if (!splitscreen) 	{ ICONX = 139; 				ICONY = 20; }
		else 				{ ICONX = BASEVIDWIDTH-315; ICONY = 60; }

		Time: 			   236, STRINGY(			   12)
		Lap:  BASEVIDWIDTH-304, STRINGY(BASEVIDHEIGHT-189)

	*/

	// Single Screen (defaults)
	// Item Window
	ITEM_X = 5 + cv_item_xoffset.value;						//   5
	ITEM_Y = 5 + cv_item_yoffset.value;						//   5
	// Level Timer
	TIME_X = BASEVIDWIDTH - 148 + cv_time_xoffset.value;	// 172
	TIME_Y = 9 + cv_time_yoffset.value;						//   9
	// Level Laps
	LAPS_X = 9 + cv_laps_xoffset.value;						//   9
	LAPS_Y = BASEVIDHEIGHT - 29 + cv_laps_yoffset.value;	// 171
	// Speedometer
	SPDM_X = 9 + cv_speed_xoffset.value; 					//   9
	SPDM_Y = BASEVIDHEIGHT - 45 + cv_speed_yoffset.value;	// 155
	// Position Number
	POSI_X = BASEVIDWIDTH  - 9 + cv_posi_xoffset.value;		// 268
	POSI_Y = BASEVIDHEIGHT - 9 + cv_posi_yoffset.value;		// 138
	// Top-Four Faces
	FACE_X = 9 + cv_face_xoffset.value;						//   9
	FACE_Y = 92 + cv_face_yoffset.value;					//  92
	// Starting countdown
	STCD_X = BASEVIDWIDTH/2 + cv_stcd_xoffset.value;		//   9
	STCD_Y = BASEVIDHEIGHT/2 + cv_stcd_yoffset.value;		//  92
	// CHECK graphic
	CHEK_Y = BASEVIDHEIGHT + cv_chek_yoffset.value;			// 200
	// Minimap
	MINI_X = BASEVIDWIDTH - 50 + cv_mini_xoffset.value;		// 270
	MINI_Y = (BASEVIDHEIGHT/2)-16 + cv_mini_yoffset.value;  //  84
	// Battle WANTED poster
	WANT_X = BASEVIDWIDTH - 55 + cv_want_xoffset.value;		// 270
	WANT_Y = BASEVIDHEIGHT- 71 + cv_want_yoffset.value;		// 176

	if (splitscreen)	// Splitscreen
	{
		ITEM_X = 5;
		ITEM_Y = 3;

		LAPS_Y = (BASEVIDHEIGHT/2)-24;

		POSI_Y = (BASEVIDHEIGHT/2)- 2;

		STCD_Y = BASEVIDHEIGHT/4;

		MINI_Y = (BASEVIDHEIGHT/2);

		if (splitscreen > 1)	// 3P/4P Small Splitscreen
		{
			// 1P (top left)
			ITEM_X = -9;
			ITEM_Y = -8;

			LAPS_X = 3;
			LAPS_Y = (BASEVIDHEIGHT/2)-13;

			POSI_X = 24;
			POSI_Y = (BASEVIDHEIGHT/2)- 16;

			// 2P (top right)
			ITEM2_X = BASEVIDWIDTH-39;
			ITEM2_Y = -8;

			LAPS2_X = BASEVIDWIDTH-3;
			LAPS2_Y = (BASEVIDHEIGHT/2)-13;

			POSI2_X = BASEVIDWIDTH -4;
			POSI2_Y = (BASEVIDHEIGHT/2)- 16;

			// Reminder that 3P and 4P are just 1P and 2P splitscreen'd to the bottom.

			STCD_X = BASEVIDWIDTH/4;

			MINI_X = (3*BASEVIDWIDTH/4);
			MINI_Y = (3*BASEVIDHEIGHT/4);

			if (splitscreen > 2) // 4P-only
			{
				MINI_X = (BASEVIDWIDTH/2);
				MINI_Y = (BASEVIDHEIGHT/2);
			}
		}
	}

	if (timeinmap > 113 || forceshowhud)
		hudtrans = (UINT8)cv_translucenthud.value;
	else if (timeinmap > 105)
		hudtrans = ((((INT32)timeinmap) - 105)*cv_translucenthud.value)/(113-105);
	else
		hudtrans = 0;

	// K_GetScreenCoords needs the right view* variables
	R_SetViewContext(stplyrnum);
	R_InterpolateView(R_GetTimeFrac(RTF_CAMERA), !cv_uncappedhud.value);
}

void K_KartPlayerHUDUpdate(player_t *player)
{
	if (player->kartstuff[k_lapanimation])
		player->kartstuff[k_lapanimation]--;

	if (player->kartstuff[k_yougotem])
		player->kartstuff[k_yougotem]--;

	if (G_BattleGametype() && (player->exiting || player->kartstuff[k_comebacktimer]))
	{
		if (player->exiting)
		{
			if (player->exiting < 6*TICRATE)
				player->kartstuff[k_cardanimation] += ((164-player->kartstuff[k_cardanimation])/8)+1;
			else if (player->exiting == 6*TICRATE)
				player->kartstuff[k_cardanimation] = 0;
			else if (player->kartstuff[k_cardanimation] < 2*TICRATE)
				player->kartstuff[k_cardanimation]++;
		}
		else
		{
			if (player->kartstuff[k_comebacktimer] < 6*TICRATE)
				player->kartstuff[k_cardanimation] -= ((164-player->kartstuff[k_cardanimation])/8)+1;
			else if (player->kartstuff[k_comebacktimer] < 9*TICRATE)
				player->kartstuff[k_cardanimation] += ((164-player->kartstuff[k_cardanimation])/8)+1;
		}

		if (player->kartstuff[k_cardanimation] > 164)
			player->kartstuff[k_cardanimation] = 164;
		if (player->kartstuff[k_cardanimation] < 0)
			player->kartstuff[k_cardanimation] = 0;
	}
	else if (G_RaceGametype() && player->exiting)
	{
		if (player->kartstuff[k_cardanimation] < 2*TICRATE)
			player->kartstuff[k_cardanimation]++;
	}
	else
	{
		player->kartstuff[k_cardanimation] = 0;
	}
}

UINT8 K_GetHudColor(void)
{
	if (cv_colorizedhud.value && cv_colorizedhudcolor.value)
		return (UINT8)cv_colorizedhudcolor.value;

	return ((stplyr && gamestate == GS_LEVEL) ? stplyr->skincolor : (UINT8)cv_playercolor.value);
}

boolean K_UseColorHud(void)
{
	return (cv_colorizedhud.value && clr_hud);
}

// Since all our extra Saturn things are in optional files
// we gotta do a bunch of extra checks to determine
// if the according Assets are actually loaded
// see D_CheckSaturnExtraFiles in d_main.c

enum
{
	SPEEDO_VANILLA,
	SPEEDO_EXTRA,
	SPEEDO_ACHII,
	SPEEDO_DIAL,
	SPEEDO_PMETER,
	SPEEDO_PMETERSMOL,
	SPEEDO_EXTRA3,
};

static SINT8 K_GetSpeedometerStyle(void)
{
	if (cv_newspeedometer.value == 2 && xtra_speedo)
		return SPEEDO_EXTRA;
	else if (cv_newspeedometer.value == 3 && achi_speedo)
		return SPEEDO_ACHII;
	else if (cv_newspeedometer.value == 4 && dial_speedo)
		return SPEEDO_DIAL;
	else if (cv_newspeedometer.value == 5 && kartz_speedo)
		return SPEEDO_PMETER;
	else if (cv_newspeedometer.value == 6 && kartz_speedo_smol)
		return SPEEDO_PMETERSMOL;
	else if (cv_newspeedometer.value == 7 && xtra_speedo3)
		return SPEEDO_EXTRA3;
	else
		return SPEEDO_VANILLA;
}

static boolean K_UseColorSpeedo(int speedostyle)
{
	if (!K_UseColorHud())
		return false;

	switch (speedostyle)
	{
		case SPEEDO_EXTRA:
			return xtra_speedo_clr;
		case SPEEDO_ACHII:
			return achi_speedo_clr;
		case SPEEDO_DIAL:
			return dial_speedo_clr;
		case SPEEDO_EXTRA3:
			return xtra_speedo_clr3;
		default:
			return false;
	}
}

enum
{
	GAUGE_DEFAULT = 1,
	GAUGE_SMALL,
	GAUGE_BIGNUM,
	GAUGE_NUMONLY,
	GAUGE_EXTRA,
};

static SINT8 K_GetDriftgaugeStyle(void)
{
	if (driftgaugegfx)
	{
		if (cv_driftgaugestyle.value == 1)
			return GAUGE_DEFAULT;
		else if (cv_driftgaugestyle.value == 2)
			return GAUGE_SMALL;
		else if (cv_driftgaugestyle.value == 3)
			return GAUGE_BIGNUM;
		else if (cv_driftgaugestyle.value == 5 && xtra_speedo3)
			return GAUGE_EXTRA;
	}

	// Fallback
	return GAUGE_NUMONLY; // (cv_driftgaugestyle.value == 4)
}

static boolean K_IsHighResolution(void)
{
	return (vid.width >= 640 && vid.height >= 400);
}

boolean K_UseHighResPortraits(void)
{
	return (cv_highresportrait.value && K_IsHighResolution());
}

// returns the players faceprefix patch
// accounts for localskins
patch_t *K_GetFacePrefix(player_t *player, INT32 skinnum)
{
	if (!player->skinlocal)
		return (K_UseHighResPortraits() ? facewantprefix[skinnum] : facerankprefix[skinnum]);
	else
		return (K_UseHighResPortraits() ? localfacewantprefix[skinnum] : localfacerankprefix[skinnum]);
}

INT32 K_calcSplitFlags(INT32 snapflags)
{
	INT32 splitflags = 0;

	if (splitscreen == 0)
		return snapflags;

	if (stplyrnum != 0)
	{
		if (splitscreen == 1 && stplyrnum == 1)
		{
			splitflags |= V_SPLITSCREEN;
		}
		else if (splitscreen > 1)
		{
			if (stplyrnum == 2 || (splitscreen == 3 && stplyrnum == 3))
				splitflags |= V_SPLITSCREEN;
			if (stplyrnum == 1 || (splitscreen == 3 && stplyrnum == 3))
				splitflags |= V_HORZSCREEN;
		}
	}

	if (splitflags & V_SPLITSCREEN)
		snapflags &= ~V_SNAPTOTOP;
	else
		snapflags &= ~V_SNAPTOBOTTOM;

	if (splitscreen > 1)
	{
		if (splitflags & V_HORZSCREEN)
			snapflags &= ~V_SNAPTOLEFT;
		else
			snapflags &= ~V_SNAPTORIGHT;
	}

	return (splitflags|snapflags);
}

void K_getItemBoxDrawinfo(drawinfo_t *out)
{
	INT32 fx, fy, fflags;

	// pain and suffering defined below
	if (splitscreen < 2) // don't change shit for THIS splitscreen.
	{
		fx = ITEM_X;
		fy = ITEM_Y;
		fflags = K_calcSplitFlags(V_SNAPTOTOP|V_SNAPTOLEFT);
	}
	else // now we're having a fun game.
	{
		if (!(stplyrnum & 1)) // If we are P1 or P3...
		{
			fx = ITEM_X;
			fy = ITEM_Y;
			fflags = V_SNAPTOLEFT|(stplyrnum & 2 ? V_SPLITSCREEN : V_SNAPTOTOP); // flip P3 to the bottom.
		}
		else // else, that means we're P2 or P4.
		{
			fx = ITEM2_X;
			fy = ITEM2_Y;
			fflags = V_SNAPTORIGHT|(stplyrnum & 2 ? V_SPLITSCREEN : V_SNAPTOTOP); // flip P4 to the bottom
		}
	}

	out->x = fx;
	out->y = fy;
	out->flags = fflags;
}

void K_getLapsDrawinfo(drawinfo_t *out)
{
	INT32 fx, fy, fflags;

	// pain and suffering defined below
	if (splitscreen < 2)	// don't change shit for THIS splitscreen.
	{
		fx = LAPS_X;
		fy = LAPS_Y;
		fflags = K_calcSplitFlags(V_SNAPTOBOTTOM|V_SNAPTOLEFT);
	}
	else
	{
		if (!(stplyrnum & 1))	// If we are P1 or P3...
		{
			fx = LAPS_X;
			fy = LAPS_Y;
			fflags = V_SNAPTOLEFT|(stplyrnum & 2 ? V_SPLITSCREEN|V_SNAPTOBOTTOM : 0);	// flip P3 to the bottom.
		}
		else // else, that means we're P2 or P4.
		{
			fx = LAPS2_X;
			fy = LAPS2_Y;
			fflags = V_SNAPTORIGHT|(stplyrnum & 2 ? V_SPLITSCREEN|V_SNAPTOBOTTOM : 0);	// flip P4 to the bottom
		}
	}

	out->x = fx;
	out->y = fy;
	out->flags = fflags;
}

void K_getMinimapDrawinfo(drawinfo_t *out)
{
	INT32 fx = MINI_X, fy = MINI_Y, fflags = (splitscreen == 3 ? 0 : V_SNAPTORIGHT);	// flags should only be 0 when it's centered (4p split)

	out->x = fx;
	out->y = fy;
	out->flags = fflags;
}

INT32 K_getMinimapTrans(void)
{
	INT32 minimaptrans = cv_kartminimap.value;

	if (!minimaptrans)
		return -1;

	if (forceshowhud)
		return (10-minimaptrans)<<FF_TRANSSHIFT;

	if (timeinmap <= 105)
		return -1;

	if (timeinmap <= 113)
		minimaptrans = ((((INT32)timeinmap) - 105)*minimaptrans)/(113-105);

	if (!minimaptrans)
		return -1;

	return (10-minimaptrans)<<FF_TRANSSHIFT;
}

patch_t *K_getItemBoxPatch(boolean small, boolean dark)
{
	UINT8 ofs = (cv_darkitembox.value && dark ? 1 : 0) + (small ? 2 : 0);
	return (cv_colorizeditembox.value && K_UseColorHud()) ? kp_itembgclr[ofs] : kp_itembg[ofs];
}

patch_t *K_getItemMulPatch(boolean small)
{
	UINT8 ofs = small ? 1 : 0;
	return K_UseColorHud() ? kp_itemmulstickerclr[ofs] : kp_itemmulsticker[ofs];
}

static void K_drawKartStats(void)
{
	INT32 x, y, spdxoffset, spdoffset, flags;

	// For 1-player display
	x = 15;
	y = 150;
	spdxoffset = 0;
	spdoffset = 0;

	if (!LUA_HudEnabled(hud_statdisplay))
		return;

	flags = V_SNAPTOBOTTOM|V_SNAPTOLEFT;

	//Internal offset for speedometer

	const UINT8 speedostyle = K_GetSpeedometerStyle();

	if (cv_kartspeedometer.value && (!splitscreen))
	{
		switch (speedostyle)
		{
			case SPEEDO_EXTRA:
			case SPEEDO_ACHII:
			case SPEEDO_EXTRA3:
				spdoffset = -10;
				break;
			case SPEEDO_DIAL:
				spdxoffset = 11;
				spdoffset = 14;
				break;
			default:
				spdoffset = -14;
				break;
		}
	}

	if (G_BattleGametype() && ((speedostyle != SPEEDO_DIAL) || splitscreen))
		spdoffset += ((stplyr->kartstuff[k_bumper] ? -5 : -8));

	// Customizations c:
	if (!splitscreen)
	{
		skin_t *fakeskin;
		INT32 flags2;

		x += 18 + cv_stat_xoffset.value + spdxoffset;
		y += cv_stat_yoffset.value + spdoffset;

		flags |= V_HUDTRANS;
		flags2 = flags|V_ALLOWLOWERCASE|V_SkinColorToHighlightcolor(stplyr->skincolor);

		fakeskin = K_GetPlayerSkin(stplyr);

		// Skin name
		if (cv_showstats_skinname.value)
		{
			if (K_IsHighResolution()) // V_DrawSmallString becomes a mess at low resolutions lel
			{
				V_DrawSmallString(x+20, y+12, flags2, fakeskin->realname);
			}
			else
			{
				V_DrawThinString(x+20, y+7, flags2, fakeskin->realname);
			}
		}

		// Icon and stats
		if (K_UseHighResPortraits())
			V_DrawSmallMappedPatch(x, y, flags, R_GetSkinFaceWant(stplyr), R_GetLocalTranslationColormap(fakeskin, fakeskin, stplyr->skincolor, GTC_CACHE, stplyr->skinlocal));
		else
			V_DrawMappedPatch(x, y, flags, R_GetSkinFaceRank(stplyr), R_GetLocalTranslationColormap(fakeskin, fakeskin, stplyr->skincolor, GTC_CACHE, stplyr->skinlocal));

		V_DrawMappedPatch(x-3, y-2, flags, kp_facenum[min(9, max(1, stplyr->kartspeed))], R_GetTranslationColormap(TC_RAINBOW, SKINCOLOR_BLUEBERRY, GTC_CACHE));
		V_DrawMappedPatch(x+10, y+10, flags, kp_facenum[min(9, max(1, stplyr->kartweight))], R_GetTranslationColormap(TC_RAINBOW, SKINCOLOR_BURGUNDY, GTC_CACHE));

		return;
	}

	if (splitscreen == 1) // I tried my best, but this is still mess :/ < :Blobcatpats: c:
	{
		x -= 10;
		y -= 40;
		y += spdoffset;

		// If we are in 2-player splitscreen, for player 1 we move hud to up and remove snapping
		// to bottom
		if (stplyrnum == 0)
		{
			y /= 2;
			flags = V_SNAPTOLEFT;
		}
		else
		{
			// Can move it down a bit
			y += 45;
		}
	}
	else
	{
		if (stplyrnum == 0 || stplyrnum == 2) // If we are P1 or P3...
		{
			// ye i just align it to position number lol
			x = POSI_X + 19;
			y = POSI_Y - 6;
			flags = V_SNAPTOLEFT|((stplyrnum == 2) ? V_SPLITSCREEN|V_SNAPTOBOTTOM : 0);	// flip P3 to the bottom.
		}
		else // else, that means we're P2 or P4.
		{
			x = POSI2_X - 75;
			y = POSI2_Y - 6;
			flags = V_SNAPTORIGHT|((stplyrnum == 3) ? V_SPLITSCREEN|V_SNAPTOBOTTOM : 0);	// flip P4 to the bottom
		}
	}

	flags |= V_HUDTRANS;

	// In splitscreen, just draw a string with stats
	V_DrawString(x, y+10, flags, va("\x84%dS \x87%dW", stplyr->kartspeed, stplyr->kartweight));
}

static void K_drawKartItem(void)
{
	// ITEM_X = BASEVIDWIDTH-50;	// 270
	// ITEM_Y = 24;					//  24

	// Why write V_DrawScaledPatch calls over and over when they're all the same?
	// Set to 'no item' just in case.
	const UINT8 offset = ((splitscreen > 1) ? 1 : 0);
	patch_t *localpatch = kp_nodraw;
	patch_t *localbg;
	boolean dark = false;

	patch_t *localinv = ((offset) ? kp_invincibility[((leveltime % (6*3)) / 3) + 7] : kp_invincibility[(leveltime % (7*3)) / 3]);
	INT32 fx = 0, fy = 0, fflags = 0;	// final coords for hud and flags...
	INT32 numberdisplaymin = cv_huditemamount.value == 2 ? 1 : 2; // No longer a constant so other things can modify this value
	INT32 itembar = 0;
	INT32 maxl = 0; // itembar's normal highest value
	const INT32 barlength = (splitscreen > 1 ? 12 : 26);
	UINT8 localcolor = SKINCOLOR_NONE;
	SINT8 colormode = TC_RAINBOW;
	UINT8 *colmap = NULL;
	UINT8 *colormap = NULL;

	if (stplyr->kartstuff[k_itemroulette])
	{
		localcolor = K_GetHudColor();

		switch ((stplyr->kartstuff[k_itemroulette] % (14*3)) / 3)
		{
			// Each case is handled in threes, to give three frames of in-game time to see the item on the roulette
			case 0: // Sneaker
				localpatch = kp_sneaker[offset];
				//localcolor = SKINCOLOR_RASPBERRY;
				break;
			case 1: // Banana
				localpatch = kp_banana[offset];
				//localcolor = SKINCOLOR_YELLOW;
				break;
			case 2: // Orbinaut
				localpatch = kp_orbinaut[3+offset];
				//localcolor = SKINCOLOR_STEEL;
				break;
			case 3: // Mine
				localpatch = kp_mine[offset];
				//localcolor = SKINCOLOR_JET;
				break;
			case 4: // Grow
				localpatch = kp_grow[offset];
				//localcolor = SKINCOLOR_TEAL;
				break;
			case 5: // Hyudoro
				localpatch = kp_hyudoro[offset];
				//localcolor = SKINCOLOR_STEEL;
				break;
			case 6: // Rocket Sneaker
				localpatch = kp_rocketsneaker[offset];
				//localcolor = SKINCOLOR_TANGERINE;
				break;
			case 7: // Jawz
				localpatch = kp_jawz[offset];
				//localcolor = SKINCOLOR_JAWZ;
				break;
			case 8: // Self-Propelled Bomb
				localpatch = kp_selfpropelledbomb[offset];
				//localcolor = SKINCOLOR_JET;
				break;
			case 9: // Shrink
				localpatch = kp_shrink[offset];
				//localcolor = SKINCOLOR_ORANGE;
				break;
			case 10: // Invincibility
				localpatch = localinv;
				//localcolor = SKINCOLOR_GREY;
				break;
			case 11: // Eggman Monitor
				localpatch = kp_eggman[offset];
				//localcolor = SKINCOLOR_ROSE;
				break;
			case 12: // Ballhog
				localpatch = kp_ballhog[offset];
				//localcolor = SKINCOLOR_LILAC;
				break;
			case 13: // Thunder Shield
				localpatch = kp_thundershield[offset];
				//localcolor = SKINCOLOR_CYAN;
				break;
			/*case 14: // Pogo Spring
				localpatch = kp_pogospring[offset];
				localcolor = SKINCOLOR_TANGERINE;
				break;
			case 15: // Kitchen Sink
				localpatch = kp_kitchensink[offset];
				localcolor = SKINCOLOR_STEEL;
				break;*/
			default:
				break;
		}
	}
	else
	{
		// I'm doing this a little weird and drawing mostly in reverse order
		// The only actual reason is to make sneakers line up this way in the code below
		// This shouldn't have any actual baring over how it functions
		// Hyudoro is first, because we're drawing it on top of the player's current item
		if (stplyr->kartstuff[k_stolentimer] > 0)
		{
			if (leveltime & 2)
				localpatch = kp_hyudoro[offset];
			else
				localpatch = kp_nodraw;
		}
		else if ((stplyr->kartstuff[k_stealingtimer] > 0) && (leveltime & 2))
		{
			localpatch = kp_hyudoro[offset];
		}
		else if (stplyr->kartstuff[k_eggmanexplode] > 1)
		{
			if (leveltime & 1)
				localpatch = kp_eggman[offset];
			else
				localpatch = kp_nodraw;
		}
		else if (stplyr->kartstuff[k_rocketsneakertimer] > 1)
		{
			itembar = stplyr->kartstuff[k_rocketsneakertimer];
			maxl = (itemtime*3) - barlength;

			if (leveltime & 1)
				localpatch = kp_rocketsneaker[offset];
			else
				localpatch = kp_nodraw;
		}
		else if (stplyr->kartstuff[k_growshrinktimer] > 0)
		{
			if (stplyr->kartstuff[k_growcancel] > 0)
			{
				itembar = stplyr->kartstuff[k_growcancel];
				maxl = 26;
			}

			if (leveltime & 1)
				localpatch = kp_grow[offset];
			else
				localpatch = kp_nodraw;
		}
		else if (stplyr->kartstuff[k_sadtimer] > 0)
		{
			if (leveltime & 2)
				localpatch = kp_sadface[offset];
			else
				localpatch = kp_nodraw;
		}
		else
		{
			if (stplyr->kartstuff[k_itemamount] <= 0)
				return;

			const boolean usemultiicon = (multiitem_icon && cv_multiitemicon.value && !offset);

			switch (stplyr->kartstuff[k_itemtype])
			{
				case KITEM_SNEAKER:
					if (usemultiicon)
					{
						if (!cv_huditemamount.value)
							numberdisplaymin = 4;
						switch(stplyr->kartstuff[k_itemamount])
						{
							case 1:
								localpatch = kp_sneaker[offset];
								break;
							case 2:
								localpatch = kp_multsneaker[0];
								break;
							default:
								localpatch = kp_multsneaker[1];
								break;
						}
					}
					else
					{
						localpatch = kp_sneaker[offset];
					}
					break;
				case KITEM_ROCKETSNEAKER:
					localpatch = kp_rocketsneaker[offset];
					break;
				case KITEM_INVINCIBILITY:
					localpatch = localinv;
					dark = true;
					break;
				case KITEM_BANANA:
					if (usemultiicon)
					{
						if (!cv_huditemamount.value)
							numberdisplaymin = 4;
						switch(stplyr->kartstuff[k_itemamount])
						{
							case 1:
								localpatch = kp_banana[offset];
								break;
							case 2:
								localpatch = kp_multbanana[0];
								break;
							case 3:
								localpatch = kp_multbanana[1];
								break;
							default:
								localpatch = kp_multbanana[2];
								break;
						}
					}
					else
					{
						localpatch = kp_banana[offset];
					}
					break;
				case KITEM_EGGMAN:
					localpatch = kp_eggman[offset];
					break;
				case KITEM_ORBINAUT:
					if (!cv_huditemamount.value)
						numberdisplaymin = offset ? 2 : 5;
					localpatch = kp_orbinaut[(offset ? 4 : min(stplyr->kartstuff[k_itemamount]-1, 3))];
					break;
				case KITEM_JAWZ:
					if (usemultiicon)
					{
						if (!cv_huditemamount.value)
							numberdisplaymin = 3;
						localpatch = ((stplyr->kartstuff[k_itemamount] == 1) ? kp_jawz[offset] : kp_multjawz);
					}
					else
					{
						localpatch = kp_jawz[offset];
					}
					break;
				case KITEM_MINE:
					localpatch = kp_mine[offset];
					break;
				case KITEM_BALLHOG:
					localpatch = kp_ballhog[offset];
					break;
				case KITEM_SPB:
					localpatch = kp_selfpropelledbomb[offset];
					dark = true;
					break;
				case KITEM_GROW:
					localpatch = kp_grow[offset];
					break;
				case KITEM_SHRINK:
					localpatch = kp_shrink[offset];
					break;
				case KITEM_THUNDERSHIELD:
					localpatch = kp_thundershield[offset];
					dark = true;
					break;
				case KITEM_HYUDORO:
					localpatch = kp_hyudoro[offset];
					break;
				case KITEM_POGOSPRING:
					localpatch = kp_pogospring[offset];
					break;
				case KITEM_KITCHENSINK:
					localpatch = kp_kitchensink[offset];
					break;
				case KITEM_SAD:
					localpatch = kp_sadface[offset];
					break;
				default:
					return;
			}

			if (stplyr->kartstuff[k_itemheld] && !(leveltime & 1))
				localpatch = kp_nodraw;
		}

		if (stplyr->kartstuff[k_itemblink] && (leveltime & 1))
		{
			colormode = TC_BLINK;

			switch (stplyr->kartstuff[k_itemblinkmode])
			{
				case 2:
					localcolor = K_RainbowColor();
					break;
				case 1:
					localcolor = SKINCOLOR_RED;
					break;
				default:
					localcolor = SKINCOLOR_WHITE;
					break;
			}
		}
	}

	localbg = K_getItemBoxPatch((boolean)offset, dark);
	drawinfo_t info;
	K_getItemBoxDrawinfo(&info);
	fx = info.x;
	fy = info.y;
	fflags = info.flags;

	boolean flipamount = splitscreen > 1 && stplyrnum & 1;	// Used for 3P/4P splitscreen to flip item amount stuff

	if (K_UseColorHud())
		colormap = R_GetTranslationColormap(TC_DEFAULT, K_GetHudColor(), GTC_CACHE);

	V_DrawMappedPatch(fx, fy, V_HUDTRANS|fflags, localbg, cv_colorizeditembox.value ? colormap : NULL);

	fixed_t rfy = fy<<FRACBITS;
	INT32 fancyflags = V_HUDTRANS|fflags;

	if (cv_fancyroulette.value && stplyr->kartstuff[k_itemroulette] && !stplyr->deadtimer)
	{
		fixed_t frac = R_GetTimeFrac(RTF_LEVEL);
		UINT8   fancystep = (offset ? 6 : 10);
		fixed_t fancyoffset = (stplyr->kartstuff[k_itemroulette] % 3)-1;

		if (fancyoffset != 0)
		{
			fancyflags &= ~V_HUDTRANS;
			fancyflags |=V_HUDTRANSHALF;
		}

		rfy += (fancystep * fancyoffset * FRACUNIT) + FixedMul(fancystep*FRACUNIT, frac) - fancystep/2*FRACUNIT;
	}

	if (localcolor != SKINCOLOR_NONE)
		colmap = R_GetTranslationColormap(colormode, localcolor, GTC_CACHE);

	// Then, the numbers:
	if (stplyr->kartstuff[k_itemamount] >= numberdisplaymin && !stplyr->kartstuff[k_itemroulette])
	{
		localbg = K_getItemMulPatch((boolean)offset);

		V_DrawMappedPatch(fx + (flipamount ? 48 : 0), fy, V_HUDTRANS|fflags|(flipamount ? V_FLIP : 0), localbg, colormap); // flip this graphic for p2 and p4 in split and shift it.
		V_DrawFixedPatch(fx<<FRACBITS, fy<<FRACBITS, FRACUNIT, V_HUDTRANS|fflags, localpatch, colmap);

		if (offset)
		{
			const INT32 xofs = flipamount ? 2 : 24; // reminder that this is for 3/4p's right end of the screen.
			V_DrawString(fx+xofs, fy+31, V_ALLOWLOWERCASE|V_HUDTRANS|fflags, va("x%d", stplyr->kartstuff[k_itemamount]));
		}
		else
		{
			V_DrawScaledPatch(fx+28, fy+41, V_HUDTRANS|fflags, kp_itemx);
			V_DrawKartString(fx+38, fy+36, V_HUDTRANS|fflags, va("%d", stplyr->kartstuff[k_itemamount]));
		}
	}
	else
		V_DrawFixedPatch(fx<<FRACBITS, rfy, FRACUNIT, fancyflags, localpatch, colmap);

	// Extensible meter, currently only used for rocket sneaker...
	if (itembar && hudtrans)
	{
		const INT32 fill = ((itembar*barlength)/maxl);
		const INT32 length = min(barlength, fill);
		const INT32 height = (offset ? 1 : 2);
		const INT32 x = (offset ? 17 : 11), y = (offset ? 27 : 35);

		V_DrawScaledPatch(fx+x, fy+y, V_HUDTRANS|fflags, kp_itemtimer[offset]);
		// The left dark "AA" edge
		V_DrawFill(fx+x+1, fy+y+1, (length == 2 ? 2 : 1), height, 12|fflags);
		// The bar itself
		if (length > 2)
		{
			V_DrawFill(fx+x+length, fy+y+1, 1, height, 12|fflags); // the right one
			if (height == 2)
				V_DrawFill(fx+x+2, fy+y+2, length-2, 1, 8|fflags); // the dulled underside
			V_DrawFill(fx+x+2, fy+y+1, length-2, 1, 120|fflags); // the shine
		}
	}

	// Quick Eggman numbers
	if (stplyr->kartstuff[k_eggmanexplode] > 1 /*&& stplyr->kartstuff[k_eggmanexplode] <= 3*TICRATE*/)
		V_DrawScaledPatch(fx+17, fy+13-offset, V_HUDTRANS|fflags, kp_eggnum[min(3, G_TicsToSeconds(stplyr->kartstuff[k_eggmanexplode]))]);
}

void K_drawKartTimestamp(tic_t drawtime, INT32 TX, INT32 TY, INT16 emblemmap, UINT8 mode)
{
	// TIME_X = BASEVIDWIDTH-124;	// 196
	// TIME_Y = 6;					//   6

	INT32 splitflags = 0;

	if (!mode)
	{
		splitflags = V_HUDTRANS|K_calcSplitFlags(V_SNAPTOTOP|V_SNAPTORIGHT);

		if (cv_timelimit.value && timelimitintics > 0)
		{
			if (drawtime >= timelimitintics)
				drawtime = 0;
			else
				drawtime = timelimitintics - drawtime;
		}
	}

	if (K_UseColorHud()) // Colourized hud
	{
		UINT8 *colormap = R_GetTranslationColormap(TC_DEFAULT, K_GetHudColor(), GTC_CACHE);
		V_DrawMappedPatch(TX, TY, splitflags, ((mode == 2) ? kp_lapstickerwideclr : kp_timestickerwideclr), colormap);
	}
	else
		V_DrawScaledPatch(TX, TY, splitflags, ((mode == 2) ? kp_lapstickerwide : kp_timestickerwide));

	TX += 33;

	if (drawtime == UINT32_MAX)
		;
	else if (mode && !drawtime)
	{
		V_DrawKartString(TX, TY+3, splitflags, va("--'--\"--"));
	}
	else
	{
		tic_t worktime = drawtime/(60*TICRATE);

		if (cv_timelimit.value && cv_overtime.value && G_BattleGametype() && !players[consoleplayer].exiting && (leveltime > (timelimitintics + starttime + TICRATE/2))) // i hate this so much
		{
			V_DrawKartString(TX, TY+3, splitflags, va("OVERTIME"));
		}
		else if (worktime < 100)
		{
			// minutes time      00 __ __
			V_DrawKartString(TX,    TY+3, splitflags, va("%d", worktime/10));
			V_DrawKartString(TX+12, TY+3, splitflags, va("%d", worktime%10));

			// apostrophe location     _'__ __
			V_DrawKartString(TX+24, TY+3, splitflags, va("'"));

			worktime = (drawtime/TICRATE % 60);

			// seconds time       _ 00 __
			V_DrawKartString(TX+36, TY+3, splitflags, va("%d", worktime/10));
			V_DrawKartString(TX+48, TY+3, splitflags, va("%d", worktime%10));

			// quotation mark location    _ __"__
			V_DrawKartString(TX+60, TY+3, splitflags, va("\""));

			worktime = G_TicsToCentiseconds(drawtime);

			// tics               _ __ 00
			V_DrawKartString(TX+72, TY+3, splitflags, va("%d", worktime/10));
			V_DrawKartString(TX+84, TY+3, splitflags, va("%d", worktime%10));
		}
		else if ((drawtime/TICRATE) & 1)
			V_DrawKartString(TX, TY+3, splitflags, va("99'59\"99"));
	}

	if (emblemmap && (modeattacking || (mode == 1)) && !demo.playback) // emblem time!
	{
		INT32 workx = TX + 96, worky = TY+18;
		SINT8 curemb = 0;
		patch_t *emblempic[3] = {NULL, NULL, NULL};
		UINT8 *emblemcol[3] = {NULL, NULL, NULL};

		emblem_t *emblem = M_GetLevelEmblems(emblemmap);
		while (emblem)
		{
			char targettext[9];

			switch (emblem->type)
			{
				case ET_TIME:
					{
						static boolean canplaysound = true;
						tic_t timetoreach = emblem->var;

						if (emblem->collected)
						{
							emblempic[curemb] = W_CachePatchName(M_GetEmblemPatch(emblem), PU_PATCH);
							emblemcol[curemb] = R_GetTranslationColormap(TC_DEFAULT, M_GetEmblemColor(emblem), GTC_CACHE);
							if (++curemb == 3)
								break;
							goto bademblem;
						}

						snprintf(targettext, 9, "%i'%02i\"%02i",
							G_TicsToMinutes(timetoreach, false),
							G_TicsToSeconds(timetoreach),
							G_TicsToCentiseconds(timetoreach));

						if (!mode)
						{
							if (stplyr->realtime > timetoreach)
							{
								splitflags = (splitflags &~ V_HUDTRANS)|V_HUDTRANSHALF;
								if (canplaysound)
								{
									S_StartSound(NULL, sfx_s3k72); //sfx_s26d); -- you STOLE fizzy lifting drinks
									canplaysound = false;
								}
							}
							else if (!canplaysound)
								canplaysound = true;
						}

						targettext[8] = 0;
					}
					break;
				default:
					goto bademblem;
			}

			V_DrawRightAlignedString(workx, worky, splitflags, targettext);
			workx -= 67;
			V_DrawSmallScaledPatch(workx + 4, worky, splitflags, W_CachePatchName("NEEDIT", PU_PATCH));

			break;

			bademblem:
			emblem = M_GetLevelEmblems(-1);
		}

		if (!mode)
			splitflags = (splitflags &~ V_HUDTRANSHALF)|V_HUDTRANS;

		while (curemb--)
		{
			workx -= 12;
			V_DrawSmallMappedPatch(workx + 4, worky, splitflags, emblempic[curemb], emblemcol[curemb]);
		}
	}
}

#define POS_DELAY_TIME 10

static void K_DrawKartPositionNum(INT32 num)
{
	// POSI_X = BASEVIDWIDTH - 51;	// 269
	// POSI_Y = BASEVIDHEIGHT- 64;	// 136

	const boolean wheeloffs = (cv_showinput.value && cv_posi_xoffset.value == 0 && cv_posi_yoffset.value == 0 && cv_wheel_xoffset.value == 0 && cv_wheel_yoffset.value == 0);
	const boolean win = (stplyr->exiting && num == 1);
	INT32 W = kp_positionnum[0][0]->width;
	fixed_t scale = FRACUNIT;
	patch_t *localpatch = kp_positionnum[0][0];
	INT32 fx = 0, fy = 0, fflags = 0;
	const INT32 xoffs = wheeloffs ? -48 : 0;
	boolean flipdraw  = false; // flip the order we draw it in for MORE splitscreen bs. fun.
	boolean flipvdraw = false; // used only for 2p splitscreen so overtaking doesn't make 1P's position fly off the screen.
	boolean overtake  = false;

	if ((cv_posanim.value && stplyr->kartstuff[k_positiondelay]) || stplyr->exiting)
	{
		if (cv_posanim.value == 2)
		{
			const UINT8 delay = (stplyr->exiting) ? POS_DELAY_TIME : stplyr->kartstuff[k_positiondelay];
			const fixed_t add = (scale * 3) >> ((splitscreen == 1) ? 1 : 2);
			scale = (scale + min((add * (delay * delay)) / (POS_DELAY_TIME * POS_DELAY_TIME), add));
		}
		else
			scale *= 2;

		overtake = true;	// this is used for splitscreen stuff in conjunction with flipdraw.
	}

	if (splitscreen || cv_smallposnum.value || wheeloffs)
	{
		scale /= 2;
	}

	W = FixedMul(W<<FRACBITS, scale)>>FRACBITS;

	// pain and suffering defined below
	if (!splitscreen)
	{
		fx = POSI_X + xoffs;
		fy = BASEVIDHEIGHT - 8 + cv_posi_yoffset.value;
		fflags = V_SNAPTOBOTTOM|V_SNAPTORIGHT;
	}
	else if (splitscreen == 1)	// for this splitscreen, we'll use case by case because it's a bit different.
	{
		fx = POSI_X;
		if (stplyrnum == 0)	// for player 1: display this at the top right, above the minimap.
		{
			fy = 30 + cv_posi_yoffset.value;
			fflags = V_SNAPTOTOP|V_SNAPTORIGHT;
			if (overtake)
				flipvdraw = true;	// make sure overtaking doesn't explode us
		}
		else	// if we're not p1, that means we're p2. display this at the bottom right, below the minimap.
		{
			fy = BASEVIDHEIGHT - 8 + cv_posi_yoffset.value;
			fflags = V_SNAPTOBOTTOM|V_SNAPTORIGHT;
		}
	}
	else
	{
		if (!(stplyrnum & 1)) // If we are P1 or P3...
		{
			fx = POSI_X;
			fy = POSI_Y;
			fflags = V_SNAPTOLEFT|((stplyrnum == 2) ? V_SPLITSCREEN|V_SNAPTOBOTTOM : 0);	// flip P3 to the bottom.
			flipdraw = true;
			if (num >= 10)
				fx += W;	// this seems dumb, but we need to do this in order for positions above 10 going off screen.
		}
		else // else, that means we're P2 or P4.
		{
			fx = POSI2_X;
			fy = POSI2_Y;
			fflags = V_SNAPTORIGHT|((stplyrnum == 3) ? V_SPLITSCREEN|V_SNAPTOBOTTOM : 0);	// flip P4 to the bottom
		}
	}

	// Special case for 0
	if (!num)
	{
		V_DrawFixedPatch(fx<<FRACBITS, fy<<FRACBITS, scale, V_HUDTRANSHALF|fflags, kp_positionnum[0][0], NULL);
		return;
	}

	I_Assert(num >= 0); // This function does not draw negative numbers

	// Draw the number
	while (num)
	{
		if (win) // 1st place winner? You get rainbows!!
		{
			localpatch = kp_winnernum[(leveltime % (NUMWINFRAMES*3)) / 3];
		}
		else if (stplyr->laps+1 >= cv_numlaps.value || stplyr->exiting) // Check for the final lap, or won
		{
			// Alternate frame every three frames
			switch (leveltime % 9)
			{
				case 1: case 2: case 3:
					if (K_IsPlayerLosing(stplyr))
						localpatch = kp_positionnum[num % 10][4];
					else
						localpatch = kp_positionnum[num % 10][1];
					break;
				case 4: case 5: case 6:
					if (K_IsPlayerLosing(stplyr))
						localpatch = kp_positionnum[num % 10][5];
					else
						localpatch = kp_positionnum[num % 10][2];
					break;
				case 7: case 8: case 9:
					if (K_IsPlayerLosing(stplyr))
						localpatch = kp_positionnum[num % 10][6];
					else
						localpatch = kp_positionnum[num % 10][3];
					break;
				default:
					localpatch = kp_positionnum[num % 10][0];
					break;
			}
		}
		else
		{
			localpatch = kp_positionnum[num % 10][0];
		}

		V_DrawFixedPatch((fx<<FRACBITS) + ((overtake && flipdraw) ? (localpatch->width*scale/2) : 0), (fy<<FRACBITS) + ((overtake && flipvdraw) ? (localpatch->height*scale/2) : 0), scale, V_HUDTRANSHALF|fflags, localpatch, NULL);
		// ^ if we overtake as p1 or p3 in splitscren, we shift it so that it doesn't go off screen.
		// ^ if we overtake as p1 in 2p splits, shift vertically so that this doesn't happen either.

		fx -= W;
		num /= 10;
	}
}

static boolean K_drawKartPositionFaces(void)
{
	// FACE_X = 15;				//  15
	// FACE_Y = 72;				//  72

	INT32 Y = FACE_Y+9; // +9 to offset where it's being drawn if there are more than one
	INT32 i, j, ranklines, strank = -1;
	boolean completed[MAXPLAYERS];
	INT32 rankplayer[MAXPLAYERS];
	INT32 bumperx, numplayersingame = 0;
	UINT8 *colormap;

	ranklines = 0;
	memset(completed, 0, sizeof (completed));
	memset(rankplayer, 0, sizeof (rankplayer));

	for (i = 0; i < MAXPLAYERS; i++)
	{
		rankplayer[i] = -1;

		if (!playeringame[i] || players[i].spectator || !players[i].mo)
			continue;

		numplayersingame++;
	}

	if (numplayersingame <= 1)
		return true;

	if (!LUA_HudEnabled(hud_minirankings))
		return false; // Don't proceed but still return true for free play above if HUD is disabled.

	for (j = 0; j < numplayersingame; j++)
	{
		UINT8 lowestposition = MAXPLAYERS+1;
		for (i = 0; i < MAXPLAYERS; i++)
		{
			if (completed[i] || !playeringame[i] || players[i].spectator || !players[i].mo)
				continue;

			if (players[i].kartstuff[k_position] >= lowestposition)
				continue;

			rankplayer[ranklines] = i;
			lowestposition = players[i].kartstuff[k_position];
		}

		i = rankplayer[ranklines];

		completed[i] = true;

		if (players+i == stplyr)
			strank = ranklines;

		ranklines++;
	}

	if (ranklines < 5)
		Y -= (9*ranklines);
	else
		Y -= (9*5);

	if (G_BattleGametype() || strank <= 2) // too close to the top, or playing battle, or a spectator? would have had (strank == -1) called out, but already caught by (strank <= 2)
	{
		i = 0;
		if (ranklines > 5) // could be both...
			ranklines = 5;
	}
	else if (strank+3 > ranklines) // too close to the bottom?
	{
		i = ranklines - 5;
		if (i < 0)
			i = 0;
	}
	else
	{
		i = strank-2;
		ranklines = strank+3;
	}

	for (; i < ranklines; i++)
	{
		player_t *player;
		player = &players[rankplayer[i]];

		if (!playeringame[rankplayer[i]])
			continue;
		if (player->spectator || !player->mo)
			continue;

		const UINT8 mocolor = player->mo->color;

		bumperx = FACE_X+19;

		if (mocolor)
		{
			if (player->mo->colorized)
				colormap = R_GetTranslationColormap(TC_RAINBOW, mocolor, GTC_CACHE);
			else
				colormap = R_GetTranslationColormap(player->skin, mocolor, GTC_CACHE);

			if (K_UseHighResPortraits())
				V_DrawSmallMappedPatch(FACE_X, Y, V_HUDTRANS|V_SNAPTOLEFT, R_GetSkinFaceWant(player), colormap);
			else
				V_DrawMappedPatch(FACE_X, Y, V_HUDTRANS|V_SNAPTOLEFT, R_GetSkinFaceRank(player), colormap);

			if (LUA_HudEnabled(hud_battlebumpers))
			{
				if (G_BattleGametype() && player->kartstuff[k_bumper] > 0)
				{
					V_DrawMappedPatch(bumperx-2, Y, V_HUDTRANS|V_SNAPTOLEFT, kp_tinybumper[0], colormap);
					for (j = 1; j < player->kartstuff[k_bumper]; j++)
					{
						bumperx += 5;
						V_DrawMappedPatch(bumperx, Y, V_HUDTRANS|V_SNAPTOLEFT, kp_tinybumper[1], colormap);
					}
				}
			}	// A new level of stupidity: checking if lua is enabled to close a bracket. :Fascinating:
		}

		if (i == strank)
			V_DrawScaledPatch(FACE_X, Y, V_HUDTRANS|V_SNAPTOLEFT, kp_facehighlight[(leveltime / 4) % 8]);

		if (G_BattleGametype() && player->kartstuff[k_bumper] <= 0)
			V_DrawScaledPatch(FACE_X-4, Y-3, V_HUDTRANS|V_SNAPTOLEFT, kp_ranknobumpers);
		else
		{
			INT32 pos = player->kartstuff[k_position];

			if (pos < 0 || pos > MAXPLAYERS)
				pos = 0;

			// Draws the little number over the face
			V_DrawScaledPatch(FACE_X-5, Y+10, V_HUDTRANS|V_SNAPTOLEFT, kp_facenum[pos]);
		}

		Y += 18;
	}

	return false;
}

//
// HU_DrawTabRankings -- moved here to take advantage of kart stuff!
//
void HU_DrawTabRankings(INT32 x, INT32 y, playersort_t *tab, INT32 scorelines, INT32 hilicol)
{
	INT32 i, rightoffset = 240;
	const UINT8 *colormap;
	const INT32 dupadjust = cv_betainterscreen.value ? 314 : vid.scaledwidth, duptweak = cv_betainterscreen.value ? -3 : (dupadjust - BASEVIDWIDTH)/2;

	boolean (*_isHighlightedPlayer)(const player_t *) = (demo.playback ? P_IsDisplayPlayer : P_IsLocalPlayer);

	//this function is designed for 9 or less score lines only
	//I_Assert(scorelines <= 9); -- not today bitch, kart fixed it up

	V_DrawFill(1-duptweak, 26, dupadjust-2, 1, 0); // Draw a horizontal line because it looks nice!

	if (scorelines > 8)
	{
		V_DrawFill(160, 26, 1, 147, 0); // Draw a vertical line to separate the two sides.
		V_DrawFill(1-duptweak, 173, dupadjust-2, 1, 0); // And a horizontal line near the bottom.
		rightoffset = (BASEVIDWIDTH/2) - 4 - x;
	}

	for (i = 0; i < scorelines; i++)
	{
		char strtime[MAXPLAYERNAME+1];
		const UINT8 pnum = tab[i].num;
		player_t *player = &players[pnum];

		if (player->spectator || !player->mo)
			continue; //ignore them.

		const boolean whiteplayer = _isHighlightedPlayer(player);
		const INT32 philicol = (whiteplayer ? V_SkinColorToHighlightcolor(player->skincolor) : 0);

		if ((netgame && pnum != serverplayer) || (cv_mindelay.value && P_IsLocalPlayer(player)))
		{
			HU_drawPlayerPing(x + ((i < 8) ? -17 : rightoffset + 11), y-4, pnum, 0);
		}

		STRBUFCPY(strtime, tab[i].name);

		if (scorelines > 8)
			V_DrawThinString(x + 20, y, philicol|V_ALLOWLOWERCASE|V_6WIDTHSPACE, strtime);
		else
			V_DrawString(x + 20, y, philicol|V_ALLOWLOWERCASE, strtime);

		if (player->mo->color)
		{
			if (player->mo->colorized)
				colormap = R_GetTranslationColormap(TC_RAINBOW, player->mo->color, GTC_CACHE);
			else
				colormap = R_GetTranslationColormap(player->skin, player->mo->color, GTC_CACHE);

			if (K_UseHighResPortraits())
				V_DrawSmallMappedPatch(x, y-4, 0, R_GetSkinFaceWant(player), colormap);
			else
				V_DrawMappedPatch(x, y-4, 0, R_GetSkinFaceRank(player), colormap);

			/*if (G_BattleGametype() && player->kartstuff[k_bumper] > 0) -- not enough space for this
			{
				INT32 bumperx = x+19;
				V_DrawMappedPatch(bumperx-2, y-4, 0, kp_tinybumper[0], colormap);
				for (j = 1; j < player->kartstuff[k_bumper]; j++)
				{
					bumperx += 5;
					V_DrawMappedPatch(bumperx, y-4, 0, kp_tinybumper[1], colormap);
				}
			}*/
		}

		if (whiteplayer)
			V_DrawScaledPatch(x, y-4, 0, kp_facehighlight[(leveltime / 4) % 8]);

		if (G_BattleGametype() && player->kartstuff[k_bumper] <= 0)
			V_DrawScaledPatch(x-4, y-7, 0, kp_ranknobumpers);
		else
		{
			INT32 pos = player->kartstuff[k_position];
			if (pos < 0 || pos > MAXPLAYERS)
				pos = 0;
			// Draws the little number over the face
			V_DrawScaledPatch(x-5, y+6, 0, kp_facenum[pos]);
		}

		if (G_RaceGametype())
		{
#define timestring(time) va("%i'%02i\"%02i", G_TicsToMinutes(time, true), G_TicsToSeconds(time), G_TicsToCentiseconds(time))
			if (scorelines > 8)
			{
				if (player->exiting)
					V_DrawRightAlignedThinString(x+rightoffset, y-1, hilicol|V_6WIDTHSPACE, timestring(player->realtime));
				else if (player->pflags & PF_TIMEOVER)
					V_DrawRightAlignedThinString(x+rightoffset, y-1, V_6WIDTHSPACE, "NO CONTEST.");
				else if (circuitmap)
					V_DrawRightAlignedThinString(x+rightoffset, y-1, V_6WIDTHSPACE, va("Lap %d", tab[i].count));
			}
			else
			{
				if (player->exiting)
					V_DrawRightAlignedString(x+rightoffset, y, hilicol, timestring(player->realtime));
				else if (player->pflags & PF_TIMEOVER)
					V_DrawRightAlignedThinString(x+rightoffset, y-1, 0, "NO CONTEST.");
				else if (circuitmap)
					V_DrawRightAlignedString(x+rightoffset, y, 0, va("Lap %d", tab[i].count));
			}
#undef timestring
		}
		else
			V_DrawRightAlignedString(x+rightoffset, y, 0, va("%u", tab[i].count));

		y += 18;
		if (i == 7)
		{
			y = 33;
			x = (BASEVIDWIDTH/2) + 4;
		}
	}
}

static boolean K_BigLap(void)
{
	return (cv_biglaps.value && (cv_numlaps.value > 9) && (K_UseColorHud() ? big_lap_color : big_lap) && (!stplyr->exiting));
}

static void K_drawKartLaps(void)
{
	const INT32 splitflags = K_calcSplitFlags(V_SNAPTOBOTTOM|V_SNAPTOLEFT);
	INT32 fx = 0, fy = 0, fflags = 0;	// stuff for 3p / 4p splitscreen.
	const boolean flipstring = splitscreen > 1 && stplyrnum & 1;  // used for 3p or 4p
	INT32 stringw = 0;	// used with the above
	const char *laps;

	drawinfo_t info;
	K_getLapsDrawinfo(&info);
	fx = info.x;
	fy = info.y;
	fflags = info.flags;

	laps = (stplyr->exiting ? "FIN" : va("%d/%d", stplyr->laps+1, cv_numlaps.value));

	// draw stuff as god intended.
	if (splitscreen > 1)
	{
		if (flipstring)
		{
			stringw = V_StringWidth(laps, 0);

			V_DrawScaledPatch(fx-stringw-13, fy, V_HUDTRANS|fflags, kp_splitlapflag);
			V_DrawRightAlignedString(fx, fy+1, V_HUDTRANS|fflags, laps);
		}
		else	// draw stuff NORMALLY.
		{
			V_DrawScaledPatch(fx, fy, V_HUDTRANS|fflags, kp_splitlapflag);
			V_DrawString(fx+13, fy+1, V_HUDTRANS|fflags, laps);
		}
	}
	else
	{
		if (K_UseColorHud()) // Colourized hud
		{
			UINT8 *colormap = R_GetTranslationColormap(TC_DEFAULT, K_GetHudColor(), GTC_CACHE);
			patch_t *biglapclr = (K_BigLap() ? ((stplyr->laps + 1 > 9) ? kp_lapstickerbig2clr : kp_lapstickerbigclr) : kp_lapstickerclr);
			V_DrawMappedPatch(fx, fy, V_HUDTRANS|splitflags, biglapclr, colormap);
		}
		else
		{
			patch_t *biglap = (K_BigLap() ? ((stplyr->laps + 1 > 9) ? kp_lapstickerbig2 : kp_lapstickerbig) : kp_lapsticker);
			V_DrawScaledPatch(fx, fy, V_HUDTRANS|splitflags, biglap);
		}

		V_DrawKartString(fx+33, fy+3, V_HUDTRANS|splitflags, laps);
	}
}

#ifdef ROTSPRITE

#define DIALSPDDIV 97090 // 1.48148; converts 200 to 135
#define MPHDIV 68283 // 1.04192; converts 141 to 135

static void K_DrawDialNum(INT32 x, INT32 y, boolean colorized, INT32 flags, INT32 num, INT32 digits, const UINT8 *colormap)
{
	INT32 w;

	if (colorized)
		w = skp_dialnumclr[0]->width;
	else
		w = skp_dialnum[0]->width;

	if (flags & V_NOSCALESTART)
		w *= vid.dup;

	if (num < 0)
		num = -num;

	// draw the number
	do
	{
		x -= (w);

		if (colorized)
			V_DrawFixedPatch(x << FRACBITS, y << FRACBITS, FRACUNIT, flags, skp_dialnumclr[num % 10], colormap);
		else
			V_DrawFixedPatch(x << FRACBITS, y << FRACBITS, FRACUNIT, flags, skp_dialnum[num % 10], colormap);

		num /= 10;
	} while (--digits);
}

static void K_DrawDialLaps(INT32 x, INT32 y, INT32 num, INT32 total, INT32 flags)
{
	INT32 fx;
	fx = x + ((num < 10) ? 0 : 6);
	V_DrawRankNum(fx, y, flags, num, (num < 10) ? 1 : 2, NULL);
	V_DrawScaledPatch(fx + 2, y, flags, frameslash);
	V_DrawRankNum(fx + 13 + ((total < 10) ? 0 : 6), y, flags, total, (total < 10) ? 1 : 2, NULL);
}

static void K_DrawDialSpeedometer(fixed_t speed,
								  fixed_t divisor,
								  INT32 splitflags,
								  boolean battlemode,
								  boolean infoactive,
								  boolean colorized)
{
	const UINT8* colormap = R_GetTranslationColormap(TC_DEFAULT, K_GetHudColor(), GTC_CACHE);

	INT32 rot = 0;
	const UINT8 infoidx = (infoactive) ? 1 : 0;
	patch_t* dialpatch;

	const fixed_t spd = FixedDiv(speed, divisor);
	const angle_t speedangle =
		FixedAngle(((min(135 * FRACUNIT, spd) - (45 * FRACUNIT))));

	rot = R_GetRollAngle(speedangle);

	if (rot)
	{
		dialpatch = W_CachePatchNameRotated("K_DSDIAL", rot, PU_PATCH);
	}
	else
	{
		dialpatch = W_CachePatchName("K_DSDIAL", PU_PATCH);
	}

	if (colorized)  // Colourized hud
	{
		V_DrawMappedPatch(
			SPDM_X, SPDM_Y - 9, (V_HUDTRANS | splitflags), skp_dialbaseclr[infoidx], colormap);
		V_DrawMappedPatch(SPDM_X,
						  SPDM_Y + 30,
						  V_HUDTRANS | splitflags,
						  skp_speedpatchesdialclr[cv_kartspeedometer.value],
						  colormap);
	}
	else
	{
		V_DrawScaledPatch(SPDM_X, SPDM_Y - 9, (V_HUDTRANS | splitflags), skp_dialbase[infoidx]);
		V_DrawScaledPatch(SPDM_X,
						  SPDM_Y + 30,
						  V_HUDTRANS | splitflags,
						  skp_speedpatchesdial[cv_kartspeedometer.value]);
	}

	K_DrawDialNum(SPDM_X + 10,
				  SPDM_Y + 25,
				  colorized,
				  V_HUDTRANS | splitflags,
				  speed / FRACUNIT,
				  3,
				  colormap);

	// gotta center the dial manually
	V_DrawMappedPatch(SPDM_X - 19, SPDM_Y + 15, (V_HUDTRANS | splitflags), (dialpatch), colormap);

	if (!infoactive)
	{
		// no need to draw info if we're not supposed to
		return;
	}

	// draw the info
	if (battlemode)
	{
		if (stplyr->kartstuff[k_bumper] <= 0)
		{
			V_DrawMappedPatch(
				SPDM_X + 28, SPDM_Y + 27, V_HUDTRANS | splitflags, kp_splitkarmabomb, colormap);
			K_DrawDialLaps(SPDM_X + 46,
						   SPDM_Y + 29,
						   stplyr->kartstuff[k_comebackpoints],
						   2,
						   V_HUDTRANS | splitflags);
		}
		else  // the above doesn't need to account for weird stuff since the max amount of karma
			  // necessary is always 2 ^^^^
		{
			V_DrawMappedPatch(
				SPDM_X + 28, SPDM_Y + 27, V_HUDTRANS | splitflags, kp_rankbumper, colormap);
			K_DrawDialLaps(SPDM_X + 46,
						   SPDM_Y + 29,
						   stplyr->kartstuff[k_bumper],
						   cv_kartbumpers.value,
						   V_HUDTRANS | splitflags);
		}
	}
	else
	{
		V_DrawScaledPatch(SPDM_X + 28, SPDM_Y + 27, V_HUDTRANS | splitflags, kp_splitlapflag);

		if (stplyr->exiting)
			V_DrawScaledPatch(SPDM_X + 39, SPDM_Y + 29, V_HUDTRANS | splitflags, skp_rankfinish);
		else
			K_DrawDialLaps(SPDM_X + 46,
						   SPDM_Y + 29,
						   stplyr->laps + 1,
						   cv_numlaps.value,
						   V_HUDTRANS | splitflags);
	}
}

#endif

static void K_drawKartSpeedometer(void)
{
	// why?
	if (cv_kartspeedometer.value == 0)
		return;

	// index 0 is the raw value, index 1 is the converted value
	fixed_t convSpeed[2] = {0,0};
#ifdef ROTSPRITE
	fixed_t dial_divisor = DIALSPDDIV;
#endif

	INT32 splitflags = K_calcSplitFlags(V_SNAPTOBOTTOM|V_SNAPTOLEFT);

	// man.
	const UINT8 speedostyle = K_GetSpeedometerStyle();

	switch (cv_kartspeedometer.value)
	{
		case 1:
			convSpeed[0] = FixedDiv(FixedMul(stplyr->speed, 142371), mapobjectscale); // 2.172409058
			convSpeed[1] = convSpeed[0] / FRACUNIT;
			break;
		case 2:
			convSpeed[0] = FixedDiv(FixedMul(stplyr->speed, 88465), mapobjectscale); // 1.349868774
			convSpeed[1] = convSpeed[0] / FRACUNIT;
#ifdef ROTSPRITE
			dial_divisor = MPHDIV;
#endif
			break;
		case 3:
			convSpeed[0] = FixedDiv(stplyr->speed, mapobjectscale);
			convSpeed[1] = convSpeed[0] / FRACUNIT;
#ifdef ROTSPRITE
			dial_divisor = MPHDIV;
#endif
			break;
		case 4:
			if (stplyr->mo)
			{
				convSpeed[0] = (FixedDiv(stplyr->speed, FixedMul(K_GetKartSpeed(stplyr, false), ORIG_FRICTION))*100);
				convSpeed[1] = convSpeed[0] >> FRACBITS;
			}
			break;
		default:
			break;
	}

	if (speedostyle == SPEEDO_VANILLA)
	{
		const char *metric = "";

		switch (cv_kartspeedometer.value)
		{
			case 1:
				metric = va("%3d km/h", convSpeed[1]);
				break;
			case 2:
				metric = va("%3d mph", convSpeed[1]);
				break;
			case 3:
				metric = va("%3d fu/t", convSpeed[1]);
				break;
			case 4:
				// if extra.kart is found, use its included % symbol
				if (!xtra_speedo)
					metric = va("%4d P", convSpeed[1]);
				else
					metric = va("%4d %%", convSpeed[1]);
			break;
			default:
				break;
		}

		V_DrawKartString(SPDM_X, SPDM_Y, V_HUDTRANS|splitflags, metric);
	}
	else if (speedostyle == SPEEDO_EXTRA) // why bother if we dont?
	{
		if (K_UseColorSpeedo(SPEEDO_EXTRA)) //Colourized hud
		{
			UINT8 *colormap = R_GetTranslationColormap(TC_DEFAULT, K_GetHudColor(), GTC_CACHE);
			V_DrawMappedPatch(SPDM_X + 1, SPDM_Y + 4, V_HUDTRANS|splitflags, skp_smallstickerclr, colormap);
		}
		else
			V_DrawScaledPatch(SPDM_X + 1, SPDM_Y + 4, V_HUDTRANS|splitflags, skp_smallsticker);

		V_DrawRankNum(SPDM_X + 26, SPDM_Y + 4, V_HUDTRANS|splitflags, convSpeed[1], 3, NULL);
		V_DrawScaledPatch(SPDM_X + 31, SPDM_Y + 4, V_HUDTRANS|splitflags, skp_speedpatches[cv_kartspeedometer.value]);
	}
	else if (speedostyle == SPEEDO_ACHII) // why bother if we dont?
	{
		if (K_UseColorSpeedo(SPEEDO_ACHII)) //Colourized hud
		{
			UINT8 *colormap = R_GetTranslationColormap(TC_DEFAULT, K_GetHudColor(), GTC_CACHE);
			V_DrawMappedPatch(SPDM_X + 1, SPDM_Y + 4, V_HUDTRANS|splitflags, skp_smallstickerachiclr, colormap);
			V_DrawRankNum(SPDM_X + 26, SPDM_Y + 4, V_HUDTRANS|splitflags, convSpeed[1], 3, NULL);
			V_DrawMappedPatch(SPDM_X + 31, SPDM_Y + 4, V_HUDTRANS|splitflags, skp_speedpatchesachiclr[cv_kartspeedometer.value], colormap);
		}
		else
		{
			V_DrawScaledPatch(SPDM_X + 1, SPDM_Y + 4, V_HUDTRANS|splitflags, skp_smallstickerachi);
			V_DrawRankNum(SPDM_X + 26, SPDM_Y + 4, V_HUDTRANS|splitflags, convSpeed[1], 3, NULL);
			V_DrawScaledPatch(SPDM_X + 31, SPDM_Y + 4, V_HUDTRANS|splitflags, skp_speedpatchesachi[cv_kartspeedometer.value]);
		}
	}
#ifdef ROTSPRITE
	else if (speedostyle == SPEEDO_DIAL)  // why bother if we dont?
	{
		K_DrawDialSpeedometer(convSpeed[0],
							dial_divisor,
							splitflags,
							(boolean)(G_BattleGametype()),
							(LUA_HudEnabled(hud_gametypeinfo)),
							(K_UseColorSpeedo(SPEEDO_DIAL)));
	}
#endif
	else if (speedostyle == SPEEDO_EXTRA3) // why bother if we dont?
	{
		if (K_UseColorSpeedo(SPEEDO_EXTRA3)) //Colourized hud
		{
			UINT8 *colormap = R_GetTranslationColormap(TC_DEFAULT, K_GetHudColor(), GTC_CACHE);
			V_DrawStretchyFixedPatch((SPDM_X-1)<<FRACBITS, (SPDM_Y + 5)<<FRACBITS, XTRA3PSCALE, XTRA3VSCALE, V_HUDTRANS|splitflags, skp_smallstickerclr3, colormap, 0);
		}
		else
			V_DrawStretchyFixedPatch((SPDM_X-1)<<FRACBITS, (SPDM_Y + 5)<<FRACBITS, XTRA3PSCALE, XTRA3VSCALE, V_HUDTRANS|splitflags, skp_smallsticker3, NULL, 0);

		V_DrawRankNum(SPDM_X + 26, SPDM_Y + 4, V_HUDTRANS|splitflags, convSpeed[1], 3, NULL);
		V_DrawScaledPatch(SPDM_X + 31, SPDM_Y + 4, V_HUDTRANS|splitflags, skp_speedpatches[cv_kartspeedometer.value]);
	}
	// Kart Z speedo bullshit...
	// Draw the Speed counter.
	else if ((speedostyle == SPEEDO_PMETER) || (speedostyle == SPEEDO_PMETERSMOL))
	{
		fixed_t fuspeed = 0;
		INT32 spdpatch = 0;
		static const INT32 speedIntervals[22] = {2, 5, 7, 10, 12, 15, 17,
												20, 22, 25, 27, 30, 32,
												35, 37, 40, 42, 45, 47,
												50, 52, 55};

		fuspeed = FixedDiv(stplyr->speed, mapobjectscale)/FRACUNIT;

		for (INT32 i = 0; i < 22; ++i)
		{
			if (fuspeed < speedIntervals[i])
			{
				spdpatch = i;
				break;
			}
		}

		if (((fuspeed < 57 && fuspeed > 54) || (fuspeed < 60 && fuspeed > 56) || (fuspeed > 59)) && (leveltime & 4))
			spdpatch = 24;
		else if (((fuspeed < 57 && fuspeed > 54) || (fuspeed < 60 && fuspeed > 56) || (fuspeed > 59)) && !(leveltime & 4))
			spdpatch = 23;

		V_DrawScaledPatch(SPDM_X, SPDM_Y, V_HUDTRANS|splitflags, (speedostyle == SPEEDO_PMETER) ? kp_kartzspeedo[spdpatch] : kp_kartzspeedo_smol[spdpatch]);
	}
}

#ifdef ROTSPRITE
#undef DIALSPDDIV
#undef MPHDIV
#endif

static void K_drawKartBumpersOrKarma(void)
{
	const UINT8 *colormap = R_GetTranslationColormap(TC_DEFAULT, K_GetHudColor(), GTC_CACHE);
	INT32 fx, fy, fflags;
	const boolean flipstring = splitscreen > 1 && stplyrnum & 1;  // same as laps, used for splitscreen
	INT32 stringw = 0;	// used with the above
	const char *bumpval = "";

	drawinfo_t info;
	K_getLapsDrawinfo(&info);
	fx = info.x;
	fy = info.y;
	fflags = info.flags;

	if (cv_battlespeedo.value && !splitscreen)
	{
		const UINT8 speedostyle = K_GetSpeedometerStyle();

		if ((speedostyle == SPEEDO_EXTRA) || (speedostyle == SPEEDO_ACHII) || (speedostyle == SPEEDO_EXTRA3))
			fy += 5;
		else
			fy += 7;
	}

	if (splitscreen > 1)
	{
		if (stplyr->kartstuff[k_bumper] <= 0)
		{
			bumpval = va("%d/2", stplyr->kartstuff[k_comebackpoints]);

			if (flipstring)
				fx -= 37;
			V_DrawMappedPatch(fx, fy-1, V_HUDTRANS|fflags, kp_splitkarmabomb, colormap);
			V_DrawString(fx+13, fy+1, V_HUDTRANS|fflags, bumpval);
		}
		else // the above doesn't need to account for weird stuff since the max amount of karma necessary is always 2 ^^^^
		{
			bumpval = va("%d/%d", stplyr->kartstuff[k_bumper], cv_kartbumpers.value);

			if (flipstring)	// for p2 and p4, assume we can have more than 10 bumpers. It's retarded but who knows.
			{
				stringw = V_StringWidth(bumpval, 0);

				V_DrawMappedPatch(fx-stringw-13, fy-1, V_HUDTRANS|fflags, kp_rankbumper, colormap);
				V_DrawRightAlignedString(fx, fy+1, V_HUDTRANS|fflags, bumpval);
			}
			else // draw bumpers normally.
			{
				V_DrawMappedPatch(fx, fy-1, V_HUDTRANS|fflags, kp_rankbumper, colormap);
				V_DrawString(fx+13, fy+1, V_HUDTRANS|fflags, bumpval);
			}
		}
	}
	else
	{
		patch_t *patch;

		if (stplyr->kartstuff[k_bumper] <= 0)
		{
			patch = (K_UseColorHud() ? kp_karmastickerclr : kp_karmasticker);
			bumpval = va("%d/2", stplyr->kartstuff[k_comebackpoints]);
		}
		else
		{
			if (stplyr->kartstuff[k_bumper] > 9 && cv_kartbumpers.value > 9)
				patch = (K_UseColorHud() ? kp_bumperstickerwideclr : kp_bumperstickerwide);
			else
				patch = (K_UseColorHud() ? kp_bumperstickerclr : kp_bumpersticker);

			bumpval = va("%d/%d", stplyr->kartstuff[k_bumper], cv_kartbumpers.value);
		}

		V_DrawMappedPatch(fx, fy, V_HUDTRANS|fflags, patch, colormap);
		V_DrawKartString(fx+47, fy+3, V_HUDTRANS|fflags, bumpval);
	}
}

#define lerp(from, to) cv_uncappedhud.value ? from + FixedMul(R_GetTimeFrac(RTF_LEVEL), to - from) : to

// converts mobj coordinates into screen coordinates
// NOTE: use with V_NOSCALESTART!
// Code updated in Lua by GenericHeroGuy for libSG
// Badly ported to C by NepDisk and acutally made to work and fixed by Indev!(Thanks so much!)
// Badly uncapped in C by GenericHeroGuy
// original code by Lat'
static boolean K_GetScreenCoords(vector2_t *vec, player_t *player, mobj_t *target, fixed_t hofs, boolean dontclip)
{
	fixed_t y, x;
	fixed_t targx, targy, targz;

	fixed_t dist;
	fixed_t distfact;
	fixed_t offset;

	fixed_t yres, xres;
	fixed_t fov;

	// this should never happen but its also kart so ¯\_(ツ)_/¯
	if (!player || P_MobjWasRemoved(target))
		return false;

	targx = lerp(target->old_x, target->x);
	targy = lerp(target->old_y, target->y);
	targz = lerp(target->old_z, target->z);

	// X coordinate
	// get difference between camangle and angle towards target
	x = (fixed_t)(viewangle - R_PointToAngle(targx, targy));

	distfact = FINECOSINE((x>>ANGLETOFINESHIFT) & FINEMASK);
    if (!distfact) distfact = 1;

	if (encoremode)
		x = -x;
	if (x < (fixed_t)ANGLE_270 || x > (fixed_t)ANGLE_90)
		return false;

	xres = vid.width<<(FRACBITS-1);
	yres = vid.height<<(FRACBITS-1);
	fov = FixedDiv(xres, FINETANGENT(((FixedAngle(cv_fov.value/2)+ANGLE_90)>>ANGLETOFINESHIFT) & 4095));

	// flipping
	const boolean targflip = target->eflags & MFE_VERTICALFLIP;
	const boolean srcflip = player->pflags & PF_FLIPCAM && player->mo->eflags & MFE_VERTICALFLIP;

	// Y coordinate
	// getting the angle difference here is a bit more involved...
	// start by getting the height difference between the camera and target
	y = viewz - targz - (targflip ? ((target->height * 2) / 3) : 0); // for some reason needs to be divided by "1.5" idk

	if (hofs)
		y = y - (targflip ? -hofs : hofs);

	// then get the distance between camera and target
	dist = R_PointToDist(targx, targy);

#ifdef HWRENDER
	// NOW we can get the angle differnce
	if (rendermode == render_opengl && !cv_glshearing.value)
	{
		angle_t yang = R_PointToAngle2(0, 0, dist, y); // not perspective
		x = FixedMul(x, FINECOSINE((yang>>ANGLETOFINESHIFT) & FINEMASK)); // perspective
		y = -aimingangle - FixedDiv(yang, distfact);

		if (y < (fixed_t)ANGLE_270 || y > (fixed_t)ANGLE_90) // clip points behind the camera
			return false;
		if (splitscreen == 1) // multiply by 1.25 for 2P splitscreen
			y = y + (y/4) ;
		if (srcflip) // flipcam
			y = -y;

		y = FixedMul(FINETANGENT((((angle_t)(-y)+ANGLE_90)>>ANGLETOFINESHIFT) & 4095), fov) + yres; // project the angle to get our final Y coordinate
	}
	else
#endif
	{
		const fixed_t fovratio = FixedDiv(90*FRACUNIT, 180*FRACUNIT - FixedMul(cv_fov.value, 4*FRACUNIT/3)-FRACUNIT*-30);

		y = FixedDiv(y, FixedMul(dist, distfact));
		if (srcflip)
			y = -y; // flipcam
		if (y != INT32_MIN) // I_Error(): FixedDiv: divide by zero
			y = FixedMul(FixedDiv(y, fovratio), xres) + yres;
		//else print("NOPE!")

		offset = FixedMul(FINETANGENT(((aimingangle+ANGLE_90)>>ANGLETOFINESHIFT) & 4095), xres);

		// this isn't fovtan... what am i even doing anymore
		if (splitscreen == 1)
			offset = 17*offset/120;

		// thanks fickle
		offset = FixedDiv(offset, fovratio);
		if (srcflip)
			offset = -offset; // flipcam
		y = y + offset;
	}

	// project the angle to get our final X coordinate
	x = FixedMul(FINETANGENT(((x+ANGLE_90)>>ANGLETOFINESHIFT) & 4095), fov);
	if (splitscreen == 1) // divide by 320/200 (1.6) on 2P splitscreen
		x = (x/2) + (x/8);
	x = x + xres;

	// now clip in screen-space
	if (!dontclip && (x < 0 || x > xres*2 || y < 0 || y > yres*2))
		return false;

	// adjust coords for splitscreen
	if (splitscreen == 1) // 2P
	{
		y = y>>1;
		if (stplyrnum > 0)
			y = y + yres;
	}
	if (splitscreen >= 2) // 3P or 4P
	{
		x = x>>1;
		y = y>>1;
		if (stplyrnum & 1)
			x = x + xres;
		if (stplyrnum >= 2)
			y = y + yres;
	}

	vec->y = y;
	vec->x = x;
	return true;
}

//Slighty fixed by Alug and further rewritten by NepDisk
//Decided to port and highly modify sunflower version for the main nametag drawing with additions by NepDisk. My previous one was broken anyway due to the changed screencoords and noscalestart
static void K_drawNameTags(void)
{
	UINT8 i, j;
	INT32 trans;
	vector2_t pos = {0};
	fixed_t namex, namey;
	int tagsdisplayed = 0;
	fixed_t distance = 0;
	fixed_t maxdistance = 0;
	boolean flipped = false;
	fixed_t z;

	char *tag;
	patch_t *icon = NULL;

	UINT8 *cm = NULL;
	UINT8 tagcolor = 0;
	INT32 vflags = 0;
	boolean usenametagrestat = false;
	fixed_t tagwidthsmall;
	fixed_t tagwidth;

	if (P_MobjWasRemoved(stplyr->mo) || (stplyr->spectator && !cv_shownametagspectator.value) || stplyr->exiting)
		return;

	maxdistance = ((10*cv_nametagdist.value) * mapobjectscale);

	for (i = 0; i < MAXPLAYERS; i++)
	{
		distance = 0;
		flipped = false;

		if (i > PLAYERSMASK)
			continue;
		if (players[i].spectator || !playeringame[i] || P_MobjWasRemoved(players[i].mo))
			continue;
		if (i == displayplayers[stplyrnum] && !cv_showownnametag.value && !(leveltime < 130))
			continue;
		if (i != displayplayers[stplyrnum] && leveltime < starttime)
			continue;
		if (players[i].kartstuff[k_hyudorotimer]) // player is invisible
			continue;
		if (maxdistance)
			distance = R_PointToDist(players[i].mo->x, players[i].mo->y);
		if (distance > maxdistance)
			continue;
		if (!P_CheckSightFast(stplyr->mo, players[i].mo))
			continue;

		z = players[i].mo->height;

		//Saltyhop hehe
		z += lerp(players[i].mo->old_spriteyoffset, players[i].mo->spriteyoffset);

		if (!K_GetScreenCoords(&pos, stplyr, players[i].mo, z, false))
			continue;

		tagsdisplayed++;

		if (tagsdisplayed > cv_nametagmaxplayers.value)
			break;

		trans = 0;

		switch (cv_nametagtrans.value)
		{
			case 1:
				if (distance > (maxdistance*3/4))
					trans = V_60TRANS;
				break;
			case 2:
				if (distance > (maxdistance*3/1))
					trans = V_90TRANS;
				else if (distance > (maxdistance*3/2))
					trans = V_80TRANS;
				else if (distance > (maxdistance*3/3))
					trans = V_60TRANS;
				else if (distance > (maxdistance*3/4))
					trans = V_40TRANS;
				else if (distance > (maxdistance*3/5))
					trans = V_20TRANS;
				break;
			case 3:
				trans =  V_40TRANS;
				break;
			case 4:
				trans = V_LocalTransFlag();
				break;
			case 0:
			default:
				break;
		}

		namex = pos.x>>FRACBITS;
		namey = pos.y>>FRACBITS;

		tag = va("%s%s ", HU_SkinColorToConsoleColor(players[i].mo->color), player_names[i]);
		icon = R_GetSkinFaceMini(players[i].mo->player);

		cm = R_GetTranslationColormap(players[i].skin, players[i].mo->color, GTC_CACHE);
		tagcolor = colortranslations[players[i].mo->color][7];
		vflags = trans | V_NOSCALESTART;
		usenametagrestat = ((cv_nametagrestat.value == 1 && (players[i].kartspeed != skins[players[i].skin].kartspeed || players[i].kartweight != skins[players[i].skin].kartweight)) || cv_nametagrestat.value == 2);
		tagwidthsmall = cv_smallnametags.value ? V_SmallStringWidth(player_names[i], V_ALLOWLOWERCASE) : V_ThinStringWidth(player_names[i], V_ALLOWLOWERCASE);
		tagwidth = vid.dup*tagwidthsmall;

		// If flipcam is on, other player is flipped relative to us when we have different
		// verticalflip flag value. Otherwise, they are simply flipped when verticalflip flag says so
		if ((stplyr->pflags & PF_FLIPCAM) && (stplyr->mo->eflags & MFE_VERTICALFLIP))
			flipped = (players[i].mo->eflags & MFE_VERTICALFLIP) != (stplyr->mo->eflags & MFE_VERTICALFLIP);
		else
			flipped = players[i].mo->eflags & MFE_VERTICALFLIP;

#ifdef HWRENDER
		// Needs extra offset. Not perfect but this will do for now
		if (rendermode == render_opengl && !cv_glshearing.value && cv_smallnametags.value)
			namey -= vid.dup*6;
#endif

		if (cv_smallnametags.value || !nametaggfx)
		{
			if (flipped)
				namey += vid.dup*5;
			else // small offset
				namey -= vid.dup*3;

			if (nametaggfx && cv_smallnametags.value == 1)
			{
				if (cv_nametagfacerank.value)
					tagwidthsmall += icon->width - vid.dup;

				// Have to draw the nametag using patches here since drawfill can't draw at this scale...
				if (!flipped)
					V_DrawFixedPatch(namex<<FRACBITS, namey<<FRACBITS, FRACUNIT/2, vflags, nametagline, cm);

				V_DrawStretchyFixedPatch(((namex+vid.dup*3)<<FRACBITS), namey<<FRACBITS,
					tagwidthsmall<<FRACBITS, FRACUNIT/2, vflags, nametagpic, cm, 0);

				namex += vid.dup*2;
				namey -= vid.dup*4;
			}

			if (cv_nametagfacerank.value)
			{
				V_DrawFixedPatch(namex<<FRACBITS, (namey - icon->height/2)<<FRACBITS, FRACUNIT/2, vflags, icon,  cm);
				namex += vid.dup*(1+icon->width/2); // add offset to other stuff
			}

			//Name
			V_DrawSmallString(namex, namey, V_ALLOWLOWERCASE | vflags, tag);

			if (usenametagrestat)
			{
				V_DrawSmallString(namex, namey - vid.dup*5, vflags, va("\x84S%d ", players[i].kartspeed));
				V_DrawSmallString(namex + vid.dup*10, namey - vid.dup*5, vflags, va("\x87W%d ", players[i].kartweight));
			}

			if (cv_nametagscore.value)
			{
				INT32 yofs = usenametagrestat ? 10 : 5;
				V_DrawSmallString(namex, namey - vid.dup*yofs, V_ALLOWLOWERCASE | vflags, va("\x8A%d ", players[i].score));
			}
		}
		else
		{
			if (cv_nametagfacerank.value)
				tagwidth += vid.dup*(icon->width+1);

			if (flipped)
			{
				for (j = 0; j < 4; j++)
				{
					V_DrawFill(namex, namey, vid.dup*3, vid.dup*4, 31 | vflags);
					V_DrawFill(namex+vid.dup, namey, vid.dup, vid.dup*4, tagcolor | vflags);
					namey += vid.dup*4;
					namex += vid.dup;
				}

				namey -= vid.dup*3;
				V_DrawFill(namex, namey+vid.dup*2, vid.dup, vid.dup, 31 | vflags); // a single black pixel
				V_DrawFill(namex+vid.dup, namey, tagwidth, vid.dup*3, 31 | vflags);
				V_DrawFill(namex+vid.dup, namey+vid.dup, tagwidth - vid.dup, vid.dup, tagcolor | vflags);
				namex += vid.dup*2;
			}
			else
			{
				for (j = 0; j < 4; j++)
				{
					namey -= vid.dup*4;
					V_DrawFill(namex, namey, vid.dup*3, vid.dup*4, 31 | vflags);
					V_DrawFill(namex+vid.dup, namey, vid.dup, vid.dup*4, tagcolor | vflags);
					namex += vid.dup;
				}

				V_DrawFill(namex, namey, vid.dup, vid.dup, 31 | vflags);
				V_DrawFill(namex+vid.dup, namey, tagwidth - vid.dup*2, vid.dup*3, 31 | vflags);
				V_DrawFill(namex+vid.dup, namey+vid.dup, tagwidth - vid.dup*3, vid.dup, tagcolor | vflags);
			}

			if (cv_nametagfacerank.value)
			{
				V_DrawMappedPatch(namex, namey - vid.dup*(icon->height+1), vflags, icon, cm);
				namex += vid.dup*(icon->height+1); // add offset to other stuff
			}

			V_DrawThinString(namex, namey - vid.dup*10, V_ALLOWLOWERCASE | vflags, tag);

			if (usenametagrestat)
			{
				V_DrawScaledPatch(namex, namey - vid.dup*20, vflags, nametagspeed);
				V_DrawScaledPatch(namex + vid.dup*18, namey - vid.dup*20, vflags, nametagweight);
				V_DrawString(namex + vid.dup*9, namey - vid.dup*19, V_ALLOWLOWERCASE | vflags, va("\x84%d ", players[i].kartspeed));
				V_DrawString(namex + vid.dup*27, namey - vid.dup*19, V_ALLOWLOWERCASE | vflags, va("\x87%d ", players[i].kartweight));
			}

			if (cv_nametagscore.value)
			{
				INT32 yofs = usenametagrestat ? 25 : 15;
				V_DrawSmallString(namex, namey - vid.dup*yofs, V_ALLOWLOWERCASE | vflags, va("\x8A%d ", players[i].score));
			}
		}
	}
}

// Based on Driftgauge refactor by GenericHeroGuy ported from lua and expanded by NepDisk
static void K_drawDriftGauge(void)
{
	vector2_t pos = {0};
	fixed_t basex, basey;
	int i;
	UINT8 *colormap = NULL;

	static const UINT8 driftcolors[3][4] = {
		{0, 0, 10, 16},       // no drift
		{215, 215, 204, 253}, // blue
		{125, 125, 151, 159}  // red
	};

	static const UINT8 driftskins[3] = {
		SKINCOLOR_NONE,
		SKINCOLOR_TEAL,
		SKINCOLOR_SALMON,
	};

	static const UINT8 driftrainbow[18] = {
		0, 31, 47, 63, 79, 95, 111, 119, 127, 143, 159, 175, 183, 191, 199, 207, 223, 247
	};

	if (camera[stplyrnum].freecam)
		return;

	if (!splitscreen && !camera->chase)
		return;

	if (forceshowhud)
		goto skipcrap; // i will skip the drift early return and you cant stop me!

	if (!stplyr->kartstuff[k_drift])
		return;

skipcrap:

	if (P_MobjWasRemoved(stplyr->mo))
		return;

	if (!K_GetScreenCoords(&pos, stplyr, stplyr->mo, FixedMul(cv_driftgaugeofs.value, cv_driftgaugeofs.value > 0 ? stplyr->mo->scale : mapobjectscale), false))
		return;

	fixed_t barx;
	fixed_t bary;
	INT32 BAR_WIDTH;

	const INT32 driftval    = K_GetKartDriftSparkValue(stplyr);
	const INT32 driftcharge = min(driftval*4, stplyr->kartstuff[k_driftcharge]);
	const INT32 driftlevel  = min(driftcharge / driftval, 2);
	const INT32 drifttrans  = ((cv_driftgaugetrans.value) ? V_LocalTransFlag() : 0);
	const SINT8 gaugestyle  = K_GetDriftgaugeStyle();

	basex = pos.x>>FRACBITS;
	basey = pos.y>>FRACBITS;

	switch (gaugestyle)
	{
		case GAUGE_DEFAULT:
		case GAUGE_SMALL:
		case GAUGE_EXTRA:
		case GAUGE_BIGNUM:
			{
				patch_t *driftpatch = NULL;

				if (gaugestyle == GAUGE_DEFAULT || gaugestyle == GAUGE_BIGNUM || gaugestyle == GAUGE_EXTRA)
				{
					barx = basex - vid.dup*23;
					BAR_WIDTH = vid.dup*47;
				}
				else
				{
					barx = basex - vid.dup*12;
					BAR_WIDTH = vid.dup*23;
				}

				bary = basey - vid.dup*2;

				if (gaugestyle == GAUGE_EXTRA) // i hate hud code i hate hud code i hate hud code i hate hud code i hate hud code.....
				{
					if (K_UseColorSpeedo(SPEEDO_EXTRA3)) // We reuse the extra speedometer patch hence this check
					{
						colormap = R_GetTranslationColormap(TC_DEFAULT, K_GetHudColor(), GTC_CACHE);
						driftpatch = skp_smallstickerclr3;
					}
					else
						driftpatch = skp_smallsticker3;

					V_DrawStretchyFixedPatch((basex - vid.dup*30)<<FRACBITS, ((basey<<FRACBITS) - FixedMul(vid.dup<<FRACBITS, 21*FRACUNIT/10)), XTRA3PSCALE, XTRA3VSCALE, V_NOSCALESTART|V_OFFSET|drifttrans, driftpatch, colormap, 0);
				}
				else
				{
					if (K_UseColorHud()) // Colourized hud
					{
						colormap = R_GetTranslationColormap(TC_DEFAULT, K_GetHudColor(), GTC_CACHE);
						driftpatch = (gaugestyle == GAUGE_SMALL ? driftgaugesmallcolor : driftgaugecolor);
					}
					else
						driftpatch = (gaugestyle == GAUGE_SMALL ? driftgaugesmall : driftgauge);

					V_DrawMappedPatch(gaugestyle == GAUGE_SMALL ? basex + vid.dup*11 : basex, basey, V_NOSCALESTART|V_OFFSET|drifttrans, driftpatch, colormap);
				}

				if (driftcharge >= driftval*4) // rainbow sparks
				{
					colormap = R_GetTranslationColormap(TC_RAINBOW, K_RainbowColor(), GTC_CACHE);

					for (i = 0; i < 4; i++)
						V_DrawFill(barx, bary+vid.dup*1+vid.dup*i, BAR_WIDTH, vid.dup, (driftrainbow[min((leveltime % 18) + 1, 17)] + i*2) | V_NOSCALESTART|drifttrans);
				}
				else // none/blue/red
				{
					const INT32 limit = (driftval * (driftcharge >= driftval*2 ? 2 : 1));
					const INT32 width = ((driftcharge - (driftcharge >= driftval ? limit : 0)) * BAR_WIDTH) / limit;

					colormap = R_GetTranslationColormap(TC_RAINBOW, driftskins[driftlevel], GTC_CACHE);

					for (i = 0; i < 4; i++)
					{
						if (driftcharge >= driftval)
							V_DrawFill(barx, bary+vid.dup*1+vid.dup*i, BAR_WIDTH, vid.dup, driftcolors[driftlevel-1][i] | V_NOSCALESTART|drifttrans);

						V_DrawFill(barx, bary+vid.dup*1+vid.dup*i, width, vid.dup, driftcolors[driftlevel][i] | V_NOSCALESTART|drifttrans);
					}
				}

				// right, also draw a cool number
				if (gaugestyle == GAUGE_BIGNUM)
					V_DrawPaddedTallColorNum(basex + (vid.dup*32), basey, V_NOSCALESTART|V_OFFSET|drifttrans, driftcharge*100 / driftval, 3, colormap);
				else
					V_DrawPingNum((gaugestyle == GAUGE_SMALL ? basex + (vid.dup*22) : basex + (vid.dup*32)), basey, V_NOSCALESTART|V_OFFSET|drifttrans, driftcharge*100 / driftval, colormap);
			}
			break;
		case GAUGE_NUMONLY:
			{
				if (driftcharge >= driftval*4)
					colormap = R_GetTranslationColormap(TC_RAINBOW, K_RainbowColor(), GTC_CACHE);
				else
					colormap = R_GetTranslationColormap(TC_RAINBOW, driftskins[driftlevel], GTC_CACHE);

				V_DrawPaddedTallColorNum(basex + (vid.dup*16), basey, V_NOSCALESTART|V_OFFSET|drifttrans, driftcharge*100 / driftval, 3, colormap);
			}
			break;
		default:
			break;
	}
}

static fixed_t K_FindCheckX(fixed_t px, fixed_t py, angle_t ang, fixed_t mx, fixed_t my)
{
	fixed_t dist, x;
	fixed_t range = RING_DIST/3;
	angle_t diff;

	range *= gamespeed+1;

	dist = abs(R_PointToDist2(px, py, mx, my));

	if (dist > range)
		return -BASEVIDWIDTH;

	diff = R_PointToAngle2(px, py, mx, my) - ang;

	if (diff < ANGLE_90 || diff > ANGLE_270)
		return -BASEVIDWIDTH;
	else
		x = (FixedMul(FINETANGENT(((diff+ANGLE_90)>>ANGLETOFINESHIFT) & 4095), 160<<FRACBITS) + (160<<FRACBITS))>>FRACBITS;

	if (encoremode)
		x = BASEVIDWIDTH-x;

	if (splitscreen > 1)
		x /= 2;

	return x;
}

static void K_drawKartWanted(void)
{
	UINT8 i, numwanted = 0;
	UINT8 *colormap = NULL;
	INT32 basex = 0, basey = 0;

	if (stplyrnum != 0)
		return;

	for (i = 0; i < 4; i++)
	{
		if (battlewanted[i] == -1)
			break;
		numwanted++;
	}

	if (numwanted <= 0)
		return;

	// set X/Y coords depending on splitscreen.
	if (splitscreen < 3)		// 1P and 2P use the same code.
	{
		basex = WANT_X;
		basey = WANT_Y;

		if (splitscreen == 2)
		{
			basey += 16;	// slight adjust for 3P
			basex -= 6;
		}
	}
	else if (splitscreen == 3)	// 4P splitscreen...
	{
		basex = BASEVIDWIDTH/2 - (kp_wantedsplit->width/2);	// center on screen
		basey = BASEVIDHEIGHT - 55;
		//basey2 = 4;
	}

	if (battlewanted[0] != -1)
		colormap = R_GetTranslationColormap(0, players[battlewanted[0]].skincolor, GTC_CACHE);

	V_DrawFixedPatch(basex<<FRACBITS, basey<<FRACBITS, FRACUNIT, V_HUDTRANS|(splitscreen < 3 ? V_SNAPTORIGHT : 0)|V_SNAPTOBOTTOM, (splitscreen > 1 ? kp_wantedsplit : kp_wanted), colormap);
	/*if (basey2)
		V_DrawFixedPatch(basex<<FRACBITS, basey2<<FRACBITS, FRACUNIT, V_HUDTRANS|V_SNAPTOTOP, (splitscreen == 3 ? kp_wantedsplit : kp_wanted), colormap);	// < used for 4p splits.*/

	for (i = 0; i < numwanted; i++)
	{
		INT32 x = basex+(splitscreen > 1 ? 13 : 8), y = basey+(splitscreen > 1 ? 16 : 21);
		fixed_t scale = FRACUNIT/2;

		if (battlewanted[i] == -1)
			break;

		player_t *p = &players[battlewanted[i]];

		if (numwanted == 1)
			scale = FRACUNIT;
		else
		{
			if (i & 1)
				x += 16;
			if (i > 1)
				y += 16;
		}

		if (p->skincolor)
		{
			colormap = R_GetTranslationColormap(TC_RAINBOW, p->skincolor, GTC_CACHE);
			V_DrawFixedPatch(x<<FRACBITS, y<<FRACBITS, FRACUNIT, V_HUDTRANS|(splitscreen < 3 ? V_SNAPTORIGHT : 0)|V_SNAPTOBOTTOM, (scale == FRACUNIT ? R_GetSkinFaceWant(p) : R_GetSkinFaceRank(p)), colormap);
			/*if (basey2)	// again with 4p stuff
				V_DrawFixedPatch(x<<FRACBITS, (y - (basey-basey2))<<FRACBITS, FRACUNIT, V_HUDTRANS|V_SNAPTOTOP, (scale == FRACUNIT ? facewantprefix[p->skin] : facerankprefix[p->skin]), colormap);*/
		}
	}
}

static void K_drawKartPlayerCheck(void)
{
	INT32 i;
	UINT8 *colormap;
	INT32 x;

	if (!stplyr->mo || stplyr->spectator || stplyr->awayviewtics)
		return;

	if (camspin[0])
		return;

	const INT32 splitflags = K_calcSplitFlags(V_SNAPTOBOTTOM);

	for (i = 0; i < MAXPLAYERS; i++)
	{
		UINT8 pnum = 0;

		if (&players[i] == stplyr)
			continue;
		if (!playeringame[i] || players[i].spectator)
			continue;
		if (!players[i].mo)
			continue;

		if ((players[i].kartstuff[k_invincibilitytimer] <= 0) && (leveltime & 2))
			pnum++; // white frames

		if (players[i].kartstuff[k_itemtype] == KITEM_GROW || players[i].kartstuff[k_growshrinktimer] > 0)
			pnum += 4;
		else if (players[i].kartstuff[k_itemtype] == KITEM_INVINCIBILITY || players[i].kartstuff[k_invincibilitytimer])
			pnum += 2;

		x = K_FindCheckX(stplyr->mo->x, stplyr->mo->y, stplyr->mo->angle, players[i].mo->x, players[i].mo->y);
		if (x <= 320 && x >= 0)
		{
			if (x < 14)
				x = 14;
			else if (x > 306)
				x = 306;

			colormap = R_GetTranslationColormap(TC_DEFAULT, players[i].mo->color, GTC_CACHE);
			V_DrawMappedPatch(x, CHEK_Y, V_HUDTRANS|splitflags, kp_check[pnum], colormap);
		}
	}
}

static boolean K_useSmallMinimapHead(player_t *player)
{
	return cv_minihead.value == 1 || (cv_minihead.value == 2 && !(player && P_IsDisplayPlayer(player)));
}

static void K_drawKartMinimapIcon(fixed_t objx, fixed_t objy, INT32 hudx, INT32 hudy, INT32 flags, INT32 blend, patch_t *icon, UINT8 *colormap, drawinfo_t *dims, boolean scaleme)
{
	// amnum xpos & ypos are the icon's speed around the HUD.
	// The number being divided by is for how fast it moves.
	// The higher the number, the slower it moves.

	// am xpos & ypos are the icon's starting position. Withouht
	// it, they wouldn't 'spawn' on the top-right side of the HUD.

	fixed_t amnumxpos, amnumypos;
	INT32 amxpos, amypos;
	fixed_t scale = FRACUNIT;
	patch_t *AutomapPic = NULL;
	INT16 w, h;

	AutomapPic = minimapinfo.minimap_pic;

	if (AutomapPic == NULL)
	{
		return; // no pic, just get outta here
	}

	amnumxpos =  (FixedMul(objx, minimapinfo.zoom) - minimapinfo.offs_x);
	amnumypos = -(FixedMul(objy, minimapinfo.zoom) - minimapinfo.offs_y);

	if (encoremode)
		amnumxpos = -amnumxpos;

	if (dims && (dims->x != 0) && (dims->y != 0))
	{
		w = dims->x;
		h = dims->y;
	}
	else
	{
		w = icon->width;
		h = icon->height;
	}

	amxpos = amnumxpos + ((hudx + (AutomapPic->width-w)/2)<<FRACBITS);
	amypos = amnumypos + ((hudy + (AutomapPic->height-h)/2)<<FRACBITS);

	if (cv_minihead.value && (scaleme))
	{
		amxpos += (w / 4)<<FRACBITS;
		amypos += (h / 4)<<FRACBITS;
		scale /= 2;
	}

	V_DrawBlendingFixedPatch(amxpos, amypos, scale, flags, icon, colormap, blend);
}

static void K_drawKartMinimapHead(mobj_t *mo, INT32 x, INT32 y, INT32 flags)
{
	// amnum xpos & ypos are the icon's speed around the HUD.
	// The number being divided by is for how fast it moves.
	// The higher the number, the slower it moves.

	// am xpos & ypos are the icon's starting position. Without
	// it, they wouldn't 'spawn' on the top-right side of the HUD.

	const skin_t *skin;
	player_t *player = mo->player;
	const boolean skinlocal = mo->skinlocal;

	fixed_t amnumxpos, amnumypos;
	INT32 amxpos, amypos, wntdamxpos, wntdamypos;
	fixed_t scale = FRACUNIT;
	patch_t *minimaphead = NULL;

#ifdef ROTSPRITE
	angle_t rollangle = 0;
	INT32 rot = 0;
#endif

	skin = K_GetMobjSkin(mo);
	minimaphead = (skinlocal ? localfacemmapprefix : facemmapprefix)[K_GetMobjSkinNum(skin, skinlocal)];

	if (minimaphead == NULL)
		return;

	amnumxpos =  (FixedMul(lerp(mo->old_x, mo->x), minimapinfo.zoom) - minimapinfo.offs_x);
	amnumypos = -(FixedMul(lerp(mo->old_y, mo->y), minimapinfo.zoom) - minimapinfo.offs_y);

	if (encoremode)
		amnumxpos = -amnumxpos;

	amxpos = amnumxpos + ((x + (minimapinfo.minimap_pic->width-minimaphead->width) / 2)<<FRACBITS);
	amypos = amnumypos + ((y + (minimapinfo.minimap_pic->height-minimaphead->height) / 2)<<FRACBITS);

	if (cv_showminimapnames.value && player && !(modeattacking || gamestate == GS_TIMEATTACK))
	{
		V_DrawCenteredSmallStringAtFixed(amxpos + (4*FRACUNIT), amypos - (3*FRACUNIT), V_ALLOWLOWERCASE|flags|V_SkinColorToHighlightcolor(mo->color), player_names[player - players]);
	}

	const boolean minihead = K_useSmallMinimapHead(mo->player);

	// thx wanted reticle for having weird offsets very cool
	wntdamxpos = minihead ? amxpos + (1<<FRACBITS) : amxpos - (4<<FRACBITS);
	wntdamypos = minihead ? amypos + (1<<FRACBITS) : amypos - (4<<FRACBITS);

	if (minihead)
	{
		amxpos += (minimaphead->width  / 4)<<FRACBITS;
		amypos += (minimaphead->height / 4)<<FRACBITS;
		scale /= 2;
	}

#ifdef ROTSPRITE
	if (cv_spinoutroll.value && player && player->spinoutrot)
	{
		// Rotate counterclockwise.
		rollangle = FixedAngle(player->spinoutrot * -1);
		rot = R_GetRollAngle(rollangle);

		if (rot)
		{
			minimaphead = W_CachePatchNameRotated(skin->facemmap, rot, PU_PATCH);
		}
	}
#endif

	if (!mo->color) // 'default' color
		V_DrawSciencePatch(amxpos, amypos, flags, minimaphead, scale);
	else
	{
		UINT8 *colormap;

		if (mo->colorized)
			colormap = R_GetTranslationColormap(TC_RAINBOW, mo->color, GTC_CACHE);
		else
			colormap = R_GetLocalTranslationColormap(mo->skin, mo->localskin, mo->color, GTC_CACHE, skinlocal);

		V_DrawFixedPatch(amxpos, amypos, scale, flags, minimaphead, colormap);

		if (player
			&& ((G_RaceGametype() && player->kartstuff[k_position] == spbplace)
			|| (G_BattleGametype() && K_IsPlayerWanted(player))))
		{
			V_DrawFixedPatch(wntdamxpos, wntdamypos, scale, flags, kp_wantedreticle, NULL);
		}
	}
}

enum
{
	MINIANGLE_NONE = 0,
	MINIANGLE_DOT,
	MINIANGLE_LIGHT
};

static void K_drawKartMinimap(void)
{
	INT32 i = 0;
	INT32 x, y;
	INT32 minimaptrans, splitflags;
	SINT8 localplayers[MAXSPLITSCREENPLAYERS];
	SINT8 numlocalplayers = 0;
	patch_t *AutomapPic;

	// Draw the HUD only when playing in a level.
	// hu_stuff needs this, unlike st_stuff.
	if (gamestate != GS_LEVEL)
		return;

	// Only draw for the first player
	if (stplyrnum != 0)
		return;

	AutomapPic = minimapinfo.minimap_pic;

	if (AutomapPic == NULL)
	{
		return; // no pic, just get outta here
	}

	minimaptrans = K_getMinimapTrans();

	// Exit early if it wouldn't draw anyway.
	if (minimaptrans == -1)
		return;

	drawinfo_t info;
	K_getMinimapDrawinfo(&info);
	x = info.x - (AutomapPic->width/2);
	y = info.y - (AutomapPic->height/2);
	splitflags = info.flags;

	splitflags |= minimaptrans;

	if (encoremode)
		V_DrawScaledPatch(x+AutomapPic->width, y, splitflags|V_FLIP, AutomapPic);
	else
		V_DrawScaledPatch(x, y, splitflags, AutomapPic);

	if (!(splitscreen == 2))
	{
		splitflags &= ~minimaptrans;
		splitflags |= V_HUDTRANSHALF;
	}

	// let offsets transfer to the heads, too!
	if (encoremode)
		x += AutomapPic->leftoffset;
	else
		x -= AutomapPic->leftoffset;
	y -= AutomapPic->topoffset;

	// initialize
	for (i = 0; i < MAXSPLITSCREENPLAYERS; i++)
		localplayers[i] = -1;

	// Player's tiny icons on the Automap. (drawn opposite direction so player 1 is drawn last in splitscreen)
	if (ghosts)
	{
		demoghost *g = ghosts;
		while (g)
		{
			K_drawKartMinimapHead(g->mo, x, y, splitflags);
			g = g->next;
		}

		if (!stplyr->mo || stplyr->spectator) // do we need the latter..?
			return;

		localplayers[numlocalplayers++] = stplyr-players;
	}
	else
	{
		for (i = MAXPLAYERS-1; i >= 0; i--)
		{
			if (!playeringame[i])
				continue;

			if (!players[i].mo || players[i].spectator)
				continue;

			if (!cv_showminimapfinished.value && players[i].exiting)
				continue;

			if (P_IsDisplayPlayer(&players[i]))
			{
				// Draw display players on top of everything else
				localplayers[numlocalplayers++] = i;
				continue;
			}

			// Now we know it's not a display player, handle non-local player exceptions.
			if (G_BattleGametype() && players[i].kartstuff[k_bumper] <= 0)
				continue;

			if (players[i].kartstuff[k_hyudorotimer] > 0)
			{
				if (!((players[i].kartstuff[k_hyudorotimer] < 1*TICRATE/2
					|| players[i].kartstuff[k_hyudorotimer] > hyudorotime-(1*TICRATE/2))
					&& !(leveltime & 1)))
					continue;
			}

			K_drawKartMinimapHead(players[i].mo, x, y, splitflags);
		}
	}

	// draw our local players here, opaque.
	splitflags &= ~V_HUDTRANSHALF;
	splitflags |= V_HUDTRANS;

	const SINT8 icondotradius = ((cv_minihead.value == 1) && !cv_showminimapnames.value) ? 8 : 10;
	patch_t* minipatch = NULL;
	INT32 rot;
	INT32 blending;

	for (i = 0; i < numlocalplayers; i++)
	{
		if (localplayers[i] == -1)
			continue; // this doesn't interest us

		mobj_t *mobj = players[localplayers[i]].mo;

		// dont draw for no contestants
		boolean playertimedout = (mobj->health <= 0 && players[localplayers[i]].pflags & PF_TIMEOVER);

		if (cv_showminimapangle.value && (minidoticon || minilighticon) && !playertimedout)
		{
			UINT8 *colormap = NULL;
			drawinfo_t dims;
			fixed_t interpx, interpy;
			fixed_t xoff = 0, yoff = 0;
			blending = 0;

			angle_t ang = R_InterpolateAngle(mobj->old_angle, mobj->angle);

			if (encoremode)
				ang = ANGLE_180 - ang;

			if (mobj->color)
			{
				if (mobj->colorized)
				{
					colormap = R_GetTranslationColormap(TC_RAINBOW, mobj->color, GTC_CACHE);
				}
				else
				{
					const INT32 skinnum = K_GetMobjLocalSkinNum(mobj->skin, mobj->localskin, mobj->skinlocal);

					// special case if startcolor is not the default (160 / Green)
					if (skinnum && skins[skinnum].starttranscolor != skins[0].starttranscolor)
					{
						colormap = R_GetTranslationColormap(TC_DEFAULT, mobj->color, GTC_CACHE);
					}
					else
					{
						colormap = R_GetLocalTranslationColormap(mobj->skin, mobj->localskin, mobj->color, GTC_CACHE, mobj->skinlocal);
					}
				}
			}

			interpx = lerp(mobj->old_x, mobj->x);
			interpy = lerp(mobj->old_y, mobj->y);

			minipatch = kp_minimapdot;

			dims.x = 0;
			dims.y = 0;

			if (cv_showminimapangle.value == MINIANGLE_DOT)
			{
				xoff = FixedMul(FCOS(ang), icondotradius);
				yoff = -FixedMul(FSIN(ang), icondotradius);
			}
			else if (cv_showminimapangle.value == MINIANGLE_LIGHT)
			{
				rot = R_GetRollAngle(ang);
				minipatch = (patch_t *)W_CachePatchNameRotated("MMAPHDLT", rot, PU_PATCH);

				blending = B_ADD;
				xoff = yoff = 0;

				dims.x = 48;
				dims.y = 24;
			}

			K_drawKartMinimapIcon(
					interpx,
					interpy,
					x + xoff,
					y + yoff,
					splitflags,
					blending,
					minipatch,
					colormap,
					&dims,
					false
			);
		}

		// draw the minimap head after the nonsense above
		K_drawKartMinimapHead(mobj, x, y, splitflags);
	}
}

#undef lerp

static void K_drawKartStartCountdown(void)
{
	INT32 pnum = 0, splitflags = K_calcSplitFlags(0); // 3

	if (leveltime >= starttime-(2*TICRATE)) // 2
		pnum++;
	if (leveltime >= starttime-TICRATE) // 1
		pnum++;
	if (leveltime >= starttime) // GO!
		pnum++;
	if ((leveltime % (2*5)) / 5) // blink
		pnum += 4;
	if (splitscreen) // splitscreen
		pnum += 8;

	V_DrawScaledPatch(STCD_X - (kp_startcountdown[pnum]->width/2), STCD_Y - (kp_startcountdown[pnum]->height/2), splitflags, kp_startcountdown[pnum]);
}

static void K_drawKartFinish(void)
{
	INT32 pnum = 0, splitflags = K_calcSplitFlags(0);

	if (!stplyr->kartstuff[k_cardanimation] || stplyr->kartstuff[k_cardanimation] >= 2*TICRATE)
		return;

	if ((stplyr->kartstuff[k_cardanimation] % (2*5)) / 5) // blink
		pnum = 1;

	if (splitscreen > 1) // 3/4p, stationary FIN
	{
		pnum += 2;
		V_DrawScaledPatch(STCD_X - (kp_racefinish[pnum]->width/2), STCD_Y - (kp_racefinish[pnum]->height/2), splitflags, kp_racefinish[pnum]);
		return;
	}

	//else -- 1/2p, scrolling FINISH
	{
		INT32 x, xval;

		if (splitscreen) // wide splitscreen
			pnum += 4;

		x = ((vid.width<<FRACBITS)/vid.dup);
		xval = (kp_racefinish[pnum]->width<<FRACBITS);
		x = (FixedMul(((TICRATE - stplyr->kartstuff[k_cardanimation])<<FRACBITS) - R_GetTimeFrac(RTF_LEVEL), xval > x ? xval : x))/TICRATE;

		if (splitscreen && stplyrnum == 1)
			x = -x;

		V_DrawFixedPatch(x + (STCD_X<<FRACBITS) - (xval>>1),
			(STCD_Y<<FRACBITS) - (kp_racefinish[pnum]->height<<(FRACBITS-1)),
			FRACUNIT,
			splitflags, kp_racefinish[pnum], NULL);
	}
}

static void K_drawBattleFullscreen(void)
{
	INT32 cardanim = stplyr->kartstuff[k_cardanimation] << FRACBITS;

	// fill in the fractional bits
	if (cardanim && cardanim != 164*FRACUNIT)
	{
		INT32 frac = R_GetTimeFrac(RTF_LEVEL) * ((164 - stplyr->kartstuff[k_cardanimation])/8 + 1);

		if (stplyr->exiting)
			cardanim += frac;
		else
			cardanim += stplyr->kartstuff[k_comebacktimer] < 6*TICRATE ? -frac : frac;
	}

	INT32 x = BASEVIDWIDTH/2;
	INT32 y = (-64*FRACUNIT) + cardanim; // card animation goes from 0 to 164, 164 is the middle of the screen
	INT32 splitflags = V_SNAPTOTOP; // I don't feel like properly supporting non-green resolutions, so you can have a misuse of SNAPTO instead
	fixed_t scale = FRACUNIT;
	boolean drawcomebacktimer = true;	// lazy hack because it's cleaner in the long run.

	if (!LUA_HudEnabled(hud_battlecomebacktimer))
		drawcomebacktimer = false;

	if (splitscreen)
	{
		if ((splitscreen == 1 && stplyrnum == 1) || (splitscreen > 1 && stplyrnum & 2))
		{
			y = (232*FRACUNIT) - (cardanim/2);
			splitflags = V_SNAPTOBOTTOM;
		}
		else
			y = (-32*FRACUNIT) + (cardanim/2);

		if (splitscreen > 1)
		{
			scale /= 2;

			if (stplyrnum & 1)
				x = 3*BASEVIDWIDTH/4;
			else
				x = BASEVIDWIDTH/4;
		}
		else
		{
			if (stplyr->exiting)
			{
				if (stplyrnum == 1)
					x = BASEVIDWIDTH-96;
				else
					x = 96;
			}
			else
				scale /= 2;
		}
	}

	if (stplyr->exiting)
	{
		if (stplyrnum == 0)
			V_DrawFadeScreen(0xFF00, 16);
		if (stplyr->exiting < 6*TICRATE && !stplyr->spectator)
		{
			if (stplyr->kartstuff[k_position] == 1)
				V_DrawFixedPatch(x<<FRACBITS, y, scale, splitflags, kp_battlewin, NULL);
			else
				V_DrawFixedPatch(x<<FRACBITS, y, scale, splitflags, (K_IsPlayerLosing(stplyr) ? kp_battlelose : kp_battlecool), NULL);
		}
		else
			K_drawKartFinish();
	}
	else if (stplyr->kartstuff[k_bumper] <= 0 && stplyr->kartstuff[k_comebacktimer] && comeback && !stplyr->spectator && drawcomebacktimer)
	{
		UINT16 t = stplyr->kartstuff[k_comebacktimer]/(10*TICRATE);
		INT32 txoff, adjust = (splitscreen > 1) ? 4 : 6; // normal string is 8, kart string is 12, half of that for ease
		INT32 ty = (BASEVIDHEIGHT/2)+66;

		txoff = adjust;

		while (t)
		{
			txoff += adjust;
			t /= 10;
		}

		if (splitscreen)
		{
			if (splitscreen > 1)
				ty = (BASEVIDHEIGHT/4)+33;
			if ((splitscreen == 1 && stplyrnum == 1) || (splitscreen > 1 && stplyrnum & 2))
				ty += (BASEVIDHEIGHT/2);
		}
		else
			V_DrawFadeScreen(0xFF00, 16);

		if (!comebackshowninfo)
			V_DrawFixedPatch(x<<FRACBITS, y, scale, splitflags, kp_battleinfo, NULL);
		else
			V_DrawFixedPatch(x<<FRACBITS, y, scale, splitflags, kp_battlewait, NULL);

		if (splitscreen > 1)
			V_DrawString(x-txoff, ty, 0, va("%d", stplyr->kartstuff[k_comebacktimer]/TICRATE));
		else
		{
			if (K_UseColorHud()) // Colourized hud
			{
				UINT8 *colormap = R_GetTranslationColormap(TC_DEFAULT, K_GetHudColor(), GTC_CACHE);
				V_DrawFixedPatch(x<<FRACBITS, ty<<FRACBITS, scale, 0, kp_timeoutstickerclr, colormap);
			}
			else
				V_DrawFixedPatch(x<<FRACBITS, ty<<FRACBITS, scale, 0, kp_timeoutsticker, NULL);

			V_DrawKartString(x-txoff, ty, 0, va("%d", stplyr->kartstuff[k_comebacktimer]/TICRATE));
		}
	}

	if (netgame && cv_showfreeplay.value && !stplyr->spectator && timeinmap > 113) // FREE PLAY?
	{
		UINT8 i;

		// check to see if there's anyone else at all
		for (i = 0; i < MAXPLAYERS; i++)
		{
			if (i == displayplayers[0])
				continue;
			if (playeringame[i])
				return;
		}

		if (LUA_HudEnabled(hud_freeplay))
			K_drawKartFreePlay(leveltime);
	}
}

static void K_drawKartFirstPerson(void)
{
	static INT32 pnum[4], turn[4], drift[4];
	INT32 pn = 0, tn = 0, dr = 0;
	INT32 target = 0, splitflags = K_calcSplitFlags(V_SNAPTOBOTTOM);
	INT32 x = BASEVIDWIDTH/2, y = BASEVIDHEIGHT;
	fixed_t scale;
	UINT8 *colmap = NULL;
	ticcmd_t *cmd = &stplyr->cmd;

	if (stplyr->spectator || !stplyr->mo || (stplyr->mo->flags2 & MF2_DONTDRAW))
		return;

	pn = pnum[stplyrnum];
	tn = turn[stplyrnum];
	dr = drift[stplyrnum];

	if (splitscreen)
	{
		y >>= 1;
		if (splitscreen > 1)
			x >>= 1;
	}

	{
		if (stplyr->speed < (20*stplyr->mo->scale) && (leveltime & 1) && !splitscreen)
			y++;
		// the following isn't EXPLICITLY right, it just gets the result we want, but i'm too lazy to look up the right way to do it
		if (stplyr->mo->flags2 & MF2_SHADOW)
			splitflags |= FF_TRANS80;
		else if (stplyr->mo->frame & FF_TRANSMASK)
			splitflags |= (stplyr->mo->frame & FF_TRANSMASK);
	}

	if (cmd->driftturn > 400) // strong left turn
		target = 2;
	else if (cmd->driftturn < -400) // strong right turn
		target = -2;
	else if (cmd->driftturn > 0) // weak left turn
		target = 1;
	else if (cmd->driftturn < 0) // weak right turn
		target = -1;
	else // forward
		target = 0;

	if (encoremode)
		target = -target;

	if (pn < target)
		pn++;
	else if (pn > target)
		pn--;

	if (pn < 0)
		splitflags |= V_FLIP; // right turn

	target = abs(pn);
	if (target > 2)
		target = 2;

	x <<= FRACBITS;
	y <<= FRACBITS;

	if (tn != cmd->driftturn/50)
		tn -= (tn - (cmd->driftturn/50))/8;

	if (dr != stplyr->kartstuff[k_drift]*16)
		dr -= (dr - (stplyr->kartstuff[k_drift]*16))/8;

	if (splitscreen == 1)
	{
		scale = (2*FRACUNIT)/3;
		y += FRACUNIT/vid.dup; // correct a one-pixel gap on the screen view (not the basevid view)
	}
	else if (splitscreen)
		scale = FRACUNIT/2;
	else
		scale = FRACUNIT;

	if (stplyr->mo)
	{
		INT32 dsone = K_GetKartDriftSparkValue(stplyr);
		INT32 dstwo = dsone*2;
		INT32 dsthree = dstwo*2;

#ifndef DONTLIKETOASTERSFPTWEAKS
		{
			const angle_t ang = R_PointToAngle2(0, 0, stplyr->rmomx, stplyr->rmomy) - stplyr->frameangle;
			// yes, the following is correct. no, you do not need to swap the x and y.
			fixed_t xoffs = -P_ReturnThrustY(stplyr->mo, ang, (BASEVIDWIDTH<<(FRACBITS-2))/2);
			fixed_t yoffs = -(P_ReturnThrustX(stplyr->mo, ang, 4*FRACUNIT) - 4*FRACUNIT);

			if (splitscreen)
				xoffs = FixedMul(xoffs, scale);

			xoffs -= (tn)*scale;
			xoffs -= (dr)*scale;

			if (stplyr->frameangle == stplyr->mo->angle)
			{
				const fixed_t mag = FixedDiv(stplyr->speed, 10*stplyr->mo->scale);

				if (mag < FRACUNIT)
				{
					xoffs = FixedMul(xoffs, mag);
					if (!splitscreen)
						yoffs = FixedMul(yoffs, mag);
				}
			}

			if (stplyr->mo->momz > 0) // TO-DO: Draw more of the kart so we can remove this if!
				yoffs += stplyr->mo->momz/3;

			if (encoremode)
				x -= xoffs;
			else
				x += xoffs;

			if (!splitscreen)
				y += yoffs;
		}

		const INT32 driftcharge = stplyr->kartstuff[k_driftcharge];

		// drift sparks!
		if ((leveltime & 1) && driftcharge)
		{
			if (driftcharge >= dsthree)
				colmap = R_GetTranslationColormap(TC_RAINBOW, K_RainbowColor(), GTC_CACHE);
			else if (driftcharge >= dstwo)
				colmap = R_GetTranslationColormap(TC_RAINBOW, SKINCOLOR_KETCHUP, GTC_CACHE);
			else if (driftcharge >= dsone)
				colmap = R_GetTranslationColormap(TC_RAINBOW, SKINCOLOR_SAPPHIRE, GTC_CACHE);
		}
		else
#endif
		// invincibility/grow/shrink!
		if (stplyr->mo->colorized && stplyr->mo->color)
			colmap = R_GetTranslationColormap(TC_RAINBOW, stplyr->mo->color, GTC_CACHE);
	}

	V_DrawFixedPatch(x, y, scale, splitflags, kp_fpview[target], colmap);

	pnum[stplyrnum]  = pn;
	turn[stplyrnum]  = tn;
	drift[stplyrnum] = dr;
}

// doesn't need to ever support 4p
static void K_drawInput(void)
{
	INT32 offs, col;
	static INT32 pn = 0;

	if (!cv_showinput.value || (timeinmap <= 105)) // dont bother
		return;

	INT32 target = 0, splitflags = (V_SNAPTOBOTTOM|V_SNAPTORIGHT|V_HUDTRANS);
	INT32 x = (BASEVIDWIDTH - 32 + cv_wheel_xoffset.value)*FRACUNIT, y = (BASEVIDHEIGHT - 24 + cv_wheel_yoffset.value)*FRACUNIT;

	const UINT8  hudcolor = K_GetHudColor();
	const INT32  accent1 = splitflags|colortranslations[hudcolor][5];
	const INT32  accent2 = splitflags|colortranslations[hudcolor][7];
	const UINT8 *hudcolormap = R_GetTranslationColormap(0, hudcolor, GTC_CACHE);

	const ticcmd_t *cmd = &stplyr->cmd;

	if (timeinmap < 113)
	{
		INT32 count = ((INT32)(timeinmap) - 105);
		INT32 frac = count > 0 && count < 6 ? R_GetTimeFrac(RTF_LEVEL) << (FRACBITS - count - 11) : 0;

		offs = 64*FRACUNIT;
		while (count-- > 0)
			offs >>= 1;
		x += (offs < FRACUNIT ? 0 : offs) - frac;
	}

#define BUTTW 8
#define BUTTH 11

#define drawbutt(xoffs, butt, symb)\
	if (cmd->buttons & butt)\
	{\
		offs = 2*FRACUNIT;\
		col = accent1;\
	}\
	else\
	{\
		offs = 0;\
		col = accent2;\
		V_DrawFill((x + xoffs*FRACUNIT)>>FRACBITS, (y + BUTTH*FRACUNIT)>>FRACBITS, BUTTW-1, 2, splitflags|31);\
	}\
	V_DrawFill((x + xoffs*FRACUNIT)>>FRACBITS, (y+offs)>>FRACBITS, BUTTW-1, BUTTH, col);\
	V_DrawFixedPatch(x + FRACUNIT + xoffs*FRACUNIT, y + offs + FRACUNIT, FRACUNIT, splitflags, tny_font[symb-HU_FONTSTART], NULL)

	drawbutt(-2*BUTTW, BT_ACCELERATE, 'A');
	drawbutt(  -BUTTW, BT_BRAKE,      'B');
	drawbutt(       0, BT_DRIFT,      'D');
	drawbutt(   BUTTW, BT_ATTACK,     'I');

#undef drawbutt

#undef BUTTW
#undef BUTTH

	y -= FRACUNIT;

	if (cv_showinput.value == 2 || cv_showinput.value == 3)
	{
		INT32 axis;
		INT32 hudforward = 0; // for the stick input display :chaosleep:
		UINT8 *shadowcolormap = NULL;
		const INT32 joyx = x>>FRACBITS;
		const INT32 joyy = y>>FRACBITS;
		static const INT32 joyxoffs = -8;
		static const INT32 joyyoffs = -24;
		const boolean usejoysprite  = (cv_showinput.value == 3 && joystickicon);

		// O backing
		if (usejoysprite)
		{
			shadowcolormap = R_GetTranslationColormap(0, SKINCOLOR_BLACK, GTC_CACHE);
			V_DrawMappedPatch(joyx+joyxoffs, joyy+joyyoffs-1, splitflags, joybacking, hudcolormap);
		}
		else
		{
			V_DrawFill(joyx+joyxoffs, joyy+joyyoffs-1, 16, 16, splitflags|accent2);
			V_DrawFill(joyx+joyxoffs, joyy+joyyoffs+15, 16, 1, splitflags|31);
		}

		// time for pain and suffering
		// kart does not have anything we can get analogue joystick y axis values from
		// during normal gameplay, so replicate shit here

		// this is horrid but we cant get actual input in replays so uhh
		if (demo.playback || !P_IsLocalPlayer(stplyr)) // yeah...........
		{
			hudforward = stplyr->kartstuff[k_throwdir] * KART_FULLTURN;
		}
		else
		{
			const boolean analogjoystickmove = cv_usejoystick[stplyrnum].value && !Joystick[stplyrnum].bGamepadStyle;
			const boolean gamepadjoystickmove = cv_usejoystick[stplyrnum].value && Joystick[stplyrnum].bGamepadStyle;
			const UINT8 ssplayer = stplyrnum+1;

			axis = JoyAxis(AXISAIM, ssplayer);

			if (analogjoystickmove && axis != 0)
			{
				// JOYAXISRANGE is supposed to be 1023 (divide by 1024)
				hudforward -= ((axis * KART_FULLTURN) / (JOYAXISRANGE-1));
			}
			else
			{
				if (InputDown(gc_aimforward, ssplayer) || (gamepadjoystickmove && axis < 0))
				{
					hudforward += KART_FULLTURN;
				}
				if (InputDown(gc_aimbackward, ssplayer) || (gamepadjoystickmove && axis > 0))
				{
					hudforward-= KART_FULLTURN;
				}
			}

			hudforward = CLAMP(hudforward, -KART_FULLTURN, KART_FULLTURN);
		}

		if (cmd->driftturn || hudforward)
		{
			INT16 turning = encoremode ? -cmd->driftturn : cmd->driftturn;

			if (usejoysprite)
			{
				V_DrawMappedPatch(joyx+joyxoffs+3-turning/80, joyy+joyyoffs+2-hudforward/80, splitflags, joyknob, shadowcolormap);
				V_DrawMappedPatch(joyx+joyxoffs+3-turning/64, joyy+joyyoffs+1-hudforward/64, splitflags, joyknob, hudcolormap);
			}
			else
			{
				// joystick hole
				V_DrawFill(joyx+joyxoffs+5, joyy+joyyoffs+4, 6, 6, splitflags|accent1);
				// joystick top and back
				V_DrawFill(joyx+joyxoffs+3-turning/80, joyy+joyyoffs+2-hudforward/80, 10, 10, splitflags|31);
				V_DrawFill(joyx+joyxoffs+3-turning/64, joyy+joyyoffs+1-hudforward/64, 10, 10, splitflags|accent1);
			}
		}
		else
		{
			if (usejoysprite)
			{
				V_DrawMappedPatch(joyx+joyxoffs+3, joyy+joyyoffs+8, splitflags, joyshadow, shadowcolormap);
				V_DrawMappedPatch(joyx+joyxoffs+3, joyy+joyyoffs+1, splitflags, joyknob, hudcolormap);
			}
			else
			{
				V_DrawFill(joyx+joyxoffs+3, joyy+joyyoffs+11, 10, 1, splitflags|accent2);
				V_DrawFill(joyx+joyxoffs+3, joyy+joyyoffs+1, 10, 10,splitflags|accent1);
			}
		}
	}
	else
	{
		if (!cmd->driftturn) // no turn
			target = 0;
		else // turning of multiple strengths!
		{
			target = ((abs(cmd->driftturn) - 1)/200)+1; // was 125, do we need another toggle for this? Zzz...
			if (target > 4)
				target = 4;
			if (cmd->driftturn < 0)
				target = -target;
		}

		if (pn != target)
		{
			if (abs(pn - target) == 1)
				pn = target;
			else if (pn < target)
				pn += 2;
			else //if (pn > target)
				pn -= 2;
		}

		if (pn < 0)
		{
			splitflags |= V_FLIP; // right turn
			x -= FRACUNIT;
		}

		target = abs(pn);
		if (target > 4)
			target = 4;

		V_DrawFixedPatch(x, y, FRACUNIT, splitflags, kp_inputwheel[target], hudcolormap);
	}
}

static void K_drawChallengerScreen(void)
{
	// This is an insanely complicated animation.
	static const UINT8 anim[52] = {
		0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13, // frame 1-14, 2 tics: HERE COMES A NEW slides in
		14,14,14,14,14,14, // frame 15, 6 tics: pause on the W
		15,16,17,18, // frame 16-19, 1 tic: CHALLENGER approaches screen
		19,20,19,20,19,20,19,20,19,20, // frame 20-21, 1 tic, 5 alternating: all text vibrates from impact
		21,22,23,24 // frame 22-25, 1 tic: CHALLENGER turns gold
	};
	const UINT8 offset = min(52-1, (3*TICRATE)-mapreset);

	V_DrawFadeScreen(0xFF00, 16); // Fade out
	V_DrawScaledPatch(0, 0, 0, kp_challenger[anim[offset]]);
}

static void K_drawLapStartAnim(void)
{
	// This is an EVEN MORE insanely complicated animation.
	const UINT8 progress = 80-stplyr->kartstuff[k_lapanimation];
	UINT8 *colormap = R_GetTranslationColormap(TC_DEFAULT, K_GetHudColor(), GTC_CACHE);
	INT32 vflags = V_SNAPTOTOP|V_HUDTRANS;

	fixed_t slideout = 32*(((progress - 76)*FRACUNIT) + R_GetTimeFrac(RTF_LEVEL));
	slideout = max(0, slideout);
	fixed_t slidein = 32*(((stplyr->kartstuff[k_lapanimation] - 76)*FRACUNIT) - R_GetTimeFrac(RTF_LEVEL));
	slidein = max(0, slidein);

	// First, draw the emblem and hand
	INT32 emblemx = (BASEVIDWIDTH << (FRACBITS - 1)) + slidein;
	INT32 y = 48*FRACUNIT - slideout;

	V_DrawFixedPatch(emblemx, y, FRACUNIT, vflags, kp_lapanim_emblem[modeattacking ? 1 : 0], colormap);

	INT32 hand = stplyr->kartstuff[k_laphand];
	if (hand >= 1 && hand <= 3)
	{
		y += 4*FRACUNIT - abs((int)(leveltime % 8)*FRACUNIT + R_GetTimeFrac(RTF_LEVEL) - 4*FRACUNIT);
		V_DrawFixedPatch(emblemx, y, FRACUNIT, vflags, kp_lapanim_hand[hand - 1], NULL);
	}

	// Then the text
	INT32 leftx = 82*FRACUNIT - slideout;
	INT32 rightx = 188*FRACUNIT + slideout;
	y = 30*FRACUNIT;

	if (stplyr->laps == (UINT8)(cv_numlaps.value - 1))
	{
		// FINAL
		V_DrawFixedPatch(leftx - 20*FRACUNIT, y, FRACUNIT, vflags, kp_lapanim_final[min(progress/2, 10)], NULL);

		// LAP
		if (progress/2 - 12 >= 0)
			V_DrawFixedPatch(rightx, y, FRACUNIT, vflags, kp_lapanim_lap[min(progress/2 - 12, 6)], NULL);
	}
	else
	{
		// LAP
		V_DrawFixedPatch(leftx, y, FRACUNIT, vflags, kp_lapanim_lap[min(progress/2, 6)], NULL);

		char *lapnum = va("%02d", stplyr->laps + 1);
		const size_t laplength = strlen(lapnum);

		for (int i = 0; i < (int)laplength; i++)
		{
			int digit = lapnum[i] - '0';
			int frame = min(2, progress/2 - 8 - (i*2));
			if (frame >= 0)
				V_DrawFixedPatch(rightx + (i*20*FRACUNIT), y, FRACUNIT, vflags, kp_lapanim_number[digit][frame], NULL);
		}
	}
}

void K_drawKartFreePlay(UINT32 flashtime)
{
	// no splitscreen support because it's not FREE PLAY if you have more than one player in-game
	if ((flashtime % TICRATE) < TICRATE/2)
		return;

	V_DrawKartString((BASEVIDWIDTH - (LAPS_X+1)) - (12*9), // mirror the laps thingy
		LAPS_Y+3, V_SNAPTOBOTTOM|V_SNAPTORIGHT|V_HUDTRANS, "FREE PLAY");
}

static void K_drawDistributionDebugger(void)
{
	patch_t *items[NUMKARTRESULTS] = {
		kp_sadface[1],
		kp_sneaker[1],
		kp_rocketsneaker[1],
		kp_invincibility[7],
		kp_banana[1],
		kp_eggman[1],
		kp_orbinaut[4],
		kp_jawz[1],
		kp_mine[1],
		kp_ballhog[1],
		kp_selfpropelledbomb[1],
		kp_grow[1],
		kp_shrink[1],
		kp_thundershield[1],
		kp_hyudoro[1],
		kp_pogospring[1],
		kp_kitchensink[1],

		kp_sneaker[1],
		kp_banana[1],
		kp_banana[1],
		kp_orbinaut[4],
		kp_orbinaut[4],
		kp_jawz[1]
	};
	INT32 useodds = 0;
	INT32 pingame = 0, bestbumper = 0;
	INT32 i;
	INT32 x = -9, y = -9;
	boolean dontforcespb = false;
	boolean spbrush = false;

	if (stplyrnum != 0) // only for p1
		return;

	// The only code duplication from the Kart, just to avoid the actual item function from calculating pingame twice
	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (!playeringame[i] || players[i].spectator)
			continue;
		pingame++;
		if (players[i].exiting)
			dontforcespb = true;
		if (players[i].kartstuff[k_bumper] > bestbumper)
			bestbumper = players[i].kartstuff[k_bumper];
	}

	if (G_RaceGametype())
		spbrush = (spbplace != -1 && stplyr->kartstuff[k_position] == spbplace+1);

	useodds = K_FindUseodds(stplyr, 0, pingame, bestbumper, spbrush, dontforcespb);

	for (i = 1; i < NUMKARTRESULTS; i++)
	{
		const INT32 itemodds = K_KartGetItemOdds(useodds, i, 0, spbrush);
		if (itemodds <= 0)
			continue;

		V_DrawScaledPatch(x, y, V_HUDTRANS|V_SNAPTOTOP, items[i]);
		V_DrawThinString(x+11, y+31, V_HUDTRANS|V_SNAPTOTOP, va("%d", itemodds));

		// Display amount for multi-items
		if (i >= NUMKARTITEMS)
		{
			INT32 amount;
			switch (i)
			{
				case KRITEM_TENFOLDBANANA:
					amount = 10;
					break;
				case KRITEM_QUADORBINAUT:
					amount = 4;
					break;
				case KRITEM_DUALJAWZ:
					amount = 2;
					break;
				default:
					amount = 3;
					break;
			}
			V_DrawString(x+24, y+31, V_ALLOWLOWERCASE|V_HUDTRANS|V_SNAPTOTOP, va("x%d", amount));
		}

		x += 32;
		if (x >= 297)
		{
			x = -9;
			y += 32;
		}
	}

	V_DrawString(0, 0, V_HUDTRANS|V_SNAPTOTOP, va("USEODDS %d", useodds));
}

static void K_drawCheckpointDebugger(void)
{
	if (stplyrnum != 0) // only for p1
		return;

	if (stplyr->starpostnum >= (numstarposts - (numstarposts/2)))
		V_DrawString(8, 184, 0, va("Checkpoint: %d / %d (Can finish)", stplyr->starpostnum, numstarposts));
	else
		V_DrawString(8, 184, 0, va("Checkpoint: %d / %d (Skip: %d)", stplyr->starpostnum, numstarposts, ((numstarposts/2) + stplyr->starpostnum)));

	V_DrawString(8, 192, 0, va("Waypoint dist: Prev %d, Next %d", stplyr->kartstuff[k_prevcheck], stplyr->kartstuff[k_nextcheck]));
}

// determines if gametype info (laps/bumpers) should be hidden
static boolean K_DisableGametypeInfo(void)
{
	if (!LUA_HudEnabled(hud_gametypeinfo))
	{
		return true;
	}

#ifdef ROTSPRITE
	if (splitscreen || (!cv_kartspeedometer.value))
	{
		// don't need to run checks if we're in splitscreen, or not using the speedometer
		return false;
	}

	const UINT8 speedostyle = K_GetSpeedometerStyle();

	if (G_RaceGametype())
	{
		if (speedostyle == SPEEDO_DIAL)
			return true;
	}
	else if (G_BattleGametype())
	{
		if ((cv_battlespeedo.value) && (speedostyle == SPEEDO_DIAL))
			return true;
	}
#endif

	return false;
}

void K_drawKartHUD(void)
{
	boolean isfreeplay = false;
	boolean battlefullscreen = false;
	boolean freecam = camera[stplyrnum].freecam;	//disable some hud elements w/ freecam
	boolean gameinfovisible = false;

	// Define the X and Y for each drawn object
	// This is handled by console/menu values
	K_initKartHUD();

	// Draw that fun first person HUD! Drawn ASAP so it looks more "real".
	if (!camera[stplyrnum].chase && !freecam)
		K_drawKartFirstPerson();

	// Draw full screen stuff that turns off the rest of the HUD
	if (mapreset && stplyrnum == 0)
	{
		K_drawChallengerScreen();
		return;
	}

	battlefullscreen = ((G_BattleGametype())
		&& (stplyr->exiting
		|| (stplyr->kartstuff[k_bumper] <= 0
		&& stplyr->kartstuff[k_comebacktimer]
		&& comeback
		&& stplyr->playerstate == PST_LIVE)));

	if (!demo.title && (!battlefullscreen || splitscreen))
	{
		// Draw the CHECK indicator before the other items, so it's overlapped by everything else

		if (LUA_HudEnabled(hud_check))	// delete lua when?
		{
			if (cv_kartcheck.value && !splitscreen && !players[displayplayers[0]].exiting && !freecam)
				K_drawKartPlayerCheck();
		}

		// Draw WANTED status
		if (G_BattleGametype())
		{
			if (LUA_HudEnabled(hud_wanted))
				K_drawKartWanted();
		}

		if (cv_kartminimap.value)
		{
			if (LUA_HudEnabled(hud_minimap))
				K_drawKartMinimap();
		}
	}

	if (battlefullscreen && !freecam)
	{
		if (LUA_HudEnabled(hud_battlefullscreen))
			K_drawBattleFullscreen();
		return;
	}

	// Draw the item window
	if (LUA_HudEnabled(hud_item) && !freecam)
		K_drawKartItem();

	if (cv_driftgauge.value && !modeattacking)
	{
		if (LUA_HudEnabled(hud_driftgauge))
			K_drawDriftGauge();
	}

	if (cv_nametag.value)
	{
		if (LUA_HudEnabled(hud_nametags))
			K_drawNameTags();
	}

	// If not splitscreen, draw...
	if (!splitscreen && !demo.title)
	{
		tic_t realtime = stplyr->realtime;

		if (cv_showlaptimes.value
			&& stplyr->kartstuff[k_lapanimation]
			&& !stplyr->exiting
			&& stplyr->laptime[LAP_LAST] != 0
			&& !midgamejoin) // due to vanilla compat, we cannot synch this oh well
		{
			if ((stplyr->kartstuff[k_lapanimation] / 5) & 1)
			{
				realtime = stplyr->laptime[LAP_LAST];
			}
			else
			{
				realtime = UINT32_MAX;
			}
		}

		// Draw the timestamp
		if (LUA_HudEnabled(hud_time))
			K_drawKartTimestamp(realtime, TIME_X, TIME_Y, gamemap, 0);

		if (!modeattacking)
		{
			// The top-four faces on the left
			//if (LUA_HudEnabled(hud_minirankings))
				isfreeplay = K_drawKartPositionFaces();
		}
	}

	if (!stplyr->spectator && !freecam) // Bottom of the screen elements, don't need in spectate mode
	{
		// get gametype info visibility ahead of time
		gameinfovisible = (!K_DisableGametypeInfo());

		if (!(splitscreen || demo.title))
		{
			if (LUA_HudEnabled(hud_inputdisplay))
				K_drawInput();
		}

		if (!demo.title && cv_showstats.value)
		{
			if (LUA_HudEnabled(hud_statdisplay))
				K_drawKartStats();
		}

		if (demo.title) // Draw title logo instead in demo.titles
		{
			INT32 x = (BASEVIDWIDTH - 32)*FRACUNIT, y = 128*FRACUNIT, offs;
			INT32 logoflags = V_SNAPTORIGHT|V_SNAPTOBOTTOM;

			if (splitscreen == 3)
			{
				x = BASEVIDWIDTH/2 + 10;
				y = BASEVIDHEIGHT/2 - 30;
			}

			if (timeinmap < 113)
			{
				INT32 count = ((INT32)(timeinmap) - 104);
				INT32 frac = count > 0 ? R_GetTimeFrac(RTF_LEVEL) << max(0, FRACBITS - count - 9) : 0;

				offs = 256*FRACUNIT;
				while (count-- > 0)
					offs >>= 1;
				x += offs - frac;
			}

			V_DrawSciencePatch(x - (54*FRACUNIT), y, logoflags, W_CachePatchName("TTKBANNR", PU_PATCH), FRACUNIT/4);
			V_DrawSciencePatch(x - (54*FRACUNIT), y + (25*FRACUNIT), logoflags, W_CachePatchName("TTKART", PU_PATCH), FRACUNIT/4);
		}
		else if (G_RaceGametype()) // Race-only elements
		{
			// Draw the lap counter
			if (gameinfovisible)
				K_drawKartLaps();

			if (!splitscreen)
			{
				// Draw the speedometer
				// TODO: Make a better speedometer. << done :p
				if (LUA_HudEnabled(hud_speedometer))
					K_drawKartSpeedometer();
			}

			if (isfreeplay)
				;
			else if (!modeattacking)
			{
				// Draw the numerical position
				if (LUA_HudEnabled(hud_position))
					K_DrawKartPositionNum(stplyr->kartstuff[k_position]);
			}
		}
		else if (G_BattleGametype()) // Battle-only
		{
			// Draw the hits left!
			if (gameinfovisible)
				K_drawKartBumpersOrKarma();

			if ((!splitscreen) && cv_battlespeedo.value)
			{
				// Draw the speedometer but in battle
				// TODO: Make a better speedometer. << done :p
				if (LUA_HudEnabled(hud_speedometer))
					K_drawKartSpeedometer();
			}
		}
	}

	// Draw the countdowns after everything else.
	if (leveltime >= starttime-(3*TICRATE)
		&& leveltime < starttime+TICRATE)
		K_drawKartStartCountdown();
	else if (racecountdown && (!splitscreen || !stplyr->exiting))
	{
		char *countstr = va("%d", racecountdown/TICRATE);

		// Eeeh dunno if would be better to add another variable instead of LAPS_Y but this works too
		int yoff = cv_dnft_yoffset.value - cv_laps_yoffset.value;
		int xoff = cv_dnft_xoffset.value;

		if (splitscreen > 1)
			V_DrawCenteredString(BASEVIDWIDTH/4+xoff, LAPS_Y+1+yoff, K_calcSplitFlags(V_SNAPTOBOTTOM), countstr);
		else
		{
			INT32 karlen = strlen(countstr)*6; // half of 12
			V_DrawKartString((BASEVIDWIDTH/2)-karlen+xoff, LAPS_Y+3+yoff, K_calcSplitFlags(V_SNAPTOBOTTOM), countstr);
		}
	}

	// Race overlays
	if (G_RaceGametype() && !freecam)
	{
		if (stplyr->exiting)
			K_drawKartFinish();
		else if (stplyr->kartstuff[k_lapanimation] && !splitscreen && cv_showlapemblem.value)
			K_drawLapStartAnim();
	}

	if (modeattacking || freecam) // everything after here is MP and debug only
		return;

	if (G_BattleGametype() && !splitscreen && (stplyr->kartstuff[k_yougotem] % 2)) // * YOU GOT EM *
		V_DrawScaledPatch(BASEVIDWIDTH/2 - (kp_yougotem->width/2), 32, V_HUDTRANS, kp_yougotem);

	// Draw FREE PLAY.
	if (isfreeplay && cv_showfreeplay.value && !stplyr->spectator && timeinmap > 113)
	{
		if (LUA_HudEnabled(hud_freeplay))
			K_drawKartFreePlay(leveltime);
	}

	if (cv_kartdebugdistribution.value)
		K_drawDistributionDebugger();

	if (cv_kartdebugcheckpoint.value)
		K_drawCheckpointDebugger();

	if (cv_kartdebugdirector.value)
		K_DrawDirectorDebugger();

	if (cv_kartdebugnodes.value)
	{
		UINT8 p;
		for (p = 0; p < MAXPLAYERS; p++)
			V_DrawString(8, 64+(8*p), V_YELLOWMAP, va("%d - %d (%dl)", p, playernode[p], players[p].cmd.latency));
	}

	if (cv_kartdebugcolorize.value && stplyr->mo && stplyr->mo->skin)
	{
		INT32 x = 0, y = 0;
		UINT8 c;

		for (c = 1; c < MAXSKINCOLORS; c++)
		{
			UINT8 *cm = R_GetTranslationColormap(TC_RAINBOW, c, GTC_CACHE);
			V_DrawFixedPatch(x<<FRACBITS, y<<FRACBITS, FRACUNIT>>1, 0, facewantprefix[stplyr->skin], cm);

			x += 16;
			if (x > BASEVIDWIDTH-16)
			{
				x = 0;
				y += 16;
			}
		}
	}
}
