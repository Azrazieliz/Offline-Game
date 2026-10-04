# Numerical Tuning Register

STATUS: TUNING PASS STARTED. Architecture is frozen. Values are classified as fixed rules, initial deterministic baselines, or deferred content/device tuning.

No Unreal compile or automation test has been run.

## Fixed numerical rules

- 10,000 basis points = 100%.
- Existence Rank uses the current 17 named bands, each with Levels 1-100.
- Territory reclamation grace = 5 in-game days.
- First gacha unlock requires more than one in-game month of continuous valid Territory control; the unlock is then permanent.
- Turn roster = up to 18 characters per side, with 6 active and two successor layers.
- Current World Mode action party baseline = protagonist + up to 2 switch/QTE companions.
- Crit Rate overflow conversion = +1% effective Crit Rate above 100% -> +2% Crit Damage.
- Hit overflow = each full +100% effective Hit creates one additional guaranteed eligible hit instance; remainder is probability for another.
- Direct offensive percent-MaxHP damage is not ordinary attack math.
- Generic perfect-dodge slow target remains 2-3 seconds.
- Android performance profiles remain 30 / 60 / 120 FPS.
- Normal installed-content target remains roughly 60-80 GB, with approximately 100 GB as a practical upper operating bound before stronger archive/eviction pressure.

## Gacha - first deterministic baseline

Standard limited banner starting point:

- base top-Origin-Rarity probability: 1.2% per pull;
- soft pity begins after pull 50;
- pulls 51-69 add +5 percentage points of top probability per pull;
- pull 70 is hard pity;
- single-feature top result uses 50% featured / 50% off-feature;
- after an off-feature top result, the next qualifying top result is guaranteed featured;
- compatible pity/guarantee carry remains;
- ten-pull floor remains at least SR+ unless a banner defines a stronger floor.

Exact discrete distribution for that schedule:

- mean pulls to any top rarity: 40.55;
- median: 52;
- mean pulls to featured with 50/50 + guarantee: **60.82**;
- median featured: 56;
- 75th percentile featured: **83**;
- 90th: **108**;
- 95th: **112**;
- 99th: **116**;
- absolute featured ceiling: 140.

This is a simulation baseline, not yet a frozen economy value.

Featured expectation check: because the first qualifying top result is featured with probability 0.5 and an off-feature result makes the next qualifying top guaranteed, expected featured cost is exactly 1.5 times the expected top-rarity cycle length for a single-feature banner under this baseline.

Currency income must be tuned against pull counts using pulls per active-hour band, pulls per in-game month, Territory/Domain passive income, one-time exploration/story/boss income, repeatable income, and offline/catch-up income. Do not freeze a real-world weekly quota because the World Director is world-time/state driven.

First revised income target for simulation: **6-9 pull-equivalents per active gameplay hour averaged across a mature save**, counting burst rewards and renewable sources together rather than paying a flat hourly stipend.

At the current featured distribution:
- 6/h -> mean featured about 10.1 h, median about 9.3 h, 95th about 18.7 h, hard ceiling about 23.3 h of equivalent value;
- 8/h -> mean about 7.6 h, median 7.0 h, 95th 14.0 h, hard ceiling 17.5 h;
- 9/h -> mean about 6.8 h, median about 6.2 h, 95th about 12.4 h, hard ceiling about 15.6 h.

The intended economy should be bursty and world-causal: exploration, bosses, story, discoveries, events, Territory/Domain development and major achievements can give large bursts; generic repeatable farming should be lower. Because there is no IAP, economy tuning should preserve excitement without importing frustration-based monetization pacing.

## Combat baselines

### Per-character stat doctrine

All gameplay stats are authored/resolved per character/Manifestation/build. The combat engine may use mathematical neutral fallbacks only for invalid/missing test data, but production balance never assumes every character starts from the same Crit Damage, Hit, Dodge, Block, SPD, resistance or other stat.

Skills/forms/equipment/Factors/Rank/effects may modify those resolved values. This keeps stat identity part of character design rather than a hidden universal template.


- **No universal character-stat baseline is authoritative.** Crit Damage, Crit Rate, Hit, Dodge, Block, HP, ATK, DEF, SPD, resistances and every other character-facing stat are explicit character/Manifestation content state and may differ radically by character/build.
- Retain DEF ratio family: defense reference / (defense reference + effective DEF), with the relevant defense reference supplied by skill/content data rather than one global stat constant.
- DefenseReference remains skill/content data rather than a universal ATK constant.
- No undeclared random damage variance.
- Use 2.5 seconds as the generic perfect-dodge slow baseline inside the accepted 2-3 second range; character-specific mechanics may differ.
- Perfect dodge has no universal reward proc.

### Negative DEF / penetration candidate

The positive-DEF family remains:

`DEF >= 0: M = R / (R + DEF)`

To preserve unbounded large-damage potential without the singularity produced by extending that denominator through negative DEF, test an exponential negative-DEF branch:

`DEF < 0: M = B^(-DEF / R)`

where `R` is the skill/content defense reference and `B` is an authored negative-defense growth base. First baseline sweep uses `B = 2`.

