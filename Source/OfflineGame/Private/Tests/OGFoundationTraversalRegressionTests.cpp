#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "World/OGStartingRegionPresentation.h"
#include "World/OGTraversalFramework.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Components/InputComponent.h"

namespace
{
struct FFoundationTestWorld
{
    UWorld* World = nullptr;
    FFoundationTestWorld()
    {
        UWorld::InitializationValues InitValues;
        InitValues.AllowAudioPlayback(false).CreatePhysicsScene(true)
            .CreateNavigation(false).CreateAISystem(false);
        World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true,
            ERHIFeatureLevel::SM5, &InitValues, false);
        World->InitializeActorsForPlay(FURL());
    }
    ~FFoundationTestWorld() { World->DestroyWorld(false); }
    AOGWorldPrototypeCharacter* Character()
    {
        FActorSpawnParameters Parameters;
        Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        return World->SpawnActor<AOGWorldPrototypeCharacter>(
            AOGWorldPrototypeCharacter::StaticClass(), FVector(0.0f, 0.0f, 100.0f),
            FRotator::ZeroRotator, Parameters);
    }
    AActor* Box(const FVector& At, const FVector& HalfExtent)
    {
        AActor* Actor = World->SpawnActor<AActor>();
        UBoxComponent* Shape = NewObject<UBoxComponent>(Actor);
        Actor->AddInstanceComponent(Shape);
        Actor->SetRootComponent(Shape);
        Shape->SetBoxExtent(HalfExtent);
        Shape->SetCollisionProfileName(TEXT("BlockAllDynamic"));
        Shape->RegisterComponent();
        Actor->SetActorLocation(At);
        return Actor;
    }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOGTeleportRejectsOccupiedLanding,
    "OfflineGame.Foundation.Traversal.TeleportRejectsOccupiedLanding",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOGTeleportRejectsOccupiedLanding::RunTest(const FString& Parameters)
{
    FFoundationTestWorld TestWorld;
    AOGWorldPrototypeCharacter* Character = TestWorld.Character();
    if (!TestNotNull(TEXT("Character spawned"), Character)) { return false; }
    Character->GetTraversalCapabilities()->GrantCapability(FOGTraversalCapabilityIds::Teleport);
    AActor* Blocker = TestWorld.World->SpawnActor<AActor>();
    UBoxComponent* Box = NewObject<UBoxComponent>(Blocker);
    Blocker->AddInstanceComponent(Box);
    Blocker->SetRootComponent(Box);
    Box->SetBoxExtent(FVector(150.0f));
    Box->SetCollisionProfileName(TEXT("BlockAllDynamic"));
    Box->RegisterComponent();
    const FVector Destination(1000.0f, 0.0f, 150.0f);
    Blocker->SetActorLocation(Destination);
    const FVector Before = Character->GetActorLocation();
    TestFalse(TEXT("An occupied destination is rejected"),
        Character->TryTeleportWithinCurrentMap(Destination));
    TestTrue(TEXT("Rejected teleport leaves the pawn where it was"),
        Character->GetActorLocation().Equals(Before));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOGTeleportEndsPreviousTraversal,
    "OfflineGame.Foundation.Traversal.TeleportEndsPreviousTraversal",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOGTeleportEndsPreviousTraversal::RunTest(const FString& Parameters)
{
    FFoundationTestWorld TestWorld;
    AOGWorldPrototypeCharacter* Character = TestWorld.Character();
    if (!TestNotNull(TEXT("Character spawned"), Character)) { return false; }
    auto* Capabilities = Character->GetTraversalCapabilities();
    Capabilities->GrantCapability(FOGTraversalCapabilityIds::Teleport);
    Capabilities->GrantCapability(FOGTraversalCapabilityIds::Flight);
    TestTrue(TEXT("Flight starts through canonical API"), Character->BeginFlight());
    Character->GetCharacterMovement()->Velocity = FVector(500.0f, 0.0f, -100.0f);
    TestTrue(TEXT("Clear destination accepted"),
        Character->TryTeleportWithinCurrentMap(FVector(1000.0f, 0.0f, 100.0f)));
    TestTrue(TEXT("Teleport does not retain the departure velocity"),
        Character->GetCharacterMovement()->Velocity.IsNearlyZero());
    TestEqual(TEXT("Landing reconciles the previous traversal mode"),
        Capabilities->GetTraversalMode(), EOGTraversalMode::Ground);
    TestTrue(TEXT("Landing movement follows physical floor/fall"),
        Character->GetCharacterMovement()->MovementMode == MOVE_Falling ||
        Character->GetCharacterMovement()->MovementMode == MOVE_Walking);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOGRealWaterHonorsCapability,
    "OfflineGame.Foundation.Traversal.RealWaterHonorsCapability",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOGRealWaterHonorsCapability::RunTest(const FString& Parameters)
{
    FFoundationTestWorld TestWorld;
    AOGWorldPrototypeCharacter* Character = TestWorld.Character();
    if (!TestNotNull(TEXT("Character spawned"), Character)) { return false; }
    Character->GetTraversalCapabilities()->RevokeCapability(FOGTraversalCapabilityIds::Swim);
    Character->GetCharacterMovement()->SetMovementMode(MOVE_Swimming);
    TestTrue(TEXT("Automatic physics water entry cannot grant a missing ability"),
        Character->GetTraversalCapabilities()->GetTraversalMode() != EOGTraversalMode::Swimming);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOGClimbLossAndDrop,
    "OfflineGame.Foundation.Traversal.ClimbLossAndDrop",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOGClimbLossAndDrop::RunTest(const FString& Parameters)
{
    FFoundationTestWorld TestWorld;
    auto* Character = TestWorld.Character();
    auto* Wall = TestWorld.World->SpawnActor<AOGFoundationClimbableWall>(
        FVector(0.0f, 0.0f, 160.0f), FRotator::ZeroRotator);
    Character->GetTraversalCapabilities()->GrantCapability(FOGTraversalCapabilityIds::Climb);
    Character->SetActorLocation(FVector(100.0f, 0.0f, 96.0f));
    Character->SetActorRotation(FRotator(0.0f, 180.0f, 0.0f));
    TestTrue(TEXT("Physical wall probe starts climbing"), Character->TryBeginClimb());
    Character->SetActorLocation(FVector(70.0f, 600.0f, 96.0f));
    Character->Tick(0.016f);
    TestEqual(TEXT("Moving laterally beyond the surface ends climbing"),
        Character->GetTraversalCapabilities()->GetTraversalMode(), EOGTraversalMode::Ground);
    TestTrue(TEXT("Surface loss restores falling physics"),
        Character->GetCharacterMovement()->MovementMode == MOVE_Falling);
    Character->SetActorLocation(FVector(100.0f, 0.0f, 96.0f));
    Character->SetActorRotation(FRotator(0.0f, 180.0f, 0.0f));
    TestTrue(TEXT("The same wall can be climbed again"), Character->TryBeginClimb());
    Character->TriggerTraversalDownPulse();
    TestEqual(TEXT("Ordinary traversal-down drops from the wall"),
        Character->GetTraversalCapabilities()->GetTraversalMode(), EOGTraversalMode::Ground);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOGClimbLedgeAndBlockedTop,
    "OfflineGame.Foundation.Traversal.ClimbLedgeAndBlockedTop",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOGClimbLedgeAndBlockedTop::RunTest(const FString& Parameters)
{
    FFoundationTestWorld TestWorld;
    auto* Character = TestWorld.Character();
    TestWorld.World->SpawnActor<AOGFoundationClimbableWall>(
        FVector(0.0f, 0.0f, 160.0f), FRotator::ZeroRotator);
    Character->GetTraversalCapabilities()->GrantCapability(FOGTraversalCapabilityIds::Climb);
    Character->SetActorLocation(FVector(100.0f, 0.0f, 96.0f));
    Character->SetActorRotation(FRotator(0.0f, 180.0f, 0.0f));
    TestTrue(TEXT("Wall entry accepted"), Character->TryBeginClimb());
    Character->SetActorLocation(FVector(68.5f, 0.0f, 300.0f));
    Character->GetCharacterMovement()->Velocity = FVector(0.0f, 0.0f, 100.0f);
    Character->Tick(0.016f);
    TestEqual(TEXT("A clear top becomes a grounded ledge exit"),
        Character->GetTraversalCapabilities()->GetTraversalMode(), EOGTraversalMode::Ground);
    TestTrue(TEXT("Capsule is placed entirely above the top"), Character->GetActorLocation().Z >= 415.0f);
    Character->SetActorLocation(FVector(100.0f, 0.0f, 96.0f));
    Character->SetActorRotation(FRotator(0.0f, 180.0f, 0.0f));
    TestTrue(TEXT("Wall entry accepted before roof test"), Character->TryBeginClimb());
    TestWorld.Box(FVector(0.0f, 0.0f, 430.0f), FVector(200.0f, 200.0f, 20.0f));
    Character->SetActorLocation(FVector(68.5f, 0.0f, 300.0f));
    Character->GetCharacterMovement()->Velocity = FVector(0.0f, 0.0f, 100.0f);
    Character->Tick(0.016f);
    TestEqual(TEXT("A roof prevents the ledge transition"),
        Character->GetTraversalCapabilities()->GetTraversalMode(), EOGTraversalMode::Climbing);
    TestTrue(TEXT("Blocked mantle never places the capsule through the roof"),
        Character->GetActorLocation().Z < 410.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOGContextInteractionObstruction,
    "OfflineGame.Foundation.Traversal.ContextInteractionObstruction",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOGContextInteractionObstruction::RunTest(const FString& Parameters)
{
    FFoundationTestWorld TestWorld;
    auto* Character = TestWorld.Character();
    auto* Marker = TestWorld.World->SpawnActor<AOGFoundationInteractableMarker>(
        FVector(300.0f, 0.0f, 60.0f), FRotator::ZeroRotator);
    TestTrue(TEXT("Visible reachable actor offers context interaction"),
        Character->GetContextInteractableForHud() == Marker);
    TestWorld.Box(FVector(150.0f, 0.0f, 100.0f), FVector(20.0f, 100.0f, 200.0f));
    TestNull(TEXT("Intervening wall blocks context interaction"),
        Character->GetContextInteractableForHud());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOGCarrierGravityAndBlockedDismount,
    "OfflineGame.Foundation.Traversal.CarrierGravityAndBlockedDismount",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOGCarrierGravityAndBlockedDismount::RunTest(const FString& Parameters)
{
    FFoundationTestWorld TestWorld;
    auto* Falling = TestWorld.World->SpawnActor<AOGFoundationTraversalCarrier>(
        FVector(1500.0f, 0.0f, 1000.0f), FRotator::ZeroRotator);
    Falling->Tick(0.2f);
    TestTrue(TEXT("An unsupported terrestrial carrier falls"), Falling->GetActorLocation().Z < 995.0f);
    TestWorld.Box(FVector(0.0f, 0.0f, -50.0f), FVector(1000.0f, 1000.0f, 50.0f));
    auto* Carrier = TestWorld.World->SpawnActor<AOGFoundationTraversalCarrier>(
        FVector(0.0f, 0.0f, 30.0f), FRotator::ZeroRotator);
    auto* Character = TestWorld.Character();
    Character->GetTraversalCapabilities()->GrantCapability(FOGTraversalCapabilityIds::Mount);
    TestTrue(TEXT("Canonical board path is used"), Character->TryBoardCarrier(Carrier));
    TestWorld.Box(FVector(200.0f, 0.0f, 200.0f), FVector(50.0f, 400.0f, 200.0f));
    TestWorld.Box(FVector(-200.0f, 0.0f, 200.0f), FVector(50.0f, 400.0f, 200.0f));
    TestWorld.Box(FVector(0.0f, 200.0f, 200.0f), FVector(400.0f, 50.0f, 200.0f));
    TestWorld.Box(FVector(0.0f, -200.0f, 200.0f), FVector(400.0f, 50.0f, 200.0f));
    Character->DismountCarrier();
    TestTrue(TEXT("No-clearance exit keeps the rider safely boarded"),
        Character->GetAttachParentActor() == Carrier);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOGBoundaryRecoveryClearsAttachment,
    "OfflineGame.Foundation.Traversal.BoundaryRecoveryClearsAttachment",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOGBoundaryRecoveryClearsAttachment::RunTest(const FString& Parameters)
{
    FFoundationTestWorld TestWorld;
    TestWorld.World->SpawnActor<AOGStartingRegionGenerator>();
    auto* Character = TestWorld.Character();
    Character->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    Character->Tick(0.0f); // The canonical map caches a stable grounded anchor.
    auto* Carrier = TestWorld.World->SpawnActor<AOGFoundationTraversalCarrier>(
        FVector(0.0f, 0.0f, 30.0f), FRotator::ZeroRotator);
    Character->GetTraversalCapabilities()->GrantCapability(FOGTraversalCapabilityIds::Mount);
    TestTrue(TEXT("Board before crossing declared boundary"), Character->TryBoardCarrier(Carrier));
    Character->SetActorLocation(FVector(100000.0f, 0.0f, 100.0f));
    Character->Tick(0.0f);
    TestNull(TEXT("Boundary recovery removes obsolete carrier attachment"), Character->GetAttachParentActor());
    TestEqual(TEXT("Boundary recovery reconciles logical traversal"),
        Character->GetTraversalCapabilities()->GetTraversalMode(), EOGTraversalMode::Ground);
    TestTrue(TEXT("Recovery remains inside the declared XY region"),
        FMath::Abs(Character->GetActorLocation().X) < 1000.0f);
    return true;
}

#endif
