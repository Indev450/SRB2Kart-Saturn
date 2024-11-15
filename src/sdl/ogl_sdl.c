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
#include "ogl_sdl.h"
#include "../i_system.h"
#include "hwsym_sdl.h"
#include "../m_argv.h"
#include "../i_video.h"

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
static boolean isnvidiagpu = false;
#endif

boolean UseScreenFBO(void)
{
	return ((supportFBO && cv_glframebuffer.value && downsample)
#if defined (__unix__)
	|| (supportFBO && isnvidiagpu && xwaylandcrap)
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

boolean LoadGL(void)
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
	return SetupGLfunc();
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
	INT32 cbpp = cv_scr_depth.value < 16 ? 16 : cv_scr_depth.value;
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

		GL_DBG_Printf("OpenGL %s\n", gl_version);
		GL_DBG_Printf("GPU: %s\n", gl_renderer);
		GL_DBG_Printf("Extensions: %s\n", gl_extensions);

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

		if (isExtAvailable("GL_EXT_texture_filter_anisotropic", gl_extensions))
			pglGetIntegerv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maximumAnisotropy);
		else
			maximumAnisotropy = 1;

		glanisotropicmode_cons_t[1].value = maximumAnisotropy;

#if defined (__unix__)
#ifdef USE_FBO_OGL
		if (strstr((const char*)gl_renderer, "NVIDIA"))
			isnvidiagpu = true;
#endif
#endif
	}

	SDL_GL_SetSwapInterval(cv_vidwait.value ? 1 : 0);

	// The screen textures need to be flushed if the width or height change so that they be remade for the correct size
	if (screen_width != w || screen_height != h)
	{
		FlushScreenTextures();

#ifdef USE_FBO_OGL
		GLFramebuffer_DeleteAttachments();
#endif
	}

	screen_width = (GLint)w;
	screen_height = (GLint)h;

	SetModelView(w, h);
	SetStates();
	pglClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);

#ifdef USE_FBO_OGL

	if (!supportFBO)
	{
		if (cv_glframebuffer.value)
			CV_SetValue(&cv_glframebuffer, 0);
	}

	if (UseScreenFBO())
		GLFramebuffer_Enable();
	else
		GLFramebuffer_Disable();
#endif

	if (!first_init)
		HWR_Startup();
	textureformatGL = cbpp > 16 ? GL_RGBA : GL_RGB5_A1;

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
	if (oldwaitvbl != waitvbl)
	{
		SDL_GL_SetSwapInterval(waitvbl ? 1 : 0);
	}

	oldwaitvbl = waitvbl;

	SDL_GetWindowSize(window, &sdlw, &sdlh);
	HWR_MakeScreenFinalTexture();

#ifdef USE_FBO_OGL
	if (UseScreenFBO())
		GLFramebuffer_Unbind();
#endif
	
	HWR_DrawScreenFinalTexture(sdlw, sdlh);

#ifdef USE_FBO_OGL
	if (UseScreenFBO())
		GLFramebuffer_Enable();
#endif

	SDL_GL_SwapWindow(window);

	GClipRect(0, 0, realwidth, realheight, NZCLIP_PLANE, FAR_ZCLIP_DEFAULT);

	// Sryder:	We need to draw the final screen texture again into the other buffer in the original position so that
	//			effects that want to take the old screen can do so after this
	HWR_DrawScreenFinalTexture(realwidth, realheight);
}

EXPORT void HWRAPI(OglSdlSetPalette) (RGBA_t *palette)
{
	INT32 i;

	for (i = 0; i < 256; i++)
	{
		myPaletteData[i].s.red   = palette[i].s.red;
		myPaletteData[i].s.green = palette[i].s.green;
		myPaletteData[i].s.blue  = palette[i].s.blue;
		myPaletteData[i].s.alpha = palette[i].s.alpha;
	}
	Flush();
}

#endif //HWRENDER
#endif //SDL
