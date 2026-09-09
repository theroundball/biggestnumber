# NPC collection POC — deck size ~10

**Status:** Planning / proof-of-concept  
**Rule:** Each NPC's **collection = loaner deck** (~10 cards). Win → take a card → it's gone from them next visit.

## Headcount math

| Pool | Count |
|------|------:|
| Total card types | 71 |
| Target cards per NPC | ~10 |
| **Minimum NPCs (no overlap)** | **8** (`ceil(71 ÷ 10)`) |
| This POC | **8 NPCs** (10 + 6×9 + 7 = 71) |

Player starter (not on NPCs for first copy): Longboard, Heelys, Scooter, Skateboard + pick (Toppings / Clover / Burger).  
NPCs still hold those card types for **dupe** wins after the player owns one.

---

## POC roster

| # | NPC (working name) | Cards | Lane |
|---|-------------------|------:|------|
| 1 | **Wheelie** | 10 | Transport + Toppings |
| 2 | **Combo Kid** | 9 | Combo sets |
| 3 | **Clock Shop** | 9 | Future seeds & finishers |
| 4 | **Graveyard Gate** | 9 | GY basics & tools |
| 5 | **Undertaker** | 9 | GY deep & ghosts |
| 6 | **Librarian** | 9 | Deck search & cantrips |
| 7 | **Digit Hermit** | 9 | Digit manipulation |
| 8 | **Odd Jobs** | 7 | Risks, spikes, stragglers |

---

## 1. Wheelie (10) — scorekeeper energy

*Loaner feel:* straight +N chains, Toppings spike, cantrip cycling. Reliable **2-digit** scores early.

| Card | Role in deck |
|------|----------------|
| Longboard | +1 |
| Heelys | +2 |
| Scooter | +3 |
| Skateboard | +4 |
| Roller Blades | +5 |
| Wagon | +6 |
| Bike | +9 |
| Toppings | ×2 + play top |
| Cycle | +2, exile self draw 1 |
| Cycle Seven | +7, exile self draw 1 |

**Loaner list:**  
`LB, Heelys, Scooter, Skateboard, RB, Wagon, Bike, Toppings, Cycle, Cycle Seven`

---

## 2. Combo Kid (9) — playground

*Loaner feel:* weak until combo pieces align; teach **set bonuses**. Usually 2-digit unless combo fires.

| Card | Combo |
|------|-------|
| Rock | RPS |
| Paper | RPS |
| Scissors | RPS |
| Shoot | RPS |
| Peanut Butter | PB&J |
| Jelly | PB&J |
| Straw | Pigs |
| Sticks | Pigs |
| Bricks | Pigs |

**Loaner:** all nine combo pieces (no +N glue — intentional fragility).

---

## 3. Clock Shop (9) — time & power

*Loaner feel:* setup rounds, then Overclock/Finale blowout. Teaches **future** rows.

| Card | Notes |
|------|-------|
| Sips | +2 ×3 rounds |
| Snail Mail | +5/+5/×5 seeds |
| Time is Too Expensive | +2×round |
| Time is Money | ×2×round |
| Seven Feet Deep | draw 1/2/7 opens |
| Overclock | ×2 + optional discards |
| Finale | draw 5, early end |
| Evaluate | pull future row early |
| Semaphore | round-1 spike / GY mult |

---

## 4. Graveyard Gate (9) — GY front door

*Loaner feel:* Bones-adjacent without full engine. Discard costs teach **payoffs**.

| Card | Notes |
|------|-------|
| Bones | GY scaling |
| Busted | +3 / discard +10 |
| Threshold | discard draw + ×3 |
| Tombstones | unique types mult |
| Jacks | discard → GY hand |
| Fishing Pole | discard → GY top |
| Shells | draw / discard / GY top |
| Roll Over | GY swaps |
| Journal | +cards played |

---

## 5. Undertaker (9) — GY deep

*Loaner feel:* shuffle-back, ghosts, exile. Slower, spikier 2–3 digit when it clicks.

| Card | Notes |
|------|-------|
| Lifeline | GY → deck bottom |
| Necromancy | exile self, shuffle GY |
| Rags to Riches | exile GY → mult |
| Birds of a Feather | GY return chain |
| Dead Rising | round-start GY return |
| Comeback | ghost +3 |
| Encore | ghost +6 |
| Get Me Outa Here | reactive +9 |
| Clover | exile 3 → ×3 |

---

## 6. Librarian (9) — deck order

