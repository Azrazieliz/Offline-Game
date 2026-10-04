# Tuning and Detailed UI Freeze

STATUS: AUTHORITATIVE PRE-AUDIO / PRE-ART-DIRECTION CHECKPOINT.

This checkpoint records the Creative Director's completed global numerical/UI questionnaire after the architecture freeze and contradiction audit. It supersedes older OPEN/provisional wording on the same details.

Music, voice/SFX production design and visual art-direction production are intentionally not re-opened here.

## 1. Rank / stat progression

- Keep the current common Rank candidate:
  `R(r) = 10^(0.40r + 0.040r²)`, Mortal r=0 through Primordial r=16.
- Within one Rank step, target approximately **35% of logarithmic growth across Levels 1-100 and 65% at the actual Rank breakthrough**.
- The within-Rank Level curve is **character-specific**, normalized to the shared 35% envelope rather than one universal linear curve.
- Character/stat-specific breakthrough bias may exceed the smooth Rank projection when the actual transformation/lore warrants it.
- Magnitude stats use character/stat-specific Rank response. Alpha is authored per character/stat within the already frozen positive-decimal domain.
- Stats, alpha values, Rank contributions and internal formula decomposition are **not exposed to the player as formulas**. Player UI shows resolved useful values only.

## 2. DEF / Rank Suppression

- Keep progressive positive DEF:
  `DEF >= 0: M = DefenseReference / (DefenseReference + DEF)`.
- Generic negative-DEF tuning candidate is accepted:
  `DEF < 0: M = 2^(-DEF / DefenseReference)`,
  while character/skill/content rules may author another base/reference.
- There is no universal damage ceiling.
- First Rank-Suppression tuning envelope remains approximately 100 / 100 / 75 / 40 / 15 / 3% at effective gaps 0 / 1 / 2 / 3 / 4 / 5+, but actual values remain channel-specific for damage, control, effect penetration, perception, presence/environment tolerance and resistance breaking.

## 3. GP / Power

- GP/Power remains a visible **summary estimate only** and never directly determines combat.
- Do **not** display the raw logarithm as GP. A result such as "32 GP" beside quadrillion-scale stats is unacceptable.
- Engineering may compute capability safely in log-domain internally to prevent overflow, but the player-facing GP is converted back into the game's ordinary large-number scale/suffix presentation.
- Exact weighting/calibration remains simulator work and may intentionally misestimate unusual/high-synergy kits.

## 4. Progression economy

- General scarcity gradient is accepted:
  acquisition < ordinary development < Current-Rarity promotion < high Rank breakthroughs < rare route completion < Transcendence < Grand Convergence,
  while actual characters/mechanics may legitimately violate the ordering.
- There is no universal real-time "X hours per Rank" rule. Progression is causal/content-specific; internal cost/time bands exist only for balance validation.

## 5. Gacha probability, income and tickets

Standard limited-banner candidate is accepted for the current simulation baseline:
- 1.2% base top-Origin-Rarity probability;
- soft pity after 50;
- +5 percentage points per pull from 51-69;
- hard pity 70;
- 50/50 featured then guaranteed;
- compatible pity/guarantee carry;
- ten-pull floor at least SR+ unless the banner authors a stronger floor.

Progression-scaled acquisition farming remains source-based rather than a hidden account multiplier.

Increase the previous acquisition-focused simulator bands by roughly **+1 to +2 pull-equivalents/hour**:
- newly qualified Ruler: approximately **4-8/h**;
- established Ruler/Domain: approximately **7-14/h**;
- ordinary Overlord-scale economy: approximately **14-27/h**;
- extreme late-game / dedicated high-order farming: approximately **27-52+/h** where the actual content/sovereignty justifies it.

These are economy-simulation bands, not guaranteed wages.

Ticket taxonomy:
- standard single-pull;
- multi-pull;
- event/dimensional/banner-compatible;
- guaranteed-rarity/restricted-pool;
- exceptional selector/designation tickets.

Tickets normally **do not expire**. Event-specific tickets remain stored for compatible reruns unless that ticket's actual mechanic explicitly says otherwise.

When currency and compatible tickets can both pay for an ordinary pull, **consume tickets first automatically**. Currency is used after compatible tickets are exhausted.

## 6. Character roster UI

The user-supplied roster screenshot is an authoritative **composition/reference target**, not a requirement to clone another game's branding.

