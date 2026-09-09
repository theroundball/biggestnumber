# Biggest Number — Progression & Overworld Plan

**Status:** Canonical design direction (2026-09)  
**Supersedes:** Shop/currency/prize-row meta in `CAMPAIGN_IMPLEMENTATION_PLAN.md` and ground-loot shop sections of `RPG_OVERWORLD_MVP_PLAN.md` for **card acquisition**. Battle core, modes, and instance upgrades still apply where noted.

**Related:**
- [`NPC_COLLECTION_POC.md`](NPC_COLLECTION_POC.md) — 8 NPCs, ~10 cards each, full 71-card assignment
- [`RPG_OVERWORLD_MVP_PLAN.md`](RPG_OVERWORLD_MVP_PLAN.md) — movement, dialogue, battle handoff (update loot to match this doc)
- [`GAME_DESIGN_BRD.md`](GAME_DESIGN_BRD.md) — in-battle rules (still authoritative for combat)
- [`cards_by_verb.pdf`](cards_by_verb.pdf) — card reference grouped by mechanical verb

---

## 1. Core loop (one sentence)

> **NPCs don't run shops. Their collection is their deck. Beat them, take a card from it — and their loaner weakens every time you do.**

No card shops. No random prize row for cards. Progression is **redistribution** of a fixed 71-card pool between the player and ~8 NPCs.

---

## 2. Battle outcomes

| Action | Result |
|--------|--------|
| **Win with NPC loaner deck** | Pick (or receive) a card from their **remaining** collection |
| **Win with your own deck** | Same pool; may gate harder cards / dupes / record beats later |
| **Loss** | No card taken; their collection unchanged |
| **Take a card** | Removed from `npc.collection` **and** their loaner deck permanently |

As you strip NPCs, their loaners hollow out; your library grows. **Crossover is emergent** — no separate “loaner cap” required (optional soft tuning: loaners tuned for ~2-digit scores early).

---

## 3. Starter deck (implemented)

| Slot | Cards |
|------|--------|
| Fixed | Longboard (+1), Heelys (+2), Scooter (+3), Skateboard (+4) |
| Pick one | Toppings · Clover · Big Kurosawa Burger |

5-card starter is weaker than any full NPC loaner → early play favors **borrowing their deck**.

---

## 4. Sell Collection gate (implemented)

- **Cannot sell** until the player owns **≥1 of every collectible card type** (`max_copies > 0`, all 71 types).
- Campaign Status shows **Collection X/71**; sell row shows **Sell locked** until complete.
- Sell wipes library (except four wheels + nostalgia pick + new starter pick), trinkets, records, upgrades — opens **Route B** (story-heavy re-collection; design TBD).

---

## 5. NPC count, modes & card split

| Metric | Value |
|--------|------:|
| Total card types | 71 |
| Target cards per NPC | ~10 |
| **POC NPCs** | **8** (`ceil(71 ÷ 10)`) |

Each POC NPC **hosts a different `CampaignMode`** (objective / battle rules) **and** owns a ~10-card collection (same list = loaner deck).

POC assignment: **10 + 6×9 + 7 = 71** across eight NPCs. See [`NPC_COLLECTION_POC.md`](NPC_COLLECTION_POC.md) for names, loaner lists, and master assignment table. Mode ↔ NPC mapping TBD when overworld lands.

---

## 6. Route A vs Route B

| | Route A (first collection) | Route B (after sell) |
|--|---------------------------|----------------------|
| **Goal** | One of each card (71/71) → sell unlocks | Re-earn collection + **trophy objectives** |
| **NPC collections** | Full at start; deplete as player wins | Refill / restock rules TBD |
| **Card access** | Win → take from NPC ground loot (their remaining collection) | Story + **objectives** unlock rarer / stronger cards |
| **Sticker paper** | Drops after completed battles | Same |
| **Trinkets** | Minimal or none | Primary trinket source (TBD) |
| **Loaner** | Strong early; weak late as you strip them | NPCs may refuse loaner or offer Route B decks |

Route A is **breadth** (spread across 8 NPCs). Route B is **depth** (harder re-collection + feat-gated power cards).

### Trophy channel (= Route B objectives)

Not a separate prize pool — **specific objectives** (e.g. score **1999** vs Y2K host, mode feats, story beats) unlock **rarer / more powerful** cards on the B run. Distinct from “beat NPC, pick any card still in their pile.”

