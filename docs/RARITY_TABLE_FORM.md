# Card Rarity & Prize Pool Form

**Fill this in, then tell me when it's ready** — I'll update `src/card_meta.cpp` and sync `docs/ROGUELIKE_RUN_HANDOFF.md` §6.

**Status:** Applied — pending `card_meta.cpp` update (2026-08-31)

**Final tally:** Common **16** · Uncommon **33** · Rare **22**  
**Created:** 2026-08-31

---

## How to fill out


| Column         | What to enter                                                                          |
| -------------- | -------------------------------------------------------------------------------------- |
| **Rarity**     | `C` = Common · `U` = Uncommon · `R` = Rare                                             |
| **Max copies** | Max copies a player can own in their **library** (also gates prize offers — see below) |


### Max copies → prize pool behavior


| Value   | Meaning                                              |
| ------- | ---------------------------------------------------- |
| **0**   | Never appears in prize/drop pools                    |
| **1**   | Singleton — offered until you own 1                  |
| **2–5** | Stackable — offered until library count hits the cap |


**Combo pieces** (Rock, Paper, Scissors, Shoot, PB&J, Straw/Sticks/Bricks) are always capped at **1 copy** in code regardless of this table — set them to `1` unless you want a code change.

**Current code** columns show what's live in `card_meta.cpp` today. **Your answer** columns are what you want going forward.

---



## All cards (71)

Edit the **Rarity** and **Max copies** columns on the right.


