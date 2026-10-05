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
- Full installed-content storage policy is governed by `docs/STORAGE_QUALITY_BUDGET.md`.
- Preferred standard-quality full install: **25-35 GB**.
- **35-50 GB** is acceptable only when measured production content justifies it without avoidable duplication.
- **50 GB** is the soft warning threshold: new content must trigger an explicit storage review.
- **60 GB** is the hard installed-content ceiling unless the Creative Director explicitly approves an exception backed by physical Galaxy S26 Ultra profiling and a documented quality/content justification.
- The former 60-80 GB normal target / ~100 GB upper bound is superseded and must not be used for planning.

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

Do not tune one fixed pull-equivalent/hour target for the entire persistent game.

Use **progression/source bands** as simulator scenarios, not account-level multipliers:

- newly qualified Ruler: roughly **4-8 pull-equivalents/hour** during acquisition-focused play;
- established Ruler/Domain: roughly **7-14/hour**;
- ordinary Overlord-scale economy: roughly **14-27/hour**;
- extreme late-game Overlord / dedicated high-order farming: roughly **27-52+/hour** where actual content/sovereignty/resource production justifies it.

These bands count ordinary currency plus the expected acquisition value of tickets and burst rewards. They are not guaranteed wages.

At 12/h, the current candidate banner's mean featured cost is about 5.1 hours of equivalent acquisition-focused value; at 25/h about 2.4 hours; at 50/h about 1.2 hours. Late-game abundance is acceptable because duplicate Manifestations remain valuable, route breadth/Convergence creates long-term demand, and the game has no monetization incentive to preserve artificial scarcity.

Low-level sources keep their authored absolute payout. High-level players get richer because they unlock/own stronger economic systems and dedicated high-order farming content, not because an invisible player-level multiplier rewrites old rewards.

The intended economy should be bursty and world-causal: exploration, bosses, story, discoveries, events, Territory/Domain development, challenge realms, high-order incursions and major achievements can give large bursts; generic repeatable low-level farming should be relatively inefficient.

### Ticket value accounting

For economy simulation, measure tickets in **pull-equivalent expected value** without displaying that abstraction to the player.

- a standard one-pull ticket = 1 normal pull-equivalent;
- a ten-pull ticket = 10 normal pull-equivalents;
- event tickets use their compatible pool's expected value;
- guaranteed-rarity/selector tickets use their actual restricted-pool expected value and are therefore worth more than one ordinary pull when appropriate.

Ticket EV is used only for economy comparison; gameplay preserves the ticket's actual special behavior.

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

### Character-specific Rank response

For magnitude-like stats, use a shared Rank scale plus a **character/stat-specific Rank response** rather than one common multiplier.

Working equivalent forms:

`ResolvedStat_s = Base_s × R(r) × G_s(r,l,build)`

or

`ResolvedStat_s = Base_s × R(r)^alpha_s × L_s(r,l) × B_s(r) × Build_s`

where:

- `R(r)` = common existential Rank coefficient;
- `alpha_s` = character-specific elasticity for stat s;
- `L_s` = character-specific within-Rank Level growth;
- `B_s(r)` = sparse character/content breakthrough bias for exceptional Rank transitions;
- `Build_s` = Factors, equipment, forms, skills, route state and temporary effects.

The two forms are equivalent when `G_s` contains `R^(alpha_s-1)`.

### Alpha domain restriction

Here **D+ means positive decimal domain**, not Divine Rank and above.

For every stat using the Rank-elasticity model at every Rank, the authored character-specific alpha must be a positive decimal within:

**1.10 <= alpha_s <= 2.50**

This is a universal tuning-domain restriction on alpha itself, while the actual alpha value remains independently authored per character/stat/build. A character may have HP alpha 2.35, ATK alpha 1.42 and DEF alpha 2.10 while another has the inverse.

At Primordial's current candidate R≈4.37e16, this alpha envelope permits enormous divergence:
- alpha 1.10 -> about 2.02e18 scale contribution;
- alpha 1.50 -> about 9.1e24;
- alpha 2.00 -> about 1.9e33;
- alpha 2.50 -> about 4.0e41.

The large-number combat representation is expected to support these magnitudes.

Thus same-Rank characters may differ by many orders of magnitude in individual stats while remaining inside the same existential band.

Do **not** assign one alpha profile by generic RPG role and reuse it across the roster. Each character/Manifestation starts from authored character data; route evolution, Factors, forms and major transformations may change the response profile itself.

