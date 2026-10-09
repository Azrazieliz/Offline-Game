#include "Content/OGContentManifest.h"
#include "Persistence/OGSQLiteWorldStore.h"
#include "Runtime/OGOptionalAssetRuntime.h"
#include "Runtime/OGPackageManagerService.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "UObject/SoftObjectPath.h"

namespace
{
struct FGate12TemporaryWorld
{
    FString Root = FPaths::Combine(
        FPaths::ProjectSavedDir(), TEXT("Automation/Gate12P0"),
        FGuid::NewGuid().ToString(EGuidFormats::Digits));
    FString Database = FPaths::Combine(Root, TEXT("gate12_native.db"));
    FOGSQLiteWorldStore Store;
    FString Error;

    bool Open()
    {
        IFileManager::Get().MakeDirectory(*Root, true);
        return Store.Open(Database, Error);
    }

    ~FGate12TemporaryWorld()
    {
        Store.Close();
        // The fixture owns only its freshly generated, GUID-scoped test directory.
        IFileManager::Get().DeleteDirectory(*Root, false, true);
    }
};

FOGContentPackageRecord Gate12EmbeddedRecord(const TCHAR* Id)
{
    FOGContentPackageRecord Record;
    Record.PackageId = FOGContentId(Id);
    Record.Version = 1;
    Record.Category = FName(TEXT("ui"));
    Record.ContentHash = TEXT("gate12-native-isolated-fixture-not-a-distribution-hash");
    Record.InstallUri = TEXT("embedded:gate12_p0_native_test");
    Record.ManifestJson = TEXT("{\"fixture\":\"gate12-native-test\",\"source_only\":true}");
    // Native test authority only. The approved Gate 11 registry does NOT authorize these flags.
    Record.bInstalled = true;
    Record.bValidated = true;
    Record.DownloadState = FName(TEXT("installed"));
    return Record;
}

bool ReadPackage(FOGSQLiteWorldStore& Store, const FOGContentId& Id,
                 FOGContentPackageRecord& Out, FString& Error)
{
    bool Found = false;
    return Store.TryReadContentPackageRecord(Id, Found, Out, Error) && Found;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOGGate12RegistryV2NativeAdapterTest,
    "OfflineGame.Gate12.RegistryV2.ExactUiPackageDtoDependencyAndOptionalFallback",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FOGGate12RegistryV2NativeAdapterTest::RunTest(const FString& Parameters)
{
    // EXACT accepted P0 package IDs. No general-purpose asset importer or physical pak mount.
    const FOGContentId RootId(TEXT("ui:shared_navigation_contract"));
    const FOGContentId ChildId(TEXT("ui:ruler_shell_navigation"));

    FOGContentPackageManifest RootDto;
    RootDto.PackageId = RootId;
    RootDto.Version = 1;
    FOGContentPackageManifest ChildDto;
    ChildDto.PackageId = ChildId;
    ChildDto.Version = 1;
    FOGPackageDependency ChildDependency;
    ChildDependency.PackageId = RootId;
    ChildDependency.MinimumVersion = 1;
    ChildDto.Dependencies.Add(ChildDependency);

    TArray<FString> ManifestErrors;
    if (!TestTrue(TEXT("Root native DTO passes frozen content validator"),
        FOGContentManifestValidator::Validate(RootDto, {}, ManifestErrors)))
        return false;
    if (!TestTrue(TEXT("Dependent native DTO passes frozen content validator"),
        FOGContentManifestValidator::Validate(ChildDto, {}, ManifestErrors)))
        return false;
    FOGContentPackageManifest BadDto = ChildDto;
    BadDto.PackageId = FOGContentId(TEXT("UI:Unstable"));
    TestFalse(TEXT("Invalid content ID refused"),
        FOGContentManifestValidator::Validate(BadDto, {}, ManifestErrors));
    BadDto = ChildDto;
    BadDto.Dependencies.Add(ChildDependency);
    TestFalse(TEXT("Duplicate native dependency refused"),
        FOGContentManifestValidator::Validate(BadDto, {}, ManifestErrors));
    BadDto = ChildDto;
    BadDto.Dependencies[0].MinimumVersion = 0;
    TestFalse(TEXT("Invalid native minimum version refused"),
        FOGContentManifestValidator::Validate(BadDto, {}, ManifestErrors));

    FGate12TemporaryWorld Fixture;
    if (!TestTrue(TEXT("Open disposable SQLite world"), Fixture.Open()))
        return false;
    TestEqual(TEXT("Fresh fixture uses schema 14"),
        Fixture.Store.GetSchemaVersion(Fixture.Error), 14);

    FOGPackageManagerService Packages(Fixture.Store);
    FOGContentPackageRecord Root = Gate12EmbeddedRecord(TEXT("ui:shared_navigation_contract"));
    FOGContentPackageRecord Child = Gate12EmbeddedRecord(TEXT("ui:ruler_shell_navigation"));

    if (!TestTrue(TEXT("Register exact Gate 12 child test record"),
        Packages.RegisterPackage(Child, Fixture.Error)))
        return false;

    FOGPackageDependencyRecord Edge;
    Edge.PackageId = ChildId;
    Edge.DependencyPackageId = RootId;
    Edge.MinimumVersion = 1;
    TestFalse(TEXT("Missing dependency registration transaction rejected"),
        Packages.SetDependency(Edge, Fixture.Error));

    TArray<FOGPackageDependencyRecord> Edges;
    if (!TestTrue(TEXT("List dependencies after rollback"),
        Fixture.Store.ListPackageDependencies(ChildId, Edges, Fixture.Error)))
        return false;
    TestTrue(TEXT("Invalid edge did not persist"), Edges.IsEmpty());

    if (!TestTrue(TEXT("Register exact Gate 12 root test record"),
        Packages.RegisterPackage(Root, Fixture.Error)))
        return false;

    Edge.MinimumVersion = 2;
    TestFalse(TEXT("Too-new dependency rejected"),
        Packages.SetDependency(Edge, Fixture.Error));
    Edges.Reset();
    if (!TestTrue(TEXT("List dependencies after unsatisfied minimum"),
        Fixture.Store.ListPackageDependencies(ChildId, Edges, Fixture.Error)))
        return false;
    TestTrue(TEXT("Rejected higher minimum not persisted"), Edges.IsEmpty());

    Edge.MinimumVersion = 1;
    if (!TestTrue(TEXT("Correct native edge accepted"),
        Packages.SetDependency(Edge, Fixture.Error)))
        return false;
    TestFalse(TEXT("Child activation before required root rejected"),
        Packages.ActivatePackage(ChildId, Fixture.Error));
    if (!TestTrue(TEXT("Root activated first"),
        Packages.ActivatePackage(RootId, Fixture.Error)))
        return false;
    if (!TestTrue(TEXT("Child activation succeeds after root"),
        Packages.ActivatePackage(ChildId, Fixture.Error)))
        return false;

    FOGContentPackageRecord Before;
    if (!TestTrue(TEXT("Read active child before absent media"),
        ReadPackage(Fixture.Store, ChildId, Before, Fixture.Error)))
        return false;
    TestTrue(TEXT("Child activation stored"), Before.bActivated);
    const FOGOptionalAssetReference Missing{
        ChildId,
        FSoftObjectPath(TEXT("/Game/Gate12P0/AbsentOptionalView.AbsentOptionalView"))
    };
    FOGOptionalAssetRuntime Loader(Fixture.Store);
    UObject* AbsentObject = nullptr;
    FString AbsentReason;
    TestFalse(TEXT("Missing optional presentation returns safe absence"),
        Loader.ActivateAndLoad(Missing, AbsentObject, AbsentReason));
    TestNull(TEXT("Missing visual does not fabricate UObject"), AbsentObject);
    Loader.ReleaseAll();

    FOGContentPackageRecord After;
    if (!TestTrue(TEXT("Read child after absent media"),
        ReadPackage(Fixture.Store, ChildId, After, Fixture.Error)))
        return false;
    TestTrue(TEXT("Missing visual preserves installed/validated/activated logical content"),
        After.bInstalled && After.bValidated && After.bActivated);
    TestEqual(TEXT("Missing visual preserves content ID"),
        After.PackageId.ToString(), Before.PackageId.ToString());
    TestEqual(TEXT("Missing visual preserves package version"),
        After.Version, Before.Version);
    TestEqual(TEXT("Missing visual preserves manifest"),
        After.ManifestJson, Before.ManifestJson);
    TestEqual(TEXT("Missing visual preserves hash metadata"),
        After.ContentHash, Before.ContentHash);

    if (!TestTrue(TEXT("Deactivate root"),
        Packages.DeactivatePackage(RootId, Fixture.Error)))
        return false;
    FOGContentPackageRecord RootAfter, ChildAfter;
    if (!TestTrue(TEXT("Read both packages following dependent deactivation"),
        ReadPackage(Fixture.Store, RootId, RootAfter, Fixture.Error) &&
        ReadPackage(Fixture.Store, ChildId, ChildAfter, Fixture.Error)))
        return false;
    TestFalse(TEXT("Root is inactive"), RootAfter.bActivated);
    TestFalse(TEXT("Dependent child is also inactive"), ChildAfter.bActivated);
    TestTrue(TEXT("Native fixture never exports installer provenance"),
        Root.InstallUri.StartsWith(TEXT("embedded:")) &&
        Child.InstallUri.StartsWith(TEXT("embedded:")));

    return true;
}

#endif
