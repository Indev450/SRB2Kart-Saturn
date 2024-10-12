// SONIC ROBO BLAST 2
//-----------------------------------------------------------------------------
// Copyright (C) 1998-2000 by DooM Legacy Team.
// Copyright (C) 1999-2018 by Sonic Team Junior.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  console.c
/// \brief Console drawing and input

#ifdef __GNUC__
#include <unistd.h>
#endif

#include "doomdef.h"
#include "console.h"
#include "g_game.h"
#include "g_input.h"
#include "hu_stuff.h"
#include "keys.h"
#include "r_main.h"
#include "r_defs.h"
#include "sounds.h"
#include "st_stuff.h"
#include "s_sound.h"
#include "v_video.h"
#include "i_video.h"
#include "z_zone.h"
#include "i_system.h"
#include "i_threads.h"
#include "d_main.h"
#include "m_menu.h"
#include "m_textinput.h"
#include "filesrch.h"

#ifdef HWRENDER
#include "hardware/hw_main.h"
#endif

#define MAXHUDLINES 20

#ifdef HAVE_THREADS
I_mutex con_mutex;

// g_in_exiting_signal_handler is an evil hack
// to avoid infinite SIGABRT recursion in the signal handler
// due to poisoned locks or mach-o kernel not supporting locks in signals
// or something like that. idk
#  define Lock_state()    if (!g_in_exiting_signal_handler) { I_lock_mutex(&con_mutex); }
#  define Unlock_state()  if (!g_in_exiting_signal_handler) { I_unlock_mutex(con_mutex); }
#else/*HAVE_THREADS*/
#  define Lock_state()
#  define Unlock_state()
#endif/*HAVE_THREADS*/

static boolean con_started = false; // console has been initialised
       boolean con_startup = false; // true at game startup, screen need refreshing
static boolean con_forcepic = true; // at startup toggle console translucency when first off
       boolean con_recalc;          // set true when screen size has changed

static tic_t con_tick; // console ticker for anim or blinking prompt cursor
                        // con_scrollup should use time (currenttime - lasttime)..

static boolean consoletoggle; // true when console key pushed, ticker will handle
static boolean consoleready;  // console prompt is ready

       INT32 con_destlines; // vid lines used by console at final position
static INT32 con_curlines;  // vid lines currently used by console

       INT32 con_clipviewtop; // (useless)

static UINT8  con_hudlines;             // number of console heads up message lines
static UINT32 con_hudtime[MAXHUDLINES]; // remaining time of display for hud msg lines

       INT32 con_clearlines;      // top screen lines to refresh when view reduced
       boolean con_hudupdate;   // when messages scroll, we need a backgrnd refresh

// console text output
static char *con_line;          // console text output current line
static size_t con_cx;           // cursor position in current line
static size_t con_cy;           // cursor line number in con_buffer, is always
                                // increasing, and wrapped around in the text
                                // buffer using modulo.

static size_t con_totallines;      // lines of console text into the console buffer
static size_t con_width;           // columns of chars, depend on vid mode width

static size_t con_scrollup;        // how many rows of text to scroll up (pgup/pgdn)
UINT32 con_scalefactor;            // text size scale factor

// hold 32 last lines of input for history
#define CON_MAXPROMPTCHARS 256
#define CON_PROMPTCHAR '$'

static char inputlines[32][CON_MAXPROMPTCHARS]; // hold last 32 prompt lines

static INT32 inputline;    // current input line number
static INT32 inputhist;    // line number of history input line to restore
static textinput_t input;
// notice: input does NOT include the "$" at the start of the line. - 11/3/16

// protos.
static void CON_InputInit(void);
static void CON_RecalcSize(void);
static void CON_ChangeHeight(void);

static void CONS_hudlines_Change(void);
static void CONS_backcolor_Change(void);

//======================================================================
//                   CONSOLE VARS AND COMMANDS
//======================================================================
#ifdef macintosh
#define CON_BUFFERSIZE 4096 // my compiler can't handle local vars >32k
#else
#define CON_BUFFERSIZE 32768
#endif

static char con_buffer[CON_BUFFERSIZE];

// CV_Unsigned can overflow when multiplied by TICRATE later, so let's use a 3-year limit instead
static CV_PossibleValue_t hudtime_cons_t[] = {{0, "MIN"}, {99999999, "MAX"}, {0, NULL}};
static consvar_t cons_hudtime = {"con_hudtime", "5", CV_SAVE, hudtime_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL};

// number of lines displayed on the HUD
static CV_PossibleValue_t hudlines_cons_t[] = {{0, "MIN"}, {MAXHUDLINES, "MAX"}, {0, NULL}};
static consvar_t cons_hudlines = {"con_hudlines", "5", CV_CALL|CV_SAVE, hudlines_cons_t, CONS_hudlines_Change, 0, NULL, NULL, 0, 0, NULL};

// number of lines console move per frame
// (con_speed needs a limit, apparently)
static CV_PossibleValue_t speed_cons_t[] = {{0, "MIN"}, {64, "MAX"}, {0, NULL}};
static consvar_t cons_speed = {"con_speed", "8", CV_SAVE, speed_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL};

// percentage of screen height to use for console
static consvar_t cons_height = {"con_height", "50", CV_SAVE, CV_Unsigned, NULL, 0, NULL, NULL, 0, 0, NULL};

static CV_PossibleValue_t backpic_cons_t[] = {{0, "translucent"}, {1, "picture"}, {0, NULL}};
// whether to use console background picture, or translucent mode
static consvar_t cons_backpic = {"con_backpic", "translucent", CV_SAVE, backpic_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL};

