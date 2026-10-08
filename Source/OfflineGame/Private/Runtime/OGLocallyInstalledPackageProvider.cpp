#include "Runtime/OGLocallyInstalledPackageProvider.h"
#include "Runtime/OGPackageManagerService.h"
#include "Persistence/OGWorldStore.h"
#include "ContentStreaming.h"
#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "IPlatformCrypto.h"
#include "IPlatformFilePak.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "PlatformCryptoTypes.h"
#include "Serialization/Archive.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/GarbageCollection.h"
#include "UObject/Package.h"
#include "UObject/UObjectIterator.h"


namespace
{
FString Normalize(const FString& Path)
{
    FString Result = FPaths::ConvertRelativePathToFull(Path);
    FPaths::NormalizeFilename(Result);
    if (!FPaths::CollapseRelativeDirectories(Result)) return FString();
    return Result;
}
bool SafePath(const FString& Path, const FString& ManagedRoot, FString& Error)
{
    const FString Full = Normalize(Path), Base = Normalize(ManagedRoot);
    if (Path.IsEmpty() || FPaths::IsRelative(Path) || Full.IsEmpty() || Base.IsEmpty() ||
        !FPaths::IsUnderDirectory(Full, Base))
    { Error = TEXT("Optional payload must remain in its native-approved managed storage root."); return false; }
    // Reject symlink/junction substitution, including ancestors of a native-approved root.
    IPlatformFile& Files = FPlatformFileManager::Get().GetPlatformFile();
    FString Current = Full;
    while (!Current.IsEmpty())
    {
        if (Files.IsSymlink(*Current) == ESymlinkResult::Symlink)
        { Error = TEXT("Optional managed storage cannot traverse symbolic links."); return false; }
        const FString Parent = FPaths::GetPath(Current);
        if (Parent == Current) break;
        Current = Parent;
    }
    return true;
}
bool IsSha256(const FString& Hash)
{
    if (Hash.Len() != 64) return false;
    for (TCHAR C : Hash) if (!FChar::IsHexDigit(C)) return false;
    return true;
}
FString ReceiptFile(const FString& Root, const FOGContentPackageRecord& Package)
{
    // The receipt body also checks the full identity: this index is never provenance.
    const FString Key = Package.PackageId.ToString() + TEXT("|") + FString::FromInt(Package.Version) + TEXT("|") + Package.ContentHash;
    return FPaths::Combine(Root, TEXT("Receipts"), FMD5::HashAnsiString(*Key) + TEXT(".json"));
}
bool ReadReceipt(const FString& Root, const FOGContentPackageRecord& Package,
    TSharedPtr<FJsonObject>& Receipt, FString& Json, FString& Error)
{
    const FString Filename = ReceiptFile(Root, Package);
    if (!SafePath(Filename, Root, Error)) return false;
    if (!FFileHelper::LoadFileToString(Json, *Filename) ||
        !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Receipt) || !Receipt)
    { Error = TEXT("No native installation receipt exists for this optional package."); return false; }
    FString Id, Hash, Manifest, Path, ManagedRoot;
    int32 Version = 0, ReceiptVersion = 0, Order = -1;
    if (!Receipt->TryGetNumberField(TEXT("receipt_version"), ReceiptVersion) || ReceiptVersion != 1 ||
        !Receipt->TryGetStringField(TEXT("package_id"), Id) || Id != Package.PackageId.ToString() ||
        !Receipt->TryGetNumberField(TEXT("version"), Version) || Version != Package.Version ||
        !Receipt->TryGetStringField(TEXT("sha256"), Hash) || !IsSha256(Hash) ||
        Package.ContentHash != TEXT("sha256:") + Hash ||
        !Receipt->TryGetStringField(TEXT("manifest"), Manifest) || Manifest != Package.ManifestJson ||
        !Receipt->TryGetStringField(TEXT("path"), Path) ||
        !Receipt->TryGetStringField(TEXT("managed_root"), ManagedRoot) ||
        !Receipt->TryGetNumberField(TEXT("mount_order"), Order) || Order < 0 || Order > 100 ||
        Package.InstallUri.IsEmpty() || FPaths::IsRelative(Package.InstallUri) ||
        !FPaths::IsSamePath(Normalize(Package.InstallUri), Path) ||
        Path != Normalize(Path) || ManagedRoot != Normalize(ManagedRoot) ||
        !SafePath(Path, ManagedRoot, Error))
    { if (Error.IsEmpty()) Error = TEXT("Imported package metadata does not match native installation provenance."); return false; }
    return true;
}
FString SerializeReceipt(const TSharedPtr<FJsonObject>& Object)
{
    FString Json;
    FJsonSerializer::Serialize(Object.ToSharedRef(), TJsonWriterFactory<>::Create(&Json));
    return Json;
}
bool WriteReceipt(const FString& Filename, const FString& Json, FString& Error)
{
    IFileManager& Files = IFileManager::Get();
    if (!Files.MakeDirectory(*FPaths::GetPath(Filename), true))
    { Error = TEXT("Cannot create the native receipt directory."); return false; }
    const FString Temp = Filename + TEXT(".") + FGuid::NewGuid().ToString(EGuidFormats::Digits) + TEXT(".tmp");
    if (!FFileHelper::SaveStringToFile(Json, *Temp, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM) ||
        !Files.Move(*Filename, *Temp, true, true))
    { Files.Delete(*Temp); Error = TEXT("Cannot publish the native installation receipt."); return false; }
    return true;
}
FPakPlatformFile* PakBackend()
{
    return static_cast<FPakPlatformFile*>(FPlatformFileManager::Get().FindPlatformFile(FPakPlatformFile::GetTypeName()));
}
bool HasIoStoreCompanion(const FString& Pak)
{
    const FString Stem = FPaths::ChangeExtension(Pak, TEXT(""));
    return IFileManager::Get().FileExists(*(Stem + TEXT(".utoc"))) ||
        IFileManager::Get().FileExists(*(Stem + TEXT(".ucas")));
}
}

