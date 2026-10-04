# Reconciliation Migration / API Matrix

STATUS: AUTHORITATIVE IMPLEMENTATION PLAN - AUDIT PASS 2.

Current persistent schema: **version 6**.

This plan reconciles the frozen architecture with the pre-detailing runtime **before** the first real UE compile/automation gate. It is intentionally specific enough to drive implementation without reopening creative design.

No Unreal compile/test has been run as part of this audit.

## 0. Migration safety rule

The current `FOGSQLiteWorldStore::Open()` auto-migrates immediately. That behavior must be wrapped/replaced before schema 0007+ is allowed to touch a user history.

Required bootstrap:

1. detect existing DB and schema version without mutating it;
2. copy the untouched DB to a pre-migration recovery path;
3. migrate a working copy;
4. run schema/application validation + `PRAGMA integrity_check`;
5. atomically promote the working copy only on success;
6. retain the untouched previous DB and migration report;
7. refuse normal continuation if validation fails.

### Migration runner contract change

Replace the current SQL-only migration definition with a definition capable of optional deterministic data transforms and validation:

```cpp
struct FOGMigrationDefinition
{
    int32 Version;
    const TCHAR* Name;
    const TCHAR* Sql;
    TFunction<bool(FOGSQLiteWorldStore&, FString&)> DataTransform;
    TFunction<bool(FOGSQLiteWorldStore&, FString&)> Validate;
};
```

Pure SQL migrations leave the callbacks empty.

Data transforms must be deterministic and idempotence-safe under the migration transaction.

---

# Migration 0007 - Character Manifestation instances / acquisition history

**Purpose:** remove duplicate-counter gameplay semantics and make every qualifying acquisition a full persistent Manifestation.

## Schema

Keep the legacy `duplicate_acquisition_count` column temporarily for migration provenance only. Production code stops writing/reading it after migration.

Add to `character_manifestations`:

- `acquisition_world_tick INTEGER NOT NULL DEFAULT 0`
- `acquisition_ordinal INTEGER NOT NULL DEFAULT 0`
- `origin_pull_event_id TEXT`
- `world_mode_anchor_territory_id TEXT`
- `world_mode_anchor_tick INTEGER`
- `lifecycle_state TEXT NOT NULL DEFAULT 'active'`
- `build_label TEXT NOT NULL DEFAULT ''`

Add indexes:

- `idx_manifestations_owner_identity(owning_ruler_entity_id, identity_content_id)`
- `idx_manifestations_owner_identity_ordinal(owning_ruler_entity_id, identity_content_id, acquisition_ordinal)`
- `idx_manifestations_anchor(world_mode_anchor_territory_id)`

## Data transform

For legacy rows with `duplicate_acquisition_count > 0`:

1. read matching historical `gacha_pull` world events in deterministic event order;
2. retain the existing Manifestation as acquisition ordinal 0 and preserve all existing progression on it;
3. materialize one additional Manifestation for each historical repeat acquisition;
4. derive stable migration IDs from `legacy_manifestation_id + acquisition_ordinal` rather than random GUID generation;
5. initialize reconstructed copies from the actual event's acquired Version/Rarity where available;
6. if an old counter exceeds recoverable event history, create deterministic migration-reconstructed copies and record a migration audit event;
7. set the legacy counter to zero or leave it untouched but ignored; it must never again drive gameplay.

## Runtime/API changes

Remove from gameplay structs:
- `FOGCharacterManifestationRecord::DuplicateAcquisitionCount`
- `FOGGachaPullResult::DuplicateAcquisitionCount`

Retain `bDuplicateIdentity` only as presentation/history meaning: "Identity was already owned before this pull."

Replace:
`TryFindCharacterManifestationByOwnerAndIdentity(...single result...)`

with:
```cpp
ListCharacterManifestationsByOwnerAndIdentity(
    RulerId,
    IdentityId,
    TArray<FOGCharacterManifestationRecord>& OutManifestations,
    FString& OutError);
```

