/* Emacs style mode select   -*- C++ -*-
 *-----------------------------------------------------------------------------
 *
 *
 *  PrBoom: a Doom port merged with LxDoom and LSDLDoom
 *  based on BOOM, a modified and improved DOOM engine
 *  Copyright (C) 1999 by
 *  id Software, Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman
 *  Copyright (C) 1999-2000 by
 *  Jess Haas, Nicolas Kalkhof, Colin Phipps, Florian Schulze
 *  Copyright 2005, 2006 by
 *  Florian Schulze, Colin Phipps, Neil Stevens, Andrey Budko
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 *  02111-1307, USA.
 *
 * DESCRIPTION:
 *
 *---------------------------------------------------------------------
 */

/*
 *
 ** gl_clipper.cpp
 **
 ** Handles visibility checks.
 ** Loosely based on the JDoom clipper.
 **
 **---------------------------------------------------------------------------
 ** Copyright 2003 Tim Stump
 ** All rights reserved.
 **
 ** Redistribution and use in source and binary forms, with or without
 ** modification, are permitted provided that the following conditions
 ** are met:
 **
 ** 1. Redistributions of source code must retain the above copyright
 **    notice, this list of conditions and the following disclaimer.
 ** 2. Redistributions in binary form must reproduce the above copyright
 **    notice, this list of conditions and the following disclaimer in the
 **    documentation and/or other materials provided with the distribution.
 ** 3. The name of the author may not be used to endorse or promote products
 **    derived from this software without specific prior written permission.
 **
 ** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 ** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 ** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 ** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 ** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 ** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 ** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 ** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 ** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 ** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 **---------------------------------------------------------------------------
 **
 */

#include <math.h>
#include "../v_video.h"
#include "hw_main.h"
#include "hw_clip.h"
#include "hw_glob.h"
#include "../r_state.h"
#include "../tables.h"
#include "r_opengl/r_opengl.h"
#include "../r_main.h"	// for cv_fov

#ifdef HAVE_SPHEREFRUSTRUM
static GLdouble viewMatrix[16];
static GLdouble projMatrix[16];
float frustum[6][4];
#endif



// SRB2CB I don't think used any of this stuff, let's disable for now since SRB2 probably doesn't want it either
// compiler complains about (p)glGetFloatv anyway, in case anyone wants this
// only r_opengl.c can use the base gl funcs as it turns out, that's a problem for whoever wants sphere frustum checks
// btw to renable define HAVE_SPHEREFRUSTRUM in hw_clip.h
#ifdef HAVE_SPHEREFRUSTRUM
//
// gld_FrustrumSetup
//

#define CALCMATRIX(a, b, c, d, e, f, g, h)\
(float)(viewMatrix[a] * projMatrix[b] + \
viewMatrix[c] * projMatrix[d] + \
viewMatrix[e] * projMatrix[f] + \
viewMatrix[g] * projMatrix[h])

#define NORMALIZE_PLANE(i)\
t = (float)sqrt(\
frustum[i][0] * frustum[i][0] + \
frustum[i][1] * frustum[i][1] + \
frustum[i][2] * frustum[i][2]); \
frustum[i][0] /= t; \
frustum[i][1] /= t; \
frustum[i][2] /= t; \
frustum[i][3] /= t

void gld_FrustrumSetup(void)
{
	float t;
	float clip[16];

	pglGetFloatv(GL_PROJECTION_MATRIX, projMatrix);
	pglGetFloatv(GL_MODELVIEW_MATRIX, viewMatrix);

	clip[0]  = CALCMATRIX(0, 0, 1, 4, 2, 8, 3, 12);
	clip[1]  = CALCMATRIX(0, 1, 1, 5, 2, 9, 3, 13);
	clip[2]  = CALCMATRIX(0, 2, 1, 6, 2, 10, 3, 14);
	clip[3]  = CALCMATRIX(0, 3, 1, 7, 2, 11, 3, 15);

	clip[4]  = CALCMATRIX(4, 0, 5, 4, 6, 8, 7, 12);
	clip[5]  = CALCMATRIX(4, 1, 5, 5, 6, 9, 7, 13);
	clip[6]  = CALCMATRIX(4, 2, 5, 6, 6, 10, 7, 14);
	clip[7]  = CALCMATRIX(4, 3, 5, 7, 6, 11, 7, 15);

	clip[8]  = CALCMATRIX(8, 0, 9, 4, 10, 8, 11, 12);
	clip[9]  = CALCMATRIX(8, 1, 9, 5, 10, 9, 11, 13);
	clip[10] = CALCMATRIX(8, 2, 9, 6, 10, 10, 11, 14);
	clip[11] = CALCMATRIX(8, 3, 9, 7, 10, 11, 11, 15);

	clip[12] = CALCMATRIX(12, 0, 13, 4, 14, 8, 15, 12);
	clip[13] = CALCMATRIX(12, 1, 13, 5, 14, 9, 15, 13);
	clip[14] = CALCMATRIX(12, 2, 13, 6, 14, 10, 15, 14);
	clip[15] = CALCMATRIX(12, 3, 13, 7, 14, 11, 15, 15);

	// Right plane
	frustum[0][0] = clip[ 3] - clip[ 0];
	frustum[0][1] = clip[ 7] - clip[ 4];
	frustum[0][2] = clip[11] - clip[ 8];
	frustum[0][3] = clip[15] - clip[12];
	NORMALIZE_PLANE(0);

	// Left plane
	frustum[1][0] = clip[ 3] + clip[ 0];
	frustum[1][1] = clip[ 7] + clip[ 4];
	frustum[1][2] = clip[11] + clip[ 8];
	frustum[1][3] = clip[15] + clip[12];
	NORMALIZE_PLANE(1);

	// Bottom plane
	frustum[2][0] = clip[ 3] + clip[ 1];
	frustum[2][1] = clip[ 7] + clip[ 5];
	frustum[2][2] = clip[11] + clip[ 9];
	frustum[2][3] = clip[15] + clip[13];
	NORMALIZE_PLANE(2);

	// Top plane
	frustum[3][0] = clip[ 3] - clip[ 1];
	frustum[3][1] = clip[ 7] - clip[ 5];
	frustum[3][2] = clip[11] - clip[ 9];
	frustum[3][3] = clip[15] - clip[13];
	NORMALIZE_PLANE(3);

	// Far plane
	frustum[4][0] = clip[ 3] - clip[ 2];
	frustum[4][1] = clip[ 7] - clip[ 6];
	frustum[4][2] = clip[11] - clip[10];
	frustum[4][3] = clip[15] - clip[14];
	NORMALIZE_PLANE(4);

	// Near plane
	frustum[5][0] = clip[ 3] + clip[ 2];
	frustum[5][1] = clip[ 7] + clip[ 6];
	frustum[5][2] = clip[11] + clip[10];
	frustum[5][3] = clip[15] + clip[14];
	NORMALIZE_PLANE(5);
}

boolean gld_SphereInFrustum(float x, float y, float z, float radius)
{
	int p;

	for (p = 0; p < 4; p++)
	{
		if (frustum[p][0] * x +
			frustum[p][1] * y +
			frustum[p][2] * z +
			frustum[p][3] <= -radius)
		{
			return false;
		}
	}
	return true;
}
#endif