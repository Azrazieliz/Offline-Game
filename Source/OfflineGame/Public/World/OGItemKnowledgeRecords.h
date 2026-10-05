#pragma once

#include "CoreMinimal.h"
#include "Content/OGContentId.h"
#include "Core/OGEntityId.h"
#include "OGItemKnowledgeRecords.generated.h"

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGItemInstanceRecord
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId ItemId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGContentId DefinitionId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId OwnerEntityId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGContentId CurrentRankId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGContentId QualityId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FString DurabilityStateJson = TEXT("{}");

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FString EvolutionStateJson = TEXT("{}");

    /** Persistent item biography/provenance. Transfer never clears this field. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FString HistoryStateJson = TEXT("{}");
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGItemModifierRecord
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId ItemId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGContentId ModifierId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 Ordinal = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FString StateJson = TEXT("{}");
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGEquipmentBindingRecord
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId WearerEntityId;

    /** Body/character-definition authored slot identity; never a universal enum. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGContentId SlotId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId ItemId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FString StateJson = TEXT("{}");
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGInventoryContainerRecord
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId ContainerId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId OwnerEntityId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGContentId ContainerTypeId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FString CapacityStateJson = TEXT("{}");
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGContainerContentRecord
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId ContainerId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId ItemId;

    /** >1 only where the item definition explicitly permits aggregation. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int64 Amount = 1;
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGItemOwnerAffinityRecord
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId ItemId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId OwnerEntityId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int64 InternalAffinityValue = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGContentId CurrentMilestoneId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int64 UpdatedWorldTick = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FString StateJson = TEXT("{}");
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGEntityEquipmentProficiencyRecord
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId OwnerEntityId;

    /** Authored proficiency family, not necessarily one proficiency per item. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGContentId ProficiencyId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int64 InternalProficiencyValue = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGContentId CurrentGradeId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int64 UpdatedWorldTick = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FString StateJson = TEXT("{}");
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGManifestationPresentationStateRecord
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId ManifestationId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGContentId SelectedSkinId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FString OutfitClothingStateJson = TEXT("{}");

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FString PresentationVariantStateJson = TEXT("{}");

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int64 UpdatedWorldTick = 0;
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGOwnedPresentationUnlockRecord
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId OwnerEntityId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGContentId PresentationId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int64 AcquiredWorldTick = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName State = FName(TEXT("owned"));
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGEntityLanguageRecord
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId EntityId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGContentId LanguageId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 SpokenProficiencyBps = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 WrittenProficiencyBps = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FString StateJson = TEXT("{}");
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGSemanticMemoryRecord
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId MemoryId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId OwnerEntityId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId SubjectEntityId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId SourceEventId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName MemoryType = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 SalienceBps = 0;

    /** Compact semantic state only; never an unlimited verbatim life transcript. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FString StateJson = TEXT("{}");
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGNpcPromotionStateRecord
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId EntityId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName CurrentSimulationTier = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int64 PromotedWorldTick = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId ReasonEventId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FString PresentationPackageStateJson = TEXT("{}");
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGCharacterAdultRuntimeStateRecord
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId CharacterEntityId;

    /** Current authored profile/body/form variant; not an access/permission flag. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGContentId CurrentProfileVariantId;

    /** Mutable libido/preferences/context expression state only. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FString MutableContextStateJson = TEXT("{}");

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int64 UpdatedWorldTick = 0;
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGHeroicRecord
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId RecordId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId SourceWorldEntityId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGContentId IdentityId;

    /** Canonical death event remains immutable source history. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId DeathEventId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int64 CreatedWorldTick = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGContentId PatternId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName GachaAccessState = FName(TEXT("sealed"));

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FString StateJson = TEXT("{}");
};