static CV_PossibleValue_t backcolor_cons_t[] = {{0, "White"}, 		{1, "Black"},		{2, "Sepia"},
												{3, "Brown"},		{4, "Pink"},		{5, "Raspberry"},
												{6, "Red"},			{7, "Creamsicle"},	{8, "Orange"},
												{9, "Gold"},		{10,"Yellow"},		{11,"Emerald"},
												{12,"Green"},		{13,"Cyan"},		{14,"Steel"},
												{15,"Periwinkle"},	{16,"Blue"},		{17,"Purple"},
												{18,"Lavender"},
												{0, NULL}};
consvar_t cons_backcolor = {"con_backcolor", "Black", CV_CALL|CV_SAVE, backcolor_cons_t, CONS_backcolor_Change, 0, NULL, NULL, 0, 0, NULL};

static CV_PossibleValue_t menuhighlight_cons_t[] =
{
	{0, 			"Gametype Default"},
	{V_YELLOWMAP, 	"Always Yellow"},
	{V_PURPLEMAP, 	"Always Purple"},
	{V_GREENMAP, 	"Always Green"},
	{V_BLUEMAP, 	"Always Blue"},
	{V_REDMAP, 		"Always Red"},
	{V_GRAYMAP, 	"Always Gray"},
	{V_ORANGEMAP, 	"Always Orange"},
	{V_SKYMAP, 		"Always Sky-Blue"},
	{V_GOLDMAP, 	"Always Gold"},
	{V_LAVENDERMAP, "Always Lavender"},
	{V_TEAMAP, 		"Always Tea-Green"},
	{V_STEELMAP,	"Always Steel-Blue"},
	{V_PINKMAP, 	"Always Pink"},
	{V_BROWNMAP, 	"Always Brown"},
	{V_PEACHMAP, 	"Always Peach"},
	{0, NULL}
};
consvar_t cons_menuhighlight = {"menuhighlight", "Gametype Default", CV_SAVE, menuhighlight_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL};

static void CON_Print(char *msg);

//
//
static void CONS_hudlines_Change(void)
{
	INT32 i;

	Lock_state();

	// Clear the currently displayed lines
	for (i = 0; i < con_hudlines; i++)
		con_hudtime[i] = 0;

	con_hudlines = cons_hudlines.value;

	Unlock_state();

	CONS_Printf(M_GetText("Number of console HUD lines is now %d\n"), con_hudlines);
}

// Clear console text buffer
//
static void CONS_Clear_f(void)
{
	Lock_state();

	memset(con_buffer, 0, CON_BUFFERSIZE);

	con_cx = 0;
	con_cy = con_totallines-1;
	con_line = &con_buffer[con_cy*con_width];
	con_scrollup = 0;

	Unlock_state();
}

// Choose english keymap
//
/*static void CONS_English_f(void)
{
	shiftxform = english_shiftxform;
	CONS_Printf(M_GetText("%s keymap.\n"), M_GetText("English"));
}*/

static char *bindtable[NUMINPUTS];

static void CONS_Bind_f(void)
{
	size_t na;
	INT32 key;

	na = COM_Argc();

	if (na != 2 && na != 3)
	{
		CONS_Printf(M_GetText("bind <keyname> [<command>]: create shortcut keys to command(s)\n"));
		CONS_Printf("\x82%s", M_GetText("Bind table :\n"));
		na = 0;
		for (key = 0; key < NUMINPUTS; key++)
			if (bindtable[key])
			{
				CONS_Printf("%s : \"%s\"\n", G_KeynumToString(key), bindtable[key]);
				na = 1;
			}
		if (!na)
			CONS_Printf(M_GetText("(empty)\n"));
		return;
	}

	key = G_KeyStringtoNum(COM_Argv(1));
	if (key <= 0 || key >= NUMINPUTS)
	{
		CONS_Alert(CONS_NOTICE, M_GetText("Invalid key name\n"));
		return;
	}

	Z_Free(bindtable[key]);
	bindtable[key] = NULL;

	if (na == 3)
		bindtable[key] = Z_StrDup(COM_Argv(2));
}

//======================================================================
//                          CONSOLE SETUP
//======================================================================

// Console BG color
UINT8 *consolebgmap = NULL;

void CON_SetupBackColormap(void)
{
	UINT16 i, palsum;
	UINT8 j, palindex;
	UINT8 *pal = W_CacheLumpName(GetPalette(), PU_CACHE);
	INT32 shift = 6;

	if (!consolebgmap)
		consolebgmap = (UINT8 *)Z_Malloc(256, PU_STATIC, NULL);

	switch (cons_backcolor.value)
	{
		case 0:		palindex = 15; 	break; 	// White
		case 1:		palindex = 31;	break; 	// Gray
		case 2:		palindex = 47;	break;	// Sepia
		case 3:		palindex = 63;	break; 	// Brown
		case 4:		palindex = 150; shift = 7; 	break; 	// Pink
		case 5:		palindex = 127; shift = 7;	break; 	// Raspberry
		case 6:		palindex = 143;	break; 	// Red
		case 7:		palindex = 86;	shift = 7;	break;	// Creamsicle
		case 8:		palindex = 95;	break; 	// Orange
		case 9:		palindex = 119; shift = 7;	break; 	// Gold
		case 10:	palindex = 111;	break; 	// Yellow
		case 11:	palindex = 191; shift = 7; 	break; 	// Emerald
		case 12:	palindex = 175;	break; 	// Green
		case 13:	palindex = 219;	break; 	// Cyan
		case 14:	palindex = 207; shift = 7;	break; 	// Steel
		case 15:	palindex = 230;	shift = 7; 	break; 	// Periwinkle
		case 16:	palindex = 239;	break; 	// Blue
		case 17:	palindex = 199; shift = 7; 	break; 	// Purple
		case 18:	palindex = 255; shift = 7; 	break; 	// Lavender
		// Default green
		default:	palindex = 175; break;

}

	// setup background colormap
	for (i = 0, j = 0; i < 768; i += 3, j++)
	{
		palsum = (pal[i] + pal[i+1] + pal[i+2]) >> shift;
		consolebgmap[j] = (UINT8)(palindex - palsum);
	}
}

