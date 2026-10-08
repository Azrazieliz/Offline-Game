#include "Runtime/OGOptionalPackageHost.h"

#include "HAL/FileManager.h"
#include "Misc/CoreDelegates.h"
#include "Misc/Paths.h"
#include "Persistence/OGWorldStore.h"
#include "Runtime/OGPackageManagerService.h"

FOGOptionalPackageHost::FOGOptionalPackageHost(IOGWorldStore& InStore, FOGOptionalPackageHostCallbacks InCallbacks)
    : Store(InStore), Callbacks(MoveTemp(InCallbacks)) {}

#if WITH_DEV_AUTOMATION_TESTS
FOGOptionalPackageHost::FOGOptionalPackageHost(IOGWorldStore& InStore,
    FOGOptionalPackageHostCallbacks InCallbacks, FOGOptionalPackageEngineAdapter InTestEngine)
    : TestEngine(MakeUnique<FOGOptionalPackageEngineAdapter>(MoveTemp(InTestEngine))),
      Store(InStore), Callbacks(MoveTemp(InCallbacks)) {}
#endif

bool FOGOptionalPackageHost::BackendAvailable() const
{
#if WITH_DEV_AUTOMATION_TESTS
    if (TestEngine) return TestEngine->IsAvailable && TestEngine->IsAvailable() && TestEngine->Mount && TestEngine->Unmount;
#endif
    return FCoreDelegates::MountPak.IsBound() && FCoreDelegates::OnUnmountPak.IsBound();
}
bool FOGOptionalPackageHost::MountContainer(const FOGContentId& Id, const FOGOptionalPackageContainer& Container)
{
#if WITH_DEV_AUTOMATION_TESTS
    if (TestEngine) return TestEngine->Mount && TestEngine->Mount(Container.Filename, Container.MountOrder);
#endif
    if (!FCoreDelegates::MountPak.IsBound()) return false;
    IPakFile* Pak = FCoreDelegates::MountPak.Execute(Container.Filename, Container.MountOrder);
    if (Pak && Callbacks.DidMount) Callbacks.DidMount(Id, Container, Pak);
    return Pak != nullptr;
}
bool FOGOptionalPackageHost::UnmountContainer(const FString& Filename)
{
#if WITH_DEV_AUTOMATION_TESTS
    if (TestEngine) return TestEngine->Unmount && TestEngine->Unmount(Filename);
#endif
    return FCoreDelegates::OnUnmountPak.IsBound() && FCoreDelegates::OnUnmountPak.Execute(Filename);
}

bool FOGOptionalPackageHost::BuildClosure(const FOGContentId& Id, TSet<FOGContentId>& Visiting,
    TSet<FOGContentId>& Complete, TArray<FOGContentPackageRecord>& Ordered, FString& Error)
{
    if (Complete.Contains(Id)) return true;
    if (!Id.IsValid() || Visiting.Contains(Id)) { Error = TEXT("Invalid optional package or dependency cycle."); return false; }
    FOGContentPackageRecord Record;
    bool Found = false;
    if (!Store.TryReadContentPackageRecord(Id, Found, Record, Error)) return false;
    if (!Found || !Record.bInstalled || !Record.bValidated || Record.DownloadState != FName(TEXT("installed")))
    { Error = TEXT("Optional package is not locally installed and validated: ") + Id.ToString(); return false; }
    Visiting.Add(Id);
    TArray<FOGPackageDependencyRecord> Dependencies;
    if (!Store.ListPackageDependencies(Id, Dependencies, Error)) return false;
    for (const FOGPackageDependencyRecord& Dependency : Dependencies)
    {
        bool DependencyFound = false;
        FOGContentPackageRecord DependencyRecord;
        if (!Store.TryReadContentPackageRecord(Dependency.DependencyPackageId, DependencyFound, DependencyRecord, Error)) return false;
        if (!DependencyFound || DependencyRecord.Version < Dependency.MinimumVersion)
        { Error = TEXT("Optional dependency is absent or below its required version."); return false; }
        if (!BuildClosure(Dependency.DependencyPackageId, Visiting, Complete, Ordered, Error)) return false;
    }
    Visiting.Remove(Id);
    Complete.Add(Id);
    Ordered.Add(Record);
    return true;
}

