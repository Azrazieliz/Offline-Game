#include "Runtime/OGPackageManagerService.h"
#include "Runtime/OGLocallyInstalledPackageProvider.h"

#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/Paths.h"
#include "Serialization/Archive.h"

bool FOGPackageManagerService::RegisterPackage(
    const FOGContentPackageRecord& Package,
    FString& OutError)
{
    FOGContentPackageRecord Registered = Package;
    // Registration/update cannot bypass dependency-aware activation.
    Registered.bActivated = false;

    if (!Store.BeginTransaction(
            OutError))
    {
        return false;
    }

    TSet<FOGContentId> Visited;
    if (!Store.UpsertContentPackageRecord(
            Registered,
            OutError) ||
        !DeactivatePackageAndDependents(
            Registered.PackageId,
            Visited,
            OutError))
    {
        FString RollbackError;
        Store.RollbackTransaction(
            RollbackError);
        return false;
    }

    if (!Store.CommitTransaction(
            OutError))
    {
        FString RollbackError;
        Store.RollbackTransaction(
            RollbackError);
        return false;
    }

    return true;
}

bool FOGPackageManagerService::SetDependency(
    const FOGPackageDependencyRecord& Dependency,
    FString& OutError)
{
    if (!Store.BeginTransaction(
            OutError))
    {
        return false;
    }

    if (!Store.UpsertPackageDependency(
            Dependency,
            OutError) ||
        !ValidateDependencyGraph(
            OutError))
    {
        FString RollbackError;
        Store.RollbackTransaction(
            RollbackError);
        return false;
    }

    // Changing dependency semantics invalidates any prior activation proof for
    // this package and every package above it in the dependency graph.
    TSet<FOGContentId> Visited;
    if (!DeactivatePackageAndDependents(
            Dependency.PackageId,
            Visited,
            OutError))
    {
        FString RollbackError;
        Store.RollbackTransaction(
            RollbackError);
        return false;
    }

    if (!Store.CommitTransaction(
            OutError))
    {
        FString RollbackError;
        Store.RollbackTransaction(
            RollbackError);
        return false;
    }

    return true;
}

