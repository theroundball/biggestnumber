# Play resolution matrix

Spec for `#2` (`resolve_played_card` / `route_played_card`). Preserve these invariants when migrating call sites.

## 1. Order of operations

1. `apply_card_play` always runs first (scoring + `on_play`, which may queue pending actions).
2. Route destination **after** apply so `defer_graveyard_until_pending` sees new pending actions.
3. Destination apply (GY / exile / held) unless the caller owns presentation (`apply_destination = false`).
4. Discard effects fire on `graveyard_push`, not on exile or held.

## 2. PlaySource

| Source | Hand removal? | Typical caller |
|--------|---------------|----------------|
| `HAND` | Yes (via hand_index) | Normal play after removal anim |
| `SCRY` | No | Pilot / Librarian confirm |
| `DECK_SEARCH` | No | Hacker pick |
| `ECHO` | No (card already left) | Idle-gate replay after first play |


## 3. PostPlayDestination (first match wins)

`swivel_follow` is the **pre-play** `swivel_waiting` snapshot. Playing Swivel itself arms the next card; it is not routed to `DECK_TOP`.

| Priority | Condition | Dest |
|----------|-----------|------|
| 1 | `exiles_self_on_play` | `EXILE` (hand) or `NONE` (scry/search/echo) |
| 2 | pre-play `swivel_follow` | `DECK_TOP` (presentation owns move) |
| 3 | `defer_graveyard_until_pending` && pending nonempty | `HELD_DEFER` |
| 4 | else | `GRAVEYARD` |

## 4. Manual test checklist

| # | Setup | Action | Assert |
|---|-------|--------|--------|
| 1 | Clover + 3+ GY | Play Clover | Exile UI before Clover in GY; ×3 after 3 picks |
| 2 | Clover + &lt;3 GY | Play Clover | No pending; Clover to GY |
| 3 | Echo + Pilot | Play Pilot | Scry; then echo replay |
| 4 | Echo + simple +N | Play | Two score apps; one removal; echo consumed |
| 5 | Echo + Swivel | Play | Deck-top follow; echo arms Swivel again after follow |
| 6 | Echo + Hacker | Play Hacker | Search resolve → idle-gate echo replay |
| 7 | Scry Miracle as top | Play | +10 miracle |
| 8 | Deck search Miracle at 0 | Play | +10 miracle |
| 9 | Necromancy | Play | Exiled, not in GY |
| 10 | Bones/Threshold | Discard | Effect sees correct GY size |
| 11 | Swivel waiting + hand card | Play | `TO_DECK_TOP` |
| 12 | Pilot from scry + Echo | Play from scry | No regression on current echo-from-scry behavior |
| 13 | Lifeline + cards in GY | Play Lifeline | GY empty after; Lifeline shuffled into deck (not stranded in GY) |
