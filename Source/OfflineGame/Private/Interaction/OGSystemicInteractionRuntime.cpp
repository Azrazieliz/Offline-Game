#include "Interaction/OGSystemicInteractionRuntime.h"
#include "Persistence/OGWorldStore.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Camera/CameraActor.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "Engine/SkeletalMesh.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
    const FName PlayerRole(TEXT("player"));
    bool Fail(FString& Error, const TCHAR* Message) { Error = Message; return false; }
    void BodySize(AActor* Actor, float& Radius, float& HalfHeight)
    {
        Radius = 35.f; HalfHeight = 90.f;
        if (auto* Character = Cast<ACharacter>(Actor))
        {
            Character->GetCapsuleComponent()->GetScaledCapsuleSize(Radius, HalfHeight);
        }
        else if (Actor)
        {
            FVector Origin, Extent;
            Actor->GetActorBounds(true, Origin, Extent);
            Radius = FMath::Max(35.f, FMath::Max(Extent.X, Extent.Y));
            HalfHeight = FMath::Max(90.f, Extent.Z);
        }
    }
}

FOGSystemicInteractionRuntime::FOGSystemicInteractionRuntime(IOGWorldStore& InStore, const FOGEntityId& InRuler,
    FOGOptionalPackageHost* OptionalPackageHost)
    : Store(InStore), RulerId(InRuler), Characters(InStore), OptionalAssets(InStore, OptionalPackageHost) {}
FOGSystemicInteractionRuntime::~FOGSystemicInteractionRuntime()
{
    if (bActive) { FString Ignored; Exit(FName(TEXT("runtime_teardown")), LastWorldTick, Ignored); }
}

const FOGInteractionActionNode* FOGSystemicInteractionRuntime::FindNode(FName Id) const
{
    return Graph.Nodes.FindByPredicate([Id](const auto& Node) { return Node.NodeId == Id; });
}

