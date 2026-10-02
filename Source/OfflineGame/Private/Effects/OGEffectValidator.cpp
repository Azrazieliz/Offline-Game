#include "Effects/OGEffectValidator.h"

bool FOGEffectValidator::Validate(
    const FOGEffectDefinition& Effect,
    TArray<FString>& OutErrors)
{
    OutErrors.Reset();

    if (!Effect.EffectId.IsValid())
    {
        OutErrors.Add(TEXT("EffectId is invalid."));
    }

    if (Effect.MechanicKind.IsNone())
    {
        OutErrors.Add(TEXT("MechanicKind is required."));
    }

    if (Effect.MaxStacks < 1)
    {
        OutErrors.Add(TEXT("MaxStacks must be at least 1."));
    }

    if ((Effect.Lifetime.Kind == EOGEffectLifetimeKind::Turns ||
         Effect.Lifetime.Kind == EOGEffectLifetimeKind::RealTimeMilliseconds) &&
        Effect.Lifetime.Magnitude <= 0)
    {
        OutErrors.Add(TEXT("Finite effect lifetime requires a positive magnitude."));
    }

    if (!Effect.RulePriority.SourceRuleId.IsValid())
    {
        OutErrors.Add(TEXT("RulePriority.SourceRuleId is invalid."));
    }

    return OutErrors.IsEmpty();
}

bool FOGEffectValidator::Validate(
    const FOGTriggerDefinition& Trigger,
    TArray<FString>& OutErrors)
{
    OutErrors.Reset();

    if (!Trigger.TriggerId.IsValid())
    {
        OutErrors.Add(TEXT("TriggerId is invalid."));
    }

    if (Trigger.EventType.IsNone())
    {
        OutErrors.Add(TEXT("EventType is required."));
    }

    if (Trigger.ActionIds.IsEmpty())
    {
        OutErrors.Add(TEXT("A trigger requires at least one action."));
    }

    TSet<FOGContentId> SeenActions;
    for (const FOGContentId& ActionId : Trigger.ActionIds)
    {
        if (!ActionId.IsValid())
        {
            OutErrors.Add(TEXT("Trigger contains an invalid ActionId."));
            continue;
        }

        if (SeenActions.Contains(ActionId))
        {
            OutErrors.Add(
                FString::Printf(
                    TEXT("Trigger contains duplicate ActionId %s."),
                    *ActionId.ToString()));
        }

        SeenActions.Add(ActionId);
    }

    for (const FOGConditionDefinition& Condition : Trigger.Conditions)
    {
        if (Condition.Predicate.IsNone())
        {
            OutErrors.Add(TEXT("Trigger contains a condition without a Predicate."));
        }
    }

    return OutErrors.IsEmpty();
}
