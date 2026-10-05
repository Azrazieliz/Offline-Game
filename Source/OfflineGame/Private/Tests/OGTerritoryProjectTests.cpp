#include "Persistence/OGSQLiteWorldStore.h"
#include "World/OGDomainCoreService.h"
#include "World/OGProjectService.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

namespace
{
FString MakeTerritoryTestDirectory()
{
    return FPaths::Combine(
        FPaths::ProjectSavedDir(),
        TEXT("Automation"),
        FGuid::NewGuid().ToString(
            EGuidFormats::Digits));
}

bool PrepareRulerAndLocation(
    FOGSQLiteWorldStore& Store,
    const FOGEntityId& RulerId,
    const FOGEntityId& LocationId,
    FString& Error)
{
    if (!Store.UpsertEntity(
            RulerId,
            TEXT("ruler"),
            0,
            TEXT("{}"),
            Error))
    {
        return false;
    }

    FOGLocationRecord Location;
    Location.LocationId = LocationId;
    Location.Kind = TEXT("region");

    return Store.UpsertLocation(
        Location,
        0,
        Error);
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOGDomainCoreCaptureBreakTest,
    "OfflineGame.WorldState.DomainCore.CaptureRequiresIntactCore",
    EAutomationTestFlags::ApplicationContextMask |
        EAutomationTestFlags::EngineFilter)

bool FOGDomainCoreCaptureBreakTest::RunTest(
    const FString& Parameters)
{
    const FString Directory =
        MakeTerritoryTestDirectory();
    const FString DatabasePath =
        FPaths::Combine(
            Directory,
            TEXT("core.db"));

    IFileManager::Get().MakeDirectory(
        *Directory,
        true);

    FOGSQLiteWorldStore Store;
    FString Error;

    TestTrue(
        TEXT("Open database"),
        Store.Open(
            DatabasePath,
            Error));

    TestEqual(
        TEXT("Schema version is 10"),
        Store.GetSchemaVersion(Error),
        10);

    const FOGEntityId OriginalRuler =
        FOGEntityId::NewId();
    const FOGEntityId CapturingRuler =
        FOGEntityId::NewId();
    const FOGEntityId LocationId =
        FOGEntityId::NewId();
    const FOGEntityId TerritoryId =
        FOGEntityId::NewId();
    const FOGEntityId CoreId =
        FOGEntityId::NewId();

    TestTrue(
        TEXT("Prepare original ruler and location"),
        PrepareRulerAndLocation(
            Store,
            OriginalRuler,
            LocationId,
            Error));

    TestTrue(
        TEXT("Persist capturing ruler"),
        Store.UpsertEntity(
            CapturingRuler,
            TEXT("ruler"),
            0,
            TEXT("{}"),
            Error));

    FOGTerritoryRecord Territory;
    Territory.TerritoryId =
        TerritoryId;
    Territory.RulerId =
        OriginalRuler;
    Territory.RootLocationId =
        LocationId;
    Territory.bMainTerritory =
        true;
    Territory.Population =
        125000;
    Territory.ControlState =
        TEXT("controlled");

    TestTrue(
        TEXT("Persist territory"),
        Store.UpsertTerritory(
            Territory,
            10,
            Error));

    bool bLocationFound = false;
    FOGLocationRecord StoredLocation;

    TestTrue(
        TEXT("Read root location"),
        Store.TryReadLocation(
            LocationId,
            bLocationFound,
            StoredLocation,
            Error));

    TestTrue(
        TEXT("Root location exists"),
        bLocationFound);

    TestTrue(
        TEXT("Territory ownership is reflected in physical location state"),
        StoredLocation.TerritoryId ==
            TerritoryId);

    FOGDomainCoreRecord Core;
    Core.CoreId = CoreId;
    Core.TerritoryId = TerritoryId;
    Core.ControllerRulerId =
        OriginalRuler;
    Core.Lifecycle =
        EOGDomainCoreLifecycle::Awakened;
    Core.CurrentDurability =
        FOGLargeNumber::FromInt64(100);
    Core.MaxDurability =
        FOGLargeNumber::FromInt64(100);

    FOGDomainCoreAspect TimeAspect;
    TimeAspect.AspectId =
        FOGContentId(TEXT("test:aspect.time"));
    TimeAspect.Grade = 1;
    Core.Aspects.Add(TimeAspect);

    TestTrue(
        TEXT("Persist awakened Domain Core"),
        Store.UpsertDomainCore(
            Core,
            20,
            Error));

    FOGDomainCoreService Service(
        Store);

    TestTrue(
        TEXT("Awakened Core becomes Territory heart"),
        Service.ActivateAwakenedCoreAsHeart(
            CoreId,
            20,
            Error));

    TestTrue(
        TEXT("Intact Core can be captured"),
        Service.CaptureIntactCore(
            CoreId,
            CapturingRuler,
            30,
            Error));

    bool bCoreFound = false;
    FOGDomainCoreRecord Captured;

    TestTrue(
        TEXT("Read captured Core"),
        Store.TryReadDomainCore(
            CoreId,
            bCoreFound,
            Captured,
            Error));

    TestTrue(
        TEXT("Core exists"),
        bCoreFound);

    TestTrue(
        TEXT("Controller changed"),
        Captured.ControllerRulerId ==
            CapturingRuler);

    TestEqual(
        TEXT("Aspect persists"),
        Captured.Aspects.Num(),
        1);

    TestTrue(
        TEXT("Destroy Core by reducing durability to zero"),
        Service.ApplyCoreDurabilityDamage(
            CoreId,
            FOGLargeNumber::FromInt64(100),
            40,
            Error));

    FOGDomainCoreRecord Broken;
    bCoreFound = false;

    TestTrue(
        TEXT("Read broken Core"),
        Store.TryReadDomainCore(
            CoreId,
            bCoreFound,
            Broken,
            Error));

    TestTrue(
        TEXT("Broken Core exists"),
        bCoreFound);

    TestEqual(
        TEXT("Zero durability means broken"),
        Broken.Lifecycle,
        EOGDomainCoreLifecycle::Broken);

    TestTrue(
        TEXT("Broken Core durability is zero"),
        Broken.CurrentDurability.IsZero());

    TestFalse(
        TEXT("Broken Core has no active controller"),
        Broken.ControllerRulerId.IsValid());

    FOGTerritoryDomainStateRecord LostHeart;
    bool bDomainStateFound = false;
    TestTrue(
        TEXT("Read Domain-heart state after Core loss"),
        Store.TryReadTerritoryDomainState(
            TerritoryId,
            bDomainStateFound,
            LostHeart,
            Error));
    TestTrue(
        TEXT("Domain-heart state exists"),
        bDomainStateFound);
    TestEqual(
        TEXT("Broken active Core triggers Domain-heart ruin"),
        LostHeart.DomainState,
        FName(TEXT("heart_lost_ruining")));
    TestFalse(
        TEXT("Lost Domain heart has no active Core"),
        LostHeart.ActiveCoreId.IsValid());

    TestFalse(
        TEXT("Broken Core cannot be captured"),
        Service.CaptureIntactCore(
            CoreId,
            OriginalRuler,
            50,
            Error));

    Store.Close();
    IFileManager::Get().DeleteDirectory(
        *Directory,
        false,
        true);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOGResourceProjectLazyProgressTest,
    "OfflineGame.WorldState.Project.AggregateResourcesAndLazyProgress",
    EAutomationTestFlags::ApplicationContextMask |
        EAutomationTestFlags::EngineFilter)

bool FOGResourceProjectLazyProgressTest::RunTest(
    const FString& Parameters)
{
    const FString Directory =
        MakeTerritoryTestDirectory();
    const FString DatabasePath =
        FPaths::Combine(
            Directory,
            TEXT("project.db"));

    IFileManager::Get().MakeDirectory(
        *Directory,
        true);

    const FOGEntityId RulerId =
        FOGEntityId::NewId();
    const FOGEntityId LocationId =
        FOGEntityId::NewId();
    const FOGContentId StoneId(
        TEXT("test:resource.stone"));

    FOGEntityId ProjectId;

    {
        FOGSQLiteWorldStore Store;
        FString Error;

        TestTrue(
            TEXT("Open database"),
            Store.Open(
                DatabasePath,
                Error));

        TestTrue(
            TEXT("Prepare ruler/location"),
            PrepareRulerAndLocation(
                Store,
                RulerId,
                LocationId,
                Error));

        TestTrue(
            TEXT("Set aggregate resource balance"),
            Store.SetResourceBalance(
                RulerId,
                StoneId,
                100,
                Error));

        FOGProjectService Service(
            Store);

        FOGProjectResourceCost Cost;
        Cost.ResourceId =
            StoneId;
        Cost.Amount =
            40;

        TestTrue(
            TEXT("Start construction-style project"),
            Service.StartProject(
                RulerId,
                LocationId,
                FOGContentId(
                    TEXT("test:project.build_gate")),
                {Cost},
                10,
                110,
                TEXT("{}"),
                ProjectId,
                Error));

        bool bKnown = false;
        int64 Balance = -1;

        TestTrue(
            TEXT("Read resource balance after project start"),
            Store.TryReadResourceBalance(
                RulerId,
                StoneId,
                bKnown,
                Balance,
                Error));

        TestTrue(
            TEXT("Resource exists"),
            bKnown);

        TestEqual(
            TEXT("Project cost is deducted transactionally"),
            Balance,
            static_cast<int64>(60));

        FOGProjectRecord Halfway;

        TestTrue(
            TEXT("Refresh project only when queried"),
            Service.RefreshProject(
                ProjectId,
                60,
                Halfway,
                Error));

        TestEqual(
            TEXT("Half elapsed time gives half progress"),
            Halfway.ProgressBps,
            5000);

        TestEqual(
            TEXT("Project remains active halfway"),
            Halfway.Status,
            EOGProjectStatus::Active);
    }

    {
        FOGSQLiteWorldStore Store;
        FString Error;

        TestTrue(
            TEXT("Reopen database"),
            Store.Open(
                DatabasePath,
                Error));

        FOGProjectService Service(
            Store);
        FOGProjectRecord Completed;

        TestTrue(
            TEXT("Refresh after due tick"),
            Service.RefreshProject(
                ProjectId,
                110,
                Completed,
                Error));

        TestEqual(
            TEXT("Due project completes"),
            Completed.Status,
            EOGProjectStatus::Completed);

        TestEqual(
            TEXT("Completed project reaches 100%"),
            Completed.ProgressBps,
            10000);
    }

    IFileManager::Get().DeleteDirectory(
        *Directory,
        false,
        true);

    return true;
}

#endif
