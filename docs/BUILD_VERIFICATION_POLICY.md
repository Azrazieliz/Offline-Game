# Build Verification Policy

Before every real Unreal compile/test run:

1. Run `python3 Scripts/verify_unreal_cpp.py`.
2. If it reports any error, do **not** launch the Unreal test.
3. Fix the reported source/configuration issues immediately.
4. Re-run preflight until it passes.
5. Only then launch the Unreal compile/automation-test run.
6. If Unreal compilation or tests expose additional problems, fix them and repeat from step 1.

The preflight is intentionally cheap and runs on standard GitHub Actions before expensive Unreal compute.

It currently checks:

- Unreal Header Tool generated-header ordering/presence;
- module-local include existence;
- reflected-property hazards caught textually, including Blueprint-incompatible integer exposure;
- SQLite world-store declaration/definition parity and duplicate definitions;
- migration/schema-version test consistency;
- automation-test declaration/implementation parity;
- required module dependencies inferred from source usage;
- Unreal 5.8 project/module/plugin descriptor consistency;
- selected stale architecture/code regressions such as removed Character Identity Version lists.

This does not replace UnrealBuildTool/UHT/compiler/automation tests. It is the mandatory low-cost gate before them.