static void CONS_backcolor_Change(void)
{
	CON_SetupBackColormap();
}

// Font colormap colors
// TODO: This could probably be improved somehow...
// These colormaps are 99% identical, with just a few changed bytes
UINT8 *yellowmap, *purplemap, *greenmap, *bluemap, *graymap, *redmap, *orangemap,\
 *skymap, *goldmap, *lavendermap, *teamap, *steelmap, *pinkmap, *brownmap, *peachmap;

static void CON_SetupColormaps(void)
{
	INT32 i;
	UINT8 *memorysrc = (UINT8 *)Z_Malloc((256*15), PU_STATIC, NULL);

	purplemap   = memorysrc;
	yellowmap   = (purplemap+256);
	greenmap    = (yellowmap+256);
	bluemap     = (greenmap+256);
	redmap      = (bluemap+256);
	graymap     = (redmap+256);
	orangemap   = (graymap+256);
	skymap      = (orangemap+256);
	lavendermap = (skymap+256);
	goldmap     = (lavendermap+256);
	teamap      = (goldmap+256);
	steelmap    = (teamap+256);
	pinkmap     = (steelmap+256);
	brownmap    = (pinkmap+256);
	peachmap    = (brownmap+256);

	// setup the other colormaps, for console text

	// these don't need to be aligned, unless you convert the
	// V_DrawMappedPatch() into optimised asm.

	for (i = 0; i < (256*15); i++, ++memorysrc)
		*memorysrc = (UINT8)(i & 0xFF); // remap each color to itself...

	// SRB2Kart: Different console font, new colors
	purplemap[120]   = (UINT8)194;
	yellowmap[120]   = (UINT8)103;
	greenmap[120]    = (UINT8)162;
	bluemap[120]     = (UINT8)228;
	redmap[120]      = (UINT8)126; // battle
	graymap[120]     =  (UINT8)10;
	orangemap[120]   =  (UINT8)85; // record attack
	skymap[120]      = (UINT8)214; // race
	lavendermap[120] = (UINT8)248;
	goldmap[120]     = (UINT8)114;
	teamap[120]      = (UINT8)177;
	steelmap[120]    = (UINT8)201;
	pinkmap[120]     = (UINT8)145;
	brownmap[120]    =  (UINT8)48;
	peachmap[120]    =  (UINT8)69; // nice

	// Init back colormap
	CON_SetupBackColormap();
}

//
// Setup the console text buffer
//
void CON_Init(void)
{
	INT32 i;

	for (i = 0; i < NUMINPUTS; i++)
		bindtable[i] = NULL;

	Lock_state();

	// clear all lines
	memset(con_buffer, 0, CON_BUFFERSIZE);

	// make sure it is ready for the loading screen
	con_width = 0;

	Unlock_state();

	CON_RecalcSize();

	CON_SetupColormaps();

	Lock_state();

	//note: CON_Ticker should always execute at least once before D_Display()
	con_clipviewtop = -1; // -1 does not clip

	con_hudlines = atoi(cons_hudlines.defaultvalue);

	Unlock_state();

	// setup console input filtering
	CON_InputInit();

	// register our commands
	//
	COM_AddCommand("cls", CONS_Clear_f);
	//COM_AddCommand("english", CONS_English_f);
	// set console full screen for game startup MAKE SURE VID_Init() done !!!
	Lock_state();

	con_destlines = vid.height;
	con_curlines = vid.height;

	Unlock_state();

	if (!dedicated)
	{
		Lock_state();

		con_started = true;
		con_startup = true; // need explicit screen refresh until we are in Doom loop
		consoletoggle = false;

		Unlock_state();

		CV_RegisterVar(&cons_hudtime);
		CV_RegisterVar(&cons_hudlines);
		CV_RegisterVar(&cons_speed);
		CV_RegisterVar(&cons_height);
		CV_RegisterVar(&cons_backpic);
		CV_RegisterVar(&cons_backcolor);
		CV_RegisterVar(&cons_menuhighlight);
		COM_AddCommand("bind", CONS_Bind_f);
	}
	else
	{
		Lock_state();

		con_started = true;
		con_startup = false; // need explicit screen refresh until we are in Doom loop
		consoletoggle = true;

		Unlock_state();
	}
}

//
// Console input initialization
//
static void CON_InputInit(void)
{
	Lock_state();

	// prepare the first prompt line
	memset(inputlines, 0, sizeof (inputlines));
	inputline = 0;

	M_TextInputInit(&input, inputlines[inputline], CON_MAXPROMPTCHARS);

	Unlock_state();
}

//======================================================================
//                        CONSOLE EXECUTION
//======================================================================

