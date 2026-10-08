#include "Runtime/OGGameCoreSubsystem.h"
#include "Runtime/OGLocallyInstalledPackageProvider.h"

#if PLATFORM_ANDROID
#include "AndroidPermissionFunctionLibrary.h"
#endif

#include "Containers/Ticker.h"
#include "Diagnostics/OGDiagnosticsBundle.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Misc/DateTime.h"
#include "Misc/App.h"
#include "Misc/CoreDelegates.h"
#include "Misc/Paths.h"
#include "OfflineGame.h"
#include "Persistence/OGRecoveryCatalogService.h"
#include "Persistence/OGSQLiteWorldStore.h"
#include "Persistence/OGSnapshotService.h"
#include "Runtime/OGAndroidMediaVolumeBridge.h"
#include "Persistence/OGWorldBootstrap.h"

void UOGGameCoreSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    OGAndroidMediaVolumeBridge::Start();

    PerformanceTelemetry.Reset();
    Collection.InitializeDependency<UOGPresentationModeSubsystem>();
    if (UOGPresentationModeSubsystem* Modes = GetGameInstance()->GetSubsystem<UOGPresentationModeSubsystem>())
    {
        Modes->OnPresentationModeChanged.AddDynamic(this, &UOGGameCoreSubsystem::HandlePresentationModeChanged);
    }

    // A technical commit throttle, never an authored calendar or simulation rate.
    CanonicalClockTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateUObject(this, &UOGGameCoreSubsystem::TickCanonicalClock), 0.5f);

#if PLATFORM_ANDROID
    const FString NotificationPermission(TEXT("android.permission.POST_NOTIFICATIONS"));
    if (!UAndroidPermissionFunctionLibrary::CheckPermission(NotificationPermission))
    {
        TArray<FString> Permissions;
        Permissions.Add(NotificationPermission);
        UAndroidPermissionFunctionLibrary::AcquirePermissions(Permissions);
    }
#endif

    PerformanceTickerHandle =
        FTSTicker::GetCoreTicker().AddTicker(
            FTickerDelegate::CreateUObject(
                this,
                &UOGGameCoreSubsystem::TickPerformanceTelemetry));

    WillDeactivateHandle =
        FCoreDelegates::ApplicationWillDeactivateDelegate.AddUObject(
            this,
            &UOGGameCoreSubsystem::HandleApplicationWillDeactivate);
    HasReactivatedHandle =
        FCoreDelegates::ApplicationHasReactivatedDelegate.AddUObject(
            this,
            &UOGGameCoreSubsystem::HandleApplicationHasReactivated);
    WillEnterBackgroundHandle =
        FCoreDelegates::ApplicationWillEnterBackgroundDelegate.AddUObject(
            this,
            &UOGGameCoreSubsystem::HandleApplicationWillEnterBackground);
    HasEnteredForegroundHandle =
        FCoreDelegates::ApplicationHasEnteredForegroundDelegate.AddUObject(
            this,
            &UOGGameCoreSubsystem::HandleApplicationHasEnteredForeground);

    const FString DatabaseDirectory =
        FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("OfflineGame"));
    IFileManager::Get().MakeDirectory(*DatabaseDirectory, true);

    const FString DatabasePath =
        FPaths::Combine(DatabaseDirectory, TEXT("WorldState.db"));
    const bool bExistingDatabase =
        IFileManager::Get().FileExists(*DatabasePath);

    FOGWorldBootstrapResult BootstrapResult;
    FString Error;
    if (!FOGWorldBootstrap::PrepareWorld(
            DatabasePath,
            BootstrapResult,
            Error))
    {
        UE_LOG(
            LogOfflineGame,
            Error,
            TEXT("Migration-safe world bootstrap refused continuation: %s"),
            *Error);
        bCoreReady = false;
        return;
    }

    WorldStore = MakeUnique<FOGSQLiteWorldStore>();
    if (!WorldStore->Open(DatabasePath, Error))
    {
        UE_LOG(
            LogOfflineGame,
            Error,
            TEXT("Failed to initialize authoritative world database: %s"),
            *Error);
        WorldStore.Reset();
        bCoreReady = false;
        return;
    }

    if (!InitializeCanonicalClock(Error))
    {
        UE_LOG(LogOfflineGame, Error, TEXT("Canonical clock initialization failed: %s"), *Error);
        bCoreReady = false;
        return;
    }

    if (BootstrapResult.bMigrationPerformed)
    {
        UE_LOG(
            LogOfflineGame,
            Log,
            TEXT("World migration promoted safely. Recovery=%s Report=%s"),
            *BootstrapResult.RecoveryDatabasePath,
            *BootstrapResult.MigrationReportPath);
    }

    if (bExistingDatabase)
    {
        if (!FlushCanonicalClockBoundary(Error))
        {
            UE_LOG(LogOfflineGame, Error, TEXT("Clock boundary failed before automatic snapshot: %s"), *Error);
            bCoreReady = false;
            return;
        }
        FString SnapshotPath;
        FString SnapshotError;
        const FString SnapshotDirectory =
            FPaths::Combine(DatabaseDirectory, TEXT("Snapshots"));

        const FString RecoveryCatalogPath =
            FPaths::Combine(
                DatabaseDirectory,
                TEXT("RecoveryCatalog.json"));

        if (!FOGSnapshotService::CreateRotatingSnapshot(
                *WorldStore,
                SnapshotDirectory,
                3,
                RecoveryCatalogPath,
                TEXT("offlinegame:canonical_world"),
                FApp::GetBuildVersion(),
                SnapshotPath,
                SnapshotError))
        {
            UE_LOG(
                LogOfflineGame,
                Warning,
                TEXT("Automatic recovery snapshot failed: %s"),
                *SnapshotError);
        }
    }

    UE_LOG(
        LogOfflineGame,
        Log,
        TEXT("Authoritative game core initialized with schema version %d."),
        WorldStore->GetSchemaVersion(Error));
}

