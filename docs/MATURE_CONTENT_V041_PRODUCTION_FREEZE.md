# Mature Content Production Freeze v0.41

STATUS: AUTHORITATIVE USER-APPROVED MATURE-CONTENT PRODUCTION FREEZE.

This document is a narrow v0.41 addendum to the v0.40 architecture. It supersedes only conflicting mature-content production/runtime details in older repo documents. All unrelated v0.40 architecture remains authoritative.

This document deliberately does not reopen or redefine eligibility policy. The Foundation implementation must not invent any new eligibility, affection, romance, appearance, morality, permission, or access-gating subsystem.

## 1. Product role

Mature/adult content is a high-frequency core gameplay pillar, not a gallery, occasional reward, detachable minigame, or isolated side mode.

It is always player-involving. The canonical protagonist/Ruler is always the player-side physical participant, including when World Mode currently has another expedition/party character under direct control.

Entry may originate from:
- direct player initiation from a character/roster surface;
- direct embodied World Mode interaction;
- Ruler/lobby interaction;
- story, event, lore, or authored context;
- spontaneous NPC/world context;
- post-combat/defeated-opponent context;
- other ordinary persistent-world situations.

## 2. Primary interaction model

The primary production model is a Koikatsu-like systemic 3D interaction runtime without its character-creation function, because characters already come from the game's Character Identity / Manifestation pipeline.

The systemic runtime is primary. Premium bespoke animation/cinematic/CG presentation may be layered on top for important scenes, but it must not replace the reusable systemic foundation.

The player can choose, where physically representable:
- participants;
- actions;
- action sequence;
- pacing;
- camera;
- positioning/staging;
- location/context.

The runtime must be data-driven and content-agnostic. It must not hard-code a morality classifier, narrative-value classifier, romance requirement, relationship requirement, location whitelist, fixed two-person assumption, or arbitrary intensity ceiling.

Applicable legal/platform/distribution constraints belong outside the in-world runtime model and must not be expressed as invented gameplay morality mechanics.

## 3. Persistent-world integration

Mature interactions are part of the same persistent world as the rest of the game.

World time advances during them.

Location is relevant to staging, camera, collision, environment presentation, and contextual behavior, but location should adapt the interaction rather than act as an arbitrary global prohibition.

The runtime should solve staging from current participants and nearby usable environment anchors. When immediate geometry is unsuitable, it should seek a workable nearby staging solution rather than disable the entire interaction category.

Entry and exit must preserve authoritative world/character state and return coherently to ordinary gameplay.

## 4. Participant model

The protagonist/Ruler is fixed as the player-side participant.

Other participants are persistent characters or NPCs selected by the player or supplied by current world/story context.

Participant count is variable-arity and must not be architecturally capped to two, three, or four. The data/runtime model must support scenes with more than ten participants. Device-performance strategies may tier simulation/rendering, but must not redefine the underlying participant model.

Multi-participant interaction is a normal supported production category rather than an exceptional architecture path.

### Participant-count performance rule

Support for 10+ participants is an architectural/state requirement, not a guarantee that 10+ participants always run at maximum character fidelity simultaneously.

The authoritative interaction runtime must remain capable of representing, binding, sequencing and persisting more than ten participants in one interaction while rendering/animation/simulation fidelity is significance-tiered by camera relevance, action relevance, visibility and device budget.

Typical runtime policy:
- protagonist and immediate focus participants: maximum available character/animation fidelity;
- nearby active secondary participants: high fidelity with selective reduction of expensive secondary systems;
- background participants: reduced animation-update rate and simplified secondary physics/IK/shadows/material cost where appropriate;
- off-camera participants: heavily throttled visual/animation evaluation or logical-state-only execution until relevant again.

Optimization must never remove participants from authoritative interaction state, reduce supported participant count as a gameplay rule, corrupt sequencing/causality, substitute a generic Character Manifestation, or prevent a participant from returning to full fidelity when relevant.

The implementation should reuse the same character significance/LOD/streaming architecture used by ordinary World Mode rather than creating a separate mature-scene character-rendering stack.

For complex multi-participant interactions, stable high-quality 60 FPS is the primary production target. 120 FPS remains optional where physical-device profiling proves it sustainable; the systemic participant model and production character fidelity must not be weakened merely to guarantee 120 FPS.

## 5. Character-state fidelity

Scene presentation uses the actual current persistent character state wherever assets/rigs support it:
- Character Identity and Manifestation;
- Version/form/transformation;
- anatomy/body configuration;
- Manifestation development;
- Factor/body-state influences;
- outfit/skin;
- equipment;
- hairstyle;
- injury/damage state;
- other relevant persistent or temporary states.

The runtime must not silently substitute a generic base body merely because an older reusable scene/template was authored against one.

Current state changes scene construction and presentation. It does not create a second generic access layer.

## 6. Reuse versus bespoke production

Roster characters receive a broad systemic interaction repertoire.

The scalable production stack is:

shared systemic interaction/action library
+ rig-family adaptation
+ character-specific expression/reaction/personality data
+ character-specific animation/state overrides
+ bespoke premium animations/scenes where worthwhile.

Shared rigs/retargeting are production tools, not a one-body restriction.

Generic NPCs may rely heavily on reusable templates and generic compatible reaction sets.

