#include "Persistence/OGSQLiteWorldStore.h"
#include "Runtime/OGPerformanceTelemetry.h"
#include "Runtime/OGVerticalSliceScenario.h"
#include "World/OGStartingRegionPresentation.h"
#include "World/OGTraversalFramework.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

namespace
{
FString MakeVerticalSlice0TestDirectory()
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
    "OfflineGame.VerticalSlice.Reconciled.PersistentEndToEndRestart",
    EAutomationTestFlags_ApplicationContextMask |
        EAutomationTestFlags::EngineFilter)

bool FOGVerticalSliceRuntimeHarnessTest::RunTest(
    const FString& Parameters)
{
    const FString Directory =
        MakeVerticalSlice0TestDirectory();
    const FString DatabasePath =
        FPaths::Combine(
            Directory,
            TEXT("vertical_slice0.db"));

    IFileManager::Get().MakeDirectory(
        *Directory,
        true);

    FOGVerticalSliceScenarioResult Fresh;

    {
        FOGSQLiteWorldStore Store;
        FString Error;

        TestTrue(
            TEXT("Open fresh reconciled Vertical Slice 0 world store"),
            Store.Open(
                DatabasePath,
                Error));

        TestEqual(
            TEXT("Schema version is 14"),
            Store.GetSchemaVersion(Error),
            14);

        const bool bFreshRan =
            FOGVerticalSliceScenarioHarness::RunFresh(
                Store,
                Fresh,
                Error);
        TestTrue(
            FString::Printf(
                TEXT("Run persistent vertical-slice scenario: %s"),
                *Error),
            bFreshRan);

        TestTrue(
            TEXT("Fresh scenario reports success"),
            Fresh.bSucceeded);

        TestTrue(
            TEXT("Fresh scenario persists a turn-battle fingerprint"),
            !Fresh.TurnBattleFingerprint.IsEmpty());

        TestTrue(
            TEXT("Fresh scenario persists action-combat presentation"),
            !Fresh.ActionDamageDisplay.IsEmpty());

        TestTrue(
            TEXT("Repeated pull creates two distinct full Manifestations"),
            Fresh.FirstManifestationId.IsValid() &&
            Fresh.SecondManifestationId.IsValid() &&
            Fresh.FirstManifestationId !=
                Fresh.SecondManifestationId);

        TestTrue(
            TEXT("Project proof identity is persisted"),
            Fresh.ProjectId.IsValid());

        TestTrue(
            TEXT("Objective-driven Dispatch proof identity is persisted"),
            Fresh.DispatchId.IsValid());

        TestTrue(
            TEXT("Continuous War proof identity is persisted"),
            Fresh.WarId.IsValid() &&
            Fresh.WarFrontId.IsValid());

        TestTrue(
            TEXT("Action consequence and Report provenance IDs are persisted"),
            Fresh.ActionCombatEventId.IsValid() &&
            Fresh.ReportId.IsValid());

        Store.Close();
    }

    {
        FOGSQLiteWorldStore Store;
        FString Error;
        FOGVerticalSliceScenarioResult Restarted;

        TestTrue(
            TEXT("Reopen reconciled Vertical Slice 0 world store"),
            Store.Open(
                DatabasePath,
                Error));

        const bool bRestartVerified =
            FOGVerticalSliceScenarioHarness::VerifyAfterRestart(
                Store,
                Restarted,
                Error);
        TestTrue(
            FString::Printf(
                TEXT("Verify same cross-mode history after restart: %s"),
                *Error),
            bRestartVerified);

        TestTrue(
            TEXT("Restart verification reports success"),
            Restarted.bSucceeded);

        TestEqual(
            TEXT("Turn-battle fingerprint survives restart"),
            Restarted.TurnBattleFingerprint,
            Fresh.TurnBattleFingerprint);

        TestEqual(
            TEXT("Action-combat presentation checkpoint survives restart"),
            Restarted.ActionDamageDisplay,
            Fresh.ActionDamageDisplay);

        TestTrue(
            TEXT("First independent Manifestation survives restart"),
            Restarted.FirstManifestationId ==
                Fresh.FirstManifestationId);

        TestTrue(
            TEXT("Second independent Manifestation survives restart"),
            Restarted.SecondManifestationId ==
                Fresh.SecondManifestationId);

        TestTrue(
            TEXT("Completed Project identity survives restart"),
            Restarted.ProjectId ==
                Fresh.ProjectId);

        TestTrue(
            TEXT("Objective-driven Dispatch identity survives restart"),
            Restarted.DispatchId ==
                Fresh.DispatchId);

        TestTrue(
            TEXT("Continuous War and front identities survive restart"),
            Restarted.WarId ==
                Fresh.WarId &&
            Restarted.WarFrontId ==
                Fresh.WarFrontId);

        TestTrue(
            TEXT("Action consequence and Report provenance survive restart"),
            Restarted.ActionCombatEventId ==
                Fresh.ActionCombatEventId &&
            Restarted.ReportId ==
                Fresh.ReportId);

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
    EAutomationTestFlags_ApplicationContextMask |
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOGWorldSafetyPolicyTest,
    "OfflineGame.Runtime.WorldSafety.MapBoundaryRecoveryPolicy",
    EAutomationTestFlags_ApplicationContextMask |
        EAutomationTestFlags::EngineFilter)

bool FOGWorldSafetyPolicyTest::RunTest(
    const FString& Parameters)
{
    constexpr float PlayableHalfExtent = 38400.0f;
    constexpr float TerrainCellSize = 1200.0f;

    const float SafeHalfExtent =
        FOGWorldSafetyPolicy::GetSafeHalfExtent(
            PlayableHalfExtent,
            TerrainCellSize);

    TestEqual(
        TEXT("Boundary inset yields expected safe half extent"),
        SafeHalfExtent,
        36600.0f);

    TestFalse(
        TEXT("Interior point remains playable"),
        FOGWorldSafetyPolicy::IsOutsidePlayableRegion(
            FVector(35000.0f, 0.0f, 0.0f),
            PlayableHalfExtent,
            TerrainCellSize));

    TestTrue(
        TEXT("Point beyond safe edge requires recovery"),
        FOGWorldSafetyPolicy::IsOutsidePlayableRegion(
            FVector(36601.0f, 0.0f, 0.0f),
            PlayableHalfExtent,
            TerrainCellSize));

    TestFalse(
        TEXT("Deep vertical traversal remains legal while XY stays in bounds"),
        FOGWorldSafetyPolicy::IsOutsidePlayableRegion(
            FVector(0.0f, 0.0f, -1000000.0f),
            PlayableHalfExtent,
            TerrainCellSize));

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOGTraversalCapabilityFrameworkTest,
    "OfflineGame.Runtime.Traversal.CapabilityFramework",
    EAutomationTestFlags_ApplicationContextMask |
        EAutomationTestFlags::EngineFilter)

bool FOGTraversalCapabilityFrameworkTest::RunTest(
    const FString& Parameters)
{
    UOGTraversalCapabilityComponent* Traversal =
        NewObject<UOGTraversalCapabilityComponent>();

    TestNotNull(
        TEXT("Traversal capability component can be instantiated"),
        Traversal);
    if (!Traversal)
    {
        return false;
    }

    TestTrue(
        TEXT("Ordinary sprint is baseline capability"),
        Traversal->HasCapability(
            FOGTraversalCapabilityIds::Sprint));
    TestTrue(
        TEXT("Baseline climb is available without generic stamina gating"),
        Traversal->HasCapability(
            FOGTraversalCapabilityIds::Climb));
    TestTrue(
        TEXT("Baseline swimming is supported"),
        Traversal->HasCapability(
            FOGTraversalCapabilityIds::Swim));
    TestFalse(
        TEXT("True flight remains content-granted"),
        Traversal->HasCapability(
            FOGTraversalCapabilityIds::Flight));

    FOGTraversalRequirement Requirement;
    Requirement.RequiredAll = {
        FOGTraversalCapabilityIds::Swim,
        FOGTraversalCapabilityIds::Dive
    };

    TestTrue(
        TEXT("All-of traversal requirements are evaluated"),
        Traversal->SatisfiesRequirement(
            Requirement));

    Requirement.RequiredAny = {
        FOGTraversalCapabilityIds::Flight,
        FOGTraversalCapabilityIds::Teleport
    };

    TestFalse(
        TEXT("Any-of requirement fails before exotic capability grant"),
        Traversal->SatisfiesRequirement(
            Requirement));

    Traversal->GrantCapability(
        FOGTraversalCapabilityIds::Flight);

    TestTrue(
        TEXT("Capability grants immediately satisfy traversal gates"),
        Traversal->SatisfiesRequirement(
            Requirement));

    TestEqual(
        TEXT("Flying mode maps to flight capability"),
        UOGTraversalCapabilityComponent::RequiredCapabilityForMode(
            EOGTraversalMode::Flying),
        FOGTraversalCapabilityIds::Flight);

    Traversal->SetTraversalMode(
        EOGTraversalMode::Flying);
    TestEqual(
        TEXT("Traversal mode is explicit and queryable"),
        Traversal->GetTraversalMode(),
        EOGTraversalMode::Flying);

    return true;
}

#endif
