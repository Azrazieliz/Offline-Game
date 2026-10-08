#include "Runtime/OGOptionalAssetRuntime.h"
#include "Persistence/OGWorldStore.h"
#include "Runtime/OGPackageManagerService.h"
#include "Runtime/OGOptionalPackageHost.h"

FOGOptionalAssetRuntime::~FOGOptionalAssetRuntime() { ReleaseAll(); }

bool FOGOptionalAssetRuntime::SetContainerHost(FOGOptionalPackageHost* InHost, FString& Reason)
{
    Reason.Reset();
    if (!Leases.IsEmpty()) { Reason = TEXT("Release optional assets before replacing their container host."); return false; }
    ContainerHost = InHost;
    return true;
}

bool FOGOptionalAssetRuntime::CheckDependencies(const FOGContentId& Id, TSet<FOGContentId>& Visiting,
    TSet<FOGContentId>& Complete, bool bRequireActivated, FString& Reason) const
{
    if (Complete.Contains(Id)) return true;
    if (!Id.IsValid() || Visiting.Contains(Id)) { Reason = TEXT("Optional package ID invalid or dependency cycle."); return false; }
    bool Found = false;
    FOGContentPackageRecord Record;
    if (!Store.TryReadContentPackageRecord(Id, Found, Record, Reason)) return false;
    if (!Found || !Record.bInstalled || !Record.bValidated || Record.DownloadState != FName(TEXT("installed")) ||
        (bRequireActivated && !Record.bActivated))
    { Reason = TEXT("Optional package absent, unvalidated or inactive: ") + Id.ToString(); return false; }
    Visiting.Add(Id);
    TArray<FOGPackageDependencyRecord> Dependencies;
    if (!Store.ListPackageDependencies(Id, Dependencies, Reason)) return false;
    for (const auto& Dependency : Dependencies)
    {
        bool DepFound = false;
        FOGContentPackageRecord Dep;
        if (!Store.TryReadContentPackageRecord(Dependency.DependencyPackageId, DepFound, Dep, Reason)) return false;
        if (!DepFound || Dep.Version < Dependency.MinimumVersion)
        { Reason = TEXT("Optional dependency unavailable or too old."); return false; }
        if (!CheckDependencies(Dependency.DependencyPackageId, Visiting, Complete, bRequireActivated, Reason)) return false;
    }
    Visiting.Remove(Id); Complete.Add(Id); return true;
}

bool FOGOptionalAssetRuntime::IsAvailable(const FOGContentId& Id, FString& Reason) const
{
    TSet<FOGContentId> Visiting, Complete;
    // This is logical availability. ActivateAndLoad separately proves actual artifact residency.
    return CheckDependencies(Id, Visiting, Complete, true, Reason);
}

bool FOGOptionalAssetRuntime::ActivateAndLoad(const FOGOptionalAssetReference& Ref, UObject*& Asset, FString& Reason)
{
    Asset = nullptr;
    Reason.Reset();
    if (!IsInGameThread()) { Reason = TEXT("Optional asset loading requires the game thread."); return false; }
    if (!Ref.AssetPath.IsValid()) { Reason = TEXT("Optional asset path absent."); return false; }
    TSet<FOGContentId> Visiting, Complete;
    if (!CheckDependencies(Ref.PackageId, Visiting, Complete, false, Reason)) return false;
    bool bNeedsHost = false;
    if (ContainerHost)
    {
        for (const FOGContentId& Id : Complete)
        {
            bool Required = false;
            if (!ContainerHost->RequiresContainer(Id, Required, Reason)) return false;
            bNeedsHost |= Required;
        }
    }
    if (!ContainerHost)
    {
        // An absent host cannot reclassify imported external-package metadata as embedded content.
        for (const FOGContentId& Id : Complete)
        {
            bool Found = false;
            FOGContentPackageRecord Record;
            if (!Store.TryReadContentPackageRecord(Id, Found, Record, Reason)) return false;
            if (!Found || (!Record.InstallUri.IsEmpty() && !Record.InstallUri.StartsWith(TEXT("embedded:"), ESearchCase::CaseSensitive)))
            { Reason = TEXT("External optional packages require the configured trusted container host."); return false; }
        }
    }
    FGuid ContainerLease;
    if (bNeedsHost)
    {
        if (!ContainerHost->EnsureMounted(Ref.PackageId, ContainerLease, Reason)) return false;
    }
    else
    {
        // Embedded assets retain the existing dependency-aware package authority.
        TSet<FOGContentId> Activated;
        TFunction<bool(const FOGContentId&)> Activate = [&](const FOGContentId& Id)
        {
            if (Activated.Contains(Id)) return true;
            TArray<FOGPackageDependencyRecord> Dependencies;
            if (!Store.ListPackageDependencies(Id, Dependencies, Reason)) return false;
            for (const auto& Dep : Dependencies) if (!Activate(Dep.DependencyPackageId)) return false;
            if (!FOGPackageManagerService(Store).ActivatePackage(Id, Reason)) return false;
            Activated.Add(Id); return true;
        };
        if (!Activate(Ref.PackageId)) return false;
    }
    if (auto* Existing = ResidentAssets.Find(Ref.AssetPath)) Asset = Existing->Get();
    else
    {
        Asset = Ref.AssetPath.TryLoad();
        if (!Asset)
        {
            if (ContainerHost && ContainerLease.IsValid())
            { FString ReleaseError; ContainerHost->Release(ContainerLease, ReleaseError); }
            Reason = TEXT("Optional asset is not locally resolvable; retain logical presentation.");
            return false;
        }
        ResidentAssets.Add(Ref.AssetPath, TStrongObjectPtr<UObject>(Asset));
    }
    FAssetLease Lease;
    Lease.PackageId = Ref.PackageId;
    Lease.ContainerLease = ContainerLease;
    Leases.FindOrAdd(Ref.AssetPath).Add(Lease);
    return true;
}

void FOGOptionalAssetRuntime::Release(const FOGOptionalAssetReference& Ref)
{
    TArray<FAssetLease>* PathLeases = Leases.Find(Ref.AssetPath);
    if (!PathLeases) return;
    const int32 Index = PathLeases->IndexOfByPredicate([&](const FAssetLease& Lease) { return Lease.PackageId == Ref.PackageId; });
    if (Index == INDEX_NONE) return;
    const FGuid ContainerLease = (*PathLeases)[Index].ContainerLease;
    PathLeases->RemoveAt(Index);
    if (PathLeases->IsEmpty()) { Leases.Remove(Ref.AssetPath); ResidentAssets.Remove(Ref.AssetPath); }
    // Drop our strong asset reference before releasing its physical-container lease.
    if (ContainerHost && ContainerLease.IsValid())
    { FString ReleaseError; ContainerHost->Release(ContainerLease, ReleaseError); }
}

void FOGOptionalAssetRuntime::ReleaseAll()
{
    ResidentAssets.Reset();
    if (ContainerHost)
        for (const auto& Path : Leases)
            for (const FAssetLease& Lease : Path.Value)
                if (Lease.ContainerLease.IsValid())
                { FString ReleaseError; ContainerHost->Release(Lease.ContainerLease, ReleaseError); }
    Leases.Reset();
}
