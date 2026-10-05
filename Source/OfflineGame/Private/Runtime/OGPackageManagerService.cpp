#include "Runtime/OGPackageManagerService.h"

bool FOGPackageManagerService::RegisterPackage(
    const FOGContentPackageRecord& Package,
    FString& OutError)
{
    return Store.UpsertContentPackageRecord(
        Package,
        OutError);
}

bool FOGPackageManagerService::SetDependency(
    const FOGPackageDependencyRecord& Dependency,
    FString& OutError)
{
    if (!Store.UpsertPackageDependency(
            Dependency,
            OutError))
    {
        return false;
    }

    if (!ValidateDependencyGraph(
            OutError))
    {
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
    TFunction<bool(const FOGContentId&)> Visit =
        [&](const FOGContentId& PackageId)
        {
            const int32* ExistingState =
                VisitState.Find(PackageId);
            if (ExistingState &&
                *ExistingState == 1)
            {
                OutError = FString::Printf(
                    TEXT("Package dependency cycle detected at %s."),
                    *PackageId.ToString());
                return false;
            }

            if (ExistingState &&
                *ExistingState == 2)
            {
                return true;
            }

            const FOGContentPackageRecord* Package =
                ById.Find(PackageId);
            if (!Package)
            {
                OutError = FString::Printf(
                    TEXT("Package dependency graph references unknown package %s."),
                    *PackageId.ToString());
                return false;
            }

            VisitState.Add(
                PackageId,
                1);

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
                    ById.Find(
                        Dependency.DependencyPackageId);
                if (!Required)
                {
                    OutError = FString::Printf(
                        TEXT("Package %s depends on missing package %s."),
                        *PackageId.ToString(),
                        *Dependency.DependencyPackageId.ToString());
                    return false;
                }

                if (Required->Version <
                    Dependency.MinimumVersion)
                {
                    OutError = FString::Printf(
                        TEXT("Package %s requires %s version %d or newer; installed version is %d."),
                        *PackageId.ToString(),
                        *Dependency.DependencyPackageId.ToString(),
                        Dependency.MinimumVersion,
                        Required->Version);
                    return false;
                }

                if (ActivationTarget &&
                    (!Required->bInstalled ||
                     !Required->bValidated ||
                     !Required->bActivated))
                {
                    OutError = FString::Printf(
                        TEXT("Package %s dependency %s must be installed, validated and activated first."),
                        *PackageId.ToString(),
                        *Dependency.DependencyPackageId.ToString());
                    return false;
                }

                if (!Visit(
                        Dependency.DependencyPackageId))
                {
                    return false;
                }
            }

            VisitState.Add(
                PackageId,
                2);
            return true;
        };

    for (const FOGContentPackageRecord& Package :
         Packages)
    {
        if (!Visit(
                Package.PackageId))
        {
            return false;
        }
    }

    if (ActivationTarget)
    {
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
            Target->DownloadState !=
                FName(TEXT("installed")))
        {
            OutError =
                TEXT("Package must be installed, downloaded and validated before activation.");
            return false;
        }
    }

    return true;
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
    return Store.SetContentPackageActivated(
        PackageId,
        false,
        OutError);
}
