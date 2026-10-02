# Unreal Build/Test Runner

## Current split

GitHub Codespaces is used for lightweight repository work and the mandatory
Unreal C++ preflight. The machine types currently available to this account are
limited to 32 GB storage, so Codespaces is **not** the active full Unreal 5.8
compile/test environment.

The repository intentionally has no Codespaces `hostRequirements` filter.
The existing Codespace remains useful for source work, migrations, scripts, and
cheap validation.

The current real Unreal validation path is the Windows fallback machine using
Epic's prebuilt Unreal Engine 5.8 on the large removable D: storage. The
self-hosted workflow is `.github/workflows/unreal-windows-self-hosted.yml`.

There is no active DigitalOcean build path.

## Mandatory verification gate

Every real Unreal test follows this exact order:

1. run `python3 Scripts/verify_unreal_cpp.py`;
2. if verification reports any error, do not launch Unreal;
3. fix the detected issues;
4. rerun verification until green;
5. compile through UnrealBuildTool/UHT;
6. only after a successful compile, run the `OfflineGame` automation tests;
7. if Unreal exposes another issue, fix it and restart from preflight.

The Windows smoke script re-runs preflight on the exact checked-out commit, so
the heavy runner cannot bypass the cheap gate.

## Codespace state

On creation, the Codespace runs the preflight through `postCreateCommand`.

The dev-container still documents the optional `CR_PAT` secret because it can
authenticate to Epic's GHCR image if a future Codespaces machine has enough
storage. That token must belong to the Epic-linked GitHub account and must never
be committed.

Do not pull the Unreal container into the current 32 GB Codespace.

## Windows Unreal validation

The Windows path uses `Scripts/windows_unreal_smoke.ps1`.

It:

- runs the repository preflight;
- locates a prebuilt UE 5.8 installation across available drives or `UE_ROOT`;
- limits UnrealBuildTool to one parallel action for the 8 GB fallback machine;
- compiles `OfflineGameEditor` for Win64 Development;
- only after compilation succeeds, launches headless automation with NullRHI;
- exports reports to `Saved/Automation/Reports`.

The full UE 5.8 compile/automation pass has **not yet been completed**. Static
GitHub validation is green, but that is not a substitute for UHT/UBT/compiler
execution.

## Storage rule

Large Unreal assets, engine files, intermediates, and build data belong on the
large D: storage. The system C: drive on the current fallback machine is too
constrained for an Unreal installation.

## Cost rule

Use free GitHub Actions/Codespaces allowance for tasks they can handle.

Any paid compute counts against the project's USD 25 cumulative ceiling. Current
recorded paid compute spend remains USD 0.