bool FOGSystemicInteractionRuntime::Enter(const FOGInteractionEntry& Request,
    const FOGInteractionActionGraph& Definition, int64 Tick, FString& Error)
{
    if (bActive) return Fail(Error, TEXT("An interaction is already active."));
    if (!RulerId.IsValid() || Tick < 0 || !Definition.GraphId.IsValid() || Request.Participants.IsEmpty())
        return Fail(Error, TEXT("A canonical Ruler, clock, graph and participants are required."));
    TSet<FOGEntityId> Entities;
    TSet<AActor*> Actors;
    int32 PlayerCount = 0;
    TSet<FName> Roles;
    for (const auto& Participant : Request.Participants)
    {
        if (!Participant.Character.EntityId.IsValid() || Participant.Role.IsNone() || Entities.Contains(Participant.Character.EntityId))
            return Fail(Error, TEXT("Each participant requires a unique persistent entity and bound role."));
        Entities.Add(Participant.Character.EntityId); Roles.Add(Participant.Role);
        if (Participant.Role == PlayerRole)
        {
            ++PlayerCount;
            if (Participant.Character.EntityId != RulerId)
                return Fail(Error, TEXT("The physical player role belongs to the canonical Ruler."));
        }
        else if (Participant.Character.EntityId == RulerId)
            return Fail(Error, TEXT("The canonical Ruler must occupy the player role."));
        if (auto* Actor = Participant.Actor.Get())
        {
            if (!Request.World.IsValid() || Actors.Contains(Actor) || Actor->GetWorld() != Request.World.Get())
                return Fail(Error, TEXT("Embodied participant bindings must be distinct actors in the staging world."));
            Actors.Add(Actor);
        }
    }
    if (PlayerCount != 1) return Fail(Error, TEXT("Exactly one canonical Ruler player-side participant is required."));
    TSet<FName> NodeIds;
    for (const auto& Node : Definition.Nodes)
    {
        if (Node.NodeId.IsNone() || NodeIds.Contains(Node.NodeId) || !Node.ActionId.IsValid() ||
            !FMath::IsFinite(Node.DurationSeconds) || Node.DurationSeconds <= 0)
            return Fail(Error, TEXT("Graph nodes require unique IDs, action IDs and finite positive durations."));
        NodeIds.Add(Node.NodeId);
        for (FName Role : Node.RequiredRoles) if (!Roles.Contains(Role))
            return Fail(Error, TEXT("A graph role has no bound participant."));
        for (const auto& Constraint : Node.Constraints)
            if (!Roles.Contains(Constraint.ParticipantRole) || !Roles.Contains(Constraint.TargetRole) ||
                Constraint.TargetOffset.ContainsNaN())
                return Fail(Error, TEXT("Constraints require bound roles and finite target offsets."));
        for (const auto& Binding : Node.AnimationBindings)
            if (!Roles.Contains(Binding.Role))
                return Fail(Error, TEXT("Animation bindings must reference an existing participant role."));
    }
    if (!NodeIds.Contains(Definition.StartNode)) return Fail(Error, TEXT("Graph start node is missing."));
    for (const auto& Node : Definition.Nodes) for (FName Next : Node.NextNodes)
        if (!NodeIds.Contains(Next)) return Fail(Error, TEXT("Graph transition references a missing node."));
    LastWorldTick = Tick;
    Entry = Request; Graph = Definition; SessionId = FOGEntityId::NewId();
    CurrentNode = NAME_None; LastCompletedNode = NAME_None;
    Elapsed = 0; Pacing = 1; Sequence.Reset(); Participants.Reset(); Restore.Reset();
    for (const auto& Binding : Entry.Participants)
    {
        FOGInteractionParticipantState State; State.Binding = Binding;
        Participants.Add(MoveTemp(State));
    }
    if (!RefreshCharacters(Error)) { Participants.Reset(); return false; }
    if (!SolveStaging(Entry.PreferredLocation, FindNode(Graph.StartNode)->EnvironmentAnchorTag, Error))
    { Participants.Reset(); return false; }
    // Persist entry before touching ordinary movement/camera state.
    if (!CommitEvent(FName(TEXT("interaction.entered")), Entry.Context, Tick, nullptr, Error))
    { Participants.Reset(); return false; }
    for (auto& Participant : Participants) if (auto* Actor = Participant.Binding.Actor.Get())
    {
        FActorRestore Saved;
        Saved.Actor = Actor; Saved.Transform = Actor->GetActorTransform(); Saved.bHidden = Actor->IsHidden();
        Saved.bHasPresence = Participant.Character.bHasPresence;
        if (Saved.bHasPresence) Saved.Presence = Participant.Character.Presence;
        if (auto* Character = Cast<ACharacter>(Actor))
        {
            auto* Movement = Character->GetCharacterMovement();
            Saved.Velocity = Movement->Velocity;
            Saved.MovementMode = static_cast<uint8>(Movement->MovementMode);
            Saved.CustomMovementMode = Movement->CustomMovementMode;
            Movement->StopMovementImmediately(); Movement->DisableMovement();
        }
        Restore.Add(Saved);
        Actor->SetActorTransform(Participant.StagingTransform, false, nullptr, ETeleportType::TeleportPhysics);
    }
    if (auto* Controller = Entry.ReturnController.Get())
    {
        ReturnViewTarget = Controller->GetViewTarget(); ReturnControlRotation = Controller->GetControlRotation();
        // SetIgnore* uses a counter; only undo the suppression owned here.
        Controller->SetIgnoreMoveInput(true); Controller->SetIgnoreLookInput(true);
        bMoveSuppressed = true; bLookSuppressed = true; bInputSuppressed = true;
    }
    bActive = true;
    SetPrivacyPresentation(Entry.bPrivacyPresentation);
    if (!StartNode(Graph.StartNode, Error)) { FString Ignored; Exit(FName(TEXT("entry_failed")), Tick, Ignored); return false; }
    return true;
}

