#pragma once

#include "CoreMinimal.h"
#include "Persistence/OGWorldStore.h"

struct OFFLINEGAME_API FOGVerticalSliceScenarioResult
{
    bool bSucceeded = false;
    FString Error;
    FString BattleFingerprint;
    FOGEntityId ManifestationId;
    FOGEntityId ProjectId;
};

/**
 * Deterministic architectural proof that connects the already-implemented
 * acquisition, shared-world, action-party, project and turn-combat foundations
 * through one authoritative SQLite history.
 *
 * It is developer/test infrastructure, not production content.
 */
class OFFLINEGAME_API FOGVerticalSliceScenarioHarness
{
public:
    static bool RunFresh(
        IOGWorldStore& Store,
        FOGVerticalSliceScenarioResult& OutResult,
        FString& OutError);

    static bool VerifyAfterRestart(
        IOGWorldStore& Store,
        FOGVerticalSliceScenarioResult& OutResult,
        FString& OutError);

    static FOGEntityId ScenarioRulerId();
    static FOGEntityId ScenarioLocationId();
    static FOGEntityId ScenarioTerritoryId();
    static FOGEntityId ScenarioCoreId();
    static FOGEntityId ScenarioCheckpointId();
};
