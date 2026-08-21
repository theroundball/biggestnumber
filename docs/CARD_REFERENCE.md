# Card reference (review copy)

Edit the **UI description** column when you want wording changes. The **Code behavior** column reflects what the game does today in `card_data.cpp`, `apply_card_play`, combos, trinkets, and related systems.

Legend:
- **Art:** `yes` = dedicated sprites in `graphics/`; `placeholder` = uses Sips sprite fallback in `make_card()`.
- Combo pieces score via `combo_system.cpp` when the full set appears together in hand, graveyard, or scry/deck-search reveal — not from their individual play effects.

| Name | UI description (editable) | Code behavior | Art |
|------|---------------------------|---------------|-----|
| Sips | +2 for each of the next 3 rounds. | On play: schedules +2 round score for each of the next 3 rounds (`future[0..2]`). No immediate score. Not in starter wheel set. | yes |
| Longboard | +1 | On play: +1 immediate round score (`immediate_plus`). | yes |
| Heelys | +2 | On play: +2 immediate. | yes |
| Scooter | +3 | On play: +3 immediate. | yes |
| Skateboard | +4 | On play: +4 immediate. | yes |
| Roller Blades | +5 | On play: +5 immediate. | yes |
| Wagon | +6 | On play: +6 immediate. | yes |
| Stoller | +7 | On play: +7 immediate. | yes |
| Rip Stick | +8 | On play: +8 immediate. | yes |
| Bike | +9 | On play: +9 immediate. | yes |
| Clover | Exile 3 cards from your graveyard, multiply round score by 3. Nothing happens when played if you don't have enough cards in your graveyard. | Enters GY before `on_play`. If GY size ≥ 3 (including Clover), queues graveyard exile picker (exactly 3) then ×3. If GY < 3, fizzles. Can exile Clover from the picker. | yes |
| Big Kurosawa Burger | Discard a hand card to multiply this round by 4. Nothing happens when played if this is your only card. | Enters GY before `on_play`. If hand still has a card to discard, queues discard-from-hand then ×4. If hand empty after routing, fizzles. Discard target shows red **X**. | yes |
| Rock | Part of the Rock-Paper-Scissors-Shoot combo. | No own play effect. Combo with Paper + Scissors + Shoot (any order in window): +100 total score, cinematic removes combo cards from hand/GY/reveal. | yes |
| Paper | Part of the Rock-Paper-Scissors-Shoot combo. | Same as Rock (combo piece). | yes |
| Scissors | Part of the Rock-Paper-Scissors-Shoot combo. | Same as Rock (combo piece). | yes |
| Shoot | Complete Rock-Paper-Scissors-Shoot for +100. | Same as Rock (combo piece). | yes |
| Peanut Butter | Part of the Peanut Butter and Jelly combo. | No own play effect. Combo with Jelly: +25. | yes |
| Jelly | Complete Peanut Butter and Jelly for +25. | Combo with Peanut Butter: +25. | yes |
| Straw | Part of the Straw, Sticks, and Bricks combo. | Combo piece for Straw + Sticks + Bricks: +50. | yes |
| Sticks | Part of the Straw, Sticks, and Bricks combo. | Combo piece. | yes |
| Bricks | Complete Straw, Sticks, and Bricks for +50. | Combo piece. | yes |
| Lifeline | Shuffle your graveyard into deck, then exile insert-number-here (1/4 of the starting deck size) cards from your deck. | On play: queues `RECLAIM_GRAVEYARD` (runs after Lifeline hits GY). Shuffles entire GY into deck, shuffles deck, exiles undrawn bottom quarter of starting deck size. | yes |
| Snail Mail | Draw 1 an extra card next round, draw an extra card two rounds from now, add 5 to your end of round multiplier three rounds from now. | On play: no immediate/on_play callback. Sets future modifiers — next round +1 draw at start, following round +1 draw, third +×5 round multiplier (via `RoundModifier` fields). | yes |
| Wishes | Draw 3 cards. | On play: draws up to 3 cards from deck (`from_deck_draw=true`; deck→hand flight animation per card). | yes |
| Busted | +3 when played. +10 when discarded | On play: +3 immediate. On discard/GY entry (`on_discard`): +10 to round. | yes |
| Swivel | The next card played is put on top of your deck instead of in your graveyard. | On play: sets `swivel_waiting=true`. Next played card routes to deck top (animation) instead of GY unless Necromancy/exile rules override. | yes |
| Roundup | Round total score up to the next highest tens place, then hundreds place, then thousandths place, and so on, incrementing each time this card is played. | On play: 1st play rounds total up to next ×10; 2nd to ×100; 3rd+ to ×1000. Adds the difference as a total-score pop. Does not affect round score. | yes |
| Hacker | Play any card from your deck. | On play: compacts deck; if cards remain, queues `DECK_SEARCH` UI. Chosen card is played via deck-search resolve (Miracle +10 if index 0). | yes |
| Librarian | Look at the top 7 cards of your deck, reorder them, and choose and play one. | On play: queues scry of up to 7 cards. Reorder with B+LR; play one with confirm (Miracle +10 if top of peek). Unplayed peek cards return to deck top in order. | yes |
| Pilot | Look at the top 3 cards of your deck, reorder them, and choose and play one. Reorder, play 1. | Same as Librarian but peek count = min(3, deck size). | yes |
| Turtle Mode | Delay round score evaluation for 3 rounds. | On play: sets `turtle_rounds_remaining=3`. Round end commits are deferred until counter hits 0; round score can accumulate across those rounds. | yes |
| Time is Too Expensive | Add 2 times the current round number to this round's score. | On play: adds `current_round * 2` to round score. | yes |
| Birds of a Feather | +5 Return birds of a feather to you hand when there are n consecutive cards named Birds of a feather in your graveyard, where n = 1 + the number of times a card named birds of a feather has been played this game. | Enters GY before `on_play`. On play: +5. When a bird enters GY, `check_birds_of_a_feather` runs: consecutive GY run of birds ≥ `1 + birds_played_count` returns that run to hand. Increments `birds_played_count` after each play. | yes |
| Necromancy | Exile this Necromancy, shuffle your graveyard and add it to the bottom of your deck. | On play: exiles self (hand play only), then GY→deck shuffle animation; on complete shuffles all GY cards into deck bottom (random order). | placeholder |
| Miracle | Auto-plays for +10 if it is the first card drawn in a round or played from the top of your deck; else +3. | **Top/first-draw mode (+10):** center play presentation from deck icon, then +10 via `play_miracle_bonus` when: first card drawn at round start (then deal 5 more), scry play from peek top, deck-search pick at index 0, or Toppings `PLAY_DECK_TOP`. **Normal hand play:** center presentation, then `on_play` adds +3. | placeholder |
| Rags to Riches | Exile n cards from your graveyard cards, then multiply your round total by the number of cards exiled this way. | On play: if GY non-empty, opens GY exile picker (optional picks); on confirm multiplies round by exiled count. B cancels. | placeholder |
| Jacks | Discard another card, then put a card from your graveyard into your hand. Nothing happens if you have no other card to discard.  | Enters GY before `on_play`. If hand non-empty and GY non-empty, queues discard-from-hand then GY→hand retrieve. Discard target shows red **X**. | yes |
| Fishing Pole | Discard a card, then put a card from your graveyard on top of your deck. Nothing happens if you have no other card to discard.  | Enters GY before `on_play`. If hand non-empty and GY non-empty, queues discard then GY→deck-top retrieve. Discard target shows red **X**. | yes |
| Cups | Draw 1, discard 1, then put a card from your graveyard on top of your deck. Nothing happens if you have no cards to draw.  | Enters GY before `on_play`. Draws 1; queues discard; if GY non-empty, queues GY→deck-top. | yes |
| Swap | Choose two digits in your total or round score and swap them. The resulting number cannot be smaller | On play: if round + total digit count ≥ 2, opens dual-score digit swap UI. Swap must strictly increase total score. | placeholder |
| Catnip | +1 to this round. Draw 1. | On play: +1 immediate, then draws 1 card (deck flight). | placeholder |
| Journal | +n to this round, where n is cards played this round. | On play: adds `cards_played_this_round + 1` to round (includes itself after increment in pipeline). | placeholder |
| Triptych | +3 to this round. Multiply by 3 if the total is divisible by 3. | On play: +3 immediate; if committed round score divisible by 3, ×3. | placeholder |
| Seeds | Exile 3 random cards from your deck, then pick 3 cards from your graveyard to put on top of your deck. Nothing happens if you have less than 3 cards in your deck. | Enters GY before `on_play`. Fizzles unless deck has ≥3 undrawn cards and GY has ≥3 cards. Exiles 3 random undrawn deck cards, then GY pick-3-to-top UI. | placeholder |
| Dilla | +18 to this round if your round score contains a 0. Otherwise +8. | On play: checks committed round score for digit 0; +18 or +8. | placeholder |
| Semaphore | If Semaphore is the first card played of round 1: +100. Last card in hand with an empty deck: add multiplier of n, where n is the number of cards in your graveyard. Otherwise +3. | On play: round 1 first card → +100; else if last card in hand and deck empty → ×GY size (if >1); else +3. | placeholder |
| Bones | Add 1+n to your round score for each card in your graveyard, where n is the number of Bones cards in your graveyard. | No play effect. On discard/GY entry: + (GY size) × (1 + other Bones already in GY). | placeholder |
| Threshold | +3 when played. When discarded, if there are 7 or more cards in your graveyard draw 1 card and multiplyer your round score by 3.  | Enters GY before `on_play`. On play: +3. On cost discard: if GY size > 7 (8+ including self), draw 1 and ×3; else +3. | placeholder |
| Tombstones | +3 when played. multiply your round score by n when discarded, where n is unique card names in your graveyard. | Enters GY before effects. On play: +3 then × unique GY types (including self). On cost discard: × unique GY types (no extra +3). | placeholder |
| Roll Over | +3 when played. And swap two graveyard cards, 3 times. | Enters GY before `on_play`. On play: +3; if GY≥2, queues 3× GY pair swap UI. | yes |
| Toppings | Multiply this round by 2, then play the top card in your deck. | On play: ×2 immediate, then queues `PLAY_DECK_TOP` — top card plays from deck HUD with center presentation (Miracle +10 if Miracle). Starter wheel card. | yes |

