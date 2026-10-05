#pragma once

#include "CoreMinimal.h"
#include "Characters/OGCharacterDefinitions.h"
#include "Content/OGContentId.h"
#include "Core/OGEntityId.h"
#include "OGGachaDefinitions.generated.h"

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGGachaPoolEntry
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGContentId IdentityId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGContentId VersionId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName Rarity = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 Weight = 1;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    bool bFeatured = false;
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGGachaBannerDefinition
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGContentId BannerId;

    /** Pity can carry between compatible banners by sharing this category. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName PityCategory = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGContentId CurrencyId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int64 PullCost = 0;

    /**
     * Ordinary one-pull ticket resources accepted by this banner, in authored
     * consumption priority. One compatible ticket is consumed before currency.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TArray<FOGContentId> CompatibleTicketIds;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName TopRarity = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 SoftPityStart = 0;

    /** Added to top-rarity entry weight per pull after SoftPityStart. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 SoftPityBonusPerPullBps = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 HardPity = 70;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    bool bFeaturedGuaranteeAfterMiss = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TArray<FOGGachaPoolEntry> Entries;
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGGachaStateRecord
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId RulerId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName PityCategory = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 PullsSinceTopRarity = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    bool bFeaturedGuarantee = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int64 TotalPulls = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int64 UpdatedWorldTick = 0;
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGGachaPullResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FOGContentId BannerId;

    UPROPERTY(BlueprintReadOnly)
    FOGContentId IdentityId;

    UPROPERTY(BlueprintReadOnly)
    FOGContentId VersionId;

    UPROPERTY(BlueprintReadOnly)
    FName Rarity = NAME_None;

    UPROPERTY(BlueprintReadOnly)
    FOGEntityId ManifestationId;

    UPROPERTY(BlueprintReadOnly)
    bool bTopRarity = false;

    UPROPERTY(BlueprintReadOnly)
    bool bFeatured = false;

    UPROPERTY(BlueprintReadOnly)
    bool bDuplicateIdentity = false;

    /** Actual resource consumed for this pull: ticket when available, else currency. */
    UPROPERTY(BlueprintReadOnly)
    FOGContentId PaymentResourceId;

    UPROPERTY(BlueprintReadOnly)
    bool bUsedTicket = false;

    UPROPERTY(BlueprintReadOnly)
    int64 Seed = 0;

    UPROPERTY(BlueprintReadOnly)
    int64 RngDrawCount = 0;

    UPROPERTY(BlueprintReadOnly)
    FOGGachaStateRecord UpdatedState;
};
