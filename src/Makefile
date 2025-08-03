# GNU Makefile for SRB2
# the poly3 Makefile adapted over and over...
#
# Copyright 1998-2000 DooM Legacy Team.
# Copyright 2020-2023 James R.
# Copyright 2003-2023 Sonic Team Junior.
#
# This program is free software distributed under the
# terms of the GNU General Public License, version 2.
# See the 'LICENSE' file for more details.
#
# Special targets:
#
# clean - remove executables and objects for this build
# cleandep - remove dependency files for this build
# distclean - remove entire executable, object and
#             dependency file directory structure.
# dump - disassemble executable
# info - print settings
#
# This Makefile can automatically detect the host system
# as well as the compiler version. If system or compiler
# version cannot be detected, you may need to set a flag
# manually.
#
#
# Platform/system flags:
#
# LINUX=1, LINUX64=1
# MINGW=1, MINGW64=1 - Windows (MinGW toolchain)
# UNIX=1 - Generic Unix like system
# FREEBSD=1
# SDL=1 - Use SDL backend. SDL is the only implemented backend though.
#         If disabled, a dummy backend will be used.
#
# A list of supported GCC versions can be found in
# Makefile.d/detect.mk -- search 'gcc_versions'.
#
# Feature flags:
#
# Safe to use online
# ------------------
# NOHW=1 - Disable OpenGL renderer.
# NOSCREENFBO=1 - Disable screen FBO for downsampling in OpenGL.
# NOSATURNPAK=1 - Disable handling the special Saturn packet and resynch system
# NOSATURNJOIN=1 - Disable the seperate Saturn allowjoin toggle
# ZDEBUG=1 - Enable more detailed memory debugging
# HAVE_MINIUPNPC=1 - Enable automated port forwarding.
#                    Already enabled by default for 32-bit
#                    Windows.
# NOCURL=1 - Disable libcurl--HTTP capability.
# NOGME=1 - Disable game music emu, retro VGM support.
# NOOPENMPT=1 - Disable module (tracker) music support.
# NOMIXER=1 - Disable SDL Mixer (audio playback).
# NOTHREADS=1 - Disable multithreading.
#
# Netplay incompatible
# --------------------
# NOMD5=1 - Disable MD5 checksum (validation tool).
# NOPOSTPROCESSING=1 - ?
# MOBJCONSISTANCY=1 - ??
# PACKETDROP=1 - ??
# NOEXECINFO=1 - Disable stack trace dump support
# DEBUGMODE=1 - Enable various debugging capabilities.
#               Also disables optimizations.
#
# Development flags:
#
# VALGRIND=1 - Enable Valgrind memory debugging support.
# PROFILEMODE=1 - Enable performance profiling (gprof).
# USEASAN=1 - Enable building with Address Sanitizer.
# NOLIBBACKTRACE=1 - Disable using Libbacktrace for crashlog traces.
#
# General flags for building:
#
# STATIC=1 - Use static linking.
# DISTCC=1
# CCACHE=1
# UPX= - UPX command to use for compressing final
#        executable.
# WINDOWSHELL=1 - Use Windows commands.
# PREFIX= - Prefix to many commands, for cross compiling.
# STABS=1 - ?
# ECHO=1 - Print out each command in the build process.
# NOECHOFILENAMES=1 - Don't print out each that is being
#                     worked on.
# SILENT=1 - Print absolutely nothing except errors.
# RELAXWARNINGS=1 - Use less compiler warnings/errors.
# ERRORMODE=1 - Treat most compiler warnings as errors.
# NOCASTALIGNWARN=1 - ?
# NOLDWARNING=1 - ?
# NOSDLMAIN=1 - ?
# SDLMAIN=1 - ?
# USELTO=1 - Enable building with LTO.
#
# Library configuration flags:
# Everything here is an override.
#
# PNG_PKGCONFIG= - libpng-config command.
# PNG_CFLAGS=, PNG_LDFLAGS=
#
# CURLCONFIG= - curl-config command.
# CURL_CFLAGS=, CURL_LDFLAGS=
#
# VALGRIND_PKGCONFIG= - pkg-config package name.
# VALGRIND_CFLAGS=, VALGRIND_LDFLAGS=
#
# LIBGME_PKGCONFIG=, LIBGME_CFLAGS=, LIBGME_LDFLAGS=

