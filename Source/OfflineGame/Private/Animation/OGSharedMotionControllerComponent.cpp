#include "Animation/OGSharedMotionControllerComponent.h"
#include "Misc/SecureHash.h"

#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Animation/AnimationAsset.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/OGSharedReferenceAnimInstance.h"

bool UOGSharedMotionControllerComponent::BindCanonicalCharacter(const FOGContentId& IdentityId)
{
    // A live actor/controller belongs to one canonical character, not to each
    // Version or wardrobe. Reject cross-identity reuse without touching its
    // active playback, native meshes or owned gameplay state.
    if (!IdentityId.IsValid() ||
        (BoundCharacterIdentity.IsValid() && BoundCharacterIdentity != IdentityId))
        return false;
    BoundCharacterIdentity = IdentityId;
    return true;
}

UOGSharedMotionControllerComponent::UOGSharedMotionControllerComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = false;
    PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

bool UOGSharedMotionControllerComponent::HaveIdenticalReferencePose(
    const USkeletalMeshComponent* A, const USkeletalMeshComponent* B)
{
    const USkeletalMesh* AMesh = A ? A->GetSkeletalMeshAsset() : nullptr;
    const USkeletalMesh* BMesh = B ? B->GetSkeletalMeshAsset() : nullptr;
    if (!AMesh || !BMesh) return false;
    const FReferenceSkeleton& ARig = AMesh->GetRefSkeleton();
    const FReferenceSkeleton& BRig = BMesh->GetRefSkeleton();
    if (ARig.GetNum() == 0 || ARig.GetNum() != BRig.GetNum()) return false;
    for (int32 Index = 0; Index < ARig.GetNum(); ++Index)
    {
        if (ARig.GetBoneName(Index) != BRig.GetBoneName(Index) ||
            ARig.GetParentIndex(Index) != BRig.GetParentIndex(Index) ||
            !ARig.GetRefBonePose()[Index].Equals(BRig.GetRefBonePose()[Index], 0.0001f))
            return false;
    }
    return true;
}

bool UOGSharedMotionControllerComponent::RegisterNativeParts(
    const TArray<USkeletalMeshComponent*>& Parts)
{
    if (Parts.IsEmpty() || RigProfile.IsNone()) return false;
    TArray<TObjectPtr<USkeletalMeshComponent>> Verified;
    TSet<USkeletalMeshComponent*> Unique;
    for (USkeletalMeshComponent* Part : Parts)
    {
        if (!Part || Part->GetOwner() != GetOwner() || Unique.Contains(Part) ||
            !Part->GetSkeletalMeshAsset() || !Part->GetSkeletalMeshAsset()->GetSkeleton())
            return false;
        Unique.Add(Part);
        Verified.Add(Part);
    }
    TArray<FOGReferenceRigAdapter> Adapters;
    for (USkeletalMeshComponent* Part : Parts)
    {
        FOGReferenceRigAdapter Adapter; FString Error;
        if (!Adapter.Bind(Part, Error)) return false;
        Adapters.Add(MoveTemp(Adapter));
    }
    StopAction();
    RestoreMorphBaselines();
    for (USkeletalMeshComponent* Previous : RegisteredParts)
        if (Previous) Previous->RemoveTickPrerequisiteComponent(this);
    RegisteredParts = MoveTemp(Verified);
    ReferenceRigs = MoveTemp(Adapters);
    for (USkeletalMeshComponent* Part : RegisteredParts) Part->AddTickPrerequisiteComponent(this);
    ApplyMorphOverlays();
    return true;
}

void UOGSharedMotionControllerComponent::SetCurrentOutfit(FName InOutfitProfile)
{
    if (InOutfitProfile != OutfitProfile)
    {
        StopAction();
        OutfitProfile = InOutfitProfile;
    }
}

bool UOGSharedMotionControllerComponent::ResolveForPart(
    const UOGSharedMotionLibrary* Library, USkeletalMeshComponent* Part,
    FName Action, FName Context, bool bAuthorizedAdult,
    int32 ParticipantCount, FName ParticipantRole,
    UAnimSequence*& OutSequence, const FOGSharedMotionEntry*& OutEntry,
    FName RequiredSourceClip, TOptional<bool> RootPolicy) const
{
    OutSequence = nullptr;
    OutEntry = nullptr;
    if (!Part) return false;
    TArray<TObjectPtr<USkeletalMeshComponent>> SearchParts;
    if (bUseIdenticalTopologyLeaderPose)
        SearchParts = RegisteredParts;
    else SearchParts.Add(Part);
    OutSequence = FOGSharedMotionResolver::Resolve(
        Library, Action, RigProfile, AnatomyProfile, OutfitProfile,
        Context, Capabilities, SearchParts, bAuthorizedAdult,
        ParticipantCount, &OutEntry, ParticipantRole, RequiredSourceClip);
    if (!OutSequence)
    {
        const auto* Global=FOGSharedMotionResolver::GetInstalledLibrary();
        if (Library!=Global) OutSequence=FOGSharedMotionResolver::Resolve(Global,Action,RigProfile,AnatomyProfile,OutfitProfile,
            Context,Capabilities,SearchParts,bAuthorizedAdult,ParticipantCount,&OutEntry,ParticipantRole,RequiredSourceClip);
    }
    return OutSequence && OutEntry && OutSequence->GetSkeleton() &&
        (!OutEntry->bUsesRootMotion ||
            (RootPolicy.Get(bPermitAuthoritativeRootMotion) && GetOwner() &&
             GetOwner()->HasAuthority() && Cast<ACharacter>(GetOwner())));
}

bool UOGSharedMotionControllerComponent::CanPlayAction(
    FName GameplayAction, FName Context, bool bAdultContextAuthorized,
    int32 ParticipantCount, FName ParticipantRole) const
{
    return CanPlayResolvedAction(GameplayAction, Context, bAdultContextAuthorized,
        ParticipantCount, ParticipantRole, NAME_None);
}

