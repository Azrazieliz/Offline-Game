#pragma once

#include "CoreMinimal.h"
#include "Persistence/OGWorldStore.h"
#include "World/OGRealityTimeRecords.h"

struct FOGWorldDirectorScheduleRequest
{
    FOGContentId ContentId;
    FOGContentId TemplateId;
    int64 EligibleSinceWorldTick = 0;
    int64 EarliestStartWorldTick = 0;
    int64 LatestStartWorldTick = 0;
    int64 ResolutionSeed = 0;
    FString DecisionProvenanceJson = TEXT("{}");
};

struct FOGOfflineCatchupGovernorRequest
{
    FOGEntityId ScopeEntityId;
    int64 CatchupWorldTick = 0;
    int32 RequestedNewIrreversibleLossBps = 0;
    int32 MaxNewIrreversibleLossBps = 0;
    bool bCausallyLockedBeforeLogout = false;
    bool bWouldCauseProtectedCatastrophicCollapse = false;
    FString DecisionStateJson = TEXT("{}");
};

struct FOGOfflineCatchupGovernorResult
{
    bool bApproved = false;
    bool bDeferred = false;
    int32 ApprovedIrreversibleLossBps = 0;
};

/**
 * Bounded deterministic World Director.
 *
 * All windows, calendars and safety-ceiling values are supplied by content or
 * tuning. The Director never invents a global cadence or promotes the current
 * tuning candidate into a C++ constant.
 */
class OFFLINEGAME_API FOGWorldDirectorService
{
public:
    explicit FOGWorldDirectorService(IOGWorldStore& InStore)
        : Store(InStore)
    {
    }

    bool ScheduleEligibleContent(
        const FOGWorldDirectorScheduleRequest& Request,
        FOGWorldDirectorScheduleRecord& OutSchedule,
        FString& OutError);

    bool DelayScheduleWithinAuthoredBound(
        const FOGEntityId& ScheduleId,
        int64 NewStartWorldTick,
        int64 DecisionWorldTick,
        const FString& DecisionProvenanceJson,
        FString& OutError);

    bool ActivateDueContent(
        const FOGEntityId& ScheduleId,
        int64 ObservedWorldTick,
        FString& OutError);

    bool CompleteSchedule(
        const FOGEntityId& ScheduleId,
        int64 WorldTick,
        const FString& OutcomeJson,
        FString& OutError);

    bool SetContentUnlockState(
        const FOGContentUnlockStateRecord& State,
        FString& OutError);

    bool RecordActiveSessionBoundary(
        const FOGEntityId& ScopeEntityId,
        int64 WorldTick,
        FString& OutError);

    bool EvaluateOfflineCatchupGovernor(
        const FOGOfflineCatchupGovernorRequest& Request,
        FOGOfflineCatchupGovernorResult& OutResult,
        FString& OutError);

private:
    static bool ValidateDecisionProvenance(
        const FString& Json,
        bool bComposedTemplate,
        FString& OutError);

    static bool ValidateJsonObject(
        const FString& Json,
        FString& OutError);

    IOGWorldStore& Store;
};
