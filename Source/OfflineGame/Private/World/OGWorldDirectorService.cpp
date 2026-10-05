#include "World/OGWorldDirectorService.h"

#include "Dom/JsonObject.h"
#include "Events/OGWorldEvent.h"
#include "Random/OGDeterministicRng.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

bool FOGWorldDirectorService::ValidateJsonObject(
    const FString& Json,
    FString& OutError)
{
    TSharedPtr<FJsonObject> Object;
    const TSharedRef<TJsonReader<>> Reader =
        TJsonReaderFactory<>::Create(Json);

    if (!FJsonSerializer::Deserialize(
            Reader,
            Object) ||
        !Object.IsValid())
    {
        OutError =
            TEXT("World Director JSON must be a valid object.");
        return false;
    }

    return true;
}

bool FOGWorldDirectorService::ValidateDecisionProvenance(
    const FString& Json,
    bool bComposedTemplate,
    FString& OutError)
{
    TSharedPtr<FJsonObject> Object;
    const TSharedRef<TJsonReader<>> Reader =
        TJsonReaderFactory<>::Create(Json);

    if (!FJsonSerializer::Deserialize(
            Reader,
            Object) ||
        !Object.IsValid())
    {
        OutError =
            TEXT("World Director decision provenance must be valid JSON.");
        return false;
    }

    FString AuditReason;
    if (!Object->TryGetStringField(
            TEXT("audit_reason"),
            AuditReason) ||
        AuditReason.IsEmpty() ||
        !Object->HasField(
            TEXT("world_state_inputs")) ||
        !Object->HasField(
            TEXT("package_versions")))
    {
        OutError =
            TEXT("World Director provenance requires audit_reason, world_state_inputs and package_versions.");
        return false;
    }

    if (bComposedTemplate &&
        !Object->HasField(
            TEXT("participating_content_ids")))
    {
        OutError =
            TEXT("Composed World Director content requires participating_content_ids provenance.");
        return false;
    }

    return true;
}

