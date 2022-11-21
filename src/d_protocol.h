// SONIC ROBO BLAST 2 KART
//-----------------------------------------------------------------------------
// Copyright (C) 2021 by "Fafabis".
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  d_protocol.h
/// \brief srb2kart:// protocol stuff

void D_SetupProtocol(void);

void D_CreateProtocol(void);
void D_DeleteProtocol(void);
#ifdef HAVE_CURL
void D_DownloadReplay(const char *url, const char *path);
#endif
