#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Runtime/OGFoundationCharacterRuntime.h"
#include "Runtime/OGOptionalAssetRuntime.h"
#include "OGCanonicalCharacterPresentation.generated.h"

class UOGGameCoreSubsystem;
class UAnimInstance;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UMeshComponent;
class USkeletalMesh;
class USkeletalMeshComponent;

/** Authored visual references. Empty/unavailable references do not replace a body. */
struct OFFLINEGAME_API FOGCharacterVisualPart
{
    FName PartId;
    FName RigFamily;
    FName SocketName;
    FTransform RelativeTransform = FTransform::Identity;
    FOGOptionalAssetReference Asset;
    FOGOptionalAssetReference PrivacyAsset;
    bool bSubstituteForPrivacy = false;
    bool bUseLeaderPose = true;
};

struct OFFLINEGAME_API FOGCharacterVisualBinding
{
    /** Body changes must declare a compatible family. No silent retargeting occurs. */
    FName RigFamily;
    FOGOptionalAssetReference Body;
    FOGOptionalAssetReference PrivacyBody;
    bool bSubstituteBodyForPrivacy = false;
    FOGOptionalAssetReference AnimationClass;
    TMap<int32, FOGOptionalAssetReference> Materials;
    TMap<int32, FOGOptionalAssetReference> PrivacyMaterials;
    TArray<FOGCharacterVisualPart> Parts;
};

/** Derived values only; the complete canonical payload remains in GetProjection(). */
struct OFFLINEGAME_API FOGCharacterVisualState
{
    FName InjuryState;
    float InjurySeverity = 0.f;
    float EquipmentDamage = 0.f;
    float OutfitDamage = 0.f;
    float Restoration = 0.f;
    FString OutfitStateJson = TEXT("{}");
    FString VariantStateJson = TEXT("{}");
    FString HairstyleId;
    FName RestorationState;
    TMap<FOGContentId, float> EquippedCondition;
};

using FOGCharacterVisualResolver = TFunction<bool(
    const FOGFoundationCharacterProjection&, bool,
    FOGCharacterVisualBinding&, FString&)>;

DECLARE_MULTICAST_DELEGATE_TwoParams(FOGCanonicalCharacterProjectionChanged,
    const FOGFoundationCharacterProjection&, bool);

/** Read-only bridge from canonical character state to the actual rig and materials. */
UCLASS(ClassGroup = (OfflineGame), meta = (BlueprintSpawnableComponent))
class OFFLINEGAME_API UOGCanonicalCharacterPresentation : public UActorComponent
{
    GENERATED_BODY()
public:
    UOGCanonicalCharacterPresentation();

    bool Configure(const FOGFoundationCharacterContext& Context,
        USkeletalMeshComponent* Mesh, FName RigFamily, FString& OutError);
    /** Explicit native content/diagnostic seam. Store and Host must outlive the binding.
     * Uses the same canonical projection and visual path as Core-backed Configure. */
    bool ConfigureFromCanonicalStore(const FOGFoundationCharacterContext& Context,
        USkeletalMeshComponent* Mesh, FName RigFamily, IOGWorldStore& Store,
        FOGOptionalPackageHost* Host, FString& OutError, UOGGameCoreSubsystem* LifecycleCore = nullptr);
    bool RefreshFromCanonicalStore(const FOGFoundationCharacterContext& Context,
        IOGWorldStore& Store, FOGOptionalPackageHost* Host, FString& OutError);
    bool Refresh(const FOGFoundationCharacterContext& Context, FString& OutError);
    bool Refresh(FString& OutError);
    void SetVisualResolver(FOGCharacterVisualResolver Resolver);
    void SetPrivacyPresentation(bool bPrivacy);
    bool IsPrivacyPresentation() const { return bPrivacyPresentation; }
    void ClearBinding();

    const FOGFoundationCharacterProjection& GetProjection() const { return Projection; }
    const FOGCharacterVisualState& GetVisualState() const { return VisualState; }
    const FString& GetAssetStatus() const { return AssetStatus; }
    bool HasProjection() const { return bHasProjection; }
    FOGCanonicalCharacterProjectionChanged OnProjectionChanged;

    /** Optional neutral diagnostic tint; authored materials may consume the other parameters. */
    UPROPERTY(EditAnywhere, Category = "OfflineGame|Presentation")
    bool bUseNeutralDiagnosticTint = false;

    UPROPERTY(EditAnywhere, Category = "OfflineGame|Presentation")
    FName DiagnosticTintParameter = FName(TEXT("Color"));

protected:
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    bool BindRig(const FOGFoundationCharacterContext&, USkeletalMeshComponent*, FName, FString&);
    void InstallDefaultVisualResolver();
    void RestoreBaseline();
    void RemoveParts();
    void DeriveVisualState();
    void ApplyMaterialState();
    void ApplyBinding(const FOGCharacterVisualBinding&, IOGWorldStore& Store, FOGOptionalPackageHost* Host);
    void HandleCanonicalRuntimeReleasing();

    UPROPERTY(Transient)
    TObjectPtr<USkeletalMeshComponent> BoundMesh;
    UPROPERTY(Transient)
    TObjectPtr<USkeletalMesh> BaselineMesh;
    UPROPERTY(Transient)
    TSubclassOf<UAnimInstance> BaselineAnimationClass;
    UPROPERTY(Transient)
    TArray<TObjectPtr<UMaterialInterface>> BaselineMaterials;
    UPROPERTY(Transient)
    TArray<TObjectPtr<UMaterialInstanceDynamic>> DynamicMaterials;
    UPROPERTY(Transient)
    TArray<TObjectPtr<UMeshComponent>> VisualParts;

    FName BoundRigFamily;
    FName CurrentRigFamily;
    FOGFoundationCharacterContext Context;
    FOGFoundationCharacterProjection Projection;
    FOGCharacterVisualState VisualState;
    FOGCharacterVisualResolver VisualResolver;
    IOGWorldStore* DefaultResolverStore = nullptr;
    bool bHasExplicitVisualResolver = false;
    TUniquePtr<FOGOptionalAssetRuntime> OptionalAssets;
    IOGWorldStore* OptionalAssetStore = nullptr;
    FOGOptionalPackageHost* OptionalAssetHost = nullptr;
    TMap<FString, TWeakObjectPtr<UObject>> CachedVisualAssets;
    TWeakObjectPtr<UOGGameCoreSubsystem> BoundCore;
    FDelegateHandle ReleaseHandle;
    FString AssetStatus;
    bool bPrivacyPresentation = false;
    bool bHasProjection = false;
};
