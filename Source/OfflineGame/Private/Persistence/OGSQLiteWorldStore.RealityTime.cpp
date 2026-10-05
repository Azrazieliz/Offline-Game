#include "Persistence/OGSQLiteWorldStore.h"

#include "sqlite/sqlite3.h"

namespace
{
bool BindRealityText(
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

FString RealityColumnText(
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

FOGEntityId ParseRealityEntityId(
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

bool BindOptionalRealityEntityId(
    sqlite3_stmt* Statement,
    int32 Index,
    const FOGEntityId& EntityId)
{
    return EntityId.IsValid()
        ? BindRealityText(
            Statement,
            Index,
            EntityId.ToString())
        : sqlite3_bind_null(
            Statement,
            Index) == SQLITE_OK;
}

bool BindOptionalRealityContentId(
    sqlite3_stmt* Statement,
    int32 Index,
    const FOGContentId& ContentId)
{
    return ContentId.IsValid()
        ? BindRealityText(
            Statement,
            Index,
            ContentId.ToString())
        : sqlite3_bind_null(
            Statement,
            Index) == SQLITE_OK;
}

bool BindOptionalRealityTick(
    sqlite3_stmt* Statement,
    int32 Index,
    bool bHasTick,
    int64 Tick)
{
    return bHasTick
        ? sqlite3_bind_int64(
            Statement,
            Index,
            Tick) == SQLITE_OK
        : sqlite3_bind_null(
            Statement,
            Index) == SQLITE_OK;
}
}

bool FOGSQLiteWorldStore::UpsertTimeDomain(
    const FOGTimeDomainRecord& Domain,
    int64 CreatedWorldTick,
    FString& OutError)
{
    OutError.Reset();

    if (!Domain.TimeDomainId.IsValid() ||
        Domain.RateNumerator <= 0 ||
        Domain.RateDenominator <= 0 ||
        (Domain.ParentTimeDomainId.IsValid() &&
         Domain.ParentTimeDomainId ==
             Domain.TimeDomainId))
    {
        OutError =
            TEXT("Time Domain record is invalid.");
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
            }
            return false;
        };

    FString Error;
    if (!UpsertEntity(
            Domain.TimeDomainId,
            TEXT("time_domain"),
            CreatedWorldTick,
            TEXT("{}"),
            Error))
    {
        return Fail(Error);
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "INSERT INTO time_domains("
        "time_domain_entity_id, parent_time_domain_entity_id, "
        "rate_numerator, rate_denominator, parent_epoch_tick, "
        "local_epoch_tick, calendar_content_id, state_json"
        ") VALUES(?, ?, ?, ?, ?, ?, ?, ?) "
        "ON CONFLICT(time_domain_entity_id) DO UPDATE SET "
        "parent_time_domain_entity_id = excluded.parent_time_domain_entity_id, "
        "rate_numerator = excluded.rate_numerator, "
        "rate_denominator = excluded.rate_denominator, "
        "parent_epoch_tick = excluded.parent_epoch_tick, "
        "local_epoch_tick = excluded.local_epoch_tick, "
        "calendar_content_id = excluded.calendar_content_id, "
        "state_json = excluded.state_json;";

    if (sqlite3_prepare_v2(
            Database,
            Sql,
            -1,
            &Statement,
            nullptr) != SQLITE_OK)
    {
        return Fail(
            LastError(TEXT("Prepare Time Domain upsert")));
    }

    const bool bSucceeded =
        BindRealityText(
            Statement,
            1,
            Domain.TimeDomainId.ToString()) &&
        BindOptionalRealityEntityId(
            Statement,
            2,
            Domain.ParentTimeDomainId) &&
        sqlite3_bind_int64(
            Statement,
            3,
            Domain.RateNumerator) == SQLITE_OK &&
        sqlite3_bind_int64(
            Statement,
            4,
            Domain.RateDenominator) == SQLITE_OK &&
        sqlite3_bind_int64(
            Statement,
            5,
            Domain.ParentEpochTick) == SQLITE_OK &&
        sqlite3_bind_int64(
            Statement,
            6,
            Domain.LocalEpochTick) == SQLITE_OK &&
        BindOptionalRealityContentId(
            Statement,
            7,
            Domain.CalendarId) &&
        BindRealityText(
            Statement,
            8,
            Domain.StateJson.IsEmpty()
                ? TEXT("{}")
                : Domain.StateJson) &&
        sqlite3_step(Statement) ==
            SQLITE_DONE;

    const FString SqlError =
        bSucceeded
            ? FString()
            : LastError(TEXT("Upsert Time Domain"));
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

bool FOGSQLiteWorldStore::TryReadTimeDomain(
    const FOGEntityId& TimeDomainId,
    bool& bOutFound,
    FOGTimeDomainRecord& OutDomain,
    FString& OutError) const
{
    bOutFound = false;
    OutDomain =
        FOGTimeDomainRecord();
    OutError.Reset();

    if (!TimeDomainId.IsValid())
    {
        OutError =
            TEXT("Time Domain lookup requires a valid ID.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "SELECT parent_time_domain_entity_id, rate_numerator, rate_denominator, "
        "parent_epoch_tick, local_epoch_tick, calendar_content_id, state_json "
        "FROM time_domains WHERE time_domain_entity_id = ?;";

    if (sqlite3_prepare_v2(
            Database,
            Sql,
            -1,
            &Statement,
            nullptr) != SQLITE_OK)
    {
        OutError =
            LastError(TEXT("Prepare Time Domain read"));
        return false;
    }

    if (!BindRealityText(
            Statement,
            1,
            TimeDomainId.ToString()))
    {
        OutError =
            LastError(TEXT("Bind Time Domain read"));
        sqlite3_finalize(Statement);
        return false;
    }

    const int32 Step =
        sqlite3_step(Statement);
    if (Step == SQLITE_ROW)
    {
        bOutFound = true;
        OutDomain.TimeDomainId =
            TimeDomainId;
        OutDomain.ParentTimeDomainId =
            ParseRealityEntityId(
                RealityColumnText(
                    Statement,
                    0));
        OutDomain.RateNumerator =
            sqlite3_column_int64(
                Statement,
                1);
        OutDomain.RateDenominator =
            sqlite3_column_int64(
                Statement,
                2);
        OutDomain.ParentEpochTick =
            sqlite3_column_int64(
                Statement,
                3);
        OutDomain.LocalEpochTick =
            sqlite3_column_int64(
                Statement,
                4);

        const FString Calendar =
            RealityColumnText(
                Statement,
                5);
        if (!Calendar.IsEmpty())
        {
            OutDomain.CalendarId =
                FOGContentId(Calendar);
        }

        OutDomain.StateJson =
            RealityColumnText(
                Statement,
                6);

        if (OutDomain.RateNumerator <= 0 ||
            OutDomain.RateDenominator <= 0)
        {
            OutError =
                TEXT("Stored Time Domain has an invalid rate.");
            sqlite3_finalize(Statement);
            return false;
        }
    }
    else if (Step != SQLITE_DONE)
    {
        OutError =
            LastError(TEXT("Read Time Domain"));
        sqlite3_finalize(Statement);
        return false;
    }

    sqlite3_finalize(Statement);
    return true;
}

bool FOGSQLiteWorldStore::UpsertRealityNode(
    const FOGRealityNodeRecord& Reality,
    int64 CreatedWorldTick,
    FString& OutError)
{
    OutError.Reset();

    if (!Reality.RealityId.IsValid() ||
        Reality.Kind.IsNone() ||
        (Reality.ParentRealityId.IsValid() &&
         Reality.ParentRealityId ==
             Reality.RealityId))
    {
        OutError =
            TEXT("Reality node record is invalid.");
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
            }
            return false;
        };

    FString Error;
    if (!UpsertEntity(
            Reality.RealityId,
            TEXT("reality_node"),
            CreatedWorldTick,
            TEXT("{}"),
            Error))
    {
        return Fail(Error);
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "INSERT INTO reality_nodes("
        "reality_entity_id, parent_reality_entity_id, kind, "
        "world_rank_content_id, time_domain_entity_id, law_profile_content_id, state_json"
        ") VALUES(?, ?, ?, ?, ?, ?, ?) "
        "ON CONFLICT(reality_entity_id) DO UPDATE SET "
        "parent_reality_entity_id = excluded.parent_reality_entity_id, "
        "kind = excluded.kind, "
        "world_rank_content_id = excluded.world_rank_content_id, "
        "time_domain_entity_id = excluded.time_domain_entity_id, "
        "law_profile_content_id = excluded.law_profile_content_id, "
        "state_json = excluded.state_json;";

    if (sqlite3_prepare_v2(
            Database,
            Sql,
            -1,
            &Statement,
            nullptr) != SQLITE_OK)
    {
        return Fail(
            LastError(TEXT("Prepare Reality node upsert")));
    }

    const bool bSucceeded =
        BindRealityText(
            Statement,
            1,
            Reality.RealityId.ToString()) &&
        BindOptionalRealityEntityId(
            Statement,
            2,
            Reality.ParentRealityId) &&
        BindRealityText(
            Statement,
            3,
            Reality.Kind.ToString()) &&
        BindOptionalRealityContentId(
            Statement,
            4,
            Reality.WorldRankId) &&
        BindOptionalRealityEntityId(
            Statement,
            5,
            Reality.TimeDomainId) &&
        BindOptionalRealityContentId(
            Statement,
            6,
            Reality.LawProfileId) &&
        BindRealityText(
            Statement,
            7,
            Reality.StateJson.IsEmpty()
                ? TEXT("{}")
                : Reality.StateJson) &&
        sqlite3_step(Statement) ==
            SQLITE_DONE;

    const FString SqlError =
        bSucceeded
            ? FString()
            : LastError(TEXT("Upsert Reality node"));
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

bool FOGSQLiteWorldStore::TryReadRealityNode(
    const FOGEntityId& RealityId,
    bool& bOutFound,
    FOGRealityNodeRecord& OutReality,
    FString& OutError) const
{
    bOutFound = false;
    OutReality =
        FOGRealityNodeRecord();
    OutError.Reset();

    if (!RealityId.IsValid())
    {
        OutError =
            TEXT("Reality-node lookup requires a valid ID.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "SELECT parent_reality_entity_id, kind, world_rank_content_id, "
        "time_domain_entity_id, law_profile_content_id, state_json "
        "FROM reality_nodes WHERE reality_entity_id = ?;";

    if (sqlite3_prepare_v2(
            Database,
            Sql,
            -1,
            &Statement,
            nullptr) != SQLITE_OK)
    {
        OutError =
            LastError(TEXT("Prepare Reality node read"));
        return false;
    }

    if (!BindRealityText(
            Statement,
            1,
            RealityId.ToString()))
    {
        OutError =
            LastError(TEXT("Bind Reality node read"));
        sqlite3_finalize(Statement);
        return false;
    }

    const int32 Step =
        sqlite3_step(Statement);
    if (Step == SQLITE_ROW)
    {
        bOutFound = true;
        OutReality.RealityId =
            RealityId;
        OutReality.ParentRealityId =
            ParseRealityEntityId(
                RealityColumnText(
                    Statement,
                    0));
        OutReality.Kind =
            FName(*RealityColumnText(
                Statement,
                1));

        const FString WorldRank =
            RealityColumnText(
                Statement,
                2);
        if (!WorldRank.IsEmpty())
        {
            OutReality.WorldRankId =
                FOGContentId(WorldRank);
        }

        OutReality.TimeDomainId =
            ParseRealityEntityId(
                RealityColumnText(
                    Statement,
                    3));

        const FString LawProfile =
            RealityColumnText(
                Statement,
                4);
        if (!LawProfile.IsEmpty())
        {
            OutReality.LawProfileId =
                FOGContentId(LawProfile);
        }

        OutReality.StateJson =
            RealityColumnText(
                Statement,
                5);

        if (OutReality.Kind.IsNone())
        {
            OutError =
                TEXT("Stored Reality node has an invalid kind.");
            sqlite3_finalize(Statement);
            return false;
        }
    }
    else if (Step != SQLITE_DONE)
    {
        OutError =
            LastError(TEXT("Read Reality node"));
        sqlite3_finalize(Statement);
        return false;
    }

    sqlite3_finalize(Statement);
    return true;
}

bool FOGSQLiteWorldStore::ListChildRealityNodes(
    const FOGEntityId& ParentRealityId,
    TArray<FOGRealityNodeRecord>& OutChildren,
    FString& OutError) const
{
    OutChildren.Reset();
    OutError.Reset();

    if (!ParentRealityId.IsValid())
    {
        OutError =
            TEXT("Reality-child list requires a valid parent Reality.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "SELECT reality_entity_id FROM reality_nodes "
        "WHERE parent_reality_entity_id = ? "
        "ORDER BY reality_entity_id ASC;";

    if (sqlite3_prepare_v2(
            Database,
            Sql,
            -1,
            &Statement,
            nullptr) != SQLITE_OK)
    {
        OutError =
            LastError(TEXT("Prepare Reality-child list"));
        return false;
    }

    if (!BindRealityText(
            Statement,
            1,
            ParentRealityId.ToString()))
    {
        OutError =
            LastError(TEXT("Bind Reality-child list"));
        sqlite3_finalize(Statement);
        return false;
    }

    TArray<FOGEntityId> Ids;
    while (true)
    {
        const int32 Step =
            sqlite3_step(Statement);
        if (Step == SQLITE_DONE)
        {
            break;
        }

        if (Step != SQLITE_ROW)
        {
            OutError =
                LastError(TEXT("Read Reality-child list"));
            sqlite3_finalize(Statement);
            return false;
        }

        const FOGEntityId ChildId =
            ParseRealityEntityId(
                RealityColumnText(
                    Statement,
                    0));
        if (!ChildId.IsValid())
        {
            OutError =
                TEXT("Stored Reality-child list contains an invalid ID.");
            sqlite3_finalize(Statement);
            return false;
        }

        Ids.Add(ChildId);
    }

    sqlite3_finalize(Statement);

    for (const FOGEntityId& ChildId : Ids)
    {
        bool bFound = false;
        FOGRealityNodeRecord Child;
        if (!TryReadRealityNode(
                ChildId,
                bFound,
                Child,
                OutError) ||
            !bFound)
        {
            if (OutError.IsEmpty())
            {
                OutError =
                    TEXT("Reality child disappeared during list read.");
            }
            OutChildren.Reset();
            return false;
        }

        OutChildren.Add(MoveTemp(Child));
    }

    return true;
}

bool FOGSQLiteWorldStore::UpsertJunction(
    const FOGJunctionRecord& Junction,
    int64 CreatedWorldTick,
    FString& OutError)
{
    OutError.Reset();

    if (!Junction.JunctionId.IsValid() ||
        !Junction.FromRealityId.IsValid() ||
        !Junction.ToRealityId.IsValid() ||
        Junction.FromRealityId ==
            Junction.ToRealityId ||
        Junction.State.IsNone() ||
        Junction.StabilityBps < 0 ||
        Junction.StabilityBps > 10000 ||
        (Junction.bHasOpenedWorldTick &&
         Junction.OpenedWorldTick < 0) ||
        (Junction.bHasClosedWorldTick &&
         Junction.ClosedWorldTick < 0))
    {
        OutError =
            TEXT("Junction record is invalid.");
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
            }
            return false;
        };

    FString Error;
    if (!UpsertEntity(
            Junction.JunctionId,
            TEXT("junction"),
            CreatedWorldTick,
            TEXT("{}"),
            Error))
    {
        return Fail(Error);
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "INSERT INTO junctions("
        "junction_entity_id, from_reality_entity_id, to_reality_entity_id, "
        "state, stability_bps, opened_world_tick, closed_world_tick, requirements_json"
        ") VALUES(?, ?, ?, ?, ?, ?, ?, ?) "
        "ON CONFLICT(junction_entity_id) DO UPDATE SET "
        "from_reality_entity_id = excluded.from_reality_entity_id, "
        "to_reality_entity_id = excluded.to_reality_entity_id, "
        "state = excluded.state, "
        "stability_bps = excluded.stability_bps, "
        "opened_world_tick = excluded.opened_world_tick, "
        "closed_world_tick = excluded.closed_world_tick, "
        "requirements_json = excluded.requirements_json;";

    if (sqlite3_prepare_v2(
            Database,
            Sql,
            -1,
            &Statement,
            nullptr) != SQLITE_OK)
    {
        return Fail(
            LastError(TEXT("Prepare Junction upsert")));
    }

    const bool bSucceeded =
        BindRealityText(
            Statement,
            1,
            Junction.JunctionId.ToString()) &&
        BindRealityText(
            Statement,
            2,
            Junction.FromRealityId.ToString()) &&
        BindRealityText(
            Statement,
            3,
            Junction.ToRealityId.ToString()) &&
        BindRealityText(
            Statement,
            4,
            Junction.State.ToString()) &&
        sqlite3_bind_int(
            Statement,
            5,
            Junction.StabilityBps) == SQLITE_OK &&
        BindOptionalRealityTick(
            Statement,
            6,
            Junction.bHasOpenedWorldTick,
            Junction.OpenedWorldTick) &&
        BindOptionalRealityTick(
            Statement,
            7,
            Junction.bHasClosedWorldTick,
            Junction.ClosedWorldTick) &&
        BindRealityText(
            Statement,
            8,
            Junction.RequirementsJson.IsEmpty()
                ? TEXT("{}")
                : Junction.RequirementsJson) &&
        sqlite3_step(Statement) ==
            SQLITE_DONE;

    const FString SqlError =
        bSucceeded
            ? FString()
            : LastError(TEXT("Upsert Junction"));
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

bool FOGSQLiteWorldStore::TryReadJunction(
    const FOGEntityId& JunctionId,
    bool& bOutFound,
    FOGJunctionRecord& OutJunction,
    FString& OutError) const
{
    bOutFound = false;
    OutJunction =
        FOGJunctionRecord();
    OutError.Reset();

    if (!JunctionId.IsValid())
    {
        OutError =
            TEXT("Junction lookup requires a valid ID.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "SELECT from_reality_entity_id, to_reality_entity_id, state, "
        "stability_bps, opened_world_tick, closed_world_tick, requirements_json "
        "FROM junctions WHERE junction_entity_id = ?;";

    if (sqlite3_prepare_v2(
            Database,
            Sql,
            -1,
            &Statement,
            nullptr) != SQLITE_OK)
    {
        OutError =
            LastError(TEXT("Prepare Junction read"));
        return false;
    }

    if (!BindRealityText(
            Statement,
            1,
            JunctionId.ToString()))
    {
        OutError =
            LastError(TEXT("Bind Junction read"));
        sqlite3_finalize(Statement);
        return false;
    }

    const int32 Step =
        sqlite3_step(Statement);
    if (Step == SQLITE_ROW)
    {
        bOutFound = true;
        OutJunction.JunctionId =
            JunctionId;
        OutJunction.FromRealityId =
            ParseRealityEntityId(
                RealityColumnText(
                    Statement,
                    0));
        OutJunction.ToRealityId =
            ParseRealityEntityId(
                RealityColumnText(
                    Statement,
                    1));
        OutJunction.State =
            FName(*RealityColumnText(
                Statement,
                2));
        OutJunction.StabilityBps =
            sqlite3_column_int(
                Statement,
                3);

        if (sqlite3_column_type(
                Statement,
                4) != SQLITE_NULL)
        {
            OutJunction.bHasOpenedWorldTick =
                true;
            OutJunction.OpenedWorldTick =
                sqlite3_column_int64(
                    Statement,
                    4);
        }

        if (sqlite3_column_type(
                Statement,
                5) != SQLITE_NULL)
        {
            OutJunction.bHasClosedWorldTick =
                true;
            OutJunction.ClosedWorldTick =
                sqlite3_column_int64(
                    Statement,
                    5);
        }

        OutJunction.RequirementsJson =
            RealityColumnText(
                Statement,
                6);

        if (!OutJunction.FromRealityId.IsValid() ||
            !OutJunction.ToRealityId.IsValid() ||
            OutJunction.State.IsNone() ||
            OutJunction.StabilityBps < 0 ||
            OutJunction.StabilityBps > 10000)
        {
            OutError =
                TEXT("Stored Junction record is invalid.");
            sqlite3_finalize(Statement);
            return false;
        }
    }
    else if (Step != SQLITE_DONE)
    {
        OutError =
            LastError(TEXT("Read Junction"));
        sqlite3_finalize(Statement);
        return false;
    }

    sqlite3_finalize(Statement);
    return true;
}

bool FOGSQLiteWorldStore::ListJunctionsFromReality(
    const FOGEntityId& FromRealityId,
    TArray<FOGJunctionRecord>& OutJunctions,
    FString& OutError) const
{
    OutJunctions.Reset();
    OutError.Reset();

    if (!FromRealityId.IsValid())
    {
        OutError =
            TEXT("Junction list requires a valid source Reality.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "SELECT junction_entity_id FROM junctions "
        "WHERE from_reality_entity_id = ? "
        "ORDER BY junction_entity_id ASC;";

    if (sqlite3_prepare_v2(
            Database,
            Sql,
            -1,
            &Statement,
            nullptr) != SQLITE_OK)
    {
        OutError =
            LastError(TEXT("Prepare Junction list"));
        return false;
    }

    if (!BindRealityText(
            Statement,
            1,
            FromRealityId.ToString()))
    {
        OutError =
            LastError(TEXT("Bind Junction list"));
        sqlite3_finalize(Statement);
        return false;
    }

    TArray<FOGEntityId> Ids;
    while (true)
    {
        const int32 Step =
            sqlite3_step(Statement);
        if (Step == SQLITE_DONE)
        {
            break;
        }

        if (Step != SQLITE_ROW)
        {
            OutError =
                LastError(TEXT("Read Junction list"));
            sqlite3_finalize(Statement);
            return false;
        }

        const FOGEntityId Id =
            ParseRealityEntityId(
                RealityColumnText(
                    Statement,
                    0));
        if (!Id.IsValid())
        {
            OutError =
                TEXT("Stored Junction list contains an invalid ID.");
            sqlite3_finalize(Statement);
            return false;
        }
        Ids.Add(Id);
    }

    sqlite3_finalize(Statement);

    for (const FOGEntityId& Id : Ids)
    {
        bool bFound = false;
        FOGJunctionRecord Junction;
        if (!TryReadJunction(
                Id,
                bFound,
                Junction,
                OutError) ||
            !bFound)
        {
            if (OutError.IsEmpty())
            {
                OutError =
                    TEXT("Junction disappeared during list read.");
            }
            OutJunctions.Reset();
            return false;
        }
        OutJunctions.Add(MoveTemp(Junction));
    }

    return true;
}
