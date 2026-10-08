#include "Runtime/OGFoundationStrategicRuntime.h"

#include "Dom/JsonObject.h"
#include "Runtime/OGReportService.h"
#include "Runtime/OGCanonicalClockRuntime.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "World/OGDomainCoreService.h"
#include "World/OGFactionWarService.h"
#include "World/OGProjectService.h"
#include "World/OGRealityGraphService.h"
#include "World/OGTerritoryControlService.h"
#include "World/OGWorldTimeService.h"

namespace
{
constexpr uint32 DiagnosticPrefix = 0xF040D1A6;
FOGEntityId DiagnosticId(uint32 Ordinal)
{
    return FOGEntityId(FGuid(DiagnosticPrefix, 0xEF041001, 0xD1A69051, Ordinal));
}
FOGEntityId DiagnosticOwner()
{
    // Same canonical scoped Ruler used by OGDiagnosticGacha::ScopedId(1).
    return FOGEntityId(FGuid(0x4f474449, 0x41474e4f, 0x53544943, 1));
}
FOGContentId DiagnosticContent(const TCHAR* Key)
{
    return FOGContentId(FString(TEXT("foundation_diagnostic:")) + Key);
}
bool IsDiagnosticId(const FOGEntityId& Id)
{
    return Id.Value.A == DiagnosticPrefix && Id.Value.B == 0xEF041001 && Id.Value.C == 0xD1A69051;
}
FName ActionKey(EOGFoundationStrategicAction Action)
{
    switch (Action)
    {
#define OG_ACTION(Name, Key) case EOGFoundationStrategicAction::Name: return FName(TEXT("foundation.strategy." Key));
        OG_ACTION(StartProject, "project_started") OG_ACTION(RefreshProject, "project_refreshed")
        OG_ACTION(StartDispatch, "dispatch_started") OG_ACTION(ResolveDispatch, "dispatch_resolved")
        OG_ACTION(DisplaceClaim, "claim_displaced") OG_ACTION(ReclaimClaim, "claim_reclaimed")
        OG_ACTION(ExpireClaim, "claim_expired") OG_ACTION(ActivateCore, "core_activated")
        OG_ACTION(DamageCore, "core_damaged") OG_ACTION(CaptureCore, "core_captured")
        OG_ACTION(FuseCores, "cores_fused") OG_ACTION(RefreshHeart, "heart_refreshed")
        OG_ACTION(SaveArmy, "army_saved") OG_ACTION(ResolveArmyInteraction, "army_resolved")
        OG_ACTION(DeclareWar, "war_declared") OG_ACTION(CreateWarFront, "front_created")
        OG_ACTION(IssueWarOrder, "order_issued") OG_ACTION(RecordWarOrderOutcome, "order_outcome")
        OG_ACTION(ResolveWar, "war_resolved") OG_ACTION(SaveLogisticsRoute, "route_saved")
        OG_ACTION(SaveRealityNode, "reality_saved") OG_ACTION(OpenJunction, "junction_opened")
        OG_ACTION(CloseJunction, "junction_closed") OG_ACTION(ScheduleContent, "content_scheduled")
        OG_ACTION(ActivateContent, "content_activated") OG_ACTION(CompleteContent, "content_completed")
        OG_ACTION(AcknowledgeReport, "report_acknowledged")
#undef OG_ACTION
    }
    return NAME_None;
}
FString Serialize(const TSharedRef<FJsonObject>& Object)
{
    FString Json;
    FJsonSerializer::Serialize(Object, TJsonWriterFactory<>::Create(&Json));
    return Json;
}
bool Parse(const FString& Json, TSharedPtr<FJsonObject>& Out, FString& Error)
{
    if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Out) || !Out.IsValid())
    { Error = TEXT("Strategic payload must be a JSON object."); return false; }
    return true;
}
/** Scope delegates nested transactions to the canonical SQLite savepoint implementation. */
class FCommandTransaction
{
public:
    explicit FCommandTransaction(IOGWorldStore& InStore) : Store(InStore) {}
    ~FCommandTransaction() { if (bOpen) { FString Error; Store.RollbackTransaction(Error); } }
    bool Begin(FString& Error) { bOpen = Store.BeginTransaction(Error); return bOpen; }
    bool Commit(FString& Error)
    { if (!Store.CommitTransaction(Error)) return false; bOpen = false; return true; }
private:
    IOGWorldStore& Store;
    bool bOpen = false;
};
bool AddEventAndReport(IOGWorldStore& Store, const FOGEntityId& Owner,
    FName Type, const FOGEntityId& Subject, int64 Tick, const FString& Payload,
    FOGFoundationStrategicCommandResult& Result, FString& Error)
{
    FOGWorldEvent Event;
    Event.EventId = FOGEntityId::NewId();
    Event.EventType = Type;
    Event.PrimaryEntity = Owner;
    Event.RelatedEntities.Add(Subject);
    Event.WorldTick = Tick;
    Event.PayloadJson = Payload;
    Event.bChronicleEligible = true;
    if (!Store.AppendWorldEvent(Event, Error)) return false;
    FOGEntityId ReportId;
    if (!FOGReportService(Store).CreateReport(Owner, Event.EventId,
        FName(TEXT("strategy")), 1, Tick, Payload, ReportId, Error)) return false;
    Result.EventId = Event.EventId;
    Result.ReportId = ReportId;
    return true;
}
bool Fail(FString& Error, const TCHAR* Reason) { Error = Reason; return false; }
bool CheckOwnedProject(IOGWorldStore& Store, const FOGEntityId& Owner,
    const FOGEntityId& Id, FOGProjectRecord& Record, FString& Error)
{
    bool bFound = false;
    if (!Store.TryReadProject(Id, bFound, Record, Error)) return false;
    return bFound && Record.OwnerEntityId == Owner ? true : Fail(Error, TEXT("Select an existing Project owned by this context."));
}
bool CheckOwnedDispatch(IOGWorldStore& Store, const FOGEntityId& Owner,
    const FOGEntityId& Id, FOGDispatchRecord& Record, FString& Error)
{
    bool bFound = false;
    if (!Store.TryReadDispatch(Id, bFound, Record, Error)) return false;
    return bFound && Record.OwnerEntityId == Owner ? true : Fail(Error, TEXT("Select an existing Dispatch owned by this context."));
}
bool CheckControlledTerritory(IOGWorldStore& Store, const FOGFoundationStrategicContext& Context,
    int64 Tick, FString& Error)
{
    FOGTerritoryEffectiveControlResult Control;
    if (!FOGTerritoryControlService(Store).EvaluateEffectiveControl(Context.TerritoryId, Tick, Control, Error)) return false;
    return Control.EffectiveRulerIds.Contains(Context.OwnerId) ? true : Fail(Error, TEXT("This context does not effectively control the selected Territory."));
}
bool CheckFaction(IOGWorldStore& Store, const FOGFoundationStrategicContext& Context, FString& Error)
{
    bool bFound = false;
    FOGFactionRecord Faction;
    if (!Store.TryReadFaction(Context.FactionId, bFound, Faction, Error)) return false;
    return bFound && Faction.LeaderRulerId == Context.OwnerId ? true : Fail(Error, TEXT("This context does not lead the selected Faction."));
}
bool CheckWar(IOGWorldStore& Store, const FOGFoundationStrategicContext& Context, FString& Error)
{
    if (!CheckFaction(Store, Context, Error)) return false;
    bool bFound = false;
    FOGWarRecord War;
    if (!Store.TryReadWar(Context.WarId, bFound, War, Error)) return false;
    if (bFound && War.Status == EOGWarStatus::Active)
        for (const auto& Participant : War.Participants)
            if (Participant.FactionId == Context.FactionId) return true;
    return Fail(Error, TEXT("The selected Faction is not participating in an active War."));
}
bool CheckDispatchParticipants(IOGWorldStore& Store, const FOGFoundationStrategicContext& Context,
    const TArray<FOGEntityId>& Participants, FString& Error)
{
    if (Participants.IsEmpty()) return Fail(Error, TEXT("A Dispatch must select actual available participants."));
    for (const auto& Id : Participants)
    {
        bool bFound = false; FOGCharacterManifestationRecord Manifestation;
        if (!Store.TryReadCharacterManifestation(Id, bFound, Manifestation, Error)) return false;
        if (bFound)
        {
            if (Manifestation.OwningRulerId != Context.OwnerId)
                return Fail(Error, TEXT("Dispatch participant Manifestation belongs to another owner."));
            continue;
        }
        FOGArmyRecord Army;
        if (!Store.TryReadArmy(Id, bFound, Army, Error)) return false;
        if (bFound)
        {
            FOGFactionRecord Faction; bool bFaction = false;
            if (!Store.TryReadFaction(Army.FactionId, bFaction, Faction, Error)) return false;
            if (!bFaction || Faction.LeaderRulerId != Context.OwnerId || Army.Headcount <= 0 || Army.State != FName(TEXT("ready")))
                return Fail(Error, TEXT("Dispatch participant Army is unavailable or belongs to another owner."));
            continue;
        }
        return Fail(Error, TEXT("Unsupported Dispatch participant: select an owned Manifestation or ready Army."));
    }
    return true;
}
void TrackResult(FOGFoundationStrategicContext& Context, EOGFoundationStrategicAction Action, const FOGEntityId& Id)
{
    switch (Action)
    {
    case EOGFoundationStrategicAction::StartProject: Context.ProjectIds.AddUnique(Id); break;
    case EOGFoundationStrategicAction::StartDispatch: Context.DispatchIds.AddUnique(Id); break;
    case EOGFoundationStrategicAction::DeclareWar: Context.WarId = Id; break;
    case EOGFoundationStrategicAction::CreateWarFront: Context.FrontId = Id; break;
    case EOGFoundationStrategicAction::IssueWarOrder: Context.OrderId = Id; break;
    case EOGFoundationStrategicAction::ScheduleContent: Context.ScheduleIds.AddUnique(Id); break;
    default: break;
    }
}
}

