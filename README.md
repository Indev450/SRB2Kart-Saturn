# SRB2Kart-Saturn

SRB2Kart-Saturn is a modification of SRB2Kart with a focus on new Features, Fixes, Optimizations and general Improvements, all while retaining 100% compatibility with vanilla SRB2Kart.

[SRB2Kart](https://srb2.org/mods/) is a kart racing mod based on the 3D Sonic the Hedgehog fangame [Sonic Robo Blast 2](https://srb2.org/), based on a modified version of [Doom Legacy](http://doomlegacy.sourceforge.net/).

For more information on Saturn´s features check out the changelogs on the [Releases](https://github.com/Indev450/SRB2Kart-Saturn/releases) page and the ingame "Saturn Options" menu.

Also be sure to check the LUASTUFF file for more information on all the extra lua features Saturn has.

We also have a Flatpak available on [Flathub](https://flathub.org/en/apps/org.srb2.SRB2Kart-Saturn)! (many thanks to riomccloud!)

## Dependencies
- SDL2 (Linux/OS X only)
- SDL2-Mixer (Linux/OS X only)
- libgme (Linux/OS X only)
- libopenmpt (Linux/OS X only)
- libbacktrace (Linux/OS X only, optional, disable support with NOLIBBACKTRACE)
- libupnp (Linux/OS X only, optional, enable support with HAVE_MINIUPNPC)

## Compiling

See [SRB2 Wiki/Source code compiling](http://wiki.srb2.org/wiki/Source_code_compiling). The compiling process for SRB2Kart is largely identical to SRB2.

Currently only the Makefile build system with GCC or Clang compiler is supported.

Visual C++ (MSVC) is unsupported.

64-bit Windows builds are supported and recommended over 32-bit builds, they will build automatically when using the MSYS2 MinGW 64-bit executable (requires the mingw-w64-x86_64-gcc package), no extra flags needed!

Alternatively the [srb2bld](https://github.com/Bijman/srb2bld) script by Bijman can be used aswell!

## Disclaimer
Kart Krew is in no way affiliated with SEGA or Sonic Team. We do not claim ownership of any of SEGA's intellectual property used in SRB2.
