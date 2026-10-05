#include "Persistence/OGSQLiteWorldStore.h"

#include "sqlite/sqlite3.h"

namespace
{
bool BindItemText(sqlite3_stmt* Statement, int32 Index, const FString& Value)
{
    FTCHARToUTF8 Utf8(*Value);
    return sqlite3_bind_text(Statement, Index, Utf8.Get(), Utf8.Length(), SQLITE_TRANSIENT) == SQLITE_OK;
}

FString ItemColumnText(sqlite3_stmt* Statement, int32 Column)
{
    const unsigned char* Text = sqlite3_column_text(Statement, Column);
    return Text ? UTF8_TO_TCHAR(reinterpret_cast<const char*>(Text)) : FString();
}

FOGEntityId ParseItemEntity(const FString& Value)
{
    if (Value.IsEmpty())
    {
        return FOGEntityId();
    }

    FGuid Guid;
    return FGuid::Parse(Value, Guid) ? FOGEntityId(Guid) : FOGEntityId();
}

bool BindOptionalItemEntity(sqlite3_stmt* Statement, int32 Index, const FOGEntityId& Id)
{
    return Id.IsValid()
        ? BindItemText(Statement, Index, Id.ToString())
        : sqlite3_bind_null(Statement, Index) == SQLITE_OK;
}

bool BindOptionalItemContent(sqlite3_stmt* Statement, int32 Index, const FOGContentId& Id)
{
    return Id.IsEmpty()
        ? sqlite3_bind_null(Statement, Index) == SQLITE_OK
        : BindItemText(Statement, Index, Id.ToString());
}
}

