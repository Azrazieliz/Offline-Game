# Implementation Order

## Phase A - Repository and contracts
- [x] initialize repository
- [x] establish Unreal project shell
- [x] stable entity ID type
- [x] event-ledger type
- [x] persistence interface
- [x] bootstrap SQLite schema
- [x] pin Unreal Engine 5.8 as the initial build target
- [x] add repository-contract CI validation
- [ ] add Unreal compile/test runner

## Phase B - Persistence foundation
- [x] SQLiteCore adapter
- [x] numbered migration runner
- [x] transactional unit-of-work API
- [x] online SQLite backup/restore
- [x] WAL checkpoint policy
- [x] database integrity command
- [x] stable entity persistence API
- [x] restart persistence automation test
- [x] backup/restore automation test
- [ ] compile and run the Unreal automation tests on the real build runner
- [ ] developer state inspector UI/commands
- [ ] automatic rotating snapshot policy

## Phase C - Character/content identity
- [x] content-definition IDs
- [x] Character Identity
- [x] Version
- [x] owned manifestation/instance
- [x] explicit canonical-adult eligibility metadata
- [x] package manifest validation
- [x] package activation registry
- [ ] compile and run Phase C Unreal automation tests on the real build runner

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
