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
/// \file  m_misc.h
/// \brief Commonly used routines
///        Default config file, PCX screenshots, file i/o

#ifdef __GNUC__

#if (__GNUC__ > 4) || (__GNUC__ == 4 && __GNUC_MINOR__ >= 3)
// Ignore "argument might be clobbered by longjmp" warning in GCC
// (if libpng is compiled with setjmp error handling)
#pragma GCC diagnostic ignored "-Wclobbered"
#endif
#include <unistd.h>
#endif

#include <errno.h>

// Extended map support.
#include <ctype.h>

#include "doomdef.h"
#include "g_game.h"
#include "m_misc.h"
#include "hu_stuff.h"
#include "st_stuff.h"
#include "v_video.h"
#include "z_zone.h"
#include "g_input.h"
#include "i_time.h"
#include "i_video.h"
#include "d_main.h"
#include "m_argv.h"
#include "i_system.h"
#include "command.h" // cv_execversion

#include "m_anigif.h"

// So that the screenshot menu auto-updates...
#include "m_menu.h"

#ifdef HWRENDER
#include "hardware/hw_main.h"
#endif

#ifdef HAVE_SDL
#include "sdl/hwsym_sdl.h"
#ifdef __linux__
#ifndef _LARGEFILE64_SOURCE
typedef off_t off64_t;
#endif
#endif
#endif

#if defined(__MINGW32__) && ((__GNUC__ > 7) || (__GNUC__ == 6 && __GNUC_MINOR__ >= 3)) && (__GNUC__ < 8)
#define PRIdS "u"
#elif defined(_WIN32) && !defined(__MINGW64__)
#define PRIdS "Iu"
#elif defined(_WIN32) && defined(__MINGW64__)
#define PRIdS PRIuPTR
#else
#define PRIdS "zu"
#endif

#ifdef HAVE_PNG

#ifndef _MSC_VER
#ifndef _LARGEFILE64_SOURCE
#define _LARGEFILE64_SOURCE
#endif
#endif

#ifndef _LFS64_LARGEFILE
#define _LFS64_LARGEFILE
#endif

#ifndef _FILE_OFFSET_BITS
#define _FILE_OFFSET_BITS 0
#endif

 #include "zlib.h"
 #include "png.h"
 #if (PNG_LIBPNG_VER_MAJOR > 1) || (PNG_LIBPNG_VER_MAJOR == 1 && PNG_LIBPNG_VER_MINOR >= 4)
  #define NO_PNG_DEBUG // 1.4.0 move png_debug to pngpriv.h
 #endif
 #ifdef PNG_WRITE_SUPPORTED
  #define USE_PNG // Only actually use PNG if write is supported.
  #if defined (PNG_WRITE_APNG_SUPPORTED) //|| !defined(PNG_STATIC)
    #include "apng.h"
    #define USE_APNG
  #endif
  // See hardware/hw_draw.c for a similar check to this one.
 #endif
#endif

