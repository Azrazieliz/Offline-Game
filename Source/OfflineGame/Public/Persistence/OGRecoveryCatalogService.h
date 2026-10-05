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

    static FString HashFile(
        const FString& FilePath);
};