bool FOGSystemicInteractionRuntime::RefreshCharacters(FString& Error)
{
    TArray<FOGFoundationCharacterProjection> Projections;
    for (const auto& Participant : Participants)
    {
        FOGFoundationCharacterProjection Projection;
        if (!Characters.Project(Participant.Binding.Character, Projection, Error)) return false;
        Projections.Add(MoveTemp(Projection));
    }
    for (int32 Index = 0; Index < Participants.Num(); ++Index)
    {
        Participants[Index].Character = MoveTemp(Projections[Index]);
        // Entry projection is read-only until staging and entry persistence succeed.
        // StartNode refreshes after activation before presentation takes ownership.
        if (Presentation && bActive) Presentation->ApplyCharacter(Participants[Index]);
    }
    return true;
}

bool FOGSystemicInteractionRuntime::SolveStaging(const FVector& Preferred, FName AnchorTag, FString& Error)
{
    if (Preferred.ContainsNaN()) return Fail(Error, TEXT("Staging position is not finite."));
    UWorld* World = Entry.World.Get();
    TArray<FVector> Centers; Centers.Add(Preferred);
    if (World)
    {
        TArray<FVector> Anchors;
        for (TActorIterator<AActor> It(World); It; ++It)
            if (It->ActorHasTag(FName(TEXT("OGInteractionAnchor"))) &&
                (AnchorTag.IsNone() || It->ActorHasTag(AnchorTag))) Anchors.Add(It->GetActorLocation());
        Anchors.Sort([Preferred](const FVector& A, const FVector& B)
            { return FVector::DistSquared(A, Preferred) < FVector::DistSquared(B, Preferred); });
        Centers.Append(Anchors);
        // Bounded geometry search budget does not bound logical participant arity.
        for (int32 Ring = 1; Ring <= 4; ++Ring) for (int32 Sector = 0; Sector < 8; ++Sector)
        {
            const double Angle = Sector * UE_DOUBLE_PI / 4.0;
            Centers.Add(Preferred + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0) * (Ring * 350.0));
        }
    }
    FCollisionQueryParams Params(SCENE_QUERY_STAT(OGInteractionStaging), false);
    for (const auto& Participant : Participants) if (Participant.Binding.Actor.IsValid()) Params.AddIgnoredActor(Participant.Binding.Actor.Get());
    float LargestRadius = 35.f;
    for (const auto& Participant : Participants)
    {
        float Radius, Height; BodySize(Participant.Binding.Actor.Get(), Radius, Height);
        LargestRadius = FMath::Max(LargestRadius, Radius);
    }
    const double Spacing = LargestRadius * 2.0 + 35.0;
    const double CircleRadius = FMath::Max(Spacing, Participants.Num() * Spacing / (2.0 * UE_DOUBLE_PI));
    for (const FVector& Center : Centers)
    {
        TArray<FTransform> Placements;
        bool bFits = true;
        for (int32 Index = 0; Index < Participants.Num(); ++Index)
        {
            const auto& Participant = Participants[Index];
            float Radius, Height; BodySize(Participant.Binding.Actor.Get(), Radius, Height);
            const double Angle = Index * 2.0 * UE_DOUBLE_PI / Participants.Num();
            FVector Position = Center + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0) * CircleRadius;
            if (World && Participant.Binding.Actor.IsValid())
            {
                FHitResult Floor;
                if (!World->LineTraceSingleByChannel(Floor, Position + FVector(0,0,500), Position - FVector(0,0,1500), ECC_WorldStatic, Params)
                    || Floor.ImpactNormal.Z < 0.5f) { bFits = false; break; }
                Position.Z = Floor.ImpactPoint.Z + Height + 2.f;
                FCollisionObjectQueryParams Objects;
                Objects.AddObjectTypesToQuery(ECC_WorldStatic); Objects.AddObjectTypesToQuery(ECC_WorldDynamic); Objects.AddObjectTypesToQuery(ECC_Pawn);
                if (World->OverlapAnyTestByObjectType(Position, FQuat::Identity, Objects,
                    FCollisionShape::MakeCapsule(Radius, FMath::Max(Height, Radius)), Params)) { bFits = false; break; }
            }
            FRotator Facing = (Center - Position).Rotation(); Facing.Pitch = 0; Facing.Roll = 0;
            FVector Scale = Participant.Binding.Actor.IsValid() ? Participant.Binding.Actor->GetActorScale3D() : FVector::OneVector;
            Placements.Add(FTransform(Facing, Position, Scale));
        }
        if (bFits)
        {
            for (int32 Index = 0; Index < Participants.Num(); ++Index) Participants[Index].StagingTransform = Placements[Index];
            return true;
        }
    }
    return Fail(Error, TEXT("No collision-safe nearby staging solution found; ordinary gameplay is preserved."));
}

