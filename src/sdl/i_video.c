// Emacs style mode select   -*- C++ -*-
// SONIC ROBO BLAST 2
//-----------------------------------------------------------------------------
//
// Copyright (C) 1993-1996 by id Software, Inc.
// Portions Copyright (C) 1998-2000 by DooM Legacy Team.
// Copyright (C) 2014-2018 by Sonic Team Junior.
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
/// \brief SRB2 graphics stuff for SDL

#include <stdlib.h>
#include <errno.h>

#include <signal.h>

#ifdef HAVE_SDL
#define _MATH_DEFINES_DEFINED
#include "SDL.h"

#ifdef HAVE_TTF
#include "i_ttf.h"
#endif

#ifdef HAVE_IMAGE
#include "SDL_image.h"
#elif defined (__unix__) || (!defined(__APPLE__) && defined (UNIXCOMMON)) // Windows & Mac don't need this, as SDL will do it for us.
#define LOAD_XPM //I want XPM!
#include "IMG_xpm.c" //Alam: I don't want to add SDL_Image.dll/so
#define HAVE_IMAGE //I have SDL_Image, sortof
#endif

#ifdef HAVE_IMAGE
#include "SDL_icon.xpm"
#endif

#include "../doomdef.h"

#ifdef _WIN32
#include "SDL_syswm.h"
#endif

#include "../doomstat.h"
#include "../p_setup.h"
#include "../i_system.h"
#include "../v_video.h"
#include "../m_argv.h"
#include "../m_menu.h"
#include "../d_main.h"
#include "../s_sound.h"
#include "../i_sound.h"  	// midi pause/unpause
#include "../i_joy.h"
#include "../st_stuff.h"
#include "../g_game.h"
#include "../i_video.h"
#include "../console.h"
#include "../command.h"
#include "../i_system.h"
#include "../hu_stuff.h" // for chat_on

#include "sdlmain.h"

#ifdef HWRENDER
#include "../hardware/hw_main.h"
#include "../hardware/hw_gl.h"
#include "../hardware/r_opengl/r_opengl.h" //for supportFBO

#include "hwsym_sdl.h" // For dynamic referencing of HW rendering functions
#include "ogl_sdl.h"
#endif

#ifdef HAVE_DISCORDRPC
#include "../discord.h"
#endif

// maximum number of windowed modes (see windowedModes[][])
#define MAXWINMODES (22)

/**	\brief
*/
static INT32 numVidModes = -1;

/**	\brief
*/
static char vidModeName[33][32]; // allow 33 different modes

rendermode_t rendermode = render_none;

#ifdef HWRENDER
unsigned msaa = 0;

// Eee probably the way i organize it isn't best ><
// Just don't know where do i put declaration and "implementation"
boolean a2c = false;
#endif

static void KeyboardLayout_OnChange(void)
{
	if (cv_keyboardlayout.value != 2)
		SDL_StopTextInput();
	HU_Shiftform();
}

static CV_PossibleValue_t keyboardlayout_cons_t[] = {{1,"Default US"}, {2, "Native"}, {3, "AZERTY"}, {0, NULL}};
consvar_t cv_keyboardlayout = {"keyboardlayout", "Default US", CV_SAVE|CV_CALL, keyboardlayout_cons_t, KeyboardLayout_OnChange, 0, NULL, NULL, 0, 0, NULL};

static void Impl_SetVsync(void);

static INT32 desktopwidth = 0, desktopheight = 0;

static void I_CheckDesktopRes(void);
#ifdef USE_FBO_OGL
static void I_ResetFBOSurface(void);
#endif
// synchronize page flipping with screen refresh
consvar_t cv_vidwait = {"vid_wait", "Off", CV_SAVE|CV_CALL|CV_NOINIT, CV_OnOff, Impl_SetVsync, 0, NULL, NULL, 0, 0, NULL};
static consvar_t cv_stretch = {"stretch", "Off", CV_SAVE|CV_NOSHOWHELP, CV_OnOff, NULL, 0, NULL, NULL, 0, 0, NULL};

// these cant be used since config is read after window creation, so need to use command line parameter instead
//static CV_PossibleValue_t msaa_cons_t[] = {{0, "Off"}, {2, "2X"}, {4, "4X"}, {8, "8X"}, {16, "16X"}, {0, NULL}};
//consvar_t cv_msaa = {"msaa", "Off", CV_SAVE, msaa_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL};

UINT8 graphics_started = 0; // Is used in console.c and screen.c

// To disable fullscreen at startup; is set in VID_PrepareModeList
boolean allow_fullscreen = false;
static SDL_bool disable_fullscreen = SDL_FALSE;
#define USE_FULLSCREEN (disable_fullscreen||!allow_fullscreen)? 0: (cv_fullscreen.value == 1)
static SDL_bool disable_mouse = SDL_FALSE;
#define USE_MOUSEINPUT (!disable_mouse && cv_usemouse.value && havefocus)
#define MOUSE_MENU false //(!disable_mouse && cv_usemouse.value && menuactive && !USE_FULLSCREEN)
#define MOUSEBUTTONS_MAX MOUSEBUTTONS

// first entry in the modelist which is not bigger than MAXVIDWIDTHxMAXVIDHEIGHT
static      INT32          firstEntry = 0;

// Total mouse motion X/Y offsets
static      INT32        mousemovex = 0, mousemovey = 0;

// SDL vars
static      SDL_Surface *icoSurface = NULL;
static      UINT32       localPalette[256];
Uint16      realwidth = BASEVIDWIDTH;
Uint16      realheight = BASEVIDHEIGHT;
#define HalfWarpMouse(x,y) if (wrapmouseok) SDL_WarpMouseInWindow(window, (Uint16)(x/2),(Uint16)(y/2))
static       SDL_bool    exposevideo = SDL_FALSE;
static       SDL_bool    usesdl2soft = SDL_FALSE;
static       SDL_bool    borderlesswindow = SDL_FALSE;

// SDL2 vars
SDL_Window   *window;
SDL_Renderer *renderer;
static SDL_Texture  *texture;
static SDL_bool      havefocus = SDL_TRUE;
static const char *fallback_resolution_name = "Fallback";

// windowed video modes from which to choose from.
static INT32 windowedModes[MAXWINMODES][2] =
{
	{2560,1440}, // 1.66
	{1920,1200}, // 1.60,6.00
	{1920,1080}, // 1.66
	{1680,1050}, // 1.60,5.25
	{1600,1200}, // 1.33
	{1600, 900}, // 1.66
	{1366, 768}, // 1.66
	{1440, 900}, // 1.60,4.50
	{1280,1024}, // 1.33?
	{1280, 960}, // 1.33,4.00
	{1280, 800}, // 1.60,4.00
	{1280, 720}, // 1.66
	{1152, 864}, // 1.33,3.60
	{1024, 768}, // 1.33,3.20
	{ 960, 600}, // 1.33,3.20
	{ 800, 600}, // 1.33,2.50
	{ 735, 415}, // 1.33,2.50
	{ 640, 480}, // 1.33,2.00
	{ 640, 400}, // 1.60,2.00
	{ 500, 300}, // 1.33
	{ 320, 240}, // 1.33,1.00
	{ 320, 200}, // 1.60,1.00
};

#define CUSTOMMODENUM 9999
static INT32 custom_width = 0;
static INT32 custom_height = 0;

static SDL_bool Impl_CreateWindow(SDL_bool fullscreen);
static void Impl_SetWindowIcon(void);

#ifdef USE_FBO_OGL
boolean downsample = false;
float InvSupersampleFactorX = 0.0;
float InvSupersampleFactorY = 0.0;
#endif