| #   | Display name          | Enum                    | Code rarity | Code max | **Rarity** | **Max copies**           | Notes                                  |
| --- | --------------------- | ----------------------- | ----------- | -------- | ---------- | ------------------------ | -------------------------------------- |
| 1   | Sips                  | `SIPS`                  | U           | 5        | C          | 5                        |                                        |
| 2   | Longboard             | `LONGBOARD`             | C           | 5        | c          | 5                        |                                        |
| 3   | Heelys                | `HEELYS`                | C           | 5        | c          | 5                        |                                        |
| 4   | Scooter               | `SCOOTER`               | C           | 5        |            |                          |                                        |
| 5   | Skateboard            | `SKATEBOARD`            | C           | 5        |            |                          |                                        |
| 6   | Roller Blades         | `ROLLER_BLADES`         | C           | 5        |            |                          |                                        |
| 7   | Wagon                 | `WAGON`                 | C           | 5        |            |                          |                                        |
| 8   | Stoller               | `STOLLER`               | C           | 5        |            |                          |                                        |
| 9   | Rip Stick             | `RIP_STICK`             | U           | 5        |            |                          | Design doc says **C**                  |
| 10  | Bike                  | `BIKE`                  | U           | 5        |            |                          | Design doc says **C**                  |
| 11  | Clover                | `CLOVER`                | U           | 5        |            |                          | Design doc says **U**                  |
| 12  | Big Kurosawa Burger   | `BIG_KUROSAWA_BURGER`   | U           | 5        |            |                          | Design doc says **U**                  |
| 13  | Rock                  | `ROCK`                  | R           | 1        |            |                          | Design doc says **C** · combo          |
| 14  | Paper                 | `PAPER`                 | R           | 1        |            |                          | Design doc says **C** · combo          |
| 15  | Scissors              | `SCISSORS`              | R           | 1        |            |                          | Design doc says **C** · combo          |
| 16  | Shoot                 | `SHOOT`                 | R           | 1        |            |                          | combo                                  |
| 17  | Peanut Butter         | `PEANUT_BUTTER`         | C           | 3        |            |                          | combo                                  |
| 18  | Jelly                 | `JELLY`                 | C           | 3        |            |                          | combo                                  |
| 19  | Straw                 | `STRAW`                 | U           | 3        |            |                          | Design doc says **C** · combo          |
| 20  | Sticks                | `STICKS`                | U           | 3        |            |                          | Design doc says **C** · combo          |
| 21  | Bricks                | `BRICKS`                | U           | 3        |            |                          | Design doc says **C** · combo          |
| 22  | Lifeline              | `LIFELINE`              | R           | 1        |            |                          | Design doc says **R**                  |
| 23  | Snail Mail            | `SNAIL_MAIL`            | U           | 5        |            |                          |                                        |
| 24  | Wishes                | `WISHES`                | U           | 2        |            |                          | Design doc says **R**                  |
| 25  | Busted                | `BUSTED`                | C           | 5        |            |                          |                                        |
| 26  | Swivel                | `SWIVEL`                | U           | 5        |            |                          |                                        |
| 27  | Roundup               | `ROUNDUP`               | R           | 1        |            |                          |                                        |
| 28  | Hacker                | `HACKER`                | R           | 1        |            |                          |                                        |
| 29  | Librarian             | `LIBRARIAN`             | U           | 3        |            |                          |                                        |
| 30  | Pilot                 | `PILOT`                 | U           | 3        |            |                          |                                        |
| 31  | Turtle Mode           | `TURTLE_MODE`           | R           | 1        |            |                          |                                        |
| 32  | Time is Too Expensive | `TIME_IS_TOO_EXPENSIVE` | U           | 2        |            |                          |                                        |
| 33  | Birds of a Feather    | `BIRDS_OF_A_FEATHER`    | U           | 5        |            |                          |                                        |
| 34  | Necromancy            | `NECROMANCY`            | R           | 1        |            |                          |                                        |
| 35  | Miracle               | `MIRACLE`               | U           | 5        |            |                          |                                        |
| 36  | Rags to Riches        | `RAGS_TO_RICHES`        | R           | 1        |            |                          |                                        |
| 37  | Jacks                 | `JACKS`                 | u           | 5        |            |                          |                                        |
| 38  | Fishing Pole          | `FISHING_POLE`          | u           | 5        |            |                          |                                        |
| 39  | Shells                | `SHELLS`                | u           | 5        |            |                          | Was "Cups" in old design doc           |
| 40  | Swap                  | `SWAP`                  | R           | 1        |            |                          |                                        |
| 41  | Catnip                | `CATNIP`                | C           | 5        |            |                          |                                        |
| 42  | Journal               | `JOURNAL`               | U           | 5        |            |                          |                                        |
| 43  | Triptych              | `TRIPTYCH`              | U           | 5        |            |                          |                                        |
| 44  | Dilla                 | `DILLA`                 | U           | 3        |            |                          | Was "Seeds" in old design doc          |
| 45  | Semaphore             | `SEMAPHORE`             | R           | 1        |            |                          |                                        |
| 46  | Bones                 | `BONES`                 | C           | 5        |            |                          |                                        |
| 47  | Threshold             | `THRESHOLD`             | U           | 2        |            |                          |                                        |
| 48  | Tombstones            | `TOMBSTONES`            | U           | 5        |            |                          |                                        |
| 49  | Roll Over             | `ROLL_OVER`             | r           | 2        |            |                          |                                        |
| 50  | Toppings              | `TOPPINGS`              | C           | 5        |            |                          | Not in old design doc §6               |
| 51  | Cycle                 | `CYCLE`                 | C           | 5        |            |                          | Not in old design doc §6               |
| 52  | Cycle Seven           | `CYCLE_SEVEN`           | U           | 3        |            |                          | Not in old design doc §6               |
| 53  | Get Me Outa Here      | `GET_ME_OUTA_HERE`      | U           | 3        |            |                          | Not in old design doc §6               |
| 54  | Comeback              | `COMEBACK`              | U           | 3        |            |                          | Not in old design doc §6               |
| 55  | Encore                | `ENCORE`                | U           | 3        |            |                          | Not in old design doc §6               |
| 56  | The Fourth            | `THE_FOURTH`            | r           | 1        |            |                          | Not in old design doc §6               |
| 57  | Palindrome            | `PALINDROME`            | r           | 1        |            |                          | Prize gate (>11011) planned separately |
| 58  | The Fifth             | `THE_FIFTH`             | r           | 1        |            |                          | Not in old design doc §6               |
| 59  | one more thing        | `SOLO`                  | U           | 3        |            |                          | Was "One More Time" in old design doc  |
| 60  | waterfall             | `SPECULATIVE`           | R           | 1        |            |                          | Not in old design doc §6               |
| 61  | Flex                  | `FLEX`                  | R           | 1        |            |                          | Not in old design doc §6               |
| 62  | Dead Rising           | `DEAD_RISING`           | U           | 5        |            |                          | Not in old design doc §6               |
| 63  | Bounty                | `BOUNTY`                | U           | 5        |            |                          | Not in old design doc §6               |
| 64  | Overclock             | `OVERCLOCK`             | R           | 1        |            |                          | Not in old design doc §6               |
| 65  | Evaluate              | `EVALUATE`              | R           | 1        |            |                          | Not in old design doc §6               |
| 66  | Build a Number        | `BUILD_A_NUMBER`        | r           | 1        |            |                          | Not in old design doc §6               |
| 67  | Minor Fall            | `MINOR_FALL`            | U           | 1        |            |                          | Not in old design doc §6               |
| 68  | Major Lift            | `MAJOR_LIFT`            | U           | 1        |            |                          | Not in old design doc §6               |
| 69  | Finale                | `FINALE`                | R           | 1        |            |                          | Not in old design doc §6               |
| 70  | Time is Money         | `TIME_IS_MONEY`         | c           | 5        |            | Not in old design doc §6 |                                        |
| 71  | 7 Feet Deep           | `SEVEN_FEET_DEEP`       | U           | 3        |            |                          | Not in old design doc §6               |


