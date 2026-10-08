#include "Combat/OGDiagnosticHumanoid.h"
#include "Combat/OGDiagnosticCombatComponent.h"
#include "World/OGStartingRegionPresentation.h"
#include "Animation/AnimInstance.h"
#include "Animation/OGDiagnosticAnimationPresentation.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

AOGDiagnosticHumanoid::AOGDiagnosticHumanoid()
{
    PrimaryActorTick.bCanEverTick = true;
    State = CreateDefaultSubobject<UOGDiagnosticEnemyStateComponent>(TEXT("CombatState"));
    AnimationPresentation = CreateDefaultSubobject<UOGDiagnosticAnimationPresentation>(TEXT("AnimationPresentation"));
    GetCapsuleComponent()->InitCapsuleSize(42.0f, 96.0f);
    GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
    GetMesh()->SetRelativeLocation(FVector(0, 0, -96));
    GetMesh()->SetRelativeRotation(FRotator(0, -90, 0));
    GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    static ConstructorHelpers::FObjectFinder<USkeletalMesh> Rig(TEXT("/Engine/Tutorial/SubEditors/TutorialAssets/Character/TutorialTPP.TutorialTPP"));
    static ConstructorHelpers::FClassFinder<UAnimInstance> Locomotion(TEXT("/Engine/Tutorial/SubEditors/TutorialAssets/Character/TutorialTPP_AnimBlueprint"));
    if (Rig.Succeeded()) GetMesh()->SetSkeletalMesh(Rig.Object);
    if (Locomotion.Succeeded()) GetMesh()->SetAnimInstanceClass(Locomotion.Class);
    GetCharacterMovement()->MaxWalkSpeed = 260.0f;
    GetCharacterMovement()->bRunPhysicsWithNoController = true;
    State->OnDefeated.AddDynamic(this, &AOGDiagnosticHumanoid::HandleDefeat);
}

void AOGDiagnosticHumanoid::Initialize(UOGDiagnosticCombatComponent& InDriver,
    const FOGCombatUnitState& Unit, bool bTurnStation)
{
    Driver = &InDriver; bStation = bTurnStation; State->Initialize(Unit);
    if (ACharacter* Player = Cast<ACharacter>(InDriver.GetOwner()))
        if (USkeletalMeshComponent* Body = Player->GetMesh())
            if (Body->GetSkeletalMeshAsset())
            { GetMesh()->SetSkeletalMesh(Body->GetSkeletalMeshAsset()); GetMesh()->SetAnimInstanceClass(Body->GetAnimClass()); }
    AnimationPresentation->BindMesh(GetMesh());
    SetBodyTint(bStation ? FLinearColor(0.15f, 0.65f, 0.8f) : FLinearColor(0.7f, 0.22f, 0.2f));
}

void AOGDiagnosticHumanoid::SetBodyTint(FLinearColor Color)
{
    if (!BodyMaterial && GetMesh()->GetMaterial(0)) BodyMaterial = GetMesh()->CreateDynamicMaterialInstance(0);
    if (BodyMaterial)
    {
        BodyMaterial->SetVectorParameterValue(TEXT("BodyColor"), Color);
        BodyMaterial->SetVectorParameterValue(TEXT("BaseColor"), Color);
    }
}

bool AOGDiagnosticHumanoid::HasLineOfSight(AActor* Other) const
{
    if (!Other || !GetWorld()) return false;
    FHitResult Hit;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(DiagnosticStrike), false, this);
    Query.AddIgnoredActor(Other);
    return !GetWorld()->LineTraceSingleByChannel(Hit, GetActorLocation() + FVector(0,0,45),
        Other->GetActorLocation() + FVector(0,0,45), ECC_Visibility, Query);
}

bool AOGDiagnosticHumanoid::CanBeTargeted_Implementation(AActor* Requester) const
{ return Driver.IsValid() && Driver->IsActive() && !Driver->IsInTurn() && !bStation && State->GetSnapshot().IsAlive(); }
FVector AOGDiagnosticHumanoid::GetTargetPoint_Implementation(AActor* Requester) const
{ return GetActorLocation() + FVector(0,0,65); }
bool AOGDiagnosticHumanoid::CanInteract_Implementation(AActor* Interactor) const
{ return Interactor && bStation && Driver.IsValid() && Driver->IsActive() && !Driver->IsInTurn() && FVector::DistSquared(GetActorLocation(), Interactor->GetActorLocation()) < FMath::Square(450.0f); }
FText AOGDiagnosticHumanoid::GetInteractionPrompt_Implementation(AActor* Interactor) const
{ return FText::FromString(TEXT("Begin turn sparring")); }
void AOGDiagnosticHumanoid::Interact_Implementation(AActor* Interactor)
{
    if (!CanInteract_Implementation(Interactor)) return;
    FString Error;
    if (!Driver->BeginTurn(*this, Error)) Driver->SetStatus(Error);
}