static INT32 Impl_SDL_Scancode_To_Keycode(SDL_Scancode code)
{
	if (code >= SDL_SCANCODE_A && code <= SDL_SCANCODE_Z)
	{
		// get lowercase ASCII
		return code - SDL_SCANCODE_A + 'a';
	}

	if (code >= SDL_SCANCODE_1 && code <= SDL_SCANCODE_9)
	{
		return code - SDL_SCANCODE_1 + '1';
	}
	else if (code == SDL_SCANCODE_0)
	{
		return '0';
	}

	if (code >= SDL_SCANCODE_F1 && code <= SDL_SCANCODE_F10)
	{
		return KEY_F1 + (code - SDL_SCANCODE_F1);
	}

	switch (code)
	{
		// F11 and F12 are separated from the rest of the function keys
		case SDL_SCANCODE_F11: return KEY_F11;
		case SDL_SCANCODE_F12: return KEY_F12;

		case SDL_SCANCODE_KP_0: return KEY_KEYPAD0;
		case SDL_SCANCODE_KP_1: return KEY_KEYPAD1;
		case SDL_SCANCODE_KP_2: return KEY_KEYPAD2;
		case SDL_SCANCODE_KP_3: return KEY_KEYPAD3;
		case SDL_SCANCODE_KP_4: return KEY_KEYPAD4;
		case SDL_SCANCODE_KP_5: return KEY_KEYPAD5;
		case SDL_SCANCODE_KP_6: return KEY_KEYPAD6;
		case SDL_SCANCODE_KP_7: return KEY_KEYPAD7;
		case SDL_SCANCODE_KP_8: return KEY_KEYPAD8;
		case SDL_SCANCODE_KP_9: return KEY_KEYPAD9;

		case SDL_SCANCODE_RETURN:         return KEY_ENTER;
		case SDL_SCANCODE_ESCAPE:         return KEY_ESCAPE;
		case SDL_SCANCODE_BACKSPACE:      return KEY_BACKSPACE;
		case SDL_SCANCODE_TAB:            return KEY_TAB;
		case SDL_SCANCODE_SPACE:          return KEY_SPACE;
		case SDL_SCANCODE_MINUS:          return KEY_MINUS;
		case SDL_SCANCODE_EQUALS:         return KEY_EQUALS;
		case SDL_SCANCODE_LEFTBRACKET:    return '[';
		case SDL_SCANCODE_RIGHTBRACKET:   return ']';
		case SDL_SCANCODE_BACKSLASH:      return '\\';
		case SDL_SCANCODE_NONUSHASH:      return '#';
		case SDL_SCANCODE_SEMICOLON:      return ';';
		case SDL_SCANCODE_APOSTROPHE:     return '\'';
		case SDL_SCANCODE_GRAVE:          return '`';
		case SDL_SCANCODE_COMMA:          return ',';
		case SDL_SCANCODE_PERIOD:         return '.';
		case SDL_SCANCODE_SLASH:          return '/';
		case SDL_SCANCODE_CAPSLOCK:       return KEY_CAPSLOCK;
		case SDL_SCANCODE_PRINTSCREEN:    return 0; // undefined?
		case SDL_SCANCODE_SCROLLLOCK:     return KEY_SCROLLLOCK;
		case SDL_SCANCODE_PAUSE:          return KEY_PAUSE;
		case SDL_SCANCODE_INSERT:         return KEY_INS;
		case SDL_SCANCODE_HOME:           return KEY_HOME;
		case SDL_SCANCODE_PAGEUP:         return KEY_PGUP;
		case SDL_SCANCODE_DELETE:         return KEY_DEL;
		case SDL_SCANCODE_END:            return KEY_END;
		case SDL_SCANCODE_PAGEDOWN:       return KEY_PGDN;
		case SDL_SCANCODE_RIGHT:          return KEY_RIGHTARROW;
		case SDL_SCANCODE_LEFT:           return KEY_LEFTARROW;
		case SDL_SCANCODE_DOWN:           return KEY_DOWNARROW;
		case SDL_SCANCODE_UP:             return KEY_UPARROW;
		case SDL_SCANCODE_NUMLOCKCLEAR:   return KEY_NUMLOCK;
		case SDL_SCANCODE_KP_DIVIDE:      return KEY_KPADSLASH;
		case SDL_SCANCODE_KP_MULTIPLY:    return '*'; // undefined?
		case SDL_SCANCODE_KP_MINUS:       return KEY_MINUSPAD;
		case SDL_SCANCODE_KP_PLUS:        return KEY_PLUSPAD;
		case SDL_SCANCODE_KP_ENTER:       return KEY_ENTER;
		case SDL_SCANCODE_KP_PERIOD:      return KEY_KPADDEL;
		case SDL_SCANCODE_NONUSBACKSLASH: return '\\';

		case SDL_SCANCODE_LSHIFT: return KEY_LSHIFT;
		case SDL_SCANCODE_RSHIFT: return KEY_RSHIFT;
		case SDL_SCANCODE_LCTRL:  return KEY_LCTRL;
		case SDL_SCANCODE_RCTRL:  return KEY_RCTRL;
		case SDL_SCANCODE_LALT:   return KEY_LALT;
		case SDL_SCANCODE_RALT:   return KEY_RALT;
		case SDL_SCANCODE_LGUI:   return KEY_LEFTWIN;
		case SDL_SCANCODE_RGUI:   return KEY_RIGHTWIN;
		default:                  break;
	}

	return 0;
}

static INT32 Impl_SDL_Keysym_To_Keycode(SDL_Keysym keysym)
{
	SDL_Keycode keycode = keysym.sym;
	SDL_Scancode scancode = keysym.scancode;

	if (keycode >= SDLK_a && keycode <= SDLK_z)
	{
		// get lowercase ASCII
		return keycode;
	}
	if (keycode >= SDLK_F1 && keycode <= SDLK_F10)
	{
		return KEY_F1 + (keycode - SDLK_F1);
	}

	switch (scancode)
	{
		case SDL_SCANCODE_APOSTROPHE:    return KEY_FR_U_GRAVE;
		case SDL_SCANCODE_LEFTBRACKET:   return '^';
		default:               break;
	}

	switch (keycode)
	{
		// F11 and F12 are separated from the rest of the function keys
		case SDLK_F11: return KEY_F11;
		case SDLK_F12: return KEY_F12;

		case SDLK_RETURN:         return KEY_ENTER;
		case SDLK_ESCAPE:         return KEY_ESCAPE;
		case SDLK_BACKSPACE:      return KEY_BACKSPACE;
		case SDLK_TAB:            return KEY_TAB;
		case SDLK_SPACE:          return KEY_SPACE;
		case SDLK_EQUALS:         return KEY_EQUALS;
		case SDLK_SEMICOLON:      return ';';
		case SDLK_COMMA:          return ',';
		case SDLK_COLON:          return ':';
		case SDLK_EXCLAIM:        return '!';
		case SDLK_DOLLAR:         return '$';
		case SDLK_ASTERISK:       return '*';
		case SDLK_PERCENT:        return '%';
		case SDLK_0:              return KEY_FR_A_GRAVE;
		case SDLK_1:
		case SDLK_AMPERSAND:      return '&';
		case SDLK_2:              return KEY_FR_E_AIGUE;
		case SDLK_3:
		case SDLK_QUOTEDBL:       return '"';
		case SDLK_4:		      return '\'';
		case SDLK_5:
		case SDLK_LEFTPAREN:      return '(';
		case SDLK_6:
		case SDLK_MINUS:          return KEY_MINUS;
		case SDLK_7:              return KEY_FR_E_GRAVE;
		case SDLK_8:
		case SDLK_UNDERSCORE:     return '_';
		case SDLK_9:              return KEY_FR_C_CEDILLE;
		case SDLK_RIGHTPAREN:     return ')';
		case SDLK_SLASH:          return '/';
		case SDLK_LESS:           return '<';


		case SDLK_KP_0: return KEY_KEYPAD0;
		case SDLK_KP_1: return KEY_KEYPAD1;
		case SDLK_KP_2: return KEY_KEYPAD2;
		case SDLK_KP_3: return KEY_KEYPAD3;
		case SDLK_KP_4: return KEY_KEYPAD4;
		case SDLK_KP_5: return KEY_KEYPAD5;
		case SDLK_KP_6: return KEY_KEYPAD6;
		case SDLK_KP_7: return KEY_KEYPAD7;
		case SDLK_KP_8: return KEY_KEYPAD8;
		case SDLK_KP_9: return KEY_KEYPAD9;

		default:                  break;
	}

	return Impl_SDL_Scancode_To_Keycode(scancode);
}

