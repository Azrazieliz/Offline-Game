#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "World/OGFoundationWaterVolume.h"
#include "World/OGFoundationIntegrationWorld.h"
#include "World/OGStartingRegionPresentation.h"
#include "World/OGTraversalFramework.h"
#include "Components/BrushComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/EngineBaseTypes.h"
#include "GameFramework/WorldSettings.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOGIntegrationGeometryAndPhysicalWater,
    "OfflineGame.Foundation.Integration.GeometryAndPhysicalWater",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOGIntegrationGeometryAndPhysicalWater::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues InitValues;
    InitValues.AllowAudioPlayback(false).CreatePhysicsScene(true)
        .CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, true, NAME_None, nullptr, true,
        ERHIFeatureLevel::SM5, &InitValues, false);
    // InitWorld creates a physics scene, but does not initialize actors or start
    // gameplay. Complete the normal engine lifecycle in this GameMode-less world.
    World->InitializeActorsForPlay(FURL());
    World->GetWorldSettings()->NotifyBeginPlay();
    TestTrue(TEXT("Isolated world has begun gameplay"), World->HasBegunPlay());
    auto* Site = World->SpawnActor<AOGFoundationIntegrationWorld>();
    TestTrue(TEXT("Deep legal XY remains valid"),
        Site->IsInsidePlayableBoundary_Implementation(FVector(-3000.0f, 3000.0f, -100000.0f)));
    TestFalse(TEXT("Out-of-range XY is rejected at any height"),
        Site->IsInsidePlayableBoundary_Implementation(FVector(3700.0f, 0.0f, 100000.0f)));
    TestFalse(TEXT("The shaft cannot become a recovery anchor"),
        Site->IsInsideStableGroundRegion_Implementation(FVector(-3000.0f, 3000.0f, -100000.0f)));

    const FTransform WaterTransform(FRotator::ZeroRotator, FVector(0.0f, 0.0f, -500.0f));
    auto* Water = World->SpawnActorDeferred<AOGFoundationWaterVolume>(
        AOGFoundationWaterVolume::StaticClass(), WaterTransform, nullptr, nullptr,
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    Water->Configure(FVector(500.0f));
    UGameplayStatics::FinishSpawningActor(Water, WaterTransform);
    bool bRegisteredByEngine = false;
    for (auto Volume = World->GetNonDefaultPhysicsVolumeIterator(); Volume; ++Volume)
        bRegisteredByEngine |= Volume->Get() == Water;
    TestTrue(TEXT("Engine lifecycle registers the physical water volume"), bRegisteredByEngine);
    TestTrue(TEXT("Water has completed initialization and BeginPlay"),
        Water->IsActorInitialized() && Water->HasActorBegunPlay());
    TestTrue(TEXT("Native brush bounds encompass the configured volume"),
        Water->GetBrushComponent()->Bounds.GetBox().IsInside(FVector(0.0f, 0.0f, -200.0f)));
    TestTrue(TEXT("Native volume encompasses the character entry point"),
        Water->EncompassesPoint(FVector(0.0f, 0.0f, -200.0f)));
    FHitResult SurfaceHit;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(FoundationWaterImmersion), true);
    TestTrue(TEXT("Stock immersion has a queryable brush collision surface"),
        Water->GetBrushComponent()->LineTraceComponent(SurfaceHit,
            FVector(0.0f, 0.0f, 100.0f), FVector(0.0f, 0.0f, -1100.0f), Query));
    TestTrue(TEXT("The physical brush surface equals the declared water surface"),
        FMath::IsNearlyEqual(SurfaceHit.ImpactPoint.Z, Water->GetSurfaceZ(), 1.0f));

    FActorSpawnParameters Spawn;
    Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Character = World->SpawnActor<AOGWorldPrototypeCharacter>(
        AOGWorldPrototypeCharacter::StaticClass(), FVector(-700.0f, 0.0f, 100.0f),
        FRotator::ZeroRotator, Spawn);
    TestTrue(TEXT("Character has completed initialization and BeginPlay"),
        Character->IsActorInitialized() && Character->HasActorBegunPlay());
    Character->GetTraversalCapabilities()->GrantCapability(FOGTraversalCapabilityIds::Swim);
    Character->GetCapsuleComponent()->SetShouldUpdatePhysicsVolume(true);
    Character->GetCharacterMovement()->SetUpdatedComponent(Character->GetCapsuleComponent());
    Character->SetActorLocation(FVector(0.0f, 0.0f, -200.0f));
    Character->GetCapsuleComponent()->UpdateOverlaps();
    Character->GetCapsuleComponent()->UpdatePhysicsVolume(true);
    TestTrue(TEXT("Native capsule overlap contains the water brush"),
        Character->GetCapsuleComponent()->IsOverlappingComponent(Water->GetBrushComponent()));
    TestTrue(TEXT("Swimming is supported by the movement component"),
        Character->GetCharacterMovement()->NavAgentProps.bCanSwim);
    TestTrue(TEXT("Entering water selects the actual physics volume"),
        Character->GetPhysicsVolume() == Water);
    TestTrue(TEXT("Engine physics-volume entry selects swimming without a station"),
        Character->GetCharacterMovement()->MovementMode == MOVE_Swimming);
    Character->SetActorLocation(FVector(-700.0f, 0.0f, 100.0f));
    Character->GetCapsuleComponent()->UpdateOverlaps();
    Character->GetCapsuleComponent()->UpdatePhysicsVolume(true);
    TestTrue(TEXT("Leaving the physical water volume stops swimming"),
        Character->GetCharacterMovement()->MovementMode != MOVE_Swimming);
    World->DestroyWorld(true);
    return true;
}

#endif
