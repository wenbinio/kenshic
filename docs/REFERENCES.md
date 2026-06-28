# External References

Projects we learn from but do **not** vendor (see [`../NOTICE.md`](../NOTICE.md)).

## RE_Kenshi / KenshiLib — GPLv3 — reference only
https://github.com/BFrizzleFoShizzle/RE_Kenshi

The most complete public reverse-engineering of Kenshi's engine. Hooks Ogre, physics,
heightmap, MyGUI, filesystem, sound, shader cache; reverses texture codecs; ships the
**KenshiLib** plugin framework. Useful as a cross-check for:

- Engine struct layouts and offset values (`GameWorld`, character arrays, zone manager).
- Hooking patterns against the fragile Ogre-based engine.
- The Ogre plugin-loader injection path.

**Rule:** read it to verify a fact (an offset, a struct field, a calling convention),
then implement independently in `mp/`. Never copy its source.

## Forgotten Construction Set (FCS) — Lo-Fi Games
Official content editor. Defines the data model `tools/ocs/` mirrors. Reference for the
`gamedata`/`.mod`/`.save` format (header `15`=save / `16`=mod; primitives `L`/`F`/`C`/`?`;
UTF-8 strings; mods as diffs over `gamedata.base`).

## Community offset/format sources
- Cheat Engine community pointer chains (offset fallbacks).
- KServerMod struct references (credited in KenshiMP offset comments).
- Steam community "gamedata/mod/save file format" guide.
