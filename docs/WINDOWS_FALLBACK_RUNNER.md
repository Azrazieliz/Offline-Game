# Windows fallback Unreal runner

The currently available personal Codespaces machines are limited to 32 GB
storage, which is not enough for the Unreal 5.8 development image plus build
headroom.

The fallback Windows machine is intentionally treated as a correctness runner,
not as the final development workstation.

Current hardware profile:
- Windows 11 x64
- Intel Core i3-1115G4, 2 physical cores / 4 logical threads
- 8 GB RAM
- 512 GB removable storage available

Epic recommends substantially stronger hardware for normal UE5 development.
For this fallback runner, the project uses Epic's prebuilt UE 5.8 engine and
never attempts to build the engine from source.

The Windows smoke script:
1. runs Scripts/verify_unreal_cpp.py;
2. refuses to continue if preflight fails;
3. discovers UE_5.8 across available drives or UE_ROOT;
4. writes a project-local UnrealBuildTool configuration with MaxParallelActions=1;
5. compiles OfflineGameEditor Win64 Development;
6. only if compilation succeeds, runs OfflineGame automation tests with NullRHI and no sound;
7. writes reports to Saved/Automation/Reports.

Recommended storage split:
- UE 5.8 installation: 512 GB storage device
- GitHub runner and repository: internal SSD when practical
- Windows pagefile: internal SSD rather than the SD card when practical

The low-memory setting is deliberately conservative. The first successful build
is about correctness, not speed.

One-time runner bootstrap:
- run Scripts/setup_windows_runner.ps1 in PowerShell;
- authenticate GitHub CLI once if requested;
- install prebuilt Unreal Engine 5.8 with Epic Games Launcher;
- start C:\actions-runner\run.cmd for the first validation.

The GitHub workflow is .github/workflows/unreal-windows-self-hosted.yml.
It always performs a cheap hosted preflight before scheduling the Windows PC.
