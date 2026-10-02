#include "Runtime/OGPerformanceTelemetry.h"

int32 FOGPerformanceTelemetry::TargetFpsForProfile(
    EOGPerformanceProfile InProfile)
{
    switch (InProfile)
    {
    case EOGPerformanceProfile::Thermal30:
        return 30;
    case EOGPerformanceProfile::High120:
        return 120;
    case EOGPerformanceProfile::Default60:
    default:
        return 60;
    }
}

void FOGPerformanceTelemetry::SetProfile(
    EOGPerformanceProfile InProfile)
{
    if (Profile == InProfile)
    {
        return;
    }

    Profile = InProfile;
    Reset();
}

void FOGPerformanceTelemetry::Reset()
{
    FrameCount = 0;
    FramesOverBudget = 0;
    HitchCount = 0;
    TotalFrameMs = 0.0;
    WorstFrameMs = 0.0;
    LastFrameMs = 0.0;
}

void FOGPerformanceTelemetry::RecordFrame(double DeltaSeconds)
{
    if (!FMath::IsFinite(DeltaSeconds) ||
        DeltaSeconds <= 0.0)
    {
        return;
    }

    LastFrameMs = DeltaSeconds * 1000.0;
    const double BudgetMs =
        1000.0 /
        static_cast<double>(
            TargetFpsForProfile(Profile));

    ++FrameCount;
    TotalFrameMs += LastFrameMs;
    WorstFrameMs =
        FMath::Max(WorstFrameMs, LastFrameMs);

    if (LastFrameMs > BudgetMs)
    {
        ++FramesOverBudget;
    }

    if (LastFrameMs > BudgetMs * 2.0)
    {
        ++HitchCount;
    }
}

FOGPerformanceTelemetrySnapshot
FOGPerformanceTelemetry::Snapshot() const
{
    FOGPerformanceTelemetrySnapshot Result;
    Result.Profile = Profile;
    Result.TargetFps =
        TargetFpsForProfile(Profile);
    Result.FrameCount = FrameCount;
    Result.FramesOverBudget =
        FramesOverBudget;
    Result.HitchCount = HitchCount;
    Result.AverageFrameMs =
        FrameCount > 0
            ? TotalFrameMs /
                static_cast<double>(FrameCount)
            : 0.0;
    Result.WorstFrameMs = WorstFrameMs;
    Result.LastFrameMs = LastFrameMs;
    return Result;
}
