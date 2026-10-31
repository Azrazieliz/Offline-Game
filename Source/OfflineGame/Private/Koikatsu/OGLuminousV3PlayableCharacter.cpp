#include "Koikatsu/OGLuminousV3PlayableCharacter.h"
#include "Interaction/OGInteractionParticipantComponent.h"
#include "Runtime/OGCanonicalCharacterPresentation.h"

#include "Koikatsu/OGLuminousV3WardrobeQAWidget.h"
#include "Animation/OGSharedMotionLibrary.h"
#include "Animation/OGSharedMotionControllerComponent.h"
#include "Combat/OGCanonicalCombatActorBindingComponent.h"
#include "Combat/OGTurnBattle.h"

#include "Animation/AnimSequence.h"
#include "Blueprint/UserWidget.h"
#include "Components/InputComponent.h"
#include "Animation/Skeleton.h"
#include "Components/SkeletalMeshComponent.h"
#include "Combat/OGDiagnosticCombatComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/GameInstance.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Runtime/OGFoundationCharacterRuntime.h"
#include "Runtime/OGGameCoreSubsystem.h"
#include "Runtime/OGPresentationModeSubsystem.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
const TCHAR* WardrobeName(EOGLuminousV3Wardrobe State)
{
    switch (State)
    {
    case EOGLuminousV3Wardrobe::Clothed: return TEXT("CLOTHED");
    case EOGLuminousV3Wardrobe::Underwear: return TEXT("UNDERWEAR");
    case EOGLuminousV3Wardrobe::Nude: return TEXT("NUDE");
    }
    return TEXT("INVALID");
}
// The additional three approved Koikatsu character Versions have real,
// independently exported source rigs, never the frozen Base/Alter meshes.
// These identifiers do not grant canonical ownership or fabricate content.
FString AdditionalLuminousSourceFolder(const FString& Version)
{
    if (Version == TEXT("character:luminous_valentine.version_awakened_v4"))
        return TEXT("Awakened");
    if (Version == TEXT("character:luminous_valentine.version_berserker_v4"))
        return TEXT("Berserker");
    if (Version == TEXT("character:luminous_valentine.version_grand_v4"))
        return TEXT("Grand");
    return FString();
}
bool ParseWardrobe(const FString& Value, EOGLuminousV3Wardrobe& Out)
{
    if (Value == TEXT("CLOTHED")) { Out = EOGLuminousV3Wardrobe::Clothed; return true; }
    if (Value == TEXT("UNDERWEAR")) { Out = EOGLuminousV3Wardrobe::Underwear; return true; }
    if (Value == TEXT("NUDE")) { Out = EOGLuminousV3Wardrobe::Nude; return true; }
    return false;
}
#if !UE_BUILD_SHIPPING
struct FNativeQAPreviewRecord
{
    FName MotionId = NAME_None;
    FString NativeAssetPath;
    FString DisplayName;
    bool bLoop = false;
};
void ReadSourceAuthenticQAMotions(FName Outfit, TArray<FNativeQAPreviewRecord>& Out)
{
    Out.Reset();
    const FString CatalogPath = FPaths::Combine(FPaths::ProjectContentDir(),
        TEXT("NoWorld/Animation/SharedMotionCatalog.json"));
    FString Json;
    if (!FFileHelper::LoadFileToString(Json, *CatalogPath)) return;
    TSharedPtr<FJsonObject> Root;
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid()) return;
    const TArray<TSharedPtr<FJsonValue>>* Entries = nullptr;
    if (!Root->TryGetArrayField(TEXT("motions"), Entries) || !Entries) return;
    for (const TSharedPtr<FJsonValue>& Value : *Entries)
    {
        const TSharedPtr<FJsonObject> Item =
            Value.IsValid() ? Value->AsObject() : nullptr;
        if (!Item.IsValid()) continue;
        bool bApproved = true;
        FString Id, Rig, Wardrobe, Native;
        if (!Item->TryGetBoolField(TEXT("approved_for_gameplay"), bApproved) ||
            !Item->TryGetStringField(TEXT("motion_id"), Id) ||
            !Item->TryGetStringField(TEXT("rig"), Rig) ||
            !Item->TryGetStringField(TEXT("outfit"), Wardrobe) ||
            !Item->TryGetStringField(TEXT("native_sequence"), Native) ||
            bApproved || Rig != TEXT("koikatsu_original") ||
            Wardrobe != Outfit.ToString() ||
            !Native.StartsWith(TEXT("/Game/NoWorld/Characters/Luminous/Alter/KoikatsuOriginalStudioMotionQA/")) ||
            Native.Contains(TEXT(".."))) continue;
        FNativeQAPreviewRecord& Record = Out.AddDefaulted_GetRef();
        Record.MotionId = FName(*Id);
        Record.NativeAssetPath = Native;
        if (!Item->TryGetStringField(TEXT("source_original_name"), Record.DisplayName))
            Item->TryGetStringField(TEXT("action"), Record.DisplayName);
        if (Record.DisplayName.IsEmpty()) Record.DisplayName = Id;
        Item->TryGetBoolField(TEXT("loop"), Record.bLoop);
    }
}
#endif

bool SameBoneTopologyAndRest(const USkeletalMesh* A, const USkeletalMesh* B)
{
    if (!A || !B) return false;
    const FReferenceSkeleton& Left = A->GetRefSkeleton();
    const FReferenceSkeleton& Right = B->GetRefSkeleton();
    // The source-authentic additional forms use their own exported rigs:
    // Awakened 1131, Berserker 886, and Grand 1123 bones after the FBX root
    // is admitted. Never compare them through Base/Alter's 975-bone proxy.
    if (Left.GetNum() != Right.GetNum() ||
        (Left.GetNum() != 975 && Left.GetNum() != 886 &&
         Left.GetNum() != 1131 && Left.GetNum() != 1123)) return false;
    for (int32 Index = 0; Index < Left.GetNum(); ++Index)
    {
        if (Left.GetBoneName(Index) != Right.GetBoneName(Index) ||
            Left.GetParentIndex(Index) != Right.GetParentIndex(Index) ||
            !Left.GetRefBonePose()[Index].Equals(Right.GetRefBonePose()[Index], 0.0001f))
            return false;
    }
    return true;
}
}

FOGLuminousV3AppearanceProjection FOGLuminousV3AppearancePolicy::Resolve(
    EOGLuminousV3Wardrobe Actual,
    bool bPrivacySfw,
    bool bExplicitContextAuthorized,
    bool bSafeClothedAssetAvailable)
{
    FOGLuminousV3AppearanceProjection Result;
    Result.ActualWardrobe = Actual;
    Result.bPrivacySfwEnabled = bPrivacySfw;
    Result.bExplicitContextAuthorized = bExplicitContextAuthorized;
    Result.bDefaultBaseAppearance = Actual == EOGLuminousV3Wardrobe::Clothed;
    // This is a pure read-only function: no store, equipment or world writes.
    switch (Actual)
    {
    case EOGLuminousV3Wardrobe::Clothed:
        Result.EffectiveRenderedWardrobe = EOGLuminousV3Wardrobe::Clothed;
        Result.bRenderable = bSafeClothedAssetAvailable;
        return Result;
    case EOGLuminousV3Wardrobe::Underwear:
    case EOGLuminousV3Wardrobe::Nude:
        if (bPrivacySfw || !bExplicitContextAuthorized)
        {
            Result.EffectiveRenderedWardrobe = EOGLuminousV3Wardrobe::Clothed;
            Result.bMaskedForPrivacyOrUnavailableAdultContext = true;
            Result.bRenderable = bSafeClothedAssetAvailable;
            return Result;
        }
        Result.EffectiveRenderedWardrobe = Actual;
        Result.bRenderable = true;
        return Result;
    default:
        Result.bRenderable = false; // Unsupported asset: never display an old explicit pose.
        return Result;
    }
}

AOGLuminousV3PlayableCharacter::AOGLuminousV3PlayableCharacter()
{
    PrimaryActorTick.bCanEverTick = true;
    SharedMotionController = CreateDefaultSubobject<UOGSharedMotionControllerComponent>(TEXT("NativeSharedMotionController"));
    CanonicalCombatBody = CreateDefaultSubobject<UOGCanonicalCombatActorBindingComponent>(TEXT("CanonicalCombatNativeBody"));
    SharedMotionController->RigProfile = TEXT("koikatsu_original");
    // Attach the WHOLE current character to one shared library. Source
    // garments and body parts need not all share identical bone topology.
    SharedMotionController->bUseIdenticalTopologyLeaderPose = false;
    SharedMotionController->bGroupPartsByReferencePose = true;
    // Existing Gate11 allocated DESIGN identifiers, verified from the original
    // preserved intake. Assignment here does NOT install a Registry package,
    // grant a Manifestation or declare this asset production-accepted.
    CanonicalLuminousIdentityId = TEXT("character:luminous_valentine.identity");
    CanonicalLuminousVersionId = TEXT("character:luminous_valentine.version_ln_base");
    // Additional form contracts are populated from each admitted wardrobe's
    // source package. Export counts never imply that another wardrobe exists.
    MotionCapabilities = {TEXT("grounded"), TEXT("swimming"), TEXT("flying"),
        TEXT("combat"), TEXT("emote"), TEXT("adult_interaction")};
}

