#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "UObject/Interface.h"
#include "OGTraversalFramework.generated.h"

class ACharacter;
class APawn;

UENUM(BlueprintType)
enum class EOGTraversalMode : uint8
{
    Ground,
    Climbing,
    Swimming,
    Diving,
    Flying,
    Mounted,
    Vehicle
};

struct OFFLINEGAME_API FOGTraversalCapabilityIds
{
    static const FName Sprint;
    static const FName Climb;
    static const FName Swim;
    static const FName Dive;
    static const FName Flight;
    static const FName Mount;
    static const FName Vehicle;
    static const FName Teleport;
    static const FName Phase;
    static const FName Tunnel;
    static const FName DimensionalTravel;
    static const FName PressureResistance;
    static const FName UnderwaterAdaptation;
    static const FName VacuumSurvival;
    static const FName HeatResistance;
    static const FName ColdResistance;
    static const FName RadiationResistance;
    static const FName DimensionalStability;
    static const FName Authority;
    static const FName Coordinates;
    static const FName SafeTransport;
    static const FName PhysicalForce;
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGTraversalRequirement
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="OfflineGame|Traversal")
    TArray<FName> RequiredAll;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="OfflineGame|Traversal")
    TArray<FName> RequiredAny;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="OfflineGame|Traversal")
    bool bAllowAlternateRoute = true;
};

UCLASS(ClassGroup=(OfflineGame), BlueprintType, meta=(BlueprintSpawnableComponent))
class OFFLINEGAME_API UOGTraversalCapabilityComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UOGTraversalCapabilityComponent();

    UFUNCTION(BlueprintPure, Category="OfflineGame|Traversal")
    EOGTraversalMode GetTraversalMode() const { return CurrentMode; }

    UFUNCTION(BlueprintCallable, Category="OfflineGame|Traversal")
    void SetTraversalMode(EOGTraversalMode NewMode);

    UFUNCTION(BlueprintPure, Category="OfflineGame|Traversal")
    bool HasCapability(FName CapabilityId) const;

    UFUNCTION(BlueprintCallable, Category="OfflineGame|Traversal")
    void GrantCapability(FName CapabilityId);

    UFUNCTION(BlueprintCallable, Category="OfflineGame|Traversal")
    void RevokeCapability(FName CapabilityId);

    UFUNCTION(BlueprintPure, Category="OfflineGame|Traversal")
    bool SatisfiesRequirement(const FOGTraversalRequirement& Requirement) const;

    UFUNCTION(BlueprintPure, Category="OfflineGame|Traversal")
    static FName RequiredCapabilityForMode(EOGTraversalMode Mode);

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="OfflineGame|Traversal")
    TArray<FName> Capabilities;

private:
    UPROPERTY(VisibleAnywhere, Category="OfflineGame|Traversal")
    EOGTraversalMode CurrentMode = EOGTraversalMode::Ground;
};

UINTERFACE(BlueprintType)
class OFFLINEGAME_API UOGTraversalGate : public UInterface
{
    GENERATED_BODY()
};

class OFFLINEGAME_API IOGTraversalGate
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="OfflineGame|Traversal")
    FOGTraversalRequirement GetTraversalRequirement(AActor* Traveler) const;
};

UINTERFACE(BlueprintType)
class OFFLINEGAME_API UOGClimbableSurface : public UInterface
{
    GENERATED_BODY()
};

class OFFLINEGAME_API IOGClimbableSurface
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="OfflineGame|Traversal")
    bool CanClimb(ACharacter* Climber, FVector HitLocation, FVector SurfaceNormal) const;
};

UINTERFACE(BlueprintType)
class OFFLINEGAME_API UOGTraversalCarrier : public UInterface
{
    GENERATED_BODY()
};

class OFFLINEGAME_API IOGTraversalCarrier
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="OfflineGame|Traversal")
    bool CanBoard(ACharacter* Rider) const;

    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="OfflineGame|Traversal")
    EOGTraversalMode GetCarrierTraversalMode() const;

    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="OfflineGame|Traversal")
    FTransform GetRiderTransform(ACharacter* Rider) const;

    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="OfflineGame|Traversal")
    APawn* GetCarrierPawn(ACharacter* Rider) const;

    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="OfflineGame|Traversal")
    void AddCarrierMovementInput(ACharacter* Rider, FVector2D Input);

    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="OfflineGame|Traversal")
    void AddCarrierVerticalInput(ACharacter* Rider, float Input);

    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="OfflineGame|Traversal")
    void OnBoarded(ACharacter* Rider);

    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="OfflineGame|Traversal")
    void OnUnboarded(ACharacter* Rider);
};
