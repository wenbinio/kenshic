# kenshic — Kenshi Co-op

A cooperative-multiplayer mod for **Kenshi** (Lo-Fi Games), built by forking and
fixing the existing community multiplayer work. This repo is a working monorepo: the
active C++ mod, the supporting save/data tooling, and the design spec.

> Status: **active development** — upstream bases imported; first fixes landed
> (continuous combat health sync; ~1,050 LOC of dead code removed). No build has been
> run yet — all changes are inspection-verified only and need a Windows + Kenshi
> machine to compile and test. See [`docs/ROADMAP.md`](docs/ROADMAP.md) for the broken
> list and [`docs/STATUS.md`](docs/STATUS.md) for session-by-session progress.

## Layout

| Path | What it is | Upstream | License |
|---|---|---|---|
| `mp/` | The co-op mod (C++): injected client `Core.dll`, dedicated `Server`, `Injector`, scanner, tests. **Our active fork.** | The404Studios/Kenshi-Online (KenshiMP) | MIT |
| `tools/ocs/` | OpenConstructionSet — C# SDK for reading/writing Kenshi's `gamedata`/`mod`/`save` files. Used for save-format interop & content authoring. | lmaydev/OpenConstructionSet | MIT |
| `docs/` | Design spec, roadmap, provenance, and references. | — | — |
| `KENSHI_COOP_SPEC.md` | Full feasibility & engineering spec (engine constraints, sync subsystems, architecture). | — | — |

**RE_Kenshi** (BFrizzleFoShizzle) is **not** vendored here — it is **GPLv3**, and
copying it would force this whole codebase to GPLv3. We use it only as an external
*reference* for engine offsets/hook techniques. See
[`docs/REFERENCES.md`](docs/REFERENCES.md) and [`NOTICE.md`](NOTICE.md).

## Architecture (inherited from KenshiMP)

Server-authoritative: one server owns truth (entities, time, factions); the
`Core.dll` injects into `kenshi_x64.exe` via the Ogre plugin loader, hooks the sim, and
exchanges state over **ENet** (reliable UDP). The server runs in either topology:

- **P2P / listen server** (default UX): the host player's game runs the authoritative
  `GameServer` **in-process** on a background thread (`EmbeddedServer`); other players
  connect directly to the host. UPnP port-mapping + a firewall-rule fallback are
  attempted automatically. Host via the in-game **HOST GAME** button or `/host [port]`.
- **Dedicated server**: the same server logic (`KenshiMP.ServerLib`) as a standalone
  console exe for always-on worlds.

See `KENSHI_COOP_SPEC.md` §5 for why server-authority is the only tractable model, and
`mp/docs/` for the inherited protocol/offset docs.

## Building & testing

The mod is **Windows-only** (MSVC / Visual Studio 2022, CMake) and requires a real
Kenshi install to run — it injects into the live game binary. It cannot be compiled or
run in a headless Linux CI. See [`docs/ROADMAP.md`](docs/ROADMAP.md) for the dev/test
loop and `mp/docs/BUILD.md` for the inherited build instructions.
