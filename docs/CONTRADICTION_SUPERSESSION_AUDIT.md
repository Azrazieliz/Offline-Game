# Contradiction / Supersession Audit

STATUS: ACTIVE - PASS 2 COMPLETE (design/runtime contract + dedicated-doc normalization + exact migration/API plan). No Unreal compile/test has been run.

## Audit authority / precedence

When two statements conflict, resolve in this order:

1. newest explicit Creative Director decision;
2. frozen dedicated architecture documents;
3. newest specialized checkpoint;
4. older cumulative-baseline sections as historical context only;
5. pre-freeze runtime code/tests as implementation evidence, never as design authority.

The cumulative DOCX intentionally contains historical checkpoints, so this audit distinguishes **historical superseded text** from **current authoritative text** rather than treating every old paragraph as simultaneously true.

## Latest maturity/adult-content correction

Current rule:

- canonical lore is the sole source of adulthood/maturity data;
- appearance is never used to infer or override adulthood;
- if canonical lore defines a character as adult/mature, adult-content support is allowed by default;
- preferences, libido, personality, relationship/history and current state shape scene expression/content rather than acting as a generic access gate;
- no per-Version "sexual eligibility" switch may silently override an Adult Identity;
- characters canonically defined as minors/children remain outside sexual-content packages.

The current source data model's Identity-level maturity field can remain, but it must be understood as **lore data**, not a visual/gameplay classifier.

---

# A. P0 - confirmed blockers / direct frozen-architecture contradictions

## A1. Content identity automation test currently references a removed field

**Files**
- `Source/OfflineGame/Private/Tests/OGContentIdentityTests.cpp`
- `Source/OfflineGame/Public/Characters/OGCharacterDefinitions.h`

The test calls `Identity.VersionIds.Add(...)`, but `FOGCharacterIdentityDefinition` has no `VersionIds` member in the current header.

**Impact:** confirmed static compile blocker once the real UE gate is attempted.

**Reconciliation:** rewrite the tests to use the current one-way Version -> Identity relationship. Do not re-add a mutable Version list to Identity merely to satisfy stale tests.

## A2. Repeat gacha acquisition still collapses into DuplicateAcquisitionCount

**Files**
- `OGCharacterDefinitions.h`
- `OGGachaDefinitions.h`
- `OGGachaService.cpp`
- `OGSQLiteWorldStore.Content.cpp`
- `OGSQLiteWorldStore.cpp` migration 0006
- `OGGachaTests.cpp`

Current runtime:
- looks up the first Manifestation by owner+Identity;
- increments `DuplicateAcquisitionCount`;
- reuses the same Manifestation ID;
- tests explicitly require the second pull to reuse the first Manifestation.

Frozen design:
- every repeat pull normally creates a new full persistent Manifestation;
- copies develop independently;
- copies may later participate in Grand Convergence;
- no generic copy-to-shards/duplicate-counter economy.

**Impact:** direct gameplay contradiction and schema/test migration requirement.

**Reconciliation:** migration 0007+ will stop using `duplicate_acquisition_count` as gameplay state, add list/query APIs for multiple Manifestations, create a new Manifestation transactionally on every qualifying pull, and rewrite tests. The legacy column may be retained temporarily only for migration provenance.

## A3. Version-level sexual eligibility is an obsolete second gate

**Files**
- `OGCharacterDefinitions.h`
- `OGContentManifest.cpp`
- `OGContentIdentityTests.cpp`

Current runtime exposes `FOGCharacterVersionDefinition::bSexualContentEligible` and validates that flag against Identity maturity.

Frozen rule now says an Adult Identity is adult-content capable by default; Version/form data can describe **available presentation/assets/profile differences**, but it cannot introduce a second generic eligibility gate.

**Reconciliation:** replace the boolean's access-control meaning with content/profile capability metadata (for example profile/scene references/tags or bespoke-content availability). Identity maturity remains lore-authoritative.

## A4. Gacha access gate is absent and the vertical slice proves the wrong order

**File**
- `OGVerticalSliceScenario.cpp`

Current vertical slice:
1. creates Ruler;
2. seeds pull currency;
3. performs gacha pull at world tick 10;
4. only later creates Territory at tick 30.

