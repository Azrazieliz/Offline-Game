#pragma once

#include "CoreMinimal.h"

struct OFFLINEGAME_API FOGBackupCatalogEntry
{
    FString BackupId;
    FString BackupPathOrUri;
    int32 SchemaVersion = 0;
    FString WorldIdentity;
    FString CreatedUtc;
    FString ContentHash;
    FString SourceBuildVersion;
    FName ValidationState = FName(TEXT("unverified"));
};

class OFFLINEGAME_API FOGRecoveryCatalogService
{
public:
    static bool AddOrUpdateEntry(
        const FString& CatalogPath,
        const FOGBackupCatalogEntry& Entry,
        FString& OutError);

    static bool LoadEntries(
        const FString& CatalogPath,
        TArray<FOGBackupCatalogEntry>& OutEntries,
        FString& OutError);

    static bool RemoveEntriesByBackupIds(
        const FString& CatalogPath,
        const TSet<FString>& BackupIds,
        FString& OutError);

    /**
     * Clears authoritative world files. Backups/catalog are preserved unless
     * bDeleteBackups is explicitly true.
     */
    static bool ClearWorld(
        const FString& WorldDatabasePath,
        const FString& CatalogPath,
        const FString& BackupDirectory,
        bool bDeleteBackups,
        FString& OutError);

    /**
     * Export the canonical world database through SQLite's online-backup path,
     * register it in the recovery catalog and leave the live database untouched.
     */
    static bool ExportWorldBackup(
        const FString& WorldDatabasePath,
        const FString& DestinationBackupPath,
        const FString& CatalogPath,
        const FString& WorldIdentity,
        const FString& SourceBuildVersion,
        FString& OutBackupId,
        FString& OutError);

    /**
     * Validate/migrate an imported backup on an isolated working copy, preserve
     * the current canonical world, then restore the validated import through
     * SQLite's backup API. Call from recovery/opening flow, not while gameplay
     * is actively mutating the world database.
     */
    static bool ImportWorldBackup(
        const FString& SourceBackupPath,
        const FString& WorldDatabasePath,
        const FString& ProtectedArchiveDirectory,
        const FString& CatalogPath,
        const FString& WorldIdentity,
        const FString& SourceBuildVersion,
        FString& OutPreservedWorldPath,
        FString& OutError);

    static FString HashFile(
        const FString& FilePath);
};