bool UOGSharedMotionControllerComponent::CanPlayResolvedAction(
    FName GameplayAction, FName Context, bool bAdultContextAuthorized,
    int32 ParticipantCount, FName ParticipantRole, FName RequiredSourceClip, TOptional<bool> RootPolicy) const
{
    if (RigProfile.IsNone() || GameplayAction.IsNone() || Context.IsNone() ||
        RegisteredParts.IsEmpty() || ParticipantCount < 1)
        return false;
    FOGSharedMotionEntry Shared;
    TSharedPtr<const FOGReferenceMotionClip, ESPMode::ThreadSafe> Source;
    TArray<TArray<FOGReferenceBoneConnection>> Mapping; FString SharedError;
    if (FindReference(GameplayAction, Context, ParticipantCount, ParticipantRole,
        Shared, Source, Mapping, SharedError, RequiredSourceClip)) return true;
    const UOGSharedMotionLibrary* Library = SharedLibrary.IsNull()
        ? nullptr : SharedLibrary.LoadSynchronous();
    if (!Library) Library = FOGSharedMotionResolver::GetInstalledLibrary();
    FName SourceId = NAME_None;
    bool bInitialLoop = false;
    USkeletalMeshComponent* Leader = nullptr;
    int32 ValidCount = 0;
    TArray<USkeletalMeshComponent*> RigDrivers;
    for (USkeletalMeshComponent* Part : RegisteredParts)
    {
        // A wardrobe can contain dozens of source parts with the exact same
        // rest rig. Preflight one authentic clip for that rig, not each part.
        if (bGroupPartsByReferencePose && !bUseIdenticalTopologyLeaderPose)
        {
            bool bFollower = false;
            for (USkeletalMeshComponent* Driver : RigDrivers)
                if (HaveIdenticalReferencePose(Driver, Part))
                { bFollower = true; break; }
            if (bFollower) { ++ValidCount; continue; }
        }
        UAnimSequence* Clip = nullptr;
        const FOGSharedMotionEntry* Entry = nullptr;
        if (!ResolveForPart(Library, Part, GameplayAction, Context,
            bAdultContextAuthorized, ParticipantCount, ParticipantRole, Clip, Entry, RequiredSourceClip, RootPolicy))
            return false;
        if (!SourceId.IsNone() &&
            (SourceId != Entry->SourceClipId || bInitialLoop != Entry->bLoop))
            return false;
        SourceId = Entry->SourceClipId;
        bInitialLoop = Entry->bLoop;
        if (bUseIdenticalTopologyLeaderPose)
        {
            for (USkeletalMeshComponent* Candidate : RegisteredParts)
                if (Candidate && Candidate->GetSkeletalMeshAsset() &&
                    Candidate->GetSkeletalMeshAsset()->GetSkeleton() == Clip->GetSkeleton())
                { Leader = Candidate; break; }
            if (!Leader) return false;
            for (USkeletalMeshComponent* Candidate : RegisteredParts)
                if (!HaveIdenticalReferencePose(Leader, Candidate)) return false;
            return true;
        }
        RigDrivers.Add(Part);
        ++ValidCount;
    }
    return ValidCount == RegisteredParts.Num();
}

bool UOGSharedMotionControllerComponent::PreviewStagedSource(FName Action,FName Context,
    bool Authorized,int32 Count,FName Role,FName& OutSource,FName& OutScene) const
{
    OutSource=NAME_None; OutScene=NAME_None;
    if (!CanPlayResolvedAction(Action,Context,Authorized,Count,Role,NAME_None,TOptional<bool>(false))) return false;
    FOGSharedMotionEntry Shared;
    TSharedPtr<const FOGReferenceMotionClip,ESPMode::ThreadSafe> Source;
    TArray<TArray<FOGReferenceBoneConnection>> Mapping; FString Error;
    if (FindReference(Action,Context,Count,Role,Shared,Source,Mapping,Error))
    { OutSource=Shared.SourceClipId; OutScene=Shared.AuthoredSceneId; return !OutSource.IsNone(); }
    const UOGSharedMotionLibrary* Library=SharedLibrary.IsNull()?nullptr:SharedLibrary.LoadSynchronous();
    if (!Library) Library=FOGSharedMotionResolver::GetInstalledLibrary();
    UAnimSequence* Sequence=nullptr; const FOGSharedMotionEntry* Entry=nullptr;
    if (!ResolveForPart(Library,RegisteredParts[0],Action,Context,Authorized,Count,Role,
        Sequence,Entry,NAME_None,TOptional<bool>(false))) return false;
    OutSource=Entry->SourceClipId; OutScene=Entry->AuthoredSceneId; return !OutSource.IsNone();
}

bool UOGSharedMotionControllerComponent::PreviewSourceClipId(
    FName GameplayAction, FName Context, bool bAdultContextAuthorized,
    int32 ParticipantCount, FName ParticipantRole, FName& OutSourceClipId) const
{
    OutSourceClipId = NAME_None;
    if (!CanPlayAction(GameplayAction, Context, bAdultContextAuthorized,
        ParticipantCount, ParticipantRole)) return false;
    FOGSharedMotionEntry Shared;
    TSharedPtr<const FOGReferenceMotionClip, ESPMode::ThreadSafe> Source;
    TArray<TArray<FOGReferenceBoneConnection>> Mapping; FString SharedError;
    if (FindReference(GameplayAction, Context, ParticipantCount, ParticipantRole,
        Shared, Source, Mapping, SharedError))
    {
        OutSourceClipId = Shared.SourceClipId;
        return !OutSourceClipId.IsNone();
    }
    const UOGSharedMotionLibrary* Library = SharedLibrary.IsNull()
        ? nullptr : SharedLibrary.LoadSynchronous();
    if (!Library) Library = FOGSharedMotionResolver::GetInstalledLibrary();
    UAnimSequence* Sequence = nullptr;
    const FOGSharedMotionEntry* Entry = nullptr;
    if (!ResolveForPart(Library, RegisteredParts[0], GameplayAction, Context,
        bAdultContextAuthorized, ParticipantCount, ParticipantRole, Sequence, Entry))
        return false;
    OutSourceClipId = Entry->SourceClipId;
    return !OutSourceClipId.IsNone();
}

bool UOGSharedMotionControllerComponent::PreviewAuthoredSceneId(
    FName GameplayAction, FName Context, bool bAdultContextAuthorized,
    int32 ParticipantCount, FName ParticipantRole, FName& OutSceneId) const
{
    OutSceneId = NAME_None;
    if (ParticipantCount < 2 ||
        !CanPlayAction(GameplayAction, Context, bAdultContextAuthorized,
            ParticipantCount, ParticipantRole)) return false;
    FOGSharedMotionEntry Shared;
    TSharedPtr<const FOGReferenceMotionClip, ESPMode::ThreadSafe> Source;
    TArray<TArray<FOGReferenceBoneConnection>> Mapping; FString SharedError;
    if (FindReference(GameplayAction, Context, ParticipantCount, ParticipantRole,
        Shared, Source, Mapping, SharedError))
    {
        OutSceneId = Shared.AuthoredSceneId;
        return !OutSceneId.IsNone();
    }
    const UOGSharedMotionLibrary* Library = SharedLibrary.IsNull()
        ? nullptr : SharedLibrary.LoadSynchronous();
    if (!Library) Library = FOGSharedMotionResolver::GetInstalledLibrary();
    UAnimSequence* Sequence = nullptr;
    const FOGSharedMotionEntry* Entry = nullptr;
    if (!ResolveForPart(Library, RegisteredParts[0], GameplayAction, Context,
            bAdultContextAuthorized, ParticipantCount, ParticipantRole,
            Sequence, Entry) || !Entry) return false;
    OutSceneId = Entry->AuthoredSceneId;
    return !OutSceneId.IsNone();
}

void UOGSharedMotionControllerComponent::SynchronizeToWorldTime(float SceneStartTime)
{
    if (!bPlaying || !GetWorld()) return;
    bSceneTimeLocked = true;
    SceneWorldStartTime = SceneStartTime;
    SceneAnchorElapsed = 0.f;
    ElapsedSeconds = FMath::Max(0.f, GetWorld()->GetTimeSeconds() - SceneWorldStartTime) * ExternalPacing;
}

