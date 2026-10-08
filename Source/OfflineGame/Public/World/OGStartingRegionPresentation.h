#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/HUD.h"
#include "Gacha/OGGachaDefinitions.h"
#include "UI/OGUiViewModels.h"
#include "World/OGTraversalFramework.h"
#include "World/OGWorldModeInterfaces.h"
#include "OGStartingRegionPresentation.generated.h"

class UCameraComponent;
class UHierarchicalInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UProceduralMeshComponent;
class USceneComponent;
class USpringArmComponent;
class UStaticMeshComponent;
class USkeletalMeshComponent;
class UTouchInterface;
class UOGDiagnosticCombatComponent;
class UOGDiagnosticAnimationPresentation;
class UOGSemanticAudioRuntime;
class UOGFoundationDiagnosticMenu;
class UOGCanonicalCharacterPresentation;
struct FOGFoundationCharacterContext;
struct FOGInteractionParticipantState;
class UOGFoundationInteractionHost;
class AOGFoundationIntegrationWorld;
class USoundClass;
class USoundMix;
class UOGTraversalCapabilityComponent;
class UOGWorldActionRuntimeComponent;
class UOGWorldPartyRuntimeComponent;
struct FOGPlayerProfileSettings;
class APlayerController;
enum class EOGActionCancelDestination : uint8;
enum class EOGTraversalMode : uint8;
struct FOGEntityId;

/** Pure traversal-safety policy shared by runtime and regression tests. */
struct OFFLINEGAME_API FOGWorldSafetyPolicy
{
    static float GetSafeHalfExtent(
        float PlayableHalfExtent,
        float TerrainCellSize)
    {
        const float BoundaryInset =
            FMath::Max(1800.0f, TerrainCellSize * 1.5f);
        return FMath::Max(0.0f, PlayableHalfExtent - BoundaryInset);
    }

    static bool IsOutsidePlayableRegion(
        const FVector& LocalLocation,
        float PlayableHalfExtent,
        float TerrainCellSize)
    {
        const float SafeHalfExtent =
            GetSafeHalfExtent(PlayableHalfExtent, TerrainCellSize);
        return FMath::Abs(LocalLocation.X) > SafeHalfExtent ||
            FMath::Abs(LocalLocation.Y) > SafeHalfExtent;
    }

};

/**
 * Presentation-side generator for the canonical starting region.
 *
 * The region seed is persisted through the authoritative world store. Geometry is
 * reconstructed deterministically from that seed so presentation can be replaced
 * or streamed without becoming authoritative world state itself.
 */
UCLASS(BlueprintType)
class OFFLINEGAME_API AOGStartingRegionGenerator
    : public AActor,
      public IOGPlayableBoundaryProvider
{
    GENERATED_BODY()

public:
    AOGStartingRegionGenerator();

    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void BeginPlay() override;

    virtual bool IsInsidePlayableBoundary_Implementation(
        const FVector& WorldLocation) const override;
    virtual bool IsInsideStableGroundRegion_Implementation(
        const FVector& WorldLocation) const override;

    UFUNCTION(BlueprintCallable, Category = "OfflineGame|World|Generation")
    void RebuildPreview();

    void SetFoundationIntegrationBoundary(AActor* Provider)
    { FoundationIntegrationBoundary = Provider; if (Provider) ClearGeneratedContent(); }

    UFUNCTION(BlueprintPure, Category = "OfflineGame|World|Generation")
    int32 GetActiveSeed() const { return ActiveSeed; }

    UFUNCTION(BlueprintPure, Category = "OfflineGame|World|Generation")
    int32 GetGeneratorVersion() const { return GeneratorVersion; }

    UFUNCTION(BlueprintPure, Category = "OfflineGame|World|Generation")
    float GetPlayableHalfExtent() const
    {
        return FMath::Clamp(TerrainCellsPerSide, 24, 128) *
            FMath::Clamp(TerrainCellSize, 400.0f, 2400.0f) *
            0.5f;
    }

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OfflineGame|World|Generation")
    int32 EditorPreviewSeed = 731942;

    UPROPERTY(Transient)
    TWeakObjectPtr<AActor> FoundationIntegrationBoundary;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OfflineGame|World|Generation", meta = (ClampMin = "24", ClampMax = "128"))
    int32 TerrainCellsPerSide = 64;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OfflineGame|World|Generation", meta = (ClampMin = "400.0", ClampMax = "2400.0"))
    float TerrainCellSize = 1200.0f;

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "OfflineGame|World")
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "OfflineGame|World")
    TObjectPtr<UProceduralMeshComponent> Terrain;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "OfflineGame|World")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> RockInstances;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "OfflineGame|World")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> TrunkInstances;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "OfflineGame|World")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> CanopyInstances;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "OfflineGame|World")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> StructureInstances;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "OfflineGame|World")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> RoofInstances;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "OfflineGame|World")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> RoadInstances;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "OfflineGame|World")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> AccentInstances;

