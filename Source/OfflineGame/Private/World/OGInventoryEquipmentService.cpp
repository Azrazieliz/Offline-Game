#include "World/OGInventoryEquipmentService.h"

bool FOGInventoryEquipmentService::CreateItem(
    const FOGEntityId& OwnerEntityId,
    const FOGContentId& DefinitionId,
    const FOGContentId& RankId,
    const FOGContentId& QualityId,
    int64 WorldTick,
    FOGEntityId& OutItemId,
    FString& OutError)
{
    OutItemId = FOGEntityId();
    OutError.Reset();

    if (!OwnerEntityId.IsValid() ||
        !DefinitionId.IsValid() ||
        (!RankId.IsEmpty() && !RankId.IsValid()) ||
        (!QualityId.IsEmpty() && !QualityId.IsValid()) ||
        WorldTick < 0)
    {
        OutError = TEXT("Item creation request is invalid.");
        return false;
    }

    FOGItemInstanceRecord Item;
    Item.ItemId = FOGEntityId::NewId();
    Item.DefinitionId = DefinitionId;
    Item.OwnerEntityId = OwnerEntityId;
    Item.CurrentRankId = RankId;
    Item.QualityId = QualityId;
    Item.HistoryStateJson = TEXT("{\"origin\":\"created\"}");

    if (!Store.UpsertItemInstance(
            Item,
            WorldTick,
            OutError))
    {
        return false;
    }

    OutItemId = Item.ItemId;
    return true;
}

bool FOGInventoryEquipmentService::TransferItem(
    const FOGEntityId& ItemId,
    const FOGEntityId& NewOwnerEntityId,
    int64 WorldTick,
    const FOGItemTransferAffinityResolver& AffinityResolver,
    FString& OutError)
{
    OutError.Reset();

    if (!ItemId.IsValid() ||
        !NewOwnerEntityId.IsValid() ||
        WorldTick < 0 ||
        !AffinityResolver)
    {
        OutError = TEXT("Item transfer request is invalid.");
        return false;
    }

    bool bFound = false;
    FOGItemInstanceRecord Item;
    if (!Store.TryReadItemInstance(
            ItemId,
            bFound,
            Item,
            OutError))
    {
        return false;
    }

    if (!bFound)
    {
        OutError = TEXT("Item transfer references an unknown item.");
        return false;
    }

    bool bAffinityFound = false;
    FOGItemOwnerAffinityRecord ExistingAffinity;
    if (!Store.TryReadItemOwnerAffinity(
            ItemId,
            NewOwnerEntityId,
            bAffinityFound,
            ExistingAffinity,
            OutError))
    {
        return false;
    }

    FOGItemOwnerAffinityRecord NewAffinity;
    if (!AffinityResolver(
            Item,
            NewOwnerEntityId,
            bAffinityFound ? &ExistingAffinity : nullptr,
            WorldTick,
            NewAffinity,
            OutError))
    {
        return false;
    }

    if (NewAffinity.ItemId != ItemId ||
        NewAffinity.OwnerEntityId != NewOwnerEntityId ||
        NewAffinity.UpdatedWorldTick != WorldTick)
    {
        OutError = TEXT("Item-authored transfer affinity resolver returned inconsistent identity/timing.");
        return false;
    }

    const bool bOwnerChanged =
        Item.OwnerEntityId !=
            NewOwnerEntityId;

    Item.OwnerEntityId =
        NewOwnerEntityId;

    if (!Store.BeginTransaction(OutError))
    {
        return false;
    }

    if ((bOwnerChanged &&
         (!Store.DeleteEquipmentBindingsForItem(
              ItemId,
              OutError) ||
          !Store.DeleteContainerContentsForItem(
              ItemId,
              OutError))) ||
        !Store.UpsertItemInstance(
            Item,
            WorldTick,
            OutError) ||
        !Store.UpsertItemOwnerAffinity(
            NewAffinity,
            OutError))
    {
        FString RollbackError;
        Store.RollbackTransaction(RollbackError);
        return false;
    }

    if (!Store.CommitTransaction(OutError))
    {
        FString RollbackError;
        Store.RollbackTransaction(RollbackError);
        return false;
    }

    return true;
}

bool FOGInventoryEquipmentService::EquipItem(
    const FOGEntityId& WearerEntityId,
    const FOGContentId& SlotId,
    const FOGEntityId& ItemId,
    const FOGEquipmentSlotValidator& SlotValidator,
    FString& OutError)
{
    OutError.Reset();

    if (!WearerEntityId.IsValid() ||
        !SlotId.IsValid() ||
        !ItemId.IsValid() ||
        !SlotValidator)
    {
        OutError = TEXT("Equipment request is invalid.");
        return false;
    }

    bool bFound = false;
    FOGItemInstanceRecord Item;
    if (!Store.TryReadItemInstance(
            ItemId,
            bFound,
            Item,
            OutError))
    {
        return false;
    }

    if (!bFound)
    {
        OutError = TEXT("Equipment request references an unknown item.");
        return false;
    }

    if (!SlotValidator(
            WearerEntityId,
            SlotId,
            Item,
            OutError))
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("Item is incompatible with the authored equipment slot.");
        }
        return false;
    }

    FOGEquipmentBindingRecord Binding;
    Binding.WearerEntityId = WearerEntityId;
    Binding.SlotId = SlotId;
    Binding.ItemId = ItemId;
    return Store.UpsertEquipmentBinding(
        Binding,
        OutError);
}

bool FOGInventoryEquipmentService::SetProficiency(
    const FOGEquipmentProficiencyRecord& Proficiency,
    FString& OutError)
{
    return Store.UpsertEquipmentProficiency(
        Proficiency,
        OutError);
}

bool FOGInventoryEquipmentService::SetPresentationState(
    const FOGManifestationPresentationStateRecord& Presentation,
    FString& OutError)
{
    return Store.UpsertManifestationPresentationState(
        Presentation,
        OutError);
}