Add:
```cpp
ListCharacterManifestationsByOwner(...);
SetManifestationAnchor(...);
SetManifestationLifecycle(...);
```

`FOGGachaService::Pull` always creates a new Manifestation for a character acquisition result.

---

# Migration 0008 - Territory claims, reclamation, sovereignty and gacha access

**Purpose:** represent overlapping control, five-day reclamation, organic Ruler/Overlord state, one-month gacha qualification and permanent unlock.

## Schema

### `location_territories`

Many-to-many physical membership/overlap:

- `location_entity_id TEXT NOT NULL`
- `territory_entity_id TEXT NOT NULL`
- `relation_kind TEXT NOT NULL DEFAULT 'contained'`
- `coverage_bps INTEGER NOT NULL DEFAULT 10000`
- PRIMARY KEY `(location_entity_id, territory_entity_id)`

The legacy `locations.territory_entity_id` may remain temporarily as a non-authoritative dominant/display cache.

### `territory_claims`

- `claim_entity_id TEXT PRIMARY KEY`
- `territory_entity_id TEXT NOT NULL`
- `ruler_entity_id TEXT NOT NULL`
- `claim_kind TEXT NOT NULL`
- `control_state TEXT NOT NULL`
- `control_strength_bps INTEGER NOT NULL DEFAULT 0`
- `claim_start_world_tick INTEGER NOT NULL`
- `effective_control_start_world_tick INTEGER`
- `displaced_world_tick INTEGER`
- `reclaim_deadline_world_tick INTEGER`
- `updated_world_tick INTEGER NOT NULL`
- `state_json TEXT NOT NULL DEFAULT '{}'`
- UNIQUE `(territory_entity_id, ruler_entity_id)`

The legacy `territories.ruler_entity_id/control_state` become cached projection fields during transition, not the source of truth.

### `ruler_sovereignty_state`

- `ruler_entity_id TEXT PRIMARY KEY`
- `current_title TEXT NOT NULL DEFAULT 'ruler'`
- `historical_peak_title TEXT NOT NULL DEFAULT 'ruler'`
- `continuous_control_start_world_tick INTEGER`
- `last_effective_control_world_tick INTEGER`
- `scope_state_json TEXT NOT NULL DEFAULT '{}'`
- `updated_world_tick INTEGER NOT NULL`

Only `ruler` and `overlord` are universal title values. Other culture/story titles live elsewhere.

### `ruler_gacha_access`

- `ruler_entity_id TEXT PRIMARY KEY`
- `qualification_start_world_tick INTEGER`
- `qualification_suspended_world_tick INTEGER`
- `unlocked_world_tick INTEGER`
- `permanently_unlocked INTEGER NOT NULL DEFAULT 0`
- `updated_world_tick INTEGER NOT NULL`

Five-day reclamation grace can suspend continuity without treating it as a true break. Expiry without reclamation resets pre-unlock qualification. Once `permanently_unlocked=1`, later territory loss does not revoke gacha.

## Services

Add:
- `FOGTerritoryControlService`
- `FOGSovereigntyService`
- `FOGRulerGachaAccessService`

Key API:
```cpp
EvaluateEffectiveControl(TerritoryId, WorldTick, ...);
BeginDisplacement(...);
ReclaimTerritory(...);
ExpireReclamationIfDue(...);
ListActiveClaimsForLocation(...);
RefreshSovereigntyState(...);
CanUseGacha(RulerId, WorldTick, ...);
RefreshGachaQualification(...);
```

Gacha service must call `CanUseGacha` before currency/pity mutation.

Manifestation World-Mode deployment validates the 0007 anchor fields.

---

# Migration 0009 - Domain heart state / Core fusion lineage

**Purpose:** preserve the already-correct Broken-vs-captured invariant while adding Core-as-heart consequences and previously defined fusion.

## Schema

### `territory_domain_state`