bool UOGSharedMotionControllerComponent::RestorePlaybackPhase(float ElapsedGameplaySeconds)
{
    if (!bPlaying || !FMath::IsFinite(ElapsedGameplaySeconds) || ElapsedGameplaySeconds<0) return false;
    ElapsedSeconds=ElapsedGameplaySeconds; SceneAnchorElapsed=ElapsedSeconds;
    if (bSceneTimeLocked && GetWorld()) SceneWorldStartTime=GetWorld()->GetTimeSeconds();
    const float SourceTime=ElapsedSeconds*PlaybackRate;
    LastContactCycle=bHasContact && SourceTime>=SourceContactSeconds
        ? (bLooping?FMath::FloorToInt((SourceTime-SourceContactSeconds)/SequenceSeconds):0):-1;
    const float Phase=bLooping?FMath::Fmod(SourceTime,SequenceSeconds)/SequenceSeconds:FMath::Clamp(SourceTime/SequenceSeconds,0.f,1.f);
    if (CurrentLeader) CurrentLeader->SetPosition(Phase*SequenceSeconds,false);
    else for (int32 I=0;I<PlayingDriverParts.Num();++I)
        if (PlayingDriverParts[I] && PlayingSequenceLengths.IsValidIndex(I))
            // One phase value is shared by all actual rig drivers, not
            // indexed by the full wardrobe mesh list. Followers inherit
            // the driver through SetLeaderPoseComponent.
            PlayingDriverParts[I]->SetPosition(
                Phase*PlayingSequenceLengths[I],false);
    if (ReferenceClip) UpdateReferencePose();
    ApplyMorphOverlays(); return true;
}

bool UOGSharedMotionControllerComponent::SetExternalPacing(float Rate)
{
    if (!FMath::IsFinite(Rate) || Rate < 0.f) return false;
    if (Rate == ExternalPacing) return true;
    if (bSceneTimeLocked && GetWorld())
    {
        const float Now = GetWorld()->GetTimeSeconds();
        // Preserve phase at the speed boundary, including a paused interval.
        SceneAnchorElapsed += FMath::Max(0.f, Now - SceneWorldStartTime) * ExternalPacing;
        ElapsedSeconds = SceneAnchorElapsed;
        SceneWorldStartTime = Now;
    }
    ExternalPacing = Rate;
    for (int32 I = 0; I < PlayingParts.Num(); ++I)
        if (PlayingParts[I])
        {
            const float LengthScale = CurrentLeader ? 1.f :
                (PlayingSequenceLengths.IsValidIndex(I) && SequenceSeconds > 0.f
                    ? PlayingSequenceLengths[I] / SequenceSeconds : 1.f);
            PlayingParts[I]->SetPlayRate(PlaybackRate * ExternalPacing * LengthScale);
        }
    return true;
}

bool UOGSharedMotionControllerComponent::SetMorphLayer(
    FName Layer, FName Morph, float Weight)
{
    if (Layer.IsNone() || Morph.IsNone() || !FMath::IsFinite(Weight) || RegisteredParts.IsEmpty()) return false;
    bool bHasNativeMorph = false;
    for (USkeletalMeshComponent* Part : RegisteredParts)
        if (Part && Part->GetSkeletalMeshAsset() &&
            Part->GetSkeletalMeshAsset()->FindMorphTarget(Morph))
            { bHasNativeMorph = true; break; }
    if (!bHasNativeMorph) return false;
    for (FMorphOverlay& Overlay : MorphOverlays)
    {
        if (Overlay.Layer == Layer && Overlay.Morph == Morph)
        {
            Overlay.Weight = FMath::Clamp(Weight, -1.f, 1.f);
            SetComponentTickEnabled(true);
            ApplyMorphOverlays();
            return true;
        }
    }
    FMorphOverlay& New = MorphOverlays.AddDefaulted_GetRef();
    New.Layer = Layer;
    New.Morph = Morph;
    New.Weight = FMath::Clamp(Weight, -1.f, 1.f);
    SetComponentTickEnabled(true);
    ApplyMorphOverlays();
    return true;
}

void UOGSharedMotionControllerComponent::ClearMorphLayer(FName Layer)
{
    MorphOverlays.RemoveAll([Layer](const FMorphOverlay& M)
        { return M.Layer == Layer; });
    ApplyMorphOverlays();
    if (!bPlaying && MorphOverlays.IsEmpty()) SetComponentTickEnabled(false);
}

void UOGSharedMotionControllerComponent::ApplyMorphOverlays()
{
    // A facial/secondary blendshape channel leaves full-body native playback
    // intact. Absent morph targets never trigger guessed skeletal retargeting.
    TMap<FName, float> Combined;
    for (const FMorphOverlay& Overlay : MorphOverlays)
        Combined.FindOrAdd(Overlay.Morph) += Overlay.Weight;
    const float RawTime = ElapsedSeconds * PlaybackRate;
    const float AuthoredTime = bPlaying && bLooping && SequenceSeconds > SMALL_NUMBER
        ? FMath::Fmod(RawTime, SequenceSeconds) : RawTime;
    if (bPlaying)
    for (const FOGSharedMorphTrack& Track : AuthoredMorphTracks)
    {
        if (Track.Morph.IsNone() || Track.Keys.IsEmpty()) continue;
        float Weight = Track.Keys[0].Weight;
        if (AuthoredTime >= Track.Keys.Last().TimeSeconds)
            Weight = Track.Keys.Last().Weight;
        else for (int32 K = 1; K < Track.Keys.Num(); ++K)
        {
            if (AuthoredTime <= Track.Keys[K].TimeSeconds)
            {
                const float Span = FMath::Max(Track.Keys[K].TimeSeconds -
                    Track.Keys[K-1].TimeSeconds, KINDA_SMALL_NUMBER);
                const float T = FMath::Clamp((AuthoredTime -
                    Track.Keys[K-1].TimeSeconds) / Span, 0.f, 1.f);
                Weight = FMath::Lerp(Track.Keys[K-1].Weight,
                    Track.Keys[K].Weight, T);
                break;
            }
        }
        Combined.FindOrAdd(ResolveSourceMorph(Track.Morph)) += Weight;
    }
    // Save each actual part's prior value only while this controller owns that channel.
    // Removing an expression restores its real wardrobe/face value, rather than zero.
    for (int32 I=MorphBaselines.Num()-1;I>=0;--I)
    {
        auto& Saved=MorphBaselines[I]; auto* Part=Saved.Part.Get();
        if (!Part || Part->GetSkeletalMeshAsset()!=Saved.Mesh.Get())
        { MorphBaselines.RemoveAtSwap(I); continue; }
        if (!Combined.Contains(Saved.Morph))
        { Part->SetMorphTarget(Saved.Morph,Saved.Weight); MorphBaselines.RemoveAtSwap(I); }
    }
    for (USkeletalMeshComponent* Part : RegisteredParts)
    {
        if (!Part || !Part->GetSkeletalMeshAsset()) continue;
        for (const auto& Pair : Combined)
            if (Part->GetSkeletalMeshAsset()->FindMorphTarget(Pair.Key))
            {
                if (!MorphBaselines.ContainsByPredicate([&](const auto& V)
                    { return V.Part.Get()==Part && V.Mesh.Get()==Part->GetSkeletalMeshAsset() && V.Morph==Pair.Key; }))
                {
                    FMorphBaseline Saved; Saved.Part=Part; Saved.Mesh=Part->GetSkeletalMeshAsset();
                    Saved.Morph=Pair.Key; Saved.Weight=Part->GetMorphTarget(Pair.Key); MorphBaselines.Add(Saved);
                }
                Part->SetMorphTarget(Pair.Key,FMath::Clamp(Pair.Value,-1.f,1.f));
            }
    }
    AppliedMorphs.Reset();
    for (const auto& Pair : Combined) AppliedMorphs.Add(Pair.Key);
}

