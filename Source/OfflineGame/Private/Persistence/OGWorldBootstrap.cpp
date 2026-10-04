#include "Persistence/OGWorldBootstrap.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Persistence/OGSQLiteWorldStore.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "sqlite/sqlite3.h"

namespace
{
bool OpenReadOnlyDatabase(
    const FString& DatabasePath,
    sqlite3*& OutDatabase,
    FString& OutError)
{
    OutDatabase = nullptr;

    FTCHARToUTF8 PathUtf8(*DatabasePath);
    const int32 Result = sqlite3_open_v2(
        PathUtf8.Get(),
        &OutDatabase,
        SQLITE_OPEN_READONLY | SQLITE_OPEN_FULLMUTEX,
        nullptr);

    if (Result != SQLITE_OK || OutDatabase == nullptr)
    {
        const FString Detail = OutDatabase
            ? UTF8_TO_TCHAR(sqlite3_errmsg(OutDatabase))
            : TEXT("database handle is null");
        OutError = FString::Printf(
            TEXT("Failed to open world database read-only: %s"),
            *Detail);

        if (OutDatabase)
        {
            sqlite3_close_v2(OutDatabase);
            OutDatabase = nullptr;
        }
        return false;
    }

    sqlite3_extended_result_codes(OutDatabase, 1);
    sqlite3_busy_timeout(OutDatabase, 5000);
    return true;
}

bool ReadSchemaVersionWithoutMutation(
    const FString& DatabasePath,
    int32& OutSchemaVersion,
    FString& OutError)
{
    OutSchemaVersion = INDEX_NONE;
    OutError.Reset();

    sqlite3* Database = nullptr;
    if (!OpenReadOnlyDatabase(DatabasePath, Database, OutError))
    {
        return false;
    }

    sqlite3_stmt* TableStatement = nullptr;
    const char* TableSql =
        "SELECT 1 FROM sqlite_master "
        "WHERE type='table' AND name='schema_migrations' LIMIT 1;";

    if (sqlite3_prepare_v2(
            Database,
            TableSql,
            -1,
            &TableStatement,
            nullptr) != SQLITE_OK)
    {
        OutError = FString::Printf(
            TEXT("Failed to inspect schema_migrations: %s"),
            UTF8_TO_TCHAR(sqlite3_errmsg(Database)));
        sqlite3_close_v2(Database);
        return false;
    }

    const int32 TableStep = sqlite3_step(TableStatement);
    const bool bHasMigrationTable = TableStep == SQLITE_ROW;
    const bool bTableReadOk =
        bHasMigrationTable || TableStep == SQLITE_DONE;
    sqlite3_finalize(TableStatement);

    if (!bTableReadOk)
    {
        OutError = FString::Printf(
            TEXT("Failed to inspect schema_migrations table state: %s"),
            UTF8_TO_TCHAR(sqlite3_errmsg(Database)));
        sqlite3_close_v2(Database);
        return false;
    }

    if (!bHasMigrationTable)
    {
        OutSchemaVersion = 0;
        sqlite3_close_v2(Database);
        return true;
    }

    sqlite3_stmt* VersionStatement = nullptr;
    const char* VersionSql =
        "SELECT COALESCE(MAX(version), 0) FROM schema_migrations;";

    if (sqlite3_prepare_v2(
            Database,
            VersionSql,
            -1,
            &VersionStatement,
            nullptr) != SQLITE_OK)
    {
        OutError = FString::Printf(
            TEXT("Failed to prepare schema-version inspection: %s"),
            UTF8_TO_TCHAR(sqlite3_errmsg(Database)));
        sqlite3_close_v2(Database);
        return false;
    }

    const int32 VersionStep = sqlite3_step(VersionStatement);
    if (VersionStep == SQLITE_ROW)
    {
        OutSchemaVersion =
            sqlite3_column_int(VersionStatement, 0);
    }
    else
    {
        OutError = FString::Printf(
            TEXT("Failed to read schema version: %s"),
            UTF8_TO_TCHAR(sqlite3_errmsg(Database)));
    }

    sqlite3_finalize(VersionStatement);
    sqlite3_close_v2(Database);
    return OutSchemaVersion != INDEX_NONE;
}

bool BackupDatabaseWithoutMutatingSource(
    const FString& SourcePath,
    const FString& DestinationPath,
    FString& OutError)
{
    OutError.Reset();

    IFileManager& Files = IFileManager::Get();
    const FString DestinationDirectory =
        FPaths::GetPath(DestinationPath);

    if (!DestinationDirectory.IsEmpty() &&
        !Files.MakeDirectory(*DestinationDirectory, true) &&
        !Files.DirectoryExists(*DestinationDirectory))
    {
        OutError = FString::Printf(
            TEXT("Failed to create migration recovery directory: %s"),
            *DestinationDirectory);
        return false;
    }

    Files.Delete(*DestinationPath, false, true, true);

    sqlite3* SourceDatabase = nullptr;
    if (!OpenReadOnlyDatabase(
            SourcePath,
            SourceDatabase,
            OutError))
    {
        return false;
    }

    sqlite3* DestinationDatabase = nullptr;
    FTCHARToUTF8 DestinationUtf8(*DestinationPath);
    const int32 OpenDestinationResult = sqlite3_open_v2(
        DestinationUtf8.Get(),
        &DestinationDatabase,
        SQLITE_OPEN_READWRITE |
            SQLITE_OPEN_CREATE |
            SQLITE_OPEN_FULLMUTEX,
        nullptr);

    if (OpenDestinationResult != SQLITE_OK ||
        DestinationDatabase == nullptr)
    {
        const FString Detail = DestinationDatabase
            ? UTF8_TO_TCHAR(sqlite3_errmsg(DestinationDatabase))
            : TEXT("database handle is null");
        OutError = FString::Printf(
            TEXT("Failed to create pre-migration recovery database: %s"),
            *Detail);

        if (DestinationDatabase)
        {
            sqlite3_close_v2(DestinationDatabase);
        }
        sqlite3_close_v2(SourceDatabase);
        return false;
    }

    sqlite3_backup* Backup = sqlite3_backup_init(
        DestinationDatabase,
        "main",
        SourceDatabase,
        "main");

    if (!Backup)
    {
        OutError = FString::Printf(
            TEXT("Failed to initialize pre-migration SQLite backup: %s"),
            UTF8_TO_TCHAR(sqlite3_errmsg(DestinationDatabase)));
        sqlite3_close_v2(DestinationDatabase);
        sqlite3_close_v2(SourceDatabase);
        return false;
    }

    const int32 StepResult =
        sqlite3_backup_step(Backup, -1);
    const int32 FinishResult =
        sqlite3_backup_finish(Backup);

    const bool bSucceeded =
        (StepResult == SQLITE_DONE ||
         StepResult == SQLITE_OK) &&
        FinishResult == SQLITE_OK;

    if (!bSucceeded)
    {
        OutError = FString::Printf(
            TEXT("Pre-migration SQLite backup failed: %s"),
            UTF8_TO_TCHAR(sqlite3_errmsg(DestinationDatabase)));
    }

    sqlite3_close_v2(DestinationDatabase);
    sqlite3_close_v2(SourceDatabase);

    if (!bSucceeded)
    {
        Files.Delete(*DestinationPath, false, true, true);
    }

    return bSucceeded;
}

bool CheckpointSourceWithoutMigration(
    const FString& DatabasePath,
    FString& OutError)
{
    OutError.Reset();

    sqlite3* Database = nullptr;
    FTCHARToUTF8 PathUtf8(*DatabasePath);
    const int32 OpenResult = sqlite3_open_v2(
        PathUtf8.Get(),
        &Database,
        SQLITE_OPEN_READWRITE | SQLITE_OPEN_FULLMUTEX,
        nullptr);

    if (OpenResult != SQLITE_OK || Database == nullptr)
    {
        const FString Detail = Database
            ? UTF8_TO_TCHAR(sqlite3_errmsg(Database))
            : TEXT("database handle is null");
        OutError = FString::Printf(
            TEXT("Failed to open source database for WAL checkpoint: %s"),
            *Detail);

        if (Database)
        {
            sqlite3_close_v2(Database);
        }
        return false;
    }

    char* ErrorMessage = nullptr;
    const int32 CheckpointResult = sqlite3_exec(
        Database,
        "PRAGMA wal_checkpoint(TRUNCATE);",
        nullptr,
        nullptr,
        &ErrorMessage);

    if (CheckpointResult != SQLITE_OK)
    {
        const FString Detail = ErrorMessage
            ? UTF8_TO_TCHAR(ErrorMessage)
            : UTF8_TO_TCHAR(sqlite3_errmsg(Database));
        OutError = FString::Printf(
            TEXT("Failed to checkpoint source database before promotion: %s"),
            *Detail);
    }

    if (ErrorMessage)
    {
        sqlite3_free(ErrorMessage);
    }

    sqlite3_close_v2(Database);
    return CheckpointResult == SQLITE_OK;
}

void DeleteSqliteSidecars(const FString& DatabasePath)
{
    IFileManager& Files = IFileManager::Get();
    Files.Delete(
        *(DatabasePath + TEXT("-wal")),
        false,
        true,
        true);
    Files.Delete(
        *(DatabasePath + TEXT("-shm")),
        false,
        true,
        true);
}

bool WriteMigrationReport(
    const FString& ReportPath,
    const FString& Status,
    const FString& DatabasePath,
    const FString& WorkingPath,
    const FOGWorldBootstrapResult& Result,
    const FString& Error,
    FString& OutError)
{
    OutError.Reset();

    TSharedRef<FJsonObject> Root =
        MakeShared<FJsonObject>();

    Root->SetStringField(
        TEXT("generated_utc"),
        FDateTime::UtcNow().ToIso8601());
    Root->SetStringField(TEXT("status"), Status);
    Root->SetStringField(
        TEXT("database_file"),
        FPaths::GetCleanFilename(DatabasePath));
    Root->SetStringField(
        TEXT("working_file"),
        FPaths::GetCleanFilename(WorkingPath));
    Root->SetStringField(
        TEXT("recovery_file"),
        FPaths::GetCleanFilename(
            Result.RecoveryDatabasePath));
    Root->SetNumberField(
        TEXT("source_schema_version"),
        Result.SourceSchemaVersion);
    Root->SetNumberField(
        TEXT("target_schema_version"),
        Result.TargetSchemaVersion);
    Root->SetBoolField(
        TEXT("migration_required"),
        Result.bMigrationRequired);
    Root->SetBoolField(
        TEXT("migration_performed"),
        Result.bMigrationPerformed);
    Root->SetStringField(
        TEXT("validation_report"),
        Result.ValidationReport);
    Root->SetStringField(TEXT("error"), Error);

    FString Json;
    const TSharedRef<TJsonWriter<>> Writer =
        TJsonWriterFactory<>::Create(&Json);

    if (!FJsonSerializer::Serialize(Root, Writer))
    {
        OutError =
            TEXT("Failed to serialize migration report.");
        return false;
    }

    const FString ReportDirectory =
        FPaths::GetPath(ReportPath);
    IFileManager& Files = IFileManager::Get();

    if (!ReportDirectory.IsEmpty() &&
        !Files.MakeDirectory(*ReportDirectory, true) &&
        !Files.DirectoryExists(*ReportDirectory))
    {
        OutError = FString::Printf(
            TEXT("Failed to create migration report directory: %s"),
            *ReportDirectory);
        return false;
    }

    if (!FFileHelper::SaveStringToFile(
            Json,
            *ReportPath,
            FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
    {
        OutError = FString::Printf(
            TEXT("Failed to write migration report: %s"),
            *ReportPath);
        return false;
    }

    return true;
}

bool RestoreAuthoritativeDatabase(
    const FString& DatabasePath,
    const FString& RecoveryPath,
    const FString& Token,
    FString& OutError)
{
    OutError.Reset();

    IFileManager& Files = IFileManager::Get();
    const FString RestoreWorkingPath =
        DatabasePath +
        FString::Printf(
            TEXT(".restore_%s"),
            *Token);

    Files.Delete(
        *RestoreWorkingPath,
        false,
        true,
        true);

    if (Files.Copy(
            *RestoreWorkingPath,
            *RecoveryPath,
            true,
            true) != COPY_OK)
    {
        OutError =
            TEXT("Failed to stage recovery database for authoritative restore.");
        return false;
    }

    DeleteSqliteSidecars(DatabasePath);

    if (!Files.Move(
            *DatabasePath,
            *RestoreWorkingPath,
            true,
            true,
            false,
            true))
    {
        OutError =
            TEXT("Failed to atomically restore the authoritative database from recovery.");
        Files.Delete(
            *RestoreWorkingPath,
            false,
            true,
            true);
        return false;
    }

    DeleteSqliteSidecars(DatabasePath);
    return true;
}
}

bool FOGWorldBootstrap::PrepareWorld(
    const FString& AbsoluteDatabasePath,
    FOGWorldBootstrapResult& OutResult,
    FString& OutError)
{
    OutResult = FOGWorldBootstrapResult();
    OutError.Reset();

    if (AbsoluteDatabasePath.IsEmpty())
    {
        OutError = TEXT("Database path is empty.");
        return false;
    }

    IFileManager& Files = IFileManager::Get();
    OutResult.TargetSchemaVersion =
        FOGSQLiteWorldStore::LatestSchemaVersion();
    OutResult.bExistingDatabase =
        Files.FileExists(*AbsoluteDatabasePath);

    if (!OutResult.bExistingDatabase)
    {
        return true;
    }

    if (!ReadSchemaVersionWithoutMutation(
            AbsoluteDatabasePath,
            OutResult.SourceSchemaVersion,
            OutError))
    {
        return false;
    }

    if (OutResult.SourceSchemaVersion >
        OutResult.TargetSchemaVersion)
    {
        OutError = FString::Printf(
            TEXT("World schema %d is newer than this build supports (%d)."),
            OutResult.SourceSchemaVersion,
            OutResult.TargetSchemaVersion);
        return false;
    }

    OutResult.bMigrationRequired =
        OutResult.SourceSchemaVersion <
        OutResult.TargetSchemaVersion;

    if (!OutResult.bMigrationRequired)
    {
        return true;
    }

    const FString DatabaseDirectory =
        FPaths::GetPath(AbsoluteDatabasePath);
    const FString RecoveryDirectory =
        FPaths::Combine(
            DatabaseDirectory,
            TEXT("MigrationRecovery"));
    const FString Token =
        FString::Printf(
            TEXT("%s_%s"),
            *FDateTime::UtcNow().ToString(
                TEXT("%Y%m%dT%H%M%S")),
            *FGuid::NewGuid()
                .ToString(EGuidFormats::Digits)
                .Left(8));

    OutResult.RecoveryDatabasePath =
        FPaths::Combine(
            RecoveryDirectory,
            FString::Printf(
                TEXT("pre_migration_schema_%d_%s.db"),
                OutResult.SourceSchemaVersion,
                *Token));
    OutResult.MigrationReportPath =
        FPaths::Combine(
            RecoveryDirectory,
            FString::Printf(
                TEXT("migration_%d_to_%d_%s.json"),
                OutResult.SourceSchemaVersion,
                OutResult.TargetSchemaVersion,
                *Token));

    const FString WorkingPath =
        AbsoluteDatabasePath +
        FString::Printf(
            TEXT(".migration_%s"),
            *Token);

    FString ReportError;
    if (!BackupDatabaseWithoutMutatingSource(
            AbsoluteDatabasePath,
            OutResult.RecoveryDatabasePath,
            OutError))
    {
        WriteMigrationReport(
            OutResult.MigrationReportPath,
            TEXT("recovery_backup_failed"),
            AbsoluteDatabasePath,
            WorkingPath,
            OutResult,
            OutError,
            ReportError);
        return false;
    }

    WriteMigrationReport(
        OutResult.MigrationReportPath,
        TEXT("recovery_preserved"),
        AbsoluteDatabasePath,
        WorkingPath,
        OutResult,
        FString(),
        ReportError);

    Files.Delete(*WorkingPath, false, true, true);
    if (Files.Copy(
            *WorkingPath,
            *OutResult.RecoveryDatabasePath,
            true,
            true) != COPY_OK)
    {
        OutError =
            TEXT("Failed to create migration working copy.");
        WriteMigrationReport(
            OutResult.MigrationReportPath,
            TEXT("working_copy_failed"),
            AbsoluteDatabasePath,
            WorkingPath,
            OutResult,
            OutError,
            ReportError);
        return false;
    }

    {
        FOGSQLiteWorldStore WorkingStore;
        FString MigrationError;

        if (!WorkingStore.Open(
                WorkingPath,
                MigrationError))
        {
            OutError = FString::Printf(
                TEXT("Working-copy migration failed: %s"),
                *MigrationError);
            WriteMigrationReport(
                OutResult.MigrationReportPath,
                TEXT("migration_failed"),
                AbsoluteDatabasePath,
                WorkingPath,
                OutResult,
                OutError,
                ReportError);
            WorkingStore.Close();
            DeleteSqliteSidecars(WorkingPath);
            Files.Delete(*WorkingPath, false, true, true);
            return false;
        }

        FString SchemaError;
        const int32 WorkingSchemaVersion =
            WorkingStore.GetSchemaVersion(
                SchemaError);

        if (WorkingSchemaVersion !=
            OutResult.TargetSchemaVersion)
        {
            OutError = FString::Printf(
                TEXT("Working-copy migration reached schema %d instead of %d. %s"),
                WorkingSchemaVersion,
                OutResult.TargetSchemaVersion,
                *SchemaError);
            WriteMigrationReport(
                OutResult.MigrationReportPath,
                TEXT("schema_validation_failed"),
                AbsoluteDatabasePath,
                WorkingPath,
                OutResult,
                OutError,
                ReportError);
            WorkingStore.Close();
            DeleteSqliteSidecars(WorkingPath);
            Files.Delete(*WorkingPath, false, true, true);
            return false;
        }

        if (!WorkingStore.RunApplicationValidation(
                OutResult.ValidationReport,
                MigrationError))
        {
            OutError = FString::Printf(
                TEXT("Working-copy validation failed: %s"),
                *MigrationError);
            WriteMigrationReport(
                OutResult.MigrationReportPath,
                TEXT("application_validation_failed"),
                AbsoluteDatabasePath,
                WorkingPath,
                OutResult,
                OutError,
                ReportError);
            WorkingStore.Close();
            DeleteSqliteSidecars(WorkingPath);
            Files.Delete(*WorkingPath, false, true, true);
            return false;
        }

        if (!WorkingStore.Checkpoint(MigrationError))
        {
            OutError = FString::Printf(
                TEXT("Working-copy checkpoint failed: %s"),
                *MigrationError);
            WriteMigrationReport(
                OutResult.MigrationReportPath,
                TEXT("working_checkpoint_failed"),
                AbsoluteDatabasePath,
                WorkingPath,
                OutResult,
                OutError,
                ReportError);
            WorkingStore.Close();
            DeleteSqliteSidecars(WorkingPath);
            Files.Delete(*WorkingPath, false, true, true);
            return false;
        }

        WorkingStore.Close();
    }

    FString CheckpointError;
    if (!CheckpointSourceWithoutMigration(
            AbsoluteDatabasePath,
            CheckpointError))
    {
        OutError = CheckpointError;
        WriteMigrationReport(
            OutResult.MigrationReportPath,
            TEXT("source_checkpoint_failed"),
            AbsoluteDatabasePath,
            WorkingPath,
            OutResult,
            OutError,
            ReportError);
        DeleteSqliteSidecars(WorkingPath);
        Files.Delete(*WorkingPath, false, true, true);
        return false;
    }

    DeleteSqliteSidecars(AbsoluteDatabasePath);
    DeleteSqliteSidecars(WorkingPath);

    if (!Files.Move(
            *AbsoluteDatabasePath,
            *WorkingPath,
            true,
            true,
            false,
            true))
    {
        OutError =
            TEXT("Atomic promotion of the validated migration working copy failed.");
        WriteMigrationReport(
            OutResult.MigrationReportPath,
            TEXT("promotion_failed"),
            AbsoluteDatabasePath,
            WorkingPath,
            OutResult,
            OutError,
            ReportError);
        Files.Delete(*WorkingPath, false, true, true);
        return false;
    }

    OutResult.bMigrationPerformed = true;

    int32 PromotedSchemaVersion = INDEX_NONE;
    FString PromotedInspectError;
    if (!ReadSchemaVersionWithoutMutation(
            AbsoluteDatabasePath,
            PromotedSchemaVersion,
            PromotedInspectError) ||
        PromotedSchemaVersion !=
            OutResult.TargetSchemaVersion)
    {
        FString RestoreError;
        RestoreAuthoritativeDatabase(
            AbsoluteDatabasePath,
            OutResult.RecoveryDatabasePath,
            Token,
            RestoreError);

        OutResult.bMigrationPerformed = false;
        OutError = FString::Printf(
            TEXT("Promoted database failed schema verification. %s Restore: %s"),
            *PromotedInspectError,
            RestoreError.IsEmpty()
                ? TEXT("completed")
                : *RestoreError);
        WriteMigrationReport(
            OutResult.MigrationReportPath,
            TEXT("promotion_verification_failed"),
            AbsoluteDatabasePath,
            WorkingPath,
            OutResult,
            OutError,
            ReportError);
        return false;
    }

    {
        FOGSQLiteWorldStore PromotedStore;
        FString ValidationError;
        FString FinalValidationReport;

        const bool bOpened =
            PromotedStore.Open(
                AbsoluteDatabasePath,
                ValidationError);
        const bool bValidated =
            bOpened &&
            PromotedStore.RunApplicationValidation(
                FinalValidationReport,
                ValidationError);
        PromotedStore.Close();

        if (!bValidated)
        {
            FString RestoreError;
            RestoreAuthoritativeDatabase(
                AbsoluteDatabasePath,
                OutResult.RecoveryDatabasePath,
                Token,
                RestoreError);

            OutResult.bMigrationPerformed = false;
            OutError = FString::Printf(
                TEXT("Promoted database failed final validation: %s Restore: %s"),
                *ValidationError,
                RestoreError.IsEmpty()
                    ? TEXT("completed")
                    : *RestoreError);
            WriteMigrationReport(
                OutResult.MigrationReportPath,
                TEXT("final_validation_failed"),
                AbsoluteDatabasePath,
                WorkingPath,
                OutResult,
                OutError,
                ReportError);
            return false;
        }

        OutResult.ValidationReport =
            FinalValidationReport;
    }

    WriteMigrationReport(
        OutResult.MigrationReportPath,
        TEXT("promoted"),
        AbsoluteDatabasePath,
        WorkingPath,
        OutResult,
        FString(),
        ReportError);

    if (!ReportError.IsEmpty())
    {
        OutError = FString::Printf(
            TEXT("Migration succeeded, but final migration report update failed: %s"),
            *ReportError);
        return false;
    }

    return true;
}