Target behavior:
- visual/card-first portrait roster;
- rarity-specific card border/frame;
- search control prominently available;
- compact filter/sort access;
- user-switchable density remains supported;
- default dense roster uses **three character cards per row (three columns)** rather than the four-across composition shown in the supplied reference; the previously accepted 2/3-column density switch remains available;
- one main roster tile per Character Identity, not one unrelated tile per Manifestation;
- card exposes only compact useful state (name, relevant rarity/Rank indicators, selected/last-used Manifestation cue, etc.) without turning the tile into a spreadsheet.

Filter by:
- Rank;
- Classes;
- Rarity;
- World Fantasm grade.

Sort by the same major dimensions where meaningful.

Favorite and Protected/Locked remain separate:
- Favorite = pin/filter/UI preference;
- Protected = prevents destructive/Convergence actions without explicit confirmation.

## 7. Character Identity page

The user-supplied character-detail screenshot is the composition reference:
- full character presentation dominates the screen;
- identity/rarity/Rank/status information remains compact around the art;
- controls and equipment/status information frame the character instead of replacing the art with a spreadsheet;
- side/upper utility icons may expose secondary views;
- bottom navigation drives the major character-management sections.

### Major bottom tabs

1. **Overview**
2. **Skills / World Fantasm**
3. **Equipment**
4. **Progression**

### Visually separate upper/utility destinations

- **Forms**
- **Manifestations**
- **History**
- **Adult**

These are not mixed into the main bottom-tab row.

The selected/current presentation uses the **last-used Manifestation** rather than maintaining a separate universal "Preferred" copy.

Manifestations show:
- editable build label;
- Rank/Level;
- Current Rarity;
- major route/form state;
- relevant equipment;
- assignment/deployment state.

Two Manifestations can be selected for side-by-side comparison of resolved stats, route state, skills and equipment.

The UI never automatically replaces the user's selected copy with a calculated "strongest" copy.

## 8. Character stats / progression UI

- Default stat view is compact and player-facing.
- A detailed stat view may expose more resolved values/comparisons, but **never exposes internal formulas, alpha values, Rank-factor decomposition or implementation coefficients**.
- Progression/routes use a zoomable graph/tree where appropriate.
- Hidden/unknown nodes remain hidden or knowledge-gated.
- A character may possess 1000+ learned techniques; do **not** dump a complete giant skill library into the main character UI.
- Skills / World Fantasm focuses on the resolved combat kit, relevant integrated techniques, current forms/field rules and manageable grouped/searchable subsets where needed.
- Meaningful character History includes acquisition, breakthroughs, route choices, major equipment, Convergence provenance and important world/story events; trivial battles are omitted.

## 9. Skins / outfits / clothing layers

Skins are added as a first-class presentation system.

- A skin/outfit is not automatically a new Character Identity, Version, form or power state.
- Skins may be cosmetic only unless their authored content explicitly carries mechanics.
- Skin compatibility can depend on body/form/Version.
- Wardrobe/skin selection is accessible from the character presentation without becoming a major bottom tab.
- Archive/replay filtering can select outfit/skin.
- Visible equipment continues to appear when compatible with the chosen outfit/skin and actual equipment state.
- Clothing/equipment remains physically layered. If every removable clothing/equipment layer is removed, the character is not forced into an artificial permanent base outfit; for content where explicit nudity is valid, the actual body is exposed.

## 10. Mature/adult character UI

For every canonically lore-adult character:
- Adult is an always-accessible character utility section;
- world/contextual adult interactions still exist independently;
- the section supports **Profile / direct interaction / available systemic interactions / Archive-Replay** as appropriate rather than only one flat scene list;
- libido/preferences use both descriptive presentation and exact values where detailed inspection is genuinely useful;
- contextual world prompts remain natural/subtle rather than giant explicit HUD buttons;
- archive/replay supports historical/current appearance and filtering by participant, Version/form, outfit/skin and location;
- replay remembers the last presentation preference;
- a fast Privacy/SFW Presentation toggle is available from a convenient pause/profile surface in addition to Settings.

This does not add a relationship/affection access gate.

## 11. Equipment / affinity / proficiency UI

Equipment UI follows the character's **actual body/slot schema**, not a universal helmet/chest/boots template.

The character remains visually central while equipped pieces are presented around/alongside them, following the supplied character-detail reference where useful.

Unequipping/removing all removable visible clothing/equipment results in the actual underlying body/clothing state rather than an invisible fallback equipment costume.

