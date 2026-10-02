# Territory, Domain Core, Resources and Projects

## Scope

This layer implements only the strategic state that produces player-facing value.

It does **not** implement:

- workers or worker schedules
- household/population agents
- taxes
- economic policy sliders
- stockpile objects
- personal businesses
- construction crews moving offscreen
- hidden per-second settlement simulation

## Territory

A Territory is persistent controlled physical space anchored to a root Location.

Minimal state:

- stable Territory entity ID
- current Ruler/controller, or neutral
- root physical Location
- Main Territory flag
- aggregate population number
- compact control state

Population is only a number. Birth/death/household detail is not simulated.

Creating/updating a Territory links its root Location back to that Territory in the shared physical-world state.

## Domain Core

A Territory can exist without a Domain Core.

A Core is a separate persistent entity that may awaken later.

Current Core state:

- owning Territory
- current controller
- lifecycle: Dormant / Awakened / Broken / Absorbed
- current/max durability using the large-number representation
- explicit Concept/Aspect IDs and grades

### Capture vs destruction

This is a hard invariant:

**A Domain Core at zero durability is Broken, not captured.**

Capturing requires the Core to remain intact while control is established.

When a Core breaks:

- durability becomes zero
- lifecycle becomes Broken
- active controller is removed
- a meaningful world event is recorded

A Broken/Absorbed Core cannot be captured through the normal capture operation.

## Resources

Resources are aggregate integer balances attached to an owning entity.

Examples can include territory materials, currencies that exist in lore, Domain resources, crafting materials and other strategic totals.

There is no separate stockpile-object simulation. A high number is a high amount of the resource.

Balances cannot become negative through the baseline resource API.

## Projects

Construction, reconstruction, repair, training and similar timed strategic work share one Project mechanism.

A Project stores:

- owner
- physical Location
- project type
- status
- start tick
- resolve tick
- progress
- sparse project-specific payload

Starting a project can consume aggregate resource costs transactionally.

Progress is lazy/event-driven:

- no worker ticks run in the background
- when queried/resolved, progress is derived from world time
- reaching the resolve tick completes the Project

Named characters/capabilities can later modify requirements or duration without creating ordinary-worker simulation.

## Persistence

Migration 0004 adds normalized tables for:

- territories
- Domain Cores
- Core Aspects
- resource balances
- projects

The automated proof covers:

- physical Location <-> Territory ownership reflection
- intact Core capture
- zero-durability Core breakage
- refusal to capture a Broken Core
- transactional project resource spending
- lazy project progress
- persistence/restart of Project state
