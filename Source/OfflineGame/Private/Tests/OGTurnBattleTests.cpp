#include "Combat/OGActionCombatAdapter.h"
#include "Combat/OGCombatMath.h"
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
            *Unit.UnitEntityId.ToString().ToLower()));
    Unit.TeamIndex = Team;
    Unit.Presence = Presence;
    Unit.CurrentHp = FOGLargeNumber::FromInt64(Hp);
    Unit.Stats.MaxHp = FOGLargeNumber::FromInt64(FMath::Max<int64>(1, Hp));
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

FOGResolvedCombatAction MakeOrdinaryAction(
    const FOGEntityId& Source,
    const TCHAR* Skill)
{
    FOGResolvedCombatAction Action;
    Action.ActionId = FOGEntityId::NewId();
    Action.SourceUnitId = Source;
    Action.SkillId = FOGContentId(Skill);
    Action.ActionDelay = 100;
    return Action;
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

    FOGCombatUnitState First =
        MakeUnit(0, EOGCombatPresence::Active, 100, 1000);
    FOGCombatUnitState Second =
        MakeUnit(1, EOGCombatPresence::Active, 50, 1000);

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
        Battle.GetState().Units[NextIndex].UnitEntityId ==
            Second.UnitEntityId);

    const FOGResolvedCombatAction Action =
        MakeOrdinaryAction(
            Second.UnitEntityId,
            TEXT("test:skill.basic"));

    TestTrue(
        TEXT("Next actor action resolves"),
        Battle.ApplyResolvedAction(Action, Error));

    TestEqual(
        TEXT("Current action value advances"),
        Battle.GetState().CurrentActionValue,
        static_cast<int64>(50));

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOGTurnBattleDefeatCompletionTest,
    "OfflineGame.Combat.Turn.DefeatCompletesAfterActionWindow",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FOGTurnBattleDefeatCompletionTest::RunTest(const FString& Parameters)
{
    FOGTurnBattleState Initial;
    Initial.BattleId = FOGEntityId::NewId();

    FOGCombatUnitState Attacker =
        MakeUnit(0, EOGCombatPresence::Active, 0, 1000);
    FOGCombatUnitState Target =
        MakeUnit(1, EOGCombatPresence::Active, 100, 100);

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
        TEXT("Damage defeats target"),
        Battle.ApplyDamage(
            Attacker.UnitEntityId,
            Target.UnitEntityId,
            FOGLargeNumber::FromInt64(150),
            FOGContentId(TEXT("test:skill.hit")),
            Error));

    TestEqual(
        TEXT("Battle waits for current action to finish"),
        Battle.GetState().Status,
        EOGTurnBattleStatus::Running);

    TestTrue(
        TEXT("Current action finishes"),
        Battle.ApplyResolvedAction(
            MakeOrdinaryAction(
                Attacker.UnitEntityId,
                TEXT("test:skill.hit")),
            Error));

    TestEqual(
        TEXT("Battle completes after defeat window and succession resolution"),
        Battle.GetState().Status,
        EOGTurnBattleStatus::Completed);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOGTurnBattleDirectSuccessionTest,
    "OfflineGame.Combat.Turn.DirectLaneSuccessionAfterAction",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FOGTurnBattleDirectSuccessionTest::RunTest(const FString& Parameters)
{
    FOGTurnBattleState Initial;
    Initial.BattleId = FOGEntityId::NewId();

    FOGCombatUnitState Attacker =
        MakeUnit(0, EOGCombatPresence::Active, 0, 1000);
    FOGCombatUnitState Opening =
        MakeUnit(1, EOGCombatPresence::Active, 100, 100);
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
        TEXT("Opening unit is defeated"),
        Battle.ApplyDamage(
            Attacker.UnitEntityId,
            Opening.UnitEntityId,
            FOGLargeNumber::FromInt64(150),
            FOGContentId(TEXT("test:skill.hit")),
            Error));

    TestEqual(
        TEXT("Successor remains reserve during current action"),
        Battle.GetState().Units[2].Presence,
        EOGCombatPresence::Reserve);

    TestTrue(
        TEXT("Action completion resolves succession"),
        Battle.ApplyResolvedAction(
            MakeOrdinaryAction(
                Attacker.UnitEntityId,
                TEXT("test:skill.hit")),
            Error));

    const FOGCombatUnitState& StoredSuccessor =
        Battle.GetState().Units[2];

    TestEqual(
        TEXT("Direct successor becomes active"),
        StoredSuccessor.Presence,
        EOGCombatPresence::Active);

    TestEqual(
        TEXT("Direct successor occupies defeated lane"),
        StoredSuccessor.OccupiedLaneIndex,
        0);

    TestTrue(
        TEXT("Successor gets a normal timeline delay"),
        StoredSuccessor.NextActionValue >
            Battle.GetState().CurrentActionValue);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOGTurnBattleRevivalBeforeSuccessionTest,
    "OfflineGame.Combat.Turn.RevivalBeforeSuccession",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FOGTurnBattleRevivalBeforeSuccessionTest::RunTest(const FString& Parameters)
{
    FOGTurnBattleState Initial;
    Initial.BattleId = FOGEntityId::NewId();

    FOGCombatUnitState Attacker =
        MakeUnit(0, EOGCombatPresence::Active, 0, 1000);
    FOGCombatUnitState Opening =
        MakeUnit(1, EOGCombatPresence::Active, 100, 100);
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

    FOGCombatTriggerBinding RevivalBinding;
    RevivalBinding.OwnerUnitId = Opening.UnitEntityId;
    RevivalBinding.Trigger.TriggerId =
        FOGContentId(TEXT("test:trigger.revival"));
    RevivalBinding.Trigger.EventType =
        OGCombatEventNames::Defeat();
    RevivalBinding.Trigger.ActionIds.Add(
        FOGContentId(TEXT("test:action.revive")));

    FOGTurnBattle Battle;
    FString Error;
    TestTrue(
        TEXT("Battle initializes with revival trigger"),
        Battle.Initialize(
            Initial,
            {RevivalBinding},
            FOGCombatConditionEvaluator(),
            Error));

    TestTrue(
        TEXT("Opening unit is defeated"),
        Battle.ApplyDamage(
            Attacker.UnitEntityId,
            Opening.UnitEntityId,
            FOGLargeNumber::FromInt64(150),
            FOGContentId(TEXT("test:skill.hit")),
            Error));

    TestTrue(
        TEXT("Current action completes and opens defeat-trigger window"),
        Battle.ApplyResolvedAction(
            MakeOrdinaryAction(
                Attacker.UnitEntityId,
                TEXT("test:skill.hit")),
            Error));

    const TArray<FOGQueuedTriggeredAction> Triggered =
        Battle.DrainTriggeredActions();

    TestEqual(
        TEXT("Revival action is queued"),
        Triggered.Num(),
        1);

    TestEqual(
        TEXT("Successor is not promoted before revival resolves"),
        Battle.GetState().Units[2].Presence,
        EOGCombatPresence::Reserve);

    TestTrue(
        TEXT("Revival restores HP"),
        Battle.ApplyHealing(
            Opening.UnitEntityId,
            Opening.UnitEntityId,
            FOGLargeNumber::FromInt64(100),
            FOGContentId(TEXT("test:action.revive")),
            Error));

    TestTrue(
        TEXT("Revived unit returns to active state"),
        Battle.ChangePresence(
            Opening.UnitEntityId,
            EOGCombatPresence::Active,
            Error));

    TestTrue(
        TEXT("Completing revival closes defeat window"),
        Battle.CompleteTriggeredAction(
            Triggered[0].QueueSequence,
            Error));

    TestEqual(
        TEXT("Revived opening unit keeps its slot"),
        Battle.GetState().Units[1].Presence,
        EOGCombatPresence::Active);

    TestEqual(
        TEXT("Successor remains reserve because revival succeeded"),
        Battle.GetState().Units[2].Presence,
        EOGCombatPresence::Reserve);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOGTurnBattleCrossLaneRebalanceTest,
    "OfflineGame.Combat.Turn.ExhaustedLaneBorrowsDeepReserve",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FOGTurnBattleCrossLaneRebalanceTest::RunTest(const FString& Parameters)
{
    FOGTurnBattleState Initial;
    Initial.BattleId = FOGEntityId::NewId();

    FOGCombatUnitState Attacker =
        MakeUnit(0, EOGCombatPresence::Active, 0, 1000);

    FOGCombatUnitState A1 =
        MakeUnit(1, EOGCombatPresence::Active, 100, 1000);
    FOGCombatUnitState B1Dead =
        MakeUnit(1, EOGCombatPresence::Defeated, 0, 0);
    FOGCombatUnitState C1 =
        MakeUnit(1, EOGCombatPresence::Reserve, 0, 1000);

    FOGCombatUnitState A2 =
        MakeUnit(1, EOGCombatPresence::Active, 100, 100);
    FOGCombatUnitState B2Dead =
        MakeUnit(1, EOGCombatPresence::Defeated, 0, 0);
    FOGCombatUnitState C2Dead =
        MakeUnit(1, EOGCombatPresence::Defeated, 0, 0);

    FOGCombatUnitState A3 =
        MakeUnit(1, EOGCombatPresence::Active, 100, 1000);
    FOGCombatUnitState B3 =
        MakeUnit(1, EOGCombatPresence::Reserve, 0, 1000);

    Initial.Teams =
    {
        MakeTeam(0, {{Attacker.UnitEntityId}}),
        MakeTeam(
            1,
            {
                {A1.UnitEntityId, B1Dead.UnitEntityId, C1.UnitEntityId},
                {A2.UnitEntityId, B2Dead.UnitEntityId, C2Dead.UnitEntityId},
                {A3.UnitEntityId, B3.UnitEntityId}
            })
    };

    Initial.Units =
    {
        Attacker,
        A1, B1Dead, C1,
        A2, B2Dead, C2Dead,
        A3, B3
    };

    FOGTurnBattle Battle;
    FString Error;
    TestTrue(TEXT("Battle initializes"), Battle.Initialize(Initial, Error));

    TestTrue(
        TEXT("A2 is defeated after its own lane reserves are already gone"),
        Battle.ApplyDamage(
            Attacker.UnitEntityId,
            A2.UnitEntityId,
            FOGLargeNumber::FromInt64(150),
            FOGContentId(TEXT("test:skill.hit")),
            Error));

    TestTrue(
        TEXT("Action completion performs team rebalance"),
        Battle.ApplyResolvedAction(
            MakeOrdinaryAction(
                Attacker.UnitEntityId,
                TEXT("test:skill.hit")),
            Error));

    const FOGCombatUnitState* StoredC1 =
        Battle.GetState().Units.FindByPredicate(
            [&C1](const FOGCombatUnitState& Unit)
            {
                return Unit.UnitEntityId == C1.UnitEntityId;
            });

    const FOGCombatUnitState* StoredA1 =
        Battle.GetState().Units.FindByPredicate(
            [&A1](const FOGCombatUnitState& Unit)
            {
                return Unit.UnitEntityId == A1.UnitEntityId;
            });

    const FOGCombatUnitState* StoredB3 =
        Battle.GetState().Units.FindByPredicate(
            [&B3](const FOGCombatUnitState& Unit)
            {
                return Unit.UnitEntityId == B3.UnitEntityId;
            });

    TestTrue(TEXT("C1 exists"), StoredC1 != nullptr);
    TestTrue(TEXT("A1 exists"), StoredA1 != nullptr);
    TestTrue(TEXT("B3 exists"), StoredB3 != nullptr);

    TestEqual(
        TEXT("C1 is promoted to fill battlefield lane 2"),
        StoredC1->OccupiedLaneIndex,
        1);

    TestEqual(
        TEXT("C1 becomes active"),
        StoredC1->Presence,
        EOGCombatPresence::Active);

    TestEqual(
        TEXT("A1 remains active in its original battlefield lane"),
        StoredA1->OccupiedLaneIndex,
        0);

    TestEqual(
        TEXT("B3 remains reserve because deeper C1 is the less disruptive fallback"),
        StoredB3->Presence,
        EOGCombatPresence::Reserve);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOGActionPartyIdentityExclusivityTest,
    "OfflineGame.Combat.Action.IdentityExclusivity",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FOGActionPartyIdentityExclusivityTest::RunTest(const FString& Parameters)
{
    FOGCombatUnitState First =
        MakeUnit(0, EOGCombatPresence::Active, 0, 1000);
    FOGCombatUnitState Second =
        MakeUnit(0, EOGCombatPresence::Active, 0, 1000);

    Second.IdentityId = First.IdentityId;

    FString Error;
    TestFalse(
        TEXT("Same Character Identity cannot occupy two action-party slots"),
        FOGActionCombatAdapter::ValidateSwitchParty(
            {First, Second},
            Error));

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOGSharedIdentityExclusivityRuleTest,
    "OfflineGame.Combat.Shared.IdentityExclusivityAndExplicitOverride",
    EAutomationTestFlags::ApplicationContextMask |
        EAutomationTestFlags::EngineFilter)

bool FOGSharedIdentityExclusivityRuleTest::RunTest(
    const FString& Parameters)
{
    FOGCombatUnitState First =
        MakeUnit(
            0,
            EOGCombatPresence::Active,
            0,
            1000);
    FOGCombatUnitState Duplicate =
        MakeUnit(
            0,
            EOGCombatPresence::Active,
            0,
            1000);
    FOGCombatUnitState Opponent =
        MakeUnit(
            1,
            EOGCombatPresence::Active,
            0,
            1000);

    Duplicate.IdentityId =
        First.IdentityId;

    FString Error;
    TestFalse(
        TEXT("Action combat rejects duplicate Character Identity by default"),
        FOGActionCombatAdapter::ValidateSwitchParty(
            {First, Duplicate},
            Error));

    FOGTurnBattleState Initial;
    Initial.BattleId =
        FOGEntityId::NewId();
    Initial.Teams =
    {
        MakeTeam(
            0,
            {
                {First.UnitEntityId},
                {Duplicate.UnitEntityId}
            }),
        MakeTeam(
            1,
            {{Opponent.UnitEntityId}})
    };
    Initial.Units =
    {
        First,
        Duplicate,
        Opponent
    };

    FOGTurnBattle DefaultBattle;
    Error.Reset();
    TestFalse(
        TEXT("Turn combat rejects the same duplicate Identity by default"),
        DefaultBattle.Initialize(
            Initial,
            Error));

    FOGIdentityExclusivityContext OverrideContext;
    OverrideContext.Override =
        [](const FOGCombatUnitState& Existing,
           const FOGCombatUnitState& Candidate)
        {
            if (Existing.IdentityId ==
                Candidate.IdentityId)
            {
                return EOGIdentityExclusivityOverrideDecision::AllowDuplicate;
            }

            return EOGIdentityExclusivityOverrideDecision::UseDefault;
        };

    Error.Reset();
    TestTrue(
        TEXT("Explicit mechanic override allows action-combat duplicate Identity"),
        FOGActionCombatAdapter::ValidateSwitchParty(
            {First, Duplicate},
            OverrideContext,
            Error));

    FOGTurnBattle OverrideBattle;
    Error.Reset();
    TestTrue(
        TEXT("The same explicit override allows turn-combat duplicate Identity"),
        OverrideBattle.Initialize(
            Initial,
            TArray<FOGCombatTriggerBinding>(),
            FOGCombatConditionEvaluator(),
            OverrideContext,
            FOGRankSuppressionResolver(),
            Error));

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOGSharedRankSuppressionHookTest,
    "OfflineGame.Combat.Shared.RankSuppressionIsChannelBasedAndDataResolved",
    EAutomationTestFlags::ApplicationContextMask |
        EAutomationTestFlags::EngineFilter)

bool FOGSharedRankSuppressionHookTest::RunTest(
    const FString& Parameters)
{
    FOGCombatUnitState Source =
        MakeUnit(
            0,
            EOGCombatPresence::Active,
            0,
            1000);
    FOGCombatUnitState Target =
        MakeUnit(
            1,
            EOGCombatPresence::Active,
            0,
            1000);

    Source.RankProjection.RankId =
        FOGContentId(
            TEXT("core:rank.sage"));
    Source.RankProjection.Level = 70;

    Target.RankProjection.RankId =
        FOGContentId(
            TEXT("core:rank.legend"));
    Target.RankProjection.Level = 10;

    int32 ResolverCalls = 0;
    FOGRankSuppressionResolver Resolver =
        [&ResolverCalls](
            const FOGRankSuppressionQuery& Query,
            int32& OutMultiplierBps,
            FString& OutError)
        {
            ++ResolverCalls;
            OutError.Reset();

            if (Query.ChannelId !=
                    OGRankSuppressionChannels::Damage() ||
                Query.SourceRankId !=
                    FOGContentId(TEXT("core:rank.sage")) ||
                Query.TargetRankId !=
                    FOGContentId(TEXT("core:rank.legend")))
            {
                OutError =
                    TEXT("Unexpected Rank-suppression query.");
                return false;
            }

            // Test-owned tuning result. Production coefficients remain outside
            // combat code and are supplied by data/tuning resolution.
            OutMultiplierBps = 4000;
            return true;
        };

    FString Error;
    int32 ActionMultiplier = 0;
    TestTrue(
        TEXT("Action combat resolves Rank hook through shared service"),
        FOGActionCombatAdapter::ResolveRankSuppressionMultiplier(
            Source,
            Target,
            OGRankSuppressionChannels::Damage(),
            Resolver,
            ActionMultiplier,
            Error));
    TestEqual(
        TEXT("Action combat receives resolver-owned multiplier"),
        ActionMultiplier,
        4000);

    FOGTurnBattleState Initial;
    Initial.BattleId =
        FOGEntityId::NewId();
    Initial.Teams =
    {
        MakeTeam(
            0,
            {{Source.UnitEntityId}}),
        MakeTeam(
            1,
            {{Target.UnitEntityId}})
    };
    Initial.Units =
    {
        Source,
        Target
    };

    FOGTurnBattle Battle;
    TestTrue(
        TEXT("Turn battle initializes with shared Rank resolver"),
        Battle.Initialize(
            Initial,
            TArray<FOGCombatTriggerBinding>(),
            FOGCombatConditionEvaluator(),
            FOGIdentityExclusivityContext(),
            Resolver,
            Error));

    int32 TurnMultiplier = 0;
    TestTrue(
        TEXT("Turn combat resolves Rank hook through same service"),
        Battle.ResolveRankSuppressionMultiplier(
            Source.UnitEntityId,
            Target.UnitEntityId,
            OGRankSuppressionChannels::Damage(),
            TurnMultiplier,
            Error));
    TestEqual(
        TEXT("Turn combat receives the same resolver-owned multiplier"),
        TurnMultiplier,
        4000);

    FOGCombatStats AttackerStats;
    AttackerStats.HitBps = 10000;
    AttackerStats.CritRateBps = 0;

    FOGCombatStats DefenderStats;

    FOGDamageRequest DamageRequest;
    DamageRequest.BaseDamage =
        FOGLargeNumber::FromInt64(
            1000);
    DamageRequest.DefenseReference =
        FOGLargeNumber::FromInt64(
            1000);
    DamageRequest.DamageType =
        EOGBaseDamageType::True;
    DamageRequest.bCanCrit = false;
    DamageRequest.bCanBeBlocked = false;
    DamageRequest.bAllowHitOverflowReplication =
        false;
    DamageRequest.RankSuppressionMultiplierBps =
        TurnMultiplier;

    FOGDeterministicRng Rng(17);
    const FOGDamageResolution Damage =
        FOGCombatMath::ResolveDamage(
            DamageRequest,
            AttackerStats,
            DefenderStats,
            Rng);

    TestEqual(
        TEXT("Damage resolution records Rank suppression multiplier"),
        Damage.RankSuppressionMultiplierBps,
        4000);
    TestTrue(
        TEXT("Rank-suppressed 1000 true damage resolves to 400"),
        Damage.TotalDamage ==
            FOGLargeNumber::FromInt64(
                400));
    TestEqual(
        TEXT("Both executors invoked the same data resolver"),
        ResolverCalls,
        2);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOGUnknownRankDoesNotInventSuppressionTest,
    "OfflineGame.Combat.Shared.UnknownRankDoesNotInventGap",
    EAutomationTestFlags::ApplicationContextMask |
        EAutomationTestFlags::EngineFilter)

bool FOGUnknownRankDoesNotInventSuppressionTest::RunTest(
    const FString& Parameters)
{
    FOGCombatUnitState Source =
        MakeUnit(
            0,
            EOGCombatPresence::Active,
            0,
            1000);
    FOGCombatUnitState Target =
        MakeUnit(
            1,
            EOGCombatPresence::Active,
            0,
            1000);

    int32 Multiplier = 0;
    FString Error;
    TestTrue(
        TEXT("Unknown Rank projection remains a valid unsuppressed interaction"),
        FOGActionCombatAdapter::ResolveRankSuppressionMultiplier(
            Source,
            Target,
            OGRankSuppressionChannels::Control(),
            FOGRankSuppressionResolver(),
            Multiplier,
            Error));
    TestEqual(
        TEXT("Combat never fabricates a Rank gap from missing projection"),
        Multiplier,
        10000);

    return true;
}

#endif
