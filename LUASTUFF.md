# SRB2Kart Saturn: Custom lua features

## mobj.localskin and player.localskin fields

Allow reading and assigning localskins.

## mobj.rollsum field

**Not netsafe**. Returns total sprite rotation value, taking sliptideroll and sloperoll cvars value into account.
Mainly for use in visual-only addons like secondcolor.

## player.sliproll field

**Not netsafe**. Returns player's current sliptide roll angle, taking sliptideroll cvar value into account.

## hud.setVoteBackground(name)

Changes vote background texture prefix (game looks for PREFIX + "W" or "C" (depends on resolution) + FRAME\_NUMBER).

## hud.add(fn, "vote") and hud.add(fn, "intermission")

Hud hooks for vote screen and intermission screen. They only get drawer (`v`) as argument.
To check if those hooks are available, check if globals FEATURE\_VOTEHUD and FEATURE\_INTERMISSIONHUD
exist.

## x, y = hud.getOffsets(item)

Returns the values of the given HUD item's `xoffset`/`yoffset` cvars.
Available for any HUD item with offset cvars.

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
