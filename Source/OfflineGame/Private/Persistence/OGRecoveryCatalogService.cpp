#include "Persistence/OGRecoveryCatalogService.h"

#include "Persistence/OGSQLiteWorldStore.h"
#include "Persistence/OGWorldBootstrap.h"
#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
bool WriteRecoveryCatalogEntries(
    const FString& CatalogPath,
    const TArray<FOGBackupCatalogEntry>& Entries,
    FString& OutError)
{
    const FString Directory =
        FPaths::GetPath(CatalogPath);
    if (!Directory.IsEmpty() &&
        !IFileManager::Get().MakeDirectory(
            *Directory,
            true) &&
        !IFileManager::Get().DirectoryExists(
            *Directory))
    {
        OutError =
            TEXT("Failed to create recovery catalog directory.");
        return false;
    }

    TSharedRef<FJsonObject> Root =
        MakeShared<FJsonObject>();
    Root->SetNumberField(
        TEXT("version"),
        1);

    TArray<TSharedPtr<FJsonValue>> JsonEntries;
    for (const FOGBackupCatalogEntry& Item :
         Entries)
    {
        TSharedRef<FJsonObject> Object =
            MakeShared<FJsonObject>();
        Object->SetStringField(
            TEXT("backup_id"),
            Item.BackupId);
        Object->SetStringField(
            TEXT("path_or_uri"),
            Item.BackupPathOrUri);
        Object->SetNumberField(
            TEXT("schema_version"),
            Item.SchemaVersion);
        Object->SetStringField(
            TEXT("world_identity"),
            Item.WorldIdentity);
        Object->SetStringField(
            TEXT("created_utc"),
            Item.CreatedUtc);
        Object->SetStringField(
            TEXT("hash"),
            Item.ContentHash);
        Object->SetStringField(
            TEXT("source_build_version"),
            Item.SourceBuildVersion);
        Object->SetStringField(
            TEXT("validation_state"),
            Item.ValidationState.ToString());
        JsonEntries.Add(
            MakeShared<FJsonValueObject>(
                Object));
    }
    Root->SetArrayField(
        TEXT("entries"),
        JsonEntries);

    FString Json;
    const TSharedRef<TJsonWriter<>> Writer =
        TJsonWriterFactory<>::Create(
            &Json);
    if (!FJsonSerializer::Serialize(
            Root,
            Writer) ||
        !FFileHelper::SaveStringToFile(
            Json,
            *CatalogPath,
            FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
    {
        OutError =
            TEXT("Failed to write recovery catalog.");
        return false;
    }

    return true;
}
}

bool FOGRecoveryCatalogService::LoadEntries(
    const FString& CatalogPath,
    TArray<FOGBackupCatalogEntry>& OutEntries,
    FString& OutError)
{
    OutEntries.Reset();
    OutError.Reset();

    if (CatalogPath.IsEmpty())
    {
        OutError =
            TEXT("Recovery catalog path is empty.");
        return false;
    }

    if (!IFileManager::Get().FileExists(
            *CatalogPath))
    {
        return true;
    }

    FString Json;
    if (!FFileHelper::LoadFileToString(
            Json,
            *CatalogPath))
    {
        OutError =
            TEXT("Failed to read recovery catalog.");
        return false;
    }

    TSharedPtr<FJsonObject> Root;
    const TSharedRef<TJsonReader<>> Reader =
        TJsonReaderFactory<>::Create(Json);
    if (!FJsonSerializer::Deserialize(
            Reader,
            Root) ||
        !Root.IsValid())
    {
        OutError =
            TEXT("Recovery catalog is invalid JSON.");
        return false;
    }

    const TArray<TSharedPtr<FJsonValue>>* Entries = nullptr;
    if (!Root->TryGetArrayField(
            TEXT("entries"),
            Entries) ||
        !Entries)
    {
        return true;
    }

    for (const TSharedPtr<FJsonValue>& Value :
         *Entries)
    {
        const TSharedPtr<FJsonObject>* Object = nullptr;
        if (!Value.IsValid() ||
            !Value->TryGetObject(Object) ||
            !Object ||
            !Object->IsValid())
        {
            continue;
        }

        FOGBackupCatalogEntry Entry;
        (*Object)->TryGetStringField(
            TEXT("backup_id"),
            Entry.BackupId);
        (*Object)->TryGetStringField(
            TEXT("path_or_uri"),
            Entry.BackupPathOrUri);
        double Schema = 0.0;
        (*Object)->TryGetNumberField(
            TEXT("schema_version"),
            Schema);
        Entry.SchemaVersion =
            static_cast<int32>(Schema);
        (*Object)->TryGetStringField(
            TEXT("world_identity"),
            Entry.WorldIdentity);
        (*Object)->TryGetStringField(
            TEXT("created_utc"),
            Entry.CreatedUtc);
        (*Object)->TryGetStringField(
            TEXT("hash"),
            Entry.ContentHash);
        (*Object)->TryGetStringField(
            TEXT("source_build_version"),
            Entry.SourceBuildVersion);
        FString Validation;
        (*Object)->TryGetStringField(
            TEXT("validation_state"),
            Validation);
        Entry.ValidationState =
            Validation.IsEmpty()
                ? FName(TEXT("unverified"))
                : FName(*Validation);

        if (!Entry.BackupId.IsEmpty() &&
            !Entry.BackupPathOrUri.IsEmpty())
        {
            OutEntries.Add(
                MoveTemp(Entry));
        }
    }

    return true;
}

bool FOGRecoveryCatalogService::AddOrUpdateEntry(
    const FString& CatalogPath,
    const FOGBackupCatalogEntry& Entry,
    FString& OutError)
{
    OutError.Reset();

    if (CatalogPath.IsEmpty() ||
        Entry.BackupId.IsEmpty() ||
        Entry.BackupPathOrUri.IsEmpty() ||
        Entry.SchemaVersion < 0 ||
        Entry.WorldIdentity.IsEmpty() ||
        Entry.CreatedUtc.IsEmpty() ||
        Entry.ContentHash.IsEmpty() ||
        Entry.SourceBuildVersion.IsEmpty() ||
        Entry.ValidationState.IsNone())
    {
        OutError =
            TEXT("Recovery catalog entry is incomplete.");
        return false;
    }

    TArray<FOGBackupCatalogEntry> Entries;
    if (!LoadEntries(
            CatalogPath,
            Entries,
            OutError))
    {
        return false;
    }

    bool bReplaced = false;
    for (FOGBackupCatalogEntry& Existing :
         Entries)
    {
        if (Existing.BackupId ==
            Entry.BackupId)
        {
            Existing =
                Entry;
            bReplaced =
                true;
            break;
        }
    }
    if (!bReplaced)
    {
        Entries.Add(
            Entry);
    }

    return WriteRecoveryCatalogEntries(
        CatalogPath,
        Entries,
        OutError);
}

bool FOGRecoveryCatalogService::RemoveEntriesByBackupIds(
    const FString& CatalogPath,
    const TSet<FString>& BackupIds,
    FString& OutError)
{
    OutError.Reset();

    if (CatalogPath.IsEmpty())
    {
        OutError =
            TEXT("Recovery catalog path is empty.");
        return false;
    }

    if (BackupIds.IsEmpty())
    {
        return true;
    }

    TArray<FOGBackupCatalogEntry> Entries;
    if (!LoadEntries(
            CatalogPath,
            Entries,
            OutError))
    {
        return false;
    }

    Entries.RemoveAll(
        [&BackupIds](
            const FOGBackupCatalogEntry& Entry)
        {
            return BackupIds.Contains(
                Entry.BackupId);
        });

    return WriteRecoveryCatalogEntries(
        CatalogPath,
        Entries,
        OutError);
}

FString FOGRecoveryCatalogService::HashFile(
    const FString& FilePath)
{
    const FMD5Hash Hash =
        FMD5Hash::HashFile(
            *FilePath);
    return LexToString(Hash);
}


bool FOGRecoveryCatalogService::ExportWorldBackup(
    const FString& WorldDatabasePath,
    const FString& DestinationBackupPath,
    const FString& CatalogPath,
    const FString& WorldIdentity,
    const FString& SourceBuildVersion,
    FString& OutBackupId,
    FString& OutError)
{
    OutBackupId.Reset();
    OutError.Reset();

    if (WorldDatabasePath.IsEmpty() ||
        DestinationBackupPath.IsEmpty() ||
        CatalogPath.IsEmpty() ||
        WorldIdentity.IsEmpty() ||
        SourceBuildVersion.IsEmpty())
    {
        OutError =
            TEXT("Manual world export requires source, destination, catalog, world identity and build version.");
        return false;
    }

    IFileManager& Files = IFileManager::Get();
    if (!Files.FileExists(*WorldDatabasePath))
    {
        OutError =
            TEXT("Canonical world database does not exist.");
        return false;
    }

    const FString DestinationDirectory =
        FPaths::GetPath(DestinationBackupPath);
    if (!DestinationDirectory.IsEmpty() &&
        !Files.MakeDirectory(
            *DestinationDirectory,
            true) &&
        !Files.DirectoryExists(
            *DestinationDirectory))
    {
        OutError =
            TEXT("Failed to create manual export directory.");
        return false;
    }

    FOGSQLiteWorldStore Store;
    if (!Store.Open(
            WorldDatabasePath,
            OutError))
    {
        return false;
    }

    FString SchemaError;
    const int32 SchemaVersion =
        Store.GetSchemaVersion(SchemaError);
    if (!SchemaError.IsEmpty() ||
        SchemaVersion <= 0)
    {
        Store.Close();
        OutError =
            SchemaError.IsEmpty()
                ? TEXT("Manual export could not determine source schema version.")
                : SchemaError;
        return false;
    }

    if (!Store.BackupTo(
            DestinationBackupPath,
            OutError))
    {
        Store.Close();
        return false;
    }
    Store.Close();

    const FString Hash =
        HashFile(DestinationBackupPath);
    if (Hash.IsEmpty())
    {
        OutError =
            TEXT("Manual export backup hash could not be computed.");
        return false;
    }

    OutBackupId =
        FString::Printf(
            TEXT("manual_export:%s"),
            *FGuid::NewGuid().ToString(
                EGuidFormats::DigitsWithHyphensLower));

    FOGBackupCatalogEntry Entry;
    Entry.BackupId = OutBackupId;
    Entry.BackupPathOrUri =
        DestinationBackupPath;
    Entry.SchemaVersion =
        SchemaVersion;
    Entry.WorldIdentity =
        WorldIdentity;
    Entry.CreatedUtc =
        FDateTime::UtcNow().ToIso8601();
    Entry.ContentHash =
        Hash;
    Entry.SourceBuildVersion =
        SourceBuildVersion;
    Entry.ValidationState =
        FName(TEXT("validated"));

    if (!AddOrUpdateEntry(
            CatalogPath,
            Entry,
            OutError))
    {
        return false;
    }

    return true;
}

bool FOGRecoveryCatalogService::ImportWorldBackup(
    const FString& SourceBackupPath,
    const FString& WorldDatabasePath,
    const FString& ProtectedArchiveDirectory,
    const FString& CatalogPath,
    const FString& WorldIdentity,
    const FString& SourceBuildVersion,
    FString& OutPreservedWorldPath,
    FString& OutError)
{
    OutPreservedWorldPath.Reset();
    OutError.Reset();

    if (SourceBackupPath.IsEmpty() ||
        WorldDatabasePath.IsEmpty() ||
        ProtectedArchiveDirectory.IsEmpty() ||
        CatalogPath.IsEmpty() ||
        WorldIdentity.IsEmpty() ||
        SourceBuildVersion.IsEmpty())
    {
        OutError =
            TEXT("Manual world import requires source, destination, protected archive, catalog, world identity and build version.");
        return false;
    }

    FString SourceFull =
        FPaths::ConvertRelativePathToFull(
            SourceBackupPath);
    FString TargetFull =
        FPaths::ConvertRelativePathToFull(
            WorldDatabasePath);
    FPaths::NormalizeFilename(SourceFull);
    FPaths::NormalizeFilename(TargetFull);
    if (SourceFull.Equals(
            TargetFull,
            ESearchCase::IgnoreCase))
    {
        OutError =
            TEXT("Import source must not be the canonical world database itself.");
        return false;
    }

    IFileManager& Files =
        IFileManager::Get();
    if (!Files.FileExists(
            *SourceBackupPath))
    {
        OutError =
            TEXT("Import source backup does not exist.");
        return false;
    }

    const FString TargetDirectory =
        FPaths::GetPath(
            WorldDatabasePath);
    if (!TargetDirectory.IsEmpty() &&
        !Files.MakeDirectory(
            *TargetDirectory,
            true) &&
        !Files.DirectoryExists(
            *TargetDirectory))
    {
        OutError =
            TEXT("Failed to create canonical world directory for import.");
        return false;
    }

    if (!Files.MakeDirectory(
            *ProtectedArchiveDirectory,
            true) &&
        !Files.DirectoryExists(
            *ProtectedArchiveDirectory))
    {
        OutError =
            TEXT("Failed to create protected recovery archive directory.");
        return false;
    }

    const FString ImportSessionId =
        FGuid::NewGuid().ToString(
            EGuidFormats::DigitsWithHyphensLower);
    const FString StagingDirectory =
        FPaths::Combine(
            TargetDirectory,
            TEXT("ImportStaging"),
            ImportSessionId);
    if (!Files.MakeDirectory(
            *StagingDirectory,
            true))
    {
        OutError =
            TEXT("Failed to create isolated import staging directory.");
        return false;
    }

    const FString StagingDatabasePath =
        FPaths::Combine(
            StagingDirectory,
            TEXT("ImportedWorld.db"));

    auto CleanupStaging =
        [&Files, &StagingDirectory]()
        {
            Files.DeleteDirectory(
                *StagingDirectory,
                false,
                true);
        };

    if (Files.Copy(
            *StagingDatabasePath,
            *SourceBackupPath,
            true,
            true) != COPY_OK)
    {
        CleanupStaging();
        OutError =
            TEXT("Failed to copy import source into isolated staging.");
        return false;
    }

    FOGWorldBootstrapResult ImportBootstrap;
    if (!FOGWorldBootstrap::PrepareWorld(
            StagingDatabasePath,
            ImportBootstrap,
            OutError))
    {
        CleanupStaging();
        return false;
    }

    FOGSQLiteWorldStore StagedStore;
    if (!StagedStore.Open(
            StagingDatabasePath,
            OutError))
    {
        CleanupStaging();
        return false;
    }

    FString StagedSchemaError;
    const int32 ImportedSchemaVersion =
        StagedStore.GetSchemaVersion(
            StagedSchemaError);
    StagedStore.Close();
    if (!StagedSchemaError.IsEmpty() ||
        ImportedSchemaVersion <= 0)
    {
        CleanupStaging();
        OutError =
            StagedSchemaError.IsEmpty()
                ? TEXT("Validated import has an invalid schema version.")
                : StagedSchemaError;
        return false;
    }

    FString PreservedBackupId;
    if (Files.FileExists(
            *WorldDatabasePath))
    {
        const FString Timestamp =
            FDateTime::UtcNow().ToString(
                TEXT("%Y%m%dT%H%M%SZ"));
        OutPreservedWorldPath =
            FPaths::Combine(
                ProtectedArchiveDirectory,
                FString::Printf(
                    TEXT("pre_import_%s_%s.db"),
                    *Timestamp,
                    *ImportSessionId.Left(8)));

        FOGSQLiteWorldStore ExistingStore;
        if (!ExistingStore.Open(
                WorldDatabasePath,
                OutError))
        {
            CleanupStaging();
            return false;
        }

        FString ExistingSchemaError;
        const int32 ExistingSchemaVersion =
            ExistingStore.GetSchemaVersion(
                ExistingSchemaError);
        if (!ExistingSchemaError.IsEmpty() ||
            ExistingSchemaVersion <= 0 ||
            !ExistingStore.BackupTo(
                OutPreservedWorldPath,
                OutError))
        {
            ExistingStore.Close();
            CleanupStaging();
            if (OutError.IsEmpty())
            {
                OutError =
                    ExistingSchemaError.IsEmpty()
                        ? TEXT("Failed to preserve canonical world before import.")
                        : ExistingSchemaError;
            }
            return false;
        }
        ExistingStore.Close();

        PreservedBackupId =
            FString::Printf(
                TEXT("pre_import:%s"),
                *ImportSessionId);

        FOGBackupCatalogEntry PreservedEntry;
        PreservedEntry.BackupId =
            PreservedBackupId;
        PreservedEntry.BackupPathOrUri =
            OutPreservedWorldPath;
        PreservedEntry.SchemaVersion =
            ExistingSchemaVersion;
        PreservedEntry.WorldIdentity =
            WorldIdentity;
        PreservedEntry.CreatedUtc =
            FDateTime::UtcNow().ToIso8601();
        PreservedEntry.ContentHash =
            HashFile(
                OutPreservedWorldPath);
        PreservedEntry.SourceBuildVersion =
            SourceBuildVersion;
        PreservedEntry.ValidationState =
            FName(TEXT("validated"));

        if (PreservedEntry.ContentHash.IsEmpty() ||
            !AddOrUpdateEntry(
                CatalogPath,
                PreservedEntry,
                OutError))
        {
            CleanupStaging();
            return false;
        }
    }

    FOGSQLiteWorldStore TargetStore;
    if (!TargetStore.Open(
            WorldDatabasePath,
            OutError))
    {
        CleanupStaging();
        return false;
    }

    if (!TargetStore.RestoreFrom(
            StagingDatabasePath,
            OutError))
    {
        TargetStore.Close();
        CleanupStaging();
        return false;
    }

    FString CheckpointError;
    if (!TargetStore.Checkpoint(
            CheckpointError))
    {
        TargetStore.Close();
        CleanupStaging();
        OutError =
            FString::Printf(
                TEXT("Imported world restored but checkpoint failed: %s"),
                *CheckpointError);
        return false;
    }
    TargetStore.Close();

    FOGWorldBootstrapResult FinalBootstrap;
    if (!FOGWorldBootstrap::PrepareWorld(
            WorldDatabasePath,
            FinalBootstrap,
            OutError))
    {
        if (!OutPreservedWorldPath.IsEmpty() &&
            Files.FileExists(
                *OutPreservedWorldPath))
        {
            FString RollbackError;
            FOGSQLiteWorldStore RollbackStore;
            if (RollbackStore.Open(
                    WorldDatabasePath,
                    RollbackError))
            {
                RollbackStore.RestoreFrom(
                    OutPreservedWorldPath,
                    RollbackError);
                RollbackStore.Checkpoint(
                    RollbackError);
                RollbackStore.Close();
            }
        }
        else
        {
            Files.Delete(
                *WorldDatabasePath,
                false,
                true,
                true);
        }

        CleanupStaging();
        return false;
    }

    FOGBackupCatalogEntry ImportedEntry;
    ImportedEntry.BackupId =
        FString::Printf(
            TEXT("import_source:%s"),
            *ImportSessionId);
    ImportedEntry.BackupPathOrUri =
        SourceBackupPath;
    ImportedEntry.SchemaVersion =
        ImportedSchemaVersion;
    ImportedEntry.WorldIdentity =
        WorldIdentity;
    ImportedEntry.CreatedUtc =
        FDateTime::UtcNow().ToIso8601();
    ImportedEntry.ContentHash =
        HashFile(
            SourceBackupPath);
    ImportedEntry.SourceBuildVersion =
        SourceBuildVersion;
    ImportedEntry.ValidationState =
        FName(TEXT("validated"));

    if (ImportedEntry.ContentHash.IsEmpty() ||
        !AddOrUpdateEntry(
            CatalogPath,
            ImportedEntry,
            OutError))
    {
        CleanupStaging();
        return false;
    }

    CleanupStaging();
    return true;
}

bool FOGRecoveryCatalogService::ClearWorld(
    const FString& WorldDatabasePath,
    const FString& CatalogPath,
    const FString& BackupDirectory,
    bool bDeleteBackups,
    FString& OutError)
{
    OutError.Reset();

    if (WorldDatabasePath.IsEmpty())
    {
        OutError =
            TEXT("World database path is empty.");
        return false;
    }

    IFileManager& Files =
        IFileManager::Get();

    bool bOk = true;
    if (Files.FileExists(
            *WorldDatabasePath))
    {
        bOk &=
            Files.Delete(
                *WorldDatabasePath,
                false,
                true,
                true);
    }

    Files.Delete(
        *(WorldDatabasePath + TEXT("-wal")),
        false,
        true,
        true);
    Files.Delete(
        *(WorldDatabasePath + TEXT("-shm")),
        false,
        true,
        true);

    if (bDeleteBackups)
    {
        if (!BackupDirectory.IsEmpty() &&
            Files.DirectoryExists(
                *BackupDirectory))
        {
            bOk &=
                Files.DeleteDirectory(
                    *BackupDirectory,
                    false,
                    true);
        }

        if (!CatalogPath.IsEmpty() &&
            Files.FileExists(
                *CatalogPath))
        {
            bOk &=
                Files.Delete(
                    *CatalogPath,
                    false,
                    true,
                    true);
        }
    }

    if (!bOk)
    {
        OutError =
            TEXT("One or more requested world-clear file operations failed.");
    }

    return bOk;
}