const TArray<FSoftObjectPath>& AOGLuminousV3PlayableCharacter::PartsFor(EOGLuminousV3Wardrobe State) const
{
    switch (State)
    {
    case EOGLuminousV3Wardrobe::Underwear: return UnderwearNativeParts;
    case EOGLuminousV3Wardrobe::Nude: return NudeNativeParts;
    default: return ClothedNativeParts;
    }
}

int32 AOGLuminousV3PlayableCharacter::ExpectedNativeBoneCount(
    EOGLuminousV3Wardrobe State) const
{
    // The accepted original Nude Koikatsu native export has an independent
    // 1122-bone authoring rig (1123 after FBX root); it MUST NOT borrow
    // CLOTHED or UNDERWEAR's 1131-bone source skeleton.
    if (CanonicalLuminousVersionId ==
        TEXT("character:luminous_valentine.version_alter_v1"))
        return State == EOGLuminousV3Wardrobe::Nude ? 1123 : 1131;
    if (!AdditionalLuminousSourceFolder(CanonicalLuminousVersionId).IsEmpty())
    {
        // Each wardrobe retains its own measured source contract. An
        // unavailable wardrobe remains zero and cannot borrow another rig.
        const int32 Index = static_cast<int32>(State);
        return AdditionalVersionNativeSkeletonBones.IsValidIndex(Index)
            ? AdditionalVersionNativeSkeletonBones[Index] : 0;
    }
    return 975;
}

int32 AOGLuminousV3PlayableCharacter::ExpectedPartsFor(EOGLuminousV3Wardrobe State) const
{
    if (CanonicalLuminousVersionId == TEXT("character:luminous_valentine.version_alter_v1"))
        return State == EOGLuminousV3Wardrobe::Clothed ||
               State == EOGLuminousV3Wardrobe::Underwear ||
               State == EOGLuminousV3Wardrobe::Nude ? 6 : 0;
    if (!AdditionalLuminousSourceFolder(CanonicalLuminousVersionId).IsEmpty())
    {
        const int32 Index = static_cast<int32>(State);
        return AdditionalVersionNativePartCounts.IsValidIndex(Index)
            ? FMath::Max(0, AdditionalVersionNativePartCounts[Index]) : 0;
    }
    switch (State)
    {
    case EOGLuminousV3Wardrobe::Clothed: return 45;
    case EOGLuminousV3Wardrobe::Underwear: return 39;
    case EOGLuminousV3Wardrobe::Nude: return 37;
    }
    return 0;
}

bool AOGLuminousV3PlayableCharacter::ValidateWardrobeAssets(
    EOGLuminousV3Wardrobe Desired, FString& Error) const
{
    const TArray<FSoftObjectPath>& Paths = PartsFor(Desired);
    if (ExpectedPartsFor(Desired) <= 0 ||
        Paths.Num() != ExpectedPartsFor(Desired))
    {
        Error = FString::Printf(TEXT("Luminous V3: wardrobe %s requires %d exact parts, got %d"),
            WardrobeName(Desired), ExpectedPartsFor(Desired), Paths.Num());
        return false;
    }
    TSet<FString> UniquePaths;
    for (int32 Index = 0; Index < Paths.Num(); ++Index)
    {
        const FString AssetPath = Paths[Index].ToString();
        if (AssetPath.IsEmpty() || UniquePaths.Contains(AssetPath))
        {
            Error = FString::Printf(TEXT("Luminous V3: invalid/duplicate source index %d"), Index);
            return false;
        }
        UniquePaths.Add(AssetPath);
        const bool bBaseSource=CanonicalLuminousVersionId==
            TEXT("character:luminous_valentine.version_ln_base") &&
            AssetPath.StartsWith(TEXT("/Game/Experimental/LuminousValentineV3/"));
        const bool bAlterSource=CanonicalLuminousVersionId==
            TEXT("character:luminous_valentine.version_alter_v1") &&
            ((Desired == EOGLuminousV3Wardrobe::Clothed &&
              AssetPath.StartsWith(TEXT("/Game/NoWorld/Characters/Luminous/Alter/NativeOriginalClothed/"))) ||
             (Desired == EOGLuminousV3Wardrobe::Underwear &&
              AssetPath.StartsWith(TEXT("/Game/NoWorld/Characters/Luminous/Alter/NativeOriginalUnderwear/"))) ||
             (Desired == EOGLuminousV3Wardrobe::Nude &&
              AssetPath.StartsWith(TEXT("/Game/NoWorld/Characters/Luminous/Alter/NativeOriginalNude/"))));
        const FString Form = AdditionalLuminousSourceFolder(CanonicalLuminousVersionId);
        const TCHAR* StateFolder = Desired == EOGLuminousV3Wardrobe::Clothed
            ? TEXT("Clothed") : Desired == EOGLuminousV3Wardrobe::Underwear
            ? TEXT("Underwear") : TEXT("Nude");
        const FString VersionNamespace = Form.IsEmpty() ? FString() :
            FString::Printf(TEXT("/Game/NoWorld/Characters/Luminous/%s/NativeOriginal%s/"),
                *Form, StateFolder);
        const bool bNewOriginalSource = !VersionNamespace.IsEmpty() &&
            AssetPath.StartsWith(VersionNamespace);
        if (!bBaseSource && !bAlterSource && !bNewOriginalSource)
        {
            Error = TEXT("Luminous V3: refusing an unexpected or cross-character asset namespace");
            return false;
        }
    }
    return true;
}

bool AOGLuminousV3PlayableCharacter::InitializeNativeWardrobeForIsolatedQA(
    EOGLuminousV3Wardrobe Desired)
{
#if WITH_EDITOR
    // No fake test meshes, no Python-only registration, no canonical writes:
    // the exact production native wardrobe assembly runs on the existing BP.
    // This entry point cannot initialize an owned character or run in PIE,
    // regular gameplay worlds or a shipped game.
    if (bNativeOwnedCharacterBound || !GetWorld() ||
        GetWorld()->WorldType != EWorldType::Editor) return false;
    FString Error;
    if (!ApplyVisualWardrobe(Desired, Error))
    {
        LastLuminousError = Error;
        return false;
    }
    CurrentWardrobe = Desired;
    EffectiveAppearance.ActualWardrobe = Desired;
    EffectiveAppearance.bRenderable = true;
    // Allow genuine nonpersistent Editor QA immediately, without attempting
    // to edit EditDefaultsOnly Blueprint fields on a spawned instance.
    bTransientEditorNativeQASession = true;
    return true;
#else
    return false;
#endif
}

bool AOGLuminousV3PlayableCharacter::ApplyVisualWardrobe(
    EOGLuminousV3Wardrobe Desired, FString& Error)
{
    if (!ValidateWardrobeAssets(Desired, Error)) return false;
    const auto& Paths = PartsFor(Desired);
    // Retain the existing pawn, capsule, camera, input and combat components.
    // Prepare the exact full state first; do not publish a half-loaded wardrobe.
    TArray<USkeletalMesh*> Prepared;
    Prepared.Reserve(Paths.Num());
    for (int32 Index = 0; Index < Paths.Num(); ++Index)
    {
        USkeletalMesh* NativeMesh = Cast<USkeletalMesh>(Paths[Index].TryLoad());
        if (!NativeMesh || !NativeMesh->GetSkeleton() ||
            NativeMesh->GetRefSkeleton().GetNum() != ExpectedNativeBoneCount(Desired))
        {
            Error = FString::Printf(TEXT("Luminous V3: missing/invalid source rig %d bones at index %d state %s"),
                ExpectedNativeBoneCount(Desired), Index, WardrobeName(Desired));
            return false;
        }
        Prepared.Add(NativeMesh);
    }
    // One component pool reused for all three states. Never instantiate 121
    // simultaneous 975-bone character parts or touch imported source UAssets.
    const int32 RequiredPartPool = FMath::Max3(
        ExpectedPartsFor(EOGLuminousV3Wardrobe::Clothed),
        ExpectedPartsFor(EOGLuminousV3Wardrobe::Underwear),
        ExpectedPartsFor(EOGLuminousV3Wardrobe::Nude));
    while (RuntimeParts.Num() < RequiredPartPool)
    {
        const int32 Index = RuntimeParts.Num();
        const FName Name(*FString::Printf(TEXT("LuminousV3NativePart_%02d"), Index));
        USkeletalMeshComponent* Component = NewObject<USkeletalMeshComponent>(this, Name);
        if (!Component) { Error = TEXT("Luminous V3: skeletal component allocation failed"); return false; }
        Component->SetupAttachment(GetRootComponent());
        Component->SetRelativeLocation(FVector(0.f, 0.f, -96.f));
        // MEASURED Luminous source axis: eyesâ†’nose = glTF +Z
        // (CLOTHED, UNDERWEAR, NUDE: +Z dot 0.9999999946). Native UE glTF
        // importer maps +Z to mesh +Y. Unreal movement actor uses +X front.
        // A component-space -90 yaw maps imported visual +Y to actor +X.
        // This is an isolated reversible presentation transform; NEVER
        // rewrite original glTF, imported skeleton, native clips or root.
        Component->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
        Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Component->SetCastShadow(true);
        Component->SetAnimationMode(EAnimationMode::AnimationSingleNode);
        AddInstanceComponent(Component);
        Component->RegisterComponent();
        RuntimeParts.Add(Component);
    }
    for (int32 Index = 0; Index < RuntimeParts.Num(); ++Index)
    {
        USkeletalMeshComponent* Component = RuntimeParts[Index];
        Component->SetLeaderPoseComponent(nullptr);
        Component->Stop();
        Component->SetSkeletalMeshAsset(Prepared.IsValidIndex(Index) ? Prepared[Index] : nullptr);
        Component->SetVisibility(Prepared.IsValidIndex(Index), true);
    }
    ActivePartCount = Prepared.Num();
    // Register the exact current wardrobe under one reusable character-independent
    // motion controller; changing clothes never duplicates the catalog.
    if (SharedMotionController)
    {
        TArray<USkeletalMeshComponent*> Native;
        for (int32 I = 0; I < ActivePartCount; ++I) Native.Add(RuntimeParts[I]);
        // Enforce one whole-character binding at runtime. Some preserved
        // native Blueprints serialize old controller defaults, so constructor
        // settings alone cannot guarantee the approved rig-cohort path.
        SharedMotionController->bUseIdenticalTopologyLeaderPose = false;
        SharedMotionController->bGroupPartsByReferencePose = true;
        SharedMotionController->RigProfile = MotionRigProfile;
        SharedMotionController->AnatomyProfile = MotionAnatomyProfile;
        SharedMotionController->Capabilities = MotionCapabilities;
        SharedMotionController->SharedLibrary = SharedMotionLibrary;
        SharedMotionController->SetCurrentOutfit(FName(WardrobeName(Desired)));
        if (!SharedMotionController->RegisterNativeParts(Native))
        {
            Error = TEXT("Shared motion component could not register actual wardrobe meshes");
            return false;
        }
    }
    // Assign only the rendered geometry. Actual selected clothing is owned by
    // CurrentWardrobe, never by the Privacy/SFW presentation resolver.
    EffectiveAppearance.EffectiveRenderedWardrobe = Desired;
    CurrentNativeClip = nullptr;
    ActiveMotionLeaderIndex = INDEX_NONE;
    bCompatibleNativeMotion = false;
    CurrentMotionPhase = EOGLuminousV3MotionPhase::Idle;
    bGate09EmotePlaying = false;
    Gate09EmoteEndTime = 0.f;
    bLuminousReady = true;
    if (GetMesh()) GetMesh()->SetHiddenInGame(true);
    UE_LOG(LogTemp, Display, TEXT("Gate12 Luminous: native wardrobe=%s parts=%d skeleton=%d"),
        WardrobeName(Desired), ActivePartCount,ExpectedNativeBoneCount(Desired));
    return true;
}

