#include "Persistence/OGSQLiteWorldStore.h"

#include "sqlite/sqlite3.h"

namespace
{
bool BindCharacterText(sqlite3_stmt* Statement, int32 Index, const FString& Value)
{
    FTCHARToUTF8 Utf8(*Value);
    return sqlite3_bind_text(Statement, Index, Utf8.Get(), Utf8.Length(), SQLITE_TRANSIENT) == SQLITE_OK;
}

FString CharacterColumnText(sqlite3_stmt* Statement, int32 Column)
{
    const unsigned char* Text = sqlite3_column_text(Statement, Column);
    return Text ? UTF8_TO_TCHAR(reinterpret_cast<const char*>(Text)) : FString();
}

FOGEntityId ParseCharacterEntity(const FString& Value)
{
    if (Value.IsEmpty())
    {
        return FOGEntityId();
    }
    FGuid Guid;
    return FGuid::Parse(Value, Guid) ? FOGEntityId(Guid) : FOGEntityId();
}

bool BindOptionalCharacterEntity(sqlite3_stmt* Statement, int32 Index, const FOGEntityId& Id)
{
    return Id.IsValid()
        ? BindCharacterText(Statement, Index, Id.ToString())
        : sqlite3_bind_null(Statement, Index) == SQLITE_OK;
}

bool BindOptionalCharacterContent(sqlite3_stmt* Statement, int32 Index, const FOGContentId& Id)
{
    return Id.IsEmpty()
        ? sqlite3_bind_null(Statement, Index) == SQLITE_OK
        : BindCharacterText(Statement, Index, Id.ToString());
}

bool ReadCharacterCount(
    sqlite3* Database,
    const char* Sql,
    int32& OutCount,
    FString& OutError)
{
    OutCount = 0;
    sqlite3_stmt* Statement = nullptr;
    if (sqlite3_prepare_v2(Database, Sql, -1, &Statement, nullptr) != SQLITE_OK)
    {
        OutError = TEXT("Failed to prepare migration-0013 validation query.");
        return false;
    }

    const int32 Step = sqlite3_step(Statement);
    if (Step != SQLITE_ROW)
    {
        OutError = TEXT("Failed to execute migration-0013 validation query.");
        sqlite3_finalize(Statement);
        return false;
    }

    OutCount = sqlite3_column_int(Statement, 0);
    sqlite3_finalize(Statement);
    return true;
}
}

