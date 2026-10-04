# UI, Controls and Presentation Architecture

STATUS: AUTHORITATIVE. Detailed UI decisions are normalized against `TUNING_UI_FREEZE.md`; if a concise summary here omits detail, the freeze document controls.

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


## Detailed UI authority normalization

The earlier working detailed-layout subsections were superseded by the completed global UI freeze and have been removed from this document to prevent two competing copies of the same contract.

Authoritative detailed UI:
- `TUNING_UI_FREEZE.md` for the full frozen interaction/layout contract;
- the normalized sections below for the concise production-facing summary.

## 15. Detailed character / roster UI freeze

The full detailed contract is frozen in `TUNING_UI_FREEZE.md`.

### Roster

- visual/card-first Character Identity roster;
- rarity-specific card borders/frames;
- search plus compact sort/filter;
- filters: Rank, Classes, Rarity, World Fantasm grade;
- sorting over the same major dimensions where meaningful;
- user-switchable density;
- default dense layout: three character cards per row (three columns), with the accepted 2/3-column density switch;
- one main roster tile per Character Identity;
- Manifestations remain grouped beneath Identity.

Favorite and Protected/Locked are separate concepts.

### Character Identity page

The user-supplied reference establishes a character-art-first composition: character presentation dominates the page while identity/progression/equipment/navigation information frames it.

Major bottom tabs:
1. Overview
2. Skills / World Fantasm
3. Equipment
4. Progression

Separate upper/utility destinations:
- Forms
- Manifestations
- History
- Adult

The current display follows the **last-used Manifestation**.

Manifestations support editable build labels and side-by-side comparison.

Internal combat formulas, alpha values and Rank-contribution decomposition are not exposed in player UI.

The main character page does not dump an exhaustive 1000+ learned-skill list. It shows the resolved/currently relevant kit and manageable grouped/searchable subsets.

## 16. Skins / wardrobe

Skins/outfits are first-class presentation content but do not automatically create a new Identity, Version, form or power state.

- compatible with body/form/Version constraints;
- cosmetic unless authored mechanics say otherwise;
- wardrobe access is contextual to the character presentation;
- archive/replay can filter by skin/outfit;
- compatible physical equipment can remain visible over/with a skin;
- removing all removable clothing/equipment reveals the actual underlying body/clothing state rather than forcing a fake permanent fallback costume.

## 17. Mature-content character UI

For canonically lore-adult characters, Adult is an always-accessible utility destination from the Character Identity page.

It may expose:
- profile;
- direct interaction;
- available systemic interactions;
- Archive-Replay.

Libido/preferences use descriptive presentation plus exact values where detailed inspection is useful.

Contextual world prompts remain subtle. Archive filters include participant, Version/form, outfit/skin and location.

Fast Privacy/SFW Presentation toggle is available from a convenient pause/profile surface.

## 18. Equipment UI / affinity / proficiency

Equipment layout follows the character's actual body/slot schema.

Affinity and proficiency are separate:
- both default to grades/stages in player UI;
- raw numeric values appear only when genuinely useful;
- affinity may use a continuous internal value plus authored milestones;
- affinity never means one universal +X% damage rule.

Equipment comparison includes resolved deltas, gained/lost skills/functions, compatibility, Item Rank/Quality/Evolution, affinity/proficiency implications and history.

Historically significant items receive a Chronicle-style history view.

Crafting UI has Known and Experiment modes.

## 19. Gacha UI

Banner page keeps featured presentation primary and directly shows:
- pull currency;
- compatible tickets;
- pity count;
- featured-guarantee state;
- pull controls;
- Details;
- History.

Details contains exact probabilities, pity behavior, pool and carry category.

Pull history is filterable by banner, Identity, rarity and date.

Multi-feature banners may expose an authored target/designation selector.

## 20. Territory / Records UI

Territory is map-first, with selected-Territory detail and quick access to Territory, Domain/Core, Dispatch, Projects, Armies/War and Logistics.

Territory and Domain remain distinct.

Large sovereignty navigation is hierarchical or fragmented when that is more usable.

Major analytical overlays are tap-to-enable and normally one major overlay is active at once.

Records always opens to its general chooser/hub rather than remembering the last internal tab.

Chronicle is timeline-first/filterable; Intelligence marks Confirmed/Estimated/Rumor/Contradicted/Outdated; Codex uses searchable entity categories.

## 21. World Mode HUD details

Companion switch portraits/status are placed on the **upper-right** in an HI3-like composition rather than next to the lower-right combat controls.

The controlled character receives the main HP/resource presentation.

Status icons are tappable for detailed information; hold is not required.

## 22. Number presentation

Player-facing numbers do **not** use scientific notation or x10^n notation.

Use integer/suffix notation (for example 6B = 6 billion) and extend the suffix system to larger magnitudes.

Stat/character screens have no fixed ten-character limit and can use the available width without absurdly shrinking text.

Floating combat numbers target roughly ten visible characters maximum:
- Unknown when knowledge does not permit precision;
- Exceed when the compact display range is exceeded.

For extremely large logical hit counts, do not show `value × N`. Resolve all hits authoritatively but repeat representative integer/suffix trigger popups only a few times for presentation.

## 23. Turn battle UI

Add 3x speed beside 1x/2x.

Use an upcoming-action timeline strip with advances/delays/interrupts.

Auto has quick presets and an advanced per-team conditional rule editor.

Normal player-facing recap is intentionally limited per character to:
- direct/active damage dealt;
- DoT damage dealt;
- total healing/restoration delivered by any means.

## 24. Opening / recovery / package UI

Use a real title/opening screen, not a bare utility menu. Final moving-vs-still treatment is deferred to the later presentation/art pass.

Primary opening actions:
- Continue;
- Recover Existing World;
- Import Backup;
- Settings.

Clear World is secondary/destructive.

Backup manager and package/storage manager expose the detailed state defined in `TUNING_UI_FREEZE.md`.

Large package downloads default to automatic download on Wi-Fi/unmetered connection.
