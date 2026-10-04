# Mature Content Architecture

STATUS: ARCHITECTURE FROZEN FOR RECONCILIATION.

This document covers sexual/adult presentation, nudity, severe violence, injury, gore and clothing/equipment damage as integrated parts of the game.

## 1. Lore-grounded adult-content rule

For characters that the **canonical lore defines as adult/mature**, adult sexual content is architecturally allowed by default. There is no additional gameplay permission layer, affection threshold, romance gate, visual-age heuristic or appearance-based filter.

Content data must mirror the lore rather than silently invent a second classification system. Appearance is never used to decide adulthood or adult-content availability.

Preferences, libido, personality, relationship/history context, Version/form and current state shape **how** adult content is expressed and which authored/systemic scene variants are appropriate; they are not a generic lock that makes an adult character unavailable for the pillar.

Tsundere behavior, role-play, teasing, contradictory surface dialogue or similar character presentation is treated as characterization, not as an automatic system veto.

Ruler Authority/contracts are not used as a substitute for character-specific adult-content characterization; scene behavior is still authored from the character's actual lore/state.

Characters canonically treated in lore as minors/children remain outside sexual-content packages.

## 2. Adult content is a core pillar

Adult content is integrated with:
- persistent character state;
- Versions/forms;
- world location;
- outfits/equipment;
- transformations;
- injuries;
- character preferences/libido;
- story/history.

It is not a detachable romance minigame and is not gated by a universal affection meter, marriage route or story-completion ladder.

Romance/partnership may exist independently and imposes no universal sexual-content limitation.

## 3. Character sexual profile

Canonically adult characters can define character-specific:
- libido / sexual drive intensity;
- preferences;
- initiative tendency;
- contextual modifiers;
- Version/form-specific changes;
- transformation/Factor influences;
- relationship/history influences;
- scene tags/compatibilities.

This profile is not a morality or affection score.

Current libido/preferences may change causally through character development, transformations, Versions, Factors, events or other authored mechanics.

## 4. Initiative and contextual behavior

Adult-content opportunities may be initiated frequently by characters themselves, by the protagonist, or by context/events.

History can influence behavior without becoming a grindable meter:
- current hostility/rivalry;
- trust/familiarity;
- romance;
- recent betrayal;
- recent rescue;
- transformations;
- mood/state;
- other authored factors.

## 5. Interaction model

Primary adult-scene presentation is **interactive hybrid content**.

Foundation:
- real-time 3D systemic scene framework;
- contextual locations throughout the persistent world;
- moderate-to-substantial interaction depth;
- player-controlled pacing/camera/contextual actions/state changes;
- compatible multi-participant adult scenes;
- character-specific behavior and preferences.

Premium authored scenes may add:
- bespoke animation;
- cinematic staging;
- illustration/CG;
- custom voice/audio;
- character-specific sequences.

The systemic layer and premium layer coexist rather than one replacing the other.

## 6. Location and participant scope

Adult scenes may occur in any physically/contextually appropriate location rather than one dedicated menu/gallery room.

All characters whose canonical lore defines them as adult/mature may potentially participate, including:
- gacha Manifestations;
- adult world-born NPCs;
- promoted NPCs;
- other adult persistent characters.

Adult-content access itself does not become unavailable because of relationship, mood, injury, Version, body type or other gameplay state. Those facts select or adapt the expression that is physically/lore-valid. A specific act/animation may be incompatible with a particular anatomy or state, in which case the system selects/adapts another valid interaction rather than turning the entire adult-content pillar off.

Multi-partner scenes are supported when the involved adult characters and physical/asset state can express them; incompatibility changes the available scene construction rather than redefining adulthood.

## 7. Persistent physical state

Adult presentation uses the character's actual current state when compatible:
- Character Version;
- Manifestation-specific development;
- Factor anatomy;
- transformations;
- body modifications;
- hairstyle;
- outfit;
- equipment;
- injuries;
- relevant temporary states.

The scene system should not silently revert the character to a generic base body merely because a scene package was authored earlier.

Serious injuries are condition-dependent in **presentation and possible actions**: scenes may reflect them, use adapted poses/actions, or include recovery where that character would actually choose/use it. Injury does not become a generic adult-content lock; it can only make specific physically impossible actions unavailable.

## 8. Modular production architecture

To scale across a very large roster, the adult-content pipeline uses:
- compatible rig families;
- modular body/outfit states;
- reusable animation foundations;
- contextual scene graphs;
- character-specific animation/personality overlays;
- bespoke premium scenes for important content;
- package streaming.

Systemic scenes are composed only from validated compatible assets/state. The runtime does not procedurally invent production-quality explicit assets.

Every major canonically adult gacha character should ship with meaningful mature-content support in the initial character package, and the long-term target is meaningful mature-content coverage for the full canonically adult roster rather than a tiny special subset.

## 9. Archive / replay

Previously experienced scenes may be replayed from the archive.

Where assets permit, replay can choose:
- historical recorded appearance/state;
- current appearance/state.

Archive replay does not rewrite canonical world history.

## 10. Mature combat presentation

The game has no arbitrary mature-violence ceiling.

Where target anatomy/material and mechanics support it, presentation may include:
- severe wounds;
- dismemberment/severing;
- persistent injuries;
- equipment destruction;
- armor/clothing destruction;
- later regeneration/reconstruction/replacement through valid abilities.

Violence materials are target-specific:
- blood;
- ichor;
- mechanical fluids;
- crystalline fracture;
- energy disruption;
- other appropriate material behavior.

Clothing/outfit damage is stateful and can expose the body on canonically adult characters when the actual clothing state reaches that point.

## 11. Privacy / SFW presentation mode

The intended baseline presentation is highly uncensored.

A single **Privacy / SFW Presentation** option can hide or substitute explicit sexual/nude/gore presentation for privacy while preserving the authoritative underlying world/character state.

This is a presentation mask, not a canonical-state rewrite or alternate game balance mode.

## 12. Reproduction and partnership

There is no general-purpose pregnancy/reproduction simulator in the baseline.

Specific species/characters/mechanics may implement reproduction where it is actually relevant.

When implemented, pregnancy/reproduction is real persistent character/world state rather than a temporary gallery flag.

Marriage/formal partnership remains lightweight and character/story-specific and is never a universal prerequisite for adult content.

## 13. Gameplay consequences

Adult scenes do not grant universal currency, affection points or generic combat buffs.

Character-specific causal consequences are allowed when the character's actual nature/progression makes them meaningful.

Examples can include development catalysts, species-specific resource interactions, transformation/evolution effects or biography/history changes.

## 14. Lore-fidelity validation boundary

The package validator does not invent its own maturity judgment from appearance.

It verifies that sexual-content references are consistent with the Character Identity's canonical lore classification. If the lore defines the character as adult/mature, adult-content packages are valid; if the lore defines the character as a minor/child, sexual-content packages are rejected.

The validation layer therefore enforces **lore consistency**, not an independent visual-age or gameplay-age policy.

Nonsexual mature systems such as blood, injury, gore and equipment/clothing damage use separate content rules.
