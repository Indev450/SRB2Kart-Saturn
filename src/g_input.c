// SONIC ROBO BLAST 2
//-----------------------------------------------------------------------------
// Copyright (C) 1998-2000 by DooM Legacy Team.
// Copyright (C) 1999-2018 by Sonic Team Junior.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  g_input.c
/// \brief handle mouse/keyboard/joystick inputs,
///        maps inputs to game controls (forward, spin, jump...)

#include "doomdef.h"
#include "doomstat.h"
#include "g_input.h"
#include "g_game.h"
#include "keys.h"
#include "k_kart.h"
#include "hu_stuff.h" // need HUFONT start & end
#include "d_net.h"
#include "console.h"

#include "i_system.h"
#include "i_video.h"
#include "r_draw.h"
#include "v_video.h"

#define MAXMOUSESENSITIVITY 100 // sensitivity steps

static CV_PossibleValue_t mousesens_cons_t[] = {{1, "MIN"}, {MAXMOUSESENSITIVITY, "MAX"}, {0, NULL}};
static CV_PossibleValue_t onecontrolperkey_cons_t[] = {{1, "One"}, {2, "Several"}, {0, NULL}};
static CV_PossibleValue_t turnsmooth_cons_t[] = {{2, "Slow"}, {1, "Fast"}, {0, "Off"}, {0, NULL}};

