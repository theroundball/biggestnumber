# Session Handoff — Lifeline Fix + Project Context

**Created:** 2026-08-12  
**Situation:** Development moved from one machine/Cursor workspace to another. This doc captures the Lifeline design discussion, current bugs, intended behavior, and what to read before implementing.

**Paste the “Bootstrap prompt” section (§1) into a new Cursor chat on the other computer after syncing the repo.**

---

## 1. Bootstrap prompt (copy from here to Cursor)

```
You are continuing work on **Biggest Number**, a Butano (GBA) C++ card game. The author was using Cursor on one Mac; they are now on a **different computer** with a synced copy of the repo. Read this file first: `docs/SESSION_HANDOFF_LIFELINE.md`, then `docs/HANDOFF.md` for broader refactor status.

## Immediate task: Lifeline ordering fix

**Card:** Lifeline — "Shuffle graveyard into deck, exile 1/4 of starting deck."

**Designer intent [CONFIRMED in discussion]:** When Lifeline's shuffle runs, **Lifeline itself must already be in the graveyard** and must be shuffled into the deck along with every other graveyard card. Today it is NOT — the shuffle runs while Lifeline is still in hand, then Lifeline goes to GY afterward, so it never gets mixed back into the library.

**This is NOT the same as Big Kurosawa Burger's discard cost.** Burger queues a pending player discard, then pays ×4. Lifeline's "exile 1/4 undrawn" is automatic — not a hand pick. The fix is **pipeline ordering**, not adding a Burger-style cost UI.

### Target resolution order for Lifeline

1. `apply_card_play` (Lifeline has no immediate score)
2. Route Lifeline to **graveyard** (`graveyard_push` — no `on_discard` on Lifeline today)
3. Merge **entire** graveyard (including Lifeline) into deck
4. `deck.shuffle`
5. `deck.remove_undrawn(starting_deck_size / 4)` — exile ¼ of **starting** deck size from undrawn pile
6. Dispatch `GRAVEYARD_CHANGED` if needed (Birds, combos, etc.)

### Current code (wrong order)

- `src/card_data.cpp` — `effect_reclaim()` drains GY into deck **inside `on_play`**, while Lifeline is still in hand
- Hand play: removal anim ends → `play_card_once` (runs `effect_reclaim`) → `finish_played_card_from_hand` (Lifeline → GY)
- See `src/game_input.cpp` ~L232–288 for hand play completion order

### Implementation guidance (prefer pipeline-aligned)

- **Avoid** Lifeline-only special cases in `game_input.cpp` if possible
- **Prefer:** split effect — route to GY first, then run shuffle+exile via pending queue or post-route hook (aligns with refactor `#2` play resolution)
- **Minimal fix:** push Lifeline to GY at start of `effect_reclaim` before draining — works but bypasses unified pipeline
- After fix: update `docs/GAME_DESIGN_BRD.md` §11 and §16; add test note to `docs/play_resolution_matrix.md`

### Do NOT git commit unless the user explicitly asks.

### Broader project context

