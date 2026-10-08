#include "Runtime/OGCanonicalCharacterPresentation.h"
#include "Runtime/OGCharacterVisualResolverProvider.h"
#include "Runtime/OGGameCoreSubsystem.h"
#include "Persistence/OGSQLiteWorldStore.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Animation/Skeleton.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/GameInstance.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "HAL/FileManager.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectGlobals.h"

namespace
{
struct FCanonicalVisualFixture
{
    FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"),
        FGuid::NewGuid().ToString(EGuidFormats::Digits));
    FOGSQLiteWorldStore Store;
    FOGFoundationCharacterContext Context;
    FOGEntityId Owner = FOGEntityId::NewId();
    FOGContentId PackageId = FOGContentId(TEXT("test:package.canonical_visual"));
    FOGContentPackageRecord Package;
    UWorld* World = nullptr;
    AActor* Actor = nullptr;
    USkeletalMeshComponent* Mesh = nullptr;
    UOGCanonicalCharacterPresentation* Presenter = nullptr;
    TStrongObjectPtr<USkeleton> Skeleton;
    TStrongObjectPtr<USkeletalMesh> Baseline;
    TStrongObjectPtr<UMaterial> ParentMaterial;
    TStrongObjectPtr<UMaterialInterface> Material;
    FString Error;
    bool Open(bool bManifestation = false)
    {
        Context.EntityId = FOGEntityId::NewId();
        IFileManager::Get().MakeDirectory(*Directory, true);
        if (!Store.Open(FPaths::Combine(Directory, TEXT("visual.db")), Error) ||
            !Store.UpsertEntity(Owner, FName(TEXT("ruler")), 0, TEXT("{}"), Error) ||
            !Store.UpsertEntity(Context.EntityId, FName(TEXT("character")), 0,
                TEXT("{\"history\":\"preserved\",\"anatomy\":\"authored_shape\"}"), Error)) return false;
        if (bManifestation)
        {
            FOGCharacterManifestationRecord Record;
            Record.ManifestationId = Context.EntityId;
            Record.OwningRulerId = Owner;
            Record.IdentityId = FOGContentId(TEXT("test:identity.exact"));
            Record.ActiveVersionId = FOGContentId(TEXT("test:version.exact"));
            Record.Level = 1;
            Record.ProgressionStateJson = TEXT("{\"development\":7}");
            if (!Store.UpsertCharacterManifestation(Record, 0, Error)) return false;
            Context.ManifestationId = Context.EntityId;
            Context.OwnerEntityId = Owner;
        }
        Package.PackageId = PackageId;
        Package.Version = 1;
        Package.ContentHash = TEXT("test_canonical_visual_hash");
        Package.Category = FName(TEXT("character_visual"));
        Package.StorageClass = FName(TEXT("embedded"));
        Package.SealedState = FName(TEXT("sealed"));
        Package.bInstalled = Package.bValidated = Package.bActivated = true;
        Package.DownloadState = FName(TEXT("installed"));
        Package.ManifestJson = TEXT("{\"unrelated_metadata\":\"preserved\"}");
        if (!Store.UpsertContentPackageRecord(Package, Error)) return false;
        UWorld::InitializationValues Values;
        Values.AllowAudioPlayback(false).RequiresHitProxies(false)
            .CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
        World = UWorld::CreateWorld(EWorldType::Game, true, NAME_None, nullptr,
            true, ERHIFeatureLevel::SM5, &Values, false);
        if (!World) return false;
        Actor = World->SpawnActor<AActor>();
        if (!Actor) return false;
        Mesh = NewObject<USkeletalMeshComponent>(Actor);
        Actor->AddInstanceComponent(Mesh);
        Actor->SetRootComponent(Mesh);
        Mesh->RegisterComponent();
        Skeleton.Reset(NewObject<USkeleton>());
        ParentMaterial.Reset(NewObject<UMaterial>());
        auto* BaselineMID = UMaterialInstanceDynamic::Create(ParentMaterial.Get(), Actor);
        // Native test material declares parameter overrides without requiring asset/shader authoring.
        for (const TCHAR* Name : { TEXT("OG_InjurySeverity"), TEXT("OG_EquipmentDamage"),
            TEXT("OG_OutfitDamage"), TEXT("OG_Restoration"), TEXT("OG_Privacy") })
        {
            FScalarParameterValue Value;
            Value.ParameterInfo = FMaterialParameterInfo(FName(Name));
            Value.ParameterValue = 0.f;
            BaselineMID->ScalarParameterValues.Add(Value);
        }
        Material.Reset(BaselineMID);
        Baseline.Reset(NewBody(Skeleton.Get()));
        Mesh->SetSkeletalMeshAsset(Baseline.Get());
        Mesh->SetMaterial(0, Material.Get());
        Presenter = NewObject<UOGCanonicalCharacterPresentation>(Actor);
        Actor->AddInstanceComponent(Presenter);
        Presenter->RegisterComponent();
        return true;
    }
    USkeletalMesh* NewBody(USkeleton* Rig)
    {
        auto* Body = NewObject<USkeletalMesh>();
        Body->SetSkeleton(Rig);
        Body->GetMaterials().Add(FSkeletalMaterial(Material.Get()));
        return Body;
    }
    FOGOptionalAssetReference Reference(UObject* Object) const
    {
        FOGOptionalAssetReference Ref;
        Ref.PackageId = PackageId;
        Ref.AssetPath = FSoftObjectPath(Object);
        return Ref;
    }
    bool Configure(UOGGameCoreSubsystem* LifecycleCore = nullptr)
    {
        return Presenter->ConfigureFromCanonicalStore(Context, Mesh, FName(TEXT("test_rig")),
            Store, nullptr, Error, LifecycleCore);
    }
    bool Refresh()
    {
        return Presenter->RefreshFromCanonicalStore(Context, Store, nullptr, Error);
    }
    bool Register(const FString& Row)
    {
        return FOGCharacterVisualResolverProvider::MergeAuthoredMetadata(Package, Row, Error) &&
            Store.UpsertContentPackageRecord(Package, Error);
    }
    bool Diagnostic()
    {
        FString Template, Row;
        FOGFoundationCharacterProjection P;
        return FFileHelper::LoadFileToString(Template,
                *FPaths::Combine(FPaths::ProjectConfigDir(), TEXT("Foundation/CharacterVisualDiagnostic.json"))) &&
            FOGFoundationCharacterRuntime(Store).Project(Context, P, Error) &&
            FOGCharacterVisualResolverProvider::MakeDiagnosticRow(P, Template, Row, Error) && Register(Row);
    }
    ~FCanonicalVisualFixture()
    {
        if (Presenter) Presenter->ClearBinding();
        if (World) { World->DestroyWorld(true); }
        Store.Close();
        IFileManager::Get().DeleteDirectory(*Directory, false, true);
    }
};
float Scalar(USkeletalMeshComponent* Mesh, FName Name)
{
    auto* MID = Cast<UMaterialInstanceDynamic>(Mesh->GetMaterial(0));
    return MID ? MID->K2_GetScalarParameterValue(Name) : -1.f;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOGCanonicalVisualMetadataTest,
    "OfflineGame.Foundation.Presentation.ExactCanonicalMetadataSelection",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FOGCanonicalVisualMetadataTest::RunTest(const FString& Parameters)
{
    FCanonicalVisualFixture F;
    if (!TestTrue(TEXT("Isolated canonical Manifestation"), F.Open(true))) return false;
    FOGFoundationCharacterRuntime Runtime(F.Store);
    FOGManifestationFormRecord Form;
    Form.ManifestationId = F.Context.EntityId;
    Form.FormId = FOGContentId(TEXT("test:form.current"));
    Form.State = FName(TEXT("active"));
    Form.StateJson = TEXT("{\"anatomy\":\"exact\"}");
    TestTrue(TEXT("Persist actual active form"), Runtime.SetForm(F.Context, Form, 1, F.Error));
    FOGManifestationFormRecord Unlocked = Form;
    Unlocked.FormId = FOGContentId(TEXT("test:form.not_selected"));
    Unlocked.State = FName(TEXT("unlocked"));
    TestTrue(TEXT("Persist unselected form"), Runtime.SetForm(F.Context, Unlocked, 1, F.Error));
    FOGFactorInstanceRecord Factor;
    Factor.FactorInstanceId = FOGEntityId::NewId();
    Factor.OwnerEntityId = F.Context.EntityId;
    Factor.SourceEntityId = F.Owner;
    Factor.FactorId = FOGContentId(TEXT("test:factor.body"));
    Factor.ExpressionWeightBps = 6000;
    Factor.StateJson = TEXT("{\"body_influence\":\"authored\"}");
    TestTrue(TEXT("Persist expressed factor"), Runtime.AcquireFactor(F.Context, Factor, {}, 2, F.Error));
    FOGEntityRankStateRecord Rank;
    Rank.AttainedRankId = Rank.PeakRankId = FOGContentId(TEXT("test:rank.developed"));
    Rank.AttainedLevel = Rank.PeakLevel = 2;
    Rank.StateJson = TEXT("{\"body_influence\":\"rank_authored\"}");
    TestTrue(TEXT("Persist existing rank body influence"), Runtime.SetRank(F.Context, Rank, 2, F.Error));
    FOGManifestationReinforcementRecord Reinforcement;
    Reinforcement.StateJson = TEXT("{\"body_influence\":\"reinforcement_authored\"}");
    TestTrue(TEXT("Persist existing reinforcement development"), Runtime.SetReinforcement(F.Context, Reinforcement, 2, F.Error));
    FOGManifestationRouteNodeRecord Route;
    Route.RouteId = FOGContentId(TEXT("test:route.development"));
    Route.NodeId = FOGContentId(TEXT("test:node.body"));
    Route.StateJson = TEXT("{\"body_influence\":\"route_authored\"}");
    TestTrue(TEXT("Persist existing route development"), Runtime.SetRouteNode(F.Context, Route, 2, F.Error));
    FOGCharacterConvergenceRecord Convergence;
    Convergence.ConvergenceId = FOGEntityId::NewId();
    Convergence.IdentityId = FOGContentId(TEXT("test:identity.exact"));
    Convergence.ResultManifestationId = F.Context.ManifestationId;
    Convergence.RuleId = FOGContentId(TEXT("test:rule.body_convergence"));
    Convergence.WorldTick = 2;
    Convergence.StateJson = TEXT("{\"body_influence\":\"convergence_authored\"}");
    TestTrue(TEXT("Persist existing current-result convergence metadata"),
        F.Store.UpsertCharacterConvergence(Convergence, 2, F.Error));
    FOGManifestationPresentationStateRecord Presentation;
    Presentation.SelectedSkinId = FOGContentId(TEXT("test:skin.current"));
    Presentation.OutfitStateJson = TEXT("{\"outfit_id\":\"test:outfit.current\",\"damage_bps\":2000}");
    Presentation.PresentationVariantStateJson = TEXT("{\"hairstyle_id\":\"test:hair.current\"}");
    TestTrue(TEXT("Persist current outfit/skin/hair"), Runtime.SetPresentation(F.Context, Presentation, 3, F.Error));
    FOGEntityId Item;
    TestTrue(TEXT("Acquire canonical equipment"), Runtime.AcquireItem(F.Context,
        FOGContentId(TEXT("test:item.current")), {}, {}, 3, Item, F.Error));
    TestTrue(TEXT("Bind canonical equipment"), Runtime.Equip(F.Context, FOGContentId(TEXT("test:slot.hand")), Item,
        [](const FOGEntityId&, const FOGContentId&, const FOGItemInstanceRecord&, FString&) { return true; }, F.Error));
    TestTrue(TEXT("Injury reaches exact selection"), Runtime.SetInjury(F.Context, FName(TEXT("wounded")), 2500, F.Owner, 4, F.Error));
    const FString Row = TEXT(R"JSON({"binding_id":"test:binding.exact","priority":50,"selector":{
        "identity_id":"test:identity.exact","version_id":"test:version.exact","form_ids":["test:form.current"],"skin_id":"test:skin.current",
        "conditions":[{"source":"entity","key":"anatomy","equals":"authored_shape"},
        {"source":"progression","key":"development","minimum":7},
        {"source":"rank","key":"body_influence","equals":"rank_authored"},
        {"source":"reinforcement","key":"body_influence","equals":"reinforcement_authored"},
        {"source":"route","route_id":"test:route.development","node_id":"test:node.body","key":"body_influence","equals":"route_authored"},
        {"source":"convergence","rule_id":"test:rule.body_convergence","key":"body_influence","equals":"convergence_authored"},
        {"source":"form","form_id":"test:form.current","key":"anatomy","equals":"exact"},
        {"source":"factor","factor_id":"test:factor.body","minimum_expression_bps":5000,"key":"body_influence","equals":"authored"},
        {"source":"outfit","key":"outfit_id","equals":"test:outfit.current"},
        {"source":"variant","key":"hairstyle_id","equals":"test:hair.current"},
        {"source":"equipment_durability","slot_id":"test:slot.hand","definition_id":"test:item.current","key":"condition_bps","minimum":1},
        {"source":"entity","key":"injury_severity_bps","minimum":2000,"maximum":3000}]},
        "binding":{"rig_family":"exact_selected_rig"}})JSON");
    TestTrue(TEXT("Register existing package metadata"), F.Register(Row));
    int32 ResolverCalls = 0;
    F.Presenter->SetVisualResolver([&](const FOGFoundationCharacterProjection& Current, bool Privacy,
        FOGCharacterVisualBinding& Result, FString& Reason)
    {
        ++ResolverCalls;
        TestTrue(TEXT("Actual Manifestation reaches resolver"), Current.bHasManifestation);
        TestEqual(TEXT("Actual active Version reaches resolver"), Current.Manifestation.ActiveVersionId.ToString(), FString(TEXT("test:version.exact")));
        TestEqual(TEXT("All persisted forms reach resolver"), Current.Forms.Num(), 2);
        TestEqual(TEXT("Factors reach resolver"), Current.Factors.Num(), 1);
        TestTrue(TEXT("Presentation reaches resolver"), Current.bHasPresentation);
        TestEqual(TEXT("Equipment reaches resolver"), Current.Equipment.Num(), 1);
        TestTrue(TEXT("Development reaches resolver"), Current.Manifestation.ProgressionStateJson.Contains(TEXT("development")));
        TestTrue(TEXT("Rank development reaches resolver"), Current.bHasRank);
        TestTrue(TEXT("Reinforcement development reaches resolver"), Current.bHasReinforcement);
        TestEqual(TEXT("Route development reaches resolver"), Current.Routes.Num(), 1);
        TestEqual(TEXT("Convergence development reaches resolver"), Current.Convergences.Num(), 1);
        return FOGCharacterVisualResolverProvider::Resolve(F.Store, Current, Privacy, Result, Reason);
    });
    TestTrue(TEXT("Presenter projects exact canonical state into resolver"), F.Configure());
    TestEqual(TEXT("One full canonical projection reaches resolver"), ResolverCalls, 1);
    FOGFoundationCharacterProjection P;
    FOGCharacterVisualBinding B;
    TestTrue(TEXT("Project actual canonical state"), Runtime.Project(F.Context, P, F.Error));
    // Exercise each authored source separately without altering the complete selector.
    TSharedPtr<FJsonObject> ExactRow;
    TestTrue(TEXT("Parse exact authored selector"), FJsonSerializer::Deserialize(
        TJsonReaderFactory<>::Create(Row), ExactRow));
    if (!ExactRow.IsValid()) return false;
    const auto Selector = ExactRow->GetObjectField(TEXT("selector"));
    const auto Conditions = Selector->GetArrayField(TEXT("conditions"));
    const FString ExactManifest = F.Package.ManifestJson;
    auto CheckSelector = [&](const TArray<TSharedPtr<FJsonValue>>& SelectedConditions, const FString& Label)
    {
        Selector->SetArrayField(TEXT("conditions"), SelectedConditions);
        auto SingleManifest = MakeShared<FJsonObject>();
        SingleManifest->SetArrayField(TEXT("character_visual_bindings"),
            {MakeShared<FJsonValueObject>(ExactRow)});
        FString Json;
        FJsonSerializer::Serialize(SingleManifest, TJsonWriterFactory<>::Create(&Json));
        F.Package.ManifestJson = Json;
        TestTrue(*FString::Printf(TEXT("Persist isolated selector: %s"), *Label),
            F.Store.UpsertContentPackageRecord(F.Package, F.Error));
        FOGCharacterVisualBinding SelectedBinding;
        TestTrue(*FString::Printf(TEXT("Canonical selector matches independently: %s"), *Label),
            FOGCharacterVisualResolverProvider::Resolve(F.Store, P, false, SelectedBinding, F.Error));
        TestEqual(*FString::Printf(TEXT("Independent selector chooses exact binding: %s"), *Label),
            SelectedBinding.RigFamily, FName(TEXT("exact_selected_rig")));
    };
    CheckSelector({}, TEXT("identity/version/active forms/skin"));
    for (int32 Index = 0; Index < Conditions.Num(); ++Index)
    {
        const auto Condition = Conditions[Index]->AsObject();
        CheckSelector({Conditions[Index]}, FString::Printf(TEXT("%d %s/%s"), Index,
            *Condition->GetStringField(TEXT("source")), *Condition->GetStringField(TEXT("key"))));
    }
    Selector->SetArrayField(TEXT("conditions"), Conditions);
    F.Package.ManifestJson = ExactManifest;
    TestTrue(TEXT("Restore complete authored selector"), F.Store.UpsertContentPackageRecord(F.Package, F.Error));
    TestTrue(TEXT("Manifestation upsert retains entity anatomy and history"),
        P.EntityStateJson.Contains(TEXT("authored_shape")) && P.EntityStateJson.Contains(TEXT("preserved")));
    TestTrue(TEXT("Every exact selector matches canonical state"), FOGCharacterVisualResolverProvider::Resolve(F.Store, P, false, B, F.Error));
    TestEqual(TEXT("Binding selected by authored metadata"), B.RigFamily, FName(TEXT("exact_selected_rig")));
    const FString EntityBefore = P.EntityStateJson;
    const FString PresentationBefore = P.Presentation.PresentationVariantStateJson;
    TestTrue(TEXT("Privacy resolves same canonical selection"), FOGCharacterVisualResolverProvider::Resolve(F.Store, P, true, B, F.Error));
    TestEqual(TEXT("Privacy leaves canonical entity payload"), P.EntityStateJson, EntityBefore);
    TestEqual(TEXT("Privacy leaves canonical presentation"), P.Presentation.PresentationVariantStateJson, PresentationBefore);
    P.Reinforcement.StateJson = TEXT("{\"body_influence\":\"another_body\"}");
    TestFalse(TEXT("Different reinforcement cannot match exact developed body"),
        FOGCharacterVisualResolverProvider::Resolve(F.Store, P, false, B, F.Error));
    P.Reinforcement = Reinforcement;
    P.Convergences[0].ResultManifestationId = F.Owner;
    TestFalse(TEXT("Another Manifestation's convergence cannot select this body"),
        FOGCharacterVisualResolverProvider::Resolve(F.Store, P, false, B, F.Error));
    P.Convergences[0] = Convergence;
    P.Manifestation.ActiveVersionId = FOGContentId(TEXT("test:version.unsupported"));
    TestFalse(TEXT("Another Version cannot match base row"), FOGCharacterVisualResolverProvider::Resolve(F.Store, P, false, B, F.Error));
    P.Manifestation.ActiveVersionId = FOGContentId(TEXT("test:version.exact"));
    P.Forms[0].State = FName(TEXT("unlocked"));
    for (auto& Entry : P.Forms) Entry.State = FName(TEXT("unlocked"));
    TestFalse(TEXT("Unlocked forms are not selected active forms"), FOGCharacterVisualResolverProvider::Resolve(F.Store, P, false, B, F.Error));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOGCanonicalVisualDefaultProviderTest,
    "OfflineGame.Foundation.Presentation.DefaultProviderAndOverrideSurviveRebind",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FOGCanonicalVisualDefaultProviderTest::RunTest(const FString& Parameters)
{
    FCanonicalVisualFixture F;
    if (!TestTrue(TEXT("Open isolated canonical fixture"), F.Open())) return false;
    TestTrue(TEXT("Author tiny diagnostic through registered package"), F.Diagnostic());
    const FString ManifestBefore = F.Package.ManifestJson;
    TestTrue(TEXT("Diagnostic reseed is idempotent"), F.Diagnostic());
    TestEqual(TEXT("Reseed preserves legitimate metadata"), F.Package.ManifestJson, ManifestBefore);
    TestTrue(TEXT("Configure auto-installs provider"), F.Configure());
    TestFalse(TEXT("Provider finds actual metadata without SetVisualResolver"), F.Presenter->GetAssetStatus().Contains(TEXT("No exact")));
    int32 Calls = 0;
    F.Presenter->SetVisualResolver([&](const FOGFoundationCharacterProjection& P, bool Privacy,
        FOGCharacterVisualBinding&, FString& Reason)
    {
        ++Calls;
        TestEqual(TEXT("Resolver receives actual entity"), P.Context.EntityId.ToString(), F.Context.EntityId.ToString());
        TestTrue(TEXT("Full canonical payload retained"), P.EntityStateJson.Contains(TEXT("preserved")));
        Reason = TEXT("explicit resolver"); return false;
    });
    F.Presenter->ClearBinding();
    TestTrue(TEXT("Reconfigure canonical selection"), F.Configure());
    TestEqual(TEXT("Explicit override survives clear/reconfigure"), Calls, 1);
    TestEqual(TEXT("Explicit override wins default provider"), F.Presenter->GetAssetStatus(), FString(TEXT("explicit resolver")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOGCanonicalVisualRigTest,
    "OfflineGame.Foundation.Presentation.BodyRigModularAndMissingOptionalPreserveCurrent",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FOGCanonicalVisualRigTest::RunTest(const FString& Parameters)
{
    FCanonicalVisualFixture F;
    if (!TestTrue(TEXT("Open isolated canonical fixture"), F.Open())) return false;
    TStrongObjectPtr<USkeletalMesh> Exact(F.NewBody(F.Skeleton.Get()));
    TStrongObjectPtr<USkeleton> OtherSkeleton(NewObject<USkeleton>());
    TStrongObjectPtr<USkeletalMesh> Incompatible(F.NewBody(OtherSkeleton.Get()));
    TStrongObjectPtr<UStaticMesh> Accessory(NewObject<UStaticMesh>());
    FOGCharacterVisualBinding Binding;
    Binding.RigFamily = FName(TEXT("test_rig"));
    Binding.Body = F.Reference(Exact.Get());
    FOGCharacterVisualPart Good;
    Good.PartId = FName(TEXT("compatible")); Good.RigFamily = Binding.RigFamily;
    Good.Asset = F.Reference(Exact.Get());
    FOGCharacterVisualPart Wrong = Good;
    Wrong.PartId = FName(TEXT("wrong_skeleton")); Wrong.Asset = F.Reference(Incompatible.Get());
    FOGCharacterVisualPart WrongStatic = Good;
    WrongStatic.PartId = FName(TEXT("wrong_static_family")); WrongStatic.RigFamily = FName(TEXT("other_rig"));
    WrongStatic.Asset = F.Reference(Accessory.Get());
    Binding.Parts = {Good, Wrong, WrongStatic};
    F.Presenter->SetVisualResolver([&](const FOGFoundationCharacterProjection&, bool,
        FOGCharacterVisualBinding& Out, FString&) { Out = Binding; return true; });
    TestTrue(TEXT("Apply compatible body"), F.Configure());
    TestTrue(TEXT("Compatible body becomes actual mesh"), F.Mesh->GetSkeletalMeshAsset() == Exact.Get());
    TArray<USkeletalMeshComponent*> Parts;
    F.Actor->GetComponents(Parts);
    TestEqual(TEXT("Only compatible leader-pose part admitted"), Parts.Num(), 2);
    TArray<UStaticMeshComponent*> StaticParts;
    F.Actor->GetComponents(StaticParts);
    TestEqual(TEXT("Static wrong family also omitted"), StaticParts.Num(), 0);
    Binding.Body = F.Reference(Incompatible.Get());
    TestTrue(TEXT("Incompatible body refresh still projects canonical state"), F.Refresh());
    TestTrue(TEXT("Another skeleton cannot overwrite current exact body"), F.Mesh->GetSkeletalMeshAsset() == Exact.Get());
    TestTrue(TEXT("Rejected rig reports retention"), F.Presenter->GetAssetStatus().Contains(TEXT("current body retained")));
    Binding.Body = F.Reference(Exact.Get()); Binding.RigFamily = FName(TEXT("other_rig"));
    TStrongObjectPtr<USkeletalMesh> OtherFamily(F.NewBody(F.Skeleton.Get()));
    Binding.Body = F.Reference(OtherFamily.Get());
    TestTrue(TEXT("Declared family mismatch refresh"), F.Refresh());
    TestTrue(TEXT("Family mismatch preserves exact body"), F.Mesh->GetSkeletalMeshAsset() == Exact.Get());
    Binding.RigFamily = FName(TEXT("test_rig"));
    Binding.Body.AssetPath = FSoftObjectPath(TEXT("/Game/Unavailable/ExactBody.ExactBody"));
    Binding.Body.PackageId = FOGContentId(TEXT("test:package.absent"));
    TestTrue(TEXT("Absent optional package degrades safely"), F.Refresh());
    TestTrue(TEXT("Missing optional body retains valid exact body"), F.Mesh->GetSkeletalMeshAsset() == Exact.Get());
    F.Presenter->SetVisualResolver([](const FOGFoundationCharacterProjection&, bool,
        FOGCharacterVisualBinding&, FString& Reason) { Reason = TEXT("unsupported exact form"); return false; });
    TestTrue(TEXT("Unsupported exact form retains presentation"), F.Refresh());
    F.Presenter->SetPrivacyPresentation(true);
    TestTrue(TEXT("Privacy unsupported exact form refresh"), F.Refresh());
    TestTrue(TEXT("Privacy cannot restore generic baseline for unsupported form"), F.Mesh->GetSkeletalMeshAsset() == Exact.Get());
    F.Presenter->ClearBinding();
    TestTrue(TEXT("Only explicit clear restores captured baseline"), F.Mesh->GetSkeletalMeshAsset() == F.Baseline.Get());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOGCanonicalVisualMaterialTest,
    "OfflineGame.Foundation.Presentation.InjuryOutfitEquipmentRestorationAndPrivacyAreDerived",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FOGCanonicalVisualMaterialTest::RunTest(const FString& Parameters)
{
    FCanonicalVisualFixture F;
    if (!TestTrue(TEXT("Open isolated canonical fixture"), F.Open())) return false;
    FOGFoundationCharacterRuntime Runtime(F.Store);
    FOGManifestationPresentationStateRecord State;
    State.OutfitStateJson = TEXT("{\"damage_bps\":3000}");
    State.PresentationVariantStateJson = TEXT("{\"hairstyle_id\":\"test:hair.actual\"}");
    TestTrue(TEXT("Persist ordinary character presentation"), Runtime.SetPresentation(F.Context, State, 1, F.Error));
    TestTrue(TEXT("Persist injury"), Runtime.SetInjury(F.Context, FName(TEXT("wounded")), 6500, F.Owner, 2, F.Error));
    FOGEntityId Item;
    TestTrue(TEXT("Acquire equipment"), Runtime.AcquireItem(F.Context, FOGContentId(TEXT("test:item.tool")), {}, {}, 2, Item, F.Error));
    TestTrue(TEXT("Equip tool"), Runtime.Equip(F.Context, FOGContentId(TEXT("test:slot.hand")), Item,
        [](const FOGEntityId&, const FOGContentId&, const FOGItemInstanceRecord&, FString&) { return true; }, F.Error));
    TestTrue(TEXT("Persist tool wear"), Runtime.DamageEquipment(F.Context, Item, 4000, F.Owner, 3, F.Error));
    FOGFoundationCharacterProjection WornProjection;
    TestTrue(TEXT("Project persisted equipment after wear"), Runtime.Project(F.Context, WornProjection, F.Error));
    TestEqual(TEXT("Wear preserves canonical equipment binding"), WornProjection.Equipment.Num(), 1);
    bool ItemFound = false;
    FOGItemInstanceRecord PersistedItem;
    TestTrue(TEXT("Read persisted worn item"), F.Store.TryReadItemInstance(Item, ItemFound, PersistedItem, F.Error));
    TestTrue(TEXT("Worn item remains present"), ItemFound);
    TSharedPtr<FJsonObject> Durability;
    TestTrue(TEXT("Parse persisted durability"), FJsonSerializer::Deserialize(
        TJsonReaderFactory<>::Create(PersistedItem.DurabilityStateJson), Durability));
    double ConditionBps = -1.;
    TestTrue(TEXT("Persisted durability has a condition"), Durability.IsValid() &&
        Durability->TryGetNumberField(TEXT("condition_bps"), ConditionBps));
    TestEqual(TEXT("Canonical condition is 6000 basis points"), ConditionBps, 6000.);
    if (WornProjection.Equipment.Num() == 1)
    {
        TestEqual(TEXT("Equipment projection uses latest persisted durability"),
            WornProjection.Equipment[0].Item.DurabilityStateJson, PersistedItem.DurabilityStateJson);
        TestFalse(TEXT("Worn equipment remains unbroken"), WornProjection.Equipment[0].bBroken);
    }
    TestTrue(TEXT("Configure uses current canonical injury/outfit/equipment"), F.Configure());
    TestEqual(TEXT("Derived injury"), F.Presenter->GetVisualState().InjurySeverity, .65f);
    TestEqual(TEXT("Derived outfit damage"), F.Presenter->GetVisualState().OutfitDamage, .3f);
    TestTrue(TEXT("Derived equipment damage"), FMath::IsNearlyEqual(F.Presenter->GetVisualState().EquipmentDamage, .4f));
    TestEqual(TEXT("Current hairstyle retained"), F.Presenter->GetVisualState().HairstyleId, FString(TEXT("test:hair.actual")));
    TestTrue(TEXT("Material projection creates actual MID"), Cast<UMaterialInstanceDynamic>(F.Mesh->GetMaterial(0)) != nullptr);
    TestEqual(TEXT("Injury reaches actual material"), Scalar(F.Mesh, TEXT("OG_InjurySeverity")), .65f);
    TestEqual(TEXT("Outfit reaches actual material"), Scalar(F.Mesh, TEXT("OG_OutfitDamage")), .3f);
    TestTrue(TEXT("Equipment reaches actual material"), FMath::IsNearlyEqual(Scalar(F.Mesh, TEXT("OG_EquipmentDamage")), .4f));
    bool Found = false; FName Kind; FString Before, After; int64 BeforeRevision = 0, AfterRevision = 0;
    TestTrue(TEXT("Read before privacy"), F.Store.TryReadEntity(F.Context.EntityId, Found, Kind, Before, BeforeRevision, F.Error));
    F.Presenter->SetPrivacyPresentation(true);
    TestTrue(TEXT("Privacy refresh only changes presentation"), F.Refresh());
    TestEqual(TEXT("Privacy masks injury material"), Scalar(F.Mesh, TEXT("OG_InjurySeverity")), 0.f);
    TestEqual(TEXT("Privacy masks outfit material"), Scalar(F.Mesh, TEXT("OG_OutfitDamage")), 0.f);
    TestEqual(TEXT("Canonical injury remains in projection"), F.Presenter->GetVisualState().InjurySeverity, .65f);
    TestTrue(TEXT("Read after privacy"), F.Store.TryReadEntity(F.Context.EntityId, Found, Kind, After, AfterRevision, F.Error));
    TestEqual(TEXT("Privacy cannot mutate canonical payload"), After, Before);
    TestEqual(TEXT("Privacy cannot increment canonical revision"), AfterRevision, BeforeRevision);
    TestTrue(TEXT("Restore actual canonical injury"), Runtime.RestoreInjury(F.Context, F.Owner, 4, F.Error));
    F.Presenter->SetPrivacyPresentation(false);
    TestTrue(TEXT("Restoration reprojects current canonical state"), F.Refresh());
    TestEqual(TEXT("Restoration reaches material"), Scalar(F.Mesh, TEXT("OG_Restoration")), 1.f);
    TestEqual(TEXT("Restored injury reaches material"), Scalar(F.Mesh, TEXT("OG_InjurySeverity")), 0.f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOGCanonicalVisualReleaseTest,
    "OfflineGame.Foundation.Presentation.CoreReleaseRestoresBaselineAndReleasesOptionalReferences",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FOGCanonicalVisualReleaseTest::RunTest(const FString& Parameters)
{
    FCanonicalVisualFixture F;
    if (!TestTrue(TEXT("Open isolated canonical fixture"), F.Open())) return false;
    // No GameInstance initialization: this never opens the real Saved/Core database.
    TStrongObjectPtr<UGameInstance> Instance(NewObject<UGameInstance>());
    TStrongObjectPtr<UOGGameCoreSubsystem> Lifecycle(NewObject<UOGGameCoreSubsystem>(Instance.Get()));
    TStrongObjectPtr<USkeletalMesh> Optional(F.NewBody(F.Skeleton.Get()));
    TWeakObjectPtr<USkeletalMesh> WeakOptional(Optional.Get());
    FOGCharacterVisualBinding Binding;
    Binding.RigFamily = FName(TEXT("test_rig")); Binding.Body = F.Reference(Optional.Get());
    F.Presenter->SetVisualResolver([Binding](const FOGFoundationCharacterProjection&, bool,
        FOGCharacterVisualBinding& Out, FString&) { Out = Binding; return true; });
    TestTrue(TEXT("Configure with real release delegate and isolated store"), F.Configure(Lifecycle.Get()));
    TestTrue(TEXT("Actual optional body in use"), F.Mesh->GetSkeletalMeshAsset() == Optional.Get());
    Optional.Reset();
    Lifecycle->OnCanonicalRuntimeReleasing.Broadcast();
    TestFalse(TEXT("Core release clears canonical projection"), F.Presenter->HasProjection());
    TestTrue(TEXT("Core release restores captured body"), F.Mesh->GetSkeletalMeshAsset() == F.Baseline.Get());
    TestTrue(TEXT("Core release restores captured material"), F.Mesh->GetMaterial(0) == F.Material.Get());
    CollectGarbage(RF_NoFlags);
    TestFalse(TEXT("No component/cache/optional runtime retains released body"), WeakOptional.IsValid());
    return true;
}
#endif