static boolean native_input_active = false;

// used to supress the games shift/alt handling
boolean I_UseNativeKeyboard(void)
{
	return (cv_keyboardlayout.value == 2 && native_input_active);
}

void I_SetTextInput(boolean enable)
{
	if (enable && !native_input_active)
	{
		SDL_StartTextInput();
		native_input_active = true;
	}
	else if (!enable && native_input_active)
	{
		SDL_StopTextInput();
		native_input_active = false;
	}
}

// Get the equivalent ASCII (Unicode?) character for a keypress.
static INT32 GetTypedChar(SDL_Keysym keysym)
{
	SDL_Event next_event;
	SDL_Keycode keycode = keysym.sym;
	SDL_Scancode scancode = keysym.scancode;

	// only use this this if on chat or console or the current menu wants inputs from us (except if its the control setup menu ig)
	if (native_input_active)
	{
		// Special cases, where we always return a fixed value.
		switch (keycode)
		{
			case SDLK_BACKSPACE: return KEY_BACKSPACE;
			case SDLK_RETURN:    return KEY_ENTER;
			default:
				break;
		}

		// Special case for console key
		if (scancode == SDL_SCANCODE_GRAVE)
			return '`';

		if (SDL_PeepEvents(&next_event, 1, SDL_PEEKEVENT, SDL_FIRSTEVENT, SDL_LASTEVENT) == 1 && next_event.type == SDL_TEXTINPUT)
		{
			if (next_event.text.text[1] == '\0') // limit to ASCII
			{
				return next_event.text.text[0];
			}
		}
	}

	// otherwise fallback to scancodes
	return Impl_SDL_Scancode_To_Keycode(scancode);
}

static INT32 SDLJoyAxis(const Sint16 axis, evtype_t which)
{
	// -32768 to 32767
	INT32 raxis = axis/32;
	UINT8 pid;

	switch (which)
	{
		case ev_joystick:
			pid = 0;
			break;
		case ev_joystick2:
			pid = 1;
			break;
		case ev_joystick3:
			pid = 2;
			break;
		case ev_joystick4:
			pid = 3;
			break;
		default:
			return 0;
	}

	if (Joystick[pid].bGamepadStyle)
	{
		// gamepad control type, on or off, live or die
		if (raxis < -(JOYAXISRANGE/2))
			raxis = -1;
		else if (raxis > (JOYAXISRANGE/2))
			raxis = 1;
		else
			raxis = 0;
	}
	else
	{
		raxis = JoyInfo[pid].scale!=1?((raxis/JoyInfo[pid].scale)*JoyInfo[pid].scale):raxis;

#ifdef SDL_JDEADZONE
		if (-SDL_JDEADZONE <= raxis && raxis <= SDL_JDEADZONE)
			raxis = 0;
#endif
	}
	return raxis;
}

static void Impl_HandleWindowEvent(SDL_WindowEvent evt)
{
#define FOCUSUNION (mousefocus | (kbfocus << 1) | (windowmoved << 2))

	static SDL_bool firsttimeonmouse = SDL_TRUE;
	static SDL_bool mousefocus = SDL_TRUE;
	static SDL_bool kbfocus = SDL_TRUE;
	static SDL_bool windowmoved = SDL_FALSE;

	const unsigned int oldfocus = FOCUSUNION;

	switch (evt.event)
	{
		case SDL_WINDOWEVENT_ENTER:
			mousefocus = SDL_TRUE;
			break;
		case SDL_WINDOWEVENT_LEAVE:
			mousefocus = SDL_FALSE;
			break;
		case SDL_WINDOWEVENT_FOCUS_GAINED:
			kbfocus = SDL_TRUE;
			mousefocus = SDL_TRUE;
			if (!cv_mousevisible.value)
				SDL_ShowCursor(SDL_FALSE);
			break;
		case SDL_WINDOWEVENT_FOCUS_LOST:
			kbfocus = SDL_FALSE;
			mousefocus = SDL_FALSE;
			SDL_ShowCursor(SDL_TRUE);
			break;
		case SDL_WINDOWEVENT_MAXIMIZED:
			break;
		case SDL_WINDOWEVENT_MOVED:
			windowmoved = SDL_TRUE;
            break;
	}

	if (FOCUSUNION == oldfocus) // No state change
	{
		return;
	}

	if (windowmoved && rendermode == render_opengl)
	{
		I_CheckDesktopRes();
#ifdef USE_FBO_OGL
		I_DownSample();
#endif
	}

	if (mousefocus && kbfocus)
	{
		// Tell game we got focus back, resume music if necessary
		window_notinfocus = false;

		S_SetMusicVolume(-1);

		if (cv_gamesounds.value)
			S_ResumeAudio(); // resume it

		if (!firsttimeonmouse)
		{
			if (cv_usemouse.value) I_StartupMouse();
		}
	}
	else if (!mousefocus && !kbfocus)
	{
		// Tell game we lost focus, pause music
		window_notinfocus = true;

		if (!cv_playmusicifunfocused.value)
			I_SetMusicVolume(0);
		if (!cv_playsoundifunfocused.value)
			S_StopSounds();

		memset(gamekeydown, 0, NUMKEYS); // TODO this is a scary memset
	}
#undef FOCUSUNION
}

static void Impl_HandleKeyboardEvent(SDL_KeyboardEvent evt, Uint32 type)
{
	event_t event;

	switch (type)
	{
		case SDL_KEYUP:
			event.type = ev_keyup;
			break;
		case SDL_KEYDOWN:
			event.type = ev_keydown;
			break;
		default:
			return;
	}

	switch (cv_keyboardlayout.value)
	{
		case 2: // "native"
			event.data1 = GetTypedChar(evt.keysym);
			break;
		case 3: // AZERTY
			event.data1 = Impl_SDL_Keysym_To_Keycode(evt.keysym);
			break;
		default:
			event.data1 = Impl_SDL_Scancode_To_Keycode(evt.keysym.scancode);
			break;
	}

	if (event.data1)
		D_PostEvent(&event);
}

static void Impl_HandleMouseMotionEvent(SDL_MouseMotionEvent evt)
{
	if (!USE_MOUSEINPUT)
		return;

	const boolean windowinfocus = (SDL_GetMouseFocus() == window && SDL_GetKeyboardFocus() == window);

	if (!windowinfocus)
	{
		SDL_ShowCursor(SDL_ENABLE);
		SDL_SetWindowGrab(window, SDL_FALSE);
		SDL_SetRelativeMouseMode(SDL_FALSE);
		return;
	}

	// If using relative mouse mode, don't post an event_t just now,
	// add on the offsets so we can make an overall event later.
	if (SDL_GetRelativeMouseMode())
	{
		if (windowinfocus)
		{
			mousemovex +=  evt.xrel;
			mousemovey += -evt.yrel;
			SDL_SetWindowGrab(window, SDL_TRUE);
		}
		return;
	}

	// If the event is from warping the pointer to middle
	// of the screen then ignore it.
	if ((evt.x == realwidth/2) && (evt.y == realheight/2))
	{
		return;
	}

	// Don't send an event_t if not in relative mouse mode anymore,
	// just grab and set relative mode
	// this fixes the stupid camera jerk on mouse entering bug
	// -- Monster Iestyn
	if (windowinfocus)
	{
		SDL_ShowCursor(SDL_DISABLE);
		SDL_SetWindowGrab(window, SDL_TRUE);
		SDL_SetRelativeMouseMode(SDL_TRUE);
	}
}