private:
    struct FRegionLayout
    {
        FVector2D SettlementCenter = FVector2D::ZeroVector;
        FVector2D AuthoritySite = FVector2D::ZeroVector;
        FVector2D RivalSite = FVector2D::ZeroVector;
        FVector2D ThirdPartySite = FVector2D::ZeroVector;
        FVector2D DungeonEntrance = FVector2D::ZeroVector;
        FVector2D TerritorialCreatureRange = FVector2D::ZeroVector;
        float RotationRadians = 0.0f;
    };

    static constexpr int32 GeneratorVersion = 1;

    int32 ActiveSeed = 0;
    FRegionLayout ActiveLayout;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> TerrainMaterial;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> RockMaterial;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> VegetationMaterial;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> WoodMaterial;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> StructureMaterial;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> RoofMaterial;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> RoadMaterial;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> AccentMaterial;

    bool ResolveCanonicalSeed(int32& OutSeed);
    void PersistCanonicalLocations(int32 Seed);
    void GenerateRegion(int32 Seed);
    void ClearGeneratedContent();
    void BuildLayout(int32 Seed);
    void BuildTerrain(int32 Seed);
    void BuildVegetationAndRocks(int32 Seed);
    void BuildSettlement(int32 Seed);
    void BuildFactionSites(int32 Seed);
    void BuildDungeonAndCreatureRange(int32 Seed);
    void BuildRoads(int32 Seed);
    void ApplyPresentationMaterials();

    float SampleHeight(float X, float Y, int32 Seed) const;
    FVector ToSurface(const FVector2D& Point, float ZOffset = 0.0f) const;
    float SampleFootprintGroundHeight(
        const FVector2D& Point,
        float YawDegrees,
        const FVector& Scale) const;
    FTransform GroundedBoxTransform(
        const FVector2D& Point,
        float YawDegrees,
        const FVector& Scale,
        float ExtraClearance = 0.0f) const;
    FVector2D RotateLocal(const FVector2D& Local) const;
    bool IsProtectedClearing(const FVector2D& Point) const;

    static FOGEntityId MakeCanonicalLocationId(int32 Seed, uint32 Ordinal);
    static FOGEntityId StartingRegionMetadataId();

    UMaterialInstanceDynamic* CreateTintedMaterial(
        UMaterialInterface* Parent,
        const FLinearColor& Color,
        float Roughness);
};

/** Minimal phone-visible combat fixture proving targeting/damage/dodge contracts. */
UCLASS()
class OFFLINEGAME_API AOGFoundationCombatTarget
    : public AActor,
      public IOGWorldTargetable
{
    GENERATED_BODY()

public:
    AOGFoundationCombatTarget();
    virtual void Tick(float DeltaSeconds) override;
    virtual float TakeDamage(
        float DamageAmount,
        struct FDamageEvent const& DamageEvent,
        class AController* EventInstigator,
        AActor* DamageCauser) override;

    virtual bool CanBeTargeted_Implementation(AActor* Requester) const override;
    virtual FVector GetTargetPoint_Implementation(AActor* Requester) const override;

    float GetCurrentHealth() const { return CurrentHealth; }
    float GetMaxHealth() const { return MaxHealth; }
    float GetLastDamageAmount() const { return LastDamageAmount; }
    bool IsDefeated() const { return CurrentHealth <= 0.0f; }

protected:
    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UStaticMeshComponent> Mesh;

private:
    float MaxHealth = 100.0f;
    float CurrentHealth = 100.0f;
    float LastDamageAmount = 0.0f;
    double NextAttackSeconds = 0.0;
};

/** Minimal phone-visible interaction fixture. */
UCLASS()
class OFFLINEGAME_API AOGFoundationInteractableMarker
    : public AActor,
      public IOGWorldInteractable
{
    GENERATED_BODY()

public:
    AOGFoundationInteractableMarker();

    virtual bool CanInteract_Implementation(AActor* Interactor) const override;
    virtual FText GetInteractionPrompt_Implementation(AActor* Interactor) const override;
    virtual void Interact_Implementation(AActor* Interactor) override;

protected:
    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UStaticMeshComponent> Mesh;

private:
    bool bActivated = false;
};

/** Phone-visible climb fixture proving the authored climb interface. */
UCLASS()
class OFFLINEGAME_API AOGFoundationClimbableWall
    : public AActor,
      public IOGClimbableSurface,
      public IOGWorldInteractable
{
    GENERATED_BODY()

public:
    AOGFoundationClimbableWall();

    virtual bool CanClimb_Implementation(
        ACharacter* Climber,
        FVector HitLocation,
        FVector SurfaceNormal) const override;
    virtual bool CanInteract_Implementation(AActor* Interactor) const override;
    virtual FText GetInteractionPrompt_Implementation(AActor* Interactor) const override;
    virtual void Interact_Implementation(AActor* Interactor) override;

protected:
    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UStaticMeshComponent> Mesh;
};