bool FOGFoundationStrategicRuntime::Execute(FOGFoundationStrategicContext& Context,
    const FOGFoundationStrategicCommand& C, FOGFoundationStrategicCommandResult& OutResult, FString& Error)
{
    OutResult = FOGFoundationStrategicCommandResult();
    Error.Reset();
    if (!Store.IsOpen() || !Context.OwnerId.IsValid() || C.WorldTick < 0)
        return Fail(Error, TEXT("A ready canonical store, owner context and canonical tick are required."));
    if (Context.bDiagnostic && Context.OwnerId != DiagnosticOwner())
        return Fail(Error, TEXT("Diagnostic triggers can only operate on the isolated diagnostic owner."));
    TSharedPtr<FJsonObject> Payload;
    if (!Parse(C.PayloadJson, Payload, Error)) return false;
    FCommandTransaction Transaction(Store);
    if (!Transaction.Begin(Error)) return false;
    FOGFoundationStrategicCommandResult Result;
    Result.EntityId = C.SubjectId;
    Result.bChanged = true;
    Result.OutcomeJson = C.PayloadJson;
    bool bOk = false;
    const auto Action = C.Action;
    switch (Action)
    {
    case EOGFoundationStrategicAction::StartProject:
        if (!CheckControlledTerritory(Store, Context, C.WorldTick, Error)) return false;
        {
            TArray<FOGTerritoryClaimRecord> Claims;
            if (!Store.ListActiveClaimsForLocation(Context.LocationId, Claims, Error)) return false;
            bool bOwnedLocation = false;
            for (const auto& Claim : Claims)
                if (Claim.TerritoryId == Context.TerritoryId && Claim.RulerId == Context.OwnerId) bOwnedLocation = true;
            if (!bOwnedLocation) return Fail(Error, TEXT("The Project Location is not in the selected controlled Territory."));
        }
        bOk = FOGProjectService(Store).StartProject(Context.OwnerId, Context.LocationId,
            C.DefinitionId, C.ResourceCosts, C.WorldTick, C.DueWorldTick, C.PayloadJson, Result.EntityId, Error);
        break;
    case EOGFoundationStrategicAction::RefreshProject:
    {
        FOGProjectRecord Before, After;
        if (!CheckOwnedProject(Store, Context.OwnerId, C.SubjectId, Before, Error)) return false;
        bOk = FOGProjectService(Store).RefreshProject(C.SubjectId, C.WorldTick, After, Error);
        Result.bChanged = Before.Status != After.Status || Before.ProgressBps != After.ProgressBps;
        break;
    }
    case EOGFoundationStrategicAction::StartDispatch:
        if (!CheckDispatchParticipants(Store, Context, C.Participants, Error)) return false;
        if (C.TargetId.IsValid())
        {
            bool bTarget = false; FName Kind; FString Json; int64 Revision = 0;
            if (!Store.TryReadEntity(C.TargetId, bTarget, Kind, Json, Revision, Error)) return false;
            if (!bTarget) return Fail(Error, TEXT("Dispatch target must be an existing canonical entity."));
        }
        bOk = FOGDispatchService(Store).StartDispatch(Context.OwnerId, C.TargetId,
            C.DispatchType, C.Participants, C.WorldTick, C.DueWorldTick, C.RiskBps,
            C.RiskToleranceBps, C.AbortPolicyJson, C.DispatchObjectives, C.DispatchConstraints,
            C.Seed, Result.EntityId, Error);
        break;
    case EOGFoundationStrategicAction::ResolveDispatch:
    {
        FOGDispatchRecord Before, After;
        if (!CheckOwnedDispatch(Store, Context.OwnerId, C.SubjectId, Before, Error)) return false;
        if (Before.Status != EOGDispatchStatus::Active) { bOk = true; Result.bChanged = false; break; }
        if (!C.DispatchResolver) return Fail(Error, TEXT("Dispatch resolution requires an authored resolver; the UI cannot declare success."));
        bOk = FOGDispatchService(Store).ResolveDispatchWithPolicy(C.SubjectId, C.WorldTick, C.DispatchResolver, After, Error);
        Result.OutcomeJson = After.ResultJson;
        break;
    }
    case EOGFoundationStrategicAction::DisplaceClaim:
    case EOGFoundationStrategicAction::ReclaimClaim:
    case EOGFoundationStrategicAction::ExpireClaim:
    {
        bool bFound = false;
        FOGTerritoryClaimRecord Claim;
        if (!Store.TryReadTerritoryClaim(Context.ClaimId, bFound, Claim, Error)) return false;
        if (!bFound || Claim.RulerId != Context.OwnerId) return Fail(Error, TEXT("Select an owned canonical Territory claim."));
        Result.EntityId = Context.ClaimId;
        FOGTerritoryControlService Service(Store);
        if (Action == EOGFoundationStrategicAction::DisplaceClaim)
        {
            if (!Context.bDiagnostic) return Fail(Error, TEXT("Displacement requires a World resolution; this direct trigger is diagnostic only."));
            bOk = Service.BeginDisplacement(Context.ClaimId, C.WorldTick, C.DueWorldTick, Error);
        }
        else if (Action == EOGFoundationStrategicAction::ReclaimClaim) bOk = Service.ReclaimTerritory(Context.ClaimId, C.WorldTick, Error);
        else bOk = Service.ExpireReclamationIfDue(Context.ClaimId, C.WorldTick, Result.bChanged, Error);
        break;
    }
    case EOGFoundationStrategicAction::ActivateCore:
    case EOGFoundationStrategicAction::DamageCore:
    case EOGFoundationStrategicAction::CaptureCore:
    {
        bool bFound = false;
        FOGDomainCoreRecord Core;
        if (!Store.TryReadDomainCore(Context.CoreId, bFound, Core, Error)) return false;
        if (!bFound || Core.ControllerRulerId != Context.OwnerId) return Fail(Error, TEXT("Select a Core controlled by this context."));
        Result.EntityId = Context.CoreId;
        FOGDomainCoreService Service(Store);
        if (Action == EOGFoundationStrategicAction::ActivateCore) bOk = Service.ActivateAwakenedCoreAsHeart(Context.CoreId, C.WorldTick, Error);
        else if (Action == EOGFoundationStrategicAction::DamageCore)
        {
            if (!Context.bDiagnostic) return Fail(Error, TEXT("Direct Core damage is a diagnostic trigger; production damage requires World resolution."));
            bOk = Service.ApplyCoreDurabilityDamage(Context.CoreId, C.Damage, C.WorldTick, Error);
        }
        else
        {
            if (!Context.bDiagnostic) return Fail(Error, TEXT("Core capture requires an authored resolved conquest, which is not supplied by this command."));
            bOk = Service.CaptureIntactCore(Context.CoreId, C.TargetId, C.WorldTick, Error);
        }
        break;
    }
    case EOGFoundationStrategicAction::FuseCores:
    {
        FOGDomainCoreRecord A, B; bool bA = false, bB = false;
        if (!Store.TryReadDomainCore(C.Fusion.AbsorberCoreId, bA, A, Error) ||
            !Store.TryReadDomainCore(C.Fusion.AbsorbedCoreId, bB, B, Error)) return false;
        if (!bA || !bB || A.ControllerRulerId != Context.OwnerId || B.ControllerRulerId != Context.OwnerId)
            return Fail(Error, TEXT("Both fusion sources must be owned and governed by an authored synthesis rule."));
        FOGDomainCoreFusionRecord Fusion;
        bOk = FOGDomainCoreService(Store).FuseCores(C.Fusion, C.WorldTick, Fusion, Error);
        Result.EntityId = Fusion.ResultCoreId;
        break;
    }
    case EOGFoundationStrategicAction::RefreshHeart:
    {
        if (!Context.bDiagnostic && C.bRuinThresholdReached)
            return Fail(Error, TEXT("Ruin threshold requires an authored calendar evaluation."));
        FOGTerritoryDomainStateRecord State;
        bOk = FOGDomainCoreService(Store).RefreshDomainHeartConsequences(Context.TerritoryId, C.WorldTick, C.bRuinThresholdReached, State, Error);
        Result.EntityId = Context.TerritoryId;
        break;
    }
    case EOGFoundationStrategicAction::SaveArmy:
        if (!CheckFaction(Store, Context, Error)) return false;
        if (C.Army.FactionId != Context.FactionId || C.Army.Headcount < 0 || C.ArmyCapabilities.IsEmpty())
            return Fail(Error, TEXT("Army requires the selected Faction and normalized capabilities."));
        {
            bool bFound = false; FOGArmyRecord Existing;
            if (!Store.TryReadArmy(C.Army.ArmyId, bFound, Existing, Error)) return false;
            if (bFound && Existing.FactionId != Context.FactionId)
                return Fail(Error, TEXT("An existing Army owned by another Faction cannot be overwritten."));
        }
        Result.EntityId = C.Army.ArmyId;
        if (!Store.UpsertArmy(C.Army, C.WorldTick, Error)) return false;
        for (auto Capability : C.ArmyCapabilities)
        {
            Capability.ArmyId = C.Army.ArmyId;
            if (!FOGStrategicResolutionService(Store).SetArmyCapability(Capability, Error)) return false;
        }
        bOk = true;
        break;
    case EOGFoundationStrategicAction::ResolveArmyInteraction:
    {
        if (!CheckFaction(Store, Context, Error)) return false;
        if (!C.ArmyResolver) return Fail(Error, TEXT("Army interaction requires a capability-based authored resolver."));
        FOGStrategicArmyResolutionResult ArmyResult;
        bOk = FOGStrategicResolutionService(Store).ResolveArmyInteraction(C.Participants, C.Seed, C.ArmyResolver, ArmyResult, Error);
        Result.EntityId = Context.ArmyId;
        Result.OutcomeJson = ArmyResult.OutcomeJson;
        break;
    }
    case EOGFoundationStrategicAction::DeclareWar:
        if (!CheckFaction(Store, Context, Error)) return false;
        bOk = FOGFactionWarService(Store).DeclareWar(Context.FactionId, C.TargetId, C.State,
            Context.TerritoryId, C.WorldTick, Result.EntityId, Error);
        break;
    case EOGFoundationStrategicAction::CreateWarFront:
        if (!CheckWar(Store, Context, Error)) return false;
        bOk = FOGFactionWarService(Store).CreateWarFront(Context.WarId, Context.LocationId,
            Context.RealityId, C.WorldTick, C.PayloadJson, Result.EntityId, Error);
        break;
    case EOGFoundationStrategicAction::IssueWarOrder:
        if (!CheckWar(Store, Context, Error)) return false;
        bOk = FOGFactionWarService(Store).IssueWarOrder(Context.WarId, Context.FrontId,
            Context.OwnerId, C.TargetId, C.DefinitionId, C.PayloadJson, C.WorldTick, Result.EntityId, Error);
        break;
    case EOGFoundationStrategicAction::RecordWarOrderOutcome:
        if (!Context.bDiagnostic) return Fail(Error, TEXT("Order outcomes require an authored Army resolution; direct outcome trigger is diagnostic only."));
        if (!CheckWar(Store, Context, Error)) return false;
        Result.EntityId = Context.OrderId;
        bOk = FOGFactionWarService(Store).RecordWarOrderOutcome(Context.OrderId, C.State, C.PayloadJson, C.WorldTick, Error);
        break;
    case EOGFoundationStrategicAction::ResolveWar:
        if (!Context.bDiagnostic) return Fail(Error, TEXT("A War cannot be resolved from a menu without an actual strategic outcome."));
        if (!CheckWar(Store, Context, Error)) return false;
        Result.EntityId = Context.WarId;
        bOk = FOGFactionWarService(Store).ResolveWar(Context.WarId, C.WarFinalStatus, C.WorldTick, C.PayloadJson, Error);
        break;
    case EOGFoundationStrategicAction::SaveLogisticsRoute:
        if (C.Route.OwnerEntityId != Context.OwnerId) return Fail(Error, TEXT("Logistics owner must match the selected context."));
        {
            bool bFound = false; FOGLogisticsRouteRecord Existing;
            if (!Store.TryReadLogisticsRoute(C.Route.RouteId, bFound, Existing, Error)) return false;
            if (bFound && Existing.OwnerEntityId != Context.OwnerId)
                return Fail(Error, TEXT("An existing Logistics route owned by another Ruler cannot be overwritten."));
        }
        Result.EntityId = C.Route.RouteId;
        bOk = FOGCivilizationLogisticsService(Store).SaveLogisticsRoute(C.Route, C.WorldTick, C.TransportValidator, Error);
        break;
    case EOGFoundationStrategicAction::SaveRealityNode:
        if (!Context.bDiagnostic) return Fail(Error, TEXT("Reality authoring requires a content/system command, not an ordinary Ruler menu."));
        if (!IsDiagnosticId(C.Reality.RealityId)) return Fail(Error, TEXT("Diagnostic Reality authoring must remain in its isolated namespace."));
        Result.EntityId = C.Reality.RealityId;
        bOk = FOGRealityGraphService(Store).SaveRealityNode(C.Reality, C.WorldTick, Error);
        break;
    case EOGFoundationStrategicAction::OpenJunction:
    {
        if (!C.JunctionRequirements) return Fail(Error, TEXT("Junction opening requires canonical, authored requirement evaluation."));
        bool bFound = false; FOGJunctionRecord Junction;
        if (!Store.TryReadJunction(Context.JunctionId, bFound, Junction, Error)) return false;
        if (!bFound || !C.JunctionRequirements(Store, Junction, Error))
        { if (Error.IsEmpty()) Error = TEXT("Junction requirements are not satisfied."); return false; }
        Result.EntityId = Context.JunctionId;
        bOk = FOGRealityGraphService(Store).OpenJunction(Context.JunctionId, C.WorldTick, true, Error);
        break;
    }
    case EOGFoundationStrategicAction::CloseJunction:
        if (!Context.bDiagnostic) return Fail(Error, TEXT("Junction closure requires an authored permission/resolution command."));
        Result.EntityId = Context.JunctionId;
        bOk = FOGRealityGraphService(Store).CloseJunction(Context.JunctionId, C.WorldTick, C.State, Error);
        break;
    case EOGFoundationStrategicAction::ScheduleContent:
    {
        FOGWorldDirectorScheduleRecord Schedule;
        bOk = FOGWorldDirectorService(Store).ScheduleEligibleContent(C.Schedule, Schedule, Error);
        Result.EntityId = Schedule.ScheduleId;
        break;
    }
    case EOGFoundationStrategicAction::ActivateContent:
        bOk = FOGWorldDirectorService(Store).ActivateDueContent(C.SubjectId, C.WorldTick, Error);
        break;
    case EOGFoundationStrategicAction::CompleteContent:
        if (!Context.bDiagnostic) return Fail(Error, TEXT("Content completion requires its authored resolver."));
        bOk = FOGWorldDirectorService(Store).CompleteSchedule(C.SubjectId, C.WorldTick, C.PayloadJson, Error);
        break;
    case EOGFoundationStrategicAction::AcknowledgeReport:
    {
        FOGReportRecord Report; bool bFound = false;
        if (!Store.TryReadReport(C.SubjectId, bFound, Report, Error)) return false;
        if (!bFound || Report.OwnerEntityId != Context.OwnerId) return Fail(Error, TEXT("Select a Report owned by this context."));
        Result.bChanged = !Report.bAcknowledged;
        bOk = !Result.bChanged || FOGReportService(Store).AcknowledgeReport(C.SubjectId, C.WorldTick, Error);
        break;
    }
    default: return Fail(Error, TEXT("Unsupported strategic action."));
    }
    if (!bOk) { if (Error.IsEmpty()) Error = TEXT("The canonical service rejected this action."); return false; }
    TSharedPtr<FJsonObject> Outcome;
    if (!Parse(Result.OutcomeJson, Outcome, Error)) return false;
    if (Result.bChanged)
    {
        auto EventPayload = MakeShared<FJsonObject>();
        EventPayload->SetStringField(TEXT("action"), ActionKey(Action).ToString());
        EventPayload->SetStringField(TEXT("subject_id"), Result.EntityId.ToString());
        EventPayload->SetBoolField(TEXT("diagnostic"), Context.bDiagnostic);
        EventPayload->SetObjectField(TEXT("outcome"), Outcome);
        if (!AddEventAndReport(Store, Context.OwnerId, ActionKey(Action), Result.EntityId,
            C.WorldTick, Serialize(EventPayload), Result, Error)) return false;
    }
    if (!Transaction.Commit(Error)) return false;
    TrackResult(Context, Action, Result.EntityId);
    OutResult = MoveTemp(Result);
    return true;
}