Frozen design:
- gacha unlocks only after continuous territorial control for more than one in-game month;
- the unlock then remains permanent;
- first World Mode deployment requires return to controlled territory / first roster anchoring.

**Impact:** current integration test encodes a superseded architecture.

**Reconciliation:** rewrite the vertical slice around territorial qualification, unlock state, pull, roster anchoring, then combat.

## A5. Turn combat does not enforce local Character Identity exclusivity

**Files**
- `OGActionCombatAdapter.cpp`
- `OGTurnBattle.cpp`

Action combat explicitly rejects two units with the same Character Identity.

Turn combat validates entity IDs, teams and formation membership but currently does not reject repeated Character Identity IDs.

Frozen design applies local Identity exclusivity to a local fight unless an explicit mechanic overrides it.

**Reconciliation:** move Identity-exclusivity validation into a shared combat/team rule with an explicit bypass mechanism so both action and turn executors use the same rule.

---

## A6. Current startup snapshot occurs **after** migrations

**Files**
- `OGGameCoreSubsystem.cpp`
- `OGSQLiteWorldStore.cpp`
- `OGSnapshotService.cpp`

Current startup detects an existing DB, calls `WorldStore->Open(...)`, and `Open()` immediately applies migrations. Only **after that** does GameCore create the rotating snapshot.

Frozen recovery rule requires the untouched pre-migration history to survive if migration fails.

**Impact:** a later migration sequence could leave the sole local DB partially upgraded across successfully committed earlier migration steps before a later step fails, without an untouched pre-migration snapshot.

**Reconciliation:** before opening/migrating the authoritative DB, create/preserve an external or side-by-side pre-migration copy; migrate a working copy; integrity-validate; atomically promote only on success. Keep numbered immutable migrations.

# B. P1 - schema/service architecture mismatches

## B1. Territory model cannot represent overlapping control/claims

Current schema:
- `locations.territory_entity_id` is singular;
- `territories.ruler_entity_id` is singular.

Frozen design permits overlapping claims/control in the same physical region.

**Reconciliation:** separate physical location membership from claims/control, likely through normalized location-territory and territory-claim/control records. A single convenience "dominant/effective controller" projection may exist but cannot be authoritative truth.

## B2. Five-day territorial reclamation is absent

No persisted:
- displacement/loss tick;
- reclamation deadline;
- prior controller continuity;
- pending-loss state.

**Reconciliation:** add explicit reclamation/continuity state and deterministic expiry processing.

## B3. Core break currently does not propagate Domain-heart ruin state

`ApplyCoreDurabilityDamage` correctly turns zero durability into Broken and clears the controller, preserving the already-correct "broken is not captured" invariant.

Missing:
- Territory/Domain heart-loss state;
- rapid Domain degradation/ruin consequences;
- extraordinary heart-reconstitution path;
- separation of physically destroyed territory vs protected functioning Core.

**Reconciliation:** retain Broken/capture behavior and add Domain-heart consequence state around it.

## B4. Core fusion is underrepresented

Current runtime stores:
- Core lifecycle;
- durability;
- flat Aspect list.

Frozen design additionally requires:
- absorbed-Core provenance;
- asymmetric fusion order;
- parent Concept identity;
- synthesized Concept/ability lineage;
- instability/mutation/corruption/overload outcomes;
- persistent fusion history.

**Reconciliation:** normalized fusion/provenance records plus data-driven Concept synthesis results.

## B5. Character progression is still mostly a generic JSON payload

`FOGCharacterManifestationRecord` currently has Level, CurrentRarity and `ProgressionStateJson`.

Frozen design requires queryable first-class state for:
- Existence Rank/Level;
- attained vs effective Rank;
- development route graph;
- reinforcement state;
- learned-skill provenance;
- forms;
- Grand Convergence state;
- equipment references;
- other core progression facts.

**Reconciliation:** normalize searchable/constraint-bearing state; reserve JSON for truly sparse extension payloads.

## B6. Existence Rank / Rank Suppression is absent from runtime

No current C++/schema module represents the canonical Mortal -> ... -> Primordial ladder, 1-100 Rank levels, attained/effective split, world suppression or channel-specific Rank Suppression.

**Reconciliation:** add data-defined rank registry + persisted character rank state + shared interaction hooks before tuning coefficients.

