#include "Diagnostics/OGDiagnosticsBundle.h"
#include "Persistence/OGSQLiteWorldStore.h"
#include "Persistence/OGSnapshotService.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
FString MakeRecoveryTestDirectory()
{
    return FPaths::Combine(
        FPaths::ProjectSavedDir(),
        TEXT("Automation"),
        FGuid::NewGuid().ToString(EGuidFormats::Digits));
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOGRecoverySnapshotRotationTest,
    "OfflineGame.Persistence.RecoverySnapshotRotation",
    EAutomationTestFlags::ApplicationContextMask |
        EAutomationTestFlags::EngineFilter)

bool FOGRecoverySnapshotRotationTest::RunTest(const FString& Parameters)
{
    const FString Directory = MakeRecoveryTestDirectory();
    const FString DatabasePath = FPaths::Combine(Directory, TEXT("world.db"));
    const FString SnapshotDirectory = FPaths::Combine(Directory, TEXT("snapshots"));
    IFileManager::Get().MakeDirectory(*Directory, true);

    FOGSQLiteWorldStore Store;
    FString Error;
    TestTrue(TEXT("Open database"), Store.Open(DatabasePath, Error));

    const FOGEntityId EntityId = FOGEntityId::NewId();
    TestTrue(TEXT("Persist state before first snapshot"), Store.UpsertEntity(
        EntityId, TEXT("ruler"), 0, TEXT("{\"step\":1}"), Error));

    FString FirstPath;
    TestTrue(TEXT("Create first rotating snapshot"),
        FOGSnapshotService::CreateRotatingSnapshot(
            Store, SnapshotDirectory, 1, FirstPath, Error));
    TestTrue(TEXT("First snapshot exists"),
        IFileManager::Get().FileExists(*FirstPath));

    TestTrue(TEXT("Mutate state before second snapshot"), Store.UpsertEntity(
        EntityId, TEXT("ruler"), 1, TEXT("{\"step\":2}"), Error));

    FString SecondPath;
    TestTrue(TEXT("Create second rotating snapshot"),
        FOGSnapshotService::CreateRotatingSnapshot(
            Store, SnapshotDirectory, 1, SecondPath, Error));
    TestTrue(TEXT("Second snapshot exists"),
        IFileManager::Get().FileExists(*SecondPath));

    TArray<FString> Snapshots;
    IFileManager::Get().FindFiles(
        Snapshots,
        *FPaths::Combine(SnapshotDirectory, TEXT("world_*.db")),
        true,
        false);
    TestEqual(TEXT("Rotation retains requested count"), Snapshots.Num(), 1);

    Store.Close();
    IFileManager::Get().DeleteDirectory(*Directory, false, true);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOGDiagnosticsBundleTest,
    "OfflineGame.Diagnostics.BundleExcludesAbsoluteSavePath",
    EAutomationTestFlags::ApplicationContextMask |
        EAutomationTestFlags::EngineFilter)

bool FOGDiagnosticsBundleTest::RunTest(const FString& Parameters)
{
    const FString Directory = MakeRecoveryTestDirectory();
    const FString DatabasePath = FPaths::Combine(Directory, TEXT("world.db"));
    const FString DiagnosticsDirectory = FPaths::Combine(Directory, TEXT("diagnostics"));
    IFileManager::Get().MakeDirectory(*Directory, true);

    FOGSQLiteWorldStore Store;
    FString Error;
    TestTrue(TEXT("Open database"), Store.Open(DatabasePath, Error));

    FOGPerformanceTelemetry Telemetry;
    Telemetry.RecordFrame(0.020);

    FString BundlePath;
    TestTrue(TEXT("Write diagnostics bundle"),
        FOGDiagnosticsBundle::Write(
            Store,
            DiagnosticsDirectory,
            Telemetry.Snapshot(),
            BundlePath,
            Error));
    TestTrue(TEXT("Diagnostics file exists"),
        IFileManager::Get().FileExists(*BundlePath));

    FString Bundle;
    TestTrue(TEXT("Read diagnostics file"),
        FFileHelper::LoadFileToString(Bundle, *BundlePath));
    TestTrue(TEXT("Bundle contains schema version"),
        Bundle.Contains(TEXT("\"schema_version\"")));
    TestTrue(TEXT("Bundle contains integrity result"),
        Bundle.Contains(TEXT("\"integrity_ok\"")));
    TestTrue(TEXT("Bundle contains performance telemetry"),
        Bundle.Contains(TEXT("\"performance\"")));
    TestTrue(TEXT("Bundle contains frame budget data"),
        Bundle.Contains(TEXT("\"frames_over_budget\"")));
    TestTrue(TEXT("Bundle contains only clean database filename"),
        Bundle.Contains(TEXT("\"database_file\": \"world.db\"")) ||
        Bundle.Contains(TEXT("\"database_file\":\"world.db\"")));
    TestFalse(TEXT("Bundle excludes absolute database path"),
        Bundle.Contains(*Directory));

    Store.Close();
    IFileManager::Get().DeleteDirectory(*Directory, false, true);
    return true;
}

#endif
