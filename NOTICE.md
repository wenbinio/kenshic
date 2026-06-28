# Provenance & Licensing

This repository vendors and adapts third-party open-source work. Each upstream's
original `LICENSE` is preserved in place inside its directory.

## Vendored (code present in this repo)

| Component | Path | Upstream | License | Notes |
|---|---|---|---|---|
| KenshiMP | `mp/` | https://github.com/The404Studios/Kenshi-Online | **MIT** (`mp/LICENSE`) | Our active fork. Prebuilt binaries (`dist/`), the experimental `rebuild/` tree, and `*.mod` backups were excluded at import; source, docs, and tests retained. |
| OpenConstructionSet | `tools/ocs/` | https://github.com/lmaydev/OpenConstructionSet | **MIT** (`tools/ocs/LICENSE`) | C# SDK for Kenshi data/save files. `bin/`/`obj/` excluded. |

KenshiMP bundles several common libraries under `mp/lib/` (enet, imgui, nlohmann/json,
minhook, spdlog), fetched at build time; each carries its own permissive license
(MIT/zlib/BSD). They are dependencies, not original work of this project.

## Referenced only (NOT vendored — do not copy code in)

| Component | Upstream | License | Why not vendored |
|---|---|---|---|
| RE_Kenshi / KenshiLib | https://github.com/BFrizzleFoShizzle/RE_Kenshi | **GPLv3** | Copyleft. Linking/copying its code would relicense this entire project as GPLv3. We use it strictly as a **reference** for reverse-engineered engine offsets and hooking techniques. Offset *values* and structural facts are not themselves copyrightable; its *source code* must stay out of this tree. |

KenshiMP's source contains comments crediting `KenshiLib`/`KServerMod` as the origin of
certain verified offset values (e.g. `mp/KenshiMP.Core/game/game_types.h`). These are
attribution comments and factual offset constants, not copied GPL source.

## This project's license

Because RE_Kenshi is kept out of the tree, this repository can remain under a permissive
license (intended: MIT, matching the KenshiMP base). **Do not paste RE_Kenshi source
into this repo** without first relicensing the whole project to GPLv3 — that is a
deliberate decision, not a default.

## Affiliation

Not affiliated with or endorsed by Lo-Fi Games. Kenshi is © Lo-Fi Games. This is an
unofficial, community-built mod that requires a legitimate copy of the game.
