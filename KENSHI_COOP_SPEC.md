# Full-Spec Kenshi Co-op Mod — Requirements & Engineering Spec

> What it would actually take to build a *complete* cooperative-multiplayer mod for
> Kenshi (Lo-Fi Games, 2018). This is a feasibility-grade engineering specification:
> the engine constraints, the prior art, the subsystems that must be synchronized,
> the reverse-engineering surface, and a realistic effort/risk estimate.

**Bottom line up front:** Kenshi has **zero** native networking. Every byte of
multiplayer must be bolted on from outside the engine through DLL injection and
function hooking, against a single-threaded, non-deterministic, ~20km-persistent-world
simulation that was never designed to be paused, forked, or replayed. A *playable*
small-group co-op is demonstrably achievable (see prior art). A *full-spec* co-op —
full single-player parity, stable, desync-free — is a multi-engineer, multi-year
reverse-engineering effort and arguably impossible to make perfectly faithful without
the source. This document scopes the gap between those two.

---

## 1. Why Kenshi has no multiplayer (the core constraints)

Kenshi is a custom C++ game built on **Ogre3D** (rendering), **MyGUI** (UI), and a
bespoke simulation layer. The constraints that make co-op hard are structural, not
incidental:

| Constraint | Consequence for co-op |
|---|---|
| **No networking layer at all** | The engine has no concept of a remote peer. All transport, serialization, and authority must be added externally. Lo-Fi stated that adding it would mean rewriting essentially every system. |
| **Single authoritative simulation** | One process simulates the whole world. There is no client/server split to exploit — you must invent one by hooking. |
| **Non-deterministic** | Heavy use of RNG (combat rolls, spawns, vendor stock, weather), background-threaded pathfinding, and floating-point physics. Two instances *will* diverge. This kills naive lockstep. |
| **Huge persistent streaming world** | ~20km² map with continuous AI: town populations, patrols, economy, raids, hunger/healing all tick whether observed or not. "What state even needs syncing" is most of the world. |
| **Background-threaded hierarchical pathfinding** | Pathing runs async off the sim thread — results arrive non-deterministically relative to frame timing, so paths differ run-to-run. |
| **Fragile engine** | Widely described (including by the dev) as held together "with duct tape." Hooks must be defensive; the engine crashes easily under unexpected state. |
| **32-bit legacy + 64-bit build** | Two binaries historically; modern work targets `kenshi_x64.exe`. Offsets differ per build/patch and must be re-derived. |

There is no official modding API for runtime behavior. The FCS / data files let you
*author content*; they do not let you *intercept the running simulation*. Everything
runtime is injection + hooking.

---

## 2. Prior art (what already exists — build on it, don't restart)

A real project does not start from zero. Three existing bodies of work define the
foundation:

### 2.1 RE_Kenshi (BFrizzleFoShizzle) — the reverse-engineering base
A code-injection mod that already hooks large parts of the engine and ships a plugin
framework (**KenshiLib**) so others can hook without recompiling. Hooks observed in its
source include `OgreHooks`, `PhysicsHooks`, `HeightmapHook`, `MyGUIHooks`, `FSHook`
(filesystem), `Sound`, `ShaderCache`, and reversed texture codecs
(`OgreDDSCodec2`, `OgreSICCodec`). **This is the single most valuable starting asset:**
it proves the hooking surface and documents many internals.

### 2.2 Kenshi-Online / KenshiMP (The404Studios) — the existing co-op attempt
A WIP client–server multiplayer mod (v0.3.x, 2–16 players). Architecture as documented:

- **Injection:** `KenshiMP.Injector.exe` launches the game; `KenshiMP.Core.dll`
  (~1.4 MB, ~90 C++ source files, 14 hook modules) injects and hooks the sim.
  Loaded via Ogre3D's plugin loader (`Plugins_x64.cfg`).