bool FOGWorldDirectorService::ScheduleEligibleContent(
    const FOGWorldDirectorScheduleRequest& Request,
    FOGWorldDirectorScheduleRecord& OutSchedule,
    FString& OutError)
{
    OutSchedule =
        FOGWorldDirectorScheduleRecord();
    OutError.Reset();

    if (!Request.ContentId.IsValid() ||
        Request.EligibleSinceWorldTick < 0 ||
        Request.EarliestStartWorldTick <
            Request.EligibleSinceWorldTick ||
        Request.LatestStartWorldTick <
            Request.EarliestStartWorldTick ||
        !ValidateDecisionProvenance(
            Request.DecisionProvenanceJson,
            Request.TemplateId.IsValid(),
            OutError))
    {
        if (OutError.IsEmpty())
        {
            OutError =
                TEXT("World Director scheduling request is invalid.");
        }
        return false;
    }

    const uint64 Span =
        static_cast<uint64>(
            Request.LatestStartWorldTick -
            Request.EarliestStartWorldTick);

    FOGDeterministicRng Rng(
        static_cast<uint64>(
            Request.ResolutionSeed));

    const uint64 Offset =
        Span == 0
            ? 0
            : Rng.NextUInt64() %
                (Span + 1ull);

    const int64 ScheduledStart =
        Request.EarliestStartWorldTick +
        static_cast<int64>(Offset);

    FOGWorldDirectorScheduleRecord Schedule;
    Schedule.ScheduleId =
        FOGEntityId::NewId();
    Schedule.ContentId =
        Request.ContentId;
    Schedule.TemplateId =
        Request.TemplateId;
    Schedule.Status =
        FName(TEXT("scheduled"));
    Schedule.bHasEligibleSinceWorldTick =
        true;
    Schedule.EligibleSinceWorldTick =
        Request.EligibleSinceWorldTick;
    Schedule.bHasScheduledStartWorldTick =
        true;
    Schedule.ScheduledStartWorldTick =
        ScheduledStart;
    Schedule.bHasLatestStartWorldTick =
        true;
    Schedule.LatestStartWorldTick =
        Request.LatestStartWorldTick;
    Schedule.ResolutionSeed =
        Request.ResolutionSeed;
    Schedule.DecisionProvenanceJson =
        Request.DecisionProvenanceJson;

    if (!Store.BeginTransaction(
            OutError))
    {
        return false;
    }

    if (!Store.UpsertWorldDirectorSchedule(
            Schedule,
            Request.EligibleSinceWorldTick,
            OutError))
    {
        FString RollbackError;
        Store.RollbackTransaction(
            RollbackError);
        return false;
    }

    bool bUnlockFound = false;
    FOGContentUnlockStateRecord Unlock;
    if (!Store.TryReadContentUnlockState(
            Request.ContentId,
            bUnlockFound,
            Unlock,
            OutError))
    {
        FString RollbackError;
        Store.RollbackTransaction(
            RollbackError);
        return false;
    }

    if (!bUnlockFound ||
        Unlock.State ==
            FName(TEXT("sealed")))
    {
        Unlock.ContentId =
            Request.ContentId;
        Unlock.State =
            FName(TEXT("eligible"));
        Unlock.bHasEligibleWorldTick =
            true;
        Unlock.EligibleWorldTick =
            Request.EligibleSinceWorldTick;

        if (!Store.UpsertContentUnlockState(
                Unlock,
                OutError))
        {
            FString RollbackError;
            Store.RollbackTransaction(
                RollbackError);
            return false;
        }
    }

    FOGWorldEvent Event;
    Event.EventId =
        FOGEntityId::NewId();
    Event.EventType =
        FName(TEXT("world_director.scheduled"));
    Event.WorldTick =
        Request.EligibleSinceWorldTick;
    Event.PrimaryEntity =
        Schedule.ScheduleId;
    Event.bChronicleEligible =
        false;
    Event.PayloadJson = FString::Printf(
        TEXT("{\"content\":\"%s\",\"template\":\"%s\",\"scheduled_start\":%lld,\"latest_start\":%lld,\"seed\":%lld,\"provenance\":%s}"),
        *Request.ContentId.ToString(),
        *Request.TemplateId.ToString(),
        static_cast<long long>(
            ScheduledStart),
        static_cast<long long>(
            Request.LatestStartWorldTick),
        static_cast<long long>(
            Request.ResolutionSeed),
        *Request.DecisionProvenanceJson);

    if (!Store.AppendWorldEvent(
            Event,
            OutError))
    {
        FString RollbackError;
        Store.RollbackTransaction(
            RollbackError);
        return false;
    }

    if (!Store.CommitTransaction(
            OutError))
    {
        FString RollbackError;
        Store.RollbackTransaction(
            RollbackError);
        return false;
    }

    OutSchedule =
        Schedule;
    return true;
}

bool FOGWorldDirectorService::DelayScheduleWithinAuthoredBound(
    const FOGEntityId& ScheduleId,
    int64 NewStartWorldTick,
    int64 DecisionWorldTick,
    const FString& DecisionProvenanceJson,
    FString& OutError)
{
    OutError.Reset();

    if (!ScheduleId.IsValid() ||
        NewStartWorldTick < 0 ||
        DecisionWorldTick < 0)
    {
        OutError =
            TEXT("World Director delay request is invalid.");
        return false;
    }

    bool bFound = false;
    FOGWorldDirectorScheduleRecord Schedule;
    if (!Store.TryReadWorldDirectorSchedule(
            ScheduleId,
            bFound,
            Schedule,
            OutError))
    {
        return false;
    }

    if (!bFound ||
        Schedule.Status !=
            FName(TEXT("scheduled")) ||
        !Schedule.bHasScheduledStartWorldTick ||
        !Schedule.bHasLatestStartWorldTick)
    {
        OutError =
            TEXT("Only an existing bounded scheduled decision can be delayed.");
        return false;
    }

    if (!ValidateDecisionProvenance(
            DecisionProvenanceJson,
            Schedule.TemplateId.IsValid(),
            OutError))
    {
        return false;
    }

    if (NewStartWorldTick <
            Schedule.ScheduledStartWorldTick ||
        NewStartWorldTick >
            Schedule.LatestStartWorldTick)
    {
        OutError =
            TEXT("World Director delay exceeds the authored scheduling bound or moves backward.");
        return false;
    }

    const int64 PreviousStart =
        Schedule.ScheduledStartWorldTick;
    Schedule.ScheduledStartWorldTick =
        NewStartWorldTick;
    Schedule.DecisionProvenanceJson =
        DecisionProvenanceJson;

    if (!Store.BeginTransaction(
            OutError))
    {
        return false;
    }

    if (!Store.UpsertWorldDirectorSchedule(
            Schedule,
            DecisionWorldTick,
            OutError))
    {
        FString RollbackError;
        Store.RollbackTransaction(
            RollbackError);
        return false;
    }

    FOGWorldEvent Event;
    Event.EventId =
        FOGEntityId::NewId();
    Event.EventType =
        FName(TEXT("world_director.delayed"));
    Event.WorldTick =
        DecisionWorldTick;
    Event.PrimaryEntity =
        Schedule.ScheduleId;
    Event.bChronicleEligible =
        false;
    Event.PayloadJson = FString::Printf(
        TEXT("{\"previous_start\":%lld,\"new_start\":%lld,\"latest_start\":%lld,\"provenance\":%s}"),
        static_cast<long long>(
            PreviousStart),
        static_cast<long long>(
            NewStartWorldTick),
        static_cast<long long>(
            Schedule.LatestStartWorldTick),
        *DecisionProvenanceJson);

    if (!Store.AppendWorldEvent(
            Event,
            OutError))
    {
        FString RollbackError;
        Store.RollbackTransaction(
            RollbackError);
        return false;
    }

    if (!Store.CommitTransaction(
            OutError))
    {
        FString RollbackError;
        Store.RollbackTransaction(
            RollbackError);
        return false;
    }

    return true;
}

