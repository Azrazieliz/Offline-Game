#include "World/OGDispatchService.h"

bool FOGDispatchService::StartDispatch(
    const FOGEntityId& OwnerEntityId,
    const FOGEntityId& TargetEntityId,
    EOGDispatchType Type,
    const TArray<FOGEntityId>& ParticipantEntityIds,
    int64 StartWorldTick,
    int64 ResolveWorldTick,
    int32 RiskBps,
    int64 ResolutionSeed,
    FOGEntityId& OutDispatchId,
    FString& OutError)
{
    return StartDispatch(
        OwnerEntityId,
        TargetEntityId,
        Type,
        ParticipantEntityIds,
        StartWorldTick,
        ResolveWorldTick,
        RiskBps,
        5000,
        TEXT("{}"),
        {},
        {},
        ResolutionSeed,
        OutDispatchId,
        OutError);
}

bool FOGDispatchService::StartDispatch(
    const FOGEntityId& OwnerEntityId,
    const FOGEntityId& TargetEntityId,
    EOGDispatchType Type,
    const TArray<FOGEntityId>& ParticipantEntityIds,
    int64 StartWorldTick,
    int64 ResolveWorldTick,
    int32 RiskBps,
    int32 RiskToleranceBps,
    const FString& AbortPolicyJson,
    const TArray<FOGDispatchObjectiveRecord>& Objectives,
    const TArray<FOGDispatchConstraintRecord>& Constraints,
    int64 ResolutionSeed,
    FOGEntityId& OutDispatchId,
    FString& OutError)
{
    OutDispatchId =
        FOGEntityId();
    OutError.Reset();

    if (!OwnerEntityId.IsValid() ||
        ResolveWorldTick <
            StartWorldTick ||
        RiskBps < 0 ||
        RiskBps > 10000 ||
        RiskToleranceBps < 0 ||
        RiskToleranceBps > 10000)
    {
        OutError =
            TEXT("Dispatch definition is invalid.");
        return false;
    }

    TSet<FOGEntityId> UniqueParticipants;
    for (const FOGEntityId& ParticipantId :
         ParticipantEntityIds)
    {
        if (!ParticipantId.IsValid() ||
            UniqueParticipants.Contains(
                ParticipantId))
        {
            OutError =
                TEXT("Dispatch contains invalid or duplicate participants.");
            return false;
        }

        UniqueParticipants.Add(
            ParticipantId);
    }

    TSet<FOGContentId> ObjectiveIds;
    for (const FOGDispatchObjectiveRecord& Objective :
         Objectives)
    {
        if (!Objective.ObjectiveId.IsValid() ||
            ObjectiveIds.Contains(
                Objective.ObjectiveId))
        {
            OutError =
                TEXT("Dispatch contains invalid or duplicate objectives.");
            return false;
        }
        ObjectiveIds.Add(
            Objective.ObjectiveId);
    }

    TSet<FOGContentId> ConstraintIds;
    for (const FOGDispatchConstraintRecord& Constraint :
         Constraints)
    {
        if (!Constraint.ConstraintId.IsValid() ||
            ConstraintIds.Contains(
                Constraint.ConstraintId))
        {
            OutError =
                TEXT("Dispatch contains invalid or duplicate constraints.");
            return false;
        }
        ConstraintIds.Add(
            Constraint.ConstraintId);
    }

    FOGDispatchRecord Dispatch;
    Dispatch.DispatchId =
        FOGEntityId::NewId();
    Dispatch.OwnerEntityId =
        OwnerEntityId;
    Dispatch.TargetEntityId =
        TargetEntityId;
    Dispatch.Type =
        Type;
    Dispatch.Status =
        EOGDispatchStatus::Active;
    Dispatch.ParticipantEntityIds =
        ParticipantEntityIds;
    Dispatch.StartWorldTick =
        StartWorldTick;
    Dispatch.ResolveWorldTick =
        ResolveWorldTick;
    Dispatch.RiskBps =
        RiskBps;
    Dispatch.RiskToleranceBps =
        RiskToleranceBps;
    Dispatch.AbortPolicyJson =
        AbortPolicyJson.IsEmpty()
            ? TEXT("{}")
            : AbortPolicyJson;
    Dispatch.ResolutionSeed =
        ResolutionSeed;

    if (!Store.BeginTransaction(
            OutError))
    {
        return false;
    }

    if (!Store.UpsertDispatch(
            Dispatch,
            StartWorldTick,
            OutError))
    {
        FString RollbackError;
        Store.RollbackTransaction(
            RollbackError);
        return false;
    }

    for (FOGDispatchObjectiveRecord Objective :
         Objectives)
    {
        Objective.DispatchId =
            Dispatch.DispatchId;

        if (!Store.UpsertDispatchObjective(
                Objective,
                OutError))
        {
            FString RollbackError;
            Store.RollbackTransaction(
                RollbackError);
            return false;
        }
    }

    for (FOGDispatchConstraintRecord Constraint :
         Constraints)
    {
        Constraint.DispatchId =
            Dispatch.DispatchId;

        if (!Store.UpsertDispatchConstraint(
                Constraint,
                OutError))
        {
            FString RollbackError;
            Store.RollbackTransaction(
                RollbackError);
            return false;
        }
    }

    if (!Store.CommitTransaction(
            OutError))
    {
        FString RollbackError;
        Store.RollbackTransaction(
            RollbackError);
        return false;
    }

    OutDispatchId =
        Dispatch.DispatchId;
    return true;
}

