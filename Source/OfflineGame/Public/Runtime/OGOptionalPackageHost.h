#pragma once

#include "CoreMinimal.h"
#include "Runtime/OGPackageReportManagementRecords.h"

class IOGWorldStore;
class IPakFile;

/** Authored host resolution. Package JSON is never itself a mount instruction. */
struct OFFLINEGAME_API FOGOptionalPackageContainer
{
    FString Filename;
    FName Format = FName(TEXT("pak"));
    int32 MountOrder = 0;
    FString VerificationIdentity;
};

struct OFFLINEGAME_API FOGOptionalPackageHostCallbacks
{
    /** Trusted installation authority distinguishes an external container from embedded/base-game content. */
    TFunction<bool(const FOGContentPackageRecord&)> UsesContainer;
    TFunction<bool(const FOGContentPackageRecord&, FOGOptionalPackageContainer&, FString&)> ResolveContainer;
    /** Read/verify actual bytes against trusted installation provenance. Mandatory on every acquire. */
    TFunction<bool(const FOGContentPackageRecord&, const FOGOptionalPackageContainer&, FString&)> VerifyArtifact;
    /** Reject a container already mounted by engine startup or a different owner. */
    TFunction<bool(const FOGOptionalPackageContainer&, FString&)> CanTakeOwnership;
    /** Must account for loaded UObjects, streaming requests and references outside the optional loader. */
    TFunction<bool(const FOGContentId&, FString&)> CanUnload;
    /** Production residency observers retain the actual engine pak so external IO references are visible. */
    TFunction<void(const FOGContentId&, const FOGOptionalPackageContainer&, IPakFile*)> DidMount;
    TFunction<void(const FOGContentId&)> DidUnmount;
};

#if WITH_DEV_AUTOMATION_TESTS
/** Isolated physical-engine seam for regression fixtures; never changes process-global delegates. */
struct FOGOptionalPackageEngineAdapter
{
    TFunction<bool()> IsAvailable;
    TFunction<bool(const FString&, int32)> Mount;
    TFunction<bool(const FString&)> Unmount;
};
#endif

/**
 * Physical optional-content host; canonical package management remains the activation authority.
 * Own one instance per process. Game-thread-only. Release asset references before container leases.
 * UnmountAll must succeed before destroying/replacing the host. No destructor silently unmounts assets.
 * Loose/base-game assets use the existing optional loader and do not require this bridge.
 */
class OFFLINEGAME_API FOGOptionalPackageHost
{
public:
    FOGOptionalPackageHost(IOGWorldStore& InStore, FOGOptionalPackageHostCallbacks InCallbacks);
#if WITH_DEV_AUTOMATION_TESTS
    FOGOptionalPackageHost(IOGWorldStore& InStore, FOGOptionalPackageHostCallbacks InCallbacks,
        FOGOptionalPackageEngineAdapter InTestEngine);
#endif
    bool Acquire(const FOGContentId& PackageId, FGuid& OutLease, FString& OutError);
    bool RequiresContainer(const FOGContentId& PackageId, bool& OutRequired, FString& OutError) const;
    bool EnsureMounted(const FOGContentId& PackageId, FGuid& OutLease, FString& OutError)
    { return Acquire(PackageId, OutLease, OutError); }
    bool Release(const FGuid& Lease, FString& OutError);
    bool Unmount(const FOGContentId& PackageId, FString& OutError);
    bool UnmountAll(FString& OutError);
    bool Reload(const FOGContentId& PackageId, FGuid& OutLease, FString& OutError);
    bool IsMounted(const FOGContentId& PackageId) const;
    bool HasOutstandingLeases() const { return !Leases.IsEmpty(); }
    bool HasMountedContainers() const { return !Mounted.IsEmpty(); }
private:
    struct FMounted
    {
        FOGContentPackageRecord Record;
        FOGOptionalPackageContainer Container;
        TArray<FOGContentId> Dependencies;
        int32 LeaseCount = 0;
    };
    bool BuildClosure(const FOGContentId&, TSet<FOGContentId>& Visiting,
        TSet<FOGContentId>& Complete, TArray<FOGContentPackageRecord>& Ordered, FString&);
    bool ResolveAndVerify(const FOGContentPackageRecord&, FOGOptionalPackageContainer&, FString&);
    void RollbackMounts(const TArray<FOGContentId>& NewlyMounted, FString& InOutError);
    bool BackendAvailable() const;
    bool MountContainer(const FOGContentId&, const FOGOptionalPackageContainer&);
    bool UnmountContainer(const FString&);
#if WITH_DEV_AUTOMATION_TESTS
    TUniquePtr<FOGOptionalPackageEngineAdapter> TestEngine;
#endif
    IOGWorldStore& Store;
    FOGOptionalPackageHostCallbacks Callbacks;
    TMap<FOGContentId, FMounted> Mounted;
    TMap<FGuid, TArray<FOGContentId>> Leases;
    TArray<FOGContentId> MountSequence;
};