namespace
{
int64 CanonicalUtcMilliseconds()
{
    const FDateTime Now = FDateTime::UtcNow();
    return Now.ToUnixTimestamp() * 1000 + Now.GetMillisecond();
}
}

bool UOGGameCoreSubsystem::InitializeCanonicalClock(FString& OutError)
{
    bCoreReady = false;
    if (!ReleaseCanonicalRuntime(&OutError)) return false;
    if (!WorldStore || !WorldStore->IsOpen())
    {
        OutError = TEXT("Cannot initialize canonical clock without an open world store.");
        return false;
    }
    FOGCanonicalClockPolicy Policy;
    if (!FOGCanonicalClockPolicy::LoadJsonFile(
            FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Data"), TEXT("FoundationClockPolicy.json")),
            Policy, OutError))
    {
        return false;
    }
    CanonicalClock = MakeUnique<FOGCanonicalClockRuntime>(*WorldStore);
    if (!CanonicalClock->LoadOrCreate(Policy, CanonicalUtcMilliseconds(), OutError))
    {
        ReleaseCanonicalRuntime();
        return false;
    }
    StrategicRuntime = MakeUnique<FOGFoundationStrategicRuntime>(*WorldStore);
    if (bOptionalPackageHostConfigured)
    {
        OptionalPackageHost = MakeUnique<FOGOptionalPackageHost>(*WorldStore, OptionalPackageCallbacks);
    }
    else
    {
        // Store authority is ready; configure residency before bCoreReady exposes
        // optional character and interaction presentation to ordinary consumers.
        InstalledPackageProvider = MakeShared<FOGLocallyInstalledPackageProvider>();
        if (!ConfigureOptionalPackageHost(InstalledPackageProvider->MakeCallbacks(*WorldStore), OutError))
        {
            ReleaseCanonicalRuntime();
            return false;
        }
    }
    StrategicRuntime->RegisterClockResolvers(*CanonicalClock);
    if (const UOGPresentationModeSubsystem* Modes = GetGameInstance()->GetSubsystem<UOGPresentationModeSubsystem>())
    {
        CanonicalClock->SetMode(Modes->GetMode() == EOGPresentationMode::Ruler
            ? EOGCanonicalClockMode::Ruler : EOGCanonicalClockMode::World);
    }
    CanonicalClock->SetPaused(GetWorld() && GetWorld()->IsPaused());
    bCanonicalClockNeedsCatchup = true;
    if (!CanonicalClock->CatchupOffline(CanonicalUtcMilliseconds(), OutError))
    {
        return false;
    }
    bCanonicalClockNeedsCatchup = false;
    CanonicalClock->ResetOnlineAnchor(FPlatformTime::Seconds());
    bCoreReady = true;
    return true;
}

