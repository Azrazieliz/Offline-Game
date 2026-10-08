#include "World/OGFoundationInteractionFixture.h"
#include "World/OGStartingRegionPresentation.h"
#include "World/OGTraversalFramework.h"
#include "World/OGInventoryEquipmentService.h"
#include "Combat/OGWorldActionRuntime.h"
#include "Combat/OGDiagnosticCombatComponent.h"
#include "Runtime/OGFoundationCharacterDiagnostics.h"
#include "Runtime/OGGameCoreSubsystem.h"
#include "Persistence/OGWorldStore.h"
#include "Components/StaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/OverlapResult.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Components/SceneComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "UObject/ConstructorHelpers.h"
#include "HAL/IConsoleManager.h"

namespace
{
IOGWorldStore* FixtureStore(const AActor* Actor)
{
    UGameInstance* Instance = Actor->GetGameInstance();
    UOGGameCoreSubsystem* Core = Instance
        ? Instance->GetSubsystem<UOGGameCoreSubsystem>() : nullptr;
    return Core ? Core->GetWorldStore() : nullptr;
}
}

AOGFoundationInteractionFixture::AOGFoundationInteractionFixture()
{
    PrimaryActorTick.bCanEverTick = false;
    Tags.Add(TEXT("OG.WorldPresentation"));
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
    Mesh->SetupAttachment(RootComponent);
    Mesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(
        TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (Cube.Succeeded()) { Mesh->SetStaticMesh(Cube.Object); }
    Text = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
    Text->SetupAttachment(RootComponent);
    Text->SetWorldSize(22.0f);
    Text->SetHorizontalAlignment(EHTA_Center);
}

void AOGFoundationInteractionFixture::Configure(uint32 Ordinal,
    EOGFoundationInteractionKind InKind, const FString& InLabel,
    const FVector& Size)
{
    Kind = InKind;
    Label = InLabel;
    OriginalSize = Size;
    StateId = FOGEntityId(FGuid(0x4f474449u, 0x41474e4fu, 0x53544943u, 100u + Ordinal));
    Mesh->SetRelativeScale3D(Size / 100.0f);
    Text->SetAbsolute(false, false, true);
    Text->SetRelativeLocation(FVector(0.0f, 0.0f, Size.Z * 0.5f + 50.0f));
    int64 Revision = 0;
    if (Kind != EOGFoundationInteractionKind::FlightStation && Kind != EOGFoundationInteractionKind::Npc)
    {
        ReadState(bActive, Revision);
    }
    if (auto* Parent = LoadObject<UMaterialInterface>(nullptr,
        TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
    {
        auto* Tint = UMaterialInstanceDynamic::Create(Parent, this);
        const FLinearColor Color = Kind == EOGFoundationInteractionKind::Door ? FLinearColor(.15f,.23f,.28f) :
            Kind == EOGFoundationInteractionKind::BreakableBarrier ? FLinearColor(.32f,.19f,.10f) :
            Kind == EOGFoundationInteractionKind::StrongBarrier ? FLinearColor(0.2f,0.2f,0.25f) :
            Kind == EOGFoundationInteractionKind::TravelPoint ? FLinearColor(0.1f,0.6f,0.45f) :
            FLinearColor(.44f,.36f,.21f);
        Tint->SetVectorParameterValue(TEXT("Color"), Color);
        Mesh->SetMaterial(0, Tint);
    }
    RefreshProjection();
}

void AOGFoundationInteractionFixture::ResetTemporaryFlightProjection()
{
    if (Kind == EOGFoundationInteractionKind::FlightStation)
    {
        bActive = false;
        RefreshProjection();
    }
}

void AOGFoundationInteractionFixture::SetTravelPeer(
    AOGFoundationInteractionFixture* Peer)
{
    TravelPeer = Peer;
}

bool AOGFoundationInteractionFixture::ReadState(bool& bOutActive,
    int64& OutRevision) const
{
    bOutActive = false;
    OutRevision = 0;
    LastError.Reset();
    IOGWorldStore* Store = FixtureStore(this);
    if (!Store) { LastError = TEXT("World store unavailable"); return false; }
    bool bFound = false;
    FName StoredKind;
    FString StateJson;
    if (!Store->TryReadEntity(StateId, bFound, StoredKind, StateJson,
        OutRevision, LastError)) { return false; }
    if (bFound && StoredKind != FName(TEXT("foundation_integration_fixture_v1")))
    {
        LastError = TEXT("Fixture entity kind mismatch");
        return false;
    }
    if (bFound && StateJson != TEXT("{\"active\":true}") &&
        StateJson != TEXT("{\"active\":false}"))
    {
        LastError = TEXT("Fixture state schema mismatch");
        return false;
    }
    // The fixture owns one explicitly versioned boolean state and no rewards.
    bOutActive = bFound && StateJson == TEXT("{\"active\":true}");
    return true;
}

bool AOGFoundationInteractionFixture::CommitState(bool bNewActive)
{
    bool bStoredActive = false;
    int64 Revision = 0;
    if (!ReadState(bStoredActive, Revision)) { return false; }
    if (!ReadCanonicalWorldTick)
    {
        LastError = TEXT("Canonical command time unavailable");
        return false;
    }
    const int64 WorldTick = ReadCanonicalWorldTick();
    if (WorldTick < 0) { LastError = TEXT("Invalid canonical command time"); return false; }
    IOGWorldStore* Store = FixtureStore(this);
    if (!Store || !Store->UpsertEntity(StateId,
        FName(TEXT("foundation_integration_fixture_v1")), WorldTick,
        bNewActive ? TEXT("{\"active\":true}") : TEXT("{\"active\":false}"),
        LastError)) { return false; }
    bActive = bNewActive;
    RefreshProjection();
    return true;
}

bool AOGFoundationInteractionFixture::IsAccessible(AActor* Interactor) const
{
    if (!Interactor || !GetWorld() ||
        FVector::DistSquared(Interactor->GetActorLocation(), GetActorLocation()) >
        FMath::Square(300.0f)) { return false; }
    FHitResult Hit;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(FoundationFixtureAccess), false,
        Interactor);
    return !GetWorld()->LineTraceSingleByChannel(Hit,
        Interactor->GetActorLocation(), GetActorLocation(), ECC_Visibility, Query)
        || Hit.GetActor() == this;
}

bool AOGFoundationInteractionFixture::CanInteract_Implementation(
    AActor* Interactor) const
{
    return Cast<AOGWorldPrototypeCharacter>(Interactor) && IsAccessible(Interactor)
        && Kind != EOGFoundationInteractionKind::BreakableBarrier
        && Kind != EOGFoundationInteractionKind::StrongBarrier
        && !(bActive && (Kind == EOGFoundationInteractionKind::Pickup ||
            Kind == EOGFoundationInteractionKind::Chest));
}

FText AOGFoundationInteractionFixture::GetInteractionPrompt_Implementation(
    AActor* Interactor) const
{
    FString Prompt = Label;
    if (!LastError.IsEmpty()) { Prompt += TEXT(" — ") + LastError; }
    else if (Kind == EOGFoundationInteractionKind::TravelPoint)
    {
        Prompt += bActive ? TEXT(" — travel to paired unlocked point")
            : TEXT(" — unlock here");
    }
    else if (Kind == EOGFoundationInteractionKind::FlightStation)
    {
        Prompt += bActive ? TEXT(" — land and return flight ability")
            : TEXT(" — borrow temporary flight ability");
    }
    else if (Kind == EOGFoundationInteractionKind::Npc)
    {
        Prompt += NpcDialogue.IsEmpty() ? TEXT(" — talk with resident") : TEXT(" — ") + NpcDialogue;
    }
    else { Prompt += bActive ? TEXT(" — active") : TEXT(" — interact"); }
    return FText::FromString(Prompt);
}

void AOGFoundationInteractionFixture::RefreshProjection()
{
    const bool bCollected = bActive && (Kind == EOGFoundationInteractionKind::Pickup ||
        Kind == EOGFoundationInteractionKind::BreakableBarrier);
    Mesh->SetVisibility(!bCollected);
    Mesh->SetCollisionEnabled(bCollected ? ECollisionEnabled::NoCollision
        : ECollisionEnabled::QueryAndPhysics);
    if (Kind == EOGFoundationInteractionKind::Door)
    {
        // Sliding door physically vacates the frame; an open door remains usable.
        Mesh->SetRelativeLocation(bActive ? FVector(0.0f, 0.0f, OriginalSize.Z)
            : FVector::ZeroVector);
    }
    FString Caption = Label + (bActive ? TEXT(" [active]") : TEXT(""));
    if (Kind == EOGFoundationInteractionKind::Npc && !NpcDialogue.IsEmpty())
    {
        Caption = Label + TEXT(": ") + NpcDialogue;
    }
    Text->SetText(FText::FromString(Caption));
    const auto* Labels=IConsoleManager::Get().FindConsoleVariable(TEXT("og.Debug.WorldLabels"));
    Text->SetVisibility(Labels && Labels->GetInt()!=0 && !bCollected);
    Text->SetTextRenderColor(FColor::White);
    Text->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f));
}

