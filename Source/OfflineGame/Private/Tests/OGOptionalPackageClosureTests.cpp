#include "Persistence/OGSQLiteWorldStore.h"
#include "Runtime/OGLocallyInstalledPackageProvider.h"
#include "Runtime/OGOptionalAssetRuntime.h"
#include "Runtime/OGOptionalPackageHost.h"
#include "Runtime/OGPackageManagerService.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "Misc/ScopeExit.h"
#include "IPlatformFilePak.h"
#include "Serialization/BufferArchive.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "Sound/SoundClass.h"
#include "Engine/DataAsset.h"
#include "sqlite/sqlite3.h"

namespace
{
struct FOptionalFixture
{
    FString Root = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(),
        TEXT("Automation/OptionalPackages"), FGuid::NewGuid().ToString(EGuidFormats::Digits)));
    FString DatabasePath = FPaths::Combine(Root, TEXT("world.db"));
    FOGSQLiteWorldStore Store;
    FString Error;
    TMap<FOGContentId, FOGContentPackageRecord> Authored;
    TArray<FString> Mounted, MountCalls, UnmountCalls;
    FString RejectMount, RejectUnmount;
    bool bSafeUnload = true;
    ~FOptionalFixture() { Store.Close(); IFileManager::Get().DeleteDirectory(*Root, false, true); }
    bool Open() { IFileManager::Get().MakeDirectory(*Root, true); return Store.Open(DatabasePath, Error); }
    bool Add(const TCHAR* Id, FOGContentPackageRecord& Record, bool bExternal = true, int32 Version = 1)
    {
        Record.PackageId = FOGContentId(Id); Record.Version = Version;
        Record.bInstalled = true; Record.bValidated = true;
        Record.ManifestJson = TEXT("{\"fixture\":\"native-authored\"}");
        Record.ContentHash = TEXT("embedded-fixture");
        if (bExternal)
        {
            Record.InstallUri = FPaths::Combine(Root, FPaths::MakeValidFileName(Id) + TEXT(".pak"));
            if (!FFileHelper::SaveStringToFile(TEXT("isolated host fixture bytes"), *Record.InstallUri,
                FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)) return false;
            FString Hash;
            if (!FOGLocallyInstalledPackageProvider::HashFileSha256(Record.InstallUri, Hash, Error)) return false;
            Record.ContentHash = TEXT("sha256:") + Hash;
        }
        else Record.InstallUri = TEXT("embedded:fixture");
        Authored.Add(Record.PackageId, Record);
        return FOGPackageManagerService(Store).RegisterPackage(Record, Error);
    }
    FOGOptionalPackageHostCallbacks Callbacks()
    {
        FOGOptionalPackageHostCallbacks C;
        C.UsesContainer = [](const auto& P) { return !P.InstallUri.IsEmpty() && !P.InstallUri.StartsWith(TEXT("embedded:")); };
        C.ResolveContainer = [this](const auto& P, auto& Out, auto& E)
        {
            const auto* Native = Authored.Find(P.PackageId);
            if (!Native || Native->InstallUri != P.InstallUri)
            { E = TEXT("Fixture native authority refuses imported path."); return false; }
            Out.Filename = Native->InstallUri; Out.VerificationIdentity = Native->ContentHash; return true;
        };
        // Physical-engine behavior alone is faked. Artifact verification still reads actual bytes
        // against the independent fixture's authored ID/version/manifest/path/hash decision.
        C.VerifyArtifact = [this](const auto& P, const auto& Container, auto& E)
        {
            const auto* Native = Authored.Find(P.PackageId);
            FString Hash;
            if (!Native || Native->Version != P.Version || Native->ManifestJson != P.ManifestJson ||
                Native->ContentHash != P.ContentHash || Native->InstallUri != Container.Filename ||
                !FOGLocallyInstalledPackageProvider::HashFileSha256(Container.Filename, Hash, E) ||
                Native->ContentHash != TEXT("sha256:") + Hash)
            { E = TEXT("Fixture native artifact verification failed."); return false; }
            return true;
        };
        C.CanTakeOwnership = [this](const auto& Container, auto& E)
        {
            if (Mounted.Contains(Container.Filename)) { E = TEXT("Already engine-owned fixture container."); return false; }
            return true;
        };
        C.CanUnload = [this](const auto&, auto& E)
        { if (!bSafeUnload) E = TEXT("Fixture external object/stream still references the package."); return bSafeUnload; };
        return C;
    }
    FOGOptionalPackageEngineAdapter Engine()
    {
        FOGOptionalPackageEngineAdapter E;
        E.IsAvailable = [] { return true; };
        E.Mount = [this](const FString& Filename, int32)
        {
            MountCalls.Add(Filename);
            if (Filename == RejectMount) return false;
            Mounted.Add(Filename); return true;
        };
        E.Unmount = [this](const FString& Filename)
        {
            UnmountCalls.Add(Filename);
            if (Filename == RejectUnmount) return false;
            Mounted.Remove(Filename); return true;
        };
        return E;
    }
    bool Read(const FOGContentId& Id, FOGContentPackageRecord& Out)
    { bool Found = false; return Store.TryReadContentPackageRecord(Id, Found, Out, Error) && Found; }
    bool Trigger(const char* Sql)
    {
        // Schema DDL from a second connection can contend with the live canonical store.
        // Release that handle, install/remove the test-only trigger, then reopen the same
        // canonical database. Hosts keep a reference to the Store object, not its handle.
        Store.Close();
        sqlite3* Database = nullptr;
        FTCHARToUTF8 Utf8(*DatabasePath);
        const bool Opened = sqlite3_open_v2(Utf8.Get(), &Database,
            SQLITE_OPEN_READWRITE | SQLITE_OPEN_FULLMUTEX, nullptr) == SQLITE_OK;
        if (Opened) sqlite3_busy_timeout(Database, 5000);
        char* SqlError = nullptr;
        const bool Success = Opened && sqlite3_exec(Database, Sql, nullptr, nullptr, &SqlError) == SQLITE_OK;
        if (!Success)
        {
            Error = SqlError ? UTF8_TO_TCHAR(SqlError)
                : (Database ? UTF8_TO_TCHAR(sqlite3_errmsg(Database)) : TEXT("Unable to open trigger fixture database."));
        }
        if (SqlError) sqlite3_free(SqlError);
        if (Database) sqlite3_close_v2(Database);
        FString ReopenError;
        const bool Reopened = Store.Open(DatabasePath, ReopenError);
        if (!Reopened) Error = Error.IsEmpty() ? ReopenError : Error + TEXT("; reopen: ") + ReopenError;
        return Success && Reopened;
    }
};

