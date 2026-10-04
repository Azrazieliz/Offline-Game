# UI, Controls and Presentation Architecture

STATUS: ARCHITECTURE FROZEN FOR RECONCILIATION.

## 1. Dual-mode navigation

The game keeps the established portrait Ruler Mode / landscape World Mode split, with automatic orientation switching by default and a player-accessible orientation lock for accessibility/use preference.

Ruler Mode has five primary bottom destinations:

1. **Home**
2. **Characters**
3. **Gacha**
4. **Territory**
5. **Records**

### Territory destination

Territory is the strategic action destination rather than one overloaded "Territory/Domain" label.

Its internal navigation separates:
- Territory;
- Domain/Core;
- Dispatch;
- Projects/construction/repair;
- armies/war controls;
- logistics/resources;
- subordinate-Ruler requests;
- strategic map and other sovereignty actions.

The strategic map remembers its last position, zoom and overlay state, with a one-tap return to Main Territory.

### Records destination

Records groups:
- Reports;
- Chronicle;
- Codex;
- Intelligence.

These remain distinct internal views under one top-level destination.

## 2. Home

Home remains deliberately low-information and character/lobby focused rather than becoming a dashboard.

The protagonist/Ruler remains the primary on-screen character. Owned characters may appear contextually. World/Domain state may subtly affect ambiance, decoration, lighting and presentation while the interaction grammar remains stable.

Urgent matters surface as contextual notices rather than permanently cluttering Home.

## 3. Character roster UX

Multiple Manifestations of one Character Identity are grouped under one Identity page.

The Identity page can expose:
- shared canonical information;
- Versions;
- owned Manifestations;
- divergent builds/routes;
- progression;
- equipment;
- skills;
- forms;
- Grand Convergence requirements/progress when known.

Copies are not presented as unrelated duplicate characters.

## 4. Reports and Android notifications

The in-game Reports system remains the authoritative information history.

The Android app may also emit real local Android notifications for player-relevant events such as:
- Dispatch completion;
- Project completion;
- banner changes;
- urgent attacks/crises;
- recovery events;
- other configured reports.

Notifications are acknowledgement-based rather than red-dot spam. Reading/dismissing a notification does not alter the underlying world event.

Known future events can be scheduled locally. Unpredictable background world events may use best-effort WorkManager/background catch-up when Android permits it; the architecture does not require an always-running hidden service and must tolerate OS background restrictions.

Privacy controls may suppress notification content or all notifications without altering world state.

## 5. World Mode HUD

The default World Mode HUD uses a **small knowledge-limited minimap**, with full map available separately.

Only known/mapped information appears.

HUD elements fade/minimize outside relevance and expand contextually for:
- combat;
- low HP;
- target lock;
- hazards;
- interactions;
- traversal state.

Damage numbers are **visible by default** and can be customized/disabled.

Enemy HP/state precision is knowledge-dependent. Unknown beings may initially expose only rough condition until observation/appraisal/research makes exact information justified.

Interaction prompts are minimalist/contextual rather than persistent icon clutter.

## 6. Difficulty

There is one **canonical global difficulty**. No Easy/Normal/Hard profile changes world rules.

No hidden rubber-banding or global player-level scaling exists.

Specific content may possess its own authored adaptive/escalating difficulty when that is part of the location/system itself:
- growing labyrinths;
- challenge realms;
- survival towers;
- reactive training spaces;
- similar content.

Such systems remain bounded by their actual world/realm metaphysics. A low-order starting-realm dungeon does not spontaneously become hyperdimensional merely because the protagonist grew stronger.

## 7. Controls and accessibility

Touch controls support:
- repositioning;
- scaling;
- opacity;
- left-handed layouts;
- hold/toggle alternatives;
- camera sensitivity;
- deadzones;
- motion-reduction options;
- color/readability alternatives;
- subtitle configuration;
- configurable haptics.

Android gamepads/controllers are first-class supported inputs.

Haptics may use different signatures for parry, perfect dodge, Ultimate, World Fantasm/Domain collision, heavy impacts and similar events, with intensity/off controls.

