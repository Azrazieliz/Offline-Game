#pragma once

#include "CoreMinimal.h"
#include "Content/OGContentId.h"
#include "Core/OGEntityId.h"
#include "OGCharacterDefinitions.generated.h"

/**
 * Sexual-content eligibility must be explicit and based on canonical lore.
 * Visual appearance never determines this value.
 */
UENUM(BlueprintType)
enum class EOGCanonicalMaturity : uint8
{
    Unknown,
    NonAdult,
    Adult
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGCharacterIdentityDefinition
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGContentId IdentityId;

    /** Localization key / internal presentation key, not a permanent display name. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FString DisplayNameKey;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    EOGCanonicalMaturity CanonicalMaturity = EOGCanonicalMaturity::Unknown;

    /**
     * Immutable acquisition-origin grading identifier.
     * Exact rarity taxonomy remains data-defined.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName OriginRarity = NAME_None;
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGCharacterVersionDefinition
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGContentId VersionId;

    /** Versions point to Identity; Identity does not own a mutable Version list. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGContentId IdentityId;

    /** Base / Awakened / Corrupted / event / etc. remains data-defined. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName VersionKind = NAME_None;

    /**
     * Descriptive authored adult-content profile. Presence is valid only when the
     * referenced Character Identity is canonically Adult; this is not an access
     * or consent gate and cannot override Identity maturity.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGContentId AdultContentProfileId;

    /** Descriptive adult presentation tags; appearance never determines maturity. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TArray<FName> AdultPresentationTags;

    /** Version/form scene-library references, descriptive rather than permission flags. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TArray<FOGContentId> AdultSceneLibraryIds;

    /** General mature visual support such as blood/injury/clothing-damage hooks. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TArray<FName> MaturePresentationTags;
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGCharacterManifestationRecord
{
    GENERATED_BODY()

    /** Persistent ruler-specific owned instance. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId ManifestationId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId OwningRulerId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGContentId IdentityId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGContentId ActiveVersionId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 Level = 1;

    /** Mutable rarity/progression state; taxonomy remains data-defined. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName CurrentRarity = NAME_None;

    /** Canonical world tick at which this owned Manifestation was acquired. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int64 AcquisitionWorldTick = 0;

    /** Zero-based acquisition order among this Ruler's Manifestations of the Identity. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 AcquisitionOrdinal = 0;

    /** Gacha-pull world event that created this Manifestation, if one is known. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId OriginPullEventId;

    /** First controlled Territory in which World Mode deployment was anchored. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId WorldModeAnchorTerritoryId;

    /** Canonical tick of first World Mode anchoring; zero while unanchored. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int64 WorldModeAnchorTick = 0;

    /** Active / archived / converged / other lifecycle states remain data-defined. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName LifecycleState = FName(TEXT("active"));

    /** Player-editable copy/build label. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FString BuildLabel;

    /**
     * Extensible progression payload until specialized progression tables are
     * introduced. This field must not become a dumping ground for indexed data.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FString ProgressionStateJson = TEXT("{}");
};
