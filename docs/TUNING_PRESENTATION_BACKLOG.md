# Remaining Tuning / Presentation Backlog

STATUS: ACTIVE. Architecture is frozen; these are tuning/content-presentation decisions before code reconciliation.

## 1. Rank / Level / stat progression
- Universal Rank coefficient candidate remains under simulation.
- Per-character/per-stat alpha is a positive decimal in [1.10, 2.50] at every Rank.
- Still tune within-Rank Level growth, breakthrough share, sparse breakthrough biases and route/form/Factor changes to stat response.
- Same-Rank individual stats may differ by many orders of magnitude.

## 2. Combat math
- Tune negative-DEF exponential base/reference.
- Tune Rank Suppression separately by interaction channel.
- Tune Crit/Hit/Dodge/Block overflow behavior and extreme-value batching.
- Tune effect grades, stacking/duration, debuff application/resistance, HP-loss caps, charge/readiness and enemy AI priorities.
- Tune encounter-specific TTK envelopes without imposing one universal TTK.

## 3. Affinity families
Affinity must not become one overloaded universal meter.

### Equipment-owner affinity
Existing design remains: long-term use can improve performance, unlock functions or drive evolution; transfer preserves item history while owner-specific affinity may partially reset/rebuild according to the item's mechanics.

Tune:
- whether affinity uses hidden continuous state, authored milestones or both;
- gain/decay/transfer behavior;
- how proficiency differs from affinity;
- item-specific unlock/evolution thresholds.

### Character relationships
Do not use one universal affection score. Persist semantic relationship/history state and only add numeric axes where a real mechanic needs them.

Adult-content access is never controlled by relationship affinity.

### Elemental / conceptual / biological / world affinity
Use content-defined compatibility/resistance relationships, not a mandatory universal rock-paper-scissors element chart.
Tune multipliers/grades per system and allow explicit abilities to override them.

## 4. Gacha economy / tickets
- Tune base rates/pity after deterministic simulation.
- Tune progression-specific currency sources rather than one global income multiplier.
- Tune standard/multi/event/guaranteed/selector ticket reward cadence.
- Tune high-level dedicated farming and Territory/Domain/Overlord production.
- Model ticket expected value for economy only; preserve actual ticket behavior in gameplay.

## 5. Equipment / crafting economy
- Tune item quality distribution, modifier counts/strengths, durability impact, repair/forging costs, evolution costs, proficiency and affinity.
- Preserve deterministic item identity and avoid random-affix loot spam.
- Tune historical/named-item emergence and transfer economics.

## 6. Strategic simulation
- Tune Project duration/acceleration/interruption.
- Tune Dispatch risk/outcomes.
- Tune route capacity/blockade/logistics.
- Tune war attrition, siege and capability resolution.
- Tune civilization vector growth/decline and high-level resource generation.
- Tune offline compression error against full-resolution reference.

## 7. World time / Director / notifications
- Tune per-world calendars and time ratios as content.
- Tune Director earliest/preferred/latest windows.
- Tune offline consequence weight budget.
- Tune Android notification thresholds/categories/quiet behavior without making notifications authoritative.

## 8. UI numerical presentation
The stronger Rank/stat model supersedes the old assumption that late-game values would remain only in B/Q-scale ranges.

Tune:
- on-field significant digits;
- suffix vs scientific/engineering notation;
- exact-detail display;
- HP/resource bar normalization;
- damage-number aggregation for extreme multi-hit/overflow;
- stat comparison deltas;
- knowledge-limited exactness for unknown entities;
- whether GP/Power remains visible and, if so, how its logarithmic/uncertain estimate is calculated.

## 9. Ruler Mode / character UI presentation
Architecture is fixed; tune density and interaction:
- character Identity -> Manifestation grouping;
- stat/progression/route comparison screens;
- equipment affinity/history presentation;
- Territory/Domain/Dispatch/Project/map information density;
- Reports/Chronicle/Codex filters;
- gacha ticket/currency/pity presentation;
- notification badge behavior.

## 10. World Mode presentation
Tune:
- FOV/camera distance;
- lock-on transition speed;
- camera shake/hit-stop/motion blur;
- damage-number lifetime/stacking;
- HUD fade timing;
- status icon density;
- target information precision;
- QTE/perfect-dodge feedback windows;
- haptic intensity patterns.

## 11. Turn-combat presentation / auto
- Tune 1x/2x/additional speed options.
- Tune animation compression vs readability.
- Tune auto-battle priority rules and conditional presets.
- Tune reserve-entry delay and timeline visualization.

## 12. Art / VFX / animation / audio
Architecture is fixed; production tuning remains per content:
- material/lighting/render variation by World;
- VFX density/readability/performance;
- reality-field collision visualization;
- animation cancel windows and hit timing;
- cinematic camera language;
- Japanese voice coverage details;
- mix loudness, dynamic range, leitmotif transition timings and adaptive-music states.

## 13. Mature-content presentation
Architecture is frozen; tune character-specific production variables such as libido/profile representation, scene pacing, animation compatibility, camera/presentation settings and Privacy/SFW masking. No generic adult-access gate is introduced.

## 14. Mobile budgets
Hard actor/VFX/streaming/memory/thermal budgets are deferred until physical S26 Ultra profiling. The design may be implemented through streaming/tiering/aggregation rather than cut.

## Recommended tuning order
1. Rank/Level/stat growth + combat math.
2. Affinity families + equipment.
3. Gacha/tickets/economy.
4. UI/large-number presentation.
5. Turn/action presentation.
6. Strategic timing/economy.
7. World Director/time/offline.
8. Physical device presentation/performance budgets.
