#pragma once

#include "CoreMinimal.h"
#include "Persistence/OGWorldStore.h"
#include "World/OGDispatchFactionWarRecords.h"

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

private:
    static void CanonicalizeAlliancePair(
        FOGEntityId& InOutA,
        FOGEntityId& InOutB);

    IOGWorldStore& Store;
};
