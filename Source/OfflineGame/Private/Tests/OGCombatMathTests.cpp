#include "Combat/OGCombatMath.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOGCritResistanceOverflowTest,
    "OfflineGame.Combat.Math.CritResistanceThenOverflow",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FOGCritResistanceOverflowTest::RunTest(const FString& Parameters)
{
    FOGCombatStats Attacker;
    Attacker.CritRateBps = 13000;
    Attacker.CritDamageBonusBps = 5000;

    FOGCombatStats Defender;
    Defender.CritRateResistanceBps = 2000;

    FOGDeterministicRng Rng(1);
    const FOGCritResolution Result =
        FOGCombatMath::ResolveCrit(
            Attacker,
            Defender,
            Rng);

    TestEqual(
        TEXT("Resistance is applied before overflow"),
        Result.EffectiveCritRateBps,
        11000);

    TestEqual(
        TEXT("Crit chance is capped at 100%"),
        Result.CritChanceBps,
        10000);

    TestEqual(
        TEXT("Only post-resistance excess becomes overflow"),
        Result.OverflowCritRateBps,
        1000);

    TestEqual(
        TEXT("1 Crit Rate overflow point becomes 2 Crit Damage points"),
        Result.OverflowCritDamageBonusBps,
        2000);

    TestEqual(
        TEXT("Base bonus plus overflow is preserved"),
        Result.EffectiveCritDamageBonusBps,
        7000);

    TestTrue(
        TEXT("100% effective crit chance always crits"),
        Result.bCritical);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOGHitOverflowReplicationTest,
    "OfflineGame.Combat.Math.HitOverflowReplication",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FOGHitOverflowReplicationTest::RunTest(const FString& Parameters)
{
    FOGCombatStats Attacker;
    Attacker.HitBps = 25000;

    FOGCombatStats Defender;
    Defender.DodgeBps = 5000;

    FOGDeterministicRng Rng(5);
    const FOGHitResolution Result =
        FOGCombatMath::ResolveHit(
            Attacker,
            Defender,
            true,
            Rng);

    TestEqual(
        TEXT("Hit subtracts Dodge first"),
        Result.EffectiveHitBps,
        20000);

    TestEqual(
        TEXT("Two full 100% bands create two guaranteed hit instances"),
        Result.GuaranteedHitInstances,
        2);

    TestEqual(
        TEXT("No remainder remains"),
        Result.OverflowRemainderBps,
        0);

    TestEqual(
        TEXT("Exactly two hit instances resolve"),
        Result.ResolvedHitInstances,
        2);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOGIndivisibleHitNoReplicationTest,
    "OfflineGame.Combat.Math.IndivisibleHitDoesNotReplicate",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FOGIndivisibleHitNoReplicationTest::RunTest(const FString& Parameters)
{
    FOGCombatStats Attacker;
    Attacker.HitBps = 30000;

    FOGCombatStats Defender;
    Defender.DodgeBps = 0;

    FOGDeterministicRng Rng(9);
    const FOGHitResolution Result =
        FOGCombatMath::ResolveHit(
            Attacker,
            Defender,
            false,
            Rng);

    TestEqual(
        TEXT("Indivisible strike resolves only once"),
        Result.ResolvedHitInstances,
        1);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOGTrueDamageBypassTest,
    "OfflineGame.Combat.Math.TrueDamageBypassesDefenseReductionAndBlock",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FOGTrueDamageBypassTest::RunTest(const FString& Parameters)
{
    FOGCombatStats Attacker;
    Attacker.HitBps = 10000;
    Attacker.CritRateBps = 0;
    Attacker.DamageDealtMultiplierBps = 10000;

    FOGCombatStats Defender;
    Defender.Defense = FOGLargeNumber::FromInt64(999999999);
    Defender.BlockRateBps = 10000;
    Defender.BlockReductionBps = 9000;
    Defender.DamageTakenMultiplierBps = 1000;
    Defender.TrueDamageResistanceBps = 0;

    FOGDamageRequest Request;
    Request.BaseDamage = FOGLargeNumber::FromInt64(1000);
    Request.DefenseReference = FOGLargeNumber::FromInt64(100);
    Request.DamageType = EOGBaseDamageType::TrueDamage;
    Request.bCanCrit = false;
    Request.bCanBeBlocked = true;
    Request.bAllowHitOverflowReplication = false;

    FOGDeterministicRng Rng(3);
    const FOGDamageResolution Result =
        FOGCombatMath::ResolveDamage(
            Request,
            Attacker,
            Defender,
            Rng);

    TestTrue(
        TEXT("True Damage keeps its base amount when TDR is zero"),
        Result.TotalDamage == FOGLargeNumber::FromInt64(1000));

    TestFalse(
        TEXT("True Damage is not blocked by ordinary Block"),
        Result.Block.bBlocked);

    TestEqual(
        TEXT("True Damage bypasses DEF multiplier"),
        Result.DefenseMultiplierBps,
        10000);

    return true;
}

#endif