bool AOGLuminousV3PlayableCharacter::IsIsolatedWardrobeQAContext() const
{
#if UE_BUILD_SHIPPING
    return false;
#else
#if WITH_EDITOR
    if (bTransientEditorNativeQASession && !bNativeOwnedCharacterBound &&
        GetWorld() && GetWorld()->WorldType == EWorldType::Editor)
        return true;
#endif
    return bRunNonPersistentWardrobeQACycle || bRunNativeMotionQACycle ||
        bEnableNativeWardrobeQATouchPanel || bEnterWorldModeOnIsolatedQAStartup;
#endif
}

void AOGLuminousV3PlayableCharacter::ConcealUnsafeVisualParts()
{
    for (USkeletalMeshComponent* Part : RuntimeParts)
    {
        if (Part) Part->SetVisibility(false, true);
    }
}

bool AOGLuminousV3PlayableCharacter::RefreshPrivacyRenderedAppearance()
{
    // Only the rendered representation is changed by Privacy. The actual
    // wardrobe, equipment, identity, Manifestation and canonical DB remain
    // completely untouched by this method.
    const bool bPrivacy = bNativeOwnedCharacterBound
        ? bOwnedPresentationSfw : GetSfwPresentation();
    // Real owned Manifestations may choose the available authentic source
    // wardrobe, while an explicit SFW presentation preference continues to
    // mask non-clothed appearance without modifying actual clothing state.
    const bool bQAAuthorized = bNativeOwnedCharacterBound ||
        IsIsolatedWardrobeQAContext();
    // Zero existing parts is never a valid safe wardrobe. Partial authentic
    // imports may exist for a newly authored Version; never substitute a
    // naked/underwear part when its clothed source has not been imported.
    const int32 RequiredClothed = ExpectedPartsFor(EOGLuminousV3Wardrobe::Clothed);
    const bool bHasSafeBase = RequiredClothed > 0 &&
        ClothedNativeParts.Num() == RequiredClothed;
    const FOGLuminousV3AppearanceProjection Decision =
        FOGLuminousV3AppearancePolicy::Resolve(
            CurrentWardrobe, bPrivacy, bQAAuthorized, bHasSafeBase);

    if (!Decision.bRenderable)
    {
        ConcealUnsafeVisualParts();
        EffectiveAppearance = Decision;
        LastLuminousError =
            TEXT("No safe/authorized Luminous presentation can be rendered.");
        return false;
    }

    if (!bLuminousReady ||
        Decision.EffectiveRenderedWardrobe != EffectiveAppearance.EffectiveRenderedWardrobe ||
        ActivePartCount != ExpectedPartsFor(Decision.EffectiveRenderedWardrobe))
    {
        FString Error;
        if (!ApplyVisualWardrobe(Decision.EffectiveRenderedWardrobe, Error))
        {
            ConcealUnsafeVisualParts();
            LastLuminousError = Error;
            return false;
        }
    }

    EffectiveAppearance = Decision;
    bLastAppliedPrivacy = bPrivacy;
    bLastAppliedExplicitContext = bQAAuthorized;
    UE_LOG(LogTemp, Display,
        TEXT("G12_LUMINOUS_PRIVACY_RENDER_ONLY actual=%s effective=%s sfw=%d masked=%d"),
        WardrobeName(CurrentWardrobe),
        WardrobeName(EffectiveAppearance.EffectiveRenderedWardrobe),
        bPrivacy ? 1 : 0,
        Decision.bMaskedForPrivacyOrUnavailableAdultContext ? 1 : 0);
    return true;
}

bool AOGLuminousV3PlayableCharacter::ResolveCanonicalControlledContext(
    FOGFoundationCharacterContext& OutContext, FString& Error) const
{
    OutContext = FOGFoundationCharacterContext();
    if (CanonicalLuminousIdentityId.IsEmpty() || CanonicalLuminousVersionId.IsEmpty())
    {
        Error = TEXT("No approved Luminous canonical Identity/Version IDs on this exact World actor.");
        return false;
    }
    if (bNativeOwnedCharacterBound)
    {
        if (BoundOwnedIdentity.ToString() != CanonicalLuminousIdentityId ||
            BoundOwnedVersion.ToString() != CanonicalLuminousVersionId)
        {
            Error = TEXT("Owned Luminous binding no longer matches this authored World pawn.");
            return false;
        }
        OutContext.EntityId = BoundOwnedManifestation;
        OutContext.ManifestationId = BoundOwnedManifestation;
        OutContext.OwnerEntityId = BoundOwnedRuler;
        return true;
    }
    const UGameInstance* GI = GetGameInstance();
    const UOGGameCoreSubsystem* Core = GI ? GI->GetSubsystem<UOGGameCoreSubsystem>() : nullptr;
    if (!Core || !Core->IsCoreReady() || !Core->GetWorldStore() || Core->GetCanonicalWorldTick() < 0)
    {
        Error = TEXT("Canonical world store is not available");
        return false;
    }
    const UOGDiagnosticCombatComponent* Combat = FindComponentByClass<UOGDiagnosticCombatComponent>();
    if (!Combat || !Combat->IsActive())
    {
        Error = TEXT("No active selected party context; wardrobe selection not persisted");
        return false;
    }
    FOGEntityId Entity;
    FOGContentId Identity;
    bool bHasManifestation = false;
    FOGCharacterManifestationRecord Manifestation;
    if (!Combat->TryGetControlledContext(Entity, Identity, bHasManifestation, Manifestation, Error)) return false;
    if (Identity.ToString() != CanonicalLuminousIdentityId ||
        !bHasManifestation || Manifestation.ActiveVersionId.ToString() != CanonicalLuminousVersionId)
    {
        Error = TEXT("Current selected canonical character is not Luminous Valentine V3; refusing cross-identity save");
        return false;
    }
    OutContext.EntityId = Entity;
    OutContext.ManifestationId = Entity;
    OutContext.OwnerEntityId = Manifestation.OwningRulerId;
    return true;
}