void AOGDiagnosticHumanoid::ShowAttackPose(double Seconds)
{
    PoseEndsAt = GetWorld()->GetTimeSeconds() + Seconds;
    AnimationPresentation->PresentTimedAction(TEXT("EnemyAttack"), Seconds);
}

void AOGDiagnosticHumanoid::HandleDefeat()
{
    if (bDefeatPresented) return;
    bDefeatPresented = true;
    GetCharacterMovement()->StopMovementImmediately();
    ConsumeMovementInputVector();
    bTelegraph = false;
    State->GetExposure().Clear();
    GetCharacterMovement()->DisableMovement();
    GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    // Temporary humanoid death presentation uses physical articulated body when
    // the engine diagnostic rig has physics; otherwise freeze a visible corpse.
    if (GetMesh()->GetPhysicsAsset())
    {
        // Ragdoll owns the visible skeletal body. A procedural child proxy
        // must not continue applying a second death pose to that moving body.
        AnimationPresentation->UnbindMesh();
        GetMesh()->SetCollisionProfileName(TEXT("Ragdoll"));
        // Corpses collide with the course, not with the moving player capsule
        // or camera. Repeated pawn depenetration must not launch a dead body.
        GetMesh()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
        GetMesh()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
        GetMesh()->SetGenerateOverlapEvents(false);
        GetMesh()->SetSimulatePhysics(true);
        GetMesh()->SetAllPhysicsLinearVelocity(FVector::ZeroVector);
        GetMesh()->SetAllPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
        GetMesh()->SetLinearDamping(1.0f);
        GetMesh()->SetAngularDamping(2.0f);
        GetWorldTimerManager().SetTimer(
            DefeatFreezeTimer,
            this,
            &AOGDiagnosticHumanoid::FreezeDefeatedRagdoll,
            0.75f,
            false);
    }
    else
    {
        AnimationPresentation->SetPresentedState(TEXT("Death"));
        GetMesh()->bPauseAnims = true;
    }
    SetBodyTint(FLinearColor(0.18f, 0.18f, 0.18f));
}

void AOGDiagnosticHumanoid::FreezeDefeatedRagdoll()
{
    if (!bDefeatPresented || State->GetSnapshot().IsAlive() || !GetMesh() ||
        !GetMesh()->IsSimulatingPhysics())
    {
        return;
    }

    // Preserve the settled ragdoll pose but remove every source of later
    // wandering: gravity, contact impulses and residual rigid-body velocity.
    GetMesh()->SetAllPhysicsLinearVelocity(FVector::ZeroVector);
    GetMesh()->SetAllPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
    GetMesh()->SetEnableGravity(false);
    GetMesh()->SetCollisionResponseToAllChannels(ECR_Ignore);
    GetMesh()->PutAllRigidBodiesToSleep();
}

void AOGDiagnosticHumanoid::Tick(float Delta)
{
    Super::Tick(Delta);
    if (!Driver.IsValid() || !State->GetSnapshot().IsAlive() || bStation) return;
    const double Now = GetWorld()->GetTimeSeconds();
    if (Now >= PoseEndsAt && !bTelegraph) SetBodyTint(FLinearColor(0.7f, 0.22f, 0.2f));
    if (Driver->IsSuspended()) { GetCharacterMovement()->StopMovementImmediately(); bTelegraph = false; return; }
    AActor* Player = Driver->GetOwner();
    FVector Direction = Player->GetActorLocation() - GetActorLocation(); Direction.Z = 0;
    const double Distance = Direction.Size();
    if (Distance > 1800.0 || !HasLineOfSight(Player))
    { GetCharacterMovement()->StopMovementImmediately(); bTelegraph = false; return; }
    Direction.Normalize();
    if (!bTelegraph) SetActorRotation(Direction.Rotation());
    if (bTelegraph)
    {
        if (Now >= AttackAt)
        {
            bTelegraph = false; NextStrikeAt = Now + 1.3;
            SetBodyTint(FLinearColor(0.7f, 0.22f, 0.2f)); ShowAttackPose();
            if (Distance <= 240.0 && FVector::DotProduct(GetActorForwardVector(), Direction) >= 0.5 && HasLineOfSight(Player))
            { FString Error; if (!Driver->ReceiveStrike(State->GetSnapshot(), Error)) Driver->SetStatus(Error); }
        }
        return;
    }
    if (Distance > 180.0) AddMovementInput(Direction, 1.0f);
    else if (Now >= NextStrikeAt)
    {
        GetCharacterMovement()->StopMovementImmediately();
        bTelegraph = true; AttackAt = Now + 0.75;
        AnimationPresentation->PresentTimedAction(TEXT("EnemyTelegraph"), 0.75f);
        SetBodyTint(FLinearColor(1.0f, 0.7f, 0.05f));
        Driver->SetStatus(TEXT("Enemy strike telegraph: dodge now."));
    }
}