- `territory_entity_id TEXT PRIMARY KEY`
- `active_core_entity_id TEXT`
- `domain_state TEXT NOT NULL DEFAULT 'none'`
- `heart_lost_world_tick INTEGER`
- `ruin_started_world_tick INTEGER`
- `reconstitution_project_entity_id TEXT`
- `state_json TEXT NOT NULL DEFAULT '{}'`

Expected states include:
- `none`
- `functional`
- `damaged`
- `heart_lost_ruining`
- `ruined`
- `reconstituting`

Physical Territory destruction is separate from this Domain-heart state.

### `domain_core_concepts`

Normalized successor/projection to the current Aspect table:

- `core_entity_id TEXT NOT NULL`
- `concept_content_id TEXT NOT NULL`
- `grade INTEGER NOT NULL DEFAULT 0`
- `origin_source_core_entity_id TEXT`
- `synthesis_rule_content_id TEXT`
- `state_json TEXT NOT NULL DEFAULT '{}'`
- PRIMARY KEY `(core_entity_id, concept_content_id)`

Existing `domain_core_aspects` rows migrate into this table.

### `domain_core_fusions`

- `fusion_entity_id TEXT PRIMARY KEY`
- `result_core_entity_id TEXT NOT NULL`
- `absorber_core_entity_id TEXT NOT NULL`
- `absorbed_core_entity_id TEXT NOT NULL`
- `fusion_world_tick INTEGER NOT NULL`
- `sequence_ordinal INTEGER NOT NULL`
- `synthesis_rule_content_id TEXT`
- `outcome_kind TEXT NOT NULL`
- `resolution_seed INTEGER NOT NULL DEFAULT 0`
- `instability_state_json TEXT NOT NULL DEFAULT '{}'`

### `domain_core_lineage`

- `result_core_entity_id TEXT NOT NULL`
- `source_core_entity_id TEXT NOT NULL`
- `fusion_entity_id TEXT NOT NULL`
- `lineage_role TEXT NOT NULL`
- PRIMARY KEY `(result_core_entity_id, source_core_entity_id, fusion_entity_id)`

## Service changes

Split current Territory Project service responsibilities:

- Project timing remains in `FOGProjectService`.
- Core damage/capture/fusion/heart state moves to `FOGDomainCoreService`.

APIs:
```cpp
ApplyCoreDurabilityDamage(...);
CaptureIntactCore(...);
FuseCores(AbsorberCoreId, AbsorbedCoreId, RuleId, Seed, ...);
RefreshDomainHeartConsequences(...);
BeginHeartReconstitution(...);
```

Core loss can trigger Domain ruin even while physical Territory ownership persists.

Physical destruction with protected Core remains repairable/functional in Domain terms.

---

# Migration 0010 - Rank, Factors, Classes and character-route progression

**Purpose:** replace `ProgressionStateJson` as the primary store for core queryable progression.

## `entity_rank_state`

- `entity_id TEXT PRIMARY KEY`
- `attained_rank_content_id TEXT NOT NULL`
- `attained_level INTEGER NOT NULL CHECK(attained_level BETWEEN 1 AND 100)`
- `effective_rank_content_id TEXT`
- `effective_level INTEGER`
- `peak_rank_content_id TEXT NOT NULL`
- `peak_level INTEGER NOT NULL`
- `updated_world_tick INTEGER NOT NULL`
- `state_json TEXT NOT NULL DEFAULT '{}'`

Rank definitions/ordering remain content data, not a C++ enum.

## `factor_instances`

- `factor_instance_id TEXT PRIMARY KEY`
- `owner_entity_id TEXT NOT NULL`
- `factor_content_id TEXT NOT NULL`
- `source_entity_id TEXT`
- `acquired_world_tick INTEGER NOT NULL`
- `purity_bps INTEGER NOT NULL DEFAULT 10000`
- `maturity_bps INTEGER NOT NULL DEFAULT 0`
- `completeness_bps INTEGER NOT NULL DEFAULT 10000`
- `expression_weight_bps INTEGER NOT NULL DEFAULT 0`
- `state_json TEXT NOT NULL DEFAULT '{}'`