// Called at screen size change to set the rows and line size of the
// console text buffer.
//
static void CON_RecalcSize(void)
{
	size_t conw, oldcon_width, oldnumlines, i, oldcon_cy;
	char *tmp_buffer;
	char *string;

	Lock_state();

	switch (cv_constextsize.value)
	{
	case V_NOSCALEPATCH:
		con_scalefactor = 1;
		break;
	case V_SMALLSCALEPATCH:
		con_scalefactor = vid.smalldupx;
		break;
	case V_MEDSCALEPATCH:
		con_scalefactor = vid.meddupx;
		break;
	default:	// Full scaling
		con_scalefactor = vid.dupx;
		break;
	}

	con_recalc = false;

	if (dedicated)
		conw = 1;
	else
		conw = (vid.width>>3) / con_scalefactor - 2;

	if (con_curlines == vid.height) // first init
	{
		con_curlines = vid.height;
		con_destlines = vid.height;
	}

	if (con_destlines > 0) // Resize console if already open
	{
		CON_ChangeHeight();
		con_curlines = con_destlines;
	}

	// check for change of video width
	if (conw == con_width)
	{
		Unlock_state();
		return; // didn't change
	}

	Unlock_state();

	tmp_buffer = Z_Malloc(CON_BUFFERSIZE, PU_STATIC, NULL);
	string = Z_Malloc(CON_BUFFERSIZE, PU_STATIC, NULL); // BP: it is a line but who know

	Lock_state();

	oldcon_width = con_width;
	oldnumlines = con_totallines;
	oldcon_cy = con_cy;
	M_Memcpy(tmp_buffer, con_buffer, CON_BUFFERSIZE);

	if (conw < 1)
		con_width = (BASEVIDWIDTH>>3) - 2;
	else
		con_width = conw;

	con_width += 11; // Graue 06-19-2004 up to 11 control chars per line

	con_totallines = CON_BUFFERSIZE / con_width;
	memset(con_buffer, ' ', CON_BUFFERSIZE);

	con_cx = 0;
	con_cy = con_totallines-1;
	con_line = &con_buffer[con_cy*con_width];
	con_scrollup = 0;

	Unlock_state();

	// re-arrange console text buffer to keep text
	if (oldcon_width) // not the first time
	{
		for (i = oldcon_cy + 1; i < oldcon_cy + oldnumlines; i++)
		{
			if (tmp_buffer[(i%oldnumlines)*oldcon_width])
			{
				M_Memcpy(string, &tmp_buffer[(i%oldnumlines)*oldcon_width], oldcon_width);
				conw = oldcon_width - 1;
				while (string[conw] == ' ' && conw)
					conw--;
				string[conw+1] = '\n';
				string[conw+2] = '\0';
				CON_Print(string);
			}
		}
	}

	Z_Free(string);
	Z_Free(tmp_buffer);
}

static void CON_ChangeHeight(void)
{
	INT32 minheight;

	Lock_state();

	minheight = 20 * con_scalefactor;	// 20 = 8+8+4

	// toggle console in
	con_destlines = (cons_height.value*vid.height)/100;
	if (con_destlines < minheight)
		con_destlines = minheight;
	else if (con_destlines > vid.height)
		con_destlines = vid.height;

	con_destlines &= ~0x3; // multiple of text row height

	Unlock_state();
}

// Handles Console moves in/out of screen (per frame)
//
static void CON_MoveConsole(void)
{
	static fixed_t fracmovement = 0;

	Lock_state();

	// instant
	if (!cons_speed.value)
	{
		con_curlines = con_destlines;
		Unlock_state();
		return;
	}

	// Not instant - Increment fracmovement fractionally
	fracmovement += FixedMul(cons_speed.value*vid.fdupy, renderdeltatics);

	if (con_curlines < con_destlines) // Move the console downwards
	{
		con_curlines += FixedInt(fracmovement); // Move by fracmovement's integer value
		if (con_curlines > con_destlines) // If we surpassed the destination...
			con_curlines = con_destlines; // ...clamp to it!
	}
	else // Move the console upwards
	{
		con_curlines -= FixedInt(fracmovement);
		if (con_curlines < con_destlines)
			con_curlines = con_destlines;
		
		if (con_destlines == 0) // If the console is being closed, not just moved up...
			con_tick = 0; // ...don't show the blinking cursor
	}
	
	fracmovement %= FRACUNIT; // Reset fracmovement's integer value, but keep the fraction

	Unlock_state();
}

INT32 CON_ShiftChar(INT32 ch)
{
	if (I_UseNativeKeyboard())
		return ch;
	
	if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z'))
	{
		if (cv_keyboardlayout.value == 3)
		{
			if (shiftdown ^ capslock)
				ch = shiftxform[ch];
			else if (altdown & 0x2)
				ch = french_altgrxform[ch];
			else
				ch = HU_FallBackFrSpecialLetter(ch);
		}
		else
		{
			if (shiftdown ^ capslock)
				ch = shiftxform[ch];
		}
	}
	else	// if we're holding shift we should still shift non letter symbols
	{
		if (cv_keyboardlayout.value == 3)
		{
			if (shiftdown)
				ch = shiftxform[ch];
			else if (altdown & 0x2)
				ch = french_altgrxform[ch];
			else
				ch = HU_FallBackFrSpecialLetter(ch);
		}
		else
		{
			if (shiftdown)
				ch = shiftxform[ch];
		}
	}

	return ch;
}

INT32 CON_ShitAndAltGrChar(INT32 ch)
{
	if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z'))
	{
		if (shiftdown ^ capslock)
			ch = shiftxform[ch];
	}
	else	// if we're holding shift we should still shift non letter symbols
	{
		if (shiftdown)
			ch = shiftxform[ch];
		else if (altdown & 0x2)
		{
			ch = french_altgrxform[ch];
		}
		else
		{
			ch = HU_FallBackFrSpecialLetter(ch);
		}
	}

	return ch;
}

// Clear time of console heads up messages
//
void CON_ClearHUD(void)
{
	INT32 i;

	Lock_state();

	for (i = 0; i < con_hudlines; i++)
		con_hudtime[i] = 0;

	Unlock_state();
}

// Force console to move out immediately
// note: con_ticker will set consoleready false
void CON_ToggleOff(void)
{
	Lock_state();

	if (!con_destlines)
	{
		Unlock_state();
		return;
	}

	con_destlines = 0;
	con_curlines = 0;
	CON_ClearHUD();
	con_forcepic = 0;
	con_clipviewtop = -1; // remove console clipping of view

	Unlock_state();
}

boolean CON_Ready(void)
{
	boolean ready;
	Lock_state();
	{
		ready = consoleready;
	}
	Unlock_state();
	return ready;
}

