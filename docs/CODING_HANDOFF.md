# Coding / Testing Only Handoff

Use this document as the execution contract for the next conversation/workstream.

## Repository

- Repository: `Azrazieliz/Offline-Game`
- Active branch: `phase-g2-runtime-harness`
- Do not restart from `main`.
- Runtime reconciliation through schema 0014, UI projections and Vertical Slice 0 is implemented; real Unreal certification remains pending.

## Authority order

1. newest explicit Creative Director revision;
2. `FINAL_PRECODING_FREEZE.md`;
3. dedicated current architecture/production documents;
4. `RECONCILIATION_MIGRATION_MATRIX.md`;
5. final cumulative pre-coding master;
6. older cumulative text only where not superseded;
7. current runtime code/tests are implementation evidence, not design authority when they conflict.

## Work mode

This workstream is **strictly coding, testing, deterministic simulator fitting, build validation, Android packaging and physical-device profiling**.

Do not:
- restart broad design questionnaires;
- re-ask frozen UI/tuning/audio/art decisions;
- weaken frozen behavior to match stale code;
- invent a second architecture because implementation is difficult.

Make routine engineering choices autonomously.

Only return to Creative Director design if a newly discovered contradiction makes two frozen player-facing requirements impossible to satisfy simultaneously.

## Current engineering task

Complete the dedicated pre-Unreal completeness/functionality audit. Fix every
source, persistence, test and documentation inconsistency found, then require
Repository Validation and Unreal C++ Preflight to be green on the exact audit
head. Do not invoke Unreal until that gate is clean.

The numbered reconciliation sequence through schema 0014, UI projections and
Vertical Slice 0 has already been implemented.

## Tuning

Use `TUNING_PARAMETER_REGISTRY.md`.

Do not hard-code simulator candidates as immutable rules. Build deterministic fitting/regression tools and version adopted tuning sets.

## Presentation implementation targets

- audio/music/voice/SFX: `AUDIO_PRODUCTION_DIRECTION.md`;
- visual/VFX/animation/cinematics: `VISUAL_ART_DIRECTION.md`;
- UI: `TUNING_UI_FREEZE.md` + normalized `UI_PRESENTATION.md`.

Final content assets are not required to prove architecture; use validated proof assets/data without contradicting production contracts.

## Mandatory test gate

Every real Unreal gate restarts from:
1. static C++ preflight;
2. UHT/UBT/MSVC/link;
3. automation.

Only after green:
4. Android cook/package/install;
5. S26 Ultra profiling.

## Paid compute

No DigitalOcean or other paid compute without explicit Creative Director approval.

Cumulative paid-compute ceiling: USD 25.
Recorded spend at handoff: USD 0.