struct FOGLocallyInstalledPackageProvider::FResidency
{
    FString Filename;
    TRefCountPtr<FPakFile> Pak;
};

FString FOGLocallyInstalledPackageProvider::DefaultRoot()
{
    return Normalize(FPaths::Combine(FPaths::ProjectPersistentDownloadDir(), TEXT("OfflineGameOptionalPackages")));
}
FOGLocallyInstalledPackageProvider::FOGLocallyInstalledPackageProvider(const FString& InRoot)
    : Root(InRoot.IsEmpty() ? DefaultRoot() : Normalize(InRoot)) {}

bool FOGLocallyInstalledPackageProvider::HashFileSha256(const FString& Filename, FString& Hash, FString& Error)
{
    Hash.Reset();
    TUniquePtr<FArchive> Reader(IFileManager::Get().CreateFileReader(*Filename));
    if (!Reader || Reader->TotalSize() < 0)
    { Error = TEXT("Cannot read optional artifact bytes."); return false; }
    auto Context = IPlatformCrypto::Get().CreateContext();
    if (!Context) { Error = TEXT("Platform SHA256 verification is unavailable."); return false; }
    auto Hasher = Context->CreateSHA256Hasher();
    if (Hasher.Init() != EPlatformCryptoResult::Success)
    { Error = TEXT("Cannot initialize optional artifact SHA256."); return false; }
    TArray<uint8> Buffer;
    Buffer.SetNumUninitialized(64 * 1024);
    int64 Remaining = Reader->TotalSize();
    while (Remaining > 0)
    {
        const int32 Count = static_cast<int32>(FMath::Min<int64>(Remaining, Buffer.Num()));
        Reader->Serialize(Buffer.GetData(), Count);
        if (Reader->IsError() || Hasher.Update(MakeArrayView<const uint8>(Buffer.GetData(), Count)) != EPlatformCryptoResult::Success)
        { Error = TEXT("Cannot hash all optional artifact bytes."); return false; }
        Remaining -= Count;
    }
    uint8 Digest[32];
    if (Reader->IsError() || Hasher.Finalize(MakeArrayView(Digest)) != EPlatformCryptoResult::Success)
    { Error = TEXT("Cannot finalize optional artifact SHA256."); return false; }
    Hash = BytesToHex(Digest, UE_ARRAY_COUNT(Digest)).ToLower();
    return true;
}