static void Impl_HandleMouseButtonEvent(SDL_MouseButtonEvent evt, Uint32 type)
{
	event_t event;

	// Ignore the event if the mouse is not actually focused on the window.
	// This can happen if you used the mouse to restore keyboard focus;
	// this apparently makes a mouse button down event but not a mouse button up event,
	// resulting in whatever key was pressed down getting "stuck" if we don't ignore it.
	// -- Monster Iestyn (28/05/18)
	if (SDL_GetMouseFocus() != window)
		return;

	/// \todo inputEvent.button.which
	if (USE_MOUSEINPUT)
	{
		SDL_memset(&event, 0, sizeof(event_t));

		switch (type)
		{
			case SDL_MOUSEBUTTONUP:
				event.type = ev_keyup;
				break;
			case SDL_MOUSEBUTTONDOWN:
				event.type = ev_keydown;
				break;
			default:
				return;
		}

		switch (evt.button)
		{
			case SDL_BUTTON_MIDDLE:
				event.data1 = KEY_MOUSE1+2;
				break;
			case SDL_BUTTON_RIGHT:
				event.data1 = KEY_MOUSE1+1;
				break;
			case SDL_BUTTON_LEFT:
				event.data1 = KEY_MOUSE1;
				break;
			case SDL_BUTTON_X1:
				event.data1 = KEY_MOUSE1+3;
				break;
			case SDL_BUTTON_X2:
				event.data1 = KEY_MOUSE1+4;
				break;
		}

		if (event.type == ev_keyup || event.type == ev_keydown)
		{
			D_PostEvent(&event);
		}
	}
}

static void Impl_HandleMouseWheelEvent(SDL_MouseWheelEvent evt)
{
	event_t event;

	if (USE_MOUSEINPUT)
	{
		SDL_memset(&event, 0, sizeof(event_t));

		if (evt.y > 0)
		{
			event.data1 = KEY_MOUSEWHEELUP;
			event.type = ev_keydown;
		}
		if (evt.y < 0)
		{
			event.data1 = KEY_MOUSEWHEELDOWN;
			event.type = ev_keydown;
		}
		if (evt.y == 0)
		{
			event.data1 = 0;
			event.type = ev_keyup;
		}
		if (event.type == ev_keyup || event.type == ev_keydown)
		{
			D_PostEvent(&event);
		}
	}
}

static void Impl_HandleControllerAxisEvent(SDL_ControllerAxisEvent evt)
{
	event_t event;
	SDL_JoystickID joyid[4];
	INT32 value;
	UINT8 i;

	// Determine the Joystick IDs for each current open joystick
	for (i = 0; i < MAXSPLITSCREENPLAYERS; i++)
	{
		joyid[i] = JoyInfo[i].id;
	}

	event.data1 = event.data2 = event.data3 = INT32_MAX;

	if (evt.which == joyid[0])
	{
		event.type = ev_joystick;
	}
	else if (evt.which == joyid[1])
	{
		event.type = ev_joystick2;
	}
	else if (evt.which == joyid[2])
	{
		event.type = ev_joystick3;
	}
	else if (evt.which == joyid[3])
	{
		event.type = ev_joystick4;
	}
	else
		return;

	//axis
	if (evt.axis > JOYAXISSET*2)
		return;

	//vaule
	value = SDLJoyAxis(evt.value, event.type);
	switch (evt.axis)
	{
		case SDL_CONTROLLER_AXIS_LEFTX:
			event.data1 = 0;
			event.data2 = value;
			break;
		case SDL_CONTROLLER_AXIS_LEFTY:
			event.data1 = 0;
			event.data3 = value;
			break;
		case SDL_CONTROLLER_AXIS_RIGHTX:
			event.data1 = 1;
			event.data2 = value;
			break;
		case SDL_CONTROLLER_AXIS_RIGHTY:
			event.data1 = 1;
			event.data3 = value;
			break;
		case SDL_CONTROLLER_AXIS_TRIGGERLEFT:
			event.data1 = 2;
			event.data2 = value;
			break;
		case SDL_CONTROLLER_AXIS_TRIGGERRIGHT:
			event.data1 = 2;
			event.data3 = value;
			break;
		default:
			return;
	}
	D_PostEvent(&event);
}

static void Impl_HandleControllerButtonEvent(SDL_ControllerButtonEvent evt, Uint32 type)
{
	event_t event;
	SDL_JoystickID joyid[4];
	UINT8 i;

	// Determine the Joystick IDs for each current open joystick
	for (i = 0; i < MAXSPLITSCREENPLAYERS; i++)
	{
		joyid[i] = JoyInfo[i].id;
	}

	if (   evt.button == SDL_CONTROLLER_BUTTON_DPAD_UP
		|| evt.button == SDL_CONTROLLER_BUTTON_DPAD_DOWN
		|| evt.button == SDL_CONTROLLER_BUTTON_DPAD_LEFT
		|| evt.button == SDL_CONTROLLER_BUTTON_DPAD_RIGHT)
	{
		// dpad buttons are mapped as the hat instead
		return;
	}

	if (evt.which == joyid[0])
	{
		event.data1 = KEY_JOY1;
	}
	else if (evt.which == joyid[1])
	{
		event.data1 = KEY_2JOY1;
	}
	else if (evt.which == joyid[2])
	{
		event.data1 = KEY_3JOY1;
	}
	else if (evt.which == joyid[3])
	{
		event.data1 = KEY_4JOY1;
	}
	else
		return;

	switch (type)
	{
		case SDL_CONTROLLERBUTTONUP:
			event.type = ev_keyup;
			break;
		case SDL_CONTROLLERBUTTONDOWN:
			event.type = ev_keydown;
			break;
		default:
			return;
	}

	if (evt.button < JOYBUTTONS)
	{
		event.data1 += evt.button;
	}
	else
		return;

	if (event.type != ev_console)
		D_PostEvent(&event);
}

