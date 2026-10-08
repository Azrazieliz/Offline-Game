#pragma once

#include "CoreMinimal.h"
#include "Persistence/OGWorldStore.h"
#include "World/OGCivilizationLogisticsService.h"
#include "World/OGDispatchService.h"
#include "World/OGStrategicResolutionService.h"
#include "World/OGWorldDirectorService.h"

class FOGCanonicalClockRuntime;

/** Selection handles only. Every projected value is read from the canonical store. */
struct OFFLINEGAME_API FOGFoundationStrategicContext
{
    FOGEntityId OwnerId;
    FOGEntityId TerritoryId;
    FOGEntityId ClaimId;
    FOGEntityId CoreId;
    FOGEntityId LocationId;
    FOGEntityId DestinationLocationId;
    FOGEntityId RealityId;
    FOGEntityId DestinationRealityId;
    FOGEntityId JunctionId;
    FOGEntityId FactionId;
    FOGEntityId OtherFactionId;
    FOGEntityId ArmyId;
    FOGEntityId OtherArmyId;
    FOGEntityId WarId;
    FOGEntityId FrontId;
    FOGEntityId OrderId;
    TArray<FOGEntityId> ProjectIds;
    TArray<FOGEntityId> DispatchIds;
    TArray<FOGEntityId> ScheduleIds;
    TArray<FOGContentId> ResourceIds;
    bool bDiagnostic = false;
};

enum class EOGFoundationStrategicAction : uint8
{
    StartProject, RefreshProject, StartDispatch, ResolveDispatch,
    DisplaceClaim, ReclaimClaim, ExpireClaim, ActivateCore, DamageCore,
    CaptureCore, FuseCores, RefreshHeart,
    SaveArmy, ResolveArmyInteraction, DeclareWar, CreateWarFront,
    IssueWarOrder, RecordWarOrderOutcome, ResolveWar,
    SaveLogisticsRoute, SaveRealityNode, OpenJunction, CloseJunction,
    ScheduleContent, ActivateContent, CompleteContent, AcknowledgeReport
};

/** Typed intent; callbacks are authored resolvers, never inferred from a display label. */
struct OFFLINEGAME_API FOGFoundationStrategicCommand
{
    EOGFoundationStrategicAction Action = EOGFoundationStrategicAction::RefreshProject;
    int64 WorldTick = 0;
    int64 DueWorldTick = 0;
    int64 Seed = 0;
    FOGEntityId SubjectId;
    FOGEntityId TargetId;
    FOGContentId DefinitionId;
    FName State;
    FString PayloadJson = TEXT("{}");
    TArray<FOGProjectResourceCost> ResourceCosts;
    TArray<FOGEntityId> Participants;
    TArray<FOGDispatchObjectiveRecord> DispatchObjectives;
    TArray<FOGDispatchConstraintRecord> DispatchConstraints;
    EOGDispatchType DispatchType = EOGDispatchType::Exploration;
    int32 RiskBps = 0;
    int32 RiskToleranceBps = 0;
    FString AbortPolicyJson = TEXT("{}");
    FOGDispatchResolver DispatchResolver;
    FOGDomainCoreFusionRequest Fusion;
    FOGLargeNumber Damage;
    bool bRuinThresholdReached = false;
    FOGArmyRecord Army;
    TArray<FOGArmyCapabilityRecord> ArmyCapabilities;
    FOGArmyCapabilityResolver ArmyResolver;
    EOGWarStatus WarFinalStatus = EOGWarStatus::Resolved;
    FOGLogisticsRouteRecord Route;
    FOGLogisticsCapabilityValidator TransportValidator;
    FOGRealityNodeRecord Reality;
    /** OpenJunction requires a caller-authored predicate evaluated against canonical state. */
    TFunction<bool(IOGWorldStore&, const FOGJunctionRecord&, FString&)> JunctionRequirements;
    FOGWorldDirectorScheduleRequest Schedule;
};

struct OFFLINEGAME_API FOGFoundationStrategicCommandResult
{
    FOGEntityId EntityId;
    FOGEntityId EventId;
    FOGEntityId ReportId;
    bool bChanged = false;
    FString OutcomeJson = TEXT("{}");
};

