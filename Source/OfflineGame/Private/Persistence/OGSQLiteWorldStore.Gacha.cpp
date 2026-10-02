#include "Persistence/OGSQLiteWorldStore.h"

#include "sqlite/sqlite3.h"

namespace
{
bool BindGachaText(sqlite3_stmt* Statement, int32 Index, const FString& Value)
{
    FTCHARToUTF8 Utf8(*Value);
    return sqlite3_bind_text(
        Statement,
        Index,
        Utf8.Get(),
        Utf8.Length(),
        SQLITE_TRANSIENT) == SQLITE_OK;
}

FString GachaColumnText(sqlite3_stmt* Statement, int32 Column)
{
    const unsigned char* Text = sqlite3_column_text(Statement, Column);
    return Text
        ? UTF8_TO_TCHAR(reinterpret_cast<const char*>(Text))
        : FString();
}
}

bool FOGSQLiteWorldStore::TryFindCharacterManifestationByOwnerAndIdentity(
    const FOGEntityId& RulerId,
    const FOGContentId& IdentityId,
    bool& bOutFound,
    FOGCharacterManifestationRecord& OutManifestation,
    FString& OutError) const
{
    bOutFound = false;
    OutManifestation = FOGCharacterManifestationRecord();
    OutError.Reset();

    if (!RulerId.IsValid() || !IdentityId.IsValid())
    {
        OutError = TEXT("Manifestation owner/identity lookup requires valid IDs.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "SELECT manifestation_entity_id "
        "FROM character_manifestations "
        "WHERE owning_ruler_entity_id = ? AND identity_content_id = ? "
        "ORDER BY rowid ASC LIMIT 1;";

    if (sqlite3_prepare_v2(Database, Sql, -1, &Statement, nullptr) != SQLITE_OK)
    {
        OutError = LastError(TEXT("Prepare Manifestation owner/identity lookup"));
        return false;
    }

    const bool bBound =
        BindGachaText(Statement, 1, RulerId.ToString()) &&
        BindGachaText(Statement, 2, IdentityId.ToString());

    if (!bBound)
    {
        OutError = LastError(TEXT("Bind Manifestation owner/identity lookup"));
        sqlite3_finalize(Statement);
        return false;
    }

    FOGEntityId ManifestationId;
    const int32 StepResult = sqlite3_step(Statement);
    if (StepResult == SQLITE_ROW)
    {
        FGuid Guid;
        if (!FGuid::Parse(GachaColumnText(Statement, 0), Guid))
        {
            OutError = TEXT("Stored Manifestation has an invalid entity ID.");
            sqlite3_finalize(Statement);
            return false;
        }
        ManifestationId = FOGEntityId(Guid);
    }
    else if (StepResult != SQLITE_DONE)
    {
        OutError = LastError(TEXT("Read Manifestation owner/identity lookup"));
        sqlite3_finalize(Statement);
        return false;
    }

    sqlite3_finalize(Statement);

    if (!ManifestationId.IsValid())
    {
        return true;
    }

    return TryReadCharacterManifestation(
        ManifestationId,
        bOutFound,
        OutManifestation,
        OutError);
}

bool FOGSQLiteWorldStore::UpsertGachaState(
    const FOGGachaStateRecord& State,
    FString& OutError)
{
    OutError.Reset();

    if (!State.RulerId.IsValid() ||
        State.PityCategory.IsNone() ||
        State.PullsSinceTopRarity < 0 ||
        State.TotalPulls < 0)
    {
        OutError = TEXT("Gacha state contains invalid owner/category/counters.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "INSERT INTO gacha_states("
        "ruler_entity_id, pity_category, pulls_since_top_rarity, "
        "featured_guaranteed, total_pulls, updated_world_tick"
        ") VALUES(?, ?, ?, ?, ?, ?) "
        "ON CONFLICT(ruler_entity_id, pity_category) DO UPDATE SET "
        "pulls_since_top_rarity = excluded.pulls_since_top_rarity, "
        "featured_guaranteed = excluded.featured_guaranteed, "
        "total_pulls = excluded.total_pulls, "
        "updated_world_tick = excluded.updated_world_tick;";

    if (sqlite3_prepare_v2(Database, Sql, -1, &Statement, nullptr) != SQLITE_OK)
    {
        OutError = LastError(TEXT("Prepare gacha-state upsert"));
        return false;
    }

    const bool bBound =
        BindGachaText(Statement, 1, State.RulerId.ToString()) &&
        BindGachaText(Statement, 2, State.PityCategory.ToString()) &&
        sqlite3_bind_int(Statement, 3, State.PullsSinceTopRarity) == SQLITE_OK &&
        sqlite3_bind_int(Statement, 4, State.bFeaturedGuarantee ? 1 : 0) == SQLITE_OK &&
        sqlite3_bind_int64(Statement, 5, State.TotalPulls) == SQLITE_OK &&
        sqlite3_bind_int64(Statement, 6, State.UpdatedWorldTick) == SQLITE_OK;

    const bool bSucceeded =
        bBound &&
        sqlite3_step(Statement) == SQLITE_DONE;

    if (!bSucceeded)
    {
        OutError = LastError(TEXT("Upsert gacha state"));
    }

    sqlite3_finalize(Statement);
    return bSucceeded;
}

bool FOGSQLiteWorldStore::TryReadGachaState(
    const FOGEntityId& RulerId,
    FName PityCategory,
    bool& bOutFound,
    FOGGachaStateRecord& OutState,
    FString& OutError) const
{
    bOutFound = false;
    OutState = FOGGachaStateRecord();
    OutError.Reset();

    if (!RulerId.IsValid() || PityCategory.IsNone())
    {
        OutError = TEXT("Gacha-state lookup requires a valid owner and pity category.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "SELECT pulls_since_top_rarity, featured_guaranteed, "
        "total_pulls, updated_world_tick "
        "FROM gacha_states "
        "WHERE ruler_entity_id = ? AND pity_category = ?;";

    if (sqlite3_prepare_v2(Database, Sql, -1, &Statement, nullptr) != SQLITE_OK)
    {
        OutError = LastError(TEXT("Prepare gacha-state read"));
        return false;
    }

    const bool bBound =
        BindGachaText(Statement, 1, RulerId.ToString()) &&
        BindGachaText(Statement, 2, PityCategory.ToString());

    if (!bBound)
    {
        OutError = LastError(TEXT("Bind gacha-state read"));
        sqlite3_finalize(Statement);
        return false;
    }

    const int32 StepResult = sqlite3_step(Statement);
    if (StepResult == SQLITE_ROW)
    {
        bOutFound = true;
        OutState.RulerId = RulerId;
        OutState.PityCategory = PityCategory;
        OutState.PullsSinceTopRarity = sqlite3_column_int(Statement, 0);
        OutState.bFeaturedGuarantee = sqlite3_column_int(Statement, 1) != 0;
        OutState.TotalPulls = sqlite3_column_int64(Statement, 2);
        OutState.UpdatedWorldTick = sqlite3_column_int64(Statement, 3);
    }
    else if (StepResult != SQLITE_DONE)
    {
        OutError = LastError(TEXT("Read gacha state"));
        sqlite3_finalize(Statement);
        return false;
    }

    sqlite3_finalize(Statement);
    return true;
}
