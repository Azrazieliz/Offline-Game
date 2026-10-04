# World Fantasm and Transcendence

This document records current design decisions that supersede older working references to "Fantasm Materialization" and to multi-depth Transcendence ladders where they conflict.

## 1. World Fantasm

World Fantasm unlocks when a character reaches **Current Rarity MR**, regardless of immutable Origin Rarity.

World Fantasm externalizes the character's inner world/perception into a real battlefield field. It must have visible field/environment presentation rather than acting as an invisible stat aura.

World Fantasm is broadly a **non-Ruler-specific analogue of Domain Authority**:

- Domain Authority is sourced from Ruler/Territory/Domain Core structures.
- World Fantasm is sourced from the character's own existence and inner world.
- Neither has categorical priority over the other.
- Conflicts resolve through ordinary rule/ability interaction: strength, counters, compatibility, relevance and explicit mechanics.

When two or more World Fantasms are active, their fields may overlap, mix, form boundaries, fracture, overwrite local portions of one another, or visibly collide. The visual field state should reflect the mechanical interaction.

World Fantasm is not an automatic-win layer. A lower-grade Fantasm can counter a higher-grade one in the right matchup.

### Default persistence

When a World Fantasm ends, its field/rule overlay collapses. Direct consequences that physically occurred may persist (damage, injuries, destruction, displaced objects, etc.). Fantasm-only terrain/entities/rules disappear unless that specific ability explicitly creates a persistent result.

## 2. World Fantasm grade from Origin Rarity

The grade is determined by **immutable Origin Rarity**, but is obtained only once Current Rarity reaches MR.

| Origin Rarity | World Fantasm grade |
| --- | --- |
| R | World Projection |
| SR | World Projection |
| SSR | World Alteration |
| UR | World Materialization |
| LR | World Manifestation |
| MR | World Negation |

These are **not sequential stages for one character**.

Example: an Origin-MR character that reaches Current Rarity MR obtains World Negation directly. It does not pass through Projection, Alteration, Materialization and Manifestation first.

Moving from MR into Transcendence may improve the existing World Fantasm (scale, stability, control, reach, efficiency, collision strength, rule expression, etc.) without automatically changing its World Fantasm grade.

## 3. Transcendence

**Transcendence is the universal rarity/state immediately after MR.**

A character enters one Transcendence grade/state rather than climbing sequentially through all five names.

Current five-grade scale, ascending:

1. Ascendant
2. Unbound
3. Exalted
4. Transcendent
5. Absolute

The intended Origin-Rarity banding mirrors the five-band World Fantasm scale:

| Origin Rarity | Transcendence grade |
| --- | --- |
| R | Ascendant |
| SR | Ascendant |
| SSR | Unbound |
| UR | Exalted |
| LR | Transcendent |
| MR | Absolute |

### Breakthrough

Transcendence is not a simple XP threshold.

General structure:

1. reach and break a power ceiling; and
2. satisfy a small set of character-specific breakthrough proofs/conditions.

Three solo conditions are a representative pattern, not a universal fixed formula. Equivalent bespoke requirements are allowed when they better fit the character.

## 4. Related resolved rules

- Percentage-of-Max-HP offensive damage is not a normal direct-attack formula. It is allowed through explicit afflictions, debuffs and other specially authored mechanics.
- Ruler Commands do not use one universal command-point economy. They are enabled by explicit conditions, encounter rules, battle types, preparation or ability-specific requirements.
- Subordinate Rulers may use both their own gacha resources and resources allocated by the superior Ruler. Under the player's Overlord hierarchy, their acquisitions ultimately enter the wider player-controlled roster ecosystem.
- Login/daily/weekly/return rewards, when present, are automatically credited. No manual login-reward claim loop.
- Surrender is contextual and uncommon, not a generic low-HP behavior. It never forces the player to spare the surrendering enemy.
- Ruler Mode Home remains character/lobby focused. Territory/Domain is one of the five bottom-navigation destinations and contains the territory overview and strategic map.
- Dedicated photo/free-camera mode is not required.

## 5. Dimensional preparation purpose

The dimensional contract/gacha preparation system exists as a means granted by dimensions to gather, develop and organize power in preparation for extremely powerful endgame monsters/enemies.

Ordinary conflicts, Ruler progression, Domain development, roster growth and character development are also preparation for those true late-game threats.

