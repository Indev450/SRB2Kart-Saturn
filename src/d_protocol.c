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

#include <stdio.h>
#include <unistd.h>
#include <limits.h>
#if defined (__unix__) || defined (UNIXCOMMON)
#include <sys/types.h>
#include <pwd.h>
#elif defined (__WIN32)
#include <tchar.h>
#endif

#ifdef HAVE_CURL
#include <curl/curl.h>
#endif

#include "doomdef.h"

#include "m_argv.h"
#include "d_main.h"
#include "d_clisrv.h"
#include "d_netfil.h"
#include "d_protocol.h"
#include "i_system.h"

#include "SDL.h"

// PROTOS
#if defined (__WIN32)
static HKEY OpenKey(HKEY hRootKey, const char* strKey);
static boolean SetStringValue(HKEY hRegistryKey, const char *valueName, const char *data);
#endif

static char *GetExePath(void)
{
	static char buf[PATH_MAX];
#if defined (__linux__)
	if (readlink("/proc/self/exe", buf, PATH_MAX) < 0)
		I_Error("PROTOCOL: Error reading /proc/");
#elif defined (__FreeBSD__)
	if (readlink("/proc/curproc/file", buf, PATH_MAX) < 0)
		I_Error("PROTOCOL: Error reading /proc/");
#elif defined (__WIN32)
	GetModuleFileName(NULL, buf, PATH_MAX);
#endif
	return buf;
}

static void RegisterProtocols(const char *path)
{
#if defined (__WIN32)
	HKEY hKey = OpenKey(HKEY_CURRENT_USER,"Software\\Classes\\srb2kart");
	SetStringValue(hKey, "URL Protocol", "");
	RegCloseKey(hKey);
	hKey = OpenKey(HKEY_CURRENT_USER,"Software\\Classes\\srb2kart\\shell\\open\\command");	
	SetStringValue(hKey, "", va("\"%s\" \"%%1\"", path));
	RegCloseKey(hKey);
#elif defined (__unix__) || defined (UNIXCOMMON)
	FILE *desktopfile = NULL;
	FILE *mimefile = NULL;
	struct passwd *pw = getpwuid(getuid());
	const char *homedir = pw->pw_dir;
	char buffer[255];
	boolean alreadyexists = false;

	desktopfile = fopen(va("%s/.local/share/applications/srb2kart-handler.desktop", homedir), "w");
	if (!desktopfile)
		I_Error("PROTOCOL: Error creating .desktop file.");

	fprintf(desktopfile,
			"[Desktop Entry]\n"
			"Type=Application\n"
			"Name=SRB2Kart Scheme Handler\n"
			"Exec=bash -c '%s %%u'\n"
			"StartupNotify=false\n"
			"MimeType=x-scheme-handler/srb2kart;\n",
			path);

	fclose(desktopfile);

	mimefile = fopen(va("%s/.local/share/applications/mimeinfo.cache", homedir), "a+");
	if (!mimefile)
		I_Error("PROTOCOL: Error opening mime file.");

	while (fgets(buffer, 255, mimefile))	
		if (strncmp(buffer, "x-scheme-handler/srb2kart=srb2kart-handler.desktop;", 43)==0)
		{
			alreadyexists = true;
			break;
		}

	if (!alreadyexists)
		fprintf(mimefile, "x-scheme-handler/srb2kart=srb2kart-handler.desktop;\n");

#endif
}

#if defined (__WIN32)
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
		I_Error("PROTOCOL: Could not create Registry Key.");

	return hKey;
}

static boolean SetStringValue(HKEY hRegistryKey, const char *valueName, const char *data)
{
	I_Assert(hRegistryKey != NULL);

	return (RegSetValueExA(hRegistryKey,
				valueName,
				0,
				REG_SZ,
				(const BYTE *)data,
				strlen(data) + 1) == ERROR_SUCCESS);
}
#endif

void D_SetupProtocol(void)
{
	FILE *fp = NULL;
	char *result = NULL;
	char buffer[PATH_MAX];
	const char *exe_path = GetExePath();
	const char *protocolfile = va("%s/protocol.txt", srb2home);

	if (dedicated)
		return;

	fp = fopen(protocolfile, "a+");
	result = fgets(buffer, PATH_MAX, fp);
	if (result) 
	{
		if (strcmp(buffer, "no") == 0)
			return;
		else if (strcmp(buffer, exe_path) != 0)
		{
			// overwrite
			fp = fopen(protocolfile, "w");
			RegisterProtocols(exe_path);
			fprintf(fp, "%s", exe_path);
			return;
		}
	}
	else // probably first time
	{
		const SDL_MessageBoxButtonData buttons[] = {
#if defined (__unix__) || defined (UNIXCOMMON)
			{ /* .flags, .buttonid, .text */        0, 0, "Yes" },
			{ SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 1, "No" }
#elif defined (__WIN32)
			{ SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 1, "No" },
			{ /* .flags, .buttonid, .text */        0, 0, "Yes" }
#endif
		};
		const SDL_MessageBoxData messageboxdata = {
			SDL_MESSAGEBOX_INFORMATION,
			NULL,
			"Register SRB2Kart Protocols",
			"Would you like to register the srb2kart:// protocol?\n"
			"This will allow you to connect to servers and load replays directly from the browser"
			"",
			SDL_arraysize(buttons),
			buttons,
			NULL
			};

		int buttonid;
		if (SDL_ShowMessageBox(&messageboxdata, &buttonid) < 0)
		{
			I_Error("Error displaying message box.");
		}

		if (buttonid == 0)
		{
			RegisterProtocols(exe_path);
			fprintf(fp, "%s", exe_path);
		}
		else
			fprintf(fp, "no");
	}
	fclose(fp);
}

#ifdef HAVE_CURL
void D_DownloadReplay(const char *url, const char *path)
{
	FILE *fp = NULL;
	CURL *curl;
	CURLcode cc;
	curl = curl_easy_init();
	if (!curl)
		I_Error("REPLAY: Error initializing CURL.");

	fp = fopen(path, "wb");
	CONS_Printf("REPLAY: URL: %s\n", url);
	curl_easy_setopt(curl, CURLOPT_URL, url);
       	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlwrite_data);
       	curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);

       	cc = curl_easy_perform(curl);
	if (cc != CURLE_OK)
		I_Error("REPLAY: URL gave response code %u.", cc);

       	curl_easy_cleanup(curl);
       	fclose(fp);
}
#endif
