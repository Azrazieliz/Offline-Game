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
- [x] add Unreal compile/test runner workflow/configuration

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
- [x] automatic rotating snapshot policy

## Phase C - Character/content identity
- [x] content-definition IDs
- [x] Character Identity
- [x] Version
- [x] owned manifestation/instance
- [x] canonical Identity maturity lore plus descriptive adult-content references; no secondary eligibility gate
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
- [x] one territory + Domain Core
- [x] aggregate resources + lazy Project engine
- [x] result-oriented dispatch framework
- [x] minimal faction/army/war state
- [ ] compile and run Phase F Unreal automation tests on the real build runner

## Phase G - Vertical Slice 0

### Checkpoint G1 - Persistent acquisition / recovery
- [x] deterministic persistent gacha service
- [x] pity / featured-guarantee persistence
- [x] repeat acquisition creates distinct persistent Manifestations under one Character Identity
- [x] automatic rotating snapshots
- [x] diagnostics bundle

### Checkpoint G2 - End-to-end runtime harness
- [x] persistent Vertical Slice 0 scenario harness rewritten against the reconciled architecture
- [x] Android runtime/build configuration
- [x] lightweight performance telemetry
- [x] **environment-readiness verification only**: UE 5.8.3 (CL 58210709), Build.bat, UnrealEditor-Cmd, UBT, VS 2026/MSVC 14.51, Windows SDK 10.0.26100, Java 21, Android API 35 / Build Tools 35.0.1 / NDK r27c / adb verified usable without compiling OfflineGame. Epic Launcher registration is empty but explicit EngineRoot is usable and non-blocking.
- [x] finish the broad design-finalization pass; architecture frozen in docs/ARCHITECTURE_FREEZE.md
- [x] complete contradiction/supersession audit pass 2 + exact schema/API migration matrix
- [x] independently re-audit full pre-coding architecture and close missing persistence/settings contracts
- [x] freeze music / voice / SFX production direction
- [x] freeze visual art / VFX / animation / cinematic production direction
- [x] define versioned tuning-parameter ownership/registry contract
- [x] normalize cumulative master into `Offline_Adult_Gacha_RPG_Master_Architecture_Baseline_v0.40_FINAL_PRECODING_FREEZE.docx`
- [ ] run deterministic simulator fitting for selected tuning candidates when balance evidence is needed (non-blocking for the first Unreal compile gate)
- [x] execute design-to-code reconciliation using docs/RECONCILIATION_MIGRATION_MATRIX.md
- [x] migrate/rebuild stale runtime contracts to the frozen architecture
- [x] complete the dedicated pre-Unreal completeness/functionality audit and static gates before any real Unreal invocation
- [ ] real UE 5.8 UHT/UBT/MSVC/link compile
- [ ] OfflineGame automation run

### Known design-to-code reconciliation before the real gate
- [x] replace duplicate-acquisition counter semantics with multiple persistent Manifestations per Character Identity + migration
- [x] update vertical-slice acquisition flow so it no longer assumes immediate gacha before stable-territory unlock/anchoring rules
- [x] extend Territory persistence/control for overlapping claims and five-day reclamation state
- [x] implement Domain Core heart-loss / ruined-Domain consequences while preserving existing Broken/capture invariants and Core fusion architecture
- [x] add named Existence/Power Rank + attained/effective Rank model and Rank Suppression data
- [x] add Factor architecture and class/progression state required by the finalized player-facing systems
- [x] add World Director scheduling/audit/offline-consequence framework before relying on Director tests
- [x] add World Rank/cosmology state only where required by the first implemented content slice
- [x] specify protagonist/character Transcendence, World Fantasm and personal World Manifestation data contracts in the reconciliation matrix
- [x] implement those reconciled higher-order progression contracts
- [x] implement frozen Ruler Mode/World HUD contracts and Android notification bridge
- [x] implement lore-authoritative mature-content profile/state interfaces and privacy-presentation mask with **no secondary adult-access/appearance gate**
- [x] implement equipment/inventory/skill provenance changes, including frozen affinity/proficiency and presentation-state contracts, needed by the first integrated content slice
- [x] implement NPC promotion/knowledge/belief/language/dialogue persistence contracts
- [x] reconcile package manager, external storage, backup/migration and offline update behavior
- [x] add Heroic Record/heroification content-state support where required by authored narrative/gacha content
- [x] implement reconciled UI/view-model projections over canonical state
- [x] rewrite Vertical Slice 0 and its restart automation over the reconciled architecture

### Device gate
- [ ] playable Android build
- [ ] sustained S26 Ultra profiling
- [ ] first scope/performance audit

## Engineering rule

Do not implement a later simulation because the design mentions it. Implement it when a vertical player-facing requirement needs it.

During the active design-finalization pass, do **not** spend the first real UE compile gate proving architecture that is already known to be stale. Verify the local engine/toolchain environment separately, complete design reconciliation, update the runtime contracts, then execute the mandatory preflight -> UHT/UBT compile -> automation sequence on the reconciled code.

Detailed UI/tuning implementation contract: `docs/TUNING_UI_FREEZE.md`.
