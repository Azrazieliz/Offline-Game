#pragma once

#include "CoreMinimal.h"
#include "Persistence/OGWorldStore.h"
#include "Progression/OGProgressionRecords.h"

/**
 * Factor persistence/orchestration.
 *
 * No Factor is equipped/unequipped. Expression weight is an emphasis signal
 * only; ListOwnedFactors always returns every causal Factor instance.
 */
class OFFLINEGAME_API FOGFactorService
{
public:
    explicit FOGFactorService(IOGWorldStore& InStore)
        : Store(InStore)
    {
    }

    bool AcquireFactor(
        FOGFactorInstanceRecord Factor,
        const TArray<FOGFactorLineageRecord>& ParentLineage,
        FString& OutError);

    bool SetExpressionWeight(
        const FOGEntityId& FactorInstanceId,
        int32 ExpressionWeightBps,
        int64 WorldTick,
        FString& OutError);

    bool ListOwnedFactors(
        const FOGEntityId& OwnerEntityId,
        TArray<FOGFactorInstanceRecord>& OutFactors,
        FString& OutError) const;

private:
    IOGWorldStore& Store;
};