bool FOGPackageManagerService::MovePackageStorage(
    const FOGContentId& PackageId,
    const FString& DestinationRoot,
    FName DestinationStorageClass,
    FString& OutNewInstallUri,
    FString& OutError)
{
    OutNewInstallUri.Reset();
    OutError.Reset();

    if (!PackageId.IsValid() ||
        DestinationRoot.IsEmpty() ||
        DestinationStorageClass.IsNone())
    {
        OutError = TEXT("Package move requires a valid package, destination and storage class.");
        return false;
    }

    bool bFound = false;
    FOGContentPackageRecord Package;
    if (!Store.TryReadContentPackageRecord(
            PackageId,
            bFound,
            Package,
            OutError))
    {
        return false;
    }
    if (!bFound)
    {
        OutError = TEXT("Package move target is unknown.");
        return false;
    }
    if (!Package.bInstalled || !Package.bValidated)
    {
        OutError = TEXT("Only installed, validated packages can be moved.");
        return false;
    }
    if (Package.bActivated)
    {
        OutError = TEXT("Deactivate the package before moving its files.");
        return false;
    }
    if (Package.InstallUri.IsEmpty())
    {
        OutError = TEXT("Package has no movable install location.");
        return false;
    }

    IFileManager& FileManager = IFileManager::Get();
    IPlatformFile& PlatformFile =
        FPlatformFileManager::Get().GetPlatformFile();

    const bool bSourceFile =
        FileManager.FileExists(*Package.InstallUri);
    const bool bSourceDirectory =
        FileManager.DirectoryExists(*Package.InstallUri);
    if (!bSourceFile && !bSourceDirectory)
    {
        OutError = TEXT("Package install location no longer exists.");
        return false;
    }

    const FString SourcePath = FPaths::ConvertRelativePathToFull(Package.InstallUri);
    FString Leaf =
        FPaths::GetCleanFilename(Package.InstallUri);
    if (Leaf.IsEmpty())
    {
        Leaf = FPaths::MakeValidFileName(PackageId.ToString());
    }

    const FString DestinationPath = FPaths::ConvertRelativePathToFull(
        FPaths::Combine(DestinationRoot, Leaf));
    if (FPaths::IsSamePath(SourcePath, DestinationPath))
    {
        OutNewInstallUri = Package.InstallUri;
        return true;
    }

    // Never merge into or replace an existing destination. A directory cannot
    // be copied into itself (or an ancestor that would subsequently be deleted).
    if (FileManager.FileExists(*DestinationPath) ||
        FileManager.DirectoryExists(*DestinationPath))
    {
        OutError = TEXT("Package move destination already exists; existing files were preserved.");
        return false;
    }
    if (bSourceDirectory &&
        (FPaths::IsUnderDirectory(DestinationPath, SourcePath) ||
         FPaths::IsUnderDirectory(SourcePath, DestinationPath)))
    {
        OutError = TEXT("Package directory move cannot use a destination nested inside or around its source.");
        return false;
    }
    if (!PlatformFile.CreateDirectoryTree(*FPaths::GetPath(DestinationPath)))
    {
        OutError = TEXT("Unable to create the destination package directory.");
        return false;
    }

    const auto RemoveOwnedCopy = [&]()
    {
        const bool bRemoved = bSourceFile
            ? (!FileManager.FileExists(*DestinationPath) ||
               PlatformFile.DeleteFile(*DestinationPath))
            : (!FileManager.DirectoryExists(*DestinationPath) ||
               PlatformFile.DeleteDirectoryRecursively(*DestinationPath));
        if (!bRemoved)
        {
            OutError += FString::Printf(
                TEXT(" Unreferenced copied files remain at '%s'; original storage is intact."),
                *DestinationPath);
        }
    };

    // Keep original storage untouched until a byte-verified copy is accepted by
    // the canonical store. Copy failure must never strand its old install URI.
    const bool bCopied = bSourceFile
        ? FileManager.Copy(*DestinationPath, *SourcePath, false, false) == COPY_OK
        : PlatformFile.CopyDirectoryTree(*DestinationPath, *SourcePath, false);
    if (!bCopied)
    {
        // A failed copy may also mean another writer acquired the destination
        // after the existence check. Never delete that unproven destination.
        OutError = FString::Printf(
            TEXT("Package copy failed; original storage remains authoritative. Inspect any partial destination at '%s' before cleanup."),
            *DestinationPath);
        return false;
    }

    const auto VerifyFileCopy = [&](const FString& From, const FString& To)
    {
        TUniquePtr<FArchive> Original(FileManager.CreateFileReader(*From));
        TUniquePtr<FArchive> Copied(FileManager.CreateFileReader(*To));
        if (!Original || !Copied || Original->TotalSize() < 0 ||
            Original->TotalSize() != Copied->TotalSize())
        {
            return false;
        }
        TArray<uint8> OriginalBytes;
        TArray<uint8> CopiedBytes;
        OriginalBytes.SetNumUninitialized(64 * 1024);
        CopiedBytes.SetNumUninitialized(64 * 1024);
        int64 Remaining = Original->TotalSize();
        while (Remaining > 0)
        {
            const int64 Chunk = FMath::Min<int64>(Remaining, OriginalBytes.Num());
            Original->Serialize(OriginalBytes.GetData(), Chunk);
            Copied->Serialize(CopiedBytes.GetData(), Chunk);
            if (Original->IsError() || Copied->IsError() ||
                FMemory::Memcmp(OriginalBytes.GetData(), CopiedBytes.GetData(), Chunk) != 0)
            {
                return false;
            }
            Remaining -= Chunk;
        }
        return !Original->IsError() && !Copied->IsError();
    };
    bool bVerified = bSourceFile
        ? VerifyFileCopy(SourcePath, DestinationPath)
        : PlatformFile.DirectoryExists(*DestinationPath);
    if (bVerified && bSourceDirectory)
    {
        FString SourceRoot = SourcePath;
        FPaths::NormalizeDirectoryName(SourceRoot);
        SourceRoot += TEXT("/");
        bVerified = PlatformFile.IterateDirectoryRecursively(*SourcePath,
            [&](const TCHAR* Entry, bool bIsDirectory)
            {
                FString RelativePath = FPaths::ConvertRelativePathToFull(Entry);
                if (!FPaths::MakePathRelativeTo(RelativePath, *SourceRoot))
                {
                    return false;
                }
                const FString CopiedPath = FPaths::Combine(DestinationPath, RelativePath);
                return bIsDirectory
                    ? PlatformFile.DirectoryExists(*CopiedPath)
                    : VerifyFileCopy(Entry, CopiedPath);
            });
    }
    if (!bVerified)
    {
        OutError = TEXT("Package copy verification failed; original storage remains authoritative.");
        RemoveOwnedCopy();
        return false;
    }

    // Native receipts are outside the importable world DB. Preserve their authority through
    // this existing storage operation; legacy/unreceipted packages gain no installation trust.
    FOGOptionalPackageStorageReceiptMove ReceiptMove;
    if (!FOGLocallyInstalledPackageProvider::PrepareStorageMove(
            Package, DestinationPath, FPaths::ConvertRelativePathToFull(DestinationRoot), ReceiptMove, OutError) ||
        !FOGLocallyInstalledPackageProvider::PublishStorageMove(ReceiptMove, OutError))
    {
        RemoveOwnedCopy();
        return false;
    }
    Package.InstallUri = DestinationPath;
    Package.StorageClass = DestinationStorageClass;
    if (!Store.UpsertContentPackageRecord(Package, OutError))
    {
        FString ReceiptError;
        if (!FOGLocallyInstalledPackageProvider::RollbackStorageMove(ReceiptMove, ReceiptError))
            OutError += TEXT(" Native receipt rollback failed (original bytes retained): ") + ReceiptError;
        RemoveOwnedCopy();
        return false;
    }

    OutNewInstallUri = DestinationPath;
    const bool bRemovedOriginal = bSourceFile
        ? PlatformFile.DeleteFile(*SourcePath)
        : PlatformFile.DeleteDirectoryRecursively(*SourcePath);
    if (!bRemovedOriginal)
    {
        // The verified destination is already canonical. Do not roll back to a
        // source directory that may have been only partially removed.
        OutError = FString::Printf(
            TEXT("Package is now stored at '%s', but old-storage cleanup at '%s' is incomplete. Preserve the new location; remove only the old remnants when safe."),
            *DestinationPath, *SourcePath);
        return false;
    }
    return true;
}