When an NPC becomes a bespoke Character/Version, it enters the richer character production pipeline rather than remaining restricted to generic-NPC treatment.

## 7. Staging and animation runtime

Foundation must support:
- reusable scene/action graphs;
- participant-role binding;
- environment-anchor discovery;
- procedural positioning/alignment;
- IK/constraint hooks where appropriate;
- camera-control hooks;
- player pacing/control hooks;
- contextual-action transitions;
- interruption/exit handling;
- character-specific animation overrides;
- modular body/outfit state binding;
- non-identical compatible rig families.

The Foundation should provide the reusable runtime and extension points, not the final explicit animation catalog.

## 8. Consequences and authoritative state

There is no generic mature-content reward economy.

Do not implement a universal mature-interaction -> currency/stat/bond reward rule.

Interactions may cause persistent history, relationship, knowledge, character-state, or world-state consequences only where the particular authored/systemic situation causally requires them.

All canonical consequences use the same authoritative persistence/world-state mechanisms as ordinary gameplay.

## 9. Replay / archive

A dedicated mature-scene replay/archive runtime is not required.

The systemic interaction can normally be performed again directly through ordinary gameplay.

Records/Chronicle may preserve that an event occurred when it matters to canonical history, but Foundation does not need to store a playable historical reconstruction or current-state replay variant.

Older mature-content replay requirements are superseded by this section.

## 10. Privacy / SFW presentation

Privacy/SFW Presentation is a presentation-only substitution layer.

It may use masking, alternate framing, fade/summary, or another low-cost presentation substitute.

It must never rewrite or fork authoritative character/world state, outcomes, history, progression, or consequences.

Because it is secondary rather than the primary intended presentation, Foundation should favor robustness and low maintenance cost over bespoke alternate-scene production.

## 11. Packaging and character-state selection

Mature-content assets should follow the character/Manifestation production model rather than one monolithic gallery package.

Changing Manifestation, form, skin/outfit, equipment, hairstyle, or other current presentation state before an interaction should naturally flow into scene construction through the ordinary character-state pipeline.

Missing optional presentation assets must degrade safely without corrupting canonical world/character state.

Character-specific packages may provide:
- compatible animation/action assets;
- expression/reaction data;
- voice/audio;
- bespoke overrides;
- premium scene/cinematic assets.

Generic NPC support may use shared reusable packages unless/until that NPC is promoted into the bespoke character pipeline.

### Signature cinematic supplement

Each proper Character Manifestation should support one optional signature authored mature-content cinematic package as a premium supplement to the systemic runtime, nominally targeting roughly 5-10 minutes where production resources justify it.

This cinematic layer is the "cherry on top" rather than the primary mature-content system. It does not replace, gate, or reduce the high-frequency systemic interaction runtime.

The preferred presentation target may be substantially more bespoke and animation-directed than ordinary systemic interactions, including pre-rendered or otherwise premium cinematic treatment where that gives the best quality/cost result.

Signature cinematics should be independently downloadable/cacheable/evictable where practical so they do not unnecessarily bloat the base APK or required hot set. Their absence must never break the Manifestation, systemic mature-content functionality, or canonical save state.

Production tooling may generate/render these cinematics offline from the current Manifestation's authored assets and references, but final cinematic asset creation remains a production task rather than a Foundation requirement.

## 12. Foundation implementation boundary

Foundation must implement and stabilize only the generic runtime needed to support this pillar:

- protagonist-fixed player participant role;
- variable-arity participant binding;
- more-than-ten-participant-capable data/runtime model;
- contextual entry from Ruler, roster, World, event/story, NPC, and post-combat contexts;
- systemic interaction/action graph;
- player-selected actions and sequence;
- player-controlled pacing;
- camera-control integration;
- location-aware staging;
- environment-anchor/placement support;
- character-state propagation;
- modular rig/animation compatibility;
- character-specific override hooks;
- persistence/world-state integration;
- clean interruption/exit/return to gameplay;
- Privacy/SFW presentation substitution;
- optional package/asset availability handling.

Foundation must not spend time producing:
- the final mature-content scene/action catalog;
- final explicit animation libraries;
- final character-specific scene libraries;
- final premium CG/cinematics;
- final voice/audio assets;
- final production staging sets;
- final per-character content quantity/tuning.

Those belong to mature-content production after Foundation is stable.

## 13. Production breadth

Every proper roster character should ultimately receive a broad systemic mature-content repertoire rather than a tiny handful of isolated scenes.

Generic NPCs can use reusable templates at scale. Promoted/bespoke NPCs can receive the richer character pipeline.

The runtime must remain open-ended enough that future authored content categories are not blocked by a narrow hard-coded scene taxonomy.

Narrative morality or extremity is not a runtime restriction category. The runtime's job is to represent authored persistent-world interactions consistently; it does not decide whether a fictional situation is morally positive or negative.

## 14. Relationship to v0.40

This file is the authoritative mature-content production/runtime addendum after v0.40.

Where it conflicts with older repo mature-content documentation on:
- replay/archive;
- participant role;
- participant count;
- interaction frequency;
- player control;
- systemic-vs-bespoke balance;
- world/location integration;
- production breadth;

this v0.41 file wins.

It does not otherwise supersede v0.40.
