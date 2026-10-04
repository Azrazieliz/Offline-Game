#pragma once

#include "CoreMinimal.h"

/**
 * Result metadata for migration-safe world bootstrap.
 *
 * Paths are returned for diagnostics/recovery UI only. The authoritative database
 * is never migrated in place: existing worlds are preserved first, migrated on a
 * working copy, validated, then promoted.
 */
struct OFFLINEGAME_API FOGWorldBootstrapResult
{
    bool bExistingDatabase = false;
    bool bMigrationRequired = false;
    bool bMigrationPerformed = false;
    int32 SourceSchemaVersion = 0;
    int32 TargetSchemaVersion = 0;
    FString RecoveryDatabasePath;
    FString MigrationReportPath;
    FString ValidationReport;
};

/**
 * Pre-open persistence bootstrap for the single canonical world.
 *
 * Existing worlds that require migration are copied through SQLite's backup API
 * into an untouched recovery database. Only the working copy is migrated.
 * Promotion happens only after schema, foreign-key, application and SQLite
 * integrity validation succeed.
 */
class OFFLINEGAME_API FOGWorldBootstrap
{
public:
    static bool PrepareWorld(
        const FString& AbsoluteDatabasePath,
        FOGWorldBootstrapResult& OutResult,
        FString& OutError);
};
