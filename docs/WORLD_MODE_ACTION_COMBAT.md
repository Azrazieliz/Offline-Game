# World Mode Action Combat

This document records the current detailed World Mode action-combat design. It supersedes older macro-only wording where they conflict.

## Status

Current authoritative baseline.

## 1. Mobile movement, attacks and targeting

- Default mobile locomotion: permanent left virtual movement stick + right-side camera drag.
- Jump is a permanent input.
- Aerial attacks are supported whenever the character/state permits them.
- The normal attack input may support character-specific tap chains.
- There is no universal hold/heavy attack requirement. Heavy, charged, stance and alternate basics are authored per character and use the input that best fits that kit.
- Basic attacks may differ in hit count, combo length, timing, branches, ranged/melee behavior, charge behavior and stance logic.
- Soft auto-targeting and manual hard lock-on both exist.
- A hard-locked target can be changed by touching the other enemy directly.
- Appropriate ranged skills/weapons may support precise manual aiming.
- Large enemies may expose several lock-on body parts/weak points.
- Dangerous/boss attacks rely mainly on animation/readability. A subtle roughly 0.5-1 second preparation cue is acceptable; conspicuous difficulty-lowering warning markers are not the baseline.
- Parryable attacks are learned from animation/readability rather than a universal bright parry indicator.

## 2. Dodge, parry and guard

- Dodge is baseline for playable characters unless an explicit state removes it.
- A perfect dodge's universal reward is simply avoiding the damage.
- Perfect dodge may produce a short approximately 2-3 second perception/time-slow presentation.
- Extra perfect-dodge rewards exist only when an explicit character/condition/effect grants them.
- Generic parrying is performed primarily by timing an attack or skill into the enemy attack.
- There is no dedicated universal parry button.
- Characters may have dedicated guards, parries, counters or defensive stances as authored kit mechanics.
- Blocking/guarding is character-specific.

## 3. Animation cancel policy

Animation canceling means interrupting an attack/skill's recovery or commitment with another action such as dodge, switch, jump or another skill before the original animation naturally finishes.

Cancel behavior is character/action-dependent. Each attack, skill and state defines its own valid cancel windows and destinations. Fast/agile actions may permit extensive dodge/switch/jump/skill cancels; highly committed actions may lock the character until their authored release point unless an explicit mechanic overrides that commitment.

## 4. Party, switching and QTE

- Immediate World Mode party: protagonist/Ruler + up to two selected companions.
- The protagonist remains a member of the party while a companion is controlled.
- Switching is near-instant unless explicitly prevented by state/mechanics.
- The outgoing character normally leaves the active field after their exit/switch animation.
- Companions do not remain persistently co-simulated on field after switching away.
- Entry attacks, exit attacks and switch effects are character-specific.
- QTE availability is character/skill-specific rather than driven by one universal team meter.
- QTEs may temporarily overlap two or three characters.
- Authored common/coordinated QTEs are supported, including multi-character combination sequences.
- A controlled companion's defeat transfers control to another available member.
- If the protagonist is defeated while an immediate revival/return mechanic is valid, combat may continue through companions until it resolves, or the protagonist may return directly if the mechanic permits.
- If the protagonist truly dies with no immediate valid continuation, canonical death/Regression handling applies unless an explicit encounter mechanic temporarily delays resolution.

## 5. Skills, resources, Ultimate and World Fantasm

- Only part of a character's broader kit is directly selectable as active buttons in World Mode.
- Visible active count/layout is character-dependent.
- Combos and QTE behaviors are inherent authored behaviors/triggers of skills/actions; they are not separate active-skill entries.
- Character-specific resources use bespoke HUD meters.
- Ultimate state is shared with the canonical character state used by turn combat.
- Normal Ultimate threshold remains 100% unless the character defines otherwise.
- 200/300% or other multi-threshold Ultimates use fast direct inputs such as tap-count, double-tap, hold or another character-appropriate gesture; no slow selector menu.
- Transformations/forms do not receive one universal extra transformation button.
- World Fantasm activation belongs to the character's own kit rather than a universal identical extra button.
- World Fantasm uses a short, non-disruptive character-specific cue/cut-in followed by the live field transformation. Repeated activations may use abbreviated presentation.
- Ruler Commands are not available in ordinary World Mode combat because the Ruler is directly fighting.
- Consumable inventory items are not usable during action combat.

## 6. Encounter flow and environmental causality

- Ordinary encounters begin seamlessly in the world without an arena/loading transition.
- Enemies may pursue over meaningful physical distances.
- Disengagement uses physical/situational logic: distance, line of sight/concealment, mobility, motivation, orders, territory and encounter design rather than a tiny universal leash.
- The player may attack physically present creatures/characters without requiring them to be pre-labeled hostile.
- Consequences follow actual world/faction/target state.
- Cliffs, water, eligible destruction and hazards remain relevant during combat.
- Friendly fire is off by default unless an explicit ability/rule/world state enables it.
- Offline single-player action combat can be completely paused.

## 7. Traversal-combat continuity

- Underwater combat uses the action-combat system with underwater-modified movement.
- Flight-capable characters may fight with free three-dimensional movement.
- Mounted combat is supported where mount, character and ability compatibility permit it.
- Combat can continue while airborne, swimming, underwater, flying, climbing or mounted where the capability/animations allow it.

## 8. Bosses, hybrid encounters and shared core

- Hybrid action/turn transitions are driven by boss/encounter mechanics.
- HP, statuses and relevant resources persist across phase transitions unless an explicit mechanic transforms them.
- Action and turn combat use the same authoritative effects, conditions, Authority, World Fantasm and character-state rule core.
- Presentation and execution timing differ; authored mechanics are not duplicated into unrelated combat systems.
- No separate action-combat assist mode is required.

## 9. Input/readability constraints

- Keep the mobile action HUD compact despite deep kits.
- Prefer fast direct inputs over combat menus.
- Character-specific exceptions are allowed where required by the authored kit.
- Difficulty/readability should come primarily from animation, timing, spatial state and player knowledge rather than highly conspicuous warning UI.
