#pragma once

#include "CoreMinimal.h"
#include "OGPerformanceTelemetry.generated.h"

UENUM(BlueprintType)
enum class EOGPerformanceProfile : uint8
{
    Thermal30,
    Default60,
    High120
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGPerformanceTelemetrySnapshot
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    EOGPerformanceProfile Profile = EOGPerformanceProfile::Default60;

    UPROPERTY(BlueprintReadOnly)
    int32 TargetFps = 60;

    UPROPERTY(BlueprintReadOnly)
    int64 FrameCount = 0;

    UPROPERTY(BlueprintReadOnly)
    int64 FramesOverBudget = 0;

    UPROPERTY(BlueprintReadOnly)
    int64 HitchCount = 0;

    UPROPERTY(BlueprintReadOnly)
    double AverageFrameMs = 0.0;

    UPROPERTY(BlueprintReadOnly)
    double WorstFrameMs = 0.0;

    UPROPERTY(BlueprintReadOnly)
    double LastFrameMs = 0.0;
};

/**
 * Constant-memory runtime telemetry for device profiling.
 *
 * Raw frame histories are intentionally not retained. The profiler records only
 * aggregates needed to identify sustained budget misses and hitches.
 */
class OFFLINEGAME_API FOGPerformanceTelemetry
{
public:
    void SetProfile(EOGPerformanceProfile InProfile);
    void Reset();
    void RecordFrame(double DeltaSeconds);

    EOGPerformanceProfile GetProfile() const { return Profile; }
    FOGPerformanceTelemetrySnapshot Snapshot() const;

    static int32 TargetFpsForProfile(EOGPerformanceProfile InProfile);

private:
    EOGPerformanceProfile Profile = EOGPerformanceProfile::Default60;
    int64 FrameCount = 0;
    int64 FramesOverBudget = 0;
    int64 HitchCount = 0;
    double TotalFrameMs = 0.0;
    double WorstFrameMs = 0.0;
    double LastFrameMs = 0.0;
};
