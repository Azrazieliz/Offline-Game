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
- [x] large stat/value representation
- [x] effect definitions
- [x] conditions/triggers
- [x] rule override/Authority precedence resolver
- [x] deterministic RNG/seed provenance
- [x] resolved character skill set
- [ ] compile and run Phase D Unreal automation tests on the real build runner

## Phase E - Combat proof
- [x] minimal turn executor foundation
- [x] action-combat shared-state adapter foundation
- [x] one shared character-kit/state model in both modes
- [x] defeat state handling inside battle
- [x] deterministic combat log foundation
- [x] timeline/defeat/identity-exclusivity automation tests authored
- [x] baseline damage/hit/crit/block resolver
- [x] resource/cost/skill readiness framework
- [x] preferred succession lanes + dynamic cross-lane fallback
- [x] revival-before-succession ordering
- [x] battle/defeat/entry trigger queue foundation
- [x] deterministic full-battle replay harness/test authored
- [ ] compile and run Phase E Unreal automation tests on the real build runner

## Phase F - Shared world-state proof
- [x] physical location state foundation
- [x] separate location knowledge/discovery projection
- [x] shared authoritative Ruler physical presence
- [x] World Mode -> Ruler Mode restart/reflection test authored
- [x] Ruler Mode / World Mode use one state contract
- [ ] one territory + Domain Core
- [ ] resources/projects
- [ ] dispatch framework
- [ ] minimal faction/war state
- [ ] compile and run Phase F Unreal automation tests on the real build runner

## Phase G - Vertical Slice 0
- [ ] playable Android build
- [ ] persistent end-to-end scenario
- [ ] diagnostics bundle
- [ ] sustained S26 Ultra profiling
- [ ] first scope/performance audit

## Engineering rule

Do not implement a later simulation because the design mentions it. Implement it when a vertical player-facing requirement needs it.
