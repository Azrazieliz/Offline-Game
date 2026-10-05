#include "Persistence/OGSQLiteWorldStore.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

namespace
{
FString MakeTestDirectory()
{
    return FPaths::Combine(
        FPaths::ProjectSavedDir(),
        TEXT("Automation"),
        FGuid::NewGuid().ToString(EGuidFormats::Digits));
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOGPersistenceRestartTest,
    "OfflineGame.Persistence.EntitySurvivesRestart",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FOGPersistenceRestartTest::RunTest(const FString& Parameters)
{
    const FString Directory = MakeTestDirectory();
    const FString DatabasePath = FPaths::Combine(Directory, TEXT("restart.db"));
    IFileManager::Get().MakeDirectory(*Directory, true);

    const FOGEntityId EntityId = FOGEntityId::NewId();

    {
        FOGSQLiteWorldStore Store;
        FString Error;
        TestTrue(TEXT("Open database"), Store.Open(DatabasePath, Error));
        TestEqual(TEXT("Schema version is 10"), Store.GetSchemaVersion(Error), 10);

        TestTrue(
            TEXT("Persist entity"),
            Store.UpsertEntity(
                EntityId,
                TEXT("test_character"),
                100,
                TEXT("{\"level\":7}"),
                Error));

        FOGWorldEvent Event;
        Event.EventId = FOGEntityId::NewId();
        Event.EventType = TEXT("test.persisted");
        Event.WorldTick = 100;
        Event.PrimaryEntity = EntityId;
        Event.PayloadJson = TEXT("{\"proof\":true}");
        Event.bChronicleEligible = true;
        TestTrue(TEXT("Append event"), Store.AppendWorldEvent(Event, Error));
    }

    {
        FOGSQLiteWorldStore Store;
        FString Error;
        TestTrue(TEXT("Reopen database"), Store.Open(DatabasePath, Error));

        bool bFound = false;
        FName Kind = NAME_None;
        FString StateJson;
        int64 Revision = -1;

        TestTrue(
            TEXT("Read entity after restart"),
            Store.TryReadEntity(EntityId, bFound, Kind, StateJson, Revision, Error));

        TestTrue(TEXT("Entity exists after restart"), bFound);
        TestEqual(TEXT("Entity kind persisted"), Kind, FName(TEXT("test_character")));
        TestEqual(TEXT("Entity state persisted"), StateJson, FString(TEXT("{\"level\":7}")));
        TestEqual(TEXT("Initial revision is zero"), Revision, static_cast<int64>(0));

        FString IntegrityReport;
        TestTrue(TEXT("Integrity check passes"), Store.RunIntegrityCheck(IntegrityReport, Error));
        TestEqual(TEXT("Integrity report is ok"), IntegrityReport, FString(TEXT("ok")));
    }

    IFileManager::Get().DeleteDirectory(*Directory, false, true);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOGPersistenceBackupRestoreTest,
    "OfflineGame.Persistence.BackupRestore",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FOGPersistenceBackupRestoreTest::RunTest(const FString& Parameters)
{
    const FString Directory = MakeTestDirectory();
    const FString DatabasePath = FPaths::Combine(Directory, TEXT("world.db"));
    const FString BackupPath = FPaths::Combine(Directory, TEXT("snapshot.db"));
    IFileManager::Get().MakeDirectory(*Directory, true);

    FOGSQLiteWorldStore Store;
    FString Error;
    TestTrue(TEXT("Open database"), Store.Open(DatabasePath, Error));

    const FOGEntityId EntityId = FOGEntityId::NewId();
    TestTrue(
        TEXT("Persist v1"),
        Store.UpsertEntity(EntityId, TEXT("test"), 0, TEXT("{\"value\":1}"), Error));

    TestTrue(TEXT("Create backup"), Store.BackupTo(BackupPath, Error));

    TestTrue(
        TEXT("Persist v2"),
        Store.UpsertEntity(EntityId, TEXT("test"), 0, TEXT("{\"value\":2}"), Error));

    TestTrue(TEXT("Restore backup"), Store.RestoreFrom(BackupPath, Error));

    bool bFound = false;
    FName Kind = NAME_None;
    FString StateJson;
    int64 Revision = -1;
    TestTrue(
        TEXT("Read restored entity"),
        Store.TryReadEntity(EntityId, bFound, Kind, StateJson, Revision, Error));

    TestTrue(TEXT("Restored entity exists"), bFound);
    TestEqual(TEXT("Backup restored previous state"), StateJson, FString(TEXT("{\"value\":1}")));

    Store.Close();
    IFileManager::Get().DeleteDirectory(*Directory, false, true);
    return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOGCharacterManifestationPersistenceTest,
    "OfflineGame.Persistence.CharacterManifestationSurvivesRestart",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FOGCharacterManifestationPersistenceTest::RunTest(const FString& Parameters)
{
    const FString Directory = MakeTestDirectory();
    const FString DatabasePath = FPaths::Combine(Directory, TEXT("manifestation.db"));
    IFileManager::Get().MakeDirectory(*Directory, true);

    const FOGEntityId RulerId = FOGEntityId::NewId();
    const FOGEntityId ManifestationId = FOGEntityId::NewId();

    {
        FOGSQLiteWorldStore Store;
        FString Error;
        TestTrue(TEXT("Open database"), Store.Open(DatabasePath, Error));
        TestEqual(TEXT("Schema version is 10"), Store.GetSchemaVersion(Error), 10);

        TestTrue(
            TEXT("Persist owning Ruler"),
            Store.UpsertEntity(RulerId, TEXT("ruler"), 0, TEXT("{}"), Error));

        FOGCharacterManifestationRecord Manifestation;
        Manifestation.ManifestationId = ManifestationId;
        Manifestation.OwningRulerId = RulerId;
        Manifestation.IdentityId = FOGContentId(TEXT("test:character.identity"));
        Manifestation.ActiveVersionId = FOGContentId(TEXT("test:character.identity.base"));
        Manifestation.Level = 37;
        Manifestation.CurrentRarity = TEXT("test_rarity");
        Manifestation.ProgressionStateJson = TEXT("{\"branch\":\"alpha\"}");

        TestTrue(
            TEXT("Persist Character Manifestation"),
            Store.UpsertCharacterManifestation(Manifestation, 10, Error));
    }

    {
        FOGSQLiteWorldStore Store;
        FString Error;
        TestTrue(TEXT("Reopen database"), Store.Open(DatabasePath, Error));

        bool bFound = false;
        FOGCharacterManifestationRecord Loaded;
        TestTrue(
            TEXT("Read Manifestation after restart"),
            Store.TryReadCharacterManifestation(
                ManifestationId,
                bFound,
                Loaded,
                Error));

        TestTrue(TEXT("Manifestation exists after restart"), bFound);
        TestTrue(TEXT("Owner persisted"), Loaded.OwningRulerId == RulerId);
        TestEqual(
            TEXT("Identity persisted"),
            Loaded.IdentityId,
            FOGContentId(TEXT("test:character.identity")));
        TestEqual(
            TEXT("Version persisted"),
            Loaded.ActiveVersionId,
            FOGContentId(TEXT("test:character.identity.base")));
        TestEqual(TEXT("Level persisted"), Loaded.Level, 37);
        TestEqual(TEXT("Current rarity persisted"), Loaded.CurrentRarity, FName(TEXT("test_rarity")));
        TestEqual(
            TEXT("Progression payload persisted"),
            Loaded.ProgressionStateJson,
            FString(TEXT("{\"branch\":\"alpha\"}")));
    }

    IFileManager::Get().DeleteDirectory(*Directory, false, true);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOGContentPackageActivationTest,
    "OfflineGame.Persistence.ContentPackage.RequiresValidationBeforeActivation",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FOGContentPackageActivationTest::RunTest(const FString& Parameters)
{
    const FString Directory = MakeTestDirectory();
    const FString DatabasePath = FPaths::Combine(Directory, TEXT("packages.db"));
    IFileManager::Get().MakeDirectory(*Directory, true);

    FOGSQLiteWorldStore Store;
    FString Error;
    TestTrue(TEXT("Open database"), Store.Open(DatabasePath, Error));

    const FOGContentId PackageId(TEXT("test:characters.pack"));

    TestTrue(
        TEXT("Register installed but unvalidated package"),
        Store.UpsertContentPackage(
            PackageId,
            1,
            TEXT("hash-v1"),
            true,
            false,
            TEXT("{}"),
            Error));

    TestFalse(
        TEXT("Unvalidated package cannot activate"),
        Store.SetContentPackageActivated(PackageId, true, Error));

    TestTrue(
        TEXT("Register validated package"),
        Store.UpsertContentPackage(
            PackageId,
            1,
            TEXT("hash-v1"),
            true,
            true,
            TEXT("{}"),
            Error));

    TestTrue(
        TEXT("Validated package can activate"),
        Store.SetContentPackageActivated(PackageId, true, Error));

    bool bKnown = false;
    bool bActivated = false;
    TestTrue(
        TEXT("Read package activation"),
        Store.IsContentPackageActivated(
            PackageId,
            bKnown,
            bActivated,
            Error));

    TestTrue(TEXT("Package is known"), bKnown);
    TestTrue(TEXT("Package is active"), bActivated);

    TestTrue(
        TEXT("Changing package content resets activation"),
        Store.UpsertContentPackage(
            PackageId,
            2,
            TEXT("hash-v2"),
            true,
            true,
            TEXT("{}"),
            Error));

    bKnown = false;
    bActivated = true;
    TestTrue(
        TEXT("Read package activation after content change"),
        Store.IsContentPackageActivated(
            PackageId,
            bKnown,
            bActivated,
            Error));

    TestTrue(TEXT("Package remains known"), bKnown);
    TestFalse(TEXT("New package version requires re-activation"), bActivated);

    Store.Close();
    IFileManager::Get().DeleteDirectory(*Directory, false, true);
    return true;
}

#endif
