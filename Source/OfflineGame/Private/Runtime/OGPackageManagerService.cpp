#include "Runtime/OGPackageManagerService.h"

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