---

## Trinkets

Run loadout supports up to **3 trinket slots** (deck editor saves 2 equipped + empty third in campaign flow). Default starter loadout: **Morel**, **Lucky 7**, **Prime Time**.

Edit the **UI description** column for player-facing text. **Code behavior** is from `trinket_system.cpp`, `game_state.cpp`, and `game_context.cpp`.

| Name | UI description (editable) | Code behavior | HUD art |
|------|---------------------------|---------------|---------|
| Morel | When a card adds to round score, add +2 more after reactions settle. | Hooks `add_from_card` only (not trinket adds or multipliers). Each positive card add increments `deferred_morel_count`; after Lucky 7 / Prime Time / score-pop reaction stack is idle, applies +2 to round with trinket score-pop flight. Can chain multiple deferred +2s. | yes |
| Lucky 7 | When score contains 7, roll 7–13 and add that amount. | After any score change check (`trinket_queue_score_check`): if round or total score **contains digit 7** and value changed, queues Lucky 7 roulette FX (~120 frames). Rolls 7–13, flies result to score, then applies add. Skips re-trigger if the added amount itself contains 7 (anti-loop). Separate checks for round vs total field. | yes |
| Echo | Echoes the first card you play with a play effect each round. | Once per round while `echo_ready`: first played card with a play effect arms `echo_pending_replay`. When idle (no pending actions, presentation FX, deck search, removal, or draw FX), replays that card's **play effect only** via center presentation (`PlaySource::ECHO`); does not re-route to GY/deck. Consumes echo for the round on replay. Shows Echo badge during armed/replay. | yes |
| Get With The Times | Card adds do nothing; gain +6 at the start of each round instead. | Blocks all positive `add_from_card` (shows empty pop, adds 0). At round start (`apply_round_start_trinkets`): +6 to round with trinket flight. | placeholder |
| Prime Time | When round or total score are prime numbers, add n where n is the number of times your round or total score have been a prime number. | After score change: if round **committed** score or total score is prime, increments proc counter for that field and adds proc count to round (+1, then +2, then +3, …). Round procs reset each round; total procs persist for the run. Trinket score-pop flight. Can chain with Lucky 7 / Morel. | placeholder |

