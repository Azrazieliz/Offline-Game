#include "Persistence/OGSQLiteWorldStore.h"

#include "Misc/SecureHash.h"
#include "sqlite/sqlite3.h"

namespace
{
bool BindSovereigntyText(
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

FString SovereigntyColumnText(
    sqlite3_stmt* Statement,
    int32 Column)
{
    const unsigned char* Text =
        sqlite3_column_text(Statement, Column);
    return Text
        ? UTF8_TO_TCHAR(
            reinterpret_cast<const char*>(Text))
        : FString();
}

FOGEntityId ParseSovereigntyEntityId(
    const FString& Value)
{
    FGuid Guid;
    return FGuid::Parse(Value, Guid)
        ? FOGEntityId(Guid)
        : FOGEntityId();
}

bool BindOptionalSovereigntyTick(
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

uint32 ReadSovereigntyDigestWord(
    const uint8* Digest)
{
    return
        (static_cast<uint32>(Digest[0]) << 24) |
        (static_cast<uint32>(Digest[1]) << 16) |
        (static_cast<uint32>(Digest[2]) << 8) |
        static_cast<uint32>(Digest[3]);
}

FOGEntityId StableSovereigntyMigrationId(
    const FOGEntityId& TerritoryId,
    const FOGEntityId& RulerId)
{
    const FString Material = FString::Printf(
        TEXT("offlinegame:migration0008:claim:%s:%s"),
        *TerritoryId.ToString(),
        *RulerId.ToString());

    FTCHARToUTF8 Utf8(*Material);
    FMD5 Md5;
    Md5.Update(
        reinterpret_cast<const uint8*>(Utf8.Get()),
        Utf8.Length());

    uint8 Digest[16];
    Md5.Final(Digest);

    FGuid Guid(
        ReadSovereigntyDigestWord(Digest),
        ReadSovereigntyDigestWord(Digest + 4),
        ReadSovereigntyDigestWord(Digest + 8),
        ReadSovereigntyDigestWord(Digest + 12));

    if (!Guid.IsValid())
    {
        Guid.D = 1;
    }

    return FOGEntityId(Guid);
}

bool IsEffectiveControlState(FName State)
{
    return State == FName(TEXT("effective")) ||
        State == FName(TEXT("controlled"));
}

bool ReadValidationCount(
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
            TEXT("Failed to prepare migration 0008 validation query.");
        return false;
    }

    const int32 Step =
        sqlite3_step(Statement);
    if (Step != SQLITE_ROW)
    {
        OutError =
            TEXT("Failed to execute migration 0008 validation query.");
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

bool ReadEarliestLegacyGachaTick(
    sqlite3* Database,
    const FOGEntityId& RulerId,
    bool& bOutFound,
    int64& OutWorldTick,
    FString& OutError)
{
    bOutFound = false;
    OutWorldTick = 0;

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "SELECT MIN(world_tick) FROM world_events "
        "WHERE event_type = 'gacha_pull' AND primary_entity_id = ?;";

    if (sqlite3_prepare_v2(
            Database,
            Sql,
            -1,
            &Statement,
            nullptr) != SQLITE_OK)
    {
        OutError =
            TEXT("Failed to prepare legacy gacha-event migration query.");
        return false;
    }

    if (!BindSovereigntyText(
            Statement,
            1,
            RulerId.ToString()))
    {
        OutError =
            TEXT("Failed to bind legacy gacha-event migration query.");
        sqlite3_finalize(Statement);
        return false;
    }

    if (sqlite3_step(Statement) != SQLITE_ROW)
    {
        OutError =
            TEXT("Failed to execute legacy gacha-event migration query.");
        sqlite3_finalize(Statement);
        return false;
    }

    if (sqlite3_column_type(
            Statement,
            0) != SQLITE_NULL)
    {
        bOutFound = true;
        OutWorldTick =
            sqlite3_column_int64(
                Statement,
                0);
    }

    sqlite3_finalize(Statement);

    if (bOutFound)
    {
        return true;
    }

    Statement = nullptr;
    const char* StateSql =
        "SELECT MIN(updated_world_tick) FROM gacha_states "
        "WHERE ruler_entity_id = ?;";

    if (sqlite3_prepare_v2(
            Database,
            StateSql,
            -1,
            &Statement,
            nullptr) != SQLITE_OK)
    {
        OutError =
            TEXT("Failed to prepare legacy gacha-state migration query.");
        return false;
    }

    if (!BindSovereigntyText(
            Statement,
            1,
            RulerId.ToString()))
    {
        OutError =
            TEXT("Failed to bind legacy gacha-state migration query.");
        sqlite3_finalize(Statement);
        return false;
    }

    if (sqlite3_step(Statement) != SQLITE_ROW)
    {
        OutError =
            TEXT("Failed to execute legacy gacha-state migration query.");
        sqlite3_finalize(Statement);
        return false;
    }

    if (sqlite3_column_type(
            Statement,
            0) != SQLITE_NULL)
    {
        bOutFound = true;
        OutWorldTick =
            sqlite3_column_int64(
                Statement,
                0);
    }

    sqlite3_finalize(Statement);
    return true;
}
}

bool FOGSQLiteWorldStore::UpsertLocationTerritory(
    const FOGLocationTerritoryRecord& Relation,
    FString& OutError)
{
    OutError.Reset();

    if (!Relation.LocationId.IsValid() ||
        !Relation.TerritoryId.IsValid() ||
        Relation.RelationKind.IsNone() ||
        Relation.CoverageBps < 0 ||
        Relation.CoverageBps > 10000)
    {
        OutError =
            TEXT("Location/Territory relation is invalid.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "INSERT INTO location_territories("
        "location_entity_id, territory_entity_id, relation_kind, coverage_bps"
        ") VALUES(?, ?, ?, ?) "
        "ON CONFLICT(location_entity_id, territory_entity_id) DO UPDATE SET "
        "relation_kind = excluded.relation_kind, "
        "coverage_bps = excluded.coverage_bps;";

    if (sqlite3_prepare_v2(
            Database,
            Sql,
            -1,
            &Statement,
            nullptr) != SQLITE_OK)
    {
        OutError =
            LastError(TEXT("Prepare Location/Territory upsert"));
        return false;
    }

    const bool bBound =
        BindSovereigntyText(
            Statement,
            1,
            Relation.LocationId.ToString()) &&
        BindSovereigntyText(
            Statement,
            2,
            Relation.TerritoryId.ToString()) &&
        BindSovereigntyText(
            Statement,
            3,
            Relation.RelationKind.ToString()) &&
        sqlite3_bind_int(
            Statement,
            4,
            Relation.CoverageBps) == SQLITE_OK;

    const bool bSucceeded =
        bBound &&
        sqlite3_step(Statement) == SQLITE_DONE;

    if (!bSucceeded)
    {
        OutError =
            LastError(TEXT("Upsert Location/Territory relation"));
    }

    sqlite3_finalize(Statement);
    return bSucceeded;
}

bool FOGSQLiteWorldStore::UpsertTerritoryClaim(
    const FOGTerritoryClaimRecord& Claim,
    int64 CreatedWorldTick,
    FString& OutError)
{
    OutError.Reset();

    if (!Claim.ClaimId.IsValid() ||
        !Claim.TerritoryId.IsValid() ||
        !Claim.RulerId.IsValid() ||
        Claim.ClaimKind.IsNone() ||
        Claim.ControlState.IsNone() ||
        Claim.ControlStrengthBps < 0 ||
        Claim.ControlStrengthBps > 10000 ||
        Claim.ClaimStartWorldTick < 0 ||
        Claim.UpdatedWorldTick < Claim.ClaimStartWorldTick ||
        (Claim.bHasReclaimDeadline &&
         (!Claim.bHasDisplacedWorldTick ||
          Claim.ReclaimDeadlineWorldTick <
              Claim.DisplacedWorldTick)))
    {
        OutError =
            TEXT("Territory claim record is invalid.");
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
            Claim.ClaimId,
            TEXT("territory_claim"),
            CreatedWorldTick,
            TEXT("{}"),
            Error))
    {
        return Fail(Error);
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "INSERT INTO territory_claims("
        "claim_entity_id, territory_entity_id, ruler_entity_id, claim_kind, "
        "control_state, control_strength_bps, claim_start_world_tick, "
        "effective_control_start_world_tick, displaced_world_tick, "
        "reclaim_deadline_world_tick, updated_world_tick, state_json"
        ") VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?) "
        "ON CONFLICT(claim_entity_id) DO UPDATE SET "
        "territory_entity_id = excluded.territory_entity_id, "
        "ruler_entity_id = excluded.ruler_entity_id, "
        "claim_kind = excluded.claim_kind, "
        "control_state = excluded.control_state, "
        "control_strength_bps = excluded.control_strength_bps, "
        "claim_start_world_tick = excluded.claim_start_world_tick, "
        "effective_control_start_world_tick = excluded.effective_control_start_world_tick, "
        "displaced_world_tick = excluded.displaced_world_tick, "
        "reclaim_deadline_world_tick = excluded.reclaim_deadline_world_tick, "
        "updated_world_tick = excluded.updated_world_tick, "
        "state_json = excluded.state_json;";

    if (sqlite3_prepare_v2(
            Database,
            Sql,
            -1,
            &Statement,
            nullptr) != SQLITE_OK)
    {
        return Fail(
            LastError(TEXT("Prepare Territory claim upsert")));
    }

    const bool bBound =
        BindSovereigntyText(
            Statement,
            1,
            Claim.ClaimId.ToString()) &&
        BindSovereigntyText(
            Statement,
            2,
            Claim.TerritoryId.ToString()) &&
        BindSovereigntyText(
            Statement,
            3,
            Claim.RulerId.ToString()) &&
        BindSovereigntyText(
            Statement,
            4,
            Claim.ClaimKind.ToString()) &&
        BindSovereigntyText(
            Statement,
            5,
            Claim.ControlState.ToString()) &&
        sqlite3_bind_int(
            Statement,
            6,
            Claim.ControlStrengthBps) == SQLITE_OK &&
        sqlite3_bind_int64(
            Statement,
            7,
            Claim.ClaimStartWorldTick) == SQLITE_OK &&
        BindOptionalSovereigntyTick(
            Statement,
            8,
            Claim.bHasEffectiveControlStart,
            Claim.EffectiveControlStartWorldTick) &&
        BindOptionalSovereigntyTick(
            Statement,
            9,
            Claim.bHasDisplacedWorldTick,
            Claim.DisplacedWorldTick) &&
        BindOptionalSovereigntyTick(
            Statement,
            10,
            Claim.bHasReclaimDeadline,
            Claim.ReclaimDeadlineWorldTick) &&
        sqlite3_bind_int64(
            Statement,
            11,
            Claim.UpdatedWorldTick) == SQLITE_OK &&
        BindSovereigntyText(
            Statement,
            12,
            Claim.StateJson.IsEmpty()
                ? TEXT("{}")
                : Claim.StateJson);

    const bool bSucceeded =
        bBound &&
        sqlite3_step(Statement) == SQLITE_DONE;
    const FString SqlError =
        bSucceeded
            ? FString()
            : LastError(TEXT("Upsert Territory claim"));
    sqlite3_finalize(Statement);

    if (!bSucceeded)
    {
        return Fail(SqlError);
    }

    if (bOwnTransaction &&
        !CommitTransaction(OutError))
    {
        FString RollbackError;
        RollbackTransaction(RollbackError);
        return false;
    }

    return true;
}

bool FOGSQLiteWorldStore::TryReadTerritoryClaim(
    const FOGEntityId& ClaimId,
    bool& bOutFound,
    FOGTerritoryClaimRecord& OutClaim,
    FString& OutError) const
{
    bOutFound = false;
    OutClaim = FOGTerritoryClaimRecord();
    OutError.Reset();

    if (!ClaimId.IsValid())
    {
        OutError =
            TEXT("Territory claim ID is invalid.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "SELECT territory_entity_id, ruler_entity_id, claim_kind, control_state, "
        "control_strength_bps, claim_start_world_tick, "
        "effective_control_start_world_tick, displaced_world_tick, "
        "reclaim_deadline_world_tick, updated_world_tick, state_json "
        "FROM territory_claims WHERE claim_entity_id = ?;";

    if (sqlite3_prepare_v2(
            Database,
            Sql,
            -1,
            &Statement,
            nullptr) != SQLITE_OK)
    {
        OutError =
            LastError(TEXT("Prepare Territory claim read"));
        return false;
    }

    if (!BindSovereigntyText(
            Statement,
            1,
            ClaimId.ToString()))
    {
        OutError =
            LastError(TEXT("Bind Territory claim read"));
        sqlite3_finalize(Statement);
        return false;
    }

    const int32 Step =
        sqlite3_step(Statement);

    if (Step == SQLITE_ROW)
    {
        const FOGEntityId TerritoryId =
            ParseSovereigntyEntityId(
                SovereigntyColumnText(
                    Statement,
                    0));
        const FOGEntityId RulerId =
            ParseSovereigntyEntityId(
                SovereigntyColumnText(
                    Statement,
                    1));

        if (!TerritoryId.IsValid() ||
            !RulerId.IsValid())
        {
            OutError =
                TEXT("Stored Territory claim contains invalid entity IDs.");
            sqlite3_finalize(Statement);
            return false;
        }

        bOutFound = true;
        OutClaim.ClaimId = ClaimId;
        OutClaim.TerritoryId = TerritoryId;
        OutClaim.RulerId = RulerId;
        OutClaim.ClaimKind =
            FName(*SovereigntyColumnText(
                Statement,
                2));
        OutClaim.ControlState =
            FName(*SovereigntyColumnText(
                Statement,
                3));
        OutClaim.ControlStrengthBps =
            sqlite3_column_int(
                Statement,
                4);
        OutClaim.ClaimStartWorldTick =
            sqlite3_column_int64(
                Statement,
                5);

        if (sqlite3_column_type(
                Statement,
                6) != SQLITE_NULL)
        {
            OutClaim.bHasEffectiveControlStart = true;
            OutClaim.EffectiveControlStartWorldTick =
                sqlite3_column_int64(
                    Statement,
                    6);
        }

        if (sqlite3_column_type(
                Statement,
                7) != SQLITE_NULL)
        {
            OutClaim.bHasDisplacedWorldTick = true;
            OutClaim.DisplacedWorldTick =
                sqlite3_column_int64(
                    Statement,
                    7);
        }

        if (sqlite3_column_type(
                Statement,
                8) != SQLITE_NULL)
        {
            OutClaim.bHasReclaimDeadline = true;
            OutClaim.ReclaimDeadlineWorldTick =
                sqlite3_column_int64(
                    Statement,
                    8);
        }

        OutClaim.UpdatedWorldTick =
            sqlite3_column_int64(
                Statement,
                9);
        OutClaim.StateJson =
            SovereigntyColumnText(
                Statement,
                10);
    }
    else if (Step != SQLITE_DONE)
    {
        OutError =
            LastError(TEXT("Read Territory claim"));
        sqlite3_finalize(Statement);
        return false;
    }

    sqlite3_finalize(Statement);
    return true;
}

namespace
{
bool ReadClaimIds(
    sqlite3* Database,
    const char* Sql,
    const TArray<FString>& BindValues,
    TArray<FOGEntityId>& OutIds,
    FString& OutError)
{
    OutIds.Reset();

    sqlite3_stmt* Statement = nullptr;
    if (sqlite3_prepare_v2(
            Database,
            Sql,
            -1,
            &Statement,
            nullptr) != SQLITE_OK)
    {
        OutError =
            TEXT("Failed to prepare Territory claim list query.");
        return false;
    }

    for (int32 Index = 0;
         Index < BindValues.Num();
         ++Index)
    {
        if (!BindSovereigntyText(
                Statement,
                Index + 1,
                BindValues[Index]))
        {
            OutError =
                TEXT("Failed to bind Territory claim list query.");
            sqlite3_finalize(Statement);
            return false;
        }
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
                TEXT("Failed to execute Territory claim list query.");
            sqlite3_finalize(Statement);
            return false;
        }

        const FOGEntityId ClaimId =
            ParseSovereigntyEntityId(
                SovereigntyColumnText(
                    Statement,
                    0));
        if (!ClaimId.IsValid())
        {
            OutError =
                TEXT("Territory claim list contains an invalid ID.");
            sqlite3_finalize(Statement);
            return false;
        }

        OutIds.Add(ClaimId);
    }

    sqlite3_finalize(Statement);
    return true;
}
}

bool FOGSQLiteWorldStore::ListTerritoryClaimsByTerritory(
    const FOGEntityId& TerritoryId,
    TArray<FOGTerritoryClaimRecord>& OutClaims,
    FString& OutError) const
{
    OutClaims.Reset();
    OutError.Reset();

    if (!TerritoryId.IsValid())
    {
        OutError =
            TEXT("Territory claim list requires a valid Territory ID.");
        return false;
    }

    TArray<FOGEntityId> Ids;
    if (!ReadClaimIds(
            Database,
            "SELECT claim_entity_id FROM territory_claims "
            "WHERE territory_entity_id = ? "
            "ORDER BY claim_start_world_tick ASC, claim_entity_id ASC;",
            {TerritoryId.ToString()},
            Ids,
            OutError))
    {
        return false;
    }

    for (const FOGEntityId& Id : Ids)
    {
        bool bFound = false;
        FOGTerritoryClaimRecord Claim;
        if (!TryReadTerritoryClaim(
                Id,
                bFound,
                Claim,
                OutError) ||
            !bFound)
        {
            if (OutError.IsEmpty())
            {
                OutError =
                    TEXT("Territory claim disappeared during list read.");
            }
            OutClaims.Reset();
            return false;
        }
        OutClaims.Add(MoveTemp(Claim));
    }

    return true;
}

bool FOGSQLiteWorldStore::ListTerritoryClaimsByRuler(
    const FOGEntityId& RulerId,
    TArray<FOGTerritoryClaimRecord>& OutClaims,
    FString& OutError) const
{
    OutClaims.Reset();
    OutError.Reset();

    if (!RulerId.IsValid())
    {
        OutError =
            TEXT("Ruler claim list requires a valid Ruler ID.");
        return false;
    }

    TArray<FOGEntityId> Ids;
    if (!ReadClaimIds(
            Database,
            "SELECT claim_entity_id FROM territory_claims "
            "WHERE ruler_entity_id = ? "
            "ORDER BY claim_start_world_tick ASC, claim_entity_id ASC;",
            {RulerId.ToString()},
            Ids,
            OutError))
    {
        return false;
    }

    for (const FOGEntityId& Id : Ids)
    {
        bool bFound = false;
        FOGTerritoryClaimRecord Claim;
        if (!TryReadTerritoryClaim(
                Id,
                bFound,
                Claim,
                OutError) ||
            !bFound)
        {
            if (OutError.IsEmpty())
            {
                OutError =
                    TEXT("Ruler claim disappeared during list read.");
            }
            OutClaims.Reset();
            return false;
        }
        OutClaims.Add(MoveTemp(Claim));
    }

    return true;
}

bool FOGSQLiteWorldStore::ListActiveClaimsForLocation(
    const FOGEntityId& LocationId,
    TArray<FOGTerritoryClaimRecord>& OutClaims,
    FString& OutError) const
{
    OutClaims.Reset();
    OutError.Reset();

    if (!LocationId.IsValid())
    {
        OutError =
            TEXT("Location claim list requires a valid Location ID.");
        return false;
    }

    TArray<FOGEntityId> Ids;
    if (!ReadClaimIds(
            Database,
            "SELECT tc.claim_entity_id "
            "FROM location_territories lt "
            "JOIN territory_claims tc "
            "ON tc.territory_entity_id = lt.territory_entity_id "
            "WHERE lt.location_entity_id = ? "
            "AND tc.control_state NOT IN ('lost','expired','withdrawn') "
            "ORDER BY tc.updated_world_tick DESC, tc.claim_entity_id ASC;",
            {LocationId.ToString()},
            Ids,
            OutError))
    {
        return false;
    }

    for (const FOGEntityId& Id : Ids)
    {
        bool bFound = false;
        FOGTerritoryClaimRecord Claim;
        if (!TryReadTerritoryClaim(
                Id,
                bFound,
                Claim,
                OutError) ||
            !bFound)
        {
            if (OutError.IsEmpty())
            {
                OutError =
                    TEXT("Location claim disappeared during list read.");
            }
            OutClaims.Reset();
            return false;
        }
        OutClaims.Add(MoveTemp(Claim));
    }

    return true;
}

bool FOGSQLiteWorldStore::UpsertRulerSovereigntyState(
    const FOGRulerSovereigntyStateRecord& State,
    FString& OutError)
{
    OutError.Reset();

    if (!State.RulerId.IsValid() ||
        State.CurrentTitle.IsNone() ||
        State.HistoricalPeakTitle.IsNone() ||
        State.UpdatedWorldTick < 0)
    {
        OutError =
            TEXT("Ruler sovereignty state is invalid.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "INSERT INTO ruler_sovereignty_state("
        "ruler_entity_id, current_title, historical_peak_title, "
        "continuous_control_start_world_tick, last_effective_control_world_tick, "
        "scope_state_json, updated_world_tick"
        ") VALUES(?, ?, ?, ?, ?, ?, ?) "
        "ON CONFLICT(ruler_entity_id) DO UPDATE SET "
        "current_title = excluded.current_title, "
        "historical_peak_title = excluded.historical_peak_title, "
        "continuous_control_start_world_tick = excluded.continuous_control_start_world_tick, "
        "last_effective_control_world_tick = excluded.last_effective_control_world_tick, "
        "scope_state_json = excluded.scope_state_json, "
        "updated_world_tick = excluded.updated_world_tick;";

    if (sqlite3_prepare_v2(
            Database,
            Sql,
            -1,
            &Statement,
            nullptr) != SQLITE_OK)
    {
        OutError =
            LastError(TEXT("Prepare sovereignty-state upsert"));
        return false;
    }

    const bool bBound =
        BindSovereigntyText(
            Statement,
            1,
            State.RulerId.ToString()) &&
        BindSovereigntyText(
            Statement,
            2,
            State.CurrentTitle.ToString()) &&
        BindSovereigntyText(
            Statement,
            3,
            State.HistoricalPeakTitle.ToString()) &&
        BindOptionalSovereigntyTick(
            Statement,
            4,
            State.bHasContinuousControlStart,
            State.ContinuousControlStartWorldTick) &&
        BindOptionalSovereigntyTick(
            Statement,
            5,
            State.bHasLastEffectiveControlTick,
            State.LastEffectiveControlWorldTick) &&
        BindSovereigntyText(
            Statement,
            6,
            State.ScopeStateJson.IsEmpty()
                ? TEXT("{}")
                : State.ScopeStateJson) &&
        sqlite3_bind_int64(
            Statement,
            7,
            State.UpdatedWorldTick) == SQLITE_OK;

    const bool bSucceeded =
        bBound &&
        sqlite3_step(Statement) == SQLITE_DONE;
    if (!bSucceeded)
    {
        OutError =
            LastError(TEXT("Upsert sovereignty state"));
    }

    sqlite3_finalize(Statement);
    return bSucceeded;
}

bool FOGSQLiteWorldStore::TryReadRulerSovereigntyState(
    const FOGEntityId& RulerId,
    bool& bOutFound,
    FOGRulerSovereigntyStateRecord& OutState,
    FString& OutError) const
{
    bOutFound = false;
    OutState = FOGRulerSovereigntyStateRecord();
    OutError.Reset();

    if (!RulerId.IsValid())
    {
        OutError =
            TEXT("Sovereignty-state lookup requires a valid Ruler ID.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "SELECT current_title, historical_peak_title, "
        "continuous_control_start_world_tick, last_effective_control_world_tick, "
        "scope_state_json, updated_world_tick "
        "FROM ruler_sovereignty_state WHERE ruler_entity_id = ?;";

    if (sqlite3_prepare_v2(
            Database,
            Sql,
            -1,
            &Statement,
            nullptr) != SQLITE_OK)
    {
        OutError =
            LastError(TEXT("Prepare sovereignty-state read"));
        return false;
    }

    if (!BindSovereigntyText(
            Statement,
            1,
            RulerId.ToString()))
    {
        OutError =
            LastError(TEXT("Bind sovereignty-state read"));
        sqlite3_finalize(Statement);
        return false;
    }

    const int32 Step =
        sqlite3_step(Statement);
    if (Step == SQLITE_ROW)
    {
        bOutFound = true;
        OutState.RulerId = RulerId;
        OutState.CurrentTitle =
            FName(*SovereigntyColumnText(
                Statement,
                0));
        OutState.HistoricalPeakTitle =
            FName(*SovereigntyColumnText(
                Statement,
                1));

        if (sqlite3_column_type(
                Statement,
                2) != SQLITE_NULL)
        {
            OutState.bHasContinuousControlStart = true;
            OutState.ContinuousControlStartWorldTick =
                sqlite3_column_int64(
                    Statement,
                    2);
        }

        if (sqlite3_column_type(
                Statement,
                3) != SQLITE_NULL)
        {
            OutState.bHasLastEffectiveControlTick = true;
            OutState.LastEffectiveControlWorldTick =
                sqlite3_column_int64(
                    Statement,
                    3);
        }

        OutState.ScopeStateJson =
            SovereigntyColumnText(
                Statement,
                4);
        OutState.UpdatedWorldTick =
            sqlite3_column_int64(
                Statement,
                5);
    }
    else if (Step != SQLITE_DONE)
    {
        OutError =
            LastError(TEXT("Read sovereignty state"));
        sqlite3_finalize(Statement);
        return false;
    }

    sqlite3_finalize(Statement);
    return true;
}

bool FOGSQLiteWorldStore::UpsertRulerGachaAccess(
    const FOGRulerGachaAccessRecord& Access,
    FString& OutError)
{
    OutError.Reset();

    if (!Access.RulerId.IsValid() ||
        Access.UpdatedWorldTick < 0 ||
        (Access.bPermanentlyUnlocked &&
         !Access.bHasUnlockedWorldTick))
    {
        OutError =
            TEXT("Ruler gacha access state is invalid.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "INSERT INTO ruler_gacha_access("
        "ruler_entity_id, qualification_start_world_tick, "
        "qualification_suspended_world_tick, unlocked_world_tick, "
        "permanently_unlocked, updated_world_tick"
        ") VALUES(?, ?, ?, ?, ?, ?) "
        "ON CONFLICT(ruler_entity_id) DO UPDATE SET "
        "qualification_start_world_tick = excluded.qualification_start_world_tick, "
        "qualification_suspended_world_tick = excluded.qualification_suspended_world_tick, "
        "unlocked_world_tick = excluded.unlocked_world_tick, "
        "permanently_unlocked = excluded.permanently_unlocked, "
        "updated_world_tick = excluded.updated_world_tick;";

    if (sqlite3_prepare_v2(
            Database,
            Sql,
            -1,
            &Statement,
            nullptr) != SQLITE_OK)
    {
        OutError =
            LastError(TEXT("Prepare gacha-access upsert"));
        return false;
    }

    const bool bBound =
        BindSovereigntyText(
            Statement,
            1,
            Access.RulerId.ToString()) &&
        BindOptionalSovereigntyTick(
            Statement,
            2,
            Access.bHasQualificationStart,
            Access.QualificationStartWorldTick) &&
        BindOptionalSovereigntyTick(
            Statement,
            3,
            Access.bHasQualificationSuspendedTick,
            Access.QualificationSuspendedWorldTick) &&
        BindOptionalSovereigntyTick(
            Statement,
            4,
            Access.bHasUnlockedWorldTick,
            Access.UnlockedWorldTick) &&
        sqlite3_bind_int(
            Statement,
            5,
            Access.bPermanentlyUnlocked
                ? 1
                : 0) == SQLITE_OK &&
        sqlite3_bind_int64(
            Statement,
            6,
            Access.UpdatedWorldTick) == SQLITE_OK;

    const bool bSucceeded =
        bBound &&
        sqlite3_step(Statement) == SQLITE_DONE;
    if (!bSucceeded)
    {
        OutError =
            LastError(TEXT("Upsert gacha-access state"));
    }

    sqlite3_finalize(Statement);
    return bSucceeded;
}

bool FOGSQLiteWorldStore::TryReadRulerGachaAccess(
    const FOGEntityId& RulerId,
    bool& bOutFound,
    FOGRulerGachaAccessRecord& OutAccess,
    FString& OutError) const
{
    bOutFound = false;
    OutAccess = FOGRulerGachaAccessRecord();
    OutError.Reset();

    if (!RulerId.IsValid())
    {
        OutError =
            TEXT("Gacha-access lookup requires a valid Ruler ID.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "SELECT qualification_start_world_tick, qualification_suspended_world_tick, "
        "unlocked_world_tick, permanently_unlocked, updated_world_tick "
        "FROM ruler_gacha_access WHERE ruler_entity_id = ?;";

    if (sqlite3_prepare_v2(
            Database,
            Sql,
            -1,
            &Statement,
            nullptr) != SQLITE_OK)
    {
        OutError =
            LastError(TEXT("Prepare gacha-access read"));
        return false;
    }

    if (!BindSovereigntyText(
            Statement,
            1,
            RulerId.ToString()))
    {
        OutError =
            LastError(TEXT("Bind gacha-access read"));
        sqlite3_finalize(Statement);
        return false;
    }

    const int32 Step =
        sqlite3_step(Statement);
    if (Step == SQLITE_ROW)
    {
        bOutFound = true;
        OutAccess.RulerId = RulerId;

        if (sqlite3_column_type(
                Statement,
                0) != SQLITE_NULL)
        {
            OutAccess.bHasQualificationStart = true;
            OutAccess.QualificationStartWorldTick =
                sqlite3_column_int64(
                    Statement,
                    0);
        }

        if (sqlite3_column_type(
                Statement,
                1) != SQLITE_NULL)
        {
            OutAccess.bHasQualificationSuspendedTick = true;
            OutAccess.QualificationSuspendedWorldTick =
                sqlite3_column_int64(
                    Statement,
                    1);
        }

        if (sqlite3_column_type(
                Statement,
                2) != SQLITE_NULL)
        {
            OutAccess.bHasUnlockedWorldTick = true;
            OutAccess.UnlockedWorldTick =
                sqlite3_column_int64(
                    Statement,
                    2);
        }

        OutAccess.bPermanentlyUnlocked =
            sqlite3_column_int(
                Statement,
                3) != 0;
        OutAccess.UpdatedWorldTick =
            sqlite3_column_int64(
                Statement,
                4);
    }
    else if (Step != SQLITE_DONE)
    {
        OutError =
            LastError(TEXT("Read gacha-access state"));
        sqlite3_finalize(Statement);
        return false;
    }

    sqlite3_finalize(Statement);
    return true;
}

bool FOGSQLiteWorldStore::MigrateTerritorySovereignty0008(
    FString& OutError)
{
    OutError.Reset();

    if (!ExecuteSql(
            TEXT("INSERT OR IGNORE INTO location_territories(")
            TEXT("location_entity_id, territory_entity_id, relation_kind, coverage_bps) ")
            TEXT("SELECT root_location_entity_id, territory_entity_id, 'contained', 10000 ")
            TEXT("FROM territories;"),
            OutError))
    {
        return false;
    }

    struct FLegacyTerritory
    {
        FOGEntityId TerritoryId;
        FOGEntityId RulerId;
        FName ControlState = NAME_None;
        int64 CreatedWorldTick = 0;
    };

    TArray<FLegacyTerritory> Territories;

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "SELECT t.territory_entity_id, t.ruler_entity_id, "
        "t.control_state, e.created_world_tick "
        "FROM territories t "
        "JOIN entities e ON e.id = t.territory_entity_id "
        "WHERE t.ruler_entity_id IS NOT NULL "
        "AND t.ruler_entity_id <> '' "
        "ORDER BY t.ruler_entity_id ASC, t.territory_entity_id ASC;";

    if (sqlite3_prepare_v2(
            Database,
            Sql,
            -1,
            &Statement,
            nullptr) != SQLITE_OK)
    {
        OutError =
            LastError(TEXT("Prepare migration 0008 legacy Territory scan"));
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
                LastError(TEXT("Read migration 0008 legacy Territory"));
            sqlite3_finalize(Statement);
            return false;
        }

        FLegacyTerritory Row;
        Row.TerritoryId =
            ParseSovereigntyEntityId(
                SovereigntyColumnText(
                    Statement,
                    0));
        Row.RulerId =
            ParseSovereigntyEntityId(
                SovereigntyColumnText(
                    Statement,
                    1));
        Row.ControlState =
            FName(*SovereigntyColumnText(
                Statement,
                2));
        Row.CreatedWorldTick =
            sqlite3_column_int64(
                Statement,
                3);

        if (!Row.TerritoryId.IsValid() ||
            !Row.RulerId.IsValid())
        {
            OutError =
                TEXT("Migration 0008 found invalid legacy Territory ownership.");
            sqlite3_finalize(Statement);
            return false;
        }

        Territories.Add(MoveTemp(Row));
    }

    sqlite3_finalize(Statement);

    TMap<FOGEntityId, int64> EarliestEffectiveControl;
    TMap<FOGEntityId, int64> LatestEffectiveControl;
    TSet<FOGEntityId> LegacyRulers;

    for (const FLegacyTerritory& Legacy :
         Territories)
    {
        LegacyRulers.Add(Legacy.RulerId);

        FOGTerritoryClaimRecord Claim;
        Claim.ClaimId =
            StableSovereigntyMigrationId(
                Legacy.TerritoryId,
                Legacy.RulerId);
        Claim.TerritoryId =
            Legacy.TerritoryId;
        Claim.RulerId =
            Legacy.RulerId;
        Claim.ClaimKind =
            FName(TEXT("legacy_control"));
        Claim.ControlState =
            Legacy.ControlState.IsNone()
                ? FName(TEXT("controlled"))
                : Legacy.ControlState;
        Claim.ControlStrengthBps =
            IsEffectiveControlState(
                Claim.ControlState)
                ? 10000
                : 0;
        Claim.ClaimStartWorldTick =
            Legacy.CreatedWorldTick;
        Claim.UpdatedWorldTick =
            Legacy.CreatedWorldTick;

        if (IsEffectiveControlState(
                Claim.ControlState))
        {
            Claim.bHasEffectiveControlStart = true;
            Claim.EffectiveControlStartWorldTick =
                Legacy.CreatedWorldTick;

            int64* ExistingEarliest =
                EarliestEffectiveControl.Find(
                    Legacy.RulerId);
            if (!ExistingEarliest ||
                Legacy.CreatedWorldTick <
                    *ExistingEarliest)
            {
                EarliestEffectiveControl.Add(
                    Legacy.RulerId,
                    Legacy.CreatedWorldTick);
            }

            int64* ExistingLatest =
                LatestEffectiveControl.Find(
                    Legacy.RulerId);
            if (!ExistingLatest ||
                Legacy.CreatedWorldTick >
                    *ExistingLatest)
            {
                LatestEffectiveControl.Add(
                    Legacy.RulerId,
                    Legacy.CreatedWorldTick);
            }
        }

        if (!UpsertTerritoryClaim(
                Claim,
                Legacy.CreatedWorldTick,
                OutError))
        {
            return false;
        }
    }

    // Include rulers that used the stale pre-qualification gacha even if they
    // no longer own a legacy Territory. Past valid gameplay is never revoked.
    Statement = nullptr;
    const char* RulerSql =
        "SELECT DISTINCT ruler_entity_id FROM gacha_states "
        "UNION "
        "SELECT DISTINCT primary_entity_id FROM world_events "
        "WHERE event_type = 'gacha_pull' AND primary_entity_id IS NOT NULL;";

    if (sqlite3_prepare_v2(
            Database,
            RulerSql,
            -1,
            &Statement,
            nullptr) != SQLITE_OK)
    {
        OutError =
            LastError(TEXT("Prepare migration 0008 legacy gacha Ruler scan"));
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
                LastError(TEXT("Read migration 0008 legacy gacha Ruler"));
            sqlite3_finalize(Statement);
            return false;
        }

        const FOGEntityId RulerId =
            ParseSovereigntyEntityId(
                SovereigntyColumnText(
                    Statement,
                    0));
        if (RulerId.IsValid())
        {
            LegacyRulers.Add(RulerId);
        }
    }

    sqlite3_finalize(Statement);

    for (const FOGEntityId& RulerId :
         LegacyRulers)
    {
        const int64* Earliest =
            EarliestEffectiveControl.Find(
                RulerId);
        const int64* Latest =
            LatestEffectiveControl.Find(
                RulerId);

        FOGRulerSovereigntyStateRecord Sovereignty;
        Sovereignty.RulerId = RulerId;
        Sovereignty.CurrentTitle =
            Earliest
                ? FName(TEXT("ruler"))
                : FName(TEXT("none"));
        Sovereignty.HistoricalPeakTitle =
            Earliest
                ? FName(TEXT("ruler"))
                : FName(TEXT("none"));

        if (Earliest)
        {
            Sovereignty.bHasContinuousControlStart = true;
            Sovereignty.ContinuousControlStartWorldTick =
                *Earliest;
        }

        if (Latest)
        {
            Sovereignty.bHasLastEffectiveControlTick = true;
            Sovereignty.LastEffectiveControlWorldTick =
                *Latest;
            Sovereignty.UpdatedWorldTick =
                *Latest;
        }

        if (!UpsertRulerSovereigntyState(
                Sovereignty,
                OutError))
        {
            return false;
        }

        bool bPriorGachaUse = false;
        int64 PriorGachaTick = 0;
        if (!ReadEarliestLegacyGachaTick(
                Database,
                RulerId,
                bPriorGachaUse,
                PriorGachaTick,
                OutError))
        {
            return false;
        }

        FOGRulerGachaAccessRecord Access;
        Access.RulerId = RulerId;

        if (bPriorGachaUse)
        {
            Access.bPermanentlyUnlocked = true;
            Access.bHasUnlockedWorldTick = true;
            Access.UnlockedWorldTick =
                PriorGachaTick;
            Access.UpdatedWorldTick =
                PriorGachaTick;
        }
        else if (Earliest)
        {
            Access.bHasQualificationStart = true;
            Access.QualificationStartWorldTick =
                *Earliest;
            Access.UpdatedWorldTick =
                *Latest;
        }

        if (!UpsertRulerGachaAccess(
                Access,
                OutError))
        {
            return false;
        }
    }

    return true;
}

bool FOGSQLiteWorldStore::ValidateTerritorySovereigntyMigration0008(
    FString& OutError) const
{
    OutError.Reset();

    int32 MissingRootRelations = 0;
    if (!ReadValidationCount(
            Database,
            "SELECT COUNT(*) FROM territories t "
            "LEFT JOIN location_territories lt "
            "ON lt.location_entity_id = t.root_location_entity_id "
            "AND lt.territory_entity_id = t.territory_entity_id "
            "WHERE lt.location_entity_id IS NULL;",
            MissingRootRelations,
            OutError))
    {
        return false;
    }

    if (MissingRootRelations != 0)
    {
        OutError = FString::Printf(
            TEXT("Migration 0008 left %d Territory roots without normalized location membership."),
            MissingRootRelations);
        return false;
    }

    int32 MissingLegacyClaims = 0;
    if (!ReadValidationCount(
            Database,
            "SELECT COUNT(*) FROM territories t "
            "LEFT JOIN territory_claims tc "
            "ON tc.territory_entity_id = t.territory_entity_id "
            "AND tc.ruler_entity_id = t.ruler_entity_id "
            "WHERE t.ruler_entity_id IS NOT NULL "
            "AND t.ruler_entity_id <> '' "
            "AND tc.claim_entity_id IS NULL;",
            MissingLegacyClaims,
            OutError))
    {
        return false;
    }

    if (MissingLegacyClaims != 0)
    {
        OutError = FString::Printf(
            TEXT("Migration 0008 left %d legacy Territory owners without claims."),
            MissingLegacyClaims);
        return false;
    }

    int32 InvalidUnlockRows = 0;
    if (!ReadValidationCount(
            Database,
            "SELECT COUNT(*) FROM ruler_gacha_access "
            "WHERE permanently_unlocked = 1 AND unlocked_world_tick IS NULL;",
            InvalidUnlockRows,
            OutError))
    {
        return false;
    }

    if (InvalidUnlockRows != 0)
    {
        OutError =
            TEXT("Migration 0008 produced permanently unlocked gacha access without an unlock tick.");
        return false;
    }

    int32 UngrantedLegacyGachaRows = 0;
    if (!ReadValidationCount(
            Database,
            "SELECT COUNT(*) FROM ("
            "SELECT DISTINCT ruler_entity_id AS ruler_id FROM gacha_states "
            "UNION "
            "SELECT DISTINCT primary_entity_id AS ruler_id FROM world_events "
            "WHERE event_type = 'gacha_pull' AND primary_entity_id IS NOT NULL"
            ") legacy "
            "LEFT JOIN ruler_gacha_access a ON a.ruler_entity_id = legacy.ruler_id "
            "WHERE a.ruler_entity_id IS NULL OR a.permanently_unlocked <> 1;",
            UngrantedLegacyGachaRows,
            OutError))
    {
        return false;
    }

    if (UngrantedLegacyGachaRows != 0)
    {
        OutError = FString::Printf(
            TEXT("Migration 0008 would retroactively revoke gacha access for %d legacy Rulers."),
            UngrantedLegacyGachaRows);
        return false;
    }

    return true;
}
