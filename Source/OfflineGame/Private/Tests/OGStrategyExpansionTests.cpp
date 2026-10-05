#include "Persistence/OGSQLiteWorldStore.h"
#include "World/OGCivilizationLogisticsService.h"
#include "World/OGDispatchService.h"
#include "World/OGFactionWarService.h"
#include "World/OGProjectService.h"
#include "World/OGStrategicResolutionService.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

namespace
{
FString MakeStrategyExpansionTestDirectory()
{
    return FPaths::Combine(
        FPaths::ProjectSavedDir(),
        TEXT("Automation"),
        FGuid::NewGuid().ToString(
            EGuidFormats::Digits));
}

bool PersistStrategyEntity(
    FOGSQLiteWorldStore& Store,
    const FOGEntityId& EntityId,
    FName Kind,
    FString& Error)
{
    return Store.UpsertEntity(
        EntityId,
        Kind,
        0,
        TEXT("{}"),
        Error);
}

bool PersistStrategyLocation(
    FOGSQLiteWorldStore& Store,
    const FOGEntityId& LocationId,
    FString& Error)
{
    FOGLocationRecord Location;
    Location.LocationId =
        LocationId;
    Location.Kind =
        FName(TEXT("region"));

    return Store.UpsertLocation(
        Location,
        0,
        Error);
}

bool PersistStrategyFaction(
    FOGSQLiteWorldStore& Store,
    const FOGEntityId& FactionId,
    FString& Error)
{
    FOGFactionRecord Faction;
    Faction.FactionId =
        FactionId;
    Faction.Kind =
        FName(TEXT("faction"));
    Faction.Population = 1000;

    return Store.UpsertFaction(
        Faction,
        0,
        Error);
}

bool PersistStrategyArmy(
    FOGSQLiteWorldStore& Store,
    const FOGEntityId& ArmyId,
    const FOGEntityId& FactionId,
    const FOGEntityId& LocationId,
    FString& Error)
{
    FOGArmyRecord Army;
    Army.ArmyId =
        ArmyId;
    Army.FactionId =
        FactionId;
    Army.LocationId =
        LocationId;
    Army.Headcount = 100;
    Army.EffectivePower =
        FOGLargeNumber::FromInt64(
            1000000);
    Army.State =
        FName(TEXT("ready"));

    return Store.UpsertArmy(
        Army,
        0,
        Error);
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOGDispatchObjectiveFidelityTest,
    "OfflineGame.Strategy.Dispatch.MandatoryObjectivesCannotBeSilentlyBypassed",
    EAutomationTestFlags::ApplicationContextMask |
        EAutomationTestFlags::EngineFilter)

bool FOGDispatchObjectiveFidelityTest::RunTest(
    const FString& Parameters)
{
    const FString Directory =
        MakeStrategyExpansionTestDirectory();
    const FString DatabasePath =
        FPaths::Combine(
            Directory,
            TEXT("dispatch_policy.db"));
    IFileManager::Get().MakeDirectory(
        *Directory,
        true);

    FOGSQLiteWorldStore Store;
    FString Error;
    TestTrue(
        TEXT("Open schema-13 database"),
        Store.Open(
            DatabasePath,
            Error));
    TestEqual(
        TEXT("Schema version is 13"),
        Store.GetSchemaVersion(
            Error),
        13);

    const FOGEntityId OwnerId =
        FOGEntityId::NewId();
    const FOGEntityId ParticipantId =
        FOGEntityId::NewId();
    const FOGEntityId RescueTargetId =
        FOGEntityId::NewId();

    TestTrue(
        TEXT("Persist Dispatch owner"),
        PersistStrategyEntity(
            Store,
            OwnerId,
            FName(TEXT("ruler")),
            Error));
    TestTrue(
        TEXT("Persist Dispatch participant"),
        PersistStrategyEntity(
            Store,
            ParticipantId,
            FName(TEXT("character")),
            Error));
    TestTrue(
        TEXT("Persist rescue target"),
        PersistStrategyEntity(
            Store,
            RescueTargetId,
            FName(TEXT("character")),
            Error));

    FOGDispatchObjectiveRecord Rescue;
    Rescue.ObjectiveId =
        FOGContentId(
            TEXT("test:objective.rescue_character"));
    Rescue.Priority = 100;
    Rescue.bMandatory = true;
    Rescue.TargetEntityId =
        RescueTargetId;

    FOGDispatchObjectiveRecord IncidentalResource;
    IncidentalResource.ObjectiveId =
        FOGContentId(
            TEXT("test:objective.collect_incidental_resource"));
    IncidentalResource.Priority = 10;
    IncidentalResource.bMandatory = false;

    FOGDispatchConstraintRecord NoAbandonment;
    NoAbandonment.ConstraintId =
        FOGContentId(
            TEXT("test:constraint.do_not_abandon_rescue"));
    NoAbandonment.StateJson =
        TEXT("{\"hard\":true}");

    FOGDispatchService Service(
        Store);

    FOGEntityId DispatchId;
    TestTrue(
        TEXT("Start objective-faithful rescue Dispatch"),
        Service.StartDispatch(
            OwnerId,
            RescueTargetId,
            EOGDispatchType::Support,
            {ParticipantId},
            10,
            20,
            2500,
            3000,
            TEXT("{\"abort_if\":\"survival_threshold\"}"),
            {Rescue, IncidentalResource},
            {NoAbandonment},
            777,
            DispatchId,
            Error));

    FOGDispatchRecord Resolved;
    TestFalse(
        TEXT("Simple success/failure finalizer cannot bypass normalized mission policy"),
        Service.ResolveDispatch(
            DispatchId,
            20,
            true,
            TEXT("{\"farm_only\":true}"),
            Resolved,
            Error));

    TestFalse(
        TEXT("Resolver that skips mandatory rescue gate is rejected"),
        Service.ResolveDispatchWithPolicy(
            DispatchId,
            20,
            [](
                const FOGDispatchResolutionContext&,
                FOGDispatchResolutionResult& OutResult,
                FString& OutError)
            {
                OutError.Reset();
                OutResult.bMandatoryObjectivesEvaluated =
                    false;
                OutResult.bHardConstraintsEvaluated =
                    true;
                OutResult.bSurvivalAbortPolicyEvaluated =
                    true;
                OutResult.bSucceeded =
                    true;
                OutResult.OutcomeState =
                    FName(TEXT("resource_farmed"));
                return true;
            },
            Resolved,
            Error));

    bool bSawMandatoryRescue = false;
    bool bSawSecondaryOpportunity = false;
    bool bSawHardConstraint = false;

    TestTrue(
        TEXT("Policy-aware resolver completes the mandatory rescue first"),
        Service.ResolveDispatchWithPolicy(
            DispatchId,
            20,
            [&bSawMandatoryRescue,
             &bSawSecondaryOpportunity,
             &bSawHardConstraint](
                const FOGDispatchResolutionContext& Context,
                FOGDispatchResolutionResult& OutResult,
                FString& OutError)
            {
                OutError.Reset();

                bSawMandatoryRescue =
                    Context.MandatoryObjectives.Num() == 1 &&
                    Context.MandatoryObjectives[0].ObjectiveId ==
                        FOGContentId(
                            TEXT("test:objective.rescue_character"));
                bSawSecondaryOpportunity =
                    Context.SecondaryObjectives.Num() == 1 &&
                    Context.SecondaryObjectives[0].ObjectiveId ==
                        FOGContentId(
                            TEXT("test:objective.collect_incidental_resource"));
                bSawHardConstraint =
                    Context.HardConstraints.Num() == 1 &&
                    Context.HardConstraints[0].ConstraintId ==
                        FOGContentId(
                            TEXT("test:constraint.do_not_abandon_rescue"));

                OutResult.bMandatoryObjectivesEvaluated =
                    true;
                OutResult.bHardConstraintsEvaluated =
                    true;
                OutResult.bSurvivalAbortPolicyEvaluated =
                    true;
                OutResult.bSucceeded =
                    true;
                OutResult.OutcomeState =
                    FName(TEXT("rescue_complete"));
                OutResult.ResultJson =
                    TEXT("{\"rescued\":true,\"incidental_resource\":false}");
                return true;
            },
            Resolved,
            Error));

    TestTrue(
        TEXT("Mandatory rescue was visible to resolver"),
        bSawMandatoryRescue);
    TestTrue(
        TEXT("Secondary opportunity remained lower-priority context"),
        bSawSecondaryOpportunity);
    TestTrue(
        TEXT("Hard mission constraint was preserved"),
        bSawHardConstraint);
    TestEqual(
        TEXT("Resolved outcome remains explicit"),
        Resolved.OutcomeState,
        FName(TEXT("rescue_complete")));

    Store.Close();

    FOGSQLiteWorldStore Reopened;
    TestTrue(
        TEXT("Reopen Dispatch database"),
        Reopened.Open(
            DatabasePath,
            Error));

    bool bFound = false;
    FOGDispatchRecord Persisted;
    TestTrue(
        TEXT("Read policy-bearing Dispatch after restart"),
        Reopened.TryReadDispatch(
            DispatchId,
            bFound,
            Persisted,
            Error));
    TestTrue(
        TEXT("Dispatch survives restart"),
        bFound);
    TestEqual(
        TEXT("Risk tolerance survives restart"),
        Persisted.RiskToleranceBps,
        3000);
    TestTrue(
        TEXT("Abort policy survives restart"),
        Persisted.AbortPolicyJson.Contains(
            TEXT("survival_threshold")));

    TArray<FOGDispatchObjectiveRecord> Objectives;
    TArray<FOGDispatchConstraintRecord> Constraints;
    TestTrue(
        TEXT("Read Dispatch objectives after restart"),
        Reopened.ListDispatchObjectives(
            DispatchId,
            Objectives,
            Error));
    TestTrue(
        TEXT("Read Dispatch constraints after restart"),
        Reopened.ListDispatchConstraints(
            DispatchId,
            Constraints,
            Error));
    TestEqual(
        TEXT("Both mission objectives survive restart"),
        Objectives.Num(),
        2);
    TestEqual(
        TEXT("Hard constraint survives restart"),
        Constraints.Num(),
        1);

    Reopened.Close();
    IFileManager::Get().DeleteDirectory(
        *Directory,
        false,
        true);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOGContinuousWarStateTest,
    "OfflineGame.Strategy.War.MultipleFrontsOrdersDisobedienceAndParticipantHistory",
    EAutomationTestFlags::ApplicationContextMask |
        EAutomationTestFlags::EngineFilter)

bool FOGContinuousWarStateTest::RunTest(
    const FString& Parameters)
{
    const FString Directory =
        MakeStrategyExpansionTestDirectory();
    const FString DatabasePath =
        FPaths::Combine(
            Directory,
            TEXT("continuous_war.db"));
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

    const FOGEntityId FactionA =
        FOGEntityId::NewId();
    const FOGEntityId FactionB =
        FOGEntityId::NewId();
    const FOGEntityId FactionC =
        FOGEntityId::NewId();
    const FOGEntityId LocationA =
        FOGEntityId::NewId();
    const FOGEntityId LocationB =
        FOGEntityId::NewId();
    const FOGEntityId ArmyA =
        FOGEntityId::NewId();

    TestTrue(
        TEXT("Persist faction A"),
        PersistStrategyFaction(
            Store,
            FactionA,
            Error));
    TestTrue(
        TEXT("Persist faction B"),
        PersistStrategyFaction(
            Store,
            FactionB,
            Error));
    TestTrue(
        TEXT("Persist faction C"),
        PersistStrategyFaction(
            Store,
            FactionC,
            Error));
    TestTrue(
        TEXT("Persist front location A"),
        PersistStrategyLocation(
            Store,
            LocationA,
            Error));
    TestTrue(
        TEXT("Persist front location B"),
        PersistStrategyLocation(
            Store,
            LocationB,
            Error));
    TestTrue(
        TEXT("Persist recipient Army"),
        PersistStrategyArmy(
            Store,
            ArmyA,
            FactionA,
            LocationA,
            Error));

    FOGFactionWarService WarService(
        Store);

    FOGEntityId WarId;
    TestTrue(
        TEXT("Declare persistent War"),
        WarService.DeclareWar(
            FactionA,
            FactionB,
            FName(TEXT("control_route")),
            LocationA,
            10,
            WarId,
            Error));

    FOGEntityId FrontA;
    FOGEntityId FrontB;
    TestTrue(
        TEXT("Create first War front"),
        WarService.CreateWarFront(
            WarId,
            LocationA,
            FOGEntityId(),
            11,
            TEXT("{\"axis\":\"north\"}"),
            FrontA,
            Error));
    TestTrue(
        TEXT("Create second simultaneous War front"),
        WarService.CreateWarFront(
            WarId,
            LocationB,
            FOGEntityId(),
            12,
            TEXT("{\"axis\":\"south\"}"),
            FrontB,
            Error));

    FOGWarObjectiveRecord Objective;
    Objective.WarId =
        WarId;
    Objective.FrontId =
        FrontA;
    Objective.ObjectiveId =
        FOGContentId(
            TEXT("test:war_objective.hold_route"));
    Objective.TargetEntityId =
        LocationA;
    Objective.Priority = 100;
    Objective.Status =
        FName(TEXT("active"));

    TestTrue(
        TEXT("Persist front-specific War objective"),
        WarService.SetWarObjective(
            Objective,
            Error));

    FOGEntityId OrderId;
    TestTrue(
        TEXT("Issue intent-based War order"),
        WarService.IssueWarOrder(
            WarId,
            FrontA,
            FactionA,
            ArmyA,
            FOGContentId(
                TEXT("test:war_intent.hold_without_excessive_losses")),
            TEXT("{\"avoid_excessive_casualties\":true}"),
            13,
            OrderId,
            Error));

    TestTrue(
        TEXT("Record commander disobedience explicitly"),
        WarService.RecordWarOrderOutcome(
            OrderId,
            FName(TEXT("disobeyed")),
            TEXT("{\"reason\":\"commander_survival_judgment\"}"),
            14,
            Error));

    TestTrue(
        TEXT("Third faction joins ongoing War"),
        WarService.JoinWar(
            WarId,
            FactionC,
            0,
            false,
            15,
            Error));
    TestTrue(
        TEXT("Third faction later leaves ongoing War"),
        WarService.LeaveWar(
            WarId,
            FactionC,
            FName(TEXT("withdrawal")),
            16,
            Error));

    TArray<FOGWarFrontRecord> Fronts;
    TestTrue(
        TEXT("List simultaneous fronts"),
        Store.ListWarFronts(
            WarId,
            Fronts,
            Error));
    TestEqual(
        TEXT("War retains both active fronts"),
        Fronts.Num(),
        2);

    bool bOrderFound = false;
    FOGWarOrderRecord Order;
    TestTrue(
        TEXT("Read original War order"),
        Store.TryReadWarOrder(
            OrderId,
            bOrderFound,
            Order,
            Error));
    TestTrue(
        TEXT("Order exists"),
        bOrderFound);
    TestTrue(
        TEXT("Original intent remains unchanged"),
        Order.IntentId ==
            FOGContentId(
                TEXT("test:war_intent.hold_without_excessive_losses")));
    TestEqual(
        TEXT("Disobedience is explicit outcome state"),
        Order.OutcomeState,
        FName(TEXT("disobeyed")));

    TArray<FOGWarParticipantHistoryRecord> History;
    TestTrue(
        TEXT("Read War participant history"),
        Store.ListWarParticipantHistory(
            WarId,
            History,
            Error));

    bool bFoundClosedThirdFactionHistory = false;
    for (const FOGWarParticipantHistoryRecord& Row :
         History)
    {
        if (Row.FactionId == FactionC &&
            Row.bHasLeftWorldTick &&
            Row.LeftWorldTick == 16 &&
            Row.Reason == FName(TEXT("withdrawal")))
        {
            bFoundClosedThirdFactionHistory =
                true;
        }
    }

    TestTrue(
        TEXT("Join/leave history survives current participant projection"),
        bFoundClosedThirdFactionHistory);

    Store.Close();
    IFileManager::Get().DeleteDirectory(
        *Directory,
        false,
        true);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOGArmyCapabilityVectorResolutionTest,
    "OfflineGame.Strategy.Army.CapabilityVectorsAreAuthoritativeNotSummaryPower",
    EAutomationTestFlags::ApplicationContextMask |
        EAutomationTestFlags::EngineFilter)

bool FOGArmyCapabilityVectorResolutionTest::RunTest(
    const FString& Parameters)
{
    const FString Directory =
        MakeStrategyExpansionTestDirectory();
    const FString DatabasePath =
        FPaths::Combine(
            Directory,
            TEXT("army_capabilities.db"));
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

    const FOGEntityId FactionA =
        FOGEntityId::NewId();
    const FOGEntityId FactionB =
        FOGEntityId::NewId();
    const FOGEntityId LocationId =
        FOGEntityId::NewId();
    const FOGEntityId ArmyA =
        FOGEntityId::NewId();
    const FOGEntityId ArmyB =
        FOGEntityId::NewId();

    TestTrue(
        TEXT("Persist faction A"),
        PersistStrategyFaction(
            Store,
            FactionA,
            Error));
    TestTrue(
        TEXT("Persist faction B"),
        PersistStrategyFaction(
            Store,
            FactionB,
            Error));
    TestTrue(
        TEXT("Persist Army location"),
        PersistStrategyLocation(
            Store,
            LocationId,
            Error));
    TestTrue(
        TEXT("Persist Army A"),
        PersistStrategyArmy(
            Store,
            ArmyA,
            FactionA,
            LocationId,
            Error));
    TestTrue(
        TEXT("Persist Army B with same cached EffectivePower"),
        PersistStrategyArmy(
            Store,
            ArmyB,
            FactionB,
            LocationId,
            Error));

    FOGStrategicResolutionService Resolution(
        Store);
    FOGStrategicArmyResolutionResult Outcome;

    TestFalse(
        TEXT("Cached EffectivePower alone cannot resolve Army interaction"),
        Resolution.ResolveArmyInteraction(
            {ArmyA, ArmyB},
            5,
            [](
                const TArray<FOGArmyResolutionInput>&,
                int64,
                FOGStrategicArmyResolutionResult& Out,
                FString& OutError)
            {
                OutError.Reset();
                Out.Outcome =
                    FName(TEXT("invalid_summary_only"));
                return true;
            },
            Outcome,
            Error));

    FOGArmyCapabilityRecord ARange;
    ARange.ArmyId =
        ArmyA;
    ARange.CapabilityId =
        FOGContentId(
            TEXT("test:capability.long_range"));
    ARange.Magnitude =
        FOGLargeNumber::FromInt64(
            900);

    FOGArmyCapabilityRecord AMobility;
    AMobility.ArmyId =
        ArmyA;
    AMobility.CapabilityId =
        FOGContentId(
            TEXT("test:capability.teleport_mobility"));
    AMobility.Magnitude =
        FOGLargeNumber::FromInt64(
            400);

    FOGArmyCapabilityRecord BDefense;
    BDefense.ArmyId =
        ArmyB;
    BDefense.CapabilityId =
        FOGContentId(
            TEXT("test:capability.fortification"));
    BDefense.Magnitude =
        FOGLargeNumber::FromInt64(
            1000);

    TestTrue(
        TEXT("Persist Army A range capability"),
        Resolution.SetArmyCapability(
            ARange,
            Error));
    TestTrue(
        TEXT("Persist Army A mobility capability"),
        Resolution.SetArmyCapability(
            AMobility,
            Error));
    TestTrue(
        TEXT("Persist Army B defense capability"),
        Resolution.SetArmyCapability(
            BDefense,
            Error));

    bool bSawCapabilityVectors = false;
    TestTrue(
        TEXT("Resolve through normalized capability vectors"),
        Resolution.ResolveArmyInteraction(
            {ArmyA, ArmyB},
            6,
            [&bSawCapabilityVectors](
                const TArray<FOGArmyResolutionInput>& Inputs,
                int64,
                FOGStrategicArmyResolutionResult& Out,
                FString& OutError)
            {
                OutError.Reset();
                bSawCapabilityVectors =
                    Inputs.Num() == 2 &&
                    Inputs[0].Capabilities.Num() == 2 &&
                    Inputs[1].Capabilities.Num() == 1;

                Out.Outcome =
                    FName(TEXT("maneuver_advantage"));
                Out.OutcomeJson =
                    TEXT("{\"basis\":\"capability_vectors\"}");
                return true;
            },
            Outcome,
            Error));

    TestTrue(
        TEXT("Resolver received normalized capability vectors"),
        bSawCapabilityVectors);
    TestEqual(
        TEXT("Capability-based outcome is explicit"),
        Outcome.Outcome,
        FName(TEXT("maneuver_advantage")));

    Store.Close();
    IFileManager::Get().DeleteDirectory(
        *Directory,
        false,
        true);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOGProjectsCivilizationAndLogisticsTest,
    "OfflineGame.Strategy.World.ProjectsCivilizationDimensionsAndCapabilityRoutes",
    EAutomationTestFlags::ApplicationContextMask |
        EAutomationTestFlags::EngineFilter)

bool FOGProjectsCivilizationAndLogisticsTest::RunTest(
    const FString& Parameters)
{
    const FString Directory =
        MakeStrategyExpansionTestDirectory();
    const FString DatabasePath =
        FPaths::Combine(
            Directory,
            TEXT("civilization_logistics.db"));
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

    const FOGEntityId CivilizationId =
        FOGEntityId::NewId();
    const FOGEntityId SpecialistId =
        FOGEntityId::NewId();
    const FOGEntityId OriginId =
        FOGEntityId::NewId();
    const FOGEntityId DestinationId =
        FOGEntityId::NewId();

    TestTrue(
        TEXT("Persist civilization entity"),
        PersistStrategyEntity(
            Store,
            CivilizationId,
            FName(TEXT("faction")),
            Error));
    TestTrue(
        TEXT("Persist named specialist"),
        PersistStrategyEntity(
            Store,
            SpecialistId,
            FName(TEXT("character")),
            Error));
    TestTrue(
        TEXT("Persist origin"),
        PersistStrategyLocation(
            Store,
            OriginId,
            Error));
    TestTrue(
        TEXT("Persist destination"),
        PersistStrategyLocation(
            Store,
            DestinationId,
            Error));

    FOGCivilizationLogisticsService Civilization(
        Store);

    FOGCivilizationStateRecord State;
    State.CivilizationEntityId =
        CivilizationId;
    State.GenreProfileId =
        FOGContentId(
            TEXT("test:civilization.cultivation_kingdom"));
    State.StateJson =
        TEXT("{\"identity_preserved\":true}");

    TestTrue(
        TEXT("Persist authored civilization identity profile"),
        Civilization.SetCivilizationState(
            State,
            Error));

    FOGCivilizationDimensionRecord Cultivation;
    Cultivation.CivilizationEntityId =
        CivilizationId;
    Cultivation.DimensionId =
        FOGContentId(
            TEXT("test:civ_dimension.cultivation_infrastructure"));
    Cultivation.Grade =
        FName(TEXT("advanced"));

    FOGCivilizationDimensionRecord Shipbuilding;
    Shipbuilding.CivilizationEntityId =
        CivilizationId;
    Shipbuilding.DimensionId =
        FOGContentId(
            TEXT("test:civ_dimension.shipbuilding"));
    Shipbuilding.Grade =
        FName(TEXT("basic"));

    TestTrue(
        TEXT("Persist advanced cultivation dimension"),
        Civilization.SetCivilizationDimension(
            Cultivation,
            Error));
    TestTrue(
        TEXT("Persist independently weak shipbuilding dimension"),
        Civilization.SetCivilizationDimension(
            Shipbuilding,
            Error));

    TArray<FOGCivilizationDimensionRecord> Dimensions;
    TestTrue(
        TEXT("List multidimensional civilization state"),
        Store.ListCivilizationDimensions(
            CivilizationId,
            Dimensions,
            Error));
    TestEqual(
        TEXT("Civilization keeps asymmetric authored dimensions"),
        Dimensions.Num(),
        2);

    FOGProjectService Projects(
        Store);
    FOGEntityId ProjectId;
    TestTrue(
        TEXT("Start generic long-running Project"),
        Projects.StartProject(
            CivilizationId,
            OriginId,
            FOGContentId(
                TEXT("test:project.reverse_engineer_gate")),
            {},
            10,
            30,
            TEXT("{\"objective\":\"learn_gate_principle\"}"),
            ProjectId,
            Error));

    FOGProjectPhaseRecord ResearchPhase;
    ResearchPhase.ProjectId =
        ProjectId;
    ResearchPhase.PhaseId =
        FOGContentId(
            TEXT("test:project_phase.analysis"));
    ResearchPhase.Sequence = 0;
    ResearchPhase.Status =
        FName(TEXT("active"));
    ResearchPhase.StartWorldTick = 10;
    ResearchPhase.ResolveWorldTick = 20;

    FOGProjectPhaseRecord PrototypePhase;
    PrototypePhase.ProjectId =
        ProjectId;
    PrototypePhase.PhaseId =
        FOGContentId(
            TEXT("test:project_phase.prototype"));
    PrototypePhase.Sequence = 1;
    PrototypePhase.Status =
        FName(TEXT("planned"));
    PrototypePhase.StartWorldTick = 20;
    PrototypePhase.ResolveWorldTick = 30;

    TestTrue(
        TEXT("Persist first meaningful Project phase"),
        Projects.SetProjectPhase(
            ResearchPhase,
            Error));
    TestTrue(
        TEXT("Persist second meaningful Project phase"),
        Projects.SetProjectPhase(
            PrototypePhase,
            Error));

    FOGProjectAssignmentRecord Assignment;
    Assignment.ProjectId =
        ProjectId;
    Assignment.AssigneeEntityId =
        SpecialistId;
    Assignment.RoleId =
        FOGContentId(
            TEXT("test:project_role.dimensional_specialist"));

    TestTrue(
        TEXT("Assign named specialist without worker micromanagement"),
        Projects.AssignProjectRole(
            Assignment,
            Error));

    TArray<FOGProjectPhaseRecord> Phases;
    TestTrue(
        TEXT("Lazy-refresh Project phases"),
        Projects.RefreshProjectPhases(
            ProjectId,
            25,
            Phases,
            Error));
    TestEqual(
        TEXT("Both authored Project phases remain"),
        Phases.Num(),
        2);
    TestEqual(
        TEXT("Analysis phase completed lazily"),
        Phases[0].Status,
        FName(TEXT("completed")));
    TestEqual(
        TEXT("Prototype phase is active at current tick"),
        Phases[1].Status,
        FName(TEXT("active")));

    FOGLogisticsRouteRecord Route;
    Route.RouteId =
        FOGEntityId::NewId();
    Route.OwnerEntityId =
        CivilizationId;
    Route.OriginLocationId =
        OriginId;
    Route.DestinationLocationId =
        DestinationId;
    Route.TransportCapabilityId =
        FOGContentId(
            TEXT("test:transport.teleport_gate"));
    Route.Capacity =
        FOGLargeNumber::FromInt64(
            500);
    Route.RiskBps = 500;
    Route.Status =
        FName(TEXT("bypassed"));

    bool bValidatedTeleportCapability = false;
    TestTrue(
        TEXT("Actual teleport capability may establish a bypass route"),
        Civilization.SaveLogisticsRoute(
            Route,
            40,
            [&bValidatedTeleportCapability](
                const FOGEntityId&,
                const FOGContentId& CapabilityId,
                const FOGEntityId&,
                const FOGEntityId&,
                const FOGEntityId&,
                const FOGEntityId&,
                FString& OutError)
            {
                OutError.Reset();
                bValidatedTeleportCapability =
                    CapabilityId ==
                        FOGContentId(
                            TEXT("test:transport.teleport_gate"));
                return bValidatedTeleportCapability;
            },
            Error));
    TestTrue(
        TEXT("Route creation consulted actual transport capability"),
        bValidatedTeleportCapability);

    Store.Close();
    IFileManager::Get().DeleteDirectory(
        *Directory,
        false,
        true);
    return true;
}

#endif
