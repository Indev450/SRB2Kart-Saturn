# SRB2Kart: Saturn

[SRB2Kart](https://srb2.org/mods/) is a kart racing mod based on the 3D Sonic the Hedgehog fangame [Sonic Robo Blast 2](https://srb2.org/), based on a modified version of [Doom Legacy](http://doomlegacy.sourceforge.net/).

## Saturn Features (Take a look at the "Saturn Options" menu c;)

- Sprites player rotation on slopes; going on hills, levitate, going down, and so on alters your sprites (configurable)
- You can alter all the base kart HUD element offsets on the fly in the Saturn Options settings menu
- `addfilelocal` Command, for those who want to use things like their custom hud in netgames (Same as addfile but completely client sided)
- Palette rendering, make the OpenGL renderer (almost) look like software mode!
- OpenMPT for Tracker module playback instead of GME
  - Fixes looping and sounds better
  - Allows filter configuration
- Colourize your Kart HUD, to either your player color or whichever colour you like! (optional / requieres extra2.kart file)
- The minimal brightness with Shaders can be configured, now you can seeeee in dark areas (0-255 , uses Sector brightness values)
- Lua HUD hooks for Intermission (ex: You can make hostmod votes still visible during intermission!)
- Lua HUD hooks for Vote Screen (ex: You can make hostmod votes still visible during map vote screen!)
- Saltyhop! do a funny hop
- Smaller Speedometer (optional / requieres extra.kart file)
- Record Attack input display can now be used everywhere
- Smaller minimap icons and also show player names on the minimap (toggable)
- Support for animated votescreens!
- `showmusiccredit` command, shows you the current music track again
- `listskins` show a list of all skins currently loaded
- Visual Portals from Haya's HEP Client
- Horizonline effect in OpenGL (watch the endless bricks on SNES Bowsers Castle!)
- Localskins! use whatever skin you like!
- Toggable lowercase menu's also from Haya's HEP client
- Everything that Galaxy has

## Improvements / Bug fixes

- Replay size has been increased to 10MiB minimum, to make replay overflows very unlikely to happen
  - can be set to 100MiB max with `maxdemosize` command in console
- Music changes now properly show the music credits (ex: Kart maps with easter eggs that alter the music)
 - The mapper still has to provide a MusicDEF for this to work.
- Configurable chatlog length (yay can read those messages from 30 mins ago without opening log.txt)
- Configurable timeout for waiting to join a full server (in vanilla this is capped to 5 minutes before it boots you out)
- Characters now spin in skin selection menu (configurable)
- HTTP Addon download speed has been raised dramatically
  - configurable with `downloadspeed` command
- HWR Drawnodes have been refactored, this should fix a few rare crashes esp in regards to OpenGL Visportals
- The Position Number in the corner is alot smaller now (half the size!)
- Toggable Lap animation
- Main menu shows the current renderer being used
- MSAA and A2C Antialiasing support (Configurable in renderer.txt file)
- A simple button to rejoin your last visited server
- Ability to show ALL maps in the mapselect screen, this includes hidden or hellmaps "showallmaps yes/no"
- Ability to toggle the Stagetitlecard "maptitles on/off"

## Performance / Debugging

- Access for `mobj_t` and `player_t` fields from Lua has been completely rewritten, for much better performance with many Lua scripts
- Lua shows Tracebacks whenever an error occurs
- The Lua Perfstats page now has multiple pages and is more organized (ps_thinkframe_page X in console)
- People with lower end hardware can disable "screen textures" with "gr_screentextures" command, huge performance gain with minimal visual loss
  - Intermission backgrounds, heat wave effects and etc are broken with this
- `ffloorclip`, exclusive to Software, which boosts performance on maps with many Floor over Floor sectors

## Technical fixes

- ZFigthing Textures in Opengl mode have been fixed in engine (well almost all of it)
- FOFs intersecting with slopes have less issues and now render correctly in Opengl
- Midtextures on slopes in Opengl are fixed, many weird guardrails and fences, aswell as some of those sticking out textures from the ground on some maps should be gone
- The annoying depraction messages for `P_TeleportMove` and `P_AproxDistance` have been silenced
- Silenced the annoying "cant find next map..." messages as theyre super useless and annoying anyways
- Added a small cooldown on `map` command due to spamming causing a SIGSEGV
- Many bugfixes to the Software renderer, so its much less prone to crashing
- Many bugfixes to the OpenGL renderer (too many too list all of them here; check the changelogs for more information)
- Bugfixes to the soundsystem, mainly regarding Sound muting
- Refactored Zone Memory Allocations (helps a bit with large maps)
- Many many more crash fixes and other bugfixes to the basegame (too many too list all of them here; check the changelogs for more information)

## MISC
- Bird's Camara Tilting feature is no longer turned on by default
- Bird's warning on the title screen has been removed

## Bugs
- On maps which change the song multiple times on map start (exp: Wandering Falls) may show multiple music credits appearing
- A2C antialiasing makes transparent surfaces even more transparent
- You tell us! c:

## Note
Check the LUASTUFF file for more information on all the lua things Saturn has.

## Dependencies
- SDL2 (Linux/OS X only)
- SDL2-Mixer (Linux/OS X only)
- libupnp (Linux/OS X only)
- libgme (Linux/OS X only)
- libopenmpt (Linux/OS X only)
- libbacktrace (Linux/OS X only)

## Compiling

See [SRB2 Wiki/Source code compiling](http://wiki.srb2.org/wiki/Source_code_compiling). The compiling process for SRB2Kart is largely identical to SRB2.

## Disclaimer
Kart Krew is in no way affiliated with SEGA or Sonic Team. We do not claim ownership of any of SEGA's intellectual property used in SRB2.
