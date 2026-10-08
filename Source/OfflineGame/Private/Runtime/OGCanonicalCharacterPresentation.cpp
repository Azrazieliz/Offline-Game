#include "Runtime/OGCanonicalCharacterPresentation.h"
#include "Runtime/OGCharacterVisualResolverProvider.h"
#include "Templates/UnrealTemplate.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimBlueprintGeneratedClass.h"
#include "Components/MeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/GameInstance.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Runtime/OGGameCoreSubsystem.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
TSharedPtr<FJsonObject> ObjectFromJson(const FString& Json)
{
    TSharedPtr<FJsonObject> Result;
    FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Result);
    return Result;
}
double BasisPointValuePrecise(const TSharedPtr<FJsonObject>& Object, const TCHAR* Key, double Default)
{
    double Value = Default * 10000.;
    if (Object.IsValid()) Object->TryGetNumberField(Key, Value);
    return FMath::IsFinite(Value) ? FMath::Clamp(Value / 10000., 0., 1.) : Default;
}
float BasisPointValue(const TSharedPtr<FJsonObject>& Object, const TCHAR* Key, float Default)
{
    return static_cast<float>(BasisPointValuePrecise(Object, Key, Default));
}
FString StringValue(const TSharedPtr<FJsonObject>& Object, const TCHAR* Key)
{
    FString Value;
    if (Object.IsValid()) Object->TryGetStringField(Key, Value);
    return Value;
}
}

UOGCanonicalCharacterPresentation::UOGCanonicalCharacterPresentation()
{
    PrimaryComponentTick.bCanEverTick = false;
}

bool UOGCanonicalCharacterPresentation::BindRig(const FOGFoundationCharacterContext& InContext,
    USkeletalMeshComponent* Mesh, FName RigFamily, FString& Error)
{
    if (!IsValid(Mesh) || Mesh->GetOwner() != GetOwner() || !InContext.EntityId.IsValid())
    {
        Error = TEXT("Character presentation requires its owner's actual rig and canonical entity.");
        return false;
    }
    if (BoundMesh != Mesh)
    {
        ClearBinding();
        BoundMesh = Mesh;
        BaselineMesh = Mesh->GetSkeletalMeshAsset();
        BaselineAnimationClass = Mesh->GetAnimClass();
        BaselineMaterials.Reset();
        for (int32 Index = 0; Index < Mesh->GetNumMaterials(); ++Index)
            BaselineMaterials.Add(Mesh->GetMaterial(Index));
        BoundRigFamily = RigFamily;
        CurrentRigFamily = RigFamily;
    }
    else if (!RigFamily.IsNone() && RigFamily != BoundRigFamily)
    {
        Error = TEXT("Rebinding an existing body to another rig family requires ClearBinding first.");
        return false;
    }
    InstallDefaultVisualResolver();
    return true;
}

void UOGCanonicalCharacterPresentation::InstallDefaultVisualResolver()
{
    if (bHasExplicitVisualResolver || VisualResolver) return;
    const TWeakObjectPtr<UOGCanonicalCharacterPresentation> WeakThis(this);
    VisualResolver = [WeakThis](const FOGFoundationCharacterProjection& P, bool Privacy,
        FOGCharacterVisualBinding& Binding, FString& Reason)
    {
        const auto* Presenter = WeakThis.Get();
        if (!Presenter || !Presenter->DefaultResolverStore)
        { Reason = TEXT("Canonical visual metadata store unavailable; current body retained."); return false; }
        return FOGCharacterVisualResolverProvider::Resolve(*Presenter->DefaultResolverStore, P, Privacy, Binding, Reason);
    };
}

bool UOGCanonicalCharacterPresentation::Configure(const FOGFoundationCharacterContext& InContext,
    USkeletalMeshComponent* Mesh, FName RigFamily, FString& Error)
{
    if (!BindRig(InContext, Mesh, RigFamily, Error)) return false;
    return Refresh(InContext, Error);
}

bool UOGCanonicalCharacterPresentation::ConfigureFromCanonicalStore(const FOGFoundationCharacterContext& InContext,
    USkeletalMeshComponent* Mesh, FName RigFamily, IOGWorldStore& Store,
    FOGOptionalPackageHost* Host, FString& Error, UOGGameCoreSubsystem* LifecycleCore)
{
    if (!BindRig(InContext, Mesh, RigFamily, Error)) return false;
    if (LifecycleCore && BoundCore.Get() != LifecycleCore)
    {
        if (BoundCore.IsValid()) BoundCore->OnCanonicalRuntimeReleasing.Remove(ReleaseHandle);
        BoundCore = LifecycleCore;
        ReleaseHandle = LifecycleCore->OnCanonicalRuntimeReleasing.AddUObject(
            this, &UOGCanonicalCharacterPresentation::HandleCanonicalRuntimeReleasing);
    }
    return RefreshFromCanonicalStore(InContext, Store, Host, Error);
}

