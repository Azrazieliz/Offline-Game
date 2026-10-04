# Tuning Parameter Registry Contract

STATUS: AUTHORITATIVE PRE-CODING CONTRACT.

Purpose: prevent simulator-tunable values from becoming accidental hard-coded architecture.

## 1. Classification

Every tunable value must be classified as one of:

- **FixedRule** - Creative/system rule that code/tests may rely on until explicitly revised.
- **TuningCandidate** - accepted starting value that deterministic simulation may change.
- **ContentAuthored** - value belongs to a character/skill/item/World/event definition.
- **DeviceProfiled** - value comes from physical S26 Ultra profiling or platform constraints.
- **Derived** - computed from other authoritative values; never independently tuned.

Each parameter records:
- stable parameter ID;
- class;
- value/type;
- unit/domain;
- valid range or invariant where applicable;
- owning system;
- source/version;
- rationale;
- simulator/test coverage;
- supersedes/superseded-by metadata.

## 2. Fixed rules currently safe to encode as invariants

- 10,000 basis points = 100%.
- Existence Rank uses the current 17 named bands with Levels 1-100, while rank definitions remain data-driven.
- Territory reclamation grace = five in-game days.
- First gacha unlock requires more than one in-game month of continuous valid Territory control and is permanent once earned.
- Turn battle supports up to 18 characters per side with up to six active and successor structure.
- World Mode immediate party baseline = protagonist + up to two switch/QTE companions.
- Effective Crit Rate above 100% converts at +1% -> +2% Crit Damage after Crit Rate Resistance.
- Hit overflow creates guaranteed additional eligible hit instances per full +100% band plus seeded remainder.
- Direct percent-MaxHP offense is not ordinary generic attack math.
- 30 / 60 / 120 FPS profiles remain supported presentation targets.
- No real-money/IAP gacha economy.
- Repeat character acquisition creates a full persistent Manifestation.

## 3. Accepted numerical candidates

Keep as versioned tuning data rather than literals spread across code:

- Rank coefficient: R(r) = 10^(0.40r + 0.040r²).
- Level/breakthrough logarithmic split target: approximately 35% / 65%.
- Generic negative-DEF branch: 2^(-DEF / DefenseReference).
- Generic lower->higher Rank-Suppression envelope: 100 / 100 / 75 / 40 / 15 / 3% for effective gaps 0 / 1 / 2 / 3 / 4 / 5+, with channel-specific values.
- Standard-banner simulation candidate: 1.2% base top rate, soft pity after 50, +5 percentage points per pull 51-69, hard pity 70, 50/50 featured then guarantee.
- Acquisition-focused progression bands: approximately 4-8 / 7-14 / 14-27 / 27-52+ pull-equivalents per hour for the currently defined progression scenarios.
- Offline newly-generated irreversible strategic-loss ceiling: approximately 15% sovereign strategic weight per catch-up resolution, as a ceiling not a target.

Changing a TuningCandidate after simulation does not constitute an architecture revision unless the change violates a FixedRule or player-facing invariant.

## 4. Content-authored values

Examples:
- per-character stat response alpha inside the frozen allowed domain;
- character-specific Level curve;
- skill multipliers/references;
- Ultimate thresholds/tiers;
- item affinity/proficiency behavior;
- breakthrough requirements;
- World calendar/time ratios;
- World Director event windows;
- project/dispatch/war capability coefficients;
- World Fantasm rules/counters;
- Domain Concepts;
- NPC/character progression content.

Content values live in validated definitions/packages, not central C++ constants.

## 5. Device-profiled values

Examples:
- preload/expedition memory limits;
- actor counts;
- HLOD distances;
- texture/mesh budgets;
- animation update tiers;
- VFX density;
- audio streaming budgets;
- package hot-set thresholds;
- thermal fallback thresholds.

Do not freeze these from desktop guesses.

## 6. Versioning

Simulation outputs and adopted parameter sets receive a monotonically versioned tuning-set ID.

A save records only tuning versions where replay/history correctness genuinely requires it. Ordinary live resolution uses the active validated tuning set unless a deterministic historical/replay contract requires the prior set.

## 7. Test rule

Tests separate:
- invariant tests for FixedRule;
- regression/property tests for TuningCandidate ranges;
- content-validation tests for ContentAuthored data;
- profiling assertions/telemetry for DeviceProfiled values.

## 8. Coding rule

No system may silently promote a TuningCandidate or DeviceProfiled value into an architectural constant merely because a prototype currently uses it.