bool FOGWorldDirectorService::ActivateDueContent(
    const FOGEntityId& ScheduleId,
    int64 ObservedWorldTick,
    FString& OutError)
{
    OutError.Reset();

    if (!ScheduleId.IsValid() ||
        ObservedWorldTick < 0)
    {
        OutError =
            TEXT("World Director activation request is invalid.");
        return false;
    }

    bool bFound = false;
    FOGWorldDirectorScheduleRecord Schedule;
    if (!Store.TryReadWorldDirectorSchedule(
            ScheduleId,
            bFound,
            Schedule,
            OutError))
    {
        return false;
    }

    if (!bFound ||
        Schedule.Status !=
            FName(TEXT("scheduled")) ||
        !Schedule.bHasScheduledStartWorldTick)
    {
        OutError =
            TEXT("Director content is not in a scheduled state.");
        return false;
    }

    if (ObservedWorldTick <
        Schedule.ScheduledStartWorldTick)
    {
        OutError =
            TEXT("Director content is not due yet.");
        return false;
    }

    const int64 EffectiveStartTick =
        Schedule.ScheduledStartWorldTick;
    Schedule.Status =
        FName(TEXT("active"));

    FOGContentUnlockStateRecord Unlock;
    bool bUnlockFound = false;
    if (!Store.TryReadContentUnlockState(
            Schedule.ContentId,
            bUnlockFound,
            Unlock,
            OutError))
    {
        return false;
    }

    Unlock.ContentId =
        Schedule.ContentId;
    Unlock.State =
        FName(TEXT("released"));
    if (!Unlock.bHasEligibleWorldTick &&
        Schedule.bHasEligibleSinceWorldTick)
    {
        Unlock.bHasEligibleWorldTick =
            true;
        Unlock.EligibleWorldTick =
            Schedule.EligibleSinceWorldTick;
    }
    Unlock.bHasReleasedWorldTick =
        true;
    Unlock.ReleasedWorldTick =
        EffectiveStartTick;

    if (!Store.BeginTransaction(
            OutError))
    {
        return false;
    }

    if (!Store.UpsertWorldDirectorSchedule(
            Schedule,
            EffectiveStartTick,
            OutError) ||
        !Store.UpsertContentUnlockState(
            Unlock,
            OutError))
    {
        FString RollbackError;
        Store.RollbackTransaction(
            RollbackError);
        return false;
    }

    FOGWorldEvent Event;
    Event.EventId =
        FOGEntityId::NewId();
    Event.EventType =
        FName(TEXT("world_director.activated"));
    Event.WorldTick =
        EffectiveStartTick;
    Event.PrimaryEntity =
        Schedule.ScheduleId;
    Event.bChronicleEligible =
        true;
    Event.PayloadJson = FString::Printf(
        TEXT("{\"content\":\"%s\",\"effective_start\":%lld,\"observed_tick\":%lld,\"seed\":%lld,\"provenance\":%s}"),
        *Schedule.ContentId.ToString(),
        static_cast<long long>(
            EffectiveStartTick),
        static_cast<long long>(
            ObservedWorldTick),
        static_cast<long long>(
            Schedule.ResolutionSeed),
        *Schedule.DecisionProvenanceJson);

    if (!Store.AppendWorldEvent(
            Event,
            OutError))
    {
        FString RollbackError;
        Store.RollbackTransaction(
            RollbackError);
        return false;
    }

    if (!Store.CommitTransaction(
            OutError))
    {
        FString RollbackError;
        Store.RollbackTransaction(
            RollbackError);
        return false;
    }

    return true;
}

