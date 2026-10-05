# Authoritative Data Model - Initial Contract

This is a schema contract, not a final list of every table.

## Stable identity

Every persistent entity receives a stable UUID-style ID.

Actor pointers, object memory addresses, array indices, display names, roster order, and package-local indices are never persistent identity.

## Entity registry

The base `entities` table answers:
- what persistent object is this?
- what broad kind is it?
- when did it enter/leave active history?
- what revision is current?

Specialized migrations add indexed domain tables later.

## Event ledger

`world_events` stores **meaningful causality**, not a complete activity log.

Good event examples:
- character recruited
- canonical death
- Domain Core captured/destroyed
- settlement destroyed
- territory changed owner
- war began/ended
- unique discovery
- major progression transformation
- important equipment destroyed

Bad event examples:
- NPC walked three metres
- deer ate grass
- worker started shift
- ordinary ambient animation played
- every blood decal created

## Knowledge facts

Knowledge is a separate projection from truth.

The world can know Fact A while the player does not.

An important NPC may have a specific fact when story/gameplay needs it.

There is no generic unlimited autobiographical memory engine.

## Schema status

The authoritative persistent schema is **version 14**.

Implemented normalized families include:
- base entity registry, world-event ledger, knowledge facts and package state;
- independent Character Manifestation instances and acquisition provenance;
- physical locations/presence, Territory membership/claims, reclamation,
  sovereignty and permanent gacha access;
- Domain-heart/Core state, Concepts, fusion and lineage;
- Rank, Factors, Classes/Crown/Grand seats, skills/provenance, routes/forms,
  reinforcement, Transcendence, World Fantasm, personal World Manifestation and
  Grand Convergence;
- reality hierarchy, local Time Domains/calendars, Junctions, World Director
  scheduling/unlocks and offline simulation state;
- objective/constraint Dispatch, continuous War/front/order history, Army
  capability vectors, Project phases/assignments, civilization dimensions and
  logistics routes;
- item/inventory/equipment state, affinity/proficiency, wardrobe/presentation,
  belief provenance, language/memory, NPC promotion, mutable adult context and
  Heroic Records;
- package dependencies/lifecycle, Reports/delivery, persistent Manifestation
  management metadata and last-used context selection.

Legacy columns retained for migration provenance do not drive gameplay. The
numbered reconciliation contract is recorded in
`RECONCILIATION_MIGRATION_MATRIX.md`.

## Canonical maturity / lore authority

Character maturity is content lore, not a runtime visual judgment.

- `Character Identity` stores the canonical lore maturity fact.
- Appearance/body design is never used to infer or override that fact.
- A lore-Adult Identity is adult-content capable by default; no Version-level generic permission switch may silently disable the pillar.
- Version/form/profile data may alter presentation, available assets and physically possible actions without redefining adulthood.
- Lore-NonAdult/minor/child Identities remain outside sexual-content packages.

The runtime validator enforces consistency with authored lore; it does not create an independent maturity policy.

## Numbers

Do not assume every combat number fits a normal float. Combat's large-number representation will be defined in the combat layer.

Ordinary population/resource counters should remain simple integers unless a real gameplay requirement proves otherwise.
