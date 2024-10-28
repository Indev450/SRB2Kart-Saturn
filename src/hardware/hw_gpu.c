// SONIC ROBO BLAST 2
//-----------------------------------------------------------------------------
// Copyright (C) 2020 by Jaime "Lactozilla" Passos.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file hw_gpu.c
/// \brief GPU low-level interface API

#ifdef HWRENDER

#include "r_opengl/r_opengl.h"

// ==========================================================================
// the hardware driver object
// ==========================================================================
struct GPURenderingAPI *GPU = NULL;

void GPUInterface_Load(struct GPURenderingAPI **api)
{
	*api = &GLInterfaceAPI;
}

#endif // HWRENDER
