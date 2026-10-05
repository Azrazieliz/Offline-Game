#include "Persistence/OGRecoveryCatalogService.h"
#include "Persistence/OGSQLiteWorldStore.h"
#include "Persistence/OGSnapshotService.h"
#include "Progression/OGGrandConvergenceService.h"
#include "Runtime/OGManifestationManagementService.h"
#include "Runtime/OGPackageManagerService.h"
#include "Runtime/OGPlayerProfileSettings.h"
#include "Runtime/OGReportService.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "sqlite/sqlite3.h"

namespace
{
FString Make0014TestDirectory()
{
    return FPaths::Combine(
        FPaths::ProjectSavedDir(),
        TEXT("Automation"),
        FGuid::NewGuid().ToString(EGuidFormats::Digits));
}

bool Persist0014Entity(
    FOGSQLiteWorldStore& Store,
    const FOGEntityId& Id,
    FName Kind,
    FString& Error)
{
    return Store.UpsertEntity(
        Id,
        Kind,
        0,
        TEXT("{}"),
        Error);
}

bool Persist0014Manifestation(
    FOGSQLiteWorldStore& Store,
    const FOGEntityId& ManifestationId,
    const FOGEntityId& RulerId,
    const FOGContentId& IdentityId,
    const FOGContentId& VersionId,
    int32 Ordinal,
    FString& Error)
{
    FOGCharacterManifestationRecord Manifestation;
    Manifestation.ManifestationId = ManifestationId;
    Manifestation.OwningRulerId = RulerId;
    Manifestation.IdentityId = IdentityId;
    Manifestation.ActiveVersionId = VersionId;
    Manifestation.Level = 1;
    Manifestation.AcquisitionWorldTick = 10 + Ordinal;
    Manifestation.AcquisitionOrdinal = Ordinal;
    Manifestation.LifecycleState = FName(TEXT("active"));
    return Store.UpsertCharacterManifestation(
        Manifestation,
        Manifestation.AcquisitionWorldTick,
        Error);
}

bool MaxReinforce0014(
    FOGSQLiteWorldStore& Store,
    const FOGEntityId& ManifestationId,
    FString& Error)
{
    FOGManifestationReinforcementRecord State;
    State.ManifestationId = ManifestationId;
    State.ReinforcementState = FName(TEXT("max_reinforced"));
    State.bMaxReinforced = true;
    State.UpdatedWorldTick = 20;
    return Store.UpsertManifestationReinforcement(
        State,
        Error);
}

bool ReadEventPayload0014(
    const FString& DatabasePath,
    const FOGEntityId& EventId,
    FString& OutPayload,
    FString& OutError)
{
    OutPayload.Reset();
    OutError.Reset();

    sqlite3* Database = nullptr;
    FTCHARToUTF8 PathUtf8(*DatabasePath);
    if (sqlite3_open_v2(
            PathUtf8.Get(),
            &Database,
            SQLITE_OPEN_READONLY | SQLITE_OPEN_FULLMUTEX,
            nullptr) != SQLITE_OK ||
        !Database)
    {
        OutError = TEXT("Failed to open test database read-only.");
        if (Database) sqlite3_close_v2(Database);
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "SELECT payload_json FROM world_events WHERE event_id = ?;";
    if (sqlite3_prepare_v2(Database, Sql, -1, &Statement, nullptr) != SQLITE_OK)
    {
        OutError = TEXT("Failed to prepare source-event read.");
        sqlite3_close_v2(Database);
        return false;
    }

    FTCHARToUTF8 IdUtf8(*EventId.ToString());
    sqlite3_bind_text(
        Statement,
        1,
        IdUtf8.Get(),
        IdUtf8.Length(),
        SQLITE_TRANSIENT);

    const int32 Step = sqlite3_step(Statement);
    if (Step == SQLITE_ROW)
    {
        const unsigned char* Text = sqlite3_column_text(Statement, 0);
        OutPayload = Text
            ? UTF8_TO_TCHAR(reinterpret_cast<const char*>(Text))
            : FString();
    }
    else
    {
        OutError = TEXT("Source world event was not found.");
    }

    sqlite3_finalize(Statement);
    sqlite3_close_v2(Database);
    return Step == SQLITE_ROW;
}

class FOGTestAndroidNotificationBridge final
    : public IOGAndroidNotificationBridge
{
public:
    bool bCalled = false;

    virtual bool ProjectNotification(
        const FOGReportRecord& Report,
        const FOGReportDeliveryRecord& Delivery,
        FString& OutPlatformNotificationId,
        FString& OutError) override
    {
        OutError.Reset();
        bCalled =
            Report.ReportId.IsValid() &&
            Delivery.Channel ==
                FName(TEXT("android_notification"));
        OutPlatformNotificationId =
            TEXT("test-notification-001");
        return bCalled;
    }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOGPackageDependencyLifecycleTest,
    "OfflineGame.Runtime.Packages.MinimumVersionsCyclesAndActivation",
    EAutomationTestFlags::ApplicationContextMask |
        EAutomationTestFlags::EngineFilter)

bool FOGPackageDependencyLifecycleTest::RunTest(
    const FString& Parameters)
{
    const FString Directory = Make0014TestDirectory();
    const FString DatabasePath = FPaths::Combine(Directory, TEXT("packages.db"));
    IFileManager::Get().MakeDirectory(*Directory, true);

    FOGSQLiteWorldStore Store;
    FString Error;
    TestTrue(TEXT("Open schema-14 database"), Store.Open(DatabasePath, Error));
    TestEqual(TEXT("Schema version is 14"), Store.GetSchemaVersion(Error), 14);

    FOGPackageManagerService Packages(Store);

    FOGContentPackageRecord Core;
    Core.PackageId = FOGContentId(TEXT("test:package.core"));
    Core.Version = 1;
    Core.ContentHash = TEXT("hash-core-v1");
    Core.bInstalled = true;
    Core.bValidated = true;
    Core.DownloadState = FName(TEXT("installed"));

    FOGContentPackageRecord Expansion;
    Expansion.PackageId = FOGContentId(TEXT("test:package.expansion"));
    Expansion.Version = 1;
    Expansion.ContentHash = TEXT("hash-expansion-v1");
    Expansion.bInstalled = true;
    Expansion.bValidated = true;
    Expansion.DownloadState = FName(TEXT("installed"));

    TestTrue(TEXT("Register core package"), Packages.RegisterPackage(Core, Error));
    TestTrue(TEXT("Register expansion package"), Packages.RegisterPackage(Expansion, Error));
    TestTrue(TEXT("Activate dependency root"), Packages.ActivatePackage(Core.PackageId, Error));

    FOGPackageDependencyRecord TooNew;
    TooNew.PackageId = Expansion.PackageId;
    TooNew.DependencyPackageId = Core.PackageId;
    TooNew.MinimumVersion = 2;
    TestFalse(
        TEXT("Dependency minimum version is enforced transactionally"),
        Packages.SetDependency(
            TooNew,
            Error));

    TArray<FOGPackageDependencyRecord> Dependencies;
    TestTrue(
        TEXT("Read dependencies after rejected minimum"),
        Store.ListPackageDependencies(
            Expansion.PackageId,
            Dependencies,
            Error));
    TestTrue(
        TEXT("Rejected dependency was rolled back"),
        Dependencies.IsEmpty());

    Core.Version = 2;
    Core.ContentHash = TEXT("hash-core-v2");
    TestTrue(TEXT("Update core package to required version"), Packages.RegisterPackage(Core, Error));
    TestTrue(TEXT("Updated package must be reactivated"), Packages.ActivatePackage(Core.PackageId, Error));

    FOGPackageDependencyRecord ValidDependency = TooNew;
    TestTrue(TEXT("Accept satisfiable dependency"), Packages.SetDependency(ValidDependency, Error));
    TestTrue(TEXT("Activate expansion after ready dependency"), Packages.ActivatePackage(Expansion.PackageId, Error));

    FOGPackageDependencyRecord Cycle;
    Cycle.PackageId = Core.PackageId;
    Cycle.DependencyPackageId = Expansion.PackageId;
    Cycle.MinimumVersion = 1;
    TestFalse(
        TEXT("Dependency cycle is rejected"),
        Packages.SetDependency(
            Cycle,
            Error));
    TestTrue(
        TEXT("Cycle rejection leaves graph valid"),
        Packages.ValidateDependencyGraph(
            Error));

    Store.Close();
    IFileManager::Get().DeleteDirectory(*Directory, false, true);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOGReportProjectionTest,
    "OfflineGame.Runtime.Reports.AcknowledgementAndAndroidProjectionDoNotRewriteWorldEvent",
    EAutomationTestFlags::ApplicationContextMask |
        EAutomationTestFlags::EngineFilter)

bool FOGReportProjectionTest::RunTest(
    const FString& Parameters)
{
    const FString Directory = Make0014TestDirectory();
    const FString DatabasePath = FPaths::Combine(Directory, TEXT("reports.db"));
    IFileManager::Get().MakeDirectory(*Directory, true);

    FOGSQLiteWorldStore Store;
    FString Error;
    TestTrue(TEXT("Open database"), Store.Open(DatabasePath, Error));

    const FOGEntityId OwnerId = FOGEntityId::NewId();
    TestTrue(TEXT("Persist Report owner"), Persist0014Entity(Store, OwnerId, FName(TEXT("ruler")), Error));

    FOGWorldEvent SourceEvent;
    SourceEvent.EventId = FOGEntityId::NewId();
    SourceEvent.EventType = FName(TEXT("territory.raid_detected"));
    SourceEvent.WorldTick = 100;
    SourceEvent.PrimaryEntity = OwnerId;
    SourceEvent.PayloadJson = TEXT("{\"immutable_source\":\"raid_alpha\"}");
    TestTrue(TEXT("Persist source world event"), Store.AppendWorldEvent(SourceEvent, Error));

    FString PayloadBefore;
    TestTrue(TEXT("Read source event before Report projection"), ReadEventPayload0014(DatabasePath, SourceEvent.EventId, PayloadBefore, Error));

    FOGReportService Reports(Store);
    FOGEntityId ReportId;
    TestTrue(
        TEXT("Create Report projection"),
        Reports.CreateReport(
            OwnerId,
            SourceEvent.EventId,
            FName(TEXT("threat")),
            90,
            101,
            TEXT("{\"summary\":\"raid detected\"}"),
            ReportId,
            Error));
    TestTrue(TEXT("Acknowledge Report"), Reports.AcknowledgeReport(ReportId, 102, Error));

    FOGTestAndroidNotificationBridge Bridge;
    TestTrue(
        TEXT("Project Report to Android notification bridge"),
        Reports.DeliverAndroidProjection(
            ReportId,
            FName(TEXT("redacted")),
            TEXT("2026-10-05T10:00:00Z"),
            TEXT("2026-10-05T10:00:01Z"),
            Bridge,
            Error));
    TestTrue(TEXT("Android bridge was invoked"), Bridge.bCalled);

    bool bFound = false;
    FOGReportDeliveryRecord Delivery;
    TestTrue(
        TEXT("Read persisted Android delivery projection"),
        Store.TryReadReportDelivery(
            ReportId,
            FName(TEXT("android_notification")),
            bFound,
            Delivery,
            Error));
    TestTrue(TEXT("Delivery projection exists"), bFound);
    TestEqual(TEXT("Delivery is marked delivered"), Delivery.State, FName(TEXT("delivered")));
    TestEqual(TEXT("Platform notification ID is retained"), Delivery.PlatformNotificationId, FString(TEXT("test-notification-001")));

    FString PayloadAfter;
    TestTrue(TEXT("Read source event after Report operations"), ReadEventPayload0014(DatabasePath, SourceEvent.EventId, PayloadAfter, Error));
    TestEqual(TEXT("Report acknowledgement/delivery never rewrites source event"), PayloadAfter, PayloadBefore);

    Store.Close();
    IFileManager::Get().DeleteDirectory(*Directory, false, true);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOGProfileAndRecoverySidecarsTest,
    "OfflineGame.Runtime.Sidecars.ProfileOrientationAndRecoveryCatalogStayOutsideWorldCausality",
    EAutomationTestFlags::ApplicationContextMask |
        EAutomationTestFlags::EngineFilter)

bool FOGProfileAndRecoverySidecarsTest::RunTest(
    const FString& Parameters)
{
    const FString Directory = Make0014TestDirectory();
    const FString DatabasePath = FPaths::Combine(Directory, TEXT("world.db"));
    const FString SettingsPath = FPaths::Combine(Directory, TEXT("Profile"), TEXT("settings.json"));
    const FString BackupDirectory = FPaths::Combine(Directory, TEXT("Backups"));
    const FString CatalogPath = FPaths::Combine(Directory, TEXT("RecoveryCatalog.json"));
    IFileManager::Get().MakeDirectory(*Directory, true);

    FOGPlayerProfileSettings Defaults;
    TestEqual(
        TEXT("Orientation defaults to automatic"),
        Defaults.OrientationLock,
        FName(TEXT("automatic")));
    TestEqual(
        TEXT("Automatic orientation follows device"),
        FOGPlayerProfileSettingsService::ResolveOrientation(
            Defaults,
            FName(TEXT("landscape"))),
        FName(TEXT("landscape")));

    FOGPlayerProfileSettings Profile = Defaults;
    Profile.bSfwPresentation = true;
    Profile.OrientationLock = FName(TEXT("portrait"));
    Profile.RosterDensity = FName(TEXT("dense"));
    Profile.bReducedMotion = true;
    Profile.AutoCombatPresetJson = TEXT("{\"focus\":\"weakest\"}");

    FString Error;
    TestTrue(TEXT("Save non-authoritative profile sidecar"), FOGPlayerProfileSettingsService::Save(SettingsPath, Profile, Error));
    TestEqual(
        TEXT("Player orientation lock overrides device"),
        FOGPlayerProfileSettingsService::ResolveOrientation(
            Profile,
            FName(TEXT("landscape"))),
        FName(TEXT("portrait")));

    FOGSQLiteWorldStore Store;
    TestTrue(TEXT("Open canonical world database"), Store.Open(DatabasePath, Error));
    const FOGEntityId MarkerId = FOGEntityId::NewId();
    TestTrue(TEXT("Persist canonical marker"), Persist0014Entity(Store, MarkerId, FName(TEXT("world_marker")), Error));

    FString SnapshotPath;
    TestTrue(
        TEXT("Create cataloged canonical backup"),
        FOGSnapshotService::CreateRotatingSnapshot(
            Store,
            BackupDirectory,
            3,
            CatalogPath,
            TEXT("test:world.alpha"),
            TEXT("test-build"),
            SnapshotPath,
            Error));
    Store.Close();

    TArray<FOGBackupCatalogEntry> Entries;
    TestTrue(TEXT("Load external recovery catalog"), FOGRecoveryCatalogService::LoadEntries(CatalogPath, Entries, Error));
    TestEqual(TEXT("Catalog contains snapshot metadata"), Entries.Num(), 1);
    TestTrue(TEXT("Catalog records backup hash"), Entries.Num() == 1 && !Entries[0].ContentHash.IsEmpty());

    TestTrue(
        TEXT("Clear World preserves backups by default"),
        FOGRecoveryCatalogService::ClearWorld(
            DatabasePath,
            CatalogPath,
            BackupDirectory,
            false,
            Error));
    TestFalse(TEXT("Authoritative world database was cleared"), IFileManager::Get().FileExists(*DatabasePath));
    TestTrue(TEXT("Backup survives Clear World"), IFileManager::Get().FileExists(*SnapshotPath));
    TestTrue(TEXT("Recovery catalog survives Clear World"), IFileManager::Get().FileExists(*CatalogPath));
    TestTrue(TEXT("Profile sidecar survives Clear World"), IFileManager::Get().FileExists(*SettingsPath));

    FOGPlayerProfileSettings Reloaded;
    TestTrue(TEXT("Reload player profile sidecar"), FOGPlayerProfileSettingsService::Load(SettingsPath, Reloaded, Error));
    TestTrue(TEXT("Privacy/SFW setting remains outside canonical DB lifecycle"), Reloaded.bSfwPresentation);
    TestEqual(TEXT("Roster density survives sidecar reload"), Reloaded.RosterDensity, FName(TEXT("dense")));

    TestTrue(
        TEXT("Explicit backup deletion may remove catalog/backups"),
        FOGRecoveryCatalogService::ClearWorld(
            DatabasePath,
            CatalogPath,
            BackupDirectory,
            true,
            Error));
    TestFalse(TEXT("Catalog removed only after explicit backup deletion"), IFileManager::Get().FileExists(*CatalogPath));

    IFileManager::Get().DeleteDirectory(*Directory, false, true);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOGProtectedManifestationConvergenceTest,
    "OfflineGame.Runtime.Management.ProtectedManifestationSurvivesBackupAndBlocksConvergenceUntilConfirmed",
    EAutomationTestFlags::ApplicationContextMask |
        EAutomationTestFlags::EngineFilter)

bool FOGProtectedManifestationConvergenceTest::RunTest(
    const FString& Parameters)
{
    const FString Directory = Make0014TestDirectory();
    const FString DatabasePath = FPaths::Combine(Directory, TEXT("protected.db"));
    const FString BackupPath = FPaths::Combine(Directory, TEXT("protected_backup.db"));
    IFileManager::Get().MakeDirectory(*Directory, true);

    FOGSQLiteWorldStore Store;
    FString Error;
    TestTrue(TEXT("Open database"), Store.Open(DatabasePath, Error));

    const FOGEntityId RulerId = FOGEntityId::NewId();
    const FOGEntityId SourceA = FOGEntityId::NewId();
    const FOGEntityId SourceB = FOGEntityId::NewId();
    const FOGEntityId ResultId = FOGEntityId::NewId();
    const FOGContentId IdentityId(TEXT("test:identity.convergence"));
    const FOGContentId VersionId(TEXT("test:version.convergence_base"));

    TestTrue(TEXT("Persist Ruler"), Persist0014Entity(Store, RulerId, FName(TEXT("ruler")), Error));
    TestTrue(TEXT("Persist source A"), Persist0014Manifestation(Store, SourceA, RulerId, IdentityId, VersionId, 0, Error));
    TestTrue(TEXT("Persist source B"), Persist0014Manifestation(Store, SourceB, RulerId, IdentityId, VersionId, 1, Error));
    TestTrue(TEXT("Max-reinforce source A"), MaxReinforce0014(Store, SourceA, Error));
    TestTrue(TEXT("Max-reinforce source B"), MaxReinforce0014(Store, SourceB, Error));

    FOGManifestationManagementService Management(Store);
    TestTrue(
        TEXT("Protect source A"),
        Management.SetFlags(
            SourceA,
            true,
            true,
            true,
            25,
            Error));

    TestTrue(TEXT("Backup canonical protection metadata"), Store.BackupTo(BackupPath, Error));

    FOGSQLiteWorldStore Backup;
    TestTrue(TEXT("Open backup on simulated migrated device"), Backup.Open(BackupPath, Error));
    bool bMetadataFound = false;
    FOGManifestationManagementMetadataRecord BackupMetadata;
    TestTrue(
        TEXT("Read Protected/Locked state from backup"),
        Backup.TryReadManifestationManagementMetadata(
            SourceA,
            bMetadataFound,
            BackupMetadata,
            Error));
    TestTrue(TEXT("Protection metadata survives backup/device transfer"), bMetadataFound && BackupMetadata.bProtected && BackupMetadata.bLocked);
    Backup.Close();

    FOGCharacterManifestationRecord Result;
    Result.ManifestationId = ResultId;
    Result.OwningRulerId = RulerId;
    Result.IdentityId = IdentityId;
    Result.ActiveVersionId = VersionId;
    Result.Level = 1;

    FOGGrandConvergenceService Convergence(Store);
    FOGEntityId ConvergenceId;
    TestFalse(
        TEXT("Protected source blocks default destructive Convergence"),
        Convergence.PerformConvergence(
            Result,
            {SourceA, SourceB},
            {},
            FOGContentId(TEXT("test:convergence.absolute")),
            30,
            TEXT("{}"),
            ConvergenceId,
            Error));

    TestTrue(
        TEXT("Explicit confirmation permits Protected-source Convergence"),
        Convergence.PerformConvergence(
            Result,
            {SourceA, SourceB},
            {},
            FOGContentId(TEXT("test:convergence.absolute")),
            30,
            TEXT("{}"),
            true,
            ConvergenceId,
            Error));
    TestTrue(TEXT("Convergence result ID exists"), ConvergenceId.IsValid());

    Store.Close();
    IFileManager::Get().DeleteDirectory(*Directory, false, true);
    return true;
}

#endif