// Console ticker: handles console move in/out, cursor blinking
//
void CON_Ticker(void)
{
	INT32 i;
	INT32 minheight;

	Lock_state();

	minheight = 20 * con_scalefactor;	// 20 = 8+8+4

	// cursor blinking
	con_tick++;
	con_tick &= 7;

	// console key was pushed
	if (consoletoggle)
	{
		consoletoggle = false;

		// toggle off console
		if (con_destlines > 0)
		{
			con_destlines = 0;
			CON_ClearHUD();
		}
		else
			CON_ChangeHeight();
	}

	// clip the view, so that the part under the console is not drawn
	con_clipviewtop = -1;
	if (cons_backpic.value) // clip only when using an opaque background
	{
		if (con_curlines > 0)
			con_clipviewtop = con_curlines - viewwindowy - 1 - 10;
		// NOTE: BIG HACK::SUBTRACT 10, SO THAT WATER DON'T COPY LINES OF THE CONSOLE
		//       WINDOW!!! (draw some more lines behind the bottom of the console)
		if (con_clipviewtop < 0)
			con_clipviewtop = -1; // maybe not necessary, provided it's < 0
	}

	// check if console ready for prompt
	if (con_destlines >= minheight)
		consoleready = true;
	else
		consoleready = false;

	// make overlay messages disappear after a while
	for (i = 0; i < con_hudlines; i++)
	{
		if (con_hudtime[i])
			con_hudtime[i]--;
	}

	Unlock_state();
}

//
// ----
//

// Handles console key input
//
boolean CON_Responder(event_t *ev)
{
	static UINT8 consdown = false; // console is treated differently due to rare usage

	// sequential completions a la 4dos
	static char completioncmd[80 + sizeof("find ")] = "find ";
	static char *completion = &completioncmd[sizeof("find ")-1];

	static INT32 comskips, varskips;

	const char *cmd = "";
	INT32 key;

	if (chat_on)
		return false;

	// let go keyup events, don't eat them
	if (ev->type != ev_keydown && ev->type != ev_console)
	{
		if (ev->data1 == gamecontrol[gc_console][0] || ev->data1 == gamecontrol[gc_console][1])
			consdown = false;
		return false;
	}

	key = ev->data1;

	// check for console toggle key
	if (ev->type != ev_console)
	{
		if (modeattacking || metalrecording)
			return false;

		if (ev->data1 >= KEY_MOUSE1) // See also: HUD_Responder
		{
			INT32 i;
			for (i = 0; i < num_gamecontrols; i++)
			{
				if (gamecontrol[i][0] == ev->data1 || gamecontrol[i][1] == ev->data1)
					break;
			}

			if (i == num_gamecontrols)
				return false;
		}

		if (key == gamecontrol[gc_console][0] || key == gamecontrol[gc_console][1])
		{
			if (consdown) // ignore repeat
				return true;
			consoletoggle = true;
			consdown = true;
			return true;
		}

		// check other keys only if console prompt is active
		if (!consoleready && key < NUMINPUTS) // metzgermeister: boundary check!!
		{
			if (!menuactive && bindtable[key])
			{
				COM_BufAddText(bindtable[key]);
				COM_BufAddText("\n");
				return true;
			}
			return false;
		}

		// escape key toggle off console
		if (key == KEY_ESCAPE)
		{
			consoletoggle = true;
			return true;
		}
	}

	Lock_state();
	M_TextInputHandle(&input, key);
	Unlock_state();

	// Always eat ctrl/shift/alt if console open, so the menu doesn't get ideas
	if (key == KEY_LSHIFT || key == KEY_RSHIFT
	 || key == KEY_LCTRL || key == KEY_RCTRL
	 || key == KEY_LALT || key == KEY_RALT)
		return true;

	// ctrl modifier -- changes behavior, adds shortcuts
	if ((cv_keyboardlayout.value != 3 && ctrldown) || (cv_keyboardlayout.value == 3 && ctrldown && !altdown))
	{
		// show all cvars/commands that match what we have inputted
		if (key == KEY_TAB)
		{
			if (!completion[0])
			{
				if (!input.length || input.length >= 40 || strchr(input.buffer, ' '))
					return true;
				strcpy(completion, input.buffer);
				comskips = varskips = 0;
			}

			COM_BufInsertText(completioncmd);

			return true;
		}
		// ---

		if (key == KEY_HOME) // oldest text in buffer
		{
			con_scrollup = (con_totallines-((con_curlines-16)>>3));
			return true;
		}
		else if (key == KEY_END) // most recent text in buffer
		{
			con_scrollup = 0;
			return true;
		}

		// Those are already handled in M_TextInputHandle, but they do extra logic... Maybe textinput_t should have some sort of callbacks?
		if (key == 'x' || key == 'X' || key == 'v' || key == 'V')
			completion[0] = 0;

		// ...why shouldn't it eat the key? if it doesn't, it just means you
		// can control Sonic from the console, which is silly
		return true;//return false;
	}

	// command completion forward (tab) and backward (shift-tab)
	if (key == KEY_TAB)
	{
		// sequential command completion forward and backward

		// remember typing for several completions (a-la-4dos)
		if (!completion[0])
		{
			if (!input.length || input.length >= 40 || strchr(input.buffer, ' '))
				return true;
			strcpy(completion, input.buffer);
			comskips = varskips = 0;
		}
		else
		{
			if (shiftdown)
			{
				if (comskips < 0)
				{
					if (--varskips < 0)
						comskips = -comskips - 2;
				}
				else if (comskips > 0) comskips--;
			}
			else
			{
				if (comskips < 0) varskips++;
				else              comskips++;
			}
		}

		if (comskips >= 0)
		{
			cmd = COM_CompleteCommand(completion, comskips);
			if (!cmd) // dirty: make sure if comskips is zero, to have a neg value
				comskips = -comskips - 1;
		}
		if (comskips < 0)
			cmd = CV_CompleteVar(completion, varskips);

		if (cmd)
		{
			Lock_state();
			M_TextInputSetString(&input, va("%s ", cmd));
			Unlock_state();
		}
		else
		{
			if (comskips > 0)
				comskips--;
			else if (varskips > 0)
				varskips--;
		}

		return true;
	}

	// move up (backward) in console textbuffer
	if (key == KEY_PGUP)
	{
		if (con_scrollup < (con_totallines-((con_curlines-16)>>3)))
			con_scrollup++;
		return true;
	}
	else if (key == KEY_PGDN)
	{
		if (con_scrollup > 0)
			con_scrollup--;
		return true;
	}

	// At this point we're messing with input
	// Clear completion
	completion[0] = 0;

	// command enter
	if (key == KEY_ENTER)
	{
		if (!input.length)
			return true;

		// push the command
		COM_BufAddText(input.buffer);
		COM_BufAddText("\n");

		CONS_Printf("\x86""%c""\x80""%s\n", CON_PROMPTCHAR, inputlines[inputline]);

		Lock_state();

		// Only add command to history if it differs from previous one
		if (strcmp(input.buffer, inputlines[(inputline-1) & 31]))
		{
			inputline = (inputline+1) & 31;
			M_TextInputInit(&input, inputlines[inputline], CON_MAXPROMPTCHARS);
		}

		inputhist = inputline;
		M_TextInputClear(&input);

		Unlock_state();

		return true;
	}

	// move back in input history
	if (key == KEY_UPARROW)
	{
		// copy one of the previous inputlines to the current
		do
			inputhist = (inputhist - 1) & 31; // cycle back
		while (inputhist != inputline && !inputlines[inputhist][0]);

		// stop at the last history input line, which is the
		// current line + 1 because we cycle through the 32 input lines
		if (inputhist == inputline)
			inputhist = (inputline + 1) & 31;

		Lock_state();
		M_TextInputSetString(&input, inputlines[inputhist]);
		Unlock_state();
		return true;
	}

	// move forward in input history
	if (key == KEY_DOWNARROW)
	{
		if (inputhist == inputline)
			return true;
		do
			inputhist = (inputhist + 1) & 31;
		while (inputhist != inputline && !inputlines[inputhist][0]);

		// back to currentline
		Lock_state();
		if (inputhist == inputline)
			M_TextInputClear(&input);
		else
			M_TextInputSetString(&input, inputlines[inputhist]);
		Unlock_state();

		return true;
	}

	return true;
}

