# Shared Rule Core - Initial Architecture

This phase establishes infrastructure without choosing final combat tuning.

## Large numbers

Combat values can exceed ordinary integer/float ranges.

The initial representation stores:
- a signed 9-digit significand
- a base-10 exponent

This gives effectively unbounded gameplay magnitude while keeping deterministic integer arithmetic for basic operations.

It is an engineering representation, not the final player-facing suffix/formatting system.

## Deterministic RNG

Gameplay randomness uses explicit seeded streams.

A replay-relevant operation must be reproducible from:
- seed
- draw order/count
- authoritative state

UI/presentation randomness must not consume authoritative gameplay RNG.

## Rule priority

Conflicting rule-like effects resolve through declared metadata:

1. Authority
2. Specificity
3. source rank
4. stable source-ID tie-break

This is infrastructure for later mechanics such as immunity/bypass, damage caps/cap bypass, revival/execution conflicts, Domain overrides and other exceptional rules.

The exact names/numeric bands of Authority tiers remain data-defined.

## Skills

Character progression resolves into the character's current integrated skill set.

The runtime does not treat every learned historical skill as a permanently separate combat button.

Active skills, passives, ultimate and form references are stable content IDs.

Evolution/Awakening/Corruption/Transcendence can all modify the same resolved-kit pipeline while remaining different player-facing concepts.

## Settled crit overflow rule

Crit Rate above its effective probability ceiling converts at:

**1% Crit Rate overflow -> +2% Crit Damage.**

The interaction order with Crit Resistance is still intentionally unresolved.

## Explicitly deferred

This phase does not decide:
- damage formula
- exact Crit/Hit/Dodge equations
- generic cooldown usage
- ultimate charge formula
- final stat list
- exact action-value formula
- final rarity progression numbers
