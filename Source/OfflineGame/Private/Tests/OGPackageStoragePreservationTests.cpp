#include "Persistence/OGSQLiteWorldStore.h"
#include "Runtime/OGPackageManagerService.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "sqlite/sqlite3.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOGPackageStoragePreservationTest,
    "OfflineGame.Runtime.Packages.StorageMovePreservesExistingAndFailedPersistence",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FOGPackageStoragePreservationTest::RunTest(const FString& Parameters)
{
    const FString Root = FPaths::ConvertRelativePathToFull(FPaths::Combine(
        FPaths::ProjectSavedDir(), TEXT("Automation"), FGuid::NewGuid().ToString(EGuidFormats::Digits)));
    const FString Source = FPaths::Combine(Root, TEXT("original"), TEXT("payload"));
    const FString DestinationRoot = FPaths::Combine(Root, TEXT("destination"));
    const FString Destination = FPaths::Combine(DestinationRoot, TEXT("payload"));
    const FString SourceFile = FPaths::Combine(Source, TEXT("item.txt"));
    const FString DestinationFile = FPaths::Combine(Destination, TEXT("item.txt"));
    IFileManager& Files = IFileManager::Get();
    Files.MakeDirectory(*Source, true);
    Files.MakeDirectory(*Destination, true);
    TestTrue(TEXT("Write original"), FFileHelper::SaveStringToFile(TEXT("original bytes"), *SourceFile));
    TestTrue(TEXT("Write valuable occupied destination"), FFileHelper::SaveStringToFile(TEXT("existing valuable bytes"), *DestinationFile));
    const FString DatabasePath = FPaths::Combine(Root, TEXT("package.db"));
    FString Error;
    FOGSQLiteWorldStore Store;
    if (!TestTrue(TEXT("Open canonical test store"), Store.Open(DatabasePath, Error))) return false;
    FOGContentPackageRecord Package;
    Package.PackageId = FOGContentId(TEXT("diagnostic:package.move_preservation"));
    Package.Version = 1;
    Package.ContentHash = TEXT("diagnostic-hash");
    Package.Category = FName(TEXT("character"));
    Package.StorageClass = FName(TEXT("local_hot"));
    Package.SealedState = FName(TEXT("unsealed"));
    Package.DownloadState = FName(TEXT("installed"));
    Package.bInstalled = true;
    Package.bValidated = true;
    Package.InstallUri = Source;
    FOGPackageManagerService Packages(Store);
    if (!TestTrue(TEXT("Register installed package"), Packages.RegisterPackage(Package, Error))) return false;
    FString NewUri;
    TestFalse(TEXT("Occupied destination is refused"), Packages.MovePackageStorage(
        Package.PackageId, DestinationRoot, FName(TEXT("managed_external")), NewUri, Error));
    FString Text;
    FFileHelper::LoadFileToString(Text, *DestinationFile);
    TestEqual(TEXT("Valuable destination is untouched"), Text, FString(TEXT("existing valuable bytes")));
    TestTrue(TEXT("Original survived occupied destination"), Files.FileExists(*SourceFile));
    TestFalse(TEXT("Directory cannot be copied into its own descendant"), Packages.MovePackageStorage(
        Package.PackageId, FPaths::Combine(Source, TEXT("nested")), FName(TEXT("managed_external")), NewUri, Error));
    TestFalse(TEXT("Rejected nested move creates no directory"), Files.DirectoryExists(*FPaths::Combine(Source, TEXT("nested"))));
    Files.DeleteDirectory(*Destination, false, true);

    // Force the real canonical write to fail after filesystem copy/verification.
    Store.Close();
    sqlite3* Database = nullptr;
    FTCHARToUTF8 PathUtf8(*DatabasePath);
    const bool bOpened = sqlite3_open_v2(PathUtf8.Get(), &Database,
        SQLITE_OPEN_READWRITE | SQLITE_OPEN_FULLMUTEX, nullptr) == SQLITE_OK;
    const bool bTrigger = bOpened && sqlite3_exec(Database,
        "CREATE TRIGGER reject_package_move BEFORE UPDATE OF install_uri ON content_packages "
        "WHEN NEW.install_uri <> OLD.install_uri BEGIN SELECT RAISE(ABORT, 'test reject move'); END;",
        nullptr, nullptr, nullptr) == SQLITE_OK;
    if (!TestTrue(TEXT("Install canonical persistence failure trigger"), bTrigger))
    {
        if (Database) sqlite3_close_v2(Database);
        return false;
    }
    sqlite3_close_v2(Database);
    Database = nullptr;
    if (!TestTrue(TEXT("Reopen canonical store with persisted failure trigger"), Store.Open(DatabasePath, Error))) return false;
    TestFalse(TEXT("Canonical write failure refuses move"), Packages.MovePackageStorage(
        Package.PackageId, DestinationRoot, FName(TEXT("managed_external")), NewUri, Error));
    TestTrue(TEXT("Original remains after canonical write failure"), Files.FileExists(*SourceFile));
    TestFalse(TEXT("Owned successful copy is removed on canonical rejection"), Files.DirectoryExists(*Destination));
    bool bFound = false;
    FOGContentPackageRecord Read;
    TestTrue(TEXT("Read canonical install URI"), Store.TryReadContentPackageRecord(Package.PackageId, bFound, Read, Error));
    TestTrue(TEXT("Package remains registered"), bFound);
    TestEqual(TEXT("Rejected update keeps original canonical URI"), Read.InstallUri, Source);
    Store.Close();
    TestEqual(TEXT("Open trigger cleanup connection"), sqlite3_open_v2(PathUtf8.Get(), &Database,
        SQLITE_OPEN_READWRITE | SQLITE_OPEN_FULLMUTEX, nullptr), SQLITE_OK);
    TestTrue(TEXT("Remove failure trigger"), sqlite3_exec(Database, "DROP TRIGGER reject_package_move;",
        nullptr, nullptr, nullptr) == SQLITE_OK);
    sqlite3_close_v2(Database);
    if (!TestTrue(TEXT("Reopen canonical store without failure trigger"), Store.Open(DatabasePath, Error))) return false;
    TestTrue(TEXT("Verified copy can now become canonical"), Packages.MovePackageStorage(
        Package.PackageId, DestinationRoot, FName(TEXT("managed_external")), NewUri, Error));
    TestEqual(TEXT("Successful move reports actual destination"), NewUri, Destination);
    TestFalse(TEXT("Original removed only after successful canonical update"), Files.DirectoryExists(*Source));
    TestTrue(TEXT("Read copied payload"), FFileHelper::LoadFileToString(Text, *DestinationFile));
    TestEqual(TEXT("Copied bytes match original"), Text, FString(TEXT("original bytes")));
    Store.Close();
    Files.DeleteDirectory(*Root, false, true);
    return true;
}
#endif
