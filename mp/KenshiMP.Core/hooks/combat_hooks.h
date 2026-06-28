#pragma once
#include <cstdint>

namespace kmp::combat_hooks {

bool Install();
void Uninstall();

// Process deferred combat events from the safe game-tick context.
// Called from Core::OnGameTick — NOT from inside a hook.
void ProcessDeferredEvents();

// Poll limb health of locally-owned characters and send C2S_LimbHealth on
// change. This is the continuous damage-sync path: ApplyDamage cannot be hooked
// (mov rax,rsp crash), so instead of intercepting each hit we sample HP at a
// fixed rate and replicate deltas. Covers all HP changes — combat, bleeding,
// healing, starvation — not just the KO/death events. Throttled internally; safe
// to call every tick. Called from Core::OnGameTick — NOT from inside a hook.
void PollOwnedHealth();

// Echo suppression: set before calling native CharacterDeath/CharacterKO from
// packet handler (server-sourced events). The hook checks this flag and skips
// pushing to the deferred queue, preventing infinite C2S→S2C→C2S echo loops.
void SetServerSourcedDeath(bool active);
void SetServerSourcedKO(bool active);

} // namespace kmp::combat_hooks