Whether dimensions consciously grant this system or whether it is an automatic multiversal defensive law remains a lore-detail decision and is not required for current implementation.


## 6. Protagonist exception

The protagonist has no collectible Origin Rarity and must not be assigned a hidden pseudo-rarity merely to fit the character tables above.

When the protagonist is a territorial Ruler, his Ruler-side higher-order reality expression is primarily **Domain Manifestation / Domain Authority**, sourced from territorial sovereignty rather than a collectible-character World Fantasm grade.

If the protagonist remains territoryless, sufficiently high personal/existential power may instead allow an intrinsic **personal World Manifestation**. Its strength is power/existence based rather than Origin-Rarity based.

### Protagonist Transcendence

The protagonist Transcends **once**.

Before that irreversible breakthrough, he may progressively qualify for:

1. Ascendant
2. Unbound
3. Exalted
4. Transcendent
5. Absolute

When he chooses to Transcend, he receives the highest grade for which both the universal minimum power/Existence foundation and his bespoke persistent-world personal proofs/conditions have already been satisfied. There are no alternate save branches: this is evaluated against the one continuing world/history. The resulting grade is permanent under normal progression.

Waiting therefore preserves access to higher potential grades, while Transcending earlier trades that potential for immediate Transcendence power. Absolute remains theoretically reachable in the continuing world unless the player performs a genuinely irreversible act that causally makes it impossible; the game must not silently brick ultimate potential because of an arbitrary hidden branch. The UI must clearly show the currently attainable grade and warn that the commitment is permanent; higher possible grades may be hinted without exposing every hidden condition.


### Qualification grammar

- Each protagonist Transcendence grade has a **universal minimum power / Existence requirement** so a weak protagonist cannot bypass the scale through an obscure proof alone.
- Above that floor, qualification requires **bespoke personal proofs** generated from what the protagonist has actually become and accomplished in the one persistent world.
- Proofs may involve Rank breakthroughs, Factors, Classes, combat feats, survival, Authority, knowledge, reality manipulation, relationships to world structures, unique achievements or other causally relevant history.
- The proof set is not a fixed quest checklist shared by every possible protagonist development path.
- The Director/UI may reveal progress or hints where the protagonist has a valid way to know them, but hidden metaphysical criteria are permitted.

### Personal World Manifestation: emergent, not templated

The protagonist's personal World Manifestation has **no fixed thematic template** and is not selected from an authored list of preset inner worlds.

It is synthesized from the protagonist's actual persistent history and existence, including Factors, developed Classes, learned skills, Rank, repeated choices, formative experiences, worldview, powers, major losses/victories, metaphysical encounters and other genuinely causal state.

The implementation may use reusable lower-level effect/VFX/rule primitives for feasibility, but those primitives must not turn the resulting Manifestation into a disguised preset template. Its identity, rules and presentation emerge from the protagonist that actually exists.

The personal World Manifestation remains mutable after unlock. Later Factors, breakthroughs, transformations, major experiences and existential changes may alter its laws, visuals, scale, efficiency, counters and available expressions while preserving historical continuity.

### Dual-reality mastery

If the protagonist possesses both an intrinsic personal World Manifestation and a territorial Domain, advanced development may create deliberate **combined techniques/states** using both systems.

- They remain independently owned powers.
- Combination is never an automatic additive stack or generic x2 multiplier.
- The combined result must follow actual compatibility between the protagonist's personal reality and the Domain's Concepts/Authority.
- Different Domains may therefore produce different combined expressions with the same protagonist.
- Losing the Domain removes Domain-dependent combined states but does not erase the intrinsic World Manifestation.

### Coexistence with Domain Authority

A personal World Manifestation is intrinsic to the protagonist and **remains his** if he later gains territory and a Domain.

Domain Manifestation / Domain Authority and personal World Manifestation are separate higher-order reality systems. They may:

- coexist;
- overlap;
- reinforce one another;
- remain independent; or
- enter an explicitly authored combined state.

They do **not** automatically fuse or stack.

Losing territory can remove or weaken Domain-derived effects but does not erase the intrinsic personal World Manifestation.

Personal World Manifestation, Domain Authority and other characters' World Fantasms resolve through the same established higher-order conflict principles: strength, Authority, counters, compatibility, relevance and explicit mechanics.