/** Minimal mount/vehicle fixture using the existing traversal-carrier contract. */
UCLASS()
class OFFLINEGAME_API AOGFoundationTraversalCarrier
    : public AActor,
      public IOGTraversalCarrier,
      public IOGWorldInteractable
{
    GENERATED_BODY()

public:
    AOGFoundationTraversalCarrier();
    virtual void Tick(float DeltaSeconds) override;

    void SetFoundationCarrierMode(EOGTraversalMode Mode)
    {
        CarrierMode = Mode;
    }

    virtual bool CanBoard_Implementation(ACharacter* Rider) const override;
    virtual EOGTraversalMode GetCarrierTraversalMode_Implementation() const override;
    virtual FTransform GetRiderTransform_Implementation(ACharacter* Rider) const override;
    virtual APawn* GetCarrierPawn_Implementation(ACharacter* Rider) const override;
    virtual void AddCarrierMovementInput_Implementation(
        ACharacter* Rider,
        FVector2D Input) override;
    virtual void AddCarrierVerticalInput_Implementation(
        ACharacter* Rider,
        float Input) override;
    virtual void OnBoarded_Implementation(ACharacter* Rider) override;
    virtual void OnUnboarded_Implementation(ACharacter* Rider) override;

    virtual bool CanInteract_Implementation(AActor* Interactor) const override;
    virtual FText GetInteractionPrompt_Implementation(AActor* Interactor) const override;
    virtual void Interact_Implementation(AActor* Interactor) override;

protected:
    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UStaticMeshComponent> Mesh;

private:
    EOGTraversalMode CarrierMode = EOGTraversalMode::Mounted;
    FVector2D PendingMovementInput = FVector2D::ZeroVector;
    float VerticalVelocity = 0.0f;
    TWeakObjectPtr<ACharacter> CurrentRider;
};

/** Context fixture for baseline swim/dive controls without granting exotic movement. */
UCLASS()
class OFFLINEGAME_API AOGFoundationSwimStation
    : public AActor,
      public IOGWorldInteractable
{
    GENERATED_BODY()

public:
    AOGFoundationSwimStation();

    virtual bool CanInteract_Implementation(AActor* Interactor) const override;
    virtual FText GetInteractionPrompt_Implementation(AActor* Interactor) const override;
    virtual void Interact_Implementation(AActor* Interactor) override;

protected:
    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UStaticMeshComponent> Mesh;
};

/** Provisional traversal body used until the authored protagonist presentation exists. */
UCLASS()
class OFFLINEGAME_API AOGWorldPrototypeCharacter : public ACharacter
{
    GENERATED_BODY()

public:
    AOGWorldPrototypeCharacter();
    void InitializeFoundationIntegration();
    void ShutdownFoundationRuntime();
    void RefreshCanonicalPresentation(const FOGFoundationCharacterContext&);
    void ApplyInteractionCharacterPresentation(const FOGInteractionParticipantState&);
    void UpdateDiagnosticAnimationState();
    void UpdateProtagonistRepresentation(const FOGEntityId& Entity, const FOGContentId& Identity);
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    void RestoreDiagnosticHardLock(AActor* Target);
    bool HandleWorldHudControl(FName Name, bool bPressed);
    void PresentAcceptedCombatAction(uint8 Command, int32 Chain);
    virtual void Landed(const FHitResult& Hit) override;

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual float TakeDamage(
        float DamageAmount,
        struct FDamageEvent const& DamageEvent,
        class AController* EventInstigator,
        AActor* DamageCauser) override;
    virtual void OnMovementModeChanged(EMovementMode PrevMovementMode, uint8 PreviousCustomMode) override;
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

    UFUNCTION(BlueprintPure, Category = "OfflineGame|World|Action")
    bool IsSprinting() const { return bSprinting; }

    UFUNCTION(BlueprintPure, Category = "OfflineGame|World|Action")
    bool IsDodging() const { return bDodging; }

    UFUNCTION(BlueprintPure, Category = "OfflineGame|World|Action")
    bool IsDodgeInvulnerable() const;

    UFUNCTION(BlueprintCallable, Category = "OfflineGame|World|Action")
    void NotifyPerfectDodge();

    UFUNCTION(BlueprintPure, Category = "OfflineGame|World|Targeting")
    AActor* GetHardLockedTarget() const { return HardLockedTarget.Get(); }

    UFUNCTION(BlueprintPure, Category = "OfflineGame|Traversal")
    UOGTraversalCapabilityComponent* GetTraversalCapabilities() const { return TraversalCapabilities; }

    UFUNCTION(BlueprintCallable, Category = "OfflineGame|Traversal")
    bool TryBeginClimb();

    UFUNCTION(BlueprintCallable, Category = "OfflineGame|Traversal")
    void EndClimb();

    UFUNCTION(BlueprintCallable, Category = "OfflineGame|Traversal")
    bool BeginFlight();

    UFUNCTION(BlueprintCallable, Category = "OfflineGame|Traversal")
    void EndFlight();

    UFUNCTION(BlueprintCallable, Category = "OfflineGame|Traversal")
    bool SetDiving(bool bDiving);

    UFUNCTION(BlueprintCallable, Category = "OfflineGame|Traversal")
    bool TryBoardCarrier(AActor* Carrier);

