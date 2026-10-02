#include "Runtime/OGGameCoreSubsystem.h"

#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "OfflineGame.h"
#include "Persistence/OGSQLiteWorldStore.h"

void UOGGameCoreSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    const FString DatabaseDirectory =
        FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("OfflineGame"));
    IFileManager::Get().MakeDirectory(*DatabaseDirectory, true);

    const FString DatabasePath =
        FPaths::Combine(DatabaseDirectory, TEXT("WorldState.db"));

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
    UE_LOG(
        LogOfflineGame,
        Log,
        TEXT("Authoritative game core initialized with schema version %d."),
        WorldStore->GetSchemaVersion(Error));
}

void UOGGameCoreSubsystem::Deinitialize()
{
    bCoreReady = false;

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