# LIBOPENMPT_PKGCONFIG=
# LIBOPENMPT_CFLAGS=, LIBOPENMPT_LDFLAGS=
#
# ZLIB_PKGCONFIG=, ZLIB_CFLAGS=, ZLIB_LDFLAGS=
#
# SDL_PKGCONFIG=
# SDL_CONFIG= - sdl-config command.
# SDL_CFLAGS=, SDL_LDFLAGS=

clean_targets=cleandep clean distclean info

.PHONY : $(clean_targets) all

<<<<<<< HEAD
 # go for a 32-bit sdl mingw exe by default
 MINGW=1
 SDL=1
 WINDOWSHELL=1
 HAVE_DISCORDRPC=0
 NOUPX=1
=======
goals:=$(or $(MAKECMDGOALS),all)
cleanonly:=$(filter $(clean_targets),$(goals))
destructive:=$(filter-out info,$(cleanonly))
>>>>>>> Saturn-Next

ifndef cleanonly
include Makefile.d/old.mk
endif

include Makefile.d/util.mk

<<<<<<< HEAD
# SRB2 data files
D_DIR?=../bin/Resources
D_FILES=$(D_DIR)/srb2.srb \
	$(D_DIR)/patch.dta \
	#$(D_DIR)/music.dta \
	$(D_DIR)/gfx.kart \
	$(D_DIR)/textures.kart \
	$(D_DIR)/chars.kart \
	$(D_DIR)/maps.kart \
	$(D_DIR)/sounds.kart \
	$(D_DIR)/patch.kart \
	$(D_DIR)/music.kart \

PKG_CONFIG?=pkg-config

ifdef PANDORA
LINUX=1
endif

ifdef LINUX64
LINUX=1
NONX86=1
# LINUX64 does not imply X86_64=1; could mean ARM64 or Itanium
endif

ifdef MINGW64
MINGW=1
NONX86=1
# MINGW64 should not necessarily imply X86_64=1, but we make that assumption elsewhere
# Once that changes, remove this
X86_64=1
endif #ifdef MINGW64

ifdef HAIKU
SDL=1
endif

include Makefile.cfg

ifdef DUMMY
NOPNG=1
NOZLIB=1
NONET=1
NOHW=1
NOIPX=1
EXENAME?=srb2dummy
OBJS=$(OBJDIR)/i_video.o
LIBS=-lm
endif

ifdef HAIKU
NOIPX=1
ifndef NONET
LIBS=-lnetwork
endif
CFLAGS+=-DUNIXCOMMON
PNG_CFLAGS?=
PNG_LDFLAGS?=-lpng
endif

ifdef PANDORA
NONX86=1
NOHW=1
endif

ifndef NOOPENMPT
HAVE_OPENMPT=1
endif

ifndef NOHW
ifndef NOSCREENFBO
USE_FBO_OGL=1
endif
endif

ifdef MINGW
include win32/Makefile.cfg
endif #ifdef MINGW

ifdef UNIX
UNIXCOMMON=1
endif

ifdef LINUX
UNIXCOMMON=1
ifndef NOGME
HAVE_LIBGME=1
endif
endif

ifdef SOLARIS
UNIXCOMMON=1
endif

ifdef FREEBSD
UNIXCOMMON=1
endif

ifdef MACOSX
UNIXCOMMON=1
endif

ifdef SDL
	include sdl/Makefile.cfg
endif #ifdef SDL

ifdef DISTCC
        CC:=distcc $(CC)
endif

ifdef CCACHE
        CC:=ccache $(CC)
endif

MSGFMT?=msgfmt

