# SRB2Kart Saturn: Custom lua features

## mobj.localskin and player.localskin fields

Allow reading and assigning localskins to players. Local skins for other objects aren't supported currently.

## mobj.rollsum field

**Not netsafe**. Returns total sprite rotation value, taking sliptideroll and sloperoll cvars value into account.
Mainly for use in visual-only addons like secondcolor.

## player.sliproll field

**Not netsafe**. Returns player's current sliptide roll angle, taking sliptideroll cvar value into account.

## hud.setVoteBackground(name)

Changes vote background texture prefix (game looks for PREFIX + "W" or "C" (depends on resolution) + FRAME\_NUMBER).

## hud.add(fn, "vote") and hud.add(fn, "intermission")

Hud hooks for vote screen and intermission screen. They only get drawer (`v`) as argument.
To check if those hooks are available, check if globals FEATURE\_VOTEHUD and FEATURE\_INERMISSIONHUD
exist.

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
