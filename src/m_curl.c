// SONIC ROBO BLAST 2 KART SATURN
//-----------------------------------------------------------------------------
// Copyright (C) 2025 by Indev.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  m_curl.c
/// \brief Common curl-related functions shared across http-mserv and file download

#include "m_curl.h"
#include "doomtype.h"
#include "i_system.h" // I_OutputMsg
#include "m_argv.h" // M_CheckParm, M_GetNextParm

static boolean curl_args_init = false;
static const char *curl_args_proxy = NULL;
static struct curl_slist *curl_args_headers = NULL;

void M_SetCURLArgs(CURL *http_handle, char *curl_errbuf)
{
	CURLcode cc;

	// Only need to do this once
	if (!curl_args_init)
	{
		curl_args_init = true;

		curl_args_proxy = M_CheckParm("-curl-proxy") ? M_GetNextParm() : NULL;

		if (M_CheckParm("-curl-header"))
		{
			const char *param = NULL;

			while ((param = M_GetNextParm()))
			{
				curl_args_headers = curl_slist_append(curl_args_headers, param);
			}
		}
	}

	if (curl_args_proxy)
	{
		cc = curl_easy_setopt(http_handle, CURLOPT_PROXY, curl_args_proxy);
		if (cc != CURLE_OK) I_OutputMsg("libcurl: %s\n", curl_errbuf);
	}

	if (curl_args_headers)
	{
		cc = curl_easy_setopt(http_handle, CURLOPT_HTTPHEADER, curl_args_headers);
		if (cc != CURLE_OK) I_OutputMsg("libcurl: %s\n", curl_errbuf);
	}
}