void UOGSharedMotionControllerComponent::ApplyAuthoredRootMotion(
    float PreviousSourceTime, float SourceTime)
{
    if (!bApplyingRootMotion || !CurrentSequence || SourceTime <= PreviousSourceTime)
        return;
    ACharacter* Character = Cast<ACharacter>(GetOwner());
    if (!Character || !Character->HasAuthority() ||
        !Character->GetCharacterMovement()) return;

    const float Length = SequenceSeconds;
    const FAnimExtractContext Extraction(0., true);
    auto ApplySegment = [Character, this, &Extraction](float Start, float End)
    {
        if (End <= Start) return;
        const FTransform Delta =
            CurrentSequence->ExtractRootMotionFromRange(Start, End, Extraction);
        if (Delta.Equals(FTransform::Identity, KINDA_SMALL_NUMBER)) return;
        // Original Koikatsu meshes use a measured visual-axis correction.
        // Root displacement follows the selected actual skeletal component
        // relative orientation rather than blindly following the capsule X.
        const USkeletalMeshComponent* Driver = CurrentLeader
            ? CurrentLeader.Get() : (PlayingParts.IsEmpty() ? nullptr : PlayingParts[0].Get());
        const FQuat VisualToActor = Driver ? Driver->GetRelativeRotation().Quaternion()
            : FQuat::Identity;
        const FQuat ActorQuat = Character->GetActorQuat();
        const FVector WorldDelta = ActorQuat.RotateVector(
            VisualToActor.RotateVector(Delta.GetTranslation()));
        const FQuat WorldRotation = ActorQuat *
            (VisualToActor * Delta.GetRotation() * VisualToActor.Inverse());
        FHitResult Hit;
        Character->GetCharacterMovement()->MoveUpdatedComponent(
            WorldDelta, WorldRotation, true, &Hit);
        if (Hit.IsValidBlockingHit())
        {
            UE_LOG(LogTemp, Verbose,
                TEXT("Shared root motion collision blocked action=%s"), *CurrentAction.ToString());
        }
    };
    if (!bLooping)
    {
        ApplySegment(FMath::Min(PreviousSourceTime, Length),
            FMath::Min(SourceTime, Length));
        return;
    }
    float Cursor = PreviousSourceTime;
    const float Stop = FMath::Min(SourceTime, Cursor + Length * 4.f);
    while (Cursor < Stop)
    {
        const float CycleStart = FMath::FloorToFloat(Cursor / Length) * Length;
        const float End = FMath::Min(Stop, CycleStart + Length);
        ApplySegment(Cursor - CycleStart, End - CycleStart);
        if (End <= Cursor + SMALL_NUMBER) break;
        Cursor = End;
    }
}

bool UOGSharedMotionControllerComponent::PlayAction(
    FName GameplayAction, FName Context, bool bAdultContextAuthorized,
    int32 ParticipantCount, FName ParticipantRole)
{
    return PlayResolvedAction(GameplayAction, Context, bAdultContextAuthorized,
        ParticipantCount, ParticipantRole, NAME_None);
}

