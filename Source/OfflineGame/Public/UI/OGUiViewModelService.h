#pragma once

#include "CoreMinimal.h"
#include "Gacha/OGGachaDefinitions.h"
#include "Persistence/OGWorldStore.h"
#include "UI/OGUiViewModels.h"
#include "World/OGWorldStateRecords.h"

/**
 * Read-only player-facing projections over canonical world state.
 *
 * These view models never become a second source of truth. Unknown information
 * stays Unknown, formulas stay internal, and presentation preferences remain
 * outside world causality.
 */
class OFFLINEGAME_API FOGUiViewModelService
{
public:
    explicit FOGUiViewModelService(IOGWorldStore& InStore)
        : Store(InStore)
    {
    }

    bool BuildRulerShell(
        const FOGEntityId& RulerId,
        FOGRulerShellViewModel& OutViewModel,
        FString& OutError) const;

    bool BuildRoster(
        const FOGEntityId& RulerId,
        TArray<FOGRosterIdentityViewModel>& OutRoster,
        FString& OutError) const;

    bool BuildCharacterIdentity(
        const FOGEntityId& RulerId,
        const FOGCharacterIdentityDefinition& Identity,
        FOGCharacterIdentityViewModel& OutViewModel,
        FString& OutError) const;

    bool BuildGacha(
        const FOGEntityId& RulerId,
        const FOGGachaBannerDefinition& Banner,
        FOGGachaViewModel& OutViewModel,
        FString& OutError) const;

    bool BuildTerritory(
        const FOGEntityId& RulerId,
        const FOGEntityId& SelectedTerritoryId,
        FOGTerritoryViewModel& OutViewModel,
        FString& OutError) const;

    bool BuildRecordsHub(
        const FOGEntityId& RulerId,
        FOGRecordsHubViewModel& OutViewModel,
        FString& OutError) const;

    bool BuildWorldHud(
        const FOGEntityId& KnowledgeOwnerId,
        const FOGEntityId& ControlledEntityId,
        const TArray<FOGEntityId>& CompanionIds,
        const TSet<FOGEntityId>& QteReadyIds,
        const FOGKnowledgeFactRecord* TargetConditionFact,
        bool bTargetConditionOutdated,
        FOGWorldHudViewModel& OutViewModel,
        FString& OutError) const;

    static FOGKnowledgeFactViewModel ProjectKnowledgeFact(
        const FOGKnowledgeFactRecord* Fact,
        bool bOutdated);

    static FOGOpeningViewModel BuildOpening(
        bool bWorldExists,
        bool bRecoverableWorldExists,
        bool bImportBackupAvailable,
        bool bReducedMotionPresentation);

    static FOGContentId CharacterLastUsedContextId(
        const FOGContentId& IdentityId);

private:
    IOGWorldStore& Store;
};