bool FOGWorldDirectorService::CompleteSchedule(
    const FOGEntityId& ScheduleId,
    int64 WorldTick,
    const FString& OutcomeJson,
    FString& OutError)
{
    OutError.Reset();

    if (!ScheduleId.IsValid() ||
        WorldTick < 0 ||
        !ValidateJsonObject(
            OutcomeJson,
            OutError))
    {
        if (OutError.IsEmpty())
        {
            OutError =
                TEXT("World Director completion request is invalid.");
        }
        return false;
    }

    bool bFound = false;
    FOGWorldDirectorScheduleRecord Schedule;
    if (!Store.TryReadWorldDirectorSchedule(
            ScheduleId,
            bFound,
            Schedule,
            OutError))
    {
        return false;
    }

    if (!bFound ||
        Schedule.Status !=
            FName(TEXT("active")))
    {
        OutError =
            TEXT("Only active Director content can complete.");
        return false;
    }

    Schedule.Status =
        FName(TEXT("completed"));

    if (!Store.BeginTransaction(
            OutError))
    {
        return false;
    }

    if (!Store.UpsertWorldDirectorSchedule(
            Schedule,
            WorldTick,
            OutError))
    {
        FString RollbackError;
        Store.RollbackTransaction(
            RollbackError);
        return false;
    }

    FOGWorldEvent Event;
    Event.EventId =
        FOGEntityId::NewId();
    Event.EventType =
        FName(TEXT("world_director.completed"));
    Event.WorldTick =
        WorldTick;
    Event.PrimaryEntity =
        Schedule.ScheduleId;
    Event.bChronicleEligible =
        true;
    Event.PayloadJson = FString::Printf(
        TEXT("{\"content\":\"%s\",\"outcome\":%s}"),
        *Schedule.ContentId.ToString(),
        *OutcomeJson);

    if (!Store.AppendWorldEvent(
            Event,
            OutError))
    {
        FString RollbackError;
        Store.RollbackTransaction(
            RollbackError);
        return false;
    }

    if (!Store.CommitTransaction(
            OutError))
    {
        FString RollbackError;
        Store.RollbackTransaction(
            RollbackError);
        return false;
    }

    return true;
}

bool FOGWorldDirectorService::SetContentUnlockState(
    const FOGContentUnlockStateRecord& State,
    FString& OutError)
{
    return Store.UpsertContentUnlockState(
        State,
        OutError);
}

bool FOGWorldDirectorService::RecordActiveSessionBoundary(
    const FOGEntityId& ScopeEntityId,
    int64 WorldTick,
    FString& OutError)
{
    OutError.Reset();

    if (!ScopeEntityId.IsValid() ||
        WorldTick < 0)
    {
        OutError =
            TEXT("Active-session boundary requires a valid scope and world tick.");
        return false;
    }

    bool bFound = false;
    FOGOfflineSimulationStateRecord State;
    if (!Store.TryReadOfflineSimulationState(
            ScopeEntityId,
            bFound,
            State,
            OutError))
    {
        return false;
    }

    if (!bFound)
    {
        State.ScopeEntityId =
            ScopeEntityId;
        State.LastActiveWorldTick =
            WorldTick;
        State.LastCatchupWorldTick =
            WorldTick;
    }
    else
    {
        if (WorldTick <
                State.LastActiveWorldTick ||
            WorldTick <
                State.LastCatchupWorldTick)
        {
            OutError =
                TEXT("Active-session boundary cannot move canonical history backward.");
            return false;
        }

        State.LastActiveWorldTick =
            WorldTick;
    }

    return Store.UpsertOfflineSimulationState(
        State,
        OutError);
}

