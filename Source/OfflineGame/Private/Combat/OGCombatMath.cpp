#include "Combat/OGCombatMath.h"

namespace
{
int32 ClampToInt32(int64 Value)
{
    return static_cast<int32>(
        FMath::Clamp<int64>(
            Value,
            static_cast<int64>(MIN_int32),
            static_cast<int64>(MAX_int32)));
}

int64 Pow10Int64(int32 Power)
{
    static constexpr int64 Values[] =
    {
        1LL,
        10LL,
        100LL,
        1000LL,
        10000LL,
        100000LL,
        1000000LL,
        10000000LL,
        100000000LL,
        1000000000LL
    };

    return Power >= 0 && Power <= 9 ? Values[Power] : 0;
}

int64 DivideRoundedNonNegative(
    int64 Value,
    int64 Divisor)
{
    if (Divisor <= 1)
    {
        return Value;
    }

    return (Value + Divisor / 2) / Divisor;
}
}

bool FOGCombatMath::RollBasisPoints(
    int32 ChanceBps,
    FOGDeterministicRng& Rng)
{
    if (ChanceBps <= 0)
    {
        return false;
    }

    if (ChanceBps >= 10000)
    {
        return true;
    }

    return Rng.NextRange(0, 10000) < ChanceBps;
}

FOGCritResolution FOGCombatMath::ResolveCrit(
    const FOGCombatStats& Attacker,
    const FOGCombatStats& Defender,
    FOGDeterministicRng& Rng)
{
    FOGCritResolution Result;

    // Settled order: opposing Crit Rate Resistance is deducted first.
    Result.EffectiveCritRateBps =
        FMath::Max(
            0,
            Attacker.CritRateBps -
            Defender.CritRateResistanceBps);

    Result.CritChanceBps =
        FMath::Min(Result.EffectiveCritRateBps, 10000);

    Result.OverflowCritRateBps =
        FMath::Max(
            0,
            Result.EffectiveCritRateBps - 10000);

    // Settled conversion: 1 Crit Rate point -> 2 Crit Damage points.
    Result.OverflowCritDamageBonusBps =
        ClampToInt32(
            static_cast<int64>(Result.OverflowCritRateBps) * 2LL);

    const int64 CritDamageAfterResistance =
        static_cast<int64>(Attacker.CritDamageBonusBps) -
        static_cast<int64>(Defender.CritDamageResistanceBps) +
        static_cast<int64>(Result.OverflowCritDamageBonusBps);

    // Crit Damage Resistance can erase the bonus, but ordinary baseline
    // critical resolution does not turn a crit into a damage penalty.
    Result.EffectiveCritDamageBonusBps =
        ClampToInt32(
            FMath::Max<int64>(0, CritDamageAfterResistance));

    Result.bCritical =
        RollBasisPoints(Result.CritChanceBps, Rng);

    return Result;
}

FOGHitResolution FOGCombatMath::ResolveHit(
    const FOGCombatStats& Attacker,
    const FOGCombatStats& Defender,
    bool bAllowOverflowReplication,
    FOGDeterministicRng& Rng)
{
    FOGHitResolution Result;

    Result.EffectiveHitBps =
        FMath::Max(
            0,
            Attacker.HitBps - Defender.DodgeBps);

    if (!bAllowOverflowReplication)
    {
        const int32 Chance =
            FMath::Min(Result.EffectiveHitBps, 10000);

        Result.GuaranteedHitInstances =
            Chance >= 10000 ? 1 : 0;
        Result.OverflowRemainderBps =
            Chance >= 10000 ? 0 : Chance;
        Result.ResolvedHitInstances =
            RollBasisPoints(Chance, Rng) ? 1 : 0;

        return Result;
    }

    Result.GuaranteedHitInstances =
        Result.EffectiveHitBps / 10000;

    Result.OverflowRemainderBps =
        Result.EffectiveHitBps % 10000;

    Result.ResolvedHitInstances =
        Result.GuaranteedHitInstances;

    if (RollBasisPoints(
            Result.OverflowRemainderBps,
            Rng))
    {
        ++Result.ResolvedHitInstances;
    }

    return Result;
}

FOGBlockResolution FOGCombatMath::ResolveBlock(
    const FOGCombatStats& Defender,
    bool bCanBeBlocked,
    FOGDeterministicRng& Rng)
{
    FOGBlockResolution Result;

    if (!bCanBeBlocked)
    {
        return Result;
    }

    Result.EffectiveBlockRateBps =
        FMath::Clamp(
            Defender.BlockRateBps,
            0,
            10000);

    Result.BlockReductionBps =
        FMath::Clamp(
            Defender.BlockReductionBps,
            0,
            10000);

    Result.bBlocked =
        RollBasisPoints(
            Result.EffectiveBlockRateBps,
            Rng);

    return Result;
}

int32 FOGCombatMath::RatioBps(
    const FOGLargeNumber& Numerator,
    const FOGLargeNumber& OtherPositiveTerm)
{
    if (Numerator.GetSign() <= 0)
    {
        return 0;
    }

    if (OtherPositiveTerm.GetSign() <= 0)
    {
        return 10000;
    }

    FOGLargeNumber A = Numerator;
    FOGLargeNumber B = OtherPositiveTerm;
    A.Normalize();
    B.Normalize();

    const int32 ExponentDelta =
        A.Exponent10 - B.Exponent10;

    // More than 9 decimal orders apart is below our stored significand precision.
    if (ExponentDelta > 9)
    {
        return 10000;
    }

    if (ExponentDelta < -9)
    {
        return 0;
    }

    int64 AlignedA = A.Significand;
    int64 AlignedB = B.Significand;

    if (ExponentDelta > 0)
    {
        AlignedB =
            DivideRoundedNonNegative(
                AlignedB,
                Pow10Int64(ExponentDelta));
    }
    else if (ExponentDelta < 0)
    {
        AlignedA =
            DivideRoundedNonNegative(
                AlignedA,
                Pow10Int64(-ExponentDelta));
    }

    const int64 Denominator =
        AlignedA + AlignedB;

    if (Denominator <= 0)
    {
        return 10000;
    }

    const int64 Scaled =
        (AlignedA * 10000LL + Denominator / 2) /
        Denominator;

    return static_cast<int32>(
        FMath::Clamp<int64>(
            Scaled,
            0,
            10000));
}