    UFUNCTION(BlueprintCallable, Category = "OfflineGame|Traversal")
    void DismountCarrier();

    UFUNCTION(BlueprintCallable, Category = "OfflineGame|Traversal")
    bool TryTeleportWithinCurrentMap(FVector Destination);

    UFUNCTION(BlueprintPure, Category = "OfflineGame|Traversal")
    bool CanTraverseGate(AActor* Gate) const;

    UFUNCTION(BlueprintImplementableEvent, Category = "OfflineGame|World|Action")
    void OnPrimaryAttackRequested(AActor* Target);

    UFUNCTION(BlueprintImplementableEvent, Category = "OfflineGame|World|Action")
    void OnSkillRequested(int32 SkillSlot, AActor* Target);

    UFUNCTION(BlueprintImplementableEvent, Category = "OfflineGame|World|Action")
    void OnUltimateRequested(AActor* Target);

    UFUNCTION(BlueprintImplementableEvent, Category = "OfflineGame|World|Action")
    void OnManualAimChanged(bool bEnabled);

    UFUNCTION(BlueprintImplementableEvent, Category = "OfflineGame|World|Action")
    void OnPerfectDodgeTriggered(float SuggestedPerceptionSlowdownSeconds);

    UFUNCTION(BlueprintImplementableEvent, Category = "OfflineGame|World|Targeting")
    void OnHardLockChanged(AActor* Target);

    AActor* GetContextInteractableForHud() const;
    bool IsFoundationSettingsOpen() const { return bFoundationSettingsOpen; }
    bool IsFoundationPauseMenuOpen() const { return bFoundationPauseMenuOpen; }
    bool IsFoundationWorldInputAllowed() const;
    void RefreshFoundationInputState();
    virtual void CalcCamera(float DeltaTime, FMinimalViewInfo& OutResult) override;
    void SetFoundationSettingsOpen(bool bOpen);
    void SetFoundationPauseMenuOpen(bool bOpen);
    int32 GetFoundationSettingsPage() const { return FoundationSettingsPage; }
    void SetFoundationSettingsPage(int32 Page);
    float GetCameraHorizontalSensitivity() const { return CameraHorizontalSensitivity; }
    float GetCameraVerticalSensitivity() const { return CameraVerticalSensitivity; }
    float GetCameraResponseExponent() const { return CameraResponseExponent; }
    bool GetInvertCameraX() const { return bInvertCameraX; }
    bool GetInvertCameraY() const { return bInvertCameraY; }
    bool GetSprintTogglePreference() const { return bSprintTogglePreference; }
    bool GetLeftHandedControls() const { return bLeftHandedControls; }
    float GetTouchControlScale() const { return TouchControlScale; }
    float GetTouchControlOpacity() const { return TouchControlOpacity; }
    float GetMovementDeadzone() const { return MovementDeadzone; }
    float GetLookDeadzone() const { return LookDeadzone; }
    float GetMovementStickInset() const { return MovementStickInset; }
    float GetMovementStickBottom() const { return MovementStickBottom; }
    float GetActionClusterHorizontalOffset() const { return ActionClusterHorizontalOffset; }
    float GetActionClusterVerticalOffset() const { return ActionClusterVerticalOffset; }
    bool GetSfwPresentation() const { return bSfwPresentation; }
    FName GetRosterDensity() const { return RosterDensity; }
    FName GetCinematicRepeatPolicy() const { return CinematicRepeatPolicy; }
    bool GetAutoDownloadEnabled() const { return bAutoDownload; }
    bool GetLargeDownloadsUnmeteredOnly() const
    {
        return bLargeDownloadsUnmeteredOnly;
    }
    bool GetReducedMotion() const { return bReducedMotion; }
    bool GetReducedCameraShake() const { return bReducedCameraShake; }
    bool GetSubtitlesEnabled() const { return bSubtitlesEnabled; }
    FName GetSubtitlePresentation() const { return SubtitlePresentation; }
    FName GetUiReadabilityProfile() const { return UiReadabilityProfile; }
    FName GetColorVisionProfile() const { return ColorVisionProfile; }
    bool GetHapticsEnabled() const { return bHapticsEnabled; }
    float GetHapticsIntensity() const { return HapticsIntensity; }
    float GetMasterVolume() const { return MasterVolume; }
    float GetMusicVolume() const { return MusicVolume; }
    float GetVoiceVolume() const { return VoiceVolume; }
    float GetSfxVolume() const { return SfxVolume; }
    float GetAmbienceVolume() const { return AmbienceVolume; }
    FName GetDynamicRangeProfile() const { return DynamicRangeProfile; }
    FName GetDamageNumberPresentation() const { return DamageNumberPresentation; }

    UFUNCTION(BlueprintPure, Category = "OfflineGame|Audio")
    USoundClass* GetFoundationMusicSoundClass() const { return FoundationMusicSoundClass; }

    UFUNCTION(BlueprintPure, Category = "OfflineGame|Audio")
    USoundClass* GetFoundationVoiceSoundClass() const { return FoundationVoiceSoundClass; }