bool AOGLuminousV3PlayableCharacter::PersistCanonicalWardrobe(
    EOGLuminousV3Wardrobe Desired, FString& Error)
{
    FOGFoundationCharacterContext Context;
    if (!ResolveCanonicalControlledContext(Context, Error)) return false;
    UOGGameCoreSubsystem* Core = GetGameInstance()->GetSubsystem<UOGGameCoreSubsystem>();
    FOGFoundationCharacterRuntime Runtime(*Core->GetWorldStore());
    FOGFoundationCharacterProjection Projection;
    if (!Runtime.Project(Context, Projection, Error)) return false;
    const FString Json = Projection.bHasPresentation ? Projection.Presentation.OutfitStateJson : TEXT("{}");
    TSharedPtr<FJsonObject> Outfit;
    if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json.IsEmpty() ? TEXT("{}") : Json), Outfit) ||
        !Outfit.IsValid())
    {
        Error = TEXT("Existing canonical outfit JSON invalid; preserving it without overwrite");
        return false;
    }
    Outfit->SetStringField(TEXT("luminous_v3_wardrobe"), WardrobeName(Desired));
    FString Updated;
    FJsonSerializer::Serialize(Outfit.ToSharedRef(), TJsonWriterFactory<>::Create(&Updated));
    FOGManifestationPresentationStateRecord Presentation =
        Projection.bHasPresentation ? Projection.Presentation : FOGManifestationPresentationStateRecord();
    Presentation.OutfitStateJson = Updated;
    if (Presentation.PresentationVariantStateJson.IsEmpty()) Presentation.PresentationVariantStateJson = TEXT("{}");
    return Runtime.SetPresentation(Context, Presentation, Core->GetCanonicalWorldTick(), Error);
}

bool AOGLuminousV3PlayableCharacter::RestoreCanonicalWardrobe()
{
    FString Error;
    FOGFoundationCharacterContext Context;
    if (!ResolveCanonicalControlledContext(Context, Error)) return false;
    UOGGameCoreSubsystem* Core = GetGameInstance()->GetSubsystem<UOGGameCoreSubsystem>();
    FOGFoundationCharacterRuntime Runtime(*Core->GetWorldStore());
    FOGFoundationCharacterProjection Projection;
    if (!Runtime.Project(Context, Projection, Error) || !Projection.bHasPresentation) return false;
    TSharedPtr<FJsonObject> Outfit;
    if (!FJsonSerializer::Deserialize(
        TJsonReaderFactory<>::Create(Projection.Presentation.OutfitStateJson), Outfit) || !Outfit.IsValid())
        return false;
    FString Value;
    EOGLuminousV3Wardrobe Saved;
    if (!Outfit->TryGetStringField(TEXT("luminous_v3_wardrobe"), Value) || !ParseWardrobe(Value, Saved))
        return false;
    const EOGLuminousV3Wardrobe Before = CurrentWardrobe;
    CurrentWardrobe = Saved;
    if (!RefreshPrivacyRenderedAppearance())
    {
        CurrentWardrobe = Before;
        return false;
    }
    return true;
}

bool AOGLuminousV3PlayableCharacter::SetLuminousWardrobe(
    EOGLuminousV3Wardrobe Desired, bool bPersist)
{
    LastLuminousError.Reset();
    if (ExpectedPartsFor(Desired) == 0) { LastLuminousError = TEXT("Unknown wardrobe"); return false; }
    // Persistence preflight before visual switch: do not display successful
    // canonical selection when registry and selected party are not authorized.
    if (bPersist)
    {
        FOGFoundationCharacterContext Context;
        if (!ResolveCanonicalControlledContext(Context, LastLuminousError)) return false;
    }
    // Production explicit-asset eligibility must come from accepted Gate11
    // character lore/asset contracts. No writable Blueprint override or guessed
    // maturity flag is trusted. Only isolated engineering QA may preview them.
    if (Desired != EOGLuminousV3Wardrobe::Clothed &&
        !bNativeOwnedCharacterBound && !IsIsolatedWardrobeQAContext())
    {
        LastLuminousError =
            TEXT("Outfit selection requires an actual owned Character or isolated engineering QA.");
        return false;
    }
    if (!ValidateWardrobeAssets(Desired, LastLuminousError)) return false;

    const EOGLuminousV3Wardrobe Before = CurrentWardrobe;
    if (bGate09EmotePlaying)
        InterruptGate09AuthoredEmote();
    // Wardrobe skeleton changes invalidate any current native clip binding.
    bSharedActionPlaying = false;
    SharedActionEndTime = 0.f;
    if (SharedMotionController) SharedMotionController->StopAction();
    CurrentWardrobe = Desired;
    if (!RefreshPrivacyRenderedAppearance())
    {
        const FString FailedRenderError = LastLuminousError;
        CurrentWardrobe = Before;
        RefreshPrivacyRenderedAppearance();
        LastLuminousError = FailedRenderError;
        return false;
    }
    if (bPersist && !PersistCanonicalWardrobe(Desired, LastLuminousError))
    {
        const FString FailedSaveError = LastLuminousError;
        CurrentWardrobe = Before;
        RefreshPrivacyRenderedAppearance();
        LastLuminousError = FailedSaveError;
        return false;
    }
    return true;
}

bool AOGLuminousV3PlayableCharacter::SetLuminousNativeMotionClip(UAnimSequence* Clip, bool bLoop)
{
    LastLuminousError.Reset();
    if (!Clip || !bLuminousReady || ActivePartCount < 4 || !RuntimeParts.IsValidIndex(3))
    {
        LastLuminousError = TEXT("Motion V3 requires a native Luminous character and animation asset");
        return false;
    }
    // Gate09 cold-import receipts show the authored clips reference the
    // Mesh000Prim000 CLOTHED Skeleton. The old hard-coded part[3] leader
    // pointed at a DIFFERENT native skeleton and would reject all four
    // verified clips. Match the actual authored skeleton, never retarget.
    USkeletalMeshComponent* Leader = nullptr;
    int32 LeaderIndex = INDEX_NONE;
    for (int32 Index = 0; Index < ActivePartCount; ++Index)
    {
        USkeletalMeshComponent* Candidate =
            RuntimeParts.IsValidIndex(Index) ? RuntimeParts[Index] : nullptr;
        if (Candidate && Candidate->GetSkeletalMeshAsset() &&
            Candidate->GetSkeletalMeshAsset()->GetSkeleton() ==
                Clip->GetSkeleton())
        {
            Leader = Candidate;
            LeaderIndex = Index;
            break;
        }
    }
    if (!Leader)
    {
        LastLuminousError =
            TEXT("Native Gate09 clip has no exact skeleton in this rendered wardrobe");
        return false;
    }
    for (int32 Index = 0; Index < ActivePartCount; ++Index)
    {
        const USkeletalMeshComponent* Component = RuntimeParts[Index];
        if (!Component || !SameBoneTopologyAndRest(
                Leader->GetSkeletalMeshAsset(), Component->GetSkeletalMeshAsset()))
        {
            LastLuminousError = FString::Printf(
                TEXT("Cannot share Motion V3 leader pose with differing native rest rig part=%d"), Index);
            return false;
        }
    }
    // Release any shared-controller action before the established direct native path.
    if (SharedMotionController && SharedMotionController->IsPlayingAction())
        SharedMotionController->StopAction();
    // Release a former leader before assigning the current clip's actual driver.
    Leader->SetLeaderPoseComponent(nullptr);
    for (int32 Index = 0; Index < ActivePartCount; ++Index)
        if (Index != LeaderIndex) RuntimeParts[Index]->SetLeaderPoseComponent(Leader);
    Leader->SetAnimationMode(EAnimationMode::AnimationSingleNode);
    Leader->PlayAnimation(Clip, bLoop);
    CurrentNativeClip = Clip;
    ActiveMotionLeaderIndex = LeaderIndex;
    bCompatibleNativeMotion = true;
    UE_LOG(LogTemp, Display,
        TEXT("G12_LUMINOUS_NATIVE_MOTION_STARTED=%s skeleton=%s matching_leader_part=%d"),
        *Clip->GetName(), *Clip->GetSkeleton()->GetPathName(),LeaderIndex);
    return true;
}

UAnimSequence* AOGLuminousV3PlayableCharacter::ResolveSharedMotion(
    FName GameplayAction, FName Context, bool bAuthorizedAdultContext,
    int32 ParticipantCount, const FOGSharedMotionEntry** OutEntry) const
{
    if (!bLuminousReady || RuntimeParts.IsEmpty()) return nullptr;
    // One content asset for every character; retarget derivatives remain
    // separate records pointing back to the same original SourceClipId.
    const UOGSharedMotionLibrary* Library = SharedMotionLibrary.IsNull()
        ? nullptr : SharedMotionLibrary.LoadSynchronous();
    if (!Library)
        Library = FOGSharedMotionResolver::GetInstalledLibrary();
    if (!Library) return nullptr;
    // Pure animation resolution must not use wardrobe privacy to suppress
    // approved actions. The effective mesh still applies the existing SFW
    // visual mask; canonical action authority is handled before this call.
    return FOGSharedMotionResolver::Resolve(
        Library, GameplayAction, MotionRigProfile, MotionAnatomyProfile,
        FName(WardrobeName(EffectiveAppearance.EffectiveRenderedWardrobe)),
        Context, MotionCapabilities, RuntimeParts, bAuthorizedAdultContext,
        ParticipantCount, OutEntry);
}