bool FOGWorldDirectorService::EvaluateOfflineCatchupGovernor(
    const FOGOfflineCatchupGovernorRequest& Request,
    FOGOfflineCatchupGovernorResult& OutResult,
    FString& OutError)
{
    OutResult =
        FOGOfflineCatchupGovernorResult();
    OutError.Reset();

    if (!Request.ScopeEntityId.IsValid() ||
        Request.CatchupWorldTick < 0 ||
        Request.RequestedNewIrreversibleLossBps < 0 ||
        Request.RequestedNewIrreversibleLossBps > 10000 ||
        Request.MaxNewIrreversibleLossBps < 0 ||
        Request.MaxNewIrreversibleLossBps > 10000 ||
        !ValidateJsonObject(
            Request.DecisionStateJson,
            OutError))
    {
        if (OutError.IsEmpty())
        {
            OutError =
                TEXT("Offline catch-up governor request is invalid.");
        }
        return false;
    }

    bool bFound = false;
    FOGOfflineSimulationStateRecord State;
    if (!Store.TryReadOfflineSimulationState(
            Request.ScopeEntityId,
            bFound,
            State,
            OutError))
    {
        return false;
    }

    if (!bFound)
    {
        OutError =
            TEXT("Offline catch-up requires a previously recorded active-session boundary.");
        return false;
    }

    if (Request.CatchupWorldTick <
            State.LastActiveWorldTick ||
        Request.CatchupWorldTick <
            State.LastCatchupWorldTick)
    {
        OutError =
            TEXT("Offline catch-up tick cannot move canonical history backward.");
        return false;
    }

    if (Request.bCausallyLockedBeforeLogout)
    {
        OutResult.bApproved = true;
        OutResult.ApprovedIrreversibleLossBps =
            Request.RequestedNewIrreversibleLossBps;
    }
    else if (
        Request.bWouldCauseProtectedCatastrophicCollapse ||
        Request.RequestedNewIrreversibleLossBps >
            Request.MaxNewIrreversibleLossBps)
    {
        OutResult.bDeferred = true;
        OutResult.ApprovedIrreversibleLossBps = 0;
    }
    else
    {
        OutResult.bApproved = true;
        OutResult.ApprovedIrreversibleLossBps =
            Request.RequestedNewIrreversibleLossBps;
    }

    State.LastCatchupWorldTick =
        Request.CatchupWorldTick;
    State.GovernorStateJson = FString::Printf(
        TEXT("{\"requested_new_loss_bps\":%d,\"max_new_loss_bps\":%d,\"approved_loss_bps\":%d,\"causally_locked\":%s,\"protected_catastrophic_collapse\":%s,\"deferred\":%s,\"decision_state\":%s}"),
        Request.RequestedNewIrreversibleLossBps,
        Request.MaxNewIrreversibleLossBps,
        OutResult.ApprovedIrreversibleLossBps,
        Request.bCausallyLockedBeforeLogout
            ? TEXT("true")
            : TEXT("false"),
        Request.bWouldCauseProtectedCatastrophicCollapse
            ? TEXT("true")
            : TEXT("false"),
        OutResult.bDeferred
            ? TEXT("true")
            : TEXT("false"),
        *Request.DecisionStateJson);

    if (!Store.BeginTransaction(
            OutError))
    {
        return false;
    }

    if (!Store.UpsertOfflineSimulationState(
            State,
            OutError))
    {
        FString RollbackError;
        Store.RollbackTransaction(
            RollbackError);
        return false;
    }

    FOGWorldEvent Event;
    Event.EventId =
        FOGEntityId::NewId();
    Event.EventType =
        FName(TEXT("world_director.offline_catchup_governed"));
    Event.WorldTick =
        Request.CatchupWorldTick;
    Event.PrimaryEntity =
        Request.ScopeEntityId;
    Event.bChronicleEligible =
        false;
    Event.PayloadJson =
        State.GovernorStateJson;

    if (!Store.AppendWorldEvent(
            Event,
            OutError))
    {
        FString RollbackError;
        Store.RollbackTransaction(
            RollbackError);
        return false;
    }

    if (!Store.CommitTransaction(
            OutError))
    {
        FString RollbackError;
        Store.RollbackTransaction(
            RollbackError);
        return false;
    }

    return true;
}