ifndef ECHO
	REMOVE:=@$(REMOVE)
	CC:=@$(CC)
	CXX:=@$(CXX)
	OBJCOPY:=@$(OBJCOPY)
	OBJDUMP:=@$(OBJDUMP)
	STRIP:=@$(STRIP)
	WINDRES:=@$(WINDRES)
	CP:=@$(CP)
	MKDIR:=@$(MKDIR)
	MKISOFS:=@$(MKISOFS)
	DD:=@$(DD)
	NDSTOOL:=@$(NDSTOOL)
	GZIP:=@$(GZIP)
	MSGFMT:=@$(MSGFMT)
	UPX:=@$(UPX)
	UPX_OPTS+=-q
endif

ifdef NONET
	OPTS+=-DNONET
	NOCURL=1
else
ifdef NO_IPV6
	OPTS+=-DNO_IPV6
endif
endif

ifdef NOHW
	OPTS+=-DNOHW
else
	OPTS+=-DHWRENDER
	OBJS+=$(OBJDIR)/hw_bsp.o $(OBJDIR)/hw_draw.o \
		 $(OBJDIR)/hw_main.o $(OBJDIR)/hw_clip.o $(OBJDIR)/hw_md2.o $(OBJDIR)/hw_cache.o \
		 $(OBJDIR)/hw_md2load.o $(OBJDIR)/hw_md3load.o $(OBJDIR)/hw_model.o $(OBJDIR)/u_list.o $(OBJDIR)/hw_batching.o $(OBJDIR)/hw_shaders.o
endif

OPTS += -DCOMPVERSION

ifndef NONX86
ifndef GCC29
	ARCHOPTS?=-msse3 -mfpmath=sse
else
	ARCHOPTS?=-mpentium
endif
else
ifdef X86_64
	ARCHOPTS?=-march=native
endif
endif


ifndef NOPNG
OPTS+=-DHAVE_PNG

ifdef PNG_PKGCONFIG
PNG_CFLAGS?=$(shell $(PKG_CONFIG) $(PNG_PKGCONFIG) --cflags)
PNG_LDFLAGS?=$(shell $(PKG_CONFIG) $(PNG_PKGCONFIG) --libs)
else
=======
>>>>>>> Saturn-Next
ifdef PREFIX
CC:=$(PREFIX)-gcc
CXX:=$(PREFIX)-g++
endif

OBJDUMP_OPTS?=--wide --source --line-numbers

OBJCOPY?=$(call Prefix,objcopy)
OBJDUMP?=$(call Prefix,objdump)
WINDRES?=$(call Prefix,windres)

GZIP?=gzip
GZIP_OPTS?=-9 -f -n
ifdef WINDOWSHELL
GZIP_OPTS+=--rsyncable
endif

UPX_OPTS?=--best --preserve-build-id
ifndef ECHO
UPX_OPTS+=-qq
endif

include Makefile.d/detect.mk

# make would try to remove the implicitly made directories
.PRECIOUS : %/ comptime.c

sources:=
makedir:=../make

# -DCOMPVERSION: flag to use comptime.h
opts:=-DCOMPVERSION -g
libs:=

# This is a list of variables names, of which if defined,
# also defines the name as a macro to the compiler.
passthru_opts:=

include Makefile.d/platform.mk
include Makefile.d/features.mk
include Makefile.d/versions.mk

ifdef DEBUGMODE
makedir:=$(makedir)/debug
endif

depdir:=$(makedir)/deps
objdir:=$(makedir)/objs

# very sophisticated dependency
sources+=\
	$(call List,Sourcefile)\
	$(call List,core/Sourcefile)

sources+=\
	$(call List,blua/Sourcefile)

depends:=$(basename $(filter %.c %.cpp %.s,$(sources)))
objects:=$(basename $(filter %.c %.cpp %.s %.nas,$(sources)))

depends:=$(depends:%=$(depdir)/%.d)

# comptime.o added directly to objects instead of thru
# sources because comptime.c includes comptime.h, but
# comptime.h may not exist yet. It's a headache so this is
# easier.
objects:=$(objects:=.o) comptime.o

# windows resource file
rc_file:=$(basename $(filter %.rc,$(sources)))
ifdef rc_file
objects+=$(rc_file:=.res)
endif

objects:=$(addprefix $(objdir)/,$(objects))