bool AOGLuminousV3PlayableCharacter::PlaySharedGameplayAction(
    FName GameplayAction, bool bAuthorizedAdultContext, int32 ParticipantCount)
{
    const FOGSharedMotionEntry* Entry = nullptr;
    const FString ActionName = GameplayAction.ToString();
    // The caller's semantic action, not the character Version, determines
    // the shared native presentation domain. Traversal used to be
    // incorrectly routed into Interaction and could never resolve.
    const FName Context = ActionName.StartsWith(TEXT("Combat."))
        ? FName(TEXT("Combat")) : ActionName.StartsWith(TEXT("Traversal."))
        ? FName(TEXT("Traversal")) :
          (ActionName.StartsWith(TEXT("AdultInteraction.")) ||
           ActionName.StartsWith(TEXT("Adult.")))
        ? FName(TEXT("AdultInteraction")) : FName(TEXT("Interaction"));
    // The controller owns ALL currently rendered parts and can choose one
    // exact native clip per compatible rig cohort or genuine reference
    // motion. Do not reject an entire character because a legacy single-
    // skeleton lookup cannot resolve every original wardrobe mesh at once.
    const bool bControllerPlayed = SharedMotionController &&
        SharedMotionController->PlayAction(GameplayAction, Context,
            bAuthorizedAdultContext, ParticipantCount);
    if (bControllerPlayed)
    {
        CurrentNativeClip = nullptr;
        ActiveMotionLeaderIndex = INDEX_NONE;
        bCompatibleNativeMotion = true;
        bSharedActionPlaying = true;
        // The shared controller has its own authored motion lifecycle.
        // Never invent a clip duration when it plays a reference packet.
        SharedActionEndTime = 0.f;
        UE_LOG(LogTemp, Display,
            TEXT("G12_SHARED_WHOLE_CHARACTER_ACTION=%s source=%s parts=%d"),
            *GameplayAction.ToString(),
            *SharedMotionController->CurrentSourceClipId.ToString(),
            SharedMotionController->GetNativePartCount());
        return true;
    }
    // Preserve the established exact native Luminous playback fallback,
    // without substituting a different character or cross-form clip.
    UAnimSequence* Clip = ResolveSharedMotion(GameplayAction, Context,
        bAuthorizedAdultContext, ParticipantCount, &Entry);
    if (!Clip || !Entry || !SetLuminousNativeMotionClip(Clip, Entry->bLoop))
        return false;
    bSharedActionPlaying = true;
    SharedActionEndTime = GetWorld()->GetTimeSeconds() +
        FMath::Max(0.02f, Clip->GetPlayLength() / FMath::Max(0.01f, Entry->PlayRate));
    UE_LOG(LogTemp, Display,
        TEXT("G12_SHARED_LEGACY_FALLBACK_ACTION=%s source=%s native=%s"),
        *GameplayAction.ToString(), *Entry->SourceClipId.ToString(),
        *Clip->GetPathName());
    return true;
}

TArray<FName> AOGLuminousV3PlayableCharacter::GetNativeQAPreviewMotionIds() const
{
    TArray<FName> Result;
#if !UE_BUILD_SHIPPING
    if (!bLuminousReady || !IsIsolatedWardrobeQAContext() ||
        bNativeOwnedCharacterBound) return Result;
    TArray<FNativeQAPreviewRecord> Candidates;
    ReadSourceAuthenticQAMotions(
        FName(WardrobeName(EffectiveAppearance.EffectiveRenderedWardrobe)), Candidates);
    Result.Reserve(Candidates.Num());
    for (const FNativeQAPreviewRecord& Candidate : Candidates)
        Result.Add(Candidate.MotionId);
#endif
    return Result;
}

TArray<FString> AOGLuminousV3PlayableCharacter::GetNativeQAPreviewMotionNames() const
{
    TArray<FString> Result;
#if !UE_BUILD_SHIPPING
    if (!bLuminousReady || !IsIsolatedWardrobeQAContext() ||
        bNativeOwnedCharacterBound) return Result;
    TArray<FNativeQAPreviewRecord> Candidates;
    ReadSourceAuthenticQAMotions(
        FName(WardrobeName(EffectiveAppearance.EffectiveRenderedWardrobe)), Candidates);
    Result.Reserve(Candidates.Num());
    for (const FNativeQAPreviewRecord& Candidate : Candidates)
        Result.Add(Candidate.DisplayName);
#endif
    return Result;
}

bool AOGLuminousV3PlayableCharacter::PreviewNativeQAMotionById(FName MotionId)
{
#if !UE_BUILD_SHIPPING
    if (MotionId.IsNone() || !bLuminousReady ||
        !IsIsolatedWardrobeQAContext() || bNativeOwnedCharacterBound)
        return false;
    TArray<FNativeQAPreviewRecord> Candidates;
    ReadSourceAuthenticQAMotions(
        FName(WardrobeName(EffectiveAppearance.EffectiveRenderedWardrobe)), Candidates);
    for (const FNativeQAPreviewRecord& Candidate : Candidates)
    {
        if (Candidate.MotionId != MotionId) continue;
        UAnimSequence* Clip = LoadObject<UAnimSequence>(
            nullptr, *Candidate.NativeAssetPath);
        if (!Clip || !SetLuminousNativeMotionClip(Clip, Candidate.bLoop))
            return false; // Every actual rendered native part must pass rig topology.
        bSharedActionPlaying = true;
        SharedActionEndTime = GetWorld()
            ? GetWorld()->GetTimeSeconds() + Clip->GetPlayLength() : 0.f;
        UE_LOG(LogTemp, Display,
            TEXT("G12_ISOLATED_REAL_MOTION_PREVIEW clip=%s id=%s parts=%d NOT_GAMEPLAY_APPROVED"),
            *Clip->GetPathName(), *MotionId.ToString(), ActivePartCount);
        return true;
    }
#endif
    return false;
}

bool AOGLuminousV3PlayableCharacter::PlayGate09AuthoredEmote(
    EOGLuminousV3AuthoredEmote Emote)
{
    // The user explicitly rejected the dance's current artistic motion
    // quality. Keep its native source in the isolated existing Gate09 QA map
    // instead of marketing unaccepted choreography as a gameplay animation.
    if(Emote==EOGLuminousV3AuthoredEmote::DancePerformance &&
       !IsIsolatedWardrobeQAContext())
    {
        LastLuminousError=TEXT(
            "Gate09 dance source is imported but awaits visual acceptance; "
            "only native QA can preview it.");
        return false;
    }
    if (!bLuminousReady ||
        EffectiveAppearance.EffectiveRenderedWardrobe != EOGLuminousV3Wardrobe::Clothed ||
        (GetCharacterMovement() &&
         (!GetCharacterMovement()->IsMovingOnGround() ||
          GetVelocity().SizeSquared2D() > FMath::Square(12.f))))
    {
        LastLuminousError = TEXT("Gate09 native emotes require stationary Luminous Base CLOTHED");
        return false;
    }
    static const TCHAR* Paths[] = {
        TEXT("/Game/Gate09_QA_20261019/TestSingle/G09_LuminousV3_Idle_01_Thoughtful.G09_LuminousV3_Idle_01_Thoughtful"),
        TEXT("/Game/Gate09_QA_20261019/NativeKoikatsu/G09_LuminousV3_Idle_02_Greeting.G09_LuminousV3_Idle_02_Greeting"),
        TEXT("/Game/Gate09_QA_20261019/NativeKoikatsu/G09_LuminousV3_Idle_03_Expressive.G09_LuminousV3_Idle_03_Expressive"),
        TEXT("/Game/Gate09_QA_20261019/NativeKoikatsu/G09_LuminousV3_Emote_Bow_Source.G09_LuminousV3_Emote_Bow_Source"),
        TEXT("/Game/Gate12NativeKoikatsu20261020/FlightDanceSwimming/G12_LuminousV3_Koikatsu_Dance_Performance.G12_LuminousV3_Koikatsu_Dance_Performance"),
    };
    const int32 Index = static_cast<int32>(Emote);
    static const FName SharedEmotes[] = {
        TEXT("Emote.Thoughtful"), TEXT("Emote.Greeting"),
        TEXT("Emote.Expressive"), TEXT("Emote.Bow"), TEXT("Emote.Dance")
    };
    // Shared library takes priority once a source passes actual gameplay
    // approval; existing native emotes stay as a conservative fallback.
    if (Index >= 0 && Index < UE_ARRAY_COUNT(SharedEmotes) &&
        Emote != EOGLuminousV3AuthoredEmote::DancePerformance)
    {
        const FOGSharedMotionEntry* Entry = nullptr;
        if (UAnimSequence* Shared = ResolveSharedMotion(
            SharedEmotes[Index], FName(TEXT("Interaction")), false, 1, &Entry))
        {
            const bool bControllerPlayed = SharedMotionController &&
                SharedMotionController->PlayAction(SharedEmotes[Index],
                    TEXT("Interaction"), false, 1);
            if (bControllerPlayed || SetLuminousNativeMotionClip(Shared, Entry && Entry->bLoop))
            {
                if (bControllerPlayed)
                {
                    CurrentNativeClip = Shared;
                    ActiveMotionLeaderIndex = INDEX_NONE;
                    bCompatibleNativeMotion = true;
                }
                bGate09EmotePlaying = true;
                Gate09EmoteEndTime = GetWorld()->GetTimeSeconds() +
                    Shared->GetPlayLength() / FMath::Max(0.01f, Entry ? Entry->PlayRate : 1.f);
                return true;
            }
        }
    }
    if (Index < 0 || Index >= UE_ARRAY_COUNT(Paths))
    {
        LastLuminousError = TEXT("Unknown Gate09 native emote");
        return false;
    }
    UAnimSequence* Clip = LoadObject<UAnimSequence>(nullptr, Paths[Index]);
    if (!Clip || !SetLuminousNativeMotionClip(Clip, false))
        return false; // Existing strict reference-skeleton / pose checks.
    bGate09EmotePlaying = true;
    Gate09EmoteEndTime = GetWorld()->GetTimeSeconds() + Clip->GetPlayLength();
    UE_LOG(LogTemp,Display,TEXT("G12_GATE09_EMOTE_STARTED index=%d clip=%s"),
        Index,*Clip->GetPathName());
    return true;
}
void AOGLuminousV3PlayableCharacter::InterruptGate09AuthoredEmote()
{
    if (!bGate09EmotePlaying) return;
    if (SharedMotionController && SharedMotionController->IsPlayingAction())
        SharedMotionController->StopAction();
    if (RuntimeParts.IsValidIndex(ActiveMotionLeaderIndex) && RuntimeParts[ActiveMotionLeaderIndex])
        RuntimeParts[ActiveMotionLeaderIndex]->Stop();
    bGate09EmotePlaying = false;
    Gate09EmoteEndTime = 0.f;
    CurrentNativeClip = nullptr;
    CurrentMotionPhase = EOGLuminousV3MotionPhase::Idle;
}
bool AOGLuminousV3PlayableCharacter::PlayResolvedTraversal(
    EOGLuminousV3MotionPhase Phase, UAnimSequence* Clip, bool bLoop)
{
    if (!Clip) return false;
    FName Action = NAME_None;
    switch (Phase)
    {
    case EOGLuminousV3MotionPhase::Walk: Action = TEXT("Traversal.Walk"); break;
    case EOGLuminousV3MotionPhase::Run: Action = TEXT("Traversal.Run"); break;
    case EOGLuminousV3MotionPhase::Jump: Action = TEXT("Traversal.Jump"); break;
    case EOGLuminousV3MotionPhase::Swimming: Action = TEXT("Traversal.Swim"); break;
    case EOGLuminousV3MotionPhase::Flying:
        Action = bLoop ? FName(TEXT("Traversal.FlightHover")) :
            FName(TEXT("Traversal.FlightTakeoff"));
        break;
    default: break;
    }
    // One animation controller is shared by every version and wardrobe. The
    // existing native direct driver remains for authentic old clips not yet
    // catalogued; exact skeleton checks apply in both paths.
    if (!Action.IsNone() && SharedMotionController &&
        SharedMotionController->PlayAction(Action, TEXT("Traversal"), false, 1))
    {
        CurrentNativeClip = Clip;
        ActiveMotionLeaderIndex = INDEX_NONE;
        bCompatibleNativeMotion = true;
        return true;
    }
    return SetLuminousNativeMotionClip(Clip, bLoop);
}

