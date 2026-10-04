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


## 15. Character-screen detailed layout

### Main roster

The Characters destination shows **one card per Character Identity**, not one card per Manifestation.

Roster density is switchable between 2-column and 3-column layouts, with 3-column as the default on the target phone.

Each identity card uses:
- character portrait/card visual;
- name;
- rarity-specific border treatment;
- current selected/last-used Manifestation's Rank/Level and Current Rarity where useful;
- owned Manifestation count;
- compact Fantasm/Rank markers when space permits.

The border is driven by the relevant displayed Rarity state and must remain readable without becoming excessive visual noise.

Roster filters:
- Rank;
- Classes;
- Rarity;
- World Fantasm grade.

Sorting supports the same major axes where meaningful.

Do not add a universal role filter merely to imitate conventional RPG archetypes.

### Identity page composition

The Identity page is visually character-first rather than stat-sheet-first.

Primary tabs:
1. **Overview**
2. **Skills / World Fantasm**
3. **Equipment**
4. **Progression**

Upper utility icons:
- **Forms**
- **Manifestations**
- **History**
- **Adult** for lore-adult identities

The upper portion prioritizes the currently used Manifestation's character presentation, with access to official Version visuals, compatible Skins and 3D presentation where available.

Manifestation switching uses **last-used context** rather than a globally forced Preferred/Strongest copy. Party, Dispatch and other contexts remember the Manifestation last used there.

### Manifestations

Manifestation cards expose:
- editable build label;
- Rank/Level;
- Current Rarity;
- major route state;
- form;
- important equipment;
- current deployment/assignment state.

Two Manifestations can be selected for comparison.

No automatic "strongest Manifestation" replacement overrides the user's last-used choice.

### Stats

Default stat view shows resolved player-facing values only.

Do **not** expose internal formulas, alpha coefficients, Rank-contribution decomposition or engineering calculation internals in normal UI.

A detailed player view may show useful sources such as equipment/effect deltas and current modifiers, but not the hidden mathematical construction.

### Progression

Routes, Factors, Classes and transformations use a zoomable graph/tree interface where appropriate.

Unknown/secret nodes remain hidden or knowledge-limited.

### Skills

A character may eventually own 1000+ learned skills. Do **not** render the full library as one giant default list.

The Skills/World Fantasm tab focuses on:
- resolved/equipped active kit;
- passives currently relevant to the build;
- World Fantasm;
- major signature/integrated skills;
- search/filter access to deeper learned-skill records only when requested.

Use lazy loading, categorization and provenance drill-down for very large skill libraries.

### History

History records meaningful persistent milestones rather than trivial battle spam.

## 16. Adult/private character UI

For every lore-adult Character Identity, the Adult upper utility icon is directly accessible.

The section supports:
- Profile;
- direct interaction entry where appropriate;
- available systemic interactions;
- Archive/Replay.

Libido/preferences presentation uses **descriptive grades/text by default**, with exact values available only in detailed inspection where the precise number is mechanically useful.

World/Ruler Mode adult prompts remain contextual rather than giant permanent HUD buttons.

Archive/Replay supports filtering by:
- participants;
- Version/form;
- outfit/Skin;
- location;
- historical vs current appearance.

The replay UI remembers the last presentation preference.

A fast Privacy/SFW presentation toggle is available from a readily accessible pause/profile surface in addition to Settings.

## 17. Equipment / crafting UI

Equipment UI follows the character's **actual body/slot schema** rather than forcing Helmet/Armor/Boots onto every character.

Equipped pieces are presented around/alongside the character/body where practical.

Removing all removable clothing/equipment can leave the character visibly naked when the character/assets/state permit it.

Affinity is shown primarily through named grades/milestones and optional progress; exact hidden values are not normally exposed.

Proficiency is presented separately, normally by grade; exact numerical values are shown only when useful to the relevant mechanic.

Equipment comparison shows:
- player-facing stat deltas;
- gained/lost skills;
- compatibility warnings;
- Rank/Quality/Evolution;
- affinity implications;
- relevant history.

Named/historical items have a Chronicle-style history panel.

Crafting provides:
- **Known** deterministic processes;
- **Experiment** mode for material/technique/facility/desired-function/appearance/property-driven invention.

