#include "Persistence/OGSnapshotService.h"
#include "Persistence/OGRecoveryCatalogService.h"

#include "HAL/FileManager.h"
#include "Misc/Paths.h"

bool FOGSnapshotService::CreateRotatingSnapshot(
    IOGWorldStore& Store,
    const FString& SnapshotDirectory,
    int32 KeepCount,
    FString& OutSnapshotPath,
    FString& OutError)
{
    return CreateRotatingSnapshot(
        Store,
        SnapshotDirectory,
        KeepCount,
        FString(),
        FString(),
        FString(),
        OutSnapshotPath,
        OutError);
}

bool FOGSnapshotService::CreateRotatingSnapshot(
    IOGWorldStore& Store,
    const FString& SnapshotDirectory,
    int32 KeepCount,
    const FString& RecoveryCatalogPath,
    const FString& WorldIdentity,
    const FString& SourceBuildVersion,
    FString& OutSnapshotPath,
    FString& OutError)
{
    OutSnapshotPath.Reset();
    OutError.Reset();

    if (!Store.IsOpen())
    {
        OutError = TEXT("Cannot snapshot a closed world store.");
        return false;
    }

    if (SnapshotDirectory.IsEmpty() || KeepCount < 1)
    {
        OutError = TEXT("Snapshot directory must be set and KeepCount must be positive.");
        return false;
    }

    IFileManager& Files = IFileManager::Get();
    if (!Files.MakeDirectory(*SnapshotDirectory, true) &&
        !Files.DirectoryExists(*SnapshotDirectory))
    {
        OutError = FString::Printf(
            TEXT("Failed to create snapshot directory: %s"),
            *SnapshotDirectory);
        return false;
    }

    const FString Timestamp =
        FDateTime::UtcNow().ToString(TEXT("%Y%m%dT%H%M%S"));
    const FString Suffix =
        FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8);

    OutSnapshotPath = FPaths::Combine(
        SnapshotDirectory,
        FString::Printf(
            TEXT("world_%s_%s.db"),
            *Timestamp,
            *Suffix));

    if (!Store.BackupTo(OutSnapshotPath, OutError))
    {
        OutSnapshotPath.Reset();
        return false;
    }

    TArray<FString> SnapshotFiles;
    Files.FindFiles(
        SnapshotFiles,
        *FPaths::Combine(SnapshotDirectory, TEXT("world_*.db")),
        true,
        false);

    SnapshotFiles.Sort(
        [](const FString& A, const FString& B)
        {
            return A > B;
        });

    for (int32 Index = KeepCount; Index < SnapshotFiles.Num(); ++Index)
    {
        const FString ObsoletePath =
            FPaths::Combine(SnapshotDirectory, SnapshotFiles[Index]);
        Files.Delete(*ObsoletePath, false, true, true);
    }

    if (!RecoveryCatalogPath.IsEmpty())
    {
        FString SchemaError;
        const int32 SchemaVersion =
            Store.GetSchemaVersion(SchemaError);
        if (SchemaVersion < 0)
        {
            OutError = FString::Printf(
                TEXT("Snapshot created but schema version could not be cataloged: %s"),
                *SchemaError);
            return false;
        }

        FOGBackupCatalogEntry Entry;
        Entry.BackupId =
            FPaths::GetBaseFilename(
                OutSnapshotPath);
        Entry.BackupPathOrUri =
            OutSnapshotPath;
        Entry.SchemaVersion =
            SchemaVersion;
        Entry.WorldIdentity =
            WorldIdentity.IsEmpty()
                ? TEXT("offlinegame:canonical_world")
                : WorldIdentity;
        Entry.CreatedUtc =
            FDateTime::UtcNow().ToIso8601();
        Entry.ContentHash =
            FOGRecoveryCatalogService::HashFile(
                OutSnapshotPath);
        Entry.SourceBuildVersion =
            SourceBuildVersion.IsEmpty()
                ? TEXT("unknown")
                : SourceBuildVersion;
        Entry.ValidationState =
            FName(TEXT("validated"));

        if (Entry.ContentHash.IsEmpty() ||
            !FOGRecoveryCatalogService::AddOrUpdateEntry(
                RecoveryCatalogPath,
                Entry,
                OutError))
        {
            return false;
        }
    }

    return true;
}
