# World Director

This document records the current authoritative World Director architecture.

The World Director is a bounded autonomous world/live-ops subsystem. It is not the Creative Director and may not change foundational rules, break pity/guarantees, fabricate resources to force outcomes, rewrite established history without an explicit mechanic, or secretly counter the player for succeeding.

## 1. Cadence

Cadence is driven primarily by **in-game/world time, eligibility and world state**, not by a rigid real-world weekly calendar.

Each Director-controlled content definition may declare:

- eligibility predicate;
- earliest activation time;
- preferred activation window;
- latest-start bound or delay budget where relevant;
- cooldown / rerun rules;
- world-state relevance;
- missability mode.

Eligible content should not be withheld for absurd lengths of time. The Director may preserve uncertainty by delaying activation within the authored bounds.

## 2. World continuity while absent

Events, faction actions, wars and systemic stories may begin, evolve and resolve while the protagonist is elsewhere.

The same rule applies through offline catch-up while the app is closed.

Missed events may leave persistent consequences such as resource loss, injuries, changed factions, destroyed infrastructure, missed opportunities, territorial changes and Chronicle history.

## 3. Offline consequence governor

Offline simulation remains causal, but closing the app must not become a random catastrophic punishment.

Routine and moderate consequences may resolve normally offline, including some peripheral territorial loss when causally justified.

The World Director may not newly manufacture a chain of catastrophic player-targeted events during absence whose practical result is arbitrary destruction of most/all player territory, the Main Domain/Core, or the whole strategic position.

High-severity crises crossing that threshold use escalation gates: they may damage, besiege, isolate, partially occupy or reach a critical unresolved state, but final irreversible collapse is normally deferred to the next active session unless the catastrophe was already causally locked in before logout by existing state or explicit player choices.

This safeguard constrains the Director rather than secretly weakening enemies or rewriting prior decisions.

### Strategic-weight catch-up ceiling

For newly generated offline consequences, the normal safety ceiling is approximately **15% of current sovereign strategic weight per catch-up resolution**.

This is a ceiling, not a periodic tax or expected loss. Reconnecting must never itself cause an arbitrary 15% loss. Most catch-ups can resolve with zero territorial loss.

Only actual eligible causal events may consume any of this budget. Consequences that were already causally locked before logout remain governed by their prior state rather than retroactively protected by this ceiling.

## 4. Approved event composition

The Director may compose systemic events from approved templates and approved characters, factions, locations, enemies, rewards and world states.

It may not invent new canonical lore facts, foundational rules, unvalidated abilities or arbitrary resources.

**Implementation requirement:** when this subsystem is implemented, autonomous composition must be explicitly identified in code/docs and every composed event must record:

- template ID;
- participating content IDs;
- package versions;
- deterministic seed;
- relevant world-state inputs;
- Director decision provenance.

Deterministic replay and validation tests are mandatory.

## 5. Sealed content and surprise

Validated future characters, events, arcs and packages may remain opaque/sealed until activation.

The Director may deliberately delay eligible sealed content, including major surprises, but only within content-authored delay bounds. It cannot indefinitely withhold eligible content for manipulation.

Major content may activate with little or no advance warning when prerequisites and delay rules allow it.

## 6. Concurrent crises

Several serious events may coexist.

The world does not serialize independent crises for player convenience, and the player is not guaranteed enough time/resources to solve every simultaneous problem.

The Director may not intentionally synchronize unrelated crises solely to punish player success; independent causal schedules may nevertheless collide naturally.

## 7. Banner/world coupling

Banner selection may use current world/story state as a relevance signal.

A featured banner does not imply the featured Character Identity is physically present in the current region. Dimensional acquisition can make identities available independently of local geography.

Banner scoring may consider:

- world/story relevance;
- recent availability;
- rerun pressure;
- novelty/diversity;
- pity-category compatibility;
- sealed-content timing.

World-state incompatibility may delay content that actually requires it, while purely dimensional/gacha opportunities need no fake local narrative justification.

## 8. Reruns and missability

Limited content remains eligible for reruns unless an explicit world-state condition temporarily prevents availability.

Rerun pressure rises with time since last availability, subject to cooldown/category/world-state constraints.

Systemic events may be fully missable. Missing them changes history rather than replaying the world until the player participates.

Authored major content declares whether it may:

- fully resolve without the player;
- transform into aftermath/follow-up content; or
- hold at a critical entry state when direct participation is genuinely required.

## 9. Audit and reproducibility

Director decisions use deterministic decision epochs/seeds and versioned rules.

Every activation, delay, rerun, composition choice and offline escalation decision records a concise audit reason.

Sealed package manifests may expose hashes/versions for validation while keeping contents spoiler-hidden from the Creative Director.

Debug tooling may replay a Director decision from the same seed/state without normal UI revealing future sealed content.


## Numerical catch-up guard checkpoint

For normal newly generated offline consequences, use roughly **15% of current sovereign strategic weight as a maximum irreversible-loss budget**, not as a target.

This never means the player loses 15% whenever they reconnect. Most catch-up periods may cause no permanent Territory loss.

Consequences already causally locked before logout may exceed/ignore this protective budget when the prior active world state genuinely made them unavoidable.

There is no one global event-every-N-hours cadence. Each Director content family authors eligibility, earliest/preferred/latest windows and cooldown/relevance bounds.