- 52-card solo score-chaser; no opponent; refactor in progress (#2d-hand next)
- Play resolution model: BRD §5.3 — play costs vs GY-entry triggers (`on_discard`)
- Burger = early GY play cost; Clover = held defer; Bones counts self in GY size
- Full refactor/handoff: `docs/HANDOFF.md`

Confirm current code matches this analysis, then implement Lifeline fix with minimal scope. Report what you changed and suggest manual test: play Lifeline with other cards in GY; verify Lifeline returns to deck pool (not stranded in GY only).
```

---

## 2. Situation (why this doc exists)

| | Before | Now |
|---|--------|-----|
| **Machine** | Original Mac (`~/Desktop/bn`) | Different computer |
| **Cursor** | Prior chat history on old machine | **New session — no prior chat context** |
| **Also used** | Windows Butano path: `C:/Users/Vigil/Desktop/butano-master/examples/biggestnumber` | May differ |

The old Cursor session covered: card-game refactor (#2a–#2c done), sprite TODO, game design (power level, branching decisions), and a **planning discussion about Lifeline** that was never implemented in code.

**This doc is the bridge** so the new agent does not re-discover Lifeline from scratch or implement the wrong model (Burger-style cost).

---

## 3. Game context (short)

- **Biggest Number** — GBA card battler built with Butano
- Player builds a 1–50 card deck, plays a **single run** against a shuffled copy of that deck, maximizes **total score**
- No opponent, no lose state — high score chase
- Zones: deck, hand, graveyard, exile, held (limbo for play costs)
- Multi-frame UI for costs: graveyard picks, scry, deck search, discard targets
- Future RPG overworld is **blocked** until refactor exit criteria (`docs/HANDOFF.md`)

---

## 4. Play resolution model (relevant to Lifeline)

Read **`docs/GAME_DESIGN_BRD.md` §5.3** in full. Summary:

### Two concepts

| Concept | When | Examples |
|---------|------|----------|
| **Play resolution** (`on_play`, pending costs) | Card is played | Scry, exile picks, discard-as-cost |
| **GY entry trigger** (`on_discard`) | Card **enters** graveyard | Bones, Busted, Threshold |

`on_discard` is **not** a play cost — it fires when a card arrives in the GY (play, discard cost, etc.).

### Play cost patterns in code today

| Pattern | Card | Behavior |
|---------|------|----------|
| **Held defer** | Clover, Jacks, Fishing Pole, Cups, Seeds | `defer_graveyard_until_pending`; card held until pending drains |
| **Early GY** | Big Kurosawa Burger | Burger to GY immediately; player discards hand card; then ×4 |
| **Synchronous `on_play`** | Lifeline, Necromancy, Roundup, Turtle | Entire effect in `effect_*` during `apply_card_play` |

**There is no unified `resolve_played_card()` at call sites yet** — `include/play_resolution.h` + `route_played_card()` exist (#2c) but hand/scry/search still use legacy paths. Effects remain **per-card `effect_*` functions** + **`pending_actions` queue** for interactive steps.

---

## 5. Lifeline — intended vs current

### Card text (in `card_data.cpp`)

"Lifeline" — Shuffle graveyard into deck, exile 1/4 of starting deck.

### Intended behavior (designer)

1. Playing Lifeline puts it in the **graveyard** (normal play routing).
2. **All** graveyard cards — **including Lifeline** — are merged into the deck.
3. Deck is shuffled.
4. Exile **one quarter of starting deck size** from the **undrawn** portion (`deck.remove_undrawn(starting_deck_size / 4)`).
5. Lifeline can be drawn again later — it is part of the recycled pool, not stuck in GY.

### Why Lifeline should be in GY during shuffle

- Matches card text: "shuffle **graveyard** into deck" — the card you played is part of that graveyard moment.
- Consistent with zone logic: you **played** Lifeline; it **went to GY**; then GY contents become deck.
- **Not** analogous to Burger: exile ¼ is **automatic**, not "pay a card to earn the shuffle."

### Former behavior (bug / ordering) — **FIXED 2026-08-12**

`effect_reclaim` used to drain GY inside `on_play` while Lifeline was still in hand, then routing put Lifeline in GY afterward — so Lifeline never re-entered the deck.

**Fix (Approach B):** `effect_reclaim` only queues `PendingActionType::RECLAIM_GRAVEYARD`. Hand/scry/search route Lifeline to GY first; `begin_next_pending_or_finish` then calls `reclaim_graveyard_into_deck()`.

### Hand play timing (why `on_play` runs before routing)

Normal hand play (`src/game_input.cpp`):

1. Confirm → start removal animation (`removing_card = true`)
2. When animation completes (~24 frames):
   - `play_card_once(state, played)` → **`effect_reclaim` runs here**
   - `finish_played_card_from_hand` → Lifeline → GY

Swivel-follow branch calls `play_card_once` **before** anim — Lifeline uses the normal branch.

### Comparison cards

| Card | Zone ops in `on_play` | Self after play |
|------|----------------------|-----------------|
| **Lifeline** | Shuffle GY→deck, exile ¼ undrawn | GY (but **after** shuffle today — wrong) |
| **Necromancy** | Shuffle GY→bottom of deck | **Exile** (`exiles_self_on_play`) — never GY |
| **Burger** | Queue discard pending only | GY **before** pending UI (early GY) |

---

## 6. Recommended implementation approaches

| Approach | Pros | Cons |
|----------|------|------|
| **A. Minimal** — `graveyard_push(LIFELINE)` at start of `effect_reclaim` before drain | Tiny diff | Bypasses pipeline; needs hand index/type passed or infer from context; fragile for scry/search if Lifeline ever played elsewhere |
| **B. Split effect** — `on_play` empty or queues `PendingActionType` for reclaim; shuffle runs when queue processes after routing | Aligns with #2 refactor; testable | New pending type or shared helper; more files |
| **C. Reorder hand handler** — route to GY before `play_card_once` for Lifeline only | — | Special case; avoid |

**Recommended:** B if touching play resolution anyway; A only as a quick fix with a comment linking to §5.3 ordering.

### Manual test after fix

1. Fill GY with 2–3 identifiable cards + play Lifeline.
2. After effect: GY should be **empty** (Lifeline merged into deck).
3. Draw over time — Lifeline should **reappear** from deck (not only remain exiled or stuck in GY).
4. Verify undrawn count dropped by ~¼ of **starting** deck size after reclaim.
5. Regression: Birds of a Feather, combos that care about `GRAVEYARD_CHANGED`.

---

## 7. Refactor status (don't block Lifeline on this)

| Step | Status |
|------|--------|
| #4, #5 UI dedup | Done |
| #2a matrix, #2b types, #2c `route_played_card()` | Done, **not wired to call sites** |
| **#2d-hand** | Next refactor step |
| Overworld | Blocked on refactor |

See `docs/HANDOFF.md` and `docs/play_resolution_matrix.md`.

---

## 8. Files to share / sync on the other computer

### Minimum (Lifeline task)

| File | Why |
|------|-----|
| `docs/SESSION_HANDOFF_LIFELINE.md` | This document |
| `src/card_data.cpp` | `effect_reclaim`, card table |
| `src/game_input.cpp` | Hand play order (~L152–290, ~L433–468) |
| `src/game_helpers.cpp` | `finish_played_card_from_hand`, `play_card_once` |
| `include/game_state.h` | `pending_actions`, deck/graveyard |
| `include/deck.h` / `src/deck.cpp` | `shuffle`, `remove_undrawn`, `compact` |
| `src/game_events.cpp` | GY events if any |

### Design context (strongly recommended)

| File | Why |
|------|-----|
| `docs/HANDOFF.md` | Full project + refactor handoff |
| `docs/GAME_DESIGN_BRD.md` | §5.3 play resolution, §11 Lifeline catalog |
| `docs/play_resolution_matrix.md` | Pipeline order, migration plan |
| `include/play_resolution.h` | Target routing API (#2c) |
| `src/play_resolution.cpp` | `route_played_card` implementation |

### If implementing pending-based reclaim

| File | Why |
|------|-----|
| `src/game_context.cpp` | `begin_next_pending_or_finish` (~L1257+) |
| `include/game_types.h` | `PendingActionType`, `GameMode` |
| `src/game_events.cpp` | `graveyard_push`, `finalize_held_played_card` |

### Optional plans (may not be in git)

Copy from other machine if missing:

- `.cursor/plans/refactor_inventory.plan.md`
- `.cursor/plans/rpg_overworld_vertical_slice_871a2829.plan.md`

### Easiest sync method

**Git push/pull** the whole `bn` repo — all of the above travel together.

---

## 9. Related open design topics (not blocking Lifeline)

From prior sessions — for context only:

- Unified Echo (no Swivel/Hacker special cases) — refactor #2d-echo
- Score-shape / tempo cards (Semaphore, Roundup, digit cards)
- 30 card sprites still needed — `SPRITE_TODO.md`
- Clover fizzle with &lt;3 GY — BRD §16 open

---

## 10. Changelog

| Date | Change |
|------|--------|
| 2026-08-12 | Initial handoff: Lifeline ordering, machine migration, bootstrap prompt |
| 2026-08-12 | **Implemented** Approach B: `RECLAIM_GRAVEYARD` pending + `reclaim_graveyard_into_deck()`; BRD/matrix updated |
