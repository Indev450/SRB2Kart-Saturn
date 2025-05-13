// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright (C) 1998-2000 by DooM Legacy Team.
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
//-----------------------------------------------------------------------------
/// \file
/// \brief SDL specific part of the OpenGL API for SRB2

#ifdef _MSC_VER
#pragma warning(disable : 4214 4244)
#endif

#ifdef HAVE_SDL
#define _MATH_DEFINES_DEFINED

#include "SDL.h"

#include "sdlmain.h"

#ifdef _MSC_VER
#pragma warning(default : 4214 4244)
#endif

#include "../doomdef.h"
#include "../d_main.h"

#ifdef HWRENDER
#include "../hardware/r_opengl/r_opengl.h"
#include "../hardware/hw_main.h"
#include "../hardware/hw_gl.h"
#include "ogl_sdl.h"
#include "../i_system.h"
#include "hwsym_sdl.h"
#include "../m_argv.h"
#include "../i_video.h"
#include "../f_finale.h"

#ifdef DEBUG_TO_FILE
#include <stdarg.h>
#if defined (_WIN32) && !defined (__CYGWIN__)
#include <direct.h>
#else
#include <unistd.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#endif

#ifdef USE_WGL_SWAP
PFNWGLEXTSWAPCONTROLPROC wglSwapIntervalEXT = NULL;
#else
typedef int (*PFNGLXSWAPINTERVALPROC) (int);
PFNGLXSWAPINTERVALPROC glXSwapIntervalSGIEXT = NULL;
#endif

#ifndef STATIC_OPENGL
PFNglClear pglClear;
PFNglGetIntegerv pglGetIntegerv;
PFNglGetString pglGetString;
#endif

#ifdef USE_FBO_OGL

#if defined (__unix__)
static boolean xwaylandcrap = false;
#endif

boolean UseScreenFBO(void)
{
	return ((supportFBO && cv_glframebuffer.value && downsample)
#if defined (__unix__)
	|| (supportFBO && xwaylandcrap)
#endif
	);
}
#endif

/**	\brief SDL video display surface
*/
INT32 oglflags = 0;
SDL_GLContext sdlglcontext = 0;

void *GetGLFunc(const char *proc)
{
	return SDL_GL_GetProcAddress(proc);
}

boolean VID_LoadOGLAPI(void)
{
#ifndef STATIC_OPENGL
	const char *OGLLibname = NULL;

	if (M_CheckParm("-OGLlib") && M_IsNextParm())
		OGLLibname = M_GetNextParm();

	if (SDL_GL_LoadLibrary(OGLLibname) != 0)
	{
		CONS_Alert(CONS_ERROR, "Could not load OpenGL Library: %s\n"
					"Falling back to Software mode.\n", SDL_GetError());
		if (!M_CheckParm("-OGLlib"))
			CONS_Printf("If you know what is the OpenGL library's name, use -OGLlib\n");
		return 0;
	}
#endif
	return true;
}

/**	\brief	The OglSdlSurface function

	\param	w	width
	\param	h	height
	\param	isFullscreen	if true, go fullscreen

	\return	if true, changed video mode
*/
static boolean first_init = false;