    UFUNCTION(BlueprintPure, Category = "OfflineGame|Audio")
    USoundClass* GetFoundationSfxSoundClass() const { return FoundationSfxSoundClass; }

    UFUNCTION(BlueprintPure, Category = "OfflineGame|Audio")
    USoundClass* GetFoundationAmbienceSoundClass() const { return FoundationAmbienceSoundClass; }
    FName GetOrientationOverride() const { return OrientationOverride; }
    void AdjustCameraHorizontalSensitivity(float Delta);
    void AdjustCameraVerticalSensitivity(float Delta);
    void AdjustCameraResponseExponent(float Delta);
    void ToggleInvertCameraX();
    void ToggleInvertCameraY();
    void ToggleSprintPreference();
    void CycleOrientationOverride();
    void ToggleLeftHandedControls();
    void AdjustTouchControlScale(float Delta);
    void AdjustTouchControlOpacity(float Delta);
    void AdjustMovementDeadzone(float Delta);
    void AdjustLookDeadzone(float Delta);
    void AdjustMovementStickInset(float Delta);
    void AdjustMovementStickBottom(float Delta);
    void AdjustActionClusterHorizontalOffset(float Delta);
    void AdjustActionClusterVerticalOffset(float Delta);
    void ToggleSfwPresentation();
    void CycleRosterDensity();
    void CycleCinematicRepeatPolicy();
    void ToggleAutoDownload();
    void ToggleLargeDownloadsUnmeteredOnly();
    void ToggleReducedMotion();
    void ToggleReducedCameraShake();
    void ToggleSubtitles();
    void CycleSubtitlePresentation();
    void CycleUiReadabilityProfile();
    void CycleColorVisionProfile();
    void ToggleHaptics();
    void AdjustHapticsIntensity(float Delta);
    void AdjustMasterVolume(float Delta);
    void AdjustMusicVolume(float Delta);
    void AdjustVoiceVolume(float Delta);
    void AdjustSfxVolume(float Delta);
    void AdjustAmbienceVolume(float Delta);
    void CycleDynamicRangeProfile();
    void CycleDamageNumberPresentation();
    void TriggerContextInteract();
    void TriggerPartySwitchFromHud(int32 SlotIndex);
    void TriggerQteFromHud(int32 SlotIndex);
    void TriggerSkillFromHud(int32 SkillSlot);
    void TriggerUltimateFromHud();
    bool HasSkillSlotForHud(int32 SkillSlot) const;
    bool HasUltimateForHud() const;

    UFUNCTION(BlueprintCallable, Category = "OfflineGame|World|Party")
    void SetPartySlotQteReadyFromContent(int32 SlotIndex, bool bReady);

    void ToggleManualAimFromHud();
    void TriggerTraversalDownPulse();
    int32 GetPartySlotCountForHud() const;
    int32 GetControlledPartySlotForHud() const;
    bool IsPartySlotAvailableForHud(int32 SlotIndex) const;
    bool IsPartySlotDefeatedForHud(int32 SlotIndex) const;
    bool IsPartySlotQteReadyForHud(int32 SlotIndex) const;
    bool IsManualAimEnabled() const { return bManualAim; }
    bool ShouldShowTraversalDownForHud() const;
    FString GetTraversalModeLabelForHud() const;
    float GetFoundationHealth() const;
    FString GetCanonicalHealthText() const;
    float GetFoundationMaxHealth() const { return FoundationMaxHealth; }
    float GetRecentFoundationDamage() const { return RecentFoundationDamage; }
    bool HasRecentFoundationDamage() const;
    const TArray<FVector2D>& GetVisitedMapTrailForHud() const
    {
        return VisitedMapTrail;
    }

    UFUNCTION(BlueprintCallable, Category = "OfflineGame|World|HUD")
    void SetControlledResourcePresentation(
        FName ResourceLabel,
        float CurrentValue,
        float MaxValue,
        bool bVisible);

    UFUNCTION(BlueprintCallable, Category = "OfflineGame|World|HUD")
    void SetTargetHudProjection(
        const FOGWorldTargetViewModel& Projection);

    UFUNCTION(BlueprintCallable, Category = "OfflineGame|World|HUD")
    void SetSecondaryTargetHudProjections(
        const TArray<FOGWorldTargetViewModel>& Projections);

    UFUNCTION(BlueprintCallable, Category = "OfflineGame|World|HUD")
    void ClearTargetHudProjection();

