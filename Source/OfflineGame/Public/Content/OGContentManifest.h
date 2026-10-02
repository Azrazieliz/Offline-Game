#pragma once

#include "CoreMinimal.h"
#include "Characters/OGCharacterDefinitions.h"
#include "Content/OGContentId.h"
#include "OGContentManifest.generated.h"

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGPackageDependency
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGContentId PackageId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 MinimumVersion = 1;
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGContentPackageManifest
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGContentId PackageId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 Version = 1;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TArray<FOGPackageDependency> Dependencies;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TArray<FOGCharacterIdentityDefinition> CharacterIdentities;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TArray<FOGCharacterVersionDefinition> CharacterVersions;
};

class OFFLINEGAME_API FOGContentManifestValidator
{
public:
    /**
     * Validates one package before activation.
     *
     * KnownExternalIdentities contains identities supplied by already validated
     * dependency packages, including canonical maturity needed to validate
     * Version-only mature-content packages safely.
     */
    static bool Validate(
        const FOGContentPackageManifest& Manifest,
        const TMap<FOGContentId, EOGCanonicalMaturity>& KnownExternalIdentities,
        TArray<FString>& OutErrors);
};