bool UOGGameCoreSubsystem::ConfigureOptionalPackageHost(
    FOGOptionalPackageHostCallbacks Callbacks, FString& OutError)
{
    OutError.Reset();
    if (!IsInGameThread() || !WorldStore || !WorldStore->IsOpen())
    {
        OutError = TEXT("Optional package authority needs the game thread and an open canonical store.");
        return false;
    }
    if (!Callbacks.UsesContainer || !Callbacks.ResolveContainer || !Callbacks.VerifyArtifact ||
        !Callbacks.CanTakeOwnership || !Callbacks.CanUnload)
    {
        OutError = TEXT("Optional package authority must provide trusted resolution, byte verification and safe residency callbacks.");
        return false;
    }
    // Existing loaders retain the host pointer. Reconfiguration is a lifetime boundary,
    // never a pointer swap beneath live character/interaction presentation.
    if (OptionalPackageHost)
    {
        OutError = TEXT("Optional package authority is already configured; release the canonical runtime before replacing it.");
        return false;
    }
    OptionalPackageCallbacks = MoveTemp(Callbacks);
    bOptionalPackageHostConfigured = true;
    OptionalPackageHost = MakeUnique<FOGOptionalPackageHost>(*WorldStore, OptionalPackageCallbacks);
    return true;
}

bool UOGGameCoreSubsystem::InstallVerifiedOptionalPackage(
    const FOGTrustedOptionalPackageInstall& Expected, const FString& SourcePak,
    FString& OutInstallUri, FString& OutError)
{
    OutInstallUri.Reset();
    OutError.Reset();
    if (!IsInGameThread() || !bCoreReady || !WorldStore || !WorldStore->IsOpen() ||
        !InstalledPackageProvider || !OptionalPackageHost)
    {
        OutError = TEXT("Trusted optional installation requires the ready project installation authority.");
        return false;
    }
    // The provider also rejects an active or physically resident replacement.
    // Consumers must release their leases and deactivate before an update.
    return InstalledPackageProvider->InstallVerifiedPak(
        *WorldStore, Expected, SourcePak, OutInstallUri, OutError);
}

bool UOGGameCoreSubsystem::ReleaseCanonicalRuntime(FString* OutError)
{
    bReleasingCanonicalRuntime = true;
    OnCanonicalRuntimeReleasing.Broadcast();
    bReleasingCanonicalRuntime = false;
    if (OptionalPackageHost)
    {
        FString UnmountError;
        if (!OptionalPackageHost->UnmountAll(UnmountError))
        {
            if (OutError) *OutError = TEXT("Canonical store replacement refused: ") + UnmountError;
            UE_LOG(LogOfflineGame, Error, TEXT("Optional package residency retained: %s"), *UnmountError);
            return false;
        }
        OptionalPackageHost.Reset();
    }
    if (InstalledPackageProvider)
    {
        // Callbacks borrow this provider. Clear them only after safe unmount and
        // recreate against the new store after import/recovery.
        OptionalPackageCallbacks = FOGOptionalPackageHostCallbacks();
        bOptionalPackageHostConfigured = false;
        InstalledPackageProvider.Reset();
    }
    StrategicRuntime.Reset();
    CanonicalClock.Reset();
    bCanonicalClockNeedsCatchup = false;
    return true;
}

bool UOGGameCoreSubsystem::PumpCanonicalClock(FString& OutError)
{
    if (!WorldStore || !WorldStore->IsOpen() || !CanonicalClock)
    {
        OutError = TEXT("Authoritative canonical clock is unavailable.");
        bCoreReady = false;
        return false;
    }
    // Interaction action pacing and suspension of 3D presentation are not game pause.
    CanonicalClock->SetPaused(GetWorld() && GetWorld()->IsPaused());
    if (bCanonicalClockNeedsCatchup)
    {
        if (!CanonicalClock->CatchupOffline(CanonicalUtcMilliseconds(), OutError))
        {
            bCoreReady = false;
            return false;
        }
        bCanonicalClockNeedsCatchup = false;
        CanonicalClock->ResetOnlineAnchor(FPlatformTime::Seconds());
    }
    bCoreReady = CanonicalClock->PumpOnline(FPlatformTime::Seconds(), CanonicalUtcMilliseconds(), OutError);
    return bCoreReady;
}