// mouse values are used once
consvar_t cv_mousesens = {"mousesens", "20", CV_SAVE, mousesens_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_mousesens2 = {"mousesens2", "20", CV_SAVE, mousesens_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_mouseysens = {"mouseysens", "20", CV_SAVE, mousesens_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_mouseysens2 = {"mouseysens2", "20", CV_SAVE, mousesens_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_controlperkey = {"controlperkey", "One", CV_SAVE, onecontrolperkey_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_turnsmooth = {"turnsmoothing", "Slow", CV_SAVE, turnsmooth_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL};

static void G_ResetPlayerDeviceRumble(INT32 player);
static void rumble_off_handle(void);
static void rumble_off_handle2(void);
static void rumble_off_handle3(void);
static void rumble_off_handle4(void);

consvar_t cv_rumble[MAXSPLITSCREENPLAYERS] = {
	{"rumble", "Off", CV_SAVE, CV_OnOff, rumble_off_handle, 0, NULL, NULL, 0, 0, NULL},
	{"rumble2", "Off", CV_SAVE, CV_OnOff, rumble_off_handle2, 0, NULL, NULL, 0, 0, NULL},
	{"rumble3", "Off", CV_SAVE, CV_OnOff, rumble_off_handle3, 0, NULL, NULL, 0, 0, NULL},
	{"rumble4", "Off", CV_SAVE, CV_OnOff, rumble_off_handle4, 0, NULL, NULL, 0, 0, NULL}
};

static void rumble_off_handle(void)
{
	if (cv_rumble[0].value == 0)
		G_ResetPlayerDeviceRumble(0);
}


static void rumble_off_handle2(void)
{
	if (cv_rumble[1].value == 0)
		G_ResetPlayerDeviceRumble(1);
}


static void rumble_off_handle3(void)
{
	if (cv_rumble[2].value == 0)
		G_ResetPlayerDeviceRumble(2);
}


static void rumble_off_handle4(void)
{
	if (cv_rumble[3].value == 0)
		G_ResetPlayerDeviceRumble(3);
}

INT32 mousex, mousey;
INT32 mlooky; // like mousey but with a custom sensitivity for mlook

INT32 mouse2x, mouse2y, mlook2y;

// joystick values are repeated
INT32 joyxmove[JOYAXISSET], joyymove[JOYAXISSET], joy2xmove[JOYAXISSET], joy2ymove[JOYAXISSET],
joy3xmove[JOYAXISSET], joy3ymove[JOYAXISSET], joy4xmove[JOYAXISSET], joy4ymove[JOYAXISSET];

// current state of the keys: true if pushed
UINT8 gamekeydown[NUMINPUTS];

// two key codes (or virtual key) per game control
INT32 gamecontrol[num_gamecontrols][2];
INT32 gamecontrolbis[num_gamecontrols][2]; // secondary splitscreen player
INT32 gamecontrol3[num_gamecontrols][2]; // tertiary splitscreen player
INT32 gamecontrol4[num_gamecontrols][2]; // quarternary splitscreen player

typedef struct
{
	UINT8 time;
	UINT8 state;
	UINT8 clicks;
} dclick_t;
static dclick_t mousedclicks[MOUSEBUTTONS];
static dclick_t joydclicks[JOYBUTTONS + JOYHATS*4];
static dclick_t mouse2dclicks[MOUSEBUTTONS];
static dclick_t joy2dclicks[JOYBUTTONS + JOYHATS*4];
static dclick_t joy3dclicks[JOYBUTTONS + JOYHATS*4];
static dclick_t joy4dclicks[JOYBUTTONS + JOYHATS*4];

// protos
static UINT8 G_CheckDoubleClick(UINT8 state, dclick_t *dt);

//
// Remaps the inputs to game controls.
//
// A game control can be triggered by one or more keys/buttons.
//
// Each key/mousebutton/joybutton triggers ONLY ONE game control.
//
void G_MapEventsToControls(event_t *ev)
{
	INT32 i;
	UINT8 flag;

	switch (ev->type)
	{
		case ev_keydown:
			if (ev->data1 < NUMINPUTS)
				gamekeydown[ev->data1] = 1;
#ifdef PARANOIA
			else
			{
				CONS_Debug(DBG_GAMELOGIC, "Bad downkey input %d\n",ev->data1);
			}

#endif
			break;

		case ev_keyup:
			if (ev->data1 < NUMINPUTS)
				gamekeydown[ev->data1] = 0;
#ifdef PARANOIA
			else
			{
				CONS_Debug(DBG_GAMELOGIC, "Bad upkey input %d\n",ev->data1);
			}
#endif
			break;

		case ev_mouse: // buttons are virtual keys
			if (menuactive || CON_Ready() || chat_on)
				break;
			mousex = (INT32)(ev->data2*((cv_mousesens.value*cv_mousesens.value)/110.0f + 0.1f));
			mousey = (INT32)(ev->data3*((cv_mousesens.value*cv_mousesens.value)/110.0f + 0.1f));
			mlooky = (INT32)(ev->data3*((cv_mouseysens.value*cv_mousesens.value)/110.0f + 0.1f));
			break;

		case ev_joystick: // buttons are virtual keys
			i = ev->data1;
			if (i >= JOYAXISSET || menuactive || CON_Ready() || chat_on)
				break;
			if (ev->data2 != INT32_MAX) joyxmove[i] = ev->data2;
			if (ev->data3 != INT32_MAX) joyymove[i] = ev->data3;
			break;

		case ev_joystick2: // buttons are virtual keys
			i = ev->data1;
			if (i >= JOYAXISSET || menuactive)
				break;
			if (ev->data2 != INT32_MAX) joy2xmove[i] = ev->data2;
			if (ev->data3 != INT32_MAX) joy2ymove[i] = ev->data3;
			break;

		case ev_joystick3:
			i = ev->data1;
			if (i >= JOYAXISSET)
				break;
			if (ev->data2 != INT32_MAX) joy3xmove[i] = ev->data2;
			if (ev->data3 != INT32_MAX) joy3ymove[i] = ev->data3;
			break;

		case ev_joystick4:
			i = ev->data1;
			if (i >= JOYAXISSET)
				break;
			if (ev->data2 != INT32_MAX) joy4xmove[i] = ev->data2;
			if (ev->data3 != INT32_MAX) joy4ymove[i] = ev->data3;
			break;

		case ev_mouse2: // buttons are virtual keys
			if (menuactive || CON_Ready() || chat_on)
				break;
			mouse2x = (INT32)(ev->data2*((cv_mousesens2.value*cv_mousesens2.value)/110.0f + 0.1f));
			mouse2y = (INT32)(ev->data3*((cv_mousesens2.value*cv_mousesens2.value)/110.0f + 0.1f));
			mlook2y = (INT32)(ev->data3*((cv_mouseysens2.value*cv_mousesens2.value)/110.0f + 0.1f));
			break;

		default:
			break;
	}

	// ALWAYS check for mouse & joystick double-clicks even if no mouse event
	for (i = 0; i < MOUSEBUTTONS; i++)
	{
		flag = G_CheckDoubleClick(gamekeydown[KEY_MOUSE1+i], &mousedclicks[i]);
		gamekeydown[KEY_DBLMOUSE1+i] = flag;
	}

	for (i = 0; i < JOYBUTTONS + JOYHATS*4; i++)
	{
		flag = G_CheckDoubleClick(gamekeydown[KEY_JOY1+i], &joydclicks[i]);
		gamekeydown[KEY_DBLJOY1+i] = flag;
	}

	for (i = 0; i < MOUSEBUTTONS; i++)
	{
		flag = G_CheckDoubleClick(gamekeydown[KEY_2MOUSE1+i], &mouse2dclicks[i]);
		gamekeydown[KEY_DBL2MOUSE1+i] = flag;
	}

	for (i = 0; i < JOYBUTTONS + JOYHATS*4; i++)
	{
		flag = G_CheckDoubleClick(gamekeydown[KEY_2JOY1+i], &joy2dclicks[i]);
		gamekeydown[KEY_DBL2JOY1+i] = flag;
	}

	for (i = 0; i < JOYBUTTONS + JOYHATS*4; i++)
	{
		flag = G_CheckDoubleClick(gamekeydown[KEY_3JOY1+i], &joy3dclicks[i]);
		gamekeydown[KEY_DBL3JOY1+i] = flag;
	}

	for (i = 0; i < JOYBUTTONS + JOYHATS*4; i++)
	{
		flag = G_CheckDoubleClick(gamekeydown[KEY_4JOY1+i], &joy4dclicks[i]);
		gamekeydown[KEY_DBL4JOY1+i] = flag;
	}
}

//
// General double-click detection routine for any kind of input.
//
static UINT8 G_CheckDoubleClick(UINT8 state, dclick_t *dt)
{
	if (state != dt->state && dt->time > 1)
	{
		dt->state = state;
		if (state)
			dt->clicks++;
		if (dt->clicks == 2)
		{
			dt->clicks = 0;
			return true;
		}
		else
			dt->time = 0;
	}
	else
	{
		dt->time++;
		if (dt->time > 20)
		{
			dt->clicks = 0;
			dt->state = 0;
		}
	}
	return false;
}

typedef struct
{
	INT32 keynum;
	const char *name;
} keyname_t;

static keyname_t keynames[] =
{
	{KEY_SPACE, "SPACE"},
	{KEY_CAPSLOCK, "CAPS LOCK"},
	{KEY_ENTER, "ENTER"},
	{KEY_TAB, "TAB"},
	{KEY_ESCAPE, "ESCAPE"},
	{KEY_BACKSPACE, "BACKSPACE"},

	{KEY_NUMLOCK, "NUMLOCK"},
	{KEY_SCROLLLOCK, "SCROLLLOCK"},

	// bill gates keys
	{KEY_LEFTWIN, "LEFTWIN"},
	{KEY_RIGHTWIN, "RIGHTWIN"},
	{KEY_MENU, "MENU"},

	{KEY_LSHIFT, "LSHIFT"},
	{KEY_RSHIFT, "RSHIFT"},
	{KEY_LSHIFT, "SHIFT"},
	{KEY_LCTRL, "LCTRL"},
	{KEY_RCTRL, "RCTRL"},
	{KEY_LCTRL, "CTRL"},
	{KEY_LALT, "LALT"},
	{KEY_RALT, "RALT"},
	{KEY_LALT, "ALT"},

	// keypad keys
	{KEY_KPADSLASH, "KEYPAD /"},
	{KEY_KEYPAD7, "KEYPAD 7"},
	{KEY_KEYPAD8, "KEYPAD 8"},
	{KEY_KEYPAD9, "KEYPAD 9"},
	{KEY_MINUSPAD, "KEYPAD -"},
	{KEY_KEYPAD4, "KEYPAD 4"},
	{KEY_KEYPAD5, "KEYPAD 5"},
	{KEY_KEYPAD6, "KEYPAD 6"},
	{KEY_PLUSPAD, "KEYPAD +"},
	{KEY_KEYPAD1, "KEYPAD 1"},
	{KEY_KEYPAD2, "KEYPAD 2"},
	{KEY_KEYPAD3, "KEYPAD 3"},
	{KEY_KEYPAD0, "KEYPAD 0"},
	{KEY_KPADDEL, "KEYPAD ."},

	// extended keys (not keypad)
	{KEY_HOME, "HOME"},
	{KEY_UPARROW, "UP ARROW"},
	{KEY_PGUP, "PGUP"},
	{KEY_LEFTARROW, "LEFT ARROW"},
	{KEY_RIGHTARROW, "RIGHT ARROW"},
	{KEY_END, "END"},
	{KEY_DOWNARROW, "DOWN ARROW"},
	{KEY_PGDN, "PGDN"},
	{KEY_INS, "INS"},
	{KEY_DEL, "DEL"},

	// other keys
	{KEY_F1, "F1"},
	{KEY_F2, "F2"},
	{KEY_F3, "F3"},
	{KEY_F4, "F4"},
	{KEY_F5, "F5"},
	{KEY_F6, "F6"},
	{KEY_F7, "F7"},
	{KEY_F8, "F8"},
	{KEY_F9, "F9"},
	{KEY_F10, "F10"},
	{KEY_F11, "F11"},
	{KEY_F12, "F12"},

	// KEY_CONSOLE has an exception in the keyname code
	{'`', "TILDE"},
	{KEY_PAUSE, "PAUSE/BREAK"},

	// virtual keys for mouse buttons and joystick buttons
	{KEY_MOUSE1+0,"MOUSE1"},
	{KEY_MOUSE1+1,"MOUSE2"},
	{KEY_MOUSE1+2,"MOUSE3"},
	{KEY_MOUSE1+3,"MOUSE4"},
	{KEY_MOUSE1+4,"MOUSE5"},
	{KEY_MOUSE1+5,"MOUSE6"},
	{KEY_MOUSE1+6,"MOUSE7"},
	{KEY_MOUSE1+7,"MOUSE8"},
	{KEY_2MOUSE1+0,"SEC_MOUSE2"}, // BP: sorry my mouse handler swap button 1 and 2
	{KEY_2MOUSE1+1,"SEC_MOUSE1"},
	{KEY_2MOUSE1+2,"SEC_MOUSE3"},
	{KEY_2MOUSE1+3,"SEC_MOUSE4"},
	{KEY_2MOUSE1+4,"SEC_MOUSE5"},
	{KEY_2MOUSE1+5,"SEC_MOUSE6"},
	{KEY_2MOUSE1+6,"SEC_MOUSE7"},
	{KEY_2MOUSE1+7,"SEC_MOUSE8"},
	{KEY_MOUSEWHEELUP, "Wheel 1 UP"},
	{KEY_MOUSEWHEELDOWN, "Wheel 1 Down"},
	{KEY_2MOUSEWHEELUP, "Wheel 2 UP"},
	{KEY_2MOUSEWHEELDOWN, "Wheel 2 Down"},

	{KEY_JOY1+0, "JOY1"},
	{KEY_JOY1+1, "JOY2"},
	{KEY_JOY1+2, "JOY3"},
	{KEY_JOY1+3, "JOY4"},
	{KEY_JOY1+4, "JOY5"},
	{KEY_JOY1+5, "JOY6"},
	{KEY_JOY1+6, "JOY7"},
	{KEY_JOY1+7, "JOY8"},
	{KEY_JOY1+8, "JOY9"},
	
#if !defined (NOMOREJOYBTN_1S)
	// we use up to 32 buttons in DirectInput
	{KEY_JOY1+9, "JOY10"},
	{KEY_JOY1+10, "JOY11"},
	{KEY_JOY1+11, "JOY12"},
	{KEY_JOY1+12, "JOY13"},
	{KEY_JOY1+13, "JOY14"},
	{KEY_JOY1+14, "JOY15"},
	{KEY_JOY1+15, "JOY16"},
	{KEY_JOY1+16, "JOY17"},
	{KEY_JOY1+17, "JOY18"},
	{KEY_JOY1+18, "JOY19"},
	{KEY_JOY1+19, "JOY20"},
	{KEY_JOY1+20, "JOY21"},
	{KEY_JOY1+21, "JOY22"},
	{KEY_JOY1+22, "JOY23"},
	{KEY_JOY1+23, "JOY24"},
	{KEY_JOY1+24, "JOY25"},
	{KEY_JOY1+25, "JOY26"},
	{KEY_JOY1+26, "JOY27"},
	{KEY_JOY1+27, "JOY28"},
	{KEY_JOY1+28, "JOY29"},
	{KEY_JOY1+29, "JOY30"},
	{KEY_JOY1+30, "JOY31"},
	{KEY_JOY1+31, "JOY32"},
#endif
	// the DOS version uses Allegro's joystick support
	{KEY_HAT1+0, "HATUP"},
	{KEY_HAT1+1, "HATDOWN"},
	{KEY_HAT1+2, "HATLEFT"},
	{KEY_HAT1+3, "HATRIGHT"},
	{KEY_HAT1+4, "HATUP2"},
	{KEY_HAT1+5, "HATDOWN2"},
	{KEY_HAT1+6, "HATLEFT2"},
	{KEY_HAT1+7, "HATRIGHT2"},
	{KEY_HAT1+8, "HATUP3"},
	{KEY_HAT1+9, "HATDOWN3"},
	{KEY_HAT1+10, "HATLEFT3"},
	{KEY_HAT1+11, "HATRIGHT3"},
	{KEY_HAT1+12, "HATUP4"},
	{KEY_HAT1+13, "HATDOWN4"},
	{KEY_HAT1+14, "HATLEFT4"},
	{KEY_HAT1+15, "HATRIGHT4"},

	{KEY_DBLMOUSE1+0, "DBLMOUSE1"},
	{KEY_DBLMOUSE1+1, "DBLMOUSE2"},
	{KEY_DBLMOUSE1+2, "DBLMOUSE3"},
	{KEY_DBLMOUSE1+3, "DBLMOUSE4"},
	{KEY_DBLMOUSE1+4, "DBLMOUSE5"},
	{KEY_DBLMOUSE1+5, "DBLMOUSE6"},
	{KEY_DBLMOUSE1+6, "DBLMOUSE7"},
	{KEY_DBLMOUSE1+7, "DBLMOUSE8"},
	{KEY_DBL2MOUSE1+0, "DBLSEC_MOUSE2"}, // BP: sorry my mouse handler swap button 1 and 2
	{KEY_DBL2MOUSE1+1, "DBLSEC_MOUSE1"},
	{KEY_DBL2MOUSE1+2, "DBLSEC_MOUSE3"},
	{KEY_DBL2MOUSE1+3, "DBLSEC_MOUSE4"},
	{KEY_DBL2MOUSE1+4, "DBLSEC_MOUSE5"},
	{KEY_DBL2MOUSE1+5, "DBLSEC_MOUSE6"},
	{KEY_DBL2MOUSE1+6, "DBLSEC_MOUSE7"},
	{KEY_DBL2MOUSE1+7, "DBLSEC_MOUSE8"},

	{KEY_DBLJOY1+0, "DBLJOY1"},
	{KEY_DBLJOY1+1, "DBLJOY2"},
	{KEY_DBLJOY1+2, "DBLJOY3"},
	{KEY_DBLJOY1+3, "DBLJOY4"},
	{KEY_DBLJOY1+4, "DBLJOY5"},
	{KEY_DBLJOY1+5, "DBLJOY6"},
	{KEY_DBLJOY1+6, "DBLJOY7"},
	{KEY_DBLJOY1+7, "DBLJOY8"},
#if !defined (NOMOREJOYBTN_1DBL)
	{KEY_DBLJOY1+8, "DBLJOY9"},
	{KEY_DBLJOY1+9, "DBLJOY10"},
	{KEY_DBLJOY1+10, "DBLJOY11"},
	{KEY_DBLJOY1+11, "DBLJOY12"},
	{KEY_DBLJOY1+12, "DBLJOY13"},
	{KEY_DBLJOY1+13, "DBLJOY14"},
	{KEY_DBLJOY1+14, "DBLJOY15"},
	{KEY_DBLJOY1+15, "DBLJOY16"},
	{KEY_DBLJOY1+16, "DBLJOY17"},
	{KEY_DBLJOY1+17, "DBLJOY18"},
	{KEY_DBLJOY1+18, "DBLJOY19"},
	{KEY_DBLJOY1+19, "DBLJOY20"},
	{KEY_DBLJOY1+20, "DBLJOY21"},
	{KEY_DBLJOY1+21, "DBLJOY22"},
	{KEY_DBLJOY1+22, "DBLJOY23"},
	{KEY_DBLJOY1+23, "DBLJOY24"},
	{KEY_DBLJOY1+24, "DBLJOY25"},
	{KEY_DBLJOY1+25, "DBLJOY26"},
	{KEY_DBLJOY1+26, "DBLJOY27"},
	{KEY_DBLJOY1+27, "DBLJOY28"},
	{KEY_DBLJOY1+28, "DBLJOY29"},
	{KEY_DBLJOY1+29, "DBLJOY30"},
	{KEY_DBLJOY1+30, "DBLJOY31"},
	{KEY_DBLJOY1+31, "DBLJOY32"},
#endif
	{KEY_DBLHAT1+0, "DBLHATUP"},
	{KEY_DBLHAT1+1, "DBLHATDOWN"},
	{KEY_DBLHAT1+2, "DBLHATLEFT"},
	{KEY_DBLHAT1+3, "DBLHATRIGHT"},
	{KEY_DBLHAT1+4, "DBLHATUP2"},
	{KEY_DBLHAT1+5, "DBLHATDOWN2"},
	{KEY_DBLHAT1+6, "DBLHATLEFT2"},
	{KEY_DBLHAT1+7, "DBLHATRIGHT2"},
	{KEY_DBLHAT1+8, "DBLHATUP3"},
	{KEY_DBLHAT1+9, "DBLHATDOWN3"},
	{KEY_DBLHAT1+10, "DBLHATLEFT3"},
	{KEY_DBLHAT1+11, "DBLHATRIGHT3"},
	{KEY_DBLHAT1+12, "DBLHATUP4"},
	{KEY_DBLHAT1+13, "DBLHATDOWN4"},
	{KEY_DBLHAT1+14, "DBLHATLEFT4"},
	{KEY_DBLHAT1+15, "DBLHATRIGHT4"},

#ifdef _PSP
	{KEY_2JOY1+0, "SEC_TRIANGLE"},
	{KEY_2JOY1+1, "SEC_CIRCLE"  },
	{KEY_2JOY1+2, "SEC_CROSS"   },
	{KEY_2JOY1+3, "SEC_SQUARE"  },
	{KEY_2JOY1+4, "SEC_LTRIGGER"},
	{KEY_2JOY1+5, "SEC_RTRIGGER"},
	{KEY_2JOY1+6, "SEC_SELECT"  },
	{KEY_2JOY1+7, "SEC_START"   },
	{KEY_2JOY1+8, "SEC_HOME"    },
	{KEY_2JOY1+9, "SEC_HOLD"    },
#define NOMOREJOYBTN_2S
#else
	{KEY_2JOY1+0, "SEC_JOY1"},
	{KEY_2JOY1+1, "SEC_JOY2"},
	{KEY_2JOY1+2, "SEC_JOY3"},
	{KEY_2JOY1+3, "SEC_JOY4"},
	{KEY_2JOY1+4, "SEC_JOY5"},
	{KEY_2JOY1+5, "SEC_JOY6"},
	{KEY_2JOY1+6, "SEC_JOY7"},
	{KEY_2JOY1+7, "SEC_JOY8"},
#endif
#if !defined (NOMOREJOYBTN_2S)
	// we use up to 32 buttons in DirectInput
	{KEY_2JOY1+8, "SEC_JOY9"},
	{KEY_2JOY1+9, "SEC_JOY10"},
	{KEY_2JOY1+10, "SEC_JOY11"},
	{KEY_2JOY1+11, "SEC_JOY12"},
	{KEY_2JOY1+12, "SEC_JOY13"},
	{KEY_2JOY1+13, "SEC_JOY14"},
	{KEY_2JOY1+14, "SEC_JOY15"},
	{KEY_2JOY1+15, "SEC_JOY16"},
	{KEY_2JOY1+16, "SEC_JOY17"},
	{KEY_2JOY1+17, "SEC_JOY18"},
	{KEY_2JOY1+18, "SEC_JOY19"},
	{KEY_2JOY1+19, "SEC_JOY20"},
	{KEY_2JOY1+20, "SEC_JOY21"},
	{KEY_2JOY1+21, "SEC_JOY22"},
	{KEY_2JOY1+22, "SEC_JOY23"},
	{KEY_2JOY1+23, "SEC_JOY24"},
	{KEY_2JOY1+24, "SEC_JOY25"},
	{KEY_2JOY1+25, "SEC_JOY26"},
	{KEY_2JOY1+26, "SEC_JOY27"},
	{KEY_2JOY1+27, "SEC_JOY28"},
	{KEY_2JOY1+28, "SEC_JOY29"},
	{KEY_2JOY1+29, "SEC_JOY30"},
	{KEY_2JOY1+30, "SEC_JOY31"},
	{KEY_2JOY1+31, "SEC_JOY32"},
#endif
	// the DOS version uses Allegro's joystick support
	{KEY_2HAT1+0,  "SEC_HATUP"},
	{KEY_2HAT1+1,  "SEC_HATDOWN"},
	{KEY_2HAT1+2,  "SEC_HATLEFT"},
	{KEY_2HAT1+3,  "SEC_HATRIGHT"},
	{KEY_2HAT1+4, "SEC_HATUP2"},
	{KEY_2HAT1+5, "SEC_HATDOWN2"},
	{KEY_2HAT1+6, "SEC_HATLEFT2"},
	{KEY_2HAT1+7, "SEC_HATRIGHT2"},
	{KEY_2HAT1+8, "SEC_HATUP3"},
	{KEY_2HAT1+9, "SEC_HATDOWN3"},
	{KEY_2HAT1+10, "SEC_HATLEFT3"},
	{KEY_2HAT1+11, "SEC_HATRIGHT3"},
	{KEY_2HAT1+12, "SEC_HATUP4"},
	{KEY_2HAT1+13, "SEC_HATDOWN4"},
	{KEY_2HAT1+14, "SEC_HATLEFT4"},
	{KEY_2HAT1+15, "SEC_HATRIGHT4"},

#ifdef _PSP
	{KEY_DBL2JOY1+0, "DBLSEC_TRIANGLE"},
	{KEY_DBL2JOY1+1, "DBLSEC_CIRCLE"  },
	{KEY_DBL2JOY1+2, "DBLSEC_CROSS"   },
	{KEY_DBL2JOY1+3, "DBLSEC_SQUARE"  },
	{KEY_DBL2JOY1+4, "DBLSEC_LTRIGGER"},
	{KEY_DBL2JOY1+5, "DBLSEC_RTRIGGER"},
	{KEY_DBL2JOY1+6, "DBLSEC_SELECT"  },
	{KEY_DBL2JOY1+7, "DBLSEC_START"   },
	{KEY_DBL2JOY1+8, "DBLSEC_HOME"    },
	{KEY_DBL2JOY1+9, "DBLSEC_HOLD"    },
#define NOMOREJOYBTN_2DBL
#else
	{KEY_DBL2JOY1+0, "DBLSEC_JOY1"},
	{KEY_DBL2JOY1+1, "DBLSEC_JOY2"},
	{KEY_DBL2JOY1+2, "DBLSEC_JOY3"},
	{KEY_DBL2JOY1+3, "DBLSEC_JOY4"},
	{KEY_DBL2JOY1+4, "DBLSEC_JOY5"},
	{KEY_DBL2JOY1+5, "DBLSEC_JOY6"},
	{KEY_DBL2JOY1+6, "DBLSEC_JOY7"},
	{KEY_DBL2JOY1+7, "DBLSEC_JOY8"},
#endif
#if !defined (NOMOREJOYBTN_2DBL)
	{KEY_DBL2JOY1+8, "DBLSEC_JOY9"},
	{KEY_DBL2JOY1+9, "DBLSEC_JOY10"},
	{KEY_DBL2JOY1+10, "DBLSEC_JOY11"},
	{KEY_DBL2JOY1+11, "DBLSEC_JOY12"},
	{KEY_DBL2JOY1+12, "DBLSEC_JOY13"},
	{KEY_DBL2JOY1+13, "DBLSEC_JOY14"},
	{KEY_DBL2JOY1+14, "DBLSEC_JOY15"},
	{KEY_DBL2JOY1+15, "DBLSEC_JOY16"},
	{KEY_DBL2JOY1+16, "DBLSEC_JOY17"},
	{KEY_DBL2JOY1+17, "DBLSEC_JOY18"},
	{KEY_DBL2JOY1+18, "DBLSEC_JOY19"},
	{KEY_DBL2JOY1+19, "DBLSEC_JOY20"},
	{KEY_DBL2JOY1+20, "DBLSEC_JOY21"},
	{KEY_DBL2JOY1+21, "DBLSEC_JOY22"},
	{KEY_DBL2JOY1+22, "DBLSEC_JOY23"},
	{KEY_DBL2JOY1+23, "DBLSEC_JOY24"},
	{KEY_DBL2JOY1+24, "DBLSEC_JOY25"},
	{KEY_DBL2JOY1+25, "DBLSEC_JOY26"},
	{KEY_DBL2JOY1+26, "DBLSEC_JOY27"},
	{KEY_DBL2JOY1+27, "DBLSEC_JOY28"},
	{KEY_DBL2JOY1+28, "DBLSEC_JOY29"},
	{KEY_DBL2JOY1+29, "DBLSEC_JOY30"},
	{KEY_DBL2JOY1+30, "DBLSEC_JOY31"},
	{KEY_DBL2JOY1+31, "DBLSEC_JOY32"},
#endif
	{KEY_DBL2HAT1+0, "DBLSEC_HATUP"},
	{KEY_DBL2HAT1+1, "DBLSEC_HATDOWN"},
	{KEY_DBL2HAT1+2, "DBLSEC_HATLEFT"},
	{KEY_DBL2HAT1+3, "DBLSEC_HATRIGHT"},
	{KEY_DBL2HAT1+4, "DBLSEC_HATUP2"},
	{KEY_DBL2HAT1+5, "DBLSEC_HATDOWN2"},
	{KEY_DBL2HAT1+6, "DBLSEC_HATLEFT2"},
	{KEY_DBL2HAT1+7, "DBLSEC_HATRIGHT2"},
	{KEY_DBL2HAT1+8, "DBLSEC_HATUP3"},
	{KEY_DBL2HAT1+9, "DBLSEC_HATDOWN3"},
	{KEY_DBL2HAT1+10, "DBLSEC_HATLEFT3"},
	{KEY_DBL2HAT1+11, "DBLSEC_HATRIGHT3"},
	{KEY_DBL2HAT1+12, "DBLSEC_HATUP4"},
	{KEY_DBL2HAT1+13, "DBLSEC_HATDOWN4"},
	{KEY_DBL2HAT1+14, "DBLSEC_HATLEFT4"},
	{KEY_DBL2HAT1+15, "DBLSEC_HATRIGHT4"},


	{KEY_3JOY1+0, "TRD_JOY1"},
	{KEY_3JOY1+1, "TRD_JOY2"},
	{KEY_3JOY1+2, "TRD_JOY3"},
	{KEY_3JOY1+3, "TRD_JOY4"},
	{KEY_3JOY1+4, "TRD_JOY5"},
	{KEY_3JOY1+5, "TRD_JOY6"},
	{KEY_3JOY1+6, "TRD_JOY7"},
	{KEY_3JOY1+7, "TRD_JOY8"},
	{KEY_3JOY1+8, "TRD_JOY9"},
	{KEY_3JOY1+9, "TRD_JOY10"},
	{KEY_3JOY1+10, "TRD_JOY11"},
	{KEY_3JOY1+11, "TRD_JOY12"},
	{KEY_3JOY1+12, "TRD_JOY13"},
	{KEY_3JOY1+13, "TRD_JOY14"},
	{KEY_3JOY1+14, "TRD_JOY15"},
	{KEY_3JOY1+15, "TRD_JOY16"},
	{KEY_3JOY1+16, "TRD_JOY17"},
	{KEY_3JOY1+17, "TRD_JOY18"},
	{KEY_3JOY1+18, "TRD_JOY19"},
	{KEY_3JOY1+19, "TRD_JOY20"},
	{KEY_3JOY1+20, "TRD_JOY21"},
	{KEY_3JOY1+21, "TRD_JOY22"},
	{KEY_3JOY1+22, "TRD_JOY23"},
	{KEY_3JOY1+23, "TRD_JOY24"},
	{KEY_3JOY1+24, "TRD_JOY25"},
	{KEY_3JOY1+25, "TRD_JOY26"},
	{KEY_3JOY1+26, "TRD_JOY27"},
	{KEY_3JOY1+27, "TRD_JOY28"},
	{KEY_3JOY1+28, "TRD_JOY29"},
	{KEY_3JOY1+29, "TRD_JOY30"},
	{KEY_3JOY1+30, "TRD_JOY31"},
	{KEY_3JOY1+31, "TRD_JOY32"},

	{KEY_DBL3JOY1+0, "DBLTRD_JOY1"},
	{KEY_DBL3JOY1+1, "DBLTRD_JOY2"},
	{KEY_DBL3JOY1+2, "DBLTRD_JOY3"},
	{KEY_DBL3JOY1+3, "DBLTRD_JOY4"},
	{KEY_DBL3JOY1+4, "DBLTRD_JOY5"},
	{KEY_DBL3JOY1+5, "DBLTRD_JOY6"},
	{KEY_DBL3JOY1+6, "DBLTRD_JOY7"},
	{KEY_DBL3JOY1+7, "DBLTRD_JOY8"},
	{KEY_DBL3JOY1+8, "DBLTRD_JOY9"},
	{KEY_DBL3JOY1+9, "DBLTRD_JOY10"},
	{KEY_DBL3JOY1+10, "DBLTRD_JOY11"},
	{KEY_DBL3JOY1+11, "DBLTRD_JOY12"},
	{KEY_DBL3JOY1+12, "DBLTRD_JOY13"},
	{KEY_DBL3JOY1+13, "DBLTRD_JOY14"},
	{KEY_DBL3JOY1+14, "DBLTRD_JOY15"},
	{KEY_DBL3JOY1+15, "DBLTRD_JOY16"},
	{KEY_DBL3JOY1+16, "DBLTRD_JOY17"},
	{KEY_DBL3JOY1+17, "DBLTRD_JOY18"},
	{KEY_DBL3JOY1+18, "DBLTRD_JOY19"},
	{KEY_DBL3JOY1+19, "DBLTRD_JOY20"},
	{KEY_DBL3JOY1+20, "DBLTRD_JOY21"},
	{KEY_DBL3JOY1+21, "DBLTRD_JOY22"},
	{KEY_DBL3JOY1+22, "DBLTRD_JOY23"},
	{KEY_DBL3JOY1+23, "DBLTRD_JOY24"},
	{KEY_DBL3JOY1+24, "DBLTRD_JOY25"},
	{KEY_DBL3JOY1+25, "DBLTRD_JOY26"},
	{KEY_DBL3JOY1+26, "DBLTRD_JOY27"},
	{KEY_DBL3JOY1+27, "DBLTRD_JOY28"},
	{KEY_DBL3JOY1+28, "DBLTRD_JOY29"},
	{KEY_DBL3JOY1+29, "DBLTRD_JOY30"},
	{KEY_DBL3JOY1+30, "DBLTRD_JOY31"},
	{KEY_DBL3JOY1+31, "DBLTRD_JOY32"},

	{KEY_3HAT1+0,  "TRD_HATUP"},
	{KEY_3HAT1+1,  "TRD_HATDOWN"},
	{KEY_3HAT1+2,  "TRD_HATLEFT"},
	{KEY_3HAT1+3,  "TRD_HATRIGHT"},
	{KEY_3HAT1+4, "TRD_HATUP2"},
	{KEY_3HAT1+5, "TRD_HATDOWN2"},
	{KEY_3HAT1+6, "TRD_HATLEFT2"},
	{KEY_3HAT1+7, "TRD_HATRIGHT2"},
	{KEY_3HAT1+8, "TRD_HATUP3"},
	{KEY_3HAT1+9, "TRD_HATDOWN3"},
	{KEY_3HAT1+10, "TRD_HATLEFT3"},
	{KEY_3HAT1+11, "TRD_HATRIGHT3"},
	{KEY_3HAT1+12, "TRD_HATUP4"},
	{KEY_3HAT1+13, "TRD_HATDOWN4"},
	{KEY_3HAT1+14, "TRD_HATLEFT4"},
	{KEY_3HAT1+15, "TRD_HATRIGHT4"},

	{KEY_DBL3HAT1+0, "DBLTRD_HATUP"},
	{KEY_DBL3HAT1+1, "DBLTRD_HATDOWN"},
	{KEY_DBL3HAT1+2, "DBLTRD_HATLEFT"},
	{KEY_DBL3HAT1+3, "DBLTRD_HATRIGHT"},
	{KEY_DBL3HAT1+4, "DBLTRD_HATUP2"},
	{KEY_DBL3HAT1+5, "DBLTRD_HATDOWN2"},
	{KEY_DBL3HAT1+6, "DBLTRD_HATLEFT2"},
	{KEY_DBL3HAT1+7, "DBLTRD_HATRIGHT2"},
	{KEY_DBL3HAT1+8, "DBLTRD_HATUP3"},
	{KEY_DBL3HAT1+9, "DBLTRD_HATDOWN3"},
	{KEY_DBL3HAT1+10, "DBLTRD_HATLEFT3"},
	{KEY_DBL3HAT1+11, "DBLTRD_HATRIGHT3"},
	{KEY_DBL3HAT1+12, "DBLTRD_HATUP4"},
	{KEY_DBL3HAT1+13, "DBLTRD_HATDOWN4"},
	{KEY_DBL3HAT1+14, "DBLTRD_HATLEFT4"},
	{KEY_DBL3HAT1+15, "DBLTRD_HATRIGHT4"},

	{KEY_4JOY1+0, "FOR_JOY1"},
	{KEY_4JOY1+1, "FOR_JOY2"},
	{KEY_4JOY1+2, "FOR_JOY3"},
	{KEY_4JOY1+3, "FOR_JOY4"},
	{KEY_4JOY1+4, "FOR_JOY5"},
	{KEY_4JOY1+5, "FOR_JOY6"},
	{KEY_4JOY1+6, "FOR_JOY7"},
	{KEY_4JOY1+7, "FOR_JOY8"},
	{KEY_4JOY1+8, "FOR_JOY9"},
	{KEY_4JOY1+9, "FOR_JOY10"},
	{KEY_4JOY1+10, "FOR_JOY11"},
	{KEY_4JOY1+11, "FOR_JOY12"},
	{KEY_4JOY1+12, "FOR_JOY13"},
	{KEY_4JOY1+13, "FOR_JOY14"},
	{KEY_4JOY1+14, "FOR_JOY15"},
	{KEY_4JOY1+15, "FOR_JOY16"},
	{KEY_4JOY1+16, "FOR_JOY17"},
	{KEY_4JOY1+17, "FOR_JOY18"},
	{KEY_4JOY1+18, "FOR_JOY19"},
	{KEY_4JOY1+19, "FOR_JOY20"},
	{KEY_4JOY1+20, "FOR_JOY21"},
	{KEY_4JOY1+21, "FOR_JOY22"},
	{KEY_4JOY1+22, "FOR_JOY23"},
	{KEY_4JOY1+23, "FOR_JOY24"},
	{KEY_4JOY1+24, "FOR_JOY25"},
	{KEY_4JOY1+25, "FOR_JOY26"},
	{KEY_4JOY1+26, "FOR_JOY27"},
	{KEY_4JOY1+27, "FOR_JOY28"},
	{KEY_4JOY1+28, "FOR_JOY29"},
	{KEY_4JOY1+29, "FOR_JOY30"},
	{KEY_4JOY1+30, "FOR_JOY31"},
	{KEY_4JOY1+31, "FOR_JOY32"},

	{KEY_DBL4JOY1+0, "DBLFOR_JOY1"},
	{KEY_DBL4JOY1+1, "DBLFOR_JOY2"},
	{KEY_DBL4JOY1+2, "DBLFOR_JOY3"},
	{KEY_DBL4JOY1+3, "DBLFOR_JOY4"},
	{KEY_DBL4JOY1+4, "DBLFOR_JOY5"},
	{KEY_DBL4JOY1+5, "DBLFOR_JOY6"},
	{KEY_DBL4JOY1+6, "DBLFOR_JOY7"},
	{KEY_DBL4JOY1+7, "DBLFOR_JOY8"},
	{KEY_DBL4JOY1+8, "DBLFOR_JOY9"},
	{KEY_DBL4JOY1+9, "DBLFOR_JOY10"},
	{KEY_DBL4JOY1+10, "DBLFOR_JOY11"},
	{KEY_DBL4JOY1+11, "DBLFOR_JOY12"},
	{KEY_DBL4JOY1+12, "DBLFOR_JOY13"},
	{KEY_DBL4JOY1+13, "DBLFOR_JOY14"},
	{KEY_DBL4JOY1+14, "DBLFOR_JOY15"},
	{KEY_DBL4JOY1+15, "DBLFOR_JOY16"},
	{KEY_DBL4JOY1+16, "DBLFOR_JOY17"},
	{KEY_DBL4JOY1+17, "DBLFOR_JOY18"},
	{KEY_DBL4JOY1+18, "DBLFOR_JOY19"},
	{KEY_DBL4JOY1+19, "DBLFOR_JOY20"},
	{KEY_DBL4JOY1+20, "DBLFOR_JOY21"},
	{KEY_DBL4JOY1+21, "DBLFOR_JOY22"},
	{KEY_DBL4JOY1+22, "DBLFOR_JOY23"},
	{KEY_DBL4JOY1+23, "DBLFOR_JOY24"},
	{KEY_DBL4JOY1+24, "DBLFOR_JOY25"},
	{KEY_DBL4JOY1+25, "DBLFOR_JOY26"},
	{KEY_DBL4JOY1+26, "DBLFOR_JOY27"},
	{KEY_DBL4JOY1+27, "DBLFOR_JOY28"},
	{KEY_DBL4JOY1+28, "DBLFOR_JOY29"},
	{KEY_DBL4JOY1+29, "DBLFOR_JOY30"},
	{KEY_DBL4JOY1+30, "DBLFOR_JOY31"},
	{KEY_DBL4JOY1+31, "DBLFOR_JOY32"},

	{KEY_4HAT1+0,  "FOR_HATUP"},
	{KEY_4HAT1+1,  "FOR_HATDOWN"},
	{KEY_4HAT1+2,  "FOR_HATLEFT"},
	{KEY_4HAT1+3,  "FOR_HATRIGHT"},
	{KEY_4HAT1+4, "FOR_HATUP2"},
	{KEY_4HAT1+5, "FOR_HATDOWN2"},
	{KEY_4HAT1+6, "FOR_HATLEFT2"},
	{KEY_4HAT1+7, "FOR_HATRIGHT2"},
	{KEY_4HAT1+8, "FOR_HATUP3"},
	{KEY_4HAT1+9, "FOR_HATDOWN3"},
	{KEY_4HAT1+10, "FOR_HATLEFT3"},
	{KEY_4HAT1+11, "FOR_HATRIGHT3"},
	{KEY_4HAT1+12, "FOR_HATUP4"},
	{KEY_4HAT1+13, "FOR_HATDOWN4"},
	{KEY_4HAT1+14, "FOR_HATLEFT4"},
	{KEY_4HAT1+15, "FOR_HATRIGHT4"},

	{KEY_DBL4HAT1+0, "DBLFOR_HATUP"},
	{KEY_DBL4HAT1+1, "DBLFOR_HATDOWN"},
	{KEY_DBL4HAT1+2, "DBLFOR_HATLEFT"},
	{KEY_DBL4HAT1+3, "DBLFOR_HATRIGHT"},
	{KEY_DBL4HAT1+4, "DBLFOR_HATUP2"},
	{KEY_DBL4HAT1+5, "DBLFOR_HATDOWN2"},
	{KEY_DBL4HAT1+6, "DBLFOR_HATLEFT2"},
	{KEY_DBL4HAT1+7, "DBLFOR_HATRIGHT2"},
	{KEY_DBL4HAT1+8, "DBLFOR_HATUP3"},
	{KEY_DBL4HAT1+9, "DBLFOR_HATDOWN3"},
	{KEY_DBL4HAT1+10, "DBLFOR_HATLEFT3"},
	{KEY_DBL4HAT1+11, "DBLFOR_HATRIGHT3"},
	{KEY_DBL4HAT1+12, "DBLFOR_HATUP4"},
	{KEY_DBL4HAT1+13, "DBLFOR_HATDOWN4"},
	{KEY_DBL4HAT1+14, "DBLFOR_HATLEFT4"},
	{KEY_DBL4HAT1+15, "DBLFOR_HATRIGHT4"},

};

static const char *gamecontrolname[num_gamecontrols] =
{
	"nothing", // a key/button mapped to gc_null has no effect
	"aimforward",
	"aimbackward",
	"turnleft",
	"turnright",
	"accelerate",
	"drift",
	"brake",
	"fire",
	"lookback",
	"camreset",
	"camtoggle",
	"spectate",
	"lookup",
	"lookdown",
	"centerview",
	"talkkey",
	"teamtalkkey",
	"scores",
	"console",
	"pause",
	"systemmenu",
	"screenshot",
	"recordgif",
	"viewpoint",
	"custom1",
	"custom2",
	"custom3",
	"director",
};

#define NUMKEYNAMES (sizeof (keynames)/sizeof (keyname_t))

static INT32 G_GetDeviceForPlayer(INT32 player)
{
	switch (player)
	{
		case 0:
			return cv_usejoystick.value;
			break;
		case 1:
			return cv_usejoystick2.value;
			break;
		case 2:
			return cv_usejoystick3.value;
			break;
		case 3:
			return cv_usejoystick4.value;
			break;
		default:
			return 0;
			break;
	}
}

static UINT16 G_GetSkinColor(INT32 player)
{
	if (gamestate == GS_LEVEL)
		return players[displayplayers[player]].skincolor;

	switch (player)
	{
		case 0:
			return cv_playercolor.value;
			break;
		case 1:
			return cv_playercolor2.value;
			break;
		case 2:
			return cv_playercolor3.value;
			break;
		case 3:
			return cv_playercolor4.value;
			break;
		default:
			return 0;
			break;
	}
}

// ehhhhhh ill maybe add splitscreen support eventually lol
void G_SetPlayerGamepadIndicatorToPlayerColor(INT32 player, UINT16 color)
{
	INT32 device;
	UINT16 skincolor;
	byteColor_t byte_color;

	I_Assert(player >= 0 && player < MAXSPLITSCREENPLAYERS);

	device = G_GetDeviceForPlayer(player);

	if (device <= 0)
	{
		return;
	}

	if (color)
		skincolor = color;
	else
		skincolor = G_GetSkinColor(player);

	byte_color = V_GetColor(colortranslations[skincolor][8]).s;

	I_SetGamepadIndicatorColor(device, byte_color.red, byte_color.green, byte_color.blue);
}

static void G_ResetPlayerDeviceRumble(INT32 player)
{
	INT32 device_id;

	device_id = G_GetDeviceForPlayer(player);

	if (device_id < 1)
	{
		return;
	}

	I_GamepadRumble(device_id, 0, 0);
}

void G_PlayerDeviceRumble(INT32 player, UINT16 low_strength, UINT16 high_strength)
{
	INT32 device_id;

	if (cv_rumble[player].value == 0)
	{
		return;
	}

	device_id = G_GetDeviceForPlayer(player);

	if (device_id < 1)
	{
		return;
	}

	I_GamepadRumble(device_id, low_strength, high_strength);
}

//
// Detach any keys associated to the given game control
// - pass the pointer to the gamecontrol table for the player being edited
void G_ClearControlKeys(INT32 (*setupcontrols)[2], INT32 control)
{
	setupcontrols[control][0] = KEY_NULL;
	setupcontrols[control][1] = KEY_NULL;
}

void G_ClearAllControlKeys(void)
{
	INT32 i;
	for (i = 0; i < num_gamecontrols; i++)
	{
		G_ClearControlKeys(gamecontrol, i);
		G_ClearControlKeys(gamecontrolbis, i);
		G_ClearControlKeys(gamecontrol3, i);
		G_ClearControlKeys(gamecontrol4, i);
	}
}

//
// Returns the name of a key (or virtual key for mouse and joy)
// the input value being an keynum
//
const char *G_KeynumToString(INT32 keynum)
{
	static char keynamestr[8];

	UINT32 j;

	// return a string with the ascii char if displayable
	if (keynum > ' ' && keynum <= 'z' && keynum != KEY_CONSOLE)
	{
		keynamestr[0] = (char)keynum;
		keynamestr[1] = '\0';
		return keynamestr;
	}

	// find a description for special keys
	for (j = 0; j < NUMKEYNAMES; j++)
		if (keynames[j].keynum == keynum)
			return keynames[j].name;

	// create a name for unknown keys
	sprintf(keynamestr, "KEY%d", keynum);
	return keynamestr;
}

INT32 G_KeyStringtoNum(const char *keystr)
{
	UINT32 j;

	if (!keystr[0])
		return 0;

	if (!keystr[1] && keystr[0] > ' ' && keystr[0] <= 'z')
		return keystr[0];

	if (!strncmp(keystr, "KEY", 3) && keystr[3] >= '0' && keystr[3] <= '9')
		return atoi(&keystr[3]);

	for (j = 0; j < NUMKEYNAMES; j++)
		if (!stricmp(keynames[j].name, keystr))
			return keynames[j].keynum;

	return 0;
}

// SRB2Kart
void G_Controldefault(UINT8 player)
{
	if (player == 0 || player == 1)
	{
		// Main controls
		gamecontrol[gc_aimforward ][0] = KEY_UPARROW;
		gamecontrol[gc_aimbackward][0] = KEY_DOWNARROW;
		gamecontrol[gc_turnleft   ][0] = KEY_LEFTARROW;
		gamecontrol[gc_turnright  ][0] = KEY_RIGHTARROW;
		gamecontrol[gc_accelerate ][0] = 'a';
		gamecontrol[gc_drift      ][0] = 's';
		gamecontrol[gc_brake      ][0] = 'd';
		gamecontrol[gc_fire       ][0] = KEY_SPACE;
		gamecontrol[gc_lookback   ][0] = KEY_LSHIFT;

		gamecontrol[gc_accelerate ][1] = KEY_JOY1+0; // A
		gamecontrol[gc_lookback   ][1] = KEY_JOY1+2; // X
		gamecontrol[gc_brake      ][1] = KEY_JOY1+1; // B
		gamecontrol[gc_fire       ][1] = KEY_JOY1+9; // LB
		gamecontrol[gc_drift      ][1] = KEY_JOY1+10; // RB

		// Extra controls
		gamecontrol[gc_pause      ][0] = KEY_PAUSE;
		gamecontrol[gc_console    ][0] = KEY_CONSOLE;
		gamecontrol[gc_screenshot ][0] = KEY_F8;
		gamecontrol[gc_recordgif  ][0] = KEY_F9;
		gamecontrol[gc_viewpoint  ][0] = KEY_F12;
		gamecontrol[gc_talkkey    ][0] = 't';
		//gamecontrol[gc_teamkey    ][0] = 'y';
		gamecontrol[gc_scores     ][0] = KEY_TAB;
		gamecontrol[gc_spectate   ][0] = '\'';
		gamecontrol[gc_lookup     ][0] = KEY_PGUP;
		gamecontrol[gc_lookdown   ][0] = KEY_PGDN;
		gamecontrol[gc_centerview ][0] = KEY_END;
		gamecontrol[gc_camreset   ][0] = KEY_HOME;
		gamecontrol[gc_camtoggle  ][0] = KEY_BACKSPACE;

		gamecontrol[gc_viewpoint  ][1] = KEY_JOY1+3; // Y
		gamecontrol[gc_pause      ][1] = KEY_JOY1+4; // Back
		gamecontrol[gc_systemmenu ][0] = KEY_JOY1+6; // Start
		//gamecontrol[gc_camtoggle  ][1] = KEY_HAT1+0; // D-Pad Up
		//gamecontrol[gc_screenshot ][1] = KEY_HAT1+1; // D-Pad Down // absolutely fucking NOT
		gamecontrol[gc_talkkey    ][1] = KEY_HAT1+1; // D-Pad Down
		gamecontrol[gc_scores     ][1] = KEY_HAT1+0; // D-Pad Up
	}

	if (player == 0 || player == 2)
	{
		// Player 2 controls
		gamecontrolbis[gc_accelerate ][0] = KEY_2JOY1+0; // A
		gamecontrolbis[gc_lookback   ][0] = KEY_2JOY1+2; // X
		gamecontrolbis[gc_brake      ][0] = KEY_2JOY1+1; // B
		gamecontrolbis[gc_fire       ][0] = KEY_2JOY1+9; // LB
		gamecontrolbis[gc_drift      ][0] = KEY_2JOY1+10; // RB
	}

	if (player == 0 || player == 3)
	{
		// Player 3 controls
		gamecontrol3[gc_accelerate ][0] = KEY_3JOY1+0; // A
		gamecontrol3[gc_lookback   ][0] = KEY_3JOY1+2; // X
		gamecontrol3[gc_brake      ][0] = KEY_3JOY1+1; // B
		gamecontrol3[gc_fire       ][0] = KEY_3JOY1+9; // LB
		gamecontrol3[gc_drift      ][0] = KEY_3JOY1+10; // RB
	}

	if (player == 0 || player == 4)
	{
		// Player 4 controls
		gamecontrol4[gc_accelerate ][0] = KEY_4JOY1+0; // A
		gamecontrol4[gc_lookback   ][0] = KEY_4JOY1+2; // X
		gamecontrol4[gc_brake      ][0] = KEY_4JOY1+1; // B
		gamecontrol4[gc_fire       ][0] = KEY_4JOY1+9; // LB
		gamecontrol4[gc_drift      ][0] = KEY_4JOY1+10; // RB
	}
}

void G_SaveKeySetting(FILE *f)
{
	INT32 i;

	for (i = 1; i < num_gamecontrols; i++)
	{
		fprintf(f, "setcontrol \"%s\" \"%s\"", gamecontrolname[i],
			G_KeynumToString(gamecontrol[i][0]));

		if (gamecontrol[i][1])
			fprintf(f, " \"%s\"\n", G_KeynumToString(gamecontrol[i][1]));
		else
			fprintf(f, "\n");
	}

	for (i = 1; i < num_gamecontrols; i++)
	{
		fprintf(f, "setcontrol2 \"%s\" \"%s\"", gamecontrolname[i],
			G_KeynumToString(gamecontrolbis[i][0]));

		if (gamecontrolbis[i][1])
			fprintf(f, " \"%s\"\n", G_KeynumToString(gamecontrolbis[i][1]));
		else
			fprintf(f, "\n");
	}

	for (i = 1; i < num_gamecontrols; i++)
	{
		fprintf(f, "setcontrol3 \"%s\" \"%s\"", gamecontrolname[i],
			G_KeynumToString(gamecontrol3[i][0]));

		if (gamecontrol3[i][1])
			fprintf(f, " \"%s\"\n", G_KeynumToString(gamecontrol3[i][1]));
		else
			fprintf(f, "\n");
	}

	for (i = 1; i < num_gamecontrols; i++)
	{
		fprintf(f, "setcontrol4 \"%s\" \"%s\"", gamecontrolname[i],
			G_KeynumToString(gamecontrol4[i][0]));

		if (gamecontrol4[i][1])
			fprintf(f, " \"%s\"\n", G_KeynumToString(gamecontrol4[i][1]));
		else
			fprintf(f, "\n");
	}
}

INT32 G_CheckDoubleUsage(INT32 keynum, boolean modify)
{
	INT32 result = gc_null;
	if (cv_controlperkey.value == 1)
	{
		INT32 i, j;
		for (i = 0; i < num_gamecontrols; i++)
		{
			for (j = 0; j < 2; j++)
			{
				if (gamecontrol[i][j] == keynum) {
					result = i;
					if (modify) gamecontrol[i][j] = KEY_NULL;
				}
				if (gamecontrolbis[i][j] == keynum) {
					result = i;
					if (modify) gamecontrolbis[i][j] = KEY_NULL;
				}
				if (gamecontrol3[i][j] == keynum) {
					result = i;
					if (modify) gamecontrol3[i][j] = KEY_NULL;
				}
				if (gamecontrol4[i][j] == keynum) {
					result = i;
					if (modify) gamecontrol4[i][j] = KEY_NULL;
				}
				if (result && !modify)
					return result;
			}
		}
	}
	return result;
}

static INT32 G_FilterKeyByVersion(INT32 numctrl, INT32 keyidx, INT32 player, INT32 *keynum1, INT32 *keynum2, boolean *nestedoverride)
{
#if 1 // SRB2Kart filters/migrations
	(void)nestedoverride;

	// Migration: 1.6 (majorexec 10) Joystick Defaults changed to use SDL Game Controllers
	if (GETMAJOREXECVERSION(cv_execversion.value) < 10)
	{
		INT32 joybuttonbase = KEY_JOY1;

		switch (player)
		{
			case 0:
				joybuttonbase = KEY_JOY1;
				break;
			case 1:
				joybuttonbase = KEY_2JOY1;
				break;
			case 2:
				joybuttonbase = KEY_3JOY1;
				break;
			case 3:
				joybuttonbase = KEY_4JOY1;
				break;
		}

		// The face buttons match, so we don't need to rebind those.

		if (keyidx == 1 && numctrl == gc_fire && *keynum2 == joybuttonbase + 4) // Xbox DInput LB
		{
			*keynum2 = joybuttonbase + 9; // SDL LEFTSHOULDER
		}
		if (keyidx == 1 && numctrl == gc_drift && *keynum2 == joybuttonbase + 5) // Xbox DInput RB
		{
			*keynum2 = joybuttonbase + 10; // SDL RIGHTSHOULDER
		}

		// Pause and Systemmenu are only bound for P1
		if (keyidx == 1 && player == 0 && numctrl == gc_pause && *keynum2 == joybuttonbase + 6) // Xbox DInput Back
		{
			*keynum2 = joybuttonbase + 4; // SDL BACK
		}
		if (keyidx == 0 && player == 0 && numctrl == gc_systemmenu && *keynum1 == joybuttonbase + 7) // Xbox DInput Start
		{
			*keynum1 = joybuttonbase + 6; // SDL START
		}
	}
#else
	{
		INT32 keynum = 0, existingctrl = 0;
		INT32 defaultkey;
		boolean defaultoverride = false;

		// get the default gamecontrol
		if (player == 0 && numctrl == gc_systemmenu)
			defaultkey = gamecontrol[numctrl][0];
		else
			defaultkey = (player == 1 ? gamecontrolbis[numctrl][0] : gamecontrol[numctrl][1]);

		// Assign joypad button defaults if there is an open slot.
		// At this point, gamecontrol/bis should have the default controls
		// (unless LOADCONFIG is being run)
		//
		// If the player runs SETCONTROL in-game, this block should not be reached
		// because EXECVERSION is locked onto the latest version.
		if (keyidx == 0 && !*keynum1)
		{
			if (*keynum2) // push keynum2 down; this is an edge case
			{
				*keynum1 = *keynum2;
				*keynum2 = 0;
				keynum = *keynum1;
			}
			else
			{
				keynum = defaultkey;
				defaultoverride = true;
			}
		}
		else if (keyidx == 1 && (!*keynum2 || (!*keynum1 && *keynum2))) // last one is the same edge case as above
		{
			keynum = defaultkey;
			defaultoverride = true;
		}
		else // default to the specified keynum
			keynum = (keyidx == 1 ? *keynum2 : *keynum1);

		// Did our last call override keynum2?
		if (*nestedoverride)
		{
			defaultoverride = true;
			*nestedoverride = false;
		}

		// Fill keynum2 with the default control
		if (keyidx == 0 && !*keynum2)
		{
			*keynum2 = defaultkey;
			// Tell the next call that this is an override
			*nestedoverride = true;

			// if keynum2 already matches keynum1, we probably recursed
			// so unset it
			if (*keynum1 == *keynum2)
			{
				*keynum2 = 0;
				*nestedoverride = false;
		}
		}

		// check if the key is being used somewhere else before passing it
		// pass it through if it's the same numctrl. This is an edge case -- when using
		// LOADCONFIG, gamecontrol is not reset with default.
		//
		// Also, only check if we're actually overriding, to preserve behavior where
		// config'd keys overwrite default keys.
		if (defaultoverride)
			existingctrl = G_CheckDoubleUsage(keynum, false);

		if (keynum && (!existingctrl || existingctrl == numctrl))
			return keynum;
		else if (keyidx == 0 && *keynum2)
		{
			// try it again and push down keynum2
			*keynum1 = *keynum2;
			*keynum2 = 0;
			return G_FilterKeyByVersion(numctrl, keyidx, player, keynum1, keynum2, nestedoverride);
			// recursion *should* be safe because we only assign keynum2 to a joy default
			// and then clear it if we find that keynum1 already has the joy default.
		}
		else
			return 0;
	}
#endif

	// All's good, so pass the keynum as-is
	if (keyidx == 1)
		return *keynum2;
	else //if (keyidx == 0)
		return *keynum1;
}

static void setcontrol(INT32 (*gc)[2])
{
	INT32 numctrl;
	const char *namectrl;
	INT32 keynum, keynum1, keynum2;
	INT32 player;
	boolean nestedoverride = false;

	if ((void*)gc == (void*)&gamecontrol4)
		player = 3;
	else if ((void*)gc == (void*)&gamecontrol3)
		player = 2;
	else if ((void*)gc == (void*)&gamecontrolbis)
		player = 1;
	else
		player = 0;

	namectrl = COM_Argv(1);
	for (numctrl = 0; numctrl < num_gamecontrols && stricmp(namectrl, gamecontrolname[numctrl]);
		numctrl++)
		;
	if (numctrl == num_gamecontrols)
	{
		CONS_Printf(M_GetText("Control '%s' unknown\n"), namectrl);
		return;
	}
	keynum1 = G_KeyStringtoNum(COM_Argv(2));
	keynum2 = G_KeyStringtoNum(COM_Argv(3));
	keynum = G_FilterKeyByVersion(numctrl, 0, player, &keynum1, &keynum2, &nestedoverride);

	if (keynum >= 0)
	{
		(void)G_CheckDoubleUsage(keynum, true);

		// if keynum was rejected, try it again with keynum2
		if (!keynum && keynum2)
		{
			keynum1 = keynum2; // push down keynum2
			keynum2 = 0;
			keynum = G_FilterKeyByVersion(numctrl, 0, player, &keynum1, &keynum2, &nestedoverride);
			if (keynum >= 0)
				(void)G_CheckDoubleUsage(keynum, true);
		}
	}

	if (keynum >= 0)
		gc[numctrl][0] = keynum;

	if (keynum2)
	{
		keynum = G_FilterKeyByVersion(numctrl, 1, player, &keynum1, &keynum2, &nestedoverride);
		if (keynum >= 0)
		{
			if (keynum != gc[numctrl][0])
				gc[numctrl][1] = keynum;
			else
				gc[numctrl][1] = 0;
		}
	}
	else
		gc[numctrl][1] = 0;
}

void Command_Setcontrol_f(void)
{
	INT32 na;

	na = (INT32)COM_Argc();

	if (na != 3 && na != 4)
	{
		CONS_Printf(M_GetText("setcontrol <controlname> <keyname> [<2nd keyname>]: set controls for player 1\n"));
		return;
	}

	setcontrol(gamecontrol);
}

void Command_Setcontrol2_f(void)
{
	INT32 na;

	na = (INT32)COM_Argc();

	if (na != 3 && na != 4)
	{
		CONS_Printf(M_GetText("setcontrol2 <controlname> <keyname> [<2nd keyname>]: set controls for player 2\n"));
		return;
	}

	setcontrol(gamecontrolbis);
}

void Command_Setcontrol3_f(void)
{
	INT32 na;

	na = (INT32)COM_Argc();

	if (na != 3 && na != 4)
	{
		CONS_Printf(M_GetText("setcontrol3 <controlname> <keyname> [<2nd keyname>]: set controls for player 3\n"));
		return;
	}

	setcontrol(gamecontrol3);
}

void Command_Setcontrol4_f(void)
{
	INT32 na;

	na = (INT32)COM_Argc();

	if (na != 3 && na != 4)
	{
		CONS_Printf(M_GetText("setcontrol4 <controlname> <keyname> [<2nd keyname>]: set controls for player 4\n"));
		return;
	}

	setcontrol(gamecontrol4);
}
