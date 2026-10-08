#include "Runtime/OGFoundationStrategicRuntime.h"
#include "Combat/OGDiagnosticGacha.h"
#include "Persistence/OGSQLiteWorldStore.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

namespace
{
struct FStrategicTestStore
{
    FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), FGuid::NewGuid().ToString(EGuidFormats::Digits));
    FOGSQLiteWorldStore Store;
    FStrategicTestStore() { IFileManager::Get().MakeDirectory(*Directory, true); }
    ~FStrategicTestStore() { Store.Close(); IFileManager::Get().DeleteDirectory(*Directory, false, true); }
    bool Open(FString& Error) { return Store.Open(FPaths::Combine(Directory, TEXT("strategy.db")), Error); }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOGFoundationNestedTransactionTest,
    "OfflineGame.Foundation.Persistence.NestedScopesPreserveOuterAtomicity",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FOGFoundationNestedTransactionTest::RunTest(const FString&)
{
    FStrategicTestStore Fixture; FString Error;
    if (!TestTrue(TEXT("Open canonical test store"), Fixture.Open(Error))) return false;
    const FOGEntityId Owner = FOGEntityId::NewId();
    const FOGContentId Resource(TEXT("diagnostic:nested_materials"));
    if (!TestTrue(TEXT("Seed canonical resource owner"),
        Fixture.Store.UpsertEntity(Owner, TEXT("diagnostic_owner"), 0, TEXT("{}"), Error)))
    {
        return false;
    }
    TestTrue(TEXT("Outer transaction"), Fixture.Store.BeginTransaction(Error));
    TestTrue(TEXT("Outer write"), Fixture.Store.SetResourceBalance(Owner, Resource, 10, Error));
    TestTrue(TEXT("Inner savepoint"), Fixture.Store.BeginTransaction(Error));
    TestTrue(TEXT("Inner write"), Fixture.Store.SetResourceBalance(Owner, Resource, 3, Error));
    TestTrue(TEXT("Rollback closes only inner savepoint"), Fixture.Store.RollbackTransaction(Error));
    bool bFound = false; int64 Balance = 0;
    TestTrue(TEXT("Read outer state after inner rollback"), Fixture.Store.TryReadResourceBalance(Owner, Resource, bFound, Balance, Error));
    TestEqual(TEXT("Outer write remains"), Balance, int64(10));
    TestTrue(TEXT("Second inner savepoint"), Fixture.Store.BeginTransaction(Error));
    TestTrue(TEXT("Second inner write"), Fixture.Store.SetResourceBalance(Owner, Resource, 8, Error));
    TestTrue(TEXT("Inner commit releases only savepoint"), Fixture.Store.CommitTransaction(Error));
    TestTrue(TEXT("Outer rollback also discards released inner write"), Fixture.Store.RollbackTransaction(Error));
    TestTrue(TEXT("Read rolled back state"), Fixture.Store.TryReadResourceBalance(Owner, Resource, bFound, Balance, Error));
    TestFalse(TEXT("Nothing became durable through inner commit"), bFound);
    TestTrue(TEXT("New outer transaction after rollback"), Fixture.Store.BeginTransaction(Error));
    TestTrue(TEXT("New outer write"), Fixture.Store.SetResourceBalance(Owner, Resource, 11, Error));
    TestTrue(TEXT("New inner transaction"), Fixture.Store.BeginTransaction(Error));
    TestTrue(TEXT("New inner write"), Fixture.Store.SetResourceBalance(Owner, Resource, 7, Error));
    TestTrue(TEXT("Close while nested rolls back every level"), Fixture.Store.IsOpen());
    Fixture.Store.Close();
    TestTrue(TEXT("Reopen resets all transaction depth"), Fixture.Open(Error));
    TestTrue(TEXT("Read after close rollback"), Fixture.Store.TryReadResourceBalance(Owner, Resource, bFound, Balance, Error));
    TestFalse(TEXT("Uncommitted nested state did not survive close"), bFound);
    TestTrue(TEXT("Begin after close"), Fixture.Store.BeginTransaction(Error));
    TestTrue(TEXT("Commit after close"), Fixture.Store.CommitTransaction(Error));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOGFoundationStrategicCanonicalCommandTest,
    "OfflineGame.Foundation.Strategy.CommandsReportsAndSeedUseSharedCanonicalRuler",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FOGFoundationStrategicCanonicalCommandTest::RunTest(const FString&)
{
    FStrategicTestStore Fixture; FString Error;
    if (!TestTrue(TEXT("Open test store"), Fixture.Open(Error))) return false;
    const auto Owner = OGDiagnosticGacha::ScopedId(1);
    if (!TestTrue(TEXT("Initialize shared training Ruler"), OGDiagnosticGacha::EnsureScopedFixture(Fixture.Store, 10, Error))) return false;
    FOGFoundationStrategicRuntime Runtime(Fixture.Store); FOGFoundationStrategicContext Context;
    if (!TestTrue(TEXT("Seed isolated records for same Ruler"), Runtime.EnsureDiagnosticContext(Owner, 10, Context, Error))) return false;
    TestTrue(TEXT("Strategy uses shared canonical owner"), Context.OwnerId == Owner);
    FOGFoundationStrategicCommandResult Result;
    if (!TestTrue(TEXT("Start authored diagnostic Project"), Runtime.ExecuteDiagnosticAction(Context,
        EOGFoundationStrategicAction::StartProject, 10, 5, Result, Error))) return false;
    bool bFound = false; FOGProjectRecord Project;
    TestTrue(TEXT("Project is persisted"), Fixture.Store.TryReadProject(Result.EntityId, bFound, Project, Error));
    TestTrue(TEXT("Project exists"), bFound);
    TestTrue(TEXT("Project owner is shared Ruler"), Project.OwnerEntityId == Owner);
    FOGReportRecord Report; FOGWorldEvent Event;
    TestTrue(TEXT("Canonical event exists"), Fixture.Store.TryReadWorldEvent(Result.EventId, bFound, Event, Error));
    TestTrue(TEXT("Read canonical Report"), Fixture.Store.TryReadReport(Result.ReportId, bFound, Report, Error));
    TestTrue(TEXT("Report references actual event"), Report.SourceWorldEventId == Event.EventId && Report.OwnerEntityId == Owner);
    int64 Balance = 0;
    TestTrue(TEXT("Read material cost"), Fixture.Store.TryReadResourceBalance(Owner, Context.ResourceIds[0], bFound, Balance, Error));
    TestEqual(TEXT("Exactly authored materials spent"), Balance, int64(18));
    TArray<FOGWorldEvent> Before, After;
    Fixture.Store.ListWorldEvents(Owner, NAME_None, false, 256, Before, Error);
    FOGFoundationStrategicContext Recovered;
    TestTrue(TEXT("Seed replay does not reset progressed fixture"), Runtime.EnsureDiagnosticContext(Owner, 11, Recovered, Error));
    Fixture.Store.TryReadResourceBalance(Owner, Context.ResourceIds[0], bFound, Balance, Error);
    TestEqual(TEXT("Seed never refills spent materials"), Balance, int64(18));
    TestTrue(TEXT("Project handle recovered from actual canonical event"), Recovered.ProjectIds.Contains(Project.ProjectId));
    FOGFoundationStrategicCommand Command; Command.Action = EOGFoundationStrategicAction::StartProject;
    Command.WorldTick = 11; Command.DueWorldTick = 15; Command.DefinitionId = FOGContentId(TEXT("diagnostic:too_expensive"));
    FOGProjectResourceCost Cost; Cost.ResourceId = Context.ResourceIds[0]; Cost.Amount = 19; Command.ResourceCosts.Add(Cost);
    TestFalse(TEXT("Insufficient resources are rejected"), Runtime.Execute(Context, Command, Result, Error));
    Fixture.Store.ListWorldEvents(Owner, NAME_None, false, 256, After, Error);
    TestEqual(TEXT("Rejected command creates no event"), After.Num(), Before.Num());
    Fixture.Store.TryReadResourceBalance(Owner, Context.ResourceIds[0], bFound, Balance, Error);
    TestEqual(TEXT("Rejected command preserves materials"), Balance, int64(18));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOGFoundationStrategicDueReplayTest,
    "OfflineGame.Foundation.Strategy.DirectorReplayDoesNotDuplicateCompletionReports",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FOGFoundationStrategicDueReplayTest::RunTest(const FString&)
{
    FStrategicTestStore Fixture; FString Error;
    if (!TestTrue(TEXT("Open test store"), Fixture.Open(Error))) return false;
    const auto Owner = OGDiagnosticGacha::ScopedId(1);
    if (!OGDiagnosticGacha::EnsureScopedFixture(Fixture.Store, 20, Error)) return false;
    FOGFoundationStrategicRuntime Runtime(Fixture.Store); FOGFoundationStrategicContext Context;
    if (!Runtime.EnsureDiagnosticContext(Owner, 20, Context, Error)) return false;
    FOGFoundationStrategicCommandResult Result;
    if (!Runtime.ExecuteDiagnosticAction(Context, EOGFoundationStrategicAction::StartProject, 20, 4, Result, Error)) return false;
    FOGWorldDirectorScheduleRecord Schedule;
    TestTrue(TEXT("Real Project deadline scheduled through Director"), Runtime.ScheduleDueAction(Context, Result.EntityId, TEXT("project"), 20, 24, Schedule, Error));
    TestTrue(TEXT("Director activates due content"), FOGWorldDirectorService(Fixture.Store).ActivateDueContent(Schedule.ScheduleId, 30, Error));
    bool bFound = false;
    const FOGEntityId PersistedScheduleId = Schedule.ScheduleId;
    if (!TestTrue(TEXT("Read activated canonical schedule"), Fixture.Store.TryReadWorldDirectorSchedule(PersistedScheduleId, bFound, Schedule, Error))) return false;
    if (!TestTrue(TEXT("Activated schedule exists"), bFound)) return false;
    FString Outcome;
    const bool bFirstResolved = FOGFoundationStrategicRuntime::ResolveDueAction(Fixture.Store, Schedule, 30, true, Outcome, Error);
    TestTrue(TEXT("First due callback completes canonical Project"), bFirstResolved);
    if (!bFirstResolved) AddError(Error);
    TArray<FOGReportRecord> Before, After;
    Fixture.Store.ListReportsByOwner(Owner, Before, Error);
    TestTrue(TEXT("Crash retry is accepted idempotently"), FOGFoundationStrategicRuntime::ResolveDueAction(Fixture.Store, Schedule, 31, true, Outcome, Error));
    Fixture.Store.ListReportsByOwner(Owner, After, Error);
    TestEqual(TEXT("Retry emits no duplicate completion Report"), After.Num(), Before.Num());
    FOGProjectRecord Project;
    Fixture.Store.TryReadProject(Context.ProjectIds.Last(), bFound, Project, Error);
    TestTrue(TEXT("Canonical Project completed"), Project.Status == EOGProjectStatus::Completed);
    return true;
}
#endif
