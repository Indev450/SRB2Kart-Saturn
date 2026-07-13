// Emacs style mode select   -*- C++ -*-
//
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
//
// Changes by Graue <graue@oceanbase.org> are in the public domain.
//
//-----------------------------------------------------------------------------
/// \file
/// \brief SRB2 system stuff for dedicated

#ifdef CMAKECONFIG
#include "config.h"
#else
#include "../config.h.in"
#endif

#include <signal.h>

// A little more than the minimum sleep duration on Windows.
// May be incorrect for other platforms, but we don't currently have a way to
// query the scheduler granularity. SDL will do what's needed to make this as
// low as possible though.
#define MIN_SLEEP_DURATION_MS 2.1

#ifdef _WIN32
#define RPC_NO_WINDOWS_H
#include <windows.h>
#include "../doomtype.h"
typedef BOOL (WINAPI *p_GetDiskFreeSpaceExA)(LPCSTR, PULARGE_INTEGER, PULARGE_INTEGER, PULARGE_INTEGER);
typedef BOOL (WINAPI *p_IsProcessorFeaturePresent) (DWORD);
typedef DWORD (WINAPI *p_timeGetTime) (void);
typedef UINT (WINAPI *p_timeEndPeriod) (UINT);
typedef HANDLE (WINAPI *p_OpenFileMappingA) (DWORD, BOOL, LPCSTR);
typedef LPVOID (WINAPI *p_MapViewOfFile) (HANDLE, DWORD, DWORD, DWORD, SIZE_T);

// This is for RtlGenRandom.
#define SystemFunction036 NTAPI SystemFunction036
#include <ntsecapi.h>
#undef SystemFunction036

#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __GNUC__
#include <unistd.h>
#endif
#if defined (__unix__) || defined (UNIXCOMMON)
#include <fcntl.h>
#endif

#include <stdio.h>
#ifdef _WIN32
#include <conio.h>
#endif

#if defined (__unix__) || defined(__APPLE__) || (defined (UNIXCOMMON) && !defined (__HAIKU__))
#include <time.h>
#if defined (__linux__)
#include <sys/vfs.h>
#elif defined(__APPLE__)
#include <sys/param.h>
#include <sys/mount.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <mach/mach.h>
#else
#include <sys/param.h>
#include <sys/mount.h>
/*For meminfo*/
#include <sys/types.h>
#ifdef FREEBSD
#include <kvm.h>
#endif
#include <nlist.h>
#include <sys/sysctl.h>
#endif
#endif

#if defined (__linux__) || (defined (UNIXCOMMON) && !defined (__HAIKU__))
#ifndef NOTERMIOS
#include <termios.h>
#include <sys/ioctl.h> // ioctl
#define HAVE_TERMIOS
#endif
#endif

#if defined (__unix__) || (defined (UNIXCOMMON) && !defined (__APPLE__))
#include <poll.h>
#include <errno.h>
#include <sys/wait.h>
#define NEWSIGNALHANDLER
#endif

#ifndef NOMUMBLE
#ifdef __linux__ // need -lrt
#include <sys/mman.h>
#ifdef MAP_FAILED
#define HAVE_SHM
#endif
#include <wchar.h>
#endif

#ifdef _WIN32
#define HAVE_MUMBLE
#define WINMUMBLE
#elif defined (HAVE_SHM)
#define HAVE_MUMBLE
#endif
#endif // NOMUMBLE

#ifndef O_BINARY
#define O_BINARY 0
#endif

#ifdef __APPLE__
#include "macosx/mac_resources.h"
#endif

#ifndef errno
#include <errno.h>
#endif

#include <time.h>

/// Locations to directly check for srb2.srb in
const char *wadDefaultPaths[] = {
#if defined (__unix__) || defined(__APPLE__) || defined (UNIXCOMMON)
	"/usr/local/share/games/SRB2Kart",
	"/usr/local/games/SRB2Kart",
	"/usr/share/games/SRB2Kart",
	"/usr/games/SRB2Kart",
#elif defined (_WIN32)
	"c:\\games\\srb2kart",
	"\\games\\srb2kart",
#endif
	NULL
};

// Folders to recurse through looking for srb2.srb
const char *wadSearchPaths[] = {
#if defined (__unix__) || defined(__APPLE__) || defined (UNIXCOMMON)
	"/usr/local/games",
	"/usr/games",
	"/usr/local",
#elif defined (_WIN32)
	"c:\\games",
	"\\games",
#endif
	NULL
};

/**	\brief WAD file to look for
*/
#define WADKEYWORD1 "srb2.srb"
#define WADKEYWORD2 "srb2.wad"
/**	\brief holds wad path
*/
static char returnWadPath[256];

#include "../doomdef.h"
#include "../m_misc.h"
#include "../i_video.h"
#include "../i_sound.h"
#include "../i_system.h"
#include "../i_time.h"
#include "../i_net.h"
#include "../screen.h" //vid.WndParent
#include "../d_net.h"
#include "../g_game.h"
#include "../filesrch.h"
#include "../z_zone.h" // Z_Free
#include "endtxt.h"

#include "../i_joy.h"

#include "../m_argv.h"

#include "../d_main.h"

#if !defined(NOMUMBLE) && defined(HAVE_MUMBLE)
// Mumble context string
#include "../d_clisrv.h"
#include "../byteptr.h"
#endif

boolean consolevent = false;
boolean framebuffer = false;
UINT8 keyboard_started = false;

#ifdef HAVE_TERMIOS
// TERMIOS console code from Quake3: thank you!
typedef struct
{
	size_t cursor;
	char buffer[256];
} feild_t;

feild_t tty_con;

// lock to prevent clearing partial lines, since not everything
// printed ends on a newline.
static boolean ttycon_ateol = true;
// some key codes that the terminal may be using
// TTimo NOTE: I'm not sure how relevant this is
static INT32 tty_erase;
static INT32 tty_eof;

static struct termios tty_tc;

// =============================================================
// tty console routines
// NOTE: if the user is editing a line when something gets printed to the early console then it won't look good
//   so we provide tty_Clear and tty_Show to be called before and after a stdout or stderr output
// =============================================================

// do a backspace
// TTimo NOTE: it seems on some terminals just sending '\b' is not enough
//   so for now, in any case we send "\b \b" .. yeah well ..
//   (there may be a way to find out if '\b' alone would work though)
// Hanicef NOTE: using \b this way is unreliable because of terminal state,
//   it's better to use \r to reset the cursor to the beginning of the
//   line and clear from there.
static void tty_Back(void)
{
	write(STDOUT_FILENO, "\r", 1);
	if (tty_con.cursor>0)
	{
		write(STDOUT_FILENO, tty_con.buffer, tty_con.cursor);
	}
	write(STDOUT_FILENO, " \b", 2);
}

static void tty_Clear(void)
{
	size_t i;
	write(STDOUT_FILENO, "\r", 1);
	if (tty_con.cursor>0)
	{
		for (i=0; i<tty_con.cursor; i++)
		{
			write(STDOUT_FILENO, " ", 1);
		}
		write(STDOUT_FILENO, "\r", 1);
	}
}

// never exit without calling this, or your terminal will be left in a pretty bad state
static void I_ShutdownConsole(void)
{
	if (consolevent)
	{
		I_OutputMsg("Shutdown tty console\n");
		consolevent = false;
		tcsetattr (STDIN_FILENO, TCSADRAIN, &tty_tc);
	}
}