bool UOGSharedMotionControllerComponent::PlayResolvedAction(
    FName GameplayAction, FName Context, bool bAdultContextAuthorized,
    int32 ParticipantCount, FName ParticipantRole, FName RequiredSourceClip)
{
    FOGSharedMotionEntry Shared;
    TSharedPtr<const FOGReferenceMotionClip, ESPMode::ThreadSafe> Source;
    TArray<TArray<FOGReferenceBoneConnection>> Mapping; FString SharedError;
    if (FindReference(GameplayAction, Context, ParticipantCount, ParticipantRole,
        Shared, Source, Mapping, SharedError, RequiredSourceClip))
        return StartReference(Shared, MoveTemp(Source), MoveTemp(Mapping));
    if (!CanPlayResolvedAction(GameplayAction, Context, bAdultContextAuthorized,
        ParticipantCount, ParticipantRole, RequiredSourceClip)) return false;

    const UOGSharedMotionLibrary* Library =
        SharedLibrary.IsNull() ? nullptr : SharedLibrary.LoadSynchronous();
    if (!Library) Library = FOGSharedMotionResolver::GetInstalledLibrary();

    TArray<UAnimSequence*> Sequences;
    TArray<const FOGSharedMotionEntry*> Entries;
    TArray<USkeletalMeshComponent*> RigDrivers;
    TArray<int32> RigDriverForPart;
    USkeletalMeshComponent* Leader = nullptr;

    // One shared semantic action drives the entire character. Exact matching
    // rest-pose mesh parts follow one rig driver. Distinct authentic rig
    // families resolve separate native clips against the SAME source action.
    for (USkeletalMeshComponent* Part : RegisteredParts)
    {
        if (!Part || !Part->GetSkeletalMeshAsset()) return false;
        if (bGroupPartsByReferencePose && !bUseIdenticalTopologyLeaderPose)
        {
            int32 ExistingDriver = INDEX_NONE;
            for (int32 I = 0; I < RigDrivers.Num(); ++I)
                if (HaveIdenticalReferencePose(RigDrivers[I], Part))
                { ExistingDriver = I; break; }
            if (ExistingDriver != INDEX_NONE)
            {
                RigDriverForPart.Add(ExistingDriver);
                continue;
            }
        }
        const FOGSharedMotionEntry* Entry = nullptr;
        UAnimSequence* Sequence = nullptr;
        if (!ResolveForPart(Library, Part, GameplayAction, Context,
            bAdultContextAuthorized, ParticipantCount, ParticipantRole,
            Sequence, Entry, RequiredSourceClip)) return false;

        if (!Sequences.IsEmpty() &&
            (Entry->SourceClipId != Entries[0]->SourceClipId ||
             Entry->bLoop != Entries[0]->bLoop))
            return false; // Prevent mismatched source timing or identity.

        RigDriverForPart.Add(RigDrivers.Num());
        RigDrivers.Add(Part);
        Sequences.Add(Sequence);
        Entries.Add(Entry);
        if (bUseIdenticalTopologyLeaderPose) break;
    }

    if (bUseIdenticalTopologyLeaderPose)
    {
        for (USkeletalMeshComponent* Part : RegisteredParts)
        {
            if (Part->GetSkeletalMeshAsset()->GetSkeleton() == Sequences[0]->GetSkeleton())
            {
                Leader = Part;
                break;
            }
        }
        if (!Leader) return false;
        for (USkeletalMeshComponent* Part : RegisteredParts)
            if (!HaveIdenticalReferencePose(Leader, Part)) return false;
    }
    else if (RigDriverForPart.Num() != RegisteredParts.Num() ||
             Sequences.Num() != RigDrivers.Num()) return false;

    // All checks finish before interrupting an existing action.
    StopAction();
    const FOGSharedMotionEntry* PrimaryEntry = Entries[0];
    CurrentSequence = Sequences[0];
    CurrentLeader = Leader;
    PlayingParts = RegisteredParts;
    if (bUseIdenticalTopologyLeaderPose)
        PlayingDriverParts.Add(Leader);
    else
        for (USkeletalMeshComponent* Driver : RigDrivers)
            PlayingDriverParts.Add(Driver);
    bPlaying = true;
    bApplyingRootMotion = PrimaryEntry->bUsesRootMotion;
    AuthoredMorphTracks = PrimaryEntry->MorphTracks;
    bSceneTimeLocked = false;
    SceneAnchorElapsed = 0.f;
    ExternalPacing = 1.f;
    bLooping = PrimaryEntry->bLoop;
    bHasContact = PrimaryEntry->ContactSeconds >= 0.f;
    SourceContactSeconds = PrimaryEntry->ContactSeconds;
    if (!bHasContact && !PrimaryEntry->ContactNotify.IsNone())
    {
        // Native authored animation notifies take precedence over guessed
        // generic contact timing. No notify means no contact event.
        for (const FAnimNotifyEvent& Notify : Sequences[0]->Notifies)
        {
            if (Notify.NotifyName == PrimaryEntry->ContactNotify)
            {
                SourceContactSeconds = Notify.GetTriggerTime();
                bHasContact = true;
                break;
            }
        }
    }
    SequenceSeconds = FMath::Max(Sequences[0]->GetPlayLength(), 0.001f);
    PlaybackRate = FMath::Clamp(PrimaryEntry->PlayRate, 0.01f, 8.f);
    ElapsedSeconds = 0.f;
    LastContactCycle = -1;
    CurrentAction = GameplayAction;
    CurrentMotionId = PrimaryEntry->MotionId;
    CurrentSourceClipId = PrimaryEntry->SourceClipId;

    if (bUseIdenticalTopologyLeaderPose)
    {
        Leader->SetLeaderPoseComponent(nullptr);
        Leader->SetAnimationMode(EAnimationMode::AnimationSingleNode);
        Leader->PlayAnimation(Sequences[0], bLooping);
        Leader->SetPlayRate(PlaybackRate);
        PlayingSequences.Add(Sequences[0]);
        PlayingSequenceLengths.Add(SequenceSeconds);
        for (USkeletalMeshComponent* Part : PlayingParts)
            if (Part != Leader) Part->SetLeaderPoseComponent(Leader);
    }
    else
    {
        // Only one clip per distinct rig driver. No redundant 45-way copies
        // of identical poses, but no forced leader across incompatible rigs.
        for (int32 I = 0; I < PlayingDriverParts.Num(); ++I)
        {
            USkeletalMeshComponent* Part = PlayingDriverParts[I];
            const float Length = FMath::Max(Sequences[I]->GetPlayLength(), 0.001f);
            Part->SetLeaderPoseComponent(nullptr);
            Part->SetAnimationMode(EAnimationMode::AnimationSingleNode);
            Part->PlayAnimation(Sequences[I], bLooping);
            Part->SetPlayRate(PlaybackRate * Length / SequenceSeconds);
            PlayingSequences.Add(Sequences[I]);
            PlayingSequenceLengths.Add(Length);
        }
        if (bGroupPartsByReferencePose)
        {
            for (int32 I = 0; I < PlayingParts.Num(); ++I)
            {
                USkeletalMeshComponent* Part = PlayingParts[I];
                USkeletalMeshComponent* Driver = PlayingDriverParts[RigDriverForPart[I]];
                if (Part != Driver) Part->SetLeaderPoseComponent(Driver);
            }
        }
    }
    SetComponentTickEnabled(true);
    return true;
}


void UOGSharedMotionControllerComponent::StopAction()
{
    if (!bPreserveCombatQueue) { CombatQueue.Reset(); CombatReceipts.Reset(); CombatScope=FOGEntityId(); }
    bPlayingQueuedCombat=false; ActiveCombatReceipt=FOGEntityId();
    ++PlaybackGeneration;
    RestoreReferencePose();
    SetComponentTickEnabled(!MorphOverlays.IsEmpty());
    for (USkeletalMeshComponent* Part : PlayingParts)
    {
        if (Part) Part->SetLeaderPoseComponent(nullptr);
    }
    for (USkeletalMeshComponent* Part : PlayingParts)
    {
        if (Part) Part->Stop();
    }
    PlayingParts.Reset();
    PlayingDriverParts.Reset();
    PlayingSequences.Reset();
    PlayingSequenceLengths.Reset();
    CurrentLeader = nullptr;
    CurrentSequence = nullptr;
    CurrentAction = NAME_None;
    CurrentMotionId = NAME_None;
    CurrentSourceClipId = NAME_None;
    AuthoredMorphTracks.Reset();
    bPlaying = false;
    bApplyingRootMotion = false;
    bSceneTimeLocked = false;
    bLooping = false;
    bHasContact = false;
    SourceContactSeconds = -1.f;
    ElapsedSeconds = 0.f;
    SequenceSeconds = 0.f;
    SceneAnchorElapsed = 0.f;
    ExternalPacing = 1.f;
    LastContactCycle = -1;
    ApplyMorphOverlays();
}

