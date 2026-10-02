# G2 Runtime Harness

Checkpoint G2 remains an architectural proof. It does not freeze any of the
mechanics that the master design baseline still marks OPEN.

## Persistent scenario harness

`FOGVerticalSliceScenarioHarness` connects the existing authoritative systems
through one SQLite history:

1. create the proof Ruler and physical wilderness location;
2. seed aggregate pull/build resources;
3. perform one deterministic persistent gacha pull;
4. validate a World Mode switch party using the acquired Manifestation;
5. persist physical presence and an Explored location-knowledge fact;
6. create one territory and Domain Core;
7. apply a persistent World Mode consequence by damaging the Domain Core;
8. start and lazily complete one aggregate Project;
9. run one deterministic turn-battle replay;
10. persist a compact checkpoint containing the acquired Manifestation, Project,
    and battle fingerprint;
11. close/reopen SQLite and verify all relevant consequences survive.

The harness uses provisional `slice:` content IDs only. It is test/developer
infrastructure, not production balance or lore.

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