    bool IsControlledResourceVisibleForHud() const
    {
        return bControlledResourceVisible;
    }
    FName GetControlledResourceLabelForHud() const
    {
        return ControlledResourceLabel;
    }
    float GetControlledResourceCurrentForHud() const
    {
        return ControlledResourceCurrent;
    }
    float GetControlledResourceMaxForHud() const
    {
        return ControlledResourceMax;
    }
    bool HasTargetHudProjectionForHud() const
    {
        return bHasTargetHudProjection;
    }
    const FOGWorldTargetViewModel& GetTargetHudProjectionForHud() const
    {
        return TargetHudProjection;
    }
    const TArray<FOGWorldTargetViewModel>&
        GetSecondaryTargetHudProjectionsForHud() const
    {
        return SecondaryTargetHudProjections;
    }

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "OfflineGame|Camera")
    TObjectPtr<USpringArmComponent> CameraBoom;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "OfflineGame|Camera")
    TObjectPtr<UCameraComponent> FollowCamera;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "OfflineGame|Prototype")
    TObjectPtr<USkeletalMeshComponent> PrototypeBody;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "OfflineGame|Traversal")
    TObjectPtr<UOGTraversalCapabilityComponent> TraversalCapabilities;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "OfflineGame|World|Action")
    TObjectPtr<UOGWorldActionRuntimeComponent> ActionRuntime;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "OfflineGame|World|Party")
    TObjectPtr<UOGWorldPartyRuntimeComponent> PartyRuntime;

    UPROPERTY(Transient)
    TObjectPtr<UTouchInterface> FoundationTouchInterface;
    UPROPERTY() TObjectPtr<UOGDiagnosticCombatComponent> DiagnosticCombat;
    UPROPERTY() TObjectPtr<UOGDiagnosticAnimationPresentation> DiagnosticAnimations;
    UPROPERTY() TObjectPtr<UOGSemanticAudioRuntime> SemanticAudio;
    UPROPERTY() TObjectPtr<UOGFoundationDiagnosticMenu> DiagnosticMenu;
    UPROPERTY() TObjectPtr<UOGCanonicalCharacterPresentation> CanonicalPresentation;
    UPROPERTY() TObjectPtr<UOGFoundationInteractionHost> InteractionHost;
    UPROPERTY() TObjectPtr<AOGFoundationIntegrationWorld> IntegrationWorld;
    UPROPERTY() TObjectPtr<ACharacter> ProtagonistRepresentation;
    UPROPERTY() TObjectPtr<USoundMix> OwnedDynamicRangeMix;
    bool bIntegrationCourseBuilt = false;
    float LastDiagnosticFacingYaw = 0.0f;
    bool bHasDiagnosticFacingSample = false;

    UPROPERTY(Transient)
    TObjectPtr<USoundMix> FoundationVolumeMix;

    UPROPERTY(Transient)
    TObjectPtr<USoundClass> FoundationMusicSoundClass;

    UPROPERTY(Transient)
    TObjectPtr<USoundClass> FoundationVoiceSoundClass;

    UPROPERTY(Transient)
    TObjectPtr<USoundClass> FoundationSfxSoundClass;

    UPROPERTY(Transient)
    TObjectPtr<USoundClass> FoundationAmbienceSoundClass;

