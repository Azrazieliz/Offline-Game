#include "Runtime/OGGameCoreSubsystem.h"

#include "OfflineGame.h"

void UOGGameCoreSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    // Bootstrap only. Persistence/content services are attached in subsequent
    // architecture milestones after migration and failure semantics are tested.
    bCoreReady = true;
    UE_LOG(LogOfflineGame, Log, TEXT("Authoritative game core subsystem initialized."));
}

void UOGGameCoreSubsystem::Deinitialize()
{
    bCoreReady = false;
    UE_LOG(LogOfflineGame, Log, TEXT("Authoritative game core subsystem deinitialized."));

    Super::Deinitialize();
}