## B7. Factor system is absent from runtime

No persisted Factor graph/provenance/potency/fusion/expression state exists.

**Reconciliation:** add Factor definition/state/provenance/synthesis structures; avoid inert tags.

## B8. Class/Crown/Grand system is absent from runtime

No automatic mastery recognition, emergent/composite Classes, Crown state or unique Grand seat state exists.

**Reconciliation:** add mastery recognition data/services and globally unique active Grand-seat ownership per canonical Class.

## B9. World Director is absent from runtime

No Director scheduler, delay budget, offline consequence governor, sealed-content activation, composition provenance or replay log exists.

**Reconciliation:** implement after authoritative world-time/event foundations are normalized.

## B10. World Rank / cosmology state is absent

No World Rank, world tolerance, Junction, dimensional hierarchy or cross-world time relation state currently exists.

**Reconciliation:** introduce only the first-slice data needed, keeping IDs/data model extensible.

## B11. Dispatch contract is too thin for frozen objective fidelity

Current Dispatch stores one type, optional target, participants, due tick, risk and final result.

Frozen design needs:
- mandatory primary objective;
- hard constraints;
- optional secondary objectives;
- risk tolerance;
- abort conditions;
- priority ordering;
- partial/aborted/delayed/captured/injury/death/disobedience outcomes.

**Reconciliation:** expand the record and resolver; preserve deterministic result-oriented simulation.

## B12. War model is a single-objective proof, not a continuous campaign model

Current War has one ObjectiveType/Target and terminal resolution.

Frozen design supports:
- multiple campaigns/fronts;
- changing objectives;
- participant entry/exit;
- command intent;
- commander autonomy;
- long-running war continuity.

**Reconciliation:** War remains the parent persistent state; add fronts/objectives/orders/campaign events beneath it.

## B13. Army model over-relies on one EffectivePower field

Current Army contains headcount, one EffectivePower number, commanders and state.

Frozen strategic resolver needs capability vectors covering Rank bands, mobility, reach, logistics, morale/cohesion where relevant, siege capability, special counters, terrain compatibility, named-character intervention and objective state.

**Reconciliation:** keep summary power only as a projection/debug convenience, not authoritative combat truth.

## B14. Generic Project model is currently linear-time only

Current project progress is calculated directly from start tick -> resolve tick.

Frozen design permits prerequisites, phases, interruptions/failure conditions, specialist assignments and non-linear project logic.

**Reconciliation:** retain lazy/event-driven progression but add optional phase/state-machine definitions.

## B15. Civilization development / logistics / world-specific time are absent

No runtime state yet covers:
- civilization-specific development vectors;
- genre-preserving advancement;
- knowledge vs reproduction capacity;
- logistics routes/capabilities;
- world calendars/time ratios;
- independent decline/recovery.

**Reconciliation:** add compact strategic records rather than fine-grained simulation.

## B16. Location accessibility must not become a universal boolean gate

`FOGLocationRecord::bPhysicallyAccessible` is acceptable as a coarse structural fact only.

Frozen exploration says access depends on the attempting entity's actual capabilities (flight, pressure resistance, teleportation, Authority, etc.).

**Reconciliation:** capability-aware traversal query decides access; no global boolean may implement "come back later" gates.

## B17. Knowledge state lacks belief provenance/uncertainty/language

Current `knowledge_facts` can store owner-specific JSON, which is a useful base, but no first-class support exists for:
- source/provenance;
- confidence/uncertainty;
- known-false vs believed-false distinction;
- rumor propagation;
- language/script knowledge.

**Reconciliation:** extend semantic fact state while keeping objective truth separate from belief.

## B18. Mature-content runtime state is not implemented

Beyond maturity metadata, there is no first-class:
- libido/preferences profile;
- contextual adult-content state;
- Version/Factor/body-state integration;
- systemic scene compatibility graph;
- archive state;
- Privacy/SFW presentation mask.

**Reconciliation:** implement as character/content/presentation data, not as combat hard-code.

## B19. Android Reports notification bridge is absent

No Android notification/WorkManager bridge exists in current source.

**Reconciliation:** add local notification service backed by authoritative Reports; world state must remain correct even if Android suppresses background execution.