No "active slot" boolean exists. All owned Factors remain causal.

## `factor_lineage`

- `child_factor_instance_id TEXT NOT NULL`
- `parent_factor_instance_id TEXT NOT NULL`
- `relation_kind TEXT NOT NULL`
- `ordinal INTEGER NOT NULL DEFAULT 0`
- PRIMARY KEY `(child_factor_instance_id, parent_factor_instance_id)`

## `entity_classes`

- `owner_entity_id TEXT NOT NULL`
- `class_content_id TEXT NOT NULL`
- `attained_tier TEXT NOT NULL` (normal/crown or content-extensible)
- `current_expression_state TEXT NOT NULL DEFAULT 'expressible'`
- `recognized_world_tick INTEGER NOT NULL`
- `updated_world_tick INTEGER NOT NULL`
- `state_json TEXT NOT NULL DEFAULT '{}'`
- PRIMARY KEY `(owner_entity_id, class_content_id)`

## `grand_class_seats`

- `class_content_id TEXT PRIMARY KEY`
- `bearer_entity_id TEXT NOT NULL`
- `appointed_world_tick INTEGER NOT NULL`
- `seat_state TEXT NOT NULL DEFAULT 'active'`
- `mandate_state_json TEXT NOT NULL DEFAULT '{}'`

The primary key enforces at most one active row/seat per canonical Class. Historical appointments remain in the event ledger/Chronicle.

## `entity_skills`

- `owner_entity_id TEXT NOT NULL`
- `skill_content_id TEXT NOT NULL`
- `learned_world_tick INTEGER NOT NULL`
- `current_state TEXT NOT NULL DEFAULT 'learned'`
- `development_state_json TEXT NOT NULL DEFAULT '{}'`
- PRIMARY KEY `(owner_entity_id, skill_content_id)`

## `skill_provenance`

- `owner_entity_id TEXT NOT NULL`
- `skill_content_id TEXT NOT NULL`
- `source_kind TEXT NOT NULL`
- `source_content_or_entity_id TEXT NOT NULL`
- `source_world_tick INTEGER NOT NULL`
- PRIMARY KEY `(owner_entity_id, skill_content_id, source_kind, source_content_or_entity_id)`

## `manifestation_route_nodes`

- `manifestation_entity_id TEXT NOT NULL`
- `route_content_id TEXT NOT NULL`
- `node_content_id TEXT NOT NULL`
- `state TEXT NOT NULL`
- `entered_world_tick INTEGER`
- `completed_world_tick INTEGER`
- `state_json TEXT NOT NULL DEFAULT '{}'`
- PRIMARY KEY `(manifestation_entity_id, route_content_id, node_content_id)`

Evolution/Awakening/Corruption are content route families, not C++ enum limits.

## `manifestation_forms`

- `manifestation_entity_id TEXT NOT NULL`
- `form_content_id TEXT NOT NULL`
- `state TEXT NOT NULL`
- `unlocked_world_tick INTEGER NOT NULL`
- `state_json TEXT NOT NULL DEFAULT '{}'`
- PRIMARY KEY `(manifestation_entity_id, form_content_id)`

## `manifestation_reinforcement`

- `manifestation_entity_id TEXT PRIMARY KEY`
- `reinforcement_state TEXT NOT NULL`
- `max_reinforced INTEGER NOT NULL DEFAULT 0`
- `updated_world_tick INTEGER NOT NULL`
- `state_json TEXT NOT NULL DEFAULT '{}'`

## `character_convergences`

- `convergence_entity_id TEXT PRIMARY KEY`
- `identity_content_id TEXT NOT NULL`
- `result_manifestation_entity_id TEXT NOT NULL`
- `rule_content_id TEXT NOT NULL`
- `world_tick INTEGER NOT NULL`
- `state_json TEXT NOT NULL DEFAULT '{}'`

## `character_convergence_sources`

