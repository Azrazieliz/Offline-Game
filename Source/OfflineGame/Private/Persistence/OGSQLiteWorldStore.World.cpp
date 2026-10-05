#include "Persistence/OGSQLiteWorldStore.h"

#include "sqlite/sqlite3.h"

namespace
{
bool BindWorldText(
    sqlite3_stmt* Statement,
    int32 Index,
    const FString& Value)
{
    FTCHARToUTF8 Utf8(*Value);
    return sqlite3_bind_text(
        Statement,
        Index,
        Utf8.Get(),
        Utf8.Length(),
        SQLITE_TRANSIENT) == SQLITE_OK;
}

FString WorldColumnText(
    sqlite3_stmt* Statement,
    int32 Column)
{
    const unsigned char* Text =
        sqlite3_column_text(
            Statement,
            Column);

    return Text
        ? UTF8_TO_TCHAR(
            reinterpret_cast<const char*>(Text))
        : FString();
}

FOGEntityId ParseWorldEntityId(
    const FString& Value)
{
    if (Value.IsEmpty())
    {
        return FOGEntityId();
    }

    FGuid Guid;
    return FGuid::Parse(Value, Guid)
        ? FOGEntityId(Guid)
        : FOGEntityId();
}

bool BindOptionalEntityId(
    sqlite3_stmt* Statement,
    int32 Index,
    const FOGEntityId& EntityId)
{
    if (!EntityId.IsValid())
    {
        return sqlite3_bind_null(
            Statement,
            Index) == SQLITE_OK;
    }

    return BindWorldText(
        Statement,
        Index,
        EntityId.ToString());
}
}

