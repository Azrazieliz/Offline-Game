# Dispatch, Factions and War — Minimal Strategic State

## Scope

This layer deliberately implements only the persistent strategic facts needed by the RPG.

It does **not** add:

- relationship / trust / reputation scores
- faction feelings
- treaty-law documents
- Casus Belli requirements
- internal politics
- occupation governance
- spy-agent life simulation
- NPC task/day simulation
- individual ordinary soldiers
- generic "negotiate peace now" gameplay

## Dispatch

Exploration, Gathering, Reconnaissance, Espionage, Support and other delegated assignments share one Dispatch record.

A Dispatch stores:

- owner
- optional target
- assignment type
- participants
- start/resolve ticks
- risk
- deterministic resolution seed
- final result

The engine does not simulate irrelevant intermediate actions.

A mission remains Active until its due tick and is then resolved once by the appropriate capability/mission resolver. The stored result is the authoritative outcome.

## Factions

A Faction stores only compact strategic identity:

- stable entity ID
- optional leader/Ruler
- kind
- aggregate population

There is no hidden opinion or reputation meter.

## Explicit faction links

The baseline state supports:

- Alliance
- directional Support
- Subordination

Links are explicit booleans/facts with update time.

Dialogue and authored events can present them richly without requiring a relationship simulation.

## Armies

Ordinary military force is aggregated.

An Army stores:

- faction
- physical Location
- headcount
- aggregate effective power
- named commanders
- compact state

Named/important commanders remain persistent individual entities. Ordinary soldiers do not become individual simulation agents.

## War

War requires no artificial Casus Belli.

A War stores:

- participants and sides
- objective type
- optional physical/entity objective target
- start/end ticks
- status
- concise resolution state

Declaring war breaks an **existing active alliance** transactionally, but does not invent a fake historical alliance if none existed.

War resolution is an API used after an actual strategic/world outcome such as victory, defeat, objective completion, withdrawal, collapse or another explicit event.

It is **not** a generic peace-negotiation button.

## Persistence

Migration 0005 adds normalized tables for:

- dispatches + participants
- factions
- explicit faction links
- armies + named commanders
- wars + participants

The automation proof covers:

- a Dispatch cannot resolve before its due tick
- its participants/seed/result survive restart
- aggregate Army headcount + named commander persistence
- alliance creation
- directional support
- declaring war breaks a real alliance
- declaring war between never-allied factions does not fabricate alliance history
- war objective/status/participants persist
- war resolution occurs only through an explicit final-state operation