boolean OglSdlSurface(INT32 w, INT32 h)
{
	const char *gllogdir = NULL;

	oglflags = 0;

	if (!first_init)
	{
		if (!gllogstream)
		{
			gllogdir = D_Home();

#ifdef DEBUG_TO_FILE
#ifdef DEFAULTDIR
			if (gllogdir)
				gllogstream = fopen(va("%s/"DEFAULTDIR"/ogllog.txt",gllogdir), "wt");
			else
#endif
				gllogstream = fopen("./ogllog.txt", "wt");
#endif
		}

		gl_version = pglGetString(GL_VERSION);
		gl_renderer = pglGetString(GL_RENDERER);
		gl_extensions = pglGetString(GL_EXTENSIONS);
		pglGetIntegerv(GL_NUM_EXTENSIONS, (GLint*)&gl_num_extensions);
		gl_vendor = pglGetString(GL_VENDOR);

		GL_DBG_Printf("OpenGL %s\n", gl_version);
		GL_DBG_Printf("GPU: %s\n", gl_renderer);
		GL_DBG_Printf("Extensions:");

		{
			// Need to do it with strtok for same reason its done like that in gr_glinfo command

			char *copy = strdup((const char*)gl_extensions);
			char *ext = strtok(copy, " ");

			if (copy == NULL)
			{
				GL_DBG_Printf("Ran out of memory listing extensions?!?!");
			}
			else
			{
				do
				{
					GL_DBG_Printf(" %s", ext);
				} while ((ext = strtok(NULL, " ")) != NULL);

				free(copy);
			}
		}

		if (strcmp((const char*)gl_renderer, "GDI Generic") == 0 &&
			strcmp((const char*)gl_version, "1.1.0") == 0)
		{
			// Oh no... Windows gave us the GDI Generic rasterizer, so something is wrong...
			// The game will crash later on when unsupported OpenGL commands are encountered.
			// Instead of a nondescript crash, show a more informative error message.
			// Also set the renderer variable back to software so the next launch won't
			// repeat this error.
			I_Error("OpenGL Error: Failed to access the GPU. Possible reasons include:\n"
					"- GPU vendor has dropped OpenGL support on your GPU and OS. (Old GPU?)\n"
					"- GPU drivers are missing or broken. You may need to update your drivers.");
		}

		SetupGLInfo();

		SetupGLFunc4();

		if (majorGL == 1 && minorGL <= 3) // GL_GENERATE_MIPMAP is unavailible for OGL 1.3 and below
			supportMipMap = false;
		else
			supportMipMap = true;

		if (GL_isExtAvailable("GL_EXT_texture_filter_anisotropic", gl_extensions))
			pglGetIntegerv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maximumAnisotropy);
		else
			maximumAnisotropy = 1;

		glanisotropicmode_cons_t[1].value = maximumAnisotropy;

#if defined (__unix__)
#ifdef USE_FBO_OGL
		char videodriver[4] = {'S','D','L',0};
		if (supportFBO && strstr((const char*)gl_renderer, "NVIDIA")
			&& (*strncpy(videodriver, SDL_GetCurrentVideoDriver(), 4) != '\0')
			&& (strncasecmp("x11",videodriver,4) == 0))
			xwaylandcrap = true;
#endif
#endif
	}

	SDL_GL_SetSwapInterval(cv_vidwait.value ? 1 : 0);

	GL_SetModelView(w, h);
	GL_SetStates();
	pglClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);

#ifdef USE_FBO_OGL
	if (UseScreenFBO())
		GL_Framebuffer_Enable();
	else
		GL_Framebuffer_Disable();
#endif

	if (!first_init)
		HWR_Startup();

	first_init = true;

	return true;
}

/**	\brief	The OglSdlFinishUpdate function

	\param	vidwait	wait for video sync

	\return	void
*/
void OglSdlFinishUpdate(boolean waitvbl)
{
	static boolean oldwaitvbl = false;
	int sdlw, sdlh;

#ifdef USE_FBO_OGL
	const boolean usefbo = UseScreenFBO();
#endif

	if (oldwaitvbl != waitvbl)
	{
		SDL_GL_SetSwapInterval(waitvbl ? 1 : 0);
	}

	oldwaitvbl = waitvbl;

	SDL_GetWindowSize(window, &sdlw, &sdlh);
	HWR_MakeScreenFinalTexture();

#ifdef USE_FBO_OGL
	if (usefbo)
	{
		GL_Framebuffer_Unbind();
	}
#endif

	HWR_DrawScreenFinalTexture(sdlw, sdlh, HWR_ShouldUsePaletteRendering());

#ifdef USE_FBO_OGL
	if (usefbo)
	{
		GL_Framebuffer_Enable();
	}
#endif

	SDL_GL_SwapWindow(window);

	GL_GClipRect(0, 0, realwidth, realheight, NZCLIP_PLANE, FAR_ZCLIP_DEFAULT);

	// Sryder:	We need to draw the final screen texture again into the other buffer in the original position so that
	//			effects that want to take the old screen can do so after this
	// well we dont need it on native res it seems
#ifdef USE_FBO_OGL
	if ((!I_CheckNativeRes() && !usefbo) || WipeInAction)
#else
	if (!I_CheckNativeRes() || WipeInAction)
#endif
		HWR_DrawScreenFinalTexture(realwidth, realheight, false);

#if defined (__unix__)
#ifdef USE_FBO_OGL
	if (loaded_config)
		xwaylandcrap = false;
#endif
#endif
}

#endif //HWRENDER
#endif //SDL
