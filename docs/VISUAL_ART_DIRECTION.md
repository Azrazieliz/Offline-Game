# Visual Art / VFX / Animation / Cinematic Production Direction

STATUS: AUTHORITATIVE PRODUCTION-DIRECTION FREEZE.

This document closes the pre-coding visual-production direction. It defines the shared rendering, character, environment, VFX, animation, cinematic and UI presentation language while preserving radical variation between Worlds and characters.

## 1. Core visual identity

The baseline is a **high-end anime / PBR hybrid**.

Characters remain clearly stylized/anime in shape language, face design and readable silhouette while materials, lighting, cloth, metal, skin response, hair, environment interaction and effects use physically coherent rendering where useful.

The goal is not photorealism and not flat toon rendering. It is premium stylized 3D with enough physical grounding that equipment, injury, weather, destruction and radically different Worlds remain convincing.

## 2. Character fidelity

Character identity takes priority over forcing the roster into one body template.

Characters may materially differ in:
- height/proportions;
- musculature/body composition;
- species/anatomy;
- wings/tails/horns;
- mechanical parts;
- non-human limbs;
- material composition;
- silhouette;
- movement language.

Shared rigs/retargeting are production tools, not visual restrictions. A nonstandard body receives a genuinely appropriate rig/animation set when humanoid reuse would compromise identity.

## 3. Face, skin, eyes and hair

Faces use anime-informed proportion/stylization with high-quality deformation and expression.

Skin:
- stylized but physically responsive;
- avoids waxy photoreal scanning;
- supports wounds, dirt, wetness, environmental influence and species-specific materials.

Eyes:
- retain strong readable anime identity;
- use layered/material treatment for depth;
- remain expressive at mobile viewing distances.

Hair:
- uses authored clump/silhouette design first;
- secondary strand detail supports, rather than destroys, the intended shape;
- physics is selective and performance-tiered.

## 4. Materials and equipment

PBR response is material-specific but stylized to preserve readability.

Metal, cloth, leather, crystal, bone, scales, machinery, magical matter and abnormal materials should not collapse into the same roughness/lighting treatment.

Physically worn equipment normally appears on the character unless an actual mechanic/state hides, transforms, internalizes or removes it.

Equipment respects body/form compatibility rather than universal humanoid armor slots.

## 5. Outfits, skins and clothing state

Outfits/skins are presentation content and do not automatically create a new Identity, Version, form or power state.

Where feasible, clothing uses modular compatible layers so:
- equipment can coexist with skins;
- damage/removal can affect actual layers;
- transformations can replace or expose relevant pieces;
- persistent state can be represented without swapping an entire character asset blindly.

No artificial permanent fallback costume is required when all valid removable layers are absent. Actual presentation follows the content/state and Privacy/SFW mask.

## 6. Character rendering readability

Characters should remain readable against highly variable Worlds.

Use contextual techniques such as:
- controlled key/rim separation;
- local contrast management;
- subtle contextual outlines;
- material-value separation;
- selective effect suppression near silhouettes.

There is **no heavy universal black contour**.

Outline strength may vary by camera distance, environment and character material.

## 7. Environment direction

Each World/Dimension owns a visual profile:
- architecture;
- materials;
- vegetation/ecology;
- atmosphere;
- lighting;
- sky;
- weather;
- cultural design language;
- technological/metaphysical language;
- destruction/recovery appearance.

Civilizations advance by deepening their own identity. A cultivation civilization should not visually converge into a generic modern city merely because it becomes advanced.

Cross-cultural influence appears only where actual history caused it.

## 8. Starting-world visual baseline

The starting World remains broadly historical-medieval and grounded relative to later dimensions.

It should emphasize:
- dangerous natural wilderness;
- believable settlements/materials;
- local cultural differentiation;
- limited foregrounding of endgame-dimensional spectacle;
- gradual discovery of stranger metaphysics.

Seed-generated regions use authored biome/style grammars so procedural combination still produces art-directed locations rather than noise.

## 9. Lighting

Lighting is cinematic but physically legible.

Global rules:
- character readability without permanent studio lighting;
- strong time/weather/world identity;
- authored exposure ranges;
- restrained bloom;
- no universal neon wash;
- no automatic "rarity = brighter environment" rule.

World laws may radically alter lighting when the fiction supports it.

High-order powers may alter sun/sky/shadow/material behavior when reality itself is being changed.

## 10. Color and palette

There is no universal Rank color code.

Color belongs to:
- character identity;
- faction/culture;
- material;
- environment;
- ability;
- reality field;
- UI semantic state.

Rarity may use readable card/frame treatment but does not dictate the entire character/effect palette.

## 11. VFX language

VFX communicates **what rule is happening**, not simply how expensive the move is.

Reusable primitives include:
- particles;
- ribbons/trails;
- decals;
- mesh effects;
- material overrides;
- volumetrics;
- lighting;
- sky changes;
- geometry deformation;
- field boundaries;
- post-process;
- environmental simulation cues.

Character-specific effects compose these primitives into authored identities.

Avoid generic "larger explosion = stronger Rank" escalation.

## 12. World Fantasm / Domain / higher-order fields

Reality fields should visibly affect the world where appropriate.

Possible channels:
- sky;
- light;
- geometry;
- materials;
- weather;
- gravity/motion cues;
- spatial distortion;
- object replacement/transformation;
- field boundaries;
- law-specific symbols/structures;
- selective negation/silence of existing effects.

Projection, Alteration, Materialization, Manifestation and Negation are behavioral grades, not universal color presets.

Overlapping fields may show:
- boundaries;
- mixed zones;
- fractures;
- dominance pockets;
- local overwrite;
- unstable interfaces;
- clean negation.

Mechanical state drives the visual result.

## 13. Power scaling presentation

