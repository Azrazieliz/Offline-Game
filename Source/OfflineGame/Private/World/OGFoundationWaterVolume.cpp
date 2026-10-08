#include "World/OGFoundationWaterVolume.h"
#include "Components/BoxComponent.h"
#include "Components/BrushComponent.h"
#include "PhysicsEngine/BodySetup.h"

AOGFoundationWaterVolume::AOGFoundationWaterVolume()
{
    PrimaryActorTick.bCanEverTick = false;
    bWaterVolume = true;
    bPhysicsOnContact = false;
    Priority = 10;
    FluidFriction = 0.3f;
    WaterBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("WaterBounds"));
    WaterBounds->SetMobility(EComponentMobility::Movable);
    WaterBounds->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    WaterBounds->SetCollisionObjectType(ECC_WorldDynamic);
    WaterBounds->SetCollisionResponseToAllChannels(ECR_Ignore);
    WaterBounds->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    WaterBounds->SetGenerateOverlapEvents(true);
    // APhysicsVolume is a brush actor. Keep its inherited brush as the actor root so
    // engine physics-volume discovery sees this as a real volume; WaterBounds remains
    // a colocated helper for deterministic point/surface queries.
    if (UBrushComponent* VolumeBrushComponent = GetBrushComponent())
    {
        RootComponent = VolumeBrushComponent;
        VolumeBrushComponent->SetMobility(EComponentMobility::Movable);
        WaterBounds->SetupAttachment(VolumeBrushComponent);
        WaterBounds->SetRelativeTransform(FTransform::Identity);
    }
    else
    {
        RootComponent = WaterBounds;
    }
    Tags.Add(TEXT("OG.WorldPresentation"));
}

void AOGFoundationWaterVolume::Configure(const FVector& HalfExtent)
{
    const FVector SafeExtent = HalfExtent.GetAbs().ComponentMax(FVector(1.0f));
    WaterBounds->SetBoxExtent(SafeExtent, true);
    if (UBrushComponent* VolumeBrushComponent = GetBrushComponent())
    {
        VolumeBrushComponent->BrushBodySetup = NewObject<UBodySetup>(VolumeBrushComponent);
        VolumeBrushComponent->BrushBodySetup->CollisionTraceFlag = CTF_UseSimpleAsComplex;
        FKBoxElem Box;
        Box.X = SafeExtent.X * 2.0f;
        Box.Y = SafeExtent.Y * 2.0f;
        Box.Z = SafeExtent.Z * 2.0f;
        VolumeBrushComponent->BrushBodySetup->AggGeom.BoxElems.Add(Box);
        VolumeBrushComponent->BrushBodySetup->CreatePhysicsMeshes();
        VolumeBrushComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        VolumeBrushComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
        VolumeBrushComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
        VolumeBrushComponent->SetGenerateOverlapEvents(true);
        // A new collision body does not refresh cached component bounds. Native
        // overlap/volume discovery needs the bounds of the completed physical box.
        VolumeBrushComponent->UpdateBounds();
        VolumeBrushComponent->RecreatePhysicsState();
        VolumeBrushComponent->UpdateOverlaps();
    }
}

bool AOGFoundationWaterVolume::ContainsPoint(
    FVector Point, float SphereRadius, float* OutDistanceToPoint) const
{
    const FVector Local = WaterBounds->GetComponentTransform()
        .InverseTransformPosition(Point).GetAbs();
    const FVector Extent = WaterBounds->GetUnscaledBoxExtent();
    const FVector Outside = (Local - Extent).ComponentMax(FVector::ZeroVector);
    const float Distance = Outside.Size();
    if (OutDistanceToPoint) { *OutDistanceToPoint = Distance; }
    return Distance <= SphereRadius;
}

bool AOGFoundationWaterVolume::IsOverlapInVolume(const USceneComponent& TestComponent) const
{
    return bPhysicsOnContact || ContainsPoint(TestComponent.GetComponentLocation());
}

float AOGFoundationWaterVolume::GetSurfaceZ() const
{
    return WaterBounds->GetComponentLocation().Z + WaterBounds->GetScaledBoxExtent().Z;
}
