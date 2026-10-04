# World Generation, Exploration and Persistent Location Evolution

This document consolidates the already-established world/exploration architecture and the latest clarifications. It does not replace earlier traversal/action-combat rules; it makes the generation, persistence and recovery grammar explicit.

## 1. Hybrid world production is authoritative

Worlds/regions use a hybrid production model rather than pure hand-authoring or pure runtime procedural generation.

The production pipeline is:

deterministic world/region seed
-> macro geography / topology / climate / laws
-> ecology / resources / hazards
-> civilization/culture/faction constraints
-> roads/routes/settlements/infrastructure
-> authored or generated high-value anchors
-> simulated prior history
-> current canonical world state

High-value authored anchors include major story locations, bespoke dungeons, unique landmarks, important settlements, major ruins, Domain structures and other content whose identity matters.

Procedural systems provide scale, variation, ordinary geography, minor settlements, ecology, infrastructure variation and lower-value content.

Large premium-quality regions are preferably generated/assembled in the external production pipeline rather than forcing the phone to synthesize them from nothing. The device handles local variation, deterministic ephemeral detail, world-state changes, reconstruction and lightweight expansion.

## 2. Canonical generation and virtually infinite expansion

Once a generated major fact is established in the persistent world, it becomes canonical.

A generated mountain, city, dungeon, road, cave system, river or other major location does not reroll merely because it is unloaded.

The game grows through a graph of region/world/dimension packages rather than one physically infinite Unreal map. Unloaded regions preserve lightweight authoritative state, history and metadata.

New packages can extend the world graph indefinitely without rewriting established geography.

## 3. Persistent change with causal restoration

Major locations can change permanently in the sense that their historical changes are real and recorded:

- settlements can be damaged, conquered, expanded, abandoned or rebuilt;
- forests can burn or recover;
- roads can emerge, decay or be restored;
- dungeons can be occupied, cleared, collapsed, repaired or repurposed;
- ruins can become settlements;
- settlements can become ruins;
- terrain can be altered by major events, Domains, catastrophes or engineering.

However, the world is not frozen in every damaged state forever.

Each persistent location/state may define a **recovery / restoration profile** describing whether and how it tends toward an earlier viable condition or another stable equilibrium over time.

Examples:

- grass/ordinary vegetation may return quickly;
- forests may regrow over years/decades depending on climate/species;
- minor destructible clutter may regenerate/reset quickly when no historical value is attached;
- wildlife populations can recover when habitat and breeding conditions survive;
- roads/bridges recover only if natural processes or actors repair them;
- settlements reconstruct only when inhabitants/factions/resources/projects actually support rebuilding;
- a burned village does not magically respawn because a timer expired;
- a cleared dungeon can be reoccupied over time if creatures/factions have a causal path to move in;
- magical terrain may revert when the responsible field ends, or remain transformed if the effect was explicitly persistent;
- world-scale scars may never naturally heal unless a sufficiently powerful restoration mechanism exists.

Restoration time is content- and state-dependent rather than one global respawn timer.

The authoritative history retains the destruction/recovery cycle even if the physical location later resembles an earlier stage again.

## 4. Destruction fidelity

Destruction remains tiered for feasibility.

- trivial objects: cheap destruction and regeneration where appropriate;
- structural objects: modular damage/breaching/collapse with real functionality consequences;
- major structures: authored structural states plus sectional destruction;
- terrain: selected craters, landslides, collapses and event-driven deformation rather than universal voxel excavation.

Physics/VFX are presentation; persistent structural state is authoritative.

## 4.1 Civilizations do not converge toward a generic modern endpoint

World evolution must preserve authored civilization identity. Increasing development means becoming more capable, grand, complex and internally mature along the civilization's own line, not replacing distinctive settings with contemporary cities.

A setting can combine genres when its history supports that combination, but the simulation never treats modernity as the default endpoint or as intrinsically more advanced than cultivation, magic, divine, eldritch, biological, draconic, cybernetic or other world-specific systems.

## 5. Later worlds and dimensions are not genre silos

A World/Dimension may combine genres, technologies, metaphysics and physical laws when its history makes the combination coherent.

Examples can include:

- cyber civilization inside a magical ruined cosmos;
- post-apocalyptic societies using divine/cultivation systems;
- oceanic worlds with orbital infrastructure;
- historical societies occupying remnants of extreme precursor technology;
- high-fantasy civilizations using dimensional industry;
- worlds with ordinary surface physics and abnormal sub-realms.

There is no rule that one World must equal one clean marketing-theme biome.

World identity comes from causal history, laws, civilizations, ecology and aesthetics rather than genre purity.

## 6. Dungeons and special locations are persistent world entities

The default dungeon is an actual place in the canonical world graph.

It can therefore have:

