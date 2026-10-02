# Shared World / Location State

## Purpose

Ruler Mode and World Mode are clients of one authoritative state.

There is no separate "Ruler Mode world" and no second canonical map copy.

## Physical truth

A Location is persistent physical/world state.

Current minimal Location record stores:

- stable location entity ID
- optional parent location
- location kind
- optional territory
- whether it is currently physically accessible

Discovery is **not** part of Location truth.

A ruin can physically exist while the player, another faction, or an important NPC does not know it exists.

## Knowledge projection

Knowledge uses the existing `knowledge_facts` projection.

For location knowledge the current levels are:

1. Rumored
2. Located
3. Observed
4. Explored

Acquiring weaker information does not downgrade stronger existing knowledge.

This is intentionally small. It is not a generic autobiographical NPC-memory simulator.

## Authoritative physical presence

`world_presence` stores where a persistent entity actually is:

- entity
- containing Location
- local double-precision position
- movement/environment context
- last meaningful world tick update

Position is local to a Location instead of one universal coordinate system, allowing worlds, dimensions, underwater regions, space sectors, underground regions and pocket realms without requiring one enormous coordinate space.

The database is updated at meaningful persistence boundaries/events rather than every rendered frame.

## Cross-mode behavior

World Mode may:

- update the Ruler's authoritative physical presence;
- discover/observe/explore a Location.

Ruler Mode then reads those same records.

Ruler Mode may later issue projects/orders against known physical locations. Their results update the same authoritative state and can become visible when World Mode streams that location.

Neither mode owns a private canonical copy.

## Simulation scope

This layer does not simulate:

- NPC daily movement globally;
- roads emerging from individual travelers;
- ecology agents;
- workers;
- hidden activity merely to justify state.

It stores the state necessary for the player to perceive and affect the world.

## Persistence

Migration 0003 adds:

- `locations`
- `world_presence`
- a knowledge-fact subject lookup index

The cross-mode automation test writes discovery/presence as World Mode, closes the database, reopens it as Ruler Mode, and verifies:

- physical presence survived;
- discovery survived;
- stronger knowledge was not downgraded;
- another observer remains unaware;
- the physical Location exists independently of either observer's knowledge.