bool FOGFoundationStrategicRuntime::BuildProjection(const FOGFoundationStrategicContext& C,
    int64 Tick, FOGFoundationStrategicProjection& Out, FString& Error) const
{
    Out = FOGFoundationStrategicProjection();
    Error.Reset();
    if (!Store.IsOpen() || !C.OwnerId.IsValid() || Tick < 0)
        return Fail(Error, TEXT("Projection requires a ready canonical context and world tick."));
    FOGFoundationStrategicProjection P;
    P.WorldTick = Tick;
    if (C.TerritoryId.IsValid())
    {
        if (!Store.TryReadTerritory(C.TerritoryId, P.bHasTerritory, P.Territory, Error) ||
            !Store.ListTerritoryClaimsByTerritory(C.TerritoryId, P.Claims, Error) ||
            !FOGTerritoryControlService(Store).EvaluateEffectiveControl(C.TerritoryId, Tick, P.EffectiveControl, Error) ||
            !Store.TryReadTerritoryDomainState(C.TerritoryId, P.bHasDomain, P.Domain, Error)) return false;
    }
    if (C.CoreId.IsValid() && !Store.TryReadDomainCore(C.CoreId, P.bHasCore, P.Core, Error)) return false;
    for (const auto& ResourceId : C.ResourceIds)
    {
        FOGResourceBalance Balance; bool bKnown = false;
        Balance.OwnerEntityId = C.OwnerId; Balance.ResourceId = ResourceId;
        if (!Store.TryReadResourceBalance(C.OwnerId, ResourceId, bKnown, Balance.Amount, Error)) return false;
        if (bKnown) P.Resources.Add(Balance);
    }
    if (!Store.ListWorldEvents(C.OwnerId, NAME_None, false, 256, P.Events, Error)) return false;
    auto ProjectIds = C.ProjectIds; auto DispatchIds = C.DispatchIds; auto ScheduleIds = C.ScheduleIds;
    // Reconstruct selection handles from canonical command events after process restart.
    // They are identifiers of the existing records, never copied authoritative state.
    for (const auto& Event : P.Events)
    {
        if (Event.RelatedEntities.IsEmpty()) continue;
        const auto& Id = Event.RelatedEntities[0];
        if (Event.EventType == ActionKey(EOGFoundationStrategicAction::StartProject)) ProjectIds.AddUnique(Id);
        if (Event.EventType == ActionKey(EOGFoundationStrategicAction::StartDispatch)) DispatchIds.AddUnique(Id);
        if (Event.EventType == ActionKey(EOGFoundationStrategicAction::ScheduleContent)) ScheduleIds.AddUnique(Id);
    }
    for (const auto& Id : ProjectIds)
    {
        bool bFound = false; FOGProjectRecord Record;
        if (!Store.TryReadProject(Id, bFound, Record, Error)) return false;
        if (!bFound || Record.OwnerEntityId != C.OwnerId) continue;
        P.Projects.Add(Record);
        TArray<FOGProjectPhaseRecord> Phases; TArray<FOGProjectAssignmentRecord> Assignments;
        if (!Store.ListProjectPhases(Id, Phases, Error) || !Store.ListProjectAssignments(Id, Assignments, Error)) return false;
        P.Phases.Append(Phases); P.Assignments.Append(Assignments);
    }
    for (const auto& Id : DispatchIds)
    {
        bool bFound = false; FOGDispatchRecord Record;
        if (!Store.TryReadDispatch(Id, bFound, Record, Error)) return false;
        if (!bFound || Record.OwnerEntityId != C.OwnerId) continue;
        P.Dispatches.Add(Record);
        TArray<FOGDispatchObjectiveRecord> Objectives; TArray<FOGDispatchConstraintRecord> Constraints;
        if (!Store.ListDispatchObjectives(Id, Objectives, Error) || !Store.ListDispatchConstraints(Id, Constraints, Error)) return false;
        P.DispatchObjectives.Append(Objectives); P.DispatchConstraints.Append(Constraints);
    }
    for (const auto& Id : { C.ArmyId, C.OtherArmyId })
    {
        if (!Id.IsValid()) continue;
        bool bFound = false; FOGArmyRecord Army;
        if (!Store.TryReadArmy(Id, bFound, Army, Error)) return false;
        if (!bFound) continue;
        P.Armies.Add(Army);
        TArray<FOGArmyCapabilityRecord> Capabilities;
        if (!Store.ListArmyCapabilities(Id, Capabilities, Error)) return false;
        P.ArmyCapabilities.Append(Capabilities);
    }
    if (C.WarId.IsValid())
    {
        if (!Store.TryReadWar(C.WarId, P.bHasWar, P.War, Error) ||
            !Store.ListWarFronts(C.WarId, P.Fronts, Error) || !Store.ListWarOrders(C.WarId, P.Orders, Error)) return false;
    }
    if (!Store.ListLogisticsRoutesByOwner(C.OwnerId, P.Routes, Error)) return false;
    for (const auto& Id : { C.RealityId, C.DestinationRealityId })
    {
        if (!Id.IsValid()) continue;
        bool bFound = false; FOGRealityNodeRecord Reality;
        if (!Store.TryReadRealityNode(Id, bFound, Reality, Error)) return false;
        if (bFound) P.Realities.Add(Reality);
    }
    if (C.RealityId.IsValid() && !Store.ListJunctionsFromReality(C.RealityId, P.Junctions, Error)) return false;
    for (const auto& Id : ScheduleIds)
    {
        bool bFound = false; FOGWorldDirectorScheduleRecord Schedule;
        if (!Store.TryReadWorldDirectorSchedule(Id, bFound, Schedule, Error)) return false;
        if (bFound) P.Schedules.Add(Schedule);
    }
    if (!Store.ListReportsByOwner(C.OwnerId, P.Reports, Error)) return false;
    for (const auto& Report : P.Reports)
    {
        TArray<FOGReportDeliveryRecord> Deliveries;
        if (!Store.ListReportDeliveries(Report.ReportId, Deliveries, Error)) return false;
        P.Deliveries.Append(Deliveries);
    }
    Out = MoveTemp(P);
    return true;
}

