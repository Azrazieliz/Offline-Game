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

## Future migrations

Planned schema families, added only when their code is ready:

- characters / identities / versions / owned manifestations
- skills / progression / equipment
- locations / territories / Domain Cores
- resources / projects
- dispatches
- factions / explicit faction links / armies / wars
- gacha state / pull history
- discoveries / Chronicle projections
- content package activation state

## Canonical-adult gate

Character data must explicitly distinguish canonical age eligibility from visual appearance.

Mature sexual-content eligibility is never inferred from model/body design alone.

The specific content systems are added later; the persistence contract reserves explicit eligibility metadata rather than guessing at runtime.

## Numbers

Do not assume every combat number fits a normal float. Combat's large-number representation will be defined in the combat layer.

Ordinary population/resource counters should remain simple integers unless a real gameplay requirement proves otherwise.