int32 FOGCombatMath::ResolveDefenseMultiplierBps(
    const FOGLargeNumber& DefenseReference,
    const FOGLargeNumber& DefenderDefense,
    int32 DefensePenetrationBps)
{
    if (DefenderDefense.GetSign() <= 0)
    {
        return 10000;
    }

    // Effective DEF = DEF * (1 - penetration).
    // Penetration above 100% becomes negative-defense pressure.
    const int64 RemainingDefenseBps =
        10000LL -
        static_cast<int64>(DefensePenetrationBps);

    if (RemainingDefenseBps >= 0)
    {
        const FOGLargeNumber EffectiveDefense =
            FOGLargeNumber::ScaleByBasisPoints(
                DefenderDefense,
                ClampToInt32(RemainingDefenseBps));

        return RatioBps(
            DefenseReference,
            EffectiveDefense);
    }

    const int64 NegativePressureBps =
        -RemainingDefenseBps;

    // Beyond 100% penetration we convert excess into an explicit multiplier.
    // This keeps negative-defense pressure readable and data-driven.
    return ClampToInt32(
        10000LL + NegativePressureBps);
}

FOGDamageResolution FOGCombatMath::ResolveDamage(
    const FOGDamageRequest& Request,
    const FOGCombatStats& Attacker,
    const FOGCombatStats& Defender,
    FOGDeterministicRng& Rng)
{
    FOGDamageResolution Result;

    Result.Hit =
        ResolveHit(
            Attacker,
            Defender,
            Request.bAllowHitOverflowReplication,
            Rng);

    if (Result.Hit.ResolvedHitInstances <= 0 ||
        Request.BaseDamage.GetSign() <= 0)
    {
        return Result;
    }

    Result.Crit =
        Request.bCanCrit
            ? ResolveCrit(Attacker, Defender, Rng)
            : FOGCritResolution();

    Result.Block =
        ResolveBlock(
            Defender,
            Request.bCanBeBlocked &&
            Request.DamageType != EOGBaseDamageType::True,
            Rng);

    FOGLargeNumber Damage =
        Request.BaseDamage;

    if (Request.DamageType == EOGBaseDamageType::Physical)
    {
        Result.DefenseMultiplierBps =
            ResolveDefenseMultiplierBps(
                Request.DefenseReference,
                Defender.Defense,
                Attacker.DefensePenetrationBps);

        Damage =
            FOGLargeNumber::ScaleByBasisPoints(
                Damage,
                Result.DefenseMultiplierBps);

        Damage =
            FOGLargeNumber::ScaleByBasisPoints(
                Damage,
                Attacker.DamageDealtMultiplierBps);

        Damage =
            FOGLargeNumber::ScaleByBasisPoints(
                Damage,
                Defender.DamageTakenMultiplierBps);

        for (const int32 MultiplierBps : Request.AdditionalMultiplierBps)
        {
            Damage =
                FOGLargeNumber::ScaleByBasisPoints(
                    Damage,
                    MultiplierBps);
        }

        if (Result.Block.bBlocked)
        {
            Damage =
                FOGLargeNumber::ScaleByBasisPoints(
                    Damage,
                    10000 - Result.Block.BlockReductionBps);
        }
    }
    else
    {
        // True Damage ignores DEF, ordinary Damage Reduction and Block.
        Result.DefenseMultiplierBps = 10000;

        Damage =
            FOGLargeNumber::ScaleByBasisPoints(
                Damage,
                Attacker.DamageDealtMultiplierBps);

        for (const int32 MultiplierBps : Request.AdditionalMultiplierBps)
        {
            Damage =
                FOGLargeNumber::ScaleByBasisPoints(
                    Damage,
                    MultiplierBps);
        }

        Damage =
            FOGLargeNumber::ScaleByBasisPoints(
                Damage,
                10000 -
                FMath::Clamp(
                    Defender.TrueDamageResistanceBps,
                    0,
                    10000));
    }

    Result.RankSuppressionMultiplierBps =
        FMath::Clamp(
            Request.RankSuppressionMultiplierBps,
            0,
            10000);

    Damage =
        FOGLargeNumber::ScaleByBasisPoints(
            Damage,
            Result.RankSuppressionMultiplierBps);

    if (Result.Crit.bCritical)
    {
        Damage =
            FOGLargeNumber::ScaleByBasisPoints(
                Damage,
                ClampToInt32(
                    10000LL +
                    static_cast<int64>(
                        Result.Crit.EffectiveCritDamageBonusBps)));
    }

    Result.DamagePerHit = Damage;

    FOGLargeNumber Total;
    for (int32 HitIndex = 0;
         HitIndex < Result.Hit.ResolvedHitInstances;
         ++HitIndex)
    {
        Total =
            FOGLargeNumber::Add(
                Total,
                Damage);
    }

    Result.TotalDamage = Total;
    return Result;
}