bool FOGSystemicInteractionRuntime::Restage(const FVector& Location, FString& Error)
{
    if (!bActive) return Fail(Error, TEXT("No active interaction."));
    const auto* Node = FindNode(CurrentNode);
    if (!SolveStaging(Location, Node ? Node->EnvironmentAnchorTag : NAME_None, Error)) return false;
    Entry.PreferredLocation = Location;
    for (const auto& Participant : Participants) if (auto* Actor = Participant.Binding.Actor.Get())
        Actor->SetActorTransform(Participant.StagingTransform, false, nullptr, ETeleportType::TeleportPhysics);
    if (Node && Presentation) for (const auto& Constraint : Node->Constraints) Presentation->ApplyConstraint(Constraint, Participants);
    return true;
}

bool FOGSystemicInteractionRuntime::SetSequence(const TArray<FName>& Requested, FString& Error)
{
    if (!bActive) return Fail(Error, TEXT("No active interaction."));
    FName Previous = CurrentNode.IsNone() ? LastCompletedNode : CurrentNode;
    for (FName Id : Requested)
    {
        if (!FindNode(Id)) return Fail(Error, TEXT("Requested action is not in the current authored graph."));
        if (Previous.IsNone())
        {
            if (Id != Graph.StartNode) return Fail(Error, TEXT("Sequence must begin at the graph start."));
        }
        else if (!FindNode(Previous)->NextNodes.Contains(Id))
            return Fail(Error, TEXT("Requested contextual transition is absent from the graph."));
        Previous = Id;
    }
    if (CurrentNode.IsNone() && !Requested.IsEmpty())
    {
        if (!StartNode(Requested[0], Error)) return false;
        Sequence = Requested;
        Sequence.RemoveAt(0);
    }
    else Sequence = Requested;
    return true;
}

bool FOGSystemicInteractionRuntime::SelectAction(FName Id, FString& Error)
{
    if (!bActive || !CurrentNode.IsNone()) return Fail(Error, TEXT("Finish or interrupt the current action before selecting another."));
    TArray<FName> Requested; Requested.Add(Id); return SetSequence(Requested, Error);
}

bool FOGSystemicInteractionRuntime::StartNode(FName Id, FString& Error)
{
    const auto* Node = FindNode(Id);
    if (!Node || !RefreshCharacters(Error)) return false;
    if (!SolveStaging(Entry.PreferredLocation, Node->EnvironmentAnchorTag, Error)) return false;
    CurrentNode = Id; Elapsed = 0;
    for (const auto& Participant : Participants) if (auto* Actor = Participant.Binding.Actor.Get())
        Actor->SetActorTransform(Participant.StagingTransform, false, nullptr, ETeleportType::TeleportPhysics);
    UpdatePresentation(*Node);
    return true;
}

bool FOGSystemicInteractionRuntime::SetPacing(double Rate, FString& Error)
{
    if (!bActive || !FMath::IsFinite(Rate) || Rate < 0) return Fail(Error, TEXT("Pacing must be finite and nonnegative for an active interaction."));
    Pacing = Rate;
    for (const auto& Pair : PlayedMontages)
        if (auto* Character = Cast<ACharacter>(Pair.Key.Get()))
            if (auto* Montage = Cast<UAnimMontage>(Pair.Value.Get()))
                if (auto* Anim = Character->GetMesh()->GetAnimInstance())
                    Anim->Montage_SetPlayRate(Montage, static_cast<float>(Rate));
    return true;
}

