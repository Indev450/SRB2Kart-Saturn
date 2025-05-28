#
# Makefile for feature flags.
#

passthru_opts+=\
	NO_IPV6 NOHW NOMD5 NOPOSTPROCESSING\
	MOBJCONSISTANCY PACKETDROP ZDEBUG\
	HAVE_MINIUPNPC\
	HAVE_DISCORDRPC DEVELOP

# build with debugging information
ifdef DEBUGMODE
PACKETDROP=1
opts+=-DPARANOIA -DRANGECHECK
endif

ifndef NOHW
opts+=-DHWRENDER
sources+=$(call List,hardware/Sourcefile)
opts+=-DUSE_FBO_OGL
endif

ifndef NOMD5
opts+=-DNOMD5
sources+=md5.c
endif

ifndef NOOPENMPT
HAVE_OPENMPT=1
endif

ifdef NOBLUAJIT
opts+=-DNOBLUAJIT
endif

ifndef NONET
ifndef NOSATURNJOIN
opts+=-DDOSATURNJOIN
endif
ifndef NOSATURNPAK
opts+=-DDOSATURNPAK
endif
endif

ifndef NOZLIB
ifndef NOPNG
ifdef PNG_PKGCONFIG
$(eval $(call Use_pkg_config,PNG_PKGCONFIG))
else
PNG_CONFIG?=$(call Prefix,libpng-config)
$(eval $(call Configure,PNG,$(PNG_CONFIG) \
	$(if $(PNG_STATIC),--static),,--ldflags))
endif
ifdef LINUX
opts+=-D_LARGEFILE64_SOURCE
endif
opts+=-DHAVE_PNG
sources+=apng.c
endif
endif

ifndef NOCURL
CURLCONFIG?=curl-config
$(eval $(call Configure,CURL,$(CURLCONFIG)))
opts+=-DHAVE_CURL
endif

ifdef HAVE_MINIUPNPC
MINIUPNPC_PKGCONFIG?=miniupnpc
$(eval $(call Use_pkg_config,MINIUPNPC))
HAVE_MINIUPNPC=1
opts+=-DHAVE_MINIUPNPC
endif

ifdef HAVE_DISCORDRPC
$(eval $(call Propogate_flags,DISCORDRPC))
libs+=-ldiscord-rpc
opts+=-DUSE_STUN
sources+=discord.c stun.c
endif

# (Valgrind is a memory debugger.)
ifdef VALGRIND
VALGRIND_PKGCONFIG?=valgrind
VALGRIND_LDFLAGS=
$(eval $(call Use_pkg_config,VALGRIND))
ZDEBUG=1
opts+=-DHAVE_VALGRIND
endif

default_packages:=\
	GME/libgme/LIBGME\
	OPENMPT/libopenmpt/LIBOPENMPT\
	ZLIB/zlib\

$(foreach p,$(default_packages),\
	$(eval $(call Check_pkg_config,$(p))))