### Affinity

The established owner-item affinity system remains.

Chosen AYL presentation/default:
- authoritative affinity may use a continuous internal value plus authored milestones;
- player-facing presentation defaults to **grades/stages and milestone state**, not a mandatory raw percentage;
- numeric detail is shown only where genuinely useful;
- affinity is not a universal +X% damage formula;
- item definitions decide what affinity changes/unlocks/evolves.

### Proficiency

- Proficiency is separate from affinity.
- Player-facing proficiency also defaults to **grades**, with raw numbers only when they convey useful information.
- A character can have high proficiency with a newly acquired low-affinity item.

Equipment comparison shows:
- useful resolved stat deltas;
- gained/lost skills/functions;
- compatibility warnings;
- Item Rank and Quality;
- Evolution state;
- affinity/proficiency implications;
- relevant item history.

Historically significant/named items receive a Chronicle-style history view.

Crafting has:
- **Known** mode for established recipes/processes;
- **Experiment** mode for genuine invention using materials, facility/technique, intended function, appearance and desired properties.

## 12. Gacha UI

Banner page:
- featured character/Version presentation is primary;
- banner state/time/context;
- visible pull currency and compatible tickets;
- **pity count and featured-guarantee state always visible**;
- pull controls;
- Details icon;
- History access.

The **Details icon** exposes:
- exact declared probabilities;
- soft/hard pity behavior;
- eligible Identity/Version pool;
- carry category;
- special ticket/pool rules.

Pull history persists and can filter by banner, Identity, rarity and date.

Multi-feature banners may use an authored target/designation selector when that banner defines one.

No new mockup/image is generated at this stage.

## 13. Territory / Domain strategic UI

Territory main destination is **map-first**.

Selected Territory opens a detail/bottom sheet plus quick access to:
- Territory;
- Domain/Core;
- Dispatch;
- Projects;
- Armies/War;
- Logistics.

Territory and Domain remain separate concepts.

Selecting a Territory can expose its linked Domain/Core contextually; a dedicated Domain list remains available for large sovereignty structures.

Large-scale navigation is hierarchical or **fragmented when more usable**, e.g.:
Reality/Dimension -> World -> Region/Territory,
rather than forcing all cosmology onto one giant map.

Major analytical overlays are tap-to-enable and generally one major overlay is active at a time:
- control;
- logistics;
- threat;
- resources;
- Projects;
- Dispatch;
- other authored strategic layers.

## 14. Records UI

Records always opens to the **general Records chooser/hub**, not the previously used internal tab.

The chooser exposes:
- Reports;
- Chronicle;
- Codex;
- Intelligence.

Chronicle:
- timeline-first;
- filterable by character, faction, Territory, World, event type and importance;
- meaningful events only.

Intelligence visibly distinguishes:
- Confirmed;
- Estimated;
- Rumor;
- Contradicted;
- Outdated;
with source/provenance available when known.

Codex uses searchable categories for Character Identities/observed Manifestations, creatures/species, Worlds/locations, powers/effects, items, factions and other entity families.

## 15. World Mode HUD / combat presentation

The immediate party remains protagonist + up to two companions.

Companion switch portraits/status sit on the **upper-right side of the screen**, following the general HI3-like placement reference rather than clustering them around the lower-right combat controls.

The controlled character receives the main HP/resource presentation.

Enemy/boss UI:
- one principal target panel;
- HP/state/status/phase/resource detail only when knowledge permits;
- smaller secondary indicators for additional targets.

Status effects use compact icon + stack/turn presentation. **Tap** opens precise effect information; holding is not required.

## 16. Large-number presentation

Do **not** use scientific notation or `x × 10^n` in player-facing presentation.

Use ordinary integer/suffix notation:
- e.g. **6B = 6 billion**;
- continue with a scalable suffix system for larger magnitudes.

### Stat/character screens

- No fixed 10-character ceiling.
- The value may use the available screen width and large-number string presentation.
- Do not solve long values by making typography absurdly small.
- Internal precision/formulas remain hidden.

### In-battle floating numbers

- Target approximately **10 visible characters maximum**.
- Use compact integer/suffix representation.
- If knowledge prevents a value from being known, display **Unknown**.
- If the compact combat presentation range itself is exceeded, display **Exceed** rather than scientific notation.

### Extreme hit-overflow presentation

Do **not** show an aggregate `value × N` label.

