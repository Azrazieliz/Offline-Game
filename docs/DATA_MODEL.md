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

Implemented authoritative schema families through migration 0006:

- base entity registry, meaningful world events, knowledge facts, and content-package state;
- owned Character Manifestations (Identity/Version definitions remain package data);
- physical locations and authoritative world presence;
- territories, Domain Cores, aggregate resources, and lazy Projects;
- result-oriented dispatches;
- factions, explicit faction links, aggregate armies, and wars;
- persistent gacha pity/featured-guarantee state plus a **legacy pre-reconciliation duplicate-acquisition column** that is no longer design-authoritative.

Gacha pull outcomes are stored as meaningful `world_events`; there is no
separate pull-history subsystem.

The frozen architecture now requires additional normalized schema families before the real validation gate. The exact numbered migration plan is specified in `RECONCILIATION_MIGRATION_MATRIX.md`.

The current migration-0006 database is therefore a **pre-reconciliation runtime schema**, not the final player-facing schema.

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
