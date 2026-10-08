#include "Runtime/OGSemanticAudioRuntime.h"

#include "Components/AudioComponent.h"
#include "Components/ForceFeedbackComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "GameFramework/ForceFeedbackEffect.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"

namespace
{
    constexpr int32 MaxOwnedEvents = 32;
    float SafeUnit(float Value)
    {
        return FMath::IsFinite(Value) ? FMath::Clamp(Value, 0.0f, 1.0f) : 0.0f;
    }
    bool SameCaption(const FOGSemanticCaptionProjection& A, const FOGSemanticCaptionProjection& B)
    {
        return A.bVisible == B.bVisible && A.EventName == B.EventName
            && A.Text.EqualTo(B.Text) && A.Speaker.EqualTo(B.Speaker)
            && A.bClosedCaption == B.bClosedCaption && A.Presentation == B.Presentation
            && A.Readability == B.Readability;
    }
}

UOGSemanticAudioRuntime::UOGSemanticAudioRuntime()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = false;
    PrimaryComponentTick.TickInterval = 0.05f;
}

bool UOGSemanticAudioRuntime::IsLocalPresentation() const
{
    if (!GetWorld() || GetWorld()->GetNetMode() == NM_DedicatedServer) return false;
    const APawn* Pawn = Cast<APawn>(GetOwner());
    return !Pawn || Pawn->IsLocallyControlled();
}

void UOGSemanticAudioRuntime::ConfigureRoutes(
    USoundClass* Music, USoundClass* Voice, USoundClass* Sfx, USoundClass* Ambience,
    bool bRoutesIncludeProfileGain, bool bMasterHandledExternally)
{
    // A class override is captured when Play starts. Stop only our instances
    // before replacing the route; never modify an authored sound's class.
    if (MusicRoute != Music || VoiceRoute != Voice || SfxRoute != Sfx || AmbienceRoute != Ambience)
        CancelOwnedPresentation();
    MusicRoute = Music;
    VoiceRoute = Voice;
    SfxRoute = Sfx;
    AmbienceRoute = Ambience;
    bExternalRouteGain = bRoutesIncludeProfileGain;
    bExternalMasterGain = bMasterHandledExternally;
    UpdateActiveGains();
}

void UOGSemanticAudioRuntime::ApplyProfile(const FOGPlayerProfileSettings& Profile)
{
    AppliedProfile = Profile; // a projection of the canonical profile service, no second store
    ApplyDynamicRangeMix();
    UpdateActiveGains();
    UpdateCaption();
}

void UOGSemanticAudioRuntime::ConfigureDynamicRangeMixes(USoundMix* Full, USoundMix* Night, USoundMix* Compressed)
{
    ReleaseDynamicRangeMixOwnership();
    FullMix = Full;
    NightMix = Night;
    CompressedMix = Compressed;
    bOwnDynamicRangeMix = true;
    ApplyDynamicRangeMix();
    UpdateActiveGains();
}

void UOGSemanticAudioRuntime::ReleaseDynamicRangeMixOwnership()
{
    if (OwnedActiveMix && GetWorld())
        UGameplayStatics::PopSoundMixModifier(this, OwnedActiveMix);
    OwnedActiveMix = nullptr;
    bOwnDynamicRangeMix = false;
    FullMix = nullptr;
    NightMix = nullptr;
    CompressedMix = nullptr;
    UpdateActiveGains();
}

void UOGSemanticAudioRuntime::ApplyDynamicRangeMix()
{
    if (!bOwnDynamicRangeMix || !GetWorld()) return;
    USoundMix* Desired = AppliedProfile.DynamicRangeProfile == FName(TEXT("night")) ? NightMix.Get()
        : AppliedProfile.DynamicRangeProfile == FName(TEXT("compressed")) ? CompressedMix.Get() : FullMix.Get();
    if (Desired == OwnedActiveMix) return;
    if (OwnedActiveMix) UGameplayStatics::PopSoundMixModifier(this, OwnedActiveMix);
    OwnedActiveMix = Desired;
    if (Desired) UGameplayStatics::PushSoundMixModifier(this, Desired);
}

