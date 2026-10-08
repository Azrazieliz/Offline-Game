#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "World/OGWorldModeInterfaces.h"
#include "World/OGWorldStateRecords.h"
#include "OGFoundationIntegrationWorld.generated.h"

class AOGWorldPrototypeCharacter;
class AOGFoundationInteractionFixture;
class UStaticMesh;
class UStaticMeshComponent;

UCLASS(NotBlueprintable, Transient)
class OFFLINEGAME_API AOGFoundationIntegrationWorld : public AActor,
    public IOGPlayableBoundaryProvider
{
    GENERATED_BODY()
public:
    AOGFoundationIntegrationWorld();
    void Build(AOGWorldPrototypeCharacter* Character,
        TFunction<int64()> ReadCanonicalWorldTick);
    bool CancelFlight(AOGWorldPrototypeCharacter* Character) { return SetFlight(Character, false); }
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual bool IsInsidePlayableBoundary_Implementation(const FVector& Location) const override;
    virtual bool IsInsideStableGroundRegion_Implementation(const FVector& Location) const override;
private:
    UStaticMeshComponent* Box(const FVector& Center, const FVector& Size,
        const FRotator& Rotation = FRotator::ZeroRotator);
    void BuildScenery();
    void Label(const FVector& Center, const FString& Text);
    void Own(AActor* Actor);
    bool SetFlight(AOGWorldPrototypeCharacter* Character, bool bEnable);
    UPROPERTY() TObjectPtr<UStaticMesh> Cube;
    UPROPERTY() TObjectPtr<UStaticMesh> SceneryCone;
    UPROPERTY() TObjectPtr<UStaticMesh> ScenerySphere;
    UPROPERTY() TArray<TObjectPtr<AActor>> OwnedActors;
    UPROPERTY() TWeakObjectPtr<AOGWorldPrototypeCharacter> FlightBorrower;
    bool bAddedFlight = false;
    UPROPERTY() TWeakObjectPtr<AOGWorldPrototypeCharacter> TravelCapabilityBorrower;
    bool bAddedTeleport = false;
    UPROPERTY() TWeakObjectPtr<AOGFoundationInteractionFixture> FlightStation;
    FOGEntityId FlightOwnerId;
};
