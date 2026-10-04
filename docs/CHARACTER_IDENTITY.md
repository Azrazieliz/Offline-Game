# Character Identity Architecture

## Three-layer ontology

The runtime deliberately separates:

1. **Character Identity**
   - immutable content-level identity
   - groups all Versions that are canonically the same character identity
   - carries canonical maturity classification and acquisition-origin metadata

2. **Character Version**
   - immutable content definition for Base / Awakened / Corrupted / historical / event / other forms
   - belongs to one Character Identity
   - may change model, kit, role, presentation, mature-content references, transformations, and other authored content

3. **Ruler-specific Manifestation**
   - mutable persistent entity owned by one Ruler
   - independently leveled/equipped/developed
   - points to its Character Identity and currently active Version/state
   - represented by a stable runtime Entity ID
   - multiple Manifestations of the same Character Identity may be owned by the same Ruler so divergent development routes can coexist

This allows several Rulers to possess independent manifestations of the same Character Identity and also allows one Ruler to develop multiple copies of that Identity along different Evolution/Awakening/Corruption/other routes.

## Definition vs save state

Character Identity and Version definitions belong to validated content packages.

Mutable manifestation state belongs to SQLite.

The save does **not** duplicate whole character definitions. It stores ownership/progression/state and resolves definitions through stable content IDs.

## Multiple owned Manifestations and local identity exclusivity

A Ruler may own multiple Manifestations of the same Character Identity. Each has independent Rank/Level, Current Rarity, equipment, learned skills, development-route state, forms, history and reinforcement state.

The combat rule remains:

> Multiple Manifestations of the same Character Identity do not normally fight simultaneously in one local encounter unless an explicit mechanic says otherwise.

This is a local encounter/team-validation rule, not a persistence ownership restriction. Copies may be trained, equipped, assigned and developed independently outside that restriction.

Fully reinforced divergent Manifestations may later participate in the Character Identity's Grand Convergence finalization, producing one Grand Manifestation that preserves and synthesizes their completed development histories according to character-specific rules.

## Canonical maturity / sexual-content eligibility

Sexual-content eligibility is explicit and cannot be inferred from appearance, local marriage law, social status or gameplay power.

Character Identity stores a validated maturity classification:
- Unknown
- NonAdult
- Adult

A Version may reference sexual content only when the defining package verifies that the parent Identity is canonically an adult/mature character. Characters canonically treated as minors/children remain excluded from sexual-content packages.

Setting-specific concepts of adulthood may separately affect politics, inheritance, military service, social roles or other fictional institutions; those do not silently override the sexual-content eligibility classification.

Scene participation uses the character-specific sexual-profile / scene-validity system defined in `MATURE_CONTENT.md`, not a universal affection or consent meter.

Blood, injury, clothing/equipment damage and other nonsexual mature-presentation systems are separate from this sexual-content gate.

## Content IDs

Content definitions use stable lowercase IDs such as:

- `core:character.test_001`
- `core:character.test_001.base`
- `region01:character.example.alt_01`

Display names are presentation/localization data and may change without touching saves.

## Future package extension

A later package may add new Versions to an existing Identity by declaring the required dependency and validating against the already-installed Identity registry.

This avoids requiring the original character package to be rebuilt for every future Version.