TArray<FOGFoundationStrategicMenuEntry> FOGFoundationStrategicRuntime::GetMenuEntries(
    const FOGFoundationStrategicContext& C)
{
    TArray<FOGFoundationStrategicMenuEntry> Entries;
    auto Add = [&Entries](EOGFoundationStrategicAction Action, const TCHAR* Label, bool bReady, const TCHAR* Reason)
    { Entries.Add({ Action, Label, bReady, bReady ? FString() : FString(Reason) }); };
    Add(EOGFoundationStrategicAction::StartProject, TEXT("Start construction Project"), C.OwnerId.IsValid() && C.LocationId.IsValid(), TEXT("Select owner and Location; an authored Project definition is required."));
    Add(EOGFoundationStrategicAction::StartDispatch, TEXT("Dispatch exploration party"), C.OwnerId.IsValid(), TEXT("Select an owner and actual party participants."));
    Add(EOGFoundationStrategicAction::RefreshProject, TEXT("Refresh Project progress"), !C.ProjectIds.IsEmpty(), TEXT("No selected canonical Project."));
    Add(EOGFoundationStrategicAction::ResolveDispatch, TEXT("Resolve due Dispatch"), C.bDiagnostic && !C.DispatchIds.IsEmpty(), TEXT("Resolution requires an authored mission resolver."));
    Add(EOGFoundationStrategicAction::ActivateCore, TEXT("Activate awakened Domain Core"), C.CoreId.IsValid(), TEXT("Select an awakened controlled Core."));
    Add(EOGFoundationStrategicAction::ReclaimClaim, TEXT("Reclaim displaced Territory"), C.ClaimId.IsValid(), TEXT("Select a displaced owned claim."));
    Add(EOGFoundationStrategicAction::DeclareWar, TEXT("Declare War"), C.FactionId.IsValid() && C.OtherFactionId.IsValid(), TEXT("Select distinct existing Factions."));
    Add(EOGFoundationStrategicAction::CreateWarFront, TEXT("Create War front"), C.WarId.IsValid(), TEXT("No selected active War."));
    Add(EOGFoundationStrategicAction::IssueWarOrder, TEXT("Issue Army order"), C.FrontId.IsValid() && C.ArmyId.IsValid(), TEXT("Select a War front and recipient Army."));
    Add(EOGFoundationStrategicAction::SaveLogisticsRoute, TEXT("Establish Logistics route"), C.bDiagnostic && C.JunctionId.IsValid(), TEXT("An owned transport capability and authored validator are required."));
    Add(EOGFoundationStrategicAction::OpenJunction, TEXT("Open Junction"), C.bDiagnostic && C.JunctionId.IsValid(), TEXT("An authored requirements evaluator is required."));
    Add(EOGFoundationStrategicAction::CloseJunction, TEXT("Close Junction"), C.bDiagnostic && C.JunctionId.IsValid(), TEXT("An authored closure permission/resolution is required."));
    if (C.bDiagnostic)
    {
        Add(EOGFoundationStrategicAction::DisplaceClaim, TEXT("Diagnostic: displace claim"), true, TEXT(""));
        Add(EOGFoundationStrategicAction::ExpireClaim, TEXT("Diagnostic: evaluate reclaim deadline"), true, TEXT(""));
        Add(EOGFoundationStrategicAction::DamageCore, TEXT("Diagnostic: damage Domain Core"), true, TEXT(""));
        Add(EOGFoundationStrategicAction::RefreshHeart, TEXT("Diagnostic: refresh Domain consequences"), true, TEXT(""));
        Add(EOGFoundationStrategicAction::ResolveArmyInteraction, TEXT("Diagnostic: capability resolution"), true, TEXT(""));
        Add(EOGFoundationStrategicAction::RecordWarOrderOutcome, TEXT("Diagnostic: record resolved Army order"), C.OrderId.IsValid(), TEXT("Issue an order first."));
    }
    return Entries;
}