## 8. Visual rendering direction

Characters use a **high-end anime/PBR hybrid** presentation.

Environment rendering may vary drastically by World/Dimension when needed while remaining technically compatible with the renderer.

No requirement forces every character into one humanoid body silhouette. All characters, not only important characters, may use materially different anatomy/forms when their content supports it:
- wings;
- tails;
- multiple limbs;
- mechanical bodies;
- nonhuman lower bodies;
- living armor;
- other body plans.

Outlines are subtle/contextual by default rather than a heavy universal contour.

## 9. Equipment visuality

Physically worn equipment normally appears on the model unless an actual state/ability hides, transforms or internalizes it.

Equipment families are role/body appropriate rather than one universal armor template. An archer, monk, mage, dragon-bodied being, armored knight or cybernetic character can have fundamentally different equipment schemas.

Outfits use modular layers where feasible:
- body/base;
- underlayers;
- clothing;
- armor;
- accessories;
- state-specific overlays.

This supports equipment changes, transformation and damage without replacing the entire character mesh.

## 10. Civilization visual identity

Civilizations preserve and deepen their authored genre/design language as they advance.

Their architecture, clothing, weapons, transport, infrastructure, symbols, materials and institutions may become dramatically grander without converging toward generic contemporary cities.

## 11. VFX and reality fields

High-order power should change the **world itself** when appropriate rather than merely adding larger particles.

Possible presentation channels include:
- lighting;
- sky;
- geometry;
- materials;
- spatial distortion;
- weather;
- sound;
- physics;
- rule visualization.

There is no universal Rank color/aura code.

World Projection / Alteration / Materialization / Manifestation / Negation share behavioral visual principles, not one color/template.

Overlapping Domains/Fantasms can show:
- boundaries;
- blending;
- fractures;
- dominance pockets;
- collision fronts;
- mixed-rule zones.

Extreme power may become visually quieter when a clean law change is more appropriate than spectacle.

Persistent consequences remain visually present when their actual state persists.

## 12. Animation production

Use **HI3rd-style animation production**:

Shared compatible rig and locomotion foundations are used where technically appropriate, but combat, movement expression, transitions, personality motion and moment-to-moment animation are heavily character-specific. Signature actions, Ultimates, World Fantasm activation and other defining moments receive fully bespoke animation so playable characters feel fundamentally distinct rather than like reskins.

Nonstandard anatomies receive genuinely different locomotion/combat sets when humanoid retargeting would be inappropriate.

## 13. Cinematics

Ultimates, summon sequences and World Fantasm/Domain activations use full presentations by default, with player parameters for shortening/repetition/auto behavior.

Story cinematics are primarily real-time/in-engine so current outfits, forms, injuries and world state remain accurate. Illustrated/prerendered sequences remain available when artistically justified.

Major first-time story cinematics are not designed around encouraging skipping. A skip control remains available for player autonomy/accessibility, but can require a deliberate hold/confirmation; replayed content may be skipped freely.

Dialogue scenes use dynamic 3D staging/camera work where appropriate, with VN/illustrated presentation available when stylistically superior.

## 14. Voice, audio and music

Primary spoken language: **Japanese**.

Voice coverage target:
- major characters/story: full or near-full;
- ordinary procedural NPCs: reusable voice families, barks and partial coverage;
- promoted/important NPCs: eligible for bespoke voice upgrades.

Player-selected/free-text protagonist dialogue is normally unvoiced; authored key protagonist lines/barks may be voiced where appropriate.

Music is strongly World/civilization-specific.

Important characters, factions, Worlds and major concepts may use leitmotifs that transform across development, corruption, recontextualization and reality-field states.

Domains/World Fantasms may alter the live music mix and motif dominance.

Environmental audio remains spatial/physical across interiors, weather, underwater, vacuum, dimensional anomalies, distant battles and enormous entities.

The composition pipeline supports authored musical briefs, leitmotif architecture, harmonic/melodic/instrumentation plans, adaptive-state maps and production specifications. Final rendered audio requires the corresponding music-production assets/tools.
