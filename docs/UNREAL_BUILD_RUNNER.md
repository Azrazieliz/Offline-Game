# Unreal Build/Test Runner

## Mandatory gate

Every real Unreal test uses this order:

1. cheap C++/UHT-oriented preflight;
2. immediate source fixes if preflight fails;
3. repeat preflight until green;
4. only then compile with UnrealBuildTool/UHT;
5. only after a successful compile, run the OfflineGame Unreal automation tests.

The self-hosted workflow enforces this with a preflight job dependency and also re-runs the preflight on the build runner before invoking Unreal.

## Engine image

Default:
`ghcr.io/epicgames/unreal-engine:dev-slim-5.8`

The GitHub account/token used to pull it must have EpicGames Unreal source/package access and `read:packages`.

## Smoke command

`Scripts/unreal_container_smoke.sh`

The script itself starts by running `Scripts/verify_unreal_cpp.py`, so direct/manual invocations cannot accidentally bypass the gate.

It then:

- pulls the UE 5.8 slim development image if absent;
- compiles `OfflineGameEditor` for Linux Development;
- starts `UnrealEditor-Cmd` with NullRHI;
- runs the complete `OfflineGame` automation-test prefix;
- exports reports under `Saved/Automation/Reports`.

## Runner label

The manual workflow expects a runner labeled:

- `self-hosted`
- `linux`
- `x64`
- `unreal-5.8`

## Cost policy

The project-wide paid cloud-compute ceiling remains USD 25 cumulative.

Free GitHub validation/preflight is always used first. Paid compute is only justified after the preflight is green and is destroyed/stopped when the requested Unreal validation work is complete.