bool FOGFoundationStrategicRuntime::EnsureDiagnosticContext(const FOGEntityId& SelectedRuler, int64 Tick,
    FOGFoundationStrategicContext& Out, FString& Error)
{
    Out = FOGFoundationStrategicContext(); Error.Reset();
    if (!Store.IsOpen() || Tick < 0) return Fail(Error, TEXT("Diagnostics require an open canonical store and canonical tick."));
    if (SelectedRuler != DiagnosticOwner()) return Fail(Error, TEXT("Select the existing shared training Ruler; diagnostics cannot mutate a production owner."));
    FOGFoundationStrategicContext C;
    C.bDiagnostic = true; C.OwnerId = SelectedRuler;
    C.TerritoryId = DiagnosticId(2); C.ClaimId = DiagnosticId(3); C.CoreId = DiagnosticId(4);
    C.LocationId = DiagnosticId(5); C.DestinationLocationId = DiagnosticId(6);
    C.RealityId = DiagnosticId(7); C.DestinationRealityId = DiagnosticId(8);
    C.JunctionId = DiagnosticId(9); C.FactionId = DiagnosticId(10); C.OtherFactionId = DiagnosticId(11);
    C.ArmyId = DiagnosticId(12); C.OtherArmyId = DiagnosticId(13);
    C.ResourceIds = { DiagnosticContent(TEXT("materials")), DiagnosticContent(TEXT("food")) };
    bool bFound = false; FName Kind; FString State; int64 Revision = 0;
    if (!Store.TryReadEntity(C.OwnerId, bFound, Kind, State, Revision, Error)) return false;
    if (!bFound || Kind != FName(TEXT("ruler"))) return Fail(Error, TEXT("Initialize the existing shared training Ruler before its strategic context."));
    if (!Store.TryReadEntity(DiagnosticId(1), bFound, Kind, State, Revision, Error)) return false;
    if (bFound)
    {
        if (Kind != FName(TEXT("foundation_diagnostic_scope")) || State != TEXT("{\"scope\":\"foundation_diagnostic.strategy.v1\"}"))
            return Fail(Error, TEXT("Diagnostic ID collision; existing entities were preserved."));
        TArray<FOGWorldEvent> Events;
        if (!Store.ListWorldEvents(C.OwnerId, NAME_None, false, 256, Events, Error)) return false;
        // Events are newest first: select the newest canonical handle for each singleton.
        for (int32 Index = Events.Num() - 1; Index >= 0; --Index)
        {
            const auto& Event = Events[Index];
            if (Event.RelatedEntities.IsEmpty()) continue;
            for (uint8 Action = 0; Action <= static_cast<uint8>(EOGFoundationStrategicAction::AcknowledgeReport); ++Action)
                if (Event.EventType == ActionKey(static_cast<EOGFoundationStrategicAction>(Action)))
                    TrackResult(C, static_cast<EOGFoundationStrategicAction>(Action), Event.RelatedEntities[0]);
        }
        Out = MoveTemp(C); return true;
    }
    // Never upsert a conflicting preexisting diagnostic ID or reset a progressed fixture.
    for (uint32 Ordinal = 2; Ordinal <= 14; ++Ordinal)
    {
        if (!Store.TryReadEntity(DiagnosticId(Ordinal), bFound, Kind, State, Revision, Error)) return false;
        if (bFound) return Fail(Error, TEXT("Diagnostic ID collision; no fixture entities were overwritten."));
    }
    FCommandTransaction Transaction(Store);
    if (!Transaction.Begin(Error)) return false;
    if (!Store.UpsertEntity(DiagnosticId(1), FName(TEXT("foundation_diagnostic_scope")), Tick,
        TEXT("{\"scope\":\"foundation_diagnostic.strategy.v1\"}"), Error)) return false;
    FOGTimeDomainRecord Time; Time.TimeDomainId = DiagnosticId(14); Time.CalendarId = DiagnosticContent(TEXT("calendar"));
    if (!FOGWorldTimeService(Store).SaveTimeDomain(Time, Tick, Error)) return false;
    FOGRealityNodeRecord Reality; Reality.RealityId = C.RealityId; Reality.Kind = FName(TEXT("diagnostic_world"));
    Reality.TimeDomainId = Time.TimeDomainId; Reality.WorldRankId = DiagnosticContent(TEXT("rank"));
    if (!FOGRealityGraphService(Store).SaveRealityNode(Reality, Tick, Error)) return false;
    Reality.ParentRealityId = C.RealityId; Reality.RealityId = C.DestinationRealityId; Reality.Kind = FName(TEXT("diagnostic_pocket"));
    if (!FOGRealityGraphService(Store).SaveRealityNode(Reality, Tick, Error)) return false;
    // Locations reference the Territory entity; the Territory record then
    // references its root Location. Create the identity before either record.
    if (!Store.UpsertEntity(C.TerritoryId, TEXT("territory"), Tick, TEXT("{}"), Error)) return false;
    FOGLocationRecord Location; Location.LocationId = C.LocationId; Location.Kind = FName(TEXT("diagnostic_settlement")); Location.TerritoryId = C.TerritoryId;
    if (!Store.UpsertLocation(Location, Tick, Error)) return false;
    Location.LocationId = C.DestinationLocationId; Location.Kind = FName(TEXT("diagnostic_outpost"));
    if (!Store.UpsertLocation(Location, Tick, Error)) return false;
    FOGTerritoryRecord Territory; Territory.TerritoryId = C.TerritoryId; Territory.RulerId = C.OwnerId;
    Territory.RootLocationId = C.LocationId; Territory.Population = 12;
    if (!Store.UpsertTerritory(Territory, Tick, Error)) return false;
    FOGLocationTerritoryRecord Membership; Membership.TerritoryId = C.TerritoryId; Membership.LocationId = C.LocationId;
    if (!Store.UpsertLocationTerritory(Membership, Error)) return false;
    Membership.LocationId = C.DestinationLocationId;
    if (!Store.UpsertLocationTerritory(Membership, Error)) return false;
    FOGTerritoryClaimRecord Claim; Claim.ClaimId = C.ClaimId; Claim.TerritoryId = C.TerritoryId; Claim.RulerId = C.OwnerId;
    Claim.ControlStrengthBps = 10000; Claim.ClaimStartWorldTick = Tick; Claim.bHasEffectiveControlStart = true;
    Claim.EffectiveControlStartWorldTick = Tick; Claim.UpdatedWorldTick = Tick;
    if (!Store.UpsertTerritoryClaim(Claim, Tick, Error)) return false;
    FOGDomainCoreRecord Core; Core.CoreId = C.CoreId; Core.TerritoryId = C.TerritoryId; Core.ControllerRulerId = C.OwnerId;
    Core.Lifecycle = EOGDomainCoreLifecycle::Awakened; Core.MaxDurability = FOGLargeNumber::FromInt64(10); Core.CurrentDurability = Core.MaxDurability;
    if (!Store.UpsertDomainCore(Core, Tick, Error) || !FOGDomainCoreService(Store).ActivateAwakenedCoreAsHeart(C.CoreId, Tick, Error)) return false;
    for (const auto& ResourceId : C.ResourceIds) if (!Store.SetResourceBalance(C.OwnerId, ResourceId, 20, Error)) return false;
    FOGFactionRecord Faction; Faction.FactionId = C.FactionId; Faction.LeaderRulerId = C.OwnerId; Faction.Kind = FName(TEXT("foundation_diagnostic"));
    if (!Store.UpsertFaction(Faction, Tick, Error)) return false;
    Faction.FactionId = C.OtherFactionId; Faction.LeaderRulerId = FOGEntityId();
    if (!Store.UpsertFaction(Faction, Tick, Error)) return false;
    for (const auto& ArmyId : { C.ArmyId, C.OtherArmyId })
    {
        FOGArmyRecord Army; Army.ArmyId = ArmyId; Army.FactionId = ArmyId == C.ArmyId ? C.FactionId : C.OtherFactionId;
        Army.LocationId = ArmyId == C.ArmyId ? C.LocationId : C.DestinationLocationId; Army.Headcount = 2;
        if (!Store.UpsertArmy(Army, Tick, Error)) return false;
        FOGArmyCapabilityRecord Capability; Capability.ArmyId = ArmyId;
        Capability.CapabilityId = DiagnosticContent(TEXT("ground_transport")); Capability.Magnitude = FOGLargeNumber::FromInt64(2);
        if (!FOGStrategicResolutionService(Store).SetArmyCapability(Capability, Error)) return false;
    }
    FOGJunctionRecord Junction; Junction.JunctionId = C.JunctionId;
    Junction.FromRealityId = C.RealityId; Junction.ToRealityId = C.DestinationRealityId;
    if (!FOGRealityGraphService(Store).SaveJunction(Junction, Tick, Error)) return false;
    FOGFoundationStrategicCommandResult Result;
    if (!AddEventAndReport(Store, C.OwnerId, FName(TEXT("foundation.strategy.diagnostic_seeded")), C.TerritoryId,
        Tick, TEXT("{\"scope\":\"foundation_diagnostic.strategy.v1\",\"diagnostic\":true}"), Result, Error)) return false;
    if (!Transaction.Commit(Error)) return false;
    Out = MoveTemp(C); return true;
}