UAnimSequence* AOGLuminousV3PlayableCharacter::ResolveAuthoredMotionClip(
    EOGLuminousV3MotionPhase Phase) const
{
    // Shared source action resolves to an exact native skeletal derivative.
    FName SharedAction = NAME_None;
    switch (Phase)
    {
    case EOGLuminousV3MotionPhase::Walk: SharedAction = TEXT("Traversal.Walk"); break;
    case EOGLuminousV3MotionPhase::Run: SharedAction = TEXT("Traversal.Run"); break;
    case EOGLuminousV3MotionPhase::Jump: SharedAction = TEXT("Traversal.Jump"); break;
    case EOGLuminousV3MotionPhase::Swimming: SharedAction = TEXT("Traversal.Swim"); break;
    case EOGLuminousV3MotionPhase::Flying: SharedAction = TEXT("Traversal.FlightHover"); break;
    default: break;
    }
    if (!SharedAction.IsNone())
        if (UAnimSequence* Shared = ResolveSharedMotion(
                SharedAction, FName(TEXT("Traversal")), false, 1))
            return Shared;

    // Existing genuine per-wardrobe clips remain functional as fallback.
    // These are genuine installed Koikatsu-compatible source animations
    // imported onto the exact skeleton of the currently rendered form.
    // Wardrobe-skeleton mismatch never falls through to another appearance.
    // Flight and swimming do not borrow Walk/Run ground gait.
    if (Phase == EOGLuminousV3MotionPhase::Flying ||
        Phase == EOGLuminousV3MotionPhase::Swimming)
    {
        if (EffectiveAppearance.EffectiveRenderedWardrobe != EOGLuminousV3Wardrobe::Clothed)
            return nullptr;
        const bool bFlight = Phase == EOGLuminousV3MotionPhase::Flying;
        const bool bHover = bFlight && GetVelocity().SizeSquared() < FMath::Square(35.0f);
        const TCHAR* Path = bFlight
            ? (bHover
                ? TEXT("/Game/Gate12NativeKoikatsu20261020/FlightDanceSwimming/G12_LuminousV3_Koikatsu_Flight_Hover.G12_LuminousV3_Koikatsu_Flight_Hover")
                : TEXT("/Game/Gate12NativeKoikatsu20261020/FlightDanceSwimming/G12_LuminousV3_Koikatsu_Flight_Start.G12_LuminousV3_Koikatsu_Flight_Start"))
            : TEXT("/Game/Gate12NativeKoikatsu20261020/FlightDanceSwimming/G12_LuminousV3_Koikatsu_Swimming_Stroke.G12_LuminousV3_Koikatsu_Swimming_Stroke");
        return LoadObject<UAnimSequence>(nullptr, Path);
    }
    const TArray<FSoftObjectPath>* Clips = nullptr;
    switch (Phase)
    {
    case EOGLuminousV3MotionPhase::Walk: Clips = &NativeWalkByWardrobe; break;
    case EOGLuminousV3MotionPhase::Run: Clips = &NativeRunByWardrobe; break;
    case EOGLuminousV3MotionPhase::Jump: Clips = &NativeJumpByWardrobe; break;
    default: return nullptr;
    }
    // Animation must follow the ACTUALLY RENDERED form skeleton, which
    // can differ from the real clothing state under Privacy/SFW masking.
    const int32 State = static_cast<int32>(EffectiveAppearance.EffectiveRenderedWardrobe);
    if (!Clips || Clips->Num() != 3 || !Clips->IsValidIndex(State)) return nullptr;
    // Legacy per-actor Blueprint paths are a compatibility fallback only.
    // Do not branch the animation library on a particular Version; the same
    // character controller resolves the action for any actual native skeleton.
    // Preserve exact-skeleton matching even for the legacy fallback.
    UAnimSequence* Sequence = Cast<UAnimSequence>((*Clips)[State].TryLoad());
    if (!Sequence || !Sequence->GetSkeleton()) return nullptr;
    for (const USkeletalMeshComponent* Part : RuntimeParts)
    {
        const USkeletalMesh* Mesh = Part ? Part->GetSkeletalMeshAsset() : nullptr;
        if (Mesh && Mesh->GetSkeleton() == Sequence->GetSkeleton())
            return Sequence;
    }
    return nullptr;
}

