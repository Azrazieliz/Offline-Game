#pragma once

#include "CoreMinimal.h"
#include "Persistence/OGWorldStore.h"

struct OFFLINEGAME_API FOGVerticalSliceScenarioResult
{
    bool bSucceeded = false;
    FString Error;

    FString TurnBattleFingerprint;
    FString ActionDamageDisplay;

    FOGEntityId FirstManifestationId;
    FOGEntityId SecondManifestationId;
    FOGEntityId ProjectId;
    FOGEntityId DispatchId;
    FOGEntityId WarId;
    FOGEntityId WarFrontId;
    FOGEntityId ActionCombatEventId;
    FOGEntityId ReportId;
};

/**
 * Reconciled Vertical Slice 0 architectural proof.
 *
 * This is deterministic developer/test infrastructure, not production content.
 * It proves one canonical history across qualification, repeated full
 * Manifestations, portrait Ruler projections, Territory loss/reclamation and
 * anchoring, landscape World Mode, action + turn combat, Domain consequences,
 * objective-faithful Dispatch, Project, War, package validation and restart.
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

    static FOGContentId ScenarioPackageId();
    static FOGContentId AcquiredIdentityId();
    static FOGContentId AcquiredVersionId();
};

