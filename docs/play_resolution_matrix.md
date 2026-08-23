# Play resolution matrix

Spec for `resolve_played_card` / `route_played_card`. Preserve these invariants when migrating call sites.

## 1. Order of operations

1. Route destination from card flags + pre-play `swivel_waiting` snapshot.
2. Apply zone placement **before** `apply_card_play` (GY / exile). Play routing does **not** fire `on_discard`.
3. `apply_card_play` (scoring + `on_play`, which may queue pending actions).
4. Tombstones: after play into GY, multiply round by unique GY types (including self).
5. Increment `cards_played_this_round` unless `PlaySource::ECHO`.
6. Swivel follow-up (`DECK_TOP`) is applied by presentation after effects when `apply_destination = false`.

Discard effects (`on_discard`) fire only on **cost discards** via `hand_remove_at_to_graveyard`, not on play routing.

## 2. PlaySource

| Source | Hand removal? | Typical caller |
|--------|---------------|----------------|
| `HAND` | Yes (via hand_index) | Normal play after removal anim |
| `SCRY` | No | Pilot / Librarian confirm |
| `DECK_SEARCH` | No | Hacker pick |
| `DECK_TOP` | No | Toppings deck-top play / mill-reveal |
| `ECHO` | No (card already left) | Idle-gate replay after first play |
| `FLASHBACK` | No (GY exile) | Ghost play from graveyard |

## 3. PostPlayDestination (first match wins)

`swivel_follow` is the **pre-play** `swivel_waiting` snapshot. Playing Swivel itself arms the next card; it is not routed to `DECK_TOP`.

| Priority | Condition | Dest |
|----------|-----------|------|
| 1 | `PlaySource::ECHO` | `NONE` |
| 2 | `PlaySource::FLASHBACK` | `EXILE` (from GY) |
| 3 | `exiles_self_on_play` | `EXILE` (hand) or `NONE` (scry/search/deck-top) |
| 4 | pre-play `swivel_follow` | `DECK_TOP` (presentation owns move when `apply_destination = false`) |
| 5 | else | `GRAVEYARD` |

## 4. Manual test checklist

| # | Setup | Action | Assert |
|---|-------|--------|--------|
| 1 | Clover + 3+ GY | Play Clover | Clover in GY before exile UI; can pick Clover; ×3 after 3 picks |
| 2 | Clover + &lt;3 GY | Play Clover | No pending; Clover in GY |
| 3 | Echo + Pilot | Play Pilot | Scry; then echo replay |
| 4 | Echo + simple +N | Play | Two score apps; one removal; echo consumed |
| 5 | Echo + Swivel | Play | Deck-top follow; echo arms Swivel again after follow |
| 6 | Echo + Hacker | Play Hacker | Search resolve → idle-gate echo replay |
| 7 | Scry Miracle as top | Play | +10 miracle |
| 8 | Deck search Miracle at 0 | Play | +10 miracle |
| 9 | Necromancy | Play | Exiled, not in GY |
| 10 | Bones/Threshold | Cost discard | Effect sees correct GY size |
| 11 | Swivel waiting + hand card | Play | `TO_DECK_TOP` |
| 12 | Pilot from scry + Echo | Play from scry | No regression on current echo-from-scry behavior |
| 13 | Lifeline + cards in GY | Play Lifeline | GY empty after; Lifeline shuffled into deck (not stranded in GY) |
| 14 | Tombstones | Play | +3 then × unique GY types (self counts) |
| 15 | Roll Over | Play | +3 then 3× GY pair swap UI |
| 16 | Swap | Play | Pick digits from round or total; total cannot decrease |
| 17 | Birds ×2 | Play twice | 2nd play needs 2 consecutive birds in GY to return |
| 18 | The Fourth + total containing 4 | Play | Pick a 4 and destination; digits slide and total is reordered |
| 19 | The Fourth + no movable 4 | Play | No picker; +4 fallback |
| 20 | The Fifth | Play | Pick one total-score digit; it becomes 5 |
| 21 | Palindrome + palindromic round | Play | Add digit 1 to both ends (single digits included) |
| 22 | Necromancy as final hand/deck card | Play | Shuffle fully resolves before empty-hand/deck end check |