With B=2:
- DEF = -R -> x2;
- DEF = -2R -> x4;
- DEF = -5R -> x32;
- DEF = -10R -> x1024;
- deeper negative DEF continues without a hard ceiling.

Skills/characters specialized around defense destruction may author a different B/reference behavior. The large-number combat type handles extreme results; tuning should prevent accidental explosions without imposing an arbitrary damage ceiling.

## Rank power and suppression

Rank uses a **shared existential Rank coefficient plus character/stat-specific growth**, not one universal stat multiplier.

First stronger simulator candidate:

`R(r) = 10^(0.40r + 0.040r²)`

where Mortal is r=0 and Primordial is r=16.

Approximate coefficient checkpoints:
- Mortal = 1;
- Awakened = 2.75;
- Hero = 1.74e2;
- Sage = 1.00e3;
- Saint = 6.92e3;
- Legend = 5.75e4;
- Celestial = 1.00e8;
- Divine = 1.74e9;
- Cosmic = 3.63e10;
- Dimensional = 9.12e11;
- Immortal = 2.75e13;
- Eternal = 1.00e15;
- Primordial = 4.37e16.

Late single-step ratios therefore become much larger than early steps; Eternal -> Primordial is about x43.65 in the common existential coefficient.

For ordinary magnitude stats, the initial model is:

`ResolvedStat_s = CharacterBase_s × R(r) × CharacterRankFactor_s(r, build)`

where `CharacterRankFactor_s` is different for every stat/character/build and may itself evolve with Levels, Factors, equipment, forms and route state.

Probability/rate stats such as Crit Rate, Hit, Dodge and Block remain character-specific too; they should use stat-appropriate response/overflow semantics rather than blindly treating the existential coefficient as a literal percentage multiplier.

The Rank coefficient is therefore a shared scale contribution, while individual stat identity remains character-authored.

The coefficient curve must be strictly increasing and superlinear. Same/adjacent Rank interactions use ordinary mechanics rather than an extra categorical suppression layer, even though raw stat differences may already be very large.

Initial simulator sweep categories:

- effective gap <= roughly 1 ordinary Rank step: ordinary resolution;
- about 2: soft suppression;
- about 3: material suppression;
- about 4: strong suppression;
- >=5 or equivalent high-band coefficient gap: extreme/existence-level suppression without a bypass.

First channel-suppression sweep for lower -> higher interaction: approximately 100% / 100% / 75% / 40% / 15% / 3% at gaps 0 / 1 / 2 / 3 / 4 / 5+, subject to channel-specific retuning.

At high Ranks, use coefficient gap as well as ordinal distance.

Tune separate channels for physical damage, effect/debuff penetration, control, perception/sensing, presence/environment tolerance, and resistance breaking.

## Turn timing

Keep continuous action value rather than round phases.

Do not freeze a universal SPD-to-seconds relation until representative kits exist.

Required constraints:
- modest SPD investment must be legible without creating infinite turn loops;
- action advance/delay/extra actions remain powerful but respect explicit readiness/resource rules;
- reserve successors enter with a real action delay;
- interrupts/triggers remain deterministic and provenance-recorded.

## Territory / Domain / World Director timing

Fixed:
- 5-day reclaim grace;
- >1 in-game month first gacha qualification.

Every Director-controlled item authors earliest start, preferred window, and latest-start/delay budget when delay is permitted. The Director may vary activation inside the authored range but cannot delay indefinitely.

Offline consequence governor:
- a safe Main Domain/Core is not newly pushed to final irreversible destruction solely because the app was closed;
- most/all player Territory is not newly wiped by Director-generated offline chains;
- moderate damage, resource loss, injury, siege, blockade, temporary occupation and some peripheral loss may resolve causally;
- catastrophes already causally locked before logout may still resolve.

## Civilization / Projects / Dispatch

No universal Project duration or civilization speed is frozen.

Tune by actual capability and world time.

Required outputs:
- Project duration by complexity;
- specialist/infrastructure acceleration;
- interruption/restart cost;
- route capacity and blockade effect;
- Dispatch success/partial/abort rates by risk tolerance;
- offline-compression error versus full-resolution reference;
- civilization-vector growth/decline without convergence toward one genre.

## Equipment

Equipment quality and Item Rank remain separate.

Ordinary equipment generation keeps deterministic base identity with usually 1-3 additional modifiers. Avoid mandatory random-affix spam.

## Mobile performance

Initial Samsung Galaxy S26 Ultra targets:
- 60 FPS primary default;
- 120 FPS high-performance option;
- 30 FPS thermal/battery fallback.

Do not freeze hard actor/VFX/streaming budgets before physical profiling.

## Production cadence

The 2-3 fully polished playable characters/week goal is a mature-pipeline throughput target, not an early implementation deadline. If throughput misses target, improve tooling/reuse/automation rather than silently reducing character individuality.

## Next deterministic simulators

1. gacha + renewable-income;
2. Rank band / suppression interactions;
3. same-Rank combat TTK and crit/hit/block/DEF sweeps;
4. turn timeline / SPD / action-advance loop detection;
5. Dispatch / Project / logistics outcomes;
6. World Director delay/offline-loss invariants;
7. package/storage/streaming budget estimation.

Simulation outputs become versioned tuning data.