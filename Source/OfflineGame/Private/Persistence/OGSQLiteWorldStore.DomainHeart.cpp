#include "Persistence/OGSQLiteWorldStore.h"

#include "sqlite/sqlite3.h"

namespace
{
bool BindDomainText(
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

FString DomainColumnText(
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

FOGEntityId ParseDomainEntityId(
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

bool BindOptionalDomainEntityId(
    sqlite3_stmt* Statement,
    int32 Index,
    const FOGEntityId& EntityId)
{
    return EntityId.IsValid()
        ? BindDomainText(
            Statement,
            Index,
            EntityId.ToString())
        : sqlite3_bind_null(
            Statement,
            Index) == SQLITE_OK;
}

bool BindOptionalDomainContentId(
    sqlite3_stmt* Statement,
    int32 Index,
    const FOGContentId& ContentId)
{
    return ContentId.IsValid()
        ? BindDomainText(
            Statement,
            Index,
            ContentId.ToString())
        : sqlite3_bind_null(
            Statement,
            Index) == SQLITE_OK;
}

bool BindOptionalDomainTick(
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

bool IsValidDomainStateName(FName State)
{
    return State == FName(TEXT("none")) ||
        State == FName(TEXT("functional")) ||
        State == FName(TEXT("damaged")) ||
        State == FName(TEXT("heart_lost_ruining")) ||
        State == FName(TEXT("ruined")) ||
        State == FName(TEXT("reconstituting"));
}

bool ReadDomainCount(
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
            TEXT("Failed to prepare migration 0009 validation query.");
        return false;
    }

    const int32 Step =
        sqlite3_step(Statement);
    if (Step != SQLITE_ROW)
    {
        OutError =
            TEXT("Failed to execute migration 0009 validation query.");
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

bool ReadCoreLossEventTick(
    sqlite3* Database,
    const FOGEntityId& CoreId,
    bool& bOutFound,
    int64& OutTick,
    FString& OutError)
{
    bOutFound = false;
    OutTick = 0;
    OutError.Reset();

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "SELECT MIN(world_tick) "
        "FROM world_events "
        "WHERE primary_entity_id = ? "
        "AND event_type IN ('domain_core.broken','domain_core.absorbed');";

    if (sqlite3_prepare_v2(
            Database,
            Sql,
            -1,
            &Statement,
            nullptr) != SQLITE_OK)
    {
        OutError =
            TEXT("Failed to prepare legacy Core-loss event lookup.");
        return false;
    }

    if (!BindDomainText(
            Statement,
            1,
            CoreId.ToString()))
    {
        OutError =
            TEXT("Failed to bind legacy Core-loss event lookup.");
        sqlite3_finalize(Statement);
        return false;
    }

    if (sqlite3_step(Statement) != SQLITE_ROW)
    {
        OutError =
            TEXT("Failed to execute legacy Core-loss event lookup.");
        sqlite3_finalize(Statement);
        return false;
    }

    if (sqlite3_column_type(
            Statement,
            0) != SQLITE_NULL)
    {
        bOutFound = true;
        OutTick =
            sqlite3_column_int64(
                Statement,
                0);
    }

    sqlite3_finalize(Statement);
    return true;
}
}

bool FOGSQLiteWorldStore::UpsertTerritoryDomainState(
    const FOGTerritoryDomainStateRecord& State,
    FString& OutError)
{
    OutError.Reset();

    if (!State.TerritoryId.IsValid() ||
        !IsValidDomainStateName(
            State.DomainState) ||
        (State.DomainState ==
             FName(TEXT("functional")) &&
         !State.ActiveCoreId.IsValid()) ||
        (State.DomainState ==
             FName(TEXT("damaged")) &&
         !State.ActiveCoreId.IsValid()) ||
        (State.DomainState ==
             FName(TEXT("reconstituting")) &&
         !State.ReconstitutionProjectId.IsValid()) ||
        ((State.DomainState ==
              FName(TEXT("heart_lost_ruining")) ||
          State.DomainState ==
              FName(TEXT("ruined"))) &&
         State.ActiveCoreId.IsValid()))
    {
        OutError =
            TEXT("Territory Domain-heart state is invalid.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "INSERT INTO territory_domain_state("
        "territory_entity_id, active_core_entity_id, domain_state, "
        "heart_lost_world_tick, ruin_started_world_tick, "
        "reconstitution_project_entity_id, state_json"
        ") VALUES(?, ?, ?, ?, ?, ?, ?) "
        "ON CONFLICT(territory_entity_id) DO UPDATE SET "
        "active_core_entity_id = excluded.active_core_entity_id, "
        "domain_state = excluded.domain_state, "
        "heart_lost_world_tick = excluded.heart_lost_world_tick, "
        "ruin_started_world_tick = excluded.ruin_started_world_tick, "
        "reconstitution_project_entity_id = excluded.reconstitution_project_entity_id, "
        "state_json = excluded.state_json;";

    if (sqlite3_prepare_v2(
            Database,
            Sql,
            -1,
            &Statement,
            nullptr) != SQLITE_OK)
    {
        OutError =
            LastError(TEXT("Prepare Territory Domain-heart upsert"));
        return false;
    }

    const bool bBound =
        BindDomainText(
            Statement,
            1,
            State.TerritoryId.ToString()) &&
        BindOptionalDomainEntityId(
            Statement,
            2,
            State.ActiveCoreId) &&
        BindDomainText(
            Statement,
            3,
            State.DomainState.ToString()) &&
        BindOptionalDomainTick(
            Statement,
            4,
            State.bHasHeartLostWorldTick,
            State.HeartLostWorldTick) &&
        BindOptionalDomainTick(
            Statement,
            5,
            State.bHasRuinStartedWorldTick,
            State.RuinStartedWorldTick) &&
        BindOptionalDomainEntityId(
            Statement,
            6,
            State.ReconstitutionProjectId) &&
        BindDomainText(
            Statement,
            7,
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
            LastError(TEXT("Upsert Territory Domain-heart state"));
    }

    sqlite3_finalize(Statement);
    return bSucceeded;
}

bool FOGSQLiteWorldStore::TryReadTerritoryDomainState(
    const FOGEntityId& TerritoryId,
    bool& bOutFound,
    FOGTerritoryDomainStateRecord& OutState,
    FString& OutError) const
{
    bOutFound = false;
    OutState = FOGTerritoryDomainStateRecord();
    OutError.Reset();

    if (!TerritoryId.IsValid())
    {
        OutError =
            TEXT("Territory Domain-heart lookup requires a valid Territory ID.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "SELECT active_core_entity_id, domain_state, "
        "heart_lost_world_tick, ruin_started_world_tick, "
        "reconstitution_project_entity_id, state_json "
        "FROM territory_domain_state WHERE territory_entity_id = ?;";

    if (sqlite3_prepare_v2(
            Database,
            Sql,
            -1,
            &Statement,
            nullptr) != SQLITE_OK)
    {
        OutError =
            LastError(TEXT("Prepare Territory Domain-heart read"));
        return false;
    }

    if (!BindDomainText(
            Statement,
            1,
            TerritoryId.ToString()))
    {
        OutError =
            LastError(TEXT("Bind Territory Domain-heart read"));
        sqlite3_finalize(Statement);
        return false;
    }

    const int32 Step =
        sqlite3_step(Statement);

    if (Step == SQLITE_ROW)
    {
        bOutFound = true;
        OutState.TerritoryId =
            TerritoryId;
        OutState.ActiveCoreId =
            ParseDomainEntityId(
                DomainColumnText(
                    Statement,
                    0));
        OutState.DomainState =
            FName(*DomainColumnText(
                Statement,
                1));

        if (sqlite3_column_type(
                Statement,
                2) != SQLITE_NULL)
        {
            OutState.bHasHeartLostWorldTick = true;
            OutState.HeartLostWorldTick =
                sqlite3_column_int64(
                    Statement,
                    2);
        }

        if (sqlite3_column_type(
                Statement,
                3) != SQLITE_NULL)
        {
            OutState.bHasRuinStartedWorldTick = true;
            OutState.RuinStartedWorldTick =
                sqlite3_column_int64(
                    Statement,
                    3);
        }

        OutState.ReconstitutionProjectId =
            ParseDomainEntityId(
                DomainColumnText(
                    Statement,
                    4));
        OutState.StateJson =
            DomainColumnText(
                Statement,
                5);

        if (!IsValidDomainStateName(
                OutState.DomainState))
        {
            OutError =
                TEXT("Stored Territory Domain-heart state is invalid.");
            sqlite3_finalize(Statement);
            return false;
        }
    }
    else if (Step != SQLITE_DONE)
    {
        OutError =
            LastError(TEXT("Read Territory Domain-heart state"));
        sqlite3_finalize(Statement);
        return false;
    }

    sqlite3_finalize(Statement);
    return true;
}

bool FOGSQLiteWorldStore::UpsertDomainCoreConcept(
    const FOGDomainCoreConceptRecord& Concept,
    FString& OutError)
{
    OutError.Reset();

    if (!Concept.CoreId.IsValid() ||
        !Concept.ConceptId.IsValid() ||
        Concept.Grade < 0)
    {
        OutError =
            TEXT("Domain Core Concept record is invalid.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "INSERT INTO domain_core_concepts("
        "core_entity_id, concept_content_id, grade, "
        "origin_source_core_entity_id, synthesis_rule_content_id, state_json"
        ") VALUES(?, ?, ?, ?, ?, ?) "
        "ON CONFLICT(core_entity_id, concept_content_id) DO UPDATE SET "
        "grade = excluded.grade, "
        "origin_source_core_entity_id = excluded.origin_source_core_entity_id, "
        "synthesis_rule_content_id = excluded.synthesis_rule_content_id, "
        "state_json = excluded.state_json;";

    if (sqlite3_prepare_v2(
            Database,
            Sql,
            -1,
            &Statement,
            nullptr) != SQLITE_OK)
    {
        OutError =
            LastError(TEXT("Prepare Domain Core Concept upsert"));
        return false;
    }

    const bool bBound =
        BindDomainText(
            Statement,
            1,
            Concept.CoreId.ToString()) &&
        BindDomainText(
            Statement,
            2,
            Concept.ConceptId.ToString()) &&
        sqlite3_bind_int(
            Statement,
            3,
            Concept.Grade) == SQLITE_OK &&
        BindOptionalDomainEntityId(
            Statement,
            4,
            Concept.OriginSourceCoreId) &&
        BindOptionalDomainContentId(
            Statement,
            5,
            Concept.SynthesisRuleId) &&
        BindDomainText(
            Statement,
            6,
            Concept.StateJson.IsEmpty()
                ? TEXT("{}")
                : Concept.StateJson);

    const bool bSucceeded =
        bBound &&
        sqlite3_step(Statement) ==
            SQLITE_DONE;
    if (!bSucceeded)
    {
        OutError =
            LastError(TEXT("Upsert Domain Core Concept"));
    }

    sqlite3_finalize(Statement);
    return bSucceeded;
}

bool FOGSQLiteWorldStore::ListDomainCoreConcepts(
    const FOGEntityId& CoreId,
    TArray<FOGDomainCoreConceptRecord>& OutConcepts,
    FString& OutError) const
{
    OutConcepts.Reset();
    OutError.Reset();

    if (!CoreId.IsValid())
    {
        OutError =
            TEXT("Domain Core Concept list requires a valid Core ID.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "SELECT concept_content_id, grade, origin_source_core_entity_id, "
        "synthesis_rule_content_id, state_json "
        "FROM domain_core_concepts WHERE core_entity_id = ? "
        "ORDER BY concept_content_id ASC;";

    if (sqlite3_prepare_v2(
            Database,
            Sql,
            -1,
            &Statement,
            nullptr) != SQLITE_OK)
    {
        OutError =
            LastError(TEXT("Prepare Domain Core Concept list"));
        return false;
    }

    if (!BindDomainText(
            Statement,
            1,
            CoreId.ToString()))
    {
        OutError =
            LastError(TEXT("Bind Domain Core Concept list"));
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
                LastError(TEXT("Read Domain Core Concept list"));
            sqlite3_finalize(Statement);
            return false;
        }

        FOGDomainCoreConceptRecord Concept;
        Concept.CoreId =
            CoreId;
        Concept.ConceptId =
            FOGContentId(
                DomainColumnText(
                    Statement,
                    0));
        Concept.Grade =
            sqlite3_column_int(
                Statement,
                1);
        Concept.OriginSourceCoreId =
            ParseDomainEntityId(
                DomainColumnText(
                    Statement,
                    2));

        const FString RuleId =
            DomainColumnText(
                Statement,
                3);
        if (!RuleId.IsEmpty())
        {
            Concept.SynthesisRuleId =
                FOGContentId(RuleId);
        }

        Concept.StateJson =
            DomainColumnText(
                Statement,
                4);

        if (!Concept.ConceptId.IsValid() ||
            Concept.Grade < 0)
        {
            OutError =
                TEXT("Stored Domain Core Concept is invalid.");
            sqlite3_finalize(Statement);
            return false;
        }

        OutConcepts.Add(MoveTemp(Concept));
    }

    sqlite3_finalize(Statement);
    return true;
}

bool FOGSQLiteWorldStore::UpsertDomainCoreFusion(
    const FOGDomainCoreFusionRecord& Fusion,
    int64 CreatedWorldTick,
    FString& OutError)
{
    OutError.Reset();

    if (!Fusion.FusionId.IsValid() ||
        !Fusion.ResultCoreId.IsValid() ||
        !Fusion.AbsorberCoreId.IsValid() ||
        !Fusion.AbsorbedCoreId.IsValid() ||
        Fusion.AbsorberCoreId ==
            Fusion.AbsorbedCoreId ||
        Fusion.FusionWorldTick < 0 ||
        Fusion.SequenceOrdinal < 0 ||
        Fusion.OutcomeKind.IsNone())
    {
        OutError =
            TEXT("Domain Core fusion record is invalid.");
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
            Fusion.FusionId,
            TEXT("domain_core_fusion"),
            CreatedWorldTick,
            TEXT("{}"),
            EntityError))
    {
        return Fail(EntityError);
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "INSERT INTO domain_core_fusions("
        "fusion_entity_id, result_core_entity_id, absorber_core_entity_id, "
        "absorbed_core_entity_id, fusion_world_tick, sequence_ordinal, "
        "synthesis_rule_content_id, outcome_kind, resolution_seed, "
        "instability_state_json"
        ") VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?) "
        "ON CONFLICT(fusion_entity_id) DO UPDATE SET "
        "result_core_entity_id = excluded.result_core_entity_id, "
        "absorber_core_entity_id = excluded.absorber_core_entity_id, "
        "absorbed_core_entity_id = excluded.absorbed_core_entity_id, "
        "fusion_world_tick = excluded.fusion_world_tick, "
        "sequence_ordinal = excluded.sequence_ordinal, "
        "synthesis_rule_content_id = excluded.synthesis_rule_content_id, "
        "outcome_kind = excluded.outcome_kind, "
        "resolution_seed = excluded.resolution_seed, "
        "instability_state_json = excluded.instability_state_json;";

    if (sqlite3_prepare_v2(
            Database,
            Sql,
            -1,
            &Statement,
            nullptr) != SQLITE_OK)
    {
        return Fail(
            LastError(TEXT("Prepare Domain Core fusion upsert")));
    }

    const bool bBound =
        BindDomainText(
            Statement,
            1,
            Fusion.FusionId.ToString()) &&
        BindDomainText(
            Statement,
            2,
            Fusion.ResultCoreId.ToString()) &&
        BindDomainText(
            Statement,
            3,
            Fusion.AbsorberCoreId.ToString()) &&
        BindDomainText(
            Statement,
            4,
            Fusion.AbsorbedCoreId.ToString()) &&
        sqlite3_bind_int64(
            Statement,
            5,
            Fusion.FusionWorldTick) == SQLITE_OK &&
        sqlite3_bind_int(
            Statement,
            6,
            Fusion.SequenceOrdinal) == SQLITE_OK &&
        BindOptionalDomainContentId(
            Statement,
            7,
            Fusion.SynthesisRuleId) &&
        BindDomainText(
            Statement,
            8,
            Fusion.OutcomeKind.ToString()) &&
        sqlite3_bind_int64(
            Statement,
            9,
            Fusion.ResolutionSeed) == SQLITE_OK &&
        BindDomainText(
            Statement,
            10,
            Fusion.InstabilityStateJson.IsEmpty()
                ? TEXT("{}")
                : Fusion.InstabilityStateJson);

    const bool bSucceeded =
        bBound &&
        sqlite3_step(Statement) ==
            SQLITE_DONE;
    const FString SqlError =
        bSucceeded
            ? FString()
            : LastError(TEXT("Upsert Domain Core fusion"));
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

bool FOGSQLiteWorldStore::ListDomainCoreFusionsForResult(
    const FOGEntityId& ResultCoreId,
    TArray<FOGDomainCoreFusionRecord>& OutFusions,
    FString& OutError) const
{
    OutFusions.Reset();
    OutError.Reset();

    if (!ResultCoreId.IsValid())
    {
        OutError =
            TEXT("Domain Core fusion list requires a valid result Core ID.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "SELECT fusion_entity_id, absorber_core_entity_id, absorbed_core_entity_id, "
        "fusion_world_tick, sequence_ordinal, synthesis_rule_content_id, "
        "outcome_kind, resolution_seed, instability_state_json "
        "FROM domain_core_fusions WHERE result_core_entity_id = ? "
        "ORDER BY sequence_ordinal ASC, fusion_entity_id ASC;";

    if (sqlite3_prepare_v2(
            Database,
            Sql,
            -1,
            &Statement,
            nullptr) != SQLITE_OK)
    {
        OutError =
            LastError(TEXT("Prepare Domain Core fusion list"));
        return false;
    }

    if (!BindDomainText(
            Statement,
            1,
            ResultCoreId.ToString()))
    {
        OutError =
            LastError(TEXT("Bind Domain Core fusion list"));
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
                LastError(TEXT("Read Domain Core fusion list"));
            sqlite3_finalize(Statement);
            return false;
        }

        FOGDomainCoreFusionRecord Fusion;
        Fusion.FusionId =
            ParseDomainEntityId(
                DomainColumnText(
                    Statement,
                    0));
        Fusion.ResultCoreId =
            ResultCoreId;
        Fusion.AbsorberCoreId =
            ParseDomainEntityId(
                DomainColumnText(
                    Statement,
                    1));
        Fusion.AbsorbedCoreId =
            ParseDomainEntityId(
                DomainColumnText(
                    Statement,
                    2));
        Fusion.FusionWorldTick =
            sqlite3_column_int64(
                Statement,
                3);
        Fusion.SequenceOrdinal =
            sqlite3_column_int(
                Statement,
                4);

        const FString RuleId =
            DomainColumnText(
                Statement,
                5);
        if (!RuleId.IsEmpty())
        {
            Fusion.SynthesisRuleId =
                FOGContentId(RuleId);
        }

        Fusion.OutcomeKind =
            FName(*DomainColumnText(
                Statement,
                6));
        Fusion.ResolutionSeed =
            sqlite3_column_int64(
                Statement,
                7);
        Fusion.InstabilityStateJson =
            DomainColumnText(
                Statement,
                8);

        if (!Fusion.FusionId.IsValid() ||
            !Fusion.AbsorberCoreId.IsValid() ||
            !Fusion.AbsorbedCoreId.IsValid() ||
            Fusion.OutcomeKind.IsNone())
        {
            OutError =
                TEXT("Stored Domain Core fusion is invalid.");
            sqlite3_finalize(Statement);
            return false;
        }

        OutFusions.Add(MoveTemp(Fusion));
    }

    sqlite3_finalize(Statement);
    return true;
}

bool FOGSQLiteWorldStore::UpsertDomainCoreLineage(
    const FOGDomainCoreLineageRecord& Lineage,
    FString& OutError)
{
    OutError.Reset();

    if (!Lineage.ResultCoreId.IsValid() ||
        !Lineage.SourceCoreId.IsValid() ||
        !Lineage.FusionId.IsValid() ||
        (Lineage.LineageRole !=
             FName(TEXT("absorber")) &&
         Lineage.LineageRole !=
             FName(TEXT("absorbed"))))
    {
        OutError =
            TEXT("Domain Core lineage record is invalid.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "INSERT INTO domain_core_lineage("
        "result_core_entity_id, source_core_entity_id, fusion_entity_id, lineage_role"
        ") VALUES(?, ?, ?, ?) "
        "ON CONFLICT(result_core_entity_id, source_core_entity_id, fusion_entity_id) "
        "DO UPDATE SET lineage_role = excluded.lineage_role;";

    if (sqlite3_prepare_v2(
            Database,
            Sql,
            -1,
            &Statement,
            nullptr) != SQLITE_OK)
    {
        OutError =
            LastError(TEXT("Prepare Domain Core lineage upsert"));
        return false;
    }

    const bool bBound =
        BindDomainText(
            Statement,
            1,
            Lineage.ResultCoreId.ToString()) &&
        BindDomainText(
            Statement,
            2,
            Lineage.SourceCoreId.ToString()) &&
        BindDomainText(
            Statement,
            3,
            Lineage.FusionId.ToString()) &&
        BindDomainText(
            Statement,
            4,
            Lineage.LineageRole.ToString());

    const bool bSucceeded =
        bBound &&
        sqlite3_step(Statement) ==
            SQLITE_DONE;
    if (!bSucceeded)
    {
        OutError =
            LastError(TEXT("Upsert Domain Core lineage"));
    }

    sqlite3_finalize(Statement);
    return bSucceeded;
}

bool FOGSQLiteWorldStore::ListDomainCoreLineage(
    const FOGEntityId& ResultCoreId,
    TArray<FOGDomainCoreLineageRecord>& OutLineage,
    FString& OutError) const
{
    OutLineage.Reset();
    OutError.Reset();

    if (!ResultCoreId.IsValid())
    {
        OutError =
            TEXT("Domain Core lineage list requires a valid result Core ID.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "SELECT source_core_entity_id, fusion_entity_id, lineage_role "
        "FROM domain_core_lineage WHERE result_core_entity_id = ? "
        "ORDER BY fusion_entity_id ASC, source_core_entity_id ASC;";

    if (sqlite3_prepare_v2(
            Database,
            Sql,
            -1,
            &Statement,
            nullptr) != SQLITE_OK)
    {
        OutError =
            LastError(TEXT("Prepare Domain Core lineage list"));
        return false;
    }

    if (!BindDomainText(
            Statement,
            1,
            ResultCoreId.ToString()))
    {
        OutError =
            LastError(TEXT("Bind Domain Core lineage list"));
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
                LastError(TEXT("Read Domain Core lineage list"));
            sqlite3_finalize(Statement);
            return false;
        }

        FOGDomainCoreLineageRecord Lineage;
        Lineage.ResultCoreId =
            ResultCoreId;
        Lineage.SourceCoreId =
            ParseDomainEntityId(
                DomainColumnText(
                    Statement,
                    0));
        Lineage.FusionId =
            ParseDomainEntityId(
                DomainColumnText(
                    Statement,
                    1));
        Lineage.LineageRole =
            FName(*DomainColumnText(
                Statement,
                2));

        if (!Lineage.SourceCoreId.IsValid() ||
            !Lineage.FusionId.IsValid())
        {
            OutError =
                TEXT("Stored Domain Core lineage is invalid.");
            sqlite3_finalize(Statement);
            return false;
        }

        OutLineage.Add(MoveTemp(Lineage));
    }

    sqlite3_finalize(Statement);
    return true;
}

bool FOGSQLiteWorldStore::MigrateDomainHeartAndConcepts0009(
    FString& OutError)
{
    OutError.Reset();

    if (!ExecuteSql(
            TEXT("INSERT OR IGNORE INTO domain_core_concepts(")
            TEXT("core_entity_id, concept_content_id, grade, ")
            TEXT("origin_source_core_entity_id, synthesis_rule_content_id, state_json) ")
            TEXT("SELECT core_entity_id, aspect_content_id, grade, core_entity_id, NULL, '{}' ")
            TEXT("FROM domain_core_aspects;"),
            OutError))
    {
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "SELECT t.territory_entity_id, dc.core_entity_id, dc.lifecycle, "
        "dc.durability_sig, dc.durability_exp, "
        "dc.max_durability_sig, dc.max_durability_exp "
        "FROM territories t "
        "LEFT JOIN domain_cores dc ON dc.territory_entity_id = t.territory_entity_id "
        "ORDER BY t.territory_entity_id ASC;";

    if (sqlite3_prepare_v2(
            Database,
            Sql,
            -1,
            &Statement,
            nullptr) != SQLITE_OK)
    {
        OutError =
            LastError(TEXT("Prepare migration 0009 Domain-heart scan"));
        return false;
    }

    TArray<FOGTerritoryDomainStateRecord> States;

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
                LastError(TEXT("Read migration 0009 Domain-heart scan"));
            sqlite3_finalize(Statement);
            return false;
        }

        const FOGEntityId TerritoryId =
            ParseDomainEntityId(
                DomainColumnText(
                    Statement,
                    0));
        if (!TerritoryId.IsValid())
        {
            OutError =
                TEXT("Migration 0009 found an invalid Territory ID.");
            sqlite3_finalize(Statement);
            return false;
        }

        FOGTerritoryDomainStateRecord State;
        State.TerritoryId =
            TerritoryId;

        if (sqlite3_column_type(
                Statement,
                1) == SQLITE_NULL)
        {
            State.DomainState =
                FName(TEXT("none"));
            States.Add(MoveTemp(State));
            continue;
        }

        const FOGEntityId CoreId =
            ParseDomainEntityId(
                DomainColumnText(
                    Statement,
                    1));
        if (!CoreId.IsValid())
        {
            OutError =
                TEXT("Migration 0009 found an invalid Domain Core ID.");
            sqlite3_finalize(Statement);
            return false;
        }

        const EOGDomainCoreLifecycle Lifecycle =
            static_cast<EOGDomainCoreLifecycle>(
                sqlite3_column_int(
                    Statement,
                    2));

        if (Lifecycle ==
            EOGDomainCoreLifecycle::Awakened)
        {
            const FOGLargeNumber Current(
                sqlite3_column_int64(
                    Statement,
                    3),
                sqlite3_column_int(
                    Statement,
                    4));
            const FOGLargeNumber Maximum(
                sqlite3_column_int64(
                    Statement,
                    5),
                sqlite3_column_int(
                    Statement,
                    6));

            State.ActiveCoreId =
                CoreId;
            State.DomainState =
                FOGLargeNumber::Compare(
                    Current,
                    Maximum) < 0
                    ? FName(TEXT("damaged"))
                    : FName(TEXT("functional"));
        }
        else if (Lifecycle ==
                     EOGDomainCoreLifecycle::Dormant)
        {
            // A Core becomes the Territory's heart only once awakened.
            State.DomainState =
                FName(TEXT("none"));
        }
        else
        {
            // Broken/Absorbed legacy Cores imply the heart is gone. Recover the
            // exact loss tick only when historical evidence exists; do not
            // fabricate one from Core creation time.
            State.DomainState =
                FName(TEXT("heart_lost_ruining"));

            bool bLossTickFound = false;
            int64 LossTick = 0;
            if (!ReadCoreLossEventTick(
                    Database,
                    CoreId,
                    bLossTickFound,
                    LossTick,
                    OutError))
            {
                sqlite3_finalize(Statement);
                return false;
            }

            if (bLossTickFound)
            {
                State.bHasHeartLostWorldTick =
                    true;
                State.HeartLostWorldTick =
                    LossTick;
                State.bHasRuinStartedWorldTick =
                    true;
                State.RuinStartedWorldTick =
                    LossTick;
            }
            else
            {
                State.StateJson =
                    TEXT("{\"migration_loss_tick\":\"unknown\"}");
            }
        }

        States.Add(MoveTemp(State));
    }

    sqlite3_finalize(Statement);

    for (const FOGTerritoryDomainStateRecord& State :
         States)
    {
        if (!UpsertTerritoryDomainState(
                State,
                OutError))
        {
            return false;
        }
    }

    return true;
}

bool FOGSQLiteWorldStore::ValidateDomainHeartMigration0009(
    FString& OutError) const
{
    OutError.Reset();

    int32 MissingTerritoryStates = 0;
    if (!ReadDomainCount(
            Database,
            "SELECT COUNT(*) FROM territories t "
            "LEFT JOIN territory_domain_state ds "
            "ON ds.territory_entity_id = t.territory_entity_id "
            "WHERE ds.territory_entity_id IS NULL;",
            MissingTerritoryStates,
            OutError))
    {
        return false;
    }

    if (MissingTerritoryStates != 0)
    {
        OutError = FString::Printf(
            TEXT("Migration 0009 left %d Territories without Domain-heart state."),
            MissingTerritoryStates);
        return false;
    }

    int32 MissingConceptProjection = 0;
    if (!ReadDomainCount(
            Database,
            "SELECT COUNT(*) FROM domain_core_aspects a "
            "LEFT JOIN domain_core_concepts c "
            "ON c.core_entity_id = a.core_entity_id "
            "AND c.concept_content_id = a.aspect_content_id "
            "WHERE c.core_entity_id IS NULL;",
            MissingConceptProjection,
            OutError))
    {
        return false;
    }

    if (MissingConceptProjection != 0)
    {
        OutError = FString::Printf(
            TEXT("Migration 0009 left %d legacy Core Aspects without Concept projection."),
            MissingConceptProjection);
        return false;
    }

    int32 InvalidActiveHearts = 0;
    if (!ReadDomainCount(
            Database,
            "SELECT COUNT(*) FROM territory_domain_state ds "
            "LEFT JOIN domain_cores dc ON dc.core_entity_id = ds.active_core_entity_id "
            "WHERE ds.domain_state IN ('functional','damaged') "
            "AND (ds.active_core_entity_id IS NULL "
            "OR dc.core_entity_id IS NULL "
            "OR dc.lifecycle IN (2,3));",
            InvalidActiveHearts,
            OutError))
    {
        return false;
    }

    if (InvalidActiveHearts != 0)
    {
        OutError = FString::Printf(
            TEXT("Migration 0009 produced %d invalid active Domain hearts."),
            InvalidActiveHearts);
        return false;
    }

    int32 InvalidLostHearts = 0;
    if (!ReadDomainCount(
            Database,
            "SELECT COUNT(*) FROM territory_domain_state "
            "WHERE domain_state IN ('heart_lost_ruining','ruined') "
            "AND active_core_entity_id IS NOT NULL;",
            InvalidLostHearts,
            OutError))
    {
        return false;
    }

    if (InvalidLostHearts != 0)
    {
        OutError = FString::Printf(
            TEXT("Migration 0009 produced %d lost-heart states that still have an active Core."),
            InvalidLostHearts);
        return false;
    }

    return true;
}