// Insert a new line in the console text buffer
//
static void CON_Linefeed(void)
{
	// set time for heads up messages
	if (con_hudlines)
		con_hudtime[con_cy%con_hudlines] = cons_hudtime.value*TICRATE;

	con_cy++;
	con_cx = 0;

	con_line = &con_buffer[(con_cy%con_totallines)*con_width];
	memset(con_line, ' ', con_width);

	// make sure the view borders are refreshed if hud messages scroll
	con_hudupdate = true; // see HU_Erase()
}

// Outputs text into the console text buffer
static void CON_Print(char *msg)
{
	size_t l;
	INT32 controlchars = 0; // for color changing
	char color = '\x80';  // keep color across lines

	if (msg == NULL)
		return;

	if (*msg == '\3') // chat text, makes ding sound
		S_StartSound(NULL, sfx_radio);
	else if (*msg == '\4') // chat action, dings and is in yellow
	{
		*msg = '\x82'; // yellow
		S_StartSound(NULL, sfx_radio);
	}

	Lock_state();

	if (!(*msg & 0x80))
	{
		con_line[con_cx++] = '\x80';
		controlchars = 1;
	}

	while (*msg)
	{
		// skip non-printable characters and white spaces
		while (*msg && *msg <= ' ')
		{
			if (*msg & 0x80)
			{
				color = con_line[con_cx++] = *(msg++);
				controlchars++;
				continue;
			}
			else if (*msg == '\r') // carriage return
			{
				con_cy--;
				CON_Linefeed();
				color = '\x80';
				controlchars = 0;
			}
			else if (*msg == '\n') // linefeed
			{
				CON_Linefeed();
				con_line[con_cx++] = color;
				controlchars = 1;
			}
			else if (*msg == ' ') // space
			{
				con_line[con_cx++] = ' ';
				if (con_cx - controlchars >= con_width-11)
				{
					CON_Linefeed();
					con_line[con_cx++] = color;
					controlchars = 1;
				}
			}
			else if (*msg == '\t')
			{
				// adds tab spaces for nice layout in console

				do
				{
					con_line[con_cx++] = ' ';
				} while ((con_cx - controlchars) % 4 != 0);

				if (con_cx - controlchars >= con_width-11)
				{
					CON_Linefeed();
					con_line[con_cx++] = color;
					controlchars = 1;
				}
			}
			msg++;
		}

		if (*msg == '\0')
		{
			Unlock_state();
			return;
		}

		// printable character
		for (l = 0; l < (con_width-11) && msg[l] > ' '; l++)
			;

		// word wrap
		if ((con_cx - controlchars) + l > con_width-11)
		{
			CON_Linefeed();
			con_line[con_cx++] = color;
			controlchars = 1;
		}

		// a word at a time
		for (; l > 0; l--)
			con_line[con_cx++] = *(msg++);
	}

	Unlock_state();
}

