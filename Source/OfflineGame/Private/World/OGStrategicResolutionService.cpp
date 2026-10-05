#include "World/OGStrategicResolutionService.h"

bool FOGStrategicResolutionService::SetArmyCapability(
    const FOGArmyCapabilityRecord& Capability,
    FString& OutError)
{
    return Store.UpsertArmyCapability(
        Capability,
        OutError);
}

bool FOGStrategicResolutionService::ResolveArmyInteraction(
    const TArray<FOGEntityId>& ArmyIds,
    int64 ResolutionSeed,
    const FOGArmyCapabilityResolver& Resolver,
    FOGStrategicArmyResolutionResult& OutResult,
    FString& OutError) const
{
    OutResult =
        FOGStrategicArmyResolutionResult();
    OutError.Reset();

    if (ArmyIds.Num() < 2 ||
        !Resolver)
    {
        OutError =
            TEXT("Strategic Army resolution requires at least two Armies and a resolver.");
        return false;
    }

    TSet<FOGEntityId> UniqueArmyIds;
    TArray<FOGArmyResolutionInput> Inputs;
    Inputs.Reserve(
        ArmyIds.Num());

    for (const FOGEntityId& ArmyId :
         ArmyIds)
    {
        if (!ArmyId.IsValid() ||
            UniqueArmyIds.Contains(
                ArmyId))
        {
            OutError =
                TEXT("Strategic Army resolution contains invalid or duplicate Army IDs.");
            return false;
        }

        UniqueArmyIds.Add(
            ArmyId);

        bool bFound = false;
        FOGArmyResolutionInput Input;
        if (!Store.TryReadArmy(
                ArmyId,
                bFound,
                Input.Army,
                OutError))
        {
            return false;
        }

        if (!bFound)
        {
            OutError =
                TEXT("Strategic Army resolution references an unknown Army.");
            return false;
        }

        if (!Store.ListArmyCapabilities(
                ArmyId,
                Input.Capabilities,
                OutError))
        {
            return false;
        }

        if (Input.Capabilities.IsEmpty())
        {
            OutError =
                TEXT("Army has no normalized capability vectors; cached EffectivePower cannot resolve strategy alone.");
            return false;
        }

        Inputs.Add(
            MoveTemp(Input));
    }

    if (!Resolver(
            Inputs,
            ResolutionSeed,
            OutResult,
            OutError))
    {
        return false;
    }

    if (OutResult.Outcome.IsNone())
    {
        OutError =
            TEXT("Strategic Army resolver must return an explicit outcome.");
        return false;
    }

    return true;
}
