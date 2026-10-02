#pragma once

#include "CoreMinimal.h"
#include "Persistence/OGWorldStore.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "OGGameCoreSubsystem.generated.h"

/**
 * Application-level owner of the authoritative game-core lifetime.
 *
 * Presentation systems may query/command the core through services exposed from
 * here, but World Mode actors and Ruler Mode widgets must not become sources of
 * authoritative state.
 */
UCLASS()
class OFFLINEGAME_API UOGGameCoreSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    UFUNCTION(BlueprintPure, Category = "OfflineGame|Core")
    bool IsCoreReady() const { return bCoreReady; }

    IOGWorldStore* GetWorldStore() const { return WorldStore.Get(); }

private:
    TUniquePtr<IOGWorldStore> WorldStore;
    bool bCoreReady = false;
};
