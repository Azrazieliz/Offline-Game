#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "World/OGWorldModeInterfaces.h"
#include "World/OGWorldStateRecords.h"
#include "Combat/OGResolvedWorldDamageTarget.h"
#include "OGFoundationInteractionFixture.generated.h"

class UStaticMeshComponent;
class UTextRenderComponent;
class AOGWorldPrototypeCharacter;

// Disposable fixture projections. Pickup/chest commands route to the existing
// inventory service; no parallel inventory or dialogue simulation is added.
UENUM()
enum class EOGFoundationInteractionKind : uint8
{
    Npc, Pickup, Chest, Usable, Door, Station, Inventory, FlightStation, TravelPoint,
    BreakableBarrier, StrongBarrier
};

UCLASS(NotBlueprintable, Transient)
class OFFLINEGAME_API AOGFoundationInteractionFixture
    : public AActor, public IOGWorldInteractable, public IOGWorldTargetable, public IOGResolvedWorldDamageTarget
{
    GENERATED_BODY()
public:
    AOGFoundationInteractionFixture();
    void Configure(uint32 Ordinal, EOGFoundationInteractionKind Kind,
        const FString& Label, const FVector& Size);
    void SetTravelPeer(AOGFoundationInteractionFixture* Peer);
    void ResetTemporaryFlightProjection();
    FOGEntityId GetCanonicalNpcEntityId() const { return CanonicalNpcEntityId; }
    virtual bool CanInteract_Implementation(AActor* Interactor) const override;
    virtual FText GetInteractionPrompt_Implementation(AActor* Interactor) const override;
    virtual void Interact_Implementation(AActor* Interactor) override;
    virtual bool CanBeTargeted_Implementation(AActor* Requester) const override;
    virtual FVector GetTargetPoint_Implementation(AActor* Requester) const override;
    virtual bool ReadResolvedWorldCombatSnapshot(FOGCombatUnitState& Out, FString& Error) const override;
    virtual bool CanReceiveResolvedWorldDamage(AActor* SourceActor) const override;
    virtual bool ApplyResolvedWorldDamage(const FOGLargeNumber& Damage, AActor* SourceActor, FString& Error) override;
    virtual float TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent,
        AController* EventInstigator, AActor* DamageCauser) override;

    // Bound only by the integration world. Must call the existing canonical
    // capability Grant/Remove API, once its actual signature is audited.
    TFunction<bool(AOGWorldPrototypeCharacter*, bool)> SetTemporaryFlight;
    // Integration hook must return the existing canonical simulation tick.
    // Never infer time from a rendered frame, actor age or entity revision.
    TFunction<int64()> ReadCanonicalWorldTick;
private:
    bool ReadState(bool& bOutActive, int64& OutRevision) const;
    bool CommitState(bool bActive);
    bool AcquireReward(AOGWorldPrototypeCharacter* Character);
    bool SpeakToNpc(AOGWorldPrototypeCharacter* Character);
    bool CanCloseDoor() const;
    void ShowInventory(AOGWorldPrototypeCharacter* Character);
    void RefreshProjection();
    bool IsAccessible(AActor* Interactor) const;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Mesh;
    UPROPERTY() TObjectPtr<UTextRenderComponent> Text;
    UPROPERTY() TWeakObjectPtr<AOGFoundationInteractionFixture> TravelPeer;
    FOGEntityId StateId;
    FOGEntityId CanonicalNpcEntityId;
    FString NpcDialogue;
    EOGFoundationInteractionKind Kind = EOGFoundationInteractionKind::Usable;
    FString Label;
    FVector OriginalSize = FVector(80.0f);
    bool bActive = false;
    mutable FString LastError;
};