## B20. Content-package runtime is too small for the frozen delivery model

Current package persistence tracks ID/version/hash/installed/validated/activated/manifest JSON.

Missing:
- install location;
- dependency activation enforcement;
- sealed state;
- hot/cold state;
- external-storage placement;
- update/download state;
- package-level media/region categories.

**Reconciliation:** extend package registry and activation validation.

## B21. Save-recovery runtime is only the first layer

Current snapshot service:
- creates ordinary SQLite snapshots;
- retains newest N;
- current GameCore uses 3 snapshots in app-private Saved data at startup.

Frozen design additionally requires:
- external user-visible protected backups;
- manual Export;
- import/recovery UX;
- migration-on-copy with old save preserved;
- explicit archive retention independent of Clear World;
- optional secondary/cloud archive.

**Reconciliation:** retain SQLite online backup primitive; build the higher recovery layer around it.

## B22. Heroic Record / heroification state is absent

No runtime model yet supports recording a dead significant world character into a later gacha-accessible Identity Pattern while preserving canonical death.

**Reconciliation:** add content/state representation when first narrative/gacha content needs it.

## B23. NPC promotion / language / dialogue architecture is absent

No current subsystem implements background-NPC promotion, semantic memory, language/script knowledge or offline hybrid free-text/structured dialogue.

**Reconciliation:** add data contracts after core character/world schemas are normalized.

## B24. Equipment/inventory physical-state model is absent

Current combat units do not yet expose the frozen physical equipment/inventory/history/affinity model.

**Reconciliation:** add equipment/item entities and storage capability without introducing loot-bloat schemas.

---

## B25. Package dependencies are validated syntactically but not enforced at activation

`FOGContentManifestValidator` checks dependency IDs/minimum versions structurally, but `SetContentPackageActivated` currently only checks the package's own installed/validated flags. It does not prove required dependency packages are installed, active and at sufficient versions.

**Reconciliation:** dependency resolution becomes part of package activation with cycle detection, minimum-version checks and deterministic error reporting.

## B26. Orientation is automatic, but runtime orientation lock is not implemented

`DefaultEngine.ini` currently uses Android `Orientation=Sensor`, which is compatible with automatic portrait/landscape switching, but the frozen player-facing manual orientation lock does not yet exist as a runtime setting/control.

**Reconciliation:** retain automatic sensor behavior as default and add an explicit player lock that Ruler/World Mode transitions respect.

# C. P2 - cumulative-baseline supersession / documentation normalization

These do not all mean the latest design is unclear; many are historical text that must be explicitly normalized so engineers/tools do not read the wrong checkpoint.

## C1. Old Overlord definition is superseded

Older cumulative sections say Overlord is specifically associated with subordinate Rulers and may have further universal milestone titles.

Current rule:
- Ruler and Overlord are the only universal sovereignty titles;
- Overlord is highest;
- Overlord requires both personal/Authority capability and Overlord-scale sovereignty;
- one world/dimension-spanning Territory can qualify without a subordinate-count requirement.

**Action:** mark/remove stale X.6/AL.2 wording in the normalized master.

## C2. Old Grand-Class model is superseded

Older AK text treats Grand primarily as a mastery tier and contains no Crown tier / unique seat.

Current AV / `CLASS_ARCHITECTURE.md`:
- Crown = non-unique apex ordinary mastery;
- Grand = one active metaphysical seat per canonical Class.

**Action:** mark AK.2 Grand wording superseded.

## C3. One stale Rank sentence still says "Common Rank"

The current ladder begins at **Mortal**, but the cumulative AN section still contains the sentence "Ordinary people may occupy different levels within Common Rank."

**Action:** replace Common -> Mortal in the normalized master.

## C4. "save-specific" protagonist wording is stale

Older text describes protagonist Transcendence proofs as "save-specific."

Current rule is one persistent world/history, not alternate saves.

**Action:** use "persistent-world personal proofs/history-specific conditions."

## C5. Mature-content terminology has been superseded again

Older cumulative sections still reference:
- adult-content consent/eligibility;
- voluntary-participation wording;
- relationship state supporting adult-content consent;
- Version-level eligibility language.