private:
    void MoveForward(float Value);
    void MoveRight(float Value);
    void Turn(float Value);
    void LookUp(float Value);
    void TurnRate(float Value);
    void LookUpRate(float Value);
    void TouchPressed(ETouchIndex::Type FingerIndex, FVector Location);
    void TouchMoved(ETouchIndex::Type FingerIndex, FVector Location);
    void TouchReleased(ETouchIndex::Type FingerIndex, FVector Location);
    void JumpPressed();
    void JumpReleased();
    void TraversalDownPressed();
    void TraversalDownReleased();
    void SprintPressed();
    void SprintReleased();
    void DodgePressed();
    void InteractPressed();
    void ToggleLockOn();
    void PrimaryAttackPressed();
    void Skill1Pressed();
    void Skill2Pressed();
    void UltimatePressed();
    void ManualAimPressed();
    void ManualAimReleased();
    void SwitchSlot1Pressed();
    void SwitchSlot2Pressed();
    void SwitchSlot3Pressed();
    void QtePressed();
    void PausePressed();
    void CreateFoundationTouchInterface();
    void RefreshFoundationTouchInterfaceIfNeeded();
    void LoadControlProfile();
    bool LoadMutablePlayerProfile(FOGPlayerProfileSettings& OutProfile) const;
    void SaveMutablePlayerProfileAndApply(const FOGPlayerProfileSettings& Profile);
    void ApplyAudioAndSubtitlePresentationSettings();
    float ShapeLookInput(float Value) const;
    bool IsCameraDragTouch(const FVector& ScreenLocation) const;
    AActor* FindBestTarget(float MaxDistance = 3500.0f) const;
    AActor* FindBestInteractable(float MaxDistance = 450.0f) const;
    void UpdateHardLock(float DeltaSeconds);
    bool PrepareActionCancel(EOGActionCancelDestination Destination);
    bool CanContinueActionInTraversalMode(EOGTraversalMode Mode) const;
    void RequestSkillSlot(int32 SkillSlot);
    void RequestPartySwitch(int32 SlotIndex);
    void ConfigureFoundationParty();
    void EndDodgeIfNeeded();
    void UpdateFoundationCombatPresentation();
    void TrySnapToGeneratedGround();
    void ValidateWorldSafety();
    bool HasClearCapsuleAt(const FVector& Location, const AActor* IgnoredActor = nullptr) const;
    bool IsInteractionLineClear(AActor* Candidate) const;
    bool FindDismountLocation(AActor* Carrier, FVector& OutLocation) const;
    void ResetTraversalForRelocation();
    void UpdateClimbSurface();
    bool IsWithinDeclaredPlayableBoundary(
        const FVector& WorldLocation,
        bool* bOutStableGroundRegion = nullptr) const;
    void RecoverToLastSafeGround(const TCHAR* Reason);
    void CacheStartingRegion();

    int32 GroundSnapAttempts = 0;
    TWeakObjectPtr<AOGStartingRegionGenerator> StartingRegion;
    bool bClimbTopBlocked = false;
    TWeakObjectPtr<AActor> CurrentClimbSurfaceActor;
    TWeakObjectPtr<UPrimitiveComponent> CurrentClimbSurfaceComponent;
    FVector LastSafeGroundedLocation = FVector::ZeroVector;
    bool bHasLastSafeGroundedLocation = false;

    float CameraHorizontalSensitivity = 1.10f;
    float CameraVerticalSensitivity = 0.82f;
    float CameraResponseExponent = 1.55f;
    bool bInvertCameraX = false;
    bool bInvertCameraY = false;
    bool bSprintTogglePreference = false;
    bool bLeftHandedControls = false;
    float TouchControlScale = 1.0f;
    float TouchControlOpacity = 0.74f;
    float MovementDeadzone = 0.08f;
    float LookDeadzone = 0.03f;
    float MovementStickInset = 0.105f;
    float MovementStickBottom = 0.14f;
    float ActionClusterHorizontalOffset = 0.0f;
    float ActionClusterVerticalOffset = 0.0f;
    bool bSfwPresentation = false;
    FName RosterDensity = FName(TEXT("dense"));
    FName CinematicRepeatPolicy = FName(TEXT("first_time"));
    bool bAutoDownload = true;
    bool bLargeDownloadsUnmeteredOnly = true;
    bool bReducedMotion = false;
    bool bReducedCameraShake = false;
    bool bSubtitlesEnabled = true;
    FName SubtitlePresentation = FName(TEXT("standard"));
    FName UiReadabilityProfile = FName(TEXT("standard"));
    FName ColorVisionProfile = FName(TEXT("standard"));
    bool bHapticsEnabled = true;
    float HapticsIntensity = 0.80f;
    float MasterVolume = 1.0f;
    float MusicVolume = 1.0f;
    float VoiceVolume = 1.0f;
    float SfxVolume = 1.0f;
    float AmbienceVolume = 1.0f;
    FName DynamicRangeProfile = FName(TEXT("full"));
    FName DamageNumberPresentation = FName(TEXT("standard"));
    FName OrientationOverride = FName(TEXT("automatic"));
    bool bFoundationSettingsOpen = false;
    bool bFoundationPauseMenuOpen = false;
    bool bOwnsFoundationInputSuppression = false;
    TWeakObjectPtr<APlayerController> FoundationInputController;
    int32 FoundationSettingsPage = 0;
    FIntPoint LastTouchViewportSize = FIntPoint::ZeroValue;

    bool bSprinting = false;
    bool bDodging = false;
    bool bManualAim = false;
    double DodgeInvulnerableUntilSeconds = 0.0;
    double DodgeEndsAtSeconds = 0.0;
    double NextDodgeAllowedSeconds = 0.0;
    double PerfectDodgeSlowEndsAtRealSeconds = 0.0;
    double AttackVisualEndsAtSeconds = 0.0;
    double RecentDamageExpiresAtSeconds = 0.0;
    float FoundationMaxHealth = 100.0f;
    float FoundationHealth = 100.0f;
    float RecentFoundationDamage = 0.0f;
    bool bFoundationSlowMotionActive = false;
    FVector LastNonZeroMoveDirection = FVector::ForwardVector;
    TArray<FVector2D> VisitedMapTrail;
    FVector2D LastMapTrailSample = FVector2D::ZeroVector;
    bool bHasMapTrailSample = false;
    bool bControlledResourceVisible = false;
    FName ControlledResourceLabel = NAME_None;
    float ControlledResourceCurrent = 0.0f;
    float ControlledResourceMax = 0.0f;
    bool bHasTargetHudProjection = false;
    FOGWorldTargetViewModel TargetHudProjection;
    TArray<FOGWorldTargetViewModel> SecondaryTargetHudProjections;
    FVector ClimbSurfaceNormal = FVector::ZeroVector;
    TWeakObjectPtr<AActor> CurrentCarrier;

    TWeakObjectPtr<AActor> HardLockedTarget;

    bool bCameraTouchActive = false;
    ETouchIndex::Type CameraTouchIndex = ETouchIndex::Touch1;
    FVector2D LastCameraTouchPosition = FVector2D::ZeroVector;
    FVector2D CameraTouchStartPosition = FVector2D::ZeroVector;
    bool bTraversalAscendHeld = false;
    bool bTraversalDescendHeld = false;
};