USoundClass* UOGSemanticAudioRuntime::GetRoute(EOGSemanticAudioCategory Category) const
{
    switch (Category)
    {
    case EOGSemanticAudioCategory::Music: return MusicRoute;
    case EOGSemanticAudioCategory::Voice: return VoiceRoute;
    case EOGSemanticAudioCategory::Ambience: return AmbienceRoute;
    default: return SfxRoute;
    }
}

float UOGSemanticAudioRuntime::GetDynamicFallbackGain(EOGSemanticAudioCategory Category) const
{
    // This is a category-balance fallback, not fabricated compressor processing.
    if (OwnedActiveMix || Category == EOGSemanticAudioCategory::Voice) return 1.0f;
    if (AppliedProfile.DynamicRangeProfile == FName(TEXT("night")))
    {
        if (Category == EOGSemanticAudioCategory::Music) return 0.86f;
        if (Category == EOGSemanticAudioCategory::Ambience) return 0.76f;
        return 0.68f;
    }
    if (AppliedProfile.DynamicRangeProfile == FName(TEXT("compressed")))
    {
        if (Category == EOGSemanticAudioCategory::Music) return 0.92f;
        if (Category == EOGSemanticAudioCategory::Ambience) return 0.88f;
        return 0.82f;
    }
    return 1.0f;
}

float UOGSemanticAudioRuntime::GetEventGain(const FOGSemanticAudioEventDefinition& Definition) const
{
    float Gain = SafeUnit(Definition.Gain);
    if (!bExternalMasterGain) Gain *= SafeUnit(AppliedProfile.MasterVolume);
    if (!bExternalRouteGain || !GetRoute(Definition.Category))
    {
        float CategoryGain = AppliedProfile.SfxVolume;
        switch (Definition.Category)
        {
        case EOGSemanticAudioCategory::Music: CategoryGain = AppliedProfile.MusicVolume; break;
        case EOGSemanticAudioCategory::Voice: CategoryGain = AppliedProfile.VoiceVolume; break;
        case EOGSemanticAudioCategory::Ambience: CategoryGain = AppliedProfile.AmbienceVolume; break;
        default: break;
        }
        Gain *= SafeUnit(CategoryGain) * GetDynamicFallbackGain(Definition.Category);
    }
    return Gain;
}

void UOGSemanticAudioRuntime::RegisterEventDefinition(FName EventName, const FOGSemanticAudioEventDefinition& Definition)
{
    if (EventName.IsNone()) return;
    StopSemanticEvent(EventName);
    FOGSemanticAudioEventDefinition Clean = Definition;
    Clean.Gain = SafeUnit(Clean.Gain);
    Clean.HapticGain = SafeUnit(Clean.HapticGain);
    Clean.CooldownSeconds = FMath::IsFinite(Clean.CooldownSeconds) ? FMath::Max(0.0f, Clean.CooldownSeconds) : 0.0f;
    Clean.Captions.RemoveAll([](const FOGSemanticCaptionCue& Cue)
    {
        return Cue.Text.IsEmpty() || !FMath::IsFinite(Cue.StartSeconds)
            || !FMath::IsFinite(Cue.DurationSeconds) || Cue.StartSeconds < 0.0f || Cue.DurationSeconds <= 0.0f;
    });
    Clean.Captions.Sort([](const FOGSemanticCaptionCue& A, const FOGSemanticCaptionCue& B)
    {
        return A.StartSeconds < B.StartSeconds;
    });
    Definitions.Add(EventName, MoveTemp(Clean));
    LastPlayedAt.Remove(EventName);
}

void UOGSemanticAudioRuntime::UnregisterEventDefinition(FName EventName)
{
    StopSemanticEvent(EventName);
    Definitions.Remove(EventName);
    LastPlayedAt.Remove(EventName);
}

bool UOGSemanticAudioRuntime::HasAuthoredSound(FName EventName) const
{
    const auto* Def = Definitions.Find(EventName);
    return Def && (Def->Sound || !Def->SoftSound.IsNull());
}