Newest rule:
- canonical lore determines Adult/NonAdult;
- appearance is never a factor;
- Adult identities are adult-content capable by default;
- preferences/libido/state shape content rather than becoming a generic access gate.

**Action:** normalize all older mature-content language in the next cumulative master.

## C6. Duplicate-gacha wording is already correctly marked SUPERSEDED

The cumulative T.6 section now explicitly marks the old duplicate-conversion model superseded and describes repeat Manifestations/Grand Convergence.

**Action:** no design decision needed; normalized master can remove the obsolete historical paragraph after audit.

## C7. Runtime architecture document contains stale hard-simplification wording

`docs/ARCHITECTURE.md` currently says Faction/War has "no politics, treaties..." and Knowledge has "no separate generic NPC-memory simulation."

The frozen architecture does allow:
- compact explicit diplomatic agreements/treaties;
- meaningful named-NPC semantic beliefs/memory;
while still rejecting a detailed grand-strategy politics simulator and universal day-life memory simulator.

**Action:** narrow the wording so simplification cannot be misread as forbidding the finalized systems.

---

# D. Confirmed foundations that should be preserved

The audit also identifies code that already matches the frozen architecture and should not be casually rewritten:

- SQLite as authoritative mutable state;
- numbered immutable migrations;
- transactional multi-fact writes;
- deterministic RNG/provenance;
- large-number combat representation;
- shared turn/action combat rule foundation;
- action-party local Identity exclusivity;
- knowledge separated from objective location truth;
- Domain Core zero-durability -> Broken and Broken cannot be captured;
- project lazy progression concept;
- aggregate population/army foundations;
- content package hash/version/activation basics;
- SQLite online backup primitive;
- no IAP/payment code present in current runtime.

---

# E. Reconciliation order produced by the audit

Do **not** compile yet.

Recommended implementation order after the remaining audit/tuning pass:

1. normalize authoritative docs and remove/mark stale cumulative contradictions;
2. fix guaranteed static compile blockers in tests/contracts;
3. migration 0007+: multiple Manifestations + remove duplicate-counter behavior;
4. Ruler/Territory control continuity + one-month gacha gate + anchoring;
5. Domain-heart/fusion schema;
6. Rank / Factor / Class / character-route normalized state;
7. shared combat Identity-exclusivity + Rank hooks;
8. World time / World Director / offline event foundation;
9. Dispatch/war/project/civilization/logistics extensions;
10. mature-content/profile/privacy state interfaces;
11. NPC promotion / knowledge-belief-language-dialogue state;
12. package/storage/backup/Android notification layer;
13. UI presentation contracts;
14. rewrite vertical slice and automation expectations;
15. static preflight -> real UE build/test gate.

## Pass-1 verdict

The current codebase is a useful **pre-detailing proof-of-architecture**, not a throwaway.

The persistence, deterministic core and several low-level services can be evolved in place.

However, the gacha/character/territory integration and several tests encode superseded assumptions strongly enough that the first real UE compile should wait until reconciliation, as previously decided.


# F. Pass 2 normalization results

Pass 2 completed the following without compiling/running Unreal:

- reconciled `PROTAGONIST_PROGRESSION.md` to the resolved Class/Factor/Rank/Transcendence architecture;
- resolved the dimensional-preparation mechanism as semi-conscious in a metaphysical/homeostatic sense;
- normalized `DATA_MODEL.md` so migration-0006 is explicitly pre-reconciliation and adulthood is lore-authoritative;
- rewrote recovery documentation around untouched pre-migration preservation;
- rewrote `VERTICAL_SLICE.md` as the frozen target contract;
- marked the current G2 harness as stale proof infrastructure rather than certification;
- removed remaining second-gate mature-content terminology from the dedicated mature-content contract;
- produced the exact numbered schema/API plan in `RECONCILIATION_MIGRATION_MATRIX.md`.

## Remaining audit work before code edits

The design contradictions are now sufficiently classified to begin **cumulative-master normalization and numerical-tuning audit**.

No additional creative questionnaire is required.

The next implementation-affecting deliverables are:
1. normalized cumulative master baseline;
2. deterministic tuning assumptions/parameter registry;
3. code migration implementation following the matrix;
4. test rewrites;
5. only then the mandatory Unreal validation sequence.
