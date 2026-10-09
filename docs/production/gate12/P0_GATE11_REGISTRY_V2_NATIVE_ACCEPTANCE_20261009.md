# Gate 12 P0 — Native Unreal acceptance after desktop access (2026-10-09)

## Verdict: PASS for P0 source-contract + isolated UE 5.8.3 acceptance; no production-asset import

Profile: `gate11.production_artifact.historical_v2_compatible.1`.
Foundation source/tag remains `3f54f07cacbf193e160d0be15a7be5a7bc730c3b`.
Exact P0 package dependency: `ui:shared_navigation_contract@1` must precede `ui:ruler_shell_navigation@1`, minimum 1.
All source DTO projections are **source-only**; none grant installed/validated/activated production rights or physical optional pak installation authority.

### Native host and source checkout

Connected Windows laptop: `LAPTOP-1LI4VRCJ`.
Original repo: `D:\UnrealProjects\Offline-Game`, clean at Foundation initially; temporarily detached on PR commit `959395f787535db86a2866f4dc822cfd6f9b8cad` for an additive C++ fixture. Unreal Engine version from UBT: **5.8.3**.
Native tests ran against `UnrealEditor-Cmd.exe`, `-unattended -nop4 -nosplash -nullrhi -UTF8Output`, `-testexit=Automation Test Queue Empty`.
All native SQLite test databases were freshly created under GUID-scoped `Saved/Automation/`; original player `Saved/OfflineGame/WorldState.db` was not targeted.
After tests, the original project was returned via `git switch content/environments-worlds`; HEAD `3f54f07cacbf193e160d0be15a7be5a7bc730c3b`, `git status --porcelain` empty. This verifies **tracked source state**, not post-restore compiled binaries.

### Native UE5.8.3 tests — actual command outputs

Use `D:\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe` and `D:\UnrealProjects\Offline-Game\OfflineGame.uproject`.
Suffix for each command: `-testexit="Automation Test Queue Empty" -unattended -nop4 -nosplash -nullrhi -UTF8Output -abslog=<below log path>`.

| -ExecCmds automation filter | Native result | Log SHA-256 |
| --- | --- | --- |
| `Automation RunTests OfflineGame.Runtime.Packages` | 2 / 2 Success, exit 0 | `30846774ED8AC87B4EE4FD147A1549FFD64B70AEEB253F2CBA254370D1FA2238` |
| `Automation RunTests OfflineGame.Runtime.OptionalPackages` | 6 / 6 Success, exit 0 | `EB893ABF03E174A763D7470D9706EB9B9BC208E71EC678CCF2D05D4D880CD9B8` |
| `Automation RunTests OfflineGame.Content` | 4 / 4 Success, exit 0 | `7C322068816F43485847EC50760D5ADDAECBA3BE2DC84FA7681FAEEB7DECAABF` |
| `Automation RunTests OfflineGame.Gate12.RegistryV2` | **1 / 1 Success**, exit 0 | `DD1CEE3CB6F2158741E172339E325025E6996142E40DDF2F9BF8B34B9665C083` |

Exact-ID native test: `OfflineGame.Gate12.RegistryV2.ExactUiPackageDtoDependencyAndOptionalFallback` written in `Source/OfflineGame/Private/Tests/OGGate12RegistryAdapterNativeTests.cpp` (added commit `959395f787535db86a2866f4dc822cfd6f9b8cad`).
The test: native `FOGContentManifestValidator` accepts two approved DTOs; rejects bad ID/duplicate dependency/zero minimum; disposable schema-14 SQLite package lifecycle rejects nonexistent/too-new dependency and premature activation; accepts dependency-root before child; missing `/Game/Gate12P0/AbsentOptionalView.AbsentOptionalView` returns null, leaves logical package metadata unchanged; deactivating root also deactivates child. The expected absent-media warning was recorded as a **Success with warning**, not a test failure.
Detailed exact UE log markers:
```
LogAutomationCommandLine: Display: Found 1 automation tests based on 'OfflineGame.Gate12.RegistryV2'
LogOfflineGame: Applied database migration 14 (packages_reports_management).
LogOfflineGame: Opened authoritative world database: .../Saved/Automation/Gate12P0/902412F7440F04A890C82583A08934DE/gate12_native.db
LogUObjectGlobals: Warning: Failed to find object 'Object /Game/Gate12P0/AbsentOptionalView.AbsentOptionalView'
LogAutomationController: Display: Test Completed. Result={Success} Name={ExactUiPackageDtoDependencyAndOptionalFallback} Path={OfflineGame.Gate12.RegistryV2.ExactUiPackageDtoDependencyAndOptionalFallback}
LogAutomationCommandLine: Display: ...Automation Test Queue Empty 1 tests performed.
LogWindows: FPlatformMisc::RequestExitWithStatus(1, 0, FEngineLoop::Tick.GScopedTestExit)
```
Native full-log locations: `D:\UnrealProjects\Offline-Game\Saved\Logs\Gate12P0_PackageNative_20261009.log`, `Gate12P0_OptionalNative_20261009.log`, `Gate12P0_ContentNative_20261009.log`, `Gate12P0_RegistryExactNative_20261009.log`.
Full exact-test log: 284,012 bytes.

