makedir:=$(makedir)/Dedicated

sources+=$(call List,dedicated/Sourcefile)

opts+=-DDEDICATED

ifdef MINGW
libs+=-mconsole
endif

NOOPENMPT=1
NOLIBGME=1
NOHW=1
NOUPNP=1