- `convergence_entity_id TEXT NOT NULL`
- `source_manifestation_entity_id TEXT NOT NULL`
- `lineage_content_id TEXT`
- `ordinal INTEGER NOT NULL`
- PRIMARY KEY `(convergence_entity_id, source_manifestation_entity_id)`

Source Manifestations become lifecycle `converged`, not physically deleted.

## Services

Add:
- `FOGRankService`
- `FOGFactorService`
- `FOGClassRecognitionService`
- `FOGCharacterProgressionService`
- `FOGGrandConvergenceService`

Combat consumes a resolved projection of these systems rather than owning them.

---

# Migration 0011 - Reality nodes, World Rank, time domains and World Director

## `reality_nodes`

- `reality_entity_id TEXT PRIMARY KEY`
- `parent_reality_entity_id TEXT`
- `kind TEXT NOT NULL` (world/dimension/subrealm/etc.)
- `world_rank_content_id TEXT`
- `time_domain_entity_id TEXT`
- `law_profile_content_id TEXT`
- `state_json TEXT NOT NULL DEFAULT '{}'`

## `time_domains`

- `time_domain_entity_id TEXT PRIMARY KEY`
- `parent_time_domain_entity_id TEXT`
- `rate_numerator INTEGER NOT NULL DEFAULT 1`
- `rate_denominator INTEGER NOT NULL DEFAULT 1`
- `parent_epoch_tick INTEGER NOT NULL DEFAULT 0`
- `local_epoch_tick INTEGER NOT NULL DEFAULT 0`
- `calendar_content_id TEXT`
- `state_json TEXT NOT NULL DEFAULT '{}'`

Global monotonic canonical ticks remain available; local calendars/time are deterministic projections.

## `junctions`

- `junction_entity_id TEXT PRIMARY KEY`
- `from_reality_entity_id TEXT NOT NULL`
- `to_reality_entity_id TEXT NOT NULL`
- `state TEXT NOT NULL`
- `stability_bps INTEGER NOT NULL DEFAULT 10000`
- `opened_world_tick INTEGER`
- `closed_world_tick INTEGER`
- `requirements_json TEXT NOT NULL DEFAULT '{}'`

## `world_director_schedule`

- `schedule_entity_id TEXT PRIMARY KEY`
- `content_id TEXT NOT NULL`
- `template_content_id TEXT`
- `status TEXT NOT NULL`
- `eligible_since_world_tick INTEGER`
- `scheduled_start_world_tick INTEGER`
- `latest_start_world_tick INTEGER`
- `resolution_seed INTEGER NOT NULL`
- `decision_provenance_json TEXT NOT NULL DEFAULT '{}'`

## `content_unlock_state`

- `content_id TEXT PRIMARY KEY`
- `state TEXT NOT NULL` (sealed/eligible/released/retired/etc.)
- `eligible_world_tick INTEGER`
- `released_world_tick INTEGER`
- `state_json TEXT NOT NULL DEFAULT '{}'`

## `offline_simulation_state`

- `scope_entity_id TEXT PRIMARY KEY`
- `last_active_world_tick INTEGER NOT NULL`
- `last_catchup_world_tick INTEGER NOT NULL`
- `governor_state_json TEXT NOT NULL DEFAULT '{}'`

World Director decisions always record template/content IDs, package versions/inputs, deterministic seed and decision provenance.

The Director is metaphysically semi-conscious at the lore layer but the runtime remains bounded deterministic scheduling/composition logic.

---

# Migration 0012 - Dispatch, war, armies, Projects, civilization and logistics

## Dispatch extension

Add to `dispatches`:
- `risk_tolerance_bps INTEGER NOT NULL DEFAULT 5000`
- `abort_policy_json TEXT NOT NULL DEFAULT '{}'`
- `outcome_state TEXT NOT NULL DEFAULT ''`
- `delay_until_world_tick INTEGER`

Add `dispatch_objectives`:
- `dispatch_entity_id`
- `objective_content_id`
- `priority INTEGER`
- `mandatory INTEGER`
- `target_entity_id`
- `state_json`
- PRIMARY KEY `(dispatch_entity_id, objective_content_id)`

