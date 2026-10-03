# Existence / Power Rank

This document records the current authoritative personal-power Rank architecture. Rank remains distinct from collectible Rarity, Class, Ruler sovereignty/milestone titles, World Rank and Transcendence.

## 1. Universal named ladder

- One universal ordered Rank ontology applies across worlds/dimensions.
- Ranks are player-facing names rather than displayed numerals. The engine may retain an internal ordinal for ordering, arithmetic and migration only.
- Cultures/worlds may use aliases for the same underlying Rank.
- Current canonical ladder, low to high:
  1. Common
  2. Enhanced
  3. Awakened
  4. Extraordinary
  5. Mythic
  6. Transmundane
  7. Astral
  8. Celestial
  9. Cosmic
  10. Dimensional
  11. Lawborne
  12. Conceptual
  13. Primordial
  14. Apex
- Rank definitions are data-driven so genuinely higher future bands can be added without treating visible names as code enums.
- Ordinary NPCs can occupy different levels inside Common Rank. The protagonist can therefore start slightly above an ordinary NPC without starting at a higher Rank.

## 2. Meaning and levels

- Rank is a qualitative existence/power bracket, not merely a label for larger stats.
- Each Rank contains Levels 1-100.
- Level 100 means the normal ceiling of that Rank has been reached. Advancement requires a real breakthrough.
- Same-Rank entities may still differ enormously through level, stats, skills, Factors, equipment, counters, experience and unique mechanics.
- Rarity does not substitute for Rank.

## 3. Superlinear scaling

- Rank-step gains are intentionally non-linear.
- Later Rank breakthroughs create much larger normalized increases than early breakthroughs.
- A very high Rank step may produce more normalized gain than several early Rank steps combined.
- Implementation uses a superlinearly growing Rank-band coefficient beneath readable/normalized stats.
- Exact coefficients are tuning data.
- Build identity, counters and explicit mechanics continue to matter.

## 4. Breakthroughs

- Reaching Level 100 unlocks eligibility to attempt/fulfill a next-Rank breakthrough rather than overflowing XP into the next Rank.
- Breakthrough requires sufficient development plus appropriate conditions.
- Lower-Rank conditions may reuse broad grammars such as training, resource integration, combat achievements, rituals, cultivation or Factor maturation.
- Higher-Rank breakthroughs become increasingly bespoke and existence-specific.
- Failure has no universal punishment; consequences exist only when that breakthrough's actual mechanic has risk.
- NPCs and owned characters use the same ontology, with aggregated offscreen resolution allowed.

## 5. Attained vs effective Rank

- Store Attained Rank/Level separately from Current Effective Rank/Level.
- Seals, injuries, curses, suppression fields, corruption, starvation and other mechanics may reduce effective Rank without erasing attained Rank.
- Removing temporary suppression may restore prior effectiveness without repeating the breakthrough.
- True structural damage/regression may lower Attained Rank, but only through an explicit mechanic.
- Peak historical Rank may be retained for Chronicle/story/diagnostics.

## 6. Rank Suppression

- Same and adjacent/near-adjacent Ranks primarily resolve through ordinary stats/mechanics.
- Larger qualitative gaps increasingly create Rank Suppression.
- Suppression may affect damage/effect penetration, resistance breaking, control, perception, presence tolerance or other interaction channels.
- The effect grows with Rank difference and inherits the superlinear importance of later Rank steps.
- Suppression is channel-specific rather than only one blanket damage multiplier.
- Exact thresholds/coefficients are deterministic tuning data.
- Qualitative baseline: same/adjacent = ordinary resolution; modest multi-Rank gap = soft suppression; large gap = strong suppression; extreme gap = interaction failure unless a bypass applies.

## 7. Lower-Rank bypasses

- Lower-Rank entities can threaten higher-Rank entities through explicit skills, artifacts, Authority, World Fantasm, Domain rules, conceptual counters, poison, seals, weaknesses or environmental conditions.
- A bypass does not make the lower entity the same Rank.
- Bypasses may apply only to specific interaction channels.

## 8. World ceilings

- Worlds/dimensions may have natural Rank ceilings/tolerance bands.
- Above-ceiling entities are not blocked by an arbitrary invisible wall.
- Greater Rank excess produces stronger world-side suppression/instability.
- Effects may include reduced manifested output, constrained abilities, higher resource cost, reality resistance, forced self-restraint, dimensional instability, environmental damage or breach phenomena.
- Avatars, vessels, seals, Domains, infrastructure or deliberate self-limitation can mitigate pressure.
- World-ceiling suppression is distinct from combat Rank Suppression.

## 9. Knowledge and UI

- Exact Rank is knowledge-dependent.
- Owned/known entities can show exact Rank and Level.
- Unknown beings may show Unknown, estimates or no Rank data depending on senses, appraisal and knowledge.
- Concealment/falsification is possible when a valid mechanic supports it.
- UI exposes Attained and Effective Rank separately when they differ.

## 10. Data representation

Authoritative state separates stable rank identity, attained level, effective overrides, peak Rank, suppression sources and discovered/estimated knowledge. Gameplay code queries declarative Rank rules rather than hard-coding display names.

Numerical curves remain simulation-tunable and require regression tests so late Rank breakthroughs preserve the intended superlinear gain without making explicit counters impossible.