ifdef DEBUGMODE
bin:=../bin/debug
else
bin:=../bin
endif

# default EXENAME (usually set by platform)
EXENAME?=srb2kart
DBGNAME?=$(EXENAME).debug

exe:=$(bin)/$(EXENAME)
dbg:=$(bin)/$(DBGNAME)

build_done==== Build is done, look for \
           $(<F) at $(abspath $(<D)) ===

all : $(exe)
	$(call Echo,$(build_done))

ifndef VALGRIND
dump : $(dbg).txt
endif

ifdef STATIC
libs+=-static
endif

# build with profiling information
ifdef PROFILEMODE
opts+=-pg
libs+=-pg
endif

ifdef DEBUGMODE
<<<<<<< HEAD

	# build with debugging information
	WINDRESFLAGS = -D_DEBUG
ifdef GCC48
	CFLAGS+=-Og
else
	CFLAGS+=-O0
endif
	CFLAGS+= -Wall -DPARANOIA -DRANGECHECK -DPACKETDROP -DMOBJCONSISTANCY
else

	# build a normal optimised version
	WINDRESFLAGS = -DNDEBUG
	CFLAGS+=-Ofast
	CPPFLAGS+=-Ofast
endif
	CFLAGS+=-g $(OPTS) $(ARCHOPTS) $(WINDRESFLAGS)

ifdef PROFILEMODE
	# build with profiling information
	CFLAGS+=-pg
	LDFLAGS+=-pg
endif

ifdef ZDEBUG
	CPPFLAGS+=-DZDEBUG
endif

ifdef DUMPCONSISTENCY
	CPPFLAGS+=-DDUMPCONSISTENCY
endif

ifdef DEVELOP
	CPPFLAGS+=-DDEVELOP
endif

OPTS+=$(CPPFLAGS)

# default EXENAME if all else fails
EXENAME?=srb2
DBGNAME?=$(EXENAME).debug

# $(OBJDIR)/dstrings.o \

# not too sophisticated dependency
# SRB2kart kart.o on line 433 below
OBJS:=$(i_main_o) \
		$(OBJDIR)/comptime.o \
		$(OBJDIR)/qs22j.o   \
		$(OBJDIR)/string.o   \
		$(OBJDIR)/d_main.o   \
		$(OBJDIR)/d_protocol.o   \
		$(OBJDIR)/d_clisrv.o \
		$(OBJDIR)/d_net.o    \
		$(OBJDIR)/d_netfil.o \
		$(OBJDIR)/d_netcmd.o \
		$(OBJDIR)/dehacked.o \
		$(OBJDIR)/z_zone.o   \
		$(OBJDIR)/f_finale.o \
		$(OBJDIR)/f_wipe.o   \
		$(OBJDIR)/g_game.o   \
		$(OBJDIR)/g_input.o  \
		$(OBJDIR)/am_map.o   \
		$(OBJDIR)/command.o  \
		$(OBJDIR)/console.o  \
		$(OBJDIR)/hu_stuff.o \
		$(OBJDIR)/y_inter.o  \
		$(OBJDIR)/st_stuff.o \
		$(OBJDIR)/k_director.o   \
		$(OBJDIR)/k_kart.o   \
		$(OBJDIR)/m_aatree.o \
		$(OBJDIR)/m_anigif.o \
		$(OBJDIR)/m_argv.o   \
		$(OBJDIR)/m_bbox.o   \
		$(OBJDIR)/m_cheat.o  \
		$(OBJDIR)/m_cond.o   \
		$(OBJDIR)/m_fixed.o  \
		$(OBJDIR)/m_menu.o   \
		$(OBJDIR)/m_misc.o   \
		$(OBJDIR)/m_perfstats.o \
		$(OBJDIR)/m_random.o \
		$(OBJDIR)/m_queue.o  \
		$(OBJDIR)/info.o     \
		$(OBJDIR)/p_ceilng.o \
		$(OBJDIR)/p_enemy.o  \
		$(OBJDIR)/p_floor.o  \
		$(OBJDIR)/p_inter.o  \
		$(OBJDIR)/p_lights.o \
		$(OBJDIR)/p_map.o    \
		$(OBJDIR)/p_maputl.o \
		$(OBJDIR)/p_mobj.o   \
		$(OBJDIR)/p_polyobj.o\
		$(OBJDIR)/p_saveg.o  \
		$(OBJDIR)/p_setup.o  \
		$(OBJDIR)/p_sight.o  \
		$(OBJDIR)/p_spec.o   \
		$(OBJDIR)/p_telept.o \
		$(OBJDIR)/p_tick.o   \
		$(OBJDIR)/p_user.o   \
		$(OBJDIR)/p_slopes.o \
		$(OBJDIR)/tables.o   \
		$(OBJDIR)/r_bsp.o    \
		$(OBJDIR)/r_data.o   \
		$(OBJDIR)/r_draw.o   \
		$(OBJDIR)/r_fps.o    \
		$(OBJDIR)/r_main.o   \
		$(OBJDIR)/r_plane.o  \
		$(OBJDIR)/r_segs.o   \
		$(OBJDIR)/r_sky.o    \
		$(OBJDIR)/r_splats.o \
		$(OBJDIR)/r_patch.o  \
		$(OBJDIR)/r_things.o \
		$(OBJDIR)/screen.o   \
		$(OBJDIR)/v_video.o  \
		$(OBJDIR)/s_sound.o  \
		$(OBJDIR)/sounds.o   \
		$(OBJDIR)/w_wad.o    \
		$(OBJDIR)/filesrch.o \
		$(OBJDIR)/mserv.o    \
		$(OBJDIR)/http-mserv.o\
		$(OBJDIR)/i_tcp.o    \
		$(OBJDIR)/i_time.o   \
		$(OBJDIR)/lzf.o	     \
		$(OBJDIR)/b_bot.o \
		$(i_net_o)      \
		$(i_system_o)   \
		$(i_sound_o)    \
		$(OBJS)

