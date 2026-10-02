#include "Runtime/OGGameCoreSubsystem.h"

#include "Containers/Ticker.h"
#include "Diagnostics/OGDiagnosticsBundle.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "OfflineGame.h"
#include "Persistence/OGSQLiteWorldStore.h"
#include "Persistence/OGSnapshotService.h"

void UOGGameCoreSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    PerformanceTelemetry.Reset();
    PerformanceTickerHandle =
        FTSTicker::GetCoreTicker().AddTicker(
            FTickerDelegate::CreateUObject(
                this,
                &UOGGameCoreSubsystem::TickPerformanceTelemetry));

    const FString DatabaseDirectory =
        FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("OfflineGame"));
    IFileManager::Get().MakeDirectory(*DatabaseDirectory, true);

    const FString DatabasePath =
        FPaths::Combine(DatabaseDirectory, TEXT("WorldState.db"));
    const bool bExistingDatabase =
        IFileManager::Get().FileExists(*DatabasePath);

    WorldStore = MakeUnique<FOGSQLiteWorldStore>();

    FString Error;
    if (!WorldStore->Open(DatabasePath, Error))
    {
        UE_LOG(
            LogOfflineGame,
            Error,
            TEXT("Failed to initialize authoritative world database: %s"),
            *Error);
        WorldStore.Reset();
        bCoreReady = false;
        return;
    }

    bCoreReady = true;

    if (bExistingDatabase)
    {
        FString SnapshotPath;
        FString SnapshotError;
        const FString SnapshotDirectory =
            FPaths::Combine(DatabaseDirectory, TEXT("Snapshots"));

        if (!FOGSnapshotService::CreateRotatingSnapshot(
                *WorldStore,
                SnapshotDirectory,
                3,
                SnapshotPath,
                SnapshotError))
        {
            UE_LOG(
                LogOfflineGame,
                Warning,
                TEXT("Automatic recovery snapshot failed: %s"),
                *SnapshotError);
        }
    }

    UE_LOG(
        LogOfflineGame,
        Log,
        TEXT("Authoritative game core initialized with schema version %d."),
        WorldStore->GetSchemaVersion(Error));
}

bool UOGGameCoreSubsystem::TickPerformanceTelemetry(
    float DeltaSeconds)
{
    PerformanceTelemetry.RecordFrame(
        static_cast<double>(DeltaSeconds));
    return true;
}

bool UOGGameCoreSubsystem::GenerateDiagnosticsBundle(
    FString& OutBundlePath,
    FString& OutError)
{
    OutBundlePath.Reset();
    OutError.Reset();

    if (!WorldStore || !WorldStore->IsOpen())
    {
        OutError = TEXT("Authoritative game core is not ready.");
        return false;
    }

    const FString OutputDirectory =
        FPaths::Combine(
            FPaths::ProjectSavedDir(),
            TEXT("OfflineGame"),
            TEXT("Diagnostics"));

    return FOGDiagnosticsBundle::Write(
        *WorldStore,
        OutputDirectory,
        OutBundlePath,
        OutError);
}

void UOGGameCoreSubsystem::Deinitialize()
{
    bCoreReady = false;

    if (PerformanceTickerHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(
            PerformanceTickerHandle);
        PerformanceTickerHandle =
            FDelegateHandle();
    }

    if (WorldStore)
    {
        FString CheckpointError;
        if (!WorldStore->Checkpoint(CheckpointError))
        {
            UE_LOG(
                LogOfflineGame,
                Warning,
                TEXT("World database checkpoint failed during shutdown: %s"),
                *CheckpointError);
        }

        WorldStore->Close();
        WorldStore.Reset();
    }

    UE_LOG(LogOfflineGame, Log, TEXT("Authoritative game core subsystem deinitialized."));
    Super::Deinitialize();
}
