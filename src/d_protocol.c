// SONIC ROBO BLAST 2 KART
//-----------------------------------------------------------------------------
// Copyright (C) 2021 by "Fafabis".
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  d_protocol.c
/// \brief srb2kart:// protocol stuff

#include "doomdef.h"

#include <curl/curl.h>
#include "m_argv.h"
#include "d_main.h"
#include "d_clisrv.h"
#include "d_protocol.h"

#if (defined (__unix__) && !defined (MSDOS)) || defined (UNIXCOMMON)
#include <sys/types.h>
#include <unistd.h>
#include <pwd.h>
#include <SDL2/SDL.h>
#else
#include "SDL.h"
#include <tchar.h>
#include <unistd.h>
#include <limits.h>
#endif

// PROTOS
#ifdef __MINGW32__
static HKEY OpenKey(HKEY hRootKey, const char* strKey);
static boolean SetStringValue(HKEY hRegistryKey, const char *valueName, const char  *data);
#endif

static char *exe_name(void)
{
	static char buf[PATH_MAX];
#if (defined (__unix__) && !defined (MSDOS)) || defined (UNIXCOMMON)
	static char *p = NULL;
	FILE *fp = NULL;
	
	if(!(fp = fopen("/proc/self/maps", "r")))
		return NULL;
	
	while(fgets(buf, PATH_MAX, fp))
		fclose(fp);

	*(p = strchr(buf, '\n')) = '\0';
	while(*p != ' ')
		p--;	
	return p+1;
#else
	GetModuleFileName(NULL, buf, PATH_MAX);
	return buf;
	#endif
}

#ifdef __MINGW32__
static HKEY OpenKey(HKEY hRootKey, const char* strKey)
{
	HKEY hKey;
	LONG nError = RegOpenKeyEx(hRootKey, strKey, 0, KEY_ALL_ACCESS, &hKey);

	if (nError==ERROR_FILE_NOT_FOUND)
	{
		CONS_Printf("PROTOCOL: Creating registry key: %s\n", strKey);
		nError = RegCreateKeyEx(hRootKey, strKey, 0, NULL, REG_OPTION_NON_VOLATILE,KEY_ALL_ACCESS,NULL, &hKey, NULL);
	}

	if (nError)
		I_Error("PROTOCOL: Could not create Registry Key.\nMaybe you forgot to run as an administrator?");

	return hKey;
}

static boolean SetStringValue(HKEY hRegistryKey, const char *valueName, const char *data)
{
	I_Assert(hRegistryKey != NULL);

	return (RegSetValueExA(hRegistryKey,
				valueName,
				0,
				REG_SZ,
				(LPBYTE)data,
				strlen(data) + 1) == ERROR_SUCCESS);
}
#endif

void D_ShowProtoWindow(void) {
	char buffer[255];
	const char *exe_path = exe_name();
	FILE *fp = NULL;
#if (defined (__unix__) && !defined (MSDOS)) || defined (UNIXCOMMON)
	FILE *desktopfile;
	FILE *mimefile;
	struct passwd *pw = getpwuid(getuid());
	const char *homedir = pw->pw_dir;
	boolean alreadyexists = false;
#endif
	if (dedicated)
		return;

	fp  = fopen(va("%s/protocol.txt", srb2home), "a+");

	while (fgets(buffer, 255, fp))
		if (strcmp(buffer, "no") == 0)
			return;

	if (strcmp(buffer, "yes") != 0) {
		const SDL_MessageBoxButtonData buttons[] = {
			{ /* .flags, .buttonid, .text */        0, 0, "Yes" },
			{ SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 1, "No" },
			{ SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 2, "Cancel" },
		};
		const SDL_MessageBoxData messageboxdata = {
			SDL_MESSAGEBOX_INFORMATION,
			NULL,
			"Register SRB2Kart Protocols",
			"Would you like to register the srb2kart:// protocol?\n"
			"This will allow you to connect to servers and load replays directly from the browser\n\n"
#if defined (__MINGW32__)
			"This will require administrator permissions.\n"
			"(if you aren't running the game as an administrator click in \"Cancel\" and run the game as administrator)."
#endif
			"",
			SDL_arraysize(buttons),
			buttons,
			NULL
			};

		int buttonid;
		if (SDL_ShowMessageBox(&messageboxdata, &buttonid) < 0) {
			I_Error("Error displaying message box.");
		}
		if (buttonid == 0) {
#if defined (__MINGW32__)
			HKEY hKey = OpenKey(HKEY_CLASSES_ROOT,"srb2kart");
			SetStringValue(hKey, "URL Protocol", "");
			RegCloseKey(hKey);
			hKey = OpenKey(HKEY_CLASSES_ROOT,"srb2kart\\shell\\open\\command");	
			SetStringValue(hKey, "", va("\"%s\" \"%%1\"", exe_path));
			RegCloseKey(hKey);
#elif (defined (__unix__) && !defined (MSDOS)) || defined (UNIXCOMMON)

			desktopfile = fopen(va("%s/.local/share/applications/srb2kart.desktop", homedir), "w");

			fprintf(desktopfile, 
				"[Desktop Entry]\n"
				"Type=Application\n"
				"Name=SRB2Kart Scheme Handler\n"
				"Exec=bash -c '%s %%u'\n"
				"StartupNotify=false\n"
				"Terminal=false\n"
				"MimeType=x-scheme-handler/srb2kart;\n"
				, exe_path);

			fclose(desktopfile);

			mimefile = fopen(va("%s/.local/share/applications/mimeinfo.cache", homedir), "a+");

			while (fgets(buffer, 255, mimefile))	
				if (strncmp(buffer, "x-scheme-handler/srb2kart=srb2kart.desktop;", 43)==0)
				{
					alreadyexists = true;
					break;
				}

			if (!alreadyexists)
				fprintf(mimefile, "x-scheme-handler/srb2kart=srb2kart.desktop;\n");

#endif
			fprintf(fp, "yes");
		}
		else 
			fprintf(fp, "no");
	}
	fclose(fp);
}

static size_t write_data(void *ptr, size_t size, size_t nmemb, FILE *stream) {
	size_t written = fwrite(ptr, size, nmemb, stream);
	return written;
}

void D_DownloadReplay(const char *url, const char *path) {
	FILE *fp = NULL;
	CURL *curl;
	CURLcode cc;
	curl = curl_easy_init();

	if (!curl) {
		I_Error("REPLAY: Error initializing CURL.");

    	}
	fp = fopen(path, "wb");
	CONS_Printf("REPLAY: URL: %s\n", url);
	curl_easy_setopt(curl, CURLOPT_URL, url);
       	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
       	curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
       	cc = curl_easy_perform(curl);
	if (cc != CURLE_OK)
		I_Error("REPLAY: URL gave response code %u.", cc);

       	curl_easy_cleanup(curl);
       	fclose(fp);
}