bool FOGPackageManagerService::ValidateDependencyGraph(
    FString& OutError) const
{
    return ValidateDependencyGraphInternal(
        nullptr,
        OutError);
}

bool FOGPackageManagerService::ValidateDependencyGraphInternal(
    const FOGContentId* ActivationTarget,
    FString& OutError) const
{
    OutError.Reset();

    TArray<FOGContentPackageRecord> Packages;
    if (!Store.ListContentPackageRecords(
            Packages,
            OutError))
    {
        return false;
    }

    TMap<FOGContentId, FOGContentPackageRecord> ById;
    for (const FOGContentPackageRecord& Package :
         Packages)
    {
        ById.Add(
            Package.PackageId,
            Package);
    }

    TMap<FOGContentId, int32> VisitState;
    TFunction<bool(const FOGContentId&)> VisitForCycle =
        [&](const FOGContentId& PackageId)
        {
            const int32* ExistingState =
                VisitState.Find(PackageId);
            if (ExistingState && *ExistingState == 1)
            {
                OutError = FString::Printf(
                    TEXT("Package dependency cycle detected at %s."),
                    *PackageId.ToString());
                return false;
            }
            if (ExistingState && *ExistingState == 2)
            {
                return true;
            }

            if (!ById.Contains(PackageId))
            {
                OutError = FString::Printf(
                    TEXT("Package dependency graph references unknown package %s."),
                    *PackageId.ToString());
                return false;
            }

            VisitState.Add(PackageId, 1);

            TArray<FOGPackageDependencyRecord> Dependencies;
            if (!Store.ListPackageDependencies(
                    PackageId,
                    Dependencies,
                    OutError))
            {
                return false;
            }

            for (const FOGPackageDependencyRecord& Dependency :
                 Dependencies)
            {
                const FOGContentPackageRecord* Required =
                    ById.Find(Dependency.DependencyPackageId);
                if (!Required)
                {
                    OutError = FString::Printf(
                        TEXT("Package %s depends on missing package %s."),
                        *PackageId.ToString(),
                        *Dependency.DependencyPackageId.ToString());
                    return false;
                }

                if (Required->Version < Dependency.MinimumVersion)
                {
                    OutError = FString::Printf(
                        TEXT("Package %s requires %s version %d or newer; installed version is %d."),
                        *PackageId.ToString(),
                        *Dependency.DependencyPackageId.ToString(),
                        Dependency.MinimumVersion,
                        Required->Version);
                    return false;
                }

                if (!VisitForCycle(
                        Dependency.DependencyPackageId))
                {
                    return false;
                }
            }

            VisitState.Add(PackageId, 2);
            return true;
        };

    for (const FOGContentPackageRecord& Package :
         Packages)
    {
        if (!VisitForCycle(
                Package.PackageId))
        {
            return false;
        }
    }

    if (!ActivationTarget)
    {
        return true;
    }

    const FOGContentPackageRecord* Target =
        ById.Find(*ActivationTarget);
    if (!Target)
    {
        OutError =
            TEXT("Package activation target is unknown.");
        return false;
    }

    if (!Target->bInstalled ||
        !Target->bValidated ||
        Target->DownloadState != FName(TEXT("installed")))
    {
        OutError =
            TEXT("Package must be installed, downloaded and validated before activation.");
        return false;
    }

    TSet<FOGContentId> ReadyVisited;
    TFunction<bool(const FOGContentId&)> ValidateReady =
        [&](const FOGContentId& PackageId)
        {
            if (ReadyVisited.Contains(PackageId))
            {
                return true;
            }
            ReadyVisited.Add(PackageId);

            TArray<FOGPackageDependencyRecord> Dependencies;
            if (!Store.ListPackageDependencies(
                    PackageId,
                    Dependencies,
                    OutError))
            {
                return false;
            }

            for (const FOGPackageDependencyRecord& Dependency :
                 Dependencies)
            {
                const FOGContentPackageRecord* Required =
                    ById.Find(Dependency.DependencyPackageId);
                if (!Required ||
                    Required->Version < Dependency.MinimumVersion ||
                    !Required->bInstalled ||
                    !Required->bValidated ||
                    !Required->bActivated)
                {
                    OutError = FString::Printf(
                        TEXT("Package %s dependency %s is not activation-ready at required version %d."),
                        *PackageId.ToString(),
                        *Dependency.DependencyPackageId.ToString(),
                        Dependency.MinimumVersion);
                    return false;
                }

                if (!ValidateReady(
                        Dependency.DependencyPackageId))
                {
                    return false;
                }
            }
            return true;
        };

    return ValidateReady(
        *ActivationTarget);
}