bool UOGCanonicalCharacterPresentation::Refresh(const FOGFoundationCharacterContext& InContext, FString& Error)
{
    if (!IsValid(BoundMesh))
    {
        Error = TEXT("Character presentation has no bound rig.");
        return false;
    }
    UWorld* World = GetWorld();
    UGameInstance* Instance = World ? World->GetGameInstance() : nullptr;
    UOGGameCoreSubsystem* Core = Instance ? Instance->GetSubsystem<UOGGameCoreSubsystem>() : nullptr;
    IOGWorldStore* Store = Core && Core->IsCoreReady() ? Core->GetWorldStore() : nullptr;
    if (!Store || !Store->IsOpen())
    {
        Error = TEXT("Canonical character store unavailable; retaining current visuals.");
        return false;
    }
    if (BoundCore.Get() != Core)
    {
        if (BoundCore.IsValid()) BoundCore->OnCanonicalRuntimeReleasing.Remove(ReleaseHandle);
        BoundCore = Core;
        ReleaseHandle = Core->OnCanonicalRuntimeReleasing.AddUObject(
            this, &UOGCanonicalCharacterPresentation::HandleCanonicalRuntimeReleasing);
    }
    return RefreshFromCanonicalStore(InContext, *Store, Core->GetOptionalPackageHost(), Error);
}

bool UOGCanonicalCharacterPresentation::RefreshFromCanonicalStore(const FOGFoundationCharacterContext& InContext,
    IOGWorldStore& Store, FOGOptionalPackageHost* Host, FString& Error)
{
    if (!IsValid(BoundMesh) || !Store.IsOpen())
    { Error = TEXT("Canonical character store or bound rig unavailable; current visuals retained."); return false; }
    InstallDefaultVisualResolver();
    FOGFoundationCharacterProjection NewProjection;
    if (!FOGFoundationCharacterRuntime(Store).Project(InContext, NewProjection, Error)) return false;
    Context = NewProjection.Context;
    Projection = MoveTemp(NewProjection);
    bHasProjection = true;
    DeriveVisualState();
    AssetStatus.Reset();
    TGuardValue<IOGWorldStore*> ResolverScope(DefaultResolverStore, &Store);
    if (VisualResolver)
    {
        FOGCharacterVisualBinding Binding;
        FString ResolveError;
        if (VisualResolver(Projection, bPrivacyPresentation, Binding, ResolveError)) ApplyBinding(Binding, Store, Host);
        else
        {
            AssetStatus = ResolveError.IsEmpty() ? TEXT("No authored visual binding; retaining current rig.") : ResolveError;
        }
    }
    ApplyMaterialState();
    OnProjectionChanged.Broadcast(Projection, bPrivacyPresentation);
    Error.Reset();
    return true;
}

bool UOGCanonicalCharacterPresentation::Refresh(FString& Error)
{
    return Refresh(Context, Error);
}

void UOGCanonicalCharacterPresentation::SetVisualResolver(FOGCharacterVisualResolver Resolver)
{
    bHasExplicitVisualResolver = true;
    VisualResolver = MoveTemp(Resolver);
    if (bHasProjection)
    {
        FString Error;
        Refresh(Error);
    }
}

void UOGCanonicalCharacterPresentation::SetPrivacyPresentation(bool bPrivacy)
{
    if (bPrivacyPresentation == bPrivacy) return;
    bPrivacyPresentation = bPrivacy;
    if (bHasProjection)
    {
        FString Error;
        if (!Refresh(Error))
        {
            // Privacy still projects material masking during a persistence outage.
            // An unsupported exact form must keep its current body/compatible material state.
            ApplyMaterialState();
            OnProjectionChanged.Broadcast(Projection, bPrivacyPresentation);
        }
    }
}

