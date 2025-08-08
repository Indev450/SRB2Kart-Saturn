makedir:=$(makedir)/Dedicated

sources+=$(call List,dedicated/Sourcefile)

opts+=-DDEDICATED

ifdef MINGW
libs+=-mconsole
endif

ifndef NOTHREADS
opts+=-DHAVE_THREADS
sources+=dedicated/i_threads.c
endif

NOOPENMPT=1
NOLIBGME=1
NOHW=1
NOUPNP=1
