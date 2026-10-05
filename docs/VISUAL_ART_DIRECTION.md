# Visual Art / VFX / Animation / Cinematic Production Direction

STATUS: AUTHORITATIVE PRODUCTION-DIRECTION FREEZE.

This document closes the pre-coding visual-production direction. It defines the shared rendering, character, environment, VFX, animation, cinematic and UI presentation language while preserving radical variation between Worlds and characters.


## 1. Core visual identity

The production renderer is a **Painterly Anime PBR** style implemented as real-time 3D in Unreal Engine.

The authoritative overall target is approximately **70% anime/stylized and 30% grounded/realistic** in the resulting image. This is an art-direction ratio, not a literal shader blend.

Primary real-time reference stack:
- **Wuthering Waves** — modern real-time material quality, environmental integration, atmospheric depth and readable character presentation;
- **Honkai Impact 3rd** — anime facial construction, character readability, stylized combat presentation, animation/cinematic language and high-emotion staging;
- **Duet Night Abyss** — fantasy silhouette extremity, gothic/ceremonial design language, dramatic cloth/ornament, washed palettes and painterlier presentation.

Secondary visual influences:
- **Reverse: 1999** — washed/faded color, painterly/brush-softened finish, editorial composition and selective unresolved detail;
- **Punishing: Gray Raven** — severe action readability and darker visual edge;
- **Arknights / Path to Nowhere** — editorial seriousness, graphic confidence and controlled palette discipline;
- **NieR:Automata / Elden Ring** — environmental melancholy, negative space, monumental silhouette, ruin, atmosphere and beautiful severity.

The game is **not** targeting photorealism, realistic-human faces or flat cel shading. Unreal's physically coherent lighting/material systems support the image, but do not overrule anime design intent.

The intended read is:
> high-end anime fantasy first; unusually sophisticated physical lighting/material/world integration second.

Compared with the three primary references, the game should trend:
- less glossy and less generically clean;
- more washed/desaturated;
- more painterly/brush-softened;
- more melancholic and mature;
- more editorial in authored presentation;
- physically convincing without drifting toward realistic-human rendering.

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

Character rendering is deliberately more anime-weighted than the overall frame.

Approximate target balance by subsystem:
- face design/shading: ~80% anime / 20% grounded;
- eyes: ~90% anime / 10% grounded;
- hair: ~80% anime / 20% physical;
- body/anatomy: ~70% anime / 30% grounded;
- skin shading: ~65% stylized / 35% physical.

### Faces
Faces use clearly anime-authored geometry and proportion before materials are applied:
- stylized facial planes;
- larger, expressive eyes;
- restrained nose/mouth geometry;
- deliberately attractive and readable jaw/cheek construction;
- high-quality deformation without scanned-human realism.

A dedicated stylized face-shading treatment should:
- soften harsh physically correct facial shadowing;
- preserve designed cheek/nose/eye-socket readability;
- reduce photoreal microdetail;
- keep environmental light response and believable volume.

### Skin
Skin remains smooth, idealized and physically responsive:
- subtle subsurface response;
- environmental color pickup;
- wounds, dirt, wetness, bruising and species-specific states where valid;
- no pore-heavy scan look;
- no waxy photoreal presentation.

### Eyes
Eyes are strongly anime:
- layered iris/cornea depth;
- authored iris art;
- controlled highlights/specular;
- expressive state changes;
- no hyper-wet photoreal eye look.

### Hair
Hair is authored by silhouette and clump first:
1. major shape/mass;
2. secondary clumps;
3. flowing locks/ribbons;
4. selective fine strands.

Physics supports motion but must not destroy the intended anime silhouette. Important characters may receive richer secondary simulation while keeping the major authored form stable.


## 4. Materials and equipment

Materials use **stylized PBR**.

Approximate target:
- clothing: ~65% stylized / 35% physical;
- weapons/equipment: ~55% stylized / 45% physical;
- abnormal/supernatural materials vary by authored rule.

Metal, cloth, leather, crystal, bone, scales, machinery, magical matter and abnormal materials remain physically distinguishable, but avoid noisy photoreal texture treatment.

Preferred material behavior:
- broad readable highlights;
- intentional value grouping;
- simplified/controlled micro-normal detail;
- hand-authored roughness hierarchy;
- painterly breakup where appropriate;
- convincing wetness, grime, damage and environmental interaction.

Avoid a default Unreal look where every asset accumulates realistic micro-scratches, roughness noise and scan-like detail.

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

Characters must remain readable against highly variable Worlds without relying on a permanent cel outline.

Use contextual techniques such as:
- local contrast management;
- controlled contextual rim/fill;
- subtle depth/normal-based contour reinforcement;
- material-value separation;
- selective VFX suppression near silhouettes;
- distance-aware edge enhancement where useful.

There is **no heavy universal black contour**.

Any outline/edge treatment should usually be subtle enough to feel absent until disabled. Anime readability comes primarily from authored geometry, value design, lighting and silhouette.


## 7. Environment direction

Environment rendering is somewhat more physically grounded than character rendering while remaining visibly art-directed.

Approximate target:
- environment: ~55–60% stylized / 40–45% physical;
- lighting: ~55% art-directed / 45% physically grounded.

Primary environmental references:
- **NieR:Automata** — melancholy, negative space, restrained color and beauty in desolation;
- **Elden Ring** — monumental silhouette, layered history, ruin and authored architectural scale;
- **Honkai Impact 3rd Part 1** — emotionally staged spaces and willingness to become visually surreal during major metaphysical/story events.

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

Worlds should prioritize strong large shapes and readable mid-scale detail over universal microdetail. Silhouette and atmosphere matter before raw asset density.

Civilizations advance by deepening their own identity. A cultivation civilization should not visually converge into a generic modern city merely because it becomes advanced.