### Native Win64 compile

The pre-existing `Build.bat` launch wrapper held a stale lock/waiting path; only this Gate 12 attempt's wrapper was terminated, without touching other existing scripts or lock files. Invoked official UnrealBuildTool through bundled .NET as a non-destructive alternative:
```powershell
& 'D:\Epic Games\UE_5.8\Engine\Binaries\ThirdParty\DotNet\10.0\win-x64\dotnet.exe' 'D:\Epic Games\UE_5.8\Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.dll' OfflineGameEditor Win64 Development '-Project=D:\UnrealProjects\Offline-Game\OfflineGame.uproject' -WaitMutex -NoHotReloadFromIDE -MaxParallelActions=1 '-Log=D:\UnrealProjects\Offline-Game\Saved\Logs\Gate12P0_NewNativeTestUBT.log'
```
Compiler: VS 14.51.36248; engine warns toolchain is newer than preferred and emitted engine-header deprecation warnings. **149/149 UBT actions Succeeded**, process exit 0. `UHT` reported UE 5.8.3, zero generated headers written. New test translation unit `OGGate12RegistryAdapterNativeTests.cpp` compiled at action 66.
UBT output:
```
[149/149] WriteMetadata OfflineGameEditor.target [NoUba]
Result: Succeeded
Total execution time: 1002.45 seconds
```
UBT complete log: `D:\UnrealProjects\Offline-Game\Saved\Logs\Gate12P0_NewNativeTestUBT.log`, 47,950 bytes, SHA-256 `E6E6D5F4A75D9EB3C0D6C8D9A89602577900C4360C437EC327D8D61650644B4D`.

### Windows source validator + portability correction

```cmd
py -3 Scripts\validate_gate12_content_contract.py --self-test --check-runtime-source Source\OfflineGame\Private\UI\OGUiViewModelService.cpp --emit-adapter Saved\Gate12\RulerShellNativeDtoMap.json
fc /b Saved\Gate12\RulerShellNativeDtoMap.json Content\Data\Production\Gate12RegistryV2NativeDtoMap_SOURCE_ONLY.json
```
First actual Windows execution found CRLF output mismatch (source self-tests PASS, `fc` FAIL). Replaced `Path.write_text` with explicit `write_bytes(UTF8 + LF)` in **commit `4f52578312b373acb218b2e463e2d2499d9d0960`**, then reran:
```
PASS: 1 positive, 7 negative source-contract cases.
PASS: sample matches frozen content ID, DTO, dependency and safe omission rules (source-only).
FC: no differences encountered
```
Committed and emitted DTO SHA-256: `40a345883fbd0d88e702e2a314476cf19518850b0156914e25208514086c5f11`.

### Authority limits and gate decision

PASS covers **source-contract profile + exact source-only DTO native validation on a disposable world + existing optional/package engine guard tests**. Native temporary test package flags are set explicitly **by the isolated native fixture**, and are not read from Gate11 JSON or promoted to real package trust. It verifies test DB isolation and preserved package metadata, **not an adversarial diff of every populated player-world entity or device save**. No production UMG/region/mesh/`.uasset` was imported; no shipping pak install, Android cook, device profiling, player-save migration, or measured installed content bytes were tested or claimed. Gate11's tracking registry remains distinct from native content DTO and trusted installer. All production artifact status labels stay unchanged.

**For Gate00:** Gate12 P0 requested source/native compatibility acceptance is technically PASS. Keep PR #16 **draft and unmerged** until Gate00 review permits promotion; broader UI/World work is not authorized. Full evidence pack with original Gate11 source snapshot + hashes/test commands is supplied separately.
