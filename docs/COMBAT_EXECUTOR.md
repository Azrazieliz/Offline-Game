# Combat Executor - Phase E Foundation

## Shared state

Turn Mode and World Mode Action Combat consume the same resolved character kit/state representation.

They are different executors, not different character systems.

## Turn team structure

Current authoritative rule:
- full turn roster: up to 18
- active at once: up to 6
- reserve characters remain part of the battle state
- there is no free arbitrary reserve switch; entry/switching is caused by defeat or explicit mechanics

## Timeline

The turn executor uses a continuous action-value timeline.

Lower `NextActionValue` acts first.

The executor does **not** hard-code the final SPD -> action-delay formula yet. Resolved actions carry their authoritative delay so the timing model can be changed without rewriting battle state/order execution.

Ultimates, counters and similar mechanics can use explicit interrupt priority.

## Resolution split

The executor is responsible for:
- legal actor/order checks
- timeline advancement
- active/reserve/defeated presence
- HP state changes supplied by resolved mechanics
- deterministic combat log
- battle completion

The math/rule layer is responsible for:
- hit/miss
- crit
- damage formula
- defense interaction
- immunity/negation/override
- effect application
- resource/cost validation
- mechanic-specific resolution

## Healing

The executor deliberately does not cap healing to Max HP.

Overheal can legally create shields/extra HP/other mechanics in this design; exact handling belongs to the mechanic resolver.

## Action combat

Initial action party architecture is:
- Ruler/protagonist
- up to two switch/QTE companions

The adapter enforces local Character Identity exclusivity.

World Mode movement, hit detection and animation remain Unreal presentation/runtime concerns; persistent character state and kit do not fork into a second character model.

## Crit overflow - settled direction

Crit Rate overflow conversion is a settled design rule:

**1% Crit Rate overflow -> +2% Crit Damage.**

Basis-point equivalent:

**100 Crit Rate bps overflow -> +200 Crit Damage bps.**

Still intentionally unresolved here:
- whether Crit Resistance reduces rate before or after overflow is computed
- exact Crit Resistance equation
- base/default Crit Damage values