static void Impl_HandleControllerAddedEvent(SDL_Event evt)
{
	INT32 i;

	// OH BOY are you in for a good time! #abominationstation
	SDL_GameController *newcontroller = SDL_GameControllerOpen(evt.cdevice.which);

	CONS_Debug(DBG_GAMELOGIC, "Joystick device index %d added\n", evt.jdevice.which + 1);

	////////////////////////////////////////////////////////////
	// Because SDL's device index is unstable, we're going to cheat here a bit:
	// For the first joystick setting that is NOT active:
	//
	// 1. Set cv_usejoystickX.value to the new device index (this does not change what is written to config.cfg)
	//
	// 2. Set OTHERS' cv_usejoystickX.value to THEIR new device index, because it likely changed
	//    * If device doesn't exist, switch cv_usejoystick back to default value (.string)
	//      * BUT: If that default index is being occupied, use ANOTHER cv_usejoystick's default value!
	////////////////////////////////////////////////////////////

	for (i = 0; i < MAXSPLITSCREENPLAYERS; i++)
	{
		if (newcontroller && (!JoyInfo[i].dev || !SDL_GameControllerGetAttached(JoyInfo[i].dev)))
		{
			UINT8 j;

			for (j = 0; j < MAXSPLITSCREENPLAYERS; j++)
			{
				if (i == j)
					continue;

				if (JoyInfo[j].dev == newcontroller)
					break;
			}

			if (j == MAXSPLITSCREENPLAYERS)
			{
				// ensures we aren't overriding a currently active device
				cv_usejoystick[i].value = evt.jdevice.which + 1;
				I_UpdateJoystickDeviceIndices(0);
			}
		}
	}

	////////////////////////////////////////////////////////////
	// Was cv_usejoystick disabled in settings?
	////////////////////////////////////////////////////////////

	for (i = 0; i < MAXSPLITSCREENPLAYERS; i++)
	{
		if (!strcmp(cv_usejoystick[i].string, "0") || !cv_usejoystick[i].value)
			cv_usejoystick[i].value = 0;
		else if (atoi(cv_usejoystick[i].string) <= I_NumJoys() // don't mess if we intentionally set higher than NumJoys
			&& cv_usejoystick[i].value) // update the cvar ONLY if a device exists
		CV_SetValue(&cv_usejoystick[i], cv_usejoystick[i].value);
	}

	////////////////////////////////////////////////////////////
	// Update all joysticks' init states
	// This is a little wasteful since cv_usejoystick already calls this, but
	// we need to do this in case CV_SetValue did nothing because the string was already same.
	// if the device is already active, this should do nothing, effectively.
	////////////////////////////////////////////////////////////

	for (i = 0; i < MAXSPLITSCREENPLAYERS; i++)
	{
		I_InitJoystick(i);
		G_SetPlayerGamepadIndicatorColor(i, G_GetSkinColor(i)); // gotta update the controller led again on reconnect
	}

	////////////////////////////////////////////////////////////

	for (i = 0; i < MAXSPLITSCREENPLAYERS; i++)
		CONS_Debug(DBG_GAMELOGIC, "Joystick%d device index: %d\n", i+1, JoyInfo[i].oldjoy);

	// update the menu
	if (currentMenu == &OP_JoystickSetDef)
		M_SetupJoystickMenu(0);

	numcontrollers = I_NumJoys();

	for (i = 0; i < MAXSPLITSCREENPLAYERS; i++)
	{
		if (JoyInfo[i].dev == newcontroller)
			break;
	}

	if (i == MAXSPLITSCREENPLAYERS)
		SDL_GameControllerClose(newcontroller);
}

static void Impl_HandleControllerRemovedEvent(void)
{
	INT32 i;

	for (i = 0; i < MAXSPLITSCREENPLAYERS; i++)
	{
		if (JoyInfo[i].dev && !SDL_GameControllerGetAttached(JoyInfo[i].dev))
		{
			CONS_Debug(DBG_GAMELOGIC, "Joystick%d removed, device index: %d\n", i+1, JoyInfo[i].oldjoy);
			I_ShutdownJoystick(i);
		}
	}

	////////////////////////////////////////////////////////////
	// Update the device indexes, because they likely changed
	// * If device doesn't exist, switch cv_usejoystick back to default value (.string)
	//   * BUT: If that default index is being occupied, use ANOTHER cv_usejoystick's default value!
	////////////////////////////////////////////////////////////

	for (i = 0; i < MAXSPLITSCREENPLAYERS; i++)
	{
		I_UpdateJoystickDeviceIndex(i);
	}

	////////////////////////////////////////////////////////////
	// Was cv_usejoystick disabled in settings?
	////////////////////////////////////////////////////////////

	for (i = 0; i < MAXSPLITSCREENPLAYERS; i++)
	{
		if (!strcmp(cv_usejoystick[i].string, "0"))
		{
			cv_usejoystick[i].value = 0;
		}
		else if (atoi(cv_usejoystick[i].string) <= I_NumJoys() // don't mess if we intentionally set higher than NumJoys
			&& cv_usejoystick[i].value) // update the cvar ONLY if a device exists
		{
			CV_SetValue(&cv_usejoystick[i], cv_usejoystick[i].value);
		}
	}

	////////////////////////////////////////////////////////////

	for (i = 0; i < MAXSPLITSCREENPLAYERS; i++)
		CONS_Debug(DBG_GAMELOGIC, "Joystick%d device index: %d\n", i+1, JoyInfo[i].oldjoy);

	// update the menu
	if (currentMenu == &OP_JoystickSetDef)
		M_SetupJoystickMenu(0);

	numcontrollers = I_NumJoys();
}

void I_GetEvent(void)
{
	SDL_Event evt;
	char* dropped_filedir;

	if (!graphics_started)
	{
		return;
	}

	mousemovex = mousemovey = 0;

	while (SDL_PollEvent(&evt))
	{
		switch (evt.type)
		{
			case SDL_WINDOWEVENT:
				Impl_HandleWindowEvent(evt.window);
				break;
			case SDL_KEYUP:
			case SDL_KEYDOWN:
				Impl_HandleKeyboardEvent(evt.key, evt.type);
				break;
			case SDL_MOUSEMOTION:
				Impl_HandleMouseMotionEvent(evt.motion);
				break;
			case SDL_MOUSEBUTTONUP:
			case SDL_MOUSEBUTTONDOWN:
				Impl_HandleMouseButtonEvent(evt.button, evt.type);
				break;
			case SDL_MOUSEWHEEL:
				Impl_HandleMouseWheelEvent(evt.wheel);
				break;
			case SDL_CONTROLLERAXISMOTION:
				Impl_HandleControllerAxisEvent(evt.caxis);
				break;
			case SDL_CONTROLLERBUTTONUP:
			case SDL_CONTROLLERBUTTONDOWN:
				Impl_HandleControllerButtonEvent(evt.cbutton, evt.type);
				break;
			case SDL_CONTROLLERDEVICEADDED:
				Impl_HandleControllerAddedEvent(evt);
				break;
			case SDL_CONTROLLERDEVICEREMOVED:
				Impl_HandleControllerRemovedEvent();
				break;
			case SDL_DROPFILE:
				dropped_filedir = evt.drop.file;
				P_AddWadFile(dropped_filedir, false);
				SDL_free(dropped_filedir);    // Free dropped_filedir memory
				break;
			case SDL_QUIT:
				I_Quit();
				M_QuitResponse('y');
				break;
		}
	}

	// Send all relative mouse movement as one single mouse event.
	if (mousemovex || mousemovey)
	{
		event_t event;
		int wwidth, wheight;
		SDL_GetWindowSize(window, &wwidth, &wheight);
		event.type = ev_mouse;
		event.data1 = 0;
		event.data2 = (INT32)lround(mousemovex * ((float)wwidth / (float)realwidth));
		event.data3 = (INT32)lround(mousemovey * ((float)wheight / (float)realheight));
		D_PostEvent(&event);
	}

	// In order to make wheels act like buttons, we have to set their state to Up.
	// This is because wheel messages don't have an up/down state.
	gamekeydown[KEY_MOUSEWHEELDOWN] = gamekeydown[KEY_MOUSEWHEELUP] = 0;
}

void I_StartupMouse(void)
{
	if (disable_mouse)
		return;

	if (!cv_mousevisible.value)
		SDL_ShowCursor(SDL_FALSE);
	else
		SDL_ShowCursor(SDL_TRUE);

	SDL_SetWindowGrab(window, SDL_FALSE);
	SDL_SetRelativeMouseMode(SDL_FALSE);
}

//
// I_OsPolling
//
void I_OsPolling(void)
{
	SDL_Keymod mod;
	INT32 i;

	if (consolevent)
		I_GetConsoleEvents();

	if (SDL_WasInit(SDL_INIT_JOYSTICK|SDL_INIT_GAMECONTROLLER) == (SDL_INIT_JOYSTICK|SDL_INIT_GAMECONTROLLER))
	{
		SDL_GameControllerUpdate();

		for (i = 0; i < MAXSPLITSCREENPLAYERS; i++)
			I_GetJoystickEvents(i);
	}

	I_GetEvent();

	mod = SDL_GetModState();
	/* Handle here so that our state is always synched with the system. */
	shiftdown = ctrldown = altdown = 0;
	capslock = false;
	if (mod & KMOD_LSHIFT) shiftdown |= 1;
	if (mod & KMOD_RSHIFT) shiftdown |= 2;
	if (mod & KMOD_LCTRL)   ctrldown |= 1;
	if (mod & KMOD_RCTRL)   ctrldown |= 2;
	if (mod & KMOD_LALT)     altdown |= 1;
	if (mod & KMOD_RALT)     altdown |= 2;
	if (mod & KMOD_CAPS) capslock = true;
}

