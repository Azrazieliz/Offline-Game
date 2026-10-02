# Combat Executor - Phase E Foundation

## Shared state

Turn Mode and World Mode Action Combat consume the same resolved character kit/state representation.

They are different executors, not different character systems.

## Turn team structure

Current authoritative rule:
- full turn roster: up to 18
- active at once: up to 6
- six preferred succession lanes: A1->B1->C1 through A6->B6->C6
- reserve characters remain part of the same battle state
- no free arbitrary reserve switch; entry/switching comes from defeat or explicit mechanics

### Preferred lane vs occupied battlefield lane

A character's **origin succession lane** is persistent formation structure.

The **occupied battlefield lane** is only the active slot currently being filled.

Direct succession is preferred, but a battlefield lane is not reserved forever for its original chain. After the revival/defeat window:

1. if the defeated character revived, it keeps its active slot;
2. otherwise the next surviving reserve from that character's origin chain is preferred;
3. if that chain is exhausted, another surviving reserve from the same team may fill the empty active slot.

The fallback policy prefers the least disruptive reserve:
- a reserve from an origin lane that already has a living active unit;
- deeper reserve depth before shallower depth, preserving nearer successors where possible;
- stable origin-lane / entity-ID tie breaks.

Example: if A2, B2 and C2 are all gone while A1 remains active and C1 is still a reserve, C1 may occupy battlefield lane 2 instead of leaving it empty. Its origin remains lane 1.

This same compaction principle applies when a sequential battle begins with previously defeated characters.

## Defeat / revival / succession order

0 HP normally creates a defeated/incapacitated state, not canonical death.

When an active unit reaches 0 HP:

1. the current action finishes;
2. OnDefeat and revival/respawn mechanics resolve;
3. if the unit is active/alive again, no successor is consumed;
4. otherwise direct succession is attempted;
5. remaining empty lanes are rebalanced from other surviving reserves;
6. promoted units enter the timeline normally unless an entry rule changes that.

This prevents temporary duplicate occupancy from revival + successor promotion.

## Timeline

The turn executor uses a continuous action-value timeline.

Lower `NextActionValue` acts first.

The executor does **not** hard-code the final SPD -> action-delay formula yet. Resolved actions carry their authoritative delay so the timing model can change without rewriting battle state/order execution.

Ultimates, counters and similar mechanics can use explicit interrupt priority.

## Trigger execution

Battle/defeat/entry passive triggers use stable content IDs and a deterministic queue.

Blueprint timing is not authoritative.

Triggered actions are resolved by the same normal effect/action machinery. The battle tracks outstanding trigger instances so ordinary turn flow and succession cannot continue before the defeat/revival window has genuinely finished.

## Resolution split

The executor owns:
- legal actor/order checks
- timeline advancement
- active/reserve/defeated presence
- origin succession vs current occupied slots
- deterministic trigger/action ordering
- HP state changes supplied by resolved mechanics
- deterministic combat log
- battle completion

The math/rule layer owns:
- hit/dodge and overflow
- crit and crit resistance
- damage/DEF
- block
- immunity/negation/override
- effect application
- resource/cost validation
- mechanic-specific resolution

## Healing

The executor deliberately does not cap healing to Max HP.

Overheal can create shields/extra HP/other mechanics when authored; exact behavior belongs to the mechanic resolver.

## Action combat

Initial action party architecture:
- Ruler/protagonist
- up to two switch/QTE companions

The adapter enforces local Character Identity exclusivity.

World Mode movement, hit detection and animation remain Unreal presentation/runtime concerns; persistent character state and kit do not fork into a second character model.

## Crit overflow - settled

Crit order is settled:

1. subtract target Crit Rate Resistance;
2. actual Crit chance is capped at 100%;
3. each remaining +1% Crit Rate overflow becomes +2% Crit Damage.

Example: 130% Crit Rate against 20% Crit Rate Resistance -> 110% effective -> guaranteed Crit +20% Crit Damage from the remaining 10% overflow.
