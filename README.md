# SRB2Kart: Saturn

[SRB2Kart](https://srb2.org/mods/) is a kart racing mod based on the 3D Sonic the Hedgehog fangame [Sonic Robo Blast 2](https://srb2.org/), based on a modified version of [Doom Legacy](http://doomlegacy.sourceforge.net/).

## Saturn Features
- Player rotation on slopes, funny squish when landing, Sprite scaling and all that 
- Kart Hud offsets are user changable
- Access for `mobj_t` and `player_t` fields from Lua has been completely rewritten, for much better performance with many Lua scripts
- Lua shows tracebacks whenever an error occurs
- The Lua perfstat page now has multiple pages and is more organized (ps_thinkframe_page X)
- "addfilelocal" Command, for those who want to use their custom hud in netgames (Same as addfile but completely client sided)
- OpenMPT for Tracker module playback (Complete with filters) fixed looping issue while even sounding better than the old GME playback
- Music changes now properly show the music credits
- Configurable chatlog size
- Lua hooks for Intermission
- Timeout for waiting to join a full server is configurable now
- Characters now spin in skin selection menu (configurable)
- The minimal brightness with Shaders can be configured, now you can seeeee in dark areas (0-255 , uses Sector brightness values)
- People with lower end hardware can disable "screen textures" with "gr_screentextures" command, huge performance gain with minimal visual loss (Note intermission screen background is broken with this)
- Software mode has gotten ffloorclip, which boosts performance on maps with many Floor over Floor roads
- Visual Portals from Haya´s HEP Client
- Everything that Galaxy has

## Technical Stuff:

- ZFigthing Textures in Opengl mode have been fixed in engine (well almost all of it)
- Fof´s intersecting with slopes have less issues and now render correctly in Opengl
- Midtextures on slopes in Opengl are fixed, all those weird guardrails and fences, aswell as some of those sticking out textures from the ground on some maps should be gone
- Multiple other fixes
- Zone memory allocations have been rewritten, speeds up some map loading, and may improve performance on busy maps
- HWR Drawnodes have been refactored, this should fix a few rare crashes esp in regards to OpenGL Visportals
- The annoying depraction messages for "P_TeleportMove" and "P_AproxDistance" have been silenced
- Silenced the annoying "cant find next map..." messages as theyre super useless and annoying anyways
- HTTP Addon download speed has been raised dramatically

## Bugs

## Dependencies
- NASM (x86 builds only)
- SDL2 (Linux/OS X only)
- SDL2-Mixer (Linux/OS X only)
- libupnp (Linux/OS X only)
- libgme (Linux/OS X only)
- libopenmpt (Linux/OS X only)

## Compiling

See [SRB2 Wiki/Source code compiling](http://wiki.srb2.org/wiki/Source_code_compiling). The compiling process for SRB2Kart is largely identical to SRB2.

## Disclaimer
Kart Krew is in no way affiliated with SEGA or Sonic Team. We do not claim ownership of any of SEGA's intellectual property used in SRB2.