bool UOGSemanticAudioRuntime::HasAuthoredCaptions(FName EventName) const
{
    const auto* Def = Definitions.Find(EventName);
    return Def && !Def->Captions.IsEmpty();
}

FOGSemanticAudioPlaybackProjection UOGSemanticAudioRuntime::GetPlaybackProjection(FName EventName) const
{
    FOGSemanticAudioPlaybackProjection Result;
    if (const auto* Definition = Definitions.Find(EventName))
    {
        Result.bRegistered = true;
        Result.Category = Definition->Category;
        Result.Route = GetRoute(Definition->Category);
        Result.EventGain = GetEventGain(*Definition);
    }
    return Result;
}

FOGSemanticAudioOwnershipProjection UOGSemanticAudioRuntime::GetOwnershipProjection() const
{
    FOGSemanticAudioOwnershipProjection Result;
    Result.ActiveEvents = ActiveEvents.Num();
    Result.PendingEvents = PendingEvents.Num();
    Result.AudioComponents = OwnedAudio.Num();
    Result.FeedbackComponents = OwnedFeedback.Num();
    Result.bOwnsDynamicRange = bOwnDynamicRangeMix;
    Result.ActiveMix = OwnedActiveMix;
    return Result;
}

bool UOGSemanticAudioRuntime::PlaySemanticEvent(FName EventName, FVector Location)
{
    const auto* Def = Definitions.Find(EventName);
    if (!Def || !IsLocalPresentation() || (Def->bWorldOnly && !bWorldActive) || Location.ContainsNaN()) return false;
    const double Now = GetWorld()->GetTimeSeconds();
    if (const double* Last = LastPlayedAt.Find(EventName))
        if (Now - *Last < Def->CooldownSeconds) return false;
    if (PendingEvents.ContainsByPredicate([EventName](const FPendingEvent& P) { return P.Name == EventName; }))
        return false;

    TArray<FSoftObjectPath> Paths;
    if (!Def->Sound && !Def->SoftSound.IsNull() && !Def->SoftSound.IsValid()) Paths.AddUnique(Def->SoftSound.ToSoftObjectPath());
    if (!Def->Attenuation && !Def->SoftAttenuation.IsNull() && !Def->SoftAttenuation.IsValid()) Paths.AddUnique(Def->SoftAttenuation.ToSoftObjectPath());
    if (AppliedProfile.bHapticsEnabled && !Def->Haptic && !Def->SoftHaptic.IsNull() && !Def->SoftHaptic.IsValid())
        Paths.AddUnique(Def->SoftHaptic.ToSoftObjectPath());
    if (!Paths.IsEmpty())
    {
        if (PendingEvents.Num() >= MaxOwnedEvents) return false;
        const uint64 Token = NextSequence++;
        FPendingEvent Pending;
        Pending.Name = EventName;
        Pending.Location = Location;
        Pending.Token = Token;
        // Start stalled so the ownership record exists even for an immediate completion.
        Pending.Handle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
            Paths, FStreamableDelegate::CreateUObject(this, &UOGSemanticAudioRuntime::FinishPendingEvent, Token),
            FStreamableManager::DefaultAsyncLoadPriority, false, true);
        if (!Pending.Handle) return false;
        PendingEvents.Add(MoveTemp(Pending));
        PendingEvents.Last().Handle->StartStalledHandle();
        return true;
    }
    return PlayResolvedEvent(EventName, *Def, Location);
}

void UOGSemanticAudioRuntime::FinishPendingEvent(uint64 Token)
{
    const int32 Index = PendingEvents.IndexOfByPredicate([Token](const FPendingEvent& P) { return P.Token == Token; });
    if (Index == INDEX_NONE) return;
    FPendingEvent Pending = MoveTemp(PendingEvents[Index]);
    PendingEvents.RemoveAt(Index);
    auto* Def = Definitions.Find(Pending.Name);
    if (!Def || !IsLocalPresentation() || (Def->bWorldOnly && !bWorldActive)) return;
    // Hold resolved assets through reflected definition references after releasing the handle.
    if (!Def->Sound) Def->Sound = Def->SoftSound.Get();
    if (!Def->Attenuation) Def->Attenuation = Def->SoftAttenuation.Get();
    if (!Def->Haptic) Def->Haptic = Def->SoftHaptic.Get();
    PlayResolvedEvent(Pending.Name, *Def, Pending.Location);
}