bool FOGSystemicInteractionRuntime::Advance(double Delta, int64 Tick, FString& Error)
{
    if (!bActive) return true;
    if (!FMath::IsFinite(Delta) || Delta < 0 || Tick < 0) return Fail(Error, TEXT("Invalid frame duration or canonical tick."));
    LastWorldTick = Tick;
    for (const auto& Saved : Restore)
        if (!Saved.Actor.IsValid()) return Exit(FName(TEXT("participant_unloaded")), Tick, Error);
    if (const auto* ActiveNode = FindNode(CurrentNode)) UpdateSignificance(*ActiveNode);
    if (CurrentNode.IsNone() || Pacing == 0) return true;
    const auto* Node = FindNode(CurrentNode);
    if (!Node) return Fail(Error, TEXT("Current action no longer exists."));
    const double Progress = Delta * Pacing;
    if (!FMath::IsFinite(Progress)) return Fail(Error, TEXT("Pacing duration overflow."));
    // At most one completed action per call: surplus becomes elapsed on the next
    // selected action only on later frames, avoiding burst consequence commits.
    Elapsed = FMath::Min(Node->DurationSeconds, Elapsed + Progress);
    if (Elapsed < Node->DurationSeconds) return true;
    if (!CommitEvent(FName(TEXT("interaction.action_completed")), Node->NodeId, Tick, &Node->Consequences, Error)) return false;
    LastCompletedNode = CurrentNode; CurrentNode = NAME_None; Elapsed = 0;
    if (!RefreshCharacters(Error)) return false;
    if (!Sequence.IsEmpty())
    {
        const FName Next = Sequence[0];
        if (!StartNode(Next, Error)) return false;
        Sequence.RemoveAt(0);
    }
    return true;
}

bool FOGSystemicInteractionRuntime::LoadOptional(const FOGOptionalAssetReference& Reference, UObject*& Asset, FString& Reason)
{
    Asset = nullptr;
    if (!Reference.AssetPath.IsValid()) { Reason = TEXT("Optional presentation reference absent."); return false; }
    const FString Key = Reference.PackageId.ToString() + TEXT("|") + Reference.AssetPath.ToString();
    if (const auto* Existing = SessionAssets.Find(Key); Existing && Existing->IsValid())
    { Asset = Existing->Get(); Reason.Reset(); return true; }
    if (!OptionalAssets.ActivateAndLoad(Reference, Asset, Reason)) return false;
    SessionAssets.Add(Key, Asset);
    return true;
}

