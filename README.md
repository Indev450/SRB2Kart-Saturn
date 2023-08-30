# SRB2Kart: Saturn

[SRB2Kart](https://srb2.org/mods/) is a kart racing mod based on the 3D Sonic the Hedgehog fangame [Sonic Robo Blast 2](https://srb2.org/), based on a modified version of [Doom Legacy](http://doomlegacy.sourceforge.net/).

## Saturn Features (Take a look at the "Saturn Options" menu c;)

- Sprites player rotation on slopes; going on hills, levitate, going down, and so on alters your sprites (configurable)
- You can alter all the base kart HUD element offsets on the fly in the Saturn Options settings menu
- "addfilelocal" Command, for those who want to use things like their custom hud in netgames (Same as addfile but completely client sided)
- OpenMPT for Tracker module playback (Complete with filters) fixed looping issues while even sounding better than the old GME playback
- The minimal brightness with Shaders can be configured, now you can seeeee in dark areas (0-255 , uses Sector brightness values)
- Lua hooks for Intermission (ex: You can make hostmod votes still visible during intermission!)
- Lua hooks for Vote Screen (ex: You can make hostmod votes still visible during map vote screen!)
- Saltyhop! do a funny hop
- Smaller Speedometer (optional / requieres extra.kart file)
- Record Attack input display can now be used everywhere
- Smaller minimap icons and also show player names on the minimap (toggable)
- "showmusiccredits" command, shows you the current music track again
- "listskins" show a list of all skins currently loaded
- Visual Portals from Haya´s HEP Client
- Toggable lowercase menu´s also from Haya´s HEP client
- Everything that Galaxy has

## Improvements / Bug fixes

- Replay size has been increased to 10MiB minimum, to make replay overflows very unlikely to happen (can be set to 100MiB max with "maxdemosize" command in console)
- Music changes now properly show the music credits (ex: Kart maps with easter eggs that alter the music) *The mapper still has to provide a MusicDEF for this to work.
- Configurable chatlog length (yay can read those messages from 30 mins ago without opening log.txt)
- Configurable timeout for waiting to join a full server (in vanilla this is capped to 5 minutes before it boots you out)
- Characters now spin in skin selection menu (configurable)
- HTTP Addon download speed has been raised dramatically (configurable with "downloadspeed" command)
- HWR Drawnodes have been refactored, this should fix a few rare crashes esp in regards to OpenGL Visportals
- The Position Number in the corner is alot smaller now (half the size!)
- Toggable Lap animation
- Main menu shows the current renderer being used
- MSAA and A2C Antialiasing support (Configurable in renderer.txt file)

## Performance / Debugging

- Access for `mobj_t` and `player_t` fields from Lua has been completely rewritten, for much better performance with many Lua scripts
- Lua shows Tracebacks whenever an error occurs
- The Lua Perfstats page now has multiple pages and is more organized (ps_thinkframe_page X in console)
- People with lower end hardware can disable "screen textures" with "gr_screentextures" command, huge performance gain with minimal visual loss (Note intermission screen background is broken with this)
- Software mode has gotten "ffloorclip", which boosts performance on maps with many Floor over Floor roads

## Technical fixes

- ZFigthing Textures in Opengl mode have been fixed in engine (well almost all of it)
- Fof's intersecting with slopes have less issues and now render correctly in Opengl
- Midtextures on slopes in Opengl are fixed, many weird guardrails and fences, aswell as some of those sticking out textures from the ground on some maps should be gone
- The annoying depraction messages for "P_TeleportMove" and "P_AproxDistance" have been silenced
- Silenced the annoying "cant find next map..." messages as theyre super useless and annoying anyways
- Added a small cooldown on "map" command due to spamming causing a SIGSEGV

## MISC
- Bird's Camara Tilting feature is no longer turned on by default
- Bird's warning on the title screen has been removed

## Bugs
- On maps which change the song multiple times on map start (exp: Wandering Falls) may show multiple music credits appearing
- A2C antialiasing makes transparent surfaces even more transparent
- You tell us! c:

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