bool AOGFoundationInteractionFixture::CanBeTargeted_Implementation(AActor* Requester) const
{
    return Requester && IsAccessible(Requester) && !bActive &&
        (Kind == EOGFoundationInteractionKind::BreakableBarrier ||
        Kind == EOGFoundationInteractionKind::StrongBarrier);
}

FVector AOGFoundationInteractionFixture::GetTargetPoint_Implementation(AActor* Requester) const
{
    return GetActorLocation();
}

bool AOGFoundationInteractionFixture::ReadResolvedWorldCombatSnapshot(FOGCombatUnitState& Out, FString& Error) const
{
    Out = FOGCombatUnitState(); Error.Reset();
    if (Kind != EOGFoundationInteractionKind::BreakableBarrier && Kind != EOGFoundationInteractionKind::StrongBarrier)
    { Error = TEXT("Fixture has no authored destructibility definition."); return false; }
    bool Destroyed = false; int64 Created = 0;
    if (!ReadState(Destroyed, Created)) { Error = LastError; return false; }
    Out.UnitEntityId = StateId;
    Out.IdentityId = FOGContentId(Kind == EOGFoundationInteractionKind::BreakableBarrier ?
        TEXT("foundation:integration_fragile_barrier") : TEXT("foundation:integration_solid_barrier"));
    Out.TeamIndex = INDEX_NONE;
    Out.Presence = Destroyed ? EOGCombatPresence::Defeated : EOGCombatPresence::Active;
    // One-hit fragile material is authored fixture content. Solid material
    // resolves zero damage through the same combat math, with no float HP copy.
    Out.Stats.MaxHp = FOGLargeNumber::FromInt64(1);
    Out.CurrentHp = Destroyed ? FOGLargeNumber() : Out.Stats.MaxHp;
    Out.Stats.DamageTakenMultiplierBps = Kind == EOGFoundationInteractionKind::StrongBarrier ? 0 : 10000;
    return true;
}

