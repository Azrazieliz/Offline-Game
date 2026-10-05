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

The first acquisition creates the first Ruler-specific Manifestation.

A later acquisition of the same Character Identity normally creates another
persistent Manifestation of that same canonical Identity. Each copy can be
leveled, equipped and developed independently so Evolution, Awakening,
Corruption and future development routes can diverge.

Copies are grouped under one Character Identity in presentation. They are not
collapsed into an inert duplicate counter. Local encounter identity exclusivity
still normally prevents simultaneous fielding of the same Character Identity
unless a specific mechanic permits it.

Fully reinforced divergent copies can later be consumed explicitly by the
character's Grand Convergence finalization. This is character development, not
automatic duplicate conversion.

## Transaction boundary

Currency deduction, pity update, Manifestation creation/update, and the pull
world event commit in one SQLite transaction. A failed pull leaves none of those
changes partially applied.


## Access gate and first deployment

Ruler status itself does not require a subordinate contract. A solitary Ruler is valid if they genuinely control and can keep territory.

The protagonist's gacha access unlocks only after a territory has remained continuously under their control for more than one in-game month. Remaining territoryless keeps gacha unavailable by choice.

A successful first acquisition creates the owned Manifestation and makes her visible immediately in portrait/vertical Ruler Mode. It does not force a physical spawn at the protagonist's current World Mode location. The Manifestation becomes eligible for active World Mode deployment only after the protagonist returns to controlled territory and completes the first roster anchoring step there.

All pullable Character Identities are intended to be female. This does not constrain the sex of non-gacha world NPCs, enemies, faction members, or Rulers.


## Implementation reconciliation status

Migration 0007 and the current gacha runtime use one full persistent
Manifestation per qualifying character acquisition. Legacy duplicate counters
are migration provenance only and do not drive gameplay. The migration preserves
recoverable acquisition history deterministically.

## Economy and repeat-acquisition freeze

The game is a standalone offline-first gacha RPG. Gacha access is earned through gameplay/world systems; there are no real-money purchases or IAP.

Pull currency remains renewable so the one persistent world cannot permanently exhaust acquisition access.

Repeat pulls create full persistent Manifestations rather than fragments or automatic material conversion. There is no hard copy cap. Copies exist for divergent development and later Grand Convergence; see `GACHA_ECONOMY.md` and `CHARACTER_PROGRESSION.md`.

There is no generic unwanted-copy-to-shards action in the frozen architecture. A Manifestation ceases independent existence only through an actual supported mechanic such as Grand Convergence or character-specific fusion/absorption.

The provisional pity foundation remains subject to deterministic tuning rather than being treated as final numerical balance.