Probability/rate/action-frequency stats can use character-specific response functions rather than blindly applying the magnitude formula. Crit/Hit/Dodge/Block/Speed may still become enormous where intended, but the runtime may analytically batch overflow effects/actions for performance instead of simulating billions of identical instances.

### Within-Rank Level / breakthrough split

Accepted target:
- approximately **35% of each Rank step's logarithmic growth** is distributed through Levels 1-100;
- approximately **65%** is delivered by the actual breakthrough into the next Rank.

The Level 1-100 curve is character-specific, not universally linear. Each authored curve is normalized to the shared 35% envelope.

Character/stat-specific breakthrough biases may exceed the smooth Rank projection when the actual transformation/lore warrants it.

### Negative DEF / penetration candidate

The positive-DEF family remains:

`DEF >= 0: M = R / (R + DEF)`

To preserve unbounded large-damage potential without the singularity produced by extending that denominator through negative DEF, test an exponential negative-DEF branch:

`DEF < 0: M = B^(-DEF / R)`

where `R` is the skill/content defense reference and `B` is an authored negative-defense growth base. The accepted generic baseline is `B = 2`; skills/characters may explicitly author another base/reference.

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

### GP / Power presentation constraint

GP/Power remains a non-authoritative summary estimate.

A logarithmic representation may be used **internally** to combine extreme values safely, but the raw logarithm is never shown as GP. Player-facing GP must be converted back into the normal large-number/suffix presentation so it remains intuitively magnitude-relative to the underlying stats.

Exact weighting/calibration remains simulator-tunable.

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

## Frozen tuning decisions - post questionnaire

### Rank / Level growth

Freeze the current universal Rank coefficient candidate:

`R(r) = 10^(0.40r + 0.040r²)`

with Mortal r=0 through Primordial r=16.

For the ordinary Rank-step envelope:
- approximately **35%** of the logarithmic Rank-step gain is expressed through Levels 1-100;
- approximately **65%** is reserved for the qualitative breakthrough into the next Rank.

The Level 1-100 curve is **character-specific**, not one shared shape. Content must still remain inside the expected Rank envelope unless a real mechanic explicitly breaks it.

Sparse breakthrough-specific stat biases are allowed to be dramatic when the transformation/lore warrants it.

### Negative DEF and Rank Suppression

Freeze the generic negative-DEF baseline:

`DEF < 0: multiplier = 2^(-DEF / DefenseReference)`

Character/skill-specific rules may author another exponential base/reference.

Freeze the first generic lower->higher Rank-Suppression envelope as:
- gap 0: 100%;
- gap 1: 100%;
- gap 2: 75%;
- gap 3: 40%;
- gap 4: 15%;
- gap 5+: 3%;

with separate channel tuning for damage, control, effect penetration, perception, presence/environment tolerance and resistance breaking.

### GP / Power

GP remains a visible estimate only.

**Implementation rule:** compute GP in log-space internally to avoid overflow, but do **not** display the logarithm itself.

Use a magnitude-preserving estimate:

`log10(GP) = weighted aggregate of log10(resolved capabilities) + kit/rank/equipment terms`

then convert that result back into the same large-number display family used by combat/stat values.

Therefore a character with quadrillion-scale or far larger actual capabilities does not show a trivial GP such as "32" merely because logarithms are used internally. Log-space is only an implementation technique.

GP may still mis-rank unusual matchup-specific kits, which is intentional and already established.

### Gacha acquisition income bands

Increase the progression-source simulation bands slightly:

- newly qualified Ruler: about **4-8 pull-equivalents/hour** during acquisition-focused play;
- established Ruler/Domain: about **7-14/hour**;
- ordinary Overlord-scale economy: about **14-27/hour**;
- extreme late-game Overlord / dedicated high-order farming: about **27-52+/hour**, with no hard ceiling where future content genuinely supports more.

These remain source/content bands, not automatic wages or hidden account-level multipliers.

### Strategic/offline

The normal newly-generated offline irreversible-loss governor may use roughly **15% of current sovereign strategic weight as a ceiling per catch-up resolution**.

This is a **maximum safety bound, not a target loss**. Reconnecting does not cause a routine 15% loss. Most catch-ups may produce no territorial loss at all.

Only actual causally generated events can consume part of that budget. Already causally locked consequences remain exempt from the newly-generated-loss ceiling.

### World time

Earth-like day/night pacing is not a universal default.

Use familiar day/night pacing only for Territories/Worlds whose actual temporal/environmental rules support it. Other realities may use radically different day lengths, cycles, calendars or no ordinary day/night cycle at all.