void AOGLuminousV3PlayableCharacter::UpdateNativeMotionFromTraversal()
{
    if (!bLuminousReady) return;
    const UCharacterMovementComponent* Movement = GetCharacterMovement();
    EOGLuminousV3MotionPhase Desired = EOGLuminousV3MotionPhase::Idle;
    if (Movement && Movement->IsFlying())
        Desired = EOGLuminousV3MotionPhase::Flying;
    else if (Movement && Movement->IsSwimming())
        Desired = EOGLuminousV3MotionPhase::Swimming;
    else if (Movement && Movement->IsFalling())
        Desired = GetVelocity().Z > 0.f ?
            EOGLuminousV3MotionPhase::Jump : EOGLuminousV3MotionPhase::Fall;
    else
    {
        const FVector Horizontal(GetVelocity().X, GetVelocity().Y, 0.0f);
        if (Horizontal.SizeSquared2D() > FMath::Square(12.0f))
        {
            const FVector LocalVelocity = GetActorTransform().InverseTransformVectorNoScale(Horizontal);
            // Only source-authorised forward CMU gait is available today;
            // never replay forward gait while travelling backwards/sideways.
            if (LocalVelocity.X > 0.0f &&
                FMath::Abs(LocalVelocity.Y) <= LocalVelocity.X * 0.65f)
                Desired = IsSprinting() || Horizontal.Size2D() > 430.0f ?
                    EOGLuminousV3MotionPhase::Run : EOGLuminousV3MotionPhase::Walk;
        }
    }
    // Gate09 verified flight-start, hover and swim are three distinct
    // Koikatsu source clips, not a single looping generic traversal gait.
    if(Desired==EOGLuminousV3MotionPhase::Flying)
    {
        const float Now=GetWorld()?GetWorld()->GetTimeSeconds():0.f;
        if(!bGate09TakeoffAlreadyPlayedThisFlight)
        {
            bGate09TakeoffAlreadyPlayedThisFlight=true;
            // Exact original character rig and Koikatsu asset; never synthesize
            // flight from a generic Walk, Run, ground Jump or dance.
            UAnimSequence* Takeoff=ResolveSharedMotion(
                FName(TEXT("Traversal.FlightTakeoff")), FName(TEXT("Traversal")), false, 1);
            if(!Takeoff)
                Takeoff=LoadObject<UAnimSequence>(nullptr,
                    TEXT("/Game/Gate12NativeKoikatsu20261020/FlightDanceSwimming/"
                         "G12_LuminousV3_Koikatsu_Flight_Start."
                         "G12_LuminousV3_Koikatsu_Flight_Start"));
            if(Takeoff && PlayResolvedTraversal(EOGLuminousV3MotionPhase::Flying,Takeoff,false))
                Gate09TakeoffEndsAt=Now+Takeoff->GetPlayLength();
            else
            {
                Gate09TakeoffEndsAt=Now;
                UE_LOG(LogTemp,Warning,
                    TEXT("GATE09_FLIGHT_TAKEOFF_NOT_PLAYABLE native clip missing or mismatch: %s"),
                    *LastLuminousError);
            }
            CurrentMotionPhase=Desired;
            return;
        }
        if(Now<Gate09TakeoffEndsAt)
            return; // Preserve the real single flight-start motion to its end.

        if(GetVelocity().SizeSquared()<FMath::Square(35.f))
        {
            if(UAnimSequence* Hover=ResolveAuthoredMotionClip(Desired))
                if(Hover!=CurrentNativeClip)
                    PlayResolvedTraversal(EOGLuminousV3MotionPhase::Flying,Hover,true);
        }
        else if(CurrentNativeClip)
        {
            // No approved moving-flight loop exists yet. Do not repeat
            // takeoff, or depict fake forward flight using stationary hover.
            if (SharedMotionController && SharedMotionController->IsPlayingAction())
                SharedMotionController->StopAction();
            if (RuntimeParts.IsValidIndex(ActiveMotionLeaderIndex) && RuntimeParts[ActiveMotionLeaderIndex])
                RuntimeParts[ActiveMotionLeaderIndex]->Stop();
            CurrentNativeClip=nullptr;
        }
        CurrentMotionPhase=Desired;
        return;
    }
    if(bGate09TakeoffAlreadyPlayedThisFlight)
    {
        bGate09TakeoffAlreadyPlayedThisFlight=false;
        Gate09TakeoffEndsAt=0.f; // Landing, fall or actual swimming ends flight.
    }
    if (Desired == CurrentMotionPhase)
        return;
    CurrentMotionPhase = Desired;
    if (Desired == EOGLuminousV3MotionPhase::Idle ||
        Desired == EOGLuminousV3MotionPhase::Fall)
    {
        if (SharedMotionController && SharedMotionController->IsPlayingAction())
            SharedMotionController->StopAction();
        if (RuntimeParts.IsValidIndex(ActiveMotionLeaderIndex) && RuntimeParts[ActiveMotionLeaderIndex])
            RuntimeParts[ActiveMotionLeaderIndex]->Stop();
        CurrentNativeClip = nullptr;
        UE_LOG(LogTemp, Display,
            TEXT("G12_LUMINOUS_MOTION_STATE_NEUTRAL=%d no_wrong_ground_gait_in_flight_or_swim"),
            static_cast<int32>(Desired));
        return;
    }
    UAnimSequence* Sequence = ResolveAuthoredMotionClip(Desired);
    if (!Sequence || !PlayResolvedTraversal(
        Desired, Sequence, Desired != EOGLuminousV3MotionPhase::Jump))
    {
        // Safe fallback if currently worn wardrobe does not share this native
        // skeleton. Never animate a different 975-bone skeleton by mistake.
        if (RuntimeParts.IsValidIndex(ActiveMotionLeaderIndex) &&
            RuntimeParts[ActiveMotionLeaderIndex])
            RuntimeParts[ActiveMotionLeaderIndex]->Stop();
        UE_LOG(LogTemp, Warning, TEXT("G12_LUMINOUS_MOTION_NATIVE_MISSING_OR_WRONG_RIG state=%d: %s"),
            static_cast<int32>(Desired), *LastLuminousError);
    }
}

void AOGLuminousV3PlayableCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);
    if (!PlayerInputComponent) return;
    PlayerInputComponent->BindKey(EKeys::Gamepad_DPad_Right, IE_Pressed,
        this, &AOGLuminousV3PlayableCharacter::CycleLuminousWardrobeFromGamepad);
    PlayerInputComponent->BindKey(EKeys::T, IE_Pressed,
        this, &AOGLuminousV3PlayableCharacter::CycleLuminousWardrobeFromGamepad);
}

void AOGLuminousV3PlayableCharacter::CycleLuminousWardrobeFromGamepad()
{
    if (!IsIsolatedWardrobeQAContext() || !bEnableNativeWardrobeQATouchPanel) return;
    const int32 NextIndex = (static_cast<int32>(CurrentWardrobe) + 1) % 3;
    const bool bSuccess = SetLuminousWardrobe(
        static_cast<EOGLuminousV3Wardrobe>(NextIndex), false);
    UE_LOG(LogTemp, Display,
        TEXT("G12_LUMINOUS_GAMEPAD_WARDROBE_CYCLE=%d result=%s"),
        NextIndex, bSuccess ? TEXT("PASS") : TEXT("FAIL"));
}

bool AOGLuminousV3PlayableCharacter::BindOwnedWorldCharacter_Implementation(
    const FOGEntityId& RulerId, const FOGContentId& IdentityId,
    const FOGContentId& VersionId, const FOGEntityId& ManifestationId,
    bool bRequestedSfwPresentation)
{
    LastLuminousError.Reset();
    if (bNativeOwnedCharacterBound || !bLuminousReady ||
        !RulerId.IsValid() || !IdentityId.IsValid() || !VersionId.IsValid() ||
        !ManifestationId.IsValid() ||
        CanonicalLuminousIdentityId != IdentityId.ToString() ||
        CanonicalLuminousVersionId != VersionId.ToString())
    {
        LastLuminousError=TEXT("Exact accepted Luminous Identity/Version or native asset binding unavailable.");
        return false;
    }
    UGameInstance* GI=GetGameInstance();
    UOGGameCoreSubsystem* Core=GI ? GI->GetSubsystem<UOGGameCoreSubsystem>() : nullptr;
    IOGWorldStore* Store=Core ? Core->GetWorldStore() : nullptr;
    if (!Core || !Core->IsCoreReady() || !Store || !Store->IsOpen())
    {
        LastLuminousError=TEXT("Canonical ownership database unavailable.");
        return false;
    }
    FOGCharacterManifestationRecord Owned;
    bool bFound=false;
    if (!Store->TryReadCharacterManifestation(
            ManifestationId,bFound,Owned,LastLuminousError) ||
        !bFound || Owned.OwningRulerId!=RulerId ||
        Owned.IdentityId!=IdentityId || Owned.ActiveVersionId!=VersionId ||
        Owned.LifecycleState!=FName(TEXT("active")))
    {
        if (LastLuminousError.IsEmpty())
            LastLuminousError=TEXT("Luminous is not an active Manifestation of this Ruler.");
        return false;
    }
    // Attach this canonical CHARACTER once to its single shared controller.
    // All of its Versions reuse the same semantic motion bank; registered
    // native parts choose the compatible skeleton derivative automatically.
    if (!SharedMotionController ||
        !SharedMotionController->BindCanonicalCharacter(IdentityId))
    {
        LastLuminousError=TEXT("Canonical character motion identity is incompatible with the live native actor.");
        return false;
    }
    BoundOwnedRuler=RulerId;
    BoundOwnedIdentity=IdentityId;
    BoundOwnedVersion=VersionId;
    BoundOwnedManifestation=ManifestationId;
    bNativeOwnedCharacterBound=true;
    bOwnedPresentationSfw=bRequestedSfwPresentation;
    CurrentWardrobe=EOGLuminousV3Wardrobe::Clothed;
    // Missing presentation state on a newly acquired Manifestation naturally
    // uses its source-authored CLOTHED default. Existing owned attire is read
    // from persistent state; no new state is written during possession.
    RestoreCanonicalWardrobe();
    if (!RefreshPrivacyRenderedAppearance())
    {
        bNativeOwnedCharacterBound=false;
        BoundOwnedRuler=FOGEntityId();
        BoundOwnedManifestation=FOGEntityId();
        BoundOwnedIdentity=FOGContentId();
        BoundOwnedVersion=FOGContentId();
        return false;
    }
    if (auto* Participant = FindComponentByClass<UOGInteractionParticipantComponent>())
    {
        TArray<USkeletalMeshComponent*> Parts; UOGSharedMotionControllerComponent* Motion = nullptr;
        FString BindingError;
        if (!UOGInteractionParticipantComponent::DiscoverNativeBinding(this, Parts, Motion, BindingError) || !Motion)
        { LastLuminousError = BindingError; bNativeOwnedCharacterBound = false; return false; }
        {
            const FString Rig = Motion->RigProfile.ToString().ToLower();
            const FOGContentId Family = FOGContentId(Rig).IsValid() ? FOGContentId(Rig) : FOGContentId(TEXT("rig:") + Rig);
            if (auto* Presenter = FindComponentByClass<UOGCanonicalCharacterPresentation>())
                Presenter->bUseNeutralDiagnosticTint = false;
            if (!Participant->BindActualNativeParticipant(ManifestationId, Family, BindingError))
            { LastLuminousError = BindingError; bNativeOwnedCharacterBound = false; return false; }
        }
    }
    return true;
}