# List of languages to compile.
# For reference, this is the command I use to build a srb2.pot file from the source code.
# (The listed source files are the ones containing translated strings).
# FILES=""; for file in `find ./ | grep "\.c" | grep -v svn`; do [ "`grep "M_GetText(" $file`" ] && FILES="$FILES $file"; done; xgettext -d srb2 -o locale/srb2.pot -kM_GetText -F --no-wrap $FILES
ifdef GETTEXT
POS:=$(BIN)/en.mo

OPTS+=-DGETTEXT
endif

ifdef PANDORA
all:	pre-build $(BIN)/$(PNDNAME)
endif

=======
debug_opts=-D_DEBUG
else # build a normal optimized version
debug_opts=-DNDEBUG
opts+=-O3
ifdef USELTO
# compile with LTO
>>>>>>> Saturn-Next
ifdef MINGW
opts+=-flto=auto -g
libs+=-flto=auto -g
else
opts+=-flto=auto
libs+=-flto=auto
endif
endif
endif

ifdef USEASAN
# compile with asan
ifdef MINGW
opts+=-fsanitize=address -g
libs+=-fsanitize=address -g
else
opts+=-fsanitize=address
libs+=-fsanitize=address
endif
opts+=-fsanitize=address
libs+=-fsanitize=address
endif

# debug_opts also get passed to windres
opts+=$(debug_opts)

opts+=$(foreach v,$(passthru_opts),$(if $($(v)),-D$(v)))

CXXFLAGS+=-std=c++17 # force c17 for c++ since most vpss run older ubuntu versions with old compilers

opts+=$(WFLAGS) $(CFLAGS)
cxxopts:=$(opts) $(CXXFLAGS)
opts+=$(CWFLAGS) # warning flags that shouldnt be passed to c++ compiler

libs+=$(LDFLAGS) -lstdc++ -lpthread
asflags:=$(ASFLAGS) -x assembler-with-cpp

cc=$(CC)
cxx=$(CXX)

ifdef DISTCC
cc=distcc $(CC)
cxx=distcc $(CXX)
endif

ifdef CCACHE
cc=ccache $(CC)
cxx=ccache $(CXX)
endif

