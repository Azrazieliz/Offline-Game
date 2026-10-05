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
        TEXT("Downgrade fixture to schema 8"),
        ExecuteRawDatabaseSql(
            DatabasePath,
            "DROP TABLE IF EXISTS heroic_records;"
            "DROP TABLE IF EXISTS character_adult_runtime_state;"
            "DROP TABLE IF EXISTS npc_promotion_state;"
            "DROP TABLE IF EXISTS semantic_memories;"
            "DROP TABLE IF EXISTS entity_languages;"
            "DROP TABLE IF EXISTS owned_presentation_unlocks;"
            "DROP TABLE IF EXISTS manifestation_presentation_state;"
            "DROP TABLE IF EXISTS entity_equipment_proficiency;"
            "DROP TABLE IF EXISTS item_owner_affinity;"
            "DROP TABLE IF EXISTS container_contents;"
            "DROP TABLE IF EXISTS inventory_containers;"
            "DROP TABLE IF EXISTS equipment_bindings;"
            "DROP TABLE IF EXISTS item_modifiers;"
            "DROP TABLE IF EXISTS item_instances;"
            "ALTER TABLE knowledge_facts DROP COLUMN language_context_content_id;"
            "ALTER TABLE knowledge_facts DROP COLUMN evidence_world_tick;"
            "ALTER TABLE knowledge_facts DROP COLUMN source_event_id;"
            "ALTER TABLE knowledge_facts DROP COLUMN source_entity_id;"
            "ALTER TABLE knowledge_facts DROP COLUMN confidence_bps;"
            "ALTER TABLE knowledge_facts DROP COLUMN belief_state;"
            "DELETE FROM schema_migrations WHERE version = 13;"
            "DROP TABLE IF EXISTS logistics_routes;"
            "DROP TABLE IF EXISTS civilization_dimensions;"
            "DROP TABLE IF EXISTS civilization_state;"
            "DROP TABLE IF EXISTS project_assignments;"
            "DROP TABLE IF EXISTS project_phases;"
            "DROP TABLE IF EXISTS army_capabilities;"
            "DROP TABLE IF EXISTS war_participant_history;"
            "DROP TABLE IF EXISTS war_orders;"
            "DROP TABLE IF EXISTS war_objectives;"
            "DROP TABLE IF EXISTS war_fronts;"
            "DROP TABLE IF EXISTS dispatch_constraints;"
            "DROP TABLE IF EXISTS dispatch_objectives;"
            "ALTER TABLE dispatches DROP COLUMN delay_until_world_tick;"
            "ALTER TABLE dispatches DROP COLUMN outcome_state;"
            "ALTER TABLE dispatches DROP COLUMN abort_policy_json;"
            "ALTER TABLE dispatches DROP COLUMN risk_tolerance_bps;"
            "DELETE FROM schema_migrations WHERE version = 12;"
            "DROP TABLE IF EXISTS offline_simulation_state;"
            "DROP TABLE IF EXISTS content_unlock_state;"
            "DROP TABLE IF EXISTS world_director_schedule;"
            "DROP TABLE IF EXISTS junctions;"
            "DROP TABLE IF EXISTS reality_nodes;"
            "DROP TABLE IF EXISTS time_domains;"
            "DELETE FROM schema_migrations WHERE version = 11;"
            "DROP TABLE IF EXISTS character_convergence_sources;"
            "DROP TABLE IF EXISTS character_convergences;"
            "DROP TABLE IF EXISTS protagonist_world_manifestation_state;"
            "DROP TABLE IF EXISTS manifestation_world_fantasm_state;"
            "DROP TABLE IF EXISTS entity_transcendence_state;"
            "DROP TABLE IF EXISTS manifestation_reinforcement;"
            "DROP TABLE IF EXISTS manifestation_forms;"
            "DROP TABLE IF EXISTS manifestation_route_nodes;"
            "DROP TABLE IF EXISTS skill_provenance;"
            "DROP TABLE IF EXISTS entity_skills;"
            "DROP TABLE IF EXISTS grand_class_seats;"
            "DROP TABLE IF EXISTS entity_classes;"
            "DROP TABLE IF EXISTS factor_lineage;"
            "DROP TABLE IF EXISTS factor_instances;"
            "DROP TABLE IF EXISTS entity_rank_state;"
            "DELETE FROM schema_migrations WHERE version = 10;"
            "DROP TABLE IF EXISTS domain_core_lineage;"
            "DROP TABLE IF EXISTS domain_core_fusions;"
            "DROP TABLE IF EXISTS domain_core_concepts;"
            "DROP TABLE IF EXISTS territory_domain_state;"
            "DELETE FROM schema_migrations WHERE version = 9;",
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
        Result.SourceSchemaVersion, 8);
    TestEqual(TEXT("Target schema recorded"),
        Result.TargetSchemaVersion, 13);
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
        TestEqual(TEXT("Promoted schema is 13"),
            Store.GetSchemaVersion(Error), 13);

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

    // Break a migration-0013 table while removing only its ledger row.
    // CREATE TABLE IF NOT EXISTS cannot repair the missing column, so the
    // working-copy validation must fail while the authoritative DB is untouched.
    TestTrue(
        TEXT("Create deterministic migration-failure fixture"),
        ExecuteRawDatabaseSql(
            DatabasePath,
            "ALTER TABLE entity_languages DROP COLUMN written_proficiency_bps;"
            "DELETE FROM schema_migrations WHERE version = 13;",
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
        TestTrue(TEXT("Create current-schema fixture"),
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
            "DROP TABLE IF EXISTS heroic_records;"
            "DROP TABLE IF EXISTS character_adult_runtime_state;"
            "DROP TABLE IF EXISTS npc_promotion_state;"
            "DROP TABLE IF EXISTS semantic_memories;"
            "DROP TABLE IF EXISTS entity_languages;"
            "DROP TABLE IF EXISTS owned_presentation_unlocks;"
            "DROP TABLE IF EXISTS manifestation_presentation_state;"
            "DROP TABLE IF EXISTS entity_equipment_proficiency;"
            "DROP TABLE IF EXISTS item_owner_affinity;"
            "DROP TABLE IF EXISTS container_contents;"
            "DROP TABLE IF EXISTS inventory_containers;"
            "DROP TABLE IF EXISTS equipment_bindings;"
            "DROP TABLE IF EXISTS item_modifiers;"
            "DROP TABLE IF EXISTS item_instances;"
            "ALTER TABLE knowledge_facts DROP COLUMN language_context_content_id;"
            "ALTER TABLE knowledge_facts DROP COLUMN evidence_world_tick;"
            "ALTER TABLE knowledge_facts DROP COLUMN source_event_id;"
            "ALTER TABLE knowledge_facts DROP COLUMN source_entity_id;"
            "ALTER TABLE knowledge_facts DROP COLUMN confidence_bps;"
            "ALTER TABLE knowledge_facts DROP COLUMN belief_state;"
            "DELETE FROM schema_migrations WHERE version = 13;"
            "DROP TABLE IF EXISTS logistics_routes;"
            "DROP TABLE IF EXISTS civilization_dimensions;"
            "DROP TABLE IF EXISTS civilization_state;"
            "DROP TABLE IF EXISTS project_assignments;"
            "DROP TABLE IF EXISTS project_phases;"
            "DROP TABLE IF EXISTS army_capabilities;"
            "DROP TABLE IF EXISTS war_participant_history;"
            "DROP TABLE IF EXISTS war_orders;"
            "DROP TABLE IF EXISTS war_objectives;"
            "DROP TABLE IF EXISTS war_fronts;"
            "DROP TABLE IF EXISTS dispatch_constraints;"
            "DROP TABLE IF EXISTS dispatch_objectives;"
            "ALTER TABLE dispatches DROP COLUMN delay_until_world_tick;"
            "ALTER TABLE dispatches DROP COLUMN outcome_state;"
            "ALTER TABLE dispatches DROP COLUMN abort_policy_json;"
            "ALTER TABLE dispatches DROP COLUMN risk_tolerance_bps;"
            "DELETE FROM schema_migrations WHERE version = 12;"
            "DROP TABLE IF EXISTS offline_simulation_state;"
            "DROP TABLE IF EXISTS content_unlock_state;"
            "DROP TABLE IF EXISTS world_director_schedule;"
            "DROP TABLE IF EXISTS junctions;"
            "DROP TABLE IF EXISTS reality_nodes;"
            "DROP TABLE IF EXISTS time_domains;"
            "DELETE FROM schema_migrations WHERE version = 11;"
            "DROP TABLE IF EXISTS character_convergence_sources;"
            "DROP TABLE IF EXISTS character_convergences;"
            "DROP TABLE IF EXISTS protagonist_world_manifestation_state;"
            "DROP TABLE IF EXISTS manifestation_world_fantasm_state;"
            "DROP TABLE IF EXISTS entity_transcendence_state;"
            "DROP TABLE IF EXISTS manifestation_reinforcement;"
            "DROP TABLE IF EXISTS manifestation_forms;"
            "DROP TABLE IF EXISTS manifestation_route_nodes;"
            "DROP TABLE IF EXISTS skill_provenance;"
            "DROP TABLE IF EXISTS entity_skills;"
            "DROP TABLE IF EXISTS grand_class_seats;"
            "DROP TABLE IF EXISTS entity_classes;"
            "DROP TABLE IF EXISTS factor_lineage;"
            "DROP TABLE IF EXISTS factor_instances;"
            "DROP TABLE IF EXISTS entity_rank_state;"
            "DELETE FROM schema_migrations WHERE version = 10;"
            "DROP TABLE IF EXISTS domain_core_lineage;"
            "DROP TABLE IF EXISTS domain_core_fusions;"
            "DROP TABLE IF EXISTS domain_core_concepts;"
            "DROP TABLE IF EXISTS territory_domain_state;"
            "DELETE FROM schema_migrations WHERE version = 9;"
            "DROP TABLE IF EXISTS ruler_gacha_access;"
            "DROP TABLE IF EXISTS ruler_sovereignty_state;"
            "DROP TABLE IF EXISTS territory_claims;"
            "DROP TABLE IF EXISTS location_territories;"
            "DELETE FROM schema_migrations WHERE version = 8;"
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

    TestTrue(
        TEXT("Clone schema-6 source for deterministic replay"),
        IFileManager::Get().Copy(
            *ReplayPath,
            *DatabasePath,
            true,
            true) == COPY_OK);

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOGTerritory0008LegacyProjectionMigrationTest,
    "OfflineGame.Persistence.Migration0008.LegacyTerritoryAndGachaStateArePreserved",
    EAutomationTestFlags::ApplicationContextMask |
        EAutomationTestFlags::EngineFilter)

bool FOGTerritory0008LegacyProjectionMigrationTest::RunTest(
    const FString& Parameters)
{
    const FString Directory =
        MakeRecoveryTestDirectory();
    const FString DatabasePath =
        FPaths::Combine(
            Directory,
            TEXT("legacy_sovereignty.db"));
    IFileManager::Get().MakeDirectory(
        *Directory,
        true);

    const FOGEntityId RulerId =
        FOGEntityId::NewId();
    const FOGEntityId LocationId =
        FOGEntityId::NewId();
    const FOGEntityId TerritoryId =
        FOGEntityId::NewId();
    const FOGEntityId PullEventId =
        FOGEntityId::NewId();

    FString Error;
    {
        FOGSQLiteWorldStore Store;
        TestTrue(
            TEXT("Create current-schema migration fixture"),
            Store.Open(
                DatabasePath,
                Error));
        TestEqual(
            TEXT("Fixture begins at schema 12"),
            Store.GetSchemaVersion(Error),
            13);

        TestTrue(
            TEXT("Persist legacy Ruler"),
            Store.UpsertEntity(
                RulerId,
                TEXT("ruler"),
                0,
                TEXT("{}"),
                Error));

        FOGLocationRecord Location;
        Location.LocationId =
            LocationId;
        Location.Kind =
            FName(TEXT("region"));
        TestTrue(
            TEXT("Persist legacy root Location"),
            Store.UpsertLocation(
                Location,
                0,
                Error));

        FOGTerritoryRecord Territory;
        Territory.TerritoryId =
            TerritoryId;
        Territory.RulerId =
            RulerId;
        Territory.RootLocationId =
            LocationId;
        Territory.bMainTerritory =
            true;
        Territory.Population = 10;
        Territory.ControlState =
            FName(TEXT("controlled"));
        TestTrue(
            TEXT("Persist legacy Territory projection"),
            Store.UpsertTerritory(
                Territory,
                10,
                Error));

        FOGGachaStateRecord GachaState;
        GachaState.RulerId =
            RulerId;
        GachaState.PityCategory =
            FName(TEXT("legacy"));
        GachaState.TotalPulls = 1;
        GachaState.UpdatedWorldTick = 20;
        TestTrue(
            TEXT("Persist pre-qualification gacha state"),
            Store.UpsertGachaState(
                GachaState,
                Error));

        FOGWorldEvent PullEvent;
        PullEvent.EventId =
            PullEventId;
        PullEvent.EventType =
            FName(TEXT("gacha_pull"));
        PullEvent.WorldTick = 20;
        PullEvent.PrimaryEntity =
            RulerId;
        PullEvent.PayloadJson =
            TEXT("{\"identity\":\"test:legacy\",\"version\":\"test:legacy.base\",\"rarity\":\"R\"}");
        PullEvent.bChronicleEligible = false;
        TestTrue(
            TEXT("Persist historical pre-qualification pull"),
            Store.AppendWorldEvent(
                PullEvent,
                Error));

        Store.Close();
    }

    TestTrue(
        TEXT("Convert fixture to valid schema 7"),
        ExecuteRawDatabaseSql(
            DatabasePath,
            "DROP TABLE IF EXISTS heroic_records;"
            "DROP TABLE IF EXISTS character_adult_runtime_state;"
            "DROP TABLE IF EXISTS npc_promotion_state;"
            "DROP TABLE IF EXISTS semantic_memories;"
            "DROP TABLE IF EXISTS entity_languages;"
            "DROP TABLE IF EXISTS owned_presentation_unlocks;"
            "DROP TABLE IF EXISTS manifestation_presentation_state;"
            "DROP TABLE IF EXISTS entity_equipment_proficiency;"
            "DROP TABLE IF EXISTS item_owner_affinity;"
            "DROP TABLE IF EXISTS container_contents;"
            "DROP TABLE IF EXISTS inventory_containers;"
            "DROP TABLE IF EXISTS equipment_bindings;"
            "DROP TABLE IF EXISTS item_modifiers;"
            "DROP TABLE IF EXISTS item_instances;"
            "ALTER TABLE knowledge_facts DROP COLUMN language_context_content_id;"
            "ALTER TABLE knowledge_facts DROP COLUMN evidence_world_tick;"
            "ALTER TABLE knowledge_facts DROP COLUMN source_event_id;"
            "ALTER TABLE knowledge_facts DROP COLUMN source_entity_id;"
            "ALTER TABLE knowledge_facts DROP COLUMN confidence_bps;"
            "ALTER TABLE knowledge_facts DROP COLUMN belief_state;"
            "DELETE FROM schema_migrations WHERE version = 13;"
            "DROP TABLE IF EXISTS logistics_routes;"
            "DROP TABLE IF EXISTS civilization_dimensions;"
            "DROP TABLE IF EXISTS civilization_state;"
            "DROP TABLE IF EXISTS project_assignments;"
            "DROP TABLE IF EXISTS project_phases;"
            "DROP TABLE IF EXISTS army_capabilities;"
            "DROP TABLE IF EXISTS war_participant_history;"
            "DROP TABLE IF EXISTS war_orders;"
            "DROP TABLE IF EXISTS war_objectives;"
            "DROP TABLE IF EXISTS war_fronts;"
            "DROP TABLE IF EXISTS dispatch_constraints;"
            "DROP TABLE IF EXISTS dispatch_objectives;"
            "ALTER TABLE dispatches DROP COLUMN delay_until_world_tick;"
            "ALTER TABLE dispatches DROP COLUMN outcome_state;"
            "ALTER TABLE dispatches DROP COLUMN abort_policy_json;"
            "ALTER TABLE dispatches DROP COLUMN risk_tolerance_bps;"
            "DELETE FROM schema_migrations WHERE version = 12;"
            "DROP TABLE IF EXISTS offline_simulation_state;"
            "DROP TABLE IF EXISTS content_unlock_state;"
            "DROP TABLE IF EXISTS world_director_schedule;"
            "DROP TABLE IF EXISTS junctions;"
            "DROP TABLE IF EXISTS reality_nodes;"
            "DROP TABLE IF EXISTS time_domains;"
            "DELETE FROM schema_migrations WHERE version = 11;"
            "DROP TABLE IF EXISTS character_convergence_sources;"
            "DROP TABLE IF EXISTS character_convergences;"
            "DROP TABLE IF EXISTS protagonist_world_manifestation_state;"
            "DROP TABLE IF EXISTS manifestation_world_fantasm_state;"
            "DROP TABLE IF EXISTS entity_transcendence_state;"
            "DROP TABLE IF EXISTS manifestation_reinforcement;"
            "DROP TABLE IF EXISTS manifestation_forms;"
            "DROP TABLE IF EXISTS manifestation_route_nodes;"
            "DROP TABLE IF EXISTS skill_provenance;"
            "DROP TABLE IF EXISTS entity_skills;"
            "DROP TABLE IF EXISTS grand_class_seats;"
            "DROP TABLE IF EXISTS entity_classes;"
            "DROP TABLE IF EXISTS factor_lineage;"
            "DROP TABLE IF EXISTS factor_instances;"
            "DROP TABLE IF EXISTS entity_rank_state;"
            "DELETE FROM schema_migrations WHERE version = 10;"
            "DROP TABLE IF EXISTS domain_core_lineage;"
            "DROP TABLE IF EXISTS domain_core_fusions;"
            "DROP TABLE IF EXISTS domain_core_concepts;"
            "DROP TABLE IF EXISTS territory_domain_state;"
            "DELETE FROM schema_migrations WHERE version = 9;"
            "DROP TABLE IF EXISTS ruler_gacha_access;"
            "DROP TABLE IF EXISTS ruler_sovereignty_state;"
            "DROP TABLE IF EXISTS territory_claims;"
            "DROP TABLE IF EXISTS location_territories;"
            "DELETE FROM schema_migrations WHERE version = 8;",
            Error));

    FOGWorldBootstrapResult Migration;
    TestTrue(
        TEXT("Safe bootstrap migrates schema 7 through current schema"),
        FOGWorldBootstrap::PrepareWorld(
            DatabasePath,
            Migration,
            Error));
    TestEqual(
        TEXT("Migration source schema"),
        Migration.SourceSchemaVersion,
        7);
    TestEqual(
        TEXT("Migration target schema"),
        Migration.TargetSchemaVersion,
        13);

    {
        FOGSQLiteWorldStore Store;
        TestTrue(
            TEXT("Open migrated current-schema database"),
            Store.Open(
                DatabasePath,
                Error));

        TArray<FOGTerritoryClaimRecord> Claims;
        TestTrue(
            TEXT("Legacy Territory owner becomes normalized claim"),
            Store.ListTerritoryClaimsByTerritory(
                TerritoryId,
                Claims,
                Error));
        TestEqual(
            TEXT("Exactly one legacy ownership claim is reconstructed"),
            Claims.Num(),
            1);
        if (Claims.Num() == 1)
        {
            TestTrue(
                TEXT("Migrated claim preserves Ruler"),
                Claims[0].RulerId ==
                    RulerId);
            TestEqual(
                TEXT("Migrated claim is effectively controlled"),
                Claims[0].ControlState,
                FName(TEXT("controlled")));
            TestTrue(
                TEXT("Migrated claim has effective-control origin"),
                Claims[0].bHasEffectiveControlStart);
            TestEqual(
                TEXT("Control origin follows legacy Territory creation"),
                Claims[0].EffectiveControlStartWorldTick,
                static_cast<int64>(10));
        }

        TArray<FOGTerritoryClaimRecord> LocationClaims;
        TestTrue(
            TEXT("Root Location is normalized into many-to-many membership"),
            Store.ListActiveClaimsForLocation(
                LocationId,
                LocationClaims,
                Error));
        TestEqual(
            TEXT("Normalized root Location resolves the migrated claim"),
            LocationClaims.Num(),
            1);

        bool bFound = false;
        FOGRulerSovereigntyStateRecord Sovereignty;
        TestTrue(
            TEXT("Read migrated sovereignty"),
            Store.TryReadRulerSovereigntyState(
                RulerId,
                bFound,
                Sovereignty,
                Error));
        TestTrue(
            TEXT("Legacy Territory owner has sovereignty state"),
            bFound);
        TestEqual(
            TEXT("Legacy controlled Territory maps to Ruler title"),
            Sovereignty.CurrentTitle,
            FName(TEXT("ruler")));

        FOGRulerGachaAccessRecord Access;
        bFound = false;
        TestTrue(
            TEXT("Read migrated gacha access"),
            Store.TryReadRulerGachaAccess(
                RulerId,
                bFound,
                Access,
                Error));
        TestTrue(
            TEXT("Legacy gacha user has access record"),
            bFound);
        TestTrue(
            TEXT("Migration never retroactively revokes already-used gacha"),
            Access.bPermanentlyUnlocked);
        TestTrue(
            TEXT("Legacy permanent unlock retains an audit tick"),
            Access.bHasUnlockedWorldTick);
        TestEqual(
            TEXT("Earliest historical pull becomes unlock provenance"),
            Access.UnlockedWorldTick,
            static_cast<int64>(20));

        Store.Close();
    }

    IFileManager::Get().DeleteDirectory(
        *Directory,
        false,
        true);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOGDomainHeart0009LegacyProjectionMigrationTest,
    "OfflineGame.Persistence.Migration0009.LegacyCoreAspectsAndHeartStateArePreserved",
    EAutomationTestFlags::ApplicationContextMask |
        EAutomationTestFlags::EngineFilter)

bool FOGDomainHeart0009LegacyProjectionMigrationTest::RunTest(
    const FString& Parameters)
{
    const FString Directory =
        MakeRecoveryTestDirectory();
    const FString DatabasePath =
        FPaths::Combine(
            Directory,
            TEXT("legacy_domain_heart.db"));
    IFileManager::Get().MakeDirectory(
        *Directory,
        true);

    const FOGEntityId RulerId =
        FOGEntityId::NewId();
    const FOGEntityId FunctionalLocationId =
        FOGEntityId::NewId();
    const FOGEntityId BrokenLocationId =
        FOGEntityId::NewId();
    const FOGEntityId FunctionalTerritoryId =
        FOGEntityId::NewId();
    const FOGEntityId BrokenTerritoryId =
        FOGEntityId::NewId();
    const FOGEntityId FunctionalCoreId =
        FOGEntityId::NewId();
    const FOGEntityId BrokenCoreId =
        FOGEntityId::NewId();

    FString Error;
    {
        FOGSQLiteWorldStore Store;
        TestTrue(
            TEXT("Create current-schema Core migration fixture"),
            Store.Open(
                DatabasePath,
                Error));
        TestEqual(
            TEXT("Fixture begins at schema 12"),
            Store.GetSchemaVersion(
                Error),
            13);

        TestTrue(
            TEXT("Persist Ruler"),
            Store.UpsertEntity(
                RulerId,
                TEXT("ruler"),
                0,
                TEXT("{}"),
                Error));

        auto PersistTerritory =
            [&Store, &RulerId, &Error](
                const FOGEntityId& LocationId,
                const FOGEntityId& TerritoryId)
            {
                FOGLocationRecord Location;
                Location.LocationId =
                    LocationId;
                Location.Kind =
                    FName(TEXT("region"));
                if (!Store.UpsertLocation(
                        Location,
                        0,
                        Error))
                {
                    return false;
                }

                FOGTerritoryRecord Territory;
                Territory.TerritoryId =
                    TerritoryId;
                Territory.RulerId =
                    RulerId;
                Territory.RootLocationId =
                    LocationId;
                Territory.Population =
                    10;
                Territory.ControlState =
                    FName(TEXT("controlled"));
                return Store.UpsertTerritory(
                    Territory,
                    0,
                    Error);
            };

        TestTrue(
            TEXT("Persist functional Territory"),
            PersistTerritory(
                FunctionalLocationId,
                FunctionalTerritoryId));
        TestTrue(
            TEXT("Persist broken-heart Territory"),
            PersistTerritory(
                BrokenLocationId,
                BrokenTerritoryId));

        FOGDomainCoreRecord FunctionalCore;
        FunctionalCore.CoreId =
            FunctionalCoreId;
        FunctionalCore.TerritoryId =
            FunctionalTerritoryId;
        FunctionalCore.ControllerRulerId =
            RulerId;
        FunctionalCore.Lifecycle =
            EOGDomainCoreLifecycle::Awakened;
        FunctionalCore.CurrentDurability =
            FOGLargeNumber::FromInt64(
                80);
        FunctionalCore.MaxDurability =
            FOGLargeNumber::FromInt64(
                100);
        FOGDomainCoreAspect TimeAspect;
        TimeAspect.AspectId =
            FOGContentId(
                TEXT("test:concept.time"));
        TimeAspect.Grade = 2;
        FunctionalCore.Aspects.Add(
            TimeAspect);

        TestTrue(
            TEXT("Persist legacy intact Core"),
            Store.UpsertDomainCore(
                FunctionalCore,
                10,
                Error));

        FOGDomainCoreRecord BrokenCore;
        BrokenCore.CoreId =
            BrokenCoreId;
        BrokenCore.TerritoryId =
            BrokenTerritoryId;
        BrokenCore.ControllerRulerId =
            FOGEntityId();
        BrokenCore.Lifecycle =
            EOGDomainCoreLifecycle::Broken;
        BrokenCore.CurrentDurability =
            FOGLargeNumber();
        BrokenCore.MaxDurability =
            FOGLargeNumber::FromInt64(
                100);
        FOGDomainCoreAspect DeathAspect;
        DeathAspect.AspectId =
            FOGContentId(
                TEXT("test:concept.death"));
        DeathAspect.Grade = 3;
        BrokenCore.Aspects.Add(
            DeathAspect);

        TestTrue(
            TEXT("Persist legacy Broken Core"),
            Store.UpsertDomainCore(
                BrokenCore,
                10,
                Error));

        FOGWorldEvent BrokenEvent;
        BrokenEvent.EventId =
            FOGEntityId::NewId();
        BrokenEvent.EventType =
            FName(TEXT("domain_core.broken"));
        BrokenEvent.WorldTick = 40;
        BrokenEvent.PrimaryEntity =
            BrokenCoreId;
        BrokenEvent.RelatedEntities.Add(
            BrokenTerritoryId);
        BrokenEvent.bChronicleEligible =
            true;
        TestTrue(
            TEXT("Persist historical Core-break event"),
            Store.AppendWorldEvent(
                BrokenEvent,
                Error));

        Store.Close();
    }

    TestTrue(
        TEXT("Convert fixture to valid schema 8"),
        ExecuteRawDatabaseSql(
            DatabasePath,
            "DROP TABLE IF EXISTS heroic_records;"
            "DROP TABLE IF EXISTS character_adult_runtime_state;"
            "DROP TABLE IF EXISTS npc_promotion_state;"
            "DROP TABLE IF EXISTS semantic_memories;"
            "DROP TABLE IF EXISTS entity_languages;"
            "DROP TABLE IF EXISTS owned_presentation_unlocks;"
            "DROP TABLE IF EXISTS manifestation_presentation_state;"
            "DROP TABLE IF EXISTS entity_equipment_proficiency;"
            "DROP TABLE IF EXISTS item_owner_affinity;"
            "DROP TABLE IF EXISTS container_contents;"
            "DROP TABLE IF EXISTS inventory_containers;"
            "DROP TABLE IF EXISTS equipment_bindings;"
            "DROP TABLE IF EXISTS item_modifiers;"
            "DROP TABLE IF EXISTS item_instances;"
            "ALTER TABLE knowledge_facts DROP COLUMN language_context_content_id;"
            "ALTER TABLE knowledge_facts DROP COLUMN evidence_world_tick;"
            "ALTER TABLE knowledge_facts DROP COLUMN source_event_id;"
            "ALTER TABLE knowledge_facts DROP COLUMN source_entity_id;"
            "ALTER TABLE knowledge_facts DROP COLUMN confidence_bps;"
            "ALTER TABLE knowledge_facts DROP COLUMN belief_state;"
            "DELETE FROM schema_migrations WHERE version = 13;"
            "DROP TABLE IF EXISTS logistics_routes;"
            "DROP TABLE IF EXISTS civilization_dimensions;"
            "DROP TABLE IF EXISTS civilization_state;"
            "DROP TABLE IF EXISTS project_assignments;"
            "DROP TABLE IF EXISTS project_phases;"
            "DROP TABLE IF EXISTS army_capabilities;"
            "DROP TABLE IF EXISTS war_participant_history;"
            "DROP TABLE IF EXISTS war_orders;"
            "DROP TABLE IF EXISTS war_objectives;"
            "DROP TABLE IF EXISTS war_fronts;"
            "DROP TABLE IF EXISTS dispatch_constraints;"
            "DROP TABLE IF EXISTS dispatch_objectives;"
            "ALTER TABLE dispatches DROP COLUMN delay_until_world_tick;"
            "ALTER TABLE dispatches DROP COLUMN outcome_state;"
            "ALTER TABLE dispatches DROP COLUMN abort_policy_json;"
            "ALTER TABLE dispatches DROP COLUMN risk_tolerance_bps;"
            "DELETE FROM schema_migrations WHERE version = 12;"
            "DROP TABLE IF EXISTS offline_simulation_state;"
            "DROP TABLE IF EXISTS content_unlock_state;"
            "DROP TABLE IF EXISTS world_director_schedule;"
            "DROP TABLE IF EXISTS junctions;"
            "DROP TABLE IF EXISTS reality_nodes;"
            "DROP TABLE IF EXISTS time_domains;"
            "DELETE FROM schema_migrations WHERE version = 11;"
            "DROP TABLE IF EXISTS character_convergence_sources;"
            "DROP TABLE IF EXISTS character_convergences;"
            "DROP TABLE IF EXISTS protagonist_world_manifestation_state;"
            "DROP TABLE IF EXISTS manifestation_world_fantasm_state;"
            "DROP TABLE IF EXISTS entity_transcendence_state;"
            "DROP TABLE IF EXISTS manifestation_reinforcement;"
            "DROP TABLE IF EXISTS manifestation_forms;"
            "DROP TABLE IF EXISTS manifestation_route_nodes;"
            "DROP TABLE IF EXISTS skill_provenance;"
            "DROP TABLE IF EXISTS entity_skills;"
            "DROP TABLE IF EXISTS grand_class_seats;"
            "DROP TABLE IF EXISTS entity_classes;"
            "DROP TABLE IF EXISTS factor_lineage;"
            "DROP TABLE IF EXISTS factor_instances;"
            "DROP TABLE IF EXISTS entity_rank_state;"
            "DELETE FROM schema_migrations WHERE version = 10;"
            "DROP TABLE IF EXISTS domain_core_lineage;"
            "DROP TABLE IF EXISTS domain_core_fusions;"
            "DROP TABLE IF EXISTS domain_core_concepts;"
            "DROP TABLE IF EXISTS territory_domain_state;"
            "DELETE FROM schema_migrations WHERE version = 9;",
            Error));

    FOGWorldBootstrapResult Migration;
    TestTrue(
        TEXT("Safe bootstrap migrates schema 8 through current schema"),
        FOGWorldBootstrap::PrepareWorld(
            DatabasePath,
            Migration,
            Error));
    TestEqual(
        TEXT("Migration source schema"),
        Migration.SourceSchemaVersion,
        8);
    TestEqual(
        TEXT("Migration target schema"),
        Migration.TargetSchemaVersion,
        13);

    {
        FOGSQLiteWorldStore Store;
        TestTrue(
            TEXT("Open migrated current-schema database"),
            Store.Open(
                DatabasePath,
                Error));

        TArray<FOGDomainCoreConceptRecord> FunctionalConcepts;
        TestTrue(
            TEXT("Read migrated functional Core Concepts"),
            Store.ListDomainCoreConcepts(
                FunctionalCoreId,
                FunctionalConcepts,
                Error));
        TestEqual(
            TEXT("Legacy Aspect becomes one normalized Concept"),
            FunctionalConcepts.Num(),
            1);
        if (FunctionalConcepts.Num() == 1)
        {
            TestTrue(
                TEXT("Concept identity is preserved"),
                FunctionalConcepts[0].ConceptId ==
                    FOGContentId(
                        TEXT("test:concept.time")));
            TestEqual(
                TEXT("Concept grade is preserved"),
                FunctionalConcepts[0].Grade,
                2);
            TestTrue(
                TEXT("Legacy Concept provenance points to source Core"),
                FunctionalConcepts[0].OriginSourceCoreId ==
                    FunctionalCoreId);
        }

        bool bStateFound = false;
        FOGTerritoryDomainStateRecord FunctionalState;
        TestTrue(
            TEXT("Read migrated intact Domain heart"),
            Store.TryReadTerritoryDomainState(
                FunctionalTerritoryId,
                bStateFound,
                FunctionalState,
                Error));
        TestTrue(
            TEXT("Intact awakened Core remains active heart"),
            bStateFound &&
            FunctionalState.ActiveCoreId ==
                FunctionalCoreId);
        TestEqual(
            TEXT("Partially damaged intact Core migrates as damaged Domain"),
            FunctionalState.DomainState,
            FName(TEXT("damaged")));

        FOGTerritoryDomainStateRecord BrokenState;
        bStateFound = false;
        TestTrue(
            TEXT("Read migrated lost Domain heart"),
            Store.TryReadTerritoryDomainState(
                BrokenTerritoryId,
                bStateFound,
                BrokenState,
                Error));
        TestTrue(
            TEXT("Broken Core leaves no active heart"),
            bStateFound &&
            !BrokenState.ActiveCoreId.IsValid());
        TestEqual(
            TEXT("Broken Core starts Domain-heart ruin"),
            BrokenState.DomainState,
            FName(TEXT("heart_lost_ruining")));
        TestTrue(
            TEXT("Historical break tick is recovered"),
            BrokenState.bHasHeartLostWorldTick);
        TestEqual(
            TEXT("Recovered heart-loss tick matches historical event"),
            BrokenState.HeartLostWorldTick,
            static_cast<int64>(40));
        TestTrue(
            TEXT("Ruin-start tick is recovered"),
            BrokenState.bHasRuinStartedWorldTick);
        TestEqual(
            TEXT("Recovered ruin-start tick matches break event"),
            BrokenState.RuinStartedWorldTick,
            static_cast<int64>(40));

        Store.Close();
    }

    IFileManager::Get().DeleteDirectory(
        *Directory,
        false,
        true);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOGProgression0010LegacyProjectionMigrationTest,
    "OfflineGame.Persistence.Migration0010.LegacyProgressionIsPreservedWithoutFabricatedOntology",
    EAutomationTestFlags::ApplicationContextMask |
        EAutomationTestFlags::EngineFilter)

bool FOGProgression0010LegacyProjectionMigrationTest::RunTest(
    const FString& Parameters)
{
    const FString Directory =
        MakeRecoveryTestDirectory();
    const FString DatabasePath =
        FPaths::Combine(
            Directory,
            TEXT("legacy_progression.db"));
    IFileManager::Get().MakeDirectory(
        *Directory,
        true);

    const FOGEntityId RulerId =
        FOGEntityId::NewId();
    const FOGEntityId ManifestationId =
        FOGEntityId::NewId();
    const FString LegacyProgression =
        TEXT("{\"legacy_route\":\"opaque\",\"legacy_score\":77}");

    FString Error;
    {
        FOGSQLiteWorldStore Store;
        TestTrue(
            TEXT("Create current-schema progression fixture"),
            Store.Open(
                DatabasePath,
                Error));
        TestEqual(
            TEXT("Fixture begins at schema 12"),
            Store.GetSchemaVersion(
                Error),
            13);

        TestTrue(
            TEXT("Persist legacy Ruler"),
            Store.UpsertEntity(
                RulerId,
                FName(TEXT("ruler")),
                0,
                TEXT("{}"),
                Error));

        FOGCharacterManifestationRecord Manifestation;
        Manifestation.ManifestationId =
            ManifestationId;
        Manifestation.OwningRulerId =
            RulerId;
        Manifestation.IdentityId =
            FOGContentId(
                TEXT("test:character.legacy_progression"));
        Manifestation.ActiveVersionId =
            FOGContentId(
                TEXT("test:character.legacy_progression.base"));
        Manifestation.Level = 77;
        Manifestation.CurrentRarity =
            FName(TEXT("SSR"));
        Manifestation.AcquisitionWorldTick = 15;
        Manifestation.AcquisitionOrdinal = 0;
        Manifestation.LifecycleState =
            FName(TEXT("active"));
        Manifestation.ProgressionStateJson =
            LegacyProgression;

        TestTrue(
            TEXT("Persist opaque legacy Manifestation progression"),
            Store.UpsertCharacterManifestation(
                Manifestation,
                15,
                Error));
        Store.Close();
    }

    TestTrue(
        TEXT("Convert fixture to valid schema 9"),
        ExecuteRawDatabaseSql(
            DatabasePath,
            "DROP TABLE IF EXISTS heroic_records;"
            "DROP TABLE IF EXISTS character_adult_runtime_state;"
            "DROP TABLE IF EXISTS npc_promotion_state;"
            "DROP TABLE IF EXISTS semantic_memories;"
            "DROP TABLE IF EXISTS entity_languages;"
            "DROP TABLE IF EXISTS owned_presentation_unlocks;"
            "DROP TABLE IF EXISTS manifestation_presentation_state;"
            "DROP TABLE IF EXISTS entity_equipment_proficiency;"
            "DROP TABLE IF EXISTS item_owner_affinity;"
            "DROP TABLE IF EXISTS container_contents;"
            "DROP TABLE IF EXISTS inventory_containers;"
            "DROP TABLE IF EXISTS equipment_bindings;"
            "DROP TABLE IF EXISTS item_modifiers;"
            "DROP TABLE IF EXISTS item_instances;"
            "ALTER TABLE knowledge_facts DROP COLUMN language_context_content_id;"
            "ALTER TABLE knowledge_facts DROP COLUMN evidence_world_tick;"
            "ALTER TABLE knowledge_facts DROP COLUMN source_event_id;"
            "ALTER TABLE knowledge_facts DROP COLUMN source_entity_id;"
            "ALTER TABLE knowledge_facts DROP COLUMN confidence_bps;"
            "ALTER TABLE knowledge_facts DROP COLUMN belief_state;"
            "DELETE FROM schema_migrations WHERE version = 13;"
            "DROP TABLE IF EXISTS logistics_routes;"
            "DROP TABLE IF EXISTS civilization_dimensions;"
            "DROP TABLE IF EXISTS civilization_state;"
            "DROP TABLE IF EXISTS project_assignments;"
            "DROP TABLE IF EXISTS project_phases;"
            "DROP TABLE IF EXISTS army_capabilities;"
            "DROP TABLE IF EXISTS war_participant_history;"
            "DROP TABLE IF EXISTS war_orders;"
            "DROP TABLE IF EXISTS war_objectives;"
            "DROP TABLE IF EXISTS war_fronts;"
            "DROP TABLE IF EXISTS dispatch_constraints;"
            "DROP TABLE IF EXISTS dispatch_objectives;"
            "ALTER TABLE dispatches DROP COLUMN delay_until_world_tick;"
            "ALTER TABLE dispatches DROP COLUMN outcome_state;"
            "ALTER TABLE dispatches DROP COLUMN abort_policy_json;"
            "ALTER TABLE dispatches DROP COLUMN risk_tolerance_bps;"
            "DELETE FROM schema_migrations WHERE version = 12;"
            "DROP TABLE IF EXISTS offline_simulation_state;"
            "DROP TABLE IF EXISTS content_unlock_state;"
            "DROP TABLE IF EXISTS world_director_schedule;"
            "DROP TABLE IF EXISTS junctions;"
            "DROP TABLE IF EXISTS reality_nodes;"
            "DROP TABLE IF EXISTS time_domains;"
            "DELETE FROM schema_migrations WHERE version = 11;"
            "DROP TABLE IF EXISTS character_convergence_sources;"
            "DROP TABLE IF EXISTS character_convergences;"
            "DROP TABLE IF EXISTS protagonist_world_manifestation_state;"
            "DROP TABLE IF EXISTS manifestation_world_fantasm_state;"
            "DROP TABLE IF EXISTS entity_transcendence_state;"
            "DROP TABLE IF EXISTS manifestation_reinforcement;"
            "DROP TABLE IF EXISTS manifestation_forms;"
            "DROP TABLE IF EXISTS manifestation_route_nodes;"
            "DROP TABLE IF EXISTS skill_provenance;"
            "DROP TABLE IF EXISTS entity_skills;"
            "DROP TABLE IF EXISTS grand_class_seats;"
            "DROP TABLE IF EXISTS entity_classes;"
            "DROP TABLE IF EXISTS factor_lineage;"
            "DROP TABLE IF EXISTS factor_instances;"
            "DROP TABLE IF EXISTS entity_rank_state;"
            "DELETE FROM schema_migrations WHERE version = 10;",
            Error));

    FOGWorldBootstrapResult Migration;
    TestTrue(
        TEXT("Safe bootstrap migrates schema 9 through current schema"),
        FOGWorldBootstrap::PrepareWorld(
            DatabasePath,
            Migration,
            Error));
    TestEqual(
        TEXT("Migration source schema"),
        Migration.SourceSchemaVersion,
        9);
    TestEqual(
        TEXT("Migration target schema"),
        Migration.TargetSchemaVersion,
        13);

    {
        FOGSQLiteWorldStore Store;
        TestTrue(
            TEXT("Open migrated current-schema database"),
            Store.Open(
                DatabasePath,
                Error));

        bool bFound = false;
        FOGCharacterManifestationRecord Manifestation;
        TestTrue(
            TEXT("Read migrated Manifestation"),
            Store.TryReadCharacterManifestation(
                ManifestationId,
                bFound,
                Manifestation,
                Error));
        TestTrue(
            TEXT("Legacy Manifestation survives"),
            bFound);
        TestEqual(
            TEXT("Opaque legacy progression JSON is retained verbatim"),
            Manifestation.ProgressionStateJson,
            LegacyProgression);
        TestEqual(
            TEXT("Legacy Level is preserved as migration provenance"),
            Manifestation.Level,
            77);

        FOGManifestationReinforcementRecord Reinforcement;
        bFound = false;
        TestTrue(
            TEXT("Read migration-created reinforcement state"),
            Store.TryReadManifestationReinforcement(
                ManifestationId,
                bFound,
                Reinforcement,
                Error));
        TestTrue(
            TEXT("Legacy Manifestation receives explicit reinforcement row"),
            bFound);
        TestEqual(
            TEXT("Legacy reinforcement remains unassessed"),
            Reinforcement.ReinforcementState,
            FName(TEXT("legacy_unassessed")));
        TestFalse(
            TEXT("Migration never fabricates max reinforcement"),
            Reinforcement.bMaxReinforced);

        FOGEntityRankStateRecord Rank;
        bFound = false;
        TestTrue(
            TEXT("Rank lookup is valid after migration"),
            Store.TryReadEntityRankState(
                ManifestationId,
                bFound,
                Rank,
                Error));
        TestFalse(
            TEXT("Opaque legacy JSON/Level never fabricates a Rank identity"),
            bFound);

        Store.Close();
    }

    IFileManager::Get().DeleteDirectory(
        *Directory,
        false,
        true);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOGRealityTime0011NonFabricatingMigrationTest,
    "OfflineGame.Persistence.Migration0011.DoesNotFabricateRealityCalendarOrDirectorState",
    EAutomationTestFlags::ApplicationContextMask |
        EAutomationTestFlags::EngineFilter)

bool FOGRealityTime0011NonFabricatingMigrationTest::RunTest(
    const FString& Parameters)
{
    const FString Directory =
        MakeRecoveryTestDirectory();
    const FString DatabasePath =
        FPaths::Combine(
            Directory,
            TEXT("legacy_reality_time.db"));
    IFileManager::Get().MakeDirectory(
        *Directory,
        true);

    const FOGEntityId LegacyWorldMarkerId =
        FOGEntityId::NewId();
    FString Error;

    {
        FOGSQLiteWorldStore Store;
        TestTrue(
            TEXT("Create current-schema fixture"),
            Store.Open(
                DatabasePath,
                Error));
        TestEqual(
            TEXT("Fixture begins at schema 12"),
            Store.GetSchemaVersion(
                Error),
            13);
        TestTrue(
            TEXT("Persist pre-0011 world-like marker"),
            Store.UpsertEntity(
                LegacyWorldMarkerId,
                FName(TEXT("legacy_world_marker")),
                10,
                TEXT("{\"legacy\":true}"),
                Error));
        Store.Close();
    }

    TestTrue(
        TEXT("Convert fixture to valid schema 10"),
        ExecuteRawDatabaseSql(
            DatabasePath,
            "DROP TABLE IF EXISTS heroic_records;"
            "DROP TABLE IF EXISTS character_adult_runtime_state;"
            "DROP TABLE IF EXISTS npc_promotion_state;"
            "DROP TABLE IF EXISTS semantic_memories;"
            "DROP TABLE IF EXISTS entity_languages;"
            "DROP TABLE IF EXISTS owned_presentation_unlocks;"
            "DROP TABLE IF EXISTS manifestation_presentation_state;"
            "DROP TABLE IF EXISTS entity_equipment_proficiency;"
            "DROP TABLE IF EXISTS item_owner_affinity;"
            "DROP TABLE IF EXISTS container_contents;"
            "DROP TABLE IF EXISTS inventory_containers;"
            "DROP TABLE IF EXISTS equipment_bindings;"
            "DROP TABLE IF EXISTS item_modifiers;"
            "DROP TABLE IF EXISTS item_instances;"
            "ALTER TABLE knowledge_facts DROP COLUMN language_context_content_id;"
            "ALTER TABLE knowledge_facts DROP COLUMN evidence_world_tick;"
            "ALTER TABLE knowledge_facts DROP COLUMN source_event_id;"
            "ALTER TABLE knowledge_facts DROP COLUMN source_entity_id;"
            "ALTER TABLE knowledge_facts DROP COLUMN confidence_bps;"
            "ALTER TABLE knowledge_facts DROP COLUMN belief_state;"
            "DELETE FROM schema_migrations WHERE version = 13;"
            "DROP TABLE IF EXISTS logistics_routes;"
            "DROP TABLE IF EXISTS civilization_dimensions;"
            "DROP TABLE IF EXISTS civilization_state;"
            "DROP TABLE IF EXISTS project_assignments;"
            "DROP TABLE IF EXISTS project_phases;"
            "DROP TABLE IF EXISTS army_capabilities;"
            "DROP TABLE IF EXISTS war_participant_history;"
            "DROP TABLE IF EXISTS war_orders;"
            "DROP TABLE IF EXISTS war_objectives;"
            "DROP TABLE IF EXISTS war_fronts;"
            "DROP TABLE IF EXISTS dispatch_constraints;"
            "DROP TABLE IF EXISTS dispatch_objectives;"
            "ALTER TABLE dispatches DROP COLUMN delay_until_world_tick;"
            "ALTER TABLE dispatches DROP COLUMN outcome_state;"
            "ALTER TABLE dispatches DROP COLUMN abort_policy_json;"
            "ALTER TABLE dispatches DROP COLUMN risk_tolerance_bps;"
            "DELETE FROM schema_migrations WHERE version = 12;"
            "DROP TABLE IF EXISTS offline_simulation_state;"
            "DROP TABLE IF EXISTS content_unlock_state;"
            "DROP TABLE IF EXISTS world_director_schedule;"
            "DROP TABLE IF EXISTS junctions;"
            "DROP TABLE IF EXISTS reality_nodes;"
            "DROP TABLE IF EXISTS time_domains;"
            "DELETE FROM schema_migrations WHERE version = 11;",
            Error));

    FOGWorldBootstrapResult Migration;
    TestTrue(
        TEXT("Safe bootstrap migrates schema 10 through current schema"),
        FOGWorldBootstrap::PrepareWorld(
            DatabasePath,
            Migration,
            Error));
    TestEqual(
        TEXT("Migration source schema"),
        Migration.SourceSchemaVersion,
        10);
    TestEqual(
        TEXT("Migration target schema"),
        Migration.TargetSchemaVersion,
        13);

    {
        FOGSQLiteWorldStore Store;
        TestTrue(
            TEXT("Open migrated current-schema database"),
            Store.Open(
                DatabasePath,
                Error));

        bool bFound = false;
        FOGRealityNodeRecord Reality;
        TestTrue(
            TEXT("Reality lookup remains valid"),
            Store.TryReadRealityNode(
                LegacyWorldMarkerId,
                bFound,
                Reality,
                Error));
        TestFalse(
            TEXT("Migration does not fabricate a Reality node from a generic legacy entity"),
            bFound);

        FOGTimeDomainRecord Domain;
        bFound = false;
        TestTrue(
            TEXT("Time-domain lookup remains valid"),
            Store.TryReadTimeDomain(
                LegacyWorldMarkerId,
                bFound,
                Domain,
                Error));
        TestFalse(
            TEXT("Migration does not fabricate a calendar/time domain"),
            bFound);

        FOGOfflineSimulationStateRecord OfflineState;
        bFound = false;
        TestTrue(
            TEXT("Offline-state lookup remains valid"),
            Store.TryReadOfflineSimulationState(
                LegacyWorldMarkerId,
                bFound,
                OfflineState,
                Error));
        TestFalse(
            TEXT("Migration does not fabricate World Director offline state"),
            bFound);

        Store.Close();
    }

    IFileManager::Get().DeleteDirectory(
        *Directory,
        false,
        true);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOGStrategy0012LegacyProjectionMigrationTest,
    "OfflineGame.Persistence.Migration0012.PreservesWarHistoryWithoutFabricatingCapabilities",
    EAutomationTestFlags::ApplicationContextMask |
        EAutomationTestFlags::EngineFilter)

bool FOGStrategy0012LegacyProjectionMigrationTest::RunTest(
    const FString& Parameters)
{
    const FString Directory =
        MakeRecoveryTestDirectory();
    const FString DatabasePath =
        FPaths::Combine(
            Directory,
            TEXT("legacy_strategy.db"));
    IFileManager::Get().MakeDirectory(
        *Directory,
        true);

    const FOGEntityId OwnerId =
        FOGEntityId::NewId();
    const FOGEntityId ParticipantId =
        FOGEntityId::NewId();
    const FOGEntityId FactionA =
        FOGEntityId::NewId();
    const FOGEntityId FactionB =
        FOGEntityId::NewId();
    const FOGEntityId LocationId =
        FOGEntityId::NewId();
    const FOGEntityId ArmyId =
        FOGEntityId::NewId();
    const FOGEntityId DispatchId =
        FOGEntityId::NewId();
    const FOGEntityId WarId =
        FOGEntityId::NewId();

    FString Error;

    {
        FOGSQLiteWorldStore Store;
        TestTrue(
            TEXT("Create current-schema strategy fixture"),
            Store.Open(
                DatabasePath,
                Error));
        TestEqual(
            TEXT("Fixture begins at schema 12"),
            Store.GetSchemaVersion(
                Error),
            13);

        TestTrue(
            TEXT("Persist legacy Dispatch owner"),
            Store.UpsertEntity(
                OwnerId,
                FName(TEXT("ruler")),
                0,
                TEXT("{}"),
                Error));
        TestTrue(
            TEXT("Persist legacy Dispatch participant"),
            Store.UpsertEntity(
                ParticipantId,
                FName(TEXT("character")),
                0,
                TEXT("{}"),
                Error));

        FOGDispatchRecord Dispatch;
        Dispatch.DispatchId =
            DispatchId;
        Dispatch.OwnerEntityId =
            OwnerId;
        Dispatch.Status =
            EOGDispatchStatus::Succeeded;
        Dispatch.ParticipantEntityIds =
            {ParticipantId};
        Dispatch.StartWorldTick = 10;
        Dispatch.ResolveWorldTick = 20;
        Dispatch.RiskBps = 2500;
        Dispatch.ResultJson =
            TEXT("{\"legacy_success\":true}");

        TestTrue(
            TEXT("Persist pre-0012-style successful Dispatch"),
            Store.UpsertDispatch(
                Dispatch,
                10,
                Error));

        FOGFactionRecord A;
        A.FactionId =
            FactionA;
        A.Kind =
            FName(TEXT("faction"));
        A.Population = 100;

        FOGFactionRecord B =
            A;
        B.FactionId =
            FactionB;

        TestTrue(
            TEXT("Persist legacy faction A"),
            Store.UpsertFaction(
                A,
                0,
                Error));
        TestTrue(
            TEXT("Persist legacy faction B"),
            Store.UpsertFaction(
                B,
                0,
                Error));

        FOGLocationRecord Location;
        Location.LocationId =
            LocationId;
        Location.Kind =
            FName(TEXT("region"));
        TestTrue(
            TEXT("Persist legacy Army location"),
            Store.UpsertLocation(
                Location,
                0,
                Error));

        FOGArmyRecord Army;
        Army.ArmyId =
            ArmyId;
        Army.FactionId =
            FactionA;
        Army.LocationId =
            LocationId;
        Army.Headcount = 50;
        Army.EffectivePower =
            FOGLargeNumber::FromInt64(
                5000);
        Army.State =
            FName(TEXT("ready"));

        TestTrue(
            TEXT("Persist legacy Army summary"),
            Store.UpsertArmy(
                Army,
                0,
                Error));

        FOGWarRecord War;
        War.WarId =
            WarId;
        War.Status =
            EOGWarStatus::Active;
        War.ObjectiveType =
            FName(TEXT("legacy_control"));
        War.ObjectiveTargetEntityId =
            LocationId;
        War.StartWorldTick = 30;

        FOGWarParticipant PA;
        PA.FactionId =
            FactionA;
        PA.SideIndex = 0;
        PA.bPrimary = true;

        FOGWarParticipant PB;
        PB.FactionId =
            FactionB;
        PB.SideIndex = 1;
        PB.bPrimary = true;

        War.Participants =
            {PA, PB};

        TestTrue(
            TEXT("Persist legacy continuous War parent"),
            Store.UpsertWar(
                War,
                30,
                Error));

        Store.Close();
    }

    TestTrue(
        TEXT("Convert fixture to valid schema 11"),
        ExecuteRawDatabaseSql(
            DatabasePath,
            "DROP TABLE IF EXISTS heroic_records;"
            "DROP TABLE IF EXISTS character_adult_runtime_state;"
            "DROP TABLE IF EXISTS npc_promotion_state;"
            "DROP TABLE IF EXISTS semantic_memories;"
            "DROP TABLE IF EXISTS entity_languages;"
            "DROP TABLE IF EXISTS owned_presentation_unlocks;"
            "DROP TABLE IF EXISTS manifestation_presentation_state;"
            "DROP TABLE IF EXISTS entity_equipment_proficiency;"
            "DROP TABLE IF EXISTS item_owner_affinity;"
            "DROP TABLE IF EXISTS container_contents;"
            "DROP TABLE IF EXISTS inventory_containers;"
            "DROP TABLE IF EXISTS equipment_bindings;"
            "DROP TABLE IF EXISTS item_modifiers;"
            "DROP TABLE IF EXISTS item_instances;"
            "ALTER TABLE knowledge_facts DROP COLUMN language_context_content_id;"
            "ALTER TABLE knowledge_facts DROP COLUMN evidence_world_tick;"
            "ALTER TABLE knowledge_facts DROP COLUMN source_event_id;"
            "ALTER TABLE knowledge_facts DROP COLUMN source_entity_id;"
            "ALTER TABLE knowledge_facts DROP COLUMN confidence_bps;"
            "ALTER TABLE knowledge_facts DROP COLUMN belief_state;"
            "DELETE FROM schema_migrations WHERE version = 13;"
            "DROP TABLE IF EXISTS logistics_routes;"
            "DROP TABLE IF EXISTS civilization_dimensions;"
            "DROP TABLE IF EXISTS civilization_state;"
            "DROP TABLE IF EXISTS project_assignments;"
            "DROP TABLE IF EXISTS project_phases;"
            "DROP TABLE IF EXISTS army_capabilities;"
            "DROP TABLE IF EXISTS war_participant_history;"
            "DROP TABLE IF EXISTS war_orders;"
            "DROP TABLE IF EXISTS war_objectives;"
            "DROP TABLE IF EXISTS war_fronts;"
            "DROP TABLE IF EXISTS dispatch_constraints;"
            "DROP TABLE IF EXISTS dispatch_objectives;"
            "ALTER TABLE dispatches DROP COLUMN delay_until_world_tick;"
            "ALTER TABLE dispatches DROP COLUMN outcome_state;"
            "ALTER TABLE dispatches DROP COLUMN abort_policy_json;"
            "ALTER TABLE dispatches DROP COLUMN risk_tolerance_bps;"
            "DELETE FROM schema_migrations WHERE version = 12;",
            Error));

    FOGWorldBootstrapResult Migration;
    TestTrue(
        TEXT("Safe bootstrap migrates schema 11 through current schema"),
        FOGWorldBootstrap::PrepareWorld(
            DatabasePath,
            Migration,
            Error));
    TestEqual(
        TEXT("Strategy migration source schema"),
        Migration.SourceSchemaVersion,
        11);
    TestEqual(
        TEXT("Current migration target schema"),
        Migration.TargetSchemaVersion,
        13);

    {
        FOGSQLiteWorldStore Store;
        TestTrue(
            TEXT("Open migrated schema-13 database"),
            Store.Open(
                DatabasePath,
                Error));

        bool bDispatchFound = false;
        FOGDispatchRecord Dispatch;
        TestTrue(
            TEXT("Read migrated Dispatch"),
            Store.TryReadDispatch(
                DispatchId,
                bDispatchFound,
                Dispatch,
                Error));
        TestTrue(
            TEXT("Legacy Dispatch survives migration"),
            bDispatchFound);
        TestEqual(
            TEXT("Migration applies normalized risk-tolerance default"),
            Dispatch.RiskToleranceBps,
            5000);
        TestEqual(
            TEXT("Legacy successful Dispatch receives inferable outcome label"),
            Dispatch.OutcomeState,
            FName(TEXT("success")));

        TArray<FOGDispatchObjectiveRecord> Objectives;
        TestTrue(
            TEXT("Read migrated Dispatch objectives"),
            Store.ListDispatchObjectives(
                DispatchId,
                Objectives,
                Error));
        TestTrue(
            TEXT("Migration never fabricates mission objectives"),
            Objectives.IsEmpty());

        TArray<FOGArmyCapabilityRecord> Capabilities;
        TestTrue(
            TEXT("Read migrated Army capability vector"),
            Store.ListArmyCapabilities(
                ArmyId,
                Capabilities,
                Error));
        TestTrue(
            TEXT("Migration never invents capability identities from cached EffectivePower"),
            Capabilities.IsEmpty());

        TArray<FOGWarParticipantHistoryRecord> History;
        TestTrue(
            TEXT("Read projected War participant history"),
            Store.ListWarParticipantHistory(
                WarId,
                History,
                Error));
        TestEqual(
            TEXT("Both authoritative legacy participants become history rows"),
            History.Num(),
            2);
        TestEqual(
            TEXT("Projected history uses canonical War start tick"),
            History[0].JoinedWorldTick,
            static_cast<int64>(30));

        bool bCivilizationFound = false;
        FOGCivilizationStateRecord Civilization;
        TestTrue(
            TEXT("Civilization lookup remains valid"),
            Store.TryReadCivilizationState(
                FactionA,
                bCivilizationFound,
                Civilization,
                Error));
        TestFalse(
            TEXT("Migration does not fabricate a universal civilization profile"),
            bCivilizationFound);

        Store.Close();
    }

    IFileManager::Get().DeleteDirectory(
        *Directory,
        false,
        true);
    return true;
}

#endif
