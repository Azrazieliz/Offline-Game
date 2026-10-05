#include "Progression/OGCharacterProgressionService.h"

bool FOGCharacterProgressionService::ValidateManifestationExists(
    const FOGEntityId& ManifestationId,
    FString& OutError) const
{
    bool bFound = false;
    FOGCharacterManifestationRecord Manifestation;
    if (!Store.TryReadCharacterManifestation(
            ManifestationId,
            bFound,
            Manifestation,
            OutError))
    {
        return false;
    }

    if (!bFound)
    {
        OutError =
            TEXT("Character progression references an unknown Manifestation.");
        return false;
    }

    return true;
}

bool FOGCharacterProgressionService::LearnSkill(
    const FOGEntitySkillRecord& Skill,
    const TArray<FOGSkillProvenanceRecord>& Provenance,
    FString& OutError)
{
    OutError.Reset();

    if (!Skill.OwnerEntityId.IsValid() ||
        !Skill.SkillId.IsValid())
    {
        OutError =
            TEXT("Skill-learning request is invalid.");
        return false;
    }

    if (!Store.BeginTransaction(
            OutError))
    {
        return false;
    }

    if (!Store.UpsertEntitySkill(
            Skill,
            OutError))
    {
        FString RollbackError;
        Store.RollbackTransaction(
            RollbackError);
        return false;
    }

    for (const FOGSkillProvenanceRecord& Source :
         Provenance)
    {
        if (Source.OwnerEntityId !=
                Skill.OwnerEntityId ||
            Source.SkillId !=
                Skill.SkillId)
        {
            OutError =
                TEXT("Skill provenance does not match learned skill owner/identity.");
            FString RollbackError;
            Store.RollbackTransaction(
                RollbackError);
            return false;
        }

        if (!Store.UpsertSkillProvenance(
                Source,
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

    return true;
}

bool FOGCharacterProgressionService::SetRouteNode(
    const FOGManifestationRouteNodeRecord& Node,
    FString& OutError)
{
    if (!ValidateManifestationExists(
            Node.ManifestationId,
            OutError))
    {
        return false;
    }

    return Store.UpsertManifestationRouteNode(
        Node,
        OutError);
}

bool FOGCharacterProgressionService::UnlockOrUpdateForm(
    const FOGManifestationFormRecord& Form,
    FString& OutError)
{
    if (!ValidateManifestationExists(
            Form.ManifestationId,
            OutError))
    {
        return false;
    }

    return Store.UpsertManifestationForm(
        Form,
        OutError);
}

bool FOGCharacterProgressionService::SetReinforcement(
    const FOGManifestationReinforcementRecord& Reinforcement,
    FString& OutError)
{
    if (!ValidateManifestationExists(
            Reinforcement.ManifestationId,
            OutError))
    {
        return false;
    }

    return Store.UpsertManifestationReinforcement(
        Reinforcement,
        OutError);
}
