# Audio / Music / Voice / SFX Production Direction

STATUS: AUTHORITATIVE PRODUCTION-DIRECTION FREEZE.

This document closes the pre-coding audio-direction pass. It defines the player-facing audio language and production contract without freezing content-specific compositions, performances or device-tuned mix numbers.

## 1. Core identity

Audio should make the game feel like a premium character-action RPG inhabiting a persistent, physically coherent world rather than a menu-driven mobile gacha.

The default presentation combines:
- character-specific anime action readability;
- cinematic impact for high-order abilities;
- strong environmental physicality;
- World/civilization-specific musical identity;
- persistent-state continuity between Ruler Mode and World Mode.

No universal sonic palette is imposed on every World. The common identity comes from production quality, readable mix hierarchy, motif continuity and consistent systemic rules.

## 2. Music architecture

Music is authored around **World, civilization, character, faction and metaphysical concept identity**, not one global genre.

Each major World defines a music profile containing:
- core instrumentation families;
- harmonic/melodic vocabulary;
- rhythmic language;
- ambience/noise palette;
- acoustic/electronic balance;
- cultural or metaphysical constraints;
- permitted cross-World hybridization rules.

A historical-medieval starting region should sound grounded in its local cultures and materials. Later magical, post-apocalyptic, cybernetic, cosmic or abnormal-physics Worlds may radically change instrumentation and production while retaining motif continuity where narrative lineage exists.

## 3. Leitmotif system

Important characters, factions, Worlds, Domains, World Fantasms and recurring concepts may own motif identities.

Motifs are allowed to:
- fragment;
- invert;
- reharmonize;
- change meter/tempo;
- move between instruments;
- corrupt or stabilize;
- combine contrapuntally;
- become diegetic;
- disappear deliberately when absence itself is meaningful.

A motif is narrative state, not a mandatory melody pasted into every scene.

The content pipeline stores motif references and relationships so later arrangements can preserve continuity without hard-coding audio logic into gameplay systems.

## 4. Adaptive music states

The runtime music model uses composable authored layers/stems where useful.

Typical states:
- safe exploration;
- uncertain/danger exploration;
- ordinary combat;
- elite/boss escalation;
- phase transition;
- victory/recovery;
- Domain / World Fantasm activation;
- overlapping reality-field conflict;
- catastrophic world event;
- Ruler Mode / strategic consequence state.

Transitions should preserve musical continuity when practical rather than restarting tracks on every state change.

Combat intensity may add or remove layers, change arrangement, or transition to a related cue. High-order reality effects may temporarily dominate or deform the current music rather than always replacing it with a separate track.

## 5. Domain / World Fantasm audio

Reality fields alter sound according to their actual rules.

Possible authored channels include:
- music stem dominance or suppression;
- spectral filtering;
- reverberation/space changes;
- altered propagation;
- impossible directional behavior;
- pitch/time distortion;
- transformed environmental loops;
- motif takeover;
- selective silence;
- material/physics-dependent impact changes.

There is no universal "World Fantasm sound" or Rank aura sound.

When multiple fields overlap, audio follows the same mechanical interaction principles as visuals: boundaries, dominance pockets, mixed zones, fracture and negation may all produce distinct results.

## 6. Voice direction

Primary authored spoken language: **Japanese**.

Coverage target:
- major playable characters and major story roles: full or near-full coverage;
- important promoted NPCs: bespoke coverage when promoted importance warrants it;
- ordinary procedural NPCs: reusable voice families, contextual barks and partial coverage;
- protagonist free-text/player-selected ordinary dialogue: normally unvoiced;
- authored key protagonist lines/barks: may be voiced.

Performance direction favors character authenticity and emotional specificity over generic archetype delivery.

Combat voice should use multiple variants and cooldown/throttling rules so frequently repeated actions do not become exhausting.

Critical information must never depend on spoken dialogue alone.

## 7. NPC voice scalability

Ordinary NPC voice families are organized by relevant physical/cultural attributes rather than by one global male/female bucket.

Reusable families may vary by:
- species/anatomy;
- age range where lore-appropriate;
- culture/region;
- vocal physiology;
- social register;
- emotional temperament.

Pitch/formant processing may support variation only within ranges that still sound intentional. Important recurring NPCs should not remain obviously generated from repetitive stock barks once their narrative importance increases.

