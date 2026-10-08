#include "Persistence/OGSQLiteWorldStore.h"

#include "sqlite/sqlite3.h"

namespace
{
bool BindStoreText(sqlite3_stmt* Statement, int32 Index, const FString& Value)
{
    FTCHARToUTF8 Utf8(*Value);
    return sqlite3_bind_text(
        Statement,
        Index,
        Utf8.Get(),
        Utf8.Length(),
        SQLITE_TRANSIENT) == SQLITE_OK;
}

FString StoreColumnText(sqlite3_stmt* Statement, int32 Column)
{
    const unsigned char* Text = sqlite3_column_text(Statement, Column);
    return Text ? UTF8_TO_TCHAR(reinterpret_cast<const char*>(Text)) : FString();
}
}

bool FOGSQLiteWorldStore::UpsertCharacterManifestation(
    const FOGCharacterManifestationRecord& Manifestation,
    int64 CreatedWorldTick,
    FString& OutError)
{
    OutError.Reset();

    if (!Manifestation.ManifestationId.IsValid() ||
        !Manifestation.OwningRulerId.IsValid() ||
        !Manifestation.IdentityId.IsValid() ||
        !Manifestation.ActiveVersionId.IsValid() ||
        Manifestation.Level < 1)
    {
        OutError = TEXT("Character Manifestation contains invalid identity/ownership/progression data.");
        return false;
    }

    const bool bOwnTransaction = !bTransactionActive;
    if (bOwnTransaction && !BeginTransaction(OutError))
    {
        return false;
    }

    auto Fail = [this, bOwnTransaction, &OutError](const FString& Error)
    {
        OutError = Error;
        if (bOwnTransaction)
        {
            FString RollbackError;
            RollbackTransaction(RollbackError);
            if (!RollbackError.IsEmpty())
            {
                OutError += FString::Printf(TEXT(" | Rollback error: %s"), *RollbackError);
            }
        }
        return false;
    };

    bool bOwnerFound = false;
    FName OwnerKind = NAME_None;
    FString OwnerStateJson;
    int64 OwnerRevision = 0;
    FString OwnerReadError;
    if (!TryReadEntity(
            Manifestation.OwningRulerId,
            bOwnerFound,
            OwnerKind,
            OwnerStateJson,
            OwnerRevision,
            OwnerReadError))
    {
        return Fail(OwnerReadError);
    }

    if (!bOwnerFound)
    {
        return Fail(TEXT("Cannot persist a Character Manifestation for an unknown owning Ruler entity."));
    }

    // Manifestation metadata shares an entity with canonical anatomy, injury and
    // history. Updating its typed record must not erase those independent fields.
    bool bManifestationEntityFound = false;
    FName ManifestationEntityKind = NAME_None;
    FString ManifestationEntityStateJson;
    int64 ManifestationEntityRevision = 0;
    FString EntityError;
    if (!TryReadEntity(Manifestation.ManifestationId, bManifestationEntityFound,
            ManifestationEntityKind, ManifestationEntityStateJson,
            ManifestationEntityRevision, EntityError))
    {
        return Fail(EntityError);
    }
    if (!UpsertEntity(
            Manifestation.ManifestationId,
            TEXT("character_manifestation"),
            CreatedWorldTick,
            bManifestationEntityFound ? ManifestationEntityStateJson : TEXT("{}"),
            EntityError))
    {
        return Fail(EntityError);
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "INSERT INTO character_manifestations("
        "manifestation_entity_id, owning_ruler_entity_id, identity_content_id, "
        "active_version_content_id, level, current_rarity, progression_state_json, "
        "acquisition_world_tick, acquisition_ordinal, origin_pull_event_id, "
        "world_mode_anchor_territory_id, world_mode_anchor_tick, lifecycle_state, build_label"
        ") VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?) "
        "ON CONFLICT(manifestation_entity_id) DO UPDATE SET "
        "owning_ruler_entity_id = excluded.owning_ruler_entity_id, "
        "identity_content_id = excluded.identity_content_id, "
        "active_version_content_id = excluded.active_version_content_id, "
        "level = excluded.level, "
        "current_rarity = excluded.current_rarity, "
        "progression_state_json = excluded.progression_state_json, "
        "acquisition_world_tick = excluded.acquisition_world_tick, "
        "acquisition_ordinal = excluded.acquisition_ordinal, "
        "origin_pull_event_id = excluded.origin_pull_event_id, "
        "world_mode_anchor_territory_id = excluded.world_mode_anchor_territory_id, "
        "world_mode_anchor_tick = excluded.world_mode_anchor_tick, "
        "lifecycle_state = excluded.lifecycle_state, "
        "build_label = excluded.build_label;";

    if (sqlite3_prepare_v2(Database, Sql, -1, &Statement, nullptr) != SQLITE_OK)
    {
        return Fail(LastError(TEXT("Prepare Character Manifestation upsert")));
    }

    const bool bBound =
        BindStoreText(Statement, 1, Manifestation.ManifestationId.ToString()) &&
        BindStoreText(Statement, 2, Manifestation.OwningRulerId.ToString()) &&
        BindStoreText(Statement, 3, Manifestation.IdentityId.ToString()) &&
        BindStoreText(Statement, 4, Manifestation.ActiveVersionId.ToString()) &&
        sqlite3_bind_int(Statement, 5, Manifestation.Level) == SQLITE_OK &&
        BindStoreText(Statement, 6, Manifestation.CurrentRarity.ToString()) &&
        BindStoreText(
            Statement,
            7,
            Manifestation.ProgressionStateJson.IsEmpty()
                ? TEXT("{}")
                : Manifestation.ProgressionStateJson) &&
        sqlite3_bind_int64(
            Statement,
            8,
            Manifestation.AcquisitionWorldTick) == SQLITE_OK &&
        sqlite3_bind_int(
            Statement,
            9,
            Manifestation.AcquisitionOrdinal) == SQLITE_OK &&
        (Manifestation.OriginPullEventId.IsValid()
            ? BindStoreText(
                Statement,
                10,
                Manifestation.OriginPullEventId.ToString())
            : sqlite3_bind_null(Statement, 10) == SQLITE_OK) &&
        (Manifestation.WorldModeAnchorTerritoryId.IsValid()
            ? BindStoreText(
                Statement,
                11,
                Manifestation.WorldModeAnchorTerritoryId.ToString())
            : sqlite3_bind_null(Statement, 11) == SQLITE_OK) &&
        (Manifestation.WorldModeAnchorTerritoryId.IsValid()
            ? sqlite3_bind_int64(
                Statement,
                12,
                Manifestation.WorldModeAnchorTick) == SQLITE_OK
            : sqlite3_bind_null(Statement, 12) == SQLITE_OK) &&
        BindStoreText(
            Statement,
            13,
            Manifestation.LifecycleState.IsNone()
                ? TEXT("active")
                : Manifestation.LifecycleState.ToString()) &&
        BindStoreText(
            Statement,
            14,
            Manifestation.BuildLabel);

    const bool bSucceeded = bBound && sqlite3_step(Statement) == SQLITE_DONE;
    const FString SqlError = bSucceeded
        ? FString()
        : LastError(TEXT("Upsert Character Manifestation"));
    sqlite3_finalize(Statement);

    if (!bSucceeded)
    {
        return Fail(SqlError);
    }

    if (bOwnTransaction && !CommitTransaction(OutError))
    {
        FString RollbackError;
        RollbackTransaction(RollbackError);
        return false;
    }

    return true;
}

bool FOGSQLiteWorldStore::TryReadCharacterManifestation(
    const FOGEntityId& ManifestationId,
    bool& bOutFound,
    FOGCharacterManifestationRecord& OutManifestation,
    FString& OutError) const
{
    bOutFound = false;
    OutManifestation = FOGCharacterManifestationRecord();
    OutError.Reset();

    if (!ManifestationId.IsValid())
    {
        OutError = TEXT("Manifestation ID is invalid.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "SELECT owning_ruler_entity_id, identity_content_id, active_version_content_id, "
        "level, current_rarity, progression_state_json, acquisition_world_tick, "
        "acquisition_ordinal, origin_pull_event_id, world_mode_anchor_territory_id, "
        "world_mode_anchor_tick, lifecycle_state, build_label "
        "FROM character_manifestations WHERE manifestation_entity_id = ?;";

    if (sqlite3_prepare_v2(Database, Sql, -1, &Statement, nullptr) != SQLITE_OK)
    {
        OutError = LastError(TEXT("Prepare Character Manifestation read"));
        return false;
    }

    if (!BindStoreText(Statement, 1, ManifestationId.ToString()))
    {
        OutError = LastError(TEXT("Bind Character Manifestation read"));
        sqlite3_finalize(Statement);
        return false;
    }

    const int32 StepResult = sqlite3_step(Statement);
    if (StepResult == SQLITE_ROW)
    {
        bOutFound = true;
        OutManifestation.ManifestationId = ManifestationId;

        FGuid OwnerGuid;
        if (!FGuid::Parse(StoreColumnText(Statement, 0), OwnerGuid))
        {
            OutError = TEXT("Stored Character Manifestation has an invalid owning Ruler ID.");
            sqlite3_finalize(Statement);
            return false;
        }

        OutManifestation.OwningRulerId = FOGEntityId(OwnerGuid);
        OutManifestation.IdentityId = FOGContentId(StoreColumnText(Statement, 1));
        OutManifestation.ActiveVersionId = FOGContentId(StoreColumnText(Statement, 2));
        OutManifestation.Level = sqlite3_column_int(Statement, 3);
        OutManifestation.CurrentRarity = FName(*StoreColumnText(Statement, 4));
        OutManifestation.ProgressionStateJson = StoreColumnText(Statement, 5);
        OutManifestation.AcquisitionWorldTick = sqlite3_column_int64(Statement, 6);
        OutManifestation.AcquisitionOrdinal = sqlite3_column_int(Statement, 7);

        const FString OriginEvent = StoreColumnText(Statement, 8);
        if (!OriginEvent.IsEmpty())
        {
            FGuid OriginGuid;
            if (!FGuid::Parse(OriginEvent, OriginGuid))
            {
                OutError = TEXT("Stored Character Manifestation has an invalid origin pull event ID.");
                sqlite3_finalize(Statement);
                return false;
            }
            OutManifestation.OriginPullEventId = FOGEntityId(OriginGuid);
        }

        const FString AnchorTerritory = StoreColumnText(Statement, 9);
        if (!AnchorTerritory.IsEmpty())
        {
            FGuid AnchorGuid;
            if (!FGuid::Parse(AnchorTerritory, AnchorGuid))
            {
                OutError = TEXT("Stored Character Manifestation has an invalid anchor Territory ID.");
                sqlite3_finalize(Statement);
                return false;
            }
            OutManifestation.WorldModeAnchorTerritoryId = FOGEntityId(AnchorGuid);
        }

        OutManifestation.WorldModeAnchorTick =
            sqlite3_column_type(Statement, 10) == SQLITE_NULL
                ? 0
                : sqlite3_column_int64(Statement, 10);
        OutManifestation.LifecycleState =
            FName(*StoreColumnText(Statement, 11));
        OutManifestation.BuildLabel =
            StoreColumnText(Statement, 12);
    }
    else if (StepResult != SQLITE_DONE)
    {
        OutError = LastError(TEXT("Read Character Manifestation"));
        sqlite3_finalize(Statement);
        return false;
    }

    sqlite3_finalize(Statement);
    return true;
}

bool FOGSQLiteWorldStore::UpsertContentPackage(
    const FOGContentId& PackageId,
    int32 Version,
    const FString& ContentHash,
    bool bInstalled,
    bool bValidated,
    const FString& ManifestJson,
    FString& OutError)
{
    OutError.Reset();

    if (!PackageId.IsValid() || Version <= 0 || ContentHash.IsEmpty())
    {
        OutError = TEXT("Content package registration requires a valid ID, positive version, and content hash.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "INSERT INTO content_packages("
        "package_id, version, content_hash, installed, validated, activated, manifest_json"
        ") VALUES(?, ?, ?, ?, ?, 0, ?) "
        "ON CONFLICT(package_id) DO UPDATE SET "
        "activated = CASE "
        "WHEN content_packages.version = excluded.version "
        "AND content_packages.content_hash = excluded.content_hash "
        "AND excluded.installed = 1 "
        "AND excluded.validated = 1 "
        "THEN content_packages.activated ELSE 0 END, "
        "version = excluded.version, "
        "content_hash = excluded.content_hash, "
        "installed = excluded.installed, "
        "validated = excluded.validated, "
        "manifest_json = excluded.manifest_json;";

    if (sqlite3_prepare_v2(Database, Sql, -1, &Statement, nullptr) != SQLITE_OK)
    {
        OutError = LastError(TEXT("Prepare content package upsert"));
        return false;
    }

    const bool bBound =
        BindStoreText(Statement, 1, PackageId.ToString()) &&
        sqlite3_bind_int(Statement, 2, Version) == SQLITE_OK &&
        BindStoreText(Statement, 3, ContentHash) &&
        sqlite3_bind_int(Statement, 4, bInstalled ? 1 : 0) == SQLITE_OK &&
        sqlite3_bind_int(Statement, 5, bValidated ? 1 : 0) == SQLITE_OK &&
        BindStoreText(
            Statement,
            6,
            ManifestJson.IsEmpty() ? TEXT("{}") : ManifestJson);

    const bool bSucceeded = bBound && sqlite3_step(Statement) == SQLITE_DONE;
    if (!bSucceeded)
    {
        OutError = LastError(TEXT("Upsert content package"));
    }

    sqlite3_finalize(Statement);
    return bSucceeded;
}

bool FOGSQLiteWorldStore::SetContentPackageActivated(
    const FOGContentId& PackageId,
    bool bActivated,
    FString& OutError)
{
    OutError.Reset();

    if (!PackageId.IsValid())
    {
        OutError = TEXT("Package ID is invalid.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql = bActivated
        ? "UPDATE content_packages SET activated = 1 "
          "WHERE package_id = ? AND installed = 1 AND validated = 1;"
        : "UPDATE content_packages SET activated = 0 WHERE package_id = ?;";

    if (sqlite3_prepare_v2(Database, Sql, -1, &Statement, nullptr) != SQLITE_OK)
    {
        OutError = LastError(TEXT("Prepare package activation update"));
        return false;
    }

    const bool bBound = BindStoreText(Statement, 1, PackageId.ToString());
    const bool bStepped = bBound && sqlite3_step(Statement) == SQLITE_DONE;
    const int32 ChangedRows = bStepped ? sqlite3_changes(Database) : 0;

    if (!bStepped)
    {
        OutError = LastError(TEXT("Update package activation"));
        sqlite3_finalize(Statement);
        return false;
    }

    sqlite3_finalize(Statement);

    if (ChangedRows == 0)
    {
        OutError = bActivated
            ? TEXT("Package is unknown or has not been both installed and validated.")
            : TEXT("Package is unknown.");
        return false;
    }

    return true;
}

bool FOGSQLiteWorldStore::IsContentPackageActivated(
    const FOGContentId& PackageId,
    bool& bOutKnown,
    bool& bOutActivated,
    FString& OutError) const
{
    bOutKnown = false;
    bOutActivated = false;
    OutError.Reset();

    if (!PackageId.IsValid())
    {
        OutError = TEXT("Package ID is invalid.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql = "SELECT activated FROM content_packages WHERE package_id = ?;";

    if (sqlite3_prepare_v2(Database, Sql, -1, &Statement, nullptr) != SQLITE_OK)
    {
        OutError = LastError(TEXT("Prepare package activation read"));
        return false;
    }

    if (!BindStoreText(Statement, 1, PackageId.ToString()))
    {
        OutError = LastError(TEXT("Bind package activation read"));
        sqlite3_finalize(Statement);
        return false;
    }

    const int32 StepResult = sqlite3_step(Statement);
    if (StepResult == SQLITE_ROW)
    {
        bOutKnown = true;
        bOutActivated = sqlite3_column_int(Statement, 0) != 0;
    }
    else if (StepResult != SQLITE_DONE)
    {
        OutError = LastError(TEXT("Read package activation"));
        sqlite3_finalize(Statement);
        return false;
    }

    sqlite3_finalize(Statement);
    return true;
}