bool UOGGameCoreSubsystem::FlushCanonicalClockBoundary(FString& OutError)
{
    if (!PumpCanonicalClock(OutError)) return false;
    bCoreReady = CanonicalClock->RecordBoundary(CanonicalUtcMilliseconds(), OutError);
    return bCoreReady;
}

bool UOGGameCoreSubsystem::TickCanonicalClock(float DeltaSeconds)
{
    if (!bApplicationBackgrounded && CanonicalClock)
    {
        FString Error;
        if (!PumpCanonicalClock(Error))
        {
            UE_LOG(LogOfflineGame, Error, TEXT("Canonical clock pump failed: %s"), *Error);
        }
        else if (const UOGPresentationModeSubsystem* Modes = GetGameInstance()->GetSubsystem<UOGPresentationModeSubsystem>())
        {
            // Also reconcile a mode boundary whose prior pump failed and was retried.
            CanonicalClock->SetMode(Modes->GetMode() == EOGPresentationMode::Ruler
                ? EOGCanonicalClockMode::Ruler : EOGCanonicalClockMode::World);
        }
    }
    return true;
}

void UOGGameCoreSubsystem::HandlePresentationModeChanged(EOGPresentationMode NewMode)
{
    if (!CanonicalClock) return;
    FString Error;
    // The previous runtime mode consumes its elapsed interval before applying the new rate.
    if (bApplicationBackgrounded)
    {
        CanonicalClock->SetMode(NewMode == EOGPresentationMode::Ruler
            ? EOGCanonicalClockMode::Ruler : EOGCanonicalClockMode::World);
        return;
    }
    if (!PumpCanonicalClock(Error))
    {
        UE_LOG(LogOfflineGame, Error, TEXT("Canonical clock mode pump failed: %s"), *Error);
        return;
    }
    CanonicalClock->SetMode(NewMode == EOGPresentationMode::Ruler
        ? EOGCanonicalClockMode::Ruler : EOGCanonicalClockMode::World);
    if (!CanonicalClock->RecordBoundary(CanonicalUtcMilliseconds(), Error))
    {
        bCoreReady = false;
        UE_LOG(LogOfflineGame, Error, TEXT("Canonical clock mode boundary failed: %s"), *Error);
    }
}

bool UOGGameCoreSubsystem::TickPerformanceTelemetry(
    float DeltaSeconds)
{
    PerformanceTelemetry.RecordFrame(
        static_cast<double>(DeltaSeconds));
    return true;
}


void UOGGameCoreSubsystem::CheckpointForLifecycleBoundary(
    const TCHAR* BoundaryName)
{
    if (!WorldStore || !WorldStore->IsOpen())
    {
        return;
    }

    FString Error;
    if (!FlushCanonicalClockBoundary(Error))
    {
        UE_LOG(LogOfflineGame, Error, TEXT("Canonical clock failed at lifecycle boundary %s: %s"),
            BoundaryName ? BoundaryName : TEXT("unknown"), *Error);
        return;
    }
    if (!WorldStore->Checkpoint(Error))
    {
        UE_LOG(
            LogOfflineGame,
            Warning,
            TEXT("World database checkpoint failed at lifecycle boundary %s: %s"),
            BoundaryName ? BoundaryName : TEXT("unknown"),
            *Error);
        return;
    }

    UE_LOG(
        LogOfflineGame,
        Log,
        TEXT("World database checkpoint completed at lifecycle boundary %s."),
        BoundaryName ? BoundaryName : TEXT("unknown"));
}

