#include "Skills/OGSkillReadiness.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOGSkillResourceReadinessTest,
    "OfflineGame.Combat.SkillReadiness.PersonalResource",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FOGSkillResourceReadinessTest::RunTest(const FString& Parameters)
{
    FOGSkillActivationDefinition Definition;
    Definition.SkillId = FOGContentId(TEXT("test:skill.ultimate_tier_2"));

    FOGSkillReadinessRequirement Requirement;
    Requirement.Kind = EOGReadinessRequirementKind::ResourceAtLeast;
    Requirement.ResourceKey = TEXT("ultimate_charge");
    Requirement.IntegerOperand = 200;
    Definition.Requirements.Add(Requirement);

    FOGSkillCostDefinition Cost;
    Cost.Kind = EOGSkillCostKind::PersonalResource;
    Cost.ResourceKey = TEXT("ultimate_charge");
    Cost.Amount = 200;
    Definition.Costs.Add(Cost);

    FOGSkillRuntimeState SkillState;
    SkillState.SkillId = Definition.SkillId;

    FOGSkillUserRuntimeState UserState;
    UserState.PersonalResources.Add(TEXT("ultimate_charge"), 275);
    UserState.CurrentHp = FOGLargeNumber::FromInt64(1000);
    UserState.MaxHp = FOGLargeNumber::FromInt64(1000);

    const FOGSkillReadinessResult Result =
        FOGSkillReadiness::Evaluate(
            Definition,
            SkillState,
            UserState);

    TestTrue(TEXT("200%-tier Ultimate is ready at 275 charge"), Result.bReady);

    int64 FutureActionValue = 0;
    FString Error;
    TestTrue(
        TEXT("Costs apply"),
        FOGSkillReadiness::ApplyCosts(
            Definition,
            SkillState,
            UserState,
            FutureActionValue,
            Error));

    TestEqual(
        TEXT("Selected lower tier consumes only its own charge"),
        UserState.PersonalResources.FindRef(TEXT("ultimate_charge")),
        static_cast<int64>(75));

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOGSkillCooldownIsExplicitTest,
    "OfflineGame.Combat.SkillReadiness.CooldownIsExplicitNotUniversal",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FOGSkillCooldownIsExplicitTest::RunTest(const FString& Parameters)
{
    FOGSkillActivationDefinition Definition;
    Definition.SkillId = FOGContentId(TEXT("test:skill.cooldown"));

    FOGSkillReadinessRequirement ReadyRequirement;
    ReadyRequirement.Kind = EOGReadinessRequirementKind::ReadyAtActionValue;
    Definition.Requirements.Add(ReadyRequirement);

    Definition.CooldownActionValue = 500;

    FOGSkillRuntimeState SkillState;
    SkillState.SkillId = Definition.SkillId;
    SkillState.ReadyAtActionValue = 0;

    FOGSkillUserRuntimeState UserState;
    UserState.CurrentActionValue = 1000;
    UserState.CurrentHp = FOGLargeNumber::FromInt64(1000);
    UserState.MaxHp = FOGLargeNumber::FromInt64(1000);

    TestTrue(
        TEXT("Skill begins ready"),
        FOGSkillReadiness::Evaluate(
            Definition,
            SkillState,
            UserState).bReady);

    int64 FutureActionValue = 1000;
    FString Error;
    TestTrue(
        TEXT("Use succeeds"),
        FOGSkillReadiness::ApplyCosts(
            Definition,
            SkillState,
            UserState,
            FutureActionValue,
            Error));

    UserState.CurrentActionValue = 1200;
    TestFalse(
        TEXT("Explicit cooldown can make this specific skill unavailable"),
        FOGSkillReadiness::Evaluate(
            Definition,
            SkillState,
            UserState).bReady);

    UserState.CurrentActionValue = 1500;
    TestTrue(
        TEXT("Skill becomes ready at its authored action-value point"),
        FOGSkillReadiness::Evaluate(
            Definition,
            SkillState,
            UserState).bReady);

    return true;
}

#endif
