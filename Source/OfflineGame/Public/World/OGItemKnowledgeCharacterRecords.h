#pragma once

#include "CoreMinimal.h"
#include "Content/OGContentId.h"
#include "Core/OGEntityId.h"
#include "OGItemKnowledgeCharacterRecords.generated.h"

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

    /** Persistent provenance/history; transfers never erase this state. */
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

    /** Character/body-definition driven slot identity. */
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

    /** Internal authored relationship value. No universal damage formula exists. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int64 AffinityValue = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGContentId MilestoneId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int64 UpdatedWorldTick = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FString StateJson = TEXT("{}");
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGEquipmentProficiencyRecord
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId OwnerEntityId;

    /** Content-defined proficiency identity; not forced to one item instance. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGContentId ProficiencyId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int64 ProficiencyValue = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGContentId GradeId;

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

    /** Presentation skin only unless its content definition explicitly points to gameplay form data. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGContentId SelectedSkinId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FString OutfitStateJson = TEXT("{}");

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
    FOGContentId MemoryTypeId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 SalienceBps = 0;

    /**
     * Semantic/structured memory state only. Do not store an unlimited verbatim
     * transcript of the entity's entire life.
     */
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
    FOGContentId SimulationTierId;

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

    /** Character/Manifestation entity. Canonical adulthood remains Identity lore. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId CharacterEntityId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGContentId CurrentProfileVariantId;

    /**
     * Mutable authored expression/context only. Deliberately contains no generic
     * eligible, permission or consent boolean.
     */
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

    /** Canonical source death event. Heroic creation never erases or rewrites it. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId DeathEventId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int64 CreatedWorldTick = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGContentId PatternId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName GachaAccessState = FName(TEXT("locked"));

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FString StateJson = TEXT("{}");
};