void UOGGameCoreSubsystem::VerifyCoreAfterResume(
    const TCHAR* BoundaryName)
{
    const bool bWasBackgrounded = bApplicationBackgrounded;
    bApplicationBackgrounded = false;

    if (!WorldStore || !WorldStore->IsOpen())
    {
        bCoreReady = false;
        UE_LOG(
            LogOfflineGame,
            Error,
            TEXT("Authoritative game core was not open after lifecycle resume boundary %s."),
            BoundaryName ? BoundaryName : TEXT("unknown"));
        return;
    }

    if (bWasBackgrounded) bCanonicalClockNeedsCatchup = true;
    FString ClockError;
    if (!PumpCanonicalClock(ClockError))
    {
        UE_LOG(LogOfflineGame, Error, TEXT("Canonical clock resume failed at %s: %s"),
            BoundaryName ? BoundaryName : TEXT("unknown"), *ClockError);
        return;
    }
    UE_LOG(
        LogOfflineGame,
        Log,
        TEXT("Authoritative game core resumed without reinitialization at lifecycle boundary %s."),
        BoundaryName ? BoundaryName : TEXT("unknown"));
}

void UOGGameCoreSubsystem::HandleApplicationWillDeactivate()
{
    if (!bApplicationBackgrounded) CheckpointForLifecycleBoundary(TEXT("will_deactivate"));
    bApplicationBackgrounded = true;
}

void UOGGameCoreSubsystem::HandleApplicationHasReactivated()
{
    VerifyCoreAfterResume(TEXT("has_reactivated"));
}

void UOGGameCoreSubsystem::HandleApplicationWillEnterBackground()
{
    if (!bApplicationBackgrounded) CheckpointForLifecycleBoundary(TEXT("will_enter_background"));
    bApplicationBackgrounded = true;
}

void UOGGameCoreSubsystem::HandleApplicationHasEnteredForeground()
{
    VerifyCoreAfterResume(TEXT("has_entered_foreground"));
}

bool UOGGameCoreSubsystem::GenerateDiagnosticsBundle(
    FString& OutBundlePath,
    FString& OutError)
{
    OutBundlePath.Reset();
    OutError.Reset();

    if (!WorldStore || !WorldStore->IsOpen())
    {
        OutError = TEXT("Authoritative game core is not ready.");
        return false;
    }
    if (!FlushCanonicalClockBoundary(OutError)) return false;

    const FString OutputDirectory =
        FPaths::Combine(
            FPaths::ProjectSavedDir(),
            TEXT("OfflineGame"),
            TEXT("Diagnostics"));

    return FOGDiagnosticsBundle::Write(
        *WorldStore,
        OutputDirectory,
        PerformanceTelemetry.Snapshot(),
        OutBundlePath,
        OutError);
}

bool UOGGameCoreSubsystem::CreateManualRecoverySnapshot(
    FString& OutSnapshotPath,
    FString& OutError)
{
    OutSnapshotPath.Reset();
    OutError.Reset();

    if (!WorldStore || !WorldStore->IsOpen())
    {
        OutError = TEXT("Authoritative game core is not ready.");
        return false;
    }
    if (!FlushCanonicalClockBoundary(OutError)) return false;

    const FString DatabaseDirectory =
        FPaths::Combine(
            FPaths::ProjectSavedDir(),
            TEXT("OfflineGame"));
    const FString SnapshotDirectory =
        FPaths::Combine(
            DatabaseDirectory,
            TEXT("ManualBackups"));
    const FString RecoveryCatalogPath =
        FPaths::Combine(
            DatabaseDirectory,
            TEXT("RecoveryCatalog.json"));

    return FOGSnapshotService::CreateRotatingSnapshot(
        *WorldStore,
        SnapshotDirectory,
        8,
        RecoveryCatalogPath,
        TEXT("offlinegame:canonical_world"),
        FApp::GetBuildVersion(),
        OutSnapshotPath,
        OutError);
}