bool FOGDispatchService::ResolveDispatch(
    const FOGEntityId& DispatchId,
    int64 CurrentWorldTick,
    bool bSucceeded,
    const FString& ResultJson,
    FOGDispatchRecord& OutDispatch,
    FString& OutError)
{
    return ResolveDispatchWithPolicy(
        DispatchId,
        CurrentWorldTick,
        [bSucceeded, ResultJson](
            const FOGDispatchResolutionContext&,
            FOGDispatchResolutionResult& OutResult,
            FString& Error)
        {
            Error.Reset();
            OutResult.bSucceeded =
                bSucceeded;
            OutResult.bMandatoryObjectivesEvaluated =
                true;
            OutResult.bHardConstraintsEvaluated =
                true;
            OutResult.bSurvivalAbortPolicyEvaluated =
                true;
            OutResult.OutcomeState =
                bSucceeded
                    ? FName(TEXT("success"))
                    : FName(TEXT("failure"));
            OutResult.ResultJson =
                ResultJson.IsEmpty()
                    ? TEXT("{}")
                    : ResultJson;
            return true;
        },
        OutDispatch,
        OutError);
}

bool FOGDispatchService::ResolveDispatchWithPolicy(
    const FOGEntityId& DispatchId,
    int64 CurrentWorldTick,
    const FOGDispatchResolver& Resolver,
    FOGDispatchRecord& OutDispatch,
    FString& OutError)
{
    OutDispatch =
        FOGDispatchRecord();
    OutError.Reset();

    if (!DispatchId.IsValid() ||
        CurrentWorldTick < 0 ||
        !Resolver)
    {
        OutError =
            TEXT("Dispatch resolution request is invalid.");
        return false;
    }

    bool bFound = false;
    if (!Store.TryReadDispatch(
            DispatchId,
            bFound,
            OutDispatch,
            OutError))
    {
        return false;
    }

    if (!bFound)
    {
        OutError =
            TEXT("Dispatch does not exist.");
        return false;
    }

    if (OutDispatch.Status !=
        EOGDispatchStatus::Active)
    {
        OutError =
            TEXT("Dispatch is not active.");
        return false;
    }

    if (CurrentWorldTick <
        OutDispatch.ResolveWorldTick)
    {
        OutError =
            TEXT("Dispatch has not reached its resolution tick.");
        return false;
    }

    TArray<FOGDispatchObjectiveRecord> Objectives;
    TArray<FOGDispatchConstraintRecord> Constraints;
    if (!Store.ListDispatchObjectives(
            DispatchId,
            Objectives,
            OutError) ||
        !Store.ListDispatchConstraints(
            DispatchId,
            Constraints,
            OutError))
    {
        return false;
    }

    FOGDispatchResolutionContext Context;
    Context.RiskToleranceBps =
        OutDispatch.RiskToleranceBps;
    Context.AbortPolicyJson =
        OutDispatch.AbortPolicyJson;
    Context.HardConstraints =
        Constraints;

    for (const FOGDispatchObjectiveRecord& Objective :
         Objectives)
    {
        if (Objective.bMandatory)
        {
            Context.MandatoryObjectives.Add(
                Objective);
        }
        else
        {
            Context.SecondaryObjectives.Add(
                Objective);
        }
    }

    FOGDispatchResolutionResult Resolution;
    if (!Resolver(
            Context,
            Resolution,
            OutError))
    {
        return false;
    }

    if (!Resolution.bMandatoryObjectivesEvaluated ||
        !Resolution.bHardConstraintsEvaluated ||
        !Resolution.bSurvivalAbortPolicyEvaluated)
    {
        OutError =
            TEXT("Dispatch resolver violated mandatory objective/constraint/abort evaluation order.");
        return false;
    }

    if (Resolution.OutcomeState.IsNone())
    {
        OutError =
            TEXT("Dispatch resolver must provide an explicit outcome state.");
        return false;
    }

    if (Resolution.bDelay)
    {
        if (Resolution.DelayUntilWorldTick <=
            CurrentWorldTick)
        {
            OutError =
                TEXT("Delayed Dispatch must move to a future world tick.");
            return false;
        }

        OutDispatch.bHasDelayUntilWorldTick =
            true;
        OutDispatch.DelayUntilWorldTick =
            Resolution.DelayUntilWorldTick;
        OutDispatch.ResolveWorldTick =
            Resolution.DelayUntilWorldTick;
        OutDispatch.OutcomeState =
            Resolution.OutcomeState;
        OutDispatch.ResultJson =
            Resolution.ResultJson.IsEmpty()
                ? TEXT("{}")
                : Resolution.ResultJson;

        return Store.UpsertDispatch(
            OutDispatch,
            OutDispatch.StartWorldTick,
            OutError);
    }

    OutDispatch.Status =
        Resolution.bSucceeded
            ? EOGDispatchStatus::Succeeded
            : EOGDispatchStatus::Failed;
    OutDispatch.OutcomeState =
        Resolution.OutcomeState;
    OutDispatch.bHasDelayUntilWorldTick =
        false;
    OutDispatch.DelayUntilWorldTick = 0;
    OutDispatch.ResultJson =
        Resolution.ResultJson.IsEmpty()
            ? TEXT("{}")
            : Resolution.ResultJson;

    return Store.UpsertDispatch(
        OutDispatch,
        OutDispatch.StartWorldTick,
        OutError);
}
