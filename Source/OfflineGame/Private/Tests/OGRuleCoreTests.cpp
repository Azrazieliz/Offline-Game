#include "Math/OGLargeNumber.h"
#include "Random/OGDeterministicRng.h"
#include "Rules/OGRulePriority.h"
#include "Rules/OGRuleResolver.h"
#include "Skills/OGResolvedSkillSet.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOGLargeNumberTest,
    "OfflineGame.Rules.LargeNumber.BasicArithmetic",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FOGLargeNumberTest::RunTest(const FString& Parameters)
{
    const FOGLargeNumber A = FOGLargeNumber::FromInt64(1500000000LL);
    const FOGLargeNumber B = FOGLargeNumber::FromInt64(2000000000LL);

    TestTrue(TEXT("A is normalized"), FMath::Abs(A.Significand) <= FOGLargeNumber::MaxSignificand);
    TestTrue(TEXT("B is normalized"), FMath::Abs(B.Significand) <= FOGLargeNumber::MaxSignificand);

    const FOGLargeNumber Sum = FOGLargeNumber::Add(A, B);
    TestTrue(
        TEXT("Sum is greater than each operand"),
        FOGLargeNumber::Compare(Sum, A) > 0 &&
        FOGLargeNumber::Compare(Sum, B) > 0);

    const FOGLargeNumber Product = FOGLargeNumber::Multiply(A, B);
    TestTrue(
        TEXT("Product is greater than operands"),
        FOGLargeNumber::Compare(Product, A) > 0 &&
        FOGLargeNumber::Compare(Product, B) > 0);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOGDeterministicRngTest,
    "OfflineGame.Rules.Rng.Reproducible",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FOGDeterministicRngTest::RunTest(const FString& Parameters)
{
    FOGDeterministicRng First(123456789ull);
    FOGDeterministicRng Second(123456789ull);

    for (int32 Index = 0; Index < 100; ++Index)
    {
        TestEqual(
            TEXT("Same seed produces same sequence"),
            First.NextUInt64(),
            Second.NextUInt64());
    }

    TestEqual(TEXT("Draw count is recorded"), First.GetDrawCount(), static_cast<uint64>(100));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOGRulePriorityTest,
    "OfflineGame.Rules.Priority.AuthorityThenSpecificity",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FOGRulePriorityTest::RunTest(const FString& Parameters)
{
    FOGRulePriority HighAuthority;
    HighAuthority.Authority = 10;
    HighAuthority.Specificity = 0;
    HighAuthority.SourceRuleId = FOGContentId(TEXT("test:rule.high"));

    FOGRulePriority SpecificLowAuthority;
    SpecificLowAuthority.Authority = 9;
    SpecificLowAuthority.Specificity = 100;
    SpecificLowAuthority.SourceRuleId = FOGContentId(TEXT("test:rule.specific"));

    TestTrue(
        TEXT("Authority wins before specificity"),
        FOGRulePriority::Compare(
            HighAuthority,
            SpecificLowAuthority) > 0);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOGResolvedSkillSetDuplicateTest,
    "OfflineGame.Rules.SkillSet.RejectsDuplicates",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FOGResolvedSkillSetDuplicateTest::RunTest(const FString& Parameters)
{
    FOGResolvedSkillSet SkillSet;
    SkillSet.ActiveSkills.Add(FOGContentId(TEXT("test:skill.fire")));
    SkillSet.PassiveSkills.Add(FOGContentId(TEXT("test:skill.fire")));

    TestTrue(
        TEXT("Duplicate integrated skills are detectable"),
        SkillSet.HasDuplicateSkills());

    return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOGRuleResolverTest,
    "OfflineGame.Rules.Priority.ResolverSelectsHighestPriority",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FOGRuleResolverTest::RunTest(const FString& Parameters)
{
    TArray<FOGRulePriority> Claims;

    FOGRulePriority Low;
    Low.Authority = 1;
    Low.Specificity = 100;
    Low.SourceRuleId = FOGContentId(TEXT("test:rule.low"));
    Claims.Add(Low);

    FOGRulePriority High;
    High.Authority = 2;
    High.Specificity = 0;
    High.SourceRuleId = FOGContentId(TEXT("test:rule.high"));
    Claims.Add(High);

    TestEqual(
        TEXT("Resolver selects the higher-authority claim"),
        FOGRuleResolver::SelectWinningIndex(Claims),
        1);

    return true;
}

#endif
