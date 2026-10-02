#include "Combat/OGBattleReplay.h"

FOGReplayResult FOGBattleReplay::Run(
    const FOGTurnBattleState& InitialState,
    uint64 Seed,
    const TArray<FOGReplayActionCommand>& Commands)
{
    FOGReplayResult Result;
    Result.Seed = Seed;

    FOGTurnBattle Battle;
    FString Error;

    if (!Battle.Initialize(
            InitialState,
            Error))
    {
        Result.Error = Error;
        return Result;
    }

    FOGDeterministicRng Rng(Seed);

    for (const FOGReplayActionCommand& Command :
         Commands)
    {
        if (Battle.GetState().Status ==
            EOGTurnBattleStatus::Completed)
        {
            break;
        }

        const FOGCombatUnitState* Attacker =
            FindUnit(
                Battle.GetState(),
                Command.SourceUnitId);

        const FOGCombatUnitState* Defender =
            FindUnit(
                Battle.GetState(),
                Command.TargetUnitId);

        if (!Attacker || !Defender)
        {
            Result.Error =
                TEXT("Replay command references a missing combat unit.");
            return Result;
        }

        FOGDamageRequest DamageRequest;
        DamageRequest.BaseDamage =
            FOGLargeNumber::ScaleByBasisPoints(
                Attacker->Stats.Attack,
                Command.SkillMultiplierBps);
        DamageRequest.DefenseReference =
            Attacker->Stats.Attack;
        DamageRequest.DamageType =
            Command.DamageType;
        DamageRequest.bCanCrit =
            Command.bCanCrit;
        DamageRequest.bCanBeBlocked =
            Command.bCanBeBlocked;
        DamageRequest.bAllowHitOverflowReplication =
            Command.bAllowHitOverflowReplication;

        const FOGDamageResolution Damage =
            FOGCombatMath::ResolveDamage(
                DamageRequest,
                Attacker->Stats,
                Defender->Stats,
                Rng);

        if (Damage.TotalDamage.GetSign() > 0)
        {
            if (!Battle.ApplyDamage(
                    Command.SourceUnitId,
                    Command.TargetUnitId,
                    Damage.TotalDamage,
                    Command.SkillId,
                    Error))
            {
                Result.Error = Error;
                return Result;
            }
        }

        FOGResolvedCombatAction Action;
        Action.ActionId = FOGEntityId::NewId();
        Action.SourceUnitId =
            Command.SourceUnitId;
        Action.TargetUnitIds.Add(
            Command.TargetUnitId);
        Action.SkillId =
            Command.SkillId;
        Action.ActionDelay =
            Command.ActionDelay;
        Action.ResolutionJson =
            FString::Printf(
                TEXT("{\"hits\":%d,\"crit\":%s,\"block\":%s,\"damage\":\"%s\",\"rng_draws\":%llu}"),
                Damage.Hit.ResolvedHitInstances,
                Damage.Crit.bCritical
                    ? TEXT("true")
                    : TEXT("false"),
                Damage.Block.bBlocked
                    ? TEXT("true")
                    : TEXT("false"),
                *Damage.TotalDamage.ToDebugString(),
                Rng.GetDrawCount());

        if (!Battle.ApplyResolvedAction(
                Action,
                Error))
        {
            Result.Error = Error;
            return Result;
        }

        // Replay proof currently uses scenarios without authored triggered
        // actions. A trigger appearing here is a deliberate unsupported input
        // rather than something silently skipped.
        const TArray<FOGQueuedTriggeredAction> Triggered =
            Battle.DrainTriggeredActions();

        if (!Triggered.IsEmpty())
        {
            Result.Error =
                TEXT("Replay proof command stream encountered a triggered action; explicit trigger-command recording is required.");
            return Result;
        }
    }

    Result.RngDrawCount =
        Rng.GetDrawCount();

    Result.DeterministicFingerprint =
        BuildFingerprint(
            Battle,
            Seed,
            Result.RngDrawCount);

    Result.bSucceeded = true;
    return Result;
}

const FOGCombatUnitState* FOGBattleReplay::FindUnit(
    const FOGTurnBattleState& State,
    const FOGEntityId& UnitId)
{
    return State.Units.FindByPredicate(
        [&UnitId](
            const FOGCombatUnitState& Unit)
        {
            return Unit.UnitEntityId ==
                   UnitId;
        });
}

FString FOGBattleReplay::BuildFingerprint(
    const FOGTurnBattle& Battle,
    uint64 Seed,
    uint64 DrawCount)
{
    TArray<FString> Parts;

    Parts.Add(
        FString::Printf(
            TEXT("seed=%llu"),
            Seed));

    Parts.Add(
        FString::Printf(
            TEXT("draws=%llu"),
            DrawCount));

    Parts.Add(
        FString::Printf(
            TEXT("status=%d"),
            static_cast<int32>(
                Battle.GetState().Status)));

    Parts.Add(
        FString::Printf(
            TEXT("av=%lld"),
            Battle.GetState().CurrentActionValue));

    TArray<const FOGCombatUnitState*> Units;
    Units.Reserve(
        Battle.GetState().Units.Num());

    for (const FOGCombatUnitState& Unit :
         Battle.GetState().Units)
    {
        Units.Add(&Unit);
    }

    Units.Sort(
        [](const FOGCombatUnitState& A,
           const FOGCombatUnitState& B)
        {
            return A.UnitEntityId.ToString().Compare(
                B.UnitEntityId.ToString(),
                ESearchCase::CaseSensitive) < 0;
        });

    for (const FOGCombatUnitState* Unit :
         Units)
    {
        Parts.Add(
            FString::Printf(
                TEXT("u:%s:%d:%d:%s:%lld"),
                *Unit->UnitEntityId.ToString(),
                static_cast<int32>(
                    Unit->Presence),
                Unit->OccupiedLaneIndex,
                *Unit->CurrentHp.ToDebugString(),
                Unit->NextActionValue));
    }

    for (const FOGCombatLogEvent& Event :
         Battle.GetLog().GetEvents())
    {
        Parts.Add(
            FString::Printf(
                TEXT("e:%lld:%d:%s:%s:%s:%s"),
                Event.Sequence,
                static_cast<int32>(
                    Event.Type),
                *Event.SourceUnitId.ToString(),
                *Event.TargetUnitId.ToString(),
                *Event.SkillId.ToString(),
                *Event.PayloadJson));
    }

    return FString::Join(
        Parts,
        TEXT("|"));
}
