#
# Mingw, if you don't know, that's Win32/Win64
#

ifndef MINGW64
EXENAME?=srb2kart-saturn_32bit.exe
else
EXENAME?=srb2kart-saturn_64bit.exe
endif

# disable dynamicbase if under msys2
ifdef MSYSTEM
libs+=-Wl,--disable-dynamicbase
endif

sources+=win32/Srb2win.rc
opts+=-DSTDC_HEADERS
libs+=-ladvapi32 -lkernel32 -lmsvcrt -luser32

ifndef DEDICATED
ifndef DUMMY
SDL?=1
endif
endif

ifndef NOHW
opts+=-DUSE_WGL_SWAP
endif

ifdef MINGW64
libs+=-lws2_32
else
#ifndef HAVE_IPV6
#libs+=-lwsock32
#else
libs+=-lws2_32
#endif
endif

ifndef MINGW64
libs+=-Wl,--large-address-aware
NOLIBBACKTRACE=1
endif

ifndef MINGW64
32=32
x86=x86
i686=i686
else
32=64
x86=x86_64
i686=x86_64
endif

mingw:=$(i686)-w64-mingw32

define _set =
$(1)_CFLAGS?=$($(1)_opts)
$(1)_LDFLAGS?=$($(1)_libs)
endef

lib:=../libs/gme
LIBGME_opts:=-I$(lib)/include
LIBGME_libs:=-L$(lib)/win$(32) -lgme
$(eval $(call _set,LIBGME))

lib:=../libs/libopenmpt
LIBOPENMPT_opts:=-I$(lib)/inc
LIBOPENMPT_libs:=-L$(lib)/lib/$(x86) -lopenmpt
$(eval $(call _set,LIBOPENMPT))

lib:=../libs/bluajit
BLUAJIT_opts:=-I$(lib)/src
BLUAJIT_libs:=-L$(lib)/lib/$(x86) -lbluajit
$(eval $(call _set,BLUAJIT))

lib:=../libs/SDL2_mixer/$(mingw)

ifdef SDL
mixer_opts:=-I$(lib)/include/SDL2
mixer_libs:=-L$(lib)/lib

lib:=../libs/SDL2/$(mingw)
SDL_opts:=-I$(lib)/include/SDL2\
	$(mixer_opts) -Dmain=SDL_main
SDL_libs:=-L$(lib)/lib $(mixer_libs)\
	-lmingw32 -lSDL2main -lSDL2 -mwindows
$(eval $(call _set,SDL))
endif

ifdef MINGW64
lib:=../libs/libbacktrace
LIBBACKTRACE_opts:=-I$(lib)/include
LIBBACKTRACE_libs:=-L$(lib)/lib/x86_64 -lbacktrace
$(eval $(call _set,LIBBACKTRACE))
else
lib:=../libs/drmingw
opts+=-I$(lib)/include
libs+=-L$(lib)/lib/win32 -lmgwhelp -lexchndl
endif

lib:=../libs/zlib
ZLIB_opts:=-I$(lib)
ZLIB_libs:=-L$(lib)/win32 -l:libz$(32).a
$(eval $(call _set,ZLIB))

ifndef PNG_CONFIG
lib:=../libs/libpng-src
PNG_opts:=-I$(lib)
PNG_libs:=-L$(lib)/projects -lpng$(32)
$(eval $(call _set,PNG))
endif

lib:=../libs/curl
CURL_opts:=-I$(lib)/include
CURL_libs:=-L$(lib)/lib$(32) -lcurl
$(eval $(call _set,CURL))

ifndef MINGW64 # miniupnc is broken with MINGW64
lib:=../libs/miniupnpc
MINIUPNPC_opts:=-I$(lib)/include -DMINIUPNP_STATICLIB
MINIUPNPC_libs:=-L$(lib)/mingw$(32) -lminiupnpc -lws2_32 -liphlpapi
$(eval $(call _set,MINIUPNPC))
endif

lib:=../libs/discord-rpc/win$(32)-dynamic
DISCORDRPC_opts+=-I$(lib)/include
DISCORDRPC_libs+=-L$(lib)/lib -ldiscord-rpc
$(eval $(call _set,DISCORDRPC))