### Hades-style NPC story (note only — do not plan implementation yet)

Post-sell Route B should eventually have **same NPC locations, evolving dialogue** across `collection_generation` (sell advances generation; library resets but relationships / flags persist). **Design note only** — not on the implementation checklist until Route B story pass.

---

## 7. Overworld UX

### Minimap (corner HUD)

| State | Meaning |
|-------|---------|
| **●** | NPC has ≥1 card you don't own (or dupes available) |
| **○** | Depleted for you |
| **!** | New Route B beat / dialogue |

Hold **Select** or **L** for larger view with NPC names.

### Depleted NPC dialogue

When their collection has nothing left for you: *“I'm tapped out — try ___.”* Points to lit minimap dot.

### Ground loot (cards + sticker paper)

After a **win**, return to overworld with **Diablo-style ground loot** (toss arc, rarity chips, walk to highlight, **Select** inspect, **A** pickup, unpicked poof on rematch). See [`RPG_OVERWORLD_MVP_PLAN.md`](RPG_OVERWORLD_MVP_PLAN.md) for UX detail.

**Cards:** not `prize_build_offers` random 3 — spawn **every card still in that NPC's collection** on the ground; player picks **one** (others poof). First library add → optional inspect screen.

**Sticker paper:** still drops after **any completed battle** (win or loss, not early exit). Pickup adds to inventory.

**Upgrades:** spend sticker paper at an **upgrade NPC / service** (existing `run_campaign_shop_scene` logic) — **not** buying cards.

---

## 8. Optional: digit-based loot piles

Secondary spice (not primary card earn):

- `pile_count = clamp(digit_count(final_score), 1, 5)` for **extra sticker paper** or upgrade flair
- **Palindrome** remains a premium digit-NPC card (Route B trophy candidate)

---

## 9. Card pricing / balance

**No shop prices for cards.** Balance levers:

- Which NPC owns which card ([`NPC_COLLECTION_POC.md`](NPC_COLLECTION_POC.md))
- Loaner vs own-deck win requirements for first copy
- Dupes per NPC (singleton vs stackable copies in their collection)
- Route B story gates per card
- NPC collection refill on sell

Lightweight economy script (future): verify 71 takes across 8 NPCs fits target hours — no battle sim required.

---

## 10. Design pillars (retained)

1. **Verbs, not dex filler** — ~71 distinct behaviors; collection teaches tools
2. **8–10 hour 100%** — finite NPC pools, curated Route B
3. **Souvenir first copy** — every card has an NPC face
4. **Irreversible sell** — graduation, not punishment
5. **Juice in battle** — meta is redistribution; combat stays rich

---

## 11. Implementation checklist

### Done (campaign layer, pre-overworld)

- [x] Starter deck: +1..+4 wheels + Toppings/Clover/Burger pick
- [x] Sell gate: `campaign_collection_complete()` — 71/71 required
- [x] Status UI: Collection X/71, Sell locked / Sell Collection

### Next (overworld)

- [ ] `NpcDef`: `collection[]` = `loaner_deck[]`, `campaign_mode`, record
- [ ] Win → spawn **all remaining NPC collection** as ground loot; pick one → `library_add_card` + remove from NPC
- [ ] Sticker paper drop after any completed battle
- [ ] Loaner battle vs own-deck battle entry points
- [ ] Minimap + depleted dialogue
- [ ] Load [`NPC_COLLECTION_POC.md`](NPC_COLLECTION_POC.md) into `world_data` (+ per-NPC `CampaignMode`)
- [ ] Retire `prize_build_offers` card path; keep upgrade shop for sticker paper
- [ ] Route B flags + collection refill rules

### Deferred

- [ ] Route B trophy objectives + rare card unlocks
- [ ] Hades-style dialogue / `collection_generation` (design note only until story pass)
- [ ] Digit pile bonus sticker drops
- [ ] `tools/economy_balance.py` — hours-to-71 sanity check


## 12. Cursor bootstrap

```
Read docs/PROGRESSION_PLAN.md and docs/NPC_COLLECTION_POC.md before overworld meta work.
Card acquisition is NPC collection wins — no shops, no prize_build_offers for cards.
Do not commit unless asked.
```