bool FOGSQLiteWorldStore::UpsertEntityLanguage(
    const FOGEntityLanguageRecord& Language,
    FString& OutError)
{
    OutError.Reset();

    if (!Language.EntityId.IsValid() ||
        !Language.LanguageId.IsValid() ||
        Language.SpokenProficiencyBps < 0 ||
        Language.SpokenProficiencyBps > 10000 ||
        Language.WrittenProficiencyBps < 0 ||
        Language.WrittenProficiencyBps > 10000)
    {
        OutError = TEXT("Entity language record is invalid.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "INSERT INTO entity_languages("
        "entity_id, language_content_id, spoken_proficiency_bps, written_proficiency_bps, state_json"
        ") VALUES(?, ?, ?, ?, ?) "
        "ON CONFLICT(entity_id, language_content_id) DO UPDATE SET "
        "spoken_proficiency_bps = excluded.spoken_proficiency_bps, "
        "written_proficiency_bps = excluded.written_proficiency_bps, "
        "state_json = excluded.state_json;";

    if (sqlite3_prepare_v2(Database, Sql, -1, &Statement, nullptr) != SQLITE_OK)
    {
        OutError = LastError(TEXT("Prepare entity language upsert"));
        return false;
    }

    const bool bSucceeded =
        BindCharacterText(Statement, 1, Language.EntityId.ToString()) &&
        BindCharacterText(Statement, 2, Language.LanguageId.ToString()) &&
        sqlite3_bind_int(Statement, 3, Language.SpokenProficiencyBps) == SQLITE_OK &&
        sqlite3_bind_int(Statement, 4, Language.WrittenProficiencyBps) == SQLITE_OK &&
        BindCharacterText(Statement, 5, Language.StateJson.IsEmpty() ? TEXT("{}") : Language.StateJson) &&
        sqlite3_step(Statement) == SQLITE_DONE;

    if (!bSucceeded)
    {
        OutError = LastError(TEXT("Upsert entity language"));
    }
    sqlite3_finalize(Statement);
    return bSucceeded;
}

bool FOGSQLiteWorldStore::ListEntityLanguages(
    const FOGEntityId& EntityId,
    TArray<FOGEntityLanguageRecord>& OutLanguages,
    FString& OutError) const
{
    OutLanguages.Reset();
    OutError.Reset();

    if (!EntityId.IsValid())
    {
        OutError = TEXT("Entity language list requires a valid entity.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "SELECT language_content_id, spoken_proficiency_bps, written_proficiency_bps, state_json "
        "FROM entity_languages WHERE entity_id = ? ORDER BY language_content_id;";

    if (sqlite3_prepare_v2(Database, Sql, -1, &Statement, nullptr) != SQLITE_OK)
    {
        OutError = LastError(TEXT("Prepare entity language list"));
        return false;
    }

    if (!BindCharacterText(Statement, 1, EntityId.ToString()))
    {
        OutError = LastError(TEXT("Bind entity language list"));
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
            OutError = LastError(TEXT("Read entity language list"));
            sqlite3_finalize(Statement);
            OutLanguages.Reset();
            return false;
        }

        FOGEntityLanguageRecord Language;
        Language.EntityId = EntityId;
        Language.LanguageId = FOGContentId(CharacterColumnText(Statement, 0));
        Language.SpokenProficiencyBps = sqlite3_column_int(Statement, 1);
        Language.WrittenProficiencyBps = sqlite3_column_int(Statement, 2);
        Language.StateJson = CharacterColumnText(Statement, 3);

        if (!Language.LanguageId.IsValid())
        {
            OutError = TEXT("Stored entity language has invalid content ID.");
            sqlite3_finalize(Statement);
            OutLanguages.Reset();
            return false;
        }
        OutLanguages.Add(MoveTemp(Language));
    }

    sqlite3_finalize(Statement);
    return true;
}

bool FOGSQLiteWorldStore::UpsertSemanticMemory(
    const FOGSemanticMemoryRecord& Memory,
    int64 CreatedWorldTick,
    FString& OutError)
{
    OutError.Reset();

    if (!Memory.MemoryId.IsValid() ||
        !Memory.OwnerEntityId.IsValid() ||
        !Memory.MemoryTypeId.IsValid() ||
        Memory.SalienceBps < 0 ||
        Memory.SalienceBps > 10000)
    {
        OutError = TEXT("Semantic memory is invalid.");
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
            Memory.MemoryId,
            FName(TEXT("semantic_memory")),
            CreatedWorldTick,
            TEXT("{}"),
            Error))
    {
        return Fail(Error);
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "INSERT INTO semantic_memories("
        "memory_entity_id, owner_entity_id, subject_entity_id, source_event_id, "
        "memory_type_content_id, salience_bps, state_json"
        ") VALUES(?, ?, ?, ?, ?, ?, ?) "
        "ON CONFLICT(memory_entity_id) DO UPDATE SET "
        "owner_entity_id = excluded.owner_entity_id, subject_entity_id = excluded.subject_entity_id, "
        "source_event_id = excluded.source_event_id, memory_type_content_id = excluded.memory_type_content_id, "
        "salience_bps = excluded.salience_bps, state_json = excluded.state_json;";

    if (sqlite3_prepare_v2(Database, Sql, -1, &Statement, nullptr) != SQLITE_OK)
    {
        return Fail(LastError(TEXT("Prepare semantic memory upsert")));
    }

    const bool bSucceeded =
        BindCharacterText(Statement, 1, Memory.MemoryId.ToString()) &&
        BindCharacterText(Statement, 2, Memory.OwnerEntityId.ToString()) &&
        BindOptionalCharacterEntity(Statement, 3, Memory.SubjectEntityId) &&
        BindOptionalCharacterEntity(Statement, 4, Memory.SourceEventId) &&
        BindCharacterText(Statement, 5, Memory.MemoryTypeId.ToString()) &&
        sqlite3_bind_int(Statement, 6, Memory.SalienceBps) == SQLITE_OK &&
        BindCharacterText(Statement, 7, Memory.StateJson.IsEmpty() ? TEXT("{}") : Memory.StateJson) &&
        sqlite3_step(Statement) == SQLITE_DONE;

    const FString SqlError = bSucceeded ? FString() : LastError(TEXT("Upsert semantic memory"));
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

bool FOGSQLiteWorldStore::TryReadSemanticMemory(
    const FOGEntityId& MemoryId,
    bool& bOutFound,
    FOGSemanticMemoryRecord& OutMemory,
    FString& OutError) const
{
    bOutFound = false;
    OutMemory = FOGSemanticMemoryRecord();
    OutError.Reset();

    if (!MemoryId.IsValid())
    {
        OutError = TEXT("Semantic memory lookup requires a valid ID.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "SELECT owner_entity_id, subject_entity_id, source_event_id, memory_type_content_id, "
        "salience_bps, state_json FROM semantic_memories WHERE memory_entity_id = ?;";

    if (sqlite3_prepare_v2(Database, Sql, -1, &Statement, nullptr) != SQLITE_OK)
    {
        OutError = LastError(TEXT("Prepare semantic memory read"));
        return false;
    }

    if (!BindCharacterText(Statement, 1, MemoryId.ToString()))
    {
        OutError = LastError(TEXT("Bind semantic memory read"));
        sqlite3_finalize(Statement);
        return false;
    }

    const int32 Step = sqlite3_step(Statement);
    if (Step == SQLITE_ROW)
    {
        bOutFound = true;
        OutMemory.MemoryId = MemoryId;
        OutMemory.OwnerEntityId = ParseCharacterEntity(CharacterColumnText(Statement, 0));
        OutMemory.SubjectEntityId = ParseCharacterEntity(CharacterColumnText(Statement, 1));
        OutMemory.SourceEventId = ParseCharacterEntity(CharacterColumnText(Statement, 2));
        OutMemory.MemoryTypeId = FOGContentId(CharacterColumnText(Statement, 3));
        OutMemory.SalienceBps = sqlite3_column_int(Statement, 4);
        OutMemory.StateJson = CharacterColumnText(Statement, 5);

        if (!OutMemory.OwnerEntityId.IsValid() || !OutMemory.MemoryTypeId.IsValid())
        {
            OutError = TEXT("Stored semantic memory is invalid.");
            sqlite3_finalize(Statement);
            return false;
        }
    }
    else if (Step != SQLITE_DONE)
    {
        OutError = LastError(TEXT("Read semantic memory"));
        sqlite3_finalize(Statement);
        return false;
    }

    sqlite3_finalize(Statement);
    return true;
}

bool FOGSQLiteWorldStore::ListSemanticMemoriesByOwner(
    const FOGEntityId& OwnerId,
    TArray<FOGSemanticMemoryRecord>& OutMemories,
    FString& OutError) const
{
    OutMemories.Reset();
    OutError.Reset();

    if (!OwnerId.IsValid())
    {
        OutError = TEXT("Semantic memory list requires a valid owner.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "SELECT memory_entity_id FROM semantic_memories "
        "WHERE owner_entity_id = ? ORDER BY salience_bps DESC, memory_entity_id;";

    if (sqlite3_prepare_v2(Database, Sql, -1, &Statement, nullptr) != SQLITE_OK)
    {
        OutError = LastError(TEXT("Prepare semantic memory list"));
        return false;
    }

    if (!BindCharacterText(Statement, 1, OwnerId.ToString()))
    {
        OutError = LastError(TEXT("Bind semantic memory list"));
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
            OutError = LastError(TEXT("Read semantic memory list"));
            sqlite3_finalize(Statement);
            return false;
        }

        const FOGEntityId Id = ParseCharacterEntity(CharacterColumnText(Statement, 0));
        if (!Id.IsValid())
        {
            OutError = TEXT("Stored semantic memory list has invalid ID.");
            sqlite3_finalize(Statement);
            return false;
        }
        Ids.Add(Id);
    }
    sqlite3_finalize(Statement);

    for (const FOGEntityId& Id : Ids)
    {
        bool bFound = false;
        FOGSemanticMemoryRecord Memory;
        if (!TryReadSemanticMemory(Id, bFound, Memory, OutError) || !bFound)
        {
            if (OutError.IsEmpty())
            {
                OutError = TEXT("Semantic memory disappeared during list read.");
            }
            OutMemories.Reset();
            return false;
        }
        OutMemories.Add(MoveTemp(Memory));
    }
    return true;
}

bool FOGSQLiteWorldStore::UpsertNpcPromotionState(
    const FOGNpcPromotionStateRecord& State,
    FString& OutError)
{
    OutError.Reset();

    if (!State.EntityId.IsValid() ||
        !State.SimulationTierId.IsValid() ||
        State.PromotedWorldTick < 0)
    {
        OutError = TEXT("NPC promotion state is invalid.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "INSERT INTO npc_promotion_state("
        "entity_id, simulation_tier_content_id, promoted_world_tick, reason_event_id, presentation_package_state_json"
        ") VALUES(?, ?, ?, ?, ?) "
        "ON CONFLICT(entity_id) DO UPDATE SET "
        "simulation_tier_content_id = excluded.simulation_tier_content_id, "
        "promoted_world_tick = excluded.promoted_world_tick, reason_event_id = excluded.reason_event_id, "
        "presentation_package_state_json = excluded.presentation_package_state_json;";

    if (sqlite3_prepare_v2(Database, Sql, -1, &Statement, nullptr) != SQLITE_OK)
    {
        OutError = LastError(TEXT("Prepare NPC promotion upsert"));
        return false;
    }

    const bool bSucceeded =
        BindCharacterText(Statement, 1, State.EntityId.ToString()) &&
        BindCharacterText(Statement, 2, State.SimulationTierId.ToString()) &&
        sqlite3_bind_int64(Statement, 3, State.PromotedWorldTick) == SQLITE_OK &&
        BindOptionalCharacterEntity(Statement, 4, State.ReasonEventId) &&
        BindCharacterText(
            Statement,
            5,
            State.PresentationPackageStateJson.IsEmpty()
                ? TEXT("{}")
                : State.PresentationPackageStateJson) &&
        sqlite3_step(Statement) == SQLITE_DONE;

    if (!bSucceeded)
    {
        OutError = LastError(TEXT("Upsert NPC promotion"));
    }
    sqlite3_finalize(Statement);
    return bSucceeded;
}

bool FOGSQLiteWorldStore::TryReadNpcPromotionState(
    const FOGEntityId& EntityId,
    bool& bOutFound,
    FOGNpcPromotionStateRecord& OutState,
    FString& OutError) const
{
    bOutFound = false;
    OutState = FOGNpcPromotionStateRecord();
    OutError.Reset();

    if (!EntityId.IsValid())
    {
        OutError = TEXT("NPC promotion lookup requires a valid entity.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "SELECT simulation_tier_content_id, promoted_world_tick, reason_event_id, presentation_package_state_json "
        "FROM npc_promotion_state WHERE entity_id = ?;";

    if (sqlite3_prepare_v2(Database, Sql, -1, &Statement, nullptr) != SQLITE_OK)
    {
        OutError = LastError(TEXT("Prepare NPC promotion read"));
        return false;
    }

    if (!BindCharacterText(Statement, 1, EntityId.ToString()))
    {
        OutError = LastError(TEXT("Bind NPC promotion read"));
        sqlite3_finalize(Statement);
        return false;
    }

    const int32 Step = sqlite3_step(Statement);
    if (Step == SQLITE_ROW)
    {
        bOutFound = true;
        OutState.EntityId = EntityId;
        OutState.SimulationTierId = FOGContentId(CharacterColumnText(Statement, 0));
        OutState.PromotedWorldTick = sqlite3_column_int64(Statement, 1);
        OutState.ReasonEventId = ParseCharacterEntity(CharacterColumnText(Statement, 2));
        OutState.PresentationPackageStateJson = CharacterColumnText(Statement, 3);

        if (!OutState.SimulationTierId.IsValid())
        {
            OutError = TEXT("Stored NPC promotion has invalid simulation tier.");
            sqlite3_finalize(Statement);
            return false;
        }
    }
    else if (Step != SQLITE_DONE)
    {
        OutError = LastError(TEXT("Read NPC promotion"));
        sqlite3_finalize(Statement);
        return false;
    }

    sqlite3_finalize(Statement);
    return true;
}

bool FOGSQLiteWorldStore::UpsertCharacterAdultRuntimeState(
    const FOGCharacterAdultRuntimeStateRecord& State,
    FString& OutError)
{
    OutError.Reset();

    if (!State.CharacterEntityId.IsValid() ||
        (!State.CurrentProfileVariantId.IsEmpty() &&
         !State.CurrentProfileVariantId.IsValid()) ||
        State.UpdatedWorldTick < 0)
    {
        OutError = TEXT("Character adult runtime state is invalid.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "INSERT INTO character_adult_runtime_state("
        "character_entity_id, current_profile_variant_content_id, mutable_context_state_json, updated_world_tick"
        ") VALUES(?, ?, ?, ?) "
        "ON CONFLICT(character_entity_id) DO UPDATE SET "
        "current_profile_variant_content_id = excluded.current_profile_variant_content_id, "
        "mutable_context_state_json = excluded.mutable_context_state_json, "
        "updated_world_tick = excluded.updated_world_tick;";

    if (sqlite3_prepare_v2(Database, Sql, -1, &Statement, nullptr) != SQLITE_OK)
    {
        OutError = LastError(TEXT("Prepare character adult runtime upsert"));
        return false;
    }

    const bool bSucceeded =
        BindCharacterText(Statement, 1, State.CharacterEntityId.ToString()) &&
        BindOptionalCharacterContent(Statement, 2, State.CurrentProfileVariantId) &&
        BindCharacterText(
            Statement,
            3,
            State.MutableContextStateJson.IsEmpty()
                ? TEXT("{}")
                : State.MutableContextStateJson) &&
        sqlite3_bind_int64(Statement, 4, State.UpdatedWorldTick) == SQLITE_OK &&
        sqlite3_step(Statement) == SQLITE_DONE;

    if (!bSucceeded)
    {
        OutError = LastError(TEXT("Upsert character adult runtime"));
    }
    sqlite3_finalize(Statement);
    return bSucceeded;
}

bool FOGSQLiteWorldStore::TryReadCharacterAdultRuntimeState(
    const FOGEntityId& EntityId,
    bool& bOutFound,
    FOGCharacterAdultRuntimeStateRecord& OutState,
    FString& OutError) const
{
    bOutFound = false;
    OutState = FOGCharacterAdultRuntimeStateRecord();
    OutError.Reset();

    if (!EntityId.IsValid())
    {
        OutError = TEXT("Character adult runtime lookup requires a valid entity.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "SELECT current_profile_variant_content_id, mutable_context_state_json, updated_world_tick "
        "FROM character_adult_runtime_state WHERE character_entity_id = ?;";

    if (sqlite3_prepare_v2(Database, Sql, -1, &Statement, nullptr) != SQLITE_OK)
    {
        OutError = LastError(TEXT("Prepare character adult runtime read"));
        return false;
    }

    if (!BindCharacterText(Statement, 1, EntityId.ToString()))
    {
        OutError = LastError(TEXT("Bind character adult runtime read"));
        sqlite3_finalize(Statement);
        return false;
    }

    const int32 Step = sqlite3_step(Statement);
    if (Step == SQLITE_ROW)
    {
        bOutFound = true;
        OutState.CharacterEntityId = EntityId;
        OutState.CurrentProfileVariantId = FOGContentId(CharacterColumnText(Statement, 0));
        OutState.MutableContextStateJson = CharacterColumnText(Statement, 1);
        OutState.UpdatedWorldTick = sqlite3_column_int64(Statement, 2);
    }
    else if (Step != SQLITE_DONE)
    {
        OutError = LastError(TEXT("Read character adult runtime"));
        sqlite3_finalize(Statement);
        return false;
    }

    sqlite3_finalize(Statement);
    return true;
}

bool FOGSQLiteWorldStore::UpsertHeroicRecord(
    const FOGHeroicRecord& Record,
    int64 CreatedWorldTick,
    FString& OutError)
{
    OutError.Reset();

    if (!Record.RecordId.IsValid() ||
        !Record.SourceWorldEntityId.IsValid() ||
        !Record.IdentityId.IsValid() ||
        !Record.DeathEventId.IsValid() ||
        Record.CreatedWorldTick < 0 ||
        (!Record.PatternId.IsEmpty() && !Record.PatternId.IsValid()) ||
        Record.GachaAccessState.IsNone())
    {
        OutError = TEXT("Heroic Record is invalid.");
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
            Record.RecordId,
            FName(TEXT("heroic_record")),
            CreatedWorldTick,
            TEXT("{}"),
            Error))
    {
        return Fail(Error);
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "INSERT INTO heroic_records("
        "record_entity_id, source_world_entity_id, identity_content_id, death_event_id, "
        "created_world_tick, pattern_content_id, gacha_access_state, state_json"
        ") VALUES(?, ?, ?, ?, ?, ?, ?, ?) "
        "ON CONFLICT(record_entity_id) DO UPDATE SET "
        "source_world_entity_id = excluded.source_world_entity_id, "
        "identity_content_id = excluded.identity_content_id, death_event_id = excluded.death_event_id, "
        "created_world_tick = excluded.created_world_tick, pattern_content_id = excluded.pattern_content_id, "
        "gacha_access_state = excluded.gacha_access_state, state_json = excluded.state_json;";

    if (sqlite3_prepare_v2(Database, Sql, -1, &Statement, nullptr) != SQLITE_OK)
    {
        return Fail(LastError(TEXT("Prepare Heroic Record upsert")));
    }

    const bool bSucceeded =
        BindCharacterText(Statement, 1, Record.RecordId.ToString()) &&
        BindCharacterText(Statement, 2, Record.SourceWorldEntityId.ToString()) &&
        BindCharacterText(Statement, 3, Record.IdentityId.ToString()) &&
        BindCharacterText(Statement, 4, Record.DeathEventId.ToString()) &&
        sqlite3_bind_int64(Statement, 5, Record.CreatedWorldTick) == SQLITE_OK &&
        BindOptionalCharacterContent(Statement, 6, Record.PatternId) &&
        BindCharacterText(Statement, 7, Record.GachaAccessState.ToString()) &&
        BindCharacterText(Statement, 8, Record.StateJson.IsEmpty() ? TEXT("{}") : Record.StateJson) &&
        sqlite3_step(Statement) == SQLITE_DONE;

    const FString SqlError = bSucceeded ? FString() : LastError(TEXT("Upsert Heroic Record"));
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

bool FOGSQLiteWorldStore::TryReadHeroicRecord(
    const FOGEntityId& RecordId,
    bool& bOutFound,
    FOGHeroicRecord& OutRecord,
    FString& OutError) const
{
    bOutFound = false;
    OutRecord = FOGHeroicRecord();
    OutError.Reset();

    if (!RecordId.IsValid())
    {
        OutError = TEXT("Heroic Record lookup requires a valid ID.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "SELECT source_world_entity_id, identity_content_id, death_event_id, created_world_tick, "
        "pattern_content_id, gacha_access_state, state_json FROM heroic_records WHERE record_entity_id = ?;";

    if (sqlite3_prepare_v2(Database, Sql, -1, &Statement, nullptr) != SQLITE_OK)
    {
        OutError = LastError(TEXT("Prepare Heroic Record read"));
        return false;
    }

    if (!BindCharacterText(Statement, 1, RecordId.ToString()))
    {
        OutError = LastError(TEXT("Bind Heroic Record read"));
        sqlite3_finalize(Statement);
        return false;
    }

    const int32 Step = sqlite3_step(Statement);
    if (Step == SQLITE_ROW)
    {
        bOutFound = true;
        OutRecord.RecordId = RecordId;
        OutRecord.SourceWorldEntityId = ParseCharacterEntity(CharacterColumnText(Statement, 0));
        OutRecord.IdentityId = FOGContentId(CharacterColumnText(Statement, 1));
        OutRecord.DeathEventId = ParseCharacterEntity(CharacterColumnText(Statement, 2));
        OutRecord.CreatedWorldTick = sqlite3_column_int64(Statement, 3);
        OutRecord.PatternId = FOGContentId(CharacterColumnText(Statement, 4));
        OutRecord.GachaAccessState = FName(*CharacterColumnText(Statement, 5));
        OutRecord.StateJson = CharacterColumnText(Statement, 6);

        if (!OutRecord.SourceWorldEntityId.IsValid() ||
            !OutRecord.IdentityId.IsValid() ||
            !OutRecord.DeathEventId.IsValid() ||
            OutRecord.GachaAccessState.IsNone())
        {
            OutError = TEXT("Stored Heroic Record is invalid.");
            sqlite3_finalize(Statement);
            return false;
        }
    }
    else if (Step != SQLITE_DONE)
    {
        OutError = LastError(TEXT("Read Heroic Record"));
        sqlite3_finalize(Statement);
        return false;
    }

    sqlite3_finalize(Statement);
    return true;
}

bool FOGSQLiteWorldStore::ListHeroicRecordsByIdentity(
    const FOGContentId& IdentityId,
    TArray<FOGHeroicRecord>& OutRecords,
    FString& OutError) const
{
    OutRecords.Reset();
    OutError.Reset();

    if (!IdentityId.IsValid())
    {
        OutError = TEXT("Heroic Record list requires a valid Identity ID.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "SELECT record_entity_id FROM heroic_records "
        "WHERE identity_content_id = ? ORDER BY created_world_tick, record_entity_id;";

    if (sqlite3_prepare_v2(Database, Sql, -1, &Statement, nullptr) != SQLITE_OK)
    {
        OutError = LastError(TEXT("Prepare Heroic Record list"));
        return false;
    }

    if (!BindCharacterText(Statement, 1, IdentityId.ToString()))
    {
        OutError = LastError(TEXT("Bind Heroic Record list"));
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
            OutError = LastError(TEXT("Read Heroic Record list"));
            sqlite3_finalize(Statement);
            return false;
        }

        const FOGEntityId Id = ParseCharacterEntity(CharacterColumnText(Statement, 0));
        if (!Id.IsValid())
        {
            OutError = TEXT("Stored Heroic Record list has invalid ID.");
            sqlite3_finalize(Statement);
            return false;
        }
        Ids.Add(Id);
    }
    sqlite3_finalize(Statement);

    for (const FOGEntityId& Id : Ids)
    {
        bool bFound = false;
        FOGHeroicRecord Record;
        if (!TryReadHeroicRecord(Id, bFound, Record, OutError) || !bFound)
        {
            if (OutError.IsEmpty())
            {
                OutError = TEXT("Heroic Record disappeared during list read.");
            }
            OutRecords.Reset();
            return false;
        }
        OutRecords.Add(MoveTemp(Record));
    }
    return true;
}

bool FOGSQLiteWorldStore::MigrateItemsKnowledgeCharacters0013(
    FString& OutError)
{
    OutError.Reset();

    // Migration 0013 intentionally does not invent item capability/affinity,
    // languages, semantic memories, promotion, adult runtime state or Heroic
    // Records from older opaque state. Knowledge columns receive only their
    // explicit schema defaults (believed, full confidence) for existing facts.
    return true;
}

bool FOGSQLiteWorldStore::ValidateItemsKnowledgeCharactersMigration0013(
    FString& OutError) const
{
    OutError.Reset();

    int32 InvalidKnowledge = 0;
    if (!ReadCharacterCount(
            Database,
            "SELECT COUNT(*) FROM knowledge_facts "
            "WHERE belief_state = '' OR confidence_bps < 0 OR confidence_bps > 10000;",
            InvalidKnowledge,
            OutError))
    {
        return false;
    }

    if (InvalidKnowledge != 0)
    {
        OutError = FString::Printf(
            TEXT("Migration 0013 produced %d invalid knowledge facts."),
            InvalidKnowledge);
        return false;
    }

    int32 InvalidLanguages = 0;
    if (!ReadCharacterCount(
            Database,
            "SELECT COUNT(*) FROM entity_languages "
            "WHERE spoken_proficiency_bps < 0 OR spoken_proficiency_bps > 10000 "
            "OR written_proficiency_bps < 0 OR written_proficiency_bps > 10000;",
            InvalidLanguages,
            OutError))
    {
        return false;
    }

    if (InvalidLanguages != 0)
    {
        OutError = FString::Printf(
            TEXT("Migration 0013 produced %d invalid language records."),
            InvalidLanguages);
        return false;
    }

    int32 InvalidMemories = 0;
    if (!ReadCharacterCount(
            Database,
            "SELECT COUNT(*) FROM semantic_memories "
            "WHERE salience_bps < 0 OR salience_bps > 10000;",
            InvalidMemories,
            OutError))
    {
        return false;
    }

    if (InvalidMemories != 0)
    {
        OutError = FString::Printf(
            TEXT("Migration 0013 produced %d invalid semantic memories."),
            InvalidMemories);
        return false;
    }

    int32 ForbiddenAdultColumns = 0;
    if (!ReadCharacterCount(
            Database,
            "SELECT COUNT(*) FROM pragma_table_info('character_adult_runtime_state') "
            "WHERE lower(name) LIKE '%eligible%' OR lower(name) LIKE '%consent%' OR lower(name) LIKE '%permission%';",
            ForbiddenAdultColumns,
            OutError))
    {
        return false;
    }

    if (ForbiddenAdultColumns != 0)
    {
        OutError = TEXT("Migration 0013 adult runtime state contains a forbidden generic access/consent field.");
        return false;
    }

    return true;
}
