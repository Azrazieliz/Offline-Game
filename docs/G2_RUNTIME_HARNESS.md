# G2 Runtime Harness

Checkpoint G2 is retained as the **reconciled Vertical Slice 0 proof
infrastructure**. The pre-detailing sequence has been replaced by the
architecture-frozen flow in `VERTICAL_SLICE.md`.

## Persistent scenario harness

`FOGVerticalSliceScenarioHarness` now proves one SQLite history across:
1. validated package/content state;
2. real Territory control and authored more-than-one-month gacha qualification;
3. permanent gacha unlock;
4. landless acquisition of two independent same-Identity Manifestations;
5. immediate portrait Ruler Mode projection while still unanchored;
6. explicit first World Mode anchoring after controlled Territory is reached;
7. shared action/turn Character-Identity exclusivity;
8. physical exploration and knowledge-limited HUD state;
9. a persistent action-combat consequence;
10. Domain/Core damage, Project completion and objective-faithful Dispatch;
11. persistent faction/War/front state;
12. deterministic turn-battle replay;
13. return to Ruler Mode over the same canonical facts;
14. process restart and persistence/integrity verification.

The harness uses provisional `slice:` content IDs only. It is
test/developer infrastructure, not production balance or lore.

## Android baseline

`Config/DefaultEngine.ini` establishes the initial Android build posture:

- Android-only shipping focus with ARM64 enabled and x86_64 disabled;
- Vulkan enabled; experimental Vulkan SM5 disabled;
- OpenGL ES 3.1/3.2 fallback shader path retained for bring-up;
- target SDK 35 and minimum install SDK 26 for UE 5.8 compatibility;
- sensor orientation so portrait Ruler Mode and landscape World Mode can coexist;
- app-specific external-files directory rather than requiring broad external
  storage access;
- data is not forced inside the APK because the final content footprint is large.

The package name remains explicitly provisional with the project token.

## Lightweight performance telemetry

`FOGPerformanceTelemetry` retains aggregate counters only: frame count,
average/worst/last frame time, frames above the selected profile budget, and
two-times-budget hitch count.

Profiles are 30/60/120 FPS, matching the current design baseline. Changing
profile resets the aggregation window so samples are not mixed across budgets.

`UOGGameCoreSubsystem` samples frame delta through the core ticker. Telemetry
does not become authoritative world state and does not create an unbounded
history.

## Verification boundary

Source-level G2 is not compile-certified until the mandatory UE 5.8 sequence
succeeds:

1. static Unreal C++ preflight;
2. UHT/UBT Win64 Development Editor compile;
3. `OfflineGame` automation tests.

Android cook/package and sustained S26 Ultra profiling remain the following
device gate.


## Gate coupling

A successful Windows compile + automation run writes
`Saved/Automation/last_unreal_gate.json` with the exact Git commit and UE root.
The Android packaging script refuses to run when this stamp is absent or stale.
This prevents a device package from bypassing the required preflight -> compile
-> automation order.

`Scripts/android_unreal_package.ps1` then uses UE Turnkey to verify the Android
SDK before attempting a Development cook/package. It is device-gate tooling; its
existence does not mean an Android package has passed yet.

Diagnostics generated through `UOGGameCoreSubsystem` now include the current
aggregate performance snapshot alongside schema/integrity metadata, without
copying world/save payloads.

## Current gate status

The source-level reconciliation is complete. Static repository/preflight checks
must be fully green before the first real UE 5.8 compile. No real Unreal,
Android package or S26 Ultra result is claimed by this document.