/** World Mode game rules for the first real playable content map. */
UCLASS()
class OFFLINEGAME_API AOGWorldPresentationGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    AOGWorldPresentationGameMode();
    virtual void StartPlay() override;
};

/** Responsive game presentation over canonical world and ruler projections. */
UCLASS()
class OFFLINEGAME_API AOGWorldPresentationHud : public AHUD
{
    GENERATED_BODY()

public:
    virtual void DrawHUD() override;
    virtual void NotifyHitBoxClick(FName BoxName) override;
    virtual void NotifyHitBoxRelease(FName BoxName) override;
    TSet<FName> PressedWorldControls;
    bool AllowsWorldGameplayInput() const
    { return FoundationSurface == TEXT("world") && !bDiagnosticMenuOpen && !bInteractionMenuOpen; }

    /** Query-owner seam for installed content or an isolated diagnostic world. */
    void SetFoundationRulerPresentationOwner(const FOGEntityId& RulerId);
    FOGEntityId GetFoundationRulerPresentationOwner() const;

    /**
     * Content/command-layer handoff for the currently available banner.
     * No banner or no command context means no pull button is exposed.
     */
    UFUNCTION(BlueprintCallable, Category = "OfflineGame|Ruler|Gacha")
    void SetActiveGachaPresentation(
        const FOGGachaBannerDefinition& Banner,
        int64 CanonicalWorldTick,
        int64 DeterministicSeed);

    UFUNCTION(BlueprintCallable, Category = "OfflineGame|Ruler|Gacha")
    void ClearActiveGachaPresentation();

private:
    void DrawGamePolygon(const TArray<FVector2D>& Points, FLinearColor Color);
    void DrawGamePanel(FLinearColor Fill, float X, float Y, float W, float H);
    void DrawGameText(const FString& Text, float X, float Y, float Height, FLinearColor Color, float MaxWidth = 0);
    void DrawGameIcon(FName Id, float X, float Y, float Size, FLinearColor Color);
    void DrawGameButton(const FString& Label, FName Id, float X, float Y, float W, float H,
        bool bPrimary = false, bool bEnabled = true, int32 Priority = 124);
    void DrawGameBackdrop(float X, float Y, float W, float H);
    int32 RosterPage = 0;
    int32 TerritoryPage = 0;
    int32 TurnPage = 0;
    int32 DiagnosticMenuPage = 0;
    int32 DiagnosticMenuPageCount = 1;
    FName GachaOverlay = NAME_None;
    int32 GachaDetailsPage = 0;
    bool DrawFoundationDiagnosticSurface(AOGWorldPrototypeCharacter*, float Scale, bool bOnlyModal);
    bool HandleFoundationDiagnosticClick(FName, AOGWorldPrototypeCharacter*);
    bool bDiagnosticMenuOpen = false;
    bool bInteractionMenuOpen = false;
    bool DrawCanonicalTurnSurface(AOGWorldPrototypeCharacter* Character, float Scale);
    void DrawFoundationText(const FString& Text, FLinearColor Color,
        float X, float Y, UFont* Font = nullptr, float TextScale = 1.0f,
        bool bScalePosition = false);
    void AddFoundationHitBox(
        const FVector2D& Position,
        const FVector2D& Size,
        FName Name,
        bool bConsumesInput,
        int32 Priority);
    void DrawFoundationCharacterStandIn(
        float X, float Y, float Width, float Height,
        const FLinearColor& Accent);
    void SetFoundationSurface(FName Surface);
    void DrawFoundationOpening(
        float Scale,
        const FLinearColor& MainText,
        const FLinearColor& SubText,
        const FLinearColor& Panel,
        const FLinearColor& Button);
    void DrawFoundationRuler(
        float Scale,
        const FLinearColor& MainText,
        const FLinearColor& SubText,
        const FLinearColor& Panel,
        const FLinearColor& Button);

    FOGEntityId FoundationRulerOwner;
    FName FoundationSurface = FName(TEXT("opening"));
    FName OpeningPage = FName(TEXT("main"));
    FName RulerDestination = FName(TEXT("home"));
    FName RecordsDestination = FName(TEXT("hub"));
    FString FoundationStatusMessage;
    int32 SelectedRosterIndex = INDEX_NONE;
    int32 SelectedManifestationIndex = INDEX_NONE;
    int32 ManifestationPage = 0;
    int32 ManifestationsPerPage = 5;
    int32 SettingsRowPage = 0;
    int32 SettingsLastRowPage = 0;
    int32 SelectedTerritoryIndex = INDEX_NONE;
    int32 PendingBackupRestoreIndex = INDEX_NONE;
    bool bPendingImportedBackupRestore = false;
    int32 ExpandedTargetStatusIndex = INDEX_NONE;
    TArray<FOGPackageStorageEntryViewModel> CachedPackageEntries;
    bool bHasActiveGachaPresentation = false;
    FOGGachaBannerDefinition ActiveGachaBanner;
    TArray<FOGGachaPullResult> LastGachaResults;
    int64 GachaCommandWorldTick = 0;
    int64 GachaCommandSeed = 0;
};
