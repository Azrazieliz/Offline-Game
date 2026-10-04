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

## Canonical maturity / lore authority

Adulthood/maturity is a **canonical lore fact** attached to Character Identity. Appearance is never used to infer or override it.

Character Identity stores:
- Unknown
- NonAdult
- Adult

This field must reflect the lore and nothing else.

If the lore defines the Identity as Adult, adult-content support is allowed by default; no second gameplay/visual eligibility classifier is added on top.

If the lore defines the Identity as NonAdult/minor/child, sexual-content packages are not valid for that Identity.

Setting-specific adulthood can also affect political/cultural systems, but the stored canonical maturity value remains the authoritative data fact used by content validation.

Preferences/libido/personality/state shape adult-content expression rather than redefining adulthood.

Blood, injury, clothing/equipment damage and other nonsexual mature-presentation systems are separate from this classification.

## Content IDs

Content definitions use stable lowercase IDs such as:

- `core:character.test_001`
- `core:character.test_001.base`
- `region01:character.example.alt_01`

Display names are presentation/localization data and may change without touching saves.

## Future package extension

A later package may add new Versions to an existing Identity by declaring the required dependency and validating against the already-installed Identity registry.

This avoids requiring the original character package to be rebuilt for every future Version.
