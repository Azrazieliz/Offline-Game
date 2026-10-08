#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Runtime/OGPlayerProfileSettings.h"
#include "Runtime/OGPresentationModeSubsystem.h"
#include "OGSemanticAudioRuntime.generated.h"

class UAudioComponent;
class UForceFeedbackComponent;
class UForceFeedbackEffect;
class USoundAttenuation;
class USoundBase;
class USoundClass;
class USoundMix;
struct FStreamableHandle;

UENUM(BlueprintType)
enum class EOGSemanticAudioCategory : uint8 { Music, Voice, Sfx, Ambience };

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGSemanticCaptionCue
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FText Text;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FText Speaker;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0")) float StartSeconds = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0.1")) float DurationSeconds = 3.0f;
    /** An authored non-dialogue description, never a generated voice transcript. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bClosedCaption = false;
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGSemanticAudioEventDefinition
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EOGSemanticAudioCategory Category = EOGSemanticAudioCategory::Sfx;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<USoundBase> Sound = nullptr;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TSoftObjectPtr<USoundBase> SoftSound;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<USoundAttenuation> Attenuation = nullptr;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TSoftObjectPtr<USoundAttenuation> SoftAttenuation;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<UForceFeedbackEffect> Haptic = nullptr;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TSoftObjectPtr<UForceFeedbackEffect> SoftHaptic;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FOGSemanticCaptionCue> Captions;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bSpatial = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bUiEvent = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bWorldOnly = true;
    /** Explicit opt-in for an authored text-only accessibility cue. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bAllowCaptionWithoutSound = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", ClampMax="1")) float Gain = 1.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", ClampMax="1")) float HapticGain = 1.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0")) float CooldownSeconds = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 CaptionPriority = 0;
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGSemanticCaptionProjection
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) bool bVisible = false;
    UPROPERTY(BlueprintReadOnly) FName EventName;
    UPROPERTY(BlueprintReadOnly) FText Text;
    UPROPERTY(BlueprintReadOnly) FText Speaker;
    UPROPERTY(BlueprintReadOnly) bool bClosedCaption = false;
    UPROPERTY(BlueprintReadOnly) float RemainingSeconds = 0.0f;
    UPROPERTY(BlueprintReadOnly) FName Presentation = FName(TEXT("standard"));
    UPROPERTY(BlueprintReadOnly) FName Readability = FName(TEXT("standard"));
};

/** Read-only snapshot of the same routing/gain decisions used for playback. */
struct OFFLINEGAME_API FOGSemanticAudioPlaybackProjection
{
    bool bRegistered = false;
    EOGSemanticAudioCategory Category = EOGSemanticAudioCategory::Sfx;
    const USoundClass* Route = nullptr;
    float EventGain = 0.0f;
};

/** Ownership diagnostics expose no mutable event/component references. */
struct OFFLINEGAME_API FOGSemanticAudioOwnershipProjection
{
    int32 ActiveEvents = 0;
    int32 PendingEvents = 0;
    int32 AudioComponents = 0;
    int32 FeedbackComponents = 0;
    bool bOwnsDynamicRange = false;
    const USoundMix* ActiveMix = nullptr;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOGSemanticCaptionChanged, const FOGSemanticCaptionProjection&, Caption);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOGSemanticEventPlayedNative, FName, UAudioComponent*);

/**
 * Presentation-only authored audio/caption/rumble dispatcher. Contains no content,
 * canonical mutations or persistence. The existing player profile remains the
 * only settings source. Its registered definitions must be supplied by content.
 */
UCLASS(ClassGroup=(OfflineGame), meta=(BlueprintSpawnableComponent))
class OFFLINEGAME_API UOGSemanticAudioRuntime : public UActorComponent
{
    GENERATED_BODY()
public:
    UOGSemanticAudioRuntime();
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    /** Routes already controlled by the pawn mix must not receive a second gain. */
    void ConfigureRoutes(USoundClass* Music, USoundClass* Voice, USoundClass* Sfx, USoundClass* Ambience,
        bool bRoutesIncludeProfileGain = true, bool bMasterHandledExternally = true);
    void ApplyProfile(const FOGPlayerProfileSettings& Profile);

    /** Optional ownership transfer: do not call while the pawn owns these mixes. */
    void ConfigureDynamicRangeMixes(USoundMix* Full, USoundMix* Night, USoundMix* Compressed);
    void ReleaseDynamicRangeMixOwnership();