void UOGCanonicalCharacterPresentation::DeriveVisualState()
{
    VisualState = FOGCharacterVisualState();
    const TSharedPtr<FJsonObject> Entity = ObjectFromJson(Projection.EntityStateJson);
    VisualState.InjuryState = FName(*StringValue(Entity, TEXT("injury_state")));
    VisualState.InjurySeverity = BasisPointValue(Entity, TEXT("injury_severity_bps"), 0.f);
    VisualState.RestorationState = FName(*StringValue(Entity, TEXT("restoration_state")));
    VisualState.Restoration = BasisPointValue(Entity, TEXT("restoration_progress_bps"), 0.f);
    if (VisualState.InjuryState == FName(TEXT("restored"))) VisualState.Restoration = 1.f;
    if (Projection.bHasPresentation)
    {
        VisualState.OutfitStateJson = Projection.Presentation.OutfitStateJson;
        VisualState.VariantStateJson = Projection.Presentation.PresentationVariantStateJson;
        const auto Outfit = ObjectFromJson(VisualState.OutfitStateJson);
        const auto Variant = ObjectFromJson(VisualState.VariantStateJson);
        VisualState.OutfitDamage = BasisPointValue(Outfit, TEXT("damage_bps"), 0.f);
        VisualState.HairstyleId = StringValue(Variant, TEXT("hairstyle_id"));
    }
    for (const auto& Equipped : Projection.Equipment)
    {
        const FOGItemInstanceRecord* CurrentItem = &Equipped.Item;
        for (const auto& InventoryItem : Projection.Inventory)
        {
            if (InventoryItem.ItemId == Equipped.Item.ItemId)
            {
                CurrentItem = &InventoryItem;
                break;
            }
        }
        // Derive both projections from persisted basis points before rounding to
        // material floats. Subtracting an already-rounded float condition loses precision.
        const double Condition = Equipped.bBroken ? 0. :
            BasisPointValuePrecise(ObjectFromJson(CurrentItem->DurabilityStateJson), TEXT("condition_bps"), 1.);
        VisualState.EquippedCondition.Add(Equipped.Binding.SlotId, static_cast<float>(Condition));
        VisualState.EquipmentDamage = FMath::Max(VisualState.EquipmentDamage,
            static_cast<float>(1. - Condition));
    }
}

