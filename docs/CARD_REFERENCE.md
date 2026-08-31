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
| Clover | Exile 3 cards from your graveyard, multiply round score by 3. Nothing happens when played if you don't have enough cards in your graveyard. | **Held** until pending UI finishes (`defer_graveyard_until_pending`). If GY size ≥ 3, queues graveyard exile picker (exactly 3) then ×3. If GY < 3, effect fizzles and Clover still goes to GY. | yes |
| Big Kurosawa Burger | Discard a hand card to multiply this round by 4. Nothing happens when played if this is your only card. | Enters GY before `on_play`. If hand still has a card to discard, queues discard-from-hand then ×4. If hand empty after routing, fizzles. Discard target shows red **X**. | yes |
| Rock | Part of the Rock-Paper-Scissors-Shoot combo. | No own play effect. Combo with Paper + Scissors + Shoot (any order in window): ×4 total score, cinematic removes combo cards from hand/GY/reveal. | yes |
| Paper | Part of the Rock-Paper-Scissors-Shoot combo. | Same as Rock (combo piece). | yes |
| Scissors | Part of the Rock-Paper-Scissors-Shoot combo. | Same as Rock (combo piece). | yes |
| Shoot | Complete Rock-Paper-Scissors-Shoot to multiply total score by 4. | Same as Rock (combo piece). | yes |
| Peanut Butter | Part of the Peanut Butter and Jelly combo. | No own play effect. Combo with Jelly: ×2 total score. | yes |
| Jelly | Complete Peanut Butter and Jelly to multiply total score by 2. | Combo with Peanut Butter: ×2 total score. | yes |
| Straw | Part of the Straw, Sticks, and Bricks combo. | Combo piece for Straw + Sticks + Bricks: ×3 total score. | yes |
| Sticks | Part of the Straw, Sticks, and Bricks combo. | Combo piece. | yes |
| Bricks | Complete Straw, Sticks, and Bricks to multiply total score by 3. | Combo piece. | yes |
| Lifeline | Put 3 cards from your graveyard onto the bottom of your deck. Cannot target Lifeline. Ghost: Shuffle your graveyard into your deck, then exile the top 3 cards. | On play: if another GY card exists, opens GY pick (up to 3, Lifeline excluded) → deck bottom. Ghost: shuffles other GY cards into deck, then exiles top 3 undrawn. | yes |
| Snail Mail | +5 next round, +5 two rounds from now, and ×5 three rounds from now. | Seeds +5, +5, and an end-of-round ×5 into the next three future modifier slots. | yes |
| Wishes | Draw 3 cards. | On play: draws up to 3 cards from deck (`from_deck_draw=true`; deck→hand flight animation per card). | yes |
| Busted | +3 when played. +10 when discarded | On play: +3 immediate. On discard/GY entry (`on_discard`): +10 to round. | yes |
| Swivel | The next card played is put on top of your deck instead of in your graveyard. | On play: sets `swivel_waiting=true`. Next played card routes to deck top (animation) instead of GY unless Necromancy/exile rules override. | yes |
| Roundup | Round total score up to the next highest tens place, then hundreds place, then thousandths place, and so on, incrementing each time this card is played. | On play: 1st play rounds total up to next ×10; 2nd to ×100; 3rd+ to ×1000. Adds the difference as a total-score pop. Does not affect round score. | yes |
| Hacker | Play any card from your deck. | On play: compacts deck; if cards remain, queues `DECK_SEARCH` UI. Chosen card is played via deck-search resolve (Miracle +10 if index 0). | yes |
| Librarian | Look at the top 7 cards of your deck, reorder them, and choose and play one. | On play: queues scry of up to 7 cards. Reorder with B+LR; play one with confirm (Miracle +10 if top of peek). Unplayed peek cards return to deck top in order. | yes |
| Pilot | Look at the top 3 cards of your deck, reorder them, and choose and play one. Reorder, play 1. | Same as Librarian but peek count = min(3, deck size). | yes |
| Turtle Mode | Delay round score evaluation for 3 rounds. | On play: sets `turtle_rounds_remaining=3`. Round end commits are deferred until counter hits 0; round score can accumulate across those rounds. Pressing B during turtle waives optional ghosts but does not bank round score until turtle expires. | yes |
| Time is Too Expensive | Add 2 times the current round number to this round's score. | On play: adds `current_round * 2` to round score. | yes |
| Time is Money | Multiply this round's score by 2 times the current round number. | On play: multiplies round score by `current_round * 2` (round 1 → ×2, round 5 → ×10). | yes |
| Birds of a Feather | +5. Consecutive runs of 2, then 3, 4, and 5 Birds in your graveyard return to deck. After 5, this stops. | On play: +5. Qualifying contiguous GY run returns via HUD flight (graveyard icon → center → deck icon) without entering GY browse mode. Threshold advances 2→3→4→5, then disables. | yes |
| Necromancy | Exile this Necromancy, shuffle your graveyard and add it to the bottom of your deck. | On play: exiles self (hand play only), then GY→deck shuffle animation; on complete shuffles all GY cards into deck bottom (random order). | placeholder |
| Miracle | Auto-plays for +10 if drawn as the first card of a card-effect draw (chains while Miracle remains on top), at round-start opening deal, or played from deck top; else +3. | **+10 paths:** opening deal first draw; scry/deck-search index 0; `PLAY_DECK_TOP`; first draw slot of `EFFECT_DECK_DRAW` (Catnip, Wishes draw 1, Shells, Cycle follow-up) with Miracle chain until non-Miracle. **Hand play:** +3 via `on_play`. Opening deal is **N attempts** (default 5). A leading Miracle consumes one attempt and does not keep dealing until hand size is N — on a 7 Feet Deep draw-1 round, Miracle is the whole opening deal. | placeholder |
| Rags to Riches | Exile n cards from your graveyard cards, then multiply your round total by the number of cards exiled this way. | On play: if GY non-empty, opens GY exile picker (optional picks); on confirm multiplies round by exiled count. B cancels. | placeholder |
| Jacks | Discard another card, then put a card from your graveyard into your hand. Nothing happens if you have no other card to discard.  | **Held** until discard + retrieve finish. If `hand.size() > 1`, queues discard-from-hand then GY→hand retrieve (freshly discarded card eligible). Solo fizzle: no pending steps, Jacks goes to GY. Discard target shows red **X**. | yes |
| Fishing Pole | Discard a card, then put a card from your graveyard on top of your deck. Nothing happens if you have no other card to discard.  | **Held** until discard + retrieve finish. If `hand.size() > 1`, queues discard then GY→deck-top retrieve. Solo fizzle: card goes to GY. Discard target shows red **X**. | yes |
| Shells | Draw 1, discard a card, then put a card from your graveyard on top of your deck. Skip unavailable steps. | **Held** until pending queue drains. Draws if possible, queues discard if another hand card exists, then GY→deck-top retrieve. Unavailable steps skip via `FIZZLE`. | yes |
| Swap | Choose two digits in your total or round score and swap them. The resulting number cannot be smaller | On play: opens a per-digit picker across both score rows. The first pick stays raised with a green underline; an invalid second pick clears both selections. Total score may stay equal but cannot decrease. | placeholder |
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
| Cycle | +2. Press Down to exile this and draw 1. | Immediate +2. Down exiles it directly from hand without firing discard effects, then draws 1. | placeholder |
| Cycle Seven | +7. Press Down to exile this and draw 1. | Immediate +7 plus Cycle. | placeholder |
| Get Me Outa Here | +9 when played, discarded, or whenever another card moves this. | Immediate +9. Extra +9 on cost discard, GY→exile, and when another card relocates it. Own hand play and same-zone shuffles do not double. Draws do not count. | placeholder |
| Comeback | +3. Ghost: exile from graveyard to play for +3. | Hand play +3 to GY. Its ghost is an optional play while cards remain in the live hand; playing it gives +3 and exiles it. Ghosts do not delay round or game end. | placeholder |
| Encore | +6. Ghost: exile from graveyard to play for +6. | Hand play +6. Ghost +6 then exile. | placeholder |
| Double Time | +8. The next card's add amounts are doubled. | Immediate +8, then `pending_double_adds` doubles the next play's `add_from_card` amounts and future add seeds. Multipliers are not doubled. | placeholder |
| The Fourth | Move a 4 in your total score without making the score smaller. | Select a 4 and destination; invalid decreasing moves cannot be confirmed. Fizzles when no valid move exists. | placeholder |
| Palindrome | If your total score is a palindrome, wrap it in 1s (2s with Dilla). If that fizzles, try your round score. Otherwise +3. | Total first, then round; +3 fallback. Single digits count; overflow fizzles. | placeholder |
| The Fifth | Replace any digit in your total score with a 5. | Opens the total-score digit picker; selecting a digit replaces it with 5. | placeholder |
| Solo | +11. Draw 1 if no other cards remain in your hand after this is played. | Immediate +11. After this card leaves the live hand (or if played from deck with an empty hand), draws 1. Ghosts do not prevent the draw. | placeholder |
| Speculative | Reveal the top card. Play it if it would make the current number bigger, then draw 1. Otherwise mill it. | Queues `MILL_REVEAL` once. If the revealed card would increase the number, it is played (Miracle +10 if Miracle) then draw 1. Otherwise it flies deck→GY with no play and no discard. | placeholder |
| Flex | Mill until you find a card that can add or multiply, then play it. | `MILL_REVEAL` loop: mill misses to GY (including future-only adds like Sips counting as a hit). Play the first add-or-multiply card. | placeholder |
| Dead Rising | Exile this. At the start of each of the next 3 rounds, put 2 random cards from your graveyard on top of your deck. | Schedules three round-start returns of 2 cards each. Empty graveyards still consume charges; Dead Rising itself is eligible. Resolves before the post-draw game-over check. HUD label `rise` / `rise 2`. | placeholder |
| 7 Feet Deep | Next 3 rounds, draw 1, then 2, then 7 instead of 5. Missing cards come from the graveyard at random. | Rare singleton. Seeds `opening_draw_count` 1/2/7. Short opening deals run the Dead Rising GY→deck movie, then a normal N-attempt deal. Empty GY deals short. Floor 1 if famine stacks. HUD shows resulting size (`draw 1`), never a negative. Miracle on draw-1 is a complete opening deal. | placeholder |
| Bounty | +n when this copy is played (n starts at 0 and +1 on play, so first play is +1). While in GY, this copy returns to hand when this round rises by its bounty (starts at 10; when this copy's n reaches 10, bounty becomes 100). | Per-copy play count and per-copy return bar (`progress/threshold` in GY). Hand overlay shows the next +n. Each copy tracks n and bounty independently. | yes |
| Overclock | Multiply this round by 2. You may discard cards from your hand. Each time you do, multiply again by 3, then 4, then 5, and so on. Stop when you like. | On play: ×2, then optional discard prompt chain with escalating multipliers. | yes |
| Evaluate | Apply the next scheduled round modifier now (+ then ×). That round still gets it later. Ghost: apply all three scheduled rows in UI order (+, ×, draw left-to-right per row), clearing each from the details list as it resolves, and end Turtle Mode. | Normal play applies the next row once without clearing. Ghost queues stepped pending actions (one UI component per frame). | yes |
| Build a Number | Replace your round score with three digit slots. Play digit cards to fill them. When all three are filled, add that number to this round. | Activates digit builder UI. Playing +1…+9 queues slot placement: cursor skips filled slots, pending digit shown in brackets, third digit auto-completes. | yes |
| Minor Fall | Move the smallest digit in your total score to the rightmost position (tie: rightmost). If that fizzles, try your round score. Otherwise +3. | Digit-slide on total, then round; +3 fallback. | placeholder |
| Major Lift | Move the largest digit in your total score to the leftmost position (tie: leftmost). If that fizzles, try your round score. Otherwise +3. | Digit-slide on total, then round; +3 fallback. | placeholder |
| Finale | Draw 5. This turn, when your hand is empty the run ends without adding this round's score to your total. | Sets `finale_active`, draws 5. Empty hand or B ends run with `final_score = total_score` and no round commit. | placeholder |

---

## Trinkets

Run loadout supports up to **3 trinket slots** (deck editor saves 2 equipped + empty third in campaign flow). Default starter loadout: **Morel**, **Lucky 7**, **Prime Time**.

Edit the **UI description** column for player-facing text. **Code behavior** is from `trinket_system.cpp`, `game_state.cpp`, and `game_context.cpp`.

| Name | UI description (editable) | Code behavior | HUD art |
|------|---------------------------|---------------|---------|
| Morel | When a card adds to round score, add +2 more after reactions settle. | Hooks `add_from_card` only (not trinket adds or multipliers). Each positive card add increments `deferred_morel_count`; after Lucky 7 / Prime Time / score-pop reaction stack is idle, applies +2 to round with trinket score-pop flight. Can chain multiple deferred +2s. | yes |
| Lucky 7 | When score contains 7, roll 7–13 and add that amount. | After any score change check (`trinket_queue_score_check`): if round or total score **contains digit 7** and value changed, queues Lucky 7 roulette FX (~120 frames). Rolls 7–13, flies result to score, then applies add. Skips re-trigger if the added amount itself contains 7 (anti-loop). Separate checks for round vs total field. | yes |
| Echo | Echoes the first card you play with a play effect each round. | The selected eligible card shows the Echo badge. Its first play resolves fully while a faded copy remains at the original hand position; after the pipeline becomes idle, the complete `CardRef` animates and resolves a full second play without another zone removal. Both plays increment the round play count. | yes |
| Get With The Times | Card adds do nothing; gain +6 at the start of each round instead. | Blocks all positive `add_from_card` (shows empty pop, adds 0). At round start (`apply_round_start_trinkets`): +6 to round with trinket flight. | placeholder |
| Prime Time | When round or total score are prime numbers, add n where n is the number of times your round or total score have been a prime number. | After score change: if round **committed** score or total score is prime, increments proc counter for that field and adds proc count to round (+1, then +2, then +3, …). Round procs reset each round; total procs persist for the run. Trinket score-pop flight. Can chain with Lucky 7 / Morel. | placeholder |
| Fibonacci | At each round start, add the next value in 1, 1, 2, 3, 5, 8… | Sequence resets each battle and adds directly to round score as a trinket-sourced effect. | placeholder |

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
| Combos | RPS+Shoot ×4 total; PB+Jelly ×2 total; Straw+Sticks+Bricks ×3 total. A zero total remains zero. Detected in hand, GY, or reveal buffers. |
| Deck draws | Any `hand_add_card(..., from_deck_draw=true)` queues deck→hand flight: starts small at deck HUD, grows to full size (~24 frames/card), sequential if multiple. Opening hand deals exactly 5 using `scheduled_hand_count` (hand + pending queue + in-flight). Deck HUD shows `deck.remaining()` plus in-transit draws. |
| Zone transfer FX | Jacks, Fishing Pole, and Shells use interactive transfer presentations. Birds return via graveyard HUD → center → deck HUD flight. Dead Rising (and 7 Feet Deep opening-draw fill) move GY cards to deck top at round start. Necromancy shuffles all GY to deck bottom. |
| Play presentation | Cards with play/discard abilities: center beat ~16 approach / **15 hold** (~¼ s) / 24 depart frames; scoring resolves at hold→depart. Center Y sits above raised hand (`PLAY_PRESENTATION_CENTER_Y`). Card hidden when depart finishes if waiting on score pops (`WAIT_PRESENTATION`). |
| Plain discards | No `on_play` / cost-only discards: fly straight to GY/deck with scale-down flight, no center hold. |
| Discard targeting | `DISCARD_TARGET` mode shows red **X** on selected hand card (Jacks, Fishing Pole, Shells, Burger, etc.). |
| Echo replay | Same center presentation as original play; badge on armed card. |
| Deck builder test flag | `DECK_EDITOR_TEST_ALL_CARDS = true` in `game_types.h` — catalog shows all cards regardless of library ownership. |

## Notes for description pass

- Threshold UI says "8+ graveyard" but code checks `graveyard.size() > 7` **after** this card enters (so 8+ total).
- Journal `n` in code is `cards_played_this_round + 1` at resolution time.
- Echo inspect text may say "last card"; code replays the **first** card with a play effect each round.
- Get With The Times code gives +6 at round start (blocks card adds).
- Prime Time inspect: "add the proc count" — 1st prime in a field +1, 2nd +2, etc.; round and total tracked separately.

_Last updated from codebase audit — update Code behavior when implementation changes._
