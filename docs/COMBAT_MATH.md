# Baseline Combat Math

This document records rules already established in the master architecture and their current implementation semantics.

## Percentage representation

10,000 basis points = 100%.

This avoids float probability drift in authoritative combat logic.

## Crit

Order is authoritative:

1. attacker Crit Rate
2. subtract target Crit Rate Resistance
3. clamp actual crit probability to 100%
4. convert all remaining Crit Rate overflow into Crit Damage at **1:2**
5. subtract target Crit Damage Resistance from Crit Damage bonus
6. resolve seeded crit roll

Example:

- 130% Crit Rate
- target has 20% Crit Rate Resistance
- effective Crit Rate = 110%
- crit is guaranteed
- 10% overflow becomes +20% Crit Damage

The ordinary baseline does not make a critical hit deal less than a normal hit if Crit Damage Resistance exceeds the available Crit Damage bonus; the bonus bottoms at zero. A bespoke rule can override that if ever desired.

Current provisional conventional base Crit Damage bonus is +50% (x1.5 total critical damage before other categories). It is a replaceable default, not architecture.

## Hit / Dodge overflow

Normal attacks use 100% baseline Hit.

Effective Hit = Hit - Dodge.

Every complete 100% effective Hit band creates one guaranteed eligible hit instance.

Any remainder is a seeded probability for one additional hit instance.

Example:

- 250% Hit
- 50% Dodge
- effective Hit = 200%
- two hit instances are guaranteed

A skill can explicitly declare itself indivisible and prohibit overflow replication.

Whether a multi-hit skill rolls crit/status once for the sequence or separately per hit remains skill-defined, as established in the baseline.

## Block

Block Rate and Block Reduction are separate values.

A successful ordinary block applies its authored reduction after ordinary physical damage modifiers.

Parry is not a generic turn-combat stat.

## Damage

Ordinary physical skills resolve an authored base magnitude, normally:

ATK x skill multiplier

but skills may instead scale from HP, DEF, missing HP, target stats, or another explicit source.

DEF uses progressive ratio mitigation rather than subtraction.

The current generic implementation uses:

defense multiplier = defense reference / (defense reference + effective DEF)

The caller explicitly supplies the defense-reference stat, so the architecture does not assume every skill is ATK-scaling.

DEF penetration is percentage-based. Penetration beyond 100% becomes negative-defense pressure and therefore bonus damage where the applicable rule permits it.

Separate authored multiplier categories are applied multiplicatively.

## True Damage

True Damage ignores:

- DEF
- ordinary resistance
- ordinary Damage Taken Reduction
- ordinary Block

It remains affected by:

- explicit True Damage Resistance
- higher-order rules/Authority that explicitly protect against True Damage
- offensive source multipliers where applicable

## Max-HP damage

Direct fixed-% Max-HP damage is not a normal generic attack mechanic.

It belongs to explicit afflictions/debuffs/special rules where authored.

Defensive HP-loss caps remain fully supported by the rule/Authority layer.

## Determinism

There is no arbitrary random damage variance.

Only declared RNG-bearing mechanics such as Crit, Dodge/Hit remainder, Block, proc/debuff application, and skill-specific randomness consume authoritative seeded RNG.
