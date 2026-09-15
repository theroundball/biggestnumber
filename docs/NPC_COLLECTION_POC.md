# NPC collection POC — deck size 10–13

**Status:** Implemented in `world_data.cpp`  
**Rule:** Each NPC's **collection = loaner deck** (10–13 cards). Win → take a card → it's gone from them next visit.

## Headcount math

| Pool | Count |
|------|------:|
| Total collectible card types | 72 |
| Target cards per NPC | 10–13 |
| **NPCs** | **8** |
| Cards on NPCs (with dupes) | 96 |

Player starter (not on NPCs for first copy): Longboard, Heelys, Scooter, Skateboard + pick (Toppings / Clover / Burger).  
NPCs still hold those card types for **dupe** wins after the player owns one.

**Phase 1 modes:** Biggest Number (7 NPCs) + Same Number (Combo Kid only).

---

## POC roster

| # | NPC | Cards | Mode |
|---|-----|------:|------|
| 1 | **Wheelie** | 10 | Biggest Number |
| 2 | **Combo Kid** | 10 | Same Number |
| 3 | **Clock Shop** | 9 | Biggest Number |
| 4 | **Graveyard** | 11 | Biggest Number |
| 5 | **Undertaker** | 13 | Biggest Number |
| 6 | **Librarian** | 13 | Biggest Number |
| 7 | **Digit Hermit** | 9 | Biggest Number |
| 8 | **Odd Jobs** | 11 | Biggest Number |

---

## Duplicate stacks (by design)

| NPC | Stack |
|-----|-------|
| Undertaker | Birds of a Feather ×5 |
| Odd Jobs | Bounty ×5 |
| Graveyard | Bones ×3 |
| Librarian | Catnip ×5 |

---

## Master assignment (source: `world_data.cpp`)

```
Wheelie:        LB, Heelys, Scooter, Skateboard, RB, Wagon, Bike, Toppings, Cycle, Cycle Seven
Combo Kid:      Rock, Paper, Scissors, Shoot, PB, Jelly, Straw, Sticks, Bricks, Make It a Combo
Clock Shop:     Sips, Snail Mail, TITE, TIM, 7 Feet Deep, Turtle Mode, Finale, Evaluate, Semaphore
Graveyard:      Bones×3, Busted, Threshold, GMOAH, Jacks, Fishing Pole, Shells, Roll Over, Journal
Undertaker:     Lifeline, Necromancy, Rags, Birds×5, Dead Rising, Comeback, Encore, Tombstones, Clover
Librarian:      Hacker, Librarian, Pilot, Miracle, Speculative, Flex, Swivel, Wishes, Catnip×5
Digit Hermit:   Swap, Fourth, Fifth, Palindrome, Build a Number, Minor Fall, Major Lift, Roundup, Dilla
Odd Jobs:       Stoller, Rip Stick, Burger, Overclock, Triptych, Solo, Bounty×5
```

### Quick index

| NPC | Count |
|-----|------:|
| Wheelie | 10 |
| Combo Kid | 10 |
| Clock Shop | 9 |
| Graveyard | 11 |
| Undertaker | 13 |
| Librarian | 13 |
| Digit Hermit | 9 |
| Odd Jobs | 11 |
| **Unique types** | **72** |

---

## Card swaps (from pre-rework layout)

| Card | From | To |
|------|------|-----|
| Overclock | Clock Shop | Odd Jobs |
| Turtle Mode | Odd Jobs | Clock Shop |
| Get Me Outa Here | Undertaker | Graveyard |
| Tombstones | Graveyard | Undertaker |

---

## Benchmark ladder (shared)

`10, 20, 35, 55, 90, 140, 220, 340, 500, 700, 850, 1000`

Per-NPC index: `taken = collection_count - cards_remaining` → `rung = NPC_BENCHMARKS[taken]`.

BN win: `score > rung`. SN win (Combo Kid): `score == rung`.

---

## Coverage checklist

All 72 collectible `CardType` values appear across sections 1–8 (singleton per type unless noted dupes).  
Sell gate: `campaign_collection_required_count()` (all types with `max_copies > 0`).