bool FOGPackageManagerService::DeactivatePackageAndDependents(
    const FOGContentId& PackageId,
    TSet<FOGContentId>& Visited,
    FString& OutError)
{
    if (!PackageId.IsValid())
    {
        OutError =
            TEXT("Package deactivation requires a valid ID.");
        return false;
    }

    if (Visited.Contains(
            PackageId))
    {
        return true;
    }
    Visited.Add(
        PackageId);

    TArray<FOGContentPackageRecord> Packages;
    if (!Store.ListContentPackageRecords(
            Packages,
            OutError))
    {
        return false;
    }

    for (const FOGContentPackageRecord& Candidate :
         Packages)
    {
        if (Candidate.PackageId ==
            PackageId)
        {
            continue;
        }

        TArray<FOGPackageDependencyRecord> Dependencies;
        if (!Store.ListPackageDependencies(
                Candidate.PackageId,
                Dependencies,
                OutError))
        {
            return false;
        }

        const bool bDependsOnTarget =
            Dependencies.ContainsByPredicate(
                [&PackageId](
                    const FOGPackageDependencyRecord& Dependency)
                {
                    return Dependency.DependencyPackageId ==
                        PackageId;
                });

        if (bDependsOnTarget &&
            !DeactivatePackageAndDependents(
                Candidate.PackageId,
                Visited,
                OutError))
        {
            return false;
        }
    }

    return Store.SetContentPackageActivated(
        PackageId,
        false,
        OutError);
}

bool FOGPackageManagerService::ActivatePackage(
    const FOGContentId& PackageId,
    FString& OutError)
{
    if (!PackageId.IsValid())
    {
        OutError =
            TEXT("Package activation requires a valid ID.");
        return false;
    }

    if (!ValidateDependencyGraphInternal(
            &PackageId,
            OutError))
    {
        return false;
    }

    return Store.SetContentPackageActivated(
        PackageId,
        true,
        OutError);
}

bool FOGPackageManagerService::DeactivatePackage(
    const FOGContentId& PackageId,
    FString& OutError)
{
    if (!Store.BeginTransaction(
            OutError))
    {
        return false;
    }

    TSet<FOGContentId> Visited;
    if (!DeactivatePackageAndDependents(
            PackageId,
            Visited,
            OutError))
    {
        FString RollbackError;
        Store.RollbackTransaction(
            RollbackError);
        return false;
    }

    if (!Store.CommitTransaction(
            OutError))
    {
        FString RollbackError;
        Store.RollbackTransaction(
            RollbackError);
        return false;
    }

    return true;
}
