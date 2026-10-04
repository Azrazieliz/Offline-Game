# Dispatch, Factions and War — Minimal Strategic State

## Scope

This layer deliberately implements only the persistent strategic facts needed by the RPG.

It does **not** add:

- relationship / trust / reputation scores
- faction feelings
- treaty-law documents
- Casus Belli requirements
- internal politics
- occupation governance
- spy-agent life simulation
- NPC task/day simulation
- individual ordinary soldiers
- generic "negotiate peace now" gameplay

## Dispatch

Exploration, Gathering, Reconnaissance, Espionage, Support and other delegated assignments share one Dispatch record.

A Dispatch stores:

- owner
- optional target
- assignment type
- participants
- start/resolve ticks
- risk
- deterministic resolution seed
- final result

The engine does not simulate irrelevant intermediate actions.

A mission remains Active until its due tick and is then resolved once by the appropriate capability/mission resolver. The stored result is the authoritative outcome.

## Factions

A Faction stores only compact strategic identity:

- stable entity ID
- optional leader/Ruler
- kind
- aggregate population

There is no hidden opinion or reputation meter.

## Explicit faction links

The baseline state supports:

- Alliance
- directional Support
- Subordination

Links are explicit booleans/facts with update time.

Dialogue and authored events can present them richly without requiring a relationship simulation.

## Armies

Ordinary military force is aggregated.

An Army stores:

- faction
- physical Location
- headcount
- aggregate effective power
- named commanders
- compact state

Named/important commanders remain persistent individual entities. Ordinary soldiers do not become individual simulation agents.

## War

War requires no artificial Casus Belli.

A War stores:

- participants and sides
- objective type
- optional physical/entity objective target
- start/end ticks
- status
- concise resolution state

Declaring war breaks an **existing active alliance** transactionally, but does not invent a fake historical alliance if none existed.

War resolution is an API used after an actual strategic/world outcome such as victory, defeat, objective completion, withdrawal, collapse or another explicit event.

It is **not** a generic peace-negotiation button.

## Persistence

Migration 0005 adds normalized tables for:

- dispatches + participants
- factions
- explicit faction links
- armies + named commanders
- wars + participants

The automation proof covers:

- a Dispatch cannot resolve before its due tick
- its participants/seed/result survive restart
- aggregate Army headcount + named commander persistence
- alliance creation
- directional support
- declaring war breaks a real alliance
- declaring war between never-allied factions does not fabricate alliance history
- war objective/status/participants persist
- war resolution occurs only through an explicit final-state operation

## Campaign, command and conquest architecture

### High-level command, not tactical micromanagement

The player's default strategic command model is **intent/objective based**. Orders can specify goals and constraints such as:

- take/hold a location;
- protect a Domain Core or route;
- raid/disrupt logistics;
- avoid excessive casualties;
- capture rather than destroy;
- delay the enemy;
- disengage if a threshold is crossed;
- prioritize/avoid specific targets.

Competent commanders choose tactical means according to their actual knowledge, personality, doctrine, skills, resources and local situation. The player may still intervene personally or issue unusually precise orders when a valid command mechanism permits it.

### Commander autonomy and disobedience

Named commanders are causal characters, not obedient pathfinding nodes. They may reinterpret, delay, refuse or disobey an order when loyalty, personality, knowledge, fear, survival instinct, conflicting Authority, impossible conditions or another real factor justifies it.

The result is recorded as a world event/command outcome; the game does not silently rewrite the original order.

### Continuous wars, campaigns and fronts

A War is a persistent world state, not a series of isolated battle instances.

One War may contain multiple campaigns/fronts/objectives that change over time. Participants may enter/leave, sides may fragment or merge, objectives may evolve, and the War may continue for months or years of world time.

Physical battles, strategic resolutions, sieges, raids, retreats, diplomacy and Ruler interventions all update the same authoritative War state.

### Decapitation does not automatically end war

Death/capture/removal of a Ruler, supreme commander or faction leader never has one universal outcome. Depending on the faction's actual structure it may cause surrender, succession, fragmentation, civil conflict, morale collapse, continued resistance, radicalization or little immediate change.

### Conquest is effective control, not a capture flag

Winning a battle or entering a location does not automatically transfer Territory. Military presence, surviving Authority, resistance, logistics, local factions and the established five-day territorial reclamation rules determine when control actually becomes real.

Occupation/governance micromanagement remains outside scope; only strategically meaningful control state is represented.

### Armies have no universal composition template

An Army may coherently consist of ordinary soldiers, monsters, undead, constructs, summons, vehicles, fleets, flying units, dimensional forces, elite squads, enormous singular entities, mixed formations or another world-valid military structure.

The strategic representation is capability-based rather than assuming every Army is 'N humanoid soldiers'. Content defines the relevant mobility, range, logistics, durability, special capabilities, commanders and counters.

### Extreme power asymmetry is preserved

Headcount does not receive artificial anti-character scaling. If one named character is actually powerful enough to destroy an ordinary army, the strategic resolver accepts that fact.

Conversely, a lower-personal-Rank force can threaten a much stronger individual when its actual formations, artifacts, sealing, specialized weapons, terrain, attrition, logistics or conceptual counters justify it.

Strategic resolution therefore uses real capabilities and interactions rather than a universal army-power number as the sole truth.

### Resolution model

The engine may maintain compact aggregate capability vectors for performance, but they are projections of actual authored state. At minimum, strategic resolution can reason about:

- effective fighting power and relevant Rank bands;
- mobility and reach;
- command quality;
- morale/cohesion where meaningful;
- logistics/supply sufficiency;
- fortification/siege capability;
- special/Authority/Domain counters;
- terrain/environment compatibility;
- named-character intervention;
- objective progress and casualty/attrition state.

Exact coefficients remain simulation/tuning data.

## Dispatch objective fidelity

Dispatch autonomy is subordinate to the assigned mission. The player specifies a primary objective, optional secondary objectives, hard constraints, risk tolerance and abort conditions.

Participants may improvise tactically and exploit opportunities only when doing so does not materially compromise the mission. A rescue team does not abandon its rescue target to farm unrelated resources. Priority order is: mandatory objective/constraints -> survival/abort rules -> secondary objectives -> compatible opportunistic actions.

True disobedience remains possible when personality, loyalty, fear, conflicting Authority or another causal factor justifies it, and is recorded as such.
