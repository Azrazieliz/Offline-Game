#pragma once

#include "CoreMinimal.h"
#include "Persistence/OGWorldStore.h"
#include "World/OGItemKnowledgeCharacterRecords.h"

using FOGEquipmentSlotValidator =
    TFunction<bool(
        const FOGEntityId& WearerEntityId,
        const FOGContentId& SlotId,
        const FOGItemInstanceRecord& Item,
        FString& OutError)>;

using FOGItemTransferAffinityResolver =
    TFunction<bool(
        const FOGItemInstanceRecord& ItemBeforeTransfer,
        const FOGEntityId& NewOwnerEntityId,
        const FOGItemOwnerAffinityRecord* ExistingNewOwnerAffinity,
        int64 WorldTick,
        FOGItemOwnerAffinityRecord& OutNewOwnerAffinity,
        FString& OutError)>;

class OFFLINEGAME_API FOGInventoryEquipmentService
{
public:
    explicit FOGInventoryEquipmentService(IOGWorldStore& InStore)
        : Store(InStore)
    {
    }

    bool CreateItem(
        const FOGEntityId& OwnerEntityId,
        const FOGContentId& DefinitionId,
        const FOGContentId& RankId,
        const FOGContentId& QualityId,
        int64 WorldTick,
        FOGEntityId& OutItemId,
        FString& OutError);

    bool TransferItem(
        const FOGEntityId& ItemId,
        const FOGEntityId& NewOwnerEntityId,
        int64 WorldTick,
        const FOGItemTransferAffinityResolver& AffinityResolver,
        FString& OutError);

    bool EquipItem(
        const FOGEntityId& WearerEntityId,
        const FOGContentId& SlotId,
        const FOGEntityId& ItemId,
        const FOGEquipmentSlotValidator& SlotValidator,
        FString& OutError);

    bool SetProficiency(
        const FOGEquipmentProficiencyRecord& Proficiency,
        FString& OutError);

    bool SetPresentationState(
        const FOGManifestationPresentationStateRecord& Presentation,
        FString& OutError);

private:
    IOGWorldStore& Store;
};
