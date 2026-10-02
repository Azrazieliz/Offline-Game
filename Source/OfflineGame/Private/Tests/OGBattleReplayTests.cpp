#include "Combat/OGBattleReplay.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

namespace
{
FOGCombatUnitState MakeReplayUnit(
    int32 Team,
    int64 ActionValue,
    int64 Hp,
    int64 Attack,
    int64 Defense)
{
    FOGCombatUnitState Unit;
    Unit.UnitEntityId =
        FOGEntityId::NewId();
    Unit.IdentityId =
        FOGContentId(
            FString::Printf(
                TEXT("test:replay.%s"),
                *Unit.UnitEntityId.ToString()));
    Unit.TeamIndex = Team;
    Unit.Presence =
        EOGCombatPresence::Active;
    Unit.CurrentHp =
        FOGLargeNumber::FromInt64(Hp);
    Unit.Stats.MaxHp =
        Unit.CurrentHp;
    Unit.Stats.Attack =
        FOGLargeNumber::FromInt64(Attack);
    Unit.Stats.Defense =
        FOGLargeNumber::FromInt64(Defense);
    Unit.Stats.CritRateBps = 5000;
    Unit.Stats.CritDamageBonusBps = 5000;
    Unit.Stats.HitBps = 10000;
    Unit.NextActionValue = ActionValue;
    Unit.DefaultActionDelay = 100;
    return Unit;
}

FOGTurnTeamState MakeReplayTeam(
    int32 TeamIndex,
    const FOGEntityId& UnitId)
{
    FOGTurnTeamState Team;
    Team.TeamIndex = TeamIndex;

    FOGTurnSuccessionLane Lane;
    Lane.LaneIndex = 0;
    Lane.OrderedUnitIds.Add(UnitId);
    Team.Lanes.Add(MoveTemp(Lane));
    return Team;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOGDeterministicFullBattleReplayTest,
    "OfflineGame.Combat.Replay.SameSeedSameCommandsSameFingerprint",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FOGDeterministicFullBattleReplayTest::RunTest(
    const FString& Parameters)
{
    FOGTurnBattleState Initial;
    Initial.BattleId =
        FOGEntityId::NewId();

    FOGCombatUnitState First =
        MakeReplayUnit(
            0,
            0,
            4000,
            900,
            200);

    FOGCombatUnitState Second =
        MakeReplayUnit(
            1,
            100,
            4000,
            850,
            250);

    Initial.Teams =
    {
        MakeReplayTeam(
            0,
            First.UnitEntityId),
        MakeReplayTeam(
            1,
            Second.UnitEntityId)
    };

    Initial.Units =
    {
        First,
        Second
    };

    TArray<FOGReplayActionCommand> Commands;

    for (int32 Round = 0;
         Round < 8;
         ++Round)
    {
        FOGReplayActionCommand FirstAction;
        FirstAction.SourceUnitId =
            First.UnitEntityId;
        FirstAction.TargetUnitId =
            Second.UnitEntityId;
        FirstAction.SkillId =
            FOGContentId(TEXT("test:skill.first"));
        FirstAction.SkillMultiplierBps =
            8000;
        FirstAction.ActionDelay = 200;
        Commands.Add(FirstAction);

        FOGReplayActionCommand SecondAction;
        SecondAction.SourceUnitId =
            Second.UnitEntityId;
        SecondAction.TargetUnitId =
            First.UnitEntityId;
        SecondAction.SkillId =
            FOGContentId(TEXT("test:skill.second"));
        SecondAction.SkillMultiplierBps =
            8000;
        SecondAction.ActionDelay = 200;
        Commands.Add(SecondAction);
    }

    const uint64 Seed =
        0x123456789ABCDEF0ull;

    const FOGReplayResult FirstRun =
        FOGBattleReplay::Run(
            Initial,
            Seed,
            Commands);

    const FOGReplayResult SecondRun =
        FOGBattleReplay::Run(
            Initial,
            Seed,
            Commands);

    TestTrue(
        TEXT("First replay succeeds"),
        FirstRun.bSucceeded);

    TestTrue(
        TEXT("Second replay succeeds"),
        SecondRun.bSucceeded);

    TestEqual(
        TEXT("RNG draw count is reproducible"),
        FirstRun.RngDrawCount,
        SecondRun.RngDrawCount);

    TestEqual(
        TEXT("Same seed/state/commands produce identical fingerprint"),
        FirstRun.DeterministicFingerprint,
        SecondRun.DeterministicFingerprint);

    return true;
}

#endif
