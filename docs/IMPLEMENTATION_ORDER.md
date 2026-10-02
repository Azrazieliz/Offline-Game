# Implementation Order

## Phase A - Repository and contracts
- [x] initialize repository
- [x] establish Unreal project shell
- [x] stable entity ID type
- [x] event-ledger type
- [x] persistence interface
- [x] bootstrap SQLite schema
- [ ] decide/pin actual Unreal engine version on the build runner
- [ ] add CI formatting/static checks

## Phase B - Persistence foundation
- [ ] SQLite adapter
- [ ] migration runner
- [ ] transactional unit-of-work API
- [ ] snapshot/backup/restore
- [ ] crash-recovery tests
- [ ] database integrity command
- [ ] developer state inspector

## Phase C - Character/content identity
- [ ] content-definition IDs
- [ ] Character Identity
- [ ] Version
- [ ] owned manifestation/instance
- [ ] explicit canonical-adult eligibility metadata
- [ ] package manifest validation
- [ ] package activation registry

## Phase D - Shared rule core
- [ ] stat/value representation
- [ ] effect definitions
- [ ] conditions/triggers
- [ ] rule override/Authority resolver
- [ ] deterministic RNG/seed provenance
- [ ] resolved character skill set

## Phase E - Combat proof
- [ ] minimal turn executor
- [ ] action-combat state adapter
- [ ] one shared character kit in both modes
- [ ] defeat/recovery
- [ ] combat log
- [ ] automated deterministic replay tests

## Phase F - Shared world-state proof
- [ ] location/discovery state
- [ ] Ruler Mode -> World Mode transition
- [ ] World Mode -> Ruler Mode state reflection
- [ ] one territory + Domain Core
- [ ] resources/projects
- [ ] dispatch framework
- [ ] minimal faction/war state

## Phase G - Vertical Slice 0
- [ ] playable Android build
- [ ] persistent end-to-end scenario
- [ ] diagnostics bundle
- [ ] sustained S26 Ultra profiling
- [ ] first scope/performance audit

## Engineering rule

Do not implement a later simulation because the design mentions it. Implement it when a vertical player-facing requirement needs it.
