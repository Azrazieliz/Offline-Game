#include "Persistence/OGSQLiteWorldStore.h"

#include "sqlite/sqlite3.h"

namespace
{
bool BindTerritoryText(
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

FString TerritoryColumnText(
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

FOGEntityId ParseTerritoryEntityId(
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

bool BindOptionalTerritoryEntityId(
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

    return BindTerritoryText(
        Statement,
        Index,
        EntityId.ToString());
}
}

bool FOGSQLiteWorldStore::UpsertTerritory(
    const FOGTerritoryRecord& Territory,
    int64 CreatedWorldTick,
    FString& OutError)
{
    OutError.Reset();

    if (!Territory.TerritoryId.IsValid() ||
        !Territory.RootLocationId.IsValid() ||
        Territory.Population < 0 ||
        Territory.ControlState.IsNone() ||
        (Territory.bMainTerritory &&
         !Territory.RulerId.IsValid()))
    {
        OutError = TEXT("Territory record is invalid.");
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
                    OutError += FString::Printf(
                        TEXT(" | Rollback error: %s"),
                        *RollbackError);
                }
            }

            return false;
        };

    FString EntityError;
    if (!UpsertEntity(
            Territory.TerritoryId,
            TEXT("territory"),
            CreatedWorldTick,
            TEXT("{}"),
            EntityError))
    {
        return Fail(EntityError);
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "INSERT INTO territories("
        "territory_entity_id, ruler_entity_id, root_location_entity_id, "
        "is_main, population, control_state"
        ") VALUES(?, ?, ?, ?, ?, ?) "
        "ON CONFLICT(territory_entity_id) DO UPDATE SET "
        "ruler_entity_id = excluded.ruler_entity_id, "
        "root_location_entity_id = excluded.root_location_entity_id, "
        "is_main = excluded.is_main, "
        "population = excluded.population, "
        "control_state = excluded.control_state;";

    if (sqlite3_prepare_v2(
            Database,
            Sql,
            -1,
            &Statement,
            nullptr) != SQLITE_OK)
    {
        return Fail(
            LastError(
                TEXT("Prepare territory upsert")));
    }

    const bool bBound =
        BindTerritoryText(
            Statement,
            1,
            Territory.TerritoryId.ToString()) &&
        BindOptionalTerritoryEntityId(
            Statement,
            2,
            Territory.RulerId) &&
        BindTerritoryText(
            Statement,
            3,
            Territory.RootLocationId.ToString()) &&
        sqlite3_bind_int(
            Statement,
            4,
            Territory.bMainTerritory ? 1 : 0) ==
            SQLITE_OK &&
        sqlite3_bind_int64(
            Statement,
            5,
            Territory.Population) ==
            SQLITE_OK &&
        BindTerritoryText(
            Statement,
            6,
            Territory.ControlState.ToString());

    const bool bSucceeded =
        bBound &&
        sqlite3_step(Statement) ==
            SQLITE_DONE;

    const FString SqlError =
        bSucceeded
            ? FString()
            : LastError(
                TEXT("Upsert territory"));

    sqlite3_finalize(Statement);

    if (!bSucceeded)
    {
        return Fail(SqlError);
    }

    sqlite3_stmt* LocationStatement = nullptr;
    const char* LocationSql =
        "UPDATE locations SET territory_entity_id = ? "
        "WHERE location_entity_id = ?;";

    if (sqlite3_prepare_v2(
            Database,
            LocationSql,
            -1,
            &LocationStatement,
            nullptr) != SQLITE_OK)
    {
        return Fail(
            LastError(
                TEXT("Prepare root-location ownership update")));
    }

    const bool bLocationBound =
        BindTerritoryText(
            LocationStatement,
            1,
            Territory.TerritoryId.ToString()) &&
        BindTerritoryText(
            LocationStatement,
            2,
            Territory.RootLocationId.ToString());

    const bool bLocationUpdated =
        bLocationBound &&
        sqlite3_step(LocationStatement) ==
            SQLITE_DONE &&
        sqlite3_changes(Database) > 0;

    const FString LocationError =
        bLocationUpdated
            ? FString()
            : TEXT("Territory root location does not exist or could not be linked.");

    sqlite3_finalize(LocationStatement);

    if (!bLocationUpdated)
    {
        return Fail(LocationError);
    }

    FOGLocationTerritoryRecord RootRelation;
    RootRelation.LocationId =
        Territory.RootLocationId;
    RootRelation.TerritoryId =
        Territory.TerritoryId;
    RootRelation.RelationKind =
        FName(TEXT("contained"));
    RootRelation.CoverageBps = 10000;

    FString RelationError;
    if (!UpsertLocationTerritory(
            RootRelation,
            RelationError))
    {
        return Fail(RelationError);
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

bool FOGSQLiteWorldStore::TryReadTerritory(
    const FOGEntityId& TerritoryId,
    bool& bOutFound,
    FOGTerritoryRecord& OutTerritory,
    FString& OutError) const
{
    bOutFound = false;
    OutTerritory = FOGTerritoryRecord();
    OutError.Reset();

    if (!TerritoryId.IsValid())
    {
        OutError = TEXT("Territory ID is invalid.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "SELECT ruler_entity_id, root_location_entity_id, "
        "is_main, population, control_state "
        "FROM territories WHERE territory_entity_id = ?;";

    if (sqlite3_prepare_v2(
            Database,
            Sql,
            -1,
            &Statement,
            nullptr) != SQLITE_OK)
    {
        OutError =
            LastError(
                TEXT("Prepare territory read"));
        return false;
    }

    if (!BindTerritoryText(
            Statement,
            1,
            TerritoryId.ToString()))
    {
        OutError =
            LastError(
                TEXT("Bind territory read"));
        sqlite3_finalize(Statement);
        return false;
    }

    const int32 StepResult =
        sqlite3_step(Statement);

    if (StepResult == SQLITE_ROW)
    {
        const FOGEntityId RootLocation =
            ParseTerritoryEntityId(
                TerritoryColumnText(
                    Statement,
                    1));

        if (!RootLocation.IsValid())
        {
            OutError =
                TEXT("Stored territory references an invalid root location.");
            sqlite3_finalize(Statement);
            return false;
        }

        bOutFound = true;
        OutTerritory.TerritoryId =
            TerritoryId;
        OutTerritory.RulerId =
            ParseTerritoryEntityId(
                TerritoryColumnText(
                    Statement,
                    0));
        OutTerritory.RootLocationId =
            RootLocation;
        OutTerritory.bMainTerritory =
            sqlite3_column_int(
                Statement,
                2) != 0;
        OutTerritory.Population =
            sqlite3_column_int64(
                Statement,
                3);
        OutTerritory.ControlState =
            FName(
                *TerritoryColumnText(
                    Statement,
                    4));
    }
    else if (StepResult != SQLITE_DONE)
    {
        OutError =
            LastError(
                TEXT("Read territory"));
        sqlite3_finalize(Statement);
        return false;
    }

    sqlite3_finalize(Statement);
    return true;
}

bool FOGSQLiteWorldStore::UpsertDomainCore(
    const FOGDomainCoreRecord& Core,
    int64 CreatedWorldTick,
    FString& OutError)
{
    OutError.Reset();

    if (!Core.CoreId.IsValid() ||
        !Core.TerritoryId.IsValid() ||
        Core.CurrentDurability.GetSign() < 0 ||
        Core.MaxDurability.GetSign() < 0 ||
        FOGLargeNumber::Compare(
            Core.CurrentDurability,
            Core.MaxDurability) > 0)
    {
        OutError = TEXT("Domain Core record is invalid.");
        return false;
    }

    if (Core.Lifecycle == EOGDomainCoreLifecycle::Broken)
    {
        if (!Core.CurrentDurability.IsZero() ||
            Core.ControllerRulerId.IsValid())
        {
            OutError =
                TEXT("A broken Domain Core must have zero durability and no active controller.");
            return false;
        }
    }

    if (Core.Lifecycle == EOGDomainCoreLifecycle::Absorbed &&
        Core.ControllerRulerId.IsValid())
    {
        OutError =
            TEXT("An absorbed Domain Core cannot retain an active controller.");
        return false;
    }

    TSet<FOGContentId> AspectIds;
    for (const FOGDomainCoreAspect& Aspect :
         Core.Aspects)
    {
        if (!Aspect.AspectId.IsValid() ||
            Aspect.Grade < 0 ||
            AspectIds.Contains(
                Aspect.AspectId))
        {
            OutError =
                TEXT("Domain Core contains invalid or duplicate Aspects.");
            return false;
        }

        AspectIds.Add(
            Aspect.AspectId);
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
                    OutError += FString::Printf(
                        TEXT(" | Rollback error: %s"),
                        *RollbackError);
                }
            }

            return false;
        };

    FString EntityError;
    if (!UpsertEntity(
            Core.CoreId,
            TEXT("domain_core"),
            CreatedWorldTick,
            TEXT("{}"),
            EntityError))
    {
        return Fail(EntityError);
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "INSERT INTO domain_cores("
        "core_entity_id, territory_entity_id, controller_ruler_entity_id, lifecycle, "
        "durability_sig, durability_exp, max_durability_sig, max_durability_exp"
        ") VALUES(?, ?, ?, ?, ?, ?, ?, ?) "
        "ON CONFLICT(core_entity_id) DO UPDATE SET "
        "territory_entity_id = excluded.territory_entity_id, "
        "controller_ruler_entity_id = excluded.controller_ruler_entity_id, "
        "lifecycle = excluded.lifecycle, "
        "durability_sig = excluded.durability_sig, "
        "durability_exp = excluded.durability_exp, "
        "max_durability_sig = excluded.max_durability_sig, "
        "max_durability_exp = excluded.max_durability_exp;";

    if (sqlite3_prepare_v2(
            Database,
            Sql,
            -1,
            &Statement,
            nullptr) != SQLITE_OK)
    {
        return Fail(
            LastError(
                TEXT("Prepare Domain Core upsert")));
    }

    const bool bBound =
        BindTerritoryText(
            Statement,
            1,
            Core.CoreId.ToString()) &&
        BindTerritoryText(
            Statement,
            2,
            Core.TerritoryId.ToString()) &&
        BindOptionalTerritoryEntityId(
            Statement,
            3,
            Core.ControllerRulerId) &&
        sqlite3_bind_int(
            Statement,
            4,
            static_cast<int32>(
                Core.Lifecycle)) ==
            SQLITE_OK &&
        sqlite3_bind_int64(
            Statement,
            5,
            Core.CurrentDurability.Significand) ==
            SQLITE_OK &&
        sqlite3_bind_int(
            Statement,
            6,
            Core.CurrentDurability.Exponent10) ==
            SQLITE_OK &&
        sqlite3_bind_int64(
            Statement,
            7,
            Core.MaxDurability.Significand) ==
            SQLITE_OK &&
        sqlite3_bind_int(
            Statement,
            8,
            Core.MaxDurability.Exponent10) ==
            SQLITE_OK;

    const bool bSucceeded =
        bBound &&
        sqlite3_step(Statement) ==
            SQLITE_DONE;

    const FString SqlError =
        bSucceeded
            ? FString()
            : LastError(
                TEXT("Upsert Domain Core"));

    sqlite3_finalize(Statement);

    if (!bSucceeded)
    {
        return Fail(SqlError);
    }

    sqlite3_stmt* DeleteStatement = nullptr;
    if (sqlite3_prepare_v2(
            Database,
            "DELETE FROM domain_core_aspects WHERE core_entity_id = ?;",
            -1,
            &DeleteStatement,
            nullptr) != SQLITE_OK)
    {
        return Fail(
            LastError(
                TEXT("Prepare Domain Core Aspect reset")));
    }

    const bool bDeleteSucceeded =
        BindTerritoryText(
            DeleteStatement,
            1,
            Core.CoreId.ToString()) &&
        sqlite3_step(DeleteStatement) ==
            SQLITE_DONE;

    const FString DeleteError =
        bDeleteSucceeded
            ? FString()
            : LastError(
                TEXT("Reset Domain Core Aspects"));

    sqlite3_finalize(DeleteStatement);

    if (!bDeleteSucceeded)
    {
        return Fail(DeleteError);
    }

    for (const FOGDomainCoreAspect& Aspect :
         Core.Aspects)
    {
        sqlite3_stmt* AspectStatement =
            nullptr;

        const char* AspectSql =
            "INSERT INTO domain_core_aspects("
            "core_entity_id, aspect_content_id, grade"
            ") VALUES(?, ?, ?);";

        if (sqlite3_prepare_v2(
                Database,
                AspectSql,
                -1,
                &AspectStatement,
                nullptr) != SQLITE_OK)
        {
            return Fail(
                LastError(
                    TEXT("Prepare Domain Core Aspect insert")));
        }

        const bool bAspectBound =
            BindTerritoryText(
                AspectStatement,
                1,
                Core.CoreId.ToString()) &&
            BindTerritoryText(
                AspectStatement,
                2,
                Aspect.AspectId.ToString()) &&
            sqlite3_bind_int(
                AspectStatement,
                3,
                Aspect.Grade) ==
                SQLITE_OK;

        const bool bAspectSucceeded =
            bAspectBound &&
            sqlite3_step(AspectStatement) ==
                SQLITE_DONE;

        const FString AspectError =
            bAspectSucceeded
                ? FString()
                : LastError(
                    TEXT("Insert Domain Core Aspect"));

        sqlite3_finalize(
            AspectStatement);

        if (!bAspectSucceeded)
        {
            return Fail(AspectError);
        }

        // Migration 0009 promotes Aspects into normalized Concepts. Keep the
        // legacy projection populated for new Cores without overwriting richer
        // fusion/synthesis provenance that may already exist.
        sqlite3_stmt* ConceptStatement = nullptr;
        const char* ConceptSql =
            "INSERT OR IGNORE INTO domain_core_concepts("
            "core_entity_id, concept_content_id, grade, "
            "origin_source_core_entity_id, synthesis_rule_content_id, state_json"
            ") VALUES(?, ?, ?, ?, NULL, '{}');";

        if (sqlite3_prepare_v2(
                Database,
                ConceptSql,
                -1,
                &ConceptStatement,
                nullptr) != SQLITE_OK)
        {
            return Fail(
                LastError(
                    TEXT("Prepare Domain Core Concept projection")));
        }

        const bool bConceptBound =
            BindTerritoryText(
                ConceptStatement,
                1,
                Core.CoreId.ToString()) &&
            BindTerritoryText(
                ConceptStatement,
                2,
                Aspect.AspectId.ToString()) &&
            sqlite3_bind_int(
                ConceptStatement,
                3,
                Aspect.Grade) == SQLITE_OK &&
            BindTerritoryText(
                ConceptStatement,
                4,
                Core.CoreId.ToString());

        const bool bConceptSucceeded =
            bConceptBound &&
            sqlite3_step(ConceptStatement) ==
                SQLITE_DONE;
        const FString ConceptError =
            bConceptSucceeded
                ? FString()
                : LastError(
                    TEXT("Project Domain Core Concept"));

        sqlite3_finalize(
            ConceptStatement);

        if (!bConceptSucceeded)
        {
            return Fail(ConceptError);
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

bool FOGSQLiteWorldStore::TryReadDomainCore(
    const FOGEntityId& CoreId,
    bool& bOutFound,
    FOGDomainCoreRecord& OutCore,
    FString& OutError) const
{
    bOutFound = false;
    OutCore = FOGDomainCoreRecord();
    OutError.Reset();

    if (!CoreId.IsValid())
    {
        OutError =
            TEXT("Domain Core ID is invalid.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "SELECT territory_entity_id, controller_ruler_entity_id, lifecycle, "
        "durability_sig, durability_exp, max_durability_sig, max_durability_exp "
        "FROM domain_cores WHERE core_entity_id = ?;";

    if (sqlite3_prepare_v2(
            Database,
            Sql,
            -1,
            &Statement,
            nullptr) != SQLITE_OK)
    {
        OutError =
            LastError(
                TEXT("Prepare Domain Core read"));
        return false;
    }

    if (!BindTerritoryText(
            Statement,
            1,
            CoreId.ToString()))
    {
        OutError =
            LastError(
                TEXT("Bind Domain Core read"));
        sqlite3_finalize(Statement);
        return false;
    }

    const int32 StepResult =
        sqlite3_step(Statement);

    if (StepResult == SQLITE_ROW)
    {
        const FOGEntityId TerritoryId =
            ParseTerritoryEntityId(
                TerritoryColumnText(
                    Statement,
                    0));

        if (!TerritoryId.IsValid())
        {
            OutError =
                TEXT("Stored Domain Core references an invalid territory.");
            sqlite3_finalize(Statement);
            return false;
        }

        bOutFound = true;
        OutCore.CoreId = CoreId;
        OutCore.TerritoryId =
            TerritoryId;
        OutCore.ControllerRulerId =
            ParseTerritoryEntityId(
                TerritoryColumnText(
                    Statement,
                    1));
        OutCore.Lifecycle =
            static_cast<EOGDomainCoreLifecycle>(
                sqlite3_column_int(
                    Statement,
                    2));
        OutCore.CurrentDurability =
            FOGLargeNumber(
                sqlite3_column_int64(
                    Statement,
                    3),
                sqlite3_column_int(
                    Statement,
                    4));
        OutCore.MaxDurability =
            FOGLargeNumber(
                sqlite3_column_int64(
                    Statement,
                    5),
                sqlite3_column_int(
                    Statement,
                    6));
    }
    else if (StepResult != SQLITE_DONE)
    {
        OutError =
            LastError(
                TEXT("Read Domain Core"));
        sqlite3_finalize(Statement);
        return false;
    }

    sqlite3_finalize(Statement);

    if (!bOutFound)
    {
        return true;
    }

    sqlite3_stmt* AspectStatement =
        nullptr;

    const char* AspectSql =
        "SELECT aspect_content_id, grade "
        "FROM domain_core_aspects "
        "WHERE core_entity_id = ? "
        "ORDER BY aspect_content_id;";

    if (sqlite3_prepare_v2(
            Database,
            AspectSql,
            -1,
            &AspectStatement,
            nullptr) != SQLITE_OK)
    {
        OutError =
            LastError(
                TEXT("Prepare Domain Core Aspect read"));
        return false;
    }

    if (!BindTerritoryText(
            AspectStatement,
            1,
            CoreId.ToString()))
    {
        OutError =
            LastError(
                TEXT("Bind Domain Core Aspect read"));
        sqlite3_finalize(
            AspectStatement);
        return false;
    }

    while (true)
    {
        const int32 AspectStep =
            sqlite3_step(
                AspectStatement);

        if (AspectStep == SQLITE_ROW)
        {
            FOGDomainCoreAspect Aspect;
            Aspect.AspectId =
                FOGContentId(
                    TerritoryColumnText(
                        AspectStatement,
                        0));
            Aspect.Grade =
                sqlite3_column_int(
                    AspectStatement,
                    1);

            if (!Aspect.AspectId.IsValid())
            {
                OutError =
                    TEXT("Stored Domain Core Aspect ID is invalid.");
                sqlite3_finalize(
                    AspectStatement);
                return false;
            }

            OutCore.Aspects.Add(
                MoveTemp(Aspect));
            continue;
        }

        if (AspectStep == SQLITE_DONE)
        {
            break;
        }

        OutError =
            LastError(
                TEXT("Read Domain Core Aspects"));
        sqlite3_finalize(
            AspectStatement);
        return false;
    }

    sqlite3_finalize(AspectStatement);
    return true;
}

bool FOGSQLiteWorldStore::SetResourceBalance(
    const FOGEntityId& OwnerEntityId,
    const FOGContentId& ResourceId,
    int64 Amount,
    FString& OutError)
{
    OutError.Reset();

    if (!OwnerEntityId.IsValid() ||
        !ResourceId.IsValid() ||
        Amount < 0)
    {
        OutError =
            TEXT("Resource balance requires valid IDs and a non-negative amount.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "INSERT INTO resource_balances("
        "owner_entity_id, resource_content_id, amount"
        ") VALUES(?, ?, ?) "
        "ON CONFLICT(owner_entity_id, resource_content_id) "
        "DO UPDATE SET amount = excluded.amount;";

    if (sqlite3_prepare_v2(
            Database,
            Sql,
            -1,
            &Statement,
            nullptr) != SQLITE_OK)
    {
        OutError =
            LastError(
                TEXT("Prepare resource balance upsert"));
        return false;
    }

    const bool bBound =
        BindTerritoryText(
            Statement,
            1,
            OwnerEntityId.ToString()) &&
        BindTerritoryText(
            Statement,
            2,
            ResourceId.ToString()) &&
        sqlite3_bind_int64(
            Statement,
            3,
            Amount) ==
            SQLITE_OK;

    const bool bSucceeded =
        bBound &&
        sqlite3_step(Statement) ==
            SQLITE_DONE;

    if (!bSucceeded)
    {
        OutError =
            LastError(
                TEXT("Set resource balance"));
    }

    sqlite3_finalize(Statement);
    return bSucceeded;
}

bool FOGSQLiteWorldStore::TryReadResourceBalance(
    const FOGEntityId& OwnerEntityId,
    const FOGContentId& ResourceId,
    bool& bOutKnown,
    int64& OutAmount,
    FString& OutError) const
{
    bOutKnown = false;
    OutAmount = 0;
    OutError.Reset();

    if (!OwnerEntityId.IsValid() ||
        !ResourceId.IsValid())
    {
        OutError =
            TEXT("Resource balance lookup requires valid IDs.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "SELECT amount FROM resource_balances "
        "WHERE owner_entity_id = ? AND resource_content_id = ?;";

    if (sqlite3_prepare_v2(
            Database,
            Sql,
            -1,
            &Statement,
            nullptr) != SQLITE_OK)
    {
        OutError =
            LastError(
                TEXT("Prepare resource balance read"));
        return false;
    }

    const bool bBound =
        BindTerritoryText(
            Statement,
            1,
            OwnerEntityId.ToString()) &&
        BindTerritoryText(
            Statement,
            2,
            ResourceId.ToString());

    if (!bBound)
    {
        OutError =
            LastError(
                TEXT("Bind resource balance read"));
        sqlite3_finalize(Statement);
        return false;
    }

    const int32 StepResult =
        sqlite3_step(Statement);

    if (StepResult == SQLITE_ROW)
    {
        bOutKnown = true;
        OutAmount =
            sqlite3_column_int64(
                Statement,
                0);
    }
    else if (StepResult != SQLITE_DONE)
    {
        OutError =
            LastError(
                TEXT("Read resource balance"));
        sqlite3_finalize(Statement);
        return false;
    }

    sqlite3_finalize(Statement);
    return true;
}

bool FOGSQLiteWorldStore::UpsertProject(
    const FOGProjectRecord& Project,
    int64 CreatedWorldTick,
    FString& OutError)
{
    OutError.Reset();

    if (!Project.ProjectId.IsValid() ||
        !Project.OwnerEntityId.IsValid() ||
        !Project.LocationId.IsValid() ||
        !Project.ProjectTypeId.IsValid() ||
        Project.ResolveWorldTick <
            Project.StartWorldTick ||
        Project.ProgressBps < 0 ||
        Project.ProgressBps > 10000)
    {
        OutError = TEXT("Project record is invalid.");
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
            Project.ProjectId,
            TEXT("project"),
            CreatedWorldTick,
            TEXT("{}"),
            EntityError))
    {
        return Fail(EntityError);
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "INSERT INTO projects("
        "project_entity_id, owner_entity_id, location_entity_id, "
        "project_type_content_id, status, start_world_tick, resolve_world_tick, "
        "progress_bps, payload_json"
        ") VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?) "
        "ON CONFLICT(project_entity_id) DO UPDATE SET "
        "owner_entity_id = excluded.owner_entity_id, "
        "location_entity_id = excluded.location_entity_id, "
        "project_type_content_id = excluded.project_type_content_id, "
        "status = excluded.status, "
        "start_world_tick = excluded.start_world_tick, "
        "resolve_world_tick = excluded.resolve_world_tick, "
        "progress_bps = excluded.progress_bps, "
        "payload_json = excluded.payload_json;";

    if (sqlite3_prepare_v2(
            Database,
            Sql,
            -1,
            &Statement,
            nullptr) != SQLITE_OK)
    {
        return Fail(
            LastError(
                TEXT("Prepare project upsert")));
    }

    const bool bBound =
        BindTerritoryText(
            Statement,
            1,
            Project.ProjectId.ToString()) &&
        BindTerritoryText(
            Statement,
            2,
            Project.OwnerEntityId.ToString()) &&
        BindTerritoryText(
            Statement,
            3,
            Project.LocationId.ToString()) &&
        BindTerritoryText(
            Statement,
            4,
            Project.ProjectTypeId.ToString()) &&
        sqlite3_bind_int(
            Statement,
            5,
            static_cast<int32>(
                Project.Status)) ==
            SQLITE_OK &&
        sqlite3_bind_int64(
            Statement,
            6,
            Project.StartWorldTick) ==
            SQLITE_OK &&
        sqlite3_bind_int64(
            Statement,
            7,
            Project.ResolveWorldTick) ==
            SQLITE_OK &&
        sqlite3_bind_int(
            Statement,
            8,
            Project.ProgressBps) ==
            SQLITE_OK &&
        BindTerritoryText(
            Statement,
            9,
            Project.PayloadJson.IsEmpty()
                ? TEXT("{}")
                : Project.PayloadJson);

    const bool bSucceeded =
        bBound &&
        sqlite3_step(Statement) ==
            SQLITE_DONE;

    const FString SqlError =
        bSucceeded
            ? FString()
            : LastError(
                TEXT("Upsert project"));

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

bool FOGSQLiteWorldStore::TryReadProject(
    const FOGEntityId& ProjectId,
    bool& bOutFound,
    FOGProjectRecord& OutProject,
    FString& OutError) const
{
    bOutFound = false;
    OutProject = FOGProjectRecord();
    OutError.Reset();

    if (!ProjectId.IsValid())
    {
        OutError =
            TEXT("Project ID is invalid.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "SELECT owner_entity_id, location_entity_id, project_type_content_id, "
        "status, start_world_tick, resolve_world_tick, progress_bps, payload_json "
        "FROM projects WHERE project_entity_id = ?;";

    if (sqlite3_prepare_v2(
            Database,
            Sql,
            -1,
            &Statement,
            nullptr) != SQLITE_OK)
    {
        OutError =
            LastError(
                TEXT("Prepare project read"));
        return false;
    }

    if (!BindTerritoryText(
            Statement,
            1,
            ProjectId.ToString()))
    {
        OutError =
            LastError(
                TEXT("Bind project read"));
        sqlite3_finalize(Statement);
        return false;
    }

    const int32 StepResult =
        sqlite3_step(Statement);

    if (StepResult == SQLITE_ROW)
    {
        const FOGEntityId Owner =
            ParseTerritoryEntityId(
                TerritoryColumnText(
                    Statement,
                    0));

        const FOGEntityId Location =
            ParseTerritoryEntityId(
                TerritoryColumnText(
                    Statement,
                    1));

        FOGContentId ProjectType(
            TerritoryColumnText(
                Statement,
                2));

        if (!Owner.IsValid() ||
            !Location.IsValid() ||
            !ProjectType.IsValid())
        {
            OutError =
                TEXT("Stored project contains invalid IDs.");
            sqlite3_finalize(Statement);
            return false;
        }

        bOutFound = true;
        OutProject.ProjectId =
            ProjectId;
        OutProject.OwnerEntityId =
            Owner;
        OutProject.LocationId =
            Location;
        OutProject.ProjectTypeId =
            MoveTemp(ProjectType);
        OutProject.Status =
            static_cast<EOGProjectStatus>(
                sqlite3_column_int(
                    Statement,
                    3));
        OutProject.StartWorldTick =
            sqlite3_column_int64(
                Statement,
                4);
        OutProject.ResolveWorldTick =
            sqlite3_column_int64(
                Statement,
                5);
        OutProject.ProgressBps =
            sqlite3_column_int(
                Statement,
                6);
        OutProject.PayloadJson =
            TerritoryColumnText(
                Statement,
                7);
    }
    else if (StepResult != SQLITE_DONE)
    {
        OutError =
            LastError(
                TEXT("Read project"));
        sqlite3_finalize(Statement);
        return false;
    }

    sqlite3_finalize(Statement);
    return true;
}
