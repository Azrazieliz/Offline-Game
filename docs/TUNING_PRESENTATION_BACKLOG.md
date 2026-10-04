# Remaining Tuning / Presentation Backlog

STATUS: RE-AUDITED AGAINST CURRENT AUTHORITATIVE BASELINE. This file lists only genuinely unresolved global/system tuning. Character-specific authored content is not treated as a missing global rule.

## Already resolved - do not reopen

- Core combat stats: HP, ATK, DEF, SPD plus character-specific special stats.
- Crit order and overflow: target Crit Rate Resistance first; effective Crit above 100% converts 1% -> +2% Crit Damage.
- Hit/Dodge overflow: full +100% effective Hit bands create additional eligible hit instances; remainder is a probability for one more.
- DEF/resistance penetration may exceed 100% where rules permit.
- Separate percentage-modifier categories multiply; authored multipliers within a category also multiply unless explicitly additive.
- No universal Healing Done/Received stat.
- Regeneration baseline is Max-HP based and normally triggers on the affected character's turn unless authored otherwise.
- Ordinary healing caps at Max HP unless a character/effect explicitly creates shield/overflow/additional HP.
- Effect state primarily uses Stack Count + Turns; potency uses authored grades/names rather than one universal potency number.
- Effect stack limits, duration decrement, cleanse, transfer, reflection, copying, spread, detonation and permanence are effect-defined.
- Authority is separate from ordinary effect grade.
- No universal elemental weakness chart.
- Equipment affinity exists and is distinct from proficiency; it may improve performance, unlock functions or drive evolution. Transfer preserves history while owner-specific affinity may be reduced/rebuilt according to the item's own mechanics.
- Generic relationship/affection dimensions are not a system. Important relationships are authored/event-history/specific-bond/synergy state only.
- GP/Power is visible summary information only and never determines combat outcome.
- On-field damage text is compact and switches to abbreviated/scientific-style representation for large values; battle recap is analytical.
- Turn auto combat is required and supports tactical priorities/conditional rules; deterministic presets remain the reliable foundation.
- Turn presentation includes at least 1x and 2x speed; Ultimate/cinematic repeat behavior is independently configurable.
- Ruler Mode navigation, Records/Territory structure, Home philosophy, Android-notification policy, World HUD/minimap behavior, touch/gamepad accessibility, art direction, VFX language, HI3-style animation direction, cinematics, Japanese voice strategy and adaptive-music philosophy are frozen in UI_PRESENTATION.md.
- World Mode controls, targeting, dodge/parry/guard, cancel policy, party/switch/QTE, combat/traversal continuity and boss hybrid rules are frozen in WORLD_MODE_ACTION_COMBAT.md.
- Gacha tickets exist as secondary access items alongside the renewable main pull currency; standard/multi/event/guaranteed-rarity/special selector-style ticket definitions are data-driven.
- High-level gacha income scales through richer actual content/sovereignty sources, not an invisible player-level payout multiplier.
- Every character-facing stat is character/Manifestation/build specific.
- Rank-stat elasticity alpha is a positive decimal in [1.10, 2.50] for every stat using the elasticity model.

## Genuinely unresolved global/system tuning

### 1. Rank / Level growth
Need deterministic simulation for:
- final universal Rank coefficient curve;
- exact within-Rank Level 1-100 contribution;
- how much of each Rank step is gradual Level growth versus the breakthrough itself;
- default handling of sparse breakthrough-specific stat biases;
- regression tests proving late Rank steps remain superlinear while same/adjacent Rank combat stays mechanically meaningful.

These are system numbers; the actual per-stat alpha/base/build response remains character-specific.

### 2. DEF and Rank Suppression
Need deterministic simulation for:
- final negative-DEF branch and its default exponential base/reference;
- exact channel-by-channel Rank Suppression curves for damage, effect penetration, control, perception, presence/environment tolerance and resistance breaking;
- interaction between raw stat disparity and categorical Rank Suppression.

No universal damage cap is to be introduced.