namespace
{
FOGDispatchResolver DiagnosticDispatchResolver(IOGWorldStore& Store, const FOGDispatchRecord& Dispatch)
{
    return [&Store, Dispatch](const FOGDispatchResolutionContext& Policy, FOGDispatchResolutionResult& Result, FString& Error)
    {
        if (Dispatch.OwnerEntityId != DiagnosticOwner() || Dispatch.Type != EOGDispatchType::Exploration ||
            Dispatch.RiskBps != 0 || Dispatch.ParticipantEntityIds.Num() != 1 || Dispatch.ParticipantEntityIds[0] != DiagnosticId(12))
            return Fail(Error, TEXT("This bounded resolver only supports the isolated zero-risk diagnostic exploration mission."));
        if (!Policy.HardConstraints.IsEmpty() || Policy.MandatoryObjectives.Num() != 1 ||
            Policy.MandatoryObjectives[0].ObjectiveId != DiagnosticContent(TEXT("inspect_outpost")) ||
            Policy.MandatoryObjectives[0].TargetEntityId != DiagnosticId(6))
            return Fail(Error, TEXT("The diagnostic resolver cannot evaluate unsupported mission policy."));
        TSharedPtr<FJsonObject> Abort;
        if (!Parse(Policy.AbortPolicyJson, Abort, Error)) return false;
        bool bReturnOnUnavailable = false;
        if (!Abort->TryGetBoolField(TEXT("return_if_unavailable"), bReturnOnUnavailable) || !bReturnOnUnavailable)
            return Fail(Error, TEXT("The diagnostic mission requires its explicit return-on-unavailable abort policy."));
        bool bFound = false; FOGLocationRecord Target;
        if (!Store.TryReadLocation(Dispatch.TargetEntityId, bFound, Target, Error)) return false;
        const bool bAccessible = bFound && Target.bPhysicallyAccessible && Target.LocationId == DiagnosticId(6);
        FOGArmyRecord Army;
        if (!Store.TryReadArmy(DiagnosticId(12), bFound, Army, Error)) return false;
        TArray<FOGArmyCapabilityRecord> Capabilities;
        if (!Store.ListArmyCapabilities(DiagnosticId(12), Capabilities, Error)) return false;
        bool bCanTravel = false;
        for (const auto& Capability : Capabilities)
            if (Capability.CapabilityId == DiagnosticContent(TEXT("ground_transport")) && Capability.Magnitude.GetSign() > 0)
                bCanTravel = true;
        const bool bPartyAvailable = bFound && Army.FactionId == DiagnosticId(10) && Army.State == FName(TEXT("ready")) && Army.Headcount > 0 && bCanTravel;
        Result.bMandatoryObjectivesEvaluated = true; Result.bHardConstraintsEvaluated = true;
        Result.bSurvivalAbortPolicyEvaluated = true;
        Result.bSucceeded = bAccessible && bPartyAvailable;
        Result.OutcomeState = Result.bSucceeded ? FName(TEXT("diagnostic_outpost_inspected")) : FName(TEXT("aborted_unavailable"));
        Result.ResultJson = Result.bSucceeded
            ? TEXT("{\"diagnostic\":true,\"target_accessible\":true,\"party_available\":true,\"reward_granted\":false}")
            : TEXT("{\"diagnostic\":true,\"aborted\":true,\"reward_granted\":false}");
        return true;
    };
}
}

