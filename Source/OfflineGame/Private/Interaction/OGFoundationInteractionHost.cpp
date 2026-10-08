#include "Interaction/OGFoundationInteractionHost.h"
#include "Combat/OGDiagnosticCombatComponent.h"
#include "Combat/OGDiagnosticHumanoid.h"
#include "Runtime/OGGameCoreSubsystem.h"
#include "World/OGStartingRegionPresentation.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Misc/CoreDelegates.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"

namespace
{
    const FName InteractionClip(TEXT("FoundationInteraction.Optional"));
    bool IsEntry(FName Id)
    {
        return Id == TEXT("world") || Id == TEXT("ruler") || Id == TEXT("roster") ||
            Id == TEXT("npc") || Id == TEXT("event") || Id == TEXT("post_combat");
    }
}

UOGFoundationInteractionHost::UOGFoundationInteractionHost()
{
    PrimaryComponentTick.bCanEverTick = true;
}
UOGFoundationInteractionHost::~UOGFoundationInteractionHost() = default;

bool UOGFoundationInteractionHost::Initialize()
{
    if (!GetOwner() || !GetWorld() || !GetWorld()->GetGameInstance()) return false;
    UOGGameCoreSubsystem* Current = GetWorld()->GetGameInstance()->GetSubsystem<UOGGameCoreSubsystem>();
    if (Core.Get() != Current)
    {
        Cancel(TEXT("core_rebind")); Runtime.Reset(); BoundStore = nullptr;
        if (Core.IsValid()) Core->OnCanonicalRuntimeReleasing.Remove(ReleaseHandle);
        ReleaseHandle.Reset(); Core = Current;
        if (Current) ReleaseHandle = Current->OnCanonicalRuntimeReleasing.AddUObject(
            this, &UOGFoundationInteractionHost::HandleCanonicalRuntimeReleasing);
    }
    Combat = GetOwner()->FindComponentByClass<UOGDiagnosticCombatComponent>();
    if (!DeactivateHandle.IsValid()) DeactivateHandle = FCoreDelegates::ApplicationWillDeactivateDelegate.AddUObject(
        this, &UOGFoundationInteractionHost::HandleApplicationDeactivated);
    if (!BackgroundHandle.IsValid()) BackgroundHandle = FCoreDelegates::ApplicationWillEnterBackgroundDelegate.AddUObject(
        this, &UOGFoundationInteractionHost::HandleApplicationDeactivated);
    return EnsureRuntime();
}

bool UOGFoundationInteractionHost::EnsureRuntime()
{
    if (!Core.IsValid() || !Combat.IsValid() || !Core->IsCoreReady() ||
        !Core->GetWorldStore() || Core->GetCanonicalWorldTick() < 0)
    { Status = TEXT("Canonical world, clock and protagonist context are not ready."); return false; }
    if (!Runtime)
    {
        BoundStore = Core->GetWorldStore();
        Runtime = MakeUnique<FOGSystemicInteractionRuntime>(*BoundStore, Combat->GetRulerId(), Core->GetOptionalPackageHost());
        Runtime->SetPresentationBridge(this);
    }
    // The release delegate clears the runtime before a Store can be destroyed.
    if (BoundStore != Core->GetWorldStore())
    { Status = TEXT("Canonical runtime changed; reinitialize the interaction host."); return false; }
    if (!Runtime->IsActive())
    {
        FString HostError;
        if (!Runtime->SetOptionalPackageHost(Core->GetOptionalPackageHost(), HostError))
        { Status = HostError; return false; }
    }
    LastTick = Core->GetCanonicalWorldTick();
    return true;
}

bool UOGFoundationInteractionHost::IsActive() const
{ return Runtime && Runtime->IsActive(); }

FString UOGFoundationInteractionHost::GetStatus() const
{
    if (!IsActive()) return Status;
    return FString::Printf(TEXT("%s | %s | %d participants | %.2fx | tick %lld"), *Status,
        Runtime->IsAwaitingAction() ? TEXT("Choose next action") : *Runtime->GetCurrentNode().ToString(),
        Runtime->GetParticipants().Num(), Runtime->GetPacing(), LastTick);
}