ifndef SILENT
# makefile will 'restart' when it finishes including the
# dependencies.
ifndef MAKE_RESTARTS
ifndef destructive
$(shell $(CC) -v)
#$(shell $(CXX) -v)
define flags =

SHELL ..... $(SHELL)

CC ........ $(cc)

CXX ....... $(cxx)

CFLAGS .... $(opts)

CXXFLAGS .. $(cxxopts)

LDFLAGS ... $(libs)

endef
$(info $(flags))
endif
# don't generate dependency files if only cleaning
ifndef cleanonly
$(info Checking dependency files...)
include $(depends)
endif
endif
endif

LD:=$(CC)
cc:=$(cc) $(opts)
cxx:=$(cxx) $(cxxopts)
ifdef UPX
upx=$(UPX) $(UPX_OPTS)
endif
windres=$(WINDRES) $(WINDRESFLAGS)\
	$(debug_opts) --include-dir=win32 -O coff

%/ :
	$(.)$(mkdir) $(call Windows_path,$@)

# this is needed so the target can be referenced in the
# prerequisites
.SECONDEXPANSION :

# 'UPX' is also recognized in the environment by upx
unexport UPX

# executable stripped of debugging symbols
$(exe) : $(dbg) | $$(@D)/
	$(.)$(OBJCOPY) --strip-debug $< $@
	$(.)-$(OBJCOPY) --add-gnu-debuglink=$< $@
ifdef UPX
	$(call Echo,Compressing final executable...)
	$(.)-$(upx) $@
endif

# original executable with debugging symbols
$(dbg) : $(objects) | $$(@D)/
	$(call Echo,Linking $(@F)...)
	$(.)$(LD) -o $@ $^ $(libs)

# disassembly of executable
$(dbg).txt : $(dbg)
	$(call Echo,Dumping debugging info...)
	$(.)$(OBJDUMP) $(OBJDUMP_OPTS) $< > $@
	$(.)$(GZIP) $(GZIP_OPTS) $@

# '::' means run unconditionally
# this really updates comptime.h
comptime.c ::
ifdef WINDOWSHELL
	-$(.)..\comptime.bat .
else
	-$(.)../comptime.sh .
endif

# I wish I could make dependencies out of rc files :(
$(objdir)/win32/Srb2win.res : \
	win32/afxres.h win32/resource.h

# dependency recipe template
# 1: source file suffix
# 2: extra flags to gcc
define _recipe =
$(depdir)/%.d : %.$(1) | $$$$(@D)/
ifndef WINDOWSHELL
ifdef Echo_name
	@printf '%-20.20s\r' $$<
endif
endif
	$(.)$(2) -MM -MF $$@ -MT $(objdir)/$$*.o $(3) $$<
endef

$(eval $(call _recipe,c,$(cc)))
$(eval $(call _recipe,cpp,$(cxx)))
$(eval $(call _recipe,s,$(cc),$(asflags)))

# compiling recipe template
# 1: target file suffix
# 2: source file suffix
# 3: compile command
define _recipe =
$(objdir)/%.$(1) : %.$(2) | $$$$(@D)/
	$(call Echo_name,$$<)
	$(.)$(3)
endef

$(eval $(call _recipe,o,c,$(cc) -c -o $$@ $$<))
$(eval $(call _recipe,o,cpp,$(cxx) -c -o $$@ $$<))
$(eval $(call _recipe,o,s,$(cc) $(asflags) -c -o $$@ $$<))
$(eval $(call _recipe,res,rc,$(windres) -i $$< -o $$@))

_rm=$(.)$(rmrf) $(call Windows_path,$(1))

cleandep :
	$(call _rm,$(depends) comptime.h)

clean :
	$(call _rm,$(exe) $(dbg) $(dbg).txt $(objects))

distclean :
	$(call _rm,../bin ../objs ../dep ../make comptime.h)

info:
ifdef WINDOWSHELL
	@REM
else
	@:
endif

#$(warning The handwritten GNU Makefile for SRB2 is deprecated, and may be removed in the future. Please consider switching to CMake.)
