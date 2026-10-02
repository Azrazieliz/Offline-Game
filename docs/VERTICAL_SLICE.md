# Vertical Slice 0 - Architectural Proof

The first slice is intentionally small. Its purpose is to prove the shared-state architecture, not approximate final content quantity.

## Required proof

A single save/history must survive this loop:

1. launch in Ruler Mode
2. perform a gacha pull
3. configure/use acquired characters
4. enter landscape World Mode
5. explore and discover a physical location
6. fight using protagonist + switch/QTE companions
7. cause a meaningful persistent state change
8. return to Ruler Mode and observe that change
9. start/resolve one project or dispatch
10. run one turn battle
11. close the process completely
12. reopen the game
13. verify the same characters, discovery, project, battle consequences, and world state still exist

## Slice content target

Provisional engineering content only:

- one compact wilderness test region
- protagonist
- 6-10 test Character Identities
- Character Identity -> Version -> owned instance path
- one test banner with configurable pity
- one enemy family
- one substantial boss
- one turn encounter
- action party: Ruler + two switch/QTE companions
- one territory
- one Domain Core
- a few resource definitions
- one construction/reconstruction project
- one dispatch
- one discovery becoming visible on the Ruler map
- minimal faction hostility/war state
- blood and defined equipment/clothing damage state hooks
- SQLite persistence
- developer diagnostics/state repair

## Explicitly not required for Slice 0

- final name/icon/branding
- final story/cosmology
- final art direction
- large roster
- final gacha rates
- final damage formulas
- full Domain concept grammar
- underwater civilization
- space
- full World Director content library
- full mature-content library
- final voices/cinematics
- remote multi-year world simulation

## Acceptance rule

The slice is successful when the core state model is trustworthy enough that adding content no longer requires redesigning persistence or identity.
