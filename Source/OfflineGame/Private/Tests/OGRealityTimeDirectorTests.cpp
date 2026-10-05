#include "Persistence/OGSQLiteWorldStore.h"
#include "World/OGRealityGraphService.h"
#include "World/OGWorldDirectorService.h"
#include "World/OGWorldTimeService.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

namespace
{
FString MakeRealityTimeTestDirectory()
{
    return FPaths::Combine(
        FPaths::ProjectSavedDir(),
        TEXT("Automation"),
        FGuid::NewGuid().ToString(
            EGuidFormats::Digits));
}

bool PersistDirectorScope(
    FOGSQLiteWorldStore& Store,
    const FOGEntityId& ScopeId,
    FString& Error)
{
    return Store.UpsertEntity(
        ScopeId,
        FName(TEXT("ruler")),
        0,
        TEXT("{}"),
        Error);
}

FString ValidDirectorProvenance(
    const TCHAR* Reason)
{
    return FString::Printf(
        TEXT("{\"audit_reason\":\"%s\",")
        TEXT("\"world_state_inputs\":{\"test_state\":1},")
        TEXT("\"package_versions\":{\"test:package\":3},")
        TEXT("\"participating_content_ids\":[\"test:character.alpha\"]}"),
        Reason);
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOGWorldTimeNestedProjectionTest,
    "OfflineGame.World.Time.NestedRationalProjectionAndCalendarResolver",
    EAutomationTestFlags::ApplicationContextMask |
        EAutomationTestFlags::EngineFilter)

bool FOGWorldTimeNestedProjectionTest::RunTest(
    const FString& Parameters)
{
    const FString Directory =
        MakeRealityTimeTestDirectory();
    const FString DatabasePath =
        FPaths::Combine(
            Directory,
            TEXT("time.db"));
    IFileManager::Get().MakeDirectory(
        *Directory,
        true);

    FOGSQLiteWorldStore Store;
    FString Error;
    TestTrue(
        TEXT("Open schema-12 database"),
        Store.Open(
            DatabasePath,
            Error));
    TestEqual(
        TEXT("Schema version is 12"),
        Store.GetSchemaVersion(
            Error),
        12);

    const FOGEntityId RootId =
        FOGEntityId::NewId();
    const FOGEntityId ChildId =
        FOGEntityId::NewId();

    FOGWorldTimeService Time(
        Store);

    FOGTimeDomainRecord Root;
    Root.TimeDomainId =
        RootId;
    Root.RateNumerator = 2;
    Root.RateDenominator = 1;
    Root.ParentEpochTick = 0;
    Root.LocalEpochTick = 0;

    TestTrue(
        TEXT("Persist root Time Domain"),
        Time.SaveTimeDomain(
            Root,
            0,
            Error));

    FOGTimeDomainRecord Child;
    Child.TimeDomainId =
        ChildId;
    Child.ParentTimeDomainId =
        RootId;
    Child.RateNumerator = 1;
    Child.RateDenominator = 2;
    Child.ParentEpochTick = 0;
    Child.LocalEpochTick = 0;
    Child.CalendarId =
        FOGContentId(
            TEXT("test:calendar.child"));

    TestTrue(
        TEXT("Persist child Time Domain"),
        Time.SaveTimeDomain(
            Child,
            0,
            Error));

    FOGLocalTimeProjection Projection;
    TestTrue(
        TEXT("Project canonical tick through nested rational domains"),
        Time.ProjectCanonicalTick(
            ChildId,
            37,
            Projection,
            Error));
    TestEqual(
        TEXT("2x parent then 1/2x child preserves deterministic local tick"),
        Projection.LocalTick,
        static_cast<int64>(37));
    TestTrue(
        TEXT("Leaf authored calendar is preserved"),
        Projection.CalendarId ==
            Child.CalendarId);

    int32 ResolverCalls = 0;
    const FOGCalendarElapsedResolver Resolver =
        [&ResolverCalls](
            const FOGCalendarElapsedQuery& Query,
            bool& bOutElapsed,
            FString& OutError)
        {
            ++ResolverCalls;
            OutError.Reset();

            if (Query.CalendarId !=
                    FOGContentId(
                        TEXT("test:calendar.child")) ||
                Query.DurationKind !=
                    FName(TEXT("month")) ||
                Query.DurationCount != 1)
            {
                OutError =
                    TEXT("Unexpected authored calendar query.");
                return false;
            }

            // Test content defines one month as 30 local ticks. Production
            // calendars remain package/content data.
            const int64 MonthTicks = 30;
            const int64 Delta =
                Query.EndLocalTick -
                Query.StartLocalTick;
            bOutElapsed =
                Query.bStrictlyMoreThan
                    ? Delta > MonthTicks
                    : Delta >= MonthTicks;
            return true;
        };

    bool bElapsed = true;
    TestTrue(
        TEXT("Resolve exactly one authored month"),
        Time.HasStrictlyMoreThanCalendarDuration(
            ChildId,
            10,
            40,
            FName(TEXT("month")),
            1,
            Resolver,
            bElapsed,
            Error));
    TestFalse(
        TEXT("Exactly one month is not strictly more than one month"),
        bElapsed);

    TestTrue(
        TEXT("Resolve more than one authored month"),
        Time.HasStrictlyMoreThanCalendarDuration(
            ChildId,
            10,
            41,
            FName(TEXT("month")),
            1,
            Resolver,
            bElapsed,
            Error));
    TestTrue(
        TEXT("31 local ticks is strictly more than the test month"),
        bElapsed);
    TestEqual(
        TEXT("Calendar semantics are delegated on each query"),
        ResolverCalls,
        2);

    Root.ParentTimeDomainId =
        ChildId;
    TestFalse(
        TEXT("Time Domain cycle is rejected"),
        Time.SaveTimeDomain(
            Root,
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
    FOGRealityGraphAndJunctionTest,
    "OfflineGame.World.Reality.GraphWorldRankAndJunctionLifecycle",
    EAutomationTestFlags::ApplicationContextMask |
        EAutomationTestFlags::EngineFilter)

bool FOGRealityGraphAndJunctionTest::RunTest(
    const FString& Parameters)
{
    const FString Directory =
        MakeRealityTimeTestDirectory();
    const FString DatabasePath =
        FPaths::Combine(
            Directory,
            TEXT("reality.db"));
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

    FOGWorldTimeService Time(
        Store);
    const FOGEntityId TimeDomainId =
        FOGEntityId::NewId();

    FOGTimeDomainRecord Domain;
    Domain.TimeDomainId =
        TimeDomainId;
    Domain.CalendarId =
        FOGContentId(
            TEXT("test:calendar.dimension"));
    TestTrue(
        TEXT("Persist Reality Time Domain"),
        Time.SaveTimeDomain(
            Domain,
            0,
            Error));

    const FOGEntityId DimensionId =
        FOGEntityId::NewId();
    const FOGEntityId WorldId =
        FOGEntityId::NewId();
    const FOGEntityId OtherWorldId =
        FOGEntityId::NewId();

    FOGRealityGraphService Reality(
        Store);

    FOGRealityNodeRecord Dimension;
    Dimension.RealityId =
        DimensionId;
    Dimension.Kind =
        FName(TEXT("dimension"));
    Dimension.TimeDomainId =
        TimeDomainId;
    Dimension.LawProfileId =
        FOGContentId(
            TEXT("test:law.dimension"));

    TestTrue(
        TEXT("Persist Dimension Reality node"),
        Reality.SaveRealityNode(
            Dimension,
            0,
            Error));

    FOGRealityNodeRecord World;
    World.RealityId =
        WorldId;
    World.ParentRealityId =
        DimensionId;
    World.Kind =
        FName(TEXT("world"));
    World.WorldRankId =
        FOGContentId(
            TEXT("test:world_rank.low"));
    World.TimeDomainId =
        TimeDomainId;

    TestTrue(
        TEXT("Persist child World Reality node"),
        Reality.SaveRealityNode(
            World,
            0,
            Error));

    FOGRealityNodeRecord OtherWorld =
        World;
    OtherWorld.RealityId =
        OtherWorldId;
    OtherWorld.WorldRankId =
        FOGContentId(
            TEXT("test:world_rank.high"));

    TestTrue(
        TEXT("Persist second World Reality node"),
        Reality.SaveRealityNode(
            OtherWorld,
            0,
            Error));

    Dimension.ParentRealityId =
        WorldId;
    TestFalse(
        TEXT("Reality parent cycle is rejected"),
        Reality.SaveRealityNode(
            Dimension,
            1,
            Error));

    TestTrue(
        TEXT("World Rank may change with explicit causal provenance"),
        Reality.SetWorldRank(
            WorldId,
            FOGContentId(
                TEXT("test:world_rank.mid")),
            10,
            TEXT("{\"cause\":\"anchor_stabilization\"}"),
            Error));

    bool bFound = false;
    FOGRealityNodeRecord StoredWorld;
    TestTrue(
        TEXT("Read evolved World"),
        Store.TryReadRealityNode(
            WorldId,
            bFound,
            StoredWorld,
            Error));
    TestTrue(
        TEXT("World exists"),
        bFound);
    TestTrue(
        TEXT("World Rank remains a content identity"),
        StoredWorld.WorldRankId ==
            FOGContentId(
                TEXT("test:world_rank.mid")));

    FOGJunctionRecord Junction;
    Junction.JunctionId =
        FOGEntityId::NewId();
    Junction.FromRealityId =
        WorldId;
    Junction.ToRealityId =
        OtherWorldId;
    Junction.State =
        FName(TEXT("closed"));
    Junction.StabilityBps = 7500;
    Junction.RequirementsJson =
        TEXT("{\"requires\":\"test:key.dimension\"}");

    TestTrue(
        TEXT("Persist causal Junction"),
        Reality.SaveJunction(
            Junction,
            10,
            Error));

    TestFalse(
        TEXT("Junction does not open when content requirements fail"),
        Reality.OpenJunction(
            Junction.JunctionId,
            20,
            false,
            Error));

    TestTrue(
        TEXT("Junction opens after requirements resolve true"),
        Reality.OpenJunction(
            Junction.JunctionId,
            21,
            true,
            Error));

    FOGJunctionRecord StoredJunction;
    bFound = false;
    TestTrue(
        TEXT("Read open Junction"),
        Store.TryReadJunction(
            Junction.JunctionId,
            bFound,
            StoredJunction,
            Error));
    TestTrue(
        TEXT("Junction exists"),
        bFound);
    TestEqual(
        TEXT("Junction is open"),
        StoredJunction.State,
        FName(TEXT("open")));

    TestTrue(
        TEXT("Close Junction causally"),
        Reality.CloseJunction(
            Junction.JunctionId,
            30,
            FName(TEXT("stability_failure")),
            Error));

    Store.Close();
    IFileManager::Get().DeleteDirectory(
        *Directory,
        false,
        true);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOGWorldDirectorDeterminismAndBoundsTest,
    "OfflineGame.World.Director.DeterministicSchedulingBoundsAndProvenance",
    EAutomationTestFlags::ApplicationContextMask |
        EAutomationTestFlags::EngineFilter)

bool FOGWorldDirectorDeterminismAndBoundsTest::RunTest(
    const FString& Parameters)
{
    const FString Directory =
        MakeRealityTimeTestDirectory();
    const FString DatabasePath =
        FPaths::Combine(
            Directory,
            TEXT("director.db"));
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

    FOGWorldDirectorService Director(
        Store);

    FOGWorldDirectorScheduleRequest Request;
    Request.ContentId =
        FOGContentId(
            TEXT("test:event.crisis_alpha"));
    Request.TemplateId =
        FOGContentId(
            TEXT("test:template.crisis"));
    Request.EligibleSinceWorldTick = 100;
    Request.EarliestStartWorldTick = 120;
    Request.LatestStartWorldTick = 180;
    Request.ResolutionSeed = 0x5A17;
    Request.DecisionProvenanceJson =
        ValidDirectorProvenance(
            TEXT("eligible_window"));

    FOGWorldDirectorScheduleRecord First;
    FOGWorldDirectorScheduleRecord Second;
    TestTrue(
        TEXT("Schedule first deterministic Director decision"),
        Director.ScheduleEligibleContent(
            Request,
            First,
            Error));
    TestTrue(
        TEXT("Schedule second replay-equivalent Director decision"),
        Director.ScheduleEligibleContent(
            Request,
            Second,
            Error));

    TestEqual(
        TEXT("Same seed and authored window resolve the same start tick"),
        First.ScheduledStartWorldTick,
        Second.ScheduledStartWorldTick);
    TestTrue(
        TEXT("Scheduled start respects earliest authored bound"),
        First.ScheduledStartWorldTick >=
            Request.EarliestStartWorldTick);
    TestTrue(
        TEXT("Scheduled start respects latest authored bound"),
        First.ScheduledStartWorldTick <=
            Request.LatestStartWorldTick);

    TestFalse(
        TEXT("Director cannot delay beyond authored latest-start bound"),
        Director.DelayScheduleWithinAuthoredBound(
            First.ScheduleId,
            Request.LatestStartWorldTick + 1,
            110,
            ValidDirectorProvenance(
                TEXT("illegal_delay")),
            Error));

    TestTrue(
        TEXT("Director may delay within authored bound"),
        Director.DelayScheduleWithinAuthoredBound(
            First.ScheduleId,
            Request.LatestStartWorldTick,
            111,
            ValidDirectorProvenance(
                TEXT("bounded_surprise_delay")),
            Error));

    TestTrue(
        TEXT("Catch-up observation activates due content at scheduled canonical start"),
        Director.ActivateDueContent(
            First.ScheduleId,
            250,
            Error));

    bool bUnlockFound = false;
    FOGContentUnlockStateRecord Unlock;
    TestTrue(
        TEXT("Read released content state"),
        Store.TryReadContentUnlockState(
            Request.ContentId,
            bUnlockFound,
            Unlock,
            Error));
    TestTrue(
        TEXT("Content release state exists"),
        bUnlockFound);
    TestEqual(
        TEXT("Release tick is effective scheduled start rather than reconnect time"),
        Unlock.ReleasedWorldTick,
        Request.LatestStartWorldTick);

    bool bScheduleFound = false;
    FOGWorldDirectorScheduleRecord Stored;
    TestTrue(
        TEXT("Read Director schedule"),
        Store.TryReadWorldDirectorSchedule(
            First.ScheduleId,
            bScheduleFound,
            Stored,
            Error));
    TestTrue(
        TEXT("Director schedule persists"),
        bScheduleFound);
    TestTrue(
        TEXT("Decision provenance is retained"),
        Stored.DecisionProvenanceJson.Contains(
            TEXT("package_versions")));
    TestEqual(
        TEXT("Deterministic resolution seed is retained"),
        Stored.ResolutionSeed,
        Request.ResolutionSeed);

    TestTrue(
        TEXT("Complete active Director event"),
        Director.CompleteSchedule(
            First.ScheduleId,
            300,
            TEXT("{\"result\":\"aftermath_persisted\"}"),
            Error));

    Store.Close();

    FOGSQLiteWorldStore Reopened;
    TestTrue(
        TEXT("Reopen Director database"),
        Reopened.Open(
            DatabasePath,
            Error));
    bScheduleFound = false;
    TestTrue(
        TEXT("Read completed schedule after restart"),
        Reopened.TryReadWorldDirectorSchedule(
            First.ScheduleId,
            bScheduleFound,
            Stored,
            Error));
    TestTrue(
        TEXT("Completed schedule survives restart"),
        bScheduleFound);
    TestEqual(
        TEXT("Completed status survives restart"),
        Stored.Status,
        FName(TEXT("completed")));

    Reopened.Close();
    IFileManager::Get().DeleteDirectory(
        *Directory,
        false,
        true);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOGWorldDirectorOfflineGovernorTest,
    "OfflineGame.World.Director.OfflineGovernorDefersNewCatastropheButPreservesLockedConsequences",
    EAutomationTestFlags::ApplicationContextMask |
        EAutomationTestFlags::EngineFilter)

bool FOGWorldDirectorOfflineGovernorTest::RunTest(
    const FString& Parameters)
{
    const FString Directory =
        MakeRealityTimeTestDirectory();
    const FString DatabasePath =
        FPaths::Combine(
            Directory,
            TEXT("offline_governor.db"));
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

    const FOGEntityId ScopeId =
        FOGEntityId::NewId();
    TestTrue(
        TEXT("Persist sovereign scope"),
        PersistDirectorScope(
            Store,
            ScopeId,
            Error));

    FOGWorldDirectorService Director(
        Store);
    TestTrue(
        TEXT("Record last active-session boundary"),
        Director.RecordActiveSessionBoundary(
            ScopeId,
            100,
            Error));

    FOGOfflineCatchupGovernorRequest Request;
    Request.ScopeEntityId =
        ScopeId;
    Request.CatchupWorldTick = 200;
    Request.RequestedNewIrreversibleLossBps = 1600;

    // Test-owned tuning input. Production value comes from the versioned tuning
    // set; the World Director service contains no 15% literal.
    Request.MaxNewIrreversibleLossBps = 1500;
    Request.DecisionStateJson =
        TEXT("{\"cause\":\"new_offline_conflict\"}");

    FOGOfflineCatchupGovernorResult Result;
    TestTrue(
        TEXT("Evaluate newly-generated loss beyond supplied ceiling"),
        Director.EvaluateOfflineCatchupGovernor(
            Request,
            Result,
            Error));
    TestTrue(
        TEXT("Excess newly-generated irreversible loss is deferred"),
        Result.bDeferred);
    TestEqual(
        TEXT("Deferred catastrophe commits no irreversible loss"),
        Result.ApprovedIrreversibleLossBps,
        0);

    Request.CatchupWorldTick = 201;
    Request.RequestedNewIrreversibleLossBps = 1000;
    Request.bWouldCauseProtectedCatastrophicCollapse =
        true;
    Request.DecisionStateJson =
        TEXT("{\"cause\":\"main_domain_collapse\"}");

    TestTrue(
        TEXT("Evaluate protected catastrophic collapse"),
        Director.EvaluateOfflineCatchupGovernor(
            Request,
            Result,
            Error));
    TestTrue(
        TEXT("New protected catastrophic final collapse is deferred"),
        Result.bDeferred);

    Request.CatchupWorldTick = 202;
    Request.RequestedNewIrreversibleLossBps = 3000;
    Request.bWouldCauseProtectedCatastrophicCollapse =
        true;
    Request.bCausallyLockedBeforeLogout =
        true;
    Request.DecisionStateJson =
        TEXT("{\"cause\":\"locked_before_logout\"}");

    TestTrue(
        TEXT("Evaluate consequence causally locked before logout"),
        Director.EvaluateOfflineCatchupGovernor(
            Request,
            Result,
            Error));
    TestTrue(
        TEXT("Causally locked consequence remains approved"),
        Result.bApproved);
    TestFalse(
        TEXT("Locked consequence is not retroactively deferred"),
        Result.bDeferred);
    TestEqual(
        TEXT("Locked consequence is not clipped by new-event ceiling"),
        Result.ApprovedIrreversibleLossBps,
        3000);

    TestFalse(
        TEXT("Active-session boundary cannot move canonical time backward"),
        Director.RecordActiveSessionBoundary(
            ScopeId,
            150,
            Error));

    Store.Close();
    IFileManager::Get().DeleteDirectory(
        *Directory,
        false,
        true);
    return true;
}

#endif