static void VID_Command_NumModes_f (void)
{
	CONS_Printf(M_GetText("%d video mode(s) available(s)\n"), VID_NumModes());
}

static void VID_Command_ModeList_f(void)
{
	// List windowed modes
	INT32 i = 0;

	CONS_Printf("NOTE: Under SDL2, all modes are supported on all platforms.\n"
	"Under opengl, fullscreen only supports native desktop resolution.\n"
	"Under software, the mode is stretched up to desktop resolution.\n");

	for (i = 0; i < MAXWINMODES; i++)
	{
		CONS_Printf("%2d: %dx%d\n", i, windowedModes[i][0], windowedModes[i][1]);
	}
}

static void VID_Command_Mode_f (void)
{
	INT32 modenum;

	if (COM_Argc()!= 2)
	{
		CONS_Printf("vid_mode <modenum> : set video mode, current video mode %i\n", vid.modenum);
		return;
	}

	modenum = atoi(COM_Argv(1));

	if (modenum >= VID_NumModes())
		CONS_Printf("Video mode not present\n");
	else
		setmodeneeded = modenum+1; // request vid mode change
}

// Get the desktop resolution from the current display the gamewindow resides on
static void I_CheckDesktopRes(void)
{
	int currentDisplayIndex = -1;
	SDL_DisplayMode curmode;

	desktopwidth = 0;
	desktopheight = 0;

	currentDisplayIndex = SDL_GetWindowDisplayIndex(window);

	// No valid index
	if (currentDisplayIndex < 0)
	{
		return;
	}

	if (SDL_GetDesktopDisplayMode(currentDisplayIndex, &curmode) != 0)
	{
		return;
	}

	desktopwidth = curmode.w;
	desktopheight = curmode.h;
}

// Check if the game resolution matches the desktop resolution
boolean I_CheckNativeRes(void)
{
	return (vid.width == desktopwidth && vid.height == desktopheight);
}

#ifdef USE_FBO_OGL

void RefreshOGLSDLSurface(void)
{
	if (rendermode == render_opengl)
		OglSdlSurface(vid.width, vid.height);
}

void I_DownSample(void)
{
	boolean needrefresh = false;

	if (!cv_glframebuffer.value || !supportFBO || (cv_glscreentextures.value == 0)) // no sense to do this crap if we cant benefit from it
	{
		downsample = false;
		return;
	}

	if (I_CheckNativeRes() && (downsample == true))
	{
		downsample = false;
		I_ResetFBOSurface();
		return;
	}

	if ((vid.width > desktopwidth) || (vid.height > desktopheight)) //check if current resolution is higher than current display resolution
	{
		downsample = true;
		needrefresh = true;
	}
	else if (downsample == true)
	{
		downsample = false;
		needrefresh = true;
	}

	if (needrefresh)
	{
		I_ResetFBOSurface();
		needrefresh = false;
	}
}

static void I_ResetFBOSurface(void)
{
	InvSupersampleFactorX = (float)(desktopwidth) / vid.width;
	InvSupersampleFactorY = (float)(desktopheight) / vid.height;
	RefreshOGLSDLSurface();
}
#endif

static void SDLSetMode(INT32 width, INT32 height, SDL_bool fullscreen)
{
	static SDL_bool wasfullscreen = SDL_FALSE;
	int sw_texture_format = SDL_PIXELFORMAT_ABGR8888;

	realwidth = vid.width;
	realheight = vid.height;

	if (window)
	{
		if (fullscreen)
		{
			wasfullscreen = SDL_TRUE;
			SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
			I_SetBorderlessWindow();
		}
		else // windowed mode
		{
			if (wasfullscreen)
			{
				wasfullscreen = SDL_FALSE;
				SDL_SetWindowFullscreen(window, 0);
				I_SetBorderlessWindow();
			}

			// Reposition window only in windowed mode
			SDL_SetWindowSize(window, width, height);
			SDL_SetWindowPosition(window,
				SDL_WINDOWPOS_CENTERED_DISPLAY(SDL_GetWindowDisplayIndex(window)),
				SDL_WINDOWPOS_CENTERED_DISPLAY(SDL_GetWindowDisplayIndex(window))
			);
		}
	}
	else
	{
		Impl_CreateWindow(fullscreen);
		wasfullscreen = fullscreen;
		SDL_SetWindowSize(window, width, height);

		if (fullscreen)
		{
			SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
			I_SetBorderlessWindow();
		}
	}

#ifdef HWRENDER
	if (rendermode == render_opengl)
	{
		I_CheckDesktopRes();
#ifdef USE_FBO_OGL
		I_DownSample();
#endif
		OglSdlSurface(vid.width, vid.height);
	}
#endif

	if (rendermode == render_soft)
	{
		SDL_RenderClear(renderer);
		SDL_RenderSetLogicalSize(renderer, width, height);

		// Set up Texture
		realwidth = width;
		realheight = height;

		if (texture != NULL)
		{
			SDL_DestroyTexture(texture);
		}

		texture = SDL_CreateTexture(renderer, sw_texture_format, SDL_TEXTUREACCESS_STREAMING, width, height);
	}
}

//
// I_UpdateNoBlit
//
void I_UpdateNoBlit(void)
{
	if (rendermode == render_none)
		return;

	if (exposevideo)
	{
#ifdef HWRENDER
		if (rendermode == render_opengl)
		{
			OglSdlFinishUpdate(cv_vidwait.value);
		}
		else
#endif
		if (rendermode == render_soft)
		{
			SDL_RenderCopy(renderer, texture, NULL, NULL);
			SDL_RenderPresent(renderer);
		}
	}

	exposevideo = SDL_FALSE;
}

//
// I_FinishUpdate
//
static SDL_Rect src_rect = { 0, 0, 0, 0 };

void I_FinishUpdate(void)
{
	if (rendermode == render_none)
		return; //Alam: No software or OpenGl surface

	SCR_CalculateFPS();

	if (st_overlay)
	{
		if (cv_ticrate.value)
			SCR_DisplayTicRate();

		const boolean isserverplayer = consoleplayer == serverplayer;

		if (cv_showping.value && ((netgame && !isserverplayer) || (simulated_lag != 0 && isserverplayer && Playing())))
			SCR_DisplayLocalPing();
	}

#ifdef HAVE_DISCORDRPC
	if (discordRequestList != NULL)
		ST_AskToJoinEnvelope();
#endif

	if (rendermode == render_soft && vid.screens[0])
	{
		void *pixels;
		int pitch;
		SDL_LockTexture(texture, NULL, &pixels, &pitch);
		int step = pitch / 4 - vid.width;
		UINT32 *restrict dst = pixels;
		UINT8 *restrict src = vid.screens[0];
		UINT32 *restrict palette = localPalette;
		for (INT32 y = 0; y < vid.height; y++)
		{
			UINT8 *restrict end = src + vid.width;
			do *dst++ = palette[*src++];
			while (src < end);
			dst += step;
		}
		SDL_UnlockTexture(texture);

		SDL_RenderClear(renderer);
		SDL_RenderCopy(renderer, texture, &src_rect, NULL);
		SDL_RenderPresent(renderer);
	}
#ifdef HWRENDER
	else if (rendermode == render_opengl)
	{
		OglSdlFinishUpdate(cv_vidwait.value);
	}
#endif

	exposevideo = SDL_FALSE;
}

//
// I_UpdateNoVsync
//
void I_UpdateNoVsync(void)
{
	INT32 real_vidwait = cv_vidwait.value;
	cv_vidwait.value = 0;
	I_FinishUpdate();
	cv_vidwait.value = real_vidwait;
}