bool FOGSQLiteWorldStore::UpsertItemInstance(
    const FOGItemInstanceRecord& Item,
    int64 CreatedWorldTick,
    FString& OutError)
{
    OutError.Reset();

    if (!Item.ItemId.IsValid() ||
        !Item.DefinitionId.IsValid() ||
        (!Item.CurrentRankId.IsEmpty() && !Item.CurrentRankId.IsValid()) ||
        (!Item.QualityId.IsEmpty() && !Item.QualityId.IsValid()))
    {
        OutError = TEXT("Item instance contains invalid identity/content data.");
        return false;
    }

    const bool bOwnTransaction = !bTransactionActive;
    if (bOwnTransaction && !BeginTransaction(OutError))
    {
        return false;
    }

    auto Fail = [this, bOwnTransaction, &OutError](const FString& Error)
    {
        OutError = Error;
        if (bOwnTransaction)
        {
            FString RollbackError;
            RollbackTransaction(RollbackError);
        }
        return false;
    };

    FString Error;
    if (!UpsertEntity(
            Item.ItemId,
            FName(TEXT("item")),
            CreatedWorldTick,
            TEXT("{}"),
            Error))
    {
        return Fail(Error);
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "INSERT INTO item_instances("
        "item_entity_id, definition_content_id, owner_entity_id, current_rank_content_id, "
        "quality_content_id, durability_state_json, evolution_state_json, history_state_json"
        ") VALUES(?, ?, ?, ?, ?, ?, ?, ?) "
        "ON CONFLICT(item_entity_id) DO UPDATE SET "
        "definition_content_id = excluded.definition_content_id, "
        "owner_entity_id = excluded.owner_entity_id, "
        "current_rank_content_id = excluded.current_rank_content_id, "
        "quality_content_id = excluded.quality_content_id, "
        "durability_state_json = excluded.durability_state_json, "
        "evolution_state_json = excluded.evolution_state_json, "
        "history_state_json = excluded.history_state_json;";

    if (sqlite3_prepare_v2(Database, Sql, -1, &Statement, nullptr) != SQLITE_OK)
    {
        return Fail(LastError(TEXT("Prepare item instance upsert")));
    }

    const bool bSucceeded =
        BindItemText(Statement, 1, Item.ItemId.ToString()) &&
        BindItemText(Statement, 2, Item.DefinitionId.ToString()) &&
        BindOptionalItemEntity(Statement, 3, Item.OwnerEntityId) &&
        BindOptionalItemContent(Statement, 4, Item.CurrentRankId) &&
        BindOptionalItemContent(Statement, 5, Item.QualityId) &&
        BindItemText(Statement, 6, Item.DurabilityStateJson.IsEmpty() ? TEXT("{}") : Item.DurabilityStateJson) &&
        BindItemText(Statement, 7, Item.EvolutionStateJson.IsEmpty() ? TEXT("{}") : Item.EvolutionStateJson) &&
        BindItemText(Statement, 8, Item.HistoryStateJson.IsEmpty() ? TEXT("{}") : Item.HistoryStateJson) &&
        sqlite3_step(Statement) == SQLITE_DONE;

    const FString SqlError = bSucceeded ? FString() : LastError(TEXT("Upsert item instance"));
    sqlite3_finalize(Statement);

    if (!bSucceeded)
    {
        return Fail(SqlError);
    }

    if (bOwnTransaction && !CommitTransaction(OutError))
    {
        FString RollbackError;
        RollbackTransaction(RollbackError);
        return false;
    }

    return true;
}

bool FOGSQLiteWorldStore::TryReadItemInstance(
    const FOGEntityId& ItemId,
    bool& bOutFound,
    FOGItemInstanceRecord& OutItem,
    FString& OutError) const
{
    bOutFound = false;
    OutItem = FOGItemInstanceRecord();
    OutError.Reset();

    if (!ItemId.IsValid())
    {
        OutError = TEXT("Item lookup requires a valid ID.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "SELECT definition_content_id, owner_entity_id, current_rank_content_id, quality_content_id, "
        "durability_state_json, evolution_state_json, history_state_json "
        "FROM item_instances WHERE item_entity_id = ?;";

    if (sqlite3_prepare_v2(Database, Sql, -1, &Statement, nullptr) != SQLITE_OK)
    {
        OutError = LastError(TEXT("Prepare item instance read"));
        return false;
    }

    if (!BindItemText(Statement, 1, ItemId.ToString()))
    {
        OutError = LastError(TEXT("Bind item instance read"));
        sqlite3_finalize(Statement);
        return false;
    }

    const int32 Step = sqlite3_step(Statement);
    if (Step == SQLITE_ROW)
    {
        bOutFound = true;
        OutItem.ItemId = ItemId;
        OutItem.DefinitionId = FOGContentId(ItemColumnText(Statement, 0));
        OutItem.OwnerEntityId = ParseItemEntity(ItemColumnText(Statement, 1));
        OutItem.CurrentRankId = FOGContentId(ItemColumnText(Statement, 2));
        OutItem.QualityId = FOGContentId(ItemColumnText(Statement, 3));
        OutItem.DurabilityStateJson = ItemColumnText(Statement, 4);
        OutItem.EvolutionStateJson = ItemColumnText(Statement, 5);
        OutItem.HistoryStateJson = ItemColumnText(Statement, 6);

        if (!OutItem.DefinitionId.IsValid())
        {
            OutError = TEXT("Stored item has invalid definition content ID.");
            sqlite3_finalize(Statement);
            return false;
        }
    }
    else if (Step != SQLITE_DONE)
    {
        OutError = LastError(TEXT("Read item instance"));
        sqlite3_finalize(Statement);
        return false;
    }

    sqlite3_finalize(Statement);
    return true;
}

bool FOGSQLiteWorldStore::ListItemInstancesByOwner(
    const FOGEntityId& OwnerId,
    TArray<FOGItemInstanceRecord>& OutItems,
    FString& OutError) const
{
    OutItems.Reset();
    OutError.Reset();

    if (!OwnerId.IsValid())
    {
        OutError = TEXT("Item owner list requires a valid owner.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "SELECT item_entity_id FROM item_instances "
        "WHERE owner_entity_id = ? ORDER BY item_entity_id;";

    if (sqlite3_prepare_v2(Database, Sql, -1, &Statement, nullptr) != SQLITE_OK)
    {
        OutError = LastError(TEXT("Prepare item owner list"));
        return false;
    }

    if (!BindItemText(Statement, 1, OwnerId.ToString()))
    {
        OutError = LastError(TEXT("Bind item owner list"));
        sqlite3_finalize(Statement);
        return false;
    }

    TArray<FOGEntityId> Ids;
    for (;;)
    {
        const int32 Step = sqlite3_step(Statement);
        if (Step == SQLITE_DONE)
        {
            break;
        }
        if (Step != SQLITE_ROW)
        {
            OutError = LastError(TEXT("Read item owner list"));
            sqlite3_finalize(Statement);
            return false;
        }

        const FOGEntityId Id = ParseItemEntity(ItemColumnText(Statement, 0));
        if (!Id.IsValid())
        {
            OutError = TEXT("Stored item list contains invalid ID.");
            sqlite3_finalize(Statement);
            return false;
        }
        Ids.Add(Id);
    }
    sqlite3_finalize(Statement);

    for (const FOGEntityId& Id : Ids)
    {
        bool bFound = false;
        FOGItemInstanceRecord Item;
        if (!TryReadItemInstance(Id, bFound, Item, OutError) || !bFound)
        {
            if (OutError.IsEmpty())
            {
                OutError = TEXT("Item disappeared during owner-list read.");
            }
            OutItems.Reset();
            return false;
        }
        OutItems.Add(MoveTemp(Item));
    }

    return true;
}

bool FOGSQLiteWorldStore::UpsertItemModifier(
    const FOGItemModifierRecord& Modifier,
    FString& OutError)
{
    OutError.Reset();

    if (!Modifier.ItemId.IsValid() ||
        !Modifier.ModifierId.IsValid() ||
        Modifier.Ordinal < 0)
    {
        OutError = TEXT("Item modifier is invalid.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "INSERT INTO item_modifiers(item_entity_id, modifier_content_id, ordinal, state_json) "
        "VALUES(?, ?, ?, ?) "
        "ON CONFLICT(item_entity_id, modifier_content_id, ordinal) DO UPDATE SET "
        "state_json = excluded.state_json;";

    if (sqlite3_prepare_v2(Database, Sql, -1, &Statement, nullptr) != SQLITE_OK)
    {
        OutError = LastError(TEXT("Prepare item modifier upsert"));
        return false;
    }

    const bool bSucceeded =
        BindItemText(Statement, 1, Modifier.ItemId.ToString()) &&
        BindItemText(Statement, 2, Modifier.ModifierId.ToString()) &&
        sqlite3_bind_int(Statement, 3, Modifier.Ordinal) == SQLITE_OK &&
        BindItemText(Statement, 4, Modifier.StateJson.IsEmpty() ? TEXT("{}") : Modifier.StateJson) &&
        sqlite3_step(Statement) == SQLITE_DONE;

    if (!bSucceeded)
    {
        OutError = LastError(TEXT("Upsert item modifier"));
    }
    sqlite3_finalize(Statement);
    return bSucceeded;
}

bool FOGSQLiteWorldStore::ListItemModifiers(
    const FOGEntityId& ItemId,
    TArray<FOGItemModifierRecord>& OutModifiers,
    FString& OutError) const
{
    OutModifiers.Reset();
    OutError.Reset();

    if (!ItemId.IsValid())
    {
        OutError = TEXT("Item modifier list requires a valid item.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "SELECT modifier_content_id, ordinal, state_json FROM item_modifiers "
        "WHERE item_entity_id = ? ORDER BY ordinal, modifier_content_id;";

    if (sqlite3_prepare_v2(Database, Sql, -1, &Statement, nullptr) != SQLITE_OK)
    {
        OutError = LastError(TEXT("Prepare item modifier list"));
        return false;
    }

    if (!BindItemText(Statement, 1, ItemId.ToString()))
    {
        OutError = LastError(TEXT("Bind item modifier list"));
        sqlite3_finalize(Statement);
        return false;
    }

    for (;;)
    {
        const int32 Step = sqlite3_step(Statement);
        if (Step == SQLITE_DONE)
        {
            break;
        }
        if (Step != SQLITE_ROW)
        {
            OutError = LastError(TEXT("Read item modifier list"));
            sqlite3_finalize(Statement);
            OutModifiers.Reset();
            return false;
        }

        FOGItemModifierRecord Modifier;
        Modifier.ItemId = ItemId;
        Modifier.ModifierId = FOGContentId(ItemColumnText(Statement, 0));
        Modifier.Ordinal = sqlite3_column_int(Statement, 1);
        Modifier.StateJson = ItemColumnText(Statement, 2);

        if (!Modifier.ModifierId.IsValid())
        {
            OutError = TEXT("Stored item modifier has invalid content ID.");
            sqlite3_finalize(Statement);
            OutModifiers.Reset();
            return false;
        }
        OutModifiers.Add(MoveTemp(Modifier));
    }

    sqlite3_finalize(Statement);
    return true;
}

bool FOGSQLiteWorldStore::UpsertEquipmentBinding(
    const FOGEquipmentBindingRecord& Binding,
    FString& OutError)
{
    OutError.Reset();

    if (!Binding.WearerEntityId.IsValid() ||
        !Binding.SlotId.IsValid() ||
        !Binding.ItemId.IsValid())
    {
        OutError = TEXT("Equipment binding is invalid.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "INSERT INTO equipment_bindings(wearer_entity_id, slot_content_id, item_entity_id, state_json) "
        "VALUES(?, ?, ?, ?) "
        "ON CONFLICT(wearer_entity_id, slot_content_id) DO UPDATE SET "
        "item_entity_id = excluded.item_entity_id, state_json = excluded.state_json;";

    if (sqlite3_prepare_v2(Database, Sql, -1, &Statement, nullptr) != SQLITE_OK)
    {
        OutError = LastError(TEXT("Prepare equipment binding upsert"));
        return false;
    }

    const bool bSucceeded =
        BindItemText(Statement, 1, Binding.WearerEntityId.ToString()) &&
        BindItemText(Statement, 2, Binding.SlotId.ToString()) &&
        BindItemText(Statement, 3, Binding.ItemId.ToString()) &&
        BindItemText(Statement, 4, Binding.StateJson.IsEmpty() ? TEXT("{}") : Binding.StateJson) &&
        sqlite3_step(Statement) == SQLITE_DONE;

    if (!bSucceeded)
    {
        OutError = LastError(TEXT("Upsert equipment binding"));
    }
    sqlite3_finalize(Statement);
    return bSucceeded;
}

bool FOGSQLiteWorldStore::TryReadEquipmentBinding(
    const FOGEntityId& WearerId,
    const FOGContentId& SlotId,
    bool& bOutFound,
    FOGEquipmentBindingRecord& OutBinding,
    FString& OutError) const
{
    bOutFound = false;
    OutBinding = FOGEquipmentBindingRecord();
    OutError.Reset();

    if (!WearerId.IsValid() || !SlotId.IsValid())
    {
        OutError = TEXT("Equipment binding lookup is invalid.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "SELECT item_entity_id, state_json FROM equipment_bindings "
        "WHERE wearer_entity_id = ? AND slot_content_id = ?;";

    if (sqlite3_prepare_v2(Database, Sql, -1, &Statement, nullptr) != SQLITE_OK)
    {
        OutError = LastError(TEXT("Prepare equipment binding read"));
        return false;
    }

    if (!BindItemText(Statement, 1, WearerId.ToString()) ||
        !BindItemText(Statement, 2, SlotId.ToString()))
    {
        OutError = LastError(TEXT("Bind equipment binding read"));
        sqlite3_finalize(Statement);
        return false;
    }

    const int32 Step = sqlite3_step(Statement);
    if (Step == SQLITE_ROW)
    {
        bOutFound = true;
        OutBinding.WearerEntityId = WearerId;
        OutBinding.SlotId = SlotId;
        OutBinding.ItemId = ParseItemEntity(ItemColumnText(Statement, 0));
        OutBinding.StateJson = ItemColumnText(Statement, 1);

        if (!OutBinding.ItemId.IsValid())
        {
            OutError = TEXT("Stored equipment binding has invalid item ID.");
            sqlite3_finalize(Statement);
            return false;
        }
    }
    else if (Step != SQLITE_DONE)
    {
        OutError = LastError(TEXT("Read equipment binding"));
        sqlite3_finalize(Statement);
        return false;
    }

    sqlite3_finalize(Statement);
    return true;
}

bool FOGSQLiteWorldStore::ListEquipmentBindings(
    const FOGEntityId& WearerId,
    TArray<FOGEquipmentBindingRecord>& OutBindings,
    FString& OutError) const
{
    OutBindings.Reset();
    OutError.Reset();

    if (!WearerId.IsValid())
    {
        OutError = TEXT("Equipment binding list requires a valid wearer.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "SELECT slot_content_id FROM equipment_bindings "
        "WHERE wearer_entity_id = ? ORDER BY slot_content_id;";

    if (sqlite3_prepare_v2(Database, Sql, -1, &Statement, nullptr) != SQLITE_OK)
    {
        OutError = LastError(TEXT("Prepare equipment binding list"));
        return false;
    }

    if (!BindItemText(Statement, 1, WearerId.ToString()))
    {
        OutError = LastError(TEXT("Bind equipment binding list"));
        sqlite3_finalize(Statement);
        return false;
    }

    TArray<FOGContentId> Slots;
    for (;;)
    {
        const int32 Step = sqlite3_step(Statement);
        if (Step == SQLITE_DONE)
        {
            break;
        }
        if (Step != SQLITE_ROW)
        {
            OutError = LastError(TEXT("Read equipment binding list"));
            sqlite3_finalize(Statement);
            return false;
        }

        FOGContentId Slot(ItemColumnText(Statement, 0));
        if (!Slot.IsValid())
        {
            OutError = TEXT("Stored equipment slot ID is invalid.");
            sqlite3_finalize(Statement);
            return false;
        }
        Slots.Add(MoveTemp(Slot));
    }
    sqlite3_finalize(Statement);

    for (const FOGContentId& Slot : Slots)
    {
        bool bFound = false;
        FOGEquipmentBindingRecord Binding;
        if (!TryReadEquipmentBinding(WearerId, Slot, bFound, Binding, OutError) || !bFound)
        {
            if (OutError.IsEmpty())
            {
                OutError = TEXT("Equipment binding disappeared during list read.");
            }
            OutBindings.Reset();
            return false;
        }
        OutBindings.Add(MoveTemp(Binding));
    }
    return true;
}

bool FOGSQLiteWorldStore::DeleteEquipmentBindingsForItem(
    const FOGEntityId& ItemId,
    FString& OutError)
{
    OutError.Reset();

    if (!ItemId.IsValid())
    {
        OutError =
            TEXT("Equipment-binding cleanup requires a valid item ID.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "DELETE FROM equipment_bindings WHERE item_entity_id = ?;";

    if (sqlite3_prepare_v2(
            Database,
            Sql,
            -1,
            &Statement,
            nullptr) != SQLITE_OK)
    {
        OutError =
            LastError(
                TEXT("Prepare equipment-binding cleanup"));
        return false;
    }

    const bool bSucceeded =
        BindItemText(
            Statement,
            1,
            ItemId.ToString()) &&
        sqlite3_step(
            Statement) == SQLITE_DONE;

    if (!bSucceeded)
    {
        OutError =
            LastError(
                TEXT("Delete equipment bindings for item"));
    }

    sqlite3_finalize(
        Statement);
    return bSucceeded;
}

bool FOGSQLiteWorldStore::UpsertInventoryContainer(
    const FOGInventoryContainerRecord& Container,
    int64 CreatedWorldTick,
    FString& OutError)
{
    OutError.Reset();

    if (!Container.ContainerId.IsValid() ||
        !Container.OwnerEntityId.IsValid() ||
        !Container.ContainerTypeId.IsValid())
    {
        OutError = TEXT("Inventory container is invalid.");
        return false;
    }

    const bool bOwnTransaction = !bTransactionActive;
    if (bOwnTransaction && !BeginTransaction(OutError))
    {
        return false;
    }

    auto Fail = [this, bOwnTransaction, &OutError](const FString& Error)
    {
        OutError = Error;
        if (bOwnTransaction)
        {
            FString RollbackError;
            RollbackTransaction(RollbackError);
        }
        return false;
    };

    FString Error;
    if (!UpsertEntity(
            Container.ContainerId,
            FName(TEXT("inventory_container")),
            CreatedWorldTick,
            TEXT("{}"),
            Error))
    {
        return Fail(Error);
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "INSERT INTO inventory_containers(container_entity_id, owner_entity_id, container_type_content_id, capacity_state_json) "
        "VALUES(?, ?, ?, ?) "
        "ON CONFLICT(container_entity_id) DO UPDATE SET "
        "owner_entity_id = excluded.owner_entity_id, "
        "container_type_content_id = excluded.container_type_content_id, "
        "capacity_state_json = excluded.capacity_state_json;";

    if (sqlite3_prepare_v2(Database, Sql, -1, &Statement, nullptr) != SQLITE_OK)
    {
        return Fail(LastError(TEXT("Prepare inventory container upsert")));
    }

    const bool bSucceeded =
        BindItemText(Statement, 1, Container.ContainerId.ToString()) &&
        BindItemText(Statement, 2, Container.OwnerEntityId.ToString()) &&
        BindItemText(Statement, 3, Container.ContainerTypeId.ToString()) &&
        BindItemText(Statement, 4, Container.CapacityStateJson.IsEmpty() ? TEXT("{}") : Container.CapacityStateJson) &&
        sqlite3_step(Statement) == SQLITE_DONE;

    const FString SqlError = bSucceeded ? FString() : LastError(TEXT("Upsert inventory container"));
    sqlite3_finalize(Statement);

    if (!bSucceeded)
    {
        return Fail(SqlError);
    }

    if (bOwnTransaction && !CommitTransaction(OutError))
    {
        FString RollbackError;
        RollbackTransaction(RollbackError);
        return false;
    }
    return true;
}

bool FOGSQLiteWorldStore::TryReadInventoryContainer(
    const FOGEntityId& ContainerId,
    bool& bOutFound,
    FOGInventoryContainerRecord& OutContainer,
    FString& OutError) const
{
    bOutFound = false;
    OutContainer = FOGInventoryContainerRecord();
    OutError.Reset();

    if (!ContainerId.IsValid())
    {
        OutError = TEXT("Inventory container lookup requires a valid ID.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "SELECT owner_entity_id, container_type_content_id, capacity_state_json "
        "FROM inventory_containers WHERE container_entity_id = ?;";

    if (sqlite3_prepare_v2(Database, Sql, -1, &Statement, nullptr) != SQLITE_OK)
    {
        OutError = LastError(TEXT("Prepare inventory container read"));
        return false;
    }

    if (!BindItemText(Statement, 1, ContainerId.ToString()))
    {
        OutError = LastError(TEXT("Bind inventory container read"));
        sqlite3_finalize(Statement);
        return false;
    }

    const int32 Step = sqlite3_step(Statement);
    if (Step == SQLITE_ROW)
    {
        bOutFound = true;
        OutContainer.ContainerId = ContainerId;
        OutContainer.OwnerEntityId = ParseItemEntity(ItemColumnText(Statement, 0));
        OutContainer.ContainerTypeId = FOGContentId(ItemColumnText(Statement, 1));
        OutContainer.CapacityStateJson = ItemColumnText(Statement, 2);

        if (!OutContainer.OwnerEntityId.IsValid() || !OutContainer.ContainerTypeId.IsValid())
        {
            OutError = TEXT("Stored inventory container is invalid.");
            sqlite3_finalize(Statement);
            return false;
        }
    }
    else if (Step != SQLITE_DONE)
    {
        OutError = LastError(TEXT("Read inventory container"));
        sqlite3_finalize(Statement);
        return false;
    }

    sqlite3_finalize(Statement);
    return true;
}

bool FOGSQLiteWorldStore::ListInventoryContainersByOwner(
    const FOGEntityId& OwnerId,
    TArray<FOGInventoryContainerRecord>& OutContainers,
    FString& OutError) const
{
    OutContainers.Reset();
    OutError.Reset();

    if (!OwnerId.IsValid())
    {
        OutError = TEXT("Inventory container list requires a valid owner.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "SELECT container_entity_id FROM inventory_containers "
        "WHERE owner_entity_id = ? ORDER BY container_entity_id;";

    if (sqlite3_prepare_v2(Database, Sql, -1, &Statement, nullptr) != SQLITE_OK)
    {
        OutError = LastError(TEXT("Prepare inventory container list"));
        return false;
    }

    if (!BindItemText(Statement, 1, OwnerId.ToString()))
    {
        OutError = LastError(TEXT("Bind inventory container list"));
        sqlite3_finalize(Statement);
        return false;
    }

    TArray<FOGEntityId> Ids;
    for (;;)
    {
        const int32 Step = sqlite3_step(Statement);
        if (Step == SQLITE_DONE)
        {
            break;
        }
        if (Step != SQLITE_ROW)
        {
            OutError = LastError(TEXT("Read inventory container list"));
            sqlite3_finalize(Statement);
            return false;
        }

        const FOGEntityId Id = ParseItemEntity(ItemColumnText(Statement, 0));
        if (!Id.IsValid())
        {
            OutError = TEXT("Stored inventory container list has invalid ID.");
            sqlite3_finalize(Statement);
            return false;
        }
        Ids.Add(Id);
    }
    sqlite3_finalize(Statement);

    for (const FOGEntityId& Id : Ids)
    {
        bool bFound = false;
        FOGInventoryContainerRecord Container;
        if (!TryReadInventoryContainer(Id, bFound, Container, OutError) || !bFound)
        {
            if (OutError.IsEmpty())
            {
                OutError = TEXT("Inventory container disappeared during list read.");
            }
            OutContainers.Reset();
            return false;
        }
        OutContainers.Add(MoveTemp(Container));
    }
    return true;
}

bool FOGSQLiteWorldStore::UpsertContainerContent(
    const FOGContainerContentRecord& Content,
    FString& OutError)
{
    OutError.Reset();

    if (!Content.ContainerId.IsValid() ||
        !Content.ItemId.IsValid() ||
        Content.Amount <= 0)
    {
        OutError = TEXT("Container content is invalid.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "INSERT INTO container_contents(container_entity_id, item_entity_id, amount) "
        "VALUES(?, ?, ?) "
        "ON CONFLICT(container_entity_id, item_entity_id) DO UPDATE SET amount = excluded.amount;";

    if (sqlite3_prepare_v2(Database, Sql, -1, &Statement, nullptr) != SQLITE_OK)
    {
        OutError = LastError(TEXT("Prepare container content upsert"));
        return false;
    }

    const bool bSucceeded =
        BindItemText(Statement, 1, Content.ContainerId.ToString()) &&
        BindItemText(Statement, 2, Content.ItemId.ToString()) &&
        sqlite3_bind_int64(Statement, 3, Content.Amount) == SQLITE_OK &&
        sqlite3_step(Statement) == SQLITE_DONE;

    if (!bSucceeded)
    {
        OutError = LastError(TEXT("Upsert container content"));
    }
    sqlite3_finalize(Statement);
    return bSucceeded;
}

bool FOGSQLiteWorldStore::ListContainerContents(
    const FOGEntityId& ContainerId,
    TArray<FOGContainerContentRecord>& OutContents,
    FString& OutError) const
{
    OutContents.Reset();
    OutError.Reset();

    if (!ContainerId.IsValid())
    {
        OutError = TEXT("Container content list requires a valid container.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "SELECT item_entity_id, amount FROM container_contents "
        "WHERE container_entity_id = ? ORDER BY item_entity_id;";

    if (sqlite3_prepare_v2(Database, Sql, -1, &Statement, nullptr) != SQLITE_OK)
    {
        OutError = LastError(TEXT("Prepare container content list"));
        return false;
    }

    if (!BindItemText(Statement, 1, ContainerId.ToString()))
    {
        OutError = LastError(TEXT("Bind container content list"));
        sqlite3_finalize(Statement);
        return false;
    }

    for (;;)
    {
        const int32 Step = sqlite3_step(Statement);
        if (Step == SQLITE_DONE)
        {
            break;
        }
        if (Step != SQLITE_ROW)
        {
            OutError = LastError(TEXT("Read container content list"));
            sqlite3_finalize(Statement);
            OutContents.Reset();
            return false;
        }

        FOGContainerContentRecord Row;
        Row.ContainerId = ContainerId;
        Row.ItemId = ParseItemEntity(ItemColumnText(Statement, 0));
        Row.Amount = sqlite3_column_int64(Statement, 1);

        if (!Row.ItemId.IsValid() || Row.Amount <= 0)
        {
            OutError = TEXT("Stored container content is invalid.");
            sqlite3_finalize(Statement);
            OutContents.Reset();
            return false;
        }
        OutContents.Add(MoveTemp(Row));
    }

    sqlite3_finalize(Statement);
    return true;
}

bool FOGSQLiteWorldStore::DeleteContainerContentsForItem(
    const FOGEntityId& ItemId,
    FString& OutError)
{
    OutError.Reset();

    if (!ItemId.IsValid())
    {
        OutError =
            TEXT("Container-content cleanup requires a valid item ID.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "DELETE FROM container_contents WHERE item_entity_id = ?;";

    if (sqlite3_prepare_v2(
            Database,
            Sql,
            -1,
            &Statement,
            nullptr) != SQLITE_OK)
    {
        OutError =
            LastError(
                TEXT("Prepare container-content cleanup"));
        return false;
    }

    const bool bSucceeded =
        BindItemText(
            Statement,
            1,
            ItemId.ToString()) &&
        sqlite3_step(
            Statement) == SQLITE_DONE;

    if (!bSucceeded)
    {
        OutError =
            LastError(
                TEXT("Delete container contents for item"));
    }

    sqlite3_finalize(
        Statement);
    return bSucceeded;
}

bool FOGSQLiteWorldStore::UpsertItemOwnerAffinity(
    const FOGItemOwnerAffinityRecord& Affinity,
    FString& OutError)
{
    OutError.Reset();

    if (!Affinity.ItemId.IsValid() ||
        !Affinity.OwnerEntityId.IsValid() ||
        (!Affinity.MilestoneId.IsEmpty() && !Affinity.MilestoneId.IsValid()) ||
        Affinity.UpdatedWorldTick < 0)
    {
        OutError = TEXT("Item-owner affinity is invalid.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "INSERT INTO item_owner_affinity("
        "item_entity_id, owner_entity_id, affinity_value, milestone_content_id, updated_world_tick, state_json"
        ") VALUES(?, ?, ?, ?, ?, ?) "
        "ON CONFLICT(item_entity_id, owner_entity_id) DO UPDATE SET "
        "affinity_value = excluded.affinity_value, milestone_content_id = excluded.milestone_content_id, "
        "updated_world_tick = excluded.updated_world_tick, state_json = excluded.state_json;";

    if (sqlite3_prepare_v2(Database, Sql, -1, &Statement, nullptr) != SQLITE_OK)
    {
        OutError = LastError(TEXT("Prepare item-owner affinity upsert"));
        return false;
    }

    const bool bSucceeded =
        BindItemText(Statement, 1, Affinity.ItemId.ToString()) &&
        BindItemText(Statement, 2, Affinity.OwnerEntityId.ToString()) &&
        sqlite3_bind_int64(Statement, 3, Affinity.AffinityValue) == SQLITE_OK &&
        BindOptionalItemContent(Statement, 4, Affinity.MilestoneId) &&
        sqlite3_bind_int64(Statement, 5, Affinity.UpdatedWorldTick) == SQLITE_OK &&
        BindItemText(Statement, 6, Affinity.StateJson.IsEmpty() ? TEXT("{}") : Affinity.StateJson) &&
        sqlite3_step(Statement) == SQLITE_DONE;

    if (!bSucceeded)
    {
        OutError = LastError(TEXT("Upsert item-owner affinity"));
    }
    sqlite3_finalize(Statement);
    return bSucceeded;
}

bool FOGSQLiteWorldStore::TryReadItemOwnerAffinity(
    const FOGEntityId& ItemId,
    const FOGEntityId& OwnerId,
    bool& bOutFound,
    FOGItemOwnerAffinityRecord& OutAffinity,
    FString& OutError) const
{
    bOutFound = false;
    OutAffinity = FOGItemOwnerAffinityRecord();
    OutError.Reset();

    if (!ItemId.IsValid() || !OwnerId.IsValid())
    {
        OutError = TEXT("Item-owner affinity lookup is invalid.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "SELECT affinity_value, milestone_content_id, updated_world_tick, state_json "
        "FROM item_owner_affinity WHERE item_entity_id = ? AND owner_entity_id = ?;";

    if (sqlite3_prepare_v2(Database, Sql, -1, &Statement, nullptr) != SQLITE_OK)
    {
        OutError = LastError(TEXT("Prepare item-owner affinity read"));
        return false;
    }

    if (!BindItemText(Statement, 1, ItemId.ToString()) ||
        !BindItemText(Statement, 2, OwnerId.ToString()))
    {
        OutError = LastError(TEXT("Bind item-owner affinity read"));
        sqlite3_finalize(Statement);
        return false;
    }

    const int32 Step = sqlite3_step(Statement);
    if (Step == SQLITE_ROW)
    {
        bOutFound = true;
        OutAffinity.ItemId = ItemId;
        OutAffinity.OwnerEntityId = OwnerId;
        OutAffinity.AffinityValue = sqlite3_column_int64(Statement, 0);
        OutAffinity.MilestoneId = FOGContentId(ItemColumnText(Statement, 1));
        OutAffinity.UpdatedWorldTick = sqlite3_column_int64(Statement, 2);
        OutAffinity.StateJson = ItemColumnText(Statement, 3);
    }
    else if (Step != SQLITE_DONE)
    {
        OutError = LastError(TEXT("Read item-owner affinity"));
        sqlite3_finalize(Statement);
        return false;
    }

    sqlite3_finalize(Statement);
    return true;
}

bool FOGSQLiteWorldStore::UpsertEquipmentProficiency(
    const FOGEquipmentProficiencyRecord& Proficiency,
    FString& OutError)
{
    OutError.Reset();

    if (!Proficiency.OwnerEntityId.IsValid() ||
        !Proficiency.ProficiencyId.IsValid() ||
        (!Proficiency.GradeId.IsEmpty() && !Proficiency.GradeId.IsValid()) ||
        Proficiency.UpdatedWorldTick < 0)
    {
        OutError = TEXT("Equipment proficiency is invalid.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "INSERT INTO entity_equipment_proficiency("
        "owner_entity_id, proficiency_content_id, proficiency_value, grade_content_id, updated_world_tick, state_json"
        ") VALUES(?, ?, ?, ?, ?, ?) "
        "ON CONFLICT(owner_entity_id, proficiency_content_id) DO UPDATE SET "
        "proficiency_value = excluded.proficiency_value, grade_content_id = excluded.grade_content_id, "
        "updated_world_tick = excluded.updated_world_tick, state_json = excluded.state_json;";

    if (sqlite3_prepare_v2(Database, Sql, -1, &Statement, nullptr) != SQLITE_OK)
    {
        OutError = LastError(TEXT("Prepare equipment proficiency upsert"));
        return false;
    }

    const bool bSucceeded =
        BindItemText(Statement, 1, Proficiency.OwnerEntityId.ToString()) &&
        BindItemText(Statement, 2, Proficiency.ProficiencyId.ToString()) &&
        sqlite3_bind_int64(Statement, 3, Proficiency.ProficiencyValue) == SQLITE_OK &&
        BindOptionalItemContent(Statement, 4, Proficiency.GradeId) &&
        sqlite3_bind_int64(Statement, 5, Proficiency.UpdatedWorldTick) == SQLITE_OK &&
        BindItemText(Statement, 6, Proficiency.StateJson.IsEmpty() ? TEXT("{}") : Proficiency.StateJson) &&
        sqlite3_step(Statement) == SQLITE_DONE;

    if (!bSucceeded)
    {
        OutError = LastError(TEXT("Upsert equipment proficiency"));
    }
    sqlite3_finalize(Statement);
    return bSucceeded;
}

bool FOGSQLiteWorldStore::TryReadEquipmentProficiency(
    const FOGEntityId& OwnerId,
    const FOGContentId& ProficiencyId,
    bool& bOutFound,
    FOGEquipmentProficiencyRecord& OutProficiency,
    FString& OutError) const
{
    bOutFound = false;
    OutProficiency = FOGEquipmentProficiencyRecord();
    OutError.Reset();

    if (!OwnerId.IsValid() || !ProficiencyId.IsValid())
    {
        OutError = TEXT("Equipment proficiency lookup is invalid.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "SELECT proficiency_value, grade_content_id, updated_world_tick, state_json "
        "FROM entity_equipment_proficiency WHERE owner_entity_id = ? AND proficiency_content_id = ?;";

    if (sqlite3_prepare_v2(Database, Sql, -1, &Statement, nullptr) != SQLITE_OK)
    {
        OutError = LastError(TEXT("Prepare equipment proficiency read"));
        return false;
    }

    if (!BindItemText(Statement, 1, OwnerId.ToString()) ||
        !BindItemText(Statement, 2, ProficiencyId.ToString()))
    {
        OutError = LastError(TEXT("Bind equipment proficiency read"));
        sqlite3_finalize(Statement);
        return false;
    }

    const int32 Step = sqlite3_step(Statement);
    if (Step == SQLITE_ROW)
    {
        bOutFound = true;
        OutProficiency.OwnerEntityId = OwnerId;
        OutProficiency.ProficiencyId = ProficiencyId;
        OutProficiency.ProficiencyValue = sqlite3_column_int64(Statement, 0);
        OutProficiency.GradeId = FOGContentId(ItemColumnText(Statement, 1));
        OutProficiency.UpdatedWorldTick = sqlite3_column_int64(Statement, 2);
        OutProficiency.StateJson = ItemColumnText(Statement, 3);
    }
    else if (Step != SQLITE_DONE)
    {
        OutError = LastError(TEXT("Read equipment proficiency"));
        sqlite3_finalize(Statement);
        return false;
    }

    sqlite3_finalize(Statement);
    return true;
}

bool FOGSQLiteWorldStore::ListEquipmentProficienciesByOwner(
    const FOGEntityId& OwnerId,
    TArray<FOGEquipmentProficiencyRecord>& OutProficiencies,
    FString& OutError) const
{
    OutProficiencies.Reset();
    OutError.Reset();

    if (!OwnerId.IsValid())
    {
        OutError = TEXT("Equipment proficiency list requires a valid owner.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "SELECT proficiency_content_id, proficiency_value, grade_content_id, updated_world_tick, state_json "
        "FROM entity_equipment_proficiency WHERE owner_entity_id = ? "
        "ORDER BY proficiency_content_id;";

    if (sqlite3_prepare_v2(Database, Sql, -1, &Statement, nullptr) != SQLITE_OK)
    {
        OutError = LastError(TEXT("Prepare equipment proficiency list"));
        return false;
    }

    if (!BindItemText(Statement, 1, OwnerId.ToString()))
    {
        OutError = LastError(TEXT("Bind equipment proficiency list"));
        sqlite3_finalize(Statement);
        return false;
    }

    for (;;)
    {
        const int32 Step = sqlite3_step(Statement);
        if (Step == SQLITE_DONE)
        {
            break;
        }
        if (Step != SQLITE_ROW)
        {
            OutError = LastError(TEXT("Read equipment proficiency list"));
            sqlite3_finalize(Statement);
            OutProficiencies.Reset();
            return false;
        }

        FOGEquipmentProficiencyRecord Row;
        Row.OwnerEntityId = OwnerId;
        Row.ProficiencyId =
            FOGContentId(ItemColumnText(Statement, 0));
        Row.ProficiencyValue =
            sqlite3_column_int64(Statement, 1);
        Row.GradeId =
            FOGContentId(ItemColumnText(Statement, 2));
        Row.UpdatedWorldTick =
            sqlite3_column_int64(Statement, 3);
        Row.StateJson =
            ItemColumnText(Statement, 4);

        if (!Row.ProficiencyId.IsValid() ||
            (!Row.GradeId.IsEmpty() &&
             !Row.GradeId.IsValid()))
        {
            OutError = TEXT("Stored equipment proficiency list contains invalid content IDs.");
            sqlite3_finalize(Statement);
            OutProficiencies.Reset();
            return false;
        }

        OutProficiencies.Add(MoveTemp(Row));
    }

    sqlite3_finalize(Statement);
    return true;
}

bool FOGSQLiteWorldStore::UpsertManifestationPresentationState(
    const FOGManifestationPresentationStateRecord& State,
    FString& OutError)
{
    OutError.Reset();

    if (!State.ManifestationId.IsValid() ||
        (!State.SelectedSkinId.IsEmpty() && !State.SelectedSkinId.IsValid()) ||
        State.UpdatedWorldTick < 0)
    {
        OutError = TEXT("Manifestation presentation state is invalid.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "INSERT INTO manifestation_presentation_state("
        "manifestation_entity_id, selected_skin_content_id, outfit_state_json, presentation_variant_state_json, updated_world_tick"
        ") VALUES(?, ?, ?, ?, ?) "
        "ON CONFLICT(manifestation_entity_id) DO UPDATE SET "
        "selected_skin_content_id = excluded.selected_skin_content_id, "
        "outfit_state_json = excluded.outfit_state_json, "
        "presentation_variant_state_json = excluded.presentation_variant_state_json, "
        "updated_world_tick = excluded.updated_world_tick;";

    if (sqlite3_prepare_v2(Database, Sql, -1, &Statement, nullptr) != SQLITE_OK)
    {
        OutError = LastError(TEXT("Prepare Manifestation presentation upsert"));
        return false;
    }

    const bool bSucceeded =
        BindItemText(Statement, 1, State.ManifestationId.ToString()) &&
        BindOptionalItemContent(Statement, 2, State.SelectedSkinId) &&
        BindItemText(Statement, 3, State.OutfitStateJson.IsEmpty() ? TEXT("{}") : State.OutfitStateJson) &&
        BindItemText(Statement, 4, State.PresentationVariantStateJson.IsEmpty() ? TEXT("{}") : State.PresentationVariantStateJson) &&
        sqlite3_bind_int64(Statement, 5, State.UpdatedWorldTick) == SQLITE_OK &&
        sqlite3_step(Statement) == SQLITE_DONE;

    if (!bSucceeded)
    {
        OutError = LastError(TEXT("Upsert Manifestation presentation"));
    }
    sqlite3_finalize(Statement);
    return bSucceeded;
}

bool FOGSQLiteWorldStore::TryReadManifestationPresentationState(
    const FOGEntityId& ManifestationId,
    bool& bOutFound,
    FOGManifestationPresentationStateRecord& OutState,
    FString& OutError) const
{
    bOutFound = false;
    OutState = FOGManifestationPresentationStateRecord();
    OutError.Reset();

    if (!ManifestationId.IsValid())
    {
        OutError = TEXT("Manifestation presentation lookup requires a valid ID.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "SELECT selected_skin_content_id, outfit_state_json, presentation_variant_state_json, updated_world_tick "
        "FROM manifestation_presentation_state WHERE manifestation_entity_id = ?;";

    if (sqlite3_prepare_v2(Database, Sql, -1, &Statement, nullptr) != SQLITE_OK)
    {
        OutError = LastError(TEXT("Prepare Manifestation presentation read"));
        return false;
    }

    if (!BindItemText(Statement, 1, ManifestationId.ToString()))
    {
        OutError = LastError(TEXT("Bind Manifestation presentation read"));
        sqlite3_finalize(Statement);
        return false;
    }

    const int32 Step = sqlite3_step(Statement);
    if (Step == SQLITE_ROW)
    {
        bOutFound = true;
        OutState.ManifestationId = ManifestationId;
        OutState.SelectedSkinId = FOGContentId(ItemColumnText(Statement, 0));
        OutState.OutfitStateJson = ItemColumnText(Statement, 1);
        OutState.PresentationVariantStateJson = ItemColumnText(Statement, 2);
        OutState.UpdatedWorldTick = sqlite3_column_int64(Statement, 3);
    }
    else if (Step != SQLITE_DONE)
    {
        OutError = LastError(TEXT("Read Manifestation presentation"));
        sqlite3_finalize(Statement);
        return false;
    }

    sqlite3_finalize(Statement);
    return true;
}

bool FOGSQLiteWorldStore::UpsertOwnedPresentationUnlock(
    const FOGOwnedPresentationUnlockRecord& Unlock,
    FString& OutError)
{
    OutError.Reset();

    if (!Unlock.OwnerEntityId.IsValid() ||
        !Unlock.PresentationId.IsValid() ||
        Unlock.AcquiredWorldTick < 0 ||
        Unlock.State.IsNone())
    {
        OutError = TEXT("Owned presentation unlock is invalid.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "INSERT INTO owned_presentation_unlocks(owner_entity_id, presentation_content_id, acquired_world_tick, state) "
        "VALUES(?, ?, ?, ?) "
        "ON CONFLICT(owner_entity_id, presentation_content_id) DO UPDATE SET "
        "acquired_world_tick = MIN(owned_presentation_unlocks.acquired_world_tick, excluded.acquired_world_tick), "
        "state = excluded.state;";

    if (sqlite3_prepare_v2(Database, Sql, -1, &Statement, nullptr) != SQLITE_OK)
    {
        OutError = LastError(TEXT("Prepare presentation unlock upsert"));
        return false;
    }

    const bool bSucceeded =
        BindItemText(Statement, 1, Unlock.OwnerEntityId.ToString()) &&
        BindItemText(Statement, 2, Unlock.PresentationId.ToString()) &&
        sqlite3_bind_int64(Statement, 3, Unlock.AcquiredWorldTick) == SQLITE_OK &&
        BindItemText(Statement, 4, Unlock.State.ToString()) &&
        sqlite3_step(Statement) == SQLITE_DONE;

    if (!bSucceeded)
    {
        OutError = LastError(TEXT("Upsert presentation unlock"));
    }
    sqlite3_finalize(Statement);
    return bSucceeded;
}

bool FOGSQLiteWorldStore::ListOwnedPresentationUnlocks(
    const FOGEntityId& OwnerId,
    TArray<FOGOwnedPresentationUnlockRecord>& OutUnlocks,
    FString& OutError) const
{
    OutUnlocks.Reset();
    OutError.Reset();

    if (!OwnerId.IsValid())
    {
        OutError = TEXT("Presentation unlock list requires a valid owner.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "SELECT presentation_content_id, acquired_world_tick, state "
        "FROM owned_presentation_unlocks WHERE owner_entity_id = ? "
        "ORDER BY acquired_world_tick, presentation_content_id;";

    if (sqlite3_prepare_v2(Database, Sql, -1, &Statement, nullptr) != SQLITE_OK)
    {
        OutError = LastError(TEXT("Prepare presentation unlock list"));
        return false;
    }

    if (!BindItemText(Statement, 1, OwnerId.ToString()))
    {
        OutError = LastError(TEXT("Bind presentation unlock list"));
        sqlite3_finalize(Statement);
        return false;
    }

    for (;;)
    {
        const int32 Step = sqlite3_step(Statement);
        if (Step == SQLITE_DONE)
        {
            break;
        }
        if (Step != SQLITE_ROW)
        {
            OutError = LastError(TEXT("Read presentation unlock list"));
            sqlite3_finalize(Statement);
            OutUnlocks.Reset();
            return false;
        }

        FOGOwnedPresentationUnlockRecord Unlock;
        Unlock.OwnerEntityId = OwnerId;
        Unlock.PresentationId = FOGContentId(ItemColumnText(Statement, 0));
        Unlock.AcquiredWorldTick = sqlite3_column_int64(Statement, 1);
        Unlock.State = FName(*ItemColumnText(Statement, 2));

        if (!Unlock.PresentationId.IsValid() || Unlock.State.IsNone())
        {
            OutError = TEXT("Stored presentation unlock is invalid.");
            sqlite3_finalize(Statement);
            OutUnlocks.Reset();
            return false;
        }
        OutUnlocks.Add(MoveTemp(Unlock));
    }

    sqlite3_finalize(Statement);
    return true;
}