void FOGSystemicInteractionRuntime::UpdatePresentation(const FOGInteractionActionNode& Node)
{
    for (const auto& Pair : PlayedMontages)
        if (auto* Character = Cast<ACharacter>(Pair.Key.Get()))
            if (auto* Montage = Cast<UAnimMontage>(Pair.Value.Get()))
                if (auto* Anim = Character->GetMesh()->GetAnimInstance()) Anim->Montage_Stop(0.1f, Montage);
    PlayedMontages.Reset();
    for (auto& Participant : Participants)
    {
        Participant.bAnimationCompatible = false; Participant.bHasOptionalPresentation = false; Participant.PresentationReason.Reset();
        const FOGInteractionAnimationBinding* Binding = nullptr;
        for (const auto& Candidate : Node.AnimationBindings)
        {
            if (Candidate.Role != Participant.Binding.Role || !Candidate.RigFamilyId.IsValid() || Candidate.RigFamilyId != Participant.Binding.RigFamilyId) continue;
            const FOGEntityId ActualManifestation = Participant.Character.bHasManifestation ?
                Participant.Character.Manifestation.ManifestationId : Participant.Binding.Character.ManifestationId;
            if (Candidate.ManifestationId.IsValid() && Candidate.ManifestationId != ActualManifestation) continue;
            if (!Candidate.VersionId.IsEmpty() && (!Participant.Character.bHasManifestation ||
                Candidate.VersionId != Participant.Character.Manifestation.ActiveVersionId)) continue;
            if (!Binding || Candidate.ManifestationId.IsValid()) Binding = &Candidate;
        }
        if (!Binding) { Participant.PresentationReason = TEXT("No compatible optional rig binding; current character state retained."); continue; }
        Participant.bAnimationCompatible = true;
        if (Entry.bPrivacyPresentation) { Participant.PresentationReason = TEXT("Privacy framing uses the same logical action and consequences."); continue; }
        UObject* Animation = nullptr;
        if (LoadOptional(Binding->Animation, Animation, Participant.PresentationReason))
        {
            if (auto* Character = Cast<ACharacter>(Participant.Binding.Actor.Get()))
            {
                auto* Mesh = Character->GetMesh();
                if (auto* Montage = Cast<UAnimMontage>(Animation))
                {
                    if (Mesh && Mesh->GetAnimInstance() && Mesh->GetSkeletalMeshAsset() && Montage->GetSkeleton() == Mesh->GetSkeletalMeshAsset()->GetSkeleton())
                    {
                        if (Mesh->GetAnimInstance()->Montage_Play(Montage, static_cast<float>(FMath::Max(0.01, Pacing))) > 0)
                        {
                            Mesh->GetAnimInstance()->Montage_SetPlayRate(Montage, static_cast<float>(Pacing));
                            PlayedMontages.Add(Participant.Binding.Actor, TWeakObjectPtr<UObject>(Montage));
                            Participant.bHasOptionalPresentation = true;
                        }
                    }
                }
                // Sequences are handed to the character's ordinary animation bridge
                // rather than replacing its AnimInstance or its current body.
            }
            if (Presentation) Presentation->ApplyAnimation(Participant.Binding.Character.EntityId, Animation);
        }
        UObject* Expression = nullptr; UObject* Reaction = nullptr; FString Ignored;
        LoadOptional(Binding->Expression, Expression, Ignored);
        LoadOptional(Binding->Reaction, Reaction, Ignored);
        if (Presentation) Presentation->ApplyReaction(Participant.Binding.Character.EntityId, Expression, Reaction);
    }
    UpdateSignificance(Node);
    if (Presentation) for (const auto& Constraint : Node.Constraints) Presentation->ApplyConstraint(Constraint, Participants);
}

void FOGSystemicInteractionRuntime::UpdateSignificance(const FOGInteractionActionNode& Node)
{
    for (auto& Participant : Participants)
    {
        Participant.bActionRelevant = Participant.Binding.Role == PlayerRole || Node.RequiredRoles.Contains(Participant.Binding.Role);
        Participant.bCameraRelevant = false;
        if (auto* Controller = Entry.ReturnController.Get()) if (auto* Actor = Participant.Binding.Actor.Get())
        {
            FVector CameraLocation; FRotator CameraRotation; Controller->GetPlayerViewPoint(CameraLocation, CameraRotation);
            const FVector Direction = (Actor->GetActorLocation() - CameraLocation).GetSafeNormal();
            Participant.bCameraRelevant = FVector::DotProduct(Direction, CameraRotation.Vector()) > 0.4f;
        }
        if (Presentation) Presentation->SetSignificance(Participant.Binding.Character.EntityId,
            Participant.bActionRelevant, Participant.bCameraRelevant);
    }
}

bool FOGSystemicInteractionRuntime::SetCamera(const FTransform& Transform, FString& Error)
{
    if (!bActive || !Entry.World.IsValid() || !Entry.ReturnController.IsValid() || Transform.ContainsNaN())
        return Fail(Error, TEXT("Camera requires an active embodied interaction and finite transform."));
    if (!CameraActor.IsValid())
    {
        auto* Camera = Entry.World->SpawnActor<ACameraActor>(Transform.GetLocation(), Transform.Rotator());
        if (!Camera) return Fail(Error, TEXT("Unable to create interaction camera."));
        CameraActor = Camera;
    }
    CameraActor->SetActorTransform(Transform);
    Entry.ReturnController->SetViewTarget(CameraActor.Get());
    if (const auto* Node = FindNode(CurrentNode)) UpdateSignificance(*Node);
    return true;
}