void UOGSharedMotionControllerComponent::TickComponent(
    float DeltaSeconds, ELevelTick TickType, FActorComponentTickFunction* TickFunction)
{
    Super::TickComponent(DeltaSeconds, TickType, TickFunction);
    if (DeltaSeconds <= 0.f) return;
    if (!bPlaying)
    {
        StartNextCombatMotion();
        ApplyMorphOverlays();
        return;
    }
    const uint64 Generation = PlaybackGeneration;
    const float Before = ElapsedSeconds * PlaybackRate;
    ElapsedSeconds = bSceneTimeLocked && GetWorld()
        ? SceneAnchorElapsed + FMath::Max(0.f, GetWorld()->GetTimeSeconds() - SceneWorldStartTime) * ExternalPacing
        : ElapsedSeconds + DeltaSeconds * ExternalPacing;
    const float SourceTime = ElapsedSeconds * PlaybackRate;
    if (ReferenceClip) UpdateReferencePose();
    if (!bPlaying) return;
    ApplyAuthoredRootMotion(Before, SourceTime);
    ApplyMorphOverlays();
    if (!ReferenceClip && !CurrentLeader && bPhaseLockIndependentParts)
    {
        // Synchronize independently imported part skeletons by normalized
        // source phase, not by assuming their native durations are identical.
        const float Normalized = bLooping
            ? FMath::Fmod(SourceTime, SequenceSeconds) / SequenceSeconds
            : FMath::Clamp(SourceTime / SequenceSeconds, 0.f, 1.f);
        for (int32 I = 0; I < PlayingDriverParts.Num(); ++I)
            if (PlayingDriverParts[I] && PlayingSequenceLengths.IsValidIndex(I))
                PlayingDriverParts[I]->SetPosition(
                    FMath::Min(Normalized * PlayingSequenceLengths[I],
                        PlayingSequenceLengths[I] - 0.0001f), false);
    }
    // Crossing authored source contact must survive a low-FPS frame that
    // jumps straight past the event (or even across a full loop boundary).
    if (bHasContact && SourceContactSeconds <= SequenceSeconds &&
        SourceTime >= SourceContactSeconds)
    {
        const int32 ContactCycle = bLooping
            ? FMath::FloorToInt((SourceTime - SourceContactSeconds) / SequenceSeconds)
            : 0;
        if (ContactCycle > LastContactCycle)
        {
            LastContactCycle = ContactCycle;
            OnAuthoredContact.Broadcast(CurrentAction, CurrentMotionId);
            // A contact receiver may finish or replace this action synchronously.
            // Never stop the new action using the old action's elapsed time.
            if (Generation != PlaybackGeneration) return;
        }
    }
    if (!bLooping && SourceTime >= SequenceSeconds)
    {
        const FName FinishedAction = CurrentAction;
        const FName FinishedMotion = CurrentMotionId;
        const FOGEntityId FinishedReceipt=ActiveCombatReceipt;
        {
            TGuardValue<bool> Preserve(bPreserveCombatQueue,true);
            StopAction();
        }
        const uint64 FinishedGeneration=PlaybackGeneration;
        { TGuardValue<FOGEntityId> FinishedScope(FinishingCombatReceipt,FinishedReceipt);
          OnNativeMotionFinished.Broadcast(this,FinishedAction,FinishedMotion);
          OnMotionFinished.Broadcast(FinishedAction, FinishedMotion); }
        if (!bPlaying && PlaybackGeneration==FinishedGeneration) StartNextCombatMotion();
    }
}

void UOGSharedMotionControllerComponent::EndPlay(
    const EEndPlayReason::Type EndPlayReason)
{
    StopAction();
    MorphOverlays.Reset();
    ApplyMorphOverlays();
    RestoreMorphBaselines();
    OnNativeMotionFinished.Clear();
    for (USkeletalMeshComponent* Part : RegisteredParts)
        if (Part) Part->RemoveTickPrerequisiteComponent(this);
    ReferenceRigs.Reset(); RegisteredParts.Reset();
    Super::EndPlay(EndPlayReason);
}