bool FOGLocallyInstalledPackageProvider::Resolve(const FOGContentPackageRecord& Package,
    FOGOptionalPackageContainer& Container, FString& Error)
{
    if (FPaths::GetExtension(Package.InstallUri).ToLower() != TEXT("pak"))
    { Error = TEXT("External optional content supports .pak only; IoStore .utoc/.ucas is unsupported."); return false; }
    TSharedPtr<FJsonObject> Receipt;
    FString Json;
    if (!ReadReceipt(Root, Package, Receipt, Json, Error)) return false;
    Container.Filename = Receipt->GetStringField(TEXT("path"));
    Container.Format = FName(TEXT("pak"));
    Container.MountOrder = Receipt->GetIntegerField(TEXT("mount_order"));
    Container.VerificationIdentity = Package.PackageId.ToString() + TEXT("|") +
        FString::FromInt(Package.Version) + TEXT("|") + Package.ContentHash + TEXT("|") + Json;
    return true;
}
bool FOGLocallyInstalledPackageProvider::Verify(const FOGContentPackageRecord& Package,
    const FOGOptionalPackageContainer& Container, FString& Error)
{
    FOGOptionalPackageContainer Resolved;
    if (!Resolve(Package, Resolved, Error) || Resolved.VerificationIdentity != Container.VerificationIdentity ||
        !FPaths::IsSamePath(Resolved.Filename, Container.Filename))
    { if (Error.IsEmpty()) Error = TEXT("Installation provenance changed during optional acquisition."); return false; }
    if (HasIoStoreCompanion(Container.Filename))
    { Error = TEXT("IoStore companions are unsupported by the optional pak host."); return false; }
    FString Hash;
    if (!HashFileSha256(Container.Filename, Hash, Error)) return false;
    if (Package.ContentHash != TEXT("sha256:") + Hash)
    { Error = TEXT("Optional artifact bytes no longer match the trusted installation SHA256."); return false; }
    TRefCountPtr<FPakFile> Pak = new FPakFile(&FPlatformFileManager::Get().GetPlatformFile(), *Container.Filename, false);
    if (!Pak->IsValid())
    { Error = TEXT("Verified optional artifact is not a valid indexed pak."); return false; }
    VerifiedPaths.FindOrAdd(Package.PackageId).Add(Container.Filename);
    return true;
}
bool FOGLocallyInstalledPackageProvider::CanTakeOwnership(const FOGOptionalPackageContainer& Container, FString& Error) const
{
    FPakPlatformFile* Backend = PakBackend();
    if (!Backend) { Error = TEXT("Engine pak platform file is unavailable."); return false; }
    TArray<FString> Mounted;
    Backend->GetMountedPakFilenames(Mounted);
    for (const FString& Filename : Mounted)
        if (FPaths::IsSamePath(Normalize(Filename), Container.Filename))
        { Error = TEXT("Optional host cannot take ownership of an engine-startup or separately mounted pak."); return false; }
    return true;
}
bool FOGLocallyInstalledPackageProvider::CanUnload(const FOGContentId& Id, FString& Error) const
{
    const TSharedPtr<FResidency>* Entry = Residency.Find(Id);
    if (!Entry) return true; // Embedded/base packages have no physical container.
    if (IsAsyncLoading() || IsGarbageCollecting() || IStreamingManager::Get().GetNumWantingResources() > 0)
    { Error = TEXT("Wait for asynchronous loading, streaming and garbage collection before optional unmount."); return false; }
    const FPakFile* Pak = (*Entry)->Pak.GetReference();
    if (!Pak) { Error = TEXT("Cannot prove optional unload safety without its actual engine pak."); return false; }
    // Consumer ownership is enforced by the host's explicit leases. At this point there are no
    // host leases, async loads, streaming work or GC in flight; loaded package objects are checked
    // below, and the engine unmount operation remains the final physical-residency authority.
    for (TObjectIterator<UObject> It; It; ++It)
    {
        const UPackage* Package = It->GetOutermost();
        if (!Package || !FPackageName::IsValidLongPackageName(Package->GetName())) continue;
        const FString Base = FPackageName::LongPackageNameToFilename(Package->GetName());
        for (const TCHAR* Extension : { TEXT(".uasset"), TEXT(".umap"), TEXT(".uexp"), TEXT(".ubulk") })
        {
            const FString Candidate = Base + Extension;
            FString Relative = Normalize(Candidate);
            FString FullMount = Normalize(Pak->GetMountPoint());
            FPaths::NormalizeDirectoryName(FullMount);
            FullMount += TEXT("/");
            const bool UnderMount = FPaths::IsUnderDirectory(Relative, FullMount) &&
                FPaths::MakePathRelativeTo(Relative, *FullMount);
            if (Pak->Find(Candidate, nullptr) == FPakFile::EFindResult::Found ||
                (UnderMount && Pak->Find(FPaths::Combine(Pak->GetMountPoint(), Relative), nullptr) == FPakFile::EFindResult::Found))
            { Error = TEXT("Loaded UObject/package still refers to optional pak content: ") + Package->GetName(); return false; }
        }
    }
    return true;
}
FOGOptionalPackageHostCallbacks FOGLocallyInstalledPackageProvider::MakeCallbacks(IOGWorldStore& Store)
{
    (void)Store; // Callbacks use native receipts; the host retains the canonical lifecycle authority.
    FOGOptionalPackageHostCallbacks Result;
    // This classification grants no mount authority: only already engine/cooked embedded assets resolve.
    Result.UsesContainer = [](const FOGContentPackageRecord& Package)
    { return !Package.InstallUri.IsEmpty() && !Package.InstallUri.StartsWith(TEXT("embedded:"), ESearchCase::CaseSensitive); };
    Result.ResolveContainer = [this](const auto& P, auto& C, auto& E) { return Resolve(P, C, E); };
    Result.VerifyArtifact = [this](const auto& P, const auto& C, auto& E) { return Verify(P, C, E); };
    Result.CanTakeOwnership = [this](const auto& C, auto& E) { return CanTakeOwnership(C, E); };
    Result.CanUnload = [this](const auto& Id, auto& E) { return CanUnload(Id, E); };
    Result.DidMount = [this](const FOGContentId& Id, const FOGOptionalPackageContainer& Container, IPakFile* Pak)
    {
        auto Entry = MakeShared<FResidency>();
        Entry->Filename = Container.Filename;
        Entry->Pak = static_cast<FPakFile*>(Pak);
        Residency.Add(Id, MoveTemp(Entry));
    };
    Result.DidUnmount = [this](const FOGContentId& Id) { Residency.Remove(Id); };
    return Result;
}