//
// I_ReadScreen
//
void I_ReadScreen(UINT8 * restrict scr, INT32 scale)
{
	if (rendermode != render_soft)
		I_Error("I_ReadScreen: called while in non-software mode");
	else if (scale == 1)
		VID_BlitLinearScreen(vid.screens[0], scr, vid.width, vid.height, vid.width, vid.width);
	else
	{
		UINT8 * restrict source = vid.screens[0];
		uintptr_t w = vid.width/scale*scale, h = vid.height/scale*scale;

		// size_t saves a lea + movsxd over INT32. mind your types!
		// uintptr_t is even better since it's guaranteed to be the size of a pointer
		for (uintptr_t y = 0; y < h; y += scale)
			for (uintptr_t x = 0; x < w; x += scale)
				*scr++ = source[y*vid.width + x];
	}
}

//
// I_SetPalette
//
void I_SetPalette(RGBA_t *palette)
{
	size_t i;

	for (i = 0; i < 256; i++)
	{
		localPalette[i] = 0xff000000;
		localPalette[i] |= palette[i].s.red << 0;
		localPalette[i] |= palette[i].s.green << 8;
		localPalette[i] |= palette[i].s.blue << 16;
	}
}

// return number of fullscreen + X11 modes
INT32 VID_NumModes(void)
{
	if (USE_FULLSCREEN && numVidModes != -1)
		return numVidModes - firstEntry;
	else
		return MAXWINMODES;
}

const char *VID_GetModeName(INT32 modeNum)
{
	if (modeNum == -1)
	{
		return fallback_resolution_name;
	}

	if (modeNum > MAXWINMODES)
		return NULL;

	sprintf(&vidModeName[modeNum][0], "%dx%d",
		windowedModes[modeNum][0],
		windowedModes[modeNum][1]);

	return &vidModeName[modeNum][0];
}

INT32 VID_GetModeForSize(INT32 w, INT32 h)
{
	int i;

	for (i = 0; i < MAXWINMODES; i++)
	{
		if (windowedModes[i][0] == w && windowedModes[i][1] == h)
		{
			return i;
		}
	}

	// did not find mode from list, make custom resolution if the values somewhat make sense
	// opengl mode does not mind about max resolution defined in screen.h
	// if not using opengl, check against the maximum as well
	if ((w >= BASEVIDWIDTH && h >= BASEVIDHEIGHT) &&
		(rendermode == render_opengl || (w <= MAXVIDWIDTH && h <= MAXVIDHEIGHT)))
	{
		custom_width = w;
		custom_height = h;
		return CUSTOMMODENUM;
	}

	return -1;
}

void VID_PrepareModeList(void)
{
	// Under SDL2, we just use the windowed modes list, and scale in windowed fullscreen.
	allow_fullscreen = true;
}

static UINT32 refresh_rate;
static UINT32 VID_GetRefreshRate(void)
{
	int index = SDL_GetWindowDisplayIndex(window);
	SDL_DisplayMode m;

	if (SDL_WasInit(SDL_INIT_VIDEO) == 0)
	{
		// Video not init yet.
		return 0;
	}

	if (SDL_GetDesktopDisplayMode(index, &m) != 0)
	{
		// Error has occurred.
		return 0;
	}

	return m.refresh_rate;
}

INT32 VID_SetMode(INT32 modeNum)
{
	vid.recalc = true;

	if (modeNum >= 0 && modeNum < MAXWINMODES)
	{
		vid.width = windowedModes[modeNum][0];
		vid.height = windowedModes[modeNum][1];
		vid.modenum = modeNum;
	}
	else if (modeNum == CUSTOMMODENUM && custom_width && custom_height)
	{
		// at this point these values are assumed to be okay
		vid.width = custom_width;
		vid.height = custom_height;
		vid.modenum = modeNum;
	}
	else
	{
		// just set the desktop resolution as a fallback
		SDL_DisplayMode mode;
		SDL_GetWindowDisplayMode(window, &mode);

		if (mode.w >= 2048)
		{
			vid.width = 1920;
			vid.height = 1200;
		}
		else
		{
			vid.width = mode.w;
			vid.height = mode.h;
		}

		vid.modenum = -1;
	}

	SDLSetMode(vid.width, vid.height, USE_FULLSCREEN);

	src_rect.w = vid.width;
	src_rect.h = vid.height;

	refresh_rate = VID_GetRefreshRate();

	return SDL_TRUE;
}


static SDL_bool Impl_CreateContext(void)
{
	// Renderer-specific stuff
#ifdef HWRENDER
	if ((rendermode == render_opengl)
	&& (vid.glstate != VID_GL_LIBRARY_ERROR))
	{
		if (!sdlglcontext)
			sdlglcontext = SDL_GL_CreateContext(window);

		if (sdlglcontext == NULL)
		{
			SDL_DestroyWindow(window);
			I_Error("Failed to create a GL context: %s\n", SDL_GetError());
		}

		SDL_GL_MakeCurrent(window, sdlglcontext);
	}
	else
#endif
	if (rendermode == render_soft)
	{
		int flags = 0; // Use this to set SDL_RENDERER_* flags now

		if (usesdl2soft)
		{
			flags |= SDL_RENDERER_SOFTWARE;
		}
		else if (cv_vidwait.value)
		{
#if SDL_VERSION_ATLEAST(2, 0, 18)
			// If SDL is new enough, we can turn off vsync later.
			flags |= SDL_RENDERER_PRESENTVSYNC;
#else
			// However, if it isn't, we should just silently turn vid_wait off
			// This is because the renderer will be created before the config
			// is read and vid_wait is set from the user's preferences, and thus
			// vid_wait will have no effect.
			CV_StealthSetValue(&cv_vidwait, 0);
#endif
		}

#ifdef _WIN32
		SDL_SetHint(SDL_HINT_RENDER_DRIVER, "direct3d11");
#else
		SDL_SetHint(SDL_HINT_RENDER_DRIVER, "opengles2");
#endif

		if (!renderer)
			renderer = SDL_CreateRenderer(window, -1, flags);
		if (renderer == NULL)
		{
			CONS_Printf(M_GetText("Couldn't create rendering context: %s\n"), SDL_GetError());
			return SDL_FALSE;
		}
		SDL_RenderSetLogicalSize(renderer, BASEVIDWIDTH, BASEVIDHEIGHT);
	}

	return SDL_TRUE;
}

static SDL_bool Impl_CreateWindow(SDL_bool fullscreen)
{
	int flags = 0;

	if (rendermode == render_none) // dedicated
		return SDL_TRUE; // Monster Iestyn -- not sure if it really matters what we return here tbh

	if (window != NULL)
		return SDL_FALSE;

	if (fullscreen)
		flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;

	if (borderlesswindow)
		flags |= SDL_WINDOW_BORDERLESS;

#ifdef HWRENDER
	if (vid.glstate == VID_GL_LIBRARY_LOADED)
		flags |= SDL_WINDOW_OPENGL;

	if (msaa)
	{
		SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
		SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, msaa);
	}

	// Without a 24-bit depth buffer many visuals are ruined by z-fighting.
	// Some GPU drivers may give us a 16-bit depth buffer since the
	// default value for SDL_GL_DEPTH_SIZE is 16.
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

	// request stencil buffer. If there are problems on weaker hw the bit count could be reduced
	// If only something like 1-bit or 2-bit stencil is available on some gpus then should implement limit for maxportals based on that.
	// 4 bits would be enough for current limit in maxportals (12)
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 4);
#endif

	// Create a window
	window = SDL_CreateWindow("SRB2Kart "VERSIONSTRING, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, realwidth, realheight, flags);

	if (window == NULL)
	{
		CONS_Printf(M_GetText("Couldn't create window: %s\n"), SDL_GetError());
		return SDL_FALSE;
	}

	return Impl_CreateContext();
}