    UFUNCTION(BlueprintCallable, Category="OfflineGame|Audio")
    void RegisterEventDefinition(FName EventName, const FOGSemanticAudioEventDefinition& Definition);
    UFUNCTION(BlueprintCallable, Category="OfflineGame|Audio")
    void UnregisterEventDefinition(FName EventName);
    /** True means playback started or optional soft content was queued, not audible. */
    UFUNCTION(BlueprintCallable, Category="OfflineGame|Audio")
    bool PlaySemanticEvent(FName EventName, FVector Location = FVector::ZeroVector);
    UFUNCTION(BlueprintCallable, Category="OfflineGame|Audio")
    void StopSemanticEvent(FName EventName);
    UFUNCTION(BlueprintCallable, Category="OfflineGame|Audio")
    void CancelOwnedPresentation();
    UFUNCTION(BlueprintCallable, Category="OfflineGame|Audio")
    void SetWorldPresentationActive(bool bActive);

    /** Forward named parameters only to this runtime's authored sound instances. */
    UFUNCTION(BlueprintCallable, Category="OfflineGame|Audio")
    void SetEnvironmentState(FName StateName, float Value);
    UFUNCTION(BlueprintPure, Category="OfflineGame|Audio")
    bool HasAuthoredSound(FName EventName) const;
    UFUNCTION(BlueprintPure, Category="OfflineGame|Audio")
    bool HasAuthoredCaptions(FName EventName) const;
    UFUNCTION(BlueprintPure, Category="OfflineGame|Audio")
    FOGSemanticCaptionProjection GetCurrentCaption() const { return CurrentCaption; }
    FOGSemanticAudioPlaybackProjection GetPlaybackProjection(FName EventName) const;
    FOGSemanticAudioOwnershipProjection GetOwnershipProjection() const;

    UPROPERTY(BlueprintAssignable, Category="OfflineGame|Audio") FOGSemanticCaptionChanged OnCaptionChanged;
    FOGSemanticEventPlayedNative OnSemanticEventPlayed;

private:
    UFUNCTION() void HandlePresentationModeChanged(EOGPresentationMode Mode);
    struct FActiveEvent
    {
        FName Name;
        FOGSemanticAudioEventDefinition Definition;
        TWeakObjectPtr<UAudioComponent> Audio;
        TWeakObjectPtr<UForceFeedbackComponent> Feedback;
        double StartedAt = 0.0;
        uint64 Sequence = 0;
        bool bSoundStarted = false;
    };
    struct FPendingEvent
    {
        FName Name;
        FVector Location = FVector::ZeroVector;
        TSharedPtr<FStreamableHandle> Handle;
        uint64 Token = 0;
    };
    bool PlayResolvedEvent(FName EventName, const FOGSemanticAudioEventDefinition& Definition, const FVector& Location);
    void FinishPendingEvent(uint64 Token);
    void StopActiveEvent(FActiveEvent& Event);
    void UpdateCaption();
    void ApplyDynamicRangeMix();
    void UpdateActiveGains();
    USoundClass* GetRoute(EOGSemanticAudioCategory Category) const;
    float GetEventGain(const FOGSemanticAudioEventDefinition& Definition) const;
    float GetDynamicFallbackGain(EOGSemanticAudioCategory Category) const;
    bool IsLocalPresentation() const;

    UPROPERTY(Transient) TMap<FName, FOGSemanticAudioEventDefinition> Definitions;
    UPROPERTY(Transient) TObjectPtr<USoundClass> MusicRoute;
    UPROPERTY(Transient) TObjectPtr<USoundClass> VoiceRoute;
    UPROPERTY(Transient) TObjectPtr<USoundClass> SfxRoute;
    UPROPERTY(Transient) TObjectPtr<USoundClass> AmbienceRoute;
    UPROPERTY(Transient) TObjectPtr<USoundMix> FullMix;
    UPROPERTY(Transient) TObjectPtr<USoundMix> NightMix;
    UPROPERTY(Transient) TObjectPtr<USoundMix> CompressedMix;
    UPROPERTY(Transient) TObjectPtr<USoundMix> OwnedActiveMix;
    UPROPERTY(Transient) TArray<TObjectPtr<UAudioComponent>> OwnedAudio;
    UPROPERTY(Transient) TArray<TObjectPtr<UForceFeedbackComponent>> OwnedFeedback;
    UPROPERTY(Transient) FOGSemanticCaptionProjection CurrentCaption;
    FOGPlayerProfileSettings AppliedProfile;
    TArray<FActiveEvent> ActiveEvents;
    TArray<FPendingEvent> PendingEvents;
    TMap<FName, double> LastPlayedAt;
    TMap<FName, float> EnvironmentParameters;
    uint64 NextSequence = 1;
    bool bWorldActive = true;
    bool bExternalRouteGain = true;
    bool bExternalMasterGain = true;
    bool bOwnDynamicRangeMix = false;
};
