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

## 5. NPC count & card split

| Metric | Value |
|--------|------:|
| Total card types | 71 |
| Target cards per NPC | ~10 |
| **NPCs needed** | **8** (`ceil(71 ÷ 10)`) |

POC assignment: **10 + 6×9 + 7 = 71** across eight NPCs. See [`NPC_COLLECTION_POC.md`](NPC_COLLECTION_POC.md) for names, loaner lists, and master assignment table.

---

## 6. Route A vs Route B

| | Route A (first collection) | Route B (after sell) |
|--|---------------------------|----------------------|
| **Goal** | One of each card (71/71) → sell unlocks | Re-earn collection + story exclusives |
| **NPC collections** | Full at start; deplete as player wins | Refill rules TBD (partial or full + new cards) |
| **Card access** | Win from any NPC with cards left | Story-gated re-unlocks; harder / more work |
| **Trinkets** | Story milestones after sell-value threshold (planned) | Primary trinket source |
| **Loaner** | Strong early; weak late | NPCs may refuse loaner or offer Route B decks |

Route A is **breadth** (easy spread across NPCs). Route B is **depth** (story, selective re-collection).

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

### No shops

Optional **themed ground pickups** (wheel shop stuff, graveyard shop stuff) reserved for **instance upgrades** or flavor only — not card purchases. Cards come only from NPC wins unless a story beat says otherwise.

---

## 8. Optional: digit-based loot piles

If post-battle ground loot returns:

- `pile_count = clamp(digit_count(final_score), 1, 5)`
- Each pile = random labeled prop → **upgrade currency** or sticker paper, **not** cards
- **Palindrome** remains a premium digit-NPC card (expensive in effort, not coins)

Primary earn rate for cards = **NPC wins**, not score magnitude.

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
- [ ] Win → remove card from NPC + `library_add_card`
- [ ] Loaner battle vs own-deck battle entry points
- [ ] Minimap + depleted dialogue
- [ ] Load [`NPC_COLLECTION_POC.md`](NPC_COLLECTION_POC.md) into `world_data`
- [ ] Retire / bypass `run_campaign_prize_scene` for card prizes on overworld path
- [ ] Route B flags + collection refill rules

### Deferred

- [ ] Trinkets from story after sell threshold
- [ ] Digit pile ground loot (upgrades only)
- [ ] `tools/economy_balance.py` — hours-to-71 sanity check

---

## 12. Cursor bootstrap

```
Read docs/PROGRESSION_PLAN.md and docs/NPC_COLLECTION_POC.md before overworld meta work.
Card acquisition is NPC collection wins — no shops, no prize_build_offers for cards.
Do not commit unless asked.
```