void UOGFoundationInteractionHost::HandleApplicationDeactivated()
{ Cancel(TEXT("application_deactivated")); }

void UOGFoundationInteractionHost::HandleCanonicalRuntimeReleasing()
{
    Cancel(TEXT("canonical_world_releasing"));
    Runtime.Reset(); BoundStore = nullptr; SelectedParticipants.Reset();
    Status = TEXT("Canonical world released; interaction restored ordinary gameplay.");
}

void UOGFoundationInteractionHost::SetSelectedParticipants(const TArray<FOGInteractionParticipant>& Selection)
{
    if (IsActive()) { Status = TEXT("Exit the current interaction before changing participant bindings."); return; }
    SelectedParticipants = Selection;
    Status = FString::Printf(TEXT("%d selected participant bindings."), SelectedParticipants.Num());
}

AActor* UOGFoundationInteractionHost::FindBoundActor(const FOGEntityId& Entity) const
{
    const TWeakObjectPtr<AActor>* Actor = BoundActors.Find(Entity);
    return Actor ? Actor->Get() : nullptr;
}

void UOGFoundationInteractionHost::BuildDiagnosticSelection(bool bAll)
{
    if (!Combat.IsValid()) return;
    TArray<FOGInteractionParticipant> Candidates;
    FOGEntityId Controlled; FOGContentId Identity; FOGCharacterManifestationRecord Copy;
    bool bCopy = false; FString Error;
    Combat->TryGetControlledContext(Controlled, Identity, bCopy, Copy, Error);
    for (const FOGCombatUnitState& Unit : Combat->GetWorldParticipantsForInteraction())
    {
        if (Unit.UnitEntityId == Combat->GetRulerId() || !Unit.UnitEntityId.IsValid()) continue;
        FOGInteractionParticipant Binding;
        Binding.Role = TEXT("participant"); Binding.Character.EntityId = Unit.UnitEntityId;
        Binding.Character.OwnerEntityId = Combat->GetRulerId();
        bool bFoundCopy = false; FOGCharacterManifestationRecord Manifestation;
        if (!BoundStore->TryReadCharacterManifestation(Unit.UnitEntityId, bFoundCopy, Manifestation, Error)) continue;
        if (bFoundCopy) Binding.Character.ManifestationId = Manifestation.ManifestationId;
        else Binding.Character.OwnerEntityId = Unit.UnitEntityId;
        if (Unit.UnitEntityId == Controlled) Binding.Actor = GetOwner();
        else for (TWeakObjectPtr<AActor> Actor : Combat->GetScenarioActors())
            if (AOGDiagnosticHumanoid* Humanoid = Cast<AOGDiagnosticHumanoid>(Actor.Get()))
                if (Humanoid->GetCombatState()->GetSnapshot().UnitEntityId == Unit.UnitEntityId)
                { Binding.Actor = Humanoid; break; }
        if (ACharacter* Character = Cast<ACharacter>(Binding.Actor.Get()))
            if (Character->GetMesh() && Character->GetMesh()->GetSkeletalMeshAsset() &&
                Character->GetMesh()->GetSkeletalMeshAsset()->GetSkeleton())
                Binding.RigFamilyId = FOGContentId(TEXT("rig:") + Character->GetMesh()->GetSkeletalMeshAsset()->GetSkeleton()->GetPathName().ToLower());
        Candidates.Add(MoveTemp(Binding));
    }
    if (bAll) SelectedParticipants = MoveTemp(Candidates);
    else if (!Candidates.IsEmpty())
    {
        SelectionIndex %= Candidates.Num(); SelectedParticipants = {Candidates[SelectionIndex]};
        SelectionIndex = (SelectionIndex + 1) % Candidates.Num();
    }
    else SelectedParticipants.Reset();
    Status = FString::Printf(TEXT("Selected %d canonical participants; unembodied bindings remain logical."), SelectedParticipants.Num());
}