bool UOGSharedMotionControllerComponent::PrepareReference(const FOGReferenceMotionClip& Clip,
    TArray<TArray<FOGReferenceBoneConnection>>& Out, FString& Error) const
{
    Out.Reset();
    if (RegisteredParts.IsEmpty() || ReferenceRigs.Num()!=RegisteredParts.Num())
    { Error=TEXT("Register the character's actual native rigs first."); return false; }
    for (int32 I=0; I<RegisteredParts.Num(); ++I)
    {
        const auto* Part=RegisteredParts[I].Get();
        if (!Part || ReferenceRigs[I].Mesh.Get()!=Part->GetSkeletalMeshAsset())
        { Out.Reset(); Error=TEXT("Character rig changed; refresh its shared-library connection."); return false; }
        TArray<FOGReferenceBoneConnection> Indices;
        if (!ReferenceRigs[I].Map(Clip,SharedSourceBoneAliases,Indices,Error)) { Out.Reset(); return false; }
        Out.Add(MoveTemp(Indices));
    }
    return true;
}
bool UOGSharedMotionControllerComponent::FindReference(FName Action,FName Context,int32 Count,FName Role,
    FOGSharedMotionEntry& OutEntry,TSharedPtr<const FOGReferenceMotionClip,ESPMode::ThreadSafe>& OutClip,
    TArray<TArray<FOGReferenceBoneConnection>>& OutMapping,FString& Error,FName RequiredSourceClip) const
{
    OutClip.Reset(); OutMapping.Reset();
    for (const auto& Entry:FOGSharedReferenceLibrary::Find(Action,Context,RigProfile,AnatomyProfile,Capabilities,Count,Role))
    {
        if (!RequiredSourceClip.IsNone() &&
            FOGSharedReferenceLibrary::CanonicalId(RequiredSourceClip)!=Entry.SourceClipId) continue;
        if (Entry.bUsesRootMotion || (!Entry.ContactNotify.IsNone() && Entry.ContactSeconds<0)) continue; // Existing collision-authorized native root path remains authoritative.
        if (!SharedLibrary.IsNull())
        {
            const auto* Explicit=SharedLibrary.LoadSynchronous();
            if (!Explicit) continue;
            bool Declared=false,DeclaresAction=false;
            for (const auto& Choice:Explicit->Motions)
                if (Choice.bApprovedForGameplay && Choice.GameplayAction==Action &&
                    Choice.CompatibleRigProfile==RigProfile &&
                    (Choice.GameplayContext.IsNone() || Choice.GameplayContext==Context) &&
                    Choice.RequiredParticipants==Count &&
                    (Count==1 ? Choice.SceneRole.IsNone() : Choice.SceneRole==Role))
                {
                    DeclaresAction=true;
                    if (FOGSharedReferenceLibrary::CanonicalId(Choice.SourceClipId)==Entry.SourceClipId)
                    { Declared=true; break; }
                }
            // Preserve an existing local semantic override. New globally installed
            // source semantics require no per-character copy or whitelist entry.
            if (DeclaresAction && !Declared) continue;
        }
        auto Clip=FOGSharedReferenceLibrary::Load(Entry.SourceClipId,Error);
        if (Clip && PrepareReference(*Clip,OutMapping,Error))
        { OutEntry=Entry; OutClip=MoveTemp(Clip); return true; }
    }
    return false;
}
TArray<FName> UOGSharedMotionControllerComponent::GetSharedSourceMotionIds() const
{
    TArray<FName> Ids=FOGSharedReferenceLibrary::GetClipIds();
    const UOGSharedMotionLibrary* Libraries[]={
        SharedLibrary.IsNull()?nullptr:SharedLibrary.LoadSynchronous(),
        FOGSharedMotionResolver::GetInstalledLibrary()};
    for (const UOGSharedMotionLibrary* Library:Libraries)
        if (Library) for (const auto& Entry:Library->Motions)
            if (Entry.bApprovedForGameplay && !Entry.SourceClipId.IsNone() && !Entry.NativeSequence.IsNull())
                Ids.AddUnique(Entry.SourceClipId);
    Ids.Sort([](FName A,FName B){ return A.LexicalLess(B); });
    return Ids;
}
bool UOGSharedMotionControllerComponent::PlaySharedSourceMotion(FName Id,bool Loop)
{
    if (Id.IsNone()) return false;
    FString Error;
    auto Clip=FOGSharedReferenceLibrary::Load(Id,Error);
    TArray<TArray<FOGReferenceBoneConnection>> Mapping;
    if (Clip && PrepareReference(*Clip,Mapping,Error))
    {
        FOGSharedMotionEntry Entry; Entry.SourceClipId=Id; Entry.MotionId=Id;
        Entry.GameplayAction=FName(*(TEXT("Library.Source.")+Id.ToString()));
        Entry.bLoop=Loop;
        return StartReference(Entry,MoveTemp(Clip),MoveTemp(Mapping));
    }
    const UOGSharedMotionLibrary* Libraries[]={
        SharedLibrary.IsNull()?nullptr:SharedLibrary.LoadSynchronous(),
        FOGSharedMotionResolver::GetInstalledLibrary()};
    for (const UOGSharedMotionLibrary* Library:Libraries)
    {
        if (!Library) continue;
        for (const auto& Entry:Library->Motions)
        {
            if (Entry.SourceClipId!=Id || !Entry.bApprovedForGameplay || Entry.NativeSequence.IsNull()) continue;
            const FName Context=Entry.GameplayContext.IsNone()?FName(TEXT("Library")):Entry.GameplayContext;
            // Retain the requested original identity even when other motions share its semantic route.
            if (!PlayResolvedAction(Entry.GameplayAction,Context,false,
                Entry.RequiredParticipants,Entry.SceneRole,Id)) continue;
            bLooping=Loop;
            for (auto* Part:PlayingParts)
                if (Part) if (auto* Node=Part->GetSingleNodeInstance()) Node->SetLooping(Loop);
            return true;
        }
    }
    return false;
}
bool UOGSharedMotionControllerComponent::StartReference(const FOGSharedMotionEntry& Entry,
    TSharedPtr<const FOGReferenceMotionClip,ESPMode::ThreadSafe> Clip,TArray<TArray<FOGReferenceBoneConnection>>&& Mapping)
{
    if (!Clip || Mapping.Num()!=RegisteredParts.Num()) return false;
    // Every native part is checked before interrupting the previous action.
    StopAction();
    ReferenceClip=MoveTemp(Clip); ReferenceMappings=MoveTemp(Mapping);
    for (USkeletalMeshComponent* Part:RegisteredParts)
    {
        FReferencePartRestore Saved; Saved.Part=Part; Saved.Mesh=Part->GetSkeletalMeshAsset();
        Saved.Mode=static_cast<uint8>(Part->GetAnimationMode());
        Saved.PreviousClass=TStrongObjectPtr<UClass>(Part->GetAnimClass());
        Saved.Leader=Part->GetLeaderPoseComponent();
        if (auto* Node=Part->GetSingleNodeInstance())
        {
            Saved.PreviousAsset=TStrongObjectPtr<UAnimationAsset>(Node->GetCurrentAsset());
            Saved.Rate=Node->GetPlayRate(); Saved.Position=Node->GetCurrentTime();
            Saved.bLoop=Node->IsLooping(); Saved.bWasPlaying=Node->IsPlaying();
        }
        ReferenceRestores.Add(MoveTemp(Saved));
        Part->SetLeaderPoseComponent(nullptr);
        Part->SetAnimInstanceClass(UOGSharedReferenceAnimInstance::StaticClass());
        auto* Instance=Cast<UOGSharedReferenceAnimInstance>(Part->GetAnimInstance());
        ReferenceRestores.Last().Instance=Instance;
        if (!Instance) { StopAction(); return false; }
    }
    bPlaying=true; bLooping=Entry.bLoop; PlaybackRate=Entry.PlayRate;
    SequenceSeconds=FMath::Max(ReferenceClip->Duration,1.f/ReferenceClip->Rate);
    CurrentAction=Entry.GameplayAction; CurrentMotionId=Entry.MotionId; CurrentSourceClipId=ReferenceClip->Id;
    AuthoredMorphTracks=Entry.MorphTracks;
    bHasContact=Entry.ContactSeconds>=0; SourceContactSeconds=Entry.ContactSeconds;
    ElapsedSeconds=0; LastContactCycle=-1; bSceneTimeLocked=false;
    UpdateReferencePose(); SetComponentTickEnabled(true);
    return true;
}
void UOGSharedMotionControllerComponent::UpdateReferencePose()
{
    if (!ReferenceClip) return;
    const double Raw=static_cast<double>(ElapsedSeconds)*PlaybackRate;
    const double Time=bLooping && SequenceSeconds>0 ? FMath::Fmod(Raw,static_cast<double>(SequenceSeconds)) : Raw;
    for (int32 I=0; I<ReferenceRestores.Num(); ++I)
    {
        auto* Part=ReferenceRestores[I].Part.Get();
        auto* Instance=ReferenceRestores[I].Instance.Get();
        if (!Part || !Instance || Part->GetAnimInstance()!=Instance ||
            ReferenceRigs[I].Mesh.Get()!=Part->GetSkeletalMeshAsset())
        { StopAction(); return; }
        TArray<FTransform> Pose;
        if (!ReferenceRigs[I].Evaluate(*ReferenceClip,ReferenceMappings[I],Time,Pose))
        { StopAction(); return; }
        Instance->SetSharedPose(Pose);
    }
}
void UOGSharedMotionControllerComponent::RestoreReferencePose()
{
    for (auto& Saved:ReferenceRestores)
    {
        auto* Part=Saved.Part.Get();
        if (!Part || (Saved.Instance.IsValid() ? Part->GetAnimInstance()!=Saved.Instance.Get() :
            Part->GetAnimClass()!=UOGSharedReferenceAnimInstance::StaticClass())) continue;
        if (Part->GetSkeletalMeshAsset()!=Saved.Mesh.Get())
        {
            // A real form/wardrobe replacement owns the new rig; never replay an old-rig asset on it.
            Part->SetAnimationMode(EAnimationMode::AnimationSingleNode);
            Part->Stop();
            continue;
        }
        if (static_cast<EAnimationMode::Type>(Saved.Mode)==EAnimationMode::AnimationSingleNode)
        {
            Part->SetAnimationMode(EAnimationMode::AnimationSingleNode);
            if (Saved.PreviousAsset.IsValid())
            {
                Part->PlayAnimation(Saved.PreviousAsset.Get(),Saved.bLoop);
                Part->SetPosition(Saved.Position,false); Part->SetPlayRate(Saved.Rate);
                if (!Saved.bWasPlaying) Part->Stop();
            }
        }
        else
        {
            Part->SetAnimInstanceClass(Saved.PreviousClass.Get());
            Part->SetAnimationMode(static_cast<EAnimationMode::Type>(Saved.Mode));
        }
        Part->SetLeaderPoseComponent(Saved.Leader.Get());
    }
    ReferenceRestores.Reset(); ReferenceMappings.Reset(); ReferenceClip.Reset();
}

