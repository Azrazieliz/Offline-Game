#pragma once

#include "CoreMinimal.h"
#include "Rules/OGRulePriority.h"

/**
 * Stateless deterministic selector for conflicting rule claims.
 *
 * Mechanic-specific systems decide what a claim means; this core only decides
 * precedence. That keeps immunity, bypass, cap, resurrection, Domain and other
 * rule conflicts on one ordering model instead of Blueprint execution order.
 */
class OFFLINEGAME_API FOGRuleResolver
{
public:
    /** Returns INDEX_NONE for an empty input. */
    static int32 SelectWinningIndex(
        const TArray<FOGRulePriority>& Priorities);
};