Authoritative combat still resolves the real hit count. Presentation repeats representative integer/suffix damage triggers a few times as a visual burst instead of attempting to draw every logical hit instance.

## 17. Turn battle UI / auto / recap

- Add **3x** speed alongside the established 1x and 2x modes.
- Ultimate/cinematic presentation remains independently configurable.
- Timeline is an upcoming-action strip showing actors/events and visible action advances/delays/interrupt insertions.
- Auto provides quick presets plus an advanced per-team conditional rule editor.

Player-facing battle recap is intentionally simple **per character**:
1. direct/active damage dealt;
2. DoT damage dealt;
3. total healing/restoration delivered by any mechanism, including direct heal, regeneration contribution, vampirism, etc.

Do not clutter the normal recap with shielding, damage prevented, damage taken, crit analytics, micro-source breakdowns or other old categories. Those may remain diagnostics if engineering needs them.

## 18. Save / recovery / package UI

The game has a real title/opening screen rather than booting directly into a utility menu.

Presentation may ultimately be moving/cinematic like HSR or still/illustrated like FGO; that art-direction choice is deliberately deferred.

Primary opening actions:
- **Continue** prominently;
- Recover Existing World;
- Import Backup;
- Settings.

Clear World is secondary/destructive and requires deliberate confirmation.

Backup manager exposes:
- automatic/manual snapshots;
- date/time;
- world/schema/build information;
- integrity/validation state;
- Export / Import / Restore actions.

Package/storage manager exposes installed Worlds/regions/characters/media packages with:
- size;
- version;
- storage location;
- update state;
- move/archive controls where Android/storage rules permit.

Metadata/update checks can occur automatically online.

**Large package downloads default to automatic download on Wi-Fi/unmetered connection.**

## 19. Projects / Dispatch / War UI and timing

Projects do not simulate worker pawns/individual laborers as a management layer.

Project duration derives from the actual project workload plus relevant:
- aggregated population/capacity where applicable;
- infrastructure;
- facilities;
- assigned named specialists/characters where meaningful;
- resources;
- techniques/Authority/automation;
- environment;
- other causal capabilities.

Dispatch remains capability/knowledge driven, not a fake flat success percentage.

UI shows qualitative risk/ranges unless the player's actual information justifies exact probability.

War likewise uses capability vectors, counters, logistics and intelligence; displayed outcome confidence respects known information.

## 20. Offline consequence governor

The accepted **15% sovereign strategic-weight figure is a ceiling/budget**, not a scheduled loss.

Reconnecting does **not** mean losing 15% of Territory.

Most catch-up intervals may cause zero permanent Territory loss. The budget only limits the maximum newly generated irreversible strategic loss from eligible offline chains under normal protection rules.

Consequences already causally locked before logout remain exempt from this protective cap where the world state genuinely made them unavoidable.

## 21. World Director cadence

Chosen AYL resolution:
- do not define one global "event every N hours" cadence;
- each Director-controlled content family authors eligibility, earliest start, preferred window, latest bound/cooldown and world-state relevance;
- the Director uses bounded quiet-period logic to avoid absurd droughts without fabricating prerequisites.

## 22. World/local time presentation

Do not impose an Earth-like day/night cycle universally.

Earth-like/familiar pacing may be used **only in Territories/Worlds whose authored cosmology/environment supports it**.

Other Worlds/Territories may use radically different day lengths, calendars, light cycles, time ratios or no meaningful conventional day/night cycle.

## 23. UI reference assets

Two Creative Director supplied screenshots are attached to the cumulative master baseline as layout/composition references:

1. **Roster reference** - portrait-card roster, rarity-framed cards, search/filter density. Adaptation target uses the project's own branding/systems and three visible roster rows.
2. **Character-detail reference** - character art occupies the main visual field while identity information, equipment/status and navigation frame it.

They are inspiration/reference only; the project must not copy another game's branding, proprietary art, labels or exact ornamental UI.

## 24. Handoff

Global numerical/UI preference questionnaire is now closed sufficiently for reconciliation.

Remaining work before/alongside implementation:
- deterministic numerical simulations for coefficients still explicitly marked simulator-tunable;
- music / voice / SFX design pass;
- visual art-direction production pass;
- reconciled schema/C++ implementation;
- mandatory Unreal/Android validation;
- physical S26 Ultra performance/usability calibration.

Do not re-ask the decisions frozen in this document unless the Creative Director explicitly revises them.
