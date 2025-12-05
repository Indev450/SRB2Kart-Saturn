# SRB2Kart Saturn: Custom lua features

## mobj.localskin and player.localskin fields

Allow reading and assigning localskins.

## mobj.rollsum field

**Not netsafe**. Returns total sprite rotation value, taking sliptideroll and sloperoll cvars value into account.
Mainly for use in visual-only addons like secondcolor.

## player.sliproll field

**Not netsafe**. Returns player's current sliptide roll angle, taking sliptideroll cvar value into account.

## hud.setVoteBackground(name[, time\_per\_frame])

Changes vote background texture prefix (game looks for PREFIX + "W" or "C" (depends on resolution) + FRAME\_NUMBER).

If time\_per\_frame is set, each frame will last this amount of tics, default value is 2

## hud.add(fn, "vote") and hud.add(fn, "intermission")

Hud hooks for vote screen and intermission screen. They only get drawer (`v`) as argument.
To check if those hooks are available, check if globals FEATURE\_VOTEHUD and FEATURE\_INTERMISSIONHUD
exist.

## x, y = hud.getOffsets(item)

Returns the values of the given HUD item's `xoffset`/`yoffset` cvars.
Available for any HUD item with offset cvars.

## patch = v.cachePatchRotated(name, rollangle)
Like v.cachePatch, it caches a new patch with the corresponding name. Unlike v.cachePatch, however,
a rollangle can be supplied to the function so that it returns a rotated patch instead.

## x, y, flags = v.getDrawInfo(item)

Returns the X, Y and flags where the given HUD item will be drawn for the current displayplayer.
Available for `item`, `gametypeinfo` and `minimap`.

## patch, colormap = v.getColorHudPatch(item)

Returns the patch and colormap to use for the given HUD item. Colorization is based on the user's settings.
Available for `item` and `itemmul`.
Extra arguments for some items:
### item
* `small`: true for small item box, false for big item box.
* `dark`: true to darken item box. Depends on `darkitembox`.
### itemmul
* `small`: true for small sticker, false for big sticker.

## hudcolor = v.getHudColor()

Returns the displayplayer's HUD color.

## colorize = v.useColorHud()

Returns true if colorization is enabled, false otherwise.

## v.interpolate(true)

Enables HUD interpolation. Each call to a drawing function will have its coordinates interpolated
by assigning an unique ID to every call.
For example, three successive calls to `v.draw` will each have their own ID and interpolation offsets.
Pass `true` to enable interpolation, `false` to disable.

Alternatively, pass a number to enable interpolation with the specified tag.
Tags allow HUD items drawn by the same call to a drawing function to be differentiated.
Valid tags range from 0-255.
For example, when iterating players, use a unique tag for each player to avoid artifacting:
```lua
for p in players.iterate do
	-- enable interpolation, using player numbers as the tag
	-- every draw will have different interp offsets per player
	v.interpolate(#p)

	-- doesn't matter for unconditional draws like this one...
	v.draw(...)

	-- but it does matter if there's a condition!
	-- without tags, this patch would warp between players
	-- whenever their `someVar` changes
	if p.someVar then v.draw(...) end
end

-- don't want interpolation anymore? then disable it
v.interpolate(false)
```
NOTE: The default tag for `true` is 0.

## v.interpLatch(true)

Enables interpolation offset latching. The next call to a drawing function will have its lerp offsets
saved and reused for all following draw calls. Disable with `false` when done.
Interpolation cannot easily "snap" between two points; latching can help overcome this by manually
setting the lerp offsets.
```lua
v.interpolate(true)

-- X coordinate for the HUD item
-- every so often, this will wrap around to zero
local x = (leveltime*4) % 256

-- draw an interpolated patch, but... when X rolls back to zero,
-- the patch will slide all the way across the screen!
v.draw(x, 100, ...)

-- this is where interpLatch comes into play
-- enable latching, then draw an invisible patch,
-- moving at the same speed as the visible patch
-- the exact coordinates don't matter, it just has to move at the right speed
v.interpLatch(true)
v.draw(leveltime*4, 0, v.cachePatch("K_TRNULL"))

-- now we can draw the patch again, and when X becomes zero,
-- it'll snap right back without sliding across the screen
v.draw(x, 150, ...)

-- don't forget to turn off latching!
v.interpLatch(false)
```
When using a custom string drawer, enable this mode to avoid interpolation artifacting when the string changes.

