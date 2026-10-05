#pragma once

#include "CoreMinimal.h"
#include "Content/OGContentId.h"
#include "Core/OGEntityId.h"
#include "OGPackageReportManagementRecords.generated.h"

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGContentPackageRecord
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGContentId PackageId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 Version = 1;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FString ContentHash;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    bool bInstalled = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    bool bValidated = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    bool bActivated = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName Category = FName(TEXT("generic"));

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FString InstallUri;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName StorageClass = FName(TEXT("local_hot"));

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName SealedState = FName(TEXT("visible"));

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName DownloadState = FName(TEXT("installed"));

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FString CompatibilityJson = TEXT("{}");

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FString ManifestJson = TEXT("{}");
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGPackageDependencyRecord
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGContentId PackageId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGContentId DependencyPackageId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 MinimumVersion = 1;
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGReportRecord
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId ReportId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId OwnerEntityId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId SourceWorldEventId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName Category = FName(TEXT("general"));

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 Priority = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int64 CreatedWorldTick = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    bool bAcknowledged = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int64 AcknowledgedWorldTick = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FString PayloadJson = TEXT("{}");
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGReportDeliveryRecord
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId ReportId;

    /** ingame / android_notification / other projection channel. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName Channel = FName(TEXT("ingame"));

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName State = FName(TEXT("pending"));

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FString ScheduledRealUtc;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FString DeliveredRealUtc;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FString PlatformNotificationId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName PrivacyState = FName(TEXT("default"));
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGManifestationManagementMetadataRecord
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId ManifestationId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    bool bFavorite = false;

    /** Protected and Locked are canonical destructive-operation guards. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    bool bProtected = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    bool bLocked = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int64 UpdatedWorldTick = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FString StateJson = TEXT("{}");
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGManifestationContextSelectionRecord
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId OwnerEntityId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGContentId ContextId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId ManifestationId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int64 UpdatedWorldTick = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FString StateJson = TEXT("{}");
};
