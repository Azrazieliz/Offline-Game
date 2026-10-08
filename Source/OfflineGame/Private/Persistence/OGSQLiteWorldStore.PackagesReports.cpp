#include "Persistence/OGSQLiteWorldStore.h"

#include "sqlite/sqlite3.h"

namespace
{
bool BindP14Text(sqlite3_stmt* Statement, int32 Index, const FString& Value)
{
    FTCHARToUTF8 Utf8(*Value);
    return sqlite3_bind_text(
        Statement,
        Index,
        Utf8.Get(),
        Utf8.Length(),
        SQLITE_TRANSIENT) == SQLITE_OK;
}

FString P14Text(sqlite3_stmt* Statement, int32 Column)
{
    const unsigned char* Value = sqlite3_column_text(Statement, Column);
    return Value ? UTF8_TO_TCHAR(reinterpret_cast<const char*>(Value)) : FString();
}

FOGEntityId P14Entity(const FString& Value)
{
    if (Value.IsEmpty())
    {
        return FOGEntityId();
    }

    FGuid Guid;
    return FGuid::Parse(Value, Guid) ? FOGEntityId(Guid) : FOGEntityId();
}

bool BindP14OptionalEntity(sqlite3_stmt* Statement, int32 Index, const FOGEntityId& Value)
{
    return Value.IsValid()
        ? BindP14Text(Statement, Index, Value.ToString())
        : sqlite3_bind_null(Statement, Index) == SQLITE_OK;
}

bool BindP14OptionalTick(sqlite3_stmt* Statement, int32 Index, bool bHasValue, int64 Value)
{
    return bHasValue
        ? sqlite3_bind_int64(Statement, Index, Value) == SQLITE_OK
        : sqlite3_bind_null(Statement, Index) == SQLITE_OK;
}

bool ReadP14Count(sqlite3* Database, const char* Sql, int32& OutCount, FString& OutError)
{
    OutCount = 0;
    sqlite3_stmt* Statement = nullptr;
    if (sqlite3_prepare_v2(Database, Sql, -1, &Statement, nullptr) != SQLITE_OK)
    {
        OutError = TEXT("Failed to prepare migration-0014 validation query.");
        return false;
    }

    const int32 Step = sqlite3_step(Statement);
    if (Step != SQLITE_ROW)
    {
        OutError = TEXT("Failed to execute migration-0014 validation query.");
        sqlite3_finalize(Statement);
        return false;
    }

    OutCount = sqlite3_column_int(Statement, 0);
    sqlite3_finalize(Statement);
    return true;
}
}