static CV_PossibleValue_t screenshot_cons_t[] = {{0, "Default"}, {1, "HOME"}, {2, "SRB2"}, {3, "CUSTOM"}, {0, NULL}};
consvar_t cv_screenshot_option = {"screenshot_option", "Default", CV_SAVE|CV_CALL, screenshot_cons_t, Screenshot_option_Onchange, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_screenshot_folder = {"screenshot_folder", "", CV_SAVE, NULL, NULL, 0, NULL, NULL, 0, 0, NULL};

static CV_PossibleValue_t moviemode_cons_t[] = {{MM_GIF, "GIF"}, {MM_APNG, "aPNG"}, {MM_SCREENSHOT, "Screenshots"}, {0, NULL}};
consvar_t cv_moviemode = {"moviemode_mode", "GIF", CV_SAVE|CV_CALL, moviemode_cons_t, Moviemode_mode_Onchange, 0, NULL, NULL, 0, 0, NULL};

static CV_PossibleValue_t zlib_mem_level_t[] = {
	{1, "(Min Memory) 1"},
	{2, "2"}, {3, "3"}, {4, "4"}, {5, "5"}, {6, "6"}, {7, "7"},
	{8, "(Optimal) 8"}, //libpng Default
	{9, "(Max Memory) 9"}, {0, NULL}};

static CV_PossibleValue_t zlib_level_t[] = {
	{0, "No Compression"},  //Z_NO_COMPRESSION
	{1, "(Fastest) 1"}, //Z_BEST_SPEED
	{2, "2"}, {3, "3"}, {4, "4"}, {5, "5"},
	{6, "(Optimal) 6"}, //Zlib Default
	{7, "7"}, {8, "8"},
	{9, "(Maximum) 9"}, //Z_BEST_COMPRESSION
	{0, NULL}};

static CV_PossibleValue_t zlib_strategy_t[] = {
	{0, "Normal"}, //Z_DEFAULT_STRATEGY
	{1, "Filtered"}, //Z_FILTERED
	{2, "Huffman Only"}, //Z_HUFFMAN_ONLY
	{3, "RLE"}, //Z_RLE
	{4, "Fixed"}, //Z_FIXED
	{0, NULL}};

static CV_PossibleValue_t zlib_window_bits_t[] = {
#ifdef WBITS_8_OK
	{8, "256"},
#endif
	{9, "512"}, {10, "1k"}, {11, "2k"}, {12, "4k"}, {13, "8k"},
	{14, "16k"}, {15, "32k"},
	{0, NULL}};

static CV_PossibleValue_t apng_delay_t[] = {
	{1, "1x"},
	{2, "1/2x"},
	{3, "1/3x"},
	{4, "1/4x"},
	{0, NULL}};

// zlib memory usage is as follows:
// (1 << (zlib_window_bits+2)) +  (1 << (zlib_level+9))
consvar_t cv_zlib_memory = {"png_memory_level", "7", CV_SAVE, zlib_mem_level_t, NULL, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_zlib_level = {"png_compress_level", "(Optimal) 6", CV_SAVE, zlib_level_t, NULL, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_zlib_strategy = {"png_strategy", "Normal", CV_SAVE, zlib_strategy_t, NULL, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_zlib_window_bits = {"png_window_size", "32k", CV_SAVE, zlib_window_bits_t, NULL, 0, NULL, NULL, 0, 0, NULL};

consvar_t cv_zlib_memorya = {"apng_memory_level", "(Max Memory) 9", CV_SAVE, zlib_mem_level_t, NULL, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_zlib_levela = {"apng_compress_level", "4", CV_SAVE, zlib_level_t, NULL, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_zlib_strategya = {"apng_strategy", "RLE", CV_SAVE, zlib_strategy_t, NULL, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_zlib_window_bitsa = {"apng_window_size", "32k", CV_SAVE, zlib_window_bits_t, NULL, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_apng_delay = {"apng_speed", "1/2x", CV_SAVE, apng_delay_t, NULL, 0, NULL, NULL, 0, 0, NULL};

boolean takescreenshot = false; // Take a screenshot this tic

moviemode_t moviemode = MM_OFF;

/** Returns the map number for a map identified by the last two characters in
  * its name.
  *
  * \param first  The first character after MAP.
  * \param second The second character after MAP.
  * \return The map number, or 0 if no map corresponds to these characters.
  * \sa G_BuildMapName
  */
INT32 M_MapNumber(char first, char second)
{
	if (isdigit(first))
	{
		if (isdigit(second))
			return ((INT32)first - '0') * 10 + ((INT32)second - '0');
		return 0;
	}

	if (!isalpha(first))
		return 0;
	if (!isalnum(second))
		return 0;

	return 100 + ((INT32)tolower(first) - 'a') * 36 + (isdigit(second) ? ((INT32)second - '0') :
		((INT32)tolower(second) - 'a') + 10);
}

// ==========================================================================
//                         FILE INPUT / OUTPUT
// ==========================================================================

// some libcs has no access function, make our own
#if 0
int access(const char *path, int amode)
{
	int accesshandle = -1;
	FILE *handle = NULL;
	if (amode == 6) // W_OK|R_OK
		handle = fopen(path, "r+");
	else if (amode == 4) // R_OK
		handle = fopen(path, "r");
	else if (amode == 2) // W_OK
		handle = fopen(path, "a+");
	else if (amode == 0) //F_OK
		handle = fopen(path, "rb");
	if (handle)
	{
		accesshandle = 0;
		fclose(handle);
	}
	return accesshandle;
}
#endif


//
// FIL_WriteFile
//
#ifndef O_BINARY
#define O_BINARY 0
#endif

/** Writes out a file.
  *
  * \param name   Name of the file to write.
  * \param source Memory location to write from.
  * \param length How many bytes to write.
  * \return True on success, false on failure.
  */
boolean FIL_WriteFile(char const *name, const void *source, size_t length)
{
	FILE *handle = NULL;
	size_t count;

	//if (FIL_WriteFileOK(name))
		handle = fopen(name, "w+b");

	if (!handle)
		return false;

	count = fwrite(source, 1, length, handle);
	fclose(handle);

	if (count < length)
		return false;

	return true;
}

/** Reads in a file, appending a zero byte at the end.
  *
  * \param name   Filename to read.
  * \param buffer Pointer to a pointer, which will be set to the location of a
  *               newly allocated buffer holding the file's contents.
  * \return Number of bytes read, not counting the zero byte added to the end,
  *         or 0 on error.
  */
size_t FIL_ReadFileTag(char const *name, UINT8 **buffer, INT32 tag)
{
	FILE *handle = NULL;
	size_t count, length;
	UINT8 *buf;

	if (FIL_ReadFileOK(name))
		handle = fopen(name, "rb");

	if (!handle)
		return 0;

	fseek(handle,0,SEEK_END);
	length = ftell(handle);
	fseek(handle,0,SEEK_SET);

	buf = Z_Malloc(length + 1, tag, NULL);
	count = fread(buf, 1, length, handle);
	fclose(handle);

	if (count < length)
	{
		Z_Free(buf);
		return 0;
	}

	// append 0 byte for script text files
	buf[length] = 0;

	*buffer = buf;
	return length;
}

/** Check if the filename exists
  *
  * \param name   Filename to check.
  * \return true if file exists, false if it doesn't.
  */
boolean FIL_FileExists(char const *name)
{
	return access(name,0)+1; //F_OK
}


/** Check if the filename OK to write
  *
  * \param name   Filename to check.
  * \return true if file write-able, false if it doesn't.
  */
boolean FIL_WriteFileOK(char const *name)
{
	return access(name,2)+1; //W_OK
}


/** Check if the filename OK to read
  *
  * \param name   Filename to check.
  * \return true if file read-able, false if it doesn't.
  */
boolean FIL_ReadFileOK(char const *name)
{
	return access(name,4)+1; //R_OK
}

/** Check if the filename OK to read/write
  *
  * \param name   Filename to check.
  * \return true if file (read/write)-able, false if it doesn't.
  */
boolean FIL_FileOK(char const *name)
{
	return access(name,6)+1; //R_OK|W_OK
}


/** Checks if a pathname has a file extension and adds the extension provided
  * if not.
  *
  * \param path      Pathname to check.
  * \param extension Extension to add if no extension is there.
  */
void FIL_DefaultExtension(char *path, const char *extension)
{
	char *src;

	// search for '.' from end to begin, add .EXT only when not found
	src = path + strlen(path) - 1;

	while (*src != '/' && src != path)
	{
		if (*src == '.')
			return; // it has an extension
		src--;
	}

	strcat(path, extension);
}

void FIL_ForceExtension(char *path, const char *extension)
{
	char *src;

	// search for '.' from end to begin, add .EXT only when not found
	src = path + strlen(path) - 1;

	while (*src != '/' && src != path)
	{
		if (*src == '.')
		{
			*src = '\0';
			break; // it has an extension
		}
		src--;
	}

	strcat(path, extension);
}

/** Checks if a filename extension is found.
  * Lump names do not contain dots.
  *
  * \param in String to check.
  * \return True if an extension is found, otherwise false.
  */
boolean FIL_CheckExtension(const char *in)
{
	while (*in++)
		if (*in == '.')
			return true;

	return false;
}

// ==========================================================================
//                        CONFIGURATION FILE
// ==========================================================================

//
// DEFAULTS
//

char configfile[MAX_WADPATH];

// ==========================================================================
//                          CONFIGURATION
// ==========================================================================
static boolean gameconfig_loaded = false; // true once config.cfg loaded AND executed

/** Saves a player's config, possibly to a particular file.
  *
  * \sa Command_LoadConfig_f
  */
void Command_SaveConfig_f(void)
{
	char tmpstr[MAX_WADPATH];

	if (COM_Argc() < 2)
	{
		CONS_Printf(M_GetText("saveconfig <filename[.cfg]> [-silent] : save config to a file\n"));
		return;
	}
	strcpy(tmpstr, COM_Argv(1));
	FIL_ForceExtension(tmpstr, ".cfg");

	M_SaveConfig(tmpstr);
	if (stricmp(COM_Argv(2), "-silent"))
		CONS_Printf(M_GetText("config saved as %s\n"), configfile);
}

/** Loads a game config, possibly from a particular file.
  *
  * \sa Command_SaveConfig_f, Command_ChangeConfig_f
  */
void Command_LoadConfig_f(void)
{
	if (COM_Argc() != 2)
	{
		CONS_Printf(M_GetText("loadconfig <filename[.cfg]> : load config from a file\n"));
		return;
	}

	strcpy(configfile, COM_Argv(1));
	FIL_ForceExtension(configfile, ".cfg");

	// load default control
	G_ClearAllControlKeys();
	G_Controldefault(0);

	// temporarily reset execversion to default
	CV_ToggleExecVersion(true);
	COM_BufInsertText(va("%s \"%s\"\n", cv_execversion.name, cv_execversion.defaultvalue));
	CV_InitFilterVar();

	// exec the config
	COM_BufInsertText(va("exec \"%s\"\n", configfile));

	// don't filter anymore vars and don't let this convsvar be changed
	COM_BufInsertText(va("%s \"%d\"\n", cv_execversion.name, EXECVERSION));
	CV_ToggleExecVersion(false);
}

/** Saves the current configuration and loads another.
  *
  * \sa Command_LoadConfig_f, Command_SaveConfig_f
  */
void Command_ChangeConfig_f(void)
{
	if (COM_Argc() != 2)
	{
		CONS_Printf(M_GetText("changeconfig <filename[.cfg]> : save current config and load another\n"));
		return;
	}

	COM_BufAddText(va("saveconfig \"%s\"\n", configfile));
	COM_BufAddText(va("loadconfig \"%s\"\n", COM_Argv(1)));
}

/** Loads the default config file.
  *
  * \sa Command_LoadConfig_f
  */
void M_FirstLoadConfig(void)
{
	// configfile is initialised by d_main when searching for the wad?

	// check for a custom config file
	if (M_CheckParm("-config") && M_IsNextParm())
	{
		strcpy(configfile, M_GetNextParm());
		CONS_Printf(M_GetText("config file: %s\n"), configfile);
	}

	// load default control
	G_Controldefault(0);

	// register execversion here before we load any configs
	CV_RegisterVar(&cv_execversion);

	// temporarily reset execversion to default
	// we shouldn't need to do this, but JUST in case...
	CV_ToggleExecVersion(true);
	COM_BufInsertText(va("%s \"%s\"\n", cv_execversion.name, cv_execversion.defaultvalue));
	CV_InitFilterVar();

	// load config, make sure those commands doesnt require the screen...
	COM_BufInsertText(va("exec \"%s\"\n", configfile));
	// no COM_BufExecute() needed; that does it right away

	// don't filter anymore vars and don't let this convsvar be changed
	COM_BufInsertText(va("%s \"%d\"\n", cv_execversion.name, EXECVERSION));
	CV_ToggleExecVersion(false);

	// make sure I_Quit() will write back the correct config
	// (do not write back the config if it crash before)
	gameconfig_loaded = true;
}

/** Saves the game configuration.
  *
  * \sa Command_SaveConfig_f
  */
void M_SaveConfig(const char *filename)
{
	FILE *f;
	char *filepath;

	// make sure not to write back the config until it's been correctly loaded
	if (!gameconfig_loaded)
		return;

	// can change the file name
	if (filename)
	{
		if (!strstr(filename, ".cfg"))
		{
			CONS_Alert(CONS_NOTICE, M_GetText("Config filename must be .cfg\n"));
			return;
		}

		// append srb2home to beginning of filename
		// but check if srb2home isn't already there, first
		if (!strstr(filename, srb2home))
			filepath = va(pandf,srb2home, filename);
		else
			filepath = Z_StrDup(filename);

		f = fopen(filepath, "w");
		// change it only if valid
		if (f)
			strcpy(configfile, filepath);
		else
		{
			CONS_Alert(CONS_ERROR, M_GetText("Couldn't save game config file %s\n"), filepath);
			return;
		}
	}
	else
	{
		if (!strstr(configfile, ".cfg"))
		{
			CONS_Alert(CONS_NOTICE, M_GetText("Config filename must be .cfg\n"));
			return;
		}

		f = fopen(configfile, "w");
		if (!f)
		{
			CONS_Alert(CONS_ERROR, M_GetText("Couldn't save game config file %s\n"), configfile);
			return;
		}
	}

	// header message
	fprintf(f, "// SRB2Kart configuration file.\n");

	// print execversion FIRST, because subsequent consvars need to be filtered
	// always print current EXECVERSION
	fprintf(f, "%s \"%d\"\n", cv_execversion.name, EXECVERSION);

	// FIXME: save key aliases if ever implemented..

	CV_SaveVariables(f);
	if (!dedicated) G_SaveKeySetting(f);

	fclose(f);
}

// ==========================================================================
//                              SCREENSHOTS
// ==========================================================================
static UINT8 screenshot_palette[768];
static void M_CreateScreenShotPalette(void)
{
	size_t i, j;
	for (i = 0, j = 0; i < 768; i += 3, j++)
	{
		RGBA_t locpal = pLocalPalette[(max(st_palette,0)*256)+j];
		screenshot_palette[i] = locpal.s.red;
		screenshot_palette[i+1] = locpal.s.green;
		screenshot_palette[i+2] = locpal.s.blue;
	}
}

#if NUMSCREENS > 2
static const char *Newsnapshotfile(const char *pathname, const char *ext)
{
	static char freename[13] = "kartXXXX.ext";
	int i = 5000; // start in the middle: num screenshots divided by 2
	int add = i; // how much to add or subtract if wrong; gets divided by 2 each time
	int result; // -1 = guess too high, 0 = correct, 1 = guess too low

	// find a file name to save it to
	strcpy(freename+9,ext);

	for (;;)
	{
		freename[4] = (char)('0' + (char)(i/1000));
		freename[5] = (char)('0' + (char)((i/100)%10));
		freename[6] = (char)('0' + (char)((i/10)%10));
		freename[7] = (char)('0' + (char)(i%10));

		if (FIL_WriteFileOK(va(pandf,pathname,freename))) // access succeeds
			result = 1; // too low
		else // access fails: equal or too high
		{
			if (!i)
				break; // not too high, so it must be equal! YAY!

			freename[4] = (char)('0' + (char)((i-1)/1000));
			freename[5] = (char)('0' + (char)(((i-1)/100)%10));
			freename[6] = (char)('0' + (char)(((i-1)/10)%10));
			freename[7] = (char)('0' + (char)((i-1)%10));
			if (!FIL_WriteFileOK(va(pandf,pathname,freename))) // access fails
				result = -1; // too high
			else
				break; // not too high, so equal, YAY!
		}

		add /= 2;

		if (!add) // don't get stuck at 5 due to truncation!
			add = 1;

		i += add * result;

		if (i < 0 || i > 9999)
			return NULL;
	}

	freename[4] = (char)('0' + (char)(i/1000));
	freename[5] = (char)('0' + (char)((i/100)%10));
	freename[6] = (char)('0' + (char)((i/10)%10));
	freename[7] = (char)('0' + (char)(i%10));

	return freename;
}
#endif

#ifdef HAVE_PNG
FUNCNORETURN static void PNG_error(png_structp PNG, png_const_charp pngtext)
{
	//CONS_Debug(DBG_RENDER, "libpng error at %p: %s", PNG, pngtext);
	I_Error("libpng error at %p: %s", PNG, pngtext);
}

static void PNG_warn(png_structp PNG, png_const_charp pngtext)
{
	CONS_Debug(DBG_RENDER, "libpng warning at %p: %s", PNG, pngtext);
}

static void M_PNGhdr(png_structp png_ptr, png_infop png_info_ptr, PNG_CONST png_uint_32 width, PNG_CONST png_uint_32 height, PNG_CONST png_byte *palette)
{
	const png_byte png_interlace = PNG_INTERLACE_NONE; //PNG_INTERLACE_ADAM7
	if (palette)
	{
		png_colorp png_PLTE = png_malloc(png_ptr, sizeof(png_color)*256); //palette
		const png_byte *pal = palette;
		png_uint_16 i;
		for (i = 0; i < 256; i++)
		{
			png_PLTE[i].red   = *pal; pal++;
			png_PLTE[i].green = *pal; pal++;
			png_PLTE[i].blue  = *pal; pal++;
		}
		png_set_IHDR(png_ptr, png_info_ptr, width, height, 8, PNG_COLOR_TYPE_PALETTE,
		 png_interlace, PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
		png_write_info_before_PLTE(png_ptr, png_info_ptr);
		png_set_PLTE(png_ptr, png_info_ptr, png_PLTE, 256);
		png_free(png_ptr, (png_voidp)png_PLTE); // safe in libpng-1.2.1+
		png_set_filter(png_ptr, PNG_FILTER_TYPE_BASE, PNG_FILTER_NONE);
		png_set_compression_strategy(png_ptr, Z_DEFAULT_STRATEGY);
	}
	else
	{
		png_set_IHDR(png_ptr, png_info_ptr, width, height, 8, PNG_COLOR_TYPE_RGB,
		 png_interlace, PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
		png_write_info_before_PLTE(png_ptr, png_info_ptr);
		png_set_compression_strategy(png_ptr, Z_FILTERED);
	}
}

static void M_PNGText(png_structp png_ptr, png_infop png_info_ptr, PNG_CONST png_byte movie)
{
#ifdef PNG_TEXT_SUPPORTED
#define SRB2PNGTXT 11 //PNG_KEYWORD_MAX_LENGTH(79) is the max
	png_text png_infotext[SRB2PNGTXT];
	char keytxt[SRB2PNGTXT][12] = {
	"Title", "Description", "Playername", "Mapnum", "Mapname",
	"Location", "Interface", "Render Mode", "Revision", "Build Date", "Build Time"};
	char titletxt[] = "SRB2Kart " VERSIONSTRING;
	png_charp playertxt =  cv_playername.zstring;
	char desctxt[] = "SRB2Kart Screenshot";
	char Movietxt[] = "SRB2Kart Movie";
	size_t i;
	char interfacetxt[] =
#ifdef HAVE_SDL
	 "SDL";
#elif defined (_WINDOWS)
	 "DirectX";
#else
	 "Unknown";
#endif
	char rendermodetxt[9];
	char maptext[8];
	char lvlttltext[48];
	char locationtxt[40];
	char ctrevision[40];
	char ctdate[40];
	char cttime[40];

	switch (rendermode)
	{
		case render_soft:
			strcpy(rendermodetxt, "Software");
			break;
		case render_opengl:
			strcpy(rendermodetxt, "OpenGL");
			break;
		default: // Just in case
			strcpy(rendermodetxt, "None");
			break;
	}

	if (gamestate == GS_LEVEL)
		snprintf(maptext, 8, "%s", G_BuildMapName(gamemap));
	else
		snprintf(maptext, 8, "Unknown");

	if (gamestate == GS_LEVEL && mapheaderinfo[gamemap-1]->lvlttl[0] != '\0')
		snprintf(lvlttltext, 48, "%s%s%s",
			mapheaderinfo[gamemap-1]->lvlttl,
			(strlen(mapheaderinfo[gamemap-1]->zonttl) > 0) ? va(" %s",mapheaderinfo[gamemap-1]->zonttl) : // SRB2kart
			((mapheaderinfo[gamemap-1]->levelflags & LF_NOZONE) ? "" : " ZONE"),
			(strlen(mapheaderinfo[gamemap-1]->actnum) > 0) ? va(" %s",mapheaderinfo[gamemap-1]->actnum) : "");
	else
		snprintf(lvlttltext, 48, "Unknown");

	if (gamestate == GS_LEVEL && players[displayplayers[0]].mo)
		snprintf(locationtxt, 40, "X:%d Y:%d Z:%d A:%d",
			players[displayplayers[0]].mo->x>>FRACBITS,
			players[displayplayers[0]].mo->y>>FRACBITS,
			players[displayplayers[0]].mo->z>>FRACBITS,
			FixedInt(AngleFixed(players[displayplayers[0]].mo->angle)));
	else
		snprintf(locationtxt, 40, "Unknown");

	memset(png_infotext,0x00,sizeof (png_infotext));

	for (i = 0; i < SRB2PNGTXT; i++)
		png_infotext[i].key  = keytxt[i];

	png_infotext[0].text = titletxt;
	if (movie)
		png_infotext[1].text = Movietxt;
	else
		png_infotext[1].text = desctxt;
	png_infotext[2].text = playertxt;
	png_infotext[3].text = maptext;
	png_infotext[4].text = lvlttltext;
	png_infotext[5].text = locationtxt;
	png_infotext[6].text = interfacetxt;
	png_infotext[7].text = rendermodetxt;
	png_infotext[8].text = strncpy(ctrevision, comprevision, sizeof(ctrevision)-1);
	png_infotext[9].text = strncpy(ctdate, compdate, sizeof(ctdate)-1);
	png_infotext[10].text = strncpy(cttime, comptime, sizeof(cttime)-1);

	png_set_text(png_ptr, png_info_ptr, png_infotext, SRB2PNGTXT);
#undef SRB2PNGTXT
#endif
}

static inline void M_PNGImage(png_structp png_ptr, png_infop png_info_ptr, PNG_CONST png_uint_32 height, png_bytep png_buf)
{
	png_uint_32 pitch = png_get_rowbytes(png_ptr, png_info_ptr);
	png_bytepp row_pointers = png_malloc(png_ptr, height* sizeof (png_bytep));
	png_uint_32 y;
	for (y = 0; y < height; y++)
	{
		row_pointers[y] = png_buf;
		png_buf += pitch;
	}
	png_write_image(png_ptr, row_pointers);
	png_free(png_ptr, (png_voidp)row_pointers);
}

#ifdef USE_APNG
static png_structp apng_ptr = NULL;
static png_infop   apng_info_ptr = NULL;
static apng_infop  apng_ainfo_ptr = NULL;
static png_FILE_p  apng_FILE = NULL;
static png_uint_32 apng_frames = 0;
#ifdef PNG_STATIC // Win32 build have static libpng
#define aPNG_set_acTL png_set_acTL
#define aPNG_write_frame_head png_write_frame_head
#define aPNG_write_frame_tail png_write_frame_tail
#else // outside libpng may not have apng support

#ifndef PNG_WRITE_APNG_SUPPORTED // libpng header may not have apng patch

#ifndef PNG_INFO_acTL
#define PNG_INFO_acTL 0x10000L
#endif
#ifndef PNG_INFO_fcTL
#define PNG_INFO_fcTL 0x20000L
#endif
#ifndef PNG_FIRST_FRAME_HIDDEN
#define PNG_FIRST_FRAME_HIDDEN       0x0001
#endif
#ifndef PNG_DISPOSE_OP_NONE
#define PNG_DISPOSE_OP_NONE        0x00
#endif
#ifndef PNG_DISPOSE_OP_BACKGROUND
#define PNG_DISPOSE_OP_BACKGROUND  0x01
#endif
#ifndef PNG_DISPOSE_OP_PREVIOUS
#define PNG_DISPOSE_OP_PREVIOUS    0x02
#endif
#ifndef PNG_BLEND_OP_SOURCE
#define PNG_BLEND_OP_SOURCE        0x00
#endif
#ifndef PNG_BLEND_OP_OVER
#define PNG_BLEND_OP_OVER          0x01
#endif
#ifndef PNG_HAVE_acTL
#define PNG_HAVE_acTL             0x4000
#endif
#ifndef PNG_HAVE_fcTL
#define PNG_HAVE_fcTL             0x8000L
#endif

#endif
typedef png_uint_32 (*P_png_set_acTL) (png_structp png_ptr,
   png_infop info_ptr, png_uint_32 num_frames, png_uint_32 num_plays);
typedef void (*P_png_write_frame_head) (png_structp png_ptr,
   png_infop info_ptr, png_bytepp row_pointers,
   png_uint_32 width, png_uint_32 height,
   png_uint_32 x_offset, png_uint_32 y_offset,
   png_uint_16 delay_num, png_uint_16 delay_den, png_byte dispose_op,
   png_byte blend_op);

typedef void (*P_png_write_frame_tail) (png_structp png_ptr,
   png_infop info_ptr);
static P_png_set_acTL aPNG_set_acTL = NULL;
static P_png_write_frame_head aPNG_write_frame_head = NULL;
static P_png_write_frame_tail aPNG_write_frame_tail = NULL;
#endif

static inline boolean M_PNGLib(void)
{
#ifdef PNG_STATIC // Win32 build have static libpng
	return true;
#else
	static void *pnglib = NULL;
	if (aPNG_set_acTL && aPNG_write_frame_head && aPNG_write_frame_tail)
		return true;
	if (pnglib)
		return false;
#ifdef _WIN32
	pnglib = GetModuleHandleA("libpng.dll");
	if (!pnglib)
		pnglib = GetModuleHandleA("libpng12.dll");
	if (!pnglib)
		pnglib = GetModuleHandleA("libpng13.dll");
#elif defined (HAVE_SDL)
#ifdef __APPLE__
	pnglib = hwOpen("libpng.dylib");
#else
	pnglib = hwOpen("libpng.so");
#endif
#endif
	if (!pnglib)
		return false;
#ifdef HAVE_SDL
	aPNG_set_acTL = hwSym("png_set_acTL", pnglib);
	aPNG_write_frame_head = hwSym("png_write_frame_head", pnglib);
	aPNG_write_frame_tail = hwSym("png_write_frame_tail", pnglib);
#endif
#ifdef _WIN32
	aPNG_set_acTL = GetProcAddress("png_set_acTL", pnglib);
	aPNG_write_frame_head = GetProcAddress("png_write_frame_head", pnglib);
	aPNG_write_frame_tail = GetProcAddress("png_write_frame_tail", pnglib);
#endif
	return (aPNG_set_acTL && aPNG_write_frame_head && aPNG_write_frame_tail);
#endif
}

static void M_PNGFrame(png_structp png_ptr, png_infop png_info_ptr, png_bytep png_buf)
{
	png_uint_32 pitch = png_get_rowbytes(png_ptr, png_info_ptr);
	PNG_CONST png_uint_32 height = vid.height;
	png_bytepp row_pointers = png_malloc(png_ptr, height* sizeof (png_bytep));
	png_uint_32 y;
	png_uint_16 framedelay = (png_uint_16)cv_apng_delay.value;

	apng_frames++;

	for (y = 0; y < height; y++)
	{
		row_pointers[y] = png_buf;
		png_buf += pitch;
	}

#ifndef PNG_STATIC
	if (aPNG_write_frame_head)
#endif
		aPNG_write_frame_head(apng_ptr, apng_info_ptr, row_pointers,
			vid.width, /* width */
			height,    /* height */
			0,         /* x offset */
			0,         /* y offset */
			framedelay, TICRATE,/* delay numerator and denominator */
			PNG_DISPOSE_OP_BACKGROUND, /* dispose */
			PNG_BLEND_OP_SOURCE        /* blend */
		                     );

	png_write_image(png_ptr, row_pointers);

#ifndef PNG_STATIC
	if (aPNG_write_frame_tail)
#endif
		aPNG_write_frame_tail(apng_ptr, apng_info_ptr);

	png_free(png_ptr, (png_voidp)row_pointers);
}

static void M_PNGfix_acTL(png_structp png_ptr, png_infop png_info_ptr,
		apng_infop png_ainfo_ptr)
{
	apng_set_acTL(png_ptr, png_info_ptr, png_ainfo_ptr, apng_frames, 0);

#ifndef NO_PNG_DEBUG
	png_debug(1, "in png_write_acTL\n");
#endif
}

static boolean M_SetupaPNG(png_const_charp filename, png_bytep pal)
{
	apng_FILE = fopen(filename,"wb+"); // + mode for reading
	if (!apng_FILE)
	{
		CONS_Debug(DBG_RENDER, "M_StartMovie: Error on opening %s for write\n", filename);
		return false;
	}

	apng_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL,
	 PNG_error, PNG_warn);
	if (!apng_ptr)
	{
		CONS_Debug(DBG_RENDER, "M_StartMovie: Error on initialize libpng\n");
		fclose(apng_FILE);
		remove(filename);
		return false;
	}

	apng_info_ptr = png_create_info_struct(apng_ptr);
	if (!apng_info_ptr)
	{
		CONS_Debug(DBG_RENDER, "M_StartMovie: Error on allocate for libpng\n");
		png_destroy_write_struct(&apng_ptr,  NULL);
		fclose(apng_FILE);
		remove(filename);
		return false;
	}

	apng_ainfo_ptr = apng_create_info_struct(apng_ptr);
	if (!apng_ainfo_ptr)
	{
		CONS_Debug(DBG_RENDER, "M_StartMovie: Error on allocate for apng\n");
		png_destroy_write_struct(&apng_ptr, &apng_info_ptr);
		fclose(apng_FILE);
		remove(filename);
		return false;
	}

	png_init_io(apng_ptr, apng_FILE);

#ifdef PNG_SET_USER_LIMITS_SUPPORTED
	png_set_user_limits(apng_ptr, MAXVIDWIDTH, MAXVIDHEIGHT);
#endif

	//png_set_filter(apng_ptr, 0, PNG_ALL_FILTERS);

	png_set_compression_level(apng_ptr, cv_zlib_levela.value);
	png_set_compression_mem_level(apng_ptr, cv_zlib_memorya.value);
	png_set_compression_strategy(apng_ptr, cv_zlib_strategya.value);
	png_set_compression_window_bits(apng_ptr, cv_zlib_window_bitsa.value);

	M_PNGhdr(apng_ptr, apng_info_ptr, vid.width, vid.height, pal);

	M_PNGText(apng_ptr, apng_info_ptr, true);

	apng_set_set_acTL_fn(apng_ptr, apng_ainfo_ptr, aPNG_set_acTL);

	apng_set_acTL(apng_ptr, apng_info_ptr, apng_ainfo_ptr, PNG_UINT_31_MAX, 0);

	apng_write_info(apng_ptr, apng_info_ptr, apng_ainfo_ptr);

	apng_frames = 0;

	return true;
}
#endif
#endif

// ==========================================================================
//                             MOVIE MODE
// ==========================================================================
#if NUMSCREENS > 2
static inline moviemode_t M_StartMovieAPNG(const char *pathname)
{
#ifdef USE_APNG
	UINT8 *palette;
	const char *freename = NULL;
	boolean ret = false;

	if (!M_PNGLib())
	{
		CONS_Alert(CONS_ERROR, "Couldn't create aPNG: libpng not found\n");
		return MM_OFF;
	}

	if (!(freename = Newsnapshotfile(pathname,"png")))
	{
		CONS_Alert(CONS_ERROR, "Couldn't create aPNG: no slots open in %s\n", pathname);
		return MM_OFF;
	}

	if (rendermode == render_soft) M_CreateScreenShotPalette();
	ret = M_SetupaPNG(va(pandf,pathname,freename), (palette = screenshot_palette));

	if (!ret)
	{
		CONS_Alert(CONS_ERROR, "Couldn't create aPNG: error creating %s in %s\n", freename, pathname);
		return MM_OFF;
	}
	return MM_APNG;
#else
	// no APNG support exists
	(void)pathname;
	CONS_Alert(CONS_ERROR, "Couldn't create aPNG: this build lacks aPNG support\n");
	return MM_OFF;
#endif
}

static inline moviemode_t M_StartMovieGIF(const char *pathname)
{
#ifdef HAVE_ANIGIF
	const char *freename;

	if (!(freename = Newsnapshotfile(pathname,"gif")))
	{
		CONS_Alert(CONS_ERROR, "Couldn't create GIF: no slots open in %s\n", pathname);
		return MM_OFF;
	}

	if (!GIF_open(va(pandf,pathname,freename)))
	{
		CONS_Alert(CONS_ERROR, "Couldn't create GIF: error creating %s in %s\n", freename, pathname);
		return MM_OFF;
	}
	return MM_GIF;
#else
	// no GIF support exists
	(void)pathname;
	CONS_Alert(CONS_ERROR, "Couldn't create GIF: this build lacks GIF support\n");
	return MM_OFF;
#endif
}
#endif

void M_StartMovie(void)
{
#if NUMSCREENS > 2
	const char *pathname = ".";

	if (moviemode)
		return;

	if (cv_screenshot_option.value == 0)
		pathname = usehome ? srb2home : srb2path;
	else if (cv_screenshot_option.value == 1)
		pathname = srb2home;
	else if (cv_screenshot_option.value == 2)
		pathname = srb2path;
	else if (cv_screenshot_option.value == 3 && *cv_screenshot_folder.string != '\0')
		pathname = cv_screenshot_folder.string;

	if (rendermode == render_none)
		I_Error("Can't make a movie without a render system\n");

	switch (cv_moviemode.value)
	{
		case MM_GIF:
			moviemode = M_StartMovieGIF(pathname);
			break;
		case MM_APNG:
			moviemode = M_StartMovieAPNG(pathname);
			break;
		case MM_SCREENSHOT:
			moviemode = MM_SCREENSHOT;
			break;
		default: //???
			return;
	}

	if (moviemode == MM_APNG)
		CONS_Printf(M_GetText("Movie mode enabled (%s).\n"), "aPNG");
	else if (moviemode == MM_GIF)
		CONS_Printf(M_GetText("Movie mode enabled (%s).\n"), "GIF");
	else if (moviemode == MM_SCREENSHOT)
		CONS_Printf(M_GetText("Movie mode enabled (%s).\n"), "screenshots");

	//singletics = (moviemode != MM_OFF);
#endif
}

void M_SaveFrame(void)
{
#if NUMSCREENS > 2
	// paranoia: should be unnecessary without singletics
	static tic_t oldtic = 0;

	if (oldtic == I_GetTime())
		return;
	else
		oldtic = I_GetTime();

	switch (moviemode)
	{
		case MM_SCREENSHOT:
			takescreenshot = true;
			return;
		case MM_GIF:
			GIF_frame();
			return;
		case MM_APNG:
#ifdef USE_APNG
			{
				UINT8 *linear = NULL;
				if (!apng_FILE) // should not happen!!
				{
					moviemode = MM_OFF;
					return;
				}

				if (rendermode == render_soft)
				{
					// munge planar buffer to linear
					linear = screens[2];
					I_ReadScreen(linear);
				}
#ifdef HWRENDER
				else
					linear = HWR_GetScreenshot();
#endif
				M_PNGFrame(apng_ptr, apng_info_ptr, (png_bytep)linear);
#ifdef HWRENDER
				if (rendermode != render_soft && linear)
					free(linear);
#endif

				if (apng_frames == PNG_UINT_31_MAX)
				{
					CONS_Alert(CONS_NOTICE, M_GetText("Max movie size reached\n"));
					M_StopMovie();
				}
			}
#else
			moviemode = MM_OFF;
#endif
			return;
		default:
			return;
	}
#endif
}

void M_StopMovie(void)
{
#if NUMSCREENS > 2
	switch (moviemode)
	{
		case MM_GIF:
			if (!GIF_close())
				return;
			break;
		case MM_APNG:
#ifdef USE_APNG
			if (!apng_FILE)
				return;

			if (apng_frames)
			{
				M_PNGfix_acTL(apng_ptr, apng_info_ptr, apng_ainfo_ptr);
				apng_write_end(apng_ptr, apng_info_ptr, apng_ainfo_ptr);
			}

			png_destroy_write_struct(&apng_ptr, &apng_info_ptr);

			fclose(apng_FILE);
			apng_FILE = NULL;
			CONS_Printf("aPNG closed; wrote %u frames\n", (UINT32)apng_frames);
			apng_frames = 0;
			break;
#else
			return;
#endif
		case MM_SCREENSHOT:
			break;
		default:
			return;
	}
	moviemode = MM_OFF;
	CONS_Printf(M_GetText("Movie mode disabled.\n"));
#endif
}

// ==========================================================================
//                            SCREEN SHOTS
// ==========================================================================
#ifdef USE_PNG
/** Writes a PNG file to disk.
  *
  * \param filename Filename to write to.
  * \param data     The image data.
  * \param width    Width of the picture.
  * \param height   Height of the picture.
  * \param palette  Palette of image data.
  *  \note if palette is NULL, BGR888 format
  */
boolean M_SavePNG(const char *filename, void *data, int width, int height, const UINT8 *palette)
{
	png_structp png_ptr;
	png_infop png_info_ptr;
	PNG_CONST png_byte *PLTE = (const png_byte *)palette;
#ifdef PNG_SETJMP_SUPPORTED
#ifdef USE_FAR_KEYWORD
	jmp_buf jmpbuf;
#endif
#endif
	png_FILE_p png_FILE;

	png_FILE = fopen(filename,"wb");
	if (!png_FILE)
	{
		CONS_Debug(DBG_RENDER, "M_SavePNG: Error on opening %s for write\n", filename);
		return false;
	}

	png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, PNG_error, PNG_warn);
	if (!png_ptr)
	{
		CONS_Debug(DBG_RENDER, "M_SavePNG: Error on initialize libpng\n");
		fclose(png_FILE);
		remove(filename);
		return false;
	}

	png_info_ptr = png_create_info_struct(png_ptr);
	if (!png_info_ptr)
	{
		CONS_Debug(DBG_RENDER, "M_SavePNG: Error on allocate for libpng\n");
		png_destroy_write_struct(&png_ptr,  NULL);
		fclose(png_FILE);
		remove(filename);
		return false;
	}

#ifdef USE_FAR_KEYWORD
	if (setjmp(jmpbuf))
#else
	if (setjmp(png_jmpbuf(png_ptr)))
#endif
	{
		//CONS_Debug(DBG_RENDER, "libpng write error on %s\n", filename);
		png_destroy_write_struct(&png_ptr, &png_info_ptr);
		fclose(png_FILE);
		remove(filename);
		return false;
	}
#ifdef USE_FAR_KEYWORD
	png_memcpy(png_jmpbuf(png_ptr),jmpbuf, sizeof (jmp_buf));
#endif
	png_init_io(png_ptr, png_FILE);

#ifdef PNG_SET_USER_LIMITS_SUPPORTED
	png_set_user_limits(png_ptr, MAXVIDWIDTH, MAXVIDHEIGHT);
#endif

	//png_set_filter(png_ptr, 0, PNG_ALL_FILTERS);

	png_set_compression_level(png_ptr, cv_zlib_level.value);
	png_set_compression_mem_level(png_ptr, cv_zlib_memory.value);
	png_set_compression_strategy(png_ptr, cv_zlib_strategy.value);
	png_set_compression_window_bits(png_ptr, cv_zlib_window_bits.value);

	M_PNGhdr(png_ptr, png_info_ptr, width, height, PLTE);

	M_PNGText(png_ptr, png_info_ptr, false);

	png_write_info(png_ptr, png_info_ptr);

	M_PNGImage(png_ptr, png_info_ptr, height, data);

	png_write_end(png_ptr, png_info_ptr);
	png_destroy_write_struct(&png_ptr, &png_info_ptr);

	fclose(png_FILE);
	return true;
}
#else
/** PCX file structure.
  */
typedef struct
{
	UINT8 manufacturer;
	UINT8 version;
	UINT8 encoding;
	UINT8 bits_per_pixel;

	UINT16 xmin, ymin;
	UINT16 xmax, ymax;
	UINT16 hres, vres;
	UINT8  palette[48];

	UINT8 reserved;
	UINT8 color_planes;
	UINT16 bytes_per_line;
	UINT16 palette_type;

	char filler[58];
	UINT8 data; ///< Unbounded; used for all picture data.
} pcx_t;

/** Writes a PCX file to disk.
  *
  * \param filename Filename to write to.
  * \param data     The image data.
  * \param width    Width of the picture.
  * \param height   Height of the picture.
  * \param palette  Palette of image data
  */
#if NUMSCREENS > 2
static boolean WritePCXfile(const char *filename, const UINT8 *data, int width, int height, const UINT8 *palette)
{
	int i;
	size_t length;
	pcx_t *pcx;
	UINT8 *pack;

	pcx = Z_Malloc(width*height*2 + 1000, PU_STATIC, NULL);

	pcx->manufacturer = 0x0a; // PCX id
	pcx->version = 5; // 256 color
	pcx->encoding = 1; // uncompressed
	pcx->bits_per_pixel = 8; // 256 color
	pcx->xmin = pcx->ymin = 0;
	pcx->xmax = SHORT(width - 1);
	pcx->ymax = SHORT(height - 1);
	pcx->hres = SHORT(width);
	pcx->vres = SHORT(height);
	memset(pcx->palette, 0, sizeof (pcx->palette));
	pcx->reserved = 0;
	pcx->color_planes = 1; // chunky image
	pcx->bytes_per_line = SHORT(width);
	pcx->palette_type = SHORT(1); // not a grey scale
	memset(pcx->filler, 0, sizeof (pcx->filler));

	// pack the image
	pack = &pcx->data;

	for (i = 0; i < width*height; i++)
	{
		if ((*data & 0xc0) != 0xc0)
			*pack++ = *data++;
		else
		{
			*pack++ = 0xc1;
			*pack++ = *data++;
		}
	}

	// write the palette
	*pack++ = 0x0c; // palette ID byte
	for (i = 0; i < 768; i++)
		*pack++ = *palette++;

	// write output file
	length = pack - (UINT8 *)pcx;
	i = FIL_WriteFile(filename, pcx, length);

	Z_Free(pcx);
	return i;
}
#endif
#endif

void M_ScreenShot(void)
{
	takescreenshot = true;
}

/** Takes a screenshot.
  * The screenshot is saved as "srb2xxxx.png" where xxxx is the lowest
  * four-digit number for which a file does not already exist.
  *
  * \sa HWR_ScreenShot
  */
void M_DoScreenShot(void)
{
#if NUMSCREENS > 2
	const char *freename = NULL, *pathname = ".";
	boolean ret = false;
	UINT8 *linear = NULL;

	// Don't take multiple screenshots, obviously
	takescreenshot = false;

	// how does one take a screenshot without a render system?
	if (rendermode == render_none)
		return;

	if (cv_screenshot_option.value == 0)
		pathname = usehome ? srb2home : srb2path;
	else if (cv_screenshot_option.value == 1)
		pathname = srb2home;
	else if (cv_screenshot_option.value == 2)
		pathname = srb2path;
	else if (cv_screenshot_option.value == 3 && *cv_screenshot_folder.string != '\0')
		pathname = cv_screenshot_folder.string;

#ifdef USE_PNG
	freename = Newsnapshotfile(pathname,"png");
#else
	if (rendermode == render_soft)
		freename = Newsnapshotfile(pathname,"pcx");
	else if (rendermode == render_opengl)
		freename = Newsnapshotfile(pathname,"tga");
#endif

	if (rendermode == render_soft)
	{
		// munge planar buffer to linear
		linear = screens[2];
		I_ReadScreen(linear);
	}

	if (!freename)
		goto failure;

	// save the pcx file
#ifdef HWRENDER
	if (rendermode == render_opengl)
		ret = HWR_Screenshot(va(pandf,pathname,freename));
	else
#endif
	{
		M_CreateScreenShotPalette();
#ifdef USE_PNG
		ret = M_SavePNG(va(pandf,pathname,freename), linear, vid.width, vid.height, screenshot_palette);
#else
		ret = WritePCXfile(va(pandf,pathname,freename), linear, vid.width, vid.height, screenshot_palette);
#endif
	}

failure:
	if (ret)
	{
		if (moviemode != MM_SCREENSHOT)
			CONS_Printf(M_GetText("Screen shot %s saved in %s\n"), freename, pathname);
	}
	else
	{
		if (freename)
			CONS_Alert(CONS_ERROR, M_GetText("Couldn't create screen shot %s in %s\n"), freename, pathname);
		else
			CONS_Alert(CONS_ERROR, M_GetText("Couldn't create screen shot in %s (all 10000 slots used!)\n"), pathname);

		if (moviemode == MM_SCREENSHOT)
			M_StopMovie();
	}
#endif
}

boolean M_ScreenshotResponder(event_t *ev)
{
	INT32 ch = -1;
	if (dedicated || ev->type != ev_keydown)
		return false;

	ch = ev->data1;

	if (ch >= KEY_MOUSE1 && menuactive) // If it's not a keyboard key, then don't allow it in the menus!
		return false;

	if (ch == KEY_F8 || ch == gamecontrol[gc_screenshot][0] || ch == gamecontrol[gc_screenshot][1]) // remappable F8
		M_ScreenShot();
	else if (ch == KEY_F9 || ch == gamecontrol[gc_recordgif][0] || ch == gamecontrol[gc_recordgif][1]) // remappable F9
		((moviemode) ? M_StopMovie : M_StartMovie)();
	else
		return false;
	return true;
}

// ==========================================================================
//                       TRANSLATION FUNCTIONS
// ==========================================================================

// M_StartupLocale.
// Sets up gettext to translate SRB2's strings.
#ifdef GETTEXT
#if defined (__unix__) || defined(__APPLE__) || defined (UNIXCOMMON)
#define GETTEXTDOMAIN1 "/usr/share/locale"
#define GETTEXTDOMAIN2 "/usr/local/share/locale"
#elif defined (_WIN32)
#define GETTEXTDOMAIN1 "."
#endif

void M_StartupLocale(void)
{
	char *textdomhandle = NULL;

	CONS_Printf("M_StartupLocale...\n");

	setlocale(LC_ALL, "");

	// Do not set numeric locale as that affects atof
	setlocale(LC_NUMERIC, "C");

	// FIXME: global name define anywhere?
#ifdef GETTEXTDOMAIN1
	textdomhandle = bindtextdomain("srb2", GETTEXTDOMAIN1);
#endif
#ifdef GETTEXTDOMAIN2
	if (!textdomhandle)
		textdomhandle = bindtextdomain("srb2", GETTEXTDOMAIN2);
#endif
#ifdef GETTEXTDOMAIN3
	if (!textdomhandle)
		textdomhandle = bindtextdomain("srb2", GETTEXTDOMAIN3);
#endif
#ifdef GETTEXTDOMAIN4
	if (!textdomhandle)
		textdomhandle = bindtextdomain("srb2", GETTEXTDOMAIN4);
#endif
	if (textdomhandle)
		textdomain("srb2");
	else
		CONS_Printf("Could not find locale text domain!\n");
}
#endif

// ==========================================================================
//                        MISC STRING FUNCTIONS
// ==========================================================================

/** Returns a temporary string made out of varargs.
  * For use with CONS_Printf().
  *
  * \param format Format string.
  * \return Pointer to a static buffer of 1024 characters, containing the
  *         resulting string.
  */
char *va(const char *format, ...)
{
	va_list argptr;
	static char string[1024];

	va_start(argptr, format);
	vsnprintf(string, 1024, format, argptr);
	va_end(argptr);

	return string;
}

/** Creates a string in the first argument that is the second argument followed
  * by the third argument followed by the first argument.
  * Useful for making filenames with full path. s1 = s2+s3+s1
  *
  * \param s1 First string, suffix, and destination.
  * \param s2 Second string. Ends up first in the result.
  * \param s3 Third string. Ends up second in the result.
  */
void strcatbf(char *s1, const char *s2, const char *s3)
{
	char tmp[1024];

	strcpy(tmp, s1);
	strcpy(s1, s2);
	strcat(s1, s3);
	strcat(s1, tmp);
}

/** Converts an ASCII Hex string into an integer. Thanks, Borland!
  * <Inuyasha> I don't know if this belongs here specifically, but it sure
  *            doesn't belong in p_spec.c, that's for sure
  *
  * \param hexStg Hexadecimal string.
  * \return an Integer based off the contents of the string.
  */
INT32 axtoi(const char *hexStg)
{
	INT32 n = 0;
	INT32 m = 0;
	INT32 count;
	INT32 intValue = 0;
	INT32 digit[8];
	while (n < 8)
	{
		if (hexStg[n] == '\0')
			break;
		if (hexStg[n] >= '0' && hexStg[n] <= '9') // 0-9
			digit[n] = (hexStg[n] & 0x0f);
		else if (hexStg[n] >= 'a' && hexStg[n] <= 'f') // a-f
			digit[n] = (hexStg[n] & 0x0f) + 9;
		else if (hexStg[n] >= 'A' && hexStg[n] <= 'F') // A-F
			digit[n] = (hexStg[n] & 0x0f) + 9;
		else
			break;
		n++;
	}
	count = n;
	m = n - 1;
	n = 0;
	while (n < count)
	{
		intValue = intValue | (digit[n] << (m << 2));
		m--;
		n++;
	}
	return intValue;
}

/** Token parser for TEXTURES, ANIMDEFS, and potentially other lumps later down the line.
  * Was originally R_GetTexturesToken when I was coding up the TEXTURES parser, until I realized I needed it for ANIMDEFS too.
  * Parses up to the next whitespace character or comma. When finding the start of the next token, whitespace is skipped.
  * Commas are not; if a comma is encountered, then THAT'S returned as the token.
  * -Shadow Hog
  *
  * \param inputString The string to be parsed. If NULL is supplied instead of a string, it will continue parsing the last supplied one.
  * The pointer to the last string supplied is stored as a static variable, so be careful not to free it while this function is still using it!
  * \return A pointer to a string, containing the fetched token. This is in freshly allocated memory, so be sure to Z_Free() it as appropriate.
*/
char *M_GetToken(const char *inputString)
{
	static const char *stringToUse = NULL; // Populated if inputString != NULL; used otherwise
	static UINT32 startPos = 0;
	static UINT32 endPos = 0;
	static UINT32 stringLength = 0;
	static UINT8 inComment = 0; // 0 = not in comment, 1 = // Single-line, 2 = /* Multi-line */
	char *texturesToken = NULL;
	UINT32 texturesTokenLength = 0;

	if (inputString != NULL)
	{
		stringToUse = inputString;
		startPos = 0;
		endPos = 0;
		stringLength = strlen(inputString);
	}
	else
	{
		startPos = endPos;
	}
	if (stringToUse == NULL)
		return NULL;

	// Try to detect comments now, in case we're pointing right at one
	if (startPos < stringLength - 1
		&& inComment == 0)
	{
		if (stringToUse[startPos] == '/'
			&& stringToUse[startPos+1] == '/')
		{
			//Single-line comment start
			inComment = 1;
		}
		else if (stringToUse[startPos] == '/'
			&& stringToUse[startPos+1] == '*')
		{
			//Multi-line comment start
			inComment = 2;
		}
	}

	// Find the first non-whitespace char, or else the end of the string trying
	while ((stringToUse[startPos] == ' '
			|| stringToUse[startPos] == '\t'
			|| stringToUse[startPos] == '\r'
			|| stringToUse[startPos] == '\n'
			|| stringToUse[startPos] == '\0'
			|| stringToUse[startPos] == '"' // we're treating this as whitespace because SLADE likes adding it for no good reason
			|| inComment != 0)
			&& startPos < stringLength)
	{
		// Try to detect comment endings now
		if (inComment == 1
			&& stringToUse[startPos] == '\n')
		{
			// End of line for a single-line comment
			inComment = 0;
		}
		else if (inComment == 2
			&& startPos < stringLength - 1
			&& stringToUse[startPos] == '*'
			&& stringToUse[startPos+1] == '/')
		{
			// End of multi-line comment
			inComment = 0;
			startPos++; // Make damn well sure we're out of the comment ending at the end of it all
		}

		startPos++;

		// Try to detect comment starts now
		if (startPos < stringLength - 1
			&& inComment == 0)
		{
			if (stringToUse[startPos] == '/'
				&& stringToUse[startPos+1] == '/')
			{
				//Single-line comment start
				inComment = 1;
			}
			else if (stringToUse[startPos] == '/'
				&& stringToUse[startPos+1] == '*')
			{
				//Multi-line comment start
				inComment = 2;
			}
		}
	}

	// If the end of the string is reached, no token is to be read
	if (startPos == stringLength) {
		endPos = stringLength;
		return NULL;
	}
	// Else, if it's one of these three symbols, capture only this one character
	else if (stringToUse[startPos] == ','
			|| stringToUse[startPos] == '{'
			|| stringToUse[startPos] == '}')
	{
		endPos = startPos + 1;
		texturesToken = (char *)Z_Malloc(2*sizeof(char),PU_STATIC,NULL);
		texturesToken[0] = stringToUse[startPos];
		texturesToken[1] = '\0';
		return texturesToken;
	}

	// Now find the end of the token. This includes several additional characters that are okay to capture as one character, but not trailing at the end of another token.
	endPos = startPos + 1;
	while ((stringToUse[endPos] != ' '
			&& stringToUse[endPos] != '\t'
			&& stringToUse[endPos] != '\r'
			&& stringToUse[endPos] != '\n'
			&& stringToUse[endPos] != ','
			&& stringToUse[endPos] != '{'
			&& stringToUse[endPos] != '}'
			&& stringToUse[endPos] != '"' // see above
			&& inComment == 0)
			&& endPos < stringLength)
	{
		endPos++;
		// Try to detect comment starts now; if it's in a comment, we don't want it in this token
		if (endPos < stringLength - 1
			&& inComment == 0)
		{
			if (stringToUse[endPos] == '/'
				&& stringToUse[endPos+1] == '/')
			{
				//Single-line comment start
				inComment = 1;
			}
			else if (stringToUse[endPos] == '/'
				&& stringToUse[endPos+1] == '*')
			{
				//Multi-line comment start
				inComment = 2;
			}
		}
	}
	texturesTokenLength = endPos - startPos;

	// Assign the memory. Don't forget an extra byte for the end of the string!
	texturesToken = (char *)Z_Malloc((texturesTokenLength+1)*sizeof(char),PU_STATIC,NULL);
	// Copy the string.
	M_Memcpy(texturesToken, stringToUse+startPos, (size_t)texturesTokenLength);
	// Make the final character NUL.
	texturesToken[texturesTokenLength] = '\0';
	return texturesToken;
}

/** Count bits in a number.
  */
UINT8 M_CountBits(UINT32 num, UINT8 size)
{
	UINT8 i, sum = 0;

	for (i = 0; i < size; ++i)
		if (num & (1 << i))
			++sum;
	return sum;
}

const char *GetRevisionString(void)
{
	static char rev[9] = {0};
	if (rev[0])
		return rev;

	if (comprevision[0] == 'r')
		strncpy(rev, comprevision, 7);
	else
		snprintf(rev, 7, "r%s", comprevision);
	rev[7] = '\0';

	return rev;
}

// Vector/matrix math
TVector *VectorMatrixMultiply(TVector v, TMatrix m)
{
	static TVector ret;

	ret[0] = FixedMul(v[0],m[0][0]) + FixedMul(v[1],m[1][0]) + FixedMul(v[2],m[2][0]) + FixedMul(v[3],m[3][0]);
	ret[1] = FixedMul(v[0],m[0][1]) + FixedMul(v[1],m[1][1]) + FixedMul(v[2],m[2][1]) + FixedMul(v[3],m[3][1]);
	ret[2] = FixedMul(v[0],m[0][2]) + FixedMul(v[1],m[1][2]) + FixedMul(v[2],m[2][2]) + FixedMul(v[3],m[3][2]);
	ret[3] = FixedMul(v[0],m[0][3]) + FixedMul(v[1],m[1][3]) + FixedMul(v[2],m[2][3]) + FixedMul(v[3],m[3][3]);

	return &ret;
}

TMatrix *RotateXMatrix(angle_t rad)
{
	static TMatrix ret;
	const angle_t fa = rad>>ANGLETOFINESHIFT;
	const fixed_t cosrad = FINECOSINE(fa), sinrad = FINESINE(fa);

	ret[0][0] = FRACUNIT; ret[0][1] =       0; ret[0][2] = 0;        ret[0][3] = 0;
	ret[1][0] =        0; ret[1][1] =  cosrad; ret[1][2] = sinrad;   ret[1][3] = 0;
	ret[2][0] =        0; ret[2][1] = -sinrad; ret[2][2] = cosrad;   ret[2][3] = 0;
	ret[3][0] =        0; ret[3][1] =       0; ret[3][2] = 0;        ret[3][3] = FRACUNIT;

	return &ret;
}

#if 0
TMatrix *RotateYMatrix(angle_t rad)
{
	static TMatrix ret;
	const angle_t fa = rad>>ANGLETOFINESHIFT;
	const fixed_t cosrad = FINECOSINE(fa), sinrad = FINESINE(fa);

	ret[0][0] = cosrad;   ret[0][1] =        0; ret[0][2] = -sinrad;   ret[0][3] = 0;
	ret[1][0] = 0;        ret[1][1] = FRACUNIT; ret[1][2] = 0;         ret[1][3] = 0;
	ret[2][0] = sinrad;   ret[2][1] =        0; ret[2][2] = cosrad;    ret[2][3] = 0;
	ret[3][0] = 0;        ret[3][1] =        0; ret[3][2] = 0;         ret[3][3] = FRACUNIT;

	return &ret;
}
#endif

TMatrix *RotateZMatrix(angle_t rad)
{
	static TMatrix ret;
	const angle_t fa = rad>>ANGLETOFINESHIFT;
	const fixed_t cosrad = FINECOSINE(fa), sinrad = FINESINE(fa);

	ret[0][0] = cosrad;    ret[0][1] = sinrad;   ret[0][2] =        0; ret[0][3] = 0;
	ret[1][0] = -sinrad;   ret[1][1] = cosrad;   ret[1][2] =        0; ret[1][3] = 0;
	ret[2][0] = 0;         ret[2][1] = 0;        ret[2][2] = FRACUNIT; ret[2][3] = 0;
	ret[3][0] = 0;         ret[3][1] = 0;        ret[3][2] =        0; ret[3][3] = FRACUNIT;

	return &ret;
}

/** Set of functions to take in a size_t as an argument,
  * put the argument in a character buffer, and return the
  * pointer to that buffer.
  * This is to eliminate usage of PRIdS, so gettext can work
  * with *all* of SRB2's strings.
  */
char *sizeu1(size_t num)
{
	static char sizeu1_buf[28];
	sprintf(sizeu1_buf, "%"PRIdS, num);
	return sizeu1_buf;
}

char *sizeu2(size_t num)
{
	static char sizeu2_buf[28];
	sprintf(sizeu2_buf, "%"PRIdS, num);
	return sizeu2_buf;
}

char *sizeu3(size_t num)
{
	static char sizeu3_buf[28];
	sprintf(sizeu3_buf, "%"PRIdS, num);
	return sizeu3_buf;
}

char *sizeu4(size_t num)
{
	static char sizeu4_buf[28];
	sprintf(sizeu4_buf, "%"PRIdS, num);
	return sizeu4_buf;
}

char *sizeu5(size_t num)
{
	static char sizeu5_buf[28];
	sprintf(sizeu5_buf, "%"PRIdS, num);
	return sizeu5_buf;
}

/*void *M_Memcpy(void *dest, const void *src, size_t n)
{
	return memcpy(dest, src, n);
}*/

void *M_Memcpy(void *dest, const void *src, size_t n)
#if defined(_WIN32) && defined(__MINGW64__)
{
	return memcpy_fast(dest, src, n);
}
#else
{
	return memcpy(dest, src, n);
}
#endif


#if defined(_WIN32) && defined(__MINGW64__)

#include <stddef.h>
#include <stdint.h>
#include <emmintrin.h>

//---------------------------------------------------------------------
// force inline for compilers
//---------------------------------------------------------------------
#ifndef INLINE
#ifdef __GNUC__
#if (__GNUC__ > 3) || ((__GNUC__ == 3) && (__GNUC_MINOR__ >= 1))
    #define INLINE         __inline__ __attribute__((always_inline))
#else
    #define INLINE         __inline__
#endif
#elif defined(_MSC_VER)
	#define INLINE __forceinline
#elif (defined(__BORLANDC__) || defined(__WATCOMC__))
    #define INLINE __inline
#else
    #define INLINE 
#endif
#endif

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough"

//---------------------------------------------------------------------
// fast copy for different sizes
//---------------------------------------------------------------------
static INLINE void memcpy_avx_16(void *dst, const void *src) {
#if 1
	__m128i m0 = _mm_loadu_si128(((const __m128i*)src) + 0);
	_mm_storeu_si128(((__m128i*)dst) + 0, m0);
#else
	*((uint64_t*)((char*)dst + 0)) = *((uint64_t*)((const char*)src + 0));
	*((uint64_t*)((char*)dst + 8)) = *((uint64_t*)((const char*)src + 8));
#endif
}

static INLINE void memcpy_avx_32(void *dst, const void *src) {
	__m256i m0 = _mm256_loadu_si256(((const __m256i*)src) + 0);
	_mm256_storeu_si256(((__m256i*)dst) + 0, m0);
}

static INLINE void memcpy_avx_64(void *dst, const void *src) {
	__m256i m0 = _mm256_loadu_si256(((const __m256i*)src) + 0);
	__m256i m1 = _mm256_loadu_si256(((const __m256i*)src) + 1);
	_mm256_storeu_si256(((__m256i*)dst) + 0, m0);
	_mm256_storeu_si256(((__m256i*)dst) + 1, m1);
}

static INLINE void memcpy_avx_128(void *dst, const void *src) {
	__m256i m0 = _mm256_loadu_si256(((const __m256i*)src) + 0);
	__m256i m1 = _mm256_loadu_si256(((const __m256i*)src) + 1);
	__m256i m2 = _mm256_loadu_si256(((const __m256i*)src) + 2);
	__m256i m3 = _mm256_loadu_si256(((const __m256i*)src) + 3);
	_mm256_storeu_si256(((__m256i*)dst) + 0, m0);
	_mm256_storeu_si256(((__m256i*)dst) + 1, m1);
	_mm256_storeu_si256(((__m256i*)dst) + 2, m2);
	_mm256_storeu_si256(((__m256i*)dst) + 3, m3);
}

static INLINE void memcpy_avx_256(void *dst, const void *src) {
	__m256i m0 = _mm256_loadu_si256(((const __m256i*)src) + 0);
	__m256i m1 = _mm256_loadu_si256(((const __m256i*)src) + 1);
	__m256i m2 = _mm256_loadu_si256(((const __m256i*)src) + 2);
	__m256i m3 = _mm256_loadu_si256(((const __m256i*)src) + 3);
	__m256i m4 = _mm256_loadu_si256(((const __m256i*)src) + 4);
	__m256i m5 = _mm256_loadu_si256(((const __m256i*)src) + 5);
	__m256i m6 = _mm256_loadu_si256(((const __m256i*)src) + 6);
	__m256i m7 = _mm256_loadu_si256(((const __m256i*)src) + 7);
	_mm256_storeu_si256(((__m256i*)dst) + 0, m0);
	_mm256_storeu_si256(((__m256i*)dst) + 1, m1);
	_mm256_storeu_si256(((__m256i*)dst) + 2, m2);
	_mm256_storeu_si256(((__m256i*)dst) + 3, m3);
	_mm256_storeu_si256(((__m256i*)dst) + 4, m4);
	_mm256_storeu_si256(((__m256i*)dst) + 5, m5);
	_mm256_storeu_si256(((__m256i*)dst) + 6, m6);
	_mm256_storeu_si256(((__m256i*)dst) + 7, m7);
}


//---------------------------------------------------------------------
// tiny memory copy with jump table optimized
//---------------------------------------------------------------------
static INLINE void *memcpy_tiny(void *dst, const void *src, size_t size) {
	unsigned char *dd = ((unsigned char*)dst) + size;
	const unsigned char *ss = ((const unsigned char*)src) + size;

	switch (size) { 
	case 128: memcpy_avx_128(dd - 128, ss - 128);
	case 0:  break;
	case 129: memcpy_avx_128(dd - 129, ss - 129);
	case 1: dd[-1] = ss[-1]; break;
	case 130: memcpy_avx_128(dd - 130, ss - 130);
	case 2: *((uint16_t*)(dd - 2)) = *((uint16_t*)(ss - 2)); break;
	case 131: memcpy_avx_128(dd - 131, ss - 131);
	case 3: *((uint16_t*)(dd - 3)) = *((uint16_t*)(ss - 3)); dd[-1] = ss[-1]; break;
	case 132: memcpy_avx_128(dd - 132, ss - 132);
	case 4: *((uint32_t*)(dd - 4)) = *((uint32_t*)(ss - 4)); break;
	case 133: memcpy_avx_128(dd - 133, ss - 133);
	case 5: *((uint32_t*)(dd - 5)) = *((uint32_t*)(ss - 5)); dd[-1] = ss[-1]; break;
	case 134: memcpy_avx_128(dd - 134, ss - 134);
	case 6: *((uint32_t*)(dd - 6)) = *((uint32_t*)(ss - 6)); *((uint16_t*)(dd - 2)) = *((uint16_t*)(ss - 2)); break;
	case 135: memcpy_avx_128(dd - 135, ss - 135);
	case 7: *((uint32_t*)(dd - 7)) = *((uint32_t*)(ss - 7)); *((uint32_t*)(dd - 4)) = *((uint32_t*)(ss - 4)); break;
	case 136: memcpy_avx_128(dd - 136, ss - 136);
	case 8: *((uint64_t*)(dd - 8)) = *((uint64_t*)(ss - 8)); break;
	case 137: memcpy_avx_128(dd - 137, ss - 137);
	case 9: *((uint64_t*)(dd - 9)) = *((uint64_t*)(ss - 9)); dd[-1] = ss[-1]; break;
	case 138: memcpy_avx_128(dd - 138, ss - 138);
	case 10: *((uint64_t*)(dd - 10)) = *((uint64_t*)(ss - 10)); *((uint16_t*)(dd - 2)) = *((uint16_t*)(ss - 2)); break;
	case 139: memcpy_avx_128(dd - 139, ss - 139);
	case 11: *((uint64_t*)(dd - 11)) = *((uint64_t*)(ss - 11)); *((uint32_t*)(dd - 4)) = *((uint32_t*)(ss - 4)); break;
	case 140: memcpy_avx_128(dd - 140, ss - 140);
	case 12: *((uint64_t*)(dd - 12)) = *((uint64_t*)(ss - 12)); *((uint32_t*)(dd - 4)) = *((uint32_t*)(ss - 4)); break;
	case 141: memcpy_avx_128(dd - 141, ss - 141);
	case 13: *((uint64_t*)(dd - 13)) = *((uint64_t*)(ss - 13)); *((uint64_t*)(dd - 8)) = *((uint64_t*)(ss - 8)); break;
	case 142: memcpy_avx_128(dd - 142, ss - 142);
	case 14: *((uint64_t*)(dd - 14)) = *((uint64_t*)(ss - 14)); *((uint64_t*)(dd - 8)) = *((uint64_t*)(ss - 8)); break;
	case 143: memcpy_avx_128(dd - 143, ss - 143);
	case 15: *((uint64_t*)(dd - 15)) = *((uint64_t*)(ss - 15)); *((uint64_t*)(dd - 8)) = *((uint64_t*)(ss - 8)); break;
	case 144: memcpy_avx_128(dd - 144, ss - 144);
	case 16: memcpy_avx_16(dd - 16, ss - 16); break;
	case 145: memcpy_avx_128(dd - 145, ss - 145);
	case 17: memcpy_avx_16(dd - 17, ss - 17); dd[-1] = ss[-1]; break;
	case 146: memcpy_avx_128(dd - 146, ss - 146);
	case 18: memcpy_avx_16(dd - 18, ss - 18); *((uint16_t*)(dd - 2)) = *((uint16_t*)(ss - 2)); break;
	case 147: memcpy_avx_128(dd - 147, ss - 147);
	case 19: memcpy_avx_16(dd - 19, ss - 19); *((uint32_t*)(dd - 4)) = *((uint32_t*)(ss - 4)); break;
	case 148: memcpy_avx_128(dd - 148, ss - 148);
	case 20: memcpy_avx_16(dd - 20, ss - 20); *((uint32_t*)(dd - 4)) = *((uint32_t*)(ss - 4)); break;
	case 149: memcpy_avx_128(dd - 149, ss - 149);
	case 21: memcpy_avx_16(dd - 21, ss - 21); *((uint64_t*)(dd - 8)) = *((uint64_t*)(ss - 8)); break;
	case 150: memcpy_avx_128(dd - 150, ss - 150);
	case 22: memcpy_avx_16(dd - 22, ss - 22); *((uint64_t*)(dd - 8)) = *((uint64_t*)(ss - 8)); break;
	case 151: memcpy_avx_128(dd - 151, ss - 151);
	case 23: memcpy_avx_16(dd - 23, ss - 23); *((uint64_t*)(dd - 8)) = *((uint64_t*)(ss - 8)); break;
	case 152: memcpy_avx_128(dd - 152, ss - 152);
	case 24: memcpy_avx_16(dd - 24, ss - 24); *((uint64_t*)(dd - 8)) = *((uint64_t*)(ss - 8)); break;
	case 153: memcpy_avx_128(dd - 153, ss - 153);
	case 25: memcpy_avx_16(dd - 25, ss - 25); memcpy_avx_16(dd - 16, ss - 16); break;
	case 154: memcpy_avx_128(dd - 154, ss - 154);
	case 26: memcpy_avx_16(dd - 26, ss - 26); memcpy_avx_16(dd - 16, ss - 16); break;
	case 155: memcpy_avx_128(dd - 155, ss - 155);
	case 27: memcpy_avx_16(dd - 27, ss - 27); memcpy_avx_16(dd - 16, ss - 16); break;
	case 156: memcpy_avx_128(dd - 156, ss - 156);
	case 28: memcpy_avx_16(dd - 28, ss - 28); memcpy_avx_16(dd - 16, ss - 16); break;
	case 157: memcpy_avx_128(dd - 157, ss - 157);
	case 29: memcpy_avx_16(dd - 29, ss - 29); memcpy_avx_16(dd - 16, ss - 16); break;
	case 158: memcpy_avx_128(dd - 158, ss - 158);
	case 30: memcpy_avx_16(dd - 30, ss - 30); memcpy_avx_16(dd - 16, ss - 16); break;
	case 159: memcpy_avx_128(dd - 159, ss - 159);
	case 31: memcpy_avx_16(dd - 31, ss - 31); memcpy_avx_16(dd - 16, ss - 16); break;
	case 160: memcpy_avx_128(dd - 160, ss - 160);
	case 32: memcpy_avx_32(dd - 32, ss - 32); break;
	case 161: memcpy_avx_128(dd - 161, ss - 161);
	case 33: memcpy_avx_32(dd - 33, ss - 33); dd[-1] = ss[-1]; break;
	case 162: memcpy_avx_128(dd - 162, ss - 162);
	case 34: memcpy_avx_32(dd - 34, ss - 34); *((uint16_t*)(dd - 2)) = *((uint16_t*)(ss - 2)); break;
	case 163: memcpy_avx_128(dd - 163, ss - 163);
	case 35: memcpy_avx_32(dd - 35, ss - 35); *((uint32_t*)(dd - 4)) = *((uint32_t*)(ss - 4)); break;
	case 164: memcpy_avx_128(dd - 164, ss - 164);
	case 36: memcpy_avx_32(dd - 36, ss - 36); *((uint32_t*)(dd - 4)) = *((uint32_t*)(ss - 4)); break;
	case 165: memcpy_avx_128(dd - 165, ss - 165);
	case 37: memcpy_avx_32(dd - 37, ss - 37); *((uint64_t*)(dd - 8)) = *((uint64_t*)(ss - 8)); break;
	case 166: memcpy_avx_128(dd - 166, ss - 166);
	case 38: memcpy_avx_32(dd - 38, ss - 38); *((uint64_t*)(dd - 8)) = *((uint64_t*)(ss - 8)); break;
	case 167: memcpy_avx_128(dd - 167, ss - 167);
	case 39: memcpy_avx_32(dd - 39, ss - 39); *((uint64_t*)(dd - 8)) = *((uint64_t*)(ss - 8)); break;
	case 168: memcpy_avx_128(dd - 168, ss - 168);
	case 40: memcpy_avx_32(dd - 40, ss - 40); *((uint64_t*)(dd - 8)) = *((uint64_t*)(ss - 8)); break;
	case 169: memcpy_avx_128(dd - 169, ss - 169);
	case 41: memcpy_avx_32(dd - 41, ss - 41); memcpy_avx_16(dd - 16, ss - 16); break;
	case 170: memcpy_avx_128(dd - 170, ss - 170);
	case 42: memcpy_avx_32(dd - 42, ss - 42); memcpy_avx_16(dd - 16, ss - 16); break;
	case 171: memcpy_avx_128(dd - 171, ss - 171);
	case 43: memcpy_avx_32(dd - 43, ss - 43); memcpy_avx_16(dd - 16, ss - 16); break;
	case 172: memcpy_avx_128(dd - 172, ss - 172);
	case 44: memcpy_avx_32(dd - 44, ss - 44); memcpy_avx_16(dd - 16, ss - 16); break;
	case 173: memcpy_avx_128(dd - 173, ss - 173);
	case 45: memcpy_avx_32(dd - 45, ss - 45); memcpy_avx_16(dd - 16, ss - 16); break;
	case 174: memcpy_avx_128(dd - 174, ss - 174);
	case 46: memcpy_avx_32(dd - 46, ss - 46); memcpy_avx_16(dd - 16, ss - 16); break;
	case 175: memcpy_avx_128(dd - 175, ss - 175);
	case 47: memcpy_avx_32(dd - 47, ss - 47); memcpy_avx_16(dd - 16, ss - 16); break;
	case 176: memcpy_avx_128(dd - 176, ss - 176);
	case 48: memcpy_avx_32(dd - 48, ss - 48); memcpy_avx_16(dd - 16, ss - 16); break;
	case 177: memcpy_avx_128(dd - 177, ss - 177);
	case 49: memcpy_avx_32(dd - 49, ss - 49); memcpy_avx_32(dd - 32, ss - 32); break;
	case 178: memcpy_avx_128(dd - 178, ss - 178);
	case 50: memcpy_avx_32(dd - 50, ss - 50); memcpy_avx_32(dd - 32, ss - 32); break;
	case 179: memcpy_avx_128(dd - 179, ss - 179);
	case 51: memcpy_avx_32(dd - 51, ss - 51); memcpy_avx_32(dd - 32, ss - 32); break;
	case 180: memcpy_avx_128(dd - 180, ss - 180);
	case 52: memcpy_avx_32(dd - 52, ss - 52); memcpy_avx_32(dd - 32, ss - 32); break;
	case 181: memcpy_avx_128(dd - 181, ss - 181);
	case 53: memcpy_avx_32(dd - 53, ss - 53); memcpy_avx_32(dd - 32, ss - 32); break;
	case 182: memcpy_avx_128(dd - 182, ss - 182);
	case 54: memcpy_avx_32(dd - 54, ss - 54); memcpy_avx_32(dd - 32, ss - 32); break;
	case 183: memcpy_avx_128(dd - 183, ss - 183);
	case 55: memcpy_avx_32(dd - 55, ss - 55); memcpy_avx_32(dd - 32, ss - 32); break;
	case 184: memcpy_avx_128(dd - 184, ss - 184);
	case 56: memcpy_avx_32(dd - 56, ss - 56); memcpy_avx_32(dd - 32, ss - 32); break;
	case 185: memcpy_avx_128(dd - 185, ss - 185);
	case 57: memcpy_avx_32(dd - 57, ss - 57); memcpy_avx_32(dd - 32, ss - 32); break;
	case 186: memcpy_avx_128(dd - 186, ss - 186);
	case 58: memcpy_avx_32(dd - 58, ss - 58); memcpy_avx_32(dd - 32, ss - 32); break;
	case 187: memcpy_avx_128(dd - 187, ss - 187);
	case 59: memcpy_avx_32(dd - 59, ss - 59); memcpy_avx_32(dd - 32, ss - 32); break;
	case 188: memcpy_avx_128(dd - 188, ss - 188);
	case 60: memcpy_avx_32(dd - 60, ss - 60); memcpy_avx_32(dd - 32, ss - 32); break;
	case 189: memcpy_avx_128(dd - 189, ss - 189);
	case 61: memcpy_avx_32(dd - 61, ss - 61); memcpy_avx_32(dd - 32, ss - 32); break;
	case 190: memcpy_avx_128(dd - 190, ss - 190);
	case 62: memcpy_avx_32(dd - 62, ss - 62); memcpy_avx_32(dd - 32, ss - 32); break;
	case 191: memcpy_avx_128(dd - 191, ss - 191);
	case 63: memcpy_avx_32(dd - 63, ss - 63); memcpy_avx_32(dd - 32, ss - 32); break;
	case 192: memcpy_avx_128(dd - 192, ss - 192);
	case 64: memcpy_avx_64(dd - 64, ss - 64); break;
	case 193: memcpy_avx_128(dd - 193, ss - 193);
	case 65: memcpy_avx_64(dd - 65, ss - 65); dd[-1] = ss[-1]; break;
	case 194: memcpy_avx_128(dd - 194, ss - 194);
	case 66: memcpy_avx_64(dd - 66, ss - 66); *((uint16_t*)(dd - 2)) = *((uint16_t*)(ss - 2)); break;
	case 195: memcpy_avx_128(dd - 195, ss - 195);
	case 67: memcpy_avx_64(dd - 67, ss - 67); *((uint32_t*)(dd - 4)) = *((uint32_t*)(ss - 4)); break;
	case 196: memcpy_avx_128(dd - 196, ss - 196);
	case 68: memcpy_avx_64(dd - 68, ss - 68); *((uint32_t*)(dd - 4)) = *((uint32_t*)(ss - 4)); break;
	case 197: memcpy_avx_128(dd - 197, ss - 197);
	case 69: memcpy_avx_64(dd - 69, ss - 69); *((uint64_t*)(dd - 8)) = *((uint64_t*)(ss - 8)); break;
	case 198: memcpy_avx_128(dd - 198, ss - 198);
	case 70: memcpy_avx_64(dd - 70, ss - 70); *((uint64_t*)(dd - 8)) = *((uint64_t*)(ss - 8)); break;
	case 199: memcpy_avx_128(dd - 199, ss - 199);
	case 71: memcpy_avx_64(dd - 71, ss - 71); *((uint64_t*)(dd - 8)) = *((uint64_t*)(ss - 8)); break;
	case 200: memcpy_avx_128(dd - 200, ss - 200);
	case 72: memcpy_avx_64(dd - 72, ss - 72); *((uint64_t*)(dd - 8)) = *((uint64_t*)(ss - 8)); break;
	case 201: memcpy_avx_128(dd - 201, ss - 201);
	case 73: memcpy_avx_64(dd - 73, ss - 73); memcpy_avx_16(dd - 16, ss - 16); break;
	case 202: memcpy_avx_128(dd - 202, ss - 202);
	case 74: memcpy_avx_64(dd - 74, ss - 74); memcpy_avx_16(dd - 16, ss - 16); break;
	case 203: memcpy_avx_128(dd - 203, ss - 203);
	case 75: memcpy_avx_64(dd - 75, ss - 75); memcpy_avx_16(dd - 16, ss - 16); break;
	case 204: memcpy_avx_128(dd - 204, ss - 204);
	case 76: memcpy_avx_64(dd - 76, ss - 76); memcpy_avx_16(dd - 16, ss - 16); break;
	case 205: memcpy_avx_128(dd - 205, ss - 205);
	case 77: memcpy_avx_64(dd - 77, ss - 77); memcpy_avx_16(dd - 16, ss - 16); break;
	case 206: memcpy_avx_128(dd - 206, ss - 206);
	case 78: memcpy_avx_64(dd - 78, ss - 78); memcpy_avx_16(dd - 16, ss - 16); break;
	case 207: memcpy_avx_128(dd - 207, ss - 207);
	case 79: memcpy_avx_64(dd - 79, ss - 79); memcpy_avx_16(dd - 16, ss - 16); break;
	case 208: memcpy_avx_128(dd - 208, ss - 208);
	case 80: memcpy_avx_64(dd - 80, ss - 80); memcpy_avx_16(dd - 16, ss - 16); break;
	case 209: memcpy_avx_128(dd - 209, ss - 209);
	case 81: memcpy_avx_64(dd - 81, ss - 81); memcpy_avx_32(dd - 32, ss - 32); break;
	case 210: memcpy_avx_128(dd - 210, ss - 210);
	case 82: memcpy_avx_64(dd - 82, ss - 82); memcpy_avx_32(dd - 32, ss - 32); break;
	case 211: memcpy_avx_128(dd - 211, ss - 211);
	case 83: memcpy_avx_64(dd - 83, ss - 83); memcpy_avx_32(dd - 32, ss - 32); break;
	case 212: memcpy_avx_128(dd - 212, ss - 212);
	case 84: memcpy_avx_64(dd - 84, ss - 84); memcpy_avx_32(dd - 32, ss - 32); break;
	case 213: memcpy_avx_128(dd - 213, ss - 213);
	case 85: memcpy_avx_64(dd - 85, ss - 85); memcpy_avx_32(dd - 32, ss - 32); break;
	case 214: memcpy_avx_128(dd - 214, ss - 214);
	case 86: memcpy_avx_64(dd - 86, ss - 86); memcpy_avx_32(dd - 32, ss - 32); break;
	case 215: memcpy_avx_128(dd - 215, ss - 215);
	case 87: memcpy_avx_64(dd - 87, ss - 87); memcpy_avx_32(dd - 32, ss - 32); break;
	case 216: memcpy_avx_128(dd - 216, ss - 216);
	case 88: memcpy_avx_64(dd - 88, ss - 88); memcpy_avx_32(dd - 32, ss - 32); break;
	case 217: memcpy_avx_128(dd - 217, ss - 217);
	case 89: memcpy_avx_64(dd - 89, ss - 89); memcpy_avx_32(dd - 32, ss - 32); break;
	case 218: memcpy_avx_128(dd - 218, ss - 218);
	case 90: memcpy_avx_64(dd - 90, ss - 90); memcpy_avx_32(dd - 32, ss - 32); break;
	case 219: memcpy_avx_128(dd - 219, ss - 219);
	case 91: memcpy_avx_64(dd - 91, ss - 91); memcpy_avx_32(dd - 32, ss - 32); break;
	case 220: memcpy_avx_128(dd - 220, ss - 220);
	case 92: memcpy_avx_64(dd - 92, ss - 92); memcpy_avx_32(dd - 32, ss - 32); break;
	case 221: memcpy_avx_128(dd - 221, ss - 221);
	case 93: memcpy_avx_64(dd - 93, ss - 93); memcpy_avx_32(dd - 32, ss - 32); break;
	case 222: memcpy_avx_128(dd - 222, ss - 222);
	case 94: memcpy_avx_64(dd - 94, ss - 94); memcpy_avx_32(dd - 32, ss - 32); break;
	case 223: memcpy_avx_128(dd - 223, ss - 223);
	case 95: memcpy_avx_64(dd - 95, ss - 95); memcpy_avx_32(dd - 32, ss - 32); break;
	case 224: memcpy_avx_128(dd - 224, ss - 224);
	case 96: memcpy_avx_64(dd - 96, ss - 96); memcpy_avx_32(dd - 32, ss - 32); break;
	case 225: memcpy_avx_128(dd - 225, ss - 225);
	case 97: memcpy_avx_64(dd - 97, ss - 97); memcpy_avx_64(dd - 64, ss - 64); break;
	case 226: memcpy_avx_128(dd - 226, ss - 226);
	case 98: memcpy_avx_64(dd - 98, ss - 98); memcpy_avx_64(dd - 64, ss - 64); break;
	case 227: memcpy_avx_128(dd - 227, ss - 227);
	case 99: memcpy_avx_64(dd - 99, ss - 99); memcpy_avx_64(dd - 64, ss - 64); break;
	case 228: memcpy_avx_128(dd - 228, ss - 228);
	case 100: memcpy_avx_64(dd - 100, ss - 100); memcpy_avx_64(dd - 64, ss - 64); break;
	case 229: memcpy_avx_128(dd - 229, ss - 229);
	case 101: memcpy_avx_64(dd - 101, ss - 101); memcpy_avx_64(dd - 64, ss - 64); break;
	case 230: memcpy_avx_128(dd - 230, ss - 230);
	case 102: memcpy_avx_64(dd - 102, ss - 102); memcpy_avx_64(dd - 64, ss - 64); break;
	case 231: memcpy_avx_128(dd - 231, ss - 231);
	case 103: memcpy_avx_64(dd - 103, ss - 103); memcpy_avx_64(dd - 64, ss - 64); break;
	case 232: memcpy_avx_128(dd - 232, ss - 232);
	case 104: memcpy_avx_64(dd - 104, ss - 104); memcpy_avx_64(dd - 64, ss - 64); break;
	case 233: memcpy_avx_128(dd - 233, ss - 233);
	case 105: memcpy_avx_64(dd - 105, ss - 105); memcpy_avx_64(dd - 64, ss - 64); break;
	case 234: memcpy_avx_128(dd - 234, ss - 234);
	case 106: memcpy_avx_64(dd - 106, ss - 106); memcpy_avx_64(dd - 64, ss - 64); break;
	case 235: memcpy_avx_128(dd - 235, ss - 235);
	case 107: memcpy_avx_64(dd - 107, ss - 107); memcpy_avx_64(dd - 64, ss - 64); break;
	case 236: memcpy_avx_128(dd - 236, ss - 236);
	case 108: memcpy_avx_64(dd - 108, ss - 108); memcpy_avx_64(dd - 64, ss - 64); break;
	case 237: memcpy_avx_128(dd - 237, ss - 237);
	case 109: memcpy_avx_64(dd - 109, ss - 109); memcpy_avx_64(dd - 64, ss - 64); break;
	case 238: memcpy_avx_128(dd - 238, ss - 238);
	case 110: memcpy_avx_64(dd - 110, ss - 110); memcpy_avx_64(dd - 64, ss - 64); break;
	case 239: memcpy_avx_128(dd - 239, ss - 239);
	case 111: memcpy_avx_64(dd - 111, ss - 111); memcpy_avx_64(dd - 64, ss - 64); break;
	case 240: memcpy_avx_128(dd - 240, ss - 240);
	case 112: memcpy_avx_64(dd - 112, ss - 112); memcpy_avx_64(dd - 64, ss - 64); break;
	case 241: memcpy_avx_128(dd - 241, ss - 241);
	case 113: memcpy_avx_64(dd - 113, ss - 113); memcpy_avx_64(dd - 64, ss - 64); break;
	case 242: memcpy_avx_128(dd - 242, ss - 242);
	case 114: memcpy_avx_64(dd - 114, ss - 114); memcpy_avx_64(dd - 64, ss - 64); break;
	case 243: memcpy_avx_128(dd - 243, ss - 243);
	case 115: memcpy_avx_64(dd - 115, ss - 115); memcpy_avx_64(dd - 64, ss - 64); break;
	case 244: memcpy_avx_128(dd - 244, ss - 244);
	case 116: memcpy_avx_64(dd - 116, ss - 116); memcpy_avx_64(dd - 64, ss - 64); break;
	case 245: memcpy_avx_128(dd - 245, ss - 245);
	case 117: memcpy_avx_64(dd - 117, ss - 117); memcpy_avx_64(dd - 64, ss - 64); break;
	case 246: memcpy_avx_128(dd - 246, ss - 246);
	case 118: memcpy_avx_64(dd - 118, ss - 118); memcpy_avx_64(dd - 64, ss - 64); break;
	case 247: memcpy_avx_128(dd - 247, ss - 247);
	case 119: memcpy_avx_64(dd - 119, ss - 119); memcpy_avx_64(dd - 64, ss - 64); break;
	case 248: memcpy_avx_128(dd - 248, ss - 248);
	case 120: memcpy_avx_64(dd - 120, ss - 120); memcpy_avx_64(dd - 64, ss - 64); break;
	case 249: memcpy_avx_128(dd - 249, ss - 249);
	case 121: memcpy_avx_64(dd - 121, ss - 121); memcpy_avx_64(dd - 64, ss - 64); break;
	case 250: memcpy_avx_128(dd - 250, ss - 250);
	case 122: memcpy_avx_64(dd - 122, ss - 122); memcpy_avx_64(dd - 64, ss - 64); break;
	case 251: memcpy_avx_128(dd - 251, ss - 251);
	case 123: memcpy_avx_64(dd - 123, ss - 123); memcpy_avx_64(dd - 64, ss - 64); break;
	case 252: memcpy_avx_128(dd - 252, ss - 252);
	case 124: memcpy_avx_64(dd - 124, ss - 124); memcpy_avx_64(dd - 64, ss - 64); break;
	case 253: memcpy_avx_128(dd - 253, ss - 253);
	case 125: memcpy_avx_64(dd - 125, ss - 125); memcpy_avx_64(dd - 64, ss - 64); break;
	case 254: memcpy_avx_128(dd - 254, ss - 254);
	case 126: memcpy_avx_64(dd - 126, ss - 126); memcpy_avx_64(dd - 64, ss - 64); break;
	case 255: memcpy_avx_128(dd - 255, ss - 255);
	case 127: memcpy_avx_64(dd - 127, ss - 127); memcpy_avx_64(dd - 64, ss - 64); break;
	case 256: memcpy_avx_256(dd - 256, ss - 256); break;
	}

	return dst;
}

//---------------------------------------------------------------------
// main routine
//---------------------------------------------------------------------
void* memcpy_fast(void *destination, const void *source, size_t size)
{
	unsigned char *dst = (unsigned char*)destination;
	const unsigned char *src = (const unsigned char*)source;
	static size_t cachesize = 0x200000; // L3-cache size
	size_t padding;

	// small memory copy
	if (size <= 256) {
		memcpy_tiny(dst, src, size);
		_mm256_zeroupper();
		return destination;
	}

	// align destination to 16 bytes boundary
	padding = (32 - (((size_t)dst) & 31)) & 31;

#if 0
	if (padding > 0) {
		__m256i head = _mm256_loadu_si256((const __m256i*)src);
		_mm256_storeu_si256((__m256i*)dst, head);
		dst += padding;
		src += padding;
		size -= padding;
	}
#else
	__m256i head = _mm256_loadu_si256((const __m256i*)src);
	_mm256_storeu_si256((__m256i*)dst, head);
	dst += padding;
	src += padding;
	size -= padding;
#endif

	// medium size copy
	if (size <= cachesize) {
		__m256i c0, c1, c2, c3, c4, c5, c6, c7;

		for (; size >= 256; size -= 256) {
			c0 = _mm256_loadu_si256(((const __m256i*)src) + 0);
			c1 = _mm256_loadu_si256(((const __m256i*)src) + 1);
			c2 = _mm256_loadu_si256(((const __m256i*)src) + 2);
			c3 = _mm256_loadu_si256(((const __m256i*)src) + 3);
			c4 = _mm256_loadu_si256(((const __m256i*)src) + 4);
			c5 = _mm256_loadu_si256(((const __m256i*)src) + 5);
			c6 = _mm256_loadu_si256(((const __m256i*)src) + 6);
			c7 = _mm256_loadu_si256(((const __m256i*)src) + 7);
			_mm_prefetch((const char*)(src + 512), _MM_HINT_NTA);
			src += 256;
			_mm256_storeu_si256((((__m256i*)dst) + 0), c0);
			_mm256_storeu_si256((((__m256i*)dst) + 1), c1);
			_mm256_storeu_si256((((__m256i*)dst) + 2), c2);
			_mm256_storeu_si256((((__m256i*)dst) + 3), c3);
			_mm256_storeu_si256((((__m256i*)dst) + 4), c4);
			_mm256_storeu_si256((((__m256i*)dst) + 5), c5);
			_mm256_storeu_si256((((__m256i*)dst) + 6), c6);
			_mm256_storeu_si256((((__m256i*)dst) + 7), c7);
			dst += 256;
		}
	}
	else {		// big memory copy
		__m256i c0, c1, c2, c3, c4, c5, c6, c7;
		/* __m256i c0, c1, c2, c3, c4, c5, c6, c7; */

		_mm_prefetch((const char*)(src), _MM_HINT_NTA);

		if ((((size_t)src) & 31) == 0) {	// source aligned
			for (; size >= 256; size -= 256) {
				c0 = _mm256_load_si256(((const __m256i*)src) + 0);
				c1 = _mm256_load_si256(((const __m256i*)src) + 1);
				c2 = _mm256_load_si256(((const __m256i*)src) + 2);
				c3 = _mm256_load_si256(((const __m256i*)src) + 3);
				c4 = _mm256_load_si256(((const __m256i*)src) + 4);
				c5 = _mm256_load_si256(((const __m256i*)src) + 5);
				c6 = _mm256_load_si256(((const __m256i*)src) + 6);
				c7 = _mm256_load_si256(((const __m256i*)src) + 7);
				_mm_prefetch((const char*)(src + 512), _MM_HINT_NTA);
				src += 256;
				_mm256_stream_si256((((__m256i*)dst) + 0), c0);
				_mm256_stream_si256((((__m256i*)dst) + 1), c1);
				_mm256_stream_si256((((__m256i*)dst) + 2), c2);
				_mm256_stream_si256((((__m256i*)dst) + 3), c3);
				_mm256_stream_si256((((__m256i*)dst) + 4), c4);
				_mm256_stream_si256((((__m256i*)dst) + 5), c5);
				_mm256_stream_si256((((__m256i*)dst) + 6), c6);
				_mm256_stream_si256((((__m256i*)dst) + 7), c7);
				dst += 256;
			}
		}
		else {							// source unaligned
			for (; size >= 256; size -= 256) {
				c0 = _mm256_loadu_si256(((const __m256i*)src) + 0);
				c1 = _mm256_loadu_si256(((const __m256i*)src) + 1);
				c2 = _mm256_loadu_si256(((const __m256i*)src) + 2);
				c3 = _mm256_loadu_si256(((const __m256i*)src) + 3);
				c4 = _mm256_loadu_si256(((const __m256i*)src) + 4);
				c5 = _mm256_loadu_si256(((const __m256i*)src) + 5);
				c6 = _mm256_loadu_si256(((const __m256i*)src) + 6);
				c7 = _mm256_loadu_si256(((const __m256i*)src) + 7);
				_mm_prefetch((const char*)(src + 512), _MM_HINT_NTA);
				src += 256;
				_mm256_stream_si256((((__m256i*)dst) + 0), c0);
				_mm256_stream_si256((((__m256i*)dst) + 1), c1);
				_mm256_stream_si256((((__m256i*)dst) + 2), c2);
				_mm256_stream_si256((((__m256i*)dst) + 3), c3);
				_mm256_stream_si256((((__m256i*)dst) + 4), c4);
				_mm256_stream_si256((((__m256i*)dst) + 5), c5);
				_mm256_stream_si256((((__m256i*)dst) + 6), c6);
				_mm256_stream_si256((((__m256i*)dst) + 7), c7);
				dst += 256;
			}
		}
		_mm_sfence();
	}

	memcpy_tiny(dst, src, size);
	_mm256_zeroupper();

	return destination;
}
#pragma GCC diagnostic pop

#endif

/** Return the appropriate message for a file error or end of file.
*/
const char *M_FileError(FILE *fp)
{
	if (ferror(fp))
		return strerror(errno);
	else
		return "end-of-file";
}