Very high power can be spectacular, but it can also become **visually quieter** when precision or law-level control is more appropriate.

Late-game readability must not be sacrificed to particle density.

When many effects overlap:
1. critical telegraphs;
2. player defensive readability;
3. active character silhouette;
4. boss/major field boundaries;
5. secondary spectacle

receive descending presentation priority.

## 14. Injury, destruction and material-specific violence

Presentation is causal and species/material specific.

Supported where mechanics/lore justify:
- wounds;
- severing/dismemberment;
- armor/equipment damage;
- clothing damage;
- mechanical breakage;
- crystalline/elemental fracture;
- blood/ichor/fluids/other material responses;
- later regeneration/reconstruction.

The system does not apply one human-blood effect to every entity.

Persistent injuries/destruction remain visible while their authoritative state persists.

## 15. Destruction and restoration

Environment destruction is tiered for feasibility but persistent state remains authoritative.

A destroyed location may:
- stay destroyed;
- be repaired;
- regrow;
- transform;
- reconstruct;
- reach another equilibrium.

Visual restoration follows actual causal recovery. The renderer never silently resets history because an area streamed out.

## 16. Animation language

Animation quality target is character-specific premium action-RPG animation with a strong **HI3rd-like production philosophy**:
- shared technical foundations where compatible;
- bespoke combat/movement expression where identity matters;
- strong anticipation and follow-through;
- responsive cancel windows according to mechanics;
- clear defensive telegraphs;
- personality in idles/transitions;
- signature Ultimates and World Fantasm activations fully bespoke.

Animation responsiveness must never be sacrificed simply to make motion more cinematic.

## 17. Locomotion

Locomotion reflects body, surface and capability.

The pipeline supports materially distinct locomotion for:
- ordinary humanoids;
- heavy armored bodies;
- quadrupedal/non-humanoid forms;
- flight;
- swimming/underwater;
- climbing;
- transformed bodies;
- zero/abnormal gravity;
- mounted/vehicle contexts.

Traversal animations and combat transitions must connect cleanly enough that World Mode does not feel like separate minigames stitched together.

## 18. Combat camera

World Mode uses a readable character-action camera.

Principles:
- controlled target framing;
- avoid excessive shake;
- telegraphs remain visible;
- large enemies may require dynamic distance/framing;
- special attacks may temporarily take camera authority only when gameplay state permits;
- camera effects are intensity-adjustable.

Ultimate/World Fantasm cinematics may use stronger authored camera language but return to a stable gameplay frame predictably.

## 19. Cinematics

Story cinematics are primarily real-time/in-engine so current:
- outfit;
- form;
- injury;
- equipment;
- world state;
- location state

remain accurate.

Illustrated or prerendered sequences remain valid when artistically superior or technically necessary.

Major first-time scenes use full presentation. Replays may allow faster skipping.

## 20. Summon / rarity / progression presentation

Gacha and breakthrough sequences are premium but do not copy another title's branding or exact visual grammar.

The sequence may use:
- dimensional/contract imagery;
- Identity-pattern formation;
- rarity reveal;
- character-specific motif/VFX accents.

Repeat acquisition still presents the new Manifestation as a meaningful full acquisition, not as shard conversion.

## 21. Ruler Mode / UI visual identity

Ruler Mode uses a premium **character-first dimensional-command** language rather than a generic dashboard.

Direction:
- dark/neutral structural surfaces with context-driven luminous accents;
- restrained dimensional-line/lattice motifs;
- depth through layered translucency/material treatment where performance allows;
- character art and world imagery dominate over decorative chrome;
- compact high-information controls without spreadsheet aesthetics;
- rarity frames are readable but not excessively ornate;
- destructive/recovery actions are visually distinct from normal navigation.

Exact brand name/logo/icon remain separate identity assets and do not block implementation.

## 22. Title/opening presentation

Default production target: **animated/moving premium title presentation**.

It may show the current protagonist/World state or an authored dimensional/lobby composition without exposing hidden spoilers.

Requirements:
- lightweight reduced-motion/static fallback;
- fast path to Continue;
- recovery/import/settings remain clearly accessible;
- title presentation can be replaced/update-packaged without changing save architecture.

This closes the previous moving-vs-still presentation deferral.

## 23. Ruler Mode <-> World Mode transition

Orientation/mode transition should feel intentional, not like launching a second app.

Use a brief transition language based on:
- dimensional/map focus;
- camera/world handoff;
- character presence;
- UI reflow.

Transition duration is kept short and can be reduced for accessibility/performance.

The transition does not mask long synchronous loading; streaming/preload should do the real engineering work.

## 24. Scalability

Visual quality degrades gracefully by subsystem rather than globally destroying the art direction.

Candidate scalable dimensions:
- effect density;
- particles;
- hair/cloth simulation;
- shadow detail;
- crowd/NPC representation;
- reflection quality;
- post effects;
- environment detail/HLOD;
- animation update rates for distant actors.

Core character silhouette, critical telegraphs and gameplay-state readability remain protected.

Exact budgets are determined on the Samsung Galaxy S26 Ultra.

## 25. Asset-production contract

Character/world/VFX assets use stable content IDs and package ownership.

The runtime must permit:
- higher-quality replacement assets;
- additional animation packs;
- additional voice/music;
- promoted-NPC presentation upgrades;
- World-specific renderer profiles;
- package archival/streaming

without changing persistent entity identity.

## 26. Final freeze

The visual, VFX, animation and cinematic direction is complete enough for implementation and asset production.

Still intentionally open as content/device work:
- individual character designs;
- individual World palettes/assets;
- exact VFX budgets;
- exact material/shader complexity budgets;
- final logo/name/icon;
- per-character animation lists;
- final device quality settings.

Those are production catalogs or empirical performance values, not missing architecture.