bool UOGFoundationInteractionHost::BeginContext(FName Context,
    const TArray<FOGInteractionParticipant>& Participants, const FOGEntityId& ContextEvent)
{
    if (!Initialize() || IsActive()) return false;
    AActor* Protagonist = Combat->GetProtagonistWorldActor();
    if (!Protagonist)
    { Status = TEXT("The canonical protagonist body must be bound before embodied interaction entry."); return false; }
    FOGInteractionEntry Entry; Entry.Context = Context; Entry.ContextEventId = ContextEvent;
    Entry.World = GetWorld(); Entry.PreferredLocation = Protagonist->GetActorLocation();
    if (APawn* Pawn = Cast<APawn>(GetOwner())) Entry.ReturnController = Cast<APlayerController>(Pawn->GetController());
    if (const AOGWorldPrototypeCharacter* Player = Cast<AOGWorldPrototypeCharacter>(GetOwner()))
        Entry.bPrivacyPresentation = Player->GetSfwPresentation();
    bool bPresence = false; FOGWorldPresenceRecord Presence; FString Error;
    if (!BoundStore->TryReadWorldPresence(Combat->GetRulerId(), bPresence, Presence, Error))
    { Status = Error; return false; }
    if (bPresence) Entry.LocationId = Presence.LocationId;
    FOGInteractionParticipant Player;
    Player.Character.EntityId = Combat->GetRulerId(); Player.Character.OwnerEntityId = Combat->GetRulerId();
    Player.Role = TEXT("player"); Player.Actor = Protagonist;
    if (ACharacter* Character = Cast<ACharacter>(Protagonist))
        if (Character->GetMesh() && Character->GetMesh()->GetSkeletalMeshAsset() && Character->GetMesh()->GetSkeletalMeshAsset()->GetSkeleton())
            Player.RigFamilyId = FOGContentId(TEXT("rig:") + Character->GetMesh()->GetSkeletalMeshAsset()->GetSkeleton()->GetPathName().ToLower());
    Entry.Participants.Add(Player);
    const TArray<FOGInteractionParticipant>& Other = Participants.IsEmpty() ? SelectedParticipants : Participants;
    for (const auto& Binding : Other)
        if (Binding.Character.EntityId != Combat->GetRulerId()) Entry.Participants.Add(Binding);
    return EnterAuthored(Entry, FOGSystemicInteractionRuntime::MakeNeutralDiagnosticGraph());
}

bool UOGFoundationInteractionHost::EnterAuthored(const FOGInteractionEntry& Entry,
    const FOGInteractionActionGraph& Graph)
{
    if (!Initialize() || IsActive()) return false;
    // Never silently replace a companion actor with the protagonist identity.
    for (const auto& Binding : Entry.Participants)
        if (Binding.Role == TEXT("player") && Entry.World.IsValid() &&
            Binding.Actor.Get() != Combat->GetProtagonistWorldActor())
        { Status = TEXT("Player-side actor must be the bound canonical protagonist body."); return false; }
    // Reveal ordinary World presentation before native entry captures visibility.
    // Otherwise a Ruler-hidden participant would be saved as permanently hidden,
    // and Privacy toggles/exit could restore that suspension inside the scene.
    // Failure and every native exit route restore the previous presentation mode.
    if (Entry.World.IsValid() && Entry.World->GetGameInstance())
    {
        PresentationMode = Entry.World->GetGameInstance()->GetSubsystem<UOGPresentationModeSubsystem>();
        if (PresentationMode.IsValid())
        {
            SavedPresentationMode = PresentationMode->GetMode();
            bOwnsPresentationMode = SavedPresentationMode != EOGPresentationMode::World;
            if (bOwnsPresentationMode) PresentationMode->SetMode(EOGPresentationMode::World);
        }
    }
    BoundActors.Reset(); MeshRestores.Reset(); ActiveGraph = Graph;
    for (const auto& Binding : Entry.Participants) BoundActors.Add(Binding.Character.EntityId, Binding.Actor);
    bPrivacy = Entry.bPrivacyPresentation;
    if (const AOGWorldPrototypeCharacter* Player = Cast<AOGWorldPrototypeCharacter>(GetOwner()))
        bLastProfilePrivacy = Player->GetSfwPresentation();
    FString Error;
    if (!Runtime->Enter(Entry, Graph, LastTick, Error))
    { RestoreGameplay(); Status = Error; return false; }
    Status = FString::Printf(TEXT("%s: %d canonical participants. Neutral actions; world time continues."),
        *Entry.Context.ToString(), Entry.Participants.Num());
    return true;
}