void CON_LogMessage(const char *msg)
{
	char txt[8192], *t;
	const char *p = msg, *e = txt+sizeof (txt)-2;

	for (t = txt; *p != '\0'; p++)
	{
		if (*p == '\n' || *p >= ' ') // don't log or console print CON_Print's control characters
			*t++ = *p;

		if (t >= e)
		{
			*t = '\0'; //end of string
			I_OutputMsg("%s", txt); //print string
			t = txt; //reset t pointer
			memset(txt,'\0', sizeof (txt)); //reset txt
		}
	}
	*t = '\0'; //end of string
	I_OutputMsg("%s", txt);
}

// Console print! Wahooo! Lots o fun!
//

void CONS_Printf(const char *fmt, ...)
{
	va_list argptr;
	static char *txt = NULL;
	boolean startup;

	if (txt == NULL)
		txt = malloc(8192);

	va_start(argptr, fmt);
	vsprintf(txt, fmt, argptr);
	va_end(argptr);

	// echo console prints to log file
	DEBFILE(txt);

	if (con_started)
		CON_Print(txt);
	
	CON_LogMessage(txt);

	Lock_state();

	// make sure new text is visible
	con_scrollup = 0;
	startup = con_startup;

	Unlock_state();

	// if not in display loop, force screen update
	if (startup)
	{
		// here we display the console text
		CON_Drawer();
		I_FinishUpdate(); // page flip or blit buffer
	}
}

void CONS_Alert(alerttype_t level, const char *fmt, ...)
{
	va_list argptr;
	static char *txt = NULL;

	if (txt == NULL)
		txt = malloc(8192);

	va_start(argptr, fmt);
	vsprintf(txt, fmt, argptr);
	va_end(argptr);

	switch (level)
	{
		case CONS_NOTICE:
			// no notice for notices, hehe
			CONS_Printf("\x83" "%s" "\x80 ", M_GetText("NOTICE:"));
			break;
		case CONS_WARNING:
			refreshdirmenu |= REFRESHDIR_WARNING;
			CONS_Printf("\x82" "%s" "\x80 ", M_GetText("WARNING:"));
			break;
		case CONS_ERROR:
			refreshdirmenu |= REFRESHDIR_ERROR;
			CONS_Printf("\x85" "%s" "\x80 ", M_GetText("ERROR:"));
			break;
	}

	// I am lazy and I feel like just letting CONS_Printf take care of things.
	// Is that okay?
	CONS_Printf("%s", txt);
}

void CONS_Debug(INT32 debugflags, const char *fmt, ...)
{
	va_list argptr;
	static char *txt = NULL;

	if ((cv_debug & debugflags) != debugflags)
		return;

	if (txt == NULL)
		txt = malloc(8192);

	va_start(argptr, fmt);
	vsprintf(txt, fmt, argptr);
	va_end(argptr);

	// Again I am lazy, oh well
	CONS_Printf("%s", txt);
}


// Print an error message, and wait for ENTER key to continue.
// To make sure the user has seen the message
//
void CONS_Error(const char *msg)
{
#ifdef RPC_NO_WINDOWS_H
	if (!graphics_started)
	{
		MessageBoxA(vid.WndParent, msg, "SRB2Kart Warning", MB_OK);
		return;
	}
#endif
	CONS_Printf("\x82%s", msg); // write error msg in different colour
	CONS_Printf(M_GetText("Press ENTER to continue\n"));

	// dirty quick hack, but for the good cause
	while (I_GetKey() != KEY_ENTER)
		I_OsPolling();
}

//======================================================================
//                          CONSOLE DRAW
//======================================================================

// draw console prompt line
//
static void CON_DrawInput(void)
{
	INT32 charwidth = (INT32)con_scalefactor << 3;
	const char *p = inputlines[inputline];
	size_t c, clen, cend;
	UINT8 lellip = 0, rellip = 0;
	INT32 x, y, i;

	y = con_curlines - 12 * con_scalefactor;
	x = charwidth*2;

	clen = con_width-13;

	if (input.length <= clen)
	{
		c = 0;
		clen = input.length;
	}
	else // input line scrolls left if it gets too long
	{
		clen -= 2; // There will always be some extra truncation -- but where is what we'll find out

		if (input.cursor <= clen/2)
		{
			// Close enough to right edge to show all
			c = 0;
			// Always will truncate right side from this position, so always draw right ellipsis
			rellip = 1;
		}
		else
		{
			// Cursor in the middle (or right side) of input
			// Move over for the ellipsis
			c = input.cursor - (clen/2) + 2;
			x += charwidth*2;
			lellip = 1;

			if (c + clen >= input.length)
			{
				// Cursor in the right side of input
				// We were too far over, so move back
				c = input.length - clen;
			}
			else
			{
				// Cursor in the middle -- ellipses on both sides
				clen -= 2;
				rellip = 1;
			}
		}
	}

	if (lellip)
	{
		x -= charwidth*3;
		if (input.select < c)
			V_DrawFill(x, y, charwidth*3, (10 * con_scalefactor), 107 | V_NOSCALESTART);
		for (i = 0; i < 3; ++i, x += charwidth)
			V_DrawCharacter(x, y, '.' | cv_constextsize.value | V_GRAYMAP | V_NOSCALESTART, !cv_allcaps.value);
	}
	else
		V_DrawCharacter(x-charwidth, y, CON_PROMPTCHAR | cv_constextsize.value | V_GRAYMAP | V_NOSCALESTART, !cv_allcaps.value);

	for (cend = c + clen; c < cend; ++c, x += charwidth)
	{
		if ((input.select > c && input.cursor <= c) || (input.select <= c && input.cursor > c))
		{
			V_DrawFill(x, y, charwidth, (10 * con_scalefactor), 107 | V_NOSCALESTART);
			V_DrawCharacter(x, y, p[c] | cv_constextsize.value | V_YELLOWMAP | V_NOSCALESTART, !cv_allcaps.value);
		}
		else
			V_DrawCharacter(x, y, p[c] | cv_constextsize.value | V_NOSCALESTART, !cv_allcaps.value);

		if (c == input.cursor && con_tick >= 4)
			V_DrawCharacter(x, y + (con_scalefactor*2), '_' | cv_constextsize.value | V_NOSCALESTART, !cv_allcaps.value);
	}
	if (cend == input.cursor && con_tick >= 4)
		V_DrawCharacter(x, y + (con_scalefactor*2), '_' | cv_constextsize.value | V_NOSCALESTART, !cv_allcaps.value);
	if (rellip)
	{
		if (input.select > cend)
			V_DrawFill(x, y, charwidth*3, (10 * con_scalefactor), 107 | V_NOSCALESTART);
		for (i = 0; i < 3; ++i, x += charwidth)
			V_DrawCharacter(x, y, '.' | cv_constextsize.value | V_GRAYMAP | V_NOSCALESTART, !cv_allcaps.value);
	}
}

