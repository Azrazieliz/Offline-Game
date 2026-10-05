#pragma once

#include "CoreMinimal.h"
#include "Persistence/OGWorldStore.h"
#include "Progression/OGProgressionRecords.h"

/**
 * Queryable Manifestation development state.
 *
 * Route IDs are open-ended content data. Evolution/Awakening/Corruption are not
 * hard-coded mutually-exclusive enum branches.
 */
class OFFLINEGAME_API FOGCharacterProgressionService
{
public:
    explicit FOGCharacterProgressionService(IOGWorldStore& InStore)
        : Store(InStore)
    {
    }

    bool LearnSkill(
        const FOGEntitySkillRecord& Skill,
        const TArray<FOGSkillProvenanceRecord>& Provenance,
        FString& OutError);

    bool SetRouteNode(
        const FOGManifestationRouteNodeRecord& Node,
        FString& OutError);

    bool UnlockOrUpdateForm(
        const FOGManifestationFormRecord& Form,
        FString& OutError);

    bool SetReinforcement(
        const FOGManifestationReinforcementRecord& Reinforcement,
        FString& OutError);

private:
    bool ValidateManifestationExists(
        const FOGEntityId& ManifestationId,
        FString& OutError) const;

    IOGWorldStore& Store;
};
