#include "Persistence/OGSQLiteWorldStore.h"

#include "sqlite/sqlite3.h"

namespace
{
bool BindProgressionText(
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

FString ProgressionColumnText(
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

FOGEntityId ParseProgressionEntityId(
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

bool BindOptionalProgressionEntityId(
    sqlite3_stmt* Statement,
    int32 Index,
    const FOGEntityId& EntityId)
{
    return EntityId.IsValid()
        ? BindProgressionText(
            Statement,
            Index,
            EntityId.ToString())
        : sqlite3_bind_null(
            Statement,
            Index) == SQLITE_OK;
}

bool BindOptionalProgressionContentId(
    sqlite3_stmt* Statement,
    int32 Index,
    const FOGContentId& ContentId)
{
    return ContentId.IsValid()
        ? BindProgressionText(
            Statement,
            Index,
            ContentId.ToString())
        : sqlite3_bind_null(
            Statement,
            Index) == SQLITE_OK;
}

bool BindOptionalProgressionTick(
    sqlite3_stmt* Statement,
    int32 Index,
    bool bHasValue,
    int64 Value)
{
    return bHasValue
        ? sqlite3_bind_int64(
            Statement,
            Index,
            Value) == SQLITE_OK
        : sqlite3_bind_null(
            Statement,
            Index) == SQLITE_OK;
}

bool IsProgressionLevelValid(int32 Level)
{
    return Level >= 1 &&
        Level <= 100;
}

bool IsBpsValid(int32 Value)
{
    return Value >= 0 &&
        Value <= 10000;
}

bool ReadProgressionCount(
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
            TEXT("Failed to prepare migration 0010 validation query.");
        return false;
    }

    const int32 Step =
        sqlite3_step(Statement);
    if (Step != SQLITE_ROW)
    {
        OutError =
            TEXT("Failed to execute migration 0010 validation query.");
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

bool FOGSQLiteWorldStore::UpsertEntityRankState(
    const FOGEntityRankStateRecord& State,
    FString& OutError)
{
    OutError.Reset();

    const bool bHasEffective =
        State.EffectiveRankId.IsValid();

    if (!State.EntityId.IsValid() ||
        !State.AttainedRankId.IsValid() ||
        !State.PeakRankId.IsValid() ||
        !IsProgressionLevelValid(
            State.AttainedLevel) ||
        !IsProgressionLevelValid(
            State.PeakLevel) ||
        (bHasEffective &&
         !IsProgressionLevelValid(
             State.EffectiveLevel)) ||
        (!bHasEffective &&
         State.EffectiveLevel != 0) ||
        State.UpdatedWorldTick < 0)
    {
        OutError =
            TEXT("Entity Rank state is invalid.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "INSERT INTO entity_rank_state("
        "entity_id, attained_rank_content_id, attained_level, "
        "effective_rank_content_id, effective_level, peak_rank_content_id, "
        "peak_level, updated_world_tick, state_json"
        ") VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?) "
        "ON CONFLICT(entity_id) DO UPDATE SET "
        "attained_rank_content_id = excluded.attained_rank_content_id, "
        "attained_level = excluded.attained_level, "
        "effective_rank_content_id = excluded.effective_rank_content_id, "
        "effective_level = excluded.effective_level, "
        "peak_rank_content_id = excluded.peak_rank_content_id, "
        "peak_level = excluded.peak_level, "
        "updated_world_tick = excluded.updated_world_tick, "
        "state_json = excluded.state_json;";

    if (sqlite3_prepare_v2(
            Database,
            Sql,
            -1,
            &Statement,
            nullptr) != SQLITE_OK)
    {
        OutError =
            LastError(TEXT("Prepare Rank-state upsert"));
        return false;
    }

    const bool bBound =
        BindProgressionText(
            Statement,
            1,
            State.EntityId.ToString()) &&
        BindProgressionText(
            Statement,
            2,
            State.AttainedRankId.ToString()) &&
        sqlite3_bind_int(
            Statement,
            3,
            State.AttainedLevel) == SQLITE_OK &&
        BindOptionalProgressionContentId(
            Statement,
            4,
            State.EffectiveRankId) &&
        (bHasEffective
            ? sqlite3_bind_int(
                Statement,
                5,
                State.EffectiveLevel) == SQLITE_OK
            : sqlite3_bind_null(
                Statement,
                5) == SQLITE_OK) &&
        BindProgressionText(
            Statement,
            6,
            State.PeakRankId.ToString()) &&
        sqlite3_bind_int(
            Statement,
            7,
            State.PeakLevel) == SQLITE_OK &&
        sqlite3_bind_int64(
            Statement,
            8,
            State.UpdatedWorldTick) == SQLITE_OK &&
        BindProgressionText(
            Statement,
            9,
            State.StateJson.IsEmpty()
                ? TEXT("{}")
                : State.StateJson);

    const bool bSucceeded =
        bBound &&
        sqlite3_step(Statement) ==
            SQLITE_DONE;

    if (!bSucceeded)
    {
        OutError =
            LastError(TEXT("Upsert Rank state"));
    }

    sqlite3_finalize(Statement);
    return bSucceeded;
}

bool FOGSQLiteWorldStore::TryReadEntityRankState(
    const FOGEntityId& EntityId,
    bool& bOutFound,
    FOGEntityRankStateRecord& OutState,
    FString& OutError) const
{
    bOutFound = false;
    OutState =
        FOGEntityRankStateRecord();
    OutError.Reset();

    if (!EntityId.IsValid())
    {
        OutError =
            TEXT("Rank-state lookup requires a valid entity ID.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "SELECT attained_rank_content_id, attained_level, "
        "effective_rank_content_id, effective_level, peak_rank_content_id, "
        "peak_level, updated_world_tick, state_json "
        "FROM entity_rank_state WHERE entity_id = ?;";

    if (sqlite3_prepare_v2(
            Database,
            Sql,
            -1,
            &Statement,
            nullptr) != SQLITE_OK)
    {
        OutError =
            LastError(TEXT("Prepare Rank-state read"));
        return false;
    }

    if (!BindProgressionText(
            Statement,
            1,
            EntityId.ToString()))
    {
        OutError =
            LastError(TEXT("Bind Rank-state read"));
        sqlite3_finalize(Statement);
        return false;
    }

    const int32 Step =
        sqlite3_step(Statement);
    if (Step == SQLITE_ROW)
    {
        bOutFound = true;
        OutState.EntityId =
            EntityId;
        OutState.AttainedRankId =
            FOGContentId(
                ProgressionColumnText(
                    Statement,
                    0));
        OutState.AttainedLevel =
            sqlite3_column_int(
                Statement,
                1);

        const FString EffectiveRank =
            ProgressionColumnText(
                Statement,
                2);
        if (!EffectiveRank.IsEmpty())
        {
            OutState.EffectiveRankId =
                FOGContentId(
                    EffectiveRank);
            OutState.EffectiveLevel =
                sqlite3_column_int(
                    Statement,
                    3);
        }

        OutState.PeakRankId =
            FOGContentId(
                ProgressionColumnText(
                    Statement,
                    4));
        OutState.PeakLevel =
            sqlite3_column_int(
                Statement,
                5);
        OutState.UpdatedWorldTick =
            sqlite3_column_int64(
                Statement,
                6);
        OutState.StateJson =
            ProgressionColumnText(
                Statement,
                7);

        if (!OutState.AttainedRankId.IsValid() ||
            !OutState.PeakRankId.IsValid() ||
            !IsProgressionLevelValid(
                OutState.AttainedLevel) ||
            !IsProgressionLevelValid(
                OutState.PeakLevel) ||
            (OutState.EffectiveRankId.IsValid() &&
             !IsProgressionLevelValid(
                 OutState.EffectiveLevel)))
        {
            OutError =
                TEXT("Stored Rank state is invalid.");
            sqlite3_finalize(Statement);
            return false;
        }
    }
    else if (Step != SQLITE_DONE)
    {
        OutError =
            LastError(TEXT("Read Rank state"));
        sqlite3_finalize(Statement);
        return false;
    }

    sqlite3_finalize(Statement);
    return true;
}

bool FOGSQLiteWorldStore::UpsertFactorInstance(
    const FOGFactorInstanceRecord& Factor,
    int64 CreatedWorldTick,
    FString& OutError)
{
    OutError.Reset();

    if (!Factor.FactorInstanceId.IsValid() ||
        !Factor.OwnerEntityId.IsValid() ||
        !Factor.FactorId.IsValid() ||
        Factor.AcquiredWorldTick < 0 ||
        !IsBpsValid(Factor.PurityBps) ||
        !IsBpsValid(Factor.MaturityBps) ||
        !IsBpsValid(Factor.CompletenessBps) ||
        !IsBpsValid(Factor.ExpressionWeightBps))
    {
        OutError =
            TEXT("Factor instance is invalid.");
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
            Factor.FactorInstanceId,
            TEXT("factor_instance"),
            CreatedWorldTick,
            TEXT("{}"),
            Error))
    {
        return Fail(Error);
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "INSERT INTO factor_instances("
        "factor_instance_id, owner_entity_id, factor_content_id, source_entity_id, "
        "acquired_world_tick, purity_bps, maturity_bps, completeness_bps, "
        "expression_weight_bps, state_json"
        ") VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?) "
        "ON CONFLICT(factor_instance_id) DO UPDATE SET "
        "owner_entity_id = excluded.owner_entity_id, "
        "factor_content_id = excluded.factor_content_id, "
        "source_entity_id = excluded.source_entity_id, "
        "acquired_world_tick = excluded.acquired_world_tick, "
        "purity_bps = excluded.purity_bps, "
        "maturity_bps = excluded.maturity_bps, "
        "completeness_bps = excluded.completeness_bps, "
        "expression_weight_bps = excluded.expression_weight_bps, "
        "state_json = excluded.state_json;";

    if (sqlite3_prepare_v2(
            Database,
            Sql,
            -1,
            &Statement,
            nullptr) != SQLITE_OK)
    {
        return Fail(
            LastError(TEXT("Prepare Factor-instance upsert")));
    }

    const bool bBound =
        BindProgressionText(
            Statement,
            1,
            Factor.FactorInstanceId.ToString()) &&
        BindProgressionText(
            Statement,
            2,
            Factor.OwnerEntityId.ToString()) &&
        BindProgressionText(
            Statement,
            3,
            Factor.FactorId.ToString()) &&
        BindOptionalProgressionEntityId(
            Statement,
            4,
            Factor.SourceEntityId) &&
        sqlite3_bind_int64(
            Statement,
            5,
            Factor.AcquiredWorldTick) == SQLITE_OK &&
        sqlite3_bind_int(
            Statement,
            6,
            Factor.PurityBps) == SQLITE_OK &&
        sqlite3_bind_int(
            Statement,
            7,
            Factor.MaturityBps) == SQLITE_OK &&
        sqlite3_bind_int(
            Statement,
            8,
            Factor.CompletenessBps) == SQLITE_OK &&
        sqlite3_bind_int(
            Statement,
            9,
            Factor.ExpressionWeightBps) == SQLITE_OK &&
        BindProgressionText(
            Statement,
            10,
            Factor.StateJson.IsEmpty()
                ? TEXT("{}")
                : Factor.StateJson);

    const bool bSucceeded =
        bBound &&
        sqlite3_step(Statement) ==
            SQLITE_DONE;
    const FString SqlError =
        bSucceeded
            ? FString()
            : LastError(TEXT("Upsert Factor instance"));
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

bool FOGSQLiteWorldStore::TryReadFactorInstance(
    const FOGEntityId& FactorInstanceId,
    bool& bOutFound,
    FOGFactorInstanceRecord& OutFactor,
    FString& OutError) const
{
    bOutFound = false;
    OutFactor =
        FOGFactorInstanceRecord();
    OutError.Reset();

    if (!FactorInstanceId.IsValid())
    {
        OutError =
            TEXT("Factor lookup requires a valid instance ID.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "SELECT owner_entity_id, factor_content_id, source_entity_id, "
        "acquired_world_tick, purity_bps, maturity_bps, completeness_bps, "
        "expression_weight_bps, state_json "
        "FROM factor_instances WHERE factor_instance_id = ?;";

    if (sqlite3_prepare_v2(
            Database,
            Sql,
            -1,
            &Statement,
            nullptr) != SQLITE_OK)
    {
        OutError =
            LastError(TEXT("Prepare Factor read"));
        return false;
    }

    if (!BindProgressionText(
            Statement,
            1,
            FactorInstanceId.ToString()))
    {
        OutError =
            LastError(TEXT("Bind Factor read"));
        sqlite3_finalize(Statement);
        return false;
    }

    const int32 Step =
        sqlite3_step(Statement);
    if (Step == SQLITE_ROW)
    {
        const FOGEntityId OwnerId =
            ParseProgressionEntityId(
                ProgressionColumnText(
                    Statement,
                    0));
        const FOGContentId FactorId(
            ProgressionColumnText(
                Statement,
                1));

        if (!OwnerId.IsValid() ||
            !FactorId.IsValid())
        {
            OutError =
                TEXT("Stored Factor contains invalid owner/content IDs.");
            sqlite3_finalize(Statement);
            return false;
        }

        bOutFound = true;
        OutFactor.FactorInstanceId =
            FactorInstanceId;
        OutFactor.OwnerEntityId =
            OwnerId;
        OutFactor.FactorId =
            FactorId;
        OutFactor.SourceEntityId =
            ParseProgressionEntityId(
                ProgressionColumnText(
                    Statement,
                    2));
        OutFactor.AcquiredWorldTick =
            sqlite3_column_int64(
                Statement,
                3);
        OutFactor.PurityBps =
            sqlite3_column_int(
                Statement,
                4);
        OutFactor.MaturityBps =
            sqlite3_column_int(
                Statement,
                5);
        OutFactor.CompletenessBps =
            sqlite3_column_int(
                Statement,
                6);
        OutFactor.ExpressionWeightBps =
            sqlite3_column_int(
                Statement,
                7);
        OutFactor.StateJson =
            ProgressionColumnText(
                Statement,
                8);
    }
    else if (Step != SQLITE_DONE)
    {
        OutError =
            LastError(TEXT("Read Factor"));
        sqlite3_finalize(Statement);
        return false;
    }

    sqlite3_finalize(Statement);
    return true;
}

bool FOGSQLiteWorldStore::ListFactorInstancesByOwner(
    const FOGEntityId& OwnerEntityId,
    TArray<FOGFactorInstanceRecord>& OutFactors,
    FString& OutError) const
{
    OutFactors.Reset();
    OutError.Reset();

    if (!OwnerEntityId.IsValid())
    {
        OutError =
            TEXT("Factor list requires a valid owner.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "SELECT factor_instance_id FROM factor_instances "
        "WHERE owner_entity_id = ? "
        "ORDER BY acquired_world_tick ASC, factor_instance_id ASC;";

    if (sqlite3_prepare_v2(
            Database,
            Sql,
            -1,
            &Statement,
            nullptr) != SQLITE_OK)
    {
        OutError =
            LastError(TEXT("Prepare Factor list"));
        return false;
    }

    if (!BindProgressionText(
            Statement,
            1,
            OwnerEntityId.ToString()))
    {
        OutError =
            LastError(TEXT("Bind Factor list"));
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
                LastError(TEXT("Read Factor list"));
            sqlite3_finalize(Statement);
            return false;
        }

        const FOGEntityId Id =
            ParseProgressionEntityId(
                ProgressionColumnText(
                    Statement,
                    0));
        if (!Id.IsValid())
        {
            OutError =
                TEXT("Stored Factor list contains an invalid instance ID.");
            sqlite3_finalize(Statement);
            return false;
        }

        Ids.Add(Id);
    }

    sqlite3_finalize(Statement);

    for (const FOGEntityId& Id : Ids)
    {
        bool bFound = false;
        FOGFactorInstanceRecord Factor;
        if (!TryReadFactorInstance(
                Id,
                bFound,
                Factor,
                OutError) ||
            !bFound)
        {
            if (OutError.IsEmpty())
            {
                OutError =
                    TEXT("Factor disappeared during list read.");
            }
            OutFactors.Reset();
            return false;
        }
        OutFactors.Add(MoveTemp(Factor));
    }

    return true;
}

bool FOGSQLiteWorldStore::UpsertFactorLineage(
    const FOGFactorLineageRecord& Lineage,
    FString& OutError)
{
    OutError.Reset();

    if (!Lineage.ChildFactorInstanceId.IsValid() ||
        !Lineage.ParentFactorInstanceId.IsValid() ||
        Lineage.ChildFactorInstanceId ==
            Lineage.ParentFactorInstanceId ||
        Lineage.RelationKind.IsNone() ||
        Lineage.Ordinal < 0)
    {
        OutError =
            TEXT("Factor lineage record is invalid.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "INSERT INTO factor_lineage("
        "child_factor_instance_id, parent_factor_instance_id, relation_kind, ordinal"
        ") VALUES(?, ?, ?, ?) "
        "ON CONFLICT(child_factor_instance_id, parent_factor_instance_id) DO UPDATE SET "
        "relation_kind = excluded.relation_kind, ordinal = excluded.ordinal;";

    if (sqlite3_prepare_v2(
            Database,
            Sql,
            -1,
            &Statement,
            nullptr) != SQLITE_OK)
    {
        OutError =
            LastError(TEXT("Prepare Factor-lineage upsert"));
        return false;
    }

    const bool bSucceeded =
        BindProgressionText(
            Statement,
            1,
            Lineage.ChildFactorInstanceId.ToString()) &&
        BindProgressionText(
            Statement,
            2,
            Lineage.ParentFactorInstanceId.ToString()) &&
        BindProgressionText(
            Statement,
            3,
            Lineage.RelationKind.ToString()) &&
        sqlite3_bind_int(
            Statement,
            4,
            Lineage.Ordinal) == SQLITE_OK &&
        sqlite3_step(Statement) ==
            SQLITE_DONE;

    if (!bSucceeded)
    {
        OutError =
            LastError(TEXT("Upsert Factor lineage"));
    }

    sqlite3_finalize(Statement);
    return bSucceeded;
}

bool FOGSQLiteWorldStore::ListFactorLineageForChild(
    const FOGEntityId& ChildFactorInstanceId,
    TArray<FOGFactorLineageRecord>& OutLineage,
    FString& OutError) const
{
    OutLineage.Reset();
    OutError.Reset();

    if (!ChildFactorInstanceId.IsValid())
    {
        OutError =
            TEXT("Factor-lineage list requires a valid child instance.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "SELECT parent_factor_instance_id, relation_kind, ordinal "
        "FROM factor_lineage WHERE child_factor_instance_id = ? "
        "ORDER BY ordinal ASC, parent_factor_instance_id ASC;";

    if (sqlite3_prepare_v2(
            Database,
            Sql,
            -1,
            &Statement,
            nullptr) != SQLITE_OK)
    {
        OutError =
            LastError(TEXT("Prepare Factor-lineage list"));
        return false;
    }

    if (!BindProgressionText(
            Statement,
            1,
            ChildFactorInstanceId.ToString()))
    {
        OutError =
            LastError(TEXT("Bind Factor-lineage list"));
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
                LastError(TEXT("Read Factor-lineage list"));
            sqlite3_finalize(Statement);
            return false;
        }

        FOGFactorLineageRecord Lineage;
        Lineage.ChildFactorInstanceId =
            ChildFactorInstanceId;
        Lineage.ParentFactorInstanceId =
            ParseProgressionEntityId(
                ProgressionColumnText(
                    Statement,
                    0));
        Lineage.RelationKind =
            FName(*ProgressionColumnText(
                Statement,
                1));
        Lineage.Ordinal =
            sqlite3_column_int(
                Statement,
                2);

        if (!Lineage.ParentFactorInstanceId.IsValid() ||
            Lineage.RelationKind.IsNone())
        {
            OutError =
                TEXT("Stored Factor lineage is invalid.");
            sqlite3_finalize(Statement);
            return false;
        }

        OutLineage.Add(MoveTemp(Lineage));
    }

    sqlite3_finalize(Statement);
    return true;
}

bool FOGSQLiteWorldStore::UpsertEntityClass(
    const FOGEntityClassRecord& ClassState,
    FString& OutError)
{
    OutError.Reset();

    if (!ClassState.OwnerEntityId.IsValid() ||
        !ClassState.ClassId.IsValid() ||
        ClassState.AttainedTier.IsNone() ||
        ClassState.CurrentExpressionState.IsNone() ||
        ClassState.RecognizedWorldTick < 0 ||
        ClassState.UpdatedWorldTick <
            ClassState.RecognizedWorldTick)
    {
        OutError =
            TEXT("Entity Class state is invalid.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "INSERT INTO entity_classes("
        "owner_entity_id, class_content_id, attained_tier, "
        "current_expression_state, recognized_world_tick, updated_world_tick, state_json"
        ") VALUES(?, ?, ?, ?, ?, ?, ?) "
        "ON CONFLICT(owner_entity_id, class_content_id) DO UPDATE SET "
        "attained_tier = excluded.attained_tier, "
        "current_expression_state = excluded.current_expression_state, "
        "recognized_world_tick = entity_classes.recognized_world_tick, "
        "updated_world_tick = excluded.updated_world_tick, "
        "state_json = excluded.state_json;";

    if (sqlite3_prepare_v2(
            Database,
            Sql,
            -1,
            &Statement,
            nullptr) != SQLITE_OK)
    {
        OutError =
            LastError(TEXT("Prepare Class-state upsert"));
        return false;
    }

    const bool bSucceeded =
        BindProgressionText(
            Statement,
            1,
            ClassState.OwnerEntityId.ToString()) &&
        BindProgressionText(
            Statement,
            2,
            ClassState.ClassId.ToString()) &&
        BindProgressionText(
            Statement,
            3,
            ClassState.AttainedTier.ToString()) &&
        BindProgressionText(
            Statement,
            4,
            ClassState.CurrentExpressionState.ToString()) &&
        sqlite3_bind_int64(
            Statement,
            5,
            ClassState.RecognizedWorldTick) == SQLITE_OK &&
        sqlite3_bind_int64(
            Statement,
            6,
            ClassState.UpdatedWorldTick) == SQLITE_OK &&
        BindProgressionText(
            Statement,
            7,
            ClassState.StateJson.IsEmpty()
                ? TEXT("{}")
                : ClassState.StateJson) &&
        sqlite3_step(Statement) ==
            SQLITE_DONE;

    if (!bSucceeded)
    {
        OutError =
            LastError(TEXT("Upsert Class state"));
    }

    sqlite3_finalize(Statement);
    return bSucceeded;
}

bool FOGSQLiteWorldStore::TryReadEntityClass(
    const FOGEntityId& OwnerEntityId,
    const FOGContentId& ClassId,
    bool& bOutFound,
    FOGEntityClassRecord& OutClass,
    FString& OutError) const
{
    bOutFound = false;
    OutClass =
        FOGEntityClassRecord();
    OutError.Reset();

    if (!OwnerEntityId.IsValid() ||
        !ClassId.IsValid())
    {
        OutError =
            TEXT("Class-state lookup requires valid owner/Class IDs.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "SELECT attained_tier, current_expression_state, "
        "recognized_world_tick, updated_world_tick, state_json "
        "FROM entity_classes WHERE owner_entity_id = ? AND class_content_id = ?;";

    if (sqlite3_prepare_v2(
            Database,
            Sql,
            -1,
            &Statement,
            nullptr) != SQLITE_OK)
    {
        OutError =
            LastError(TEXT("Prepare Class-state read"));
        return false;
    }

    const bool bBound =
        BindProgressionText(
            Statement,
            1,
            OwnerEntityId.ToString()) &&
        BindProgressionText(
            Statement,
            2,
            ClassId.ToString());

    if (!bBound)
    {
        OutError =
            LastError(TEXT("Bind Class-state read"));
        sqlite3_finalize(Statement);
        return false;
    }

    const int32 Step =
        sqlite3_step(Statement);
    if (Step == SQLITE_ROW)
    {
        bOutFound = true;
        OutClass.OwnerEntityId =
            OwnerEntityId;
        OutClass.ClassId =
            ClassId;
        OutClass.AttainedTier =
            FName(*ProgressionColumnText(
                Statement,
                0));
        OutClass.CurrentExpressionState =
            FName(*ProgressionColumnText(
                Statement,
                1));
        OutClass.RecognizedWorldTick =
            sqlite3_column_int64(
                Statement,
                2);
        OutClass.UpdatedWorldTick =
            sqlite3_column_int64(
                Statement,
                3);
        OutClass.StateJson =
            ProgressionColumnText(
                Statement,
                4);
    }
    else if (Step != SQLITE_DONE)
    {
        OutError =
            LastError(TEXT("Read Class state"));
        sqlite3_finalize(Statement);
        return false;
    }

    sqlite3_finalize(Statement);
    return true;
}

bool FOGSQLiteWorldStore::ListEntityClasses(
    const FOGEntityId& OwnerEntityId,
    TArray<FOGEntityClassRecord>& OutClasses,
    FString& OutError) const
{
    OutClasses.Reset();
    OutError.Reset();

    if (!OwnerEntityId.IsValid())
    {
        OutError =
            TEXT("Class list requires a valid owner.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "SELECT class_content_id FROM entity_classes "
        "WHERE owner_entity_id = ? ORDER BY class_content_id ASC;";

    if (sqlite3_prepare_v2(
            Database,
            Sql,
            -1,
            &Statement,
            nullptr) != SQLITE_OK)
    {
        OutError =
            LastError(TEXT("Prepare Class list"));
        return false;
    }

    if (!BindProgressionText(
            Statement,
            1,
            OwnerEntityId.ToString()))
    {
        OutError =
            LastError(TEXT("Bind Class list"));
        sqlite3_finalize(Statement);
        return false;
    }

    TArray<FOGContentId> Ids;
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
                LastError(TEXT("Read Class list"));
            sqlite3_finalize(Statement);
            return false;
        }

        FOGContentId ClassId(
            ProgressionColumnText(
                Statement,
                0));
        if (!ClassId.IsValid())
        {
            OutError =
                TEXT("Stored Class list contains an invalid Class ID.");
            sqlite3_finalize(Statement);
            return false;
        }
        Ids.Add(MoveTemp(ClassId));
    }

    sqlite3_finalize(Statement);

    for (const FOGContentId& ClassId : Ids)
    {
        bool bFound = false;
        FOGEntityClassRecord ClassState;
        if (!TryReadEntityClass(
                OwnerEntityId,
                ClassId,
                bFound,
                ClassState,
                OutError) ||
            !bFound)
        {
            if (OutError.IsEmpty())
            {
                OutError =
                    TEXT("Class disappeared during list read.");
            }
            OutClasses.Reset();
            return false;
        }
        OutClasses.Add(MoveTemp(ClassState));
    }

    return true;
}

bool FOGSQLiteWorldStore::UpsertGrandClassSeat(
    const FOGGrandClassSeatRecord& Seat,
    FString& OutError)
{
    OutError.Reset();

    if (!Seat.ClassId.IsValid() ||
        !Seat.BearerEntityId.IsValid() ||
        Seat.AppointedWorldTick < 0 ||
        Seat.SeatState.IsNone())
    {
        OutError =
            TEXT("Grand Class seat is invalid.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "INSERT INTO grand_class_seats("
        "class_content_id, bearer_entity_id, appointed_world_tick, "
        "seat_state, mandate_state_json"
        ") VALUES(?, ?, ?, ?, ?) "
        "ON CONFLICT(class_content_id) DO UPDATE SET "
        "appointed_world_tick = grand_class_seats.appointed_world_tick, "
        "seat_state = excluded.seat_state, "
        "mandate_state_json = excluded.mandate_state_json "
        "WHERE grand_class_seats.bearer_entity_id = excluded.bearer_entity_id;";

    if (sqlite3_prepare_v2(
            Database,
            Sql,
            -1,
            &Statement,
            nullptr) != SQLITE_OK)
    {
        OutError =
            LastError(TEXT("Prepare Grand-seat upsert"));
        return false;
    }

    const bool bStepped =
        BindProgressionText(
            Statement,
            1,
            Seat.ClassId.ToString()) &&
        BindProgressionText(
            Statement,
            2,
            Seat.BearerEntityId.ToString()) &&
        sqlite3_bind_int64(
            Statement,
            3,
            Seat.AppointedWorldTick) == SQLITE_OK &&
        BindProgressionText(
            Statement,
            4,
            Seat.SeatState.ToString()) &&
        BindProgressionText(
            Statement,
            5,
            Seat.MandateStateJson.IsEmpty()
                ? TEXT("{}")
                : Seat.MandateStateJson) &&
        sqlite3_step(Statement) ==
            SQLITE_DONE;

    const bool bChanged =
        bStepped &&
        sqlite3_changes(Database) == 1;

    if (!bChanged)
    {
        OutError =
            bStepped
                ? TEXT("Grand Class seat is occupied by another bearer; explicit vacancy is required.")
                : LastError(TEXT("Upsert Grand Class seat"));
    }

    sqlite3_finalize(Statement);
    return bChanged;
}

bool FOGSQLiteWorldStore::TryReadGrandClassSeat(
    const FOGContentId& ClassId,
    bool& bOutFound,
    FOGGrandClassSeatRecord& OutSeat,
    FString& OutError) const
{
    bOutFound = false;
    OutSeat =
        FOGGrandClassSeatRecord();
    OutError.Reset();

    if (!ClassId.IsValid())
    {
        OutError =
            TEXT("Grand-seat lookup requires a valid Class ID.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "SELECT bearer_entity_id, appointed_world_tick, seat_state, mandate_state_json "
        "FROM grand_class_seats WHERE class_content_id = ?;";

    if (sqlite3_prepare_v2(
            Database,
            Sql,
            -1,
            &Statement,
            nullptr) != SQLITE_OK)
    {
        OutError =
            LastError(TEXT("Prepare Grand-seat read"));
        return false;
    }

    if (!BindProgressionText(
            Statement,
            1,
            ClassId.ToString()))
    {
        OutError =
            LastError(TEXT("Bind Grand-seat read"));
        sqlite3_finalize(Statement);
        return false;
    }

    const int32 Step =
        sqlite3_step(Statement);
    if (Step == SQLITE_ROW)
    {
        const FOGEntityId Bearer =
            ParseProgressionEntityId(
                ProgressionColumnText(
                    Statement,
                    0));
        if (!Bearer.IsValid())
        {
            OutError =
                TEXT("Stored Grand seat has an invalid bearer.");
            sqlite3_finalize(Statement);
            return false;
        }

        bOutFound = true;
        OutSeat.ClassId =
            ClassId;
        OutSeat.BearerEntityId =
            Bearer;
        OutSeat.AppointedWorldTick =
            sqlite3_column_int64(
                Statement,
                1);
        OutSeat.SeatState =
            FName(*ProgressionColumnText(
                Statement,
                2));
        OutSeat.MandateStateJson =
            ProgressionColumnText(
                Statement,
                3);
    }
    else if (Step != SQLITE_DONE)
    {
        OutError =
            LastError(TEXT("Read Grand seat"));
        sqlite3_finalize(Statement);
        return false;
    }

    sqlite3_finalize(Statement);
    return true;
}

bool FOGSQLiteWorldStore::DeleteGrandClassSeat(
    const FOGContentId& ClassId,
    FString& OutError)
{
    OutError.Reset();

    if (!ClassId.IsValid())
    {
        OutError =
            TEXT("Grand-seat vacancy requires a valid Class ID.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    if (sqlite3_prepare_v2(
            Database,
            "DELETE FROM grand_class_seats WHERE class_content_id = ?;",
            -1,
            &Statement,
            nullptr) != SQLITE_OK)
    {
        OutError =
            LastError(TEXT("Prepare Grand-seat vacancy"));
        return false;
    }

    const bool bSucceeded =
        BindProgressionText(
            Statement,
            1,
            ClassId.ToString()) &&
        sqlite3_step(Statement) ==
            SQLITE_DONE;

    if (!bSucceeded)
    {
        OutError =
            LastError(TEXT("Vacate Grand Class seat"));
    }

    sqlite3_finalize(Statement);
    return bSucceeded;
}

bool FOGSQLiteWorldStore::UpsertEntitySkill(
    const FOGEntitySkillRecord& Skill,
    FString& OutError)
{
    OutError.Reset();

    if (!Skill.OwnerEntityId.IsValid() ||
        !Skill.SkillId.IsValid() ||
        Skill.LearnedWorldTick < 0 ||
        Skill.CurrentState.IsNone())
    {
        OutError =
            TEXT("Entity skill state is invalid.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "INSERT INTO entity_skills("
        "owner_entity_id, skill_content_id, learned_world_tick, "
        "current_state, development_state_json"
        ") VALUES(?, ?, ?, ?, ?) "
        "ON CONFLICT(owner_entity_id, skill_content_id) DO UPDATE SET "
        "learned_world_tick = entity_skills.learned_world_tick, "
        "current_state = excluded.current_state, "
        "development_state_json = excluded.development_state_json;";

    if (sqlite3_prepare_v2(
            Database,
            Sql,
            -1,
            &Statement,
            nullptr) != SQLITE_OK)
    {
        OutError =
            LastError(TEXT("Prepare skill upsert"));
        return false;
    }

    const bool bSucceeded =
        BindProgressionText(
            Statement,
            1,
            Skill.OwnerEntityId.ToString()) &&
        BindProgressionText(
            Statement,
            2,
            Skill.SkillId.ToString()) &&
        sqlite3_bind_int64(
            Statement,
            3,
            Skill.LearnedWorldTick) == SQLITE_OK &&
        BindProgressionText(
            Statement,
            4,
            Skill.CurrentState.ToString()) &&
        BindProgressionText(
            Statement,
            5,
            Skill.DevelopmentStateJson.IsEmpty()
                ? TEXT("{}")
                : Skill.DevelopmentStateJson) &&
        sqlite3_step(Statement) ==
            SQLITE_DONE;

    if (!bSucceeded)
    {
        OutError =
            LastError(TEXT("Upsert skill"));
    }

    sqlite3_finalize(Statement);
    return bSucceeded;
}

bool FOGSQLiteWorldStore::ListEntitySkills(
    const FOGEntityId& OwnerEntityId,
    TArray<FOGEntitySkillRecord>& OutSkills,
    FString& OutError) const
{
    OutSkills.Reset();
    OutError.Reset();

    if (!OwnerEntityId.IsValid())
    {
        OutError =
            TEXT("Skill list requires a valid owner.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "SELECT skill_content_id, learned_world_tick, current_state, "
        "development_state_json FROM entity_skills "
        "WHERE owner_entity_id = ? ORDER BY skill_content_id ASC;";

    if (sqlite3_prepare_v2(
            Database,
            Sql,
            -1,
            &Statement,
            nullptr) != SQLITE_OK)
    {
        OutError =
            LastError(TEXT("Prepare skill list"));
        return false;
    }

    if (!BindProgressionText(
            Statement,
            1,
            OwnerEntityId.ToString()))
    {
        OutError =
            LastError(TEXT("Bind skill list"));
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
                LastError(TEXT("Read skill list"));
            sqlite3_finalize(Statement);
            return false;
        }

        FOGEntitySkillRecord Skill;
        Skill.OwnerEntityId =
            OwnerEntityId;
        Skill.SkillId =
            FOGContentId(
                ProgressionColumnText(
                    Statement,
                    0));
        Skill.LearnedWorldTick =
            sqlite3_column_int64(
                Statement,
                1);
        Skill.CurrentState =
            FName(*ProgressionColumnText(
                Statement,
                2));
        Skill.DevelopmentStateJson =
            ProgressionColumnText(
                Statement,
                3);

        if (!Skill.SkillId.IsValid() ||
            Skill.CurrentState.IsNone())
        {
            OutError =
                TEXT("Stored skill state is invalid.");
            sqlite3_finalize(Statement);
            return false;
        }

        OutSkills.Add(MoveTemp(Skill));
    }

    sqlite3_finalize(Statement);
    return true;
}

bool FOGSQLiteWorldStore::UpsertSkillProvenance(
    const FOGSkillProvenanceRecord& Provenance,
    FString& OutError)
{
    OutError.Reset();

    if (!Provenance.OwnerEntityId.IsValid() ||
        !Provenance.SkillId.IsValid() ||
        Provenance.SourceKind.IsNone() ||
        Provenance.SourceContentOrEntityId.IsEmpty() ||
        Provenance.SourceWorldTick < 0)
    {
        OutError =
            TEXT("Skill provenance is invalid.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "INSERT INTO skill_provenance("
        "owner_entity_id, skill_content_id, source_kind, "
        "source_content_or_entity_id, source_world_tick"
        ") VALUES(?, ?, ?, ?, ?) "
        "ON CONFLICT(owner_entity_id, skill_content_id, source_kind, "
        "source_content_or_entity_id) DO UPDATE SET "
        "source_world_tick = MIN(skill_provenance.source_world_tick, excluded.source_world_tick);";

    if (sqlite3_prepare_v2(
            Database,
            Sql,
            -1,
            &Statement,
            nullptr) != SQLITE_OK)
    {
        OutError =
            LastError(TEXT("Prepare skill provenance upsert"));
        return false;
    }

    const bool bSucceeded =
        BindProgressionText(
            Statement,
            1,
            Provenance.OwnerEntityId.ToString()) &&
        BindProgressionText(
            Statement,
            2,
            Provenance.SkillId.ToString()) &&
        BindProgressionText(
            Statement,
            3,
            Provenance.SourceKind.ToString()) &&
        BindProgressionText(
            Statement,
            4,
            Provenance.SourceContentOrEntityId) &&
        sqlite3_bind_int64(
            Statement,
            5,
            Provenance.SourceWorldTick) == SQLITE_OK &&
        sqlite3_step(Statement) ==
            SQLITE_DONE;

    if (!bSucceeded)
    {
        OutError =
            LastError(TEXT("Upsert skill provenance"));
    }

    sqlite3_finalize(Statement);
    return bSucceeded;
}

bool FOGSQLiteWorldStore::ListSkillProvenance(
    const FOGEntityId& OwnerEntityId,
    const FOGContentId& SkillId,
    TArray<FOGSkillProvenanceRecord>& OutProvenance,
    FString& OutError) const
{
    OutProvenance.Reset();
    OutError.Reset();

    if (!OwnerEntityId.IsValid() ||
        !SkillId.IsValid())
    {
        OutError =
            TEXT("Skill-provenance list requires valid owner/skill IDs.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "SELECT source_kind, source_content_or_entity_id, source_world_tick "
        "FROM skill_provenance WHERE owner_entity_id = ? AND skill_content_id = ? "
        "ORDER BY source_world_tick ASC, source_kind ASC, source_content_or_entity_id ASC;";

    if (sqlite3_prepare_v2(
            Database,
            Sql,
            -1,
            &Statement,
            nullptr) != SQLITE_OK)
    {
        OutError =
            LastError(TEXT("Prepare skill provenance list"));
        return false;
    }

    if (!BindProgressionText(
            Statement,
            1,
            OwnerEntityId.ToString()) ||
        !BindProgressionText(
            Statement,
            2,
            SkillId.ToString()))
    {
        OutError =
            LastError(TEXT("Bind skill provenance list"));
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
                LastError(TEXT("Read skill provenance list"));
            sqlite3_finalize(Statement);
            return false;
        }

        FOGSkillProvenanceRecord Provenance;
        Provenance.OwnerEntityId =
            OwnerEntityId;
        Provenance.SkillId =
            SkillId;
        Provenance.SourceKind =
            FName(*ProgressionColumnText(
                Statement,
                0));
        Provenance.SourceContentOrEntityId =
            ProgressionColumnText(
                Statement,
                1);
        Provenance.SourceWorldTick =
            sqlite3_column_int64(
                Statement,
                2);

        if (Provenance.SourceKind.IsNone() ||
            Provenance.SourceContentOrEntityId.IsEmpty())
        {
            OutError =
                TEXT("Stored skill provenance is invalid.");
            sqlite3_finalize(Statement);
            return false;
        }

        OutProvenance.Add(
            MoveTemp(Provenance));
    }

    sqlite3_finalize(Statement);
    return true;
}

bool FOGSQLiteWorldStore::UpsertManifestationRouteNode(
    const FOGManifestationRouteNodeRecord& Node,
    FString& OutError)
{
    OutError.Reset();

    if (!Node.ManifestationId.IsValid() ||
        !Node.RouteId.IsValid() ||
        !Node.NodeId.IsValid() ||
        Node.State.IsNone() ||
        (Node.bHasEnteredWorldTick &&
         Node.EnteredWorldTick < 0) ||
        (Node.bHasCompletedWorldTick &&
         Node.CompletedWorldTick < 0) ||
        (Node.bHasEnteredWorldTick &&
         Node.bHasCompletedWorldTick &&
         Node.CompletedWorldTick <
             Node.EnteredWorldTick))
    {
        OutError =
            TEXT("Manifestation route node is invalid.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "INSERT INTO manifestation_route_nodes("
        "manifestation_entity_id, route_content_id, node_content_id, state, "
        "entered_world_tick, completed_world_tick, state_json"
        ") VALUES(?, ?, ?, ?, ?, ?, ?) "
        "ON CONFLICT(manifestation_entity_id, route_content_id, node_content_id) "
        "DO UPDATE SET state = excluded.state, "
        "entered_world_tick = COALESCE(manifestation_route_nodes.entered_world_tick, excluded.entered_world_tick), "
        "completed_world_tick = excluded.completed_world_tick, "
        "state_json = excluded.state_json;";

    if (sqlite3_prepare_v2(
            Database,
            Sql,
            -1,
            &Statement,
            nullptr) != SQLITE_OK)
    {
        OutError =
            LastError(TEXT("Prepare route-node upsert"));
        return false;
    }

    const bool bSucceeded =
        BindProgressionText(
            Statement,
            1,
            Node.ManifestationId.ToString()) &&
        BindProgressionText(
            Statement,
            2,
            Node.RouteId.ToString()) &&
        BindProgressionText(
            Statement,
            3,
            Node.NodeId.ToString()) &&
        BindProgressionText(
            Statement,
            4,
            Node.State.ToString()) &&
        BindOptionalProgressionTick(
            Statement,
            5,
            Node.bHasEnteredWorldTick,
            Node.EnteredWorldTick) &&
        BindOptionalProgressionTick(
            Statement,
            6,
            Node.bHasCompletedWorldTick,
            Node.CompletedWorldTick) &&
        BindProgressionText(
            Statement,
            7,
            Node.StateJson.IsEmpty()
                ? TEXT("{}")
                : Node.StateJson) &&
        sqlite3_step(Statement) ==
            SQLITE_DONE;

    if (!bSucceeded)
    {
        OutError =
            LastError(TEXT("Upsert route node"));
    }

    sqlite3_finalize(Statement);
    return bSucceeded;
}

bool FOGSQLiteWorldStore::ListManifestationRouteNodes(
    const FOGEntityId& ManifestationId,
    TArray<FOGManifestationRouteNodeRecord>& OutNodes,
    FString& OutError) const
{
    OutNodes.Reset();
    OutError.Reset();

    if (!ManifestationId.IsValid())
    {
        OutError =
            TEXT("Route-node list requires a valid Manifestation.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "SELECT route_content_id, node_content_id, state, entered_world_tick, "
        "completed_world_tick, state_json FROM manifestation_route_nodes "
        "WHERE manifestation_entity_id = ? "
        "ORDER BY route_content_id ASC, node_content_id ASC;";

    if (sqlite3_prepare_v2(
            Database,
            Sql,
            -1,
            &Statement,
            nullptr) != SQLITE_OK)
    {
        OutError =
            LastError(TEXT("Prepare route-node list"));
        return false;
    }

    if (!BindProgressionText(
            Statement,
            1,
            ManifestationId.ToString()))
    {
        OutError =
            LastError(TEXT("Bind route-node list"));
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
                LastError(TEXT("Read route-node list"));
            sqlite3_finalize(Statement);
            return false;
        }

        FOGManifestationRouteNodeRecord Node;
        Node.ManifestationId =
            ManifestationId;
        Node.RouteId =
            FOGContentId(
                ProgressionColumnText(
                    Statement,
                    0));
        Node.NodeId =
            FOGContentId(
                ProgressionColumnText(
                    Statement,
                    1));
        Node.State =
            FName(*ProgressionColumnText(
                Statement,
                2));

        if (sqlite3_column_type(
                Statement,
                3) != SQLITE_NULL)
        {
            Node.bHasEnteredWorldTick =
                true;
            Node.EnteredWorldTick =
                sqlite3_column_int64(
                    Statement,
                    3);
        }

        if (sqlite3_column_type(
                Statement,
                4) != SQLITE_NULL)
        {
            Node.bHasCompletedWorldTick =
                true;
            Node.CompletedWorldTick =
                sqlite3_column_int64(
                    Statement,
                    4);
        }

        Node.StateJson =
            ProgressionColumnText(
                Statement,
                5);

        if (!Node.RouteId.IsValid() ||
            !Node.NodeId.IsValid() ||
            Node.State.IsNone())
        {
            OutError =
                TEXT("Stored route node is invalid.");
            sqlite3_finalize(Statement);
            return false;
        }

        OutNodes.Add(MoveTemp(Node));
    }

    sqlite3_finalize(Statement);
    return true;
}

bool FOGSQLiteWorldStore::UpsertManifestationForm(
    const FOGManifestationFormRecord& Form,
    FString& OutError)
{
    OutError.Reset();

    if (!Form.ManifestationId.IsValid() ||
        !Form.FormId.IsValid() ||
        Form.State.IsNone() ||
        Form.UnlockedWorldTick < 0)
    {
        OutError =
            TEXT("Manifestation form is invalid.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "INSERT INTO manifestation_forms("
        "manifestation_entity_id, form_content_id, state, unlocked_world_tick, state_json"
        ") VALUES(?, ?, ?, ?, ?) "
        "ON CONFLICT(manifestation_entity_id, form_content_id) DO UPDATE SET "
        "state = excluded.state, "
        "unlocked_world_tick = manifestation_forms.unlocked_world_tick, "
        "state_json = excluded.state_json;";

    if (sqlite3_prepare_v2(
            Database,
            Sql,
            -1,
            &Statement,
            nullptr) != SQLITE_OK)
    {
        OutError =
            LastError(TEXT("Prepare form upsert"));
        return false;
    }

    const bool bSucceeded =
        BindProgressionText(
            Statement,
            1,
            Form.ManifestationId.ToString()) &&
        BindProgressionText(
            Statement,
            2,
            Form.FormId.ToString()) &&
        BindProgressionText(
            Statement,
            3,
            Form.State.ToString()) &&
        sqlite3_bind_int64(
            Statement,
            4,
            Form.UnlockedWorldTick) == SQLITE_OK &&
        BindProgressionText(
            Statement,
            5,
            Form.StateJson.IsEmpty()
                ? TEXT("{}")
                : Form.StateJson) &&
        sqlite3_step(Statement) ==
            SQLITE_DONE;

    if (!bSucceeded)
    {
        OutError =
            LastError(TEXT("Upsert form"));
    }

    sqlite3_finalize(Statement);
    return bSucceeded;
}

bool FOGSQLiteWorldStore::ListManifestationForms(
    const FOGEntityId& ManifestationId,
    TArray<FOGManifestationFormRecord>& OutForms,
    FString& OutError) const
{
    OutForms.Reset();
    OutError.Reset();

    if (!ManifestationId.IsValid())
    {
        OutError =
            TEXT("Form list requires a valid Manifestation.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "SELECT form_content_id, state, unlocked_world_tick, state_json "
        "FROM manifestation_forms WHERE manifestation_entity_id = ? "
        "ORDER BY unlocked_world_tick ASC, form_content_id ASC;";

    if (sqlite3_prepare_v2(
            Database,
            Sql,
            -1,
            &Statement,
            nullptr) != SQLITE_OK)
    {
        OutError =
            LastError(TEXT("Prepare form list"));
        return false;
    }

    if (!BindProgressionText(
            Statement,
            1,
            ManifestationId.ToString()))
    {
        OutError =
            LastError(TEXT("Bind form list"));
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
                LastError(TEXT("Read form list"));
            sqlite3_finalize(Statement);
            return false;
        }

        FOGManifestationFormRecord Form;
        Form.ManifestationId =
            ManifestationId;
        Form.FormId =
            FOGContentId(
                ProgressionColumnText(
                    Statement,
                    0));
        Form.State =
            FName(*ProgressionColumnText(
                Statement,
                1));
        Form.UnlockedWorldTick =
            sqlite3_column_int64(
                Statement,
                2);
        Form.StateJson =
            ProgressionColumnText(
                Statement,
                3);

        if (!Form.FormId.IsValid() ||
            Form.State.IsNone())
        {
            OutError =
                TEXT("Stored Manifestation form is invalid.");
            sqlite3_finalize(Statement);
            return false;
        }

        OutForms.Add(MoveTemp(Form));
    }

    sqlite3_finalize(Statement);
    return true;
}

bool FOGSQLiteWorldStore::UpsertManifestationReinforcement(
    const FOGManifestationReinforcementRecord& Reinforcement,
    FString& OutError)
{
    OutError.Reset();

    if (!Reinforcement.ManifestationId.IsValid() ||
        Reinforcement.ReinforcementState.IsNone() ||
        Reinforcement.UpdatedWorldTick < 0)
    {
        OutError =
            TEXT("Manifestation reinforcement state is invalid.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "INSERT INTO manifestation_reinforcement("
        "manifestation_entity_id, reinforcement_state, max_reinforced, "
        "updated_world_tick, state_json"
        ") VALUES(?, ?, ?, ?, ?) "
        "ON CONFLICT(manifestation_entity_id) DO UPDATE SET "
        "reinforcement_state = excluded.reinforcement_state, "
        "max_reinforced = excluded.max_reinforced, "
        "updated_world_tick = excluded.updated_world_tick, "
        "state_json = excluded.state_json;";

    if (sqlite3_prepare_v2(
            Database,
            Sql,
            -1,
            &Statement,
            nullptr) != SQLITE_OK)
    {
        OutError =
            LastError(TEXT("Prepare reinforcement upsert"));
        return false;
    }

    const bool bSucceeded =
        BindProgressionText(
            Statement,
            1,
            Reinforcement.ManifestationId.ToString()) &&
        BindProgressionText(
            Statement,
            2,
            Reinforcement.ReinforcementState.ToString()) &&
        sqlite3_bind_int(
            Statement,
            3,
            Reinforcement.bMaxReinforced
                ? 1
                : 0) == SQLITE_OK &&
        sqlite3_bind_int64(
            Statement,
            4,
            Reinforcement.UpdatedWorldTick) == SQLITE_OK &&
        BindProgressionText(
            Statement,
            5,
            Reinforcement.StateJson.IsEmpty()
                ? TEXT("{}")
                : Reinforcement.StateJson) &&
        sqlite3_step(Statement) ==
            SQLITE_DONE;

    if (!bSucceeded)
    {
        OutError =
            LastError(TEXT("Upsert reinforcement"));
    }

    sqlite3_finalize(Statement);
    return bSucceeded;
}

bool FOGSQLiteWorldStore::TryReadManifestationReinforcement(
    const FOGEntityId& ManifestationId,
    bool& bOutFound,
    FOGManifestationReinforcementRecord& OutReinforcement,
    FString& OutError) const
{
    bOutFound = false;
    OutReinforcement =
        FOGManifestationReinforcementRecord();
    OutError.Reset();

    if (!ManifestationId.IsValid())
    {
        OutError =
            TEXT("Reinforcement lookup requires a valid Manifestation.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "SELECT reinforcement_state, max_reinforced, updated_world_tick, state_json "
        "FROM manifestation_reinforcement WHERE manifestation_entity_id = ?;";

    if (sqlite3_prepare_v2(
            Database,
            Sql,
            -1,
            &Statement,
            nullptr) != SQLITE_OK)
    {
        OutError =
            LastError(TEXT("Prepare reinforcement read"));
        return false;
    }

    if (!BindProgressionText(
            Statement,
            1,
            ManifestationId.ToString()))
    {
        OutError =
            LastError(TEXT("Bind reinforcement read"));
        sqlite3_finalize(Statement);
        return false;
    }

    const int32 Step =
        sqlite3_step(Statement);
    if (Step == SQLITE_ROW)
    {
        bOutFound = true;
        OutReinforcement.ManifestationId =
            ManifestationId;
        OutReinforcement.ReinforcementState =
            FName(*ProgressionColumnText(
                Statement,
                0));
        OutReinforcement.bMaxReinforced =
            sqlite3_column_int(
                Statement,
                1) != 0;
        OutReinforcement.UpdatedWorldTick =
            sqlite3_column_int64(
                Statement,
                2);
        OutReinforcement.StateJson =
            ProgressionColumnText(
                Statement,
                3);
    }
    else if (Step != SQLITE_DONE)
    {
        OutError =
            LastError(TEXT("Read reinforcement"));
        sqlite3_finalize(Statement);
        return false;
    }

    sqlite3_finalize(Statement);
    return true;
}

bool FOGSQLiteWorldStore::UpsertEntityTranscendenceState(
    const FOGEntityTranscendenceStateRecord& State,
    FString& OutError)
{
    OutError.Reset();

    if (!State.EntityId.IsValid() ||
        !State.GradeId.IsValid() ||
        State.BreakthroughWorldTick < 0)
    {
        OutError =
            TEXT("Transcendence state is invalid.");
        return false;
    }

    bool bFound = false;
    FOGEntityTranscendenceStateRecord Existing;
    if (!TryReadEntityTranscendenceState(
            State.EntityId,
            bFound,
            Existing,
            OutError))
    {
        return false;
    }

    if (bFound &&
        (Existing.GradeId !=
             State.GradeId ||
         Existing.BreakthroughWorldTick !=
             State.BreakthroughWorldTick))
    {
        OutError =
            TEXT("Ordinary progression cannot replace an existing irreversible Transcendence breakthrough.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "INSERT INTO entity_transcendence_state("
        "entity_id, grade_content_id, breakthrough_world_tick, "
        "qualification_snapshot_json, proof_provenance_json, state_json"
        ") VALUES(?, ?, ?, ?, ?, ?) "
        "ON CONFLICT(entity_id) DO UPDATE SET "
        "qualification_snapshot_json = excluded.qualification_snapshot_json, "
        "proof_provenance_json = excluded.proof_provenance_json, "
        "state_json = excluded.state_json;";

    if (sqlite3_prepare_v2(
            Database,
            Sql,
            -1,
            &Statement,
            nullptr) != SQLITE_OK)
    {
        OutError =
            LastError(TEXT("Prepare Transcendence upsert"));
        return false;
    }

    const bool bSucceeded =
        BindProgressionText(
            Statement,
            1,
            State.EntityId.ToString()) &&
        BindProgressionText(
            Statement,
            2,
            State.GradeId.ToString()) &&
        sqlite3_bind_int64(
            Statement,
            3,
            State.BreakthroughWorldTick) == SQLITE_OK &&
        BindProgressionText(
            Statement,
            4,
            State.QualificationSnapshotJson.IsEmpty()
                ? TEXT("{}")
                : State.QualificationSnapshotJson) &&
        BindProgressionText(
            Statement,
            5,
            State.ProofProvenanceJson.IsEmpty()
                ? TEXT("{}")
                : State.ProofProvenanceJson) &&
        BindProgressionText(
            Statement,
            6,
            State.StateJson.IsEmpty()
                ? TEXT("{}")
                : State.StateJson) &&
        sqlite3_step(Statement) ==
            SQLITE_DONE;

    if (!bSucceeded)
    {
        OutError =
            LastError(TEXT("Upsert Transcendence state"));
    }

    sqlite3_finalize(Statement);
    return bSucceeded;
}

bool FOGSQLiteWorldStore::TryReadEntityTranscendenceState(
    const FOGEntityId& EntityId,
    bool& bOutFound,
    FOGEntityTranscendenceStateRecord& OutState,
    FString& OutError) const
{
    bOutFound = false;
    OutState =
        FOGEntityTranscendenceStateRecord();
    OutError.Reset();

    if (!EntityId.IsValid())
    {
        OutError =
            TEXT("Transcendence lookup requires a valid entity.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "SELECT grade_content_id, breakthrough_world_tick, "
        "qualification_snapshot_json, proof_provenance_json, state_json "
        "FROM entity_transcendence_state WHERE entity_id = ?;";

    if (sqlite3_prepare_v2(
            Database,
            Sql,
            -1,
            &Statement,
            nullptr) != SQLITE_OK)
    {
        OutError =
            LastError(TEXT("Prepare Transcendence read"));
        return false;
    }

    if (!BindProgressionText(
            Statement,
            1,
            EntityId.ToString()))
    {
        OutError =
            LastError(TEXT("Bind Transcendence read"));
        sqlite3_finalize(Statement);
        return false;
    }

    const int32 Step =
        sqlite3_step(Statement);
    if (Step == SQLITE_ROW)
    {
        bOutFound = true;
        OutState.EntityId =
            EntityId;
        OutState.GradeId =
            FOGContentId(
                ProgressionColumnText(
                    Statement,
                    0));
        OutState.BreakthroughWorldTick =
            sqlite3_column_int64(
                Statement,
                1);
        OutState.QualificationSnapshotJson =
            ProgressionColumnText(
                Statement,
                2);
        OutState.ProofProvenanceJson =
            ProgressionColumnText(
                Statement,
                3);
        OutState.StateJson =
            ProgressionColumnText(
                Statement,
                4);

        if (!OutState.GradeId.IsValid())
        {
            OutError =
                TEXT("Stored Transcendence state has an invalid grade.");
            sqlite3_finalize(Statement);
            return false;
        }
    }
    else if (Step != SQLITE_DONE)
    {
        OutError =
            LastError(TEXT("Read Transcendence state"));
        sqlite3_finalize(Statement);
        return false;
    }

    sqlite3_finalize(Statement);
    return true;
}

bool FOGSQLiteWorldStore::UpsertManifestationWorldFantasmState(
    const FOGManifestationWorldFantasmStateRecord& State,
    FString& OutError)
{
    OutError.Reset();

    if (!State.ManifestationId.IsValid() ||
        !State.GradeId.IsValid() ||
        State.UnlockedWorldTick < 0 ||
        State.UpdatedWorldTick <
            State.UnlockedWorldTick)
    {
        OutError =
            TEXT("Manifestation World Fantasm state is invalid.");
        return false;
    }

    bool bFound = false;
    FOGManifestationWorldFantasmStateRecord Existing;
    if (!TryReadManifestationWorldFantasmState(
            State.ManifestationId,
            bFound,
            Existing,
            OutError))
    {
        return false;
    }

    if (bFound &&
        (Existing.GradeId !=
             State.GradeId ||
         Existing.UnlockedWorldTick !=
             State.UnlockedWorldTick))
    {
        OutError =
            TEXT("World Fantasm base grade/unlock provenance is immutable.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "INSERT INTO manifestation_world_fantasm_state("
        "manifestation_entity_id, grade_content_id, unlocked_world_tick, "
        "expression_profile_content_id, evolution_state_json, updated_world_tick"
        ") VALUES(?, ?, ?, ?, ?, ?) "
        "ON CONFLICT(manifestation_entity_id) DO UPDATE SET "
        "expression_profile_content_id = excluded.expression_profile_content_id, "
        "evolution_state_json = excluded.evolution_state_json, "
        "updated_world_tick = excluded.updated_world_tick;";

    if (sqlite3_prepare_v2(
            Database,
            Sql,
            -1,
            &Statement,
            nullptr) != SQLITE_OK)
    {
        OutError =
            LastError(TEXT("Prepare World Fantasm upsert"));
        return false;
    }

    const bool bSucceeded =
        BindProgressionText(
            Statement,
            1,
            State.ManifestationId.ToString()) &&
        BindProgressionText(
            Statement,
            2,
            State.GradeId.ToString()) &&
        sqlite3_bind_int64(
            Statement,
            3,
            State.UnlockedWorldTick) == SQLITE_OK &&
        BindOptionalProgressionContentId(
            Statement,
            4,
            State.ExpressionProfileId) &&
        BindProgressionText(
            Statement,
            5,
            State.EvolutionStateJson.IsEmpty()
                ? TEXT("{}")
                : State.EvolutionStateJson) &&
        sqlite3_bind_int64(
            Statement,
            6,
            State.UpdatedWorldTick) == SQLITE_OK &&
        sqlite3_step(Statement) ==
            SQLITE_DONE;

    if (!bSucceeded)
    {
        OutError =
            LastError(TEXT("Upsert World Fantasm state"));
    }

    sqlite3_finalize(Statement);
    return bSucceeded;
}

bool FOGSQLiteWorldStore::TryReadManifestationWorldFantasmState(
    const FOGEntityId& ManifestationId,
    bool& bOutFound,
    FOGManifestationWorldFantasmStateRecord& OutState,
    FString& OutError) const
{
    bOutFound = false;
    OutState =
        FOGManifestationWorldFantasmStateRecord();
    OutError.Reset();

    if (!ManifestationId.IsValid())
    {
        OutError =
            TEXT("World Fantasm lookup requires a valid Manifestation.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "SELECT grade_content_id, unlocked_world_tick, "
        "expression_profile_content_id, evolution_state_json, updated_world_tick "
        "FROM manifestation_world_fantasm_state WHERE manifestation_entity_id = ?;";

    if (sqlite3_prepare_v2(
            Database,
            Sql,
            -1,
            &Statement,
            nullptr) != SQLITE_OK)
    {
        OutError =
            LastError(TEXT("Prepare World Fantasm read"));
        return false;
    }

    if (!BindProgressionText(
            Statement,
            1,
            ManifestationId.ToString()))
    {
        OutError =
            LastError(TEXT("Bind World Fantasm read"));
        sqlite3_finalize(Statement);
        return false;
    }

    const int32 Step =
        sqlite3_step(Statement);
    if (Step == SQLITE_ROW)
    {
        bOutFound = true;
        OutState.ManifestationId =
            ManifestationId;
        OutState.GradeId =
            FOGContentId(
                ProgressionColumnText(
                    Statement,
                    0));
        OutState.UnlockedWorldTick =
            sqlite3_column_int64(
                Statement,
                1);

        const FString Profile =
            ProgressionColumnText(
                Statement,
                2);
        if (!Profile.IsEmpty())
        {
            OutState.ExpressionProfileId =
                FOGContentId(Profile);
        }

        OutState.EvolutionStateJson =
            ProgressionColumnText(
                Statement,
                3);
        OutState.UpdatedWorldTick =
            sqlite3_column_int64(
                Statement,
                4);
    }
    else if (Step != SQLITE_DONE)
    {
        OutError =
            LastError(TEXT("Read World Fantasm state"));
        sqlite3_finalize(Statement);
        return false;
    }

    sqlite3_finalize(Statement);
    return true;
}

bool FOGSQLiteWorldStore::UpsertProtagonistWorldManifestationState(
    const FOGProtagonistWorldManifestationStateRecord& State,
    FString& OutError)
{
    OutError.Reset();

    if (!State.OwnerEntityId.IsValid() ||
        State.UnlockedWorldTick < 0 ||
        State.UpdatedWorldTick <
            State.UnlockedWorldTick)
    {
        OutError =
            TEXT("Protagonist World Manifestation state is invalid.");
        return false;
    }

    bool bFound = false;
    FOGProtagonistWorldManifestationStateRecord Existing;
    if (!TryReadProtagonistWorldManifestationState(
            State.OwnerEntityId,
            bFound,
            Existing,
            OutError))
    {
        return false;
    }

    if (bFound &&
        Existing.UnlockedWorldTick !=
            State.UnlockedWorldTick)
    {
        OutError =
            TEXT("Personal World Manifestation unlock history is immutable.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "INSERT INTO protagonist_world_manifestation_state("
        "owner_entity_id, unlocked_world_tick, expression_profile_content_id, "
        "provenance_state_json, evolution_state_json, updated_world_tick"
        ") VALUES(?, ?, ?, ?, ?, ?) "
        "ON CONFLICT(owner_entity_id) DO UPDATE SET "
        "expression_profile_content_id = excluded.expression_profile_content_id, "
        "provenance_state_json = excluded.provenance_state_json, "
        "evolution_state_json = excluded.evolution_state_json, "
        "updated_world_tick = excluded.updated_world_tick;";

    if (sqlite3_prepare_v2(
            Database,
            Sql,
            -1,
            &Statement,
            nullptr) != SQLITE_OK)
    {
        OutError =
            LastError(TEXT("Prepare personal World Manifestation upsert"));
        return false;
    }

    const bool bSucceeded =
        BindProgressionText(
            Statement,
            1,
            State.OwnerEntityId.ToString()) &&
        sqlite3_bind_int64(
            Statement,
            2,
            State.UnlockedWorldTick) == SQLITE_OK &&
        BindOptionalProgressionContentId(
            Statement,
            3,
            State.ExpressionProfileId) &&
        BindProgressionText(
            Statement,
            4,
            State.ProvenanceStateJson.IsEmpty()
                ? TEXT("{}")
                : State.ProvenanceStateJson) &&
        BindProgressionText(
            Statement,
            5,
            State.EvolutionStateJson.IsEmpty()
                ? TEXT("{}")
                : State.EvolutionStateJson) &&
        sqlite3_bind_int64(
            Statement,
            6,
            State.UpdatedWorldTick) == SQLITE_OK &&
        sqlite3_step(Statement) ==
            SQLITE_DONE;

    if (!bSucceeded)
    {
        OutError =
            LastError(TEXT("Upsert personal World Manifestation"));
    }

    sqlite3_finalize(Statement);
    return bSucceeded;
}

bool FOGSQLiteWorldStore::TryReadProtagonistWorldManifestationState(
    const FOGEntityId& OwnerEntityId,
    bool& bOutFound,
    FOGProtagonistWorldManifestationStateRecord& OutState,
    FString& OutError) const
{
    bOutFound = false;
    OutState =
        FOGProtagonistWorldManifestationStateRecord();
    OutError.Reset();

    if (!OwnerEntityId.IsValid())
    {
        OutError =
            TEXT("Personal World Manifestation lookup requires a valid owner.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "SELECT unlocked_world_tick, expression_profile_content_id, "
        "provenance_state_json, evolution_state_json, updated_world_tick "
        "FROM protagonist_world_manifestation_state WHERE owner_entity_id = ?;";

    if (sqlite3_prepare_v2(
            Database,
            Sql,
            -1,
            &Statement,
            nullptr) != SQLITE_OK)
    {
        OutError =
            LastError(TEXT("Prepare personal World Manifestation read"));
        return false;
    }

    if (!BindProgressionText(
            Statement,
            1,
            OwnerEntityId.ToString()))
    {
        OutError =
            LastError(TEXT("Bind personal World Manifestation read"));
        sqlite3_finalize(Statement);
        return false;
    }

    const int32 Step =
        sqlite3_step(Statement);
    if (Step == SQLITE_ROW)
    {
        bOutFound = true;
        OutState.OwnerEntityId =
            OwnerEntityId;
        OutState.UnlockedWorldTick =
            sqlite3_column_int64(
                Statement,
                0);

        const FString Profile =
            ProgressionColumnText(
                Statement,
                1);
        if (!Profile.IsEmpty())
        {
            OutState.ExpressionProfileId =
                FOGContentId(Profile);
        }

        OutState.ProvenanceStateJson =
            ProgressionColumnText(
                Statement,
                2);
        OutState.EvolutionStateJson =
            ProgressionColumnText(
                Statement,
                3);
        OutState.UpdatedWorldTick =
            sqlite3_column_int64(
                Statement,
                4);
    }
    else if (Step != SQLITE_DONE)
    {
        OutError =
            LastError(TEXT("Read personal World Manifestation"));
        sqlite3_finalize(Statement);
        return false;
    }

    sqlite3_finalize(Statement);
    return true;
}

bool FOGSQLiteWorldStore::UpsertCharacterConvergence(
    const FOGCharacterConvergenceRecord& Convergence,
    int64 CreatedWorldTick,
    FString& OutError)
{
    OutError.Reset();

    if (!Convergence.ConvergenceId.IsValid() ||
        !Convergence.IdentityId.IsValid() ||
        !Convergence.ResultManifestationId.IsValid() ||
        !Convergence.RuleId.IsValid() ||
        Convergence.WorldTick < 0)
    {
        OutError =
            TEXT("Character Convergence record is invalid.");
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
            Convergence.ConvergenceId,
            TEXT("character_convergence"),
            CreatedWorldTick,
            TEXT("{}"),
            Error))
    {
        return Fail(Error);
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "INSERT INTO character_convergences("
        "convergence_entity_id, identity_content_id, result_manifestation_entity_id, "
        "rule_content_id, world_tick, state_json"
        ") VALUES(?, ?, ?, ?, ?, ?) "
        "ON CONFLICT(convergence_entity_id) DO UPDATE SET "
        "identity_content_id = excluded.identity_content_id, "
        "result_manifestation_entity_id = excluded.result_manifestation_entity_id, "
        "rule_content_id = excluded.rule_content_id, "
        "world_tick = excluded.world_tick, "
        "state_json = excluded.state_json;";

    if (sqlite3_prepare_v2(
            Database,
            Sql,
            -1,
            &Statement,
            nullptr) != SQLITE_OK)
    {
        return Fail(
            LastError(TEXT("Prepare Character Convergence upsert")));
    }

    const bool bSucceeded =
        BindProgressionText(
            Statement,
            1,
            Convergence.ConvergenceId.ToString()) &&
        BindProgressionText(
            Statement,
            2,
            Convergence.IdentityId.ToString()) &&
        BindProgressionText(
            Statement,
            3,
            Convergence.ResultManifestationId.ToString()) &&
        BindProgressionText(
            Statement,
            4,
            Convergence.RuleId.ToString()) &&
        sqlite3_bind_int64(
            Statement,
            5,
            Convergence.WorldTick) == SQLITE_OK &&
        BindProgressionText(
            Statement,
            6,
            Convergence.StateJson.IsEmpty()
                ? TEXT("{}")
                : Convergence.StateJson) &&
        sqlite3_step(Statement) ==
            SQLITE_DONE;

    const FString SqlError =
        bSucceeded
            ? FString()
            : LastError(TEXT("Upsert Character Convergence"));
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

bool FOGSQLiteWorldStore::TryReadCharacterConvergence(
    const FOGEntityId& ConvergenceId,
    bool& bOutFound,
    FOGCharacterConvergenceRecord& OutConvergence,
    FString& OutError) const
{
    bOutFound = false;
    OutConvergence =
        FOGCharacterConvergenceRecord();
    OutError.Reset();

    if (!ConvergenceId.IsValid())
    {
        OutError =
            TEXT("Convergence lookup requires a valid ID.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "SELECT identity_content_id, result_manifestation_entity_id, "
        "rule_content_id, world_tick, state_json "
        "FROM character_convergences WHERE convergence_entity_id = ?;";

    if (sqlite3_prepare_v2(
            Database,
            Sql,
            -1,
            &Statement,
            nullptr) != SQLITE_OK)
    {
        OutError =
            LastError(TEXT("Prepare Convergence read"));
        return false;
    }

    if (!BindProgressionText(
            Statement,
            1,
            ConvergenceId.ToString()))
    {
        OutError =
            LastError(TEXT("Bind Convergence read"));
        sqlite3_finalize(Statement);
        return false;
    }

    const int32 Step =
        sqlite3_step(Statement);
    if (Step == SQLITE_ROW)
    {
        bOutFound = true;
        OutConvergence.ConvergenceId =
            ConvergenceId;
        OutConvergence.IdentityId =
            FOGContentId(
                ProgressionColumnText(
                    Statement,
                    0));
        OutConvergence.ResultManifestationId =
            ParseProgressionEntityId(
                ProgressionColumnText(
                    Statement,
                    1));
        OutConvergence.RuleId =
            FOGContentId(
                ProgressionColumnText(
                    Statement,
                    2));
        OutConvergence.WorldTick =
            sqlite3_column_int64(
                Statement,
                3);
        OutConvergence.StateJson =
            ProgressionColumnText(
                Statement,
                4);

        if (!OutConvergence.IdentityId.IsValid() ||
            !OutConvergence.ResultManifestationId.IsValid() ||
            !OutConvergence.RuleId.IsValid())
        {
            OutError =
                TEXT("Stored Character Convergence is invalid.");
            sqlite3_finalize(Statement);
            return false;
        }
    }
    else if (Step != SQLITE_DONE)
    {
        OutError =
            LastError(TEXT("Read Convergence"));
        sqlite3_finalize(Statement);
        return false;
    }

    sqlite3_finalize(Statement);
    return true;
}

bool FOGSQLiteWorldStore::ListCharacterConvergencesByIdentity(
    const FOGContentId& IdentityId,
    TArray<FOGCharacterConvergenceRecord>& OutConvergences,
    FString& OutError) const
{
    OutConvergences.Reset();
    OutError.Reset();

    if (!IdentityId.IsValid())
    {
        OutError =
            TEXT("Convergence list requires a valid Character Identity.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "SELECT convergence_entity_id FROM character_convergences "
        "WHERE identity_content_id = ? "
        "ORDER BY world_tick ASC, convergence_entity_id ASC;";

    if (sqlite3_prepare_v2(
            Database,
            Sql,
            -1,
            &Statement,
            nullptr) != SQLITE_OK)
    {
        OutError =
            LastError(TEXT("Prepare Convergence list"));
        return false;
    }

    if (!BindProgressionText(
            Statement,
            1,
            IdentityId.ToString()))
    {
        OutError =
            LastError(TEXT("Bind Convergence list"));
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
                LastError(TEXT("Read Convergence list"));
            sqlite3_finalize(Statement);
            return false;
        }

        const FOGEntityId Id =
            ParseProgressionEntityId(
                ProgressionColumnText(
                    Statement,
                    0));
        if (!Id.IsValid())
        {
            OutError =
                TEXT("Stored Convergence list contains an invalid ID.");
            sqlite3_finalize(Statement);
            return false;
        }
        Ids.Add(Id);
    }

    sqlite3_finalize(Statement);

    for (const FOGEntityId& Id : Ids)
    {
        bool bFound = false;
        FOGCharacterConvergenceRecord Convergence;
        if (!TryReadCharacterConvergence(
                Id,
                bFound,
                Convergence,
                OutError) ||
            !bFound)
        {
            if (OutError.IsEmpty())
            {
                OutError =
                    TEXT("Convergence disappeared during list read.");
            }
            OutConvergences.Reset();
            return false;
        }
        OutConvergences.Add(MoveTemp(Convergence));
    }

    return true;
}

bool FOGSQLiteWorldStore::UpsertCharacterConvergenceSource(
    const FOGCharacterConvergenceSourceRecord& Source,
    FString& OutError)
{
    OutError.Reset();

    if (!Source.ConvergenceId.IsValid() ||
        !Source.SourceManifestationId.IsValid() ||
        Source.Ordinal < 0)
    {
        OutError =
            TEXT("Character Convergence source is invalid.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "INSERT INTO character_convergence_sources("
        "convergence_entity_id, source_manifestation_entity_id, lineage_content_id, ordinal"
        ") VALUES(?, ?, ?, ?) "
        "ON CONFLICT(convergence_entity_id, source_manifestation_entity_id) DO UPDATE SET "
        "lineage_content_id = excluded.lineage_content_id, ordinal = excluded.ordinal;";

    if (sqlite3_prepare_v2(
            Database,
            Sql,
            -1,
            &Statement,
            nullptr) != SQLITE_OK)
    {
        OutError =
            LastError(TEXT("Prepare Convergence-source upsert"));
        return false;
    }

    const bool bSucceeded =
        BindProgressionText(
            Statement,
            1,
            Source.ConvergenceId.ToString()) &&
        BindProgressionText(
            Statement,
            2,
            Source.SourceManifestationId.ToString()) &&
        BindOptionalProgressionContentId(
            Statement,
            3,
            Source.LineageId) &&
        sqlite3_bind_int(
            Statement,
            4,
            Source.Ordinal) == SQLITE_OK &&
        sqlite3_step(Statement) ==
            SQLITE_DONE;

    if (!bSucceeded)
    {
        OutError =
            LastError(TEXT("Upsert Convergence source"));
    }

    sqlite3_finalize(Statement);
    return bSucceeded;
}

bool FOGSQLiteWorldStore::ListCharacterConvergenceSources(
    const FOGEntityId& ConvergenceId,
    TArray<FOGCharacterConvergenceSourceRecord>& OutSources,
    FString& OutError) const
{
    OutSources.Reset();
    OutError.Reset();

    if (!ConvergenceId.IsValid())
    {
        OutError =
            TEXT("Convergence-source list requires a valid Convergence ID.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "SELECT source_manifestation_entity_id, lineage_content_id, ordinal "
        "FROM character_convergence_sources WHERE convergence_entity_id = ? "
        "ORDER BY ordinal ASC, source_manifestation_entity_id ASC;";

    if (sqlite3_prepare_v2(
            Database,
            Sql,
            -1,
            &Statement,
            nullptr) != SQLITE_OK)
    {
        OutError =
            LastError(TEXT("Prepare Convergence-source list"));
        return false;
    }

    if (!BindProgressionText(
            Statement,
            1,
            ConvergenceId.ToString()))
    {
        OutError =
            LastError(TEXT("Bind Convergence-source list"));
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
                LastError(TEXT("Read Convergence-source list"));
            sqlite3_finalize(Statement);
            return false;
        }

        FOGCharacterConvergenceSourceRecord Source;
        Source.ConvergenceId =
            ConvergenceId;
        Source.SourceManifestationId =
            ParseProgressionEntityId(
                ProgressionColumnText(
                    Statement,
                    0));

        const FString LineageId =
            ProgressionColumnText(
                Statement,
                1);
        if (!LineageId.IsEmpty())
        {
            Source.LineageId =
                FOGContentId(LineageId);
        }

        Source.Ordinal =
            sqlite3_column_int(
                Statement,
                2);

        if (!Source.SourceManifestationId.IsValid() ||
            Source.Ordinal < 0)
        {
            OutError =
                TEXT("Stored Convergence source is invalid.");
            sqlite3_finalize(Statement);
            return false;
        }

        OutSources.Add(MoveTemp(Source));
    }

    sqlite3_finalize(Statement);
    return true;
}

bool FOGSQLiteWorldStore::MigrateProgression0010(
    FString& OutError)
{
    OutError.Reset();

    // Opaque pre-0010 progression JSON is preserved on the legacy row for
    // provenance. Do not fabricate Rank/Class/Factor/route identities from it.
    return ExecuteSql(
        TEXT("INSERT OR IGNORE INTO manifestation_reinforcement(")
        TEXT("manifestation_entity_id, reinforcement_state, max_reinforced, ")
        TEXT("updated_world_tick, state_json) ")
        TEXT("SELECT manifestation_entity_id, 'legacy_unassessed', 0, ")
        TEXT("acquisition_world_tick, ")
        TEXT("'{\"legacy_progression_state_retained\":true}' ")
        TEXT("FROM character_manifestations;"),
        OutError);
}

bool FOGSQLiteWorldStore::ValidateProgressionMigration0010(
    FString& OutError) const
{
    OutError.Reset();

    int32 MissingReinforcementRows = 0;
    if (!ReadProgressionCount(
            Database,
            "SELECT COUNT(*) FROM character_manifestations cm "
            "LEFT JOIN manifestation_reinforcement mr "
            "ON mr.manifestation_entity_id = cm.manifestation_entity_id "
            "WHERE mr.manifestation_entity_id IS NULL;",
            MissingReinforcementRows,
            OutError))
    {
        return false;
    }

    if (MissingReinforcementRows != 0)
    {
        OutError = FString::Printf(
            TEXT("Migration 0010 left %d Manifestations without reinforcement state."),
            MissingReinforcementRows);
        return false;
    }

    int32 InvalidRankRows = 0;
    if (!ReadProgressionCount(
            Database,
            "SELECT COUNT(*) FROM entity_rank_state "
            "WHERE attained_level NOT BETWEEN 1 AND 100 "
            "OR peak_level NOT BETWEEN 1 AND 100 "
            "OR ((effective_rank_content_id IS NULL) <> (effective_level IS NULL)) "
            "OR (effective_level IS NOT NULL AND effective_level NOT BETWEEN 1 AND 100);",
            InvalidRankRows,
            OutError))
    {
        return false;
    }

    if (InvalidRankRows != 0)
    {
        OutError = FString::Printf(
            TEXT("Migration 0010 produced %d invalid Rank rows."),
            InvalidRankRows);
        return false;
    }

    int32 ActiveSlotColumns = 0;
    sqlite3_stmt* TableInfo = nullptr;
    if (sqlite3_prepare_v2(
            Database,
            "PRAGMA table_info(factor_instances);",
            -1,
            &TableInfo,
            nullptr) != SQLITE_OK)
    {
        OutError =
            LastError(TEXT("Prepare Factor schema validation"));
        return false;
    }

    while (true)
    {
        const int32 Step =
            sqlite3_step(TableInfo);
        if (Step == SQLITE_DONE)
        {
            break;
        }

        if (Step != SQLITE_ROW)
        {
            OutError =
                LastError(TEXT("Read Factor schema validation"));
            sqlite3_finalize(TableInfo);
            return false;
        }

        const FString Column =
            ProgressionColumnText(
                TableInfo,
                1);
        if (Column.Equals(
                TEXT("active"),
                ESearchCase::IgnoreCase) ||
            Column.Equals(
                TEXT("active_slot"),
                ESearchCase::IgnoreCase) ||
            Column.Equals(
                TEXT("is_active"),
                ESearchCase::IgnoreCase))
        {
            ++ActiveSlotColumns;
        }
    }

    sqlite3_finalize(TableInfo);

    if (ActiveSlotColumns != 0)
    {
        OutError =
            TEXT("Migration 0010 introduced a forbidden Factor active-slot column.");
        return false;
    }

    int32 DuplicateGrandSeats = 0;
    if (!ReadProgressionCount(
            Database,
            "SELECT COUNT(*) FROM ("
            "SELECT class_content_id FROM grand_class_seats "
            "GROUP BY class_content_id HAVING COUNT(*) > 1);",
            DuplicateGrandSeats,
            OutError))
    {
        return false;
    }

    if (DuplicateGrandSeats != 0)
    {
        OutError =
            TEXT("Migration 0010 produced duplicate Grand seats for a canonical Class.");
        return false;
    }

    return true;
}
