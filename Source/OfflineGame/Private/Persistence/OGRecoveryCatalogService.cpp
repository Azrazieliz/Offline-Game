#include "Persistence/OGRecoveryCatalogService.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
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
    return Hash.ToString();
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
