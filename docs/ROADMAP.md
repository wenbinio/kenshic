# Roadmap — The Broken List

Derived from KenshiMP's own audits (`mp/docs/audit-2026-06-03-gap-report.json`,
`mp/docs/MULTIPLAYER-FIXES-2026-06-04.md`) plus the spec in `../KENSHI_COOP_SPEC.md` §4.
Ordered by **dependency**: lower items don't matter until the blocker above them clears.

## Legend
✅ works · ⚠️ partial/fragile · ❌ broken/missing · 🔭 needs live game to verify

## 0. Foundation (salvageable, per upstream audit)
- ✅ ENet transport, protocol, packet streaming (4,900+ pkts reliable)
- ✅ Entity registry, interpolation, MovRaxRsp hook infra, MyGUI UI
- ✅ `HandlePlayerReady` build break — **already resolved in this `main`**
  (`server.cpp:2419`): every symbol it references (`ConnectedPlayer.isReady`,
  `S2C_AllPlayersReady`, `KMP_CHANNEL_RELIABLE_ORDERED`) resolves. The 06-03 audit
  predates the 06-04 fix pass. No action needed.

## 1. 🚨 THE BLOCKER — sync loop deadlocked at "game loaded" gate
**Symptom:** remote characters never appear. **Root cause (not networking):**
`GameWorldSingleton` resolves `UNRESOLVED` on some builds (~30 patterns fail), so
`OnGameLoaded()` never fires (it depends on the CharacterCreate loading-burst detector,
`core.cpp:1346-1362`), so every `S2C_EntitySpawn (0x31)` is dropped and the deferred
queue never drains.
- ✅ `DeferredSpawnQueue` now buffers spawns (done upstream 06-04).
- ✅ **90s hard-timeout fallback** — already ported to `main` (`core.cpp:1403`),
  alongside 60/120s CharacterIterator fallbacks (`core.cpp:1382-1397`). The gate now
  force-opens even if `GameWorldSingleton` never resolves.
- ❌ **The real remaining gap:** forcing `OnGameLoaded` opens the gate, but if
  `GameWorldSingleton` is still unresolved, drained spawns have no `GameWorld` to attach
  to (`spawn_manager.cpp:539`). So robust **`GameWorldSingleton` resolution** is what's
  left — and it's **binary-dependent** (pattern scan against the user's specific build).
  Candidate fix: capture the frame-update `this` pointer as a runtime GameWorld source
  (`Hook_GameFrameUpdate` rcx, `game_tick_hooks.cpp:39`) — but that hook isn't even
  installed today, and whether `rcx` *is* GameWorld needs on-machine verification.
- 🔭 **Needs a live Kenshi install** — not inspection-verifiable. Deferred until we have
  a Windows + Kenshi test box; logging probes required to confirm `rcx` semantics.

## 2. ⚠️ Combat — continuous damage sync added (PvP); NPC HP next
`ApplyDamage (0x7A33A0)` **cannot** be hooked (`mov rax,rsp` prologue + hundreds of
rapid calls → deterministic crash; `combat_hooks.cpp`). The whole `LimbHealth` pipeline
already existed end-to-end (client read → `C2S_LimbHealth` → server broadcast →
`S2C_LimbHealth` → remote client writes HP back), but health was only ever sent
*piggybacked after a KO/death event* — so mid-fight HP never moved on remote screens
("damage bars broken").
- ✅ **Continuous health sync** — `combat_hooks::PollOwnedHealth()` (new) samples the 7
  limb values of each **locally-owned** character at 4 Hz and sends `C2S_LimbHealth` on
  change (>0.5 HP). Routes around ApplyDamage entirely; also covers bleeding, healing,
  and starvation, not just hits. Wired into both `OnGameTick` paths (`core.cpp`).
  Conflict-free: each character has exactly one authoritative reporter (its owner), and
  it never echoes incoming `S2C_LimbHealth` (which only writes *remote* characters).
  🔭 Needs the live game to confirm HP visibly tracks across clients.
- ❌ **NPC / enemy HP** — not yet synced (only player-owned chars are polled). Correct
  fix: the **host** additionally polls server-owned (`ownerPlayerId == 0`) entities, with
  **interest management** (only NPCs near a player) to avoid broadcasting HP for every
  world NPC. Single authoritative reporter (host) = no conflict. Deferred: needs the
  live game to verify server-owned NPCs are registered with linked game objects on the
  host, and to tune the relevancy radius.
- 💭 Optional later: client→host **attack intent** for server-authoritative hit
  resolution (true determinism). The poll above makes this lower priority — visible
  damage now replicates without it.

## 3. ❌ Inventory / items — incomplete
Container contents, equip, trade, loot, money. Needs stable item identity/dedup across
peers and race-free shared-loot handling.

## 4. ❌ AI — local only (the hard one)
NPC squads, town pops, patrols, raids, animals are decided locally + RNG → guaranteed
divergence. Needs server-authoritative AI/RNG ownership. See spec §4 #6.

## 5. ⚠️ Client prediction (Phase 7) — `ReconcileLocal` is a stub
Local player skips server-authoritative reconciliation → desync under packet loss.
"Acceptable" for basic play; required for polish.

## 6. 🧹 Architectural debt (blocks reasoning about everything above)
Three spawn mechanisms, two position pipelines, 4–7 overlapping "orchestrators", an SDK
abstraction initialized but never queried. Consolidate as we touch each subsystem.

---

## Dev/test loop
The mod injects into the live 64-bit game; it can't be built or run in Linux CI.
Inspectable/fixable here without the game: the server compile break (§0), the
OnGameLoaded fallback + resolution hardening (§1), wiring `C2S_AttackIntent` (§2), and
the orchestrator consolidation (§6). Anything marked 🔭 needs a Windows + Kenshi machine
to confirm. Build: `mp/docs/BUILD.md`.