## cameras[] global array

Local player cameras that can be accessed outside of hud hooks.
`#cameras` will return amount of available cameras. (0 on dedicated server, 1 with 1 player, 4 with 4p splitscreen, etc)

## mobj.spritexscale, mobj.spriteyscale, mobj.spritexoffset, mobj.spriteyoffset, mobj.rollangle, mobj.sloperoll, mobj.rollmodel fields

Same fields as in SRB2 2.2

## S\_StopSoundByNum

Allows stopping specific sound globally. Because this function starts with S\_, like state, checking
if it exists is a bit more complicated:

```lua
if rawget(_G, "S_StopSoundByNum") ~= nil then
    ... -- Can use it
end
```
## addHook("ServerJoin", function())

ServerJoin hook called when client joins server (starting a listen server also calls it for host).
Generally it was made to make loading custom config files less hacky, but can be used for anything else too.

## player.viewrollangle field

Returns player's current view roll angle for the Screen Tilting feature, useful for HUD elements.

## G_SetPlayerGamepadIndicatorColor(player, skincolor)

Set a custom color for Supported Gamepads with RGB LED functionality.
To be used with Displayplayers.
Only takes Skincolors.
Best to be used in a Loop to ensure the color wont get overwritten by the game.

## G_PlayerDeviceRumble(player, low_strength, high_strength, -optional- duration)

Add Gamepad Rumble support for things.
To be used with Displayplayers.
Duration is in milliseconds and is optional to set, default value is 84ms.

## P_CheckSightFast(mo1, mo2)

Exactly same as P_CheckSight but uses cheaper algorithm, useful for things like nametags. Doesn't work exactly
like P_CheckSight so don't use it for anything gameplay-related.

## musicdef_t

Userdata structure representing a musicdef. Fields:

`musicdef.name` - song identifier ("kmap01" for example).

`musicdef.usage`, `musicdef.source` - fields from vanilla MUSICDEFS lump.

`musicdef.filename` - unused for now.

`musicdef.title`, `musicdef.alttitle`, `musicdef.authors` - fields from MUSCINFO lump.

`#musicdef` - returns integer id for musicdef (which can be used as index in `musicdefs`).

All fields are read-only.

## S_FindMusicCredit(name)

Returns musicdef corresponding to music with given identifier. For example, `S_FindMusicCredit("kmap01")` will return
musicdef for green hills music.

## musicdefs

Global table for all musicdefs, similar to mobjinfo, states, etc. Can take either integer indices,
from `0` to `#musicdefs-1`, or string indices (which is equal to calling `S_FindMusicCredit`).

## addHook("MusicCredit", function(musicdef))

Hook is called whenever `S_ShowMusicCredit` (either from game or mod) is called. Takes musicdef as only argument, returning true
will overwrite vanilla behavior (not show music credit), can be used to implement custom music credit pop-ups.

## addHook("SetupVote", function(result, gt, secondgt, prevmap))

Hook is called whenever server sets up options for vote screen. `result` is table where lua can store options to,
(for example, `result[1] = 1` will force green hills as first option), gt is current gametype, secondgt is
alternative gametype picked for 3rd entry (for example, gt can be `GT_RACE`, and secondgt can be `GT_MATCH`)
prevmap is the previous map that was played (small note - in game code, it is offset by -1, but for lua its offset back,
so if previous gamemap was 1, prevmap would also be 1. Which means you can safely do mapheaderinfo[prevmap] to get info on previous map)

Limitations: currently, game can only change gametype for 3rd option in map (meaning for example, if current gametype is race, and you
put battle map in first option, when it gets picked that map will be still run in race gametype). That means, game will only handle maps with
different gametype if it is put in 3rd vote option.

# Other changes

## P_PlayRinglossSound(source, damager)

Add optional `damager` argument (should be `mobj_t`), which causes hurt sound also play for damager
if `karthitemdialog` option is enabled

## K_PlayHitEmSound(mobj, victim)

Add optional `victim` argument (should be a player's `mobj_t`), which causes "hit em" sound to be
delayed and played for victim too if `karthitemdialog` option is enabled