bool AOGFoundationInteractionFixture::CanReceiveResolvedWorldDamage(AActor* SourceActor) const
{
    return CanBeTargeted_Implementation(SourceActor);
}

bool AOGFoundationInteractionFixture::ApplyResolvedWorldDamage(const FOGLargeNumber& Damage,
    AActor* SourceActor, FString& Error)
{
    Error.Reset();
    if (Damage.GetSign() < 0 || !CanReceiveResolvedWorldDamage(SourceActor))
    { Error = TEXT("Destructible target is unavailable or outside attack range/line of sight."); return false; }
    if (Kind == EOGFoundationInteractionKind::StrongBarrier || Damage.GetSign() == 0) return true;
    if (!CommitState(true)) { Error = LastError; return false; }
    return true;
}

float AOGFoundationInteractionFixture::TakeDamage(float DamageAmount,
    FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
    // Engine damage ingress is converted once, at its float API boundary.
    // Canonical active combat calls ApplyResolvedWorldDamage directly.
    if (!FMath::IsFinite(DamageAmount) || DamageAmount <= 0.0f) return 0.0f;
    FString Error;
    const FOGLargeNumber Damage = FOGLargeNumber::FromInt64(FMath::Max<int64>(1, FMath::CeilToInt64(DamageAmount)));
    return ApplyResolvedWorldDamage(Damage, DamageCauser, Error) &&
        Kind == EOGFoundationInteractionKind::BreakableBarrier ? DamageAmount : 0.0f;
}

