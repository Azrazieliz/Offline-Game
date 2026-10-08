#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Combat/OGCombatTypes.h"
#include "OGWorldActionRuntime.generated.h"

UENUM(BlueprintType)
enum class EOGActionCancelDestination : uint8
{
    Dodge,
    Jump,
    Switch,
    Skill,
    Ultimate
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGActionCancelWindow
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float OpensAtSeconds = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ClosesAtSeconds = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<EOGActionCancelDestination> Destinations;

    bool Allows(float ElapsedSeconds, EOGActionCancelDestination Destination) const;
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGWorldActionState
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName ActionId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float StartedAtSeconds = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FOGActionCancelWindow> CancelWindows;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bAllowWhileAirborne = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bAllowWhileSwimming = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bAllowWhileDiving = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bAllowWhileFlying = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bAllowWhileClimbing = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bAllowWhileMounted = true;
};

UCLASS(ClassGroup=(OfflineGame), BlueprintType, meta=(BlueprintSpawnableComponent))
class OFFLINEGAME_API UOGWorldActionRuntimeComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UOGWorldActionRuntimeComponent();

    UFUNCTION(BlueprintCallable, Category="OfflineGame|World|Action")
    void BeginAuthoredAction(const FOGWorldActionState& State);

    UFUNCTION(BlueprintCallable, Category="OfflineGame|World|Action")
    void EndAuthoredAction();

    UFUNCTION(BlueprintPure, Category="OfflineGame|World|Action")
    bool HasActiveAction() const { return bHasActiveAction; }

    UFUNCTION(BlueprintPure, Category="OfflineGame|World|Action")
    bool CanCancelTo(EOGActionCancelDestination Destination) const;

    UFUNCTION(BlueprintPure, Category="OfflineGame|World|Action")
    bool CanContinueInTraversalMode(uint8 TraversalMode) const;

private:
    UPROPERTY()
    FOGWorldActionState ActiveAction;

    bool bHasActiveAction = false;
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGWorldPartySlot
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FOGCombatUnitState Unit;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bAvailable = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bDefeated = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bQteReady = false;
};

UCLASS(ClassGroup=(OfflineGame), BlueprintType, meta=(BlueprintSpawnableComponent))
class OFFLINEGAME_API UOGWorldPartyRuntimeComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UOGWorldPartyRuntimeComponent();
    bool ApplyResolvedCharacterProjection(int32 SlotIndex, const FOGCombatUnitState& Projection, FString& Error);
    bool ApplyResolvedHp(int32 SlotIndex, const FOGEntityId& ExpectedUnitId,
        const FOGLargeNumber& NewHp, FString& OutError);
    bool AssignResolvedCompanion(int32 SlotIndex, const FOGCombatUnitState& Unit, FString& Error);
    bool RestorePresentationParty(const TArray<FOGWorldPartySlot>& SavedSlots, int32 SavedControl, FString& Error);


    UFUNCTION(BlueprintCallable, Category="OfflineGame|World|Party")
    bool ConfigureParty(const TArray<FOGCombatUnitState>& Units, FString& OutError);

    UFUNCTION(BlueprintPure, Category="OfflineGame|World|Party")
    int32 GetControlledSlot() const { return ControlledSlot; }

    UFUNCTION(BlueprintPure, Category="OfflineGame|World|Party")
    const TArray<FOGWorldPartySlot>& GetSlots() const { return Slots; }

    UFUNCTION(BlueprintCallable, Category="OfflineGame|World|Party")
    bool TrySwitchTo(int32 SlotIndex, bool bStateAllowsSwitch = true);

    UFUNCTION(BlueprintCallable, Category="OfflineGame|World|Party")
    bool TryTriggerQte(int32 SlotIndex);

    UFUNCTION(BlueprintCallable, Category="OfflineGame|World|Party")
    int32 MarkDefeatedAndResolveFallback(int32 SlotIndex);

    UFUNCTION(BlueprintCallable, Category="OfflineGame|World|Party")
    void SetQteReady(int32 SlotIndex, bool bReady);

    UFUNCTION(BlueprintImplementableEvent, Category="OfflineGame|World|Party")
    void OnControlTransferRequested(int32 PreviousSlot, int32 NewSlot);

    UFUNCTION(BlueprintImplementableEvent, Category="OfflineGame|World|Party")
    void OnQteRequested(int32 SourceSlot, int32 QteSlot);

private:
    UPROPERTY()
    TArray<FOGWorldPartySlot> Slots;

    UPROPERTY()
    int32 ControlledSlot = INDEX_NONE;
};