static void Impl_SetWindowIcon(void)
{
	if (window == NULL || icoSurface == NULL)
	{
		return;
	}

	SDL_SetWindowIcon(window, icoSurface);
}

static FILE * OpenRendererFile(const char * mode)
{
	char * path = va(pandf,srb2home,"renderer.txt");
	return fopen(path, mode);
}

void I_StartupGraphics(void)
{
	if (dedicated)
	{
		rendermode = render_none;
		return;
	}

	if (graphics_started)
		return;

	COM_AddCommand("vid_nummodes", VID_Command_NumModes_f);
	COM_AddCommand("vid_modelist", VID_Command_ModeList_f);
	COM_AddCommand("vid_mode", VID_Command_Mode_f);
	CV_RegisterVar(&cv_vidwait);
	CV_RegisterVar(&cv_stretch);
	disable_mouse = M_CheckParm("-nomouse");
	disable_fullscreen = M_CheckParm("-win") ? 1 : 0;

	keyboard_started = true;

#if !defined(HAVE_TTF)
	// Previously audio was init here for questionable reasons?
	if (SDL_InitSubSystem(SDL_INIT_VIDEO) < 0)
	{
		CONS_Printf(M_GetText("Couldn't initialize SDL's Video System: %s\n"), SDL_GetError());
		return;
	}
#endif
	{
		const char *vd = SDL_GetCurrentVideoDriver();
		//CONS_Printf(M_GetText("Starting up with video driver: %s\n"), vd);
		if (vd && (strncasecmp(vd, "fbcon", 6) == 0))
			framebuffer = SDL_TRUE;
	}

	if (M_CheckParm("-software"))
		rendermode = render_soft;
#ifdef HWRENDER
	else if (M_CheckParm("-opengl"))
		rendermode = render_opengl;

	msaa = 0; boolean msaa_set = false;
	a2c = false; boolean a2c_set = false;

	if (M_CheckParm("-msaa") && M_IsNextParm())
	{
		const char* str = M_GetNextParm();
		if (sscanf(str, "%u", &msaa))
		{
			msaa_set = true;
		}
	}

	if (M_CheckParm("-a2c"))
	{
		a2c = true;
		a2c_set = true;
	}
	else if (M_CheckParm("-noa2c"))
	{
		a2c_set = true;
	}

    {
		char   line[16];
		char * word;
		FILE * file = OpenRendererFile("r");

		if (file != NULL)
		{
			while (fgets(line, sizeof line, file) != NULL)
			{
				word = strtok(line, " \n");

				if (rendermode == render_none)
				{
					if (strcasecmp(word, "software") == 0)
					{
						rendermode = render_soft;
					}
					else if (strcasecmp(word, "opengl") == 0)
					{
						rendermode = render_opengl;
					}

					if (rendermode != render_none)
					{
						CONS_Printf("Using last known renderer: %s\n", line);
					}
			    }

				if (!msaa_set)
				{
					if (strcasecmp(word, "msaa") == 0)
					{
						const char *nextword = strtok(NULL, " \n");

						if (!nextword || !sscanf(nextword, "%u", &msaa))
						{
							CONS_Alert(CONS_ERROR, "Malformed MSAA entry in renderer.txt\n");
						}
						else
						{
							CONS_Printf("Using last know MSAA value: %u\n", msaa);
						}
					}
				}

				if (!a2c_set)
				{
					if (strcasecmp(word, "a2c") == 0)
					{
						a2c = true;
						CONS_Printf("Using a2c because it was specified to be used earlier\n");
					}
				}
			}

			fclose(file);
		}
    }
#endif

	if (rendermode == render_none)
	{
#ifdef HWRENDER
		rendermode = render_opengl;
		CONS_Printf("Defaulting to OpenGL renderer.\n");
#else
		rendermode = render_soft;
		CONS_Printf("Using default software renderer.\n");
#endif
	}

	{
		FILE * file = OpenRendererFile("w");

		if (file != NULL)
		{
			if (rendermode == render_soft)
			{
				fputs("software\n", file);
			}
			else if (rendermode == render_opengl)
			{
				fputs("opengl\n", file);
			}

#ifdef HWRENDER
			fprintf(file, "msaa %u\n", msaa);

			if (a2c)
				fputs("a2c\n", file);
#endif
			fclose(file);
		}
		else
		{
			CONS_Printf("Could not save renderer to file: %s\n", strerror(errno));
		}
	}

	usesdl2soft = M_CheckParm("-softblit");
	borderlesswindow = M_CheckParm("-borderless");

	VID_Command_ModeList_f();

#ifdef HWRENDER
	if (rendermode == render_opengl)
	{
		vid.glstate = (VID_LoadOGLAPI() && GL_Init()) ? VID_GL_LIBRARY_LOADED : VID_GL_LIBRARY_ERROR;

		if (vid.glstate == VID_GL_LIBRARY_ERROR)
		{
			CONS_Alert(CONS_ERROR, "Could not initialize OpenGL\n" "Falling back to Software mode.\n");
			rendermode = render_soft;
		}
	}
#endif

	// Fury: we do window initialization after GL setup to allow
	// SDL_GL_LoadLibrary to work well on Windows

	// Create window
	VID_SetMode(VID_GetModeForSize(BASEVIDWIDTH, BASEVIDHEIGHT));

	vid.width = BASEVIDWIDTH; // Default size for startup
	vid.height = BASEVIDHEIGHT; // BitsPerPixel is the SDL interface's
	vid.recalc = true; // Set up the console stufff

#ifdef HAVE_TTF
	I_ShutdownTTF();
#endif
	// Window icon
#ifdef HAVE_IMAGE
	icoSurface = IMG_ReadXPMFromArray(SDL_icon_xpm);
#endif
	Impl_SetWindowIcon();

	VID_SetMode(VID_GetModeForSize(BASEVIDWIDTH, BASEVIDHEIGHT));

	realwidth = (Uint16)vid.width;
	realheight = (Uint16)vid.height;

	SDL_RaiseWindow(window);

	graphics_started = true;

	SDL_StopTextInput();
}

void I_ShutdownGraphics(void)
{
	rendermode = render_none;

	if (icoSurface)
		SDL_FreeSurface(icoSurface);
	icoSurface = NULL;

	I_OutputMsg("I_ShutdownGraphics(): ");

	// was graphics initialized anyway?
	if (!graphics_started)
	{
		I_OutputMsg("graphics never started\n");
		return;
	}

	graphics_started = false;
	I_OutputMsg("shut down\n");

#ifdef HWRENDER
	if (sdlglcontext)
	{
		SDL_GL_DeleteContext(sdlglcontext);
	}
#endif

	SDL_QuitSubSystem(SDL_INIT_VIDEO);
	framebuffer = SDL_FALSE;
}

UINT32 I_GetRefreshRate(void)
{
	// Moved to VID_GetRefreshRate.
	// Precalculating it like that won't work as
	// well for windowed mode since you can drag
	// the window around, but very slow PCs might have
	// trouble querying mode over and over again.
	return refresh_rate;
}

static void Impl_SetVsync(void)
{
#if SDL_VERSION_ATLEAST(2,0,18)
	if (renderer)
		SDL_RenderSetVSync(renderer, cv_vidwait.value);
#endif
#ifdef HWRENDER
	if (!renderer && rendermode == render_opengl &&
	sdlglcontext != NULL && SDL_GL_GetCurrentContext() == sdlglcontext)
	{
		SDL_GL_SetSwapInterval(cv_vidwait.value ? 1 : 0);
	}
#endif
}

void I_SetBorderlessWindow(void)
{
	SDL_bool borderless = (cv_fullscreen.value == 2) ? SDL_FALSE : SDL_TRUE;
	SDL_SetWindowBordered(window, borderless);
}

#endif
