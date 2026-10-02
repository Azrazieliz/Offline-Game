# Offline-Game

Engineering repository for the offline-first Android gacha RPG defined by the current master architecture baseline.

> **Status:** pre-production architecture / vertical-slice foundation.

The repository name is intentionally provisional. Final game name, icon, branding, story terminology, and visual identity are deferred until the runtime architecture is secure.

## Product invariants

- Unreal Engine is the shipped runtime.
- Android-first; Samsung Galaxy S26 Ultra is the primary device target.
- Portrait **Ruler Mode** and landscape **World Mode** operate on the same persistent history.
- SQLite is the authoritative mutable world-state store; spawned Unreal actors are presentation/runtime projections.
- Core pillars protected from scope cuts:
  - gacha / character collection
  - turn combat and World Mode action combat
  - mature content and mature combat presentation
  - exploration / traversal
  - character progression
  - Ruler / Domain / Core progression
  - persistent world consequences and meaningful warfare
- Minor systems must not become simulations unless their simulation creates meaningful player-facing value.
- Prefer numbers, tags, state transitions, orders, and event results over background micromanagement.
- Preserve the experience, not unnecessary implementation complexity.

## Initial engineering target

The first vertical slice proves one architectural statement:

> Gacha, character progression, turn combat, World Mode exploration/action combat, territory state, and world history all read and write one authoritative persistent state.

See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) and [docs/VERTICAL_SLICE.md](docs/VERTICAL_SLICE.md).

## Repository policy

Critical rules live in C++ and data definitions that can be tested. Blueprints may compose presentation but must not be the only source of authoritative gameplay logic.

Generated or optional content must be versioned and validated before activation.

This repository is owned as an engineering workspace: implementation details may change aggressively while player-facing design invariants remain stable.