Add `dispatch_constraints`:
- `dispatch_entity_id`
- `constraint_content_id`
- `state_json`
- PRIMARY KEY `(dispatch_entity_id, constraint_content_id)`

Resolver priority:
mandatory objective/constraints -> survival/abort -> secondary objectives -> compatible opportunities.

## Continuous-war extension

Add:
### `war_fronts`
- front ID, parent War, location/reality scope, state, start/end ticks, state JSON.

### `war_objectives`
- War/front ID, objective ID, target, priority, status, state JSON.

### `war_orders`
- order ID, War/front, issuer, recipient/army, intent, constraints JSON, issued tick, outcome state, outcome JSON.

### `war_participant_history`
- War, faction, side, joined tick, left tick, reason.

Existing `wars` remains the parent persistent conflict.

## Army capability vectors

Add `army_capabilities`:
- `army_entity_id`
- `capability_content_id`
- `magnitude_sig INTEGER`
- `magnitude_exp INTEGER`
- `state_json`
- PRIMARY KEY `(army_entity_id, capability_content_id)`

`EffectivePower` may remain as a cached summary/debug projection only.

## Projects

Add `project_phases`:
- project ID, phase ID, sequence, status, start/resolve ticks, progress, payload.

Add `project_assignments`:
- project ID, assignee entity ID, role content ID, state JSON.

Simple projects may still resolve as one lazy phase.

## Civilization

Add `civilization_state`:
- civilization/faction entity ID PK
- genre/profile content ID
- state JSON

Add `civilization_dimensions`:
- civilization ID
- dimension content ID (e.g. cultivation infrastructure, magical institutions, shipbuilding, draconic architecture - authored per civilization)
- grade/state
- PRIMARY KEY

No universal "modernization level" exists.

## Logistics

Add `logistics_routes`:
- route ID
- owner entity
- origin/destination location/reality
- transport capability ID
- capacity
- risk
- status
- state JSON

Valid owned teleportation/dimensional capabilities can create/bypass routes according to their actual rules.

---

# Migration 0013 - Items/inventory, knowledge, NPC promotion, adult state and Heroic Records

## Items / equipment / inventory

### `item_instances`
- item entity ID PK
- definition content ID
- owner entity ID
- current Rank content ID
- quality content ID
- durability state
- evolution state JSON
- history state JSON

### `item_modifiers`
- item ID
- modifier content ID
- ordinal
- state JSON

### `equipment_bindings`
- wearer entity ID
- slot content ID
- item entity ID
- state JSON
- PRIMARY KEY `(wearer, slot)`

Slots are character/body-definition driven, not a universal armor enum.

### `inventory_containers`
- container entity ID PK
- owner entity ID
- container type content ID
- capacity state JSON

### `container_contents`
- container ID
- item entity ID
- amount for aggregatable stackable items
- PRIMARY KEY

## Knowledge / belief

Extend `knowledge_facts` with:
- `belief_state TEXT NOT NULL DEFAULT 'believed'`
- `confidence_bps INTEGER NOT NULL DEFAULT 10000`
- `source_entity_id TEXT`
- `source_event_id TEXT`
- `evidence_world_tick INTEGER`
- `language_context_content_id TEXT`

Add `entity_languages`:
- entity ID
- language content ID
- spoken proficiency
- written/script proficiency
- state JSON

Add `semantic_memories`:
- memory ID PK
- owner entity
- subject entity
- source event
- memory type
- salience
- state JSON

No unlimited verbatim life transcript is stored.

## NPC promotion

Add `npc_promotion_state`:
- entity ID PK
- current simulation tier
- promoted world tick
- reason event ID
- presentation package state JSON

Promotion never replaces the existing entity ID/history.

## Adult-content runtime state

Canonical adulthood remains **content Identity lore** and is not duplicated as a gameplay gate.

Add `character_adult_runtime_state` only for mutable expression/context:
- character/Manifestation entity ID PK
- current profile variant content ID
- mutable libido/preferences/context state JSON
- updated world tick

