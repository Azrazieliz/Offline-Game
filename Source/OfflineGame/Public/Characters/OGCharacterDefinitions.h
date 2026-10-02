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
     * Explicit permission for sexual-content references attached to this Version.
     * Validation rejects this unless the Character Identity is canonically Adult.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    bool bSexualContentEligible = false;

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

    /**
     * Duplicate pulls advance this persistent counter instead of creating a
     * second local Manifestation of the same Character Identity.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 DuplicateAcquisitionCount = 0;

    /**
     * Extensible progression payload until specialized progression tables are
     * introduced. This field must not become a dumping ground for indexed data.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FString ProgressionStateJson = TEXT("{}");
};