void FOGSystemicInteractionRuntime::SetPrivacyPresentation(bool Privacy)
{
    Entry.bPrivacyPresentation = Privacy;
    // Mask only presentation. Runtime state, participant binding and consequences
    // are untouched, even when optional assets are missing.
    for (auto& Saved : Restore) if (auto* Actor = Saved.Actor.Get()) Actor->SetActorHiddenInGame(Privacy || Saved.bHidden);
    if (Presentation) Presentation->SetPrivacy(Privacy);
    if (bActive) if (const auto* Node = FindNode(CurrentNode)) UpdatePresentation(*Node);
}

bool FOGSystemicInteractionRuntime::CommitEvent(FName Type, FName Reason, int64 Tick,
    const FOGInteractionAuthoredConsequences* Consequences, FString& Error)
{
    FOGWorldEvent Event;
    Event.EventId = FOGEntityId::NewId(); Event.EventType = Type; Event.WorldTick = Tick; Event.PrimaryEntity = RulerId;
    for (const auto& Participant : Participants) Event.RelatedEntities.Add(Participant.Binding.Character.EntityId);
    auto Payload = MakeShared<FJsonObject>();
    Payload->SetStringField(TEXT("session_id"), SessionId.ToString());
    Payload->SetStringField(TEXT("graph_id"), Graph.GraphId.ToString());
    Payload->SetStringField(TEXT("context"), Entry.Context.ToString());
    Payload->SetStringField(TEXT("context_event_id"), Entry.ContextEventId.ToString());
    Payload->SetStringField(TEXT("location_id"), Entry.LocationId.ToString());
    Payload->SetStringField(TEXT("reason"), Reason.ToString());
    TArray<TSharedPtr<FJsonValue>> Bindings;
    for (const auto& Participant : Participants)
    {
        auto Binding = MakeShared<FJsonObject>();
        Binding->SetStringField(TEXT("entity_id"), Participant.Binding.Character.EntityId.ToString());
        Binding->SetStringField(TEXT("role"), Participant.Binding.Role.ToString());
        if (Participant.Character.bHasManifestation)
            Binding->SetStringField(TEXT("manifestation_id"), Participant.Character.Manifestation.ManifestationId.ToString());
        Bindings.Add(MakeShared<FJsonValueObject>(Binding));
    }
    Payload->SetArrayField(TEXT("participants"), Bindings);
    // Privacy and installed asset availability deliberately never enter canonical payload.
    FJsonSerializer::Serialize(Payload, TJsonWriterFactory<>::Create(&Event.PayloadJson));
    if (!Store.BeginTransaction(Error)) return false;
    auto Abort = [&]() { FString RollbackError; if (!Store.RollbackTransaction(RollbackError)) Error += TEXT("; rollback: ") + RollbackError; return false; };
    // Persist the causal event before authored consequences that may reference it by
    // source_event_id. The surrounding transaction keeps event + consequences atomic.
    if (!Store.AppendWorldEvent(Event, Error)) return Abort();
    if (Consequences)
    {
        for (auto Record : Consequences->Knowledge)
        {
            Record.SourceEventId = Event.EventId; Record.UpdatedWorldTick = Tick;
            if (!Store.UpsertKnowledgeFact(Record, Error)) return Abort();
        }
        for (auto Record : Consequences->Memories)
        {
            if (!Record.MemoryId.IsValid()) Record.MemoryId = FOGEntityId::NewId();
            Record.SourceEventId = Event.EventId;
            if (!Store.UpsertSemanticMemory(Record, Tick, Error)) return Abort();
        }
        for (auto Record : Consequences->Presentation)
        {
            Record.UpdatedWorldTick = Tick;
            if (!Store.UpsertManifestationPresentationState(Record, Error)) return Abort();
        }
        for (auto Record : Consequences->Presence)
        {
            Record.UpdatedWorldTick = Tick;
            if (!Store.UpsertWorldPresence(Record, Error)) return Abort();
        }
    }
    if (!Store.CommitTransaction(Error)) return Abort();
    return true;
}