TArray<FOGFoundationInteractionRow> UOGFoundationInteractionHost::GetRows() const
{
    TArray<FOGFoundationInteractionRow> Rows;
    if (!IsActive())
    {
        const bool bReady = Combat.IsValid() && Combat->IsActive() && Combat->GetProtagonistWorldActor() &&
            Core.IsValid() && Core->IsCoreReady();
        Rows.Add({TEXT("world"), TEXT("World interaction"), bReady});
        Rows.Add({TEXT("ruler"), TEXT("Ruler interaction"), bReady});
        Rows.Add({TEXT("roster"), TEXT("Roster interaction"), bReady});
        Rows.Add({TEXT("npc"), TEXT("NPC context interaction"), bReady && !SelectedParticipants.IsEmpty()});
        const bool bEvent = bReady && Combat->GetResolvedEncounterEventId().IsValid();
        Rows.Add({TEXT("event"), TEXT("Event context interaction"), bEvent});
        Rows.Add({TEXT("post_combat"), TEXT("Post-combat interaction"), bEvent});
        Rows.Add({TEXT("select_next"), TEXT("Select next canonical participant"), bReady});
        Rows.Add({TEXT("select_all"), TEXT("Select available diagnostic group"), bReady});
    }
    else
    {
        Rows.Add({TEXT("align"), TEXT("Choose: align"), Runtime->IsAwaitingAction()});
        Rows.Add({TEXT("handoff"), TEXT("Choose: handoff"), Runtime->IsAwaitingAction()});
        Rows.Add({TEXT("sequence"), TEXT("Queue handoff then align"), true});
        Rows.Add({TEXT("pacing"), FString::Printf(TEXT("Pacing %.2fx: change"), Runtime->GetPacing()), true});
        Rows.Add({TEXT("camera"), TEXT("Orbit camera"), true});
        Rows.Add({TEXT("restage"), TEXT("Find nearby staging"), true});
        Rows.Add({TEXT("privacy"), bPrivacy ? TEXT("Presentation: masked") : TEXT("Presentation: visible"), true});
        Rows.Add({TEXT("exit"), TEXT("Return to ordinary gameplay"), true});
    }
    return Rows;
}

bool UOGFoundationInteractionHost::ActivateRow(FName Id)
{
    if (!Initialize()) return false;
    const auto Rows = GetRows();
    const auto* Row = Rows.FindByPredicate([Id](const auto& Value) { return Value.Id == Id; });
    if (!Row || !Row->bEnabled) { Status = TEXT("This interaction control is unavailable in the current state."); return false; }
    if (IsEntry(Id))
        return BeginContext(Id, {}, (Id == TEXT("event") || Id == TEXT("post_combat")) ?
            Combat->GetResolvedEncounterEventId() : FOGEntityId());
    if (Id == TEXT("select_next") || Id == TEXT("select_all"))
    { BuildDiagnosticSelection(Id == TEXT("select_all")); return true; }
    if (Id == TEXT("exit")) { Cancel(); return true; }
    FString Error; bool bResult = false;
    if (Id == TEXT("align") || Id == TEXT("handoff")) bResult = Runtime->SelectAction(Id, Error);
    else if (Id == TEXT("sequence"))
        bResult = Runtime->SetSequence({FName(TEXT("handoff")), FName(TEXT("align"))}, Error);
    else if (Id == TEXT("pacing"))
    {
        const double Rate = Runtime->GetPacing();
        bResult = Runtime->SetPacing(Rate == 0 ? 0.5 : Rate < 1 ? 1.0 : Rate < 2 ? 2.0 : 0.0, Error);
        UpdateAnimationPacing();
    }
    else if (Id == TEXT("camera"))
    {
        const FVector Center = Combat->GetProtagonistWorldActor() ?
            Combat->GetProtagonistWorldActor()->GetActorLocation() : GetOwner()->GetActorLocation();
        CameraAngle += PI / 4;
        const FVector Camera = Center + FVector(FMath::Cos(CameraAngle) * 450, FMath::Sin(CameraAngle) * 450, 180);
        bResult = Runtime->SetCamera(FTransform((Center - Camera).Rotation(), Camera), Error);
    }
    else if (Id == TEXT("restage"))
        bResult = Runtime->Restage(GetOwner()->GetActorLocation() + GetOwner()->GetActorRightVector() * 150, Error);
    else if (Id == TEXT("privacy"))
    { bPrivacy = !bPrivacy; Runtime->SetPrivacyPresentation(bPrivacy); bResult = true; }
    Status = bResult ? FString::Printf(TEXT("%s accepted; canonical tick %lld."), *Id.ToString(), LastTick) : Error;
    return bResult;
}

