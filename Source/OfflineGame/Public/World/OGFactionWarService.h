#pragma once

#include "CoreMinimal.h"
#include "Persistence/OGWorldStore.h"
#include "World/OGDispatchFactionWarRecords.h"
#include "World/OGStrategyExpansionRecords.h"

class OFFLINEGAME_API FOGFactionWarService
{
public:
    explicit FOGFactionWarService(IOGWorldStore& InStore)
        : Store(InStore)
    {
    }

    bool FormAlliance(
        const FOGEntityId& FactionA,
        const FOGEntityId& FactionB,
        int64 WorldTick,
        FString& OutError);

    bool BreakAlliance(
        const FOGEntityId& FactionA,
        const FOGEntityId& FactionB,
        int64 WorldTick,
        FString& OutError);

    bool SetSupport(
        const FOGEntityId& SupportingFaction,
        const FOGEntityId& SupportedFaction,
        bool bActive,
        int64 WorldTick,
        FString& OutError);

    bool SetSubordination(
        const FOGEntityId& SubordinateFaction,
        const FOGEntityId& OverlordFaction,
        bool bActive,
        int64 WorldTick,
        FString& OutError);

    /**
     * No Casus Belli or treaty prerequisite. If the factions were allied, the
     * alliance is explicitly broken as part of the same transaction.
     */
    bool DeclareWar(
        const FOGEntityId& AttackerFaction,
        const FOGEntityId& DefenderFaction,
        FName ObjectiveType,
        const FOGEntityId& ObjectiveTargetEntityId,
        int64 WorldTick,
        FOGEntityId& OutWarId,
        FString& OutError);

    /**
     * Called only after actual strategic/world resolution. This is not a
     * universal "negotiate peace now" command.
     */
    bool ResolveWar(
        const FOGEntityId& WarId,
        EOGWarStatus FinalStatus,
        int64 WorldTick,
        const FString& ResolutionJson,
        FString& OutError);

    bool CreateWarFront(
        const FOGEntityId& WarId,
        const FOGEntityId& LocationId,
        const FOGEntityId& RealityId,
        int64 WorldTick,
        const FString& StateJson,
        FOGEntityId& OutFrontId,
        FString& OutError);

    bool SetWarObjective(
        const FOGWarObjectiveRecord& Objective,
        FString& OutError);

    bool IssueWarOrder(
        const FOGEntityId& WarId,
        const FOGEntityId& FrontId,
        const FOGEntityId& IssuerEntityId,
        const FOGEntityId& RecipientEntityId,
        const FOGContentId& IntentId,
        const FString& ConstraintsJson,
        int64 WorldTick,
        FOGEntityId& OutOrderId,
        FString& OutError);

    /**
     * Outcomes such as obeyed / reinterpreted / delayed / refused / disobeyed
     * are recorded explicitly. The original order intent/constraints remain.
     */
    bool RecordWarOrderOutcome(
        const FOGEntityId& OrderId,
        FName OutcomeState,
        const FString& OutcomeJson,
        int64 WorldTick,
        FString& OutError);

    bool JoinWar(
        const FOGEntityId& WarId,
        const FOGEntityId& FactionId,
        int32 SideIndex,
        bool bPrimary,
        int64 WorldTick,
        FString& OutError);

    bool LeaveWar(
        const FOGEntityId& WarId,
        const FOGEntityId& FactionId,
        FName Reason,
        int64 WorldTick,
        FString& OutError);

private:
    static void CanonicalizeAlliancePair(
        FOGEntityId& InOutA,
        FOGEntityId& InOutB);

    IOGWorldStore& Store;
};