void UOGCanonicalCharacterPresentation::ApplyBinding(const FOGCharacterVisualBinding& Binding, IOGWorldStore& Store,
    FOGOptionalPackageHost* Host)
{
    if (!IsValid(BoundMesh)) return;
    if (OptionalAssetStore != &Store || OptionalAssetHost != Host)
    {
        // Release only after actual components stop referring to the old optional visuals.
        if (OptionalAssets) RestoreBaseline();
        CachedVisualAssets.Reset();
        OptionalAssets.Reset();
        OptionalAssetStore = &Store;
        OptionalAssetHost = Host;
        OptionalAssets = MakeUnique<FOGOptionalAssetRuntime>(Store, Host);
    }
    FOGOptionalAssetRuntime& Assets = *OptionalAssets;
    auto Load = [&](const FOGOptionalAssetReference& Ref) -> UObject*
    {
        if (!Ref.AssetPath.IsValid()) return nullptr;
        const FString Key = Ref.PackageId.ToString() + TEXT("|") + Ref.AssetPath.ToString();
        if (const auto* Existing = CachedVisualAssets.Find(Key); Existing && Existing->IsValid())
            return Existing->Get();
        UObject* Object = nullptr;
        FString Reason;
        if (!Assets.IsAvailable(Ref.PackageId, Reason) || !Assets.ActivateAndLoad(Ref, Object, Reason))
        {
            if (!AssetStatus.IsEmpty()) AssetStatus += TEXT("; ");
            AssetStatus += Reason;
            return nullptr;
        }
        CachedVisualAssets.Add(Key, Object);
        return Object;
    };
    UClass* AnimationClass = Cast<UClass>(Load(Binding.AnimationClass));
    if (AnimationClass && !AnimationClass->IsChildOf(UAnimInstance::StaticClass()))
    {
        AnimationClass = nullptr;
        AssetStatus += TEXT(" Invalid animation class.");
    }
    const bool SubstituteBody = bPrivacyPresentation && Binding.bSubstituteBodyForPrivacy;
    const auto& BodyRef = SubstituteBody ? Binding.PrivacyBody : Binding.Body;
    USkeletalMesh* Body = Cast<USkeletalMesh>(Load(BodyRef));
    if (SubstituteBody && !Body)
    {
        AssetStatus += TEXT(" Privacy body unavailable; current exact body retained with material masking.");
    }
    bool bBodyRejected = false;
    if (Body && Body != BoundMesh->GetSkeletalMeshAsset())
    {
        const USkeletalMesh* Current = BoundMesh->GetSkeletalMeshAsset();
        const bool SameSkeleton = Current && Current->GetSkeleton() && Current->GetSkeleton() == Body->GetSkeleton();
        const bool SameFamily = !Current || CurrentRigFamily.IsNone() || CurrentRigFamily == Binding.RigFamily;
        const UAnimBlueprintGeneratedClass* AnimBlueprint = Cast<UAnimBlueprintGeneratedClass>(AnimationClass);
        const bool AnimationCompatible = AnimBlueprint && Body->GetSkeleton() &&
            AnimBlueprint->GetTargetSkeleton() == Body->GetSkeleton();
        if (Binding.RigFamily.IsNone() || (!(SameSkeleton && SameFamily) && !AnimationCompatible))
        {
            AssetStatus += TEXT(" Body rig requires declared family and compatible animation; current body retained.");
            AnimationClass = nullptr;
            bBodyRejected = true;
        }
        else
        {
            BoundMesh->SetSkeletalMeshAsset(Body);
            CurrentRigFamily = Binding.RigFamily;
            DynamicMaterials.Reset();
        }
    }
    if (AnimationClass)
    {
        const auto* AnimBlueprint = Cast<UAnimBlueprintGeneratedClass>(AnimationClass);
        if (!AnimBlueprint || !BoundMesh->GetSkeletalMeshAsset() ||
            AnimBlueprint->GetTargetSkeleton() == BoundMesh->GetSkeletalMeshAsset()->GetSkeleton())
            BoundMesh->SetAnimInstanceClass(AnimationClass);
        else AssetStatus += TEXT(" Animation skeleton differs from the retained body.");
    }
    // Material/part overrides for another rig cannot leak onto the retained body.
    if (bBodyRejected || (!Binding.RigFamily.IsNone() && Binding.RigFamily != CurrentRigFamily)) return;
    TSet<int32> MaterialSlots;
    for (const auto& Entry : Binding.Materials) MaterialSlots.Add(Entry.Key);
    for (const auto& Entry : Binding.PrivacyMaterials) MaterialSlots.Add(Entry.Key);
    for (int32 Slot : MaterialSlots)
    {
        const auto* Privacy = bPrivacyPresentation ? Binding.PrivacyMaterials.Find(Slot) : nullptr;
        const auto* Normal = Binding.Materials.Find(Slot);
        const auto* Reference = Privacy ? Privacy : Normal;
        UMaterialInterface* Material = Reference ? Cast<UMaterialInterface>(Load(*Reference)) : nullptr;
        if (Material && Slot >= 0 && Slot < BoundMesh->GetNumMaterials())
            BoundMesh->SetMaterial(Slot, Material);
        // Missing optional references preserve the valid current material rather than
        // applying a baseline slot authored for a different body/form.
    }
    DynamicMaterials.Reset();
    RemoveParts();
    TSet<FName> Seen;
    for (const auto& Part : Binding.Parts)
    {
        if (Part.PartId.IsNone() || Seen.Contains(Part.PartId))
        {
            AssetStatus += TEXT(" Duplicate or unnamed modular visual part ignored.");
            continue;
        }
        Seen.Add(Part.PartId);
        if (!Part.RigFamily.IsNone() && Part.RigFamily != CurrentRigFamily)
        { AssetStatus += TEXT(" Modular rig incompatible; part omitted."); continue; }
        UObject* Asset = Load(bPrivacyPresentation && Part.bSubstituteForPrivacy ? Part.PrivacyAsset : Part.Asset);
        UMeshComponent* Component = nullptr;
        if (USkeletalMesh* PartMesh = Cast<USkeletalMesh>(Asset))
        {
            const bool SkeletonCompatible = BoundMesh->GetSkeletalMeshAsset() && PartMesh->GetSkeleton() &&
                PartMesh->GetSkeleton() == BoundMesh->GetSkeletalMeshAsset()->GetSkeleton();
            if (Part.bUseLeaderPose && !SkeletonCompatible)
            {
                AssetStatus += TEXT(" Modular rig incompatible; part omitted.");
                continue;
            }
            auto* SkeletalPart = NewObject<USkeletalMeshComponent>(GetOwner());
            SkeletalPart->SetSkeletalMeshAsset(PartMesh);
            if (Part.bUseLeaderPose) SkeletalPart->SetLeaderPoseComponent(BoundMesh);
            Component = SkeletalPart;
        }
        else if (UStaticMesh* StaticMesh = Cast<UStaticMesh>(Asset))
        {
            auto* StaticPart = NewObject<UStaticMeshComponent>(GetOwner());
            StaticPart->SetStaticMesh(StaticMesh);
            Component = StaticPart;
        }
        if (!Component) continue;
        Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Component->SetGenerateOverlapEvents(false);
        Component->SetCanEverAffectNavigation(false);
        Component->SetupAttachment(BoundMesh, Part.SocketName);
        Component->SetRelativeTransform(Part.RelativeTransform);
        GetOwner()->AddInstanceComponent(Component);
        Component->RegisterComponent();
        VisualParts.Add(Component);
    }
}

