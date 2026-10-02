# Unreal Build/Test Runner — GitHub Codespaces

## Selected runner

GitHub Codespaces is the active Unreal validation environment.

There is no DigitalOcean or self-hosted-runner path in the active workflow.

The repository's `.devcontainer/devcontainer.json` requires a Codespaces host with at least:

- 8 vCPU
- 24 GB RAM
- 64 GB storage

GitHub will only offer machine types that meet or exceed those declared host requirements.

## Mandatory verification gate

Every real Unreal test follows this exact order:

1. run `python3 Scripts/verify_unreal_cpp.py`;
2. if verification reports any error, do not launch Unreal;
3. fix the detected issues;
4. rerun verification until green;
5. compile through UnrealBuildTool/UHT;
6. only after a successful compile, run the `OfflineGame` automation tests.

The build script itself re-runs preflight, so the gate is enforced even if the command is launched directly from the Codespace terminal.

## Codespace initialization

On first creation, the Codespace automatically runs the cheap C++ preflight through `postCreateCommand`.

The dev-container configuration also recommends a Codespaces secret named `CR_PAT`.

`CR_PAT` must belong to the Epic-linked GitHub account and have `read:packages`, because Epic's Unreal Engine development container is hosted in GitHub Container Registry.

Do not commit this token.

## Real Unreal validation

From the Codespace terminal:

`Scripts/unreal_container_smoke.sh`

Or run the VS Code task:

`Unreal 5.8: Verify + Compile + Test`

Default Unreal image:

`ghcr.io/epicgames/unreal-engine:dev-slim-5.8`

The smoke run:

- verifies the C++ repository first;
- authenticates to GHCR when needed;
- pulls the UE 5.8 slim development image;
- compiles `OfflineGameEditor` for Linux Development;
- launches `UnrealEditor-Cmd` headlessly with NullRHI;
- runs all tests under the `OfflineGame` automation prefix;
- exports reports to `Saved/Automation/Reports`.

## Disk rule

The first Codespaces run is also the disk-fit test.

If the 64 GB host cannot hold the Unreal image plus project intermediates, use a larger Codespaces machine/storage option rather than switching providers.

## Cost rule

Use included Codespaces allowance first.

Any billable Codespaces usage counts against the project's USD 25 cumulative compute ceiling.