bool FOGLocallyInstalledPackageProvider::InstallVerifiedPak(IOGWorldStore& Store,
    const FOGTrustedOptionalPackageInstall& Expected, const FString& SourcePak, FString& Uri, FString& Error)
{
    Uri.Reset(); Error.Reset();
    if (!IsInGameThread() || !Expected.PackageId.IsValid() || Expected.Version < 1 ||
        !IsSha256(Expected.Sha256) || Expected.MountOrder < 0 || Expected.MountOrder > 100 ||
        SourcePak.IsEmpty() || FPaths::IsRelative(SourcePak) || FPaths::GetExtension(SourcePak).ToLower() != TEXT("pak"))
    { Error = TEXT("Native optional installation requires ID/version/SHA256, an absolute .pak source and mount order 0..100."); return false; }
    TSharedPtr<FJsonObject> Manifest;
    if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Expected.ManifestJson), Manifest) || !Manifest)
    { Error = TEXT("Trusted optional installation manifest is invalid."); return false; }
    if (HasIoStoreCompanion(SourcePak))
    { Error = TEXT("IoStore installation is unsupported; supply a standalone pak."); return false; }
    bool Found = false;
    FOGContentPackageRecord Previous;
    if (!Store.TryReadContentPackageRecord(Expected.PackageId, Found, Previous, Error)) return false;
    if (Found && Previous.bActivated)
    { Error = TEXT("Deactivate and release optional consumers before replacing their installation."); return false; }
    if (const TSet<FString>* Existing = VerifiedPaths.Find(Expected.PackageId))
    {
        FPakPlatformFile* Backend = PakBackend();
        if (Backend)
        {
            TArray<FString> Mounted; Backend->GetMountedPakFilenames(Mounted);
            for (const FString& Filename : Mounted)
                if (Existing->Contains(Normalize(Filename)))
                { Error = TEXT("Unmount optional content before replacing its installation."); return false; }
        }
    }
    FString Hash;
    if (!HashFileSha256(SourcePak, Hash, Error)) return false;
    if (!Hash.Equals(Expected.Sha256, ESearchCase::IgnoreCase))
    { Error = TEXT("Source pak SHA256 does not match the native installer decision."); return false; }
    const FString PayloadDirectory = FPaths::Combine(Root, TEXT("Artifacts"), FGuid::NewGuid().ToString(EGuidFormats::Digits));
    const FString Destination = FPaths::Combine(PayloadDirectory, TEXT("content.pak"));
    if (!SafePath(Destination, Root, Error)) return false;
    IFileManager& Files = IFileManager::Get();
    if (!Files.MakeDirectory(*PayloadDirectory, true) || Files.Copy(*Destination, *SourcePak, false, false) != COPY_OK)
    { Error = TEXT("Optional installation copy failed; no installation was registered."); return false; }
    if (!HashFileSha256(Destination, Hash, Error) || !Hash.Equals(Expected.Sha256, ESearchCase::IgnoreCase))
    { Files.DeleteDirectory(*PayloadDirectory, false, true); if (Error.IsEmpty()) Error = TEXT("Copied optional bytes failed SHA256 verification."); return false; }
    TRefCountPtr<FPakFile> Pak = new FPakFile(&FPlatformFileManager::Get().GetPlatformFile(), *Destination, false);
    if (!Pak->IsValid())
    { Pak.SafeRelease(); Files.DeleteDirectory(*PayloadDirectory, false, true); Error = TEXT("Native optional installation requires a valid indexed pak."); return false; }
    Pak.SafeRelease();
    // Installation updates byte provenance, not the user's independent management metadata.
    FOGContentPackageRecord Package = Found ? Previous : FOGContentPackageRecord();
    Package.PackageId = Expected.PackageId; Package.Version = Expected.Version;
    Package.ContentHash = TEXT("sha256:") + Hash; Package.ManifestJson = Expected.ManifestJson;
    Package.InstallUri = Destination; Package.bInstalled = true; Package.bValidated = true;
    Package.DownloadState = FName(TEXT("installed"));
    if (!Found) { Package.Category = FName(TEXT("optional")); Package.StorageClass = FName(TEXT("local_hot")); }
    auto Receipt = MakeShared<FJsonObject>();
    Receipt->SetNumberField(TEXT("receipt_version"), 1);
    Receipt->SetStringField(TEXT("package_id"), Expected.PackageId.ToString());
    Receipt->SetNumberField(TEXT("version"), Expected.Version);
    Receipt->SetStringField(TEXT("sha256"), Hash);
    Receipt->SetStringField(TEXT("manifest"), Expected.ManifestJson);
    Receipt->SetStringField(TEXT("path"), Destination);
    Receipt->SetStringField(TEXT("managed_root"), Root);
    Receipt->SetNumberField(TEXT("mount_order"), Expected.MountOrder);
    const FString ReceiptFilename = ReceiptFile(Root, Package);
    FString OldJson;
    const bool HadReceipt = Files.FileExists(*ReceiptFilename);
    if (HadReceipt && !FFileHelper::LoadFileToString(OldJson, *ReceiptFilename))
    { Files.DeleteDirectory(*PayloadDirectory, false, true); Error = TEXT("Cannot preserve existing installation receipt."); return false; }
    if (!SafePath(ReceiptFilename, Root, Error) || !WriteReceipt(ReceiptFilename, SerializeReceipt(Receipt), Error))
    { Files.DeleteDirectory(*PayloadDirectory, false, true); return false; }
    if (!FOGPackageManagerService(Store).RegisterPackage(Package, Error))
    {
        FString CleanupError;
        const bool Restored = HadReceipt ? WriteReceipt(ReceiptFilename, OldJson, CleanupError) : Files.Delete(*ReceiptFilename);
        Files.DeleteDirectory(*PayloadDirectory, false, true);
        if (!Restored) Error += TEXT(" Receipt rollback failed: ") + CleanupError;
        return false;
    }
    Uri = Destination;
    return true;
}

bool FOGLocallyInstalledPackageProvider::PrepareStorageMove(const FOGContentPackageRecord& Package,
    const FString& Destination, const FString& DestinationRoot, FOGOptionalPackageStorageReceiptMove& Move, FString& Error)
{
    Move = FOGOptionalPackageStorageReceiptMove();
    const FString Root = DefaultRoot();
    const FString ReceiptFilename = ReceiptFile(Root, Package);
    if (!IFileManager::Get().FileExists(*ReceiptFilename)) return true; // Preserve legacy storage behavior without minting trust.
    TSharedPtr<FJsonObject> Receipt; FString Json;
    if (!ReadReceipt(Root, Package, Receipt, Json, Error) || !SafePath(Destination, DestinationRoot, Error)) return false;
    FOGOptionalPackageContainer Container;
    Container.Filename = Normalize(Package.InstallUri);
    if (FPakPlatformFile* Backend = PakBackend())
    {
        TArray<FString> Mounted; Backend->GetMountedPakFilenames(Mounted);
        for (const FString& Filename : Mounted)
            if (FPaths::IsSamePath(Normalize(Filename), Container.Filename))
            { Error = TEXT("Unmount optional pak content before moving its trusted storage."); return false; }
    }
    FString Hash;
    if (!HashFileSha256(Package.InstallUri, Hash, Error) || Package.ContentHash != TEXT("sha256:") + Hash ||
        !HashFileSha256(Destination, Hash, Error) || Package.ContentHash != TEXT("sha256:") + Hash)
    { if (Error.IsEmpty()) Error = TEXT("Package move bytes do not match native installation provenance."); return false; }
    Receipt->SetStringField(TEXT("path"), Normalize(Destination));
    Receipt->SetStringField(TEXT("managed_root"), Normalize(DestinationRoot));
    Move.ReceiptFilename = ReceiptFilename; Move.OriginalJson = Json; Move.DestinationJson = SerializeReceipt(Receipt);
    return true;
}
bool FOGLocallyInstalledPackageProvider::PublishStorageMove(const FOGOptionalPackageStorageReceiptMove& Move, FString& Error)
{
    return Move.ReceiptFilename.IsEmpty() || WriteReceipt(Move.ReceiptFilename, Move.DestinationJson, Error);
}
bool FOGLocallyInstalledPackageProvider::RollbackStorageMove(const FOGOptionalPackageStorageReceiptMove& Move, FString& Error)
{
    return Move.ReceiptFilename.IsEmpty() || WriteReceipt(Move.ReceiptFilename, Move.OriginalJson, Error);
}