void UOGCanonicalCharacterPresentation::ApplyMaterialState()
{
    if (!IsValid(BoundMesh) || !bHasProjection) return;
    DynamicMaterials.SetNum(BoundMesh->GetNumMaterials());
    for (int32 Index = 0; Index < BoundMesh->GetNumMaterials(); ++Index)
    {
        UMaterialInstanceDynamic* Material = DynamicMaterials[Index];
        if (!Material || BoundMesh->GetMaterial(Index) != Material)
        {
            Material = Cast<UMaterialInstanceDynamic>(BoundMesh->GetMaterial(Index));
            if (!Material) Material = BoundMesh->CreateAndSetMaterialInstanceDynamic(Index);
            DynamicMaterials[Index] = Material;
        }
        if (!Material) continue;
        // Privacy changes the projection sent to materials, never the canonical payload.
        Material->SetScalarParameterValue(TEXT("OG_InjurySeverity"), bPrivacyPresentation ? 0.f : VisualState.InjurySeverity);
        Material->SetScalarParameterValue(TEXT("OG_EquipmentDamage"), VisualState.EquipmentDamage);
        Material->SetScalarParameterValue(TEXT("OG_OutfitDamage"), bPrivacyPresentation ? 0.f : VisualState.OutfitDamage);
        Material->SetScalarParameterValue(TEXT("OG_Restoration"), VisualState.Restoration);
        Material->SetScalarParameterValue(TEXT("OG_Privacy"), bPrivacyPresentation ? 1.f : 0.f);
        if (bUseNeutralDiagnosticTint)
        {
            const float Severity = bPrivacyPresentation ? VisualState.EquipmentDamage :
                FMath::Max(VisualState.InjurySeverity, VisualState.EquipmentDamage);
            Material->SetVectorParameterValue(DiagnosticTintParameter,
                FMath::Lerp(FLinearColor(.25f, .62f, .9f), FLinearColor(.95f, .48f, .12f), Severity));
        }
    }
}

void UOGCanonicalCharacterPresentation::RemoveParts()
{
    for (const auto& Part : VisualParts) if (IsValid(Part.Get())) Part->DestroyComponent();
    VisualParts.Reset();
}

void UOGCanonicalCharacterPresentation::RestoreBaseline()
{
    RemoveParts();
    if (IsValid(BoundMesh))
    {
        if (BoundMesh->GetSkeletalMeshAsset() != BaselineMesh) BoundMesh->SetSkeletalMeshAsset(BaselineMesh);
        if (BoundMesh->GetAnimClass() != BaselineAnimationClass.Get()) BoundMesh->SetAnimInstanceClass(BaselineAnimationClass);
        for (int32 Index = 0; Index < BaselineMaterials.Num(); ++Index)
            BoundMesh->SetMaterial(Index, BaselineMaterials[Index]);
    }
    DynamicMaterials.Reset();
    CurrentRigFamily = BoundRigFamily;
}

void UOGCanonicalCharacterPresentation::ClearBinding()
{
    RestoreBaseline();
    BoundMesh = nullptr;
    BaselineMesh = nullptr;
    BaselineAnimationClass = nullptr;
    BaselineMaterials.Reset();
    CachedVisualAssets.Reset();
    OptionalAssets.Reset();
    OptionalAssetStore = nullptr;
    OptionalAssetHost = nullptr;
    DefaultResolverStore = nullptr;
    if (BoundCore.IsValid()) BoundCore->OnCanonicalRuntimeReleasing.Remove(ReleaseHandle);
    ReleaseHandle.Reset();
    BoundCore.Reset();
    Projection = FOGFoundationCharacterProjection();
    Context = FOGFoundationCharacterContext();
    VisualState = FOGCharacterVisualState();
    bHasProjection = false;
    AssetStatus.Reset();
}

void UOGCanonicalCharacterPresentation::HandleCanonicalRuntimeReleasing()
{
    ClearBinding();
}

void UOGCanonicalCharacterPresentation::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    ClearBinding();
    OnProjectionChanged.Clear();
    VisualResolver = nullptr;
    Super::EndPlay(EndPlayReason);
}
