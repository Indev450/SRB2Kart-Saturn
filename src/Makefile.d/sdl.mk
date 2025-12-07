#
# Makefile options for SDL2 backend.
#

#
# SDL...., *looks at Alam*, THIS IS A MESS!
#
# ...a little bird flexes its muscles...
#

makedir:=$(makedir)/SDL

opts+=-DHAVE_SDL
sources+=$(call List,sdl/Sourcefile)

NOUPNP=1

# FIXME: UNTESTED
#ifdef PANDORA
#include sdl/SRB2Pandora/Makefile.cfg
#endif #ifdef PANDORA

# FIXME: UNTESTED
#ifdef CYGWIN32
#include sdl/MakeCYG.cfg
#endif #ifdef CYGWIN32

ifndef NOHW
sources+=sdl/ogl_sdl.c
endif

NOMIXER=1

ifdef NOMIXER
#sources+=sdl/dummy_sound.c
sources+=sdl/sdl_sound.c
SNDFILE_PKGCONFIG?=sndfile
$(eval $(call Use_pkg_config,SNDFILE))
else
opts+=-DHAVE_MIXER
sources+=sdl/mixer_sound.c

  ifndef HAIKU # Haiku has a special import path
    libs+=-lSDL2_mixer
  endif
endif

ifndef NOTHREADS
opts+=-DHAVE_THREADS
sources+=sdl/i_threads.c
endif

SDL_PKGCONFIG?=sdl3
$(eval $(call Use_pkg_config,SDL))

ifdef MINGW
ifndef NOSDLMAIN
SDLMAIN=1
endif
endif

ifdef SDLMAIN
opts+=-DSDLMAIN
else
ifdef MINGW
opts+=-Umain
libs+=-mconsole
endif
endif