bool UOGSemanticAudioRuntime::PlayResolvedEvent(FName EventName,
    const FOGSemanticAudioEventDefinition& Definition, const FVector& Location)
{
    if (ActiveEvents.Num() >= MaxOwnedEvents || !GetWorld()) return false;
    USoundBase* Sound = Definition.Sound ? Definition.Sound.Get() : Definition.SoftSound.Get();
    USoundAttenuation* Attenuation = Definition.Attenuation ? Definition.Attenuation.Get() : Definition.SoftAttenuation.Get();
    UForceFeedbackEffect* Haptic = Definition.Haptic ? Definition.Haptic.Get() : Definition.SoftHaptic.Get();
    FActiveEvent Event;
    Event.Name = EventName;
    Event.Definition = Definition;
    Event.StartedAt = GetWorld()->GetTimeSeconds();
    Event.Sequence = NextSequence++;

    // A spatial event must have authored attenuation to avoid an unbounded loud source.
    if (Sound && (!Definition.bSpatial || Attenuation))
    {
        UAudioComponent* Audio = NewObject<UAudioComponent>(GetOwner() ? GetOwner() : static_cast<UObject*>(this));
        Audio->bAutoActivate = false;
        Audio->bAutoDestroy = false;
        Audio->bStopWhenOwnerDestroyed = true;
        Audio->bIsUISound = Definition.bUiEvent;
        Audio->bAllowSpatialization = Definition.bSpatial;
        Audio->bSuppressSubtitles = !Definition.Captions.IsEmpty();
        // Verified UE 5.8 API: set the per-component override before registration/play.
        Audio->SoundClassOverride = GetRoute(Definition.Category);
        Audio->SetSound(Sound);
        if (Definition.bSpatial) Audio->SetAttenuationSettings(Attenuation);
        else
        {
            FSoundAttenuationSettings TwoDimensional;
            TwoDimensional.bAttenuate = false;
            TwoDimensional.bSpatialize = false;
            TwoDimensional.bAttenuateWithLPF = false;
            Audio->SetAttenuationOverrides(TwoDimensional);
        }
        Audio->SetVolumeMultiplier(GetEventGain(Definition));
        Audio->RegisterComponentWithWorld(GetWorld());
        Audio->SetWorldLocation(Location);
        for (const auto& Pair : EnvironmentParameters) Audio->SetFloatParameter(Pair.Key, Pair.Value);
        OwnedAudio.Add(Audio);
        Event.Audio = Audio;
        Audio->Play();
        Event.bSoundStarted = Audio->IsPlaying();
        if (!Event.bSoundStarted)
        {
            Audio->DestroyComponent();
            OwnedAudio.Remove(Audio);
            Event.Audio.Reset();
        }
    }

    const APawn* Pawn = Cast<APawn>(GetOwner());
    if (Haptic && Pawn && Pawn->IsLocallyControlled() && AppliedProfile.bHapticsEnabled
        && SafeUnit(AppliedProfile.HapticsIntensity) > 0.0f && Definition.HapticGain > 0.0f)
    {
        UForceFeedbackComponent* Feedback = NewObject<UForceFeedbackComponent>(GetOwner());
        Feedback->bAutoActivate = false;
        Feedback->bAutoDestroy = false;
        Feedback->bStopWhenOwnerDestroyed = true;
        Feedback->SetForceFeedbackEffect(Haptic);
        Feedback->SetIntensityMultiplier(SafeUnit(AppliedProfile.HapticsIntensity) * Definition.HapticGain);
        if (GetOwner()->GetRootComponent()) Feedback->SetupAttachment(GetOwner()->GetRootComponent());
        Feedback->RegisterComponentWithWorld(GetWorld());
        OwnedFeedback.Add(Feedback);
        Event.Feedback = Feedback;
        Feedback->Play();
    }

    const bool bCaption = !Definition.Captions.IsEmpty() && (Event.bSoundStarted || Definition.bAllowCaptionWithoutSound);
    if (!Event.bSoundStarted && !Event.Feedback.IsValid() && !bCaption) return false;
    LastPlayedAt.Add(EventName, Event.StartedAt);
    const TWeakObjectPtr<UAudioComponent> PlayedAudio = Event.Audio;
    ActiveEvents.Add(MoveTemp(Event));
    SetComponentTickEnabled(true);
    UpdateCaption();
    OnSemanticEventPlayed.Broadcast(EventName, PlayedAudio.Get());
    return true;
}