bool UOGGameCoreSubsystem::ExportCanonicalWorldBackup(
    const FString& DestinationBackupPath,
    FString& OutBackupId,
    FString& OutError)
{
    OutBackupId.Reset();
    OutError.Reset();

    if (!WorldStore || !WorldStore->IsOpen())
    {
        OutError = TEXT("Authoritative game core is not ready.");
        return false;
    }
    if (!FlushCanonicalClockBoundary(OutError)) return false;

    FString CheckpointError;
    if (!WorldStore->Checkpoint(CheckpointError))
    {
        OutError = FString::Printf(
            TEXT("World checkpoint failed before export: %s"),
            *CheckpointError);
        return false;
    }

    const FString DatabaseDirectory =
        FPaths::Combine(
            FPaths::ProjectSavedDir(),
            TEXT("OfflineGame"));
    return FOGRecoveryCatalogService::ExportWorldBackup(
        FPaths::Combine(
            DatabaseDirectory,
            TEXT("WorldState.db")),
        DestinationBackupPath,
        FPaths::Combine(
            DatabaseDirectory,
            TEXT("RecoveryCatalog.json")),
        TEXT("offlinegame:canonical_world"),
        FApp::GetBuildVersion(),
        OutBackupId,
        OutError);
}

bool UOGGameCoreSubsystem::ImportCanonicalWorldBackup(
    const FString& SourceBackupPath,
    FString& OutPreservedWorldPath,
    FString& OutError)
{
    OutPreservedWorldPath.Reset();
    OutError.Reset();

    if (!WorldStore || !WorldStore->IsOpen())
    {
        OutError = TEXT("Authoritative game core is not ready.");
        return false;
    }
    if (!FlushCanonicalClockBoundary(OutError)) return false;

    const FString DatabaseDirectory =
        FPaths::Combine(
            FPaths::ProjectSavedDir(),
            TEXT("OfflineGame"));
    const FString DatabasePath =
        FPaths::Combine(
            DatabaseDirectory,
            TEXT("WorldState.db"));

    FString CheckpointError;
    if (!WorldStore->Checkpoint(CheckpointError))
    {
        OutError = FString::Printf(
            TEXT("World checkpoint failed before import: %s"),
            *CheckpointError);
        return false;
    }

    if (!ReleaseCanonicalRuntime(&OutError)) return false;
    WorldStore->Close();
    WorldStore.Reset();
    bCoreReady = false;

    const bool bImported =
        FOGRecoveryCatalogService::ImportWorldBackup(
            SourceBackupPath,
            DatabasePath,
            FPaths::Combine(
                DatabaseDirectory,
                TEXT("ImportRecovery")),
            FPaths::Combine(
                DatabaseDirectory,
                TEXT("RecoveryCatalog.json")),
            TEXT("offlinegame:canonical_world"),
            FApp::GetBuildVersion(),
            OutPreservedWorldPath,
            OutError);

    WorldStore = MakeUnique<FOGSQLiteWorldStore>();
    FString ReopenError;
    if (!WorldStore->Open(
            DatabasePath,
            ReopenError))
    {
        WorldStore.Reset();
        bCoreReady = false;
        OutError = FString::Printf(
            TEXT("%s Reopen failed: %s"),
            *OutError,
            *ReopenError);
        return false;
    }

    FString ClockError;
    if (!InitializeCanonicalClock(ClockError))
    {
        OutError = FString::Printf(TEXT("%s Clock reload failed: %s"), *OutError, *ClockError);
        return false;
    }
    return bImported;
}