bool FOGFoundationStrategicRuntime::ExecuteDiagnosticAction(FOGFoundationStrategicContext& Context,
    EOGFoundationStrategicAction Action, int64 Tick, int64 Duration,
    FOGFoundationStrategicCommandResult& Result, FString& Error)
{
    Result = FOGFoundationStrategicCommandResult(); Error.Reset();
    if (!Context.bDiagnostic || Context.OwnerId != DiagnosticOwner())
        return Fail(Error, TEXT("Diagnostic actions require the explicit isolated diagnostic context."));
    if (Tick < 0 || Duration <= 0 || Tick > MAX_int64 - Duration)
        return Fail(Error, TEXT("Supply a positive authored diagnostic duration in canonical ticks without overflow."));
    FOGFoundationStrategicCommand C; C.Action = Action; C.WorldTick = Tick; C.DueWorldTick = Tick + Duration;
    C.PayloadJson = TEXT("{\"diagnostic\":true}");
    switch (Action)
    {
    case EOGFoundationStrategicAction::StartProject:
    {
        C.DefinitionId = DiagnosticContent(TEXT("outpost_repairs"));
        FOGProjectResourceCost Cost; Cost.ResourceId = DiagnosticContent(TEXT("materials")); Cost.Amount = 2;
        C.ResourceCosts.Add(Cost); break;
    }
    case EOGFoundationStrategicAction::RefreshProject:
        if (Context.ProjectIds.IsEmpty()) return Fail(Error, TEXT("Start a diagnostic Project first."));
        C.SubjectId = Context.ProjectIds.Last(); break;
    case EOGFoundationStrategicAction::StartDispatch:
    {
        C.TargetId = Context.DestinationLocationId; C.Participants = { Context.ArmyId };
        C.AbortPolicyJson = TEXT("{\"return_if_unavailable\":true}");
        FOGDispatchObjectiveRecord Objective; Objective.ObjectiveId = DiagnosticContent(TEXT("inspect_outpost"));
        Objective.bMandatory = true; Objective.TargetEntityId = Context.DestinationLocationId;
        C.DispatchObjectives.Add(Objective); C.Seed = 4001; break;
    }
    case EOGFoundationStrategicAction::ResolveDispatch:
    {
        if (Context.DispatchIds.IsEmpty()) return Fail(Error, TEXT("Start a diagnostic Dispatch first."));
        C.SubjectId = Context.DispatchIds.Last(); FOGDispatchRecord Dispatch;
        if (!CheckOwnedDispatch(Store, Context.OwnerId, C.SubjectId, Dispatch, Error)) return false;
        C.DispatchResolver = DiagnosticDispatchResolver(Store, Dispatch); break;
    }
    case EOGFoundationStrategicAction::DeclareWar:
        C.TargetId = Context.OtherFactionId; C.State = FName(TEXT("diagnostic_exercise")); break;
    case EOGFoundationStrategicAction::IssueWarOrder:
        C.TargetId = Context.ArmyId; C.DefinitionId = DiagnosticContent(TEXT("hold_outpost"));
        C.PayloadJson = TEXT("{\"diagnostic\":true,\"do_not_attack\":true}"); break;
    case EOGFoundationStrategicAction::RecordWarOrderOutcome:
        C.State = FName(TEXT("delayed"));
        C.PayloadJson = TEXT("{\"diagnostic\":true,\"reason\":\"awaiting_world_resolution\"}"); break;
    case EOGFoundationStrategicAction::DamageCore: C.Damage = FOGLargeNumber::FromInt64(2); break;
    case EOGFoundationStrategicAction::OpenJunction:
        C.JunctionRequirements = [](IOGWorldStore& Store, const FOGJunctionRecord& Junction, FString& Error)
        {
            if (Junction.JunctionId != DiagnosticId(9) || Junction.FromRealityId != DiagnosticId(7) || Junction.ToRealityId != DiagnosticId(8))
                return Fail(Error, TEXT("Diagnostic opening only applies to the isolated fixture Junction."));
            bool bA = false, bB = false; FOGRealityNodeRecord A, B;
            return Store.TryReadRealityNode(Junction.FromRealityId, bA, A, Error) &&
                Store.TryReadRealityNode(Junction.ToRealityId, bB, B, Error) && bA && bB && Junction.StabilityBps > 0;
        };
        break;
    case EOGFoundationStrategicAction::CloseJunction: C.State = FName(TEXT("diagnostic_manual_closure")); break;
    case EOGFoundationStrategicAction::SaveLogisticsRoute:
        C.Route.RouteId = DiagnosticId(15); C.Route.OwnerEntityId = Context.OwnerId;
        C.Route.OriginLocationId = Context.LocationId; C.Route.DestinationLocationId = Context.DestinationLocationId;
        C.Route.OriginRealityId = Context.RealityId; C.Route.DestinationRealityId = Context.DestinationRealityId;
        C.Route.TransportCapabilityId = DiagnosticContent(TEXT("ground_transport")); C.Route.Capacity = FOGLargeNumber::FromInt64(2);
        C.TransportValidator = [&Store = Store](const FOGEntityId& Owner, const FOGContentId& Capability,
            const FOGEntityId& Origin, const FOGEntityId& OriginReality, const FOGEntityId& Destination,
            const FOGEntityId& DestinationReality, FString& Error)
        {
            if (Owner != DiagnosticOwner() || Capability != DiagnosticContent(TEXT("ground_transport")) ||
                Origin != DiagnosticId(5) || Destination != DiagnosticId(6) || OriginReality != DiagnosticId(7) || DestinationReality != DiagnosticId(8))
                return Fail(Error, TEXT("The diagnostic transport validator only supports its own authored endpoints."));
            bool bFound = false; FOGJunctionRecord Junction;
            if (!Store.TryReadJunction(DiagnosticId(9), bFound, Junction, Error)) return false;
            if (!bFound || Junction.State != FName(TEXT("open"))) return Fail(Error, TEXT("Open the diagnostic Junction before establishing this cross-Reality route."));
            TArray<FOGArmyCapabilityRecord> Capabilities;
            if (!Store.ListArmyCapabilities(DiagnosticId(12), Capabilities, Error)) return false;
            for (const auto& Owned : Capabilities)
                if (Owned.CapabilityId == Capability && Owned.Magnitude.GetSign() > 0) return true;
            return Fail(Error, TEXT("No positive owned transport capability exists."));
        };
        break;
    case EOGFoundationStrategicAction::ResolveArmyInteraction:
        C.Participants = { Context.ArmyId, Context.OtherArmyId }; C.Seed = 4002;
        C.ArmyResolver = [](const TArray<FOGArmyResolutionInput>& Armies, int64,
            FOGStrategicArmyResolutionResult& Out, FString& Error)
        {
            if (Armies.Num() != 2 || Armies[0].Army.ArmyId != DiagnosticId(12) || Armies[1].Army.ArmyId != DiagnosticId(13))
                return Fail(Error, TEXT("The diagnostic resolver only evaluates its two isolated fixture Armies."));
            FOGLargeNumber Magnitude[2];
            for (int32 Index = 0; Index < 2; ++Index)
                for (const auto& Capability : Armies[Index].Capabilities)
                    if (Capability.CapabilityId == DiagnosticContent(TEXT("ground_transport")))
                        Magnitude[Index] = FOGLargeNumber::Add(Magnitude[Index], Capability.Magnitude);
            Out.Outcome = FName(TEXT("diagnostic_capability_review"));
            auto Json = MakeShared<FJsonObject>(); Json->SetBoolField(TEXT("diagnostic"), true);
            Json->SetNumberField(TEXT("transport_comparison"), FOGLargeNumber::Compare(Magnitude[0], Magnitude[1]));
            Json->SetBoolField(TEXT("war_resolved"), false); Out.OutcomeJson = Serialize(Json); return true;
        };
        break;
    case EOGFoundationStrategicAction::DisplaceClaim:
    case EOGFoundationStrategicAction::ReclaimClaim:
    case EOGFoundationStrategicAction::ExpireClaim:
    case EOGFoundationStrategicAction::ActivateCore:
    case EOGFoundationStrategicAction::RefreshHeart:
    case EOGFoundationStrategicAction::CreateWarFront: break;
    default: return Fail(Error, TEXT("This menu action needs additional authored parameters and has no diagnostic shortcut."));
    }
    return Execute(Context, C, Result, Error);
}

FName FOGFoundationStrategicRuntime::DueResolverKey() { return FName(TEXT("foundation.strategy.due_action.v1")); }

void FOGFoundationStrategicRuntime::RegisterClockResolvers(FOGCanonicalClockRuntime& Clock)
{
    Clock.RegisterResolver(DueResolverKey(), [](IOGWorldStore& Store,
        const FOGWorldDirectorScheduleRecord& Schedule, int64 Tick, bool bOffline,
        FString& Outcome, FString& Error)
    { return ResolveDueAction(Store, Schedule, Tick, bOffline, Outcome, Error); });
}

