#pragma once

#include "CoreMinimal.h"
#include "Effects/OGEffectDefinition.h"
#include "Effects/OGTriggerDefinition.h"

class OFFLINEGAME_API FOGEffectValidator
{
public:
    static bool Validate(
        const FOGEffectDefinition& Effect,
        TArray<FString>& OutErrors);

    static bool Validate(
        const FOGTriggerDefinition& Trigger,
        TArray<FString>& OutErrors);
};
