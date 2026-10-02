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
   - points to its Character Identity and currently active Version
   - represented by a stable runtime Entity ID

This allows several Rulers to possess independent manifestations of the same Character Identity without exhausting a global unique pool.

## Definition vs save state

Character Identity and Version definitions belong to validated content packages.

Mutable manifestation state belongs to SQLite.

The save does **not** duplicate whole character definitions. It stores ownership/progression/state and resolves definitions through stable content IDs.

## Local identity exclusivity

The existing design rule remains for combat:

> Multiple Versions/Manifestations of the same Character Identity do not normally fight simultaneously in one local encounter unless an explicit mechanic says otherwise.

This is a combat-team validation rule and is not enforced by the persistence layer.

## Canonical maturity

Sexual-content eligibility is explicit and cannot be inferred from appearance.

Character Identity stores:
- Unknown
- NonAdult
- Adult

A Version may reference sexual content only when the defining package can verify that the parent Identity is canonically Adult.

Blood, injury, clothing/equipment damage and other mature combat-presentation systems are separate from this sexual-content gate.

## Content IDs

Content definitions use stable lowercase IDs such as:

- `core:character.test_001`
- `core:character.test_001.base`
- `region01:character.example.alt_01`

Display names are presentation/localization data and may change without touching saves.

## Future package extension

A later package may add new Versions to an existing Identity by declaring the required dependency and validating against the already-installed Identity registry.

This avoids requiring the original character package to be rebuilt for every future Version.
