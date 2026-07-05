# Project Status & Session Log

Handoff-grade state of the Kenshi co-op effort. Read this + `ROADMAP.md` to resume work
without any prior context. Keep this file updated at the end of each work session.

**Branch:** `claude/kenshi-coop-mod-spec-grhuze` (all work; pushed to origin)
**Last updated:** 2026-07-05

---

## Commit history (what happened, in order)

1. **`339f1a2`** — `KENSHI_COOP_SPEC.md`: full feasibility/engineering spec. Engine
   constraints, prior art, the 17 sync subsystems, why server-authoritative is the only
   viable architecture (§5), scope tiers T0–T5, risks.
2. **`1e43dcf`** — Imported upstream bases: `mp/` ← KenshiMP (MIT), `tools/ocs/` ←
   OpenConstructionSet (MIT). RE_Kenshi (GPLv3) deliberately NOT vendored — reference
   only (see `NOTICE.md`). Excluded binaries, upstream `rebuild/` tree, `.mod` backups.
3. **`5e42fbc`** — **Combat fix:** `combat_hooks::PollOwnedHealth()` — continuous limb
   health sync at 4 Hz for locally-owned characters, routing around the un-hookable
   `ApplyDamage`. Wired into both `OnGameTick` paths. Also corrected ROADMAP: the 06-03
   audit's `HandlePlayerReady` build break and missing 90s timeout were already fixed
   in upstream main by their 06-04 pass (verified symbol-by-symbol).
4. **`9f69794`** — **Consolidation:** deleted ~1,050 LOC of provably-dead modules
   (`zone_interest`, `ownership`, `sync_facilitator`, `building_hooks`, `save_hooks`,
   `kmp/compression`) + their call-sites/includes/CMake entries. Dangling-ref sweep
   clean. Live-path merges deferred (ROADMAP §6).

## Key decisions (do not re-litigate without cause)

- **License wall:** repo stays MIT. RE_Kenshi is GPLv3 — never copy its source in;
  use it only to verify offsets/techniques. Documented in `NOTICE.md`.
- **Architecture:** server-authoritative ("server owns truth, clients own input"),
  inherited from KenshiMP. Lockstep rejected (engine is non-deterministic).
- **`ApplyDamage` is never hooked** (`mov rax,rsp` prologue + rapid-fire calls =
  deterministic crash). Damage replicates by **sampling** (health poll), not
  interception. Don't try to hook it again.
- **Consolidation strategy:** delete provably-dead code first; defer live-path merges
  (spawn mechanisms, legacy in-core sync path, renames) until a game can verify.
- **Poll ownership model:** each character has exactly one authoritative HP reporter
  (its owning player; host will own NPCs later) — prevents echo loops and conflicts.

## Environment facts (for future remote sessions)

- This sandbox is **Linux; the mod is Windows-only** (MSVC/VS2022 + CMake, injects into
  `kenshi_x64.exe`). Nothing has been compiled yet — all changes inspection-verified.
- The session git proxy is **scoped to `wenbinio/kenshic` only** — `git clone` of other
  GitHub repos gets 403. To fetch upstream sources, use codeload tarballs through the
  HTTPS proxy: `curl -L --cacert /root/.ccr/ca-bundle.crt
  https://codeload.github.com/<owner>/<repo>/tar.gz/refs/heads/<branch>`.
- Steam community pages 403 on fetch; use mirrors or GitHub docs for format references.

## Next steps, in recommended order

1. **First build** (needs Windows + Kenshi): compile per `mp/docs/BUILD.md`; fix
   fallout; then verify 🔭 items — remote chars spawn (§1), HP tracks live (§2).
2. **Inspection-safe cleanups** (can do here): dead protocol enums `S2C_ZoneData`
   (0x12) / `C2S_EntityAck` (0x15); dead server `combat_resolver`/`HandleAttackIntent`
   path (unreachable until attack-intent is sent).
3. **NPC HP sync** (mostly game-dependent): host polls server-owned
   (`ownerPlayerId == 0`) entities with a relevancy radius. Design in ROADMAP §2.
4. **§1 blocker hardening** (game-dependent): `GameWorldSingleton` resolution.
   Candidate: capture `rcx` from `Hook_GameFrameUpdate` (`game_tick_hooks.cpp:39`) as a
   runtime GameWorld source — but that hook is currently never installed, and whether
   `rcx` is GameWorld needs on-machine logging probes first.
5. **Spawn-mechanism merge** (game-dependent): 3 spawners → one `SpawnService`
   (FactoryCreate + mod templates), per the upstream reorg verdict.

## Primary references inside this repo

- `KENSHI_COOP_SPEC.md` — the why and the map (subsystems, risks, tiers).
- `docs/ROADMAP.md` — the broken list with per-item status and file/line pointers.
- `mp/docs/audit-2026-06-03-gap-report.json` — upstream 18-agent audit;
  `orchestratorVerdict` + `rewriteOrDelete` drive the consolidation. **Caution:** it
  predates upstream's 06-04 fixes — always re-verify claims against current code.
- `mp/docs/` — inherited protocol/offsets/build/testing docs.