Cross-cultural influence appears only where actual history caused it.


## 8. Starting-world visual baseline

The starting World is an **original established-fantasy kingdom-region**, not a renamed simulation of real medieval history.

Visual priorities:
- stable and outwardly peaceful inhabited regions contrasted with credible danger;
- established but locally low-proficiency fantasy;
- believable settlements/materials without historical-simulation obligations;
- austere beauty, solitude and mystery;
- weather/time-of-day identity;
- humans may dominate local population without making the protagonist canonically human;
- dangerous wildlife, rare monsters and human/political violence remain visually grounded;
- ancient/endgame-dimensional spectacle is not foregrounded;
- stranger metaphysics emerge gradually.

Seed-generated regions use authored biome/style grammars so procedural combination still produces art-directed locations rather than noise.


## 9. Lighting

Lighting is cinematic, atmospheric and physically coherent, but explicitly subordinate to art direction.

Preferred situations include:
- overcast/diffused light;
- moonlight;
- fog-filtered light;
- sunset/dawn;
- selective warm interiors;
- strong breaks in cloud cover;
- contextual supernatural illumination.

Characters should belong to the environment rather than appear permanently studio-lit. Contextual readability aids are allowed, but should remain subtle.

Global rules:
- strong time/weather/world identity;
- authored exposure ranges;
- restrained bloom;
- no universal neon wash;
- no automatic "rarity = brighter environment" rule;
- preserve useful midtones and atmospheric whites;
- avoid unnecessary crushed blacks and uncontrolled HDR emissive clipping.

World laws may radically alter lighting when the fiction supports it.

High-order powers may alter sun/sky/shadow/material behavior when reality itself is being changed.


## 10. Color and palette

The global palette philosophy is **washed/faded/desaturated foundations with selective strong accents**.

Reference influence:
- Duet Night Abyss for pale/black/white foundations and controlled accent colors;
- Reverse: 1999 for faded pigment, print-like/editorial softness and restrained saturation;
- the selected controlled concept tests for desaturated environments with disciplined crimson/blue accents.

Typical foundations may include:
- ash gray;
- weathered white;
- charcoal/faded black;
- cold silver;
- stone blue;
- dusty violet;
- worn gold;
- muted earth/vegetation tones.

Then use one or two deliberate character/World-specific accents such as crimson, cobalt, violet, amber, turquoise, rose or other authored hues.

This should be achieved primarily through asset authoring, lighting and World palette profiles, not a single global post-process filter.

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

VFX is strongly authored/stylized (roughly ~80% stylized / 20% physical in normal presentation) and communicates **what rule is happening**, not simply how expensive the move is.

Niagara and material systems should favor strong readable shapes over undifferentiated particle density.

Reusable primitives include:
- ribbons/trails;
- brush-textured particles;
- ink/calligraphic streaks;
- decals and cracks;
- mesh effects/fragments;
- material overrides;
- flat graphic slices;
- volumetrics;
- lighting;
- sky changes;
- geometry deformation;
- spatial masks/field boundaries;
- post-process;
- environmental simulation cues.

Normal abilities should favor fewer, stronger elements rather than maximum particle counts.

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

Animation quality target is premium anime character-action animation, with **Honkai Impact 3rd** as a major production-language reference and Wuthering Waves / Duet Night Abyss as complementary readability/design references.

Principles:
- anime-action timing rather than constant realistic mocap weight;
- strong anticipation and follow-through;
- clean readable key poses;
- stylized acceleration/deceleration;
- responsive cancel windows according to mechanics;
- clear defensive telegraphs;
- personality in idles/transitions;
- signature Ultimates and World Fantasm activations fully bespoke.

Physical grounding remains important in:
- cloth;
- hair secondary motion;
- equipment inertia;
- impact response;
- environment interaction.

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

Story cinematics are primarily real-time/in-engine so current outfit, form, injury, equipment, world state and location state remain accurate.

In-engine cinematics may temporarily push presentation closer to premium key art through:
- stronger art-directed grading;
- selective painterly VFX;
- controlled depth of field;
- authored lens/camera language;
- stronger compositional lighting;
- deliberate exposure changes;
- brush/ink/graphic overlays where content supports them.

**HI3 Part 1** is a key reference for audiovisual/cinematic escalation and emotional staging, not for copying exact compositions or assets.

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

The primary empirical target remains the Samsung Galaxy S26 Ultra.

Visual quality degrades gracefully by subsystem rather than globally destroying the art direction.

Protect first:
1. anime face identity;
2. eye readability;
3. hair silhouette;
4. character materials and major costume shapes;
5. animation quality/readability;
6. critical combat/VFX telegraphs;
7. major environment silhouette and atmosphere.

Scale more aggressively where needed:
- distant microdetail;
- secondary reflections;
- minor hair/cloth simulation;
- small debris;
- particle counts;
- shadow resolution;
- distant NPC animation/update rate;
- environment detail/HLOD;
- noncritical post effects.

The chosen anime-first style should exploit art direction rather than maximal realism as the route to perceived quality.

Exact shader/material/VFX budgets remain device-profiled production values.

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

The renderer target is now explicitly frozen as **Painterly Anime PBR**, approximately **70% anime/stylized / 30% grounded-realistic overall**, using Wuthering Waves, Honkai Impact 3rd and Duet Night Abyss as the primary real-time 3D reference family, with the documented washed-palette, painterly/editorial and mature-atmosphere revisions layered on top.

Still intentionally open as content/device work:
- individual character designs;
- individual World palettes/assets;
- exact VFX budgets;
- exact material/shader complexity budgets;
- final logo/name/icon;
- per-character animation lists;
- final device quality settings.

Those are production catalogs or empirical performance values, not missing architecture.