bool UOGSharedMotionControllerComponent::EnqueueCombatAction(FName Action,
    const FOGEntityId& Battle,const FOGEntityId& Receipt)
{
    if (!IsInGameThread() || !Battle.IsValid() || !Receipt.IsValid() || Action.IsNone()) return false;
    if (CombatScope==Battle && CombatReceipts.Contains(Receipt)) return true;
    if (bSceneTimeLocked && !bPlayingQueuedCombat) return false;
    if (!CanPlayAction(Action,TEXT("Combat"),false,1,NAME_None)) return false;
    // A coordinated scene owns its current playback until it explicitly exits.
    if (bPlaying && !bPlayingQueuedCombat && !CurrentAction.ToString().StartsWith(TEXT("Traversal.")) &&
        !CurrentAction.ToString().StartsWith(TEXT("Combat."))) return false;
    if (CombatScope!=Battle)
    {
        const bool StopOld=bPlayingQueuedCombat;
        CombatQueue.Reset(); CombatReceipts.Reset(); CombatScope=Battle;
        if (StopOld) { TGuardValue<bool> Preserve(bPreserveCombatQueue,true); StopAction(); }
    }
    CombatReceipts.Add(Receipt);
    FQueuedCombatMotion Queued; Queued.Action=Action; Queued.Receipt=Receipt; CombatQueue.Add(Queued);
    if (!bPlaying || bLooping || CurrentAction.ToString().StartsWith(TEXT("Traversal.")))
        return StartNextCombatMotion();
    SetComponentTickEnabled(true); return true;
}
bool UOGSharedMotionControllerComponent::StartNextCombatMotion()
{
    while (!CombatQueue.IsEmpty())
    {
        const auto Next=CombatQueue[0]; CombatQueue.RemoveAt(0);
        TGuardValue<bool> Preserve(bPreserveCombatQueue,true);
        if (PlayAction(Next.Action,TEXT("Combat"),false,1,NAME_None))
        { bPlayingQueuedCombat=true; ActiveCombatReceipt=Next.Receipt; bLooping=false; return true; }
        UE_LOG(LogTemp,Warning,TEXT("Accepted combat motion %s became unavailable for the current rig."),*Next.Action.ToString());
    }
    return false;
}
void UOGSharedMotionControllerComponent::CancelCombatActions(const FOGEntityId& Battle)
{
    if (CombatScope!=Battle) return;
    const bool StopCurrent=bPlayingQueuedCombat;
    CombatQueue.Reset(); CombatReceipts.Reset(); CombatScope=FOGEntityId();
    if (StopCurrent) StopAction();
}
bool UOGSharedMotionControllerComponent::EnqueueCombatCue(FName Cue,const FOGEntityId& Battle,
    const FOGEntityId& Action,const FOGEntityId& Subject)
{
    if (!Action.IsValid() || !Subject.IsValid()) return false;
    const FString Text=TEXT("presentation|")+Action.ToString()+TEXT("|")+Subject.ToString()+TEXT("|")+Cue.ToString();
    FGuid Id; FGuid::ParseExact(FMD5::HashAnsiString(*Text),EGuidFormats::Digits,Id);
    return EnqueueCombatAction(Cue,Battle,FOGEntityId(Id));
}

bool UOGSharedMotionControllerComponent::StartImmediateCombatAction(FName Action,
    const FOGEntityId& Battle,const FOGEntityId& Receipt)
{
    if (!IsInGameThread() || !Battle.IsValid() || !Receipt.IsValid() || Action.IsNone()) return false;
    if (CombatScope==Battle && CombatReceipts.Contains(Receipt) &&
        (ActiveCombatReceipt==Receipt || !CombatQueue.ContainsByPredicate([&](const auto& Entry){return Entry.Receipt==Receipt;}))) return true;
    if ((bSceneTimeLocked && !bPlayingQueuedCombat) || !CanPlayAction(Action,TEXT("Combat"),false,1,NAME_None)) return false;
    if (bPlaying && !bPlayingQueuedCombat && !CurrentAction.ToString().StartsWith(TEXT("Traversal.")) &&
        !CurrentAction.ToString().StartsWith(TEXT("Combat."))) return false;
    if (CombatScope!=Battle) CombatReceipts.Reset();
    CombatQueue.Reset(); CombatScope=Battle;
    TGuardValue<bool> Preserve(bPreserveCombatQueue,true);
    if (!PlayAction(Action,TEXT("Combat"),false,1,NAME_None)) return false;
    CombatReceipts.Add(Receipt); ActiveCombatReceipt=Receipt; bPlayingQueuedCombat=true; return true;
}
void UOGSharedMotionControllerComponent::CancelCombatAction(const FOGEntityId& Battle,const FOGEntityId& Receipt)
{
    if (CombatScope!=Battle || !Receipt.IsValid()) return;
    CombatQueue.RemoveAll([&](const auto& Entry){return Entry.Receipt==Receipt;});
    if (bPlayingQueuedCombat && ActiveCombatReceipt==Receipt)
    { TGuardValue<bool> Preserve(bPreserveCombatQueue,true); StopAction(); StartNextCombatMotion(); }
}

bool UOGSharedMotionControllerComponent::IsCombatMotionPending(const FOGEntityId& Receipt) const
{ return Receipt.IsValid() && ((bPlayingQueuedCombat && ActiveCombatReceipt==Receipt) ||
    CombatQueue.ContainsByPredicate([&](const auto& Entry){return Entry.Receipt==Receipt;})); }

FName UOGSharedMotionControllerComponent::ResolveSourceMorph(FName Source) const
{ const auto* Native=SharedSourceMorphAliases.Find(Source); return Native?*Native:Source; }
bool UOGSharedMotionControllerComponent::SetSourceMorphLayer(FName Layer,FName Source,float Weight)
{ return SetMorphLayer(Layer,ResolveSourceMorph(Source),Weight); }

void UOGSharedMotionControllerComponent::CompleteCombatAction(const FOGEntityId& Battle,const FOGEntityId& Receipt)
{
    if (CombatScope!=Battle || !Receipt.IsValid()) return;
    CombatQueue.RemoveAll([&](const auto& Entry){return Entry.Receipt==Receipt;});
    if (!bPlayingQueuedCombat || ActiveCombatReceipt!=Receipt) return;
    const FName Action=CurrentAction,Motion=CurrentMotionId;
    { TGuardValue<bool> Preserve(bPreserveCombatQueue,true); StopAction(); }
    const uint64 Generation=PlaybackGeneration;
    { TGuardValue<FOGEntityId> Finished(FinishingCombatReceipt,Receipt);
      OnNativeMotionFinished.Broadcast(this,Action,Motion); OnMotionFinished.Broadcast(Action,Motion); }
    if (!bPlaying && PlaybackGeneration==Generation) StartNextCombatMotion();
}

void UOGSharedMotionControllerComponent::RestoreMorphBaselines()
{
    for (const auto& Saved:MorphBaselines)
        if (auto* Part=Saved.Part.Get(); Part && Part->GetSkeletalMeshAsset()==Saved.Mesh.Get())
            Part->SetMorphTarget(Saved.Morph,Saved.Weight);
    MorphBaselines.Reset(); AppliedMorphs.Reset();
}
bool UOGSharedMotionControllerComponent::HasSourceMorph(FName Source) const
{
    const FName Native=ResolveSourceMorph(Source);
    if (Source.IsNone() || Native.IsNone()) return false;
    for (const USkeletalMeshComponent* Part:RegisteredParts)
        if (Part && Part->GetSkeletalMeshAsset() && Part->GetSkeletalMeshAsset()->FindMorphTarget(Native)) return true;
    return false;
}
