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
#include <string.h>
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

static boolean RegisterProtocols(const char *path)
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
	boolean newfolder = false;
    	struct stat sb;
	char *applicationsfolder = va("%s/.local/share/applications/", homedir);

	if (system("which update-desktop-database > /dev/null 2>&1"))
	{
		// command not found, probably doesn't have freedesktop, so let's ignore		
		CONS_Alert(CONS_ERROR, "Unable to register protocols. Your system doesn't seem to have freedesktop.\n");
		return false;
	}

	if (stat(applicationsfolder, &sb) == -1) 
	{
		// location doesn't exist, so let's actually create it
		newfolder = true;
		I_mkdir(applicationsfolder, 0755);
	}

	desktopfile = fopen(va("%s/.local/share/applications/srb2kart-handler.desktop", homedir), "w");
	if (!desktopfile)
		I_Error("Unable to register protocols. Error creating .desktop file.");

	fprintf(desktopfile,
			"[Desktop Entry]\n"
			"Type=Application\n"
			"Name=SRB2Kart Scheme Handler\n"
			"Exec=bash -c '%s %%u'\n"
			"NoDisplay=true\n"
			"StartupNotify=false\n"
			"MimeType=x-scheme-handler/srb2kart;\n",
			path);

	fclose(desktopfile);

	mimefile = fopen(va("%s/.local/share/applications/mimeinfo.cache", homedir), "a+");
	if (!mimefile)
		I_Error("Unable to register protocols. Error opening mime file.");

	while (fgets(buffer, 255, mimefile))
	{
		if (strncmp(buffer, "x-scheme-handler/srb2kart=srb2kart-handler.desktop;", 43)==0)
		{
			alreadyexists = true;
			break;
		}
	}

	if (!alreadyexists)
		fprintf(mimefile, "x-scheme-handler/srb2kart=srb2kart-handler.desktop;\n");
	
	if (newfolder)
	{
		if (system(va("update-desktop-database %s/.local/share/applications/", homedir)) == -1)
		{
			CONS_Alert(CONS_ERROR, "Unable to register protocols. Could not run call to run update-desktop-database sucessfully.\n");
			return false;
		}
	}

#endif
	return true;
}

static void DeleteProtocols(void)
{
#if defined (__WIN32)

	HKEY hKey;
	RegOpenKeyEx(HKEY_CURRENT_USER, _T("Software\\Classes\\srb2kart\\shell\\open\\command"), 0, KEY_ALL_ACCESS, &hKey);
	RegDeleteValue(hKey, _T(""));
	RegCloseKey(hKey);
	RegDeleteKey(HKEY_CURRENT_USER, _T("Software\\Classes\\srb2kart\\shell\\open\\command"));
	RegOpenKeyEx(HKEY_CURRENT_USER, _T("Software\\Classes\\srb2kart\\shell\\open"), 0, KEY_ALL_ACCESS, &hKey);
	RegDeleteValue(hKey, _T(""));
	RegCloseKey(hKey);
	RegDeleteKey(HKEY_CURRENT_USER, _T("Software\\Classes\\srb2kart\\shell\\open"));
	RegOpenKeyEx(HKEY_CURRENT_USER, _T("Software\\Classes\\srb2kart\\shell"), 0, KEY_ALL_ACCESS, &hKey);
	RegDeleteValue(hKey, _T(""));
	RegCloseKey(hKey);
	RegDeleteKey(HKEY_CURRENT_USER, _T("Software\\Classes\\srb2kart\\shell"));
	RegOpenKeyEx(HKEY_CURRENT_USER, _T("Software\\Classes\\srb2kart"), 0, KEY_ALL_ACCESS, &hKey);
	RegDeleteValue(hKey, _T("URL Protocol"));
	RegDeleteValue(hKey, _T(""));
	RegCloseKey(hKey);
	RegDeleteKey(HKEY_CURRENT_USER, _T("Software\\Classes\\srb2kart"));
#elif defined (__unix__) || defined (UNIXCOMMON)
	struct passwd *pw = getpwuid(getuid());
	const char *homedir = pw->pw_dir;
	remove(va("%s/.local/share/applications/srb2kart-handler.desktop", homedir));
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
		I_Error("Unable to register protocols. Could not create Registry Key.");

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
		if (RegisterProtocols(exe_path))
			fprintf(fp, "%s", exe_path);
		else
			fprintf(fp, "no");
	}
	fclose(fp);
}

void D_DeleteProtocol(void)
{
	FILE *fp = NULL;
	DeleteProtocols();
	fp = fopen(va("%s/protocol.txt", srb2home), "w");
	fprintf(fp, "no");
	fclose(fp);
}

void D_CreateProtocol(void)
{
	FILE *fp = NULL;
	const char *exe_path = GetExePath();
	fp = fopen(va("%s/protocol.txt", srb2home), "w");
	RegisterProtocols(exe_path);
	fprintf(fp, "%s", exe_path);
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
		I_Error("Unable to download replay. Error initializing CURL.");

	fp = fopen(path, "wb");
	CONS_Printf("REPLAY: URL: %s\n", url);
	curl_easy_setopt(curl, CURLOPT_URL, url);
       	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlwrite_data);
       	curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);

       	cc = curl_easy_perform(curl);
	if (cc != CURLE_OK)
		I_Error("Unable to download replay. URL gave response code %u.", cc);

       	curl_easy_cleanup(curl);
       	fclose(fp);
}
#endif
