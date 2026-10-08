#include "Effects/OGEffectValidator.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOGEffectValidationTest,
    "OfflineGame.Rules.Effects.Validation",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FOGEffectValidationTest::RunTest(const FString& Parameters)
{
    FOGEffectDefinition Effect;
    Effect.EffectId = FOGContentId(TEXT("test:effect.burn"));
    Effect.FamilyId = FOGContentId(TEXT("test:effect_family.burn"));
    Effect.MechanicKind = TEXT("damage_over_time");
    Effect.MaxStacks = 10;
    Effect.Lifetime.Kind = EOGEffectLifetimeKind::Turns;
    Effect.Lifetime.Magnitude = 3;
    Effect.RulePriority.SourceRuleId = Effect.EffectId;

    TArray<FString> Errors;
    TestTrue(
        TEXT("Mechanically explicit effect validates"),
        FOGEffectValidator::Validate(Effect, Errors));

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOGTriggerValidationTest,
    "OfflineGame.Rules.Triggers.Validation",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FOGTriggerValidationTest::RunTest(const FString& Parameters)
{
    FOGTriggerDefinition Trigger;
    Trigger.TriggerId = FOGContentId(TEXT("test:trigger.counter"));
    Trigger.EventType = TEXT("OnDamageReceived");
    Trigger.ActionIds.Add(FOGContentId(TEXT("test:action.counter_attack")));

    FOGConditionDefinition Condition;
    Condition.Predicate = TEXT("SourceIsHostile");
    Trigger.Conditions.Add(Condition);

    TArray<FString> Errors;
    TestTrue(
        TEXT("Automatic trigger validates"),
        FOGEffectValidator::Validate(Trigger, Errors));

    return true;
}

#endif