- **Transport:** **ENet** (reliable UDP), 3 channels, UDP 7777, **40+ message types**.
- **Server:** dedicated `KenshiMP.Server.exe` (~515 KB) — authoritative
  (`WorldPersistence`, `GameState`, `AuthValidator`, `EntityRegistry`).
- **Authority model:** "server owns truth, clients own input"; 8-way validation
  decision tree; generation tracking to prevent "ghost control."
- **Memory access:** ~20+ verified offsets; IDA-style wildcard byte-pattern scanning
  of `kenshi_x64.exe`, RIP-relative resolution, Cheat-Engine pointer-chain fallbacks.
- **Synchronized today:** player positions (real-time), death/KO events, building
  state, squad management, faction data, shared world state.
- **Broken / missing today:** combat damage application (`ApplyDamage` hook crashes),
  inventory sync (incomplete), **AI not synchronized** (local AI decisions diverge),
  client prediction (Phase 7, stub only), mid-join (was "invisible characters," patched
  with a `DeferredSpawnQueue`), Steam load deadlock (worked around with a 90s timeout).

**Read this gap list as the spec's to-do list.** What's broken there is precisely the
hard 80%.

### 2.3 OpenConstructionSet (lmaydev) + FCS — the data/save model
A managed SDK that reads/writes Kenshi's data and save files, locates installs, and
models load order and the `ModDataContext`. Needed for save-format interop and for
authoring any content the mod ships.

---

## 3. The data & save file format (what "world state" actually is)

Co-op persistence has to interoperate with Kenshi's own format. Community reversing
gives a usable model:

- **File header tag:** `15` = game **save** file, `16` = editor (**mod**) file. Type-16
  files carry mod version/author/description/dependencies.
- **Primitive type codes:** `L` = 4-byte signed int, `F` = 4-byte float,
  `C` = 1-byte char, `?` = 1-byte bool; strings are length-prefixed UTF-8.
- **Records/instances:** the world is a graph of typed objects (items) referenced by
  **string IDs**; mods layer **diffs** over `gamedata.base` without rewriting it
  (override/append semantics), and saves capture live instance state (`.platoon` and
  related save files, decoded by FCS into readable form).
- **Implication for co-op:** the authoritative server can reuse this format for
  persistence, but **live runtime state lives in memory, not in these files** — the
  files are checkpoints, not a wire protocol. Sync must hook memory, then optionally
  serialize to this format for save/load.

---

## 4. Subsystems that must be synchronized (the real scope)

This is the heart of "full spec." Each row is an independent engineering problem.
Difficulty is for *full parity*, not a demo.

| # | Subsystem | What must sync | Difficulty | Notes / failure mode |
|---|---|---|---|---|
| 1 | **Player squad ownership** | Which player commands which characters; input → orders | Med | Multiple cursors / selection sets; "ghost control" races. |
| 2 | **Character transform** | Position, rotation, animation state, ragdoll | Med | Already works in prior art; interpolation/prediction needed for smoothness. |
| 3 | **Movement & orders** | Move/attack/follow/job commands | Med | Must be intent-based (send orders, not positions) to survive desync. |
| 4 | **Combat** | Hit rolls, damage, limb/blood state, KO/death, stun | **Hard** | RNG-driven; `ApplyDamage` is where the existing mod crashes. Must make rolls server-authoritative. |
| 5 | **Inventory & items** | Container contents, equip, trade, loot, money | **Hard** | Item identity/dedup across peers; race conditions on shared loot. Incomplete in prior art. |
| 6 | **AI** | NPC squads, town pops, patrols, raids, animals, dialogue triggers | **Very Hard** | The big one. AI decisions are local & RNG-driven; unsynced AI = total divergence. Needs server-authoritative AI tick. |
| 7 | **World economy** | Vendor stock, prices, faction wealth, shop restock | Hard | RNG restock; must be server-owned and pushed. |
| 8 | **Factions & relations** | Reputation, alliances, bounties, faction events | Med | Global state; relatively low-frequency; serialize on change. |
| 9 | **Time & game speed** | Day/night, simulation clock, pause, time-scale (1×/5×) | **Hard** | Kenshi lets players speed up/pause time. In co-op, **time must be a single shared authority** — you cannot let two players run different timescales. UX problem too. |
| 10 | **Base building & construction** | Placement, build progress, ownership, power, storage | Hard | Continuous progress state; placement validation; partially works in prior art. |
| 11 | **Research & tech** | Tech tree progress, blueprints | Easy–Med | Shared or per-player? Design decision. Low frequency. |
| 12 | **Production / farming** | Crop growth, machines, hunger, healing-over-time | Hard | Continuous tick state; must derive from shared clock. |
| 13 | **Environment** | Weather, acid rain, fog, day length, region effects | Med | RNG-driven; push from server. |
| 14 | **Spawns** | Homeless squads, wandering traders, ninjas, bounty hunters | Hard | RNG spawn tables; must be server-authoritative and broadcast. |
| 15 | **World map / fast travel** | Map markers, discovered locations, fast-move | Med | Per-player vs shared discovery is a design call. |
| 16 | **Persistence / save-load** | Authoritative save, mid-session join, reconnect | **Hard** | Single shared save; must serialize merged world (see §3) and re-hydrate joiners. |
| 17 | **Imports / new game** | Character import, world states, mods load order | Med | All peers must run identical mod set + version + offsets. |

