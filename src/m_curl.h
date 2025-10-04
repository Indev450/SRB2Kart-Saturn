// SONIC ROBO BLAST 2 KART SATURN
//-----------------------------------------------------------------------------
// Copyright (C) 2025 by Indev.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  m_curl.h
/// \brief Common curl-related functions shared across http-mserv and file download

#ifndef __M_CURL__
#define __M_CURL__

#include <curl/curl.h>

// Applies CLI args to curl handle, such as -curl-proxy
void M_SetCURLArgs(CURL *http_handle, char *curl_errbuf);

#endif
