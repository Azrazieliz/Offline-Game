#pragma once

#include "CoreMinimal.h"
#include "Persistence/OGWorldStore.h"
#include "Runtime/OGPackageReportManagementRecords.h"

/**
 * Content-package lifecycle authority. Activation is dependency-aware and
 * refuses cycles, missing/old dependencies, or dependencies that are not
 * installed, validated and activated.
 */
class OFFLINEGAME_API FOGPackageManagerService
{
public:
    explicit FOGPackageManagerService(IOGWorldStore& InStore)
        : Store(InStore)
    {
    }

    bool RegisterPackage(
        const FOGContentPackageRecord& Package,
        FString& OutError);

    bool SetDependency(
        const FOGPackageDependencyRecord& Dependency,
        FString& OutError);

    bool ActivatePackage(
        const FOGContentId& PackageId,
        FString& OutError);

    bool DeactivatePackage(
        const FOGContentId& PackageId,
        FString& OutError);

    bool MovePackageStorage(
        const FOGContentId& PackageId,
        const FString& DestinationRoot,
        FName DestinationStorageClass,
        FString& OutNewInstallUri,
        FString& OutError);

    bool ValidateDependencyGraph(
        FString& OutError) const;

private:
    bool ValidateDependencyGraphInternal(
        const FOGContentId* ActivationTarget,
        FString& OutError) const;

    bool DeactivatePackageAndDependents(
        const FOGContentId& PackageId,
        TSet<FOGContentId>& Visited,
        FString& OutError);

    IOGWorldStore& Store;
};
