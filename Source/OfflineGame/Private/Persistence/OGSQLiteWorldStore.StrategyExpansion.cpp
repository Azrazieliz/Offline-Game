#include "Persistence/OGSQLiteWorldStore.h"

#include "sqlite/sqlite3.h"

namespace
{
bool BindExpansionText(
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

FString ExpansionColumnText(
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

FOGEntityId ParseExpansionEntityId(
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

bool BindOptionalExpansionEntityId(
    sqlite3_stmt* Statement,
    int32 Index,
    const FOGEntityId& EntityId)
{
    return EntityId.IsValid()
        ? BindExpansionText(
            Statement,
            Index,
            EntityId.ToString())
        : sqlite3_bind_null(
            Statement,
            Index) == SQLITE_OK;
}

bool BindOptionalExpansionTick(
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

bool ReadExpansionCount(
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
            TEXT("Failed to prepare strategy migration validation query.");
        return false;
    }

    const int32 Step =
        sqlite3_step(Statement);
    if (Step != SQLITE_ROW)
    {
        OutError =
            TEXT("Failed to execute strategy migration validation query.");
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

bool FOGSQLiteWorldStore::UpsertDispatchObjective(
    const FOGDispatchObjectiveRecord& Objective,
    FString& OutError)
{
    OutError.Reset();

    if (!Objective.DispatchId.IsValid() ||
        !Objective.ObjectiveId.IsValid())
    {
        OutError =
            TEXT("Dispatch objective is invalid.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "INSERT INTO dispatch_objectives("
        "dispatch_entity_id, objective_content_id, priority, mandatory, target_entity_id, state_json"
        ") VALUES(?, ?, ?, ?, ?, ?) "
        "ON CONFLICT(dispatch_entity_id, objective_content_id) DO UPDATE SET "
        "priority = excluded.priority, mandatory = excluded.mandatory, "
        "target_entity_id = excluded.target_entity_id, state_json = excluded.state_json;";

    if (sqlite3_prepare_v2(
            Database,
            Sql,
            -1,
            &Statement,
            nullptr) != SQLITE_OK)
    {
        OutError =
            LastError(TEXT("Prepare dispatch objective upsert"));
        return false;
    }

    const bool bSucceeded =
        BindExpansionText(
            Statement,
            1,
            Objective.DispatchId.ToString()) &&
        BindExpansionText(
            Statement,
            2,
            Objective.ObjectiveId.ToString()) &&
        sqlite3_bind_int(
            Statement,
            3,
            Objective.Priority) == SQLITE_OK &&
        sqlite3_bind_int(
            Statement,
            4,
            Objective.bMandatory ? 1 : 0) == SQLITE_OK &&
        BindOptionalExpansionEntityId(
            Statement,
            5,
            Objective.TargetEntityId) &&
        BindExpansionText(
            Statement,
            6,
            Objective.StateJson.IsEmpty()
                ? TEXT("{}")
                : Objective.StateJson) &&
        sqlite3_step(Statement) == SQLITE_DONE;

    if (!bSucceeded)
    {
        OutError =
            LastError(TEXT("Upsert dispatch objective"));
    }

    sqlite3_finalize(Statement);
    return bSucceeded;
}

bool FOGSQLiteWorldStore::ListDispatchObjectives(
    const FOGEntityId& DispatchId,
    TArray<FOGDispatchObjectiveRecord>& OutObjectives,
    FString& OutError) const
{
    OutObjectives.Reset();
    OutError.Reset();

    if (!DispatchId.IsValid())
    {
        OutError =
            TEXT("Dispatch objective list requires a valid Dispatch ID.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "SELECT objective_content_id, priority, mandatory, target_entity_id, state_json "
        "FROM dispatch_objectives WHERE dispatch_entity_id = ? "
        "ORDER BY mandatory DESC, priority DESC, objective_content_id ASC;";

    if (sqlite3_prepare_v2(
            Database,
            Sql,
            -1,
            &Statement,
            nullptr) != SQLITE_OK)
    {
        OutError =
            LastError(TEXT("Prepare dispatch objective list"));
        return false;
    }

    if (!BindExpansionText(
            Statement,
            1,
            DispatchId.ToString()))
    {
        OutError =
            LastError(TEXT("Bind dispatch objective list"));
        sqlite3_finalize(Statement);
        return false;
    }

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
                LastError(TEXT("Read dispatch objective list"));
            sqlite3_finalize(Statement);
            OutObjectives.Reset();
            return false;
        }

        FOGDispatchObjectiveRecord Objective;
        Objective.DispatchId =
            DispatchId;
        Objective.ObjectiveId =
            FOGContentId(
                ExpansionColumnText(
                    Statement,
                    0));
        Objective.Priority =
            sqlite3_column_int(
                Statement,
                1);
        Objective.bMandatory =
            sqlite3_column_int(
                Statement,
                2) != 0;
        Objective.TargetEntityId =
            ParseExpansionEntityId(
                ExpansionColumnText(
                    Statement,
                    3));
        Objective.StateJson =
            ExpansionColumnText(
                Statement,
                4);

        if (!Objective.ObjectiveId.IsValid())
        {
            OutError =
                TEXT("Stored Dispatch objective has an invalid content ID.");
            sqlite3_finalize(Statement);
            OutObjectives.Reset();
            return false;
        }

        OutObjectives.Add(
            MoveTemp(Objective));
    }

    sqlite3_finalize(Statement);
    return true;
}

bool FOGSQLiteWorldStore::UpsertDispatchConstraint(
    const FOGDispatchConstraintRecord& Constraint,
    FString& OutError)
{
    OutError.Reset();

    if (!Constraint.DispatchId.IsValid() ||
        !Constraint.ConstraintId.IsValid())
    {
        OutError =
            TEXT("Dispatch constraint is invalid.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "INSERT INTO dispatch_constraints("
        "dispatch_entity_id, constraint_content_id, state_json"
        ") VALUES(?, ?, ?) "
        "ON CONFLICT(dispatch_entity_id, constraint_content_id) DO UPDATE SET "
        "state_json = excluded.state_json;";

    if (sqlite3_prepare_v2(
            Database,
            Sql,
            -1,
            &Statement,
            nullptr) != SQLITE_OK)
    {
        OutError =
            LastError(TEXT("Prepare dispatch constraint upsert"));
        return false;
    }

    const bool bSucceeded =
        BindExpansionText(
            Statement,
            1,
            Constraint.DispatchId.ToString()) &&
        BindExpansionText(
            Statement,
            2,
            Constraint.ConstraintId.ToString()) &&
        BindExpansionText(
            Statement,
            3,
            Constraint.StateJson.IsEmpty()
                ? TEXT("{}")
                : Constraint.StateJson) &&
        sqlite3_step(Statement) == SQLITE_DONE;

    if (!bSucceeded)
    {
        OutError =
            LastError(TEXT("Upsert dispatch constraint"));
    }

    sqlite3_finalize(Statement);
    return bSucceeded;
}

bool FOGSQLiteWorldStore::ListDispatchConstraints(
    const FOGEntityId& DispatchId,
    TArray<FOGDispatchConstraintRecord>& OutConstraints,
    FString& OutError) const
{
    OutConstraints.Reset();
    OutError.Reset();

    if (!DispatchId.IsValid())
    {
        OutError =
            TEXT("Dispatch constraint list requires a valid Dispatch ID.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "SELECT constraint_content_id, state_json "
        "FROM dispatch_constraints WHERE dispatch_entity_id = ? "
        "ORDER BY constraint_content_id ASC;";

    if (sqlite3_prepare_v2(
            Database,
            Sql,
            -1,
            &Statement,
            nullptr) != SQLITE_OK)
    {
        OutError =
            LastError(TEXT("Prepare dispatch constraint list"));
        return false;
    }

    if (!BindExpansionText(
            Statement,
            1,
            DispatchId.ToString()))
    {
        OutError =
            LastError(TEXT("Bind dispatch constraint list"));
        sqlite3_finalize(Statement);
        return false;
    }

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
                LastError(TEXT("Read dispatch constraint list"));
            sqlite3_finalize(Statement);
            OutConstraints.Reset();
            return false;
        }

        FOGDispatchConstraintRecord Constraint;
        Constraint.DispatchId =
            DispatchId;
        Constraint.ConstraintId =
            FOGContentId(
                ExpansionColumnText(
                    Statement,
                    0));
        Constraint.StateJson =
            ExpansionColumnText(
                Statement,
                1);

        if (!Constraint.ConstraintId.IsValid())
        {
            OutError =
                TEXT("Stored Dispatch constraint has an invalid content ID.");
            sqlite3_finalize(Statement);
            OutConstraints.Reset();
            return false;
        }

        OutConstraints.Add(
            MoveTemp(Constraint));
    }

    sqlite3_finalize(Statement);
    return true;
}

bool FOGSQLiteWorldStore::UpsertArmyCapability(
    const FOGArmyCapabilityRecord& Capability,
    FString& OutError)
{
    OutError.Reset();

    if (!Capability.ArmyId.IsValid() ||
        !Capability.CapabilityId.IsValid())
    {
        OutError =
            TEXT("Army capability is invalid.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "INSERT INTO army_capabilities("
        "army_entity_id, capability_content_id, magnitude_sig, magnitude_exp, state_json"
        ") VALUES(?, ?, ?, ?, ?) "
        "ON CONFLICT(army_entity_id, capability_content_id) DO UPDATE SET "
        "magnitude_sig = excluded.magnitude_sig, magnitude_exp = excluded.magnitude_exp, "
        "state_json = excluded.state_json;";

    if (sqlite3_prepare_v2(
            Database,
            Sql,
            -1,
            &Statement,
            nullptr) != SQLITE_OK)
    {
        OutError =
            LastError(TEXT("Prepare Army capability upsert"));
        return false;
    }

    const bool bSucceeded =
        BindExpansionText(
            Statement,
            1,
            Capability.ArmyId.ToString()) &&
        BindExpansionText(
            Statement,
            2,
            Capability.CapabilityId.ToString()) &&
        sqlite3_bind_int64(
            Statement,
            3,
            Capability.Magnitude.Significand) == SQLITE_OK &&
        sqlite3_bind_int(
            Statement,
            4,
            Capability.Magnitude.Exponent10) == SQLITE_OK &&
        BindExpansionText(
            Statement,
            5,
            Capability.StateJson.IsEmpty()
                ? TEXT("{}")
                : Capability.StateJson) &&
        sqlite3_step(Statement) == SQLITE_DONE;

    if (!bSucceeded)
    {
        OutError =
            LastError(TEXT("Upsert Army capability"));
    }

    sqlite3_finalize(Statement);
    return bSucceeded;
}

bool FOGSQLiteWorldStore::ListArmyCapabilities(
    const FOGEntityId& ArmyId,
    TArray<FOGArmyCapabilityRecord>& OutCapabilities,
    FString& OutError) const
{
    OutCapabilities.Reset();
    OutError.Reset();

    if (!ArmyId.IsValid())
    {
        OutError =
            TEXT("Army capability list requires a valid Army ID.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "SELECT capability_content_id, magnitude_sig, magnitude_exp, state_json "
        "FROM army_capabilities WHERE army_entity_id = ? "
        "ORDER BY capability_content_id ASC;";

    if (sqlite3_prepare_v2(
            Database,
            Sql,
            -1,
            &Statement,
            nullptr) != SQLITE_OK)
    {
        OutError =
            LastError(TEXT("Prepare Army capability list"));
        return false;
    }

    if (!BindExpansionText(
            Statement,
            1,
            ArmyId.ToString()))
    {
        OutError =
            LastError(TEXT("Bind Army capability list"));
        sqlite3_finalize(Statement);
        return false;
    }

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
                LastError(TEXT("Read Army capability list"));
            sqlite3_finalize(Statement);
            OutCapabilities.Reset();
            return false;
        }

        FOGArmyCapabilityRecord Capability;
        Capability.ArmyId =
            ArmyId;
        Capability.CapabilityId =
            FOGContentId(
                ExpansionColumnText(
                    Statement,
                    0));
        Capability.Magnitude =
            FOGLargeNumber(
                sqlite3_column_int64(
                    Statement,
                    1),
                sqlite3_column_int(
                    Statement,
                    2));
        Capability.StateJson =
            ExpansionColumnText(
                Statement,
                3);

        if (!Capability.CapabilityId.IsValid())
        {
            OutError =
                TEXT("Stored Army capability has an invalid content ID.");
            sqlite3_finalize(Statement);
            OutCapabilities.Reset();
            return false;
        }

        OutCapabilities.Add(
            MoveTemp(Capability));
    }

    sqlite3_finalize(Statement);
    return true;
}

bool FOGSQLiteWorldStore::UpsertWarFront(
    const FOGWarFrontRecord& Front,
    int64 CreatedWorldTick,
    FString& OutError)
{
    OutError.Reset();

    if (!Front.FrontId.IsValid() ||
        !Front.WarId.IsValid() ||
        Front.State.IsNone() ||
        Front.StartWorldTick < 0 ||
        (Front.bHasEndWorldTick &&
         Front.EndWorldTick <
             Front.StartWorldTick))
    {
        OutError =
            TEXT("War front is invalid.");
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
            Front.FrontId,
            FName(TEXT("war_front")),
            CreatedWorldTick,
            TEXT("{}"),
            Error))
    {
        return Fail(Error);
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "INSERT INTO war_fronts("
        "front_entity_id, war_entity_id, location_entity_id, reality_entity_id, "
        "state, start_world_tick, end_world_tick, state_json"
        ") VALUES(?, ?, ?, ?, ?, ?, ?, ?) "
        "ON CONFLICT(front_entity_id) DO UPDATE SET "
        "war_entity_id = excluded.war_entity_id, "
        "location_entity_id = excluded.location_entity_id, "
        "reality_entity_id = excluded.reality_entity_id, "
        "state = excluded.state, start_world_tick = excluded.start_world_tick, "
        "end_world_tick = excluded.end_world_tick, state_json = excluded.state_json;";

    if (sqlite3_prepare_v2(
            Database,
            Sql,
            -1,
            &Statement,
            nullptr) != SQLITE_OK)
    {
        return Fail(
            LastError(TEXT("Prepare War front upsert")));
    }

    const bool bSucceeded =
        BindExpansionText(
            Statement,
            1,
            Front.FrontId.ToString()) &&
        BindExpansionText(
            Statement,
            2,
            Front.WarId.ToString()) &&
        BindOptionalExpansionEntityId(
            Statement,
            3,
            Front.LocationId) &&
        BindOptionalExpansionEntityId(
            Statement,
            4,
            Front.RealityId) &&
        BindExpansionText(
            Statement,
            5,
            Front.State.ToString()) &&
        sqlite3_bind_int64(
            Statement,
            6,
            Front.StartWorldTick) == SQLITE_OK &&
        BindOptionalExpansionTick(
            Statement,
            7,
            Front.bHasEndWorldTick,
            Front.EndWorldTick) &&
        BindExpansionText(
            Statement,
            8,
            Front.StateJson.IsEmpty()
                ? TEXT("{}")
                : Front.StateJson) &&
        sqlite3_step(Statement) == SQLITE_DONE;

    const FString SqlError =
        bSucceeded
            ? FString()
            : LastError(TEXT("Upsert War front"));
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

bool FOGSQLiteWorldStore::TryReadWarFront(
    const FOGEntityId& FrontId,
    bool& bOutFound,
    FOGWarFrontRecord& OutFront,
    FString& OutError) const
{
    bOutFound = false;
    OutFront =
        FOGWarFrontRecord();
    OutError.Reset();

    if (!FrontId.IsValid())
    {
        OutError =
            TEXT("War front lookup requires a valid ID.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "SELECT war_entity_id, location_entity_id, reality_entity_id, state, "
        "start_world_tick, end_world_tick, state_json "
        "FROM war_fronts WHERE front_entity_id = ?;";

    if (sqlite3_prepare_v2(
            Database,
            Sql,
            -1,
            &Statement,
            nullptr) != SQLITE_OK)
    {
        OutError =
            LastError(TEXT("Prepare War front read"));
        return false;
    }

    if (!BindExpansionText(
            Statement,
            1,
            FrontId.ToString()))
    {
        OutError =
            LastError(TEXT("Bind War front read"));
        sqlite3_finalize(Statement);
        return false;
    }

    const int32 Step =
        sqlite3_step(Statement);
    if (Step == SQLITE_ROW)
    {
        bOutFound = true;
        OutFront.FrontId =
            FrontId;
        OutFront.WarId =
            ParseExpansionEntityId(
                ExpansionColumnText(
                    Statement,
                    0));
        OutFront.LocationId =
            ParseExpansionEntityId(
                ExpansionColumnText(
                    Statement,
                    1));
        OutFront.RealityId =
            ParseExpansionEntityId(
                ExpansionColumnText(
                    Statement,
                    2));
        OutFront.State =
            FName(*ExpansionColumnText(
                Statement,
                3));
        OutFront.StartWorldTick =
            sqlite3_column_int64(
                Statement,
                4);

        if (sqlite3_column_type(
                Statement,
                5) != SQLITE_NULL)
        {
            OutFront.bHasEndWorldTick =
                true;
            OutFront.EndWorldTick =
                sqlite3_column_int64(
                    Statement,
                    5);
        }

        OutFront.StateJson =
            ExpansionColumnText(
                Statement,
                6);

        if (!OutFront.WarId.IsValid() ||
            OutFront.State.IsNone())
        {
            OutError =
                TEXT("Stored War front is invalid.");
            sqlite3_finalize(Statement);
            return false;
        }
    }
    else if (Step != SQLITE_DONE)
    {
        OutError =
            LastError(TEXT("Read War front"));
        sqlite3_finalize(Statement);
        return false;
    }

    sqlite3_finalize(Statement);
    return true;
}

bool FOGSQLiteWorldStore::ListWarFronts(
    const FOGEntityId& WarId,
    TArray<FOGWarFrontRecord>& OutFronts,
    FString& OutError) const
{
    OutFronts.Reset();
    OutError.Reset();

    if (!WarId.IsValid())
    {
        OutError =
            TEXT("War front list requires a valid War ID.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "SELECT front_entity_id FROM war_fronts "
        "WHERE war_entity_id = ? ORDER BY start_world_tick, front_entity_id;";

    if (sqlite3_prepare_v2(
            Database,
            Sql,
            -1,
            &Statement,
            nullptr) != SQLITE_OK)
    {
        OutError =
            LastError(TEXT("Prepare War front list"));
        return false;
    }

    if (!BindExpansionText(
            Statement,
            1,
            WarId.ToString()))
    {
        OutError =
            LastError(TEXT("Bind War front list"));
        sqlite3_finalize(Statement);
        return false;
    }

    TArray<FOGEntityId> FrontIds;
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
                LastError(TEXT("Read War front list"));
            sqlite3_finalize(Statement);
            return false;
        }

        const FOGEntityId FrontId =
            ParseExpansionEntityId(
                ExpansionColumnText(
                    Statement,
                    0));

        if (!FrontId.IsValid())
        {
            OutError =
                TEXT("Stored War front list contains invalid ID.");
            sqlite3_finalize(Statement);
            return false;
        }

        FrontIds.Add(FrontId);
    }

    sqlite3_finalize(Statement);

    for (const FOGEntityId& FrontId :
         FrontIds)
    {
        bool bFound = false;
        FOGWarFrontRecord Front;
        if (!TryReadWarFront(
                FrontId,
                bFound,
                Front,
                OutError) ||
            !bFound)
        {
            if (OutError.IsEmpty())
            {
                OutError =
                    TEXT("War front disappeared during list read.");
            }
            OutFronts.Reset();
            return false;
        }

        OutFronts.Add(
            MoveTemp(Front));
    }

    return true;
}

bool FOGSQLiteWorldStore::UpsertWarObjective(
    const FOGWarObjectiveRecord& Objective,
    FString& OutError)
{
    OutError.Reset();

    if (!Objective.WarId.IsValid() ||
        !Objective.ObjectiveId.IsValid() ||
        Objective.Status.IsNone())
    {
        OutError =
            TEXT("War objective is invalid.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "INSERT INTO war_objectives("
        "war_entity_id, front_entity_id, objective_content_id, target_entity_id, "
        "priority, status, state_json"
        ") VALUES(?, ?, ?, ?, ?, ?, ?) "
        "ON CONFLICT(war_entity_id, front_entity_id, objective_content_id) DO UPDATE SET "
        "target_entity_id = excluded.target_entity_id, priority = excluded.priority, "
        "status = excluded.status, state_json = excluded.state_json;";

    if (sqlite3_prepare_v2(
            Database,
            Sql,
            -1,
            &Statement,
            nullptr) != SQLITE_OK)
    {
        OutError =
            LastError(TEXT("Prepare War objective upsert"));
        return false;
    }

    const bool bSucceeded =
        BindExpansionText(
            Statement,
            1,
            Objective.WarId.ToString()) &&
        BindExpansionText(
            Statement,
            2,
            Objective.FrontId.IsValid()
                ? Objective.FrontId.ToString()
                : FString()) &&
        BindExpansionText(
            Statement,
            3,
            Objective.ObjectiveId.ToString()) &&
        BindOptionalExpansionEntityId(
            Statement,
            4,
            Objective.TargetEntityId) &&
        sqlite3_bind_int(
            Statement,
            5,
            Objective.Priority) == SQLITE_OK &&
        BindExpansionText(
            Statement,
            6,
            Objective.Status.ToString()) &&
        BindExpansionText(
            Statement,
            7,
            Objective.StateJson.IsEmpty()
                ? TEXT("{}")
                : Objective.StateJson) &&
        sqlite3_step(Statement) == SQLITE_DONE;

    if (!bSucceeded)
    {
        OutError =
            LastError(TEXT("Upsert War objective"));
    }

    sqlite3_finalize(Statement);
    return bSucceeded;
}

bool FOGSQLiteWorldStore::ListWarObjectives(
    const FOGEntityId& WarId,
    TArray<FOGWarObjectiveRecord>& OutObjectives,
    FString& OutError) const
{
    OutObjectives.Reset();
    OutError.Reset();

    if (!WarId.IsValid())
    {
        OutError =
            TEXT("War objective list requires a valid War ID.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "SELECT front_entity_id, objective_content_id, target_entity_id, "
        "priority, status, state_json "
        "FROM war_objectives WHERE war_entity_id = ? "
        "ORDER BY priority DESC, objective_content_id ASC;";

    if (sqlite3_prepare_v2(
            Database,
            Sql,
            -1,
            &Statement,
            nullptr) != SQLITE_OK)
    {
        OutError =
            LastError(TEXT("Prepare War objective list"));
        return false;
    }

    if (!BindExpansionText(
            Statement,
            1,
            WarId.ToString()))
    {
        OutError =
            LastError(TEXT("Bind War objective list"));
        sqlite3_finalize(Statement);
        return false;
    }

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
                LastError(TEXT("Read War objective list"));
            sqlite3_finalize(Statement);
            OutObjectives.Reset();
            return false;
        }

        FOGWarObjectiveRecord Objective;
        Objective.WarId =
            WarId;
        Objective.FrontId =
            ParseExpansionEntityId(
                ExpansionColumnText(
                    Statement,
                    0));
        Objective.ObjectiveId =
            FOGContentId(
                ExpansionColumnText(
                    Statement,
                    1));
        Objective.TargetEntityId =
            ParseExpansionEntityId(
                ExpansionColumnText(
                    Statement,
                    2));
        Objective.Priority =
            sqlite3_column_int(
                Statement,
                3);
        Objective.Status =
            FName(*ExpansionColumnText(
                Statement,
                4));
        Objective.StateJson =
            ExpansionColumnText(
                Statement,
                5);

        if (!Objective.ObjectiveId.IsValid() ||
            Objective.Status.IsNone())
        {
            OutError =
                TEXT("Stored War objective is invalid.");
            sqlite3_finalize(Statement);
            OutObjectives.Reset();
            return false;
        }

        OutObjectives.Add(
            MoveTemp(Objective));
    }

    sqlite3_finalize(Statement);
    return true;
}

bool FOGSQLiteWorldStore::UpsertWarOrder(
    const FOGWarOrderRecord& Order,
    int64 CreatedWorldTick,
    FString& OutError)
{
    OutError.Reset();

    if (!Order.OrderId.IsValid() ||
        !Order.WarId.IsValid() ||
        !Order.IssuerEntityId.IsValid() ||
        !Order.RecipientEntityId.IsValid() ||
        !Order.IntentId.IsValid() ||
        Order.IssuedWorldTick < 0 ||
        Order.OutcomeState.IsNone())
    {
        OutError =
            TEXT("War order is invalid.");
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
            Order.OrderId,
            FName(TEXT("war_order")),
            CreatedWorldTick,
            TEXT("{}"),
            Error))
    {
        return Fail(Error);
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "INSERT INTO war_orders("
        "order_entity_id, war_entity_id, front_entity_id, issuer_entity_id, "
        "recipient_entity_id, intent_content_id, constraints_json, issued_world_tick, "
        "outcome_state, outcome_json"
        ") VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?) "
        "ON CONFLICT(order_entity_id) DO UPDATE SET "
        "war_entity_id = excluded.war_entity_id, front_entity_id = excluded.front_entity_id, "
        "issuer_entity_id = excluded.issuer_entity_id, recipient_entity_id = excluded.recipient_entity_id, "
        "intent_content_id = excluded.intent_content_id, constraints_json = excluded.constraints_json, "
        "issued_world_tick = excluded.issued_world_tick, outcome_state = excluded.outcome_state, "
        "outcome_json = excluded.outcome_json;";

    if (sqlite3_prepare_v2(
            Database,
            Sql,
            -1,
            &Statement,
            nullptr) != SQLITE_OK)
    {
        return Fail(
            LastError(TEXT("Prepare War order upsert")));
    }

    const bool bSucceeded =
        BindExpansionText(
            Statement,
            1,
            Order.OrderId.ToString()) &&
        BindExpansionText(
            Statement,
            2,
            Order.WarId.ToString()) &&
        BindOptionalExpansionEntityId(
            Statement,
            3,
            Order.FrontId) &&
        BindExpansionText(
            Statement,
            4,
            Order.IssuerEntityId.ToString()) &&
        BindExpansionText(
            Statement,
            5,
            Order.RecipientEntityId.ToString()) &&
        BindExpansionText(
            Statement,
            6,
            Order.IntentId.ToString()) &&
        BindExpansionText(
            Statement,
            7,
            Order.ConstraintsJson.IsEmpty()
                ? TEXT("{}")
                : Order.ConstraintsJson) &&
        sqlite3_bind_int64(
            Statement,
            8,
            Order.IssuedWorldTick) == SQLITE_OK &&
        BindExpansionText(
            Statement,
            9,
            Order.OutcomeState.ToString()) &&
        BindExpansionText(
            Statement,
            10,
            Order.OutcomeJson.IsEmpty()
                ? TEXT("{}")
                : Order.OutcomeJson) &&
        sqlite3_step(Statement) == SQLITE_DONE;

    const FString SqlError =
        bSucceeded
            ? FString()
            : LastError(TEXT("Upsert War order"));
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

bool FOGSQLiteWorldStore::TryReadWarOrder(
    const FOGEntityId& OrderId,
    bool& bOutFound,
    FOGWarOrderRecord& OutOrder,
    FString& OutError) const
{
    bOutFound = false;
    OutOrder =
        FOGWarOrderRecord();
    OutError.Reset();

    if (!OrderId.IsValid())
    {
        OutError =
            TEXT("War order lookup requires a valid ID.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "SELECT war_entity_id, front_entity_id, issuer_entity_id, recipient_entity_id, "
        "intent_content_id, constraints_json, issued_world_tick, outcome_state, outcome_json "
        "FROM war_orders WHERE order_entity_id = ?;";

    if (sqlite3_prepare_v2(
            Database,
            Sql,
            -1,
            &Statement,
            nullptr) != SQLITE_OK)
    {
        OutError =
            LastError(TEXT("Prepare War order read"));
        return false;
    }

    if (!BindExpansionText(
            Statement,
            1,
            OrderId.ToString()))
    {
        OutError =
            LastError(TEXT("Bind War order read"));
        sqlite3_finalize(Statement);
        return false;
    }

    const int32 Step =
        sqlite3_step(Statement);
    if (Step == SQLITE_ROW)
    {
        bOutFound = true;
        OutOrder.OrderId =
            OrderId;
        OutOrder.WarId =
            ParseExpansionEntityId(
                ExpansionColumnText(
                    Statement,
                    0));
        OutOrder.FrontId =
            ParseExpansionEntityId(
                ExpansionColumnText(
                    Statement,
                    1));
        OutOrder.IssuerEntityId =
            ParseExpansionEntityId(
                ExpansionColumnText(
                    Statement,
                    2));
        OutOrder.RecipientEntityId =
            ParseExpansionEntityId(
                ExpansionColumnText(
                    Statement,
                    3));
        OutOrder.IntentId =
            FOGContentId(
                ExpansionColumnText(
                    Statement,
                    4));
        OutOrder.ConstraintsJson =
            ExpansionColumnText(
                Statement,
                5);
        OutOrder.IssuedWorldTick =
            sqlite3_column_int64(
                Statement,
                6);
        OutOrder.OutcomeState =
            FName(*ExpansionColumnText(
                Statement,
                7));
        OutOrder.OutcomeJson =
            ExpansionColumnText(
                Statement,
                8);

        if (!OutOrder.WarId.IsValid() ||
            !OutOrder.IssuerEntityId.IsValid() ||
            !OutOrder.RecipientEntityId.IsValid() ||
            !OutOrder.IntentId.IsValid() ||
            OutOrder.OutcomeState.IsNone())
        {
            OutError =
                TEXT("Stored War order is invalid.");
            sqlite3_finalize(Statement);
            return false;
        }
    }
    else if (Step != SQLITE_DONE)
    {
        OutError =
            LastError(TEXT("Read War order"));
        sqlite3_finalize(Statement);
        return false;
    }

    sqlite3_finalize(Statement);
    return true;
}

bool FOGSQLiteWorldStore::ListWarOrders(
    const FOGEntityId& WarId,
    TArray<FOGWarOrderRecord>& OutOrders,
    FString& OutError) const
{
    OutOrders.Reset();
    OutError.Reset();

    if (!WarId.IsValid())
    {
        OutError =
            TEXT("War order list requires a valid War ID.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "SELECT order_entity_id FROM war_orders "
        "WHERE war_entity_id = ? ORDER BY issued_world_tick, order_entity_id;";

    if (sqlite3_prepare_v2(
            Database,
            Sql,
            -1,
            &Statement,
            nullptr) != SQLITE_OK)
    {
        OutError =
            LastError(TEXT("Prepare War order list"));
        return false;
    }

    if (!BindExpansionText(
            Statement,
            1,
            WarId.ToString()))
    {
        OutError =
            LastError(TEXT("Bind War order list"));
        sqlite3_finalize(Statement);
        return false;
    }

    TArray<FOGEntityId> OrderIds;
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
                LastError(TEXT("Read War order list"));
            sqlite3_finalize(Statement);
            return false;
        }

        const FOGEntityId OrderId =
            ParseExpansionEntityId(
                ExpansionColumnText(
                    Statement,
                    0));
        if (!OrderId.IsValid())
        {
            OutError =
                TEXT("Stored War order list contains invalid ID.");
            sqlite3_finalize(Statement);
            return false;
        }

        OrderIds.Add(OrderId);
    }

    sqlite3_finalize(Statement);

    for (const FOGEntityId& OrderId :
         OrderIds)
    {
        bool bFound = false;
        FOGWarOrderRecord Order;
        if (!TryReadWarOrder(
                OrderId,
                bFound,
                Order,
                OutError) ||
            !bFound)
        {
            if (OutError.IsEmpty())
            {
                OutError =
                    TEXT("War order disappeared during list read.");
            }
            OutOrders.Reset();
            return false;
        }

        OutOrders.Add(
            MoveTemp(Order));
    }

    return true;
}

bool FOGSQLiteWorldStore::UpsertWarParticipantHistory(
    const FOGWarParticipantHistoryRecord& History,
    FString& OutError)
{
    OutError.Reset();

    if (!History.WarId.IsValid() ||
        !History.FactionId.IsValid() ||
        History.SideIndex < 0 ||
        History.JoinedWorldTick < 0 ||
        (History.bHasLeftWorldTick &&
         History.LeftWorldTick <
             History.JoinedWorldTick))
    {
        OutError =
            TEXT("War participant history is invalid.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "INSERT INTO war_participant_history("
        "war_entity_id, faction_entity_id, side_index, joined_world_tick, left_world_tick, reason"
        ") VALUES(?, ?, ?, ?, ?, ?) "
        "ON CONFLICT(war_entity_id, faction_entity_id, joined_world_tick) DO UPDATE SET "
        "side_index = excluded.side_index, left_world_tick = excluded.left_world_tick, reason = excluded.reason;";

    if (sqlite3_prepare_v2(
            Database,
            Sql,
            -1,
            &Statement,
            nullptr) != SQLITE_OK)
    {
        OutError =
            LastError(TEXT("Prepare War participant history upsert"));
        return false;
    }

    const bool bSucceeded =
        BindExpansionText(
            Statement,
            1,
            History.WarId.ToString()) &&
        BindExpansionText(
            Statement,
            2,
            History.FactionId.ToString()) &&
        sqlite3_bind_int(
            Statement,
            3,
            History.SideIndex) == SQLITE_OK &&
        sqlite3_bind_int64(
            Statement,
            4,
            History.JoinedWorldTick) == SQLITE_OK &&
        BindOptionalExpansionTick(
            Statement,
            5,
            History.bHasLeftWorldTick,
            History.LeftWorldTick) &&
        (History.Reason.IsNone()
            ? sqlite3_bind_null(
                Statement,
                6) == SQLITE_OK
            : BindExpansionText(
                Statement,
                6,
                History.Reason.ToString())) &&
        sqlite3_step(Statement) == SQLITE_DONE;

    if (!bSucceeded)
    {
        OutError =
            LastError(TEXT("Upsert War participant history"));
    }

    sqlite3_finalize(Statement);
    return bSucceeded;
}

bool FOGSQLiteWorldStore::ListWarParticipantHistory(
    const FOGEntityId& WarId,
    TArray<FOGWarParticipantHistoryRecord>& OutHistory,
    FString& OutError) const
{
    OutHistory.Reset();
    OutError.Reset();

    if (!WarId.IsValid())
    {
        OutError =
            TEXT("War participant history list requires a valid War ID.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "SELECT faction_entity_id, side_index, joined_world_tick, left_world_tick, reason "
        "FROM war_participant_history WHERE war_entity_id = ? "
        "ORDER BY joined_world_tick, faction_entity_id;";

    if (sqlite3_prepare_v2(
            Database,
            Sql,
            -1,
            &Statement,
            nullptr) != SQLITE_OK)
    {
        OutError =
            LastError(TEXT("Prepare War participant history list"));
        return false;
    }

    if (!BindExpansionText(
            Statement,
            1,
            WarId.ToString()))
    {
        OutError =
            LastError(TEXT("Bind War participant history list"));
        sqlite3_finalize(Statement);
        return false;
    }

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
                LastError(TEXT("Read War participant history list"));
            sqlite3_finalize(Statement);
            OutHistory.Reset();
            return false;
        }

        FOGWarParticipantHistoryRecord History;
        History.WarId =
            WarId;
        History.FactionId =
            ParseExpansionEntityId(
                ExpansionColumnText(
                    Statement,
                    0));
        History.SideIndex =
            sqlite3_column_int(
                Statement,
                1);
        History.JoinedWorldTick =
            sqlite3_column_int64(
                Statement,
                2);

        if (sqlite3_column_type(
                Statement,
                3) != SQLITE_NULL)
        {
            History.bHasLeftWorldTick =
                true;
            History.LeftWorldTick =
                sqlite3_column_int64(
                    Statement,
                    3);
        }

        const FString Reason =
            ExpansionColumnText(
                Statement,
                4);
        if (!Reason.IsEmpty())
        {
            History.Reason =
                FName(*Reason);
        }

        if (!History.FactionId.IsValid())
        {
            OutError =
                TEXT("Stored War participant history has invalid faction.");
            sqlite3_finalize(Statement);
            OutHistory.Reset();
            return false;
        }

        OutHistory.Add(
            MoveTemp(History));
    }

    sqlite3_finalize(Statement);
    return true;
}

bool FOGSQLiteWorldStore::UpsertProjectPhase(
    const FOGProjectPhaseRecord& Phase,
    FString& OutError)
{
    OutError.Reset();

    if (!Phase.ProjectId.IsValid() ||
        !Phase.PhaseId.IsValid() ||
        Phase.Sequence < 0 ||
        Phase.Status.IsNone() ||
        Phase.StartWorldTick < 0 ||
        Phase.ResolveWorldTick <
            Phase.StartWorldTick ||
        Phase.ProgressBps < 0 ||
        Phase.ProgressBps > 10000)
    {
        OutError =
            TEXT("Project phase is invalid.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "INSERT INTO project_phases("
        "project_entity_id, phase_content_id, sequence, status, start_world_tick, "
        "resolve_world_tick, progress_bps, payload_json"
        ") VALUES(?, ?, ?, ?, ?, ?, ?, ?) "
        "ON CONFLICT(project_entity_id, phase_content_id) DO UPDATE SET "
        "sequence = excluded.sequence, status = excluded.status, "
        "start_world_tick = excluded.start_world_tick, resolve_world_tick = excluded.resolve_world_tick, "
        "progress_bps = excluded.progress_bps, payload_json = excluded.payload_json;";

    if (sqlite3_prepare_v2(
            Database,
            Sql,
            -1,
            &Statement,
            nullptr) != SQLITE_OK)
    {
        OutError =
            LastError(TEXT("Prepare Project phase upsert"));
        return false;
    }

    const bool bSucceeded =
        BindExpansionText(
            Statement,
            1,
            Phase.ProjectId.ToString()) &&
        BindExpansionText(
            Statement,
            2,
            Phase.PhaseId.ToString()) &&
        sqlite3_bind_int(
            Statement,
            3,
            Phase.Sequence) == SQLITE_OK &&
        BindExpansionText(
            Statement,
            4,
            Phase.Status.ToString()) &&
        sqlite3_bind_int64(
            Statement,
            5,
            Phase.StartWorldTick) == SQLITE_OK &&
        sqlite3_bind_int64(
            Statement,
            6,
            Phase.ResolveWorldTick) == SQLITE_OK &&
        sqlite3_bind_int(
            Statement,
            7,
            Phase.ProgressBps) == SQLITE_OK &&
        BindExpansionText(
            Statement,
            8,
            Phase.PayloadJson.IsEmpty()
                ? TEXT("{}")
                : Phase.PayloadJson) &&
        sqlite3_step(Statement) == SQLITE_DONE;

    if (!bSucceeded)
    {
        OutError =
            LastError(TEXT("Upsert Project phase"));
    }

    sqlite3_finalize(Statement);
    return bSucceeded;
}

bool FOGSQLiteWorldStore::ListProjectPhases(
    const FOGEntityId& ProjectId,
    TArray<FOGProjectPhaseRecord>& OutPhases,
    FString& OutError) const
{
    OutPhases.Reset();
    OutError.Reset();

    if (!ProjectId.IsValid())
    {
        OutError =
            TEXT("Project phase list requires a valid Project ID.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "SELECT phase_content_id, sequence, status, start_world_tick, "
        "resolve_world_tick, progress_bps, payload_json "
        "FROM project_phases WHERE project_entity_id = ? "
        "ORDER BY sequence, phase_content_id;";

    if (sqlite3_prepare_v2(
            Database,
            Sql,
            -1,
            &Statement,
            nullptr) != SQLITE_OK)
    {
        OutError =
            LastError(TEXT("Prepare Project phase list"));
        return false;
    }

    if (!BindExpansionText(
            Statement,
            1,
            ProjectId.ToString()))
    {
        OutError =
            LastError(TEXT("Bind Project phase list"));
        sqlite3_finalize(Statement);
        return false;
    }

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
                LastError(TEXT("Read Project phase list"));
            sqlite3_finalize(Statement);
            OutPhases.Reset();
            return false;
        }

        FOGProjectPhaseRecord Phase;
        Phase.ProjectId =
            ProjectId;
        Phase.PhaseId =
            FOGContentId(
                ExpansionColumnText(
                    Statement,
                    0));
        Phase.Sequence =
            sqlite3_column_int(
                Statement,
                1);
        Phase.Status =
            FName(*ExpansionColumnText(
                Statement,
                2));
        Phase.StartWorldTick =
            sqlite3_column_int64(
                Statement,
                3);
        Phase.ResolveWorldTick =
            sqlite3_column_int64(
                Statement,
                4);
        Phase.ProgressBps =
            sqlite3_column_int(
                Statement,
                5);
        Phase.PayloadJson =
            ExpansionColumnText(
                Statement,
                6);

        if (!Phase.PhaseId.IsValid() ||
            Phase.Status.IsNone())
        {
            OutError =
                TEXT("Stored Project phase is invalid.");
            sqlite3_finalize(Statement);
            OutPhases.Reset();
            return false;
        }

        OutPhases.Add(
            MoveTemp(Phase));
    }

    sqlite3_finalize(Statement);
    return true;
}

bool FOGSQLiteWorldStore::UpsertProjectAssignment(
    const FOGProjectAssignmentRecord& Assignment,
    FString& OutError)
{
    OutError.Reset();

    if (!Assignment.ProjectId.IsValid() ||
        !Assignment.AssigneeEntityId.IsValid() ||
        !Assignment.RoleId.IsValid())
    {
        OutError =
            TEXT("Project assignment is invalid.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "INSERT INTO project_assignments("
        "project_entity_id, assignee_entity_id, role_content_id, state_json"
        ") VALUES(?, ?, ?, ?) "
        "ON CONFLICT(project_entity_id, assignee_entity_id, role_content_id) DO UPDATE SET "
        "state_json = excluded.state_json;";

    if (sqlite3_prepare_v2(
            Database,
            Sql,
            -1,
            &Statement,
            nullptr) != SQLITE_OK)
    {
        OutError =
            LastError(TEXT("Prepare Project assignment upsert"));
        return false;
    }

    const bool bSucceeded =
        BindExpansionText(
            Statement,
            1,
            Assignment.ProjectId.ToString()) &&
        BindExpansionText(
            Statement,
            2,
            Assignment.AssigneeEntityId.ToString()) &&
        BindExpansionText(
            Statement,
            3,
            Assignment.RoleId.ToString()) &&
        BindExpansionText(
            Statement,
            4,
            Assignment.StateJson.IsEmpty()
                ? TEXT("{}")
                : Assignment.StateJson) &&
        sqlite3_step(Statement) == SQLITE_DONE;

    if (!bSucceeded)
    {
        OutError =
            LastError(TEXT("Upsert Project assignment"));
    }

    sqlite3_finalize(Statement);
    return bSucceeded;
}

bool FOGSQLiteWorldStore::ListProjectAssignments(
    const FOGEntityId& ProjectId,
    TArray<FOGProjectAssignmentRecord>& OutAssignments,
    FString& OutError) const
{
    OutAssignments.Reset();
    OutError.Reset();

    if (!ProjectId.IsValid())
    {
        OutError =
            TEXT("Project assignment list requires a valid Project ID.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "SELECT assignee_entity_id, role_content_id, state_json "
        "FROM project_assignments WHERE project_entity_id = ? "
        "ORDER BY role_content_id, assignee_entity_id;";

    if (sqlite3_prepare_v2(
            Database,
            Sql,
            -1,
            &Statement,
            nullptr) != SQLITE_OK)
    {
        OutError =
            LastError(TEXT("Prepare Project assignment list"));
        return false;
    }

    if (!BindExpansionText(
            Statement,
            1,
            ProjectId.ToString()))
    {
        OutError =
            LastError(TEXT("Bind Project assignment list"));
        sqlite3_finalize(Statement);
        return false;
    }

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
                LastError(TEXT("Read Project assignment list"));
            sqlite3_finalize(Statement);
            OutAssignments.Reset();
            return false;
        }

        FOGProjectAssignmentRecord Assignment;
        Assignment.ProjectId =
            ProjectId;
        Assignment.AssigneeEntityId =
            ParseExpansionEntityId(
                ExpansionColumnText(
                    Statement,
                    0));
        Assignment.RoleId =
            FOGContentId(
                ExpansionColumnText(
                    Statement,
                    1));
        Assignment.StateJson =
            ExpansionColumnText(
                Statement,
                2);

        if (!Assignment.AssigneeEntityId.IsValid() ||
            !Assignment.RoleId.IsValid())
        {
            OutError =
                TEXT("Stored Project assignment is invalid.");
            sqlite3_finalize(Statement);
            OutAssignments.Reset();
            return false;
        }

        OutAssignments.Add(
            MoveTemp(Assignment));
    }

    sqlite3_finalize(Statement);
    return true;
}

bool FOGSQLiteWorldStore::UpsertCivilizationState(
    const FOGCivilizationStateRecord& Civilization,
    FString& OutError)
{
    OutError.Reset();

    if (!Civilization.CivilizationEntityId.IsValid() ||
        !Civilization.GenreProfileId.IsValid())
    {
        OutError =
            TEXT("Civilization state is invalid.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "INSERT INTO civilization_state("
        "civilization_entity_id, genre_profile_content_id, state_json"
        ") VALUES(?, ?, ?) "
        "ON CONFLICT(civilization_entity_id) DO UPDATE SET "
        "genre_profile_content_id = excluded.genre_profile_content_id, "
        "state_json = excluded.state_json;";

    if (sqlite3_prepare_v2(
            Database,
            Sql,
            -1,
            &Statement,
            nullptr) != SQLITE_OK)
    {
        OutError =
            LastError(TEXT("Prepare Civilization state upsert"));
        return false;
    }

    const bool bSucceeded =
        BindExpansionText(
            Statement,
            1,
            Civilization.CivilizationEntityId.ToString()) &&
        BindExpansionText(
            Statement,
            2,
            Civilization.GenreProfileId.ToString()) &&
        BindExpansionText(
            Statement,
            3,
            Civilization.StateJson.IsEmpty()
                ? TEXT("{}")
                : Civilization.StateJson) &&
        sqlite3_step(Statement) == SQLITE_DONE;

    if (!bSucceeded)
    {
        OutError =
            LastError(TEXT("Upsert Civilization state"));
    }

    sqlite3_finalize(Statement);
    return bSucceeded;
}

bool FOGSQLiteWorldStore::TryReadCivilizationState(
    const FOGEntityId& CivilizationEntityId,
    bool& bOutFound,
    FOGCivilizationStateRecord& OutCivilization,
    FString& OutError) const
{
    bOutFound = false;
    OutCivilization =
        FOGCivilizationStateRecord();
    OutError.Reset();

    if (!CivilizationEntityId.IsValid())
    {
        OutError =
            TEXT("Civilization lookup requires a valid entity ID.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "SELECT genre_profile_content_id, state_json "
        "FROM civilization_state WHERE civilization_entity_id = ?;";

    if (sqlite3_prepare_v2(
            Database,
            Sql,
            -1,
            &Statement,
            nullptr) != SQLITE_OK)
    {
        OutError =
            LastError(TEXT("Prepare Civilization state read"));
        return false;
    }

    if (!BindExpansionText(
            Statement,
            1,
            CivilizationEntityId.ToString()))
    {
        OutError =
            LastError(TEXT("Bind Civilization state read"));
        sqlite3_finalize(Statement);
        return false;
    }

    const int32 Step =
        sqlite3_step(Statement);
    if (Step == SQLITE_ROW)
    {
        bOutFound = true;
        OutCivilization.CivilizationEntityId =
            CivilizationEntityId;
        OutCivilization.GenreProfileId =
            FOGContentId(
                ExpansionColumnText(
                    Statement,
                    0));
        OutCivilization.StateJson =
            ExpansionColumnText(
                Statement,
                1);

        if (!OutCivilization.GenreProfileId.IsValid())
        {
            OutError =
                TEXT("Stored Civilization state has invalid genre profile.");
            sqlite3_finalize(Statement);
            return false;
        }
    }
    else if (Step != SQLITE_DONE)
    {
        OutError =
            LastError(TEXT("Read Civilization state"));
        sqlite3_finalize(Statement);
        return false;
    }

    sqlite3_finalize(Statement);
    return true;
}

bool FOGSQLiteWorldStore::UpsertCivilizationDimension(
    const FOGCivilizationDimensionRecord& Dimension,
    FString& OutError)
{
    OutError.Reset();

    if (!Dimension.CivilizationEntityId.IsValid() ||
        !Dimension.DimensionId.IsValid() ||
        Dimension.Grade.IsNone())
    {
        OutError =
            TEXT("Civilization dimension is invalid.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "INSERT INTO civilization_dimensions("
        "civilization_entity_id, dimension_content_id, grade, state_json"
        ") VALUES(?, ?, ?, ?) "
        "ON CONFLICT(civilization_entity_id, dimension_content_id) DO UPDATE SET "
        "grade = excluded.grade, state_json = excluded.state_json;";

    if (sqlite3_prepare_v2(
            Database,
            Sql,
            -1,
            &Statement,
            nullptr) != SQLITE_OK)
    {
        OutError =
            LastError(TEXT("Prepare Civilization dimension upsert"));
        return false;
    }

    const bool bSucceeded =
        BindExpansionText(
            Statement,
            1,
            Dimension.CivilizationEntityId.ToString()) &&
        BindExpansionText(
            Statement,
            2,
            Dimension.DimensionId.ToString()) &&
        BindExpansionText(
            Statement,
            3,
            Dimension.Grade.ToString()) &&
        BindExpansionText(
            Statement,
            4,
            Dimension.StateJson.IsEmpty()
                ? TEXT("{}")
                : Dimension.StateJson) &&
        sqlite3_step(Statement) == SQLITE_DONE;

    if (!bSucceeded)
    {
        OutError =
            LastError(TEXT("Upsert Civilization dimension"));
    }

    sqlite3_finalize(Statement);
    return bSucceeded;
}

bool FOGSQLiteWorldStore::ListCivilizationDimensions(
    const FOGEntityId& CivilizationEntityId,
    TArray<FOGCivilizationDimensionRecord>& OutDimensions,
    FString& OutError) const
{
    OutDimensions.Reset();
    OutError.Reset();

    if (!CivilizationEntityId.IsValid())
    {
        OutError =
            TEXT("Civilization dimension list requires a valid entity ID.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "SELECT dimension_content_id, grade, state_json "
        "FROM civilization_dimensions WHERE civilization_entity_id = ? "
        "ORDER BY dimension_content_id ASC;";

    if (sqlite3_prepare_v2(
            Database,
            Sql,
            -1,
            &Statement,
            nullptr) != SQLITE_OK)
    {
        OutError =
            LastError(TEXT("Prepare Civilization dimension list"));
        return false;
    }

    if (!BindExpansionText(
            Statement,
            1,
            CivilizationEntityId.ToString()))
    {
        OutError =
            LastError(TEXT("Bind Civilization dimension list"));
        sqlite3_finalize(Statement);
        return false;
    }

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
                LastError(TEXT("Read Civilization dimension list"));
            sqlite3_finalize(Statement);
            OutDimensions.Reset();
            return false;
        }

        FOGCivilizationDimensionRecord Dimension;
        Dimension.CivilizationEntityId =
            CivilizationEntityId;
        Dimension.DimensionId =
            FOGContentId(
                ExpansionColumnText(
                    Statement,
                    0));
        Dimension.Grade =
            FName(*ExpansionColumnText(
                Statement,
                1));
        Dimension.StateJson =
            ExpansionColumnText(
                Statement,
                2);

        if (!Dimension.DimensionId.IsValid() ||
            Dimension.Grade.IsNone())
        {
            OutError =
                TEXT("Stored Civilization dimension is invalid.");
            sqlite3_finalize(Statement);
            OutDimensions.Reset();
            return false;
        }

        OutDimensions.Add(
            MoveTemp(Dimension));
    }

    sqlite3_finalize(Statement);
    return true;
}

bool FOGSQLiteWorldStore::UpsertLogisticsRoute(
    const FOGLogisticsRouteRecord& Route,
    int64 CreatedWorldTick,
    FString& OutError)
{
    OutError.Reset();

    if (!Route.RouteId.IsValid() ||
        !Route.OwnerEntityId.IsValid() ||
        (!Route.OriginLocationId.IsValid() &&
         !Route.OriginRealityId.IsValid()) ||
        (!Route.DestinationLocationId.IsValid() &&
         !Route.DestinationRealityId.IsValid()) ||
        !Route.TransportCapabilityId.IsValid() ||
        Route.RiskBps < 0 ||
        Route.RiskBps > 10000 ||
        Route.Status.IsNone())
    {
        OutError =
            TEXT("Logistics route is invalid.");
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
            Route.RouteId,
            FName(TEXT("logistics_route")),
            CreatedWorldTick,
            TEXT("{}"),
            Error))
    {
        return Fail(Error);
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "INSERT INTO logistics_routes("
        "route_entity_id, owner_entity_id, origin_location_entity_id, origin_reality_entity_id, "
        "destination_location_entity_id, destination_reality_entity_id, "
        "transport_capability_content_id, capacity_sig, capacity_exp, risk_bps, status, state_json"
        ") VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?) "
        "ON CONFLICT(route_entity_id) DO UPDATE SET "
        "owner_entity_id = excluded.owner_entity_id, "
        "origin_location_entity_id = excluded.origin_location_entity_id, "
        "origin_reality_entity_id = excluded.origin_reality_entity_id, "
        "destination_location_entity_id = excluded.destination_location_entity_id, "
        "destination_reality_entity_id = excluded.destination_reality_entity_id, "
        "transport_capability_content_id = excluded.transport_capability_content_id, "
        "capacity_sig = excluded.capacity_sig, capacity_exp = excluded.capacity_exp, "
        "risk_bps = excluded.risk_bps, status = excluded.status, state_json = excluded.state_json;";

    if (sqlite3_prepare_v2(
            Database,
            Sql,
            -1,
            &Statement,
            nullptr) != SQLITE_OK)
    {
        return Fail(
            LastError(TEXT("Prepare Logistics route upsert")));
    }

    const bool bSucceeded =
        BindExpansionText(
            Statement,
            1,
            Route.RouteId.ToString()) &&
        BindExpansionText(
            Statement,
            2,
            Route.OwnerEntityId.ToString()) &&
        BindOptionalExpansionEntityId(
            Statement,
            3,
            Route.OriginLocationId) &&
        BindOptionalExpansionEntityId(
            Statement,
            4,
            Route.OriginRealityId) &&
        BindOptionalExpansionEntityId(
            Statement,
            5,
            Route.DestinationLocationId) &&
        BindOptionalExpansionEntityId(
            Statement,
            6,
            Route.DestinationRealityId) &&
        BindExpansionText(
            Statement,
            7,
            Route.TransportCapabilityId.ToString()) &&
        sqlite3_bind_int64(
            Statement,
            8,
            Route.Capacity.Significand) == SQLITE_OK &&
        sqlite3_bind_int(
            Statement,
            9,
            Route.Capacity.Exponent10) == SQLITE_OK &&
        sqlite3_bind_int(
            Statement,
            10,
            Route.RiskBps) == SQLITE_OK &&
        BindExpansionText(
            Statement,
            11,
            Route.Status.ToString()) &&
        BindExpansionText(
            Statement,
            12,
            Route.StateJson.IsEmpty()
                ? TEXT("{}")
                : Route.StateJson) &&
        sqlite3_step(Statement) == SQLITE_DONE;

    const FString SqlError =
        bSucceeded
            ? FString()
            : LastError(TEXT("Upsert Logistics route"));
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

bool FOGSQLiteWorldStore::TryReadLogisticsRoute(
    const FOGEntityId& RouteId,
    bool& bOutFound,
    FOGLogisticsRouteRecord& OutRoute,
    FString& OutError) const
{
    bOutFound = false;
    OutRoute =
        FOGLogisticsRouteRecord();
    OutError.Reset();

    if (!RouteId.IsValid())
    {
        OutError =
            TEXT("Logistics route lookup requires a valid ID.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "SELECT owner_entity_id, origin_location_entity_id, origin_reality_entity_id, "
        "destination_location_entity_id, destination_reality_entity_id, "
        "transport_capability_content_id, capacity_sig, capacity_exp, risk_bps, status, state_json "
        "FROM logistics_routes WHERE route_entity_id = ?;";

    if (sqlite3_prepare_v2(
            Database,
            Sql,
            -1,
            &Statement,
            nullptr) != SQLITE_OK)
    {
        OutError =
            LastError(TEXT("Prepare Logistics route read"));
        return false;
    }

    if (!BindExpansionText(
            Statement,
            1,
            RouteId.ToString()))
    {
        OutError =
            LastError(TEXT("Bind Logistics route read"));
        sqlite3_finalize(Statement);
        return false;
    }

    const int32 Step =
        sqlite3_step(Statement);
    if (Step == SQLITE_ROW)
    {
        bOutFound = true;
        OutRoute.RouteId =
            RouteId;
        OutRoute.OwnerEntityId =
            ParseExpansionEntityId(
                ExpansionColumnText(
                    Statement,
                    0));
        OutRoute.OriginLocationId =
            ParseExpansionEntityId(
                ExpansionColumnText(
                    Statement,
                    1));
        OutRoute.OriginRealityId =
            ParseExpansionEntityId(
                ExpansionColumnText(
                    Statement,
                    2));
        OutRoute.DestinationLocationId =
            ParseExpansionEntityId(
                ExpansionColumnText(
                    Statement,
                    3));
        OutRoute.DestinationRealityId =
            ParseExpansionEntityId(
                ExpansionColumnText(
                    Statement,
                    4));
        OutRoute.TransportCapabilityId =
            FOGContentId(
                ExpansionColumnText(
                    Statement,
                    5));
        OutRoute.Capacity =
            FOGLargeNumber(
                sqlite3_column_int64(
                    Statement,
                    6),
                sqlite3_column_int(
                    Statement,
                    7));
        OutRoute.RiskBps =
            sqlite3_column_int(
                Statement,
                8);
        OutRoute.Status =
            FName(*ExpansionColumnText(
                Statement,
                9));
        OutRoute.StateJson =
            ExpansionColumnText(
                Statement,
                10);

        if (!OutRoute.OwnerEntityId.IsValid() ||
            !OutRoute.TransportCapabilityId.IsValid() ||
            OutRoute.Status.IsNone())
        {
            OutError =
                TEXT("Stored Logistics route is invalid.");
            sqlite3_finalize(Statement);
            return false;
        }
    }
    else if (Step != SQLITE_DONE)
    {
        OutError =
            LastError(TEXT("Read Logistics route"));
        sqlite3_finalize(Statement);
        return false;
    }

    sqlite3_finalize(Statement);
    return true;
}

bool FOGSQLiteWorldStore::ListLogisticsRoutesByOwner(
    const FOGEntityId& OwnerEntityId,
    TArray<FOGLogisticsRouteRecord>& OutRoutes,
    FString& OutError) const
{
    OutRoutes.Reset();
    OutError.Reset();

    if (!OwnerEntityId.IsValid())
    {
        OutError =
            TEXT("Logistics route list requires a valid owner.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "SELECT route_entity_id FROM logistics_routes "
        "WHERE owner_entity_id = ? ORDER BY route_entity_id;";

    if (sqlite3_prepare_v2(
            Database,
            Sql,
            -1,
            &Statement,
            nullptr) != SQLITE_OK)
    {
        OutError =
            LastError(TEXT("Prepare Logistics route list"));
        return false;
    }

    if (!BindExpansionText(
            Statement,
            1,
            OwnerEntityId.ToString()))
    {
        OutError =
            LastError(TEXT("Bind Logistics route list"));
        sqlite3_finalize(Statement);
        return false;
    }

    TArray<FOGEntityId> RouteIds;
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
                LastError(TEXT("Read Logistics route list"));
            sqlite3_finalize(Statement);
            return false;
        }

        const FOGEntityId RouteId =
            ParseExpansionEntityId(
                ExpansionColumnText(
                    Statement,
                    0));
        if (!RouteId.IsValid())
        {
            OutError =
                TEXT("Stored Logistics route list contains invalid ID.");
            sqlite3_finalize(Statement);
            return false;
        }

        RouteIds.Add(RouteId);
    }

    sqlite3_finalize(Statement);

    for (const FOGEntityId& RouteId :
         RouteIds)
    {
        bool bFound = false;
        FOGLogisticsRouteRecord Route;
        if (!TryReadLogisticsRoute(
                RouteId,
                bFound,
                Route,
                OutError) ||
            !bFound)
        {
            if (OutError.IsEmpty())
            {
                OutError =
                    TEXT("Logistics route disappeared during list read.");
            }
            OutRoutes.Reset();
            return false;
        }

        OutRoutes.Add(
            MoveTemp(Route));
    }

    return true;
}

bool FOGSQLiteWorldStore::MigrateStrategy0012(
    FString& OutError)
{
    OutError.Reset();

    // Existing dispatch final states are explicit enough to project a legacy
    // outcome label. Objectives/constraints are not inferred from older fields.
    if (!ExecuteSql(
            TEXT(
                "UPDATE dispatches SET outcome_state = "
                "CASE status "
                "WHEN 2 THEN 'success' "
                "WHEN 3 THEN 'failure' "
                "WHEN 4 THEN 'cancelled' "
                "ELSE '' END "
                "WHERE outcome_state = '';"),
            OutError))
    {
        return false;
    }

    // The existing participant row and parent War start tick are authoritative
    // historical facts. Project them into the new history table without
    // inventing later leave reasons/times.
    return ExecuteSql(
        TEXT(
            "INSERT OR IGNORE INTO war_participant_history("
            "war_entity_id, faction_entity_id, side_index, joined_world_tick, left_world_tick, reason"
            ") SELECT wp.war_entity_id, wp.faction_entity_id, wp.side_index, "
            "w.start_world_tick, NULL, 'legacy_projected' "
            "FROM war_participants wp "
            "JOIN wars w ON w.war_entity_id = wp.war_entity_id;"),
        OutError);
}

bool FOGSQLiteWorldStore::ValidateStrategyMigration0012(
    FString& OutError) const
{
    OutError.Reset();

    int32 InvalidDispatches = 0;
    if (!ReadExpansionCount(
            Database,
            "SELECT COUNT(*) FROM dispatches "
            "WHERE risk_tolerance_bps < 0 OR risk_tolerance_bps > 10000 "
            "OR (delay_until_world_tick IS NOT NULL AND delay_until_world_tick < start_world_tick);",
            InvalidDispatches,
            OutError))
    {
        return false;
    }

    if (InvalidDispatches != 0)
    {
        OutError = FString::Printf(
            TEXT("Migration 0012 produced %d invalid Dispatch policy rows."),
            InvalidDispatches);
        return false;
    }

    int32 MissingWarHistory = 0;
    if (!ReadExpansionCount(
            Database,
            "SELECT COUNT(*) FROM war_participants wp "
            "WHERE NOT EXISTS ("
            "SELECT 1 FROM war_participant_history h "
            "WHERE h.war_entity_id = wp.war_entity_id "
            "AND h.faction_entity_id = wp.faction_entity_id);",
            MissingWarHistory,
            OutError))
    {
        return false;
    }

    if (MissingWarHistory != 0)
    {
        OutError = FString::Printf(
            TEXT("Migration 0012 failed to preserve %d War participant histories."),
            MissingWarHistory);
        return false;
    }

    int32 InvalidPhases = 0;
    if (!ReadExpansionCount(
            Database,
            "SELECT COUNT(*) FROM project_phases "
            "WHERE sequence < 0 OR resolve_world_tick < start_world_tick "
            "OR progress_bps < 0 OR progress_bps > 10000;",
            InvalidPhases,
            OutError))
    {
        return false;
    }

    if (InvalidPhases != 0)
    {
        OutError = FString::Printf(
            TEXT("Migration 0012 produced %d invalid Project phases."),
            InvalidPhases);
        return false;
    }

    int32 InvalidRoutes = 0;
    if (!ReadExpansionCount(
            Database,
            "SELECT COUNT(*) FROM logistics_routes "
            "WHERE risk_bps < 0 OR risk_bps > 10000 "
            "OR (origin_location_entity_id IS NULL AND origin_reality_entity_id IS NULL) "
            "OR (destination_location_entity_id IS NULL AND destination_reality_entity_id IS NULL);",
            InvalidRoutes,
            OutError))
    {
        return false;
    }

    if (InvalidRoutes != 0)
    {
        OutError = FString::Printf(
            TEXT("Migration 0012 produced %d invalid Logistics routes."),
            InvalidRoutes);
        return false;
    }

    return true;
}