void UOGSemanticAudioRuntime::StopActiveEvent(FActiveEvent& Event)
{
    if (UAudioComponent* Audio = Event.Audio.Get())
    {
        Audio->Stop();
        Audio->DestroyComponent();
        OwnedAudio.Remove(Audio);
    }
    if (UForceFeedbackComponent* Feedback = Event.Feedback.Get())
    {
        Feedback->Stop();
        Feedback->DestroyComponent();
        OwnedFeedback.Remove(Feedback);
    }
    Event.Audio.Reset();
    Event.Feedback.Reset();
}

void UOGSemanticAudioRuntime::StopSemanticEvent(FName EventName)
{
    // Remove before cancellation: callbacks cannot resurrect cancelled presentation.
    for (int32 I = PendingEvents.Num() - 1; I >= 0; --I)
    {
        if (PendingEvents[I].Name != EventName) continue;
        auto Handle = PendingEvents[I].Handle;
        PendingEvents.RemoveAt(I);
        if (Handle) Handle->CancelHandle();
    }
    for (int32 I = ActiveEvents.Num() - 1; I >= 0; --I)
    {
        if (ActiveEvents[I].Name != EventName) continue;
        StopActiveEvent(ActiveEvents[I]);
        ActiveEvents.RemoveAt(I);
    }
    UpdateCaption();
    SetComponentTickEnabled(!ActiveEvents.IsEmpty());
}

void UOGSemanticAudioRuntime::CancelOwnedPresentation()
{
    TArray<FPendingEvent> Cancelled = MoveTemp(PendingEvents);
    PendingEvents.Reset();
    for (auto& Pending : Cancelled) if (Pending.Handle) Pending.Handle->CancelHandle();
    for (auto& Event : ActiveEvents) StopActiveEvent(Event);
    ActiveEvents.Reset();
    OwnedAudio.Reset();
    OwnedFeedback.Reset();
    LastPlayedAt.Reset();
    UpdateCaption();
    SetComponentTickEnabled(false);
}

void UOGSemanticAudioRuntime::SetWorldPresentationActive(bool bActive)
{
    bWorldActive = bActive;
    if (bActive) return;
    TArray<FName> WorldNames;
    for (const auto& Pair : Definitions) if (Pair.Value.bWorldOnly) WorldNames.Add(Pair.Key);
    for (FName Name : WorldNames) StopSemanticEvent(Name);
}

void UOGSemanticAudioRuntime::SetEnvironmentState(FName StateName, float Value)
{
    if (StateName.IsNone() || !FMath::IsFinite(Value)) return;
    EnvironmentParameters.Add(StateName, Value);
    for (const auto& Event : ActiveEvents)
        if (UAudioComponent* Audio = Event.Audio.Get()) Audio->SetFloatParameter(StateName, Value);
}

void UOGSemanticAudioRuntime::UpdateActiveGains()
{
    for (auto& Event : ActiveEvents)
    {
        if (UAudioComponent* Audio = Event.Audio.Get()) Audio->SetVolumeMultiplier(GetEventGain(Event.Definition));
        if (UForceFeedbackComponent* Feedback = Event.Feedback.Get())
        {
            if (!AppliedProfile.bHapticsEnabled || SafeUnit(AppliedProfile.HapticsIntensity) <= 0.0f)
            {
                Feedback->Stop();
                Feedback->DestroyComponent();
                OwnedFeedback.Remove(Feedback);
                Event.Feedback.Reset();
            }
            else Feedback->SetIntensityMultiplier(SafeUnit(AppliedProfile.HapticsIntensity) * Event.Definition.HapticGain);
        }
    }
}

