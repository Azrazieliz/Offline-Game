#pragma once

#include "CoreMinimal.h"
#include "Persistence/OGWorldStore.h"
#include "World/OGDispatchFactionWarRecords.h"
#include "World/OGStrategyExpansionRecords.h"

struct FOGArmyResolutionInput
{
    FOGArmyRecord Army;
    TArray<FOGArmyCapabilityRecord> Capabilities;
};

struct FOGStrategicArmyResolutionResult
{
    FName Outcome = NAME_None;
    FString OutcomeJson = TEXT("{}");
};

using FOGArmyCapabilityResolver =
    TFunction<bool(
        const TArray<FOGArmyResolutionInput>& Armies,
        int64 ResolutionSeed,
        FOGStrategicArmyResolutionResult& OutResult,
        FString& OutError)>;

/**
 * Shared strategic resolution entry point.
 *
 * EffectivePower is never sufficient authority for a strategic outcome.
 * Resolution consumes normalized capability vectors and may additionally use
 * named commander/entity state through the supplied content/system resolver.
 */
class OFFLINEGAME_API FOGStrategicResolutionService
{
public:
    explicit FOGStrategicResolutionService(IOGWorldStore& InStore)
        : Store(InStore)
    {
    }

    bool SetArmyCapability(
        const FOGArmyCapabilityRecord& Capability,
        FString& OutError);

    bool ResolveArmyInteraction(
        const TArray<FOGEntityId>& ArmyIds,
        int64 ResolutionSeed,
        const FOGArmyCapabilityResolver& Resolver,
        FOGStrategicArmyResolutionResult& OutResult,
        FString& OutError) const;

private:
    IOGWorldStore& Store;
};
