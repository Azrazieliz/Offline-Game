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

## Sovereignty, loss and Domain-heart rules

### Effective territorial control

A Territory exists because a Ruler actually exercises sovereign control over physical space. Population, buildings, formal borders and a Domain Core are not prerequisites.

Control can arise through any world-valid mechanism: occupation, conquest, inheritance, agreement, settlement, construction, overwhelming personal presence or another explicit causal route. There is no universal abstract "claim button" that overrides reality.

Claims/control may overlap. The world can therefore contain contested zones where more than one Ruler has a real claim or partial/effective control; local state and Authority determine who can actually exercise which powers there.

### Five-day reclamation window

Loss of practical control does not immediately erase the previous Territory identity. The displaced Ruler has a **five in-game day reclamation window**.

- If effective control is re-established within five days, territorial continuity is preserved.
- If it is not reclaimed by the end of that window, the prior Territory ownership is considered lost and the location transitions to the resulting neutral/new-controller state.
- Systems that require continuous sovereignty may suspend during the reclamation window and only treat it as a true continuity break if the five-day deadline expires.

### Domain Core origin and role

The existing rule remains: ordinary Territory comes first. Once the Territory reaches the required Rank/power/development conditions, it can **crystallize/awaken its own Domain Core**. The Core is a progression milestone of that Territory, not a generic portable object that creates sovereignty by itself.

Once awakened, the Core becomes the Territory's metaphysical **heart/anchor**. Core evolution remains independent enough that Ruler/Core mismatch is possible, and it may accumulate Concepts/Aspects, Domain Reserve, historical traits and even sentience as already defined.

### Core loss versus territorial destruction

The state of the Core and the state of the physical Territory are intentionally different failure axes.

- A Territory whose infrastructure/settlements are devastated but whose Core remains protected is still a functioning Domain in metaphysical terms. It can be rebuilt through reconstruction/repair Projects.
- Losing/destroying the awakened Core is catastrophic because the Domain has lost its heart. The Territory rapidly degrades/ruins at the Domain-system level rather than simply becoming a normal damaged settlement.
- Reconstituting a lost Domain heart is not an ordinary repair action. It requires an extraordinarily expensive/high-order recovery path and may be impossible for some Domains.
- A Core at zero durability is still Broken, not captured. Normal capture requires preserving positive Core durability and separately establishing control.

### Populationless Domains

A Domain can exist with no population. However, population/infrastructure normally contribute enormous stabilizing, developmental and resource value. A Ruler who matures and sustains a serious Domain almost entirely through personal power must therefore be an extreme outlier relative to their own Rank/Level.

This is deliberately scary rather than normalized: a solitary Domain-bearing Ruler, especially an Overlord-scale one, should signal exceptional personal danger rather than 'empty territory is easier to run.'

### Overlord and multi-Domain sovereignty

Direct or hierarchical sovereignty over multiple independent Domain hearts is **Overlord-scale structure**, not ordinary single-Domain Ruler progression.

An Overlord may hold several Domains directly, govern them through subordinate Rulers, or combine both arrangements. This expands the earlier Overlord rule rather than creating a separate multi-Domain title.