bool FOGOptionalPackageHost::ResolveAndVerify(const FOGContentPackageRecord& Record,
    FOGOptionalPackageContainer& Container, FString& Error)
{
    if (!Callbacks.UsesContainer || !Callbacks.ResolveContainer || !Callbacks.VerifyArtifact ||
        !Callbacks.CanTakeOwnership || !Callbacks.CanUnload)
    { Error = TEXT("Optional package host requires trusted resolution, artifact verification and safe-unload callbacks."); return false; }
    if (!Callbacks.UsesContainer(Record))
    { Container.Format = FName(TEXT("embedded")); Container.VerificationIdentity = TEXT("host-embedded"); return true; }
    if (!Callbacks.ResolveContainer(Record, Container, Error)) return false;
    const FString Extension = FPaths::GetExtension(Container.Filename).ToLower();
    if (Container.Format != FName(TEXT("pak")) || Extension != TEXT("pak"))
    { Error = TEXT("This host supports .pak containers only; IoStore .utoc/.ucas mounting is unsupported."); return false; }
    if (Container.Filename.IsEmpty() || FPaths::IsRelative(Container.Filename) ||
        Container.VerificationIdentity.IsEmpty() || Container.MountOrder < 0 || Container.MountOrder > 100)
    { Error = TEXT("Container needs an absolute trusted path, verification identity and valid authored mount order."); return false; }
    Container.Filename = FPaths::ConvertRelativePathToFull(Container.Filename);
    FPaths::NormalizeFilename(Container.Filename);
    if (!IFileManager::Get().FileExists(*Container.Filename))
    { Error = TEXT("Optional container is absent; keep logical character/world state."); return false; }
    // bValidated/ContentHash are imported metadata, not proof that these bytes are approved.
    return Callbacks.VerifyArtifact(Record, Container, Error);
}

bool FOGOptionalPackageHost::Acquire(const FOGContentId& Id, FGuid& OutLease, FString& Error)
{
    OutLease.Invalidate();
    Error.Reset();
    if (!IsInGameThread()) { Error = TEXT("Optional container operations require the game thread."); return false; }
    TSet<FOGContentId> Visiting, Complete;
    TArray<FOGContentPackageRecord> Ordered;
    if (!BuildClosure(Id, Visiting, Complete, Ordered, Error)) return false;
    TArray<FOGOptionalPackageContainer> Containers;
    // Verify the entire closure before mounting any file or changing activation.
    for (const FOGContentPackageRecord& Record : Ordered)
    {
        FOGOptionalPackageContainer Container;
        if (!ResolveAndVerify(Record, Container, Error)) return false;
        for (const auto& Existing : Mounted)
        {
            if (!Container.Filename.IsEmpty() && Existing.Key != Record.PackageId && FPaths::IsSamePath(Existing.Value.Container.Filename, Container.Filename))
            { Error = TEXT("Two package identities cannot own the same mounted container."); return false; }
        }
        for (const FOGOptionalPackageContainer& Previous : Containers)
        {
            if (!Container.Filename.IsEmpty() && FPaths::IsSamePath(Previous.Filename, Container.Filename))
            { Error = TEXT("Dependency closure assigns one container to multiple package identities."); return false; }
        }
        if (const FMounted* Existing = Mounted.Find(Record.PackageId))
        {
            if (Existing->Record.Version != Record.Version || Existing->Record.ContentHash != Record.ContentHash ||
                Existing->Record.ManifestJson != Record.ManifestJson ||
                Existing->Record.InstallUri != Record.InstallUri ||
                Existing->Container.VerificationIdentity != Container.VerificationIdentity ||
                !FPaths::IsSamePath(Existing->Container.Filename, Container.Filename) ||
                Existing->Container.MountOrder != Container.MountOrder)
            { Error = TEXT("Mounted package changed; release consumers and unmount before reloading."); return false; }
            TArray<FOGPackageDependencyRecord> CurrentDependencies;
            if (!Store.ListPackageDependencies(Record.PackageId, CurrentDependencies, Error)) return false;
            if (CurrentDependencies.Num() != Existing->Dependencies.Num())
            { Error = TEXT("Mounted package dependency graph changed; release and reload it."); return false; }
            for (const auto& Dependency : CurrentDependencies)
                if (!Existing->Dependencies.Contains(Dependency.DependencyPackageId))
                { Error = TEXT("Mounted package dependency graph changed; release and reload it."); return false; }
        }
        Containers.Add(MoveTemp(Container));
    }
    TArray<FOGContentId> NewlyMounted;
    for (int32 Index = 0; Index < Ordered.Num(); ++Index)
    {
        const auto& Record = Ordered[Index];
        if (Mounted.Contains(Record.PackageId)) continue;
        TArray<FOGPackageDependencyRecord> Dependencies;
        if (!Store.ListPackageDependencies(Record.PackageId, Dependencies, Error))
        { RollbackMounts(NewlyMounted, Error); return false; }
        const bool bEmbedded = Containers[Index].Format == FName(TEXT("embedded"));
        if (!bEmbedded && !Callbacks.CanTakeOwnership(Containers[Index], Error))
        { RollbackMounts(NewlyMounted, Error); return false; }
        if (!bEmbedded && !BackendAvailable())
        { Error = TEXT("Engine pak mount/unmount backend is unavailable on this host."); RollbackMounts(NewlyMounted, Error); return false; }
        if (!bEmbedded && !MountContainer(Record.PackageId, Containers[Index]))
        { Error = TEXT("Engine rejected optional pak mount."); RollbackMounts(NewlyMounted, Error); return false; }
        FMounted Entry;
        Entry.Record = Record;
        Entry.Container = Containers[Index];
        for (const auto& Dependency : Dependencies) Entry.Dependencies.Add(Dependency.DependencyPackageId);
        Mounted.Add(Record.PackageId, MoveTemp(Entry));
        MountSequence.Add(Record.PackageId);
        NewlyMounted.Add(Record.PackageId);
    }
    if (!Store.BeginTransaction(Error)) { RollbackMounts(NewlyMounted, Error); return false; }
    FOGPackageManagerService Manager(Store);
    for (const auto& Record : Ordered)
    {
        if (!Manager.ActivatePackage(Record.PackageId, Error))
        {
            FString RollbackError;
            Store.RollbackTransaction(RollbackError);
            if (!RollbackError.IsEmpty()) Error += TEXT(" Store rollback: ") + RollbackError;
            RollbackMounts(NewlyMounted, Error);
            return false;
        }
    }
    if (!Store.CommitTransaction(Error))
    {
        FString RollbackError;
        Store.RollbackTransaction(RollbackError);
        if (!RollbackError.IsEmpty()) Error += TEXT(" Store rollback: ") + RollbackError;
        RollbackMounts(NewlyMounted, Error);
        return false;
    }
    TArray<FOGContentId> Closure;
    for (const auto& Record : Ordered)
    {
        ++Mounted.FindChecked(Record.PackageId).LeaseCount;
        Closure.Add(Record.PackageId);
    }
    OutLease = FGuid::NewGuid();
    Leases.Add(OutLease, MoveTemp(Closure));
    return true;
}