There is **no eligible/consent/permission boolean** in this table.

Adult Identity + installed relevant content means the pillar is system-available. Current body/injury/Version/profile selects/adapts what can physically be presented.

Privacy/SFW Presentation is a user setting, not canonical world state.

## Heroic Records

Add `heroic_records`:
- record entity ID PK
- source world entity ID
- identity content ID
- death event ID
- created world tick
- pattern content ID
- gacha access state
- state JSON

The source death remains canonical.

---

# Migration 0014 - Packages, Reports/notifications and recovery metadata

## Content packages

Extend `content_packages` with:
- `category TEXT NOT NULL DEFAULT 'generic'`
- `install_uri TEXT NOT NULL DEFAULT ''`
- `storage_class TEXT NOT NULL DEFAULT 'local_hot'`
- `sealed_state TEXT NOT NULL DEFAULT 'visible'`
- `download_state TEXT NOT NULL DEFAULT 'installed'`
- `compatibility_json TEXT NOT NULL DEFAULT '{}'`

Add normalized `package_dependencies`:
- package ID
- dependency package ID
- minimum version
- PRIMARY KEY

Activation must verify dependencies are installed, validated, active/available as required and at sufficient versions; detect cycles.

## Reports

Add `reports`:
- report entity ID PK
- owner/ruler entity ID
- source world event ID
- category
- priority
- created world tick
- acknowledged world tick
- payload JSON

Add `report_delivery`:
- report ID
- channel (`ingame`, `android_notification`)
- state
- scheduled real UTC
- delivered real UTC
- platform notification ID
- privacy state
- PRIMARY KEY `(report_id, channel)`

Android notifications are projections of Reports and never authoritative world state.

## Backup/recovery catalog

The protected backup catalog should live outside the canonical world DB so a corrupt/missing world DB cannot destroy recovery metadata.

Store sidecar metadata containing:
- backup ID/path/URI;
- schema version;
- world identity;
- creation time;
- hash;
- source app/build version;
- validation status.

Clear World does not delete this catalog/backups unless the user explicitly chooses backup deletion.

---

# C++ contract normalization before implementation

## Character definitions

`FOGCharacterVersionDefinition::bSexualContentEligible` is removed.

Replace it with **descriptive content references**, for example:
- `AdultContentProfileId`
- `AdultPresentationTags`
- Version/form scene-library references

These describe authored content/presentation and never override Identity maturity.

`EOGCanonicalMaturity` remains Identity lore data:
- Unknown
- NonAdult
- Adult

Appearance is never an input to this value.

## Content manifest validator

Validator requirements:
- Version -> Identity exists;
- Adult-content references are permitted automatically when Identity lore maturity is Adult;
- NonAdult/minor/child Identity cannot contain sexual-content references;
- no appearance/body field is consulted;
- package dependencies/cycles/versions validate before activation.

## Combat shared identity rule

Create one shared validator used by both turn and action modes:
```cpp
ValidateLocalIdentityExclusivity(
    Units,
    const FOGIdentityExclusivityContext& Context,
    FString& OutError);
```

Default: one Character Identity per local encounter side/encounter according to current design.

Explicit mechanics can provide a rule override; neither executor hard-codes separate logic.

## Runtime services to add/reconcile

- `FOGWorldBootstrapService` - migration-safe open/recovery.
- `FOGTerritoryControlService`
- `FOGSovereigntyService`
- `FOGRulerGachaAccessService`
- `FOGDomainCoreService`
- `FOGRankService`
- `FOGFactorService`
- `FOGClassRecognitionService`
- `FOGCharacterProgressionService`
- `FOGGrandConvergenceService`
- `FOGWorldTimeService`
- `FOGWorldDirectorService`
- `FOGStrategicResolutionService`
- `FOGReportService`
- `FOGPackageManagerService`
- Android notification bridge beneath Reports.

Existing useful services are evolved/reused rather than discarded.

---

# Test rewrite matrix - author now, run only after reconciliation