- a physical/reality location;
- ownership/control;
- discovered/undiscovered state;
- inhabitants;
- resource state;
- structural damage;
- traps/mechanisms;
- history;
- faction use;
- cleared/occupied/abandoned/repaired/reconfigured states;
- links to Territory/Domain systems where appropriate.

Clearing a dungeon does not automatically erase it or force a universal weekly reset.

Repopulation/reoccupation happens only through causal world processes.

Instanced or apparently resettable dungeons remain valid when their metaphysics justify them: loops, generated dream spaces, temporal recursion, pocket realms, simulations, ritual realms, regenerating labyrinths, etc.

## 7. Exploration knowledge, not completion percentage

Physical existence and player knowledge remain separate.

The map/Codex only exposes what the protagonist/Ruler has learned through:

- direct exploration;
- maps/documents;
- scouts/Dispatch;
- allies/trade;
- intelligence;
- sensors/magic/technology;
- rumors;
- captured knowledge;
- other valid information channels.

Knowledge can be uncertain, outdated or incomplete.

There is no universal omniscient "82% region completion" score used as the exploration fantasy.

A purchased or captured map provides information rather than teleporting exploration state into physical reality.

The player can know that a location exists without having visited it, and can physically pass near something without recognizing its significance.

## 8. Danger distribution has no player-level equalization

World generation does not guarantee that nearby content is appropriate for the current protagonist.

A weak starting character can encounter:

- powerful monsters;
- hostile Rulers;
- high-grade artifacts/resources;
- dangerous weather;
- inaccessible/hostile environments;
- ancient locations;
- dimensional anomalies;
- enemies that should simply be avoided.

Generation follows ecology, history, territory and world laws rather than player level.

No hidden enemy scaling makes everything remain equally difficult.

Warnings should be diegetic when possible: observation, equipment, companions, rumors, environmental signs, appraisal or learned experience.

## 9. Traversal barriers must be causal

There are no abstract "requires level 40" exploration walls.

A location may be inaccessible because the protagonist currently lacks a real requirement such as:

- flight;
- pressure resistance;
- underwater adaptation;
- vacuum survival;
- heat/cold/radiation resistance;
- dimensional stability;
- a key/ritual;
- sufficient physical force;
- teleportation;
- phasing/tunneling;
- Authority;
- knowledge/coordinates;
- safe transport;
- another explicit world-valid capability.

If the player legitimately acquires another way around the barrier, the game accepts it.

Examples:

- true flight bypasses a mountain pass;
- teleportation bypasses a road;
- overwhelming strength breaks a physical barrier;
- phasing ignores a wall if the wall has no anti-phasing rule;
- dimensional travel can bypass geography when the relevant ability really allows it.

The world answers capabilities, not designer-prescribed route order.

## 10. Traversal continuity

Existing traversal architecture remains authoritative:

- unrestricted ordinary sprint outside demanding conditions;
- climbing without a constant generic exploration stamina tax;
- swimming and diving with real drowning/environment rules;
- free 3D flight when the source grants it;
- mounts as real entities;
- vehicles/ships/airships/spacecraft where content supports them;
- teleport infrastructure and personal teleport capabilities;
- underwater, underground, sky, space and pocket-realm exploration;
- combat continuing across traversal modes when animations/capabilities permit.

## 11. Starting world integration

The starting region remains:

- deterministic once generated;
- broadly historical-medieval;
- one coherent initial biome/context rather than an artificial theme park;
- beautiful but credibly dangerous;
- capable of containing content beyond the protagonist's early strength;
- free of mandatory tutorial gates;
- seeded with three relevant factions and context-appropriate settlements/dungeons;
- not privileged as the permanent capital.

The generation system should avoid foregrounding endgame dimensional spectacle at spawn while still allowing later history/exploration to reveal deeper layers naturally.

## 12. Content authoring contract

A region/world package defines enough information for deterministic instantiation and long-term evolution:

- stable world/region IDs and seed provenance;
- parent cosmological node;
- law/environment profile;
- World Rank/tolerance data where relevant;
- geography/topology descriptors;
- biome/ecology/resource grammars;
- civilization/faction grammars;
- settlement/road/infrastructure rules;
- dungeon/location grammars;
- authored anchors;
- restoration/recovery profiles;
- danger/threat distribution rules;
- traversal/environment requirements;
- historical seed/state;
- package/version dependencies.

Runtime procedural generation may choose among validated definitions but must never invent arbitrary executable game code.

## 13. Implementation principle

Only player-relevant or history-relevant changes persist at high fidelity.

Ephemeral grass, debris and ambient detail can regenerate deterministically.

Important buildings, roads, Territory, Domain structures, dungeons, settlements, terrain scars and other meaningful state persist through compact authoritative records.

This preserves a living world without storing every blade of grass.
