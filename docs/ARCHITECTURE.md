# Runtime Architecture v0

This document is the first implementation architecture derived from the current master design baseline. It is intentionally smaller than the full game design.

## 1. Architectural objective

The runtime must support a very large persistent game without turning every fictional process into a simulation.

**Rule:** if the player only needs the result, store/calculate the result. Simulate a process only when the process itself creates meaningful interaction.

## 2. Authority model

### Authoritative

- SQLite-backed mutable world state
- deterministic gameplay/domain services
- content definitions and validated package manifests
- append-only meaningful world events
- recorded seeds/provenance where reproduction matters

### Non-authoritative presentation

- spawned Unreal Actors
- widgets
- VFX and blood decals
- ragdolls/temporary physics
- ordinary ambient NPC actions
- streamed terrain presentation
- animation state

Presentation can request state changes through gameplay services; it does not silently become truth.

## 3. Runtime families

The implementation should resist creating one engine per design noun.

1. **Entity / Character State**
   - stable IDs
   - Character Identity / Version / owned instance
   - progression
   - equipment
   - current skill set
   - life/injury state
   - mature visual/content eligibility metadata

2. **Capability / Rule Engine**
   - skills and effects
   - Authority / rule overrides
   - World Fantasm field/reality rules
   - traversal capabilities
   - environmental capabilities
   - Domain rules
   - deterministic conflict resolution
   - World Fantasm and Domain interactions share this rule-conflict layer; see `WORLD_FANTASM_TRANSCENDENCE.md`

3. **Combat**
   - shared definitions
   - turn executor
   - World Mode action executor
   - strategic war resolver

4. **World / Location State**
   - regions/locations
   - discovered/known state
   - environmental state
   - physical ownership
   - important roads/connections
   - location-specific changes

5. **Resources / Projects**
   - resource totals
   - crafting inputs/results
   - construction/reconstruction
   - repair
   - training and other timed projects
   - no worker simulation

6. **Dispatch**
   - exploration
   - espionage
   - gathering
   - support
   - other offscreen assignments
   - no NPC task/day simulation

7. **Faction / War**
   - faction identity
   - population as aggregate number
   - explicit war/alliance/support/subordination states
   - armies as aggregate forces until promoted near the player
   - no politics, treaties, faction-feeling model, occupation governance, or legal system

8. **Knowledge / Events / Chronicle**
   - facts known by the player/faction or specific important NPCs
   - meaningful world events
   - Chronicle projection
   - no separate generic NPC-memory simulation

9. **Gacha / Content / World Director**
   - banner definitions and state
   - pity/guarantee state
   - Character Identity manifestations
   - validated modular content packages
   - bounded event/banner scheduling

## 4. Simulation policy

### Zero/near-zero computation while irrelevant

- population birth/death detail
- ordinary NPC work
- ordinary NPC schedules
- finance
- politics
- law/crime
- relationship simulation
- food webs
- invasive species
- worker movement
- shipment/crate movement
- nested clock updates

### Event or aggregate calculation

- population change
- resource generation
- distant faction changes
- distant ecology
- construction progress
- dispatch results
- logistics availability
- remote war progression
- time-ratio conversion

### High fidelity only when relevant

- current action-combat scene
- turn battle
- important named character
- nearby major event
- active Domain/Core interaction
- important cargo/convoy explicitly promoted into gameplay
- local environmental hazards
- player-visible mature combat presentation

## 5. Cross-mode rule

Ruler Mode and World Mode are two clients of the same game core.

A World Mode discovery updates authoritative state and becomes visible in Ruler Mode.

A Ruler Mode project/dispatch changes authoritative state and its result can later become physically visible in World Mode.

Neither mode keeps a private canonical copy.

## 6. Persistence rule

SQLite is hidden behind an interface. SQL never leaks into combat/UI code.

Every write that changes multiple facts must be transactional.

Every schema change is a numbered immutable migration.

Long-lived saves require:
- migration tests
- backup/snapshot policy
- crash recovery
- WAL/checkpoint policy
- validation/repair tooling

## 7. Content rule

Gameplay logic is data-driven where values are expected to change.

Large characters/worlds/cinematics are modular packages.

The runtime may combine and simulate validated content; it does not fabricate production-quality assets at runtime.

## 8. Unreal dependency rule

Gameplay/domain state must not require a spawned Actor or Widget to exist.

Blueprint is for composition/presentation where useful. Critical rules should remain reviewable/testable C++ or declarative data.

## 9. Performance doctrine

Primary device target: Samsung Galaxy S26 Ultra.

The architecture assumes:
- aggressive World Mode streaming
- Ruler Mode unload/suspension of expensive 3D presentation
- local high fidelity, remote aggregate state
- 60 FPS default with 30/120 profiles where appropriate
- profiling decides feature cost, not theoretical elegance