bool FOGFoundationStrategicRuntime::BindScheduledActions(const FOGFoundationStrategicContext& Context,
    FOGCanonicalClockRuntime& Clock, FString& Error)
{
    RegisterClockResolvers(Clock);
    FOGFoundationStrategicProjection Projection;
    if (!BuildProjection(Context, Clock.GetCanonicalWorldTick(), Projection, Error)) return false;
    for (const auto& Schedule : Projection.Schedules)
    {
        if (Schedule.Status != FName(TEXT("scheduled")) && Schedule.Status != FName(TEXT("active"))) continue;
        TSharedPtr<FJsonObject> Provenance;
        if (!Parse(Schedule.DecisionProvenanceJson, Provenance, Error)) return false;
        FString Resolver;
        if (!Provenance->TryGetStringField(TEXT("resolver_key"), Resolver) || Resolver != DueResolverKey().ToString()) continue;
        // Costs were paid when the Project/Dispatch started. This resolver adds
        // zero new irreversible loss; no catastrophic-collapse bypass is requested.
        if (!Clock.RegisterDueAction(Schedule.ScheduleId, DueResolverKey(), Context.OwnerId,
            0, 0, false, false, Error)) return false;
    }
    return true;
}

bool FOGFoundationStrategicRuntime::ScheduleDueAction(FOGFoundationStrategicContext& Context,
    const FOGEntityId& Subject, FName Kind, int64 Tick, int64 DueTick,
    FOGWorldDirectorScheduleRecord& OutSchedule, FString& Error)
{
    OutSchedule = FOGWorldDirectorScheduleRecord(); Error.Reset();
    if (Tick < 0 || DueTick < Tick || !Subject.IsValid()) return Fail(Error, TEXT("Due action requires a valid subject and bounded canonical deadline."));
    if (Kind == FName(TEXT("project")))
    {
        FOGProjectRecord Project;
        if (!CheckOwnedProject(Store, Context.OwnerId, Subject, Project, Error)) return false;
        if (Project.ResolveWorldTick != DueTick) return Fail(Error, TEXT("Project schedule deadline must equal its canonical resolution tick."));
    }
    else if (Kind == FName(TEXT("dispatch")))
    {
        FOGDispatchRecord Dispatch;
        if (!CheckOwnedDispatch(Store, Context.OwnerId, Subject, Dispatch, Error)) return false;
        if (!Context.bDiagnostic || Dispatch.OwnerEntityId != DiagnosticOwner())
            return Fail(Error, TEXT("Production Dispatch scheduling requires registration of its authored mission resolver."));
        if (Dispatch.ResolveWorldTick != DueTick) return Fail(Error, TEXT("Dispatch schedule deadline must equal its canonical resolution tick."));
    }
    else return Fail(Error, TEXT("Unsupported due action kind; provide an authored Director resolver."));
    const auto Content = FOGContentId(FString(TEXT("foundation_runtime:due/")) + Kind.ToString() + TEXT("/") + Subject.ToString());
    TArray<FOGWorldDirectorScheduleRecord> Existing;
    if (!Store.ListWorldDirectorSchedulesByContent(Content, Existing, Error)) return false;
    if (!Existing.IsEmpty())
    {
        OutSchedule = Existing[0]; Context.ScheduleIds.AddUnique(OutSchedule.ScheduleId); return true;
    }
    auto Provenance = MakeShared<FJsonObject>();
    Provenance->SetStringField(TEXT("audit_reason"), TEXT("Resolve an existing canonical strategic record at its own authored deadline."));
    Provenance->SetObjectField(TEXT("world_state_inputs"), MakeShared<FJsonObject>());
    Provenance->SetObjectField(TEXT("package_versions"), MakeShared<FJsonObject>());
    Provenance->SetStringField(TEXT("subject_id"), Subject.ToString());
    Provenance->SetStringField(TEXT("owner_id"), Context.OwnerId.ToString());
    Provenance->SetStringField(TEXT("action_kind"), Kind.ToString());
    Provenance->SetStringField(TEXT("resolver_key"), DueResolverKey().ToString());
    FOGFoundationStrategicCommand C; C.Action = EOGFoundationStrategicAction::ScheduleContent; C.WorldTick = Tick;
    C.Schedule.ContentId = Content; C.Schedule.EligibleSinceWorldTick = Tick;
    C.Schedule.EarliestStartWorldTick = DueTick; C.Schedule.LatestStartWorldTick = DueTick;
    C.Schedule.DecisionProvenanceJson = Serialize(Provenance);
    FOGFoundationStrategicCommandResult Result;
    if (!Execute(Context, C, Result, Error)) return false;
    bool bFound = false;
    if (!Store.TryReadWorldDirectorSchedule(Result.EntityId, bFound, OutSchedule, Error)) return false;
    return bFound || Fail(Error, TEXT("The newly persisted Director schedule could not be read."));
}

bool FOGFoundationStrategicRuntime::ResolveDueAction(IOGWorldStore& Store,
    const FOGWorldDirectorScheduleRecord& Schedule, int64 ObservedTick, bool bOffline,
    FString& OutcomeJson, FString& Error)
{
    OutcomeJson.Reset(); Error.Reset();
    if (Schedule.Status != FName(TEXT("active")) || !Schedule.bHasScheduledStartWorldTick ||
        ObservedTick < Schedule.ScheduledStartWorldTick)
        return Fail(Error, TEXT("Only active due Director schedules may resolve strategic records."));
    TSharedPtr<FJsonObject> Provenance;
    if (!Parse(Schedule.DecisionProvenanceJson, Provenance, Error)) return false;
    FString SubjectString, OwnerString, Kind, Resolver;
    if (!Provenance->TryGetStringField(TEXT("subject_id"), SubjectString) ||
        !Provenance->TryGetStringField(TEXT("owner_id"), OwnerString) ||
        !Provenance->TryGetStringField(TEXT("action_kind"), Kind) ||
        !Provenance->TryGetStringField(TEXT("resolver_key"), Resolver) || Resolver != DueResolverKey().ToString())
        return Fail(Error, TEXT("Strategic schedule is missing canonical subject/owner/resolver provenance."));
    FGuid SubjectGuid, OwnerGuid;
    if (!FGuid::Parse(SubjectString, SubjectGuid) || !FGuid::Parse(OwnerString, OwnerGuid))
        return Fail(Error, TEXT("Strategic schedule contains invalid entity IDs."));
    FOGFoundationStrategicContext Context; Context.OwnerId = FOGEntityId(OwnerGuid);
    Context.bDiagnostic = Context.OwnerId == DiagnosticOwner();
    FOGFoundationStrategicCommand C; C.SubjectId = FOGEntityId(SubjectGuid);
    C.WorldTick = Schedule.ScheduledStartWorldTick;
    if (Schedule.ContentId != FOGContentId(FString(TEXT("foundation_runtime:due/")) + Kind + TEXT("/") + C.SubjectId.ToString()))
        return Fail(Error, TEXT("Schedule content ID does not match its canonical action provenance."));
    if (Kind == TEXT("project"))
    {
        FOGProjectRecord Project;
        if (!CheckOwnedProject(Store, Context.OwnerId, C.SubjectId, Project, Error)) return false;
        if (Project.ResolveWorldTick != C.WorldTick) return Fail(Error, TEXT("Director and Project canonical deadlines disagree."));
        C.Action = EOGFoundationStrategicAction::RefreshProject;
    }
    else if (Kind == TEXT("dispatch"))
    {
        FOGDispatchRecord Dispatch;
        if (!CheckOwnedDispatch(Store, Context.OwnerId, C.SubjectId, Dispatch, Error)) return false;
        if (Dispatch.ResolveWorldTick != C.WorldTick) return Fail(Error, TEXT("Director and Dispatch canonical deadlines disagree."));
        C.Action = EOGFoundationStrategicAction::ResolveDispatch;
        C.DispatchResolver = DiagnosticDispatchResolver(Store, Dispatch);
    }
    else return Fail(Error, TEXT("No strategic due resolver is registered for this kind."));
    FOGFoundationStrategicCommandResult Result;
    if (!FOGFoundationStrategicRuntime(Store).Execute(Context, C, Result, Error)) return false;
    auto Outcome = MakeShared<FJsonObject>(); Outcome->SetStringField(TEXT("subject_id"), C.SubjectId.ToString());
    Outcome->SetStringField(TEXT("action_kind"), Kind); Outcome->SetBoolField(TEXT("offline"), bOffline);
    Outcome->SetBoolField(TEXT("changed"), Result.bChanged); Outcome->SetBoolField(TEXT("idempotent_replay"), !Result.bChanged);
    OutcomeJson = Serialize(Outcome); return true;
}