## 18. Gacha screen

Banner screen presents:
- featured banner visual;
- title/state/time where relevant;
- available currency and compatible tickets;
- pity count;
- guarantee state;
- 1-pull / multi-pull controls;
- Details icon;
- History.

Pity and guarantee are visible directly.

Detailed declared probabilities, soft-pity behavior, exact eligible pool and carry category are placed behind the **Details** icon rather than cluttering the primary banner view.

Pull History is persistent and filterable.

Multi-feature banners may use an authored target/designation selector when that banner defines one.

Compatible tickets are auto-consumed before pull currency.

## 19. Territory / strategic presentation

Territory destination is **map-first**.

Selecting a Territory opens contextual detail and access to:
- Territory;
- linked Domain/Core;
- Dispatch;
- Projects;
- Armies/War;
- Logistics.

Territory and Domain remain semantically distinct.

Large sovereignties use hierarchical/fragmented navigation rather than forcing the full cosmology onto one map:
- Reality/Dimension;
- World;
- region/Territory;
- optional fragmented/paged strategic sectors where appropriate.

Major map overlays are individual tap-to-enable modes such as control, logistics, threat, resources, Projects and Dispatch.

## 20. Records landing behavior

Entering Records always returns to the **general category chooser** for Reports / Chronicle / Codex / Intelligence rather than restoring the last internal tab.

Chronicle is timeline-first and filterable.

Intelligence visibly distinguishes:
- Confirmed;
- Estimated;
- Rumor;
- Contradicted;
- Outdated.

Codex shares searchable categories for characters, observed Manifestations, species/creatures, Worlds/locations, powers/effects, items and factions.

## 21. World Mode combat HUD placement

The immediate companion/switch portraits sit in the **upper-right** area of the landscape combat screen, following the preferred HI3-like spatial grammar rather than beside the right-side attack controls.

The controlled character receives the primary HP/resource presentation.

Enemy/boss HUD uses one principal target panel, with smaller secondary indicators for additional targets.

Statuses use compact icon/stack/turn presentation. Tapping the effect opens detailed grade, turns, Authority and authored behavior. Source information does not require a hold gesture.

## 22. Large-number presentation

Do not use scientific notation or explicit x10^n notation in normal player-facing UI.

Use integer/abbreviated suffix notation:
- 6B for six billion;
- equivalent suffix families for larger values.

Stat screens may use the full available screen width and do not have a fixed digit-count limit.

Battle/on-field values remain compact with a practical target around **10 displayed characters/digits**. If a value cannot be represented readably within the current combat presentation, show a higher-order abbreviation or an **Unknown / Exceeds** style state as appropriate rather than scientific notation.

Typography must remain normal game-UI typography; do not shrink text to absurdly tiny sizes just to expose every digit.

Do not aggregate overflow hits into a synthetic "x2000" label. Preserve integer hit presentation and show repeated trigger numbers only a manageable number of times visually, while the authoritative resolver may batch internal computation for performance.

## 23. Turn-combat presentation

Turn battle supports 1x / 2x / **3x** speed.

Timeline uses an upcoming-action strip with advances, delays and interrupt insertions.

Auto-combat UI provides:
- quick presets such as Aggressive / Safe / Boss / Farm;
- advanced per-team conditional rules.

Battle recap is intentionally compact and character-centric.

Per character, show only:
- active/direct damage dealt;
- DoT damage dealt;
- total healing/sustain delivered through any means, including direct healing, regeneration/vampirism-derived support where attributable.

Do not expand the default recap into dozens of analytical subcategories.

## 24. Opening / recovery / package UI

The game has a real opening screen rather than booting directly into a dashboard.

Presentation may be animated/moving like a premium live title screen or still/illustrated like a high-quality static title screen, depending on final production assets.

Primary action:
- Continue

Secondary:
- Recover Existing World;
- Import Backup;
- Settings.

Destructive Clear World/New World actions are not promoted as normal primary actions.

Backup manager shows snapshot date, world progress, schema/build version and integrity state with explicit Export/Import/Restore actions.

Package manager exposes installed Worlds/regions/characters/media packages, size, version, storage location, update state and move/archive controls.

Large package downloads default to Wi-Fi/unmetered automatic download, configurable in Settings.
