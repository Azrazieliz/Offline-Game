#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PhysicsVolume.h"
#include "OGFoundationWaterVolume.generated.h"

class UBoxComponent;

// Disposable content: water is an actual Unreal physics volume, never a
// station that manually forces MOVE_Swimming. Units are centimetres.
UCLASS(NotBlueprintable, Transient)
class OFFLINEGAME_API AOGFoundationWaterVolume : public APhysicsVolume
{
    GENERATED_BODY()
public:
    AOGFoundationWaterVolume();
    void Configure(const FVector& HalfExtent);
    virtual bool IsOverlapInVolume(const USceneComponent& TestComponent) const override;
    bool ContainsPoint(FVector Point, float SphereRadius = 0.0f,
        float* OutDistanceToPoint = nullptr) const;
    float GetSurfaceZ() const;
private:
    UPROPERTY() TObjectPtr<UBoxComponent> WaterBounds;
};