void UOGFoundationInteractionHost::Cancel(FName Reason)
{
    if (Runtime && Runtime->IsActive())
    {
        FString Error;
        const int64 Tick = Core.IsValid() && Core->GetCanonicalWorldTick() >= 0 ? Core->GetCanonicalWorldTick() : LastTick;
        const bool bSaved = Runtime->Exit(Reason, Tick, Error);
        Status = bSaved ? TEXT("Returned to ordinary gameplay.") : Error;
    }
    RestoreGameplay();
}

void UOGFoundationInteractionHost::TickComponent(float Delta, ELevelTick Tick,
    FActorComponentTickFunction* Function)
{
    Super::TickComponent(Delta, Tick, Function);
    if (!IsActive()) return;
    if (!EnsureRuntime()) { Cancel(TEXT("core_unavailable")); return; }
    if (const AOGWorldPrototypeCharacter* Player = Cast<AOGWorldPrototypeCharacter>(GetOwner()))
        if (Player->GetSfwPresentation() != bLastProfilePrivacy)
        {
            bLastProfilePrivacy = Player->GetSfwPresentation();
            Runtime->SetPrivacyPresentation(bLastProfilePrivacy);
        }
    FString Error;
    // Only action progress uses pacing. The independent Core ticker remains the
    // sole canonical time authority, including when selected pacing is zero.
    if (!Runtime->Advance(Delta, LastTick, Error))
    { Cancel(TEXT("runtime_error")); Status = Error; }
    UpdateAnimationPacing();
}

void UOGFoundationInteractionHost::ApplyCharacter(const FOGInteractionParticipantState& State)
{
    ACharacter* Actor = Cast<ACharacter>(FindBoundActor(State.Binding.Character.EntityId));
    if (Actor && Actor->GetMesh() && !MeshRestores.Contains(State.Binding.Character.EntityId))
    {
        FMeshRestore Saved; Saved.Mesh = Actor->GetMesh();
        Saved.TickPolicy = static_cast<uint8>(Saved.Mesh->VisibilityBasedAnimTickOption);
        Saved.bUpdateRateOptimization = Saved.Mesh->bEnableUpdateRateOptimizations;
        Saved.Animation = Actor->FindComponentByClass<UOGDiagnosticAnimationPresentation>();
        if (Saved.Animation.IsValid())
            if (const auto* Previous = Saved.Animation->Clips.Find(InteractionClip))
            {
                Saved.bHadClip = true; Saved.PreviousClip = *Previous;
                Saved.PreviousSequence = TStrongObjectPtr<UAnimSequence>(Previous->Sequence.Get());
            }
        MeshRestores.Add(State.Binding.Character.EntityId, Saved);
        // Embodied staging takes ownership from ordinary diagnostic locomotion/actions.
        if (Saved.Animation.IsValid()) Saved.Animation->ResetPresentation();
    }
    // Character refresh marks an action boundary: optional clips from the
    // completed action must not leak into a subsequent unbound action.
    if (FMeshRestore* Saved = MeshRestores.Find(State.Binding.Character.EntityId);
        Saved && Saved->bOwnsClip && Saved->Animation.IsValid())
    { Saved->Animation->SetPresentedState(NAME_None); Saved->bOwnsClip = false; }
    OnCharacterPresentation.Broadcast(State);
}

void UOGFoundationInteractionHost::SetSignificance(const FOGEntityId& Entity, bool bAction, bool bCamera)
{
    if (FMeshRestore* Saved = MeshRestores.Find(Entity); Saved && Saved->Mesh.IsValid())
    {
        Saved->Mesh->bEnableUpdateRateOptimizations = !(bAction || bCamera);
        Saved->Mesh->VisibilityBasedAnimTickOption = bAction || bCamera ?
            EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones : EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
    }
}