bool FOGSQLiteWorldStore::UpsertContentPackageRecord(
    const FOGContentPackageRecord& Package,
    FString& OutError)
{
    OutError.Reset();

    if (!Package.PackageId.IsValid() ||
        Package.Version <= 0 ||
        Package.ContentHash.IsEmpty() ||
        Package.Category.IsNone() ||
        Package.StorageClass.IsNone() ||
        Package.SealedState.IsNone() ||
        Package.DownloadState.IsNone())
    {
        OutError = TEXT("Content package record is invalid.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "INSERT INTO content_packages("
        "package_id, version, content_hash, installed, validated, activated, manifest_json, "
        "category, install_uri, storage_class, sealed_state, download_state, compatibility_json"
        ") VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?) "
        "ON CONFLICT(package_id) DO UPDATE SET "
        "version = excluded.version, content_hash = excluded.content_hash, "
        "installed = excluded.installed, validated = excluded.validated, "
        "activated = CASE "
        "WHEN content_packages.version = excluded.version "
        "AND content_packages.content_hash = excluded.content_hash "
        "AND excluded.installed = 1 AND excluded.validated = 1 "
        "THEN excluded.activated ELSE 0 END, "
        "manifest_json = excluded.manifest_json, category = excluded.category, "
        "install_uri = excluded.install_uri, storage_class = excluded.storage_class, "
        "sealed_state = excluded.sealed_state, download_state = excluded.download_state, "
        "compatibility_json = excluded.compatibility_json;";

    if (sqlite3_prepare_v2(Database, Sql, -1, &Statement, nullptr) != SQLITE_OK)
    {
        OutError = LastError(TEXT("Prepare content package record upsert"));
        return false;
    }

    const bool bSucceeded =
        BindP14Text(Statement, 1, Package.PackageId.ToString()) &&
        sqlite3_bind_int(Statement, 2, Package.Version) == SQLITE_OK &&
        BindP14Text(Statement, 3, Package.ContentHash) &&
        sqlite3_bind_int(Statement, 4, Package.bInstalled ? 1 : 0) == SQLITE_OK &&
        sqlite3_bind_int(Statement, 5, Package.bValidated ? 1 : 0) == SQLITE_OK &&
        sqlite3_bind_int(Statement, 6, Package.bActivated ? 1 : 0) == SQLITE_OK &&
        BindP14Text(Statement, 7, Package.ManifestJson.IsEmpty() ? TEXT("{}") : Package.ManifestJson) &&
        BindP14Text(Statement, 8, Package.Category.ToString()) &&
        BindP14Text(Statement, 9, Package.InstallUri) &&
        BindP14Text(Statement, 10, Package.StorageClass.ToString()) &&
        BindP14Text(Statement, 11, Package.SealedState.ToString()) &&
        BindP14Text(Statement, 12, Package.DownloadState.ToString()) &&
        BindP14Text(Statement, 13, Package.CompatibilityJson.IsEmpty() ? TEXT("{}") : Package.CompatibilityJson) &&
        sqlite3_step(Statement) == SQLITE_DONE;

    if (!bSucceeded)
    {
        OutError = LastError(TEXT("Upsert content package record"));
    }
    sqlite3_finalize(Statement);
    return bSucceeded;
}

bool FOGSQLiteWorldStore::TryReadContentPackageRecord(
    const FOGContentId& PackageId,
    bool& bOutFound,
    FOGContentPackageRecord& OutPackage,
    FString& OutError) const
{
    bOutFound = false;
    OutPackage = FOGContentPackageRecord();
    OutError.Reset();

    if (!PackageId.IsValid())
    {
        OutError = TEXT("Content package lookup requires a valid Package ID.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "SELECT version, content_hash, installed, validated, activated, manifest_json, "
        "category, install_uri, storage_class, sealed_state, download_state, compatibility_json "
        "FROM content_packages WHERE package_id = ?;";

    if (sqlite3_prepare_v2(Database, Sql, -1, &Statement, nullptr) != SQLITE_OK)
    {
        OutError = LastError(TEXT("Prepare content package record read"));
        return false;
    }

    if (!BindP14Text(Statement, 1, PackageId.ToString()))
    {
        OutError = LastError(TEXT("Bind content package record read"));
        sqlite3_finalize(Statement);
        return false;
    }

    const int32 Step = sqlite3_step(Statement);
    if (Step == SQLITE_ROW)
    {
        bOutFound = true;
        OutPackage.PackageId = PackageId;
        OutPackage.Version = sqlite3_column_int(Statement, 0);
        OutPackage.ContentHash = P14Text(Statement, 1);
        OutPackage.bInstalled = sqlite3_column_int(Statement, 2) != 0;
        OutPackage.bValidated = sqlite3_column_int(Statement, 3) != 0;
        OutPackage.bActivated = sqlite3_column_int(Statement, 4) != 0;
        OutPackage.ManifestJson = P14Text(Statement, 5);
        OutPackage.Category = FName(*P14Text(Statement, 6));
        OutPackage.InstallUri = P14Text(Statement, 7);
        OutPackage.StorageClass = FName(*P14Text(Statement, 8));
        OutPackage.SealedState = FName(*P14Text(Statement, 9));
        OutPackage.DownloadState = FName(*P14Text(Statement, 10));
        OutPackage.CompatibilityJson = P14Text(Statement, 11);

        if (OutPackage.Version <= 0 ||
            OutPackage.ContentHash.IsEmpty() ||
            OutPackage.Category.IsNone() ||
            OutPackage.StorageClass.IsNone() ||
            OutPackage.SealedState.IsNone() ||
            OutPackage.DownloadState.IsNone())
        {
            OutError = TEXT("Stored content package record is invalid.");
            sqlite3_finalize(Statement);
            return false;
        }
    }
    else if (Step != SQLITE_DONE)
    {
        OutError = LastError(TEXT("Read content package record"));
        sqlite3_finalize(Statement);
        return false;
    }

    sqlite3_finalize(Statement);
    return true;
}

bool FOGSQLiteWorldStore::ListContentPackageRecords(
    TArray<FOGContentPackageRecord>& OutPackages,
    FString& OutError) const
{
    OutPackages.Reset();
    OutError.Reset();

    sqlite3_stmt* Statement = nullptr;
    const char* Sql = "SELECT package_id FROM content_packages ORDER BY package_id;";
    if (sqlite3_prepare_v2(Database, Sql, -1, &Statement, nullptr) != SQLITE_OK)
    {
        OutError = LastError(TEXT("Prepare content package record list"));
        return false;
    }

    TArray<FOGContentId> PackageIds;
    for (;;)
    {
        const int32 Step = sqlite3_step(Statement);
        if (Step == SQLITE_DONE)
        {
            break;
        }
        if (Step != SQLITE_ROW)
        {
            OutError = LastError(TEXT("Read content package record list"));
            sqlite3_finalize(Statement);
            return false;
        }

        FOGContentId Id(P14Text(Statement, 0));
        if (!Id.IsValid())
        {
            OutError = TEXT("Stored package list contains an invalid ID.");
            sqlite3_finalize(Statement);
            return false;
        }
        PackageIds.Add(MoveTemp(Id));
    }
    sqlite3_finalize(Statement);

    for (const FOGContentId& Id : PackageIds)
    {
        bool bFound = false;
        FOGContentPackageRecord Package;
        if (!TryReadContentPackageRecord(Id, bFound, Package, OutError) || !bFound)
        {
            if (OutError.IsEmpty())
            {
                OutError = TEXT("Content package disappeared during list read.");
            }
            OutPackages.Reset();
            return false;
        }
        OutPackages.Add(MoveTemp(Package));
    }

    return true;
}

bool FOGSQLiteWorldStore::UpsertPackageDependency(
    const FOGPackageDependencyRecord& Dependency,
    FString& OutError)
{
    OutError.Reset();

    if (!Dependency.PackageId.IsValid() ||
        !Dependency.DependencyPackageId.IsValid() ||
        Dependency.PackageId == Dependency.DependencyPackageId ||
        Dependency.MinimumVersion <= 0)
    {
        OutError = TEXT("Package dependency is invalid.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "INSERT INTO package_dependencies(package_id, dependency_package_id, minimum_version) "
        "VALUES(?, ?, ?) "
        "ON CONFLICT(package_id, dependency_package_id) DO UPDATE SET "
        "minimum_version = excluded.minimum_version;";

    if (sqlite3_prepare_v2(Database, Sql, -1, &Statement, nullptr) != SQLITE_OK)
    {
        OutError = LastError(TEXT("Prepare package dependency upsert"));
        return false;
    }

    const bool bSucceeded =
        BindP14Text(Statement, 1, Dependency.PackageId.ToString()) &&
        BindP14Text(Statement, 2, Dependency.DependencyPackageId.ToString()) &&
        sqlite3_bind_int(Statement, 3, Dependency.MinimumVersion) == SQLITE_OK &&
        sqlite3_step(Statement) == SQLITE_DONE;

    if (!bSucceeded)
    {
        OutError = LastError(TEXT("Upsert package dependency"));
    }
    sqlite3_finalize(Statement);
    return bSucceeded;
}

bool FOGSQLiteWorldStore::ListPackageDependencies(
    const FOGContentId& PackageId,
    TArray<FOGPackageDependencyRecord>& OutDependencies,
    FString& OutError) const
{
    OutDependencies.Reset();
    OutError.Reset();

    if (!PackageId.IsValid())
    {
        OutError = TEXT("Package dependency list requires a valid Package ID.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "SELECT dependency_package_id, minimum_version FROM package_dependencies "
        "WHERE package_id = ? ORDER BY dependency_package_id;";

    if (sqlite3_prepare_v2(Database, Sql, -1, &Statement, nullptr) != SQLITE_OK)
    {
        OutError = LastError(TEXT("Prepare package dependency list"));
        return false;
    }

    if (!BindP14Text(Statement, 1, PackageId.ToString()))
    {
        OutError = LastError(TEXT("Bind package dependency list"));
        sqlite3_finalize(Statement);
        return false;
    }

    for (;;)
    {
        const int32 Step = sqlite3_step(Statement);
        if (Step == SQLITE_DONE)
        {
            break;
        }
        if (Step != SQLITE_ROW)
        {
            OutError = LastError(TEXT("Read package dependency list"));
            sqlite3_finalize(Statement);
            OutDependencies.Reset();
            return false;
        }

        FOGPackageDependencyRecord Row;
        Row.PackageId = PackageId;
        Row.DependencyPackageId = FOGContentId(P14Text(Statement, 0));
        Row.MinimumVersion = sqlite3_column_int(Statement, 1);

        if (!Row.DependencyPackageId.IsValid() || Row.MinimumVersion <= 0)
        {
            OutError = TEXT("Stored package dependency is invalid.");
            sqlite3_finalize(Statement);
            OutDependencies.Reset();
            return false;
        }
        OutDependencies.Add(MoveTemp(Row));
    }

    sqlite3_finalize(Statement);
    return true;
}

bool FOGSQLiteWorldStore::UpsertReport(
    const FOGReportRecord& Report,
    int64 CreatedEntityWorldTick,
    FString& OutError)
{
    OutError.Reset();

    if (!Report.ReportId.IsValid() ||
        !Report.OwnerEntityId.IsValid() ||
        Report.Category.IsNone() ||
        Report.CreatedWorldTick < 0 ||
        (Report.bAcknowledged &&
         Report.AcknowledgedWorldTick < Report.CreatedWorldTick))
    {
        OutError = TEXT("Report record is invalid.");
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
        }
        return false;
    };

    FString Error;
    if (!UpsertEntity(
            Report.ReportId,
            FName(TEXT("report")),
            CreatedEntityWorldTick,
            TEXT("{}"),
            Error))
    {
        return Fail(Error);
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "INSERT INTO reports(report_entity_id, owner_entity_id, source_world_event_id, category, "
        "priority, created_world_tick, acknowledged_world_tick, payload_json) "
        "VALUES(?, ?, ?, ?, ?, ?, ?, ?) "
        "ON CONFLICT(report_entity_id) DO UPDATE SET "
        "owner_entity_id = excluded.owner_entity_id, "
        "source_world_event_id = excluded.source_world_event_id, category = excluded.category, "
        "priority = excluded.priority, created_world_tick = excluded.created_world_tick, "
        "acknowledged_world_tick = excluded.acknowledged_world_tick, payload_json = excluded.payload_json;";

    if (sqlite3_prepare_v2(Database, Sql, -1, &Statement, nullptr) != SQLITE_OK)
    {
        return Fail(LastError(TEXT("Prepare report upsert")));
    }

    const bool bSucceeded =
        BindP14Text(Statement, 1, Report.ReportId.ToString()) &&
        BindP14Text(Statement, 2, Report.OwnerEntityId.ToString()) &&
        BindP14OptionalEntity(Statement, 3, Report.SourceWorldEventId) &&
        BindP14Text(Statement, 4, Report.Category.ToString()) &&
        sqlite3_bind_int(Statement, 5, Report.Priority) == SQLITE_OK &&
        sqlite3_bind_int64(Statement, 6, Report.CreatedWorldTick) == SQLITE_OK &&
        BindP14OptionalTick(Statement, 7, Report.bAcknowledged, Report.AcknowledgedWorldTick) &&
        BindP14Text(Statement, 8, Report.PayloadJson.IsEmpty() ? TEXT("{}") : Report.PayloadJson) &&
        sqlite3_step(Statement) == SQLITE_DONE;

    const FString SqlError = bSucceeded ? FString() : LastError(TEXT("Upsert report"));
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

bool FOGSQLiteWorldStore::TryReadReport(
    const FOGEntityId& ReportId,
    bool& bOutFound,
    FOGReportRecord& OutReport,
    FString& OutError) const
{
    bOutFound = false;
    OutReport = FOGReportRecord();
    OutError.Reset();

    if (!ReportId.IsValid())
    {
        OutError = TEXT("Report lookup requires a valid ID.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "SELECT owner_entity_id, source_world_event_id, category, priority, created_world_tick, "
        "acknowledged_world_tick, payload_json FROM reports WHERE report_entity_id = ?;";

    if (sqlite3_prepare_v2(Database, Sql, -1, &Statement, nullptr) != SQLITE_OK)
    {
        OutError = LastError(TEXT("Prepare report read"));
        return false;
    }

    if (!BindP14Text(Statement, 1, ReportId.ToString()))
    {
        OutError = LastError(TEXT("Bind report read"));
        sqlite3_finalize(Statement);
        return false;
    }

    const int32 Step = sqlite3_step(Statement);
    if (Step == SQLITE_ROW)
    {
        bOutFound = true;
        OutReport.ReportId = ReportId;
        OutReport.OwnerEntityId = P14Entity(P14Text(Statement, 0));
        OutReport.SourceWorldEventId = P14Entity(P14Text(Statement, 1));
        OutReport.Category = FName(*P14Text(Statement, 2));
        OutReport.Priority = sqlite3_column_int(Statement, 3);
        OutReport.CreatedWorldTick = sqlite3_column_int64(Statement, 4);
        if (sqlite3_column_type(Statement, 5) != SQLITE_NULL)
        {
            OutReport.bAcknowledged = true;
            OutReport.AcknowledgedWorldTick = sqlite3_column_int64(Statement, 5);
        }
        OutReport.PayloadJson = P14Text(Statement, 6);

        if (!OutReport.OwnerEntityId.IsValid() || OutReport.Category.IsNone())
        {
            OutError = TEXT("Stored Report is invalid.");
            sqlite3_finalize(Statement);
            return false;
        }
    }
    else if (Step != SQLITE_DONE)
    {
        OutError = LastError(TEXT("Read report"));
        sqlite3_finalize(Statement);
        return false;
    }

    sqlite3_finalize(Statement);
    return true;
}

bool FOGSQLiteWorldStore::ListReportsByOwner(
    const FOGEntityId& OwnerId,
    TArray<FOGReportRecord>& OutReports,
    FString& OutError) const
{
    OutReports.Reset();
    OutError.Reset();

    if (!OwnerId.IsValid())
    {
        OutError = TEXT("Report list requires a valid owner.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "SELECT report_entity_id FROM reports WHERE owner_entity_id = ? "
        "ORDER BY acknowledged_world_tick IS NULL DESC, priority DESC, created_world_tick DESC;";

    if (sqlite3_prepare_v2(Database, Sql, -1, &Statement, nullptr) != SQLITE_OK)
    {
        OutError = LastError(TEXT("Prepare report list"));
        return false;
    }

    if (!BindP14Text(Statement, 1, OwnerId.ToString()))
    {
        OutError = LastError(TEXT("Bind report list"));
        sqlite3_finalize(Statement);
        return false;
    }

    TArray<FOGEntityId> Ids;
    for (;;)
    {
        const int32 Step = sqlite3_step(Statement);
        if (Step == SQLITE_DONE) break;
        if (Step != SQLITE_ROW)
        {
            OutError = LastError(TEXT("Read report list"));
            sqlite3_finalize(Statement);
            return false;
        }

        const FOGEntityId Id = P14Entity(P14Text(Statement, 0));
        if (!Id.IsValid())
        {
            OutError = TEXT("Stored report list contains an invalid ID.");
            sqlite3_finalize(Statement);
            return false;
        }
        Ids.Add(Id);
    }
    sqlite3_finalize(Statement);

    for (const FOGEntityId& Id : Ids)
    {
        bool bFound = false;
        FOGReportRecord Report;
        if (!TryReadReport(Id, bFound, Report, OutError) || !bFound)
        {
            if (OutError.IsEmpty()) OutError = TEXT("Report disappeared during list read.");
            OutReports.Reset();
            return false;
        }
        OutReports.Add(MoveTemp(Report));
    }
    return true;
}

bool FOGSQLiteWorldStore::UpsertReportDelivery(
    const FOGReportDeliveryRecord& Delivery,
    FString& OutError)
{
    OutError.Reset();

    if (!Delivery.ReportId.IsValid() ||
        Delivery.Channel.IsNone() ||
        Delivery.State.IsNone() ||
        Delivery.PrivacyState.IsNone())
    {
        OutError = TEXT("Report delivery is invalid.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "INSERT INTO report_delivery(report_entity_id, channel, state, scheduled_real_utc, "
        "delivered_real_utc, platform_notification_id, privacy_state) "
        "VALUES(?, ?, ?, ?, ?, ?, ?) "
        "ON CONFLICT(report_entity_id, channel) DO UPDATE SET "
        "state = excluded.state, scheduled_real_utc = excluded.scheduled_real_utc, "
        "delivered_real_utc = excluded.delivered_real_utc, "
        "platform_notification_id = excluded.platform_notification_id, "
        "privacy_state = excluded.privacy_state;";

    if (sqlite3_prepare_v2(Database, Sql, -1, &Statement, nullptr) != SQLITE_OK)
    {
        OutError = LastError(TEXT("Prepare report delivery upsert"));
        return false;
    }

    const bool bSucceeded =
        BindP14Text(Statement, 1, Delivery.ReportId.ToString()) &&
        BindP14Text(Statement, 2, Delivery.Channel.ToString()) &&
        BindP14Text(Statement, 3, Delivery.State.ToString()) &&
        (Delivery.ScheduledRealUtc.IsEmpty()
            ? sqlite3_bind_null(Statement, 4) == SQLITE_OK
            : BindP14Text(Statement, 4, Delivery.ScheduledRealUtc)) &&
        (Delivery.DeliveredRealUtc.IsEmpty()
            ? sqlite3_bind_null(Statement, 5) == SQLITE_OK
            : BindP14Text(Statement, 5, Delivery.DeliveredRealUtc)) &&
        (Delivery.PlatformNotificationId.IsEmpty()
            ? sqlite3_bind_null(Statement, 6) == SQLITE_OK
            : BindP14Text(Statement, 6, Delivery.PlatformNotificationId)) &&
        BindP14Text(Statement, 7, Delivery.PrivacyState.ToString()) &&
        sqlite3_step(Statement) == SQLITE_DONE;

    if (!bSucceeded)
    {
        OutError = LastError(TEXT("Upsert report delivery"));
    }
    sqlite3_finalize(Statement);
    return bSucceeded;
}

bool FOGSQLiteWorldStore::TryReadReportDelivery(
    const FOGEntityId& ReportId,
    FName Channel,
    bool& bOutFound,
    FOGReportDeliveryRecord& OutDelivery,
    FString& OutError) const
{
    bOutFound = false;
    OutDelivery = FOGReportDeliveryRecord();
    OutError.Reset();

    if (!ReportId.IsValid() || Channel.IsNone())
    {
        OutError = TEXT("Report delivery lookup is invalid.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "SELECT state, scheduled_real_utc, delivered_real_utc, platform_notification_id, privacy_state "
        "FROM report_delivery WHERE report_entity_id = ? AND channel = ?;";

    if (sqlite3_prepare_v2(Database, Sql, -1, &Statement, nullptr) != SQLITE_OK)
    {
        OutError = LastError(TEXT("Prepare report delivery read"));
        return false;
    }

    if (!BindP14Text(Statement, 1, ReportId.ToString()) ||
        !BindP14Text(Statement, 2, Channel.ToString()))
    {
        OutError = LastError(TEXT("Bind report delivery read"));
        sqlite3_finalize(Statement);
        return false;
    }

    const int32 Step = sqlite3_step(Statement);
    if (Step == SQLITE_ROW)
    {
        bOutFound = true;
        OutDelivery.ReportId = ReportId;
        OutDelivery.Channel = Channel;
        OutDelivery.State = FName(*P14Text(Statement, 0));
        OutDelivery.ScheduledRealUtc = P14Text(Statement, 1);
        OutDelivery.DeliveredRealUtc = P14Text(Statement, 2);
        OutDelivery.PlatformNotificationId = P14Text(Statement, 3);
        OutDelivery.PrivacyState = FName(*P14Text(Statement, 4));

        if (OutDelivery.State.IsNone() || OutDelivery.PrivacyState.IsNone())
        {
            OutError = TEXT("Stored Report delivery is invalid.");
            sqlite3_finalize(Statement);
            return false;
        }
    }
    else if (Step != SQLITE_DONE)
    {
        OutError = LastError(TEXT("Read report delivery"));
        sqlite3_finalize(Statement);
        return false;
    }

    sqlite3_finalize(Statement);
    return true;
}

bool FOGSQLiteWorldStore::ListReportDeliveries(
    const FOGEntityId& ReportId,
    TArray<FOGReportDeliveryRecord>& OutDeliveries,
    FString& OutError) const
{
    OutDeliveries.Reset();
    OutError.Reset();

    if (!ReportId.IsValid())
    {
        OutError = TEXT("Report delivery list requires a valid Report ID.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "SELECT channel FROM report_delivery WHERE report_entity_id = ? ORDER BY channel;";

    if (sqlite3_prepare_v2(Database, Sql, -1, &Statement, nullptr) != SQLITE_OK)
    {
        OutError = LastError(TEXT("Prepare report delivery list"));
        return false;
    }

    if (!BindP14Text(Statement, 1, ReportId.ToString()))
    {
        OutError = LastError(TEXT("Bind report delivery list"));
        sqlite3_finalize(Statement);
        return false;
    }

    TArray<FName> Channels;
    for (;;)
    {
        const int32 Step = sqlite3_step(Statement);
        if (Step == SQLITE_DONE) break;
        if (Step != SQLITE_ROW)
        {
            OutError = LastError(TEXT("Read report delivery list"));
            sqlite3_finalize(Statement);
            return false;
        }
        Channels.Add(FName(*P14Text(Statement, 0)));
    }
    sqlite3_finalize(Statement);

    for (FName Channel : Channels)
    {
        bool bFound = false;
        FOGReportDeliveryRecord Delivery;
        if (!TryReadReportDelivery(ReportId, Channel, bFound, Delivery, OutError) || !bFound)
        {
            if (OutError.IsEmpty()) OutError = TEXT("Report delivery disappeared during list read.");
            OutDeliveries.Reset();
            return false;
        }
        OutDeliveries.Add(MoveTemp(Delivery));
    }
    return true;
}

bool FOGSQLiteWorldStore::UpsertManifestationManagementMetadata(
    const FOGManifestationManagementMetadataRecord& Metadata,
    FString& OutError)
{
    OutError.Reset();

    if (!Metadata.ManifestationId.IsValid() || Metadata.UpdatedWorldTick < 0)
    {
        OutError = TEXT("Manifestation management metadata is invalid.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "INSERT INTO manifestation_management_metadata("
        "manifestation_entity_id, favorite, protected_state, locked_state, updated_world_tick, state_json"
        ") VALUES(?, ?, ?, ?, ?, ?) "
        "ON CONFLICT(manifestation_entity_id) DO UPDATE SET "
        "favorite = excluded.favorite, protected_state = excluded.protected_state, "
        "locked_state = excluded.locked_state, updated_world_tick = excluded.updated_world_tick, "
        "state_json = excluded.state_json;";

    if (sqlite3_prepare_v2(Database, Sql, -1, &Statement, nullptr) != SQLITE_OK)
    {
        OutError = LastError(TEXT("Prepare Manifestation management metadata upsert"));
        return false;
    }

    const bool bSucceeded =
        BindP14Text(Statement, 1, Metadata.ManifestationId.ToString()) &&
        sqlite3_bind_int(Statement, 2, Metadata.bFavorite ? 1 : 0) == SQLITE_OK &&
        sqlite3_bind_int(Statement, 3, Metadata.bProtected ? 1 : 0) == SQLITE_OK &&
        sqlite3_bind_int(Statement, 4, Metadata.bLocked ? 1 : 0) == SQLITE_OK &&
        sqlite3_bind_int64(Statement, 5, Metadata.UpdatedWorldTick) == SQLITE_OK &&
        BindP14Text(Statement, 6, Metadata.StateJson.IsEmpty() ? TEXT("{}") : Metadata.StateJson) &&
        sqlite3_step(Statement) == SQLITE_DONE;

    if (!bSucceeded)
    {
        OutError = LastError(TEXT("Upsert Manifestation management metadata"));
    }
    sqlite3_finalize(Statement);
    return bSucceeded;
}

bool FOGSQLiteWorldStore::TryReadManifestationManagementMetadata(
    const FOGEntityId& ManifestationId,
    bool& bOutFound,
    FOGManifestationManagementMetadataRecord& OutMetadata,
    FString& OutError) const
{
    bOutFound = false;
    OutMetadata = FOGManifestationManagementMetadataRecord();
    OutError.Reset();

    if (!ManifestationId.IsValid())
    {
        OutError = TEXT("Manifestation management metadata lookup requires a valid ID.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "SELECT favorite, protected_state, locked_state, updated_world_tick, state_json "
        "FROM manifestation_management_metadata WHERE manifestation_entity_id = ?;";

    if (sqlite3_prepare_v2(Database, Sql, -1, &Statement, nullptr) != SQLITE_OK)
    {
        OutError = LastError(TEXT("Prepare Manifestation management metadata read"));
        return false;
    }

    if (!BindP14Text(Statement, 1, ManifestationId.ToString()))
    {
        OutError = LastError(TEXT("Bind Manifestation management metadata read"));
        sqlite3_finalize(Statement);
        return false;
    }

    const int32 Step = sqlite3_step(Statement);
    if (Step == SQLITE_ROW)
    {
        bOutFound = true;
        OutMetadata.ManifestationId = ManifestationId;
        OutMetadata.bFavorite = sqlite3_column_int(Statement, 0) != 0;
        OutMetadata.bProtected = sqlite3_column_int(Statement, 1) != 0;
        OutMetadata.bLocked = sqlite3_column_int(Statement, 2) != 0;
        OutMetadata.UpdatedWorldTick = sqlite3_column_int64(Statement, 3);
        OutMetadata.StateJson = P14Text(Statement, 4);
    }
    else if (Step != SQLITE_DONE)
    {
        OutError = LastError(TEXT("Read Manifestation management metadata"));
        sqlite3_finalize(Statement);
        return false;
    }

    sqlite3_finalize(Statement);
    return true;
}

bool FOGSQLiteWorldStore::UpsertManifestationContextSelection(
    const FOGManifestationContextSelectionRecord& Selection,
    FString& OutError)
{
    OutError.Reset();

    if (!Selection.OwnerEntityId.IsValid() ||
        !Selection.ContextId.IsValid() ||
        !Selection.ManifestationId.IsValid() ||
        Selection.UpdatedWorldTick < 0)
    {
        OutError = TEXT("Manifestation context selection is invalid.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "INSERT INTO manifestation_context_selection("
        "owner_entity_id, context_content_id, manifestation_entity_id, updated_world_tick, state_json"
        ") VALUES(?, ?, ?, ?, ?) "
        "ON CONFLICT(owner_entity_id, context_content_id) DO UPDATE SET "
        "manifestation_entity_id = excluded.manifestation_entity_id, "
        "updated_world_tick = excluded.updated_world_tick, state_json = excluded.state_json;";

    if (sqlite3_prepare_v2(Database, Sql, -1, &Statement, nullptr) != SQLITE_OK)
    {
        OutError = LastError(TEXT("Prepare Manifestation context selection upsert"));
        return false;
    }

    const bool bSucceeded =
        BindP14Text(Statement, 1, Selection.OwnerEntityId.ToString()) &&
        BindP14Text(Statement, 2, Selection.ContextId.ToString()) &&
        BindP14Text(Statement, 3, Selection.ManifestationId.ToString()) &&
        sqlite3_bind_int64(Statement, 4, Selection.UpdatedWorldTick) == SQLITE_OK &&
        BindP14Text(Statement, 5, Selection.StateJson.IsEmpty() ? TEXT("{}") : Selection.StateJson) &&
        sqlite3_step(Statement) == SQLITE_DONE;

    if (!bSucceeded)
    {
        OutError = LastError(TEXT("Upsert Manifestation context selection"));
    }
    sqlite3_finalize(Statement);
    return bSucceeded;
}

bool FOGSQLiteWorldStore::TryReadManifestationContextSelection(
    const FOGEntityId& OwnerId,
    const FOGContentId& ContextId,
    bool& bOutFound,
    FOGManifestationContextSelectionRecord& OutSelection,
    FString& OutError) const
{
    bOutFound = false;
    OutSelection = FOGManifestationContextSelectionRecord();
    OutError.Reset();

    if (!OwnerId.IsValid() || !ContextId.IsValid())
    {
        OutError = TEXT("Manifestation context selection lookup is invalid.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "SELECT manifestation_entity_id, updated_world_tick, state_json "
        "FROM manifestation_context_selection WHERE owner_entity_id = ? AND context_content_id = ?;";

    if (sqlite3_prepare_v2(Database, Sql, -1, &Statement, nullptr) != SQLITE_OK)
    {
        OutError = LastError(TEXT("Prepare Manifestation context selection read"));
        return false;
    }

    if (!BindP14Text(Statement, 1, OwnerId.ToString()) ||
        !BindP14Text(Statement, 2, ContextId.ToString()))
    {
        OutError = LastError(TEXT("Bind Manifestation context selection read"));
        sqlite3_finalize(Statement);
        return false;
    }

    const int32 Step = sqlite3_step(Statement);
    if (Step == SQLITE_ROW)
    {
        bOutFound = true;
        OutSelection.OwnerEntityId = OwnerId;
        OutSelection.ContextId = ContextId;
        OutSelection.ManifestationId = P14Entity(P14Text(Statement, 0));
        OutSelection.UpdatedWorldTick = sqlite3_column_int64(Statement, 1);
        OutSelection.StateJson = P14Text(Statement, 2);

        if (!OutSelection.ManifestationId.IsValid())
        {
            OutError = TEXT("Stored Manifestation context selection is invalid.");
            sqlite3_finalize(Statement);
            return false;
        }
    }
    else if (Step != SQLITE_DONE)
    {
        OutError = LastError(TEXT("Read Manifestation context selection"));
        sqlite3_finalize(Statement);
        return false;
    }

    sqlite3_finalize(Statement);
    return true;
}

bool FOGSQLiteWorldStore::MigratePackagesReportsManagement0014(FString& OutError)
{
    OutError.Reset();

    // Existing content packages receive explicit compatibility defaults only.
    // Reports, delivery projections and management guards cannot be reconstructed
    // safely from older generic state and are intentionally not fabricated.
    return true;
}

bool FOGSQLiteWorldStore::ValidatePackagesReportsManagementMigration0014(
    FString& OutError) const
{
    OutError.Reset();

    int32 InvalidPackages = 0;
    if (!ReadP14Count(
            Database,
            "SELECT COUNT(*) FROM content_packages WHERE version <= 0 "
            "OR category = '' OR storage_class = '' OR sealed_state = '' OR download_state = '';",
            InvalidPackages,
            OutError))
    {
        return false;
    }
    if (InvalidPackages != 0)
    {
        OutError = FString::Printf(TEXT("Migration 0014 produced %d invalid package records."), InvalidPackages);
        return false;
    }

    int32 InvalidDependencies = 0;
    if (!ReadP14Count(
            Database,
            "SELECT COUNT(*) FROM package_dependencies "
            "WHERE package_id = dependency_package_id OR minimum_version <= 0;",
            InvalidDependencies,
            OutError))
    {
        return false;
    }
    if (InvalidDependencies != 0)
    {
        OutError = FString::Printf(TEXT("Migration 0014 produced %d invalid package dependencies."), InvalidDependencies);
        return false;
    }

    int32 DependencyCycles = 0;
    if (!ReadP14Count(
            Database,
            "WITH RECURSIVE reach(start_package_id, dependency_package_id) AS ("
            "SELECT package_id, dependency_package_id FROM package_dependencies "
            "UNION "
            "SELECT reach.start_package_id, d.dependency_package_id "
            "FROM reach JOIN package_dependencies d "
            "ON d.package_id = reach.dependency_package_id"
            ") "
            "SELECT COUNT(*) FROM reach "
            "WHERE start_package_id = dependency_package_id;",
            DependencyCycles,
            OutError))
    {
        return false;
    }
    if (DependencyCycles != 0)
    {
        OutError = FString::Printf(
            TEXT("Migration 0014 contains %d package dependency cycle path(s)."),
            DependencyCycles);
        return false;
    }

    int32 InvalidActivatedPackages = 0;
    if (!ReadP14Count(
            Database,
            "SELECT COUNT(*) FROM content_packages "
            "WHERE activated = 1 AND "
            "(installed <> 1 OR validated <> 1 OR lower(download_state) <> 'installed');",
            InvalidActivatedPackages,
            OutError))
    {
        return false;
    }
    if (InvalidActivatedPackages != 0)
    {
        OutError = FString::Printf(
            TEXT("Migration 0014 contains %d activated packages that are not installation/validation ready."),
            InvalidActivatedPackages);
        return false;
    }

    int32 InvalidActiveDependencyEdges = 0;
    if (!ReadP14Count(
            Database,
            "SELECT COUNT(*) "
            "FROM package_dependencies d "
            "JOIN content_packages p ON p.package_id = d.package_id "
            "JOIN content_packages required ON required.package_id = d.dependency_package_id "
            "WHERE p.activated = 1 AND "
            "(required.version < d.minimum_version OR "
            " required.installed <> 1 OR required.validated <> 1 OR "
            " required.activated <> 1 OR lower(required.download_state) <> 'installed');",
            InvalidActiveDependencyEdges,
            OutError))
    {
        return false;
    }
    if (InvalidActiveDependencyEdges != 0)
    {
        OutError = FString::Printf(
            TEXT("Migration 0014 contains %d active package dependency edges that are not activation-ready."),
            InvalidActiveDependencyEdges);
        return false;
    }

    int32 InvalidReports = 0;
    if (!ReadP14Count(
            Database,
            "SELECT COUNT(*) FROM reports WHERE category = '' OR created_world_tick < 0 "
            "OR (acknowledged_world_tick IS NOT NULL AND acknowledged_world_tick < created_world_tick);",
            InvalidReports,
            OutError))
    {
        return false;
    }
    if (InvalidReports != 0)
    {
        OutError = FString::Printf(TEXT("Migration 0014 produced %d invalid Reports."), InvalidReports);
        return false;
    }

    int32 InvalidManagement = 0;
    if (!ReadP14Count(
            Database,
            "SELECT COUNT(*) FROM manifestation_management_metadata "
            "WHERE favorite NOT IN (0,1) OR protected_state NOT IN (0,1) OR locked_state NOT IN (0,1);",
            InvalidManagement,
            OutError))
    {
        return false;
    }
    if (InvalidManagement != 0)
    {
        OutError = FString::Printf(TEXT("Migration 0014 produced %d invalid Manifestation management rows."), InvalidManagement);
        return false;
    }

    int32 InvalidContextSelections = 0;
    if (!ReadP14Count(
            Database,
            "SELECT COUNT(*) "
            "FROM manifestation_context_selection s "
            "JOIN character_manifestations m "
            "ON m.manifestation_entity_id = s.manifestation_entity_id "
            "WHERE s.owner_entity_id <> m.owning_ruler_entity_id;",
            InvalidContextSelections,
            OutError))
    {
        return false;
    }
    if (InvalidContextSelections != 0)
    {
        OutError = FString::Printf(
            TEXT("Migration 0014 contains %d cross-owner Manifestation context selections."),
            InvalidContextSelections);
        return false;
    }

    int32 InvalidDeliveries = 0;
    if (!ReadP14Count(
            Database,
            "SELECT COUNT(*) FROM report_delivery "
            "WHERE channel = '' OR state = '' OR privacy_state = '';",
            InvalidDeliveries,
            OutError))
    {
        return false;
    }
    if (InvalidDeliveries != 0)
    {
        OutError = FString::Printf(
            TEXT("Migration 0014 contains %d invalid Report delivery rows."),
            InvalidDeliveries);
        return false;
    }

    return true;
}