### 3. SPD / action-value timing
Continuous action value is frozen, but still tune:
- exact SPD -> action-delay mapping;
- reserve-entry delay baseline;
- action advance/delay conversion;
- deterministic detection of accidental infinite action loops.

There is no artificial rule preventing an intentionally extreme speed character from taking many actions if the build genuinely produces that result.

### 4. Gacha probability / income / ticket cadence
Still tune:
- whether the current 1.2% / soft-50 / hard-70 / 50-50->guarantee candidate becomes final;
- exact renewable-income curves by progression/content source;
- ticket reward frequency and guaranteed-ticket rarity;
- high-level dedicated farming output;
- banner-category carry rules where categories differ;
- expected acquisition pace versus the mature 2-3-character/week production target.

Low-level and late-game incomes are intentionally different because their available content/economies are different.

### 5. Progression economy
Still tune character/content cost grammars for:
- Current Rarity reinforcement/promotion;
- Rank breakthroughs;
- Evolution/Awakening/Corruption/other route nodes;
- Transcendence;
- Grand Convergence;
- high-order Factor development/fusion;
- item Rank/evolution/forging.

These should not become one universal currency curve; simulation should establish useful cost bands and scarcity ratios that content then specializes.

### 6. Equipment affinity / proficiency numbers
The behavior is already defined; only numerical representation is open.

Need tune/choose implementation defaults for:
- proficiency gain curve;
- owner-item affinity gain/decay/transfer retention;
- how authored affinity milestones map to item unlock/evolution;
- whether a specific item's affinity is continuous, milestone-only or hybrid.

There should be no universal affinity = +X% damage formula. The item definition decides what affinity means mechanically.

### 7. GP / Power estimate
Visibility and non-authoritative status are resolved. Still need a stable estimate formula that:
- handles enormous stats without overflow;
- remains readable across Rank bands;
- incorporates kit/equipment/Rank without pretending to solve every matchup;
- can show Unknown/Estimated/ranges when knowledge is incomplete.

A logarithmic estimate is the current engineering preference, not yet frozen.

### 8. Strategic simulation coefficients
Architecture is resolved; numbers remain:
- Project durations/acceleration/interruption loss;
- Dispatch risk/partial/abort/injury/death calibration;
- logistics route capacity/risk/blockade effects;
- war attrition/siege/capability resolution coefficients;
- civilization-vector growth/decline rates;
- Territory/Domain resource generation rates;
- offline compression error tolerance.

### 9. World Director / offline numeric bounds
Behavior is resolved; still tune:
- event cadence ranges by content type;
- quiet-period/delay budgets;
- offline consequence strategic-weight budget;
- catch-up compression limits.

These are not creative presentation questions.

### 10. Large-number UI implementation
The presentation principle is already resolved: compact on-field text, abbreviated/scientific-style large values, analytical recap.

Only implementation defaults remain:
- exact threshold where Auto formatting switches from normal digits/suffixes to scientific/engineering notation;
- significant-digit count by context;
- aggregation threshold for enormous repeated hit instances;
- exact-detail inspection formatting.

These can be chosen during UI implementation and usability testing rather than reopened as architecture.

### 11. Per-device presentation/performance calibration
Art/VFX/animation/audio direction is frozen. Physical S26 Ultra profiling must determine:
- actor/component budgets;
- VFX density/LOD;
- streaming distances/cache sizes;
- memory budgets;
- 30/60/120 FPS tradeoffs;
- thermal fallback behavior;
- camera shake/motion-blur defaults only insofar as comfort/performance testing requires adjustment.

Character-specific cancel windows, QTE conditions, animation timings, boss telegraphs, music transitions, scene pacing and similar values are content authoring, not missing global tuning.

## Recommended closure order

1. Rank/Level curve + DEF/Rank Suppression.
2. SPD/action-value.
3. Gacha probability/income/tickets.
4. Progression-economy cost bands.
5. Equipment affinity/proficiency defaults + GP estimate.
6. Strategic/Director coefficients.
7. Large-number formatting defaults.
8. Implement reconciled code.
9. Physical-device presentation/performance calibration after the first working Android build.