No real Unreal tests are to be run until implementation catches up.

## P0 compile/static proofs

- Character manifest tests use one-way Version -> Identity; no nonexistent `Identity.VersionIds`.
- no production reference to `DuplicateAcquisitionCount`.
- no production reference to `bSexualContentEligible`.

## Acquisition

- first pull creates Manifestation A;
- second same-Identity pull creates distinct Manifestation B;
- both survive restart independently;
- no copy cap;
- Grand Convergence explicitly consumes selected source lifecycle into one result.

## Gacha sovereignty

- no gacha before continuous qualification;
- five-day reclaim before unlock suspends continuity without false reset;
- expiry without reclaim resets qualification;
- unlock occurs after > one in-game month;
- unlock is permanent thereafter;
- pull away from Territory is allowed after unlock;
- new pull remains World-Mode unanchored until controlled Territory is reached.

## Territory/Core

- overlapping claims can coexist on one physical Location;
- effective control resolves without deleting rival claim history;
- reclaim within five days restores continuity;
- missed deadline makes loss real;
- physically ruined Territory + protected Core remains Domain-functional/repairable;
- Broken/lost Core triggers heart-loss ruin path;
- zero durability remains Broken, never capture;
- intact capture preserves positive durability;
- Core A absorbing B differs from B absorbing A when synthesis rules say so;
- fusion provenance survives restart.

## Rank/Factor/Class/progression

- rank definitions are data-driven;
- attained/effective Rank diverge/restores correctly;
- all acquired Factors remain causal with no slot-based disable;
- fusion lineage/provenance persists;
- multiple Classes coexist;
- Crown is non-unique;
- one active Grand seat per canonical Class;
- route nodes are open-ended content IDs;
- source Manifestations become historical/converged, not deleted.

## Combat

- action and turn combat share same Identity-exclusivity rule;
- explicit override can permit an exceptional encounter;
- Rank Suppression hooks are channel-based and data-tunable.

## World Director/time

- Director scheduling records deterministic provenance;
- delay never exceeds authored bound;
- offline consequence governor does not fabricate catastrophic wipe;
- world/local time projection is deterministic.

## Strategy

- rescue Dispatch cannot abandon mandatory rescue to farm unrelated reward;
- hard constraints/abort conditions survive restart;
- War supports multiple fronts/objectives;
- commander disobedience is explicit outcome, not silent order rewrite;
- army resolver consumes capability vectors rather than only summary power.

## Mature/lore validation

- Adult Identity validates adult-content references regardless of appearance metadata;
- Version cannot independently disable Adult Identity access;
- NonAdult Identity rejects sexual-content package references;
- runtime adult state contains no generic access/consent flag;
- Privacy mode changes presentation only.

## Packages/recovery/Android

- dependency minimum versions/cycles are enforced;
- pre-migration original remains untouched on injected migration failure;
- corrupt working copy never replaces authoritative DB;
- external backup catalog survives Clear World;
- Report acknowledgement and Android-delivery state do not rewrite the source world event;
- orientation defaults automatic and respects player lock.

---

# Implementation ordering

1. migration-safe bootstrap/recovery wrapper;
2. 0007 multiple Manifestations + gacha rewrite;
3. 0008 Territory claims/reclamation/gacha access/anchoring;
4. 0009 Core heart/fusion;
5. 0010 Rank/Factors/Classes/routes/Convergence;
6. shared combat Identity/Rank hooks;
7. 0011 time/cosmology/World Director;
8. 0012 strategic/civilization/logistics expansion;
9. 0013 items/knowledge/NPC/adult-state/Heroic Records;
10. 0014 packages/Reports/Android/recovery catalog;
11. reconciled UI/view-model projections;
12. rewrite vertical-slice and automation tests;
13. static Unreal C++ preflight;
14. UE 5.8 UHT/UBT/MSVC/link;
15. automation tests;
16. Android cook/package/install;
17. physical S26 Ultra profiling.

This order is dependency-driven, not a gameplay priority ranking.
