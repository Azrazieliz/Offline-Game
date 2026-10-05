#include "Persistence/OGSQLiteWorldStore.h"

#include "sqlite/sqlite3.h"

namespace
{
bool BindDirectorText(
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

FString DirectorColumnText(
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

FOGEntityId ParseDirectorEntityId(
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

bool BindOptionalDirectorContentId(
    sqlite3_stmt* Statement,
    int32 Index,
    const FOGContentId& ContentId)
{
    return ContentId.IsValid()
        ? BindDirectorText(
            Statement,
            Index,
            ContentId.ToString())
        : sqlite3_bind_null(
            Statement,
            Index) == SQLITE_OK;
}

bool BindOptionalDirectorTick(
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

bool ReadDirectorCount(
    sqlite3* Database,
    const char* Sql,
    int32& OutCount,
    FString& OutError)
{
    OutCount = 0;

    sqlite3_stmt* Statement = nullptr;
    if (sqlite3_prepare_v2(
            Database,
            Sql,
            -1,
            &Statement,
            nullptr) != SQLITE_OK)
    {
        OutError =
            TEXT("Failed to prepare migration 0011 validation query.");
        return false;
    }

    const int32 Step =
        sqlite3_step(Statement);
    if (Step != SQLITE_ROW)
    {
        OutError =
            TEXT("Failed to execute migration 0011 validation query.");
        sqlite3_finalize(Statement);
        return false;
    }

    OutCount =
        sqlite3_column_int(
            Statement,
            0);
    sqlite3_finalize(Statement);
    return true;
}
}

bool FOGSQLiteWorldStore::UpsertWorldDirectorSchedule(
    const FOGWorldDirectorScheduleRecord& Schedule,
    int64 CreatedWorldTick,
    FString& OutError)
{
    OutError.Reset();

    if (!Schedule.ScheduleId.IsValid() ||
        !Schedule.ContentId.IsValid() ||
        Schedule.Status.IsNone() ||
        (Schedule.bHasEligibleSinceWorldTick &&
         Schedule.EligibleSinceWorldTick < 0) ||
        (Schedule.bHasScheduledStartWorldTick &&
         Schedule.ScheduledStartWorldTick < 0) ||
        (Schedule.bHasLatestStartWorldTick &&
         Schedule.LatestStartWorldTick < 0) ||
        (Schedule.bHasScheduledStartWorldTick &&
         Schedule.bHasLatestStartWorldTick &&
         Schedule.ScheduledStartWorldTick >
             Schedule.LatestStartWorldTick))
    {
        OutError =
            TEXT("World Director schedule is invalid.");
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
            Schedule.ScheduleId,
            TEXT("world_director_schedule"),
            CreatedWorldTick,
            TEXT("{}"),
            Error))
    {
        return Fail(Error);
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "INSERT INTO world_director_schedule("
        "schedule_entity_id, content_id, template_content_id, status, "
        "eligible_since_world_tick, scheduled_start_world_tick, latest_start_world_tick, "
        "resolution_seed, decision_provenance_json"
        ") VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?) "
        "ON CONFLICT(schedule_entity_id) DO UPDATE SET "
        "content_id = excluded.content_id, "
        "template_content_id = excluded.template_content_id, "
        "status = excluded.status, "
        "eligible_since_world_tick = excluded.eligible_since_world_tick, "
        "scheduled_start_world_tick = excluded.scheduled_start_world_tick, "
        "latest_start_world_tick = excluded.latest_start_world_tick, "
        "resolution_seed = excluded.resolution_seed, "
        "decision_provenance_json = excluded.decision_provenance_json;";

    if (sqlite3_prepare_v2(
            Database,
            Sql,
            -1,
            &Statement,
            nullptr) != SQLITE_OK)
    {
        return Fail(
            LastError(TEXT("Prepare World Director schedule upsert")));
    }

    const bool bSucceeded =
        BindDirectorText(
            Statement,
            1,
            Schedule.ScheduleId.ToString()) &&
        BindDirectorText(
            Statement,
            2,
            Schedule.ContentId.ToString()) &&
        BindOptionalDirectorContentId(
            Statement,
            3,
            Schedule.TemplateId) &&
        BindDirectorText(
            Statement,
            4,
            Schedule.Status.ToString()) &&
        BindOptionalDirectorTick(
            Statement,
            5,
            Schedule.bHasEligibleSinceWorldTick,
            Schedule.EligibleSinceWorldTick) &&
        BindOptionalDirectorTick(
            Statement,
            6,
            Schedule.bHasScheduledStartWorldTick,
            Schedule.ScheduledStartWorldTick) &&
        BindOptionalDirectorTick(
            Statement,
            7,
            Schedule.bHasLatestStartWorldTick,
            Schedule.LatestStartWorldTick) &&
        sqlite3_bind_int64(
            Statement,
            8,
            Schedule.ResolutionSeed) == SQLITE_OK &&
        BindDirectorText(
            Statement,
            9,
            Schedule.DecisionProvenanceJson.IsEmpty()
                ? TEXT("{}")
                : Schedule.DecisionProvenanceJson) &&
        sqlite3_step(Statement) ==
            SQLITE_DONE;

    const FString SqlError =
        bSucceeded
            ? FString()
            : LastError(TEXT("Upsert World Director schedule"));
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

bool FOGSQLiteWorldStore::TryReadWorldDirectorSchedule(
    const FOGEntityId& ScheduleId,
    bool& bOutFound,
    FOGWorldDirectorScheduleRecord& OutSchedule,
    FString& OutError) const
{
    bOutFound = false;
    OutSchedule =
        FOGWorldDirectorScheduleRecord();
    OutError.Reset();

    if (!ScheduleId.IsValid())
    {
        OutError =
            TEXT("World Director schedule lookup requires a valid ID.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "SELECT content_id, template_content_id, status, eligible_since_world_tick, "
        "scheduled_start_world_tick, latest_start_world_tick, resolution_seed, "
        "decision_provenance_json "
        "FROM world_director_schedule WHERE schedule_entity_id = ?;";

    if (sqlite3_prepare_v2(
            Database,
            Sql,
            -1,
            &Statement,
            nullptr) != SQLITE_OK)
    {
        OutError =
            LastError(TEXT("Prepare World Director schedule read"));
        return false;
    }

    if (!BindDirectorText(
            Statement,
            1,
            ScheduleId.ToString()))
    {
        OutError =
            LastError(TEXT("Bind World Director schedule read"));
        sqlite3_finalize(Statement);
        return false;
    }

    const int32 Step =
        sqlite3_step(Statement);
    if (Step == SQLITE_ROW)
    {
        bOutFound = true;
        OutSchedule.ScheduleId =
            ScheduleId;
        OutSchedule.ContentId =
            FOGContentId(
                DirectorColumnText(
                    Statement,
                    0));

        const FString TemplateId =
            DirectorColumnText(
                Statement,
                1);
        if (!TemplateId.IsEmpty())
        {
            OutSchedule.TemplateId =
                FOGContentId(TemplateId);
        }

        OutSchedule.Status =
            FName(*DirectorColumnText(
                Statement,
                2));

        if (sqlite3_column_type(
                Statement,
                3) != SQLITE_NULL)
        {
            OutSchedule.bHasEligibleSinceWorldTick =
                true;
            OutSchedule.EligibleSinceWorldTick =
                sqlite3_column_int64(
                    Statement,
                    3);
        }

        if (sqlite3_column_type(
                Statement,
                4) != SQLITE_NULL)
        {
            OutSchedule.bHasScheduledStartWorldTick =
                true;
            OutSchedule.ScheduledStartWorldTick =
                sqlite3_column_int64(
                    Statement,
                    4);
        }

        if (sqlite3_column_type(
                Statement,
                5) != SQLITE_NULL)
        {
            OutSchedule.bHasLatestStartWorldTick =
                true;
            OutSchedule.LatestStartWorldTick =
                sqlite3_column_int64(
                    Statement,
                    5);
        }

        OutSchedule.ResolutionSeed =
            sqlite3_column_int64(
                Statement,
                6);
        OutSchedule.DecisionProvenanceJson =
            DirectorColumnText(
                Statement,
                7);

        if (!OutSchedule.ContentId.IsValid() ||
            OutSchedule.Status.IsNone())
        {
            OutError =
                TEXT("Stored World Director schedule is invalid.");
            sqlite3_finalize(Statement);
            return false;
        }
    }
    else if (Step != SQLITE_DONE)
    {
        OutError =
            LastError(TEXT("Read World Director schedule"));
        sqlite3_finalize(Statement);
        return false;
    }

    sqlite3_finalize(Statement);
    return true;
}

bool FOGSQLiteWorldStore::ListWorldDirectorSchedulesByContent(
    const FOGContentId& ContentId,
    TArray<FOGWorldDirectorScheduleRecord>& OutSchedules,
    FString& OutError) const
{
    OutSchedules.Reset();
    OutError.Reset();

    if (!ContentId.IsValid())
    {
        OutError =
            TEXT("World Director schedule list requires a valid content ID.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "SELECT schedule_entity_id FROM world_director_schedule "
        "WHERE content_id = ? "
        "ORDER BY scheduled_start_world_tick ASC, schedule_entity_id ASC;";

    if (sqlite3_prepare_v2(
            Database,
            Sql,
            -1,
            &Statement,
            nullptr) != SQLITE_OK)
    {
        OutError =
            LastError(TEXT("Prepare World Director schedule list"));
        return false;
    }

    if (!BindDirectorText(
            Statement,
            1,
            ContentId.ToString()))
    {
        OutError =
            LastError(TEXT("Bind World Director schedule list"));
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
                LastError(TEXT("Read World Director schedule list"));
            sqlite3_finalize(Statement);
            return false;
        }

        const FOGEntityId Id =
            ParseDirectorEntityId(
                DirectorColumnText(
                    Statement,
                    0));
        if (!Id.IsValid())
        {
            OutError =
                TEXT("Stored Director schedule list contains an invalid ID.");
            sqlite3_finalize(Statement);
            return false;
        }
        Ids.Add(Id);
    }

    sqlite3_finalize(Statement);

    for (const FOGEntityId& Id : Ids)
    {
        bool bFound = false;
        FOGWorldDirectorScheduleRecord Schedule;
        if (!TryReadWorldDirectorSchedule(
                Id,
                bFound,
                Schedule,
                OutError) ||
            !bFound)
        {
            if (OutError.IsEmpty())
            {
                OutError =
                    TEXT("Director schedule disappeared during list read.");
            }
            OutSchedules.Reset();
            return false;
        }

        OutSchedules.Add(MoveTemp(Schedule));
    }

    return true;
}

bool FOGSQLiteWorldStore::UpsertContentUnlockState(
    const FOGContentUnlockStateRecord& Unlock,
    FString& OutError)
{
    OutError.Reset();

    if (!Unlock.ContentId.IsValid() ||
        Unlock.State.IsNone() ||
        (Unlock.bHasEligibleWorldTick &&
         Unlock.EligibleWorldTick < 0) ||
        (Unlock.bHasReleasedWorldTick &&
         Unlock.ReleasedWorldTick < 0) ||
        (Unlock.bHasEligibleWorldTick &&
         Unlock.bHasReleasedWorldTick &&
         Unlock.ReleasedWorldTick <
             Unlock.EligibleWorldTick))
    {
        OutError =
            TEXT("Content unlock state is invalid.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "INSERT INTO content_unlock_state("
        "content_id, state, eligible_world_tick, released_world_tick, state_json"
        ") VALUES(?, ?, ?, ?, ?) "
        "ON CONFLICT(content_id) DO UPDATE SET "
        "state = excluded.state, "
        "eligible_world_tick = excluded.eligible_world_tick, "
        "released_world_tick = excluded.released_world_tick, "
        "state_json = excluded.state_json;";

    if (sqlite3_prepare_v2(
            Database,
            Sql,
            -1,
            &Statement,
            nullptr) != SQLITE_OK)
    {
        OutError =
            LastError(TEXT("Prepare content unlock upsert"));
        return false;
    }

    const bool bSucceeded =
        BindDirectorText(
            Statement,
            1,
            Unlock.ContentId.ToString()) &&
        BindDirectorText(
            Statement,
            2,
            Unlock.State.ToString()) &&
        BindOptionalDirectorTick(
            Statement,
            3,
            Unlock.bHasEligibleWorldTick,
            Unlock.EligibleWorldTick) &&
        BindOptionalDirectorTick(
            Statement,
            4,
            Unlock.bHasReleasedWorldTick,
            Unlock.ReleasedWorldTick) &&
        BindDirectorText(
            Statement,
            5,
            Unlock.StateJson.IsEmpty()
                ? TEXT("{}")
                : Unlock.StateJson) &&
        sqlite3_step(Statement) ==
            SQLITE_DONE;

    if (!bSucceeded)
    {
        OutError =
            LastError(TEXT("Upsert content unlock state"));
    }

    sqlite3_finalize(Statement);
    return bSucceeded;
}

bool FOGSQLiteWorldStore::TryReadContentUnlockState(
    const FOGContentId& ContentId,
    bool& bOutFound,
    FOGContentUnlockStateRecord& OutUnlock,
    FString& OutError) const
{
    bOutFound = false;
    OutUnlock =
        FOGContentUnlockStateRecord();
    OutError.Reset();

    if (!ContentId.IsValid())
    {
        OutError =
            TEXT("Content unlock lookup requires a valid content ID.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "SELECT state, eligible_world_tick, released_world_tick, state_json "
        "FROM content_unlock_state WHERE content_id = ?;";

    if (sqlite3_prepare_v2(
            Database,
            Sql,
            -1,
            &Statement,
            nullptr) != SQLITE_OK)
    {
        OutError =
            LastError(TEXT("Prepare content unlock read"));
        return false;
    }

    if (!BindDirectorText(
            Statement,
            1,
            ContentId.ToString()))
    {
        OutError =
            LastError(TEXT("Bind content unlock read"));
        sqlite3_finalize(Statement);
        return false;
    }

    const int32 Step =
        sqlite3_step(Statement);
    if (Step == SQLITE_ROW)
    {
        bOutFound = true;
        OutUnlock.ContentId =
            ContentId;
        OutUnlock.State =
            FName(*DirectorColumnText(
                Statement,
                0));

        if (sqlite3_column_type(
                Statement,
                1) != SQLITE_NULL)
        {
            OutUnlock.bHasEligibleWorldTick =
                true;
            OutUnlock.EligibleWorldTick =
                sqlite3_column_int64(
                    Statement,
                    1);
        }

        if (sqlite3_column_type(
                Statement,
                2) != SQLITE_NULL)
        {
            OutUnlock.bHasReleasedWorldTick =
                true;
            OutUnlock.ReleasedWorldTick =
                sqlite3_column_int64(
                    Statement,
                    2);
        }

        OutUnlock.StateJson =
            DirectorColumnText(
                Statement,
                3);

        if (OutUnlock.State.IsNone())
        {
            OutError =
                TEXT("Stored content unlock state is invalid.");
            sqlite3_finalize(Statement);
            return false;
        }
    }
    else if (Step != SQLITE_DONE)
    {
        OutError =
            LastError(TEXT("Read content unlock state"));
        sqlite3_finalize(Statement);
        return false;
    }

    sqlite3_finalize(Statement);
    return true;
}

bool FOGSQLiteWorldStore::UpsertOfflineSimulationState(
    const FOGOfflineSimulationStateRecord& State,
    FString& OutError)
{
    OutError.Reset();

    if (!State.ScopeEntityId.IsValid() ||
        State.LastActiveWorldTick < 0 ||
        State.LastCatchupWorldTick < 0)
    {
        OutError =
            TEXT("Offline simulation state is invalid.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "INSERT INTO offline_simulation_state("
        "scope_entity_id, last_active_world_tick, last_catchup_world_tick, governor_state_json"
        ") VALUES(?, ?, ?, ?) "
        "ON CONFLICT(scope_entity_id) DO UPDATE SET "
        "last_active_world_tick = excluded.last_active_world_tick, "
        "last_catchup_world_tick = excluded.last_catchup_world_tick, "
        "governor_state_json = excluded.governor_state_json;";

    if (sqlite3_prepare_v2(
            Database,
            Sql,
            -1,
            &Statement,
            nullptr) != SQLITE_OK)
    {
        OutError =
            LastError(TEXT("Prepare offline simulation upsert"));
        return false;
    }

    const bool bSucceeded =
        BindDirectorText(
            Statement,
            1,
            State.ScopeEntityId.ToString()) &&
        sqlite3_bind_int64(
            Statement,
            2,
            State.LastActiveWorldTick) == SQLITE_OK &&
        sqlite3_bind_int64(
            Statement,
            3,
            State.LastCatchupWorldTick) == SQLITE_OK &&
        BindDirectorText(
            Statement,
            4,
            State.GovernorStateJson.IsEmpty()
                ? TEXT("{}")
                : State.GovernorStateJson) &&
        sqlite3_step(Statement) ==
            SQLITE_DONE;

    if (!bSucceeded)
    {
        OutError =
            LastError(TEXT("Upsert offline simulation state"));
    }

    sqlite3_finalize(Statement);
    return bSucceeded;
}

bool FOGSQLiteWorldStore::TryReadOfflineSimulationState(
    const FOGEntityId& ScopeEntityId,
    bool& bOutFound,
    FOGOfflineSimulationStateRecord& OutState,
    FString& OutError) const
{
    bOutFound = false;
    OutState =
        FOGOfflineSimulationStateRecord();
    OutError.Reset();

    if (!ScopeEntityId.IsValid())
    {
        OutError =
            TEXT("Offline simulation lookup requires a valid scope entity.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "SELECT last_active_world_tick, last_catchup_world_tick, governor_state_json "
        "FROM offline_simulation_state WHERE scope_entity_id = ?;";

    if (sqlite3_prepare_v2(
            Database,
            Sql,
            -1,
            &Statement,
            nullptr) != SQLITE_OK)
    {
        OutError =
            LastError(TEXT("Prepare offline simulation read"));
        return false;
    }

    if (!BindDirectorText(
            Statement,
            1,
            ScopeEntityId.ToString()))
    {
        OutError =
            LastError(TEXT("Bind offline simulation read"));
        sqlite3_finalize(Statement);
        return false;
    }

    const int32 Step =
        sqlite3_step(Statement);
    if (Step == SQLITE_ROW)
    {
        bOutFound = true;
        OutState.ScopeEntityId =
            ScopeEntityId;
        OutState.LastActiveWorldTick =
            sqlite3_column_int64(
                Statement,
                0);
        OutState.LastCatchupWorldTick =
            sqlite3_column_int64(
                Statement,
                1);
        OutState.GovernorStateJson =
            DirectorColumnText(
                Statement,
                2);
    }
    else if (Step != SQLITE_DONE)
    {
        OutError =
            LastError(TEXT("Read offline simulation state"));
        sqlite3_finalize(Statement);
        return false;
    }

    sqlite3_finalize(Statement);
    return true;
}

bool FOGSQLiteWorldStore::ValidateRealityTimeDirectorMigration0011(
    FString& OutError) const
{
    OutError.Reset();

    int32 InvalidTimeDomains = 0;
    if (!ReadDirectorCount(
            Database,
            "SELECT COUNT(*) FROM time_domains "
            "WHERE rate_numerator <= 0 OR rate_denominator <= 0 "
            "OR time_domain_entity_id = parent_time_domain_entity_id;",
            InvalidTimeDomains,
            OutError))
    {
        return false;
    }

    if (InvalidTimeDomains != 0)
    {
        OutError = FString::Printf(
            TEXT("Migration 0011 produced %d invalid Time Domain rows."),
            InvalidTimeDomains);
        return false;
    }

    int32 InvalidRealityParents = 0;
    if (!ReadDirectorCount(
            Database,
            "SELECT COUNT(*) FROM reality_nodes "
            "WHERE reality_entity_id = parent_reality_entity_id;",
            InvalidRealityParents,
            OutError))
    {
        return false;
    }

    if (InvalidRealityParents != 0)
    {
        OutError = FString::Printf(
            TEXT("Migration 0011 produced %d self-parented Reality nodes."),
            InvalidRealityParents);
        return false;
    }

    int32 InvalidJunctions = 0;
    if (!ReadDirectorCount(
            Database,
            "SELECT COUNT(*) FROM junctions "
            "WHERE from_reality_entity_id = to_reality_entity_id "
            "OR stability_bps < 0 OR stability_bps > 10000;",
            InvalidJunctions,
            OutError))
    {
        return false;
    }

    if (InvalidJunctions != 0)
    {
        OutError = FString::Printf(
            TEXT("Migration 0011 produced %d invalid Junction rows."),
            InvalidJunctions);
        return false;
    }

    int32 InvalidSchedules = 0;
    if (!ReadDirectorCount(
            Database,
            "SELECT COUNT(*) FROM world_director_schedule "
            "WHERE scheduled_start_world_tick IS NOT NULL "
            "AND latest_start_world_tick IS NOT NULL "
            "AND scheduled_start_world_tick > latest_start_world_tick;",
            InvalidSchedules,
            OutError))
    {
        return false;
    }

    if (InvalidSchedules != 0)
    {
        OutError = FString::Printf(
            TEXT("Migration 0011 produced %d schedules beyond authored latest-start bounds."),
            InvalidSchedules);
        return false;
    }

    return true;
}
