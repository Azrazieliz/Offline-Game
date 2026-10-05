#include "Persistence/OGSQLiteWorldStore.h"
#include "Runtime/OGPerformanceTelemetry.h"
#include "Runtime/OGVerticalSliceScenario.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

namespace
{
FString MakeG2TestDirectory()
{
    return FPaths::Combine(
        FPaths::ProjectSavedDir(),
        TEXT("Automation"),
        FGuid::NewGuid().ToString(
            EGuidFormats::Digits));
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOGVerticalSliceRuntimeHarnessTest,
    "OfflineGame.VerticalSlice.G2.PersistentEndToEndRestart",
    EAutomationTestFlags::ApplicationContextMask |
        EAutomationTestFlags::EngineFilter)

bool FOGVerticalSliceRuntimeHarnessTest::RunTest(
    const FString& Parameters)
{
    const FString Directory =
        MakeG2TestDirectory();
    const FString DatabasePath =
        FPaths::Combine(
            Directory,
            TEXT("g2_slice.db"));

    IFileManager::Get().MakeDirectory(
        *Directory,
        true);

    FOGVerticalSliceScenarioResult Fresh;

    {
        FOGSQLiteWorldStore Store;
        FString Error;

        TestTrue(
            TEXT("Open fresh G2 world store"),
            Store.Open(
                DatabasePath,
                Error));

        TestEqual(
            TEXT("Schema version is 8"),
            Store.GetSchemaVersion(Error),
            8);

        TestTrue(
            TEXT("Run persistent vertical-slice scenario"),
            FOGVerticalSliceScenarioHarness::RunFresh(
                Store,
                Fresh,
                Error));

        TestTrue(
            TEXT("Fresh scenario reports success"),
            Fresh.bSucceeded);

        TestTrue(
            TEXT("Fresh scenario persists a battle fingerprint"),
            !Fresh.BattleFingerprint.IsEmpty());

        Store.Close();
    }

    {
        FOGSQLiteWorldStore Store;
        FString Error;
        FOGVerticalSliceScenarioResult Restarted;

        TestTrue(
            TEXT("Reopen G2 world store"),
            Store.Open(
                DatabasePath,
                Error));

        TestTrue(
            TEXT("Verify same cross-mode history after restart"),
            FOGVerticalSliceScenarioHarness::VerifyAfterRestart(
                Store,
                Restarted,
                Error));

        TestTrue(
            TEXT("Restart verification reports success"),
            Restarted.bSucceeded);

        TestEqual(
            TEXT("Turn-battle fingerprint survives restart"),
            Restarted.BattleFingerprint,
            Fresh.BattleFingerprint);

        TestTrue(
            TEXT("Acquired Manifestation identity survives restart"),
            Restarted.ManifestationId ==
                Fresh.ManifestationId);

        TestTrue(
            TEXT("Completed Project identity survives restart"),
            Restarted.ProjectId ==
                Fresh.ProjectId);

        Store.Close();
    }

    IFileManager::Get().DeleteDirectory(
        *Directory,
        false,
        true);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOGPerformanceTelemetryAggregationTest,
    "OfflineGame.Runtime.PerformanceTelemetry.ConstantMemoryBudgets",
    EAutomationTestFlags::ApplicationContextMask |
        EAutomationTestFlags::EngineFilter)

bool FOGPerformanceTelemetryAggregationTest::RunTest(
    const FString& Parameters)
{
    FOGPerformanceTelemetry Telemetry;

    TestEqual(
        TEXT("Default profile targets 60 FPS"),
        FOGPerformanceTelemetry::TargetFpsForProfile(
            EOGPerformanceProfile::Default60),
        60);

    Telemetry.RecordFrame(0.010);
    Telemetry.RecordFrame(0.020);
    Telemetry.RecordFrame(0.040);

    FOGPerformanceTelemetrySnapshot Snapshot =
        Telemetry.Snapshot();

    TestEqual(
        TEXT("All valid frames are counted"),
        Snapshot.FrameCount,
        static_cast<int64>(3));

    TestEqual(
        TEXT("60 FPS budget misses are counted"),
        Snapshot.FramesOverBudget,
        static_cast<int64>(2));

    TestEqual(
        TEXT("Two-times-budget hitch is counted"),
        Snapshot.HitchCount,
        static_cast<int64>(1));

    TestTrue(
        TEXT("Worst frame is retained"),
        FMath::IsNearlyEqual(
            Snapshot.WorstFrameMs,
            40.0,
            0.001));

    Telemetry.SetProfile(
        EOGPerformanceProfile::High120);
    Snapshot = Telemetry.Snapshot();

    TestEqual(
        TEXT("Profile change selects 120 FPS budget"),
        Snapshot.TargetFps,
        120);

    TestEqual(
        TEXT("Profile change resets aggregation window"),
        Snapshot.FrameCount,
        static_cast<int64>(0));

    return true;
}

#endif
