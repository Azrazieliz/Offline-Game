#pragma once
#include "CoreMinimal.h"
#include "Content/OGContentId.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/StrongObjectPtr.h"

class IOGWorldStore;
class FOGOptionalPackageHost;
struct OFFLINEGAME_API FOGOptionalAssetReference
{
    FOGContentId PackageId;
    FSoftObjectPath AssetPath;
};

/** Optional presentation residency. Never rewrites character/world records. Host outlives this loader. */
class OFFLINEGAME_API FOGOptionalAssetRuntime
{
public:
    explicit FOGOptionalAssetRuntime(IOGWorldStore& InStore, FOGOptionalPackageHost* InHost = nullptr)
        : Store(InStore), ContainerHost(InHost) {}
    ~FOGOptionalAssetRuntime();
    bool SetContainerHost(FOGOptionalPackageHost* InHost, FString& OutReason);
    bool IsAvailable(const FOGContentId& PackageId, FString& OutReason) const;
    bool ActivateAndLoad(const FOGOptionalAssetReference& Reference, UObject*& OutAsset, FString& OutReason);
    void Release(const FOGOptionalAssetReference& Reference);
    void ReleaseAll();
private:
    struct FAssetLease { FOGContentId PackageId; FGuid ContainerLease; };
    bool CheckDependencies(const FOGContentId&, TSet<FOGContentId>& Visiting, TSet<FOGContentId>& Complete, bool bRequireActivated, FString&) const;
    IOGWorldStore& Store;
    FOGOptionalPackageHost* ContainerHost = nullptr;
    TMap<FSoftObjectPath, TStrongObjectPtr<UObject>> ResidentAssets;
    TMap<FSoftObjectPath, TArray<FAssetLease>> Leases;
};