static void I_StartupConsole(void)
{
	struct termios tc;

	// TTimo
	// https://zerowing.idsoftware.com/bugzilla/show_bug.cgi?id=390 (404)
	// then SIGTTIN or SIGTOU is emitted, if not catched, turns into a SIGSTP
	signal(SIGTTIN, SIG_IGN);
	signal(SIGTTOU, SIG_IGN);

	consolevent = !M_CheckParm("-noconsole");
	framebuffer = M_CheckParm("-framebuffer");

	if (framebuffer)
		consolevent = false;

	if (!consolevent) return;

	if (isatty(STDIN_FILENO)!=1)
	{
		I_OutputMsg("stdin is not a tty, tty console mode failed\n");
		consolevent = false;
		return;
	}
	memset(&tty_con, 0x00, sizeof(tty_con));
	tcgetattr (0, &tty_tc);
	tty_erase = tty_tc.c_cc[VERASE];
	tty_eof = tty_tc.c_cc[VEOF];
	tc = tty_tc;
	/*
	 ECHO: don't echo input characters
	 ICANON: enable canonical mode.  This  enables  the  special
	  characters  EOF,  EOL,  EOL2, ERASE, KILL, REPRINT,
	  STATUS, and WERASE, and buffers by lines.
	 ISIG: when any of the characters  INTR,  QUIT,  SUSP,  or
	  DSUSP are received, generate the corresponding signal
	*/
	tc.c_lflag &= ~(ECHO | ICANON);
	/*
	 ISTRIP strip off bit 8
	 INPCK enable input parity checking
	 */
	tc.c_iflag &= ~(ISTRIP | INPCK);
	tc.c_cc[VMIN] = 0; //1?
	tc.c_cc[VTIME] = 0;
	tcsetattr (0, TCSADRAIN, &tc);
}

void I_GetConsoleEvents(void)
{
	// we use this when sending back commands
	event_t ev = {0,0,0,0};
	char key = 0;
	struct pollfd pfd =
	{
		.fd = STDIN_FILENO,
		.events = POLLIN,
		.revents = 0,
	};

	if (!consolevent)
		return;

	for (;;)
	{
		if (poll(&pfd, 1, 0) < 1 || !(pfd.revents & POLLIN))
			return;

		ev.type = ev_console;
		ev.data1 = 0;
		if (read(STDIN_FILENO, &key, 1) == -1 || !key)
			return;

		// we have something
		// backspace?
		// NOTE TTimo testing a lot of values .. seems it's the only way to get it to work everywhere
		if ((key == tty_erase) || (key == 127) || (key == 8))
		{
			if (tty_con.cursor > 0)
			{
				tty_con.cursor--;
				tty_con.buffer[tty_con.cursor] = '\0';
				tty_Back();
			}
			ev.data1 = KEY_BACKSPACE;
		}
		else if (key < ' ') // check if this is a control char
		{
			if (key == '\n')
			{
				tty_Clear();
				tty_con.cursor = 0;
				ev.data1 = KEY_ENTER;
			}
			else if (key == 0x4) // ^D, aka EOF
			{
				// shut down, most unix programs behave this way
				I_Quit();
			}
			else continue;
		}
		else if (tty_con.cursor < sizeof(tty_con.buffer))
		{
			// push regular character
			ev.data1 = tty_con.buffer[tty_con.cursor] = key;
			tty_con.cursor++;
			// print the current line (this is differential)
			write(STDOUT_FILENO, &key, 1);
		}
		if (ev.data1) D_PostEvent(&ev);
	}
}

#elif defined (_WIN32)
static BOOL I_ReadyConsole(HANDLE ci)
{
	DWORD gotinput;
	if (ci == INVALID_HANDLE_VALUE) return FALSE;
	if (WaitForSingleObject(ci,0) != WAIT_OBJECT_0) return FALSE;
	if (GetFileType(ci) != FILE_TYPE_CHAR) return FALSE;
	if (!GetConsoleMode(ci, &gotinput)) return FALSE;
	return (GetNumberOfConsoleInputEvents(ci, &gotinput) && gotinput);
}

static boolean entering_con_command = false;

static void Impl_HandleKeyboardConsoleEvent(KEY_EVENT_RECORD evt, HANDLE co)
{
	event_t event;
	CONSOLE_SCREEN_BUFFER_INFO CSBI;
	DWORD t;

	memset(&event,0x00,sizeof (event));

	if (evt.bKeyDown)
	{
		event.type = ev_console;
		entering_con_command = true;
		switch (evt.wVirtualKeyCode)
		{
			case VK_ESCAPE:
			case VK_TAB:
				event.data1 = KEY_NULL;
				break;
			case VK_RETURN:
				entering_con_command = false;
				// Fall through.
			default:
				//event.data1 = MapVirtualKey(evt.wVirtualKeyCode,2); // convert in to char
				event.data1 = evt.uChar.AsciiChar;
		}
		if (co != INVALID_HANDLE_VALUE && GetFileType(co) == FILE_TYPE_CHAR && GetConsoleMode(co, &t))
		{
			if (event.data1 && event.data1 != KEY_LSHIFT && event.data1 != KEY_RSHIFT)
			{
#ifdef _UNICODE
				WriteConsole(co, &evt.uChar.UnicodeChar, 1, &t, NULL);
#else
				WriteConsole(co, &evt.uChar.AsciiChar, 1 , &t, NULL);
#endif
			}
			if (evt.wVirtualKeyCode == VK_BACK
				&& GetConsoleScreenBufferInfo(co,&CSBI))
			{
				WriteConsoleOutputCharacterA(co, " ",1, CSBI.dwCursorPosition, &t);
			}
		}
	}
	if (event.data1) D_PostEvent(&event);
}

void I_GetConsoleEvents(void)
{
	HANDLE ci = GetStdHandle(STD_INPUT_HANDLE);
	HANDLE co = GetStdHandle(STD_OUTPUT_HANDLE);
	INPUT_RECORD input;
	DWORD t;

	while (I_ReadyConsole(ci) && ReadConsoleInput(ci, &input, 1, &t) && t)
	{
		switch (input.EventType)
		{
			case KEY_EVENT:
				Impl_HandleKeyboardConsoleEvent(input.Event.KeyEvent, co);
				break;
			case MOUSE_EVENT:
			case WINDOW_BUFFER_SIZE_EVENT:
			case MENU_EVENT:
			case FOCUS_EVENT:
				break;
		}
	}
}