void FOGSystemicInteractionRuntime::RestoreGameplay()
{
    for (const auto& Saved : Restore) if (auto* Actor = Saved.Actor.Get())
    {
        FTransform ReturnTransform = Saved.Transform;
        for (const auto& Participant : Participants)
            if (Participant.Binding.Actor == Saved.Actor && Saved.bHasPresence && Participant.Character.bHasPresence &&
                Participant.Character.Presence.LocationId == Saved.Presence.LocationId)
                ReturnTransform.AddToTranslation(Participant.Character.Presence.LocalPosition - Saved.Presence.LocalPosition);
        Actor->SetActorTransform(ReturnTransform, false, nullptr, ETeleportType::TeleportPhysics);
        Actor->SetActorHiddenInGame(Saved.bHidden);
        if (auto* Character = Cast<ACharacter>(Actor))
        {
            if (const auto* Played = PlayedMontages.Find(Saved.Actor))
                if (auto* Montage = Cast<UAnimMontage>(Played->Get()))
                    if (auto* Anim = Character->GetMesh()->GetAnimInstance()) Anim->Montage_Stop(0.1f, Montage);
            auto* Movement = Character->GetCharacterMovement();
            Movement->SetMovementMode(static_cast<EMovementMode>(Saved.MovementMode), Saved.CustomMovementMode);
            Movement->Velocity = Saved.Velocity;
        }
    }
    if (auto* Controller = Entry.ReturnController.Get(); Controller && bInputSuppressed)
    {
        if (bMoveSuppressed) Controller->SetIgnoreMoveInput(false);
        if (bLookSuppressed) Controller->SetIgnoreLookInput(false);
        if (ReturnViewTarget.IsValid()) Controller->SetViewTarget(ReturnViewTarget.Get());
        Controller->SetControlRotation(ReturnControlRotation);
    }
    if (CameraActor.IsValid()) CameraActor->Destroy();
    CameraActor.Reset(); ReturnViewTarget.Reset(); Restore.Reset(); PlayedMontages.Reset();
    bMoveSuppressed = false; bLookSuppressed = false; bInputSuppressed = false;
    if (Presentation && bActive) Presentation->RestoreGameplay();
    // Presentation clears its actual optional clip/material references before residency leases.
    SessionAssets.Reset();
    OptionalAssets.ReleaseAll();
    bActive = false; CurrentNode = NAME_None; Sequence.Reset();
}

bool FOGSystemicInteractionRuntime::Exit(FName Reason, int64 Tick, FString& Error)
{
    if (!bActive) return true;
    bool Persisted = Tick >= 0 && CommitEvent(FName(TEXT("interaction.exited")), Reason, Tick, nullptr, Error);
    if (Tick < 0) Error = TEXT("Exit restored gameplay but canonical clock was unavailable.");
    // Refresh actual consequences before returning actors to ordinary presentation.
    FString RefreshError;
    if (!RefreshCharacters(RefreshError) && Persisted) { Error = RefreshError; Persisted = false; }
    // Persistence failure must not trap ordinary controls in staged mode.
    RestoreGameplay();
    return Persisted;
}

FOGInteractionActionGraph FOGSystemicInteractionRuntime::MakeNeutralDiagnosticGraph()
{
    FOGInteractionActionGraph Graph;
    Graph.GraphId = FOGContentId(TEXT("foundation:interaction.diagnostic")); Graph.StartNode = FName(TEXT("align"));
    FOGInteractionActionNode Align; Align.NodeId = Graph.StartNode;
    Align.ActionId = FOGContentId(TEXT("foundation:action.align")); Align.DurationSeconds = 1.0;
    Align.RequiredRoles.Add(PlayerRole); Align.NextNodes.Add(FName(TEXT("handoff"))); Align.NextNodes.Add(Align.NodeId);
    FOGInteractionActionNode Handoff; Handoff.NodeId = FName(TEXT("handoff"));
    Handoff.ActionId = FOGContentId(TEXT("foundation:action.handoff")); Handoff.DurationSeconds = 2.0;
    Handoff.RequiredRoles.Add(PlayerRole); Handoff.NextNodes.Add(Align.NodeId); Handoff.NextNodes.Add(Handoff.NodeId);
    Graph.Nodes.Add(Align); Graph.Nodes.Add(Handoff); return Graph;
}