**The dependency that dominates everything: #6 AI + #9 time + #4/#14 RNG.** Until the
simulation has one authoritative owner of *time, randomness, and AI*, every other
system desyncs no matter how well it's individually synced.

---

## 5. The architectural decision that defines the project

There are three viable models. Only one is realistic for Kenshi.

1. **Deterministic lockstep** (send only inputs; every peer simulates identically).
   *Rejected.* Requires bit-for-bit determinism Kenshi does not have (RNG, threaded
   pathfinding, FP). Would require seeding/serializing every RNG source and forcing the
   pathfinder deterministic — effectively impossible without source.

2. **Server-authoritative simulation (one host simulates, others are thin clients).**
   *Recommended.* One instance (host or headless server) is the **only** real
   simulation. Time, RNG, AI, economy, spawns all live there. Clients send **intent**
   (orders/input) and receive **state deltas** for entities near them; they render and
   predict but never own truth. This matches the prior-art direction ("server owns
   truth, clients own input") and is the only tractable path. Cost: the host carries the
   full sim; clients need interpolation/prediction to hide latency; bandwidth scales
   with visible entity count (Kenshi towns are dense).

3. **Instanced / sharded co-op** (players share economy/factions but simulate separate
   regions; merge on proximity). *Fallback.* Sidesteps full-world sync by only syncing
   when players are co-located. Lower fidelity but far cheaper; a pragmatic "full spec"
   compromise.

**Recommendation:** server-authoritative (model 2), with an interest-management /
relevancy system (only replicate entities within each client's view radius) to keep
bandwidth sane, and model 3's sharding as a fallback for distant players.

---

## 6. Reverse-engineering surface required

Everything below must be re-derived per game build (offsets break on patches):

- **Process & injection:** Ogre plugin-loader injection (reuse RE_Kenshi/KenshiMP
  approach), DLL load ordering, anti-deadlock on Steam init.
- **Pattern scanning:** IDA-style wildcard signatures over `kenshi_x64.exe` +
  RIP-relative resolution so the mod survives minor patches without hand-edited
  offsets; Cheat-Engine pointer chains as fallback.
- **Core hooks to locate and stabilize:**
  - Sim main tick / update loop (insert the network pump + authority gate here).
  - RNG source(s) — to seed and centralize randomness on the server.
  - `ApplyDamage` / combat resolution (currently crashes — needs correct ABI/calling
    convention and re-entrancy guards).
  - Entity create/destroy/spawn (for the `EntityRegistry` and `DeferredSpawnQueue`).
  - AI decision entry points (to gate/replace with authoritative results).
  - Inventory/item transfer functions.
  - Pathfinding request/result (to make movement intent-based).
  - Time/game-speed control.
  - Save/load serialization (to interop with §3 format).
  - MyGUI hooks for all multiplayer UI (lobby, player list, chat, ownership cues).
- **Entity identity:** a stable network ID ↔ in-memory pointer registry, surviving
  save/load and streaming load/unload.

---

## 7. Networking & infrastructure

- **Transport:** ENet (reliable + unreliable UDP channels) — already proven here.
  Reliable channel for orders/economy/faction/spawn events; unreliable for
  high-frequency transforms; separate channel for bulk state (join/snapshot).
- **Snapshotting:** initial full-world snapshot on join (serialize via §3 model →
  ship → re-hydrate), then **delta replication** per tick scoped by interest management.
- **Authority/validation:** server validates every client command (range, ownership,
  legality) to prevent desync *and* trivial cheating; generation counters to reject
  stale/duplicate commands.
- **Topology:** dedicated headless server is cleanest (host doesn't get a sim
  advantage and can persist); listen-server (one player hosts) is easier to ship and
  matches the casual co-op use case. Support both; P2P only as relay-assisted fallback.
- **NAT traversal:** STUN/hole-punching or a relay for listen-server hosting.
- **Versioning:** strict handshake — identical game build, mod version, offset table,
  and content load order, or refuse to connect.

---

## 8. Non-networking work that's still mandatory

- **UI (MyGUI):** lobby/host/join screens, player list, per-character ownership
  indicators, multi-player selection that doesn't collide, chat, connection/desync
  status, shared-time indicator.
- **Shared time UX:** because one player can't pause/speed-time unilaterally, you need a
  voting/host-controlled time-scale model and clear feedback. This is a *design*
  problem as much as engineering.
- **Persistence service:** authoritative save, autosave, crash recovery, mid-session
  join hydration, reconnect.
- **Crash resilience:** the engine is fragile; hooks need guards, watchdogs, and
  graceful desync recovery (re-snapshot a drifted client rather than crash).
- **Diagnostics:** desync detection (state hashing across peers), logging, replay for
  debugging — without these the project is unmaintainable.
- **Distribution:** installer that places the DLL, edits `Plugins_x64.cfg`, manages the
  injector, and keeps all peers on matching versions (cf. the "simplified installer"
  community fork).

---

## 9. Scope tiers — MVP to full spec

| Tier | Scope | Roughly where prior art sits |
|---|---|---|
| **T0 — Tech demo** | Inject, connect, see each other's characters move; shared positions only. | ✅ Done (KenshiMP). |
| **T1 — Playable co-op** | + death/KO, buildings, squads, factions, shared world state, late-join. | ✅ ~v0.3 (unstable). |
| **T2 — Combat & loot** | Server-authoritative combat (fix `ApplyDamage`), inventory/trade/loot sync. | ⚠️ Broken/partial. |
| **T3 — Living world** | Server-authoritative **AI**, economy/vendors, spawns, weather, shared time. | ❌ Not done — the hard 80%. |
| **T4 — Full parity** | Production/farming/research/all continuous ticks, robust persistence, reconnect, desync auto-recovery, polished UX. | ❌ "Full spec." |
| **T5 — Shippable** | Stable for hours, multi-patch-resilient offset scanning, installer, docs, anti-cheat. | ❌ |

"Full-spec" = **T4–T5**. Prior art is stuck transitioning T1→T2.

---

## 10. Effort, team, and risk

**Team profile (the rare part):** you need engineers who can do *all* of —
x64 reverse engineering (IDA/Ghidra/Cheat Engine), C++ function hooking against a
fragile closed-source engine, and netcode (authority, interest management, prediction).
That intersection is small; it's why no polished mod exists.

**Rough effort to T4/T5:** with 2–3 such engineers, realistically **18–36 months**, and
the AI-synchronization problem (§4 #6) is the schedule risk that could sink it entirely.
A T1–T2 "fun experiment" is achievable by a small team in months (and largely already
exists to fork).

**Top risks, ranked:**
1. **AI desync** — no clean authoritative-AI hook may exist; may require replacing whole
   AI subsystems. *Highest risk; can be a hard blocker.*
2. **Determinism / RNG** — must centralize every randomness source on the server or
   accept perpetual drift.
3. **Engine fragility** — crashes under novel state; every hook is a stability liability.
4. **Patch fragility** — offsets break on game updates; mitigated but not solved by
   pattern scanning. (Kenshi is largely stable/EOL-ish, which *helps* here.)
5. **Bandwidth** — dense towns + full replication; needs aggressive interest management.
6. **Legal/community** — unaffiliated with Lo-Fi; depends on reverse engineering and
   redistribution of injected code. Keep it a separate, non-infringing mod.

---

## 11. Recommended path forward

1. **Fork, don't restart.** Base on **RE_Kenshi/KenshiLib** for the hook framework and
   **KenshiMP** for the netcode skeleton and offset table. Use **OpenConstructionSet**
   for save/data interop.
2. **Lock the architecture:** server-authoritative (§5 model 2) + interest management,
   with region sharding (model 3) as the distant-player fallback.
3. **Solve the trio first:** authoritative **time + RNG + AI** before polishing
   anything else. Nothing downstream is stable until these have one owner. Build desync
   detection (state hashing) on day one to measure progress.
4. **Then T2:** fix `ApplyDamage`/combat as server-authoritative; inventory/loot with
   item-identity dedup.
5. **Then T3/T4:** economy, spawns, weather, production, persistence, reconnect.
6. **Throughout:** strict version/offset handshake, watchdogs, re-snapshot recovery,
   and an installer that keeps peers in lockstep on versions.

### Alternative worth stating plainly
If the goal is *good co-op Kenshi* rather than *modding Kenshi 1 specifically*, the
lowest-risk path is **Kenshi 2** (Unreal Engine) — a modern engine with real
multiplayer primitives would make the entire §4/§5 problem an order of magnitude more
tractable than fighting Kenshi 1's bespoke, non-deterministic, networking-less engine.
For Kenshi 1, **T2 + a sharded model 3** is the best effort-to-payoff target; **true
T4–T5 full parity is the kind of thing that's *possible* but realistically never fully
shipped without the source.**

---

## Sources

- [Kenshi Unofficial Multiplayer Mod — Kenshi Wiki](https://kenshi.fandom.com/wiki/Kenshi_Unofficial_Multiplayer_Mod)
- [The404Studios/Kenshi-Online (KenshiMP) — GitHub](https://github.com/The404Studios/Kenshi-Online) · [README](https://github.com/The404Studios/Kenshi-Online/blob/main/README.md)
- [im-blatnoyua/kenshi-online-simplified — GitHub](https://github.com/im-blatnoyua/kenshi-online-simplified)
- [BFrizzleFoShizzle/RE_Kenshi — GitHub](https://github.com/BFrizzleFoShizzle/RE_Kenshi)
- [lmaydev/OpenConstructionSet — GitHub](https://github.com/lmaydev/OpenConstructionSet)
- [Kenshi gamedata/mod/save file format — Steam Guide](https://steamcommunity.com/sharedfiles/filedetails/?id=797652627)
- [Forgotten Construction Set — Kenshi Wiki](https://kenshi.fandom.com/wiki/Forgotten_Construction_Set)
- [Is there hope for Online/Multiplayer/Co-Op? — Lo-Fi Games forums](https://www.lofigames.com/phpBB3/viewtopic.php?t=15320)
- [Dev Blog #19: Optimisation — Lo-Fi Games](https://lofigames.com/dev-blog-19-optimisation/) (engine/pathfinding internals)
- [Kenshi — Ogre3D Forums thread](https://forums.ogre3d.org/viewtopic.php?t=42488) (engine basis)
