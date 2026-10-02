#include "Persistence/OGSQLiteWorldStore.h"

#include "sqlite/sqlite3.h"

namespace
{
bool BindStrategyText(
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

FString StrategyColumnText(
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

FOGEntityId ParseStrategyEntityId(
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

bool BindOptionalStrategyEntityId(
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

    return BindStrategyText(
        Statement,
        Index,
        EntityId.ToString());
}
}

bool FOGSQLiteWorldStore::UpsertDispatch(
    const FOGDispatchRecord& Dispatch,
    int64 CreatedWorldTick,
    FString& OutError)
{
    OutError.Reset();

    if (!Dispatch.DispatchId.IsValid() ||
        !Dispatch.OwnerEntityId.IsValid() ||
        Dispatch.ResolveWorldTick <
            Dispatch.StartWorldTick ||
        Dispatch.RiskBps < 0 ||
        Dispatch.RiskBps > 10000)
    {
        OutError =
            TEXT("Dispatch record is invalid.");
        return false;
    }

    TSet<FOGEntityId> Participants;
    for (const FOGEntityId& Participant :
         Dispatch.ParticipantEntityIds)
    {
        if (!Participant.IsValid() ||
            Participants.Contains(
                Participant))
        {
            OutError =
                TEXT("Dispatch contains invalid or duplicate participants.");
            return false;
        }

        Participants.Add(
            Participant);
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

    FString EntityError;
    if (!UpsertEntity(
            Dispatch.DispatchId,
            TEXT("dispatch"),
            CreatedWorldTick,
            TEXT("{}"),
            EntityError))
    {
        return Fail(EntityError);
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "INSERT INTO dispatches("
        "dispatch_entity_id, owner_entity_id, target_entity_id, dispatch_type, status, "
        "start_world_tick, resolve_world_tick, risk_bps, resolution_seed, result_json"
        ") VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?) "
        "ON CONFLICT(dispatch_entity_id) DO UPDATE SET "
        "owner_entity_id = excluded.owner_entity_id, "
        "target_entity_id = excluded.target_entity_id, "
        "dispatch_type = excluded.dispatch_type, "
        "status = excluded.status, "
        "start_world_tick = excluded.start_world_tick, "
        "resolve_world_tick = excluded.resolve_world_tick, "
        "risk_bps = excluded.risk_bps, "
        "resolution_seed = excluded.resolution_seed, "
        "result_json = excluded.result_json;";

    if (sqlite3_prepare_v2(
            Database,
            Sql,
            -1,
            &Statement,
            nullptr) != SQLITE_OK)
    {
        return Fail(
            LastError(
                TEXT("Prepare dispatch upsert")));
    }

    const bool bBound =
        BindStrategyText(
            Statement,
            1,
            Dispatch.DispatchId.ToString()) &&
        BindStrategyText(
            Statement,
            2,
            Dispatch.OwnerEntityId.ToString()) &&
        BindOptionalStrategyEntityId(
            Statement,
            3,
            Dispatch.TargetEntityId) &&
        sqlite3_bind_int(
            Statement,
            4,
            static_cast<int32>(
                Dispatch.Type)) ==
            SQLITE_OK &&
        sqlite3_bind_int(
            Statement,
            5,
            static_cast<int32>(
                Dispatch.Status)) ==
            SQLITE_OK &&
        sqlite3_bind_int64(
            Statement,
            6,
            Dispatch.StartWorldTick) ==
            SQLITE_OK &&
        sqlite3_bind_int64(
            Statement,
            7,
            Dispatch.ResolveWorldTick) ==
            SQLITE_OK &&
        sqlite3_bind_int(
            Statement,
            8,
            Dispatch.RiskBps) ==
            SQLITE_OK &&
        sqlite3_bind_int64(
            Statement,
            9,
            Dispatch.ResolutionSeed) ==
            SQLITE_OK &&
        BindStrategyText(
            Statement,
            10,
            Dispatch.ResultJson.IsEmpty()
                ? TEXT("{}")
                : Dispatch.ResultJson);

    const bool bSucceeded =
        bBound &&
        sqlite3_step(Statement) ==
            SQLITE_DONE;

    const FString SqlError =
        bSucceeded
            ? FString()
            : LastError(
                TEXT("Upsert dispatch"));

    sqlite3_finalize(Statement);

    if (!bSucceeded)
    {
        return Fail(SqlError);
    }

    sqlite3_stmt* Reset = nullptr;
    if (sqlite3_prepare_v2(
            Database,
            "DELETE FROM dispatch_participants WHERE dispatch_entity_id = ?;",
            -1,
            &Reset,
            nullptr) != SQLITE_OK)
    {
        return Fail(
            LastError(
                TEXT("Prepare dispatch participant reset")));
    }

    const bool bReset =
        BindStrategyText(
            Reset,
            1,
            Dispatch.DispatchId.ToString()) &&
        sqlite3_step(Reset) ==
            SQLITE_DONE;

    const FString ResetError =
        bReset
            ? FString()
            : LastError(
                TEXT("Reset dispatch participants"));

    sqlite3_finalize(Reset);

    if (!bReset)
    {
        return Fail(ResetError);
    }

    for (int32 Index = 0;
         Index <
             Dispatch.ParticipantEntityIds.Num();
         ++Index)
    {
        sqlite3_stmt* Insert = nullptr;
        const char* InsertSql =
            "INSERT INTO dispatch_participants("
            "dispatch_entity_id, participant_entity_id, ordinal"
            ") VALUES(?, ?, ?);";

        if (sqlite3_prepare_v2(
                Database,
                InsertSql,
                -1,
                &Insert,
                nullptr) != SQLITE_OK)
        {
            return Fail(
                LastError(
                    TEXT("Prepare dispatch participant insert")));
        }

        const bool bInsert =
            BindStrategyText(
                Insert,
                1,
                Dispatch.DispatchId.ToString()) &&
            BindStrategyText(
                Insert,
                2,
                Dispatch.ParticipantEntityIds[Index].ToString()) &&
            sqlite3_bind_int(
                Insert,
                3,
                Index) ==
                SQLITE_OK &&
            sqlite3_step(Insert) ==
                SQLITE_DONE;

        const FString InsertError =
            bInsert
                ? FString()
                : LastError(
                    TEXT("Insert dispatch participant"));

        sqlite3_finalize(Insert);

        if (!bInsert)
        {
            return Fail(InsertError);
        }
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

bool FOGSQLiteWorldStore::TryReadDispatch(
    const FOGEntityId& DispatchId,
    bool& bOutFound,
    FOGDispatchRecord& OutDispatch,
    FString& OutError) const
{
    bOutFound = false;
    OutDispatch =
        FOGDispatchRecord();
    OutError.Reset();

    if (!DispatchId.IsValid())
    {
        OutError =
            TEXT("Dispatch ID is invalid.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "SELECT owner_entity_id, target_entity_id, dispatch_type, status, "
        "start_world_tick, resolve_world_tick, risk_bps, resolution_seed, result_json "
        "FROM dispatches WHERE dispatch_entity_id = ?;";

    if (sqlite3_prepare_v2(
            Database,
            Sql,
            -1,
            &Statement,
            nullptr) != SQLITE_OK)
    {
        OutError =
            LastError(
                TEXT("Prepare dispatch read"));
        return false;
    }

    if (!BindStrategyText(
            Statement,
            1,
            DispatchId.ToString()))
    {
        OutError =
            LastError(
                TEXT("Bind dispatch read"));
        sqlite3_finalize(Statement);
        return false;
    }

    const int32 Step =
        sqlite3_step(Statement);

    if (Step == SQLITE_ROW)
    {
        const FOGEntityId Owner =
            ParseStrategyEntityId(
                StrategyColumnText(
                    Statement,
                    0));

        if (!Owner.IsValid())
        {
            OutError =
                TEXT("Stored dispatch has an invalid owner.");
            sqlite3_finalize(Statement);
            return false;
        }

        bOutFound = true;
        OutDispatch.DispatchId =
            DispatchId;
        OutDispatch.OwnerEntityId =
            Owner;
        OutDispatch.TargetEntityId =
            ParseStrategyEntityId(
                StrategyColumnText(
                    Statement,
                    1));
        OutDispatch.Type =
            static_cast<EOGDispatchType>(
                sqlite3_column_int(
                    Statement,
                    2));
        OutDispatch.Status =
            static_cast<EOGDispatchStatus>(
                sqlite3_column_int(
                    Statement,
                    3));
        OutDispatch.StartWorldTick =
            sqlite3_column_int64(
                Statement,
                4);
        OutDispatch.ResolveWorldTick =
            sqlite3_column_int64(
                Statement,
                5);
        OutDispatch.RiskBps =
            sqlite3_column_int(
                Statement,
                6);
        OutDispatch.ResolutionSeed =
            sqlite3_column_int64(
                Statement,
                7);
        OutDispatch.ResultJson =
            StrategyColumnText(
                Statement,
                8);
    }
    else if (Step != SQLITE_DONE)
    {
        OutError =
            LastError(
                TEXT("Read dispatch"));
        sqlite3_finalize(Statement);
        return false;
    }

    sqlite3_finalize(Statement);

    if (!bOutFound)
    {
        return true;
    }

    sqlite3_stmt* Participants = nullptr;
    const char* ParticipantSql =
        "SELECT participant_entity_id "
        "FROM dispatch_participants "
        "WHERE dispatch_entity_id = ? "
        "ORDER BY ordinal;";

    if (sqlite3_prepare_v2(
            Database,
            ParticipantSql,
            -1,
            &Participants,
            nullptr) != SQLITE_OK)
    {
        OutError =
            LastError(
                TEXT("Prepare dispatch participants read"));
        return false;
    }

    if (!BindStrategyText(
            Participants,
            1,
            DispatchId.ToString()))
    {
        OutError =
            LastError(
                TEXT("Bind dispatch participants read"));
        sqlite3_finalize(Participants);
        return false;
    }

    while (true)
    {
        const int32 ParticipantStep =
            sqlite3_step(
                Participants);

        if (ParticipantStep == SQLITE_ROW)
        {
            const FOGEntityId ParticipantId =
                ParseStrategyEntityId(
                    StrategyColumnText(
                        Participants,
                        0));

            if (!ParticipantId.IsValid())
            {
                OutError =
                    TEXT("Stored dispatch participant ID is invalid.");
                sqlite3_finalize(Participants);
                return false;
            }

            OutDispatch.ParticipantEntityIds.Add(
                ParticipantId);
            continue;
        }

        if (ParticipantStep == SQLITE_DONE)
        {
            break;
        }

        OutError =
            LastError(
                TEXT("Read dispatch participants"));
        sqlite3_finalize(Participants);
        return false;
    }

    sqlite3_finalize(Participants);
    return true;
}

bool FOGSQLiteWorldStore::UpsertFaction(
    const FOGFactionRecord& Faction,
    int64 CreatedWorldTick,
    FString& OutError)
{
    OutError.Reset();

    if (!Faction.FactionId.IsValid() ||
        Faction.Kind.IsNone() ||
        Faction.Population < 0)
    {
        OutError =
            TEXT("Faction record is invalid.");
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

    FString EntityError;
    if (!UpsertEntity(
            Faction.FactionId,
            TEXT("faction"),
            CreatedWorldTick,
            TEXT("{}"),
            EntityError))
    {
        return Fail(EntityError);
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "INSERT INTO factions("
        "faction_entity_id, leader_ruler_entity_id, kind, population"
        ") VALUES(?, ?, ?, ?) "
        "ON CONFLICT(faction_entity_id) DO UPDATE SET "
        "leader_ruler_entity_id = excluded.leader_ruler_entity_id, "
        "kind = excluded.kind, "
        "population = excluded.population;";

    if (sqlite3_prepare_v2(
            Database,
            Sql,
            -1,
            &Statement,
            nullptr) != SQLITE_OK)
    {
        return Fail(
            LastError(
                TEXT("Prepare faction upsert")));
    }

    const bool bSucceeded =
        BindStrategyText(
            Statement,
            1,
            Faction.FactionId.ToString()) &&
        BindOptionalStrategyEntityId(
            Statement,
            2,
            Faction.LeaderRulerId) &&
        BindStrategyText(
            Statement,
            3,
            Faction.Kind.ToString()) &&
        sqlite3_bind_int64(
            Statement,
            4,
            Faction.Population) ==
            SQLITE_OK &&
        sqlite3_step(Statement) ==
            SQLITE_DONE;

    const FString Error =
        bSucceeded
            ? FString()
            : LastError(
                TEXT("Upsert faction"));

    sqlite3_finalize(Statement);

    if (!bSucceeded)
    {
        return Fail(Error);
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

bool FOGSQLiteWorldStore::TryReadFaction(
    const FOGEntityId& FactionId,
    bool& bOutFound,
    FOGFactionRecord& OutFaction,
    FString& OutError) const
{
    bOutFound = false;
    OutFaction = FOGFactionRecord();
    OutError.Reset();

    if (!FactionId.IsValid())
    {
        OutError = TEXT("Faction ID is invalid.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "SELECT leader_ruler_entity_id, kind, population "
        "FROM factions WHERE faction_entity_id = ?;";

    if (sqlite3_prepare_v2(
            Database,
            Sql,
            -1,
            &Statement,
            nullptr) != SQLITE_OK)
    {
        OutError =
            LastError(
                TEXT("Prepare faction read"));
        return false;
    }

    if (!BindStrategyText(
            Statement,
            1,
            FactionId.ToString()))
    {
        OutError =
            LastError(
                TEXT("Bind faction read"));
        sqlite3_finalize(Statement);
        return false;
    }

    const int32 Step =
        sqlite3_step(Statement);

    if (Step == SQLITE_ROW)
    {
        bOutFound = true;
        OutFaction.FactionId =
            FactionId;
        OutFaction.LeaderRulerId =
            ParseStrategyEntityId(
                StrategyColumnText(
                    Statement,
                    0));
        OutFaction.Kind =
            FName(
                *StrategyColumnText(
                    Statement,
                    1));
        OutFaction.Population =
            sqlite3_column_int64(
                Statement,
                2);
    }
    else if (Step != SQLITE_DONE)
    {
        OutError =
            LastError(
                TEXT("Read faction"));
        sqlite3_finalize(Statement);
        return false;
    }

    sqlite3_finalize(Statement);
    return true;
}

bool FOGSQLiteWorldStore::UpsertFactionLink(
    const FOGFactionLinkRecord& Link,
    FString& OutError)
{
    OutError.Reset();

    if (!Link.SourceFactionId.IsValid() ||
        !Link.TargetFactionId.IsValid() ||
        Link.SourceFactionId ==
            Link.TargetFactionId)
    {
        OutError =
            TEXT("Faction link requires two distinct valid factions.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "INSERT INTO faction_links("
        "source_faction_entity_id, target_faction_entity_id, link_type, active, updated_world_tick"
        ") VALUES(?, ?, ?, ?, ?) "
        "ON CONFLICT(source_faction_entity_id, target_faction_entity_id, link_type) "
        "DO UPDATE SET active = excluded.active, updated_world_tick = excluded.updated_world_tick;";

    if (sqlite3_prepare_v2(
            Database,
            Sql,
            -1,
            &Statement,
            nullptr) != SQLITE_OK)
    {
        OutError =
            LastError(
                TEXT("Prepare faction link upsert"));
        return false;
    }

    const bool bSucceeded =
        BindStrategyText(
            Statement,
            1,
            Link.SourceFactionId.ToString()) &&
        BindStrategyText(
            Statement,
            2,
            Link.TargetFactionId.ToString()) &&
        sqlite3_bind_int(
            Statement,
            3,
            static_cast<int32>(
                Link.Type)) ==
            SQLITE_OK &&
        sqlite3_bind_int(
            Statement,
            4,
            Link.bActive ? 1 : 0) ==
            SQLITE_OK &&
        sqlite3_bind_int64(
            Statement,
            5,
            Link.UpdatedWorldTick) ==
            SQLITE_OK &&
        sqlite3_step(Statement) ==
            SQLITE_DONE;

    if (!bSucceeded)
    {
        OutError =
            LastError(
                TEXT("Upsert faction link"));
    }

    sqlite3_finalize(Statement);
    return bSucceeded;
}

bool FOGSQLiteWorldStore::TryReadFactionLink(
    const FOGEntityId& SourceFactionId,
    const FOGEntityId& TargetFactionId,
    EOGFactionLinkType Type,
    bool& bOutFound,
    FOGFactionLinkRecord& OutLink,
    FString& OutError) const
{
    bOutFound = false;
    OutLink = FOGFactionLinkRecord();
    OutError.Reset();

    if (!SourceFactionId.IsValid() ||
        !TargetFactionId.IsValid())
    {
        OutError =
            TEXT("Faction link lookup requires valid faction IDs.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "SELECT active, updated_world_tick "
        "FROM faction_links "
        "WHERE source_faction_entity_id = ? "
        "AND target_faction_entity_id = ? "
        "AND link_type = ?;";

    if (sqlite3_prepare_v2(
            Database,
            Sql,
            -1,
            &Statement,
            nullptr) != SQLITE_OK)
    {
        OutError =
            LastError(
                TEXT("Prepare faction link read"));
        return false;
    }

    const bool bBound =
        BindStrategyText(
            Statement,
            1,
            SourceFactionId.ToString()) &&
        BindStrategyText(
            Statement,
            2,
            TargetFactionId.ToString()) &&
        sqlite3_bind_int(
            Statement,
            3,
            static_cast<int32>(Type)) ==
            SQLITE_OK;

    if (!bBound)
    {
        OutError =
            LastError(
                TEXT("Bind faction link read"));
        sqlite3_finalize(Statement);
        return false;
    }

    const int32 Step =
        sqlite3_step(Statement);

    if (Step == SQLITE_ROW)
    {
        bOutFound = true;
        OutLink.SourceFactionId =
            SourceFactionId;
        OutLink.TargetFactionId =
            TargetFactionId;
        OutLink.Type = Type;
        OutLink.bActive =
            sqlite3_column_int(
                Statement,
                0) != 0;
        OutLink.UpdatedWorldTick =
            sqlite3_column_int64(
                Statement,
                1);
    }
    else if (Step != SQLITE_DONE)
    {
        OutError =
            LastError(
                TEXT("Read faction link"));
        sqlite3_finalize(Statement);
        return false;
    }

    sqlite3_finalize(Statement);
    return true;
}

bool FOGSQLiteWorldStore::UpsertArmy(
    const FOGArmyRecord& Army,
    int64 CreatedWorldTick,
    FString& OutError)
{
    OutError.Reset();

    if (!Army.ArmyId.IsValid() ||
        !Army.FactionId.IsValid() ||
        !Army.LocationId.IsValid() ||
        Army.Headcount < 0 ||
        Army.EffectivePower.GetSign() < 0 ||
        Army.State.IsNone())
    {
        OutError =
            TEXT("Army record is invalid.");
        return false;
    }

    TSet<FOGEntityId> Commanders;
    for (const FOGEntityId& Commander :
         Army.CommanderEntityIds)
    {
        if (!Commander.IsValid() ||
            Commanders.Contains(
                Commander))
        {
            OutError =
                TEXT("Army has invalid or duplicate commanders.");
            return false;
        }
        Commanders.Add(Commander);
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

    FString EntityError;
    if (!UpsertEntity(
            Army.ArmyId,
            TEXT("army"),
            CreatedWorldTick,
            TEXT("{}"),
            EntityError))
    {
        return Fail(EntityError);
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "INSERT INTO armies("
        "army_entity_id, faction_entity_id, location_entity_id, headcount, power_sig, power_exp, state"
        ") VALUES(?, ?, ?, ?, ?, ?, ?) "
        "ON CONFLICT(army_entity_id) DO UPDATE SET "
        "faction_entity_id = excluded.faction_entity_id, "
        "location_entity_id = excluded.location_entity_id, "
        "headcount = excluded.headcount, "
        "power_sig = excluded.power_sig, "
        "power_exp = excluded.power_exp, "
        "state = excluded.state;";

    if (sqlite3_prepare_v2(
            Database,
            Sql,
            -1,
            &Statement,
            nullptr) != SQLITE_OK)
    {
        return Fail(
            LastError(
                TEXT("Prepare army upsert")));
    }

    const bool bSucceeded =
        BindStrategyText(
            Statement,
            1,
            Army.ArmyId.ToString()) &&
        BindStrategyText(
            Statement,
            2,
            Army.FactionId.ToString()) &&
        BindStrategyText(
            Statement,
            3,
            Army.LocationId.ToString()) &&
        sqlite3_bind_int64(
            Statement,
            4,
            Army.Headcount) ==
            SQLITE_OK &&
        sqlite3_bind_int64(
            Statement,
            5,
            Army.EffectivePower.Significand) ==
            SQLITE_OK &&
        sqlite3_bind_int(
            Statement,
            6,
            Army.EffectivePower.Exponent10) ==
            SQLITE_OK &&
        BindStrategyText(
            Statement,
            7,
            Army.State.ToString()) &&
        sqlite3_step(Statement) ==
            SQLITE_DONE;

    const FString Error =
        bSucceeded
            ? FString()
            : LastError(
                TEXT("Upsert army"));

    sqlite3_finalize(Statement);

    if (!bSucceeded)
    {
        return Fail(Error);
    }

    sqlite3_stmt* Reset = nullptr;
    if (sqlite3_prepare_v2(
            Database,
            "DELETE FROM army_commanders WHERE army_entity_id = ?;",
            -1,
            &Reset,
            nullptr) != SQLITE_OK)
    {
        return Fail(
            LastError(
                TEXT("Prepare army commander reset")));
    }

    const bool bReset =
        BindStrategyText(
            Reset,
            1,
            Army.ArmyId.ToString()) &&
        sqlite3_step(Reset) ==
            SQLITE_DONE;

    const FString ResetError =
        bReset
            ? FString()
            : LastError(
                TEXT("Reset army commanders"));

    sqlite3_finalize(Reset);

    if (!bReset)
    {
        return Fail(ResetError);
    }

    for (int32 Index = 0;
         Index <
             Army.CommanderEntityIds.Num();
         ++Index)
    {
        sqlite3_stmt* Insert = nullptr;
        const char* InsertSql =
            "INSERT INTO army_commanders("
            "army_entity_id, commander_entity_id, ordinal"
            ") VALUES(?, ?, ?);";

        if (sqlite3_prepare_v2(
                Database,
                InsertSql,
                -1,
                &Insert,
                nullptr) != SQLITE_OK)
        {
            return Fail(
                LastError(
                    TEXT("Prepare army commander insert")));
        }

        const bool bInsert =
            BindStrategyText(
                Insert,
                1,
                Army.ArmyId.ToString()) &&
            BindStrategyText(
                Insert,
                2,
                Army.CommanderEntityIds[Index].ToString()) &&
            sqlite3_bind_int(
                Insert,
                3,
                Index) ==
                SQLITE_OK &&
            sqlite3_step(Insert) ==
                SQLITE_DONE;

        const FString InsertError =
            bInsert
                ? FString()
                : LastError(
                    TEXT("Insert army commander"));

        sqlite3_finalize(Insert);

        if (!bInsert)
        {
            return Fail(InsertError);
        }
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

bool FOGSQLiteWorldStore::TryReadArmy(
    const FOGEntityId& ArmyId,
    bool& bOutFound,
    FOGArmyRecord& OutArmy,
    FString& OutError) const
{
    bOutFound = false;
    OutArmy = FOGArmyRecord();
    OutError.Reset();

    if (!ArmyId.IsValid())
    {
        OutError = TEXT("Army ID is invalid.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "SELECT faction_entity_id, location_entity_id, headcount, power_sig, power_exp, state "
        "FROM armies WHERE army_entity_id = ?;";

    if (sqlite3_prepare_v2(
            Database,
            Sql,
            -1,
            &Statement,
            nullptr) != SQLITE_OK)
    {
        OutError =
            LastError(
                TEXT("Prepare army read"));
        return false;
    }

    if (!BindStrategyText(
            Statement,
            1,
            ArmyId.ToString()))
    {
        OutError =
            LastError(
                TEXT("Bind army read"));
        sqlite3_finalize(Statement);
        return false;
    }

    const int32 Step =
        sqlite3_step(Statement);

    if (Step == SQLITE_ROW)
    {
        const FOGEntityId FactionId =
            ParseStrategyEntityId(
                StrategyColumnText(
                    Statement,
                    0));
        const FOGEntityId LocationId =
            ParseStrategyEntityId(
                StrategyColumnText(
                    Statement,
                    1));

        if (!FactionId.IsValid() ||
            !LocationId.IsValid())
        {
            OutError =
                TEXT("Stored army has invalid references.");
            sqlite3_finalize(Statement);
            return false;
        }

        bOutFound = true;
        OutArmy.ArmyId = ArmyId;
        OutArmy.FactionId =
            FactionId;
        OutArmy.LocationId =
            LocationId;
        OutArmy.Headcount =
            sqlite3_column_int64(
                Statement,
                2);
        OutArmy.EffectivePower =
            FOGLargeNumber(
                sqlite3_column_int64(
                    Statement,
                    3),
                sqlite3_column_int(
                    Statement,
                    4));
        OutArmy.State =
            FName(
                *StrategyColumnText(
                    Statement,
                    5));
    }
    else if (Step != SQLITE_DONE)
    {
        OutError =
            LastError(
                TEXT("Read army"));
        sqlite3_finalize(Statement);
        return false;
    }

    sqlite3_finalize(Statement);

    if (!bOutFound)
    {
        return true;
    }

    sqlite3_stmt* Commanders = nullptr;
    const char* CommanderSql =
        "SELECT commander_entity_id "
        "FROM army_commanders "
        "WHERE army_entity_id = ? "
        "ORDER BY ordinal;";

    if (sqlite3_prepare_v2(
            Database,
            CommanderSql,
            -1,
            &Commanders,
            nullptr) != SQLITE_OK)
    {
        OutError =
            LastError(
                TEXT("Prepare army commanders read"));
        return false;
    }

    if (!BindStrategyText(
            Commanders,
            1,
            ArmyId.ToString()))
    {
        OutError =
            LastError(
                TEXT("Bind army commanders read"));
        sqlite3_finalize(Commanders);
        return false;
    }

    while (true)
    {
        const int32 CommanderStep =
            sqlite3_step(
                Commanders);

        if (CommanderStep == SQLITE_ROW)
        {
            const FOGEntityId CommanderId =
                ParseStrategyEntityId(
                    StrategyColumnText(
                        Commanders,
                        0));

            if (!CommanderId.IsValid())
            {
                OutError =
                    TEXT("Stored army commander ID is invalid.");
                sqlite3_finalize(Commanders);
                return false;
            }

            OutArmy.CommanderEntityIds.Add(
                CommanderId);
            continue;
        }

        if (CommanderStep == SQLITE_DONE)
        {
            break;
        }

        OutError =
            LastError(
                TEXT("Read army commanders"));
        sqlite3_finalize(Commanders);
        return false;
    }

    sqlite3_finalize(Commanders);
    return true;
}

bool FOGSQLiteWorldStore::UpsertWar(
    const FOGWarRecord& War,
    int64 CreatedWorldTick,
    FString& OutError)
{
    OutError.Reset();

    if (!War.WarId.IsValid() ||
        War.ObjectiveType.IsNone() ||
        (War.Status == EOGWarStatus::Active &&
         War.EndWorldTick != 0) ||
        (War.Status != EOGWarStatus::Active &&
         War.EndWorldTick <
             War.StartWorldTick) ||
        War.Participants.Num() < 2)
    {
        OutError =
            TEXT("War record is invalid.");
        return false;
    }

    TSet<FOGEntityId> Factions;
    TSet<int32> Sides;

    for (const FOGWarParticipant& Participant :
         War.Participants)
    {
        if (!Participant.FactionId.IsValid() ||
            Participant.SideIndex < 0 ||
            Factions.Contains(
                Participant.FactionId))
        {
            OutError =
                TEXT("War contains invalid or duplicate participants.");
            return false;
        }

        Factions.Add(
            Participant.FactionId);
        Sides.Add(
            Participant.SideIndex);
    }

    if (Sides.Num() < 2)
    {
        OutError =
            TEXT("War requires at least two opposing sides.");
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

    FString EntityError;
    if (!UpsertEntity(
            War.WarId,
            TEXT("war"),
            CreatedWorldTick,
            TEXT("{}"),
            EntityError))
    {
        return Fail(EntityError);
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "INSERT INTO wars("
        "war_entity_id, status, objective_type, objective_target_entity_id, "
        "start_world_tick, end_world_tick, resolution_json"
        ") VALUES(?, ?, ?, ?, ?, ?, ?) "
        "ON CONFLICT(war_entity_id) DO UPDATE SET "
        "status = excluded.status, "
        "objective_type = excluded.objective_type, "
        "objective_target_entity_id = excluded.objective_target_entity_id, "
        "start_world_tick = excluded.start_world_tick, "
        "end_world_tick = excluded.end_world_tick, "
        "resolution_json = excluded.resolution_json;";

    if (sqlite3_prepare_v2(
            Database,
            Sql,
            -1,
            &Statement,
            nullptr) != SQLITE_OK)
    {
        return Fail(
            LastError(
                TEXT("Prepare war upsert")));
    }

    const bool bSucceeded =
        BindStrategyText(
            Statement,
            1,
            War.WarId.ToString()) &&
        sqlite3_bind_int(
            Statement,
            2,
            static_cast<int32>(
                War.Status)) ==
            SQLITE_OK &&
        BindStrategyText(
            Statement,
            3,
            War.ObjectiveType.ToString()) &&
        BindOptionalStrategyEntityId(
            Statement,
            4,
            War.ObjectiveTargetEntityId) &&
        sqlite3_bind_int64(
            Statement,
            5,
            War.StartWorldTick) ==
            SQLITE_OK &&
        sqlite3_bind_int64(
            Statement,
            6,
            War.EndWorldTick) ==
            SQLITE_OK &&
        BindStrategyText(
            Statement,
            7,
            War.ResolutionJson.IsEmpty()
                ? TEXT("{}")
                : War.ResolutionJson) &&
        sqlite3_step(Statement) ==
            SQLITE_DONE;

    const FString Error =
        bSucceeded
            ? FString()
            : LastError(
                TEXT("Upsert war"));

    sqlite3_finalize(Statement);

    if (!bSucceeded)
    {
        return Fail(Error);
    }

    sqlite3_stmt* Reset = nullptr;
    if (sqlite3_prepare_v2(
            Database,
            "DELETE FROM war_participants WHERE war_entity_id = ?;",
            -1,
            &Reset,
            nullptr) != SQLITE_OK)
    {
        return Fail(
            LastError(
                TEXT("Prepare war participant reset")));
    }

    const bool bReset =
        BindStrategyText(
            Reset,
            1,
            War.WarId.ToString()) &&
        sqlite3_step(Reset) ==
            SQLITE_DONE;

    const FString ResetError =
        bReset
            ? FString()
            : LastError(
                TEXT("Reset war participants"));

    sqlite3_finalize(Reset);

    if (!bReset)
    {
        return Fail(ResetError);
    }

    for (const FOGWarParticipant& Participant :
         War.Participants)
    {
        sqlite3_stmt* Insert = nullptr;
        const char* InsertSql =
            "INSERT INTO war_participants("
            "war_entity_id, faction_entity_id, side_index, primary_participant"
            ") VALUES(?, ?, ?, ?);";

        if (sqlite3_prepare_v2(
                Database,
                InsertSql,
                -1,
                &Insert,
                nullptr) != SQLITE_OK)
        {
            return Fail(
                LastError(
                    TEXT("Prepare war participant insert")));
        }

        const bool bInsert =
            BindStrategyText(
                Insert,
                1,
                War.WarId.ToString()) &&
            BindStrategyText(
                Insert,
                2,
                Participant.FactionId.ToString()) &&
            sqlite3_bind_int(
                Insert,
                3,
                Participant.SideIndex) ==
                SQLITE_OK &&
            sqlite3_bind_int(
                Insert,
                4,
                Participant.bPrimary ? 1 : 0) ==
                SQLITE_OK &&
            sqlite3_step(Insert) ==
                SQLITE_DONE;

        const FString InsertError =
            bInsert
                ? FString()
                : LastError(
                    TEXT("Insert war participant"));

        sqlite3_finalize(Insert);

        if (!bInsert)
        {
            return Fail(InsertError);
        }
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

bool FOGSQLiteWorldStore::TryReadWar(
    const FOGEntityId& WarId,
    bool& bOutFound,
    FOGWarRecord& OutWar,
    FString& OutError) const
{
    bOutFound = false;
    OutWar = FOGWarRecord();
    OutError.Reset();

    if (!WarId.IsValid())
    {
        OutError = TEXT("War ID is invalid.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "SELECT status, objective_type, objective_target_entity_id, "
        "start_world_tick, end_world_tick, resolution_json "
        "FROM wars WHERE war_entity_id = ?;";

    if (sqlite3_prepare_v2(
            Database,
            Sql,
            -1,
            &Statement,
            nullptr) != SQLITE_OK)
    {
        OutError =
            LastError(
                TEXT("Prepare war read"));
        return false;
    }

    if (!BindStrategyText(
            Statement,
            1,
            WarId.ToString()))
    {
        OutError =
            LastError(
                TEXT("Bind war read"));
        sqlite3_finalize(Statement);
        return false;
    }

    const int32 Step =
        sqlite3_step(Statement);

    if (Step == SQLITE_ROW)
    {
        bOutFound = true;
        OutWar.WarId =
            WarId;
        OutWar.Status =
            static_cast<EOGWarStatus>(
                sqlite3_column_int(
                    Statement,
                    0));
        OutWar.ObjectiveType =
            FName(
                *StrategyColumnText(
                    Statement,
                    1));
        OutWar.ObjectiveTargetEntityId =
            ParseStrategyEntityId(
                StrategyColumnText(
                    Statement,
                    2));
        OutWar.StartWorldTick =
            sqlite3_column_int64(
                Statement,
                3);
        OutWar.EndWorldTick =
            sqlite3_column_int64(
                Statement,
                4);
        OutWar.ResolutionJson =
            StrategyColumnText(
                Statement,
                5);
    }
    else if (Step != SQLITE_DONE)
    {
        OutError =
            LastError(
                TEXT("Read war"));
        sqlite3_finalize(Statement);
        return false;
    }

    sqlite3_finalize(Statement);

    if (!bOutFound)
    {
        return true;
    }

    sqlite3_stmt* Participants = nullptr;
    const char* ParticipantSql =
        "SELECT faction_entity_id, side_index, primary_participant "
        "FROM war_participants "
        "WHERE war_entity_id = ? "
        "ORDER BY side_index, faction_entity_id;";

    if (sqlite3_prepare_v2(
            Database,
            ParticipantSql,
            -1,
            &Participants,
            nullptr) != SQLITE_OK)
    {
        OutError =
            LastError(
                TEXT("Prepare war participants read"));
        return false;
    }

    if (!BindStrategyText(
            Participants,
            1,
            WarId.ToString()))
    {
        OutError =
            LastError(
                TEXT("Bind war participants read"));
        sqlite3_finalize(Participants);
        return false;
    }

    while (true)
    {
        const int32 ParticipantStep =
            sqlite3_step(
                Participants);

        if (ParticipantStep == SQLITE_ROW)
        {
            FOGWarParticipant Participant;
            Participant.FactionId =
                ParseStrategyEntityId(
                    StrategyColumnText(
                        Participants,
                        0));
            Participant.SideIndex =
                sqlite3_column_int(
                    Participants,
                    1);
            Participant.bPrimary =
                sqlite3_column_int(
                    Participants,
                    2) != 0;

            if (!Participant.FactionId.IsValid())
            {
                OutError =
                    TEXT("Stored war participant ID is invalid.");
                sqlite3_finalize(Participants);
                return false;
            }

            OutWar.Participants.Add(
                MoveTemp(Participant));
            continue;
        }

        if (ParticipantStep == SQLITE_DONE)
        {
            break;
        }

        OutError =
            LastError(
                TEXT("Read war participants"));
        sqlite3_finalize(Participants);
        return false;
    }

    sqlite3_finalize(Participants);
    return true;
}
