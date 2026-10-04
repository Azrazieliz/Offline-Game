# Detailed Finalization Register

STATUS: BROAD ARCHITECTURE FROZEN FOR TUNING AND CODE RECONCILIATION.

A system is design-final when its player-facing rules, authoritative state transitions, content-authoring contract, failure/edge behavior and UI exposure are specified. Numerical values remain provisional until deterministic simulation or S26 Ultra profiling validates them. Ongoing content catalogs remain open-ended by design.

## Creative/system architecture - resolved

- Ruler/Overlord sovereignty: `RULER_PROGRESSION.md`.
- Class/Crown/Grand architecture: `CLASS_ARCHITECTURE.md`.
- Existence/Power Rank: `EXISTENCE_POWER_RANK.md`.
- Protagonist Factors: `PROTAGONIST_FACTORS.md`.
- Protagonist Transcendence / personal World Manifestation / Domain combination: `WORLD_FANTASM_TRANSCENDENCE.md`.
- Character progression / divergent Manifestations / Grand Convergence: `CHARACTER_PROGRESSION.md`.
- World Director: `WORLD_DIRECTOR.md`.
- Cosmology / World Rank / Junctions / existential-threat ecology: `COSMOLOGY.md`.
- World generation / exploration / persistent locations / causal restoration: `WORLD_GENERATION_EXPLORATION.md`.
- Civilization / logistics / Projects / Dispatch / population / world time: `CIVILIZATION_LOGISTICS_TIME.md`.
- Territory / Domain Core / five-day reclamation / Core fusion: `TERRITORY_PROJECTS.md`.
- Factions / armies / continuous war / command: `DISPATCH_FACTION_WAR.md`.
- Ruler Mode / World HUD / controls / art / VFX / animation / voice / music: `UI_PRESENTATION.md`.
- Mature/adult-content production and state integration: `MATURE_CONTENT.md`.
- Equipment / skills / NPC generation-promotion / dialogue / knowledge / language / memory: `CHARACTER_SYSTEMS_PRODUCTION.md`.
- Gacha economy / copy behavior / Heroic Record heroification: `GACHA_ECONOMY.md`.
- Persistence / backups / content packages / Android delivery: `PERSISTENCE_PACKAGING.md`.
- Production cadence / Worlds / bosses / challenge content / narrative continuity / device-feasibility doctrine: `CONTENT_PRODUCTION_NARRATIVE.md`.
- Freeze status and handoff to reconciliation: `ARCHITECTURE_FREEZE.md`.
- Reconciliation migration/API matrix: `RECONCILIATION_MIGRATION_MATRIX.md`.
- Tuning / detailed UI freeze: `TUNING_UI_FREEZE.md`.
- Audio / music / voice / SFX production direction: `AUDIO_PRODUCTION_DIRECTION.md`.
- Visual art / VFX / animation / cinematic production direction: `VISUAL_ART_DIRECTION.md`.
- Tuning parameter ownership/versioning contract: `TUNING_PARAMETER_REGISTRY.md`.

## Numerical tuning and deterministic simulation - preference pass closed; simulator fitting remains

These values are intentionally not frozen by questionnaire and should be tuned with deterministic simulators and later physical-device validation:

- validate the accepted standard-banner candidate, progression-specific income bands and ticket cadence;
- progression costs, Current Rarity reinforcement, Grand Convergence costs;
- validate accepted DEF/negative-DEF behavior, Rank Suppression envelope, 35/65 Level/breakthrough split, SPD/action-value timing and AI priorities;
- economy/resource/project/war/dispatch timing, risk and output;
- equipment quality/modifier distributions, evolution/forging costs;
- Transcendence/Rank-breakthrough thresholds;
- challenge-mode growth curves;
- Android performance/streaming/VFX/actor budgets.

## Production/content - open-ended by design

- roughly 2-3 fully polished playable characters per week after the pipeline is mature;
- complete kits/progression/animation/voice-tier/mature-content support for produced major characters;
- character-specific World Fantasms;
- Domain/Core Concept content;
- NPCs, monsters, bosses, factions, locations, dungeons, equipment, materials, Factors, Classes and story events;
- starting World depth plus parallel later-World seeding;
- opening and major narrative arcs (spoiler-protected from the Creative Director unless explicitly requested);
- per-character action-combat animation/cancel/QTE/Ultimate content;
- per-boss hybrid mechanics;
- music/audio assets from the frozen composition/audio direction.

## Engineering reconciliation - ready to execute

- contradiction/schema/API comparison: **audit pass 2 complete**;
- numbered immutable migration/API design: **complete in `RECONCILIATION_MIGRATION_MATRIX.md`**;
- replace stale duplicate-counter gacha persistence with multiple Manifestation instances;
- rewrite stale vertical-slice assumptions;
- implement/reconcile Rank, Factors, Classes, progression routes, Territory control/reclamation, Core heart/fusion consequences, World Director, World Rank, UI, mature-content state, NPC/dialogue state and package delivery as needed by the first integrated build;
- rewrite/add deterministic tests against frozen behavior.

## Validation gate after reconciliation

1. static Unreal C++ preflight;
2. UE 5.8 UHT/UBT/MSVC/link compile;
3. OfflineGame automation tests;
4. Android cook/package/install;
5. physical Samsung Galaxy S26 Ultra profiling;
6. optimize by streaming/tiering/abstraction without materially changing the player-facing design.


## Detailed UI checkpoint

Global roster/Character Identity/Gacha/Territory/Records/combat/save/package UI preferences are frozen in `TUNING_UI_FREEZE.md`. Do not re-run that questionnaire unless explicitly revised.


## Pre-coding finalization status

The independent pre-coding audit and production-direction passes are complete.

No remaining broad Creative Director architecture/design questionnaire blocks coding.

Closed after the earlier freeze:
- higher-order progression persistence for Transcendence, Manifestation World Fantasm and protagonist personal World Manifestation;
- equipment affinity/proficiency persistence;
- presentation/wardrobe and management-metadata persistence boundaries;
- canonical-world versus device/profile settings separation;
- stale inherited documentation normalization;
- audio/music/voice/SFX production direction;
- visual/VFX/animation/cinematic production direction.

Remaining non-code work is deliberately content production or evidence-driven tuning and does not block architecture reconciliation.
