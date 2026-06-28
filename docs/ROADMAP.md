# Roadmap — The Broken List

Derived from KenshiMP's own audits (`mp/docs/audit-2026-06-03-gap-report.json`,
`mp/docs/MULTIPLAYER-FIXES-2026-06-04.md`) plus the spec in `../KENSHI_COOP_SPEC.md` §4.
Ordered by **dependency**: lower items don't matter until the blocker above them clears.

## Legend
✅ works · ⚠️ partial/fragile · ❌ broken/missing · 🔭 needs live game to verify

## 0. Foundation (salvageable, per upstream audit)
- ✅ ENet transport, protocol, packet streaming (4,900+ pkts reliable)
- ✅ Entity registry, interpolation, MovRaxRsp hook infra, MyGUI UI
- ⚠️ Dedicated server compiles *mostly* — `HandlePlayerReady` references a
  non-existent struct → **build break to fix first** (pure code, no game needed).

## 1. 🚨 THE BLOCKER — sync loop deadlocked at "game loaded" gate
**Symptom:** remote characters never appear. **Root cause (not networking):**
`GameWorldSingleton` resolves `UNRESOLVED` on some builds (~30 patterns fail), so
`OnGameLoaded()` never fires (it depends on the CharacterCreate loading-burst detector,
`core.cpp:1346-1362`), so every `S2C_EntitySpawn (0x31)` is dropped and the deferred
queue never drains.
- ⚠️ `DeferredSpawnQueue` now buffers spawns (done upstream 06-04).
- ❌ **OnGameLoaded deadlock** — fix `GameWorldSingleton` resolution; port the **90s
  hard-timeout fallback** (exists in upstream `rebuild/`, never ported to `main`);
  harden the global-pointer fallback (`core.cpp:1381`) so it can fire when patterns fail.
- 🔭 Final verification needs a live Kenshi install.

## 2. ❌ Combat — only death/KO syncs
`C2S_AttackIntent` is **never sent**, so no real combat sync. `ApplyDamage (0x7A33A0)`
**cannot** be hooked directly (`mov rax,rsp` prologue + hundreds of rapid calls →
deterministic crash; `combat_hooks.cpp:270`). Plan: send attack **intent** from clients,
resolve damage **server-authoritatively**, broadcast results — never hook ApplyDamage.

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
