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

## Character stat ownership

Every character-facing combat stat belongs to the resolved entity/build state rather than a universal base template. This includes Crit Damage and Crit Rate as well as Hit, Dodge, Block, SPD, HP, ATK, DEF, resistances and future character-facing stats.

Shared combat math defines **how** values interact; character/content data defines **what those values are**.

## Settled crit overflow rule

Crit Rate above its effective probability ceiling converts at:

**1% Crit Rate overflow -> +2% Crit Damage.**

Crit Rate Resistance is applied before overflow conversion, as specified in `COMBAT_MATH.md`.

## Current tuning / content boundary

The original Phase-D deferrals have since been resolved or reclassified by later authoritative documents.

Resolved architecture:
- generic damage / DEF / negative-DEF behavior: `COMBAT_MATH.md`;
- Crit and Hit/Dodge overflow ordering: `COMBAT_MATH.md`;
- no universal cooldown/team-skill-point model: `SKILL_READINESS.md`;
- character-owned Ultimate resource/threshold/tiers: `SKILL_READINESS.md`;
- character-facing stats are explicit per Character/Manifestation/build: this document + `COMBAT_MATH.md`;
- Rarity/Rank/progression architecture: dedicated progression documents.

Still intentionally simulator/content-tunable:
- exact SPD -> action-value coefficients;
- individual skill/resource values;
- per-character stat sets and growth response;
- content-specific cooldown/readiness mechanics;
- final balance coefficients.

These are not missing Creative Director architecture and must live in the versioned tuning/content contracts rather than being guessed into C++.
