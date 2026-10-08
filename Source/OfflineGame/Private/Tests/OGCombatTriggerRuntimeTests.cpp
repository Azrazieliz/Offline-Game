#include "Combat/OGCombatTriggerRuntime.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOGCombatTriggerOrderingTest,
    "OfflineGame.Combat.Triggers.DeterministicOrdering",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FOGCombatTriggerOrderingTest::RunTest(
    const FString& Parameters)
{
    const FOGEntityId Owner = FOGEntityId::NewId();

    FOGCombatTriggerBinding LaterLexical;
    LaterLexical.OwnerUnitId = Owner;
    LaterLexical.Trigger.TriggerId =
        FOGContentId(TEXT("test:trigger.zeta"));
    LaterLexical.Trigger.EventType =
        OGCombatEventNames::Defeat();
    LaterLexical.Trigger.ActionIds.Add(
        FOGContentId(TEXT("test:action.zeta")));

    FOGCombatTriggerBinding EarlierLexical;
    EarlierLexical.OwnerUnitId = Owner;
    EarlierLexical.Trigger.TriggerId =
        FOGContentId(TEXT("test:trigger.alpha"));
    EarlierLexical.Trigger.EventType =
        OGCombatEventNames::Defeat();
    EarlierLexical.Trigger.ActionIds.Add(
        FOGContentId(TEXT("test:action.alpha")));

    FOGCombatTriggerRuntime Runtime;
    Runtime.SetBindings(
        {LaterLexical, EarlierLexical});

    FOGCombatTriggerContext Context;
    Context.EventType = OGCombatEventNames::Defeat();
    Context.TargetUnitId = Owner;
    Context.SourceSequence = 42;

    Runtime.QueueForEvent(Context);
    const TArray<FOGQueuedTriggeredAction> Queued =
        Runtime.DrainQueuedActions();

    TestEqual(
        TEXT("Two matching trigger actions queue"),
        Queued.Num(),
        2);

    TestTrue(
        TEXT("Stable lexical trigger order is deterministic"),
        Queued[0].TriggerId ==
            FOGContentId(TEXT("test:trigger.alpha")));

    TestEqual(
        TEXT("Event provenance sequence is retained"),
        Queued[0].SourceSequence,
        static_cast<int64>(42));

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOGCombatTriggerConditionTest,
    "OfflineGame.Combat.Triggers.ConditionEvaluator",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FOGCombatTriggerConditionTest::RunTest(
    const FString& Parameters)
{
    const FOGEntityId Owner = FOGEntityId::NewId();

    FOGCombatTriggerBinding Binding;
    Binding.OwnerUnitId = Owner;
    Binding.Trigger.TriggerId =
        FOGContentId(TEXT("test:trigger.owner_defeated"));
    Binding.Trigger.EventType =
        OGCombatEventNames::Defeat();
    Binding.Trigger.ActionIds.Add(
        FOGContentId(TEXT("test:action.revive")));

    FOGConditionDefinition Condition;
    Condition.Predicate = TEXT("OwnerIsTarget");
    Binding.Trigger.Conditions.Add(Condition);

    FOGCombatTriggerRuntime Runtime;
    Runtime.SetBindings({Binding});
    Runtime.SetConditionEvaluator(
        [](const FOGCombatTriggerBinding& Candidate,
           const FOGConditionDefinition& Requirement,
           const FOGCombatTriggerContext& Context)
        {
            if (Requirement.Predicate ==
                FName(TEXT("OwnerIsTarget")))
            {
                return Candidate.OwnerUnitId ==
                       Context.TargetUnitId;
            }

            return false;
        });

    FOGCombatTriggerContext Context;
    Context.EventType = OGCombatEventNames::Defeat();
    Context.TargetUnitId = Owner;

    Runtime.QueueForEvent(Context);

    TestEqual(
        TEXT("Matching conditional defeat passive queues"),
        Runtime.DrainQueuedActions().Num(),
        1);

    return true;
}

#endif