// draw the last lines of console text to the top of the screen
static void CON_DrawHudlines(void)
{
	UINT8 *p;
	size_t i;
	INT32 y;
	INT32 charflags = 0;
	INT32 charwidth = 8 * con_scalefactor;
	INT32 charheight = 8 * con_scalefactor;

	if (!con_hudlines)
		return;

	if (chat_on && OLDCHAT)
		y = charheight; // leave place for chat input in the first row of text (only do it if consolechat is on.)
	else
		y = 0;

	for (i = con_cy - con_hudlines; i <= con_cy; i++)
	{
		size_t c;
		INT32 x;

		if ((signed)i < 0)
			continue;
		if (con_hudtime[i%con_hudlines] == 0)
			continue;

		p = (UINT8 *)&con_buffer[(i%con_totallines)*con_width];

		for (c = 0, x = 0; c < con_width; c++, x += charwidth, p++)
		{
			while (*p & 0x80) // Graue 06-19-2004
			{
				charflags = (*p & 0x7f) << V_CHARCOLORSHIFT;
				p++;
			}
			if (*p < HU_FONTSTART)
				;//charwidth = 4 * con_scalefactor;
			else
			{
				//charwidth = SHORT(hu_font['A'-HU_FONTSTART]->width) * con_scalefactor;
				V_DrawCharacter(x, y, (INT32)(*p) | charflags | cv_constextsize.value | V_NOSCALESTART, !cv_allcaps.value);
			}
		}

		//V_DrawCharacter(x, y, (p[c]&0xff) | cv_constextsize.value | V_NOSCALESTART, !cv_allcaps.value);
		y += charheight;
	}

	// top screen lines that might need clearing when view is reduced
	con_clearlines = y; // this is handled by HU_Erase();
}

// draw the console background, text, and prompt if enough place
//
static void CON_DrawConsole(void)
{
	UINT8 *p;
	size_t i;
	INT32 y;
	INT32 charflags = 0;
	INT32 charwidth = (INT32)con_scalefactor << 3;
	INT32 charheight = charwidth;
	INT32 minheight = 20 * con_scalefactor;	// 20 = 8+8+4

	if (con_curlines <= 0)
		return;

	//FIXME: refresh borders only when console bg is translucent
	con_clearlines = con_curlines; // clear console draw from view borders
	con_hudupdate = true; // always refresh while console is on

	// draw console background
	if (cons_backpic.value || con_forcepic)
	{
		patch_t *con_backpic = W_CachePatchName("KARTKREW", PU_CACHE);

		// Jimita: CON_DrawBackpic just called V_DrawScaledPatch
		V_DrawFixedPatch(0, 0, FRACUNIT/2, 0, con_backpic, NULL);

		W_UnlockCachedPatch(con_backpic);
	}
	else
	{
		// inu: no more width (was always 0 and vid.width)
		if (rendermode != render_none)
			V_DrawFadeConsBack(con_curlines); // translucent background
	}

	// draw console text lines from top to bottom
	if (con_curlines < minheight)
		return;

	i = con_cy - con_scrollup;

	// skip the last empty line due to the cursor being at the start of a new line
	i--;

	i -= (con_curlines - minheight) / charheight;

	if (rendermode == render_none) return;

	for (y = (con_curlines-minheight) % charheight; y <= con_curlines-minheight; y += charheight, i++)
	{
		INT32 x;
		size_t c;

		p = (UINT8 *)&con_buffer[((i > 0 ? i : 0)%con_totallines)*con_width];

		for (c = 0, x = charwidth; c < con_width; c++, x += charwidth, p++)
		{
			while (*p & 0x80)
			{
				charflags = (*p & 0x7f) << V_CHARCOLORSHIFT;
				p++;
			}
			V_DrawCharacter(x, y, (INT32)(*p) | charflags | cv_constextsize.value | V_NOSCALESTART, !cv_allcaps.value);
		}
	}

	// draw prompt if enough place (not while game startup)
	if ((con_curlines == con_destlines) && (con_curlines >= minheight) && !con_startup)
		CON_DrawInput();
}

// Console refresh drawer, call each frame
//
void CON_Drawer(void)
{
	Lock_state();

	if (!con_started || !graphics_started)
	{
		Unlock_state();
		return;
	}

	if (con_recalc)
	{
		CON_RecalcSize();
		
		if (con_curlines <= 0)
			CON_ClearHUD();
	}
	
	// console movement
	if (con_curlines != con_destlines)
		CON_MoveConsole();

	if (con_curlines > 0)
		CON_DrawConsole();
	else if (gamestate == GS_LEVEL || gamestate == GS_INTERMISSION || gamestate == GS_CUTSCENE || gamestate == GS_CREDITS
		|| gamestate == GS_VOTING || gamestate == GS_EVALUATION || gamestate == GS_WAITINGPLAYERS)
		CON_DrawHudlines();

	Unlock_state();
}