bool FOGOptionalPackageHost::Release(const FGuid& Lease, FString& Error)
{
    Error.Reset();
    if (!IsInGameThread()) { Error = TEXT("Optional container operations require the game thread."); return false; }
    const TArray<FOGContentId>* Closure = Leases.Find(Lease);
    if (!Closure) { Error = TEXT("Unknown or already released optional container lease."); return false; }
    for (const FOGContentId& Id : *Closure) --Mounted.FindChecked(Id).LeaseCount;
    Leases.Remove(Lease);
    return true;
}

bool FOGOptionalPackageHost::Unmount(const FOGContentId& Id, FString& Error)
{
    Error.Reset();
    if (!IsInGameThread()) { Error = TEXT("Optional container operations require the game thread."); return false; }
    FMounted* Entry = Mounted.Find(Id);
    if (!Entry) return true;
    if (Entry->LeaseCount != 0) { Error = TEXT("Optional container is still leased by a consumer."); return false; }
    for (const auto& Other : Mounted)
    {
        if (Other.Key != Id && Other.Value.Dependencies.Contains(Id))
        { Error = TEXT("Unmount optional dependents before their dependency."); return false; }
    }
    if (!Callbacks.CanUnload || !Callbacks.CanUnload(Id, Error)) return false;
    if (Entry->Container.Format != FName(TEXT("embedded")) &&
        !UnmountContainer(Entry->Container.Filename))
    { Error = TEXT("Engine rejected optional container unmount; residency remains tracked."); return false; }
    if (Entry->Container.Format != FName(TEXT("embedded")) && Callbacks.DidUnmount) Callbacks.DidUnmount(Id);
    Mounted.Remove(Id);
    MountSequence.Remove(Id);
    // Logical activation and character/world records are independent of physical residency.
    return true;
}

bool FOGOptionalPackageHost::UnmountAll(FString& Error)
{
    Error.Reset();
    if (HasOutstandingLeases()) { Error = TEXT("Release optional consumers before host shutdown."); return false; }
    const TArray<FOGContentId> Order = MountSequence;
    for (int32 Index = Order.Num() - 1; Index >= 0; --Index)
        if (!Unmount(Order[Index], Error)) return false;
    return true;
}

void FOGOptionalPackageHost::RollbackMounts(const TArray<FOGContentId>& New, FString& Error)
{
    for (int32 Index = New.Num() - 1; Index >= 0; --Index)
    {
        FString CleanupError;
        if (!Unmount(New[Index], CleanupError)) Error += TEXT(" Mount rollback retained residency: ") + CleanupError;
    }
}

bool FOGOptionalPackageHost::Reload(const FOGContentId& Id, FGuid& OutLease, FString& Error)
{
    OutLease.Invalidate();
    if (!Unmount(Id, Error)) return false;
    return Acquire(Id, OutLease, Error);
}

bool FOGOptionalPackageHost::IsMounted(const FOGContentId& Id) const { return Mounted.Contains(Id); }

bool FOGOptionalPackageHost::RequiresContainer(const FOGContentId& Id, bool& Required, FString& Error) const
{
    Required = false;
    Error.Reset();
    if (!Callbacks.UsesContainer) { Error = TEXT("Optional host has no trusted installation authority."); return false; }
    bool Found = false;
    FOGContentPackageRecord Record;
    if (!Store.TryReadContentPackageRecord(Id, Found, Record, Error)) return false;
    if (!Found) { Error = TEXT("Unknown optional package."); return false; }
    Required = Callbacks.UsesContainer(Record);
    return true;
}
