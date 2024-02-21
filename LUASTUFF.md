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

Returns the values of the given HUD item's `xoffset`/`yoffset` cvars, or nil if unsupported.
Uses the same strings as `hud.enabled`.

## x, y, flags = v.getDrawInfo(item)

Returns the X, Y and flags where the given HUD item will be drawn for the current displayplayer.
Available for `item`, `gametypeinfo` and `minimap`.

## v.drawItemBox(x, y, flags, small, dark, colormap)

Draws an item box at the given coordinates.
`small`: true to draw small item box, false to draw large item box.
`dark`: true if item box should be darkened.
`colormap`: Colormap to draw the item box with. If nil, use displayplayer's HUD colormap.

## v.drawItemMul(x, y, flags, small, colormap)

Draws a multi-item sticker at the given coordinates.
`small`: true to draw small sticker, false to draw large sticker.
`colormap`: Colormap to draw the sticker with. If nil, use displayplayer's HUD colormap.

## hudcolor = v.getHudColor()

Returns the displayplayer's HUD color.

## colorize = v.useColorHud()

Returns true if colorization is enabled, false otherwise.

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
