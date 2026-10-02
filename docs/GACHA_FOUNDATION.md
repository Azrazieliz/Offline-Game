# Gacha Foundation

Phase G begins by connecting acquisition to the same authoritative state used by
combat, exploration, territory, and projects.

## Persistent state

Pity is stored per Ruler and pity category, so compatible banners may share and
carry pity without tying persistence to a transient banner screen.

Stored state is intentionally compact:

- pulls since the last top-rarity acquisition;
- featured guarantee flag;
- total pull count;
- last update world tick.

Pull outcomes are recorded as meaningful world events instead of maintaining a
second history system.

## Determinism

A pull receives an explicit seed. The selected identity/version is reproduced
from the banner definition, current persistent pity state, and that seed.

Hard pity restricts the eligible pool to top-rarity entries. Soft pity increases
top-rarity entry weights after the configured threshold. A featured guarantee
does not create a top-rarity result by itself; it only redirects the next
top-rarity result to a featured top-rarity entry.

## Ownership and duplicates

The first acquisition creates the Ruler-specific Manifestation.

A later acquisition of the same Character Identity does not create a second
simultaneous Manifestation. It increments DuplicateAcquisitionCount on the owned
Manifestation. Later awakening/evolution systems can consume or interpret that
counter without losing acquisition history.

## Transaction boundary

Currency deduction, pity update, Manifestation creation/update, and the pull
world event commit in one SQLite transaction. A failed pull leaves none of those
changes partially applied.