Promotion can attach a richer bespoke voice package without replacing the NPC's persistent identity.

## 8. SFX grammar

SFX is organized by causal source, material and power expression.

Core families include:
- weapon/contact impacts;
- body/material impacts;
- movement/traversal;
- guard/parry/perfect dodge;
- skill resources/charges;
- UI interaction;
- environment/weather;
- machinery/technology;
- magic/energy/metaphysical effects;
- Authority/Domain/World Fantasm effects;
- destruction and persistent aftermath.

The same nominal action can sound radically different when anatomy, material, environment or reality rules differ.

High power does not always mean louder or denser. Extremely high-order effects may become cleaner, quieter or unnaturally sparse when that better communicates law-level control.

## 9. Combat readability hierarchy

The mix prioritizes:
1. player-critical telegraphs and defensive timing cues;
2. the controlled character's action confirmation;
3. lethal/major enemy actions;
4. important party/QTE/Ultimate cues;
5. ordinary combat impacts;
6. non-critical ambience.

Presentation spectacle may temporarily exceed this hierarchy only when player control is suspended or the mechanic is already resolved.

Perfect dodge, parry, Ultimate readiness and major reality-field collisions should have distinct recognizable signatures.

## 10. Spatial/environmental audio

World Mode audio remains spatial and physically grounded.

Environmental behavior may account for:
- interior/exterior transition;
- occlusion;
- distance;
- terrain/material;
- underwater;
- vacuum or low-medium environments;
- caves/large halls;
- weather;
- enormous entities;
- distant warfare;
- dimensional anomalies.

World laws may explicitly override ordinary acoustics. Such overrides are authored mechanics, not random DSP decoration.

## 11. Ruler Mode audio

Ruler Mode is lower-density than World Mode.

It uses:
- lobby/character-focused ambience;
- subtle world-state adaptation;
- restrained UI feedback;
- reports/urgent-event cues;
- character presence where contextually appropriate.

It should not sound like a generic productivity dashboard.

Switching between Ruler Mode and World Mode should preserve thematic continuity while allowing heavy 3D/spatial systems to unload.

## 12. UI audio

UI sounds are concise, premium and low-fatigue.

Distinct semantic families exist for:
- navigation;
- confirm;
- back/cancel;
- destructive confirmation;
- acquisition/gacha;
- rarity/progression breakthrough;
- error/invalid;
- urgent report;
- package/download/recovery status.

Rarity feedback may be distinctive but does not impose one universal Rank color/sound ontology.

## 13. Mature-content / privacy presentation

Privacy/SFW Presentation masks presentation only; it does not rewrite canonical character/world state.

Audio content packages follow the same lore/content eligibility rules as the visual pillar.

When Privacy/SFW Presentation is active, explicit presentation assets are replaced/suppressed according to the authored presentation profile without mutating underlying progression/history.

## 14. Mix and accessibility

Player-facing controls include:
- master;
- music;
- voice;
- SFX;
- ambience;
- optional dynamic-range profile;
- haptics intensity/off where platform-supported.

Subtitles/captions remain available for meaningful speech and critical non-speech cues.

Reduced-audio or muted play must remain mechanically viable through visual/haptic redundancy.

Exact loudness, compression, codec and memory budgets are engineering/device-profiled values rather than Creative Director questionnaire items.

## 15. Haptics relationship

Haptics and audio share semantic event identities but are independently configurable.

Distinct haptic families may accompany:
- parry;
- perfect dodge;
- heavy hit;
- Ultimate;
- summon;
- World Fantasm/Domain collision;
- major destruction.

Haptics never become required to understand a mechanic.

## 16. Production assets and runtime contract

Master production assets should be retained at high quality and transcoded/streamed for Android through content packages.

Content definitions reference stable audio IDs. Gameplay code requests semantic events/states; it does not hard-code raw file names.

Adaptive cues, motifs, language variants and package dependencies must remain replaceable/updateable without save migration.

## 17. Final freeze

The audio architecture and production direction are complete enough for code and asset production.

Still intentionally open as content/tuning:
- individual compositions;
- individual character performances;
- exact World instrumentation catalogs;
- final mix/loudness/codec settings;
- per-device streaming/memory budgets.

Those are production content or empirical engineering values, not missing architecture.