void AOGFoundationInteractionFixture::Interact_Implementation(AActor* Interactor)
{
    if (!CanInteract_Implementation(Interactor)) { return; }
    auto* Character = Cast<AOGWorldPrototypeCharacter>(Interactor);
    LastError.Reset();
    if (Kind == EOGFoundationInteractionKind::Npc)
    {
        SpeakToNpc(Character);
        return;
    }
    if (Kind == EOGFoundationInteractionKind::Inventory)
    {
        ShowInventory(Character);
        return;
    }
    if (Kind == EOGFoundationInteractionKind::Door && bActive && !CanCloseDoor())
    {
        LastError = TEXT("Doorway occupied; move clear before closing");
        return;
    }
    if (Kind == EOGFoundationInteractionKind::Pickup ||
        Kind == EOGFoundationInteractionKind::Chest)
    {
        AcquireReward(Character);
        return;
    }
    if (Kind == EOGFoundationInteractionKind::FlightStation)
    {
        const bool bRequestedFlight = !bActive;
        if (!SetTemporaryFlight || !SetTemporaryFlight(Character, bRequestedFlight))
        {
            LastError = TEXT("Temporary flight grant unavailable");
            return;
        }
        // Temporary capability lifetime is session-only, unlike entity fixtures.
        bActive = bRequestedFlight;
        RefreshProjection();
        return;
    }
    if (Kind == EOGFoundationInteractionKind::TravelPoint && bActive)
    {
        bool bPeerUnlocked = false;
        int64 Revision = 0;
        if (!TravelPeer.IsValid() ||
            !TravelPeer->ReadState(bPeerUnlocked, Revision) || !bPeerUnlocked)
        {
            LastError = TEXT("Visit and unlock the other point on foot first");
            return;
        }
        FVector Destination = TravelPeer->GetActorLocation() + FVector(180.0f, 0.0f, 90.0f);
        if (!Character->TryTeleportWithinCurrentMap(Destination))
        {
            LastError = TEXT("Teleport capability or destination rejected");
        }
        return;
    }
    CommitState(Kind == EOGFoundationInteractionKind::Door ||
        Kind == EOGFoundationInteractionKind::Usable ? !bActive : true);
}

bool AOGFoundationInteractionFixture::AcquireReward(AOGWorldPrototypeCharacter* Character)
{
    auto* Party = Character->FindComponentByClass<UOGWorldPartyRuntimeComponent>();
    IOGWorldStore* Store = FixtureStore(this);
    if (!Party || !Store || !ReadCanonicalWorldTick ||
        !Party->GetSlots().IsValidIndex(Party->GetControlledSlot()))
    {
        LastError = TEXT("Canonical inventory owner/time unavailable");
        return false;
    }
    const FOGWorldPartySlot& Slot = Party->GetSlots()[Party->GetControlledSlot()];
    const FOGEntityId InventoryOwnerId = Slot.Unit.UnitEntityId;
    const int64 WorldTick = ReadCanonicalWorldTick();
    if (!Slot.bAvailable || Slot.bDefeated || !InventoryOwnerId.IsValid() || WorldTick < 0)
    {
        LastError = TEXT("Canonical inventory command context invalid");
        return false;
    }
    if (!Store->BeginTransaction(LastError)) { return false; }
    auto Rollback = [Store]()
    {
        FString Ignored;
        Store->RollbackTransaction(Ignored);
    };
    bool bStoredActive = false;
    int64 Revision = 0;
    if (!ReadState(bStoredActive, Revision)) { Rollback(); return false; }
    if (bStoredActive)
    {
        Rollback();
        bActive = true;
        RefreshProjection();
        return true;
    }
    FOGInventoryEquipmentService Inventory(*Store);
    FOGEntityId ItemId;
    const FOGContentId Definition(FString(Kind == EOGFoundationInteractionKind::Chest
        ? TEXT("foundation:integration_chest_reward")
        : TEXT("foundation:integration_pickup_sample")));
    // Disposable definitions, no Rank/Quality restriction. CreateItem validates
    // IDs and uses the same authoritative item-instance record as production.
    if (!Inventory.CreateItem(InventoryOwnerId, Definition, FOGContentId(), FOGContentId(),
            WorldTick, ItemId, LastError) ||
        !Store->UpsertEntity(StateId, FName(TEXT("foundation_integration_fixture_v1")),
            WorldTick, TEXT("{\"active\":true}"), LastError) ||
        !Store->CommitTransaction(LastError))
    {
        Rollback();
        return false;
    }
    // Projection changes only after the inventory + fixture transaction commits.
    bActive = true;
    RefreshProjection();
    return true;
}


