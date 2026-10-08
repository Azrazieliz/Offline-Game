#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "OGPresentationModeSubsystem.generated.h"

class UActorComponent;

UENUM(BlueprintType)
enum class EOGPresentationMode : uint8
{
    Ruler,
    World
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FOGPresentationModeChanged,
    EOGPresentationMode,
    NewMode);

/**
 * Owns the presentation-mode boundary without owning canonical world state.
 * Ruler Mode suspends registered expensive 3D presentation while the
 * authoritative simulation/database remain alive.
 */
UCLASS()
class OFFLINEGAME_API UOGPresentationModeSubsystem
    : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    UFUNCTION(BlueprintPure, Category="OfflineGame|Presentation")
    EOGPresentationMode GetMode() const { return CurrentMode; }

    UFUNCTION(BlueprintPure, Category="OfflineGame|Presentation")
    bool IsWorldPresentationSuspended() const { return bWorldPresentationSuspended; }

    UFUNCTION(BlueprintCallable, Category="OfflineGame|Presentation")
    void SetMode(EOGPresentationMode NewMode);

    UFUNCTION(BlueprintCallable, Category="OfflineGame|Presentation")
    void RegisterWorldPresentationActor(AActor* Actor);

    UFUNCTION(BlueprintCallable, Category="OfflineGame|Presentation")
    void UnregisterWorldPresentationActor(AActor* Actor);

    UPROPERTY(BlueprintAssignable, Category="OfflineGame|Presentation")
    FOGPresentationModeChanged OnPresentationModeChanged;

private:
    void DiscoverTaggedWorldPresentationActors();
    void SuspendWorldPresentation();
    void RestoreWorldPresentation();
    void SuspendActorPresentation(AActor* Actor);
    void RestoreActorPresentation(AActor* Actor);
    void ApplyOrientationForMode(EOGPresentationMode Mode);
    void ApplyWorldInputSuppression(bool bSuppress);

    TWeakObjectPtr<class APlayerController> SuppressedController;
    bool bOwnsInputSuppression = false;
    EOGPresentationMode CurrentMode = EOGPresentationMode::World;
    bool bWorldPresentationSuspended = false;

    TSet<TWeakObjectPtr<AActor>> RegisteredWorldActors;
    TMap<TWeakObjectPtr<AActor>, bool> PriorHiddenState;
    TMap<TWeakObjectPtr<AActor>, bool> PriorTickState;
    TMap<TWeakObjectPtr<AActor>, TMap<TWeakObjectPtr<UActorComponent>, bool>>
        PriorComponentTickState;
};