void UOGSemanticAudioRuntime::UpdateCaption()
{
    FOGSemanticCaptionProjection Next;
    Next.Presentation = AppliedProfile.SubtitlePresentation;
    Next.Readability = AppliedProfile.UiReadabilityProfile;
    int32 BestPriority = MIN_int32;
    uint64 BestSequence = 0;
    const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
    if (AppliedProfile.bSubtitlesEnabled)
    {
        for (const auto& Event : ActiveEvents)
        {
            if (!Event.bSoundStarted && !Event.Definition.bAllowCaptionWithoutSound) continue;
            const double Age = Now - Event.StartedAt;
            for (const auto& Cue : Event.Definition.Captions)
            {
                if (Age < Cue.StartSeconds || Age >= Cue.StartSeconds + Cue.DurationSeconds) continue;
                if (Event.Definition.CaptionPriority < BestPriority
                    || (Event.Definition.CaptionPriority == BestPriority && Event.Sequence < BestSequence)) continue;
                BestPriority = Event.Definition.CaptionPriority;
                BestSequence = Event.Sequence;
                Next.bVisible = true;
                Next.EventName = Event.Name;
                Next.Text = Cue.Text;
                Next.Speaker = Cue.Speaker;
                Next.bClosedCaption = Cue.bClosedCaption;
                Next.RemainingSeconds = static_cast<float>(Cue.StartSeconds + Cue.DurationSeconds - Age);
            }
        }
    }
    const bool bChanged = !SameCaption(CurrentCaption, Next);
    CurrentCaption = MoveTemp(Next);
    if (bChanged)
    {
        const FOGSemanticCaptionProjection BroadcastCaption = CurrentCaption;
        OnCaptionChanged.Broadcast(BroadcastCaption);
    }
}

void UOGSemanticAudioRuntime::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    if (!IsLocalPresentation()) { CancelOwnedPresentation(); return; }
    const double Now = GetWorld()->GetTimeSeconds();
    for (int32 I = ActiveEvents.Num() - 1; I >= 0; --I)
    {
        auto& Event = ActiveEvents[I];
        bool bPendingCaption = false;
        if (Event.bSoundStarted || Event.Definition.bAllowCaptionWithoutSound)
            for (const auto& Cue : Event.Definition.Captions)
                if (Now - Event.StartedAt < Cue.StartSeconds + Cue.DurationSeconds) { bPendingCaption = true; break; }
        const UAudioComponent* Audio = Event.Audio.Get();
        const UForceFeedbackComponent* Feedback = Event.Feedback.Get();
        if ((!Audio || !Audio->IsPlaying()) && (!Feedback || !Feedback->IsActive()) && !bPendingCaption)
        {
            StopActiveEvent(Event);
            ActiveEvents.RemoveAt(I);
        }
    }
    UpdateCaption();
    SetComponentTickEnabled(!ActiveEvents.IsEmpty());
}

void UOGSemanticAudioRuntime::BeginPlay()
{
    Super::BeginPlay();
    if (UGameInstance* Instance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
        if (auto* Mode = Instance->GetSubsystem<UOGPresentationModeSubsystem>())
        {
            Mode->OnPresentationModeChanged.AddDynamic(this, &UOGSemanticAudioRuntime::HandlePresentationModeChanged);
            HandlePresentationModeChanged(Mode->GetMode());
        }
}

void UOGSemanticAudioRuntime::HandlePresentationModeChanged(EOGPresentationMode Mode)
{ SetWorldPresentationActive(Mode == EOGPresentationMode::World); }

void UOGSemanticAudioRuntime::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (UGameInstance* Instance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
        if (auto* Mode = Instance->GetSubsystem<UOGPresentationModeSubsystem>())
            Mode->OnPresentationModeChanged.RemoveDynamic(this, &UOGSemanticAudioRuntime::HandlePresentationModeChanged);
    CancelOwnedPresentation();
    ReleaseDynamicRangeMixOwnership();
    OnCaptionChanged.Clear();
    OnSemanticEventPlayed.Clear();
    Super::EndPlay(EndPlayReason);
}
