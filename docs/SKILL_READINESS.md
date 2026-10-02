# Skill Readiness / Costs

The combat architecture intentionally has no universal team Skill Point pool and no universal cooldown model.

Each selectable ability declares only the gating/cost mechanics it actually uses.

## Generic readiness requirements

The baseline implementation supports:

- personal resource threshold
- required state
- forbidden state
- limited uses
- explicit action-value/cooldown readiness
- HP threshold
- previous-skill requirement

Additional bespoke predicates can be added through the effect/rule layer rather than expanding every character into a separate subsystem.

## Generic costs

Common reusable costs are:

- personal resource consumption
- Max-HP percentage cost
- future action-value cost

Other costs can be specific mechanics.

## Ultimates

Ultimate/NP charge is represented as a character-owned resource, not a global team resource.

The character definition decides:

- base threshold
- maximum overgauge
- charge generation
- available activation tiers
- selected tier cost
- whether use is outside normal turn order
- interruption/seal/drain behavior
- charge retained after defeat/revival

UI can normalize the character's base threshold to 100% without requiring the internal resource scale itself to equal 100.

A character with enough charge for a higher tier can deliberately choose a lower tier; only the selected tier's cost is consumed and the remainder stays stored.

## Basic attacks

A normal free Basic simply declares no cost/readiness requirement beyond ordinary ability state.

This does not mean Basics are filler; their authored mechanics remain unrestricted.
