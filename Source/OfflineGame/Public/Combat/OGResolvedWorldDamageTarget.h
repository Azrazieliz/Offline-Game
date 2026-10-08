#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Combat/OGCombatTypes.h"
#include "OGResolvedWorldDamageTarget.generated.h"

/** Canonical combat resolves the damage; the target owns its persisted outcome. */
UINTERFACE(MinimalAPI, meta=(CannotImplementInterfaceInBlueprint))
class UOGResolvedWorldDamageTarget : public UInterface
{
    GENERATED_BODY()
};

class OFFLINEGAME_API IOGResolvedWorldDamageTarget
{
    GENERATED_BODY()
public:
    virtual bool ReadResolvedWorldCombatSnapshot(FOGCombatUnitState& Out, FString& Error) const = 0;
    virtual bool CanReceiveResolvedWorldDamage(AActor* SourceActor) const = 0;
    virtual bool ApplyResolvedWorldDamage(const FOGLargeNumber& Damage, AActor* SourceActor, FString& Error) = 0;
};
