# Build Compute Budget

## Hard ceiling

Total paid external/cloud compute budget for this project is **USD 25 cumulative** unless the Creative Director explicitly changes it later.

This is not a per-run, per-call, monthly, or per-provider allowance.

## Spending policy

1. Use free GitHub Actions for every task they can handle.
2. Use free credits/trials only when they are genuinely suitable for Unreal work.
3. Keep runner/bootstrap logic provider-agnostic.
4. Paid compute is reserved for Unreal compilation, automation tests, cooking, and Android packaging when free infrastructure cannot perform the job.
5. Ephemeral paid runners must be destroyed after the required build/test work.
6. Track cumulative known paid spend across providers.
7. Never intentionally exceed USD 25 total.

Current recorded paid spend: **USD 0**.