### Trinket reaction order (summary)

1. Card / combo `add_from_card` or multiplier applies → score check queued (Lucky 7, Prime Time).
2. Reaction queue drains (Lucky roulette → score pop lands → Prime procs, etc.).
3. Morel deferred +2 flushes when reaction stack is idle (`trinket_flush_deferred_modifiers`).
4. Morel +2 itself queues another score check.

Trinket adds and multipliers do **not** trigger Morel. Get With The Times blocks step 1 card adds entirely.

---

## Shared systems (not per-card rows)

| System | Behavior |
|--------|----------|
| Instance upgrades | Plus digit, ×2 upgrade, Lead/Yeast gravity modify effective +N/×N on play via `CardInstance` pool. |
| Combos | RPS+Shoot +100; PB+Jelly +25; Straw+Sticks+Bricks +50. Detected in hand, GY, or reveal buffers. |
| Deck draws | Any `hand_add_card(..., from_deck_draw=true)` queues deck→hand flight: starts small at deck HUD, grows to full size (~8 frames/card), sequential if multiple. Opening hand deals exactly 5 using `scheduled_hand_count` (hand + pending queue + in-flight). |
| Zone transfer FX | GY→hand (Jacks): small→full flight to hand slot. GY→deck top (Fishing Pole, Cups): grow, full in middle ~20%, shrink into deck. Necromancy shuffle: representative GY→deck flight then shuffles all GY to deck bottom. |
| Play presentation | Cards with play/discard abilities: center beat ~16 approach / **15 hold** (~¼ s) / 24 depart frames; scoring resolves at hold→depart. Center Y sits above raised hand (`PLAY_PRESENTATION_CENTER_Y`). Card hidden when depart finishes if waiting on score pops (`WAIT_PRESENTATION`). |
| Plain discards | No `on_play` / cost-only discards: fly straight to GY/deck with scale-down flight, no center hold. |
| Discard targeting | `DISCARD_TARGET` mode shows red **X** on selected hand card (Jacks, Fishing Pole, Cups, Burger, etc.). |
| Echo replay | Same center presentation as original play; badge on armed card. |
| Deck builder test flag | `DECK_EDITOR_TEST_ALL_CARDS = true` in `game_types.h` — catalog shows all cards regardless of library ownership. |

## Notes for description pass

- Threshold UI says "8+ graveyard" but code checks `graveyard.size() > 7` **after** this card enters (so 8+ total).
- Journal `n` in code is `cards_played_this_round + 1` at resolution time.
- Echo inspect text may say "last card"; code replays the **first** card with a play effect each round.
- Get With The Times code gives +6 at round start (blocks card adds).
- Prime Time inspect: "add the proc count" — 1st prime in a field +1, 2nd +2, etc.; round and total tracked separately.

_Last updated from codebase audit — update Code behavior when implementation changes._