void AOGLuminousV3PlayableCharacter::BeginPlay()
{
    Super::BeginPlay();
    CurrentWardrobe = InitialWardrobe;
    if (!RefreshPrivacyRenderedAppearance())
    {
        UE_LOG(LogTemp, Error, TEXT("Gate12 Luminous native startup failed: %s"), *LastLuminousError);
        return;
    }
    // Only applies if the selected saved canonical character is *actually*
    // this Gate11 Luminous identity; it cannot overwrite a Ruler/another copy.
    RestoreCanonicalWardrobe();
    if ((bRunNonPersistentWardrobeQACycle || bEnterWorldModeOnIsolatedQAStartup) &&
        bLuminousReady && GetWorld())
        GetWorldTimerManager().SetTimer(QAWorldModeTimer, this,
            &AOGLuminousV3PlayableCharacter::ActivateNativeQAWorldMode, 1.25f, false);
}

void AOGLuminousV3PlayableCharacter::ActivateNativeQAWorldMode()
{
    if ((!bRunNonPersistentWardrobeQACycle && !bEnterWorldModeOnIsolatedQAStartup) ||
        !bLuminousReady || !GetWorld()) return;
    UGameInstance* GI = GetGameInstance();
    UOGPresentationModeSubsystem* Presentation = GI ? GI->GetSubsystem<UOGPresentationModeSubsystem>() : nullptr;
    if (!Presentation)
    {
        UE_LOG(LogTemp, Error, TEXT("G12_LUMINOUS_NATIVE_QA_WORLD_MODE=FAIL_NO_SUBSYSTEM"));
        return;
    }
    Presentation->SetMode(EOGPresentationMode::World);
    UE_LOG(LogTemp, Display, TEXT("G12_LUMINOUS_NATIVE_QA_WORLD_MODE=%s"),
        Presentation->GetMode() == EOGPresentationMode::World ? TEXT("PASS") : TEXT("FAIL"));
    QANextCycleSeconds = GetWorld()->GetTimeSeconds() + 3.0f;
}

void AOGLuminousV3PlayableCharacter::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!bLuminousReady || !GetWorld()) return;
    const bool bEffectiveSfw = bNativeOwnedCharacterBound
        ? bOwnedPresentationSfw : GetSfwPresentation();
    if (bEffectiveSfw != bLastAppliedPrivacy ||
        (bNativeOwnedCharacterBound || IsIsolatedWardrobeQAContext()) != bLastAppliedExplicitContext)
    {
        // Device Privacy is non-authoritative, so toggling it cannot call
        // SetLuminousWardrobe(bPersist=true) or modify the canonical store.
        RefreshPrivacyRenderedAppearance();
    }
    if (!UE_BUILD_SHIPPING && IsIsolatedWardrobeQAContext() &&
        bEnableNativeWardrobeQATouchPanel && !WardrobeQAWidget &&
        IsLocallyControlled() && GetController())
    {
        WardrobeQAWidget = CreateWidget<UOGLuminousV3WardrobeQAWidget>(
            GetWorld(), UOGLuminousV3WardrobeQAWidget::StaticClass());
        if (WardrobeQAWidget)
        {
            WardrobeQAWidget->SetTargetCharacter(this);
            WardrobeQAWidget->AddToViewport(70);
            UE_LOG(LogTemp, Display, TEXT("G12_LUMINOUS_TOUCH_WARDROBE_PANEL=CREATED"));
        }
    }
    const float Now = GetWorld()->GetTimeSeconds();
    // An owned Luminous whose native geometry is already loaded becomes the
    // physical actor of the existing GameCore battle when one starts later.
    // The binding remains purely visual and never creates combat units.
    if (bNativeOwnedCharacterBound && CanonicalCombatBody &&
        Now>=NextCanonicalCombatBodySyncSeconds)
    {
        NextCanonicalCombatBodySyncSeconds=Now+0.6f;
        UGameInstance* GI=GetGameInstance();
        const UOGGameCoreSubsystem* Core=GI
            ? GI->GetSubsystem<UOGGameCoreSubsystem>() : nullptr;
        const FOGTurnBattle* Battle=Core?Core->GetOwnedLuminousTurnBattle():nullptr;
        if (Battle && Battle->GetState().Status==EOGTurnBattleStatus::Running &&
            !CanonicalCombatBody->IsBoundTo(Battle->GetState().BattleId,
                BoundOwnedManifestation))
        {
            FString Error;
            if (!CanonicalCombatBody->BindActualNativeUnit(
                BoundOwnedManifestation,BoundOwnedIdentity,Error))
                UE_LOG(LogTemp,Verbose,TEXT("Owned Luminous combat visual binding pending: %s"),*Error);
        }
        else if (!Battle || Battle->GetState().Status!=EOGTurnBattleStatus::Running)
            CanonicalCombatBody->UnbindVisual();
    }
    if (const auto* Participant = FindComponentByClass<UOGInteractionParticipantComponent>();
        Participant && Participant->IsInInteractionPresentation()) return;
    if (bGate09EmotePlaying)
    {
        const UCharacterMovementComponent* Move = GetCharacterMovement();
        if (Now >= Gate09EmoteEndTime ||
            (Move && (!Move->IsMovingOnGround() ||
              GetVelocity().SizeSquared2D() > FMath::Square(12.f))))
            InterruptGate09AuthoredEmote();
        else
            return; // Allow the entire authored pose to play, without Idle resetting it.
    }
    if (bSharedActionPlaying)
    {
        if (Now < SharedActionEndTime ||
            (SharedActionEndTime <= 0.f && SharedMotionController &&
             SharedMotionController->IsPlayingAction()))
            return; // Shared source lifecycle, not guessed Version-specific timing.
        bSharedActionPlaying = false;
        SharedActionEndTime = 0.f;
        if (SharedMotionController && SharedMotionController->IsPlayingAction())
            SharedMotionController->StopAction();
        CurrentNativeClip = nullptr;
        CurrentMotionPhase = EOGLuminousV3MotionPhase::Idle;
    }
    if (bRunNonPersistentWardrobeQACycle && QACycleStep < 3 &&
        Now >= QANextCycleSeconds)
    {
        const EOGLuminousV3Wardrobe Target = QACycleStep == 0 ? EOGLuminousV3Wardrobe::Underwear :
            QACycleStep == 1 ? EOGLuminousV3Wardrobe::Nude : EOGLuminousV3Wardrobe::Clothed;
        const bool bSuccess = SetLuminousWardrobe(Target, false);
        UE_LOG(LogTemp, Display, TEXT("G12_LUMINOUS_NATIVE_QA_WARDROBE_STEP=%d target=%s result=%s native_parts=%d error=%s"),
            QACycleStep, WardrobeName(Target), bSuccess ? TEXT("PASS") : TEXT("FAIL"),
            ActivePartCount, *LastLuminousError);
        if (!bSuccess) { QACycleStep = 3; return; }
        ++QACycleStep;
        QANextCycleSeconds = Now + 3.0f;
    }
    // Only QA map: after the three wardrobe transitions, prove three genuine
    // Luminous native 975-bone clips can actually play on the real character.
    if (bRunNativeMotionQACycle)
    {
        if (bRunNonPersistentWardrobeQACycle && QACycleStep < 3) return;
        if (QANextMotionSeconds <= 0.f) QANextMotionSeconds = Now + 2.0f;
        if (QAMotionCycleStep < 3 && Now >= QANextMotionSeconds)
        {
            const EOGLuminousV3MotionPhase Phase = QAMotionCycleStep == 0 ?
                EOGLuminousV3MotionPhase::Walk : QAMotionCycleStep == 1 ?
                EOGLuminousV3MotionPhase::Run : EOGLuminousV3MotionPhase::Jump;
            UAnimSequence* Clip = ResolveAuthoredMotionClip(Phase);
            const bool bValid = Clip && PlayResolvedTraversal(
                Phase, Clip, Phase != EOGLuminousV3MotionPhase::Jump);
            CurrentMotionPhase = Phase;
            UE_LOG(LogTemp, Display,
                TEXT("G12_LUMINOUS_NATIVE_MOTION_QA_STEP=%d motion=%d result=%s active_native_part_count=%d clip=%s error=%s"),
                QAMotionCycleStep, static_cast<int32>(Phase),
                bValid ? TEXT("PASS") : TEXT("FAIL"), ActivePartCount,
                Clip ? *Clip->GetName() : TEXT("MISSING"), *LastLuminousError);
            if (!bValid) { QAMotionCycleStep = 3; return; }
            ++QAMotionCycleStep;
            QANextMotionSeconds = Now + 3.0f;
        }
        return;
    }
    UpdateNativeMotionFromTraversal();
}

void AOGLuminousV3PlayableCharacter::EndPlay(const EEndPlayReason::Type Reason)
{
    if (WardrobeQAWidget)
    {
        WardrobeQAWidget->RemoveFromParent();
        WardrobeQAWidget = nullptr;
    }
    GetWorldTimerManager().ClearTimer(QAWorldModeTimer);
    CurrentNativeClip = nullptr;
    bGate09EmotePlaying = false;
    bSharedActionPlaying = false;
    SharedActionEndTime = 0.f;
    ActiveMotionLeaderIndex = INDEX_NONE;
    Gate09EmoteEndTime = 0.f;
    bGate09TakeoffAlreadyPlayedThisFlight=false;
    Gate09TakeoffEndsAt=0.f;
    if (SharedMotionController) SharedMotionController->StopAction();
    RuntimeParts.Reset();
    bCompatibleNativeMotion = false;
    bLuminousReady = false;
    Super::EndPlay(Reason);
}