bool AOGFoundationInteractionFixture::CanCloseDoor() const
{
    if (!GetWorld()) { return false; }
    TArray<FOverlapResult> Occupants;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(FoundationDoorClosure), false, this);
    GetWorld()->OverlapMultiByObjectType(Occupants, GetActorLocation(), GetActorQuat(),
        FCollisionObjectQueryParams(ECC_Pawn), FCollisionShape::MakeBox(OriginalSize * 0.5f), Query);
    return Occupants.IsEmpty();
}

void AOGFoundationInteractionFixture::ShowInventory(AOGWorldPrototypeCharacter* Character)
{
    auto* Party = Character ? Character->FindComponentByClass<UOGWorldPartyRuntimeComponent>() : nullptr;
    IOGWorldStore* Store = FixtureStore(this);
    if (!Party || !Store || !Party->GetSlots().IsValidIndex(Party->GetControlledSlot()))
    {
        LastError = TEXT("Canonical inventory owner unavailable");
        return;
    }
    const FOGWorldPartySlot& Slot = Party->GetSlots()[Party->GetControlledSlot()];
    TArray<FOGItemInstanceRecord> Items;
    if (!Slot.Unit.UnitEntityId.IsValid() ||
        !Store->ListItemInstancesByOwner(Slot.Unit.UnitEntityId, Items, LastError)) { return; }
    LastError = FString::Printf(TEXT("Controlled manifestation owns %d item(s)"), Items.Num());
    Text->SetText(FText::FromString(Label + TEXT(": ") + LastError));
}


bool AOGFoundationInteractionFixture::SpeakToNpc(AOGWorldPrototypeCharacter* Character)
{
    IOGWorldStore* Store = FixtureStore(this);
    UOGDiagnosticCombatComponent* Combat = Character ? Character->FindComponentByClass<UOGDiagnosticCombatComponent>() : nullptr;
    if (!Store || !Combat || !Combat->IsActive() || !ReadCanonicalWorldTick)
    { LastError = TEXT("Canonical dialogue participant/time unavailable"); return false; }
    const int64 Tick = ReadCanonicalWorldTick();
    FOGFoundationCharacterContext Context;
    FOGContentId Identity; bool HasManifestation = false; FOGCharacterManifestationRecord Manifestation;
    if (Tick < 0 || !Combat->TryGetControlledContext(Context.EntityId, Identity, HasManifestation, Manifestation, LastError)) return false;
    Context.OwnerEntityId = Combat->GetRulerId();
    if (HasManifestation) Context.ManifestationId = Manifestation.ManifestationId;
    FOGFoundationCharacterDiagnosticResult Result;
    if (!FOGFoundationCharacterDiagnostics(*Store).ExecuteDiagnosticAction(Context,
        EOGFoundationCharacterDiagnosticAction::SpeakToNpc, Tick, Result, LastError)) return false;
    CanonicalNpcEntityId = Result.NpcEntityId;
    NpcDialogue = Result.Dialogue.Text;
    // The visible resident is the same entity used by menu observation/memory,
    // promotion and local dialogue; its presence follows this physical fixture.
    bool Found = false; FOGWorldPresenceRecord VisitorPresence;
    if (!Store->TryReadWorldPresence(Context.EntityId, Found, VisitorPresence, LastError)) return false;
    if (Found && CanonicalNpcEntityId.IsValid())
    {
        FOGFoundationCharacterContext Resident; Resident.EntityId = CanonicalNpcEntityId;
        VisitorPresence.EntityId = CanonicalNpcEntityId;
        VisitorPresence.LocalPosition += GetActorLocation() - Character->GetActorLocation();
        VisitorPresence.MovementContext = FName(TEXT("Ground"));
        if (!FOGFoundationCharacterRuntime(*Store).SetPresence(Resident, VisitorPresence, Tick, LastError)) return false;
    }
    RefreshProjection();
    return true;
}