bool WriteEmptyIndexedPak(const FString& Filename)
{
    // Real unencrypted version-8 pak index/footer, with zero files. No UnrealPak process required.
    FBufferArchive Index;
    FString MountPoint = FPaths::ProjectContentDir();
    int32 EntryCount = 0;
    Index << MountPoint; Index << EntryCount;
    FPakInfo Info;
    Info.Version = FPakInfo::PakFile_Version_FNameBasedCompressionMethod;
    Info.IndexOffset = 0; Info.IndexSize = Index.Num();
    FSHA1::HashBuffer(Index.GetData(), Index.Num(), Info.IndexHash.Hash);
    FBufferArchive Bytes;
    Bytes.Serialize(Index.GetData(), Index.Num());
    Info.Serialize(Bytes, Info.Version);
    return FFileHelper::SaveArrayToFile(Bytes, *Filename);
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOGOptionalPackageHostReadinessTest,
    "OfflineGame.Runtime.OptionalPackages.EmbeddedExternalTrustAndLeases",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FOGOptionalPackageHostReadinessTest::RunTest(const FString&)
{
    FOptionalFixture F;
    if (!TestTrue(TEXT("Open isolated canonical store"), F.Open())) return false;
    FOGContentPackageRecord Embedded, External, Imported;
    if (!TestTrue(TEXT("Register embedded package"), F.Add(TEXT("diagnostic:optional.embedded"), Embedded, false)) ||
        !TestTrue(TEXT("Register independently approved external package"), F.Add(TEXT("diagnostic:optional.external"), External))) return false;
    FOGOptionalPackageHost Host(F.Store, F.Callbacks(), F.Engine());
    FGuid A, B;
    TestTrue(TEXT("Embedded package acquires without engine mount"), Host.Acquire(Embedded.PackageId, A, F.Error));
    TestEqual(TEXT("Embedded package made zero external mount calls"), F.MountCalls.Num(), 0);
    TestTrue(TEXT("Release embedded lease"), Host.Release(A, F.Error));
    TestTrue(TEXT("Remove embedded residency bookkeeping"), Host.Unmount(Embedded.PackageId, F.Error));
    TestTrue(TEXT("External package mounts after byte verification"), Host.Acquire(External.PackageId, A, F.Error));
    TestTrue(TEXT("Second consumer shares physical mount"), Host.Acquire(External.PackageId, B, F.Error));
    TestEqual(TEXT("One physical mount serves both consumers"), F.MountCalls.Num(), 1);
    TestFalse(TEXT("Leased container cannot unload"), Host.Unmount(External.PackageId, F.Error));
    TestTrue(TEXT("Release first consumer"), Host.Release(A, F.Error));
    TestFalse(TEXT("Second lease still prevents unload"), Host.Unmount(External.PackageId, F.Error));
    TestTrue(TEXT("Release last consumer"), Host.Release(B, F.Error));
    F.bSafeUnload = false;
    TestFalse(TEXT("External UObject/stream guard blocks unload after loader releases"), Host.Unmount(External.PackageId, F.Error));
    TestTrue(TEXT("Residency remains tracked while engine safety guard refuses"), Host.IsMounted(External.PackageId));
    F.bSafeUnload = true;
    TestTrue(TEXT("Safe engine unload succeeds"), Host.Unmount(External.PackageId, F.Error));
    F.Mounted.Add(External.InstallUri); // Simulate a startup/foreign owner in the isolated engine.
    TestFalse(TEXT("Host refuses to steal an engine-startup mount"), Host.Acquire(External.PackageId, A, F.Error));
    TestEqual(TEXT("Ownership refusal issues no mount call"), F.MountCalls.Num(), 1);
    F.Mounted.Remove(External.InstallUri);
    TestFalse(TEXT("Duplicate released lease cannot decrement residency"), Host.Release(B, F.Error));
    Imported = External; Imported.PackageId = FOGContentId(TEXT("diagnostic:optional.imported"));
    TestTrue(TEXT("Imported metadata can enter world store"), FOGPackageManagerService(F.Store).RegisterPackage(Imported, F.Error));
    TestFalse(TEXT("Installed/validated metadata cannot authorize local bytes"), Host.Acquire(Imported.PackageId, A, F.Error));
    TestFalse(TEXT("Failed untrusted acquire returns no lease"), A.IsValid());
    TestEqual(TEXT("Untrusted metadata performed no mount"), F.MountCalls.Num(), 1);
    FOGOptionalAssetRuntime NoHost(F.Store);
    FOGOptionalAssetReference Ref{External.PackageId, FSoftObjectPath(TEXT("/Game/Absent.Absent"))};
    UObject* Asset = nullptr;
    TestFalse(TEXT("Absent host cannot treat external metadata as embedded"), NoHost.ActivateAndLoad(Ref, Asset, F.Error));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOGOptionalPackageClosureOrderTest,
    "OfflineGame.Runtime.OptionalPackages.DependencyVersionsPhysicalIdentityAndOrder",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FOGOptionalPackageClosureOrderTest::RunTest(const FString&)
{
    FOptionalFixture F;
    if (!TestTrue(TEXT("Open dependency fixture"), F.Open())) return false;
    FOGContentPackageRecord Dep, Parent;
    if (!F.Add(TEXT("diagnostic:optional.dependency"), Dep) || !F.Add(TEXT("diagnostic:optional.parent"), Parent)) return false;
    FOGPackageDependencyRecord Edge;
    Edge.PackageId = Parent.PackageId; Edge.DependencyPackageId = Dep.PackageId; Edge.MinimumVersion = 2;
    TestTrue(TEXT("Persist minimum-version dependency"), F.Store.UpsertPackageDependency(Edge, F.Error));
    FOGOptionalPackageHost Host(F.Store, F.Callbacks(), F.Engine());
    FGuid Lease;
    TestFalse(TEXT("Too-old dependency rejects whole closure before mounts"), Host.Acquire(Parent.PackageId, Lease, F.Error));
    TestEqual(TEXT("Old dependency caused no physical operation"), F.MountCalls.Num(), 0);
    Dep.Version = 2; Dep.bInstalled = false;
    TestTrue(TEXT("Persist unavailable local dependency"), F.Store.UpsertContentPackageRecord(Dep, F.Error));
    TestFalse(TEXT("Unavailable dependency refuses the entire closure"), Host.Acquire(Parent.PackageId, Lease, F.Error));
    TestEqual(TEXT("Unavailable dependency performs no physical mount"), F.MountCalls.Num(), 0);
    Dep.bInstalled = true;
    Dep.Version = 2; F.Authored.Add(Dep.PackageId, Dep);
    TestTrue(TEXT("Upgrade dependency through canonical manager"), FOGPackageManagerService(F.Store).RegisterPackage(Dep, F.Error));
    if (!TestTrue(TEXT("Acquire ready dependency closure"), Host.Acquire(Parent.PackageId, Lease, F.Error)) ||
        !TestEqual(TEXT("Ready closure mounted exactly two containers"), F.MountCalls.Num(), 2)) return false;
    TestEqual(TEXT("Dependency is mounted first"), F.MountCalls[0], Dep.InstallUri);
    TestEqual(TEXT("Dependent is mounted second"), F.MountCalls[1], Parent.InstallUri);
    TestTrue(TEXT("Release closure lease"), Host.Release(Lease, F.Error));
    TestFalse(TEXT("Dependency cannot unmount ahead of mounted dependent"), Host.Unmount(Dep.PackageId, F.Error));
    if (!TestTrue(TEXT("Reverse dependency shutdown succeeds"), Host.UnmountAll(F.Error)) ||
        !TestEqual(TEXT("Reverse shutdown unmounted exactly two containers"), F.UnmountCalls.Num(), 2)) return false;
    TestEqual(TEXT("Dependent is unmounted first"), F.UnmountCalls[0], Parent.InstallUri);
    TestEqual(TEXT("Dependency is unmounted last"), F.UnmountCalls[1], Dep.InstallUri);
    Parent.InstallUri = Dep.InstallUri; Parent.ContentHash = Dep.ContentHash; F.Authored.Add(Parent.PackageId, Parent);
    TestTrue(TEXT("Persist duplicate physical identity fixture"), FOGPackageManagerService(F.Store).RegisterPackage(Parent, F.Error));
    const int32 Before = F.MountCalls.Num();
    TestFalse(TEXT("Two logical IDs cannot own one physical container"), Host.Acquire(Parent.PackageId, Lease, F.Error));
    TestEqual(TEXT("Duplicate closure mounts nothing"), F.MountCalls.Num(), Before);
    Edge.PackageId = Dep.PackageId; Edge.DependencyPackageId = Parent.PackageId; Edge.MinimumVersion = 1;
    TestTrue(TEXT("Inject cycle through store seam"), F.Store.UpsertPackageDependency(Edge, F.Error));
    TestFalse(TEXT("Dependency cycle refuses acquisition"), Host.Acquire(Parent.PackageId, Lease, F.Error));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOGOptionalPackageRollbackTest,
    "OfflineGame.Runtime.OptionalPackages.FailedMountActivationAndUnmountRetainResidency",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FOGOptionalPackageRollbackTest::RunTest(const FString&)
{
    FOptionalFixture F;
    if (!F.Open()) return false;
    FOGContentPackageRecord Dep, Parent;
    if (!F.Add(TEXT("diagnostic:optional.rollback_dep"), Dep) || !F.Add(TEXT("diagnostic:optional.rollback_parent"), Parent)) return false;
    FOGPackageDependencyRecord Edge;
    Edge.PackageId = Parent.PackageId; Edge.DependencyPackageId = Dep.PackageId;
    if (!FOGPackageManagerService(F.Store).SetDependency(Edge, F.Error)) return false;
    FOGOptionalPackageHost Host(F.Store, F.Callbacks(), F.Engine());
    FGuid Lease;
    F.RejectMount = Parent.InstallUri;
    TestFalse(TEXT("Second physical mount failure rolls back first mount"), Host.Acquire(Parent.PackageId, Lease, F.Error));
    TestFalse(TEXT("Successful physical rollback leaves no container"), Host.HasMountedContainers());
    TestFalse(TEXT("Failure never grants a lease"), Host.HasOutstandingLeases());
    F.RejectUnmount = Dep.InstallUri;
    TestFalse(TEXT("Mount failure with unmount failure is reported"), Host.Acquire(Parent.PackageId, Lease, F.Error));
    TestTrue(TEXT("Failed physical rollback retains dependency residency"), Host.IsMounted(Dep.PackageId));
    TestTrue(TEXT("Failure explains retained residency"), F.Error.Contains(TEXT("rollback retained")));
    F.RejectMount.Reset(); F.RejectUnmount.Reset();
    TestTrue(TEXT("Retained dependency can be safely retried"), Host.UnmountAll(F.Error));
    TestTrue(TEXT("Install real canonical activation failure trigger"), F.Trigger(
        "CREATE TRIGGER reject_optional_activation BEFORE UPDATE OF activated ON content_packages "
        "WHEN NEW.package_id = 'diagnostic:optional.rollback_parent' AND NEW.activated = 1 "
        "BEGIN SELECT RAISE(ABORT, 'fixture activation rejection'); END;"));
    F.RejectUnmount = Parent.InstallUri;
    TestFalse(TEXT("Canonical activation failure rolls back logical activation"), Host.Acquire(Parent.PackageId, Lease, F.Error));
    FOGContentPackageRecord Read;
    TestTrue(TEXT("Read original dependency after rollback"), F.Read(Dep.PackageId, Read));
    TestFalse(TEXT("Dependency activation was rolled back"), Read.bActivated);
    TestTrue(TEXT("Read original parent after rollback"), F.Read(Parent.PackageId, Read));
    TestFalse(TEXT("Parent activation remains false"), Read.bActivated);
    TestTrue(TEXT("Failed parent unmount remains tracked"), Host.IsMounted(Parent.PackageId));
    TestTrue(TEXT("Still-mounted dependent protects dependency"), Host.IsMounted(Dep.PackageId));
    TestFalse(TEXT("Canonical failure grants no consumer lease"), Host.HasOutstandingLeases());
    F.RejectUnmount.Reset();
    TestTrue(TEXT("Remove canonical failure trigger"), F.Trigger("DROP TRIGGER reject_optional_activation;"));
    TestTrue(TEXT("Retry reverse dependency cleanup"), Host.UnmountAll(F.Error));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOGOptionalAssetLeaseTest,
    "OfflineGame.Runtime.OptionalPackages.MissingAssetsAndHostReplacementPreserveLogicalState",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FOGOptionalAssetLeaseTest::RunTest(const FString&)
{
    FOptionalFixture F;
    if (!F.Open()) return false;
    FOGContentPackageRecord Package;
    if (!F.Add(TEXT("diagnostic:optional.asset_lease"), Package)) return false;
    FOGOptionalPackageHost Host(F.Store, F.Callbacks(), F.Engine());
    FOGOptionalPackageHost Replacement(F.Store, F.Callbacks(), F.Engine());
    FOGOptionalAssetRuntime Loader(F.Store, &Host);
    FOGOptionalAssetReference Missing{Package.PackageId, FSoftObjectPath(TEXT("/Game/__ClosureMissing__/NoAsset.NoAsset"))};
    UObject* Asset = nullptr;
    TestFalse(TEXT("Missing optional asset falls back"), Loader.ActivateAndLoad(Missing, Asset, F.Error));
    TestNull(TEXT("Missing asset returns null"), Asset);
    TestFalse(TEXT("Missing asset releases its physical lease"), Host.HasOutstandingLeases());
    FOGContentPackageRecord Read;
    TestTrue(TEXT("Logical package remains present"), F.Read(Package.PackageId, Read));
    TestEqual(TEXT("Missing asset preserves content identity"), Read.ContentHash, Package.ContentHash);
    TestEqual(TEXT("Missing asset preserves manifest"), Read.ManifestJson, Package.ManifestJson);
    TestTrue(TEXT("Missing asset leaves local installation intact"), Read.bInstalled && Read.bValidated);
    TStrongObjectPtr<UObject> Existing(NewObject<USoundClass>(GetTransientPackage()));
    FOGOptionalAssetReference Ref{Package.PackageId, FSoftObjectPath(Existing.Get())};
    TestTrue(TEXT("Load already-resolvable fixture asset"), Loader.ActivateAndLoad(Ref, Asset, F.Error));
    TestTrue(TEXT("Asset residency owns physical lease"), Host.HasOutstandingLeases());
    TestFalse(TEXT("Replacing a leased host is refused"), Loader.SetContainerHost(&Replacement, F.Error));
    TestFalse(TEXT("Destroying/unmounting leased host is refused"), Host.UnmountAll(F.Error));
    Loader.Release(Ref);
    TestFalse(TEXT("Release drops final physical lease"), Host.HasOutstandingLeases());
    TestTrue(TEXT("Host replacement succeeds after asset references release"), Loader.SetContainerHost(&Replacement, F.Error));
    TestTrue(TEXT("Old host can now finish physical shutdown"), Host.UnmountAll(F.Error));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOGOptionalNativeReceiptTest,
    "OfflineGame.Runtime.OptionalPackages.NativeReceiptsVerifyBytesAndRejectImportedProvenance",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FOGOptionalNativeReceiptTest::RunTest(const FString&)
{
    FOptionalFixture F;
    if (!F.Open()) return false;
    const FString Source = FPaths::Combine(F.Root, TEXT("native.pak"));
    TestTrue(TEXT("Write a real indexed pak fixture"), WriteEmptyIndexedPak(Source));
    const FString Vector = FPaths::Combine(F.Root, TEXT("sha256-vector"));
    const uint8 VectorBytes[] = { 'a', 'b', 'c' };
    TestTrue(TEXT("Write standard SHA256 vector bytes"), FFileHelper::SaveArrayToFile(MakeArrayView(VectorBytes), *Vector));
    FString Hash;
    TestTrue(TEXT("Stream standard SHA256 vector"), FOGLocallyInstalledPackageProvider::HashFileSha256(Vector, Hash, F.Error));
    TestEqual(TEXT("SHA256 abc known vector"), Hash,
        FString(TEXT("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad")));
    FOGLocallyInstalledPackageProvider Provider(FPaths::Combine(F.Root, TEXT("native-authority")));
    FOGTrustedOptionalPackageInstall Expected;
    Expected.PackageId = FOGContentId(TEXT("diagnostic:optional.native_receipt")); Expected.Version = 3;
    Expected.ManifestJson = TEXT("{\"purpose\":\"native-receipt-fixture\"}");
    if (!FOGLocallyInstalledPackageProvider::HashFileSha256(Source, Expected.Sha256, F.Error)) return false;
    FString Uri;
    TestTrue(TEXT("Native installer verifies and registers real pak"), Provider.InstallVerifiedPak(F.Store, Expected, Source, Uri, F.Error));
    FOGContentPackageRecord Record;
    TestTrue(TEXT("Read native-installed canonical record"), F.Read(Expected.PackageId, Record));
    auto Callbacks = Provider.MakeCallbacks(F.Store);
    FOGOptionalPackageContainer Container;
    TestTrue(TEXT("External native record needs physical container"), Callbacks.UsesContainer(Record));
    TestTrue(TEXT("Native receipt resolves exact managed path"), Callbacks.ResolveContainer(Record, Container, F.Error));
    TestTrue(TEXT("Real byte verifier approves native-installed artifact"), Callbacks.VerifyArtifact(Record, Container, F.Error));
    FOGContentPackageRecord Forged = Record;
    Forged.PackageId = FOGContentId(TEXT("diagnostic:optional.forged"));
    TestFalse(TEXT("Imported ID cannot borrow another native receipt"), Callbacks.ResolveContainer(Forged, Container, F.Error));
    Forged = Record; Forged.Version++;
    TestFalse(TEXT("Imported version cannot alter provenance"), Callbacks.ResolveContainer(Forged, Container, F.Error));
    Forged = Record; Forged.ManifestJson = TEXT("{\"purpose\":\"imported\"}");
    TestFalse(TEXT("Imported manifest must match native content expectation"), Callbacks.ResolveContainer(Forged, Container, F.Error));
    Forged = Record; Forged.InstallUri = Source;
    TestFalse(TEXT("Imported local path cannot redirect approved bytes"), Callbacks.ResolveContainer(Forged, Container, F.Error));
    Forged = Record; Forged.InstallUri = FPaths::ChangeExtension(Uri, TEXT("utoc"));
    TestFalse(TEXT("IoStore external URI is explicitly unsupported"), Callbacks.ResolveContainer(Forged, Container, F.Error));
    FOGContentPackageRecord Embedded = Record; Embedded.InstallUri = TEXT("embedded:foundation_character_visuals");
    TestFalse(TEXT("Explicit embedded convention grants no external mount authority"), Callbacks.UsesContainer(Embedded));
    TestTrue(TEXT("Resolve original after provenance refusal checks"), Callbacks.ResolveContainer(Record, Container, F.Error));
    TestTrue(TEXT("Mutate installed bytes after registration"), FFileHelper::SaveStringToFile(TEXT("tampered bytes"), *Uri));
    TestFalse(TEXT("Receipt metadata alone cannot authorize changed bytes"), Callbacks.VerifyArtifact(Record, Container, F.Error));
    TestFalse(TEXT("Imported fake SHA256 cannot mint native install authority"), Provider.InstallVerifiedPak(
        F.Store, FOGTrustedOptionalPackageInstall{Expected.PackageId, 4, FString::ChrN(64, TEXT('0')), TEXT("{}"), 0},
        Source, Uri, F.Error));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOGOptionalNativeStorageMoveTest,
    "OfflineGame.Runtime.OptionalPackages.NativeStorageMovePreservesReceiptAndFailedPersistence",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FOGOptionalNativeStorageMoveTest::RunTest(const FString&)
{
    FOptionalFixture F;
    if (!F.Open()) return false;
    const FString Source = FPaths::Combine(F.Root, TEXT("native-move.pak"));
    if (!WriteEmptyIndexedPak(Source)) return false;
    FOGLocallyInstalledPackageProvider Provider;
    FOGTrustedOptionalPackageInstall Expected;
    Expected.PackageId = FOGContentId(TEXT("diagnostic:optional.native_move_") +
        FGuid::NewGuid().ToString(EGuidFormats::Digits).ToLower());
    if (!FOGLocallyInstalledPackageProvider::HashFileSha256(Source, Expected.Sha256, F.Error)) return false;
    FString OriginalUri;
    if (!TestTrue(TEXT("Install native storage-move fixture"), Provider.InstallVerifiedPak(
            F.Store, Expected, Source, OriginalUri, F.Error))) return false;
    FOGContentPackageRecord Original;
    if (!F.Read(Expected.PackageId, Original)) return false;
    const FString ReceiptKey = Original.PackageId.ToString() + TEXT("|") + FString::FromInt(Original.Version) + TEXT("|") + Original.ContentHash;
    const FString ReceiptFilename = FPaths::Combine(FOGLocallyInstalledPackageProvider::DefaultRoot(),
        TEXT("Receipts"), FMD5::HashAnsiString(*ReceiptKey) + TEXT(".json"));
    ON_SCOPE_EXIT
    {
        IFileManager::Get().Delete(*ReceiptFilename);
        IFileManager::Get().DeleteDirectory(*FPaths::GetPath(OriginalUri), false, true);
    };
    auto Callbacks = Provider.MakeCallbacks(F.Store);
    FOGOptionalPackageContainer Container;
    const FString DestinationRoot = FPaths::Combine(F.Root, TEXT("moved-storage"));
    FString NewUri;
    TestTrue(TEXT("Install canonical storage persistence failure trigger"), F.Trigger(
        "CREATE TRIGGER reject_native_move BEFORE UPDATE OF install_uri ON content_packages "
        "WHEN NEW.install_uri <> OLD.install_uri BEGIN SELECT RAISE(ABORT, 'native move rejection'); END;"));
    TestFalse(TEXT("Native storage move reports canonical rejection"), FOGPackageManagerService(F.Store).MovePackageStorage(
        Original.PackageId, DestinationRoot, FName(TEXT("managed_external")), NewUri, F.Error));
    TestTrue(TEXT("Failed canonical move preserves original bytes"), IFileManager::Get().FileExists(*OriginalUri));
    TestTrue(TEXT("Failed move restores independent original receipt"), Callbacks.ResolveContainer(Original, Container, F.Error));
    TestTrue(TEXT("Restored receipt verifies original bytes"), Callbacks.VerifyArtifact(Original, Container, F.Error));
    TestFalse(TEXT("Rejected copied payload is removed"), IFileManager::Get().FileExists(
        *FPaths::Combine(DestinationRoot, FPaths::GetCleanFilename(OriginalUri))));
    TestTrue(TEXT("Remove canonical storage failure trigger"), F.Trigger("DROP TRIGGER reject_native_move;"));
    TestTrue(TEXT("Existing native storage operation migrates receipt"), FOGPackageManagerService(F.Store).MovePackageStorage(
        Original.PackageId, DestinationRoot, FName(TEXT("managed_external")), NewUri, F.Error));
    FOGContentPackageRecord Moved;
    TestTrue(TEXT("Read moved canonical installation"), F.Read(Original.PackageId, Moved));
    TestEqual(TEXT("Canonical URI is the copied destination"), Moved.InstallUri, NewUri);
    TestFalse(TEXT("Old bytes removed only after receipt and canonical update"), IFileManager::Get().FileExists(*OriginalUri));
    TestTrue(TEXT("Independent authority resolves migrated receipt"), Callbacks.ResolveContainer(Moved, Container, F.Error));
    TestTrue(TEXT("Migrated bytes preserve native hash verification"), Callbacks.VerifyArtifact(Moved, Container, F.Error));
    TestFalse(TEXT("Imported old URI cannot redirect the migrated receipt"), Callbacks.ResolveContainer(Original, Container, F.Error));
    return true;
}

#endif