struct OFFLINEGAME_API FOGFoundationStrategicProjection
{
    int64 WorldTick = 0;
    bool bHasTerritory = false;
    bool bHasCore = false;
    bool bHasDomain = false;
    FOGTerritoryRecord Territory;
    FOGTerritoryEffectiveControlResult EffectiveControl;
    FOGDomainCoreRecord Core;
    FOGTerritoryDomainStateRecord Domain;
    TArray<FOGResourceBalance> Resources;
    TArray<FOGTerritoryClaimRecord> Claims;
    TArray<FOGProjectRecord> Projects;
    TArray<FOGProjectPhaseRecord> Phases;
    TArray<FOGProjectAssignmentRecord> Assignments;
    TArray<FOGDispatchRecord> Dispatches;
    TArray<FOGDispatchObjectiveRecord> DispatchObjectives;
    TArray<FOGDispatchConstraintRecord> DispatchConstraints;
    TArray<FOGArmyRecord> Armies;
    TArray<FOGArmyCapabilityRecord> ArmyCapabilities;
    bool bHasWar = false;
    FOGWarRecord War;
    TArray<FOGWarFrontRecord> Fronts;
    TArray<FOGWarOrderRecord> Orders;
    TArray<FOGLogisticsRouteRecord> Routes;
    TArray<FOGRealityNodeRecord> Realities;
    TArray<FOGJunctionRecord> Junctions;
    TArray<FOGWorldDirectorScheduleRecord> Schedules;
    TArray<FOGReportRecord> Reports;
    TArray<FOGReportDeliveryRecord> Deliveries;
    TArray<FOGWorldEvent> Events;
};

struct OFFLINEGAME_API FOGFoundationStrategicMenuEntry
{
    EOGFoundationStrategicAction Action;
    FString Label;
    bool bEnabled = false;
    FString DisabledReason;
};

/**
 * Native command/projection adapter shared by Ruler HUD and World interaction.
 * Contains no balances, simulation clock or authoritative runtime cache.
 * Callers supply the canonical tick from UOGGameCoreSubsystem and authored resolvers.
 */
class OFFLINEGAME_API FOGFoundationStrategicRuntime
{
public:
    explicit FOGFoundationStrategicRuntime(IOGWorldStore& InStore) : Store(InStore) {}

    bool Execute(FOGFoundationStrategicContext& Context,
        const FOGFoundationStrategicCommand& Command,
        FOGFoundationStrategicCommandResult& OutResult, FString& OutError);
    bool BuildProjection(const FOGFoundationStrategicContext& Context, int64 WorldTick,
        FOGFoundationStrategicProjection& OutProjection, FString& OutError) const;
    static TArray<FOGFoundationStrategicMenuEntry> GetMenuEntries(
        const FOGFoundationStrategicContext& Context);

    /** Idempotent, opt-in definitions in an isolated Foundation diagnostic namespace. */
    bool EnsureDiagnosticContext(const FOGEntityId& SelectedDiagnosticRulerId, int64 WorldTick,
        FOGFoundationStrategicContext& OutContext, FString& OutError);
    /** Explicit native menu action; never grants production resources to a player. */
    bool ExecuteDiagnosticAction(FOGFoundationStrategicContext& Context,
        EOGFoundationStrategicAction Action, int64 WorldTick, int64 DiagnosticDurationTicks,
        FOGFoundationStrategicCommandResult& OutResult, FString& OutError);

    /** Clock dispatcher callback. Schedule provenance binds a real existing entity ID. */
    static bool ResolveDueAction(IOGWorldStore& Store,
        const FOGWorldDirectorScheduleRecord& Schedule, int64 ObservedWorldTick,
        bool bOffline, FString& OutOutcomeJson, FString& OutError);
    static FName DueResolverKey();
    void RegisterClockResolvers(FOGCanonicalClockRuntime& Clock);
    bool BindScheduledActions(const FOGFoundationStrategicContext& Context,
        FOGCanonicalClockRuntime& Clock, FString& OutError);
    /** Create a bounded Director schedule for a real Project/Dispatch; no second queue. */
    bool ScheduleDueAction(FOGFoundationStrategicContext& Context,
        const FOGEntityId& SubjectId, FName Kind, int64 WorldTick, int64 DueWorldTick,
        FOGWorldDirectorScheduleRecord& OutSchedule, FString& OutError);

private:
    IOGWorldStore& Store;
};