bool FOGSQLiteWorldStore::UpsertLocation(
    const FOGLocationRecord& Location,
    int64 CreatedWorldTick,
    FString& OutError)
{
    OutError.Reset();

    if (!Location.LocationId.IsValid() ||
        Location.Kind.IsNone())
    {
        OutError =
            TEXT("Location requires a valid ID and kind.");
        return false;
    }

    const bool bOwnTransaction =
        !bTransactionActive;

    if (bOwnTransaction &&
        !BeginTransaction(OutError))
    {
        return false;
    }

    auto Fail =
        [this, bOwnTransaction, &OutError](
            const FString& Error)
        {
            OutError = Error;

            if (bOwnTransaction)
            {
                FString RollbackError;
                RollbackTransaction(
                    RollbackError);

                if (!RollbackError.IsEmpty())
                {
                    OutError +=
                        FString::Printf(
                            TEXT(" | Rollback error: %s"),
                            *RollbackError);
                }
            }

            return false;
        };

    FString EntityError;
    if (!UpsertEntity(
            Location.LocationId,
            TEXT("location"),
            CreatedWorldTick,
            TEXT("{}"),
            EntityError))
    {
        return Fail(EntityError);
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "INSERT INTO locations("
        "location_entity_id, parent_location_entity_id, kind, "
        "territory_entity_id, physically_accessible"
        ") VALUES(?, ?, ?, ?, ?) "
        "ON CONFLICT(location_entity_id) DO UPDATE SET "
        "parent_location_entity_id = excluded.parent_location_entity_id, "
        "kind = excluded.kind, "
        "territory_entity_id = excluded.territory_entity_id, "
        "physically_accessible = excluded.physically_accessible;";

    if (sqlite3_prepare_v2(
            Database,
            Sql,
            -1,
            &Statement,
            nullptr) != SQLITE_OK)
    {
        return Fail(
            LastError(
                TEXT("Prepare location upsert")));
    }

    const bool bBound =
        BindWorldText(
            Statement,
            1,
            Location.LocationId.ToString()) &&
        BindOptionalEntityId(
            Statement,
            2,
            Location.ParentLocationId) &&
        BindWorldText(
            Statement,
            3,
            Location.Kind.ToString()) &&
        BindOptionalEntityId(
            Statement,
            4,
            Location.TerritoryId) &&
        sqlite3_bind_int(
            Statement,
            5,
            Location.bPhysicallyAccessible
                ? 1
                : 0) == SQLITE_OK;

    const bool bSucceeded =
        bBound &&
        sqlite3_step(Statement) ==
            SQLITE_DONE;

    const FString SqlError =
        bSucceeded
            ? FString()
            : LastError(
                TEXT("Upsert location"));

    sqlite3_finalize(Statement);

    if (!bSucceeded)
    {
        return Fail(SqlError);
    }

    if (bOwnTransaction &&
        !CommitTransaction(OutError))
    {
        FString RollbackError;
        RollbackTransaction(
            RollbackError);
        return false;
    }

    return true;
}

bool FOGSQLiteWorldStore::TryReadLocation(
    const FOGEntityId& LocationId,
    bool& bOutFound,
    FOGLocationRecord& OutLocation,
    FString& OutError) const
{
    bOutFound = false;
    OutLocation = FOGLocationRecord();
    OutError.Reset();

    if (!LocationId.IsValid())
    {
        OutError =
            TEXT("Location ID is invalid.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "SELECT parent_location_entity_id, kind, "
        "territory_entity_id, physically_accessible "
        "FROM locations WHERE location_entity_id = ?;";

    if (sqlite3_prepare_v2(
            Database,
            Sql,
            -1,
            &Statement,
            nullptr) != SQLITE_OK)
    {
        OutError =
            LastError(
                TEXT("Prepare location read"));
        return false;
    }

    if (!BindWorldText(
            Statement,
            1,
            LocationId.ToString()))
    {
        OutError =
            LastError(
                TEXT("Bind location read"));
        sqlite3_finalize(Statement);
        return false;
    }

    const int32 StepResult =
        sqlite3_step(Statement);

    if (StepResult == SQLITE_ROW)
    {
        bOutFound = true;
        OutLocation.LocationId =
            LocationId;
        OutLocation.ParentLocationId =
            ParseWorldEntityId(
                WorldColumnText(
                    Statement,
                    0));
        OutLocation.Kind =
            FName(
                *WorldColumnText(
                    Statement,
                    1));
        OutLocation.TerritoryId =
            ParseWorldEntityId(
                WorldColumnText(
                    Statement,
                    2));
        OutLocation.bPhysicallyAccessible =
            sqlite3_column_int(
                Statement,
                3) != 0;
    }
    else if (StepResult != SQLITE_DONE)
    {
        OutError =
            LastError(
                TEXT("Read location"));
        sqlite3_finalize(Statement);
        return false;
    }

    sqlite3_finalize(Statement);
    return true;
}

bool FOGSQLiteWorldStore::UpsertWorldPresence(
    const FOGWorldPresenceRecord& Presence,
    FString& OutError)
{
    OutError.Reset();

    if (!Presence.EntityId.IsValid() ||
        !Presence.LocationId.IsValid())
    {
        OutError =
            TEXT("World presence requires valid entity and location IDs.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "INSERT INTO world_presence("
        "entity_id, location_entity_id, local_x, local_y, local_z, "
        "movement_context, updated_world_tick"
        ") VALUES(?, ?, ?, ?, ?, ?, ?) "
        "ON CONFLICT(entity_id) DO UPDATE SET "
        "location_entity_id = excluded.location_entity_id, "
        "local_x = excluded.local_x, "
        "local_y = excluded.local_y, "
        "local_z = excluded.local_z, "
        "movement_context = excluded.movement_context, "
        "updated_world_tick = excluded.updated_world_tick;";

    if (sqlite3_prepare_v2(
            Database,
            Sql,
            -1,
            &Statement,
            nullptr) != SQLITE_OK)
    {
        OutError =
            LastError(
                TEXT("Prepare world presence upsert"));
        return false;
    }

    const bool bBound =
        BindWorldText(
            Statement,
            1,
            Presence.EntityId.ToString()) &&
        BindWorldText(
            Statement,
            2,
            Presence.LocationId.ToString()) &&
        sqlite3_bind_double(
            Statement,
            3,
            Presence.LocalPosition.X) ==
            SQLITE_OK &&
        sqlite3_bind_double(
            Statement,
            4,
            Presence.LocalPosition.Y) ==
            SQLITE_OK &&
        sqlite3_bind_double(
            Statement,
            5,
            Presence.LocalPosition.Z) ==
            SQLITE_OK &&
        BindWorldText(
            Statement,
            6,
            Presence.MovementContext.ToString()) &&
        sqlite3_bind_int64(
            Statement,
            7,
            Presence.UpdatedWorldTick) ==
            SQLITE_OK;

    const bool bSucceeded =
        bBound &&
        sqlite3_step(Statement) ==
            SQLITE_DONE;

    if (!bSucceeded)
    {
        OutError =
            LastError(
                TEXT("Upsert world presence"));
    }

    sqlite3_finalize(Statement);
    return bSucceeded;
}

bool FOGSQLiteWorldStore::TryReadWorldPresence(
    const FOGEntityId& EntityId,
    bool& bOutFound,
    FOGWorldPresenceRecord& OutPresence,
    FString& OutError) const
{
    bOutFound = false;
    OutPresence =
        FOGWorldPresenceRecord();
    OutError.Reset();

    if (!EntityId.IsValid())
    {
        OutError =
            TEXT("World presence entity ID is invalid.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "SELECT location_entity_id, local_x, local_y, local_z, "
        "movement_context, updated_world_tick "
        "FROM world_presence WHERE entity_id = ?;";

    if (sqlite3_prepare_v2(
            Database,
            Sql,
            -1,
            &Statement,
            nullptr) != SQLITE_OK)
    {
        OutError =
            LastError(
                TEXT("Prepare world presence read"));
        return false;
    }

    if (!BindWorldText(
            Statement,
            1,
            EntityId.ToString()))
    {
        OutError =
            LastError(
                TEXT("Bind world presence read"));
        sqlite3_finalize(Statement);
        return false;
    }

    const int32 StepResult =
        sqlite3_step(Statement);

    if (StepResult == SQLITE_ROW)
    {
        const FOGEntityId LocationId =
            ParseWorldEntityId(
                WorldColumnText(
                    Statement,
                    0));

        if (!LocationId.IsValid())
        {
            OutError =
                TEXT("Stored world presence references an invalid location ID.");
            sqlite3_finalize(Statement);
            return false;
        }

        bOutFound = true;
        OutPresence.EntityId =
            EntityId;
        OutPresence.LocationId =
            LocationId;
        OutPresence.LocalPosition =
            FVector3d(
                sqlite3_column_double(
                    Statement,
                    1),
                sqlite3_column_double(
                    Statement,
                    2),
                sqlite3_column_double(
                    Statement,
                    3));
        OutPresence.MovementContext =
            FName(
                *WorldColumnText(
                    Statement,
                    4));
        OutPresence.UpdatedWorldTick =
            sqlite3_column_int64(
                Statement,
                5);
    }
    else if (StepResult != SQLITE_DONE)
    {
        OutError =
            LastError(
                TEXT("Read world presence"));
        sqlite3_finalize(Statement);
        return false;
    }

    sqlite3_finalize(Statement);
    return true;
}

bool FOGSQLiteWorldStore::UpsertKnowledgeFact(
    const FOGKnowledgeFactRecord& Fact,
    FString& OutError)
{
    OutError.Reset();

    if (!Fact.OwnerEntityId.IsValid() ||
        Fact.FactKey.IsNone())
    {
        OutError =
            TEXT("Knowledge fact requires a valid owner and fact key.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "INSERT INTO knowledge_facts("
        "owner_entity_id, fact_key, subject_entity_id, value_json, "
        "learned_world_tick, updated_world_tick, belief_state, confidence_bps, "
        "source_entity_id, source_event_id, evidence_world_tick, language_context_content_id"
        ") VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?) "
        "ON CONFLICT(owner_entity_id, fact_key, subject_entity_id) "
        "DO UPDATE SET "
        "value_json = excluded.value_json, "
        "learned_world_tick = MIN(knowledge_facts.learned_world_tick, excluded.learned_world_tick), "
        "updated_world_tick = excluded.updated_world_tick, "
        "belief_state = excluded.belief_state, confidence_bps = excluded.confidence_bps, "
        "source_entity_id = excluded.source_entity_id, source_event_id = excluded.source_event_id, "
        "evidence_world_tick = excluded.evidence_world_tick, "
        "language_context_content_id = excluded.language_context_content_id;";

    if (sqlite3_prepare_v2(
            Database,
            Sql,
            -1,
            &Statement,
            nullptr) != SQLITE_OK)
    {
        OutError =
            LastError(
                TEXT("Prepare knowledge fact upsert"));
        return false;
    }

    const FString SubjectId =
        Fact.SubjectEntityId.IsValid()
            ? Fact.SubjectEntityId.ToString()
            : FString();

    const bool bBound =
        BindWorldText(
            Statement,
            1,
            Fact.OwnerEntityId.ToString()) &&
        BindWorldText(
            Statement,
            2,
            Fact.FactKey.ToString()) &&
        BindWorldText(
            Statement,
            3,
            SubjectId) &&
        BindWorldText(
            Statement,
            4,
            Fact.ValueJson.IsEmpty()
                ? TEXT("{}")
                : Fact.ValueJson) &&
        sqlite3_bind_int64(
            Statement,
            5,
            Fact.LearnedWorldTick) ==
            SQLITE_OK &&
        sqlite3_bind_int64(
            Statement,
            6,
            Fact.UpdatedWorldTick) ==
            SQLITE_OK &&
        BindWorldText(
            Statement,
            7,
            Fact.BeliefState.IsNone()
                ? FString(TEXT("believed"))
                : Fact.BeliefState.ToString()) &&
        sqlite3_bind_int(
            Statement,
            8,
            FMath::Clamp(
                Fact.ConfidenceBps,
                0,
                10000)) == SQLITE_OK &&
        BindOptionalEntityId(
            Statement,
            9,
            Fact.SourceEntityId) &&
        BindOptionalEntityId(
            Statement,
            10,
            Fact.SourceEventId) &&
        (Fact.bHasEvidenceWorldTick
            ? sqlite3_bind_int64(
                Statement,
                11,
                Fact.EvidenceWorldTick) == SQLITE_OK
            : sqlite3_bind_null(
                Statement,
                11) == SQLITE_OK) &&
        (Fact.LanguageContextId.IsValid()
            ? BindWorldText(
                Statement,
                12,
                Fact.LanguageContextId.ToString())
            : sqlite3_bind_null(
                Statement,
                12) == SQLITE_OK);

    const bool bSucceeded =
        bBound &&
        sqlite3_step(Statement) ==
            SQLITE_DONE;

    if (!bSucceeded)
    {
        OutError =
            LastError(
                TEXT("Upsert knowledge fact"));
    }

    sqlite3_finalize(Statement);
    return bSucceeded;
}

bool FOGSQLiteWorldStore::TryReadKnowledgeFact(
    const FOGEntityId& OwnerEntityId,
    FName FactKey,
    const FOGEntityId& SubjectEntityId,
    bool& bOutFound,
    FOGKnowledgeFactRecord& OutFact,
    FString& OutError) const
{
    bOutFound = false;
    OutFact =
        FOGKnowledgeFactRecord();
    OutError.Reset();

    if (!OwnerEntityId.IsValid() ||
        FactKey.IsNone())
    {
        OutError =
            TEXT("Knowledge fact lookup requires a valid owner and fact key.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "SELECT value_json, learned_world_tick, updated_world_tick, "
        "belief_state, confidence_bps, source_entity_id, source_event_id, "
        "evidence_world_tick, language_context_content_id "
        "FROM knowledge_facts "
        "WHERE owner_entity_id = ? AND fact_key = ? AND subject_entity_id = ?;";

    if (sqlite3_prepare_v2(
            Database,
            Sql,
            -1,
            &Statement,
            nullptr) != SQLITE_OK)
    {
        OutError =
            LastError(
                TEXT("Prepare knowledge fact read"));
        return false;
    }

    const FString SubjectId =
        SubjectEntityId.IsValid()
            ? SubjectEntityId.ToString()
            : FString();

    const bool bBound =
        BindWorldText(
            Statement,
            1,
            OwnerEntityId.ToString()) &&
        BindWorldText(
            Statement,
            2,
            FactKey.ToString()) &&
        BindWorldText(
            Statement,
            3,
            SubjectId);

    if (!bBound)
    {
        OutError =
            LastError(
                TEXT("Bind knowledge fact read"));
        sqlite3_finalize(Statement);
        return false;
    }

    const int32 StepResult =
        sqlite3_step(Statement);

    if (StepResult == SQLITE_ROW)
    {
        bOutFound = true;
        OutFact.OwnerEntityId =
            OwnerEntityId;
        OutFact.FactKey =
            FactKey;
        OutFact.SubjectEntityId =
            SubjectEntityId;
        OutFact.ValueJson =
            WorldColumnText(
                Statement,
                0);
        OutFact.LearnedWorldTick =
            sqlite3_column_int64(
                Statement,
                1);
        OutFact.UpdatedWorldTick =
            sqlite3_column_int64(
                Statement,
                2);
        OutFact.BeliefState =
            FName(
                *WorldColumnText(
                    Statement,
                    3));
        OutFact.ConfidenceBps =
            sqlite3_column_int(
                Statement,
                4);
        OutFact.SourceEntityId =
            ParseWorldEntityId(
                WorldColumnText(
                    Statement,
                    5));
        OutFact.SourceEventId =
            ParseWorldEntityId(
                WorldColumnText(
                    Statement,
                    6));

        if (sqlite3_column_type(
                Statement,
                7) != SQLITE_NULL)
        {
            OutFact.bHasEvidenceWorldTick =
                true;
            OutFact.EvidenceWorldTick =
                sqlite3_column_int64(
                    Statement,
                    7);
        }

        const FString LanguageContext =
            WorldColumnText(
                Statement,
                8);
        if (!LanguageContext.IsEmpty())
        {
            OutFact.LanguageContextId =
                FOGContentId(
                    LanguageContext);
        }
    }
    else if (StepResult != SQLITE_DONE)
    {
        OutError =
            LastError(
                TEXT("Read knowledge fact"));
        sqlite3_finalize(Statement);
        return false;
    }

    sqlite3_finalize(Statement);
    return true;
}
