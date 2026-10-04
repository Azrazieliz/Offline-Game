#include "Persistence/OGSQLiteWorldStore.h"

#include "Dom/JsonObject.h"
#include "Misc/SecureHash.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "sqlite/sqlite3.h"

namespace
{
bool BindManifestationText(
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

FString ManifestationColumnText(
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

uint32 ReadDigestWord(const uint8* Digest)
{
    return
        (static_cast<uint32>(Digest[0]) << 24) |
        (static_cast<uint32>(Digest[1]) << 16) |
        (static_cast<uint32>(Digest[2]) << 8) |
        static_cast<uint32>(Digest[3]);
}

FOGEntityId StableMigrationEntityId(
    const FOGEntityId& LegacyManifestationId,
    const FString& Purpose)
{
    const FString Material = FString::Printf(
        TEXT("offlinegame:migration0007:%s:%s"),
        *LegacyManifestationId.ToString(),
        *Purpose);

    FTCHARToUTF8 Utf8(*Material);
    FMD5 Md5;
    Md5.Update(
        reinterpret_cast<const uint8*>(Utf8.Get()),
        Utf8.Length());

    uint8 Digest[16];
    Md5.Final(Digest);

    FGuid Guid(
        ReadDigestWord(Digest),
        ReadDigestWord(Digest + 4),
        ReadDigestWord(Digest + 8),
        ReadDigestWord(Digest + 12));

    if (!Guid.IsValid())
    {
        Guid.D = 1;
    }

    return FOGEntityId(Guid);
}

struct FLegacyManifestationMigrationRow
{
    FOGCharacterManifestationRecord Manifestation;
    int32 DuplicateCount = 0;
    int64 CreatedWorldTick = 0;
};

struct FLegacyGachaEvent
{
    FOGEntityId EventId;
    int64 WorldTick = 0;
    FOGContentId VersionId;
    FName Rarity = NAME_None;
};

bool ParseGachaPayload(
    const FString& PayloadJson,
    FOGContentId& OutIdentityId,
    FOGContentId& OutVersionId,
    FName& OutRarity)
{
    TSharedPtr<FJsonObject> Json;
    const TSharedRef<TJsonReader<>> Reader =
        TJsonReaderFactory<>::Create(PayloadJson);

    if (!FJsonSerializer::Deserialize(Reader, Json) ||
        !Json.IsValid())
    {
        return false;
    }

    FString Identity;
    FString Version;
    FString Rarity;
    if (!Json->TryGetStringField(
            TEXT("identity"),
            Identity) ||
        !Json->TryGetStringField(
            TEXT("version"),
            Version) ||
        !Json->TryGetStringField(
            TEXT("rarity"),
            Rarity))
    {
        return false;
    }

    OutIdentityId = FOGContentId(Identity);
    OutVersionId = FOGContentId(Version);
    OutRarity = FName(*Rarity);
    return OutIdentityId.IsValid() &&
        OutVersionId.IsValid() &&
        !OutRarity.IsNone();
}

bool ReadCountQuery(
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
            TEXT("Failed to prepare migration 0007 validation query.");
        return false;
    }

    if (sqlite3_step(Statement) != SQLITE_ROW)
    {
        OutError =
            TEXT("Failed to execute migration 0007 validation query.");
        sqlite3_finalize(Statement);
        return false;
    }

    OutCount = sqlite3_column_int(Statement, 0);
    sqlite3_finalize(Statement);
    return true;
}
}

bool FOGSQLiteWorldStore::ListCharacterManifestationsByOwnerAndIdentity(
    const FOGEntityId& RulerId,
    const FOGContentId& IdentityId,
    TArray<FOGCharacterManifestationRecord>& OutManifestations,
    FString& OutError) const
{
    OutManifestations.Reset();
    OutError.Reset();

    if (!RulerId.IsValid() ||
        !IdentityId.IsValid())
    {
        OutError =
            TEXT("Manifestation list lookup requires valid owner and Identity IDs.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "SELECT manifestation_entity_id "
        "FROM character_manifestations "
        "WHERE owning_ruler_entity_id = ? "
        "AND identity_content_id = ? "
        "ORDER BY acquisition_ordinal ASC, manifestation_entity_id ASC;";

    if (sqlite3_prepare_v2(
            Database,
            Sql,
            -1,
            &Statement,
            nullptr) != SQLITE_OK)
    {
        OutError =
            LastError(TEXT("Prepare Manifestation owner/identity list"));
        return false;
    }

    if (!BindManifestationText(
            Statement,
            1,
            RulerId.ToString()) ||
        !BindManifestationText(
            Statement,
            2,
            IdentityId.ToString()))
    {
        OutError =
            LastError(TEXT("Bind Manifestation owner/identity list"));
        sqlite3_finalize(Statement);
        return false;
    }

    TArray<FOGEntityId> ManifestationIds;
    while (true)
    {
        const int32 Step = sqlite3_step(Statement);
        if (Step == SQLITE_DONE)
        {
            break;
        }

        if (Step != SQLITE_ROW)
        {
            OutError =
                LastError(TEXT("Read Manifestation owner/identity list"));
            sqlite3_finalize(Statement);
            return false;
        }

        FGuid Guid;
        if (!FGuid::Parse(
                ManifestationColumnText(Statement, 0),
                Guid))
        {
            OutError =
                TEXT("Stored Manifestation list contains an invalid entity ID.");
            sqlite3_finalize(Statement);
            return false;
        }

        ManifestationIds.Add(FOGEntityId(Guid));
    }

    sqlite3_finalize(Statement);

    OutManifestations.Reserve(
        ManifestationIds.Num());

    for (const FOGEntityId& ManifestationId :
         ManifestationIds)
    {
        bool bFound = false;
        FOGCharacterManifestationRecord Manifestation;
        if (!TryReadCharacterManifestation(
                ManifestationId,
                bFound,
                Manifestation,
                OutError))
        {
            OutManifestations.Reset();
            return false;
        }

        if (!bFound)
        {
            OutError =
                TEXT("Manifestation list referenced a missing persistence row.");
            OutManifestations.Reset();
            return false;
        }

        OutManifestations.Add(MoveTemp(Manifestation));
    }

    return true;
}

bool FOGSQLiteWorldStore::ListCharacterManifestationsByOwner(
    const FOGEntityId& RulerId,
    TArray<FOGCharacterManifestationRecord>& OutManifestations,
    FString& OutError) const
{
    OutManifestations.Reset();
    OutError.Reset();

    if (!RulerId.IsValid())
    {
        OutError =
            TEXT("Manifestation owner list requires a valid Ruler ID.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "SELECT manifestation_entity_id "
        "FROM character_manifestations "
        "WHERE owning_ruler_entity_id = ? "
        "ORDER BY identity_content_id ASC, acquisition_ordinal ASC, manifestation_entity_id ASC;";

    if (sqlite3_prepare_v2(
            Database,
            Sql,
            -1,
            &Statement,
            nullptr) != SQLITE_OK)
    {
        OutError =
            LastError(TEXT("Prepare Manifestation owner list"));
        return false;
    }

    if (!BindManifestationText(
            Statement,
            1,
            RulerId.ToString()))
    {
        OutError =
            LastError(TEXT("Bind Manifestation owner list"));
        sqlite3_finalize(Statement);
        return false;
    }

    TArray<FOGEntityId> ManifestationIds;
    while (true)
    {
        const int32 Step = sqlite3_step(Statement);
        if (Step == SQLITE_DONE)
        {
            break;
        }

        if (Step != SQLITE_ROW)
        {
            OutError =
                LastError(TEXT("Read Manifestation owner list"));
            sqlite3_finalize(Statement);
            return false;
        }

        FGuid Guid;
        if (!FGuid::Parse(
                ManifestationColumnText(Statement, 0),
                Guid))
        {
            OutError =
                TEXT("Stored Manifestation list contains an invalid entity ID.");
            sqlite3_finalize(Statement);
            return false;
        }

        ManifestationIds.Add(FOGEntityId(Guid));
    }

    sqlite3_finalize(Statement);

    OutManifestations.Reserve(
        ManifestationIds.Num());

    for (const FOGEntityId& ManifestationId :
         ManifestationIds)
    {
        bool bFound = false;
        FOGCharacterManifestationRecord Manifestation;
        if (!TryReadCharacterManifestation(
                ManifestationId,
                bFound,
                Manifestation,
                OutError))
        {
            OutManifestations.Reset();
            return false;
        }

        if (!bFound)
        {
            OutError =
                TEXT("Manifestation owner list referenced a missing persistence row.");
            OutManifestations.Reset();
            return false;
        }

        OutManifestations.Add(MoveTemp(Manifestation));
    }

    return true;
}

bool FOGSQLiteWorldStore::SetManifestationAnchor(
    const FOGEntityId& ManifestationId,
    const FOGEntityId& TerritoryId,
    int64 WorldTick,
    FString& OutError)
{
    OutError.Reset();

    if (!ManifestationId.IsValid() ||
        !TerritoryId.IsValid() ||
        WorldTick < 0)
    {
        OutError =
            TEXT("Manifestation anchoring requires valid Manifestation/Territory IDs and a non-negative tick.");
        return false;
    }

    bool bManifestationFound = false;
    FOGCharacterManifestationRecord Existing;
    if (!TryReadCharacterManifestation(
            ManifestationId,
            bManifestationFound,
            Existing,
            OutError))
    {
        return false;
    }

    if (!bManifestationFound)
    {
        OutError =
            TEXT("Cannot anchor an unknown Manifestation.");
        return false;
    }

    bool bTerritoryFound = false;
    FOGTerritoryRecord Territory;
    if (!TryReadTerritory(
            TerritoryId,
            bTerritoryFound,
            Territory,
            OutError))
    {
        return false;
    }

    if (!bTerritoryFound)
    {
        OutError =
            TEXT("Cannot anchor a Manifestation to an unknown Territory.");
        return false;
    }

    if (Existing.WorldModeAnchorTerritoryId.IsValid())
    {
        if (Existing.WorldModeAnchorTerritoryId ==
                TerritoryId &&
            Existing.WorldModeAnchorTick ==
                WorldTick)
        {
            return true;
        }

        OutError =
            TEXT("Manifestation first World Mode anchor is immutable once established.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "UPDATE character_manifestations "
        "SET world_mode_anchor_territory_id = ?, "
        "world_mode_anchor_tick = ? "
        "WHERE manifestation_entity_id = ? "
        "AND world_mode_anchor_territory_id IS NULL;";

    if (sqlite3_prepare_v2(
            Database,
            Sql,
            -1,
            &Statement,
            nullptr) != SQLITE_OK)
    {
        OutError =
            LastError(TEXT("Prepare Manifestation anchor update"));
        return false;
    }

    const bool bBound =
        BindManifestationText(
            Statement,
            1,
            TerritoryId.ToString()) &&
        sqlite3_bind_int64(
            Statement,
            2,
            WorldTick) == SQLITE_OK &&
        BindManifestationText(
            Statement,
            3,
            ManifestationId.ToString());

    const bool bStepped =
        bBound &&
        sqlite3_step(Statement) == SQLITE_DONE;
    const int32 Changed =
        bStepped
            ? sqlite3_changes(Database)
            : 0;

    if (!bStepped)
    {
        OutError =
            LastError(TEXT("Update Manifestation anchor"));
        sqlite3_finalize(Statement);
        return false;
    }

    sqlite3_finalize(Statement);

    if (Changed != 1)
    {
        OutError =
            TEXT("Manifestation anchor was not updated.");
        return false;
    }

    return true;
}

bool FOGSQLiteWorldStore::SetManifestationLifecycle(
    const FOGEntityId& ManifestationId,
    FName LifecycleState,
    FString& OutError)
{
    OutError.Reset();

    if (!ManifestationId.IsValid() ||
        LifecycleState.IsNone())
    {
        OutError =
            TEXT("Manifestation lifecycle update requires a valid ID and non-empty state.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "UPDATE character_manifestations "
        "SET lifecycle_state = ? "
        "WHERE manifestation_entity_id = ?;";

    if (sqlite3_prepare_v2(
            Database,
            Sql,
            -1,
            &Statement,
            nullptr) != SQLITE_OK)
    {
        OutError =
            LastError(TEXT("Prepare Manifestation lifecycle update"));
        return false;
    }

    const bool bBound =
        BindManifestationText(
            Statement,
            1,
            LifecycleState.ToString()) &&
        BindManifestationText(
            Statement,
            2,
            ManifestationId.ToString());

    const bool bStepped =
        bBound &&
        sqlite3_step(Statement) == SQLITE_DONE;
    const int32 Changed =
        bStepped
            ? sqlite3_changes(Database)
            : 0;

    if (!bStepped)
    {
        OutError =
            LastError(TEXT("Update Manifestation lifecycle"));
        sqlite3_finalize(Statement);
        return false;
    }

    sqlite3_finalize(Statement);

    if (Changed != 1)
    {
        OutError =
            TEXT("Cannot update lifecycle for an unknown Manifestation.");
        return false;
    }

    return true;
}

bool FOGSQLiteWorldStore::MigrateLegacyDuplicateManifestations0007(
    FString& OutError)
{
    OutError.Reset();

    TArray<FLegacyManifestationMigrationRow> LegacyRows;

    sqlite3_stmt* LegacyStatement = nullptr;
    const char* LegacySql =
        "SELECT cm.manifestation_entity_id, cm.owning_ruler_entity_id, "
        "cm.identity_content_id, cm.active_version_content_id, cm.level, "
        "cm.current_rarity, cm.progression_state_json, "
        "cm.duplicate_acquisition_count, e.created_world_tick "
        "FROM character_manifestations cm "
        "LEFT JOIN entities e ON e.id = cm.manifestation_entity_id "
        "ORDER BY cm.owning_ruler_entity_id ASC, "
        "cm.identity_content_id ASC, cm.manifestation_entity_id ASC;";

    if (sqlite3_prepare_v2(
            Database,
            LegacySql,
            -1,
            &LegacyStatement,
            nullptr) != SQLITE_OK)
    {
        OutError =
            LastError(TEXT("Prepare migration 0007 legacy Manifestation scan"));
        return false;
    }

    while (true)
    {
        const int32 Step =
            sqlite3_step(LegacyStatement);
        if (Step == SQLITE_DONE)
        {
            break;
        }

        if (Step != SQLITE_ROW)
        {
            OutError =
                LastError(TEXT("Read migration 0007 legacy Manifestation"));
            sqlite3_finalize(LegacyStatement);
            return false;
        }

        FLegacyManifestationMigrationRow Row;

        FGuid ManifestationGuid;
        FGuid OwnerGuid;
        if (!FGuid::Parse(
                ManifestationColumnText(
                    LegacyStatement,
                    0),
                ManifestationGuid) ||
            !FGuid::Parse(
                ManifestationColumnText(
                    LegacyStatement,
                    1),
                OwnerGuid))
        {
            OutError =
                TEXT("Migration 0007 found invalid legacy Manifestation/owner IDs.");
            sqlite3_finalize(LegacyStatement);
            return false;
        }

        Row.Manifestation.ManifestationId =
            FOGEntityId(ManifestationGuid);
        Row.Manifestation.OwningRulerId =
            FOGEntityId(OwnerGuid);
        Row.Manifestation.IdentityId =
            FOGContentId(
                ManifestationColumnText(
                    LegacyStatement,
                    2));
        Row.Manifestation.ActiveVersionId =
            FOGContentId(
                ManifestationColumnText(
                    LegacyStatement,
                    3));
        Row.Manifestation.Level =
            sqlite3_column_int(
                LegacyStatement,
                4);
        Row.Manifestation.CurrentRarity =
            FName(*ManifestationColumnText(
                LegacyStatement,
                5));
        Row.Manifestation.ProgressionStateJson =
            ManifestationColumnText(
                LegacyStatement,
                6);
        Row.Manifestation.AcquisitionOrdinal = 0;
        Row.Manifestation.LifecycleState =
            FName(TEXT("active"));
        Row.DuplicateCount =
            sqlite3_column_int(
                LegacyStatement,
                7);
        Row.CreatedWorldTick =
            sqlite3_column_type(
                LegacyStatement,
                8) == SQLITE_NULL
                ? 0
                : sqlite3_column_int64(
                    LegacyStatement,
                    8);

        if (Row.DuplicateCount < 0)
        {
            OutError =
                TEXT("Migration 0007 found a negative legacy duplicate count.");
            sqlite3_finalize(LegacyStatement);
            return false;
        }

        LegacyRows.Add(MoveTemp(Row));
    }

    sqlite3_finalize(LegacyStatement);

    for (FLegacyManifestationMigrationRow& Row :
         LegacyRows)
    {
        TArray<FLegacyGachaEvent> Events;

        sqlite3_stmt* EventStatement = nullptr;
        const char* EventSql =
            "SELECT event_id, world_tick, payload_json, related_entities_json "
            "FROM world_events "
            "WHERE event_type = 'gacha_pull' "
            "AND primary_entity_id = ? "
            "ORDER BY world_tick ASC, event_id ASC;";

        if (sqlite3_prepare_v2(
                Database,
                EventSql,
                -1,
                &EventStatement,
                nullptr) != SQLITE_OK)
        {
            OutError =
                LastError(TEXT("Prepare migration 0007 gacha event scan"));
            return false;
        }

        if (!BindManifestationText(
                EventStatement,
                1,
                Row.Manifestation.OwningRulerId.ToString()))
        {
            OutError =
                LastError(TEXT("Bind migration 0007 gacha event owner"));
            sqlite3_finalize(EventStatement);
            return false;
        }

        while (true)
        {
            const int32 Step =
                sqlite3_step(EventStatement);
            if (Step == SQLITE_DONE)
            {
                break;
            }

            if (Step != SQLITE_ROW)
            {
                OutError =
                    LastError(TEXT("Read migration 0007 gacha event"));
                sqlite3_finalize(EventStatement);
                return false;
            }

            const FString RelatedJson =
                ManifestationColumnText(
                    EventStatement,
                    3);
            if (!RelatedJson.Contains(
                    Row.Manifestation.ManifestationId.ToString(),
                    ESearchCase::CaseSensitive))
            {
                continue;
            }

            FOGContentId EventIdentityId;
            FOGContentId EventVersionId;
            FName EventRarity = NAME_None;
            if (!ParseGachaPayload(
                    ManifestationColumnText(
                        EventStatement,
                        2),
                    EventIdentityId,
                    EventVersionId,
                    EventRarity) ||
                EventIdentityId !=
                    Row.Manifestation.IdentityId)
            {
                continue;
            }

            FGuid EventGuid;
            if (!FGuid::Parse(
                    ManifestationColumnText(
                        EventStatement,
                        0),
                    EventGuid))
            {
                OutError =
                    TEXT("Migration 0007 found an invalid historical gacha event ID.");
                sqlite3_finalize(EventStatement);
                return false;
            }

            FLegacyGachaEvent Event;
            Event.EventId =
                FOGEntityId(EventGuid);
            Event.WorldTick =
                sqlite3_column_int64(
                    EventStatement,
                    1);
            Event.VersionId =
                EventVersionId;
            Event.Rarity =
                EventRarity;
            Events.Add(MoveTemp(Event));
        }

        sqlite3_finalize(EventStatement);

        if (!Events.IsEmpty())
        {
            Row.Manifestation.AcquisitionWorldTick =
                Events[0].WorldTick;
            Row.Manifestation.OriginPullEventId =
                Events[0].EventId;
        }
        else
        {
            Row.Manifestation.AcquisitionWorldTick =
                Row.CreatedWorldTick;
        }

        if (!UpsertCharacterManifestation(
                Row.Manifestation,
                Row.CreatedWorldTick,
                OutError))
        {
            return false;
        }

        TArray<FOGEntityId> ReconstructedIds;
        int32 MissingHistoricalEvents = 0;

        for (int32 Ordinal = 1;
             Ordinal <= Row.DuplicateCount;
             ++Ordinal)
        {
            FOGCharacterManifestationRecord Copy;
            Copy.ManifestationId =
                StableMigrationEntityId(
                    Row.Manifestation.ManifestationId,
                    FString::Printf(
                        TEXT("manifestation:%d"),
                        Ordinal));
            Copy.OwningRulerId =
                Row.Manifestation.OwningRulerId;
            Copy.IdentityId =
                Row.Manifestation.IdentityId;
            Copy.Level = 1;
            Copy.AcquisitionOrdinal = Ordinal;
            Copy.LifecycleState =
                FName(TEXT("active"));
            Copy.ProgressionStateJson =
                TEXT("{}");

            if (Events.IsValidIndex(Ordinal))
            {
                const FLegacyGachaEvent& Event =
                    Events[Ordinal];
                Copy.ActiveVersionId =
                    Event.VersionId;
                Copy.CurrentRarity =
                    Event.Rarity;
                Copy.AcquisitionWorldTick =
                    Event.WorldTick;
                Copy.OriginPullEventId =
                    Event.EventId;
            }
            else
            {
                ++MissingHistoricalEvents;
                Copy.ActiveVersionId =
                    Row.Manifestation.ActiveVersionId;
                Copy.CurrentRarity =
                    Row.Manifestation.CurrentRarity;
                Copy.AcquisitionWorldTick =
                    Row.CreatedWorldTick;
            }

            bool bExisting = false;
            FName ExistingKind = NAME_None;
            FString ExistingState;
            int64 ExistingRevision = 0;
            if (!TryReadEntity(
                    Copy.ManifestationId,
                    bExisting,
                    ExistingKind,
                    ExistingState,
                    ExistingRevision,
                    OutError))
            {
                return false;
            }

            if (bExisting &&
                ExistingKind !=
                    FName(TEXT("character_manifestation")))
            {
                OutError =
                    TEXT("Migration 0007 deterministic Manifestation ID collided with a non-Manifestation entity.");
                return false;
            }

            if (!UpsertCharacterManifestation(
                    Copy,
                    Copy.AcquisitionWorldTick,
                    OutError))
            {
                return false;
            }

            ReconstructedIds.Add(
                Copy.ManifestationId);
        }

        sqlite3_stmt* ResetStatement = nullptr;
        const char* ResetSql =
            "UPDATE character_manifestations "
            "SET duplicate_acquisition_count = 0 "
            "WHERE manifestation_entity_id = ?;";

        if (sqlite3_prepare_v2(
                Database,
                ResetSql,
                -1,
                &ResetStatement,
                nullptr) != SQLITE_OK)
        {
            OutError =
                LastError(TEXT("Prepare migration 0007 legacy counter reset"));
            return false;
        }

        const bool bResetBound =
            BindManifestationText(
                ResetStatement,
                1,
                Row.Manifestation.ManifestationId.ToString());
        const bool bResetSucceeded =
            bResetBound &&
            sqlite3_step(ResetStatement) ==
                SQLITE_DONE;

        if (!bResetSucceeded)
        {
            OutError =
                LastError(TEXT("Reset migration 0007 legacy duplicate counter"));
            sqlite3_finalize(ResetStatement);
            return false;
        }

        sqlite3_finalize(ResetStatement);

        if (MissingHistoricalEvents > 0)
        {
            FOGWorldEvent AuditEvent;
            AuditEvent.EventId =
                StableMigrationEntityId(
                    Row.Manifestation.ManifestationId,
                    TEXT("audit"));
            AuditEvent.EventType =
                FName(TEXT("migration.0007.reconstructed_manifestations"));
            AuditEvent.WorldTick =
                Row.CreatedWorldTick;
            AuditEvent.PrimaryEntity =
                Row.Manifestation.OwningRulerId;
            AuditEvent.RelatedEntities.Add(
                Row.Manifestation.ManifestationId);
            AuditEvent.RelatedEntities.Append(
                ReconstructedIds);
            AuditEvent.bChronicleEligible = false;
            AuditEvent.PayloadJson =
                FString::Printf(
                    TEXT("{\"identity\":\"%s\",\"legacy_duplicates\":%d,\"historical_events\":%d,\"reconstructed_without_event\":%d}"),
                    *Row.Manifestation.IdentityId.ToString(),
                    Row.DuplicateCount,
                    Events.Num(),
                    MissingHistoricalEvents);

            if (!AppendWorldEvent(
                    AuditEvent,
                    OutError))
            {
                return false;
            }
        }
    }

    return true;
}

bool FOGSQLiteWorldStore::ValidateManifestationMigration0007(
    FString& OutError) const
{
    OutError.Reset();

    int32 LegacyCounterRows = 0;
    if (!ReadCountQuery(
            Database,
            "SELECT COUNT(*) FROM character_manifestations "
            "WHERE duplicate_acquisition_count <> 0;",
            LegacyCounterRows,
            OutError))
    {
        return false;
    }

    if (LegacyCounterRows != 0)
    {
        OutError = FString::Printf(
            TEXT("Migration 0007 left %d gameplay duplicate counters non-zero."),
            LegacyCounterRows);
        return false;
    }

    int32 InvalidStateRows = 0;
    if (!ReadCountQuery(
            Database,
            "SELECT COUNT(*) FROM character_manifestations "
            "WHERE acquisition_ordinal < 0 "
            "OR lifecycle_state IS NULL "
            "OR lifecycle_state = '';",
            InvalidStateRows,
            OutError))
    {
        return false;
    }

    if (InvalidStateRows != 0)
    {
        OutError = FString::Printf(
            TEXT("Migration 0007 produced %d Manifestations with invalid acquisition/lifecycle state."),
            InvalidStateRows);
        return false;
    }

    int32 DuplicateOrdinalGroups = 0;
    if (!ReadCountQuery(
            Database,
            "SELECT COUNT(*) FROM ("
            "SELECT owning_ruler_entity_id, identity_content_id, acquisition_ordinal "
            "FROM character_manifestations "
            "GROUP BY owning_ruler_entity_id, identity_content_id, acquisition_ordinal "
            "HAVING COUNT(*) > 1);",
            DuplicateOrdinalGroups,
            OutError))
    {
        return false;
    }

    if (DuplicateOrdinalGroups != 0)
    {
        OutError = FString::Printf(
            TEXT("Migration 0007 produced %d duplicate owner/Identity acquisition ordinals."),
            DuplicateOrdinalGroups);
        return false;
    }

    return true;
}