static void I_StartupConsole(void)
{
	HANDLE ci, co;
	const INT32 ded = M_CheckParm("-dedicated");
	BOOL gotConsole = FALSE;
	if (M_CheckParm("-console") || ded)
		gotConsole = AllocConsole();
#ifdef _DEBUG
	else if (M_CheckParm("-noconsole") && !ded)
#else
	else if (!M_CheckParm("-console") && !ded)
#endif
	{
		FreeConsole();
		gotConsole = FALSE;
	}

	if (gotConsole)
	{
		SetConsoleTitleA("SRB2Kart Console");
		consolevent = true;
	}

	//Let get the real console HANDLE, because Mingw's Bash is bad!
	ci = CreateFile(TEXT("CONIN$") ,               GENERIC_READ, FILE_SHARE_READ,  NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	co = CreateFile(TEXT("CONOUT$"), GENERIC_WRITE|GENERIC_READ, FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (ci != INVALID_HANDLE_VALUE)
	{
		const DWORD CM = ENABLE_LINE_INPUT|ENABLE_ECHO_INPUT|ENABLE_PROCESSED_INPUT;
		SetStdHandle(STD_INPUT_HANDLE, ci);
		if (GetFileType(ci) == FILE_TYPE_CHAR)
			SetConsoleMode(ci, CM); //default mode but no ENABLE_MOUSE_INPUT
	}
	if (co != INVALID_HANDLE_VALUE)
	{
		SetStdHandle(STD_OUTPUT_HANDLE, co);
		SetStdHandle(STD_ERROR_HANDLE, co);
	}
}
static inline void I_ShutdownConsole(void){}
#else
void I_GetConsoleEvents(void){}
static inline void I_StartupConsole(void)
{
#ifdef _DEBUG
	consolevent = !M_CheckParm("-noconsole");
#else
	consolevent = M_CheckParm("-console");
#endif

	framebuffer = M_CheckParm("-framebuffer");

	if (framebuffer)
		consolevent = false;
}
static inline void I_ShutdownConsole(void){}
#endif


//
//I_OutputMsg
//
void I_OutputMsg(const char *fmt, ...)
{
	size_t len;
	char *txt;
	va_list  argptr;

	va_start(argptr,fmt);
	len = vsnprintf(NULL, 0, fmt, argptr);
	va_end(argptr);
	if (len == 0)
		return;

	txt = malloc(len+1);
	va_start(argptr,fmt);
	vsprintf(txt, fmt, argptr);
	va_end(argptr);

#ifdef HAVE_TTF
	if (TTF_WasInit()) I_TTFDrawText(currentfont, solid, DEFAULTFONTFGR, DEFAULTFONTFGG, DEFAULTFONTFGB,  DEFAULTFONTFGA,
	DEFAULTFONTBGR, DEFAULTFONTBGG, DEFAULTFONTBGB, DEFAULTFONTBGA, txt);
#endif

	len = strlen(txt);

#ifdef LOGMESSAGES
	if (logstream)
	{
		size_t d = fwrite(txt, len, 1, logstream);
		fflush(logstream);
		(void)d;
	}
#endif

#if defined (_WIN32)
#ifdef DEBUGFILE
	if (debugfile != stderr)
#endif
	{
		HANDLE co = GetStdHandle(STD_OUTPUT_HANDLE);
		DWORD bytesWritten;

		if (co == INVALID_HANDLE_VALUE)
		{
			free(txt);
			return;
		}

		if (GetFileType(co) == FILE_TYPE_CHAR && GetConsoleMode(co, &bytesWritten))
		{
			static COORD coordNextWrite = {0,0};
			LPVOID oldLines = NULL;
			INT oldLength;
			CONSOLE_SCREEN_BUFFER_INFO csbi;

			// Save the lines that we're going to obliterate.
			GetConsoleScreenBufferInfo(co, &csbi);
			oldLength = csbi.dwSize.X * (csbi.dwCursorPosition.Y - coordNextWrite.Y) + csbi.dwCursorPosition.X - coordNextWrite.X;

			if (oldLength > 0)
			{
				LPVOID blank = malloc(oldLength);
				if (!blank)
				{
					free(txt);
					return;
				}
				memset(blank, ' ', oldLength); // Blank out.
				oldLines = malloc(oldLength*sizeof(TCHAR));
				if (!oldLines)
				{
					free(txt);
					free(blank);
					return;
				}

				ReadConsoleOutputCharacter(co, oldLines, oldLength, coordNextWrite, &bytesWritten);

				// Move to where we what to print - which is where we would've been,
				// had console input not been in the way,
				SetConsoleCursorPosition(co, coordNextWrite);

				WriteConsoleA(co, blank, oldLength, &bytesWritten, NULL);
				free(blank);

				// And back to where we want to print again.
				SetConsoleCursorPosition(co, coordNextWrite);
			}

			// Actually write the string now!
			WriteConsoleA(co, txt, (DWORD)len, &bytesWritten, NULL);

			// Next time, output where we left off.
			GetConsoleScreenBufferInfo(co, &csbi);
			coordNextWrite = csbi.dwCursorPosition;

			// Restore what was overwritten.
			if (oldLines && entering_con_command)
				WriteConsole(co, oldLines, oldLength, &bytesWritten, NULL);
			if (oldLines) free(oldLines);
		}
		else // Redirected to a file.
			WriteFile(co, txt, (DWORD)len, &bytesWritten, NULL);
	}
#else
#ifdef HAVE_TERMIOS
	if (consolevent && ttycon_ateol)
	{
		tty_Clear();
		ttycon_ateol = false;
	}
#endif

	if (!framebuffer)
		fprintf(stderr, "%s", txt);
#ifdef HAVE_TERMIOS
	if (consolevent && txt[len-1] == '\n')
	{
		write(STDOUT_FILENO, tty_con.buffer, tty_con.cursor);
		ttycon_ateol = true;
	}
#endif

	// 2004-03-03 AJR Since not all messages end in newline, some were getting displayed late.
	if (!framebuffer)
		fflush(stderr);

#endif
	free(txt);
}

//
// I_GetKey
//
INT32 I_GetKey (void)
{
	// Warning: I_GetKey empties the event queue till next keypress
	event_t *ev;
	INT32 rc = 0;

	// return the first keypress from the event queue
	for (; eventtail != eventhead; eventtail = (eventtail+1)&(MAXEVENTS-1))
	{
		ev = &events[eventtail];
		if (ev->type == ev_keydown || ev->type == ev_console)
		{
			rc = ev->data1;
			continue;
		}
	}

	return rc;
}

//
// I_JoyScale
//
void I_JoyScale(void)
{
}

void I_JoyScale2(void)
{
}

void I_JoyScale3(void)
{
}

void I_JoyScale4(void)
{
}

/**	\brief	Shuts down joystick 1


	\return void


*/
void I_ShutdownJoystick(UINT8 index)
{
	(void)index;
}

//
// I_InitJoystick
//
void I_InitJoystick(UINT8 index)
{
	(void)index;
}

void I_InitJoystick1(void)
{
}

void I_InitJoystick2(void)
{
}

void I_InitJoystick3(void)
{
}

void I_InitJoystick4(void)
{
}

INT32 I_NumJoys(void)
{
	return 0;
}

const char *I_GetJoyName(INT32 joyindex)
{
	(void)joyindex;
	return NULL;
}

void I_SetJoystickFocus(void)
{
}

boolean I_GamepadHasLED(INT32 playernum)
{
	(void)playernum;
	return false;
}

boolean I_GamepadHasRumble(INT32 playernum)
{
	(void)playernum;
	return false;
}

void I_GamepadRumble(INT32 playernum, UINT16 low_strength, UINT16 high_strength, UINT32 duration)
{
	(void)playernum;
	(void)low_strength;
	(void)high_strength;
	(void)duration;
}

void I_SetGamepadIndicatorColor(INT32 playernum, UINT8 red, UINT8 green, UINT8 blue)
{
	(void)playernum;
	(void)red;
	(void)green;
	(void)blue;
}

#ifndef NOMUMBLE
#ifdef HAVE_MUMBLE
// Best Mumble positional audio settings:
// Minimum distance 3.0 m
// Bloom 175%
// Maximum distance 80.0 m
// Minimum volume 50%
#define DEG2RAD (0.017453292519943295769236907684883l) // TAU/360 or PI/180
#define MUMBLEUNIT (64.0f) // FRACUNITS in a Meter

static struct {
#ifdef WINMUMBLE
	UINT32 uiVersion;
	DWORD uiTick;
#else
	UINT32 uiVersion;
	UINT32 uiTick;
#endif
	float fAvatarPosition[3];
	float fAvatarFront[3];
	float fAvatarTop[3]; // defaults to Y-is-up (only used for leaning)
	wchar_t name[256]; // game name
	float fCameraPosition[3];
	float fCameraFront[3];
	float fCameraTop[3]; // defaults to Y-is-up (only used for leaning)
	wchar_t identity[256]; // player id
#ifdef WINMUMBLE
	UINT32 context_len;
#else
	UINT32 context_len;
#endif
	unsigned char context[256]; // server/team
	wchar_t description[2048]; // game description
} *mumble = NULL;
#endif // HAVE_MUMBLE

static void I_SetupMumble(void)
{
#ifdef WINMUMBLE
	HANDLE hMap = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, L"MumbleLink");
	if (!hMap)
		return;

	mumble = MapViewOfFile(hMap, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(*mumble));
	if (!mumble)
		CloseHandle(hMap);
#elif defined (HAVE_SHM)
	int shmfd;
	char memname[256];

	snprintf(memname, 256, "/MumbleLink.%d", getuid());
	shmfd = shm_open(memname, O_RDWR, S_IRUSR | S_IWUSR);

	if(shmfd < 0)
		return;

	mumble = mmap(NULL, sizeof(*mumble), PROT_READ | PROT_WRITE, MAP_SHARED, shmfd, 0);
	if (mumble == MAP_FAILED)
		mumble = NULL;
#endif
}

void I_UpdateMumble(const mobj_t *mobj, const listener_t listener)
{
#ifdef HAVE_MUMBLE
	float angle;
	fixed_t anglef;

	if (!mumble)
		return;

	if (mumble->uiVersion != 2)
	{
		wcsncpy(mumble->name, L"SRB2Kart " VERSIONSTRINGW, 256);
		wcsncpy(mumble->description, L"Sonic Robo Blast 2 Kart with integrated Mumble Link support.", 2048);
		mumble->uiVersion = 2;
	}
	mumble->uiTick++;

	// Zero out, but never delink.
	if (!netgame || gamestate != GS_LEVEL)
	{
		mumble->fAvatarPosition[0] = mumble->fAvatarPosition[1] = mumble->fAvatarPosition[2] = 0.0f;
		mumble->fAvatarFront[0] = 1.0f;
		mumble->fAvatarFront[1] = mumble->fAvatarFront[2] = 0.0f;
		mumble->fCameraPosition[0] = mumble->fCameraPosition[1] = mumble->fCameraPosition[2] = 0.0f;
		mumble->fCameraFront[0] = 1.0f;
		mumble->fCameraFront[1] = mumble->fCameraFront[2] = 0.0f;
		return;
	}

	{
		UINT8 *p = mumble->context;
		WRITEMEM(p, server_context, 8);
		WRITEINT16(p, gamemap);
		mumble->context_len = (UINT32)(p - mumble->context);
	}

	if (mobj)
	{
		mumble->fAvatarPosition[0] = FixedToFloat(mobj->x) / MUMBLEUNIT;
		mumble->fAvatarPosition[1] = FixedToFloat(mobj->z) / MUMBLEUNIT;
		mumble->fAvatarPosition[2] = FixedToFloat(mobj->y) / MUMBLEUNIT;

		anglef = AngleFixed(mobj->angle);
		angle = (float)(FixedToFloat(anglef) * DEG2RAD);
		mumble->fAvatarFront[0] = cosf(angle);
		mumble->fAvatarFront[1] = 0.0f;
		mumble->fAvatarFront[2] = sinf(angle);
	}
	else
	{
		mumble->fAvatarPosition[0] = mumble->fAvatarPosition[1] = mumble->fAvatarPosition[2] = 0.0f;
		mumble->fAvatarFront[0] = 1.0f;
		mumble->fAvatarFront[1] = mumble->fAvatarFront[2] = 0.0f;
	}

	mumble->fCameraPosition[0] = FixedToFloat(listener.x) / MUMBLEUNIT;
	mumble->fCameraPosition[1] = FixedToFloat(listener.z) / MUMBLEUNIT;
	mumble->fCameraPosition[2] = FixedToFloat(listener.y) / MUMBLEUNIT;

	anglef = AngleFixed(listener.angle);
	angle = (float)(FixedToFloat(anglef) * DEG2RAD);
	mumble->fCameraFront[0] = cosf(angle);
	mumble->fCameraFront[1] = 0.0f;
	mumble->fCameraFront[2] = sinf(angle);
#else
	(void)mobj;
	(void)listener;
#endif // HAVE_MUMBLE
}
#undef WINMUMBLE
#endif // NOMUMBLE

//
// I_GetTime
// returns time in 1/TICRATE second tics
//

precise_t I_GetPreciseTime(void)
{
#if defined (__unix__) || defined(__APPLE__) || defined (UNIXCOMMON)
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (precise_t)ts.tv_sec * 1000000000 + ts.tv_nsec;
#elif defined (_WIN32)
	LARGE_INTEGER counter;
	QueryPerformanceCounter(&counter);
	return (precise_t)counter.QuadPart;
#else
	return 0;
#endif
}

UINT64 I_GetPrecisePrecision(void)
{
#if defined (__unix__) || defined(__APPLE__) || defined (UNIXCOMMON)
	return 1000000000;
#elif defined (_WIN32)
	LARGE_INTEGER frequency;
	QueryPerformanceFrequency(&frequency);
	return (UINT64)frequency.QuadPart;
#else
	return 1000000;
#endif
}

void I_StartupTimer(void){}

void I_Sleep(UINT32 ms)
{
#if defined (__unix__) || defined(__APPLE__) || defined (UNIXCOMMON)
	struct timespec ts = {
		.tv_sec = ms / 1000,
		.tv_nsec = ms % 1000 * 1000000,
	};
	int status;
	do status = clock_nanosleep(CLOCK_MONOTONIC, 0, &ts, &ts);
	while (status == EINTR);
#elif defined (_WIN32)
	Sleep(ms);
#else
	(void)ms;
#warning No sleep function for this system!
#endif
}

void I_SleepDuration(precise_t duration)
{
#if defined(__linux__) || defined(__FreeBSD__) || defined(__HAIKU__)
	UINT64 precision = I_GetPrecisePrecision();
	struct timespec ts = {
		.tv_sec = duration / precision,
		.tv_nsec = duration * 1000000000 / precision % 1000000000,
	};
	int status;
	do status = clock_nanosleep(CLOCK_MONOTONIC, 0, &ts, &ts);
	while (status == EINTR);
#elif defined (MIN_SLEEP_DURATION_MS)
	UINT64 precision = I_GetPrecisePrecision();
	INT32 sleepvalue = cv_sleep.value;
	UINT64 delaygranularity;
	precise_t cur;
	precise_t dest;

	{
		double gran = round(((double)(precision / 1000) * sleepvalue * MIN_SLEEP_DURATION_MS));
		delaygranularity = (UINT64)gran;
	}

	cur = I_GetPreciseTime();
	dest = cur + duration;

	// the reason this is not dest > cur is because the precise counter may wrap
	// two's complement arithmetic is our friend here, though!
	// e.g. cur 0xFFFFFFFFFFFFFFFE = -2, dest 0x0000000000000001 = 1
	// 0x0000000000000001 - 0xFFFFFFFFFFFFFFFE = 3
	while ((INT64)(dest - cur) > 0)
	{
		// If our cv_sleep value exceeds the remaining sleep duration, use the
		// hard sleep function.
		if (sleepvalue > 0 && (dest - cur) > delaygranularity)
		{
			I_Sleep(sleepvalue);
		}

		// Otherwise, this is a spinloop.

		cur = I_GetPreciseTime();
	}
#endif
}

static volatile sig_atomic_t g_in_exiting_signal_handler = false;

static void I_PrintSignal(INT32 signal_num, boolean core_dumped, char *signal_msg)
{
	const char *sigmsg;

	switch (signal_num)
	{
#ifdef SIGINT
		case SIGINT:
			sigmsg = ("SIGINT - SRB2Kart-Saturn was interrupted prematurely by the user.");
			break;
#endif
		case SIGILL: // illegal instruction - invalid function image
			sigmsg = ("SIGILL - SRB2Kart-Saturn has attempted to execute an illegal instruction and needs to close.");
			break;
		case SIGFPE: // mathematical exception
			sigmsg = ("SIGFPE - SRB2Kart-Saturn has encountered a mathematical exception and needs to close.");
			break;
		case SIGSEGV: // segment violation
			sigmsg = ("SIGSEGV - SRB2Kart-Saturn has attempted to access a memory location that it shouldn't and needs to close.");
			break;
#ifdef SIGTERM
		case SIGTERM: // Software termination signal from terminate
			sigmsg = ("SIGTERM - SRB2Kart-Saturn was terminated by a terminate signal.");
			break;
#endif
#ifdef SIGKILL
		case SIGKILL: // Software termination signal from kill
			sigmsg = ("SIGKILL - SRB2Kart-Saturn was terminated by a kill signal.");
			break;
#endif
#ifdef SIGBREAK
		case SIGBREAK: // Ctrl-Break sequence
			sigmsg =("SIGBREAK - SRB2Kart-Saturn was terminated by a Ctrl-Break sequence.")
			break;
#endif
		case SIGABRT: // abnormal termination triggered by abort call
			sigmsg = ("SIGABRT - SRB2Kart-Saturn was terminated by an abort signal.");
			break;
		default:
			sprintf(signal_msg, "Signal number %d", signal_num);
			sigmsg = (core_dumped ? "Unknown signal" : signal_msg);
			break;
	}

	if (core_dumped)
	{
		if (sigmsg)
			sprintf(signal_msg, "%s (core dumped)", sigmsg);
		else
			strcat(signal_msg, " (core dumped)");
	}
	else
	{
		sprintf(signal_msg, "%s", sigmsg);
	}
}

static void I_ReportSignal(int num, int coredumped)
{
	char sigmsg[512];

	I_PrintSignal(num, coredumped, sigmsg);
	size_t len = strlen(sigmsg);
	snprintf(sigmsg + len, sizeof(sigmsg) - len, "\n\nCrash report has been saved into %s", CRASH_LOGFILE_NAME);
	I_OutputMsg("\nProcess killed by signal: %s\n\n", sigmsg);
}

boolean I_In_Exiting_Signal_Handler(void)
{
	return g_in_exiting_signal_handler;
}

#ifndef NEWSIGNALHANDLER
FUNCNORETURN static ATTRNORETURN void signal_handler(INT32 num)
{
	g_in_exiting_signal_handler = true;

	D_QuitNetGame(); // Fix server freezes

#ifdef HAVE_LIBBACKTRACE
	write_backtrace(BT_CRASH_REASON_SIGNAL(num));
#endif

	I_ReportSignal(num, 0);
	I_ShutdownSystem();
	signal(num, SIG_DFL);               //default signal action
	raise(num);
	I_Quit();
}
#endif

static volatile sig_atomic_t interrupted = 0;

boolean I_Interrupted(void)
{
	return interrupted;
}

static void quit_handler(int num)
{
	(void)num;
	interrupted = 1;
}

#ifdef HAVE_LIBBACKTRACE
#include <backtrace.h>
// TODO - move this to some header file instead
extern struct backtrace_state *bt_state;

typedef struct bt_crash_reason_s {
	enum {
		BTCRASH_SIGNAL,
		BTCRASH_ERRORMSG,
	} type;

	union {
		INT32 signal;
		const char *errormsg;
	} value;
} bt_crash_reason_t;

#define BT_CRASH_REASON_SIGNAL(num) (bt_crash_reason_t){ .type = BTCRASH_SIGNAL, .value = { .signal = num } }
#define BT_CRASH_REASON_ERRORMSG(msg) (bt_crash_reason_t){ .type = BTCRASH_ERRORMSG, .value = { .errormsg = msg } }

typedef struct bt_out_buf_s {
	boolean error;
	char *pos;
	size_t size;
} bt_out_buf_t;

static void bt_syminfo_cb(void *data, uintptr_t pc, const char *symname, uintptr_t symval, uintptr_t symsize)
{
	(void)symval;
	(void)symsize;

	bt_out_buf_t *buf = (bt_out_buf_t*)data;

	if (!symname)
		symname = "???";

	int n = snprintf(buf->pos, buf->size, "%p %s\n", (void*)pc, symname);

	if (n <= 0)
	{
		buf->size = 0;
		return;
	}

	buf->pos += n;
	buf->size -= n;
}

static int bt_simple_cb(void *data, uintptr_t pc)
{
	bt_out_buf_t *buf = (bt_out_buf_t*)data;

	backtrace_syminfo(bt_state, pc, bt_syminfo_cb, NULL, data);

	if (!buf->size) return 1;

	return 0;
}

static int bt_full_cb(void *data, uintptr_t pc, const char *filename, int lineno, const char *function)
{
	bt_out_buf_t *buf = (bt_out_buf_t*)data;

	if (!filename) filename = "???";
	if (!function) function = "???";

	int n = snprintf(buf->pos, buf->size, "%p %s\n\t%s:%d\n", (void*)pc, function, filename, lineno);

	if (n <= 0) return 1;

	buf->pos += n;
	buf->size -= n;

	if (!buf->size) return 1;

	return 0;
}

static void bt_error_cb(void *data, const char *msg, int errnum)
{
	(void)msg; // We don't need this

	bt_out_buf_t *buf = (bt_out_buf_t*)data;

	// No debug info
	if (errnum == -1)
		buf->error = true;
}

static void write_backtrace(bt_crash_reason_t reason)
{
	char sig_msg[512];
	const char *filename = va("%s" PATHSEP CRASH_LOGFILE_NAME, srb2home);
	FILE *out = fopen(filename, "a+");

	time_t rawtime;
	struct tm *timeinfo;

	const size_t BUFSIZE = 8192;
	char backtrace[BUFSIZE];

	bt_out_buf_t buf;
	buf.error = false;
	buf.pos = backtrace;
	buf.size = BUFSIZE;

	if (!out)
	{
		fprintf(stderr, "\nWARNING: Couldn't open crash log for writing! Make sure your permissions are correct. Please save the below report!\n");
		out = stderr;
	}

	// Get the current time as a string.
	time(&rawtime);
	timeinfo = localtime(&rawtime);

	fprintf(out, "------------------------\n\n");

	fprintf(out, "Program name: %s %s\n", SRB2APPLICATION, VERSIONSTRING);

	fprintf(out, "Dedicated build\n");
	fprintf(out, "Platform: %s\n", I_GetSysName());

	if (compdate && comptime && comprevision && compbranch)
		fprintf(out, "Compiled: %s %s, commit %s, branch %s\n", compdate, comptime, comprevision, compbranch);

	if (gamestate == GS_LEVEL && gamemap)
	{
		char *title = G_BuildMapTitle(gamemap);

		if (title)
		{
			fprintf(out, "Game map: %s (%s)\n", title, G_BuildMapName(gamemap));
			Z_Free(title);
		}
		else
			fprintf(out, "Game map: %s\n", G_BuildMapName(gamemap));
	}

	fprintf(out, "Time of crash: %s\n", asctime(timeinfo));

	fprintf(out, "Caused by: ");

	switch (reason.type)
	{
		case BTCRASH_SIGNAL:
			I_PrintSignal(reason.value.signal, false, sig_msg);
			fprintf(out, "%s", sig_msg);
		break;

		case BTCRASH_ERRORMSG:
			fprintf(out, "%s", reason.value.errormsg);
		break;
	}

	fprintf(out, "\nBacktrace:\n");

	// Try to get full backtrace, it will print files and line numbers
	backtrace_full(bt_state, 2, bt_full_cb, bt_error_cb, (void*)&buf);

	if (buf.error)
	{
		// Fall back to simple backtrace, only prints function names
		backtrace_simple(bt_state, 2, bt_simple_cb, NULL, (void*)&buf);
	}

	fputs(backtrace, out);

	if (out != stderr)
	{
		fclose(out);
		fprintf(stderr, "Crash report created, find %s in your SRB2Kart directory\n", CRASH_LOGFILE_NAME);
	}
}
#endif

static void I_RegisterSignals(void)
{
#ifdef SIGINT
	signal(SIGINT,   quit_handler);
#endif
#ifdef SIGBREAK
	signal(SIGBREAK, quit_handler);
#endif
#ifdef SIGTERM
	signal(SIGTERM,  quit_handler);
#endif
#ifndef NEWSIGNALHANDLER
	// If these defines don't exist,
	// then compilation would have failed above us...
	signal(SIGILL,  signal_handler);
	signal(SIGSEGV, signal_handler);
	signal(SIGABRT, signal_handler);
	signal(SIGFPE,  signal_handler);
#endif
}

#ifdef NEWSIGNALHANDLER
static void signal_handler_child(INT32 num)
{
	g_in_exiting_signal_handler = true;

#ifdef HAVE_LIBBACKTRACE
	write_backtrace(BT_CRASH_REASON_SIGNAL(num));
#endif

	signal(num, SIG_DFL); // default signal action
	raise(num);
}

static void I_RegisterChildSignals(void)
{
	// If these defines don't exist,
	// then compilation would have failed above us...
	signal(SIGILL,  signal_handler_child);
	signal(SIGSEGV, signal_handler_child);
	signal(SIGABRT, signal_handler_child);
	signal(SIGFPE,  signal_handler_child);
}
#endif

#ifdef NEWSIGNALHANDLER
FUNCNORETURN static ATTRNORETURN void newsignalhandler_Warn(const char *pr)
{
	char text[128];

	snprintf(text, sizeof text,
			"Error while setting up signal reporting: %s: %s",
			pr,
			strerror(errno)
	);

	I_OutputMsg("%s\n", text);

	I_ShutdownConsole();
	exit(-1);
}

static void I_Fork(void)
{
	int child;
	int status;
	int signum;
	int c;

	child = fork();

	switch (child)
	{
		case -1:
			newsignalhandler_Warn("fork()");
			break;
		case 0:
			I_RegisterChildSignals();
			break;
		default:
			// ignore those, those are handled by child process
			// otherwise parent might exit before it
			// and the below stuff wont run and your terminal will be left in an awkward state
#ifdef SIGINT
			signal(SIGINT,   SIG_IGN);
#endif
#ifdef SIGBREAK
			signal(SIGBREAK, SIG_IGN);
#endif
#ifdef SIGTERM
			signal(SIGTERM,  SIG_IGN);
#endif
			if (logstream)
				fclose(logstream);/* the child has this */

			c = wait(&status);
#ifdef LOGMESSAGES
			/* By the way, exit closes files. */
			logstream = fopen(logfilename, "at");
#else
			logstream = 0;
#endif
			if (c == -1)
			{
				kill(child, SIGKILL);
				newsignalhandler_Warn("wait()");
			}
			else
			{
				if (WIFSIGNALED (status))
				{
					signum = WTERMSIG (status);
#ifdef WCOREDUMP
					I_ReportSignal(signum, WCOREDUMP (status));
#else
					I_ReportSignal(signum, 0);
#endif
					status = 128 + signum;
				}
				else if (WIFEXITED (status))
				{
					status = WEXITSTATUS (status);
				}

				I_ShutdownConsole();
				exit(status);
			}
	}
}
#endif/*NEWSIGNALHANDLER*/

int I_OpenURL(const char *url)
{
	return -1;
}

INT32 I_StartupSystem(void)
{
	I_StartupConsole();
#ifdef NEWSIGNALHANDLER
	// This is useful when debugging. It lets GDB attach to
	// the correct process easily.
	if (!M_CheckParm("-nofork"))
		I_Fork();
#endif
#ifdef HAVE_THREADS
	I_StartThreads();
	I_AddExitFunc(I_StopThreads);
#endif
	I_RegisterSignals();
#ifndef NOMUMBLE
	I_SetupMumble();
#endif
	return 0;
}

//
// I_Quit
//
void I_Quit(void)
{
	static boolean quiting = false;

	/* prevent recursive I_Quit() */
	if (quiting)
		goto death;

	quiting = false;
	M_SaveConfig(NULL); //save game config, cvars..
	D_SaveBan(); // save the ban list
	G_SaveGameData(false); // Tails 12-08-2002
	//added:16-02-98: when recording a demo, should exit using 'q' key,
	//        but sometimes we forget and use 'F10'.. so save here too.

	if (demo.recording)
		G_CheckDemoStatus();

	D_QuitNetGame();
	I_ShutdownMusic();
	I_ShutdownSound();
	// use this for 1.28 19990220 by Kin
	I_ShutdownGraphics();
	I_ShutdownSystem();

	/* if option -noendtxt is set, don't print the text */
	if (!M_CheckParm("-noendtxt") && W_CheckNumForName("ENDOOM") != LUMPERROR)
	{
		printf("\r");
		ShowEndTxt();
	}

	if (myargmalloc)
		free(myargv); // Deallocate allocated memory

death:
	exit(0);
}

//
// I_Error
//
/**	\brief phuck recursive errors
*/
static INT32 errorcount = 0;

/**	\brief recursive error detecting
*/
static boolean shutdowning = false;

FUNCIERROR void ATTRNORETURN I_Error(const char *error, ...)
{
	va_list argptr;
	char buffer[8192];

	// recursive error detecting
	if (shutdowning)
	{
		errorcount++;
		// try to shutdown each subsystem separately
		if (errorcount == 1)
			I_ShutdownMusic();
		if (errorcount == 2)
			I_ShutdownSound();
		if (errorcount == 3)
			I_ShutdownGraphics();
		if (errorcount == 4)
			I_ShutdownSystem();
		if (errorcount == 5)
		{
			M_SaveConfig(NULL);
			G_SaveGameData(false);
		}
		if (errorcount > 20)
		{
			va_start(argptr, error);
			vsnprintf(buffer, 8192, error, argptr);
			va_end(argptr);

			I_OutputMsg("SRB2Kart %s Recursive Error", buffer);

			exit(-1); // recursive errors detected
		}
	}

	shutdowning = true;

	// Display error message in the console before we start shutting it down
	va_start(argptr, error);
	vsnprintf(buffer, 8192, error, argptr);
	va_end(argptr);
	I_OutputMsg("\nI_Error(): %s\n", buffer);

#ifdef HAVE_LIBBACKTRACE
	write_backtrace(BT_CRASH_REASON_ERRORMSG(buffer));
#endif
	// ---

	M_SaveConfig(NULL); // save game config, cvars..
	D_SaveBan(); // save the ban list
	G_SaveGameData(false); // Tails 12-08-2002

	// Shutdown. Here might be other errors.
	if (demo.recording)
		G_CheckDemoStatus();

	D_QuitNetGame();
	I_ShutdownMusic();
	I_ShutdownSound();
	// use this for 1.28 19990220 by Kin
	I_ShutdownGraphics();
	I_ShutdownSystem();

#if defined (PARANOIA) && defined (__CYGWIN__)
	*(INT32 *)2 = 4; //Alam: Debug!
#endif

	exit(-1);
}

/**	\brief quit function table
*/
static quitfuncptr quit_funcs[MAX_QUIT_FUNCS]; /* initialized to all bits 0 */

//
//  Adds a function to the list that need to be called by I_SystemShutdown().
//
void I_AddExitFunc(void (*func)())
{
	INT32 c;

	for (c = 0; c < MAX_QUIT_FUNCS; c++)
	{
		if (!quit_funcs[c])
		{
			quit_funcs[c] = func;
			break;
		}
	}
}


//
//  Removes a function from the list that need to be called by
//   I_SystemShutdown().
//
void I_RemoveExitFunc(void (*func)())
{
	INT32 c;

	for (c = 0; c < MAX_QUIT_FUNCS; c++)
	{
		if (quit_funcs[c] == func)
		{
			while (c < MAX_QUIT_FUNCS-1)
			{
				quit_funcs[c] = quit_funcs[c+1];
				c++;
			}
			quit_funcs[MAX_QUIT_FUNCS-1] = NULL;
			break;
		}
	}
}

#if !(defined (__unix__) || defined(__APPLE__) || defined (UNIXCOMMON))
static void Shittycopyerror(const char *name)
{
	I_OutputMsg(
			"Error copying log file: %s: %s\n",
			name,
			strerror(errno)
	);
}

static void Shittylogcopy(void)
{
	char buf[8192];
	FILE *fp;
	size_t r;
	if (fseek(logstream, 0, SEEK_SET) == -1)
	{
		Shittycopyerror("fseek");
	}
	else if (( fp = fopen(logfilename, "wt") ))
	{
		while (( r = fread(buf, 1, sizeof buf, logstream) ))
		{
			if (fwrite(buf, 1, r, fp) < r)
			{
				Shittycopyerror("fwrite");
				break;
			}
		}
		if (ferror(logstream))
		{
			Shittycopyerror("fread");
		}
		fclose(fp);
	}
	else
	{
		Shittycopyerror(logfilename);
	}
}
#endif/*!(defined (__unix__) || defined(__APPLE__) || defined (UNIXCOMMON))*/

//
//  Closes down everything. This includes restoring the initial
//  palette and video mode, and removing whatever mouse, keyboard, and
//  timer routines have been installed.
//
//  NOTE: Shutdown user funcs are effectively called in reverse order.
//
void I_ShutdownSystem(void)
{
	INT32 c;

#ifdef NEWSIGNALHANDLER
	if (M_CheckParm("-nofork"))
#endif
		I_ShutdownConsole();

	for (c = MAX_QUIT_FUNCS-1; c >= 0; c--)
		if (quit_funcs[c])
			(*quit_funcs[c])();
#ifdef LOGMESSAGES
	if (logstream)
	{
		I_OutputMsg("I_ShutdownSystem(): end of logstream.\n");
#if !(defined (__unix__) || defined(__APPLE__) || defined (UNIXCOMMON))
		Shittylogcopy();
#endif
		fclose(logstream);
		logstream = NULL;
	}
#endif
}

void I_GetDiskFreeSpace(INT64 *freespace)
{
#if defined (__unix__) || defined(__APPLE__) || defined (UNIXCOMMON)
#if defined (SOLARIS) || defined (__HAIKU__)
	*freespace = INT32_MAX;
	return;
#else // Both Linux and BSD have this, apparently.
	struct statfs stfs;
	if (statfs(".", &stfs) == -1)
	{
		*freespace = INT32_MAX;
		return;
	}
	*freespace = stfs.f_bavail * stfs.f_bsize;
#endif
#elif defined (_WIN32)
	static p_GetDiskFreeSpaceExA pfnGetDiskFreeSpaceEx = NULL;
	static boolean testwin95 = false;
	ULARGE_INTEGER usedbytes, lfreespace;

	if (!testwin95)
	{
		pfnGetDiskFreeSpaceEx = (p_GetDiskFreeSpaceExA)(LPVOID)GetProcAddress(GetModuleHandleA("kernel32.dll"), "GetDiskFreeSpaceExA");
		testwin95 = true;
	}
	if (pfnGetDiskFreeSpaceEx)
	{
		if (pfnGetDiskFreeSpaceEx(NULL, &lfreespace, &usedbytes, NULL))
			*freespace = lfreespace.QuadPart;
		else
			*freespace = INT32_MAX;
	}
	else
	{
		DWORD SectorsPerCluster, BytesPerSector, NumberOfFreeClusters, TotalNumberOfClusters;
		GetDiskFreeSpace(NULL, &SectorsPerCluster, &BytesPerSector,
						 &NumberOfFreeClusters, &TotalNumberOfClusters);
		*freespace = BytesPerSector*SectorsPerCluster*NumberOfFreeClusters;
	}
#else // Dummy for platform independent; 1GB should be enough
	*freespace = 1024*1024*1024;
#endif
}

char *I_GetUserName(void)
{
	static char username[MAXPLAYERNAME+1];
	char *p;
#ifdef _WIN32
	DWORD i = MAXPLAYERNAME;

	if (!GetUserNameA(username, &i))
#endif
	{
		p = I_GetEnv("USER");
		if (!p)
		{
			p = I_GetEnv("user");
			if (!p)
			{
				p = I_GetEnv("USERNAME");
				if (!p)
				{
					p = I_GetEnv("username");
					if (!p)
					{
						return NULL;
					}
				}
			}
		}
		strncpy(username, p, MAXPLAYERNAME);
	}

	if (!fastcmp(username, ""))
		return username;

	return NULL; // dummy for platform independent version
}

INT32 I_mkdir(const char *dirname, INT32 unixright)
{
//[segabor]
#if defined (__unix__) || defined(__APPLE__) || defined (UNIXCOMMON) || defined (__CYGWIN__) || defined (__OS2__)
	return mkdir(dirname, unixright);
#elif defined (_WIN32)
	UNREFERENCED_PARAMETER(unixright); /// \todo should implement ntright under nt...
	return CreateDirectoryA(dirname, NULL);
#else
	(void)dirname;
	(void)unixright;
	return false;
#endif
}

char *I_GetEnv(const char *name)
{
	return getenv(name);
}

INT32 I_PutEnv(char *variable)
{
	return putenv(variable);
}

size_t I_GetRandomBytes(char *destination, size_t count)
{
#if defined (__unix__) || defined (UNIXCOMMON) || defined(__APPLE__)
	FILE *rndsource;
	size_t actual_bytes;

	if (!(rndsource = fopen("/dev/urandom", "r")))
		if (!(rndsource = fopen("/dev/random", "r")))
			actual_bytes = 0;

	if (rndsource)
	{
		actual_bytes = fread(destination, 1, count, rndsource);
		fclose(rndsource);
	}

	if (actual_bytes == 0)
		I_OutputMsg("I_GetRandomBytes(): couldn't get any random bytes");

	return actual_bytes;
#elif defined (_WIN32)
	if (RtlGenRandom(destination, count))
		return count;

	I_OutputMsg("I_GetRandomBytes(): couldn't get any random bytes");
	return 0;
#else
	#warning SDL I_GetRandomBytes is not implemented on this platform.
	return 0;
#endif
}

INT32 I_ClipboardCopy(const char *data, size_t size)
{
	(void)data;
	(void)size;
	return -1;
}

const char *I_ClipboardPaste(void)
{
	return NULL;
}

/**	\brief	The isWadPathOk function

	\param	path	string path to check

	\return if true, wad file found


*/
static boolean isWadPathOk(const char *path)
{
	char *wad3path = malloc(256);

	if (!wad3path)
		return false;

	sprintf(wad3path, pandf, path, WADKEYWORD1);

	if (FIL_ReadFileOK(wad3path))
	{
		free(wad3path);
		return true;
	}

	sprintf(wad3path, pandf, path, WADKEYWORD2);

	if (FIL_ReadFileOK(wad3path))
	{
		free(wad3path);
		return true;
	}

	free(wad3path);
	return false;
}

static void pathonly(char *s)
{
	size_t j;

	for (j = strlen(s); j != (size_t)-1; j--)
		if ((s[j] == '\\') || (s[j] == ':') || (s[j] == '/'))
		{
			if (s[j] == ':') s[j+1] = 0;
			else s[j] = 0;
			return;
		}
}

/**	\brief	search for srb2.srb in the given path

	\param	searchDir	starting path

	\return	WAD path if not NULL


*/
static const char *searchWad(const char *searchDir)
{
	static char tempsw[256] = "";
	filestatus_t fstemp;

	strcpy(tempsw, WADKEYWORD1);
	fstemp = filesearch(tempsw, searchDir, NULL, true, 20);
	if (fstemp == FS_FOUND)
	{
		pathonly(tempsw);
		return tempsw;
	}

	return NULL;
}

#define CHECKWADPATH(ret) \
do { \
	I_OutputMsg(",%s", ret); \
	if (isWadPathOk(ret)) \
		return ret; \
} while (0)

#define SEARCHWAD(str) \
do { \
	WadPath = searchWad(str); \
	if (WadPath) \
		return WadPath; \
} while (0)

/**	\brief go through all possible paths and look for srb2.srb

  \return path to srb2.srb if any
*/
static const char *locateWad(void)
{
	const char *envstr;
	const char *WadPath;
	int i;

	I_OutputMsg("SRB2WADDIR");
	// does SRB2WADDIR exist?
	if (((envstr = I_GetEnv("SRB2WADDIR")) != NULL) && isWadPathOk(envstr))
		return envstr;

#ifndef NOCWD
	// examine current dir
	strcpy(returnWadPath, ".");
	I_OutputMsg(",%s", returnWadPath);
	if (isWadPathOk(returnWadPath))
		return NULL;
#endif

#ifndef NOHOME
#ifdef DEFAULTDIR
	I_OutputMsg(",HOME/" DEFAULTDIR);
	// examine user jart directory
	if ((envstr = I_GetEnv("HOME")) != NULL)
	{
		sprintf(returnWadPath, "%s" PATHSEP DEFAULTDIR, envstr);
		CHECKWADPATH(returnWadPath);
	}
#endif
#endif

#ifdef __APPLE__
	OSX_GetResourcesPath(returnWadPath);
	CHECKWADPATH(returnWadPath);
#endif

	// examine default dirs
	for (i = 0; wadDefaultPaths[i]; i++)
	{
		strcpy(returnWadPath, wadDefaultPaths[i]);
		CHECKWADPATH(returnWadPath);
	}

	// search paths
	for (i = 0; wadSearchPaths[i]; i++)
	{
		I_OutputMsg(", in:%s", wadSearchPaths[i]);
		SEARCHWAD(wadSearchPaths[i]);
	}

	// if nothing was found
	return NULL;
}

const char *I_LocateWad(void)
{
	const char *waddir = NULL;

	I_OutputMsg("Looking for WADs in: ");
	waddir = locateWad();
	I_OutputMsg("\n");

	if (waddir)
	{
		// change to the directory where we found srb2.srb
#if defined (_WIN32)
		waddir = _fullpath(NULL, waddir, MAX_PATH);
		SetCurrentDirectoryA(waddir);
#else
		waddir = realpath(waddir, NULL);
		if (waddir == NULL || chdir(waddir) == -1)
			I_OutputMsg("Couldn't change working directory\n");
#endif
	}

	return waddir;
}

#ifdef __linux__
#define MEMINFO_FILE "/proc/meminfo"
#define MEMTOTAL "MemTotal:"
#define MEMAVAILABLE "MemAvailable:"
#define MEMFREE "MemFree:"
#define CACHED "Cached:"
#define BUFFERS "Buffers:"
#define SHMEM "Shmem:"

/* Parse the contents of /proc/meminfo (in buf), return value of "name"
 * (example: MemTotal) */
static long get_entry(const char* name, const char* buf)
{
	long val;
	char* hit = strstr(buf, name);
	if (hit == NULL) {
		return -1;
	}

	errno = 0;
	val = strtol(hit + strlen(name), NULL, 10);
	if (errno != 0) {
		CONS_Alert(CONS_ERROR, M_GetText("get_entry: strtol() failed: %s\n"), strerror(errno));
		return -1;
	}
	return val;
}
#endif

size_t I_GetFreeMem(size_t *total)
{
#ifdef FREEBSD
	u_int v_free_count, v_page_size, v_page_count;
	size_t size = sizeof(v_free_count);
	sysctlbyname("vm.stats.vm.v_free_count", &v_free_count, &size, NULL, 0);
	size = sizeof(v_page_size);
	sysctlbyname("vm.stats.vm.v_page_size", &v_page_size, &size, NULL, 0);
	size = sizeof(v_page_count);
	sysctlbyname("vm.stats.vm.v_page_count", &v_page_count, &size, NULL, 0);

	if (total)
		*total = v_page_count * v_page_size;
	return v_free_count * v_page_size;
#elif defined (SOLARIS)
	/* Just guess */
	if (total)
		*total = 32 << 20;
	return 32 << 20;
#elif defined (_WIN32)
	MEMORYSTATUSEX info;

	info.dwLength = sizeof (MEMORYSTATUSEX);
	GlobalMemoryStatusEx( &info );
	if (total)
		*total = (size_t)info.ullTotalPhys;
	return (size_t)info.ullAvailPhys;
#elif defined (__OS2__)
	UINT32 pr_arena;

	if (total)
		DosQuerySysInfo( QSV_TOTPHYSMEM, QSV_TOTPHYSMEM,
							(PVOID) total, sizeof (UINT32));
	DosQuerySysInfo( QSV_MAXPRMEM, QSV_MAXPRMEM,
				(PVOID) &pr_arena, sizeof (UINT32));

	return pr_arena;
#elif defined (__linux__)
	/* Linux */
	char buf[1024];
	char *memTag;
	size_t freeKBytes;
	size_t totalKBytes;
	INT32 n;
	INT32 meminfo_fd = -1;
	long Cached;
	long MemFree;
	long Buffers;
	long Shmem;
	long MemAvailable = -1;

	meminfo_fd = open(MEMINFO_FILE, O_RDONLY);
	n = read(meminfo_fd, buf, 1023);
	close(meminfo_fd);

	if (n < 0)
	{
		// Error
		if (total)
			*total = 0;
		return 0;
	}

	buf[n] = '\0';
	if ((memTag = strstr(buf, MEMTOTAL)) == NULL)
	{
		// Error
		if (total)
			*total = 0;
		return 0;
	}

	memTag += sizeof (MEMTOTAL);
	totalKBytes = strtoul(memTag, NULL, 10);

	if ((memTag = strstr(buf, MEMAVAILABLE)) == NULL)
	{
		Cached = get_entry(CACHED, buf);
		MemFree = get_entry(MEMFREE, buf);
		Buffers = get_entry(BUFFERS, buf);
		Shmem = get_entry(SHMEM, buf);
		MemAvailable = Cached + MemFree + Buffers - Shmem;

		if (MemAvailable == -1)
		{
			// Error
			if (total)
				*total = 0;
			return 0;
		}
		freeKBytes = MemAvailable;
	}
	else
	{
		memTag += sizeof (MEMAVAILABLE);
		freeKBytes = strtoul(memTag, NULL, 10);
	}

	if (total)
		*total = totalKBytes << 10;
	return freeKBytes << 10;
#elif defined(__APPLE__)
	/* macOS */
	mach_port_t host = mach_host_self();
	kern_return_t kr;
	mach_msg_type_number_t count;
	vm_size_t v_page_size;
	struct vm_statistics64 vm_stats;
	uint64_t total_mem, free_mem;
	size_t size;

	size = sizeof(total_mem);
	if (sysctlbyname("hw.memsize", &total_mem, &size, NULL, 0) < 0)
		total_mem = 0;

	kr = host_page_size(host, &v_page_size);
	if (kr != KERN_SUCCESS)
		v_page_size = 4096;

	count = HOST_VM_INFO64_COUNT;
	kr = host_statistics64(host, HOST_VM_INFO64, (host_info64_t)&vm_stats, &count);
	if (kr == KERN_SUCCESS)
		free_mem = (uint64_t)(vm_stats.free_count + vm_stats.inactive_count) * v_page_size;
	else
		free_mem = 0;

	if (total)
		*total = (size_t)total_mem;
	return (size_t)free_mem;
#else
	// Guess 48 MB.
	if (total)
	{
		*total = 48<<20;
	}
	return 48<<20;
#endif
}

// note CPUAFFINITY code used to reside here
void I_RegisterSysCommands(void) {}

const char *I_GetSysName(void)
{
	// reference: https://sourceforge.net/p/predef/wiki/OperatingSystems/
#if defined(_WIN32) || defined(__CYGWIN__)
	return "Windows";
#elif defined(__APPLE__)
	return "Mac OS";
#elif defined(__linux__)
	return "Linux";
#elif defined(__FreeBSD__)
	return "FreeBSD";
#elif defined(__OpenBSD__)
	return "OpenBSD";
#elif defined(__NetBSD__)
	return "NetBSD";
#elif defined(__DragonFly__)
	return "DragonFly BSD";
#elif defined(__gnu_hurd__)
	return "GNU Hurd"; // for anyone mental enough to set up an SRB2 server on GNU Hurd
#elif defined(__hpux)
	return "HP-UX";
#elif defined(EPLAN9)
	return "Plan 9";
#elif defined(__HAIKU__)
	return "Haiku";
#elif defined(__BEOS__)
	return "BeOS";
#elif defined(__minix)
	return "Minix";
#elif defined(__sun)
#if defined(__SVR4) || defined(__srv4__)
	return "Solaris"; // this would be so cursed...
#else
	return "SunOS";
#endif
#elif defined(_AIX)
	return "AIX";
#elif defined(__SYLLABLE__)
	return "SyllableOS"; // RIP SyllableOS, i still miss you ;-;
#else
	return "Unknown";
#endif
}

boolean I_InitNetwork(void)
{
	// this must exist, but this is actually handled in i_tcp.c
	return false;
}