bool UOGGameCoreSubsystem::ClearCanonicalWorld(
    bool bDeleteBackups,
    FString& OutError)
{
    OutError.Reset();

    if (!WorldStore || !WorldStore->IsOpen())
    {
        OutError = TEXT("Authoritative game core is not ready.");
        return false;
    }
    if (!FlushCanonicalClockBoundary(OutError)) return false;

    FString SafetySnapshot;
    if (!CreateManualRecoverySnapshot(
            SafetySnapshot,
            OutError))
    {
        return false;
    }

    const FString DatabaseDirectory =
        FPaths::Combine(
            FPaths::ProjectSavedDir(),
            TEXT("OfflineGame"));
    const FString DatabasePath =
        FPaths::Combine(
            DatabaseDirectory,
            TEXT("WorldState.db"));
    const FString CatalogPath =
        FPaths::Combine(
            DatabaseDirectory,
            TEXT("RecoveryCatalog.json"));
    const FString BackupDirectory =
        FPaths::Combine(
            DatabaseDirectory,
            TEXT("ManualBackups"));

    FString CheckpointError;
    if (!WorldStore->Checkpoint(CheckpointError))
    {
        OutError = FString::Printf(
            TEXT("World checkpoint failed before clear: %s"),
            *CheckpointError);
        return false;
    }

    if (!ReleaseCanonicalRuntime(&OutError)) return false;
    WorldStore->Close();
    WorldStore.Reset();
    bCoreReady = false;

    if (!FOGRecoveryCatalogService::ClearWorld(
            DatabasePath,
            CatalogPath,
            BackupDirectory,
            bDeleteBackups,
            OutError))
    {
        WorldStore = MakeUnique<FOGSQLiteWorldStore>();
        FString ReopenError;
        bCoreReady =
            IFileManager::Get().FileExists(*DatabasePath) &&
            WorldStore->Open(DatabasePath, ReopenError);
        if (!bCoreReady)
        {
            WorldStore.Reset();
            OutError += FString::Printf(TEXT(" Reopen failed after clear refusal: %s"), *ReopenError);
        }
        else
        {
            FString ClockError;
            if (!InitializeCanonicalClock(ClockError))
                OutError += FString::Printf(TEXT(" Clock reload failed: %s"), *ClockError);
        }
        return false;
    }

    FOGWorldBootstrapResult BootstrapResult;
    if (!FOGWorldBootstrap::PrepareWorld(
            DatabasePath,
            BootstrapResult,
            OutError))
    {
        return false;
    }

    WorldStore = MakeUnique<FOGSQLiteWorldStore>();
    if (!WorldStore->Open(
            DatabasePath,
            OutError))
    {
        WorldStore.Reset();
        return false;
    }

    return InitializeCanonicalClock(OutError);
}

void UOGGameCoreSubsystem::Deinitialize()
{
    if (WorldStore && WorldStore->IsOpen())
    {
        FString ClockError;
        if (!bApplicationBackgrounded && !FlushCanonicalClockBoundary(ClockError))
            UE_LOG(LogOfflineGame, Error, TEXT("Canonical clock shutdown boundary failed: %s"), *ClockError);
    }
    bCoreReady = false;
    if (UOGPresentationModeSubsystem* Modes = GetGameInstance()->GetSubsystem<UOGPresentationModeSubsystem>())
        Modes->OnPresentationModeChanged.RemoveDynamic(this, &UOGGameCoreSubsystem::HandlePresentationModeChanged);
    if (CanonicalClockTickerHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(CanonicalClockTickerHandle);
        CanonicalClockTickerHandle = FTSTicker::FDelegateHandle();
    }
    const bool bReleasedCanonicalRuntime = ReleaseCanonicalRuntime();

    if (WillDeactivateHandle.IsValid())
    {
        FCoreDelegates::ApplicationWillDeactivateDelegate.Remove(WillDeactivateHandle);
        WillDeactivateHandle.Reset();
    }
    if (HasReactivatedHandle.IsValid())
    {
        FCoreDelegates::ApplicationHasReactivatedDelegate.Remove(HasReactivatedHandle);
        HasReactivatedHandle.Reset();
    }
    if (WillEnterBackgroundHandle.IsValid())
    {
        FCoreDelegates::ApplicationWillEnterBackgroundDelegate.Remove(WillEnterBackgroundHandle);
        WillEnterBackgroundHandle.Reset();
    }
    if (HasEnteredForegroundHandle.IsValid())
    {
        FCoreDelegates::ApplicationHasEnteredForegroundDelegate.Remove(HasEnteredForegroundHandle);
        HasEnteredForegroundHandle.Reset();
    }

    if (PerformanceTickerHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(
            PerformanceTickerHandle);
        PerformanceTickerHandle =
            FTSTicker::FDelegateHandle();
    }

    if (WorldStore && bReleasedCanonicalRuntime)
    {
        FString CheckpointError;
        if (!WorldStore->Checkpoint(CheckpointError))
        {
            UE_LOG(
                LogOfflineGame,
                Warning,
                TEXT("World database checkpoint failed during shutdown: %s"),
                *CheckpointError);
        }

        WorldStore->Close();
        WorldStore.Reset();
    }

    UE_LOG(LogOfflineGame, Log, TEXT("Authoritative game core subsystem deinitialized."));
    OGAndroidMediaVolumeBridge::Stop();
    Super::Deinitialize();
}