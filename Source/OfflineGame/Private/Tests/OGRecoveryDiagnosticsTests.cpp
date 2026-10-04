#include "Diagnostics/OGDiagnosticsBundle.h"
#include "Persistence/OGSQLiteWorldStore.h"
#include "Persistence/OGSnapshotService.h"
#include "Persistence/OGWorldBootstrap.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "sqlite/sqlite3.h"

namespace
{
FString MakeRecoveryTestDirectory()
{
    return FPaths::Combine(
        FPaths::ProjectSavedDir(),
        TEXT("Automation"),
        FGuid::NewGuid().ToString(EGuidFormats::Digits));
}

bool ExecuteRawDatabaseSql(
    const FString& DatabasePath,
    const char* Sql,
    FString& OutError)
{
    OutError.Reset();

    sqlite3* Database = nullptr;
    FTCHARToUTF8 PathUtf8(*DatabasePath);
    if (sqlite3_open_v2(
            PathUtf8.Get(),
            &Database,
            SQLITE_OPEN_READWRITE | SQLITE_OPEN_FULLMUTEX,
            nullptr) != SQLITE_OK ||
        Database == nullptr)
    {
        OutError = TEXT("Failed to open raw test database.");
        if (Database)
        {
            sqlite3_close_v2(Database);
        }
        return false;
    }

    char* ErrorMessage = nullptr;
    const int32 Result =
        sqlite3_exec(
            Database,
            Sql,
            nullptr,
            nullptr,
            &ErrorMessage);

    if (Result != SQLITE_OK)
    {
        OutError = ErrorMessage
            ? UTF8_TO_TCHAR(ErrorMessage)
            : UTF8_TO_TCHAR(sqlite3_errmsg(Database));
    }

    if (ErrorMessage)
    {
        sqlite3_free(ErrorMessage);
    }

    sqlite3_close_v2(Database);
    return Result == SQLITE_OK;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOGRecoverySnapshotRotationTest,
    "OfflineGame.Persistence.RecoverySnapshotRotation",
    EAutomationTestFlags::ApplicationContextMask |
        EAutomationTestFlags::EngineFilter)

bool FOGRecoverySnapshotRotationTest::RunTest(const FString& Parameters)
{
    const FString Directory = MakeRecoveryTestDirectory();
    const FString DatabasePath = FPaths::Combine(Directory, TEXT("world.db"));
    const FString SnapshotDirectory = FPaths::Combine(Directory, TEXT("snapshots"));
    IFileManager::Get().MakeDirectory(*Directory, true);

    FOGSQLiteWorldStore Store;
    FString Error;
    TestTrue(TEXT("Open database"), Store.Open(DatabasePath, Error));

    const FOGEntityId EntityId = FOGEntityId::NewId();
    TestTrue(TEXT("Persist state before first snapshot"), Store.UpsertEntity(
        EntityId, TEXT("ruler"), 0, TEXT("{\"step\":1}"), Error));

    FString FirstPath;
    TestTrue(TEXT("Create first rotating snapshot"),
        FOGSnapshotService::CreateRotatingSnapshot(
            Store, SnapshotDirectory, 1, FirstPath, Error));
    TestTrue(TEXT("First snapshot exists"),
        IFileManager::Get().FileExists(*FirstPath));

    TestTrue(TEXT("Mutate state before second snapshot"), Store.UpsertEntity(
        EntityId, TEXT("ruler"), 1, TEXT("{\"step\":2}"), Error));

    FString SecondPath;
    TestTrue(TEXT("Create second rotating snapshot"),
        FOGSnapshotService::CreateRotatingSnapshot(
            Store, SnapshotDirectory, 1, SecondPath, Error));
    TestTrue(TEXT("Second snapshot exists"),
        IFileManager::Get().FileExists(*SecondPath));

    TArray<FString> Snapshots;
    IFileManager::Get().FindFiles(
        Snapshots,
        *FPaths::Combine(SnapshotDirectory, TEXT("world_*.db")),
        true,
        false);
    TestEqual(TEXT("Rotation retains requested count"), Snapshots.Num(), 1);

    Store.Close();
    IFileManager::Get().DeleteDirectory(*Directory, false, true);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOGDiagnosticsBundleTest,
    "OfflineGame.Diagnostics.BundleExcludesAbsoluteSavePath",
    EAutomationTestFlags::ApplicationContextMask |
        EAutomationTestFlags::EngineFilter)

bool FOGDiagnosticsBundleTest::RunTest(const FString& Parameters)
{
    const FString Directory = MakeRecoveryTestDirectory();
    const FString DatabasePath = FPaths::Combine(Directory, TEXT("world.db"));
    const FString DiagnosticsDirectory = FPaths::Combine(Directory, TEXT("diagnostics"));
    IFileManager::Get().MakeDirectory(*Directory, true);

    FOGSQLiteWorldStore Store;
    FString Error;
    TestTrue(TEXT("Open database"), Store.Open(DatabasePath, Error));

    FOGPerformanceTelemetry Telemetry;
    Telemetry.RecordFrame(0.020);

    FString BundlePath;
    TestTrue(TEXT("Write diagnostics bundle"),
        FOGDiagnosticsBundle::Write(
            Store,
            DiagnosticsDirectory,
            Telemetry.Snapshot(),
            BundlePath,
            Error));
    TestTrue(TEXT("Diagnostics file exists"),
        IFileManager::Get().FileExists(*BundlePath));

    FString Bundle;
    TestTrue(TEXT("Read diagnostics file"),
        FFileHelper::LoadFileToString(Bundle, *BundlePath));
    TestTrue(TEXT("Bundle contains schema version"),
        Bundle.Contains(TEXT("\"schema_version\"")));
    TestTrue(TEXT("Bundle contains integrity result"),
        Bundle.Contains(TEXT("\"integrity_ok\"")));
    TestTrue(TEXT("Bundle contains performance telemetry"),
        Bundle.Contains(TEXT("\"performance\"")));
    TestTrue(TEXT("Bundle contains frame budget data"),
        Bundle.Contains(TEXT("\"frames_over_budget\"")));
    TestTrue(TEXT("Bundle contains only clean database filename"),
        Bundle.Contains(TEXT("\"database_file\": \"world.db\"")) ||
        Bundle.Contains(TEXT("\"database_file\":\"world.db\"")));
    TestFalse(TEXT("Bundle excludes absolute database path"),
        Bundle.Contains(*Directory));

    Store.Close();
    IFileManager::Get().DeleteDirectory(*Directory, false, true);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOGMigrationSafeBootstrapPromotionTest,
    "OfflineGame.Persistence.MigrationBootstrap.PromotesValidatedWorkingCopy",
    EAutomationTestFlags::ApplicationContextMask |
        EAutomationTestFlags::EngineFilter)

bool FOGMigrationSafeBootstrapPromotionTest::RunTest(const FString& Parameters)
{
    const FString Directory = MakeRecoveryTestDirectory();
    const FString DatabasePath =
        FPaths::Combine(Directory, TEXT("world.db"));
    IFileManager::Get().MakeDirectory(*Directory, true);

    const FOGEntityId MarkerId = FOGEntityId::NewId();
    FString Error;

    {
        FOGSQLiteWorldStore Store;
        TestTrue(TEXT("Create current-schema database"),
            Store.Open(DatabasePath, Error));
        TestTrue(TEXT("Persist pre-migration marker"),
            Store.UpsertEntity(
                MarkerId,
                TEXT("migration_marker"),
                42,
                TEXT("{\"preserve\":true}"),
                Error));
        Store.Close();
    }

    TestTrue(
        TEXT("Downgrade fixture to schema 6"),
        ExecuteRawDatabaseSql(
            DatabasePath,
            "DROP INDEX IF EXISTS idx_manifestations_owner_identity;"
            "DROP INDEX IF EXISTS idx_manifestations_owner_identity_ordinal;"
            "DROP INDEX IF EXISTS idx_manifestations_anchor;"
            "ALTER TABLE character_manifestations DROP COLUMN build_label;"
            "ALTER TABLE character_manifestations DROP COLUMN lifecycle_state;"
            "ALTER TABLE character_manifestations DROP COLUMN world_mode_anchor_tick;"
            "ALTER TABLE character_manifestations DROP COLUMN world_mode_anchor_territory_id;"
            "ALTER TABLE character_manifestations DROP COLUMN origin_pull_event_id;"
            "ALTER TABLE character_manifestations DROP COLUMN acquisition_ordinal;"
            "ALTER TABLE character_manifestations DROP COLUMN acquisition_world_tick;"
            "DELETE FROM schema_migrations WHERE version = 7;",
            Error));

    FOGWorldBootstrapResult Result;
    TestTrue(
        TEXT("Migration-safe bootstrap succeeds"),
        FOGWorldBootstrap::PrepareWorld(
            DatabasePath,
            Result,
            Error));
    TestTrue(TEXT("Migration was required"),
        Result.bMigrationRequired);
    TestTrue(TEXT("Migration was promoted"),
        Result.bMigrationPerformed);
    TestEqual(TEXT("Source schema recorded"),
        Result.SourceSchemaVersion, 6);
    TestEqual(TEXT("Target schema recorded"),
        Result.TargetSchemaVersion, 7);
    TestTrue(TEXT("Untouched recovery database retained"),
        IFileManager::Get().FileExists(
            *Result.RecoveryDatabasePath));
    TestTrue(TEXT("Migration report retained"),
        IFileManager::Get().FileExists(
            *Result.MigrationReportPath));

    {
        FOGSQLiteWorldStore Store;
        TestTrue(TEXT("Open promoted authoritative database"),
            Store.Open(DatabasePath, Error));
        TestEqual(TEXT("Promoted schema is 7"),
            Store.GetSchemaVersion(Error), 7);

        bool bFound = false;
        FName Kind = NAME_None;
        FString StateJson;
        int64 Revision = -1;
        TestTrue(TEXT("Read pre-migration marker"),
            Store.TryReadEntity(
                MarkerId,
                bFound,
                Kind,
                StateJson,
                Revision,
                Error));
        TestTrue(TEXT("Marker survived migration"),
            bFound);
        TestEqual(TEXT("Marker state survived migration"),
            StateJson,
            FString(TEXT("{\"preserve\":true}")));
    }

    FString ReportJson;
    TestTrue(TEXT("Read migration report"),
        FFileHelper::LoadFileToString(
            ReportJson,
            *Result.MigrationReportPath));
    TestTrue(TEXT("Report records promoted status"),
        ReportJson.Contains(TEXT("\"promoted\"")));

    IFileManager::Get().DeleteDirectory(
        *Directory,
        false,
        true);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOGMigrationSafeBootstrapFailureTest,
    "OfflineGame.Persistence.MigrationBootstrap.FailurePreservesAuthoritativeDatabase",
    EAutomationTestFlags::ApplicationContextMask |
        EAutomationTestFlags::EngineFilter)

bool FOGMigrationSafeBootstrapFailureTest::RunTest(const FString& Parameters)
{
    const FString Directory = MakeRecoveryTestDirectory();
    const FString DatabasePath =
        FPaths::Combine(Directory, TEXT("world.db"));
    IFileManager::Get().MakeDirectory(*Directory, true);

    FString Error;
    {
        FOGSQLiteWorldStore Store;
        TestTrue(TEXT("Create current-schema database"),
            Store.Open(DatabasePath, Error));
        TestTrue(TEXT("Persist failure-test marker"),
            Store.UpsertEntity(
                FOGEntityId::NewId(),
                TEXT("failure_marker"),
                99,
                TEXT("{\"stable\":true}"),
                Error));
        Store.Close();
    }

    // Leave migration-0007 schema effects in place while removing only its
    // ledger row. Reapplying 0007 must fail on the working copy because the
    // acquisition_world_tick column already exists.
    TestTrue(
        TEXT("Create deterministic migration-failure fixture"),
        ExecuteRawDatabaseSql(
            DatabasePath,
            "DELETE FROM schema_migrations WHERE version = 7;",
            Error));

    TArray<uint8> BeforeBytes;
    TestTrue(TEXT("Read authoritative bytes before failed bootstrap"),
        FFileHelper::LoadFileToArray(
            BeforeBytes,
            *DatabasePath));

    FOGWorldBootstrapResult Result;
    TestFalse(
        TEXT("Unsafe migration is refused"),
        FOGWorldBootstrap::PrepareWorld(
            DatabasePath,
            Result,
            Error));
    TestTrue(TEXT("Bootstrap reports migration failure"),
        Error.Contains(TEXT("Working-copy migration failed")));
    TestTrue(TEXT("Pre-migration recovery is retained"),
        IFileManager::Get().FileExists(
            *Result.RecoveryDatabasePath));
    TestTrue(TEXT("Failure report is retained"),
        IFileManager::Get().FileExists(
            *Result.MigrationReportPath));

    TArray<uint8> AfterBytes;
    TestTrue(TEXT("Read authoritative bytes after failed bootstrap"),
        FFileHelper::LoadFileToArray(
            AfterBytes,
            *DatabasePath));
    TestEqual(TEXT("Authoritative byte count is unchanged"),
        AfterBytes.Num(),
        BeforeBytes.Num());

    const bool bBytesUnchanged =
        AfterBytes.Num() == BeforeBytes.Num() &&
        (AfterBytes.Num() == 0 ||
         FMemory::Memcmp(
             AfterBytes.GetData(),
             BeforeBytes.GetData(),
             AfterBytes.Num()) == 0);
    TestTrue(TEXT("Authoritative database remains untouched"),
        bBytesUnchanged);

    FString ReportJson;
    TestTrue(TEXT("Read failed migration report"),
        FFileHelper::LoadFileToString(
            ReportJson,
            *Result.MigrationReportPath));
    TestTrue(TEXT("Report records migration_failed status"),
        ReportJson.Contains(TEXT("migration_failed")));

    IFileManager::Get().DeleteDirectory(
        *Directory,
        false,
        true);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOGManifestation0007LegacyFanOutTest,
    "OfflineGame.Persistence.Migration0007.LegacyDuplicateCounterFansOutDeterministically",
    EAutomationTestFlags::ApplicationContextMask |
        EAutomationTestFlags::EngineFilter)

bool FOGManifestation0007LegacyFanOutTest::RunTest(const FString& Parameters)
{
    const FString Directory = MakeRecoveryTestDirectory();
    const FString DatabasePath =
        FPaths::Combine(Directory, TEXT("legacy_manifestations.db"));
    const FString ReplayPath =
        FPaths::Combine(Directory, TEXT("legacy_manifestations_replay.db"));
    IFileManager::Get().MakeDirectory(*Directory, true);

    const FOGEntityId RulerId = FOGEntityId::NewId();
    const FOGEntityId LegacyManifestationId = FOGEntityId::NewId();
    const FOGEntityId FirstPullEventId = FOGEntityId::NewId();
    const FOGEntityId SecondPullEventId = FOGEntityId::NewId();
    const FOGContentId IdentityId(TEXT("test:character.migration_alpha"));
    const FOGContentId BaseVersionId(TEXT("test:character.migration_alpha.base"));
    const FOGContentId AltVersionId(TEXT("test:character.migration_alpha.alt"));

    FString Error;
    {
        FOGSQLiteWorldStore Store;
        TestTrue(TEXT("Create schema-7 fixture"),
            Store.Open(DatabasePath, Error));
        TestTrue(TEXT("Persist migration-test Ruler"),
            Store.UpsertEntity(
                RulerId,
                TEXT("ruler"),
                0,
                TEXT("{}"),
                Error));

        FOGCharacterManifestationRecord Legacy;
        Legacy.ManifestationId = LegacyManifestationId;
        Legacy.OwningRulerId = RulerId;
        Legacy.IdentityId = IdentityId;
        Legacy.ActiveVersionId = BaseVersionId;
        Legacy.Level = 44;
        Legacy.CurrentRarity = FName(TEXT("SR"));
        Legacy.ProgressionStateJson =
            TEXT("{\"legacy_progress\":44}");
        Legacy.AcquisitionWorldTick = 10;
        Legacy.AcquisitionOrdinal = 0;
        Legacy.LifecycleState = FName(TEXT("active"));

        TestTrue(TEXT("Persist legacy Manifestation"),
            Store.UpsertCharacterManifestation(
                Legacy,
                10,
                Error));

        FOGWorldEvent FirstPull;
        FirstPull.EventId = FirstPullEventId;
        FirstPull.EventType = FName(TEXT("gacha_pull"));
        FirstPull.WorldTick = 10;
        FirstPull.PrimaryEntity = RulerId;
        FirstPull.RelatedEntities.Add(LegacyManifestationId);
        FirstPull.PayloadJson =
            TEXT("{\"banner\":\"test:banner\",\"identity\":\"test:character.migration_alpha\",")
            TEXT("\"version\":\"test:character.migration_alpha.base\",\"rarity\":\"SR\",")
            TEXT("\"featured\":true,\"duplicate\":false,\"seed\":1,\"rng_draws\":1}");
        TestTrue(TEXT("Persist original acquisition event"),
            Store.AppendWorldEvent(FirstPull, Error));

        FOGWorldEvent SecondPull;
        SecondPull.EventId = SecondPullEventId;
        SecondPull.EventType = FName(TEXT("gacha_pull"));
        SecondPull.WorldTick = 20;
        SecondPull.PrimaryEntity = RulerId;
        SecondPull.RelatedEntities.Add(LegacyManifestationId);
        SecondPull.PayloadJson =
            TEXT("{\"banner\":\"test:banner\",\"identity\":\"test:character.migration_alpha\",")
            TEXT("\"version\":\"test:character.migration_alpha.alt\",\"rarity\":\"SSR\",")
            TEXT("\"featured\":false,\"duplicate\":true,\"seed\":2,\"rng_draws\":1}");
        TestTrue(TEXT("Persist repeat acquisition event"),
            Store.AppendWorldEvent(SecondPull, Error));

        Store.Close();
    }

    TestTrue(
        TEXT("Convert fixture to legacy schema 6 with duplicate counter"),
        ExecuteRawDatabaseSql(
            DatabasePath,
            "UPDATE character_manifestations SET duplicate_acquisition_count = 1;"
            "DROP INDEX IF EXISTS idx_manifestations_owner_identity;"
            "DROP INDEX IF EXISTS idx_manifestations_owner_identity_ordinal;"
            "DROP INDEX IF EXISTS idx_manifestations_anchor;"
            "ALTER TABLE character_manifestations DROP COLUMN build_label;"
            "ALTER TABLE character_manifestations DROP COLUMN lifecycle_state;"
            "ALTER TABLE character_manifestations DROP COLUMN world_mode_anchor_tick;"
            "ALTER TABLE character_manifestations DROP COLUMN world_mode_anchor_territory_id;"
            "ALTER TABLE character_manifestations DROP COLUMN origin_pull_event_id;"
            "ALTER TABLE character_manifestations DROP COLUMN acquisition_ordinal;"
            "ALTER TABLE character_manifestations DROP COLUMN acquisition_world_tick;"
            "DELETE FROM schema_migrations WHERE version = 7;",
            Error));

    TestEqual(
        TEXT("Clone schema-6 source for deterministic replay"),
        IFileManager::Get().Copy(
            *ReplayPath,
            *DatabasePath,
            true,
            true),
        COPY_OK);

    FOGWorldBootstrapResult Migration;
    TestTrue(TEXT("Migrate legacy source through safe bootstrap"),
        FOGWorldBootstrap::PrepareWorld(
            DatabasePath,
            Migration,
            Error));

    FOGEntityId MigratedRepeatId;
    {
        FOGSQLiteWorldStore Store;
        TestTrue(TEXT("Open migrated source"),
            Store.Open(DatabasePath, Error));

        TArray<FOGCharacterManifestationRecord> Manifestations;
        TestTrue(TEXT("List migrated Manifestations"),
            Store.ListCharacterManifestationsByOwnerAndIdentity(
                RulerId,
                IdentityId,
                Manifestations,
                Error));
        TestEqual(TEXT("Legacy counter becomes two full Manifestations"),
            Manifestations.Num(), 2);

        if (Manifestations.Num() == 2)
        {
            const FOGCharacterManifestationRecord& Original =
                Manifestations[0];
            const FOGCharacterManifestationRecord& Repeat =
                Manifestations[1];

            TestTrue(TEXT("Original Manifestation ID is preserved"),
                Original.ManifestationId == LegacyManifestationId);
            TestEqual(TEXT("Original progression is preserved"),
                Original.Level, 44);
            TestEqual(TEXT("Original acquisition ordinal is zero"),
                Original.AcquisitionOrdinal, 0);
            TestTrue(TEXT("Original pull provenance is recovered"),
                Original.OriginPullEventId == FirstPullEventId);

            TestTrue(TEXT("Repeat is a distinct persistent Manifestation"),
                Repeat.ManifestationId != LegacyManifestationId);
            TestEqual(TEXT("Repeat acquisition ordinal is one"),
                Repeat.AcquisitionOrdinal, 1);
            TestTrue(TEXT("Repeat pull provenance is recovered"),
                Repeat.OriginPullEventId == SecondPullEventId);
            TestEqual(TEXT("Repeat Version comes from historical event"),
                Repeat.ActiveVersionId, AltVersionId);
            TestEqual(TEXT("Repeat Rarity comes from historical event"),
                Repeat.CurrentRarity, FName(TEXT("SSR")));

            MigratedRepeatId = Repeat.ManifestationId;
        }

        Store.Close();
    }

    FOGWorldBootstrapResult ReplayMigration;
    TestTrue(TEXT("Migrate identical schema-6 source again"),
        FOGWorldBootstrap::PrepareWorld(
            ReplayPath,
            ReplayMigration,
            Error));

    {
        FOGSQLiteWorldStore Store;
        TestTrue(TEXT("Open deterministic replay migration"),
            Store.Open(ReplayPath, Error));

        TArray<FOGCharacterManifestationRecord> Manifestations;
        TestTrue(TEXT("List replay Manifestations"),
            Store.ListCharacterManifestationsByOwnerAndIdentity(
                RulerId,
                IdentityId,
                Manifestations,
                Error));
        TestEqual(TEXT("Replay also has two Manifestations"),
            Manifestations.Num(), 2);

        if (Manifestations.Num() == 2)
        {
            TestTrue(TEXT("Reconstructed Manifestation ID is deterministic"),
                Manifestations[1].ManifestationId ==
                    MigratedRepeatId);
        }

        Store.Close();
    }

    IFileManager::Get().DeleteDirectory(
        *Directory,
        false,
        true);
    return true;
}

#endif