void UOGFoundationInteractionHost::ApplyConstraint(const FOGInteractionConstraint& Constraint,
    const TArray<FOGInteractionParticipantState>& Participants)
{ OnConstraintPresentation.Broadcast(Constraint, Participants); }

void UOGFoundationInteractionHost::ApplyAnimation(const FOGEntityId& Entity, UObject* Asset)
{
    FMeshRestore* Saved = MeshRestores.Find(Entity);
    UAnimSequence* Sequence = Cast<UAnimSequence>(Asset);
    if (!Saved || !Saved->Mesh.IsValid() || !Saved->Animation.IsValid() || !Sequence ||
        !Saved->Mesh->GetSkeletalMeshAsset() ||
        Sequence->GetSkeleton() != Saved->Mesh->GetSkeletalMeshAsset()->GetSkeleton()) return;
    FOGDiagnosticAnimationClip Clip; Clip.Sequence = Sequence; Clip.bLoop = true;
    Saved->Animation->Clips.Add(InteractionClip, Clip); Saved->bOwnsClip = true;
    Saved->Animation->SetPresentedState(NAME_None);
    Saved->Animation->SetPresentedState(InteractionClip);
    UpdateAnimationPacing();
}

void UOGFoundationInteractionHost::ApplyReaction(const FOGEntityId& Entity, UObject* Expression, UObject* Reaction)
{ OnReactionPresentation.Broadcast(Entity, Expression, Reaction); }

void UOGFoundationInteractionHost::SetPrivacy(bool Privacy)
{
    bPrivacy = Privacy;
    // The runtime owns masking and restores each actor's original visibility.
    // No canonical state command is issued by this presentation callback.
}

void UOGFoundationInteractionHost::UpdateAnimationPacing()
{
    if (!Runtime) return;
    for (auto& Pair : MeshRestores)
        if (Pair.Value.bOwnsClip && Pair.Value.Mesh.IsValid() && Pair.Value.Animation.IsValid())
            if (const auto* Clip = Pair.Value.Animation->Clips.Find(InteractionClip))
                if (auto* Instance = Pair.Value.Mesh->GetSingleNodeInstance();
                    Instance && Instance->GetCurrentAsset() == Clip->Sequence)
                    Pair.Value.Mesh->SetPlayRate(static_cast<float>(Runtime->GetPacing()));
}

void UOGFoundationInteractionHost::RestoreGameplay()
{
    for (auto& Pair : MeshRestores)
    {
        FMeshRestore& Saved = Pair.Value;
        if (Saved.Animation.IsValid())
        {
            if (Saved.bOwnsClip) Saved.Animation->SetPresentedState(NAME_None);
            if (Saved.bHadClip) Saved.Animation->Clips.Add(InteractionClip, Saved.PreviousClip);
            else Saved.Animation->Clips.Remove(InteractionClip);
        }
        if (Saved.Mesh.IsValid())
        {
            Saved.Mesh->VisibilityBasedAnimTickOption = static_cast<EVisibilityBasedAnimTickOption>(Saved.TickPolicy);
            Saved.Mesh->bEnableUpdateRateOptimizations = Saved.bUpdateRateOptimization;
        }
    }
    MeshRestores.Reset(); BoundActors.Reset();
    if (bOwnsPresentationMode)
    {
        bOwnsPresentationMode = false;
        if (PresentationMode.IsValid()) PresentationMode->SetMode(SavedPresentationMode);
    }
    PresentationMode.Reset();
}

void UOGFoundationInteractionHost::EndPlay(const EEndPlayReason::Type Reason)
{
    Cancel(TEXT("host_end_play")); Runtime.Reset(); BoundStore = nullptr;
    if (Core.IsValid()) Core->OnCanonicalRuntimeReleasing.Remove(ReleaseHandle);
    ReleaseHandle.Reset(); Core.Reset();
    FCoreDelegates::ApplicationWillDeactivateDelegate.Remove(DeactivateHandle);
    FCoreDelegates::ApplicationWillEnterBackgroundDelegate.Remove(BackgroundHandle);
    DeactivateHandle.Reset(); BackgroundHandle.Reset();
    Super::EndPlay(Reason);
}