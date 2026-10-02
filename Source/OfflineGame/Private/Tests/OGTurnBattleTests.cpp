#include "Combat/OGActionCombatAdapter.h"
#include "Combat/OGTurnBattle.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

namespace
{
FOGCombatUnitState MakeUnit(
    int32 Team,
    EOGCombatPresence Presence,
    int64 ActionValue,
    int64 Hp)
{
    FOGCombatUnitState Unit;
    Unit.UnitEntityId = FOGEntityId::NewId();
    Unit.IdentityId = FOGContentId(
        FString::Printf(
            TEXT("test:identity.%s"),
            *Unit.UnitEntityId.ToString()));
    Unit.TeamIndex = Team;
    Unit.Presence = Presence;
    Unit.CurrentHp = FOGLargeNumber::FromInt64(Hp);
    Unit.Stats.MaxHp = Unit.CurrentHp;
    Unit.NextActionValue = ActionValue;
    Unit.DefaultActionDelay = 100;
    return Unit;
}

FOGTurnTeamState MakeTeam(
    int32 TeamIndex,
    const TArray<TArray<FOGEntityId>>& LaneUnits)
{
    FOGTurnTeamState Team;
    Team.TeamIndex = TeamIndex;

    for (int32 LaneIndex = 0;
         LaneIndex < LaneUnits.Num();
         ++LaneIndex)
    {
        FOGTurnSuccessionLane Lane;
        Lane.LaneIndex = LaneIndex;
        Lane.OrderedUnitIds = LaneUnits[LaneIndex];
        Team.Lanes.Add(MoveTemp(Lane));
    }

    return Team;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOGTurnBattleTimelineTest,
    "OfflineGame.Combat.Turn.TimelineOrdering",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FOGTurnBattleTimelineTest::RunTest(const FString& Parameters)
{
    FOGTurnBattleState Initial;
    Initial.BattleId = FOGEntityId::NewId();

    FOGCombatUnitState First = MakeUnit(0, EOGCombatPresence::Active, 100, 1000);
    FOGCombatUnitState Second = MakeUnit(1, EOGCombatPresence::Active, 50, 1000);

    Initial.Teams =
    {
        MakeTeam(0, {{First.UnitEntityId}}),
        MakeTeam(1, {{Second.UnitEntityId}})
    };
    Initial.Units = {First, Second};

    FOGTurnBattle Battle;
    FString Error;
    TestTrue(TEXT("Battle initializes"), Battle.Initialize(Initial, Error));

    const int32 NextIndex = Battle.SelectNextActingUnitIndex();
    TestTrue(TEXT("A next actor exists"), NextIndex != INDEX_NONE);
    TestTrue(
        TEXT("Lowest action value acts first"),
        Battle.GetState().Units[NextIndex].UnitEntityId == Second.UnitEntityId);

    FOGResolvedCombatAction Action;
    Action.ActionId = FOGEntityId::NewId();
    Action.SourceUnitId = Second.UnitEntityId;
    Action.SkillId = FOGContentId(TEXT("test:skill.basic"));
    Action.ActionDelay = 100;

    TestTrue(TEXT("Next actor action resolves"), Battle.ApplyResolvedAction(Action, Error));
    TestEqual(
        TEXT("Current action value advances"),
        Battle.GetState().CurrentActionValue,
        static_cast<int64>(50));

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOGTurnBattleDefeatTest,
    "OfflineGame.Combat.Turn.DefeatPersistsInsideBattle",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FOGTurnBattleDefeatTest::RunTest(const FString& Parameters)
{
    FOGTurnBattleState Initial;
    Initial.BattleId = FOGEntityId::NewId();

    FOGCombatUnitState Attacker = MakeUnit(0, EOGCombatPresence::Active, 0, 1000);
    FOGCombatUnitState Target = MakeUnit(1, EOGCombatPresence::Active, 0, 100);

    Initial.Teams =
    {
        MakeTeam(0, {{Attacker.UnitEntityId}}),
        MakeTeam(1, {{Target.UnitEntityId}})
    };
    Initial.Units = {Attacker, Target};

    FOGTurnBattle Battle;
    FString Error;
    TestTrue(TEXT("Battle initializes"), Battle.Initialize(Initial, Error));

    TestTrue(
        TEXT("Damage resolves"),
        Battle.ApplyDamage(
            Attacker.UnitEntityId,
            Target.UnitEntityId,
            FOGLargeNumber::FromInt64(150),
            FOGContentId(TEXT("test:skill.hit")),
            Error));

    const FOGCombatUnitState& StoredTarget = Battle.GetState().Units[1];
    TestEqual(
        TEXT("Target enters defeated state"),
        StoredTarget.Presence,
        EOGCombatPresence::Defeated);

    TestEqual(
        TEXT("Battle completes when only one team has living units"),
        Battle.GetState().Status,
        EOGTurnBattleStatus::Completed);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOGTurnBattleSuccessionTest,
    "OfflineGame.Combat.Turn.DirectLaneSuccessionAfterAction",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FOGTurnBattleSuccessionTest::RunTest(const FString& Parameters)
{
    FOGTurnBattleState Initial;
    Initial.BattleId = FOGEntityId::NewId();

    FOGCombatUnitState Attacker =
        MakeUnit(0, EOGCombatPresence::Active, 0, 1000);

    FOGCombatUnitState Opening =
        MakeUnit(1, EOGCombatPresence::Active, 0, 100);

    FOGCombatUnitState Successor =
        MakeUnit(1, EOGCombatPresence::Reserve, 0, 1000);

    Initial.Teams =
    {
        MakeTeam(0, {{Attacker.UnitEntityId}}),
        MakeTeam(
            1,
            {{
                Opening.UnitEntityId,
                Successor.UnitEntityId
            }})
    };
    Initial.Units = {Attacker, Opening, Successor};

    FOGTurnBattle Battle;
    FString Error;
    TestTrue(TEXT("Battle initializes"), Battle.Initialize(Initial, Error));

    TestTrue(
        TEXT("Opening unit can be defeated during an action"),
        Battle.ApplyDamage(
            Attacker.UnitEntityId,
            Opening.UnitEntityId,
            FOGLargeNumber::FromInt64(150),
            FOGContentId(TEXT("test:skill.hit")),
            Error));

    TestEqual(
        TEXT("Successor does not enter before action resolution completes"),
        Battle.GetState().Units[2].Presence,
        EOGCombatPresence::Reserve);

    FOGResolvedCombatAction Action;
    Action.ActionId = FOGEntityId::NewId();
    Action.SourceUnitId = Attacker.UnitEntityId;
    Action.SkillId = FOGContentId(TEXT("test:skill.hit"));
    Action.ActionDelay = 100;

    TestTrue(
        TEXT("Action completion resolves pending succession"),
        Battle.ApplyResolvedAction(Action, Error));

    const FOGCombatUnitState& StoredSuccessor =
        Battle.GetState().Units[2];

    TestEqual(
        TEXT("Direct successor becomes active"),
        StoredSuccessor.Presence,
        EOGCombatPresence::Active);

    TestTrue(
        TEXT("Successor enters timeline with a normal delay, not a free immediate action"),
        StoredSuccessor.NextActionValue >
            Battle.GetState().CurrentActionValue);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOGActionPartyIdentityExclusivityTest,
    "OfflineGame.Combat.Action.IdentityExclusivity",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FOGActionPartyIdentityExclusivityTest::RunTest(const FString& Parameters)
{
    FOGCombatUnitState First = MakeUnit(0, EOGCombatPresence::Active, 0, 1000);
    FOGCombatUnitState Second = MakeUnit(0, EOGCombatPresence::Active, 0, 1000);
    Second.IdentityId = First.IdentityId;

    FString Error;
    TestFalse(
        TEXT("Same Character Identity cannot occupy two action-party slots"),
        FOGActionCombatAdapter::ValidateSwitchParty(
            {First, Second},
            Error));

    return true;
}

#endif