*Loaner feel:* peek, play-from-deck, Miracle top. **Skill-testing** loaner.

| Card | Notes |
|------|-------|
| Hacker | play from deck |
| Librarian | scry 7, play 1 |
| Pilot | scry 3, play 1 |
| Miracle | topdeck reward |
| Speculative | mill-to-play |
| Flex | mill until hit |
| Swivel | next card to top |
| Wishes | draw 3 |
| Catnip | +1 draw 1 |

---

## 7. Digit Hermit (9) — shape the number

*Loaner feel:* puzzle scoring; **Palindrome** bomb when total fits. Own-deck only for late bombs.

| Card | Notes |
|------|-------|
| Swap | digit swap |
| The Fourth | move 4s |
| The Fifth | replace with 5 |
| Palindrome | wrap in 1s (**premium take**) |
| Build a Number | 3-digit builder |
| Minor Fall | min digit right |
| Major Lift | max digit left |
| Roundup | round total up tiers |
| Dilla | +18 if 0 in score |

---

## 8. Odd Jobs (7) — misc power

*Loaner feel:* uneven; rewards players who know cards. Smallest pool — **runs out fast**.

| Card | Notes |
|------|-------|
| Stoller | +7 transport |
| Rip Stick | +8 transport |
| Big Kurosawa Burger | discard → ×4 |
| Turtle Mode | delay commit |
| Triptych | +3, ×3 if ÷3 |
| Solo | +11, draw if alone |
| Bounty | per-copy GY tracker |

**Loaner list:**  
`Stoller, Rip Stick, Burger, Turtle, Triptych, Solo, Bounty`

---

## Coverage checklist

All 71 `CardType` values appear **exactly once** across sections 1–8.  
**Verify in implementation:** enum order vs this table when building `NpcDef`.

### Master assignment (copy/paste)

```
Wheelie:        LB, Heelys, Scooter, Skateboard, RB, Wagon, Bike, Toppings, Cycle, Cycle Seven
Combo Kid:      Rock, Paper, Scissors, Shoot, PB, Jelly, Straw, Sticks, Bricks
Clock Shop:     Sips, Snail Mail, TITE, TIM, 7 Feet Deep, Overclock, Finale, Evaluate, Semaphore
Graveyard Gate: Bones, Busted, Threshold, Tombstones, Jacks, Fishing Pole, Shells, Roll Over, Journal
Undertaker:     Lifeline, Necromancy, Rags, Birds, Dead Rising, Comeback, Encore, GMOAH, Clover
Librarian:      Hacker, Librarian, Pilot, Miracle, Speculative, Flex, Swivel, Wishes, Catnip
Digit Hermit:   Swap, Fourth, Fifth, Palindrome, Build a Number, Minor Fall, Major Lift, Roundup, Dilla
Odd Jobs:       Stoller, Rip Stick, Burger, Turtle, Triptych, Solo, Bounty
```

### Quick index (NPC owner)

| NPC | Count |
|-----|------:|
| Wheelie | 10 |
| Combo Kid | 9 |
| Clock Shop | 9 |
| Graveyard Gate | 9 |
| Undertaker | 9 |
| Librarian | 9 |
| Digit Hermit | 9 |
| Odd Jobs | 7 |
| **Total** | **71** |

---

## Playthrough sketch (Route A)

1. Starter 5 cards — weaker than any NPC loaner.  
2. Visit **Wheelie** — loaner 10 transport lines → take Bike or Toppings.  
3. Visit **Graveyard Gate** — take Bones; their loaner weakens.  
4. Crossover: your deck ≈ 8–12 cards, mix of takes.  
5. Chase last types from **Combo Kid** (9) and **Digit Hermit** (Palindrome last).  
6. **71/71** → Sell Collection unlocks.  
7. Route B: NPC collections refill (design TBD) + story-only adds.

---

## Minimap states (reminder)

| Icon | Condition |
|------|-----------|
| ● | NPC has ≥1 card you don't own (or dupes allowed) |
| ○ | Their collection is empty **for you** |
| ! | New Route B dialogue / beat |

---

## Open tuning

- [ ] Odd Jobs is only 7 cards — fine for a “side” NPC, or steal 2 from Clock Shop for 9 each  
- [ ] Which cards require **own deck** win vs loaner win for first copy  
- [ ] Max dupes per NPC (run 1× each vs Bones×3 on Undertaker)  
- [ ] Route B collection refill rules  
- [ ] Hardcode `NpcDef` in `world_data` when overworld lands