---



## Quick-fill template (copy/paste friendly)

If you prefer editing a compact list, copy this block and fill in `C`/`U`/`R` and the number after the comma:

```
SIPS, U, 5
LONGBOARD, C, 5
HEELYS, C, 5
SCOOTER, C, 5
SKATEBOARD, C, 5
ROLLER_BLADES, C, 5
WAGON, C, 5
STOLLER, C, 5
RIP_STICK, , 
BIKE, , 
CLOVER, , 
BIG_KUROSAWA_BURGER, , 
ROCK, , 
PAPER, , 
SCISSORS, , 
SHOOT, , 
PEANUT_BUTTER, C, 3
JELLY, C, 3
STRAW, , 
STICKS, , 
BRICKS, , 
LIFELINE, , 
SNAIL_MAIL, U, 5
WISHES, , 
BUSTED, C, 3
SWIVEL, U, 1
ROUNDUP, R, 1
HACKER, R, 1
LIBRARIAN, R, 1
PILOT, U, 1
TURTLE_MODE, R, 1
TIME_IS_TOO_EXPENSIVE, U, 2
BIRDS_OF_A_FEATHER, U, 5
NECROMANCY, R, 1
MIRACLE, U, 5
RAGS_TO_RICHES, R, 1
JACKS, R, 1
FISHING_POLE, R, 1
SHELLS, R, 1
SWAP, R, 1
CATNIP, C, 3
JOURNAL, U, 5
TRIPTYCH, U, 5
DILLA, U, 2
SEMAPHORE, R, 1
BONES, C, 3
THRESHOLD, U, 2
TOMBSTONES, U, 2
ROLL_OVER, U, 2
TOPPINGS, , 
CYCLE, , 
CYCLE_SEVEN, , 
GET_ME_OUTA_HERE, , 
COMEBACK, , 
ENCORE, , 
THE_FOURTH, , 
PALINDROME, , 
THE_FIFTH, , 
SOLO, , 
SPECULATIVE, , 
FLEX, , 
DEAD_RISING, , 
BOUNTY, , 
OVERCLOCK, , 
EVALUATE, , 
BUILD_A_NUMBER, , 
MINOR_FALL, , 
MAJOR_LIFT, , 
FINALE, , 
TIME_IS_MONEY, , 
SEVEN_FEET_DEEP, , 
```

---



## Tally (fill in after editing)


| Rarity               | Count |
| -------------------- | ----- |
| Common               | 16    |
| Uncommon             | 33    |
| Rare                 | 22    |
| **Excluded (max 0)** | 0     |


**Code today:** Common 17 · Uncommon 31 · Rare 23  
**Design doc §6 (stale):** Common 18 · Uncommon 22 · Rare 11 — missing 19 newer cards

---



## Optional: special prize rules

These are **separate** from rarity/max copies but affect whether a card can appear as a prize. Check any you want:

- [ ] **Palindrome** — only offerable when `biggest_number_record > 11011` (planned in FEATURE_PLAN)
- [ ] Other score-gated cards: _______________________________

---



## When done

Reply in chat: **"rarity form done"** (or paste the quick-fill block). I'll apply it to code and update the design doc.