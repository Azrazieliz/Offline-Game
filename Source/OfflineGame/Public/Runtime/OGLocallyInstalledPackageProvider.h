#pragma once
#include "CoreMinimal.h"
#include "Runtime/OGOptionalPackageHost.h"

/**
 * Native installer decision, supplied by authored content or an authenticated installation flow.
 * NEVER construct this decision from an imported world DB, bValidated, InstallUri or ContentHash.
 */
struct OFFLINEGAME_API FOGTrustedOptionalPackageInstall
{
    FOGContentId PackageId;
    int32 Version = 1;
    FString Sha256;
    FString ManifestJson = TEXT("{}");
    int32 MountOrder = 0;
};

/** Prepared receipt migration; owned only by the canonical package-storage operation. */
struct OFFLINEGAME_API FOGOptionalPackageStorageReceiptMove
{
    FString ReceiptFilename;
    FString OriginalJson;
    FString DestinationJson;
};

/**
 * Local installation authority outside the importable world DB. The default root is the platform's
 * persistent download directory. Receipts bind native-approved ID/version/manifest/SHA256 to an
 * exact normalized managed path. Callbacks borrow this object; it must outlive the physical host.
 */
class OFFLINEGAME_API FOGLocallyInstalledPackageProvider
{
public:
    explicit FOGLocallyInstalledPackageProvider(const FString& InRoot = FString());
    static FString DefaultRoot();
    FOGOptionalPackageHostCallbacks MakeCallbacks(IOGWorldStore& Store);
    bool InstallVerifiedPak(IOGWorldStore& Store, const FOGTrustedOptionalPackageInstall& Expected,
        const FString& SourcePak, FString& OutInstallUri, FString& OutError);
    static bool HashFileSha256(const FString& Filename, FString& OutHash, FString& OutError);
    /** Called directly by the existing package manager; legacy unreceipted files gain no trust. */
    static bool PrepareStorageMove(const FOGContentPackageRecord& Package,
        const FString& Destination, const FString& DestinationRoot,
        FOGOptionalPackageStorageReceiptMove& OutMove, FString& OutError);
    static bool PublishStorageMove(const FOGOptionalPackageStorageReceiptMove& Move, FString& OutError);
    static bool RollbackStorageMove(const FOGOptionalPackageStorageReceiptMove& Move, FString& OutError);
private:
    struct FResidency;
    FString Root;
    TMap<FOGContentId, TSharedPtr<FResidency>> Residency;
    TMap<FOGContentId, TSet<FString>> VerifiedPaths;
    bool Resolve(const FOGContentPackageRecord&, FOGOptionalPackageContainer&, FString&);
    bool Verify(const FOGContentPackageRecord&, const FOGOptionalPackageContainer&, FString&);
    bool CanTakeOwnership(const FOGOptionalPackageContainer&, FString&) const;
    bool CanUnload(const FOGContentId&, FString&) const;
};
