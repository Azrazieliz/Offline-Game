#include "Combat/OGTurnBattle.h"

namespace
{
int32 CountActiveForTeam(
    const TArray<FOGCombatUnitState>& Units,
    int32 TeamIndex)
{
    int32 Count = 0;
    for (const FOGCombatUnitState& Unit : Units)
    {
        if (Unit.TeamIndex == TeamIndex &&
            Unit.Presence == EOGCombatPresence::Active &&
            Unit.IsAlive())
        {
            ++Count;
        }
    }
    return Count;
}
}

bool FOGTurnBattle::Initialize(
    const FOGTurnBattleState& InitialState,
    FString& OutError)
{
    return Initialize(
        InitialState,
        TArray<FOGCombatTriggerBinding>(),
        FOGCombatConditionEvaluator(),
        OutError);
}

bool FOGTurnBattle::Initialize(
    const FOGTurnBattleState& InitialState,
    TArray<FOGCombatTriggerBinding> TriggerBindings,
    FOGCombatConditionEvaluator ConditionEvaluator,
    FString& OutError)
{
    State = InitialState;
    Log.Reset();
    TriggerRuntime.Reset();
    PendingTriggeredActions.Reset();
    OutstandingTriggeredActionSequences.Reset();
    PendingDefeatedUnitIds.Reset();
    OutError.Reset();

    if (!ValidateInitialState(OutError))
    {
        State.Status = EOGTurnBattleStatus::NotStarted;
        return false;
    }

    if (!NormalizeOpeningLanes(OutError))
    {
        State.Status = EOGTurnBattleStatus::NotStarted;
        return false;
    }

    TriggerRuntime.SetBindings(MoveTemp(TriggerBindings));
    TriggerRuntime.SetConditionEvaluator(MoveTemp(ConditionEvaluator));

    State.Status = EOGTurnBattleStatus::Running;

    const int64 StartSequence = Log.GetNextSequence();

    FOGCombatLogEvent Event;
    Event.Type = EOGCombatLogEventType::BattleStarted;
    Event.PayloadJson = FString::Printf(
        TEXT("{\"battle_id\":\"%s\"}"),
        *State.BattleId.ToString());
    Log.Append(MoveTemp(Event));

    QueueTriggerEvent(
        OGCombatEventNames::BattleStart(),
        FOGEntityId(),
        FOGEntityId(),
        FOGContentId(),
        StartSequence);

    FlushTriggerRuntimeQueueToBattleQueue();
    EvaluateBattleCompletion();
    return true;
}

bool FOGTurnBattle::ValidateInitialState(FString& OutError) const
{
    if (!State.BattleId.IsValid())
    {
        OutError = TEXT("Battle requires a valid BattleId.");
        return false;
    }

    if (State.Teams.Num() < 2)
    {
        OutError = TEXT("Turn battle requires at least two teams.");
        return false;
    }

    TSet<int32> TeamIndices;
    for (const FOGTurnTeamState& Team : State.Teams)
    {
        if (TeamIndices.Contains(Team.TeamIndex))
        {
            OutError = TEXT("Turn battle contains duplicate team indices.");
            return false;
        }
        TeamIndices.Add(Team.TeamIndex);
    }

    TSet<FOGEntityId> UnitIds;
    for (const FOGCombatUnitState& Unit : State.Units)
    {
        if (!Unit.UnitEntityId.IsValid())
        {
            OutError = TEXT("Turn battle contains a unit with an invalid entity ID.");
            return false;
        }

        if (!TeamIndices.Contains(Unit.TeamIndex))
        {
            OutError = TEXT("Turn battle contains a unit assigned to an unknown team.");
            return false;
        }

        if (UnitIds.Contains(Unit.UnitEntityId))
        {
            OutError = TEXT("Turn battle contains duplicate unit entity IDs.");
            return false;
        }

        UnitIds.Add(Unit.UnitEntityId);
    }

    TSet<FOGEntityId> FormationIds;

    for (const FOGTurnTeamState& Team : State.Teams)
    {
        if (Team.Lanes.Num() > FOGTurnTeamState::MaxLaneCount)
        {
            OutError = TEXT("Turn team exceeds six succession lanes.");
            return false;
        }

        if (Team.GetRosterSize() > FOGTurnTeamState::MaxRosterSize)
        {
            OutError = TEXT("Turn team exceeds the 18-character roster limit.");
            return false;
        }

        if (CountActiveForTeam(State.Units, Team.TeamIndex) >
            FOGTurnTeamState::MaxActiveSize)
        {
            OutError = TEXT("Turn team exceeds the 6-character active limit.");
            return false;
        }

        TSet<int32> LaneIndices;

        for (const FOGTurnSuccessionLane& Lane : Team.Lanes)
        {
            if (Lane.LaneIndex < 0 ||
                Lane.LaneIndex >= FOGTurnTeamState::MaxLaneCount)
            {
                OutError = TEXT("Succession lane index must be between 0 and 5.");
                return false;
            }

            if (LaneIndices.Contains(Lane.LaneIndex))
            {
                OutError = TEXT("Turn team contains duplicate succession lane indices.");
                return false;
            }

            LaneIndices.Add(Lane.LaneIndex);

            if (Lane.OrderedUnitIds.IsEmpty() ||
                Lane.OrderedUnitIds.Num() >
                    FOGTurnTeamState::MaxLaneSize)
            {
                OutError = TEXT("Each succession lane must contain one to three characters.");
                return false;
            }

            int32 ActiveInOriginLane = 0;

            for (const FOGEntityId& UnitId : Lane.OrderedUnitIds)
            {
                if (!UnitIds.Contains(UnitId))
                {
                    OutError = TEXT("Formation references a combat unit that does not exist.");
                    return false;
                }

                if (FormationIds.Contains(UnitId))
                {
                    OutError = TEXT("A combat unit appears in more than one succession lane.");
                    return false;
                }

                const FOGCombatUnitState* Unit = FindUnit(UnitId);
                if (!Unit || Unit->TeamIndex != Team.TeamIndex)
                {
                    OutError = TEXT("Succession lane contains a unit assigned to another team.");
                    return false;
                }

                if (Unit->Presence == EOGCombatPresence::Active &&
                    Unit->IsAlive())
                {
                    ++ActiveInOriginLane;
                }

                FormationIds.Add(UnitId);
            }

            if (ActiveInOriginLane > 1)
            {
                OutError = TEXT("Initial formation may not start with multiple active units from the same origin succession lane.");
                return false;
            }
        }
    }

    if (FormationIds.Num() != State.Units.Num())
    {
        OutError = TEXT("Every combat unit must belong to exactly one succession lane.");
        return false;
    }

    return true;
}

bool FOGTurnBattle::NormalizeOpeningLanes(FString& OutError)
{
    OutError.Reset();

    for (FOGCombatUnitState& Unit : State.Units)
    {
        if (Unit.Presence != EOGCombatPresence::Active ||
            !Unit.IsAlive())
        {
            Unit.OccupiedLaneIndex = INDEX_NONE;
        }
    }

    for (const FOGTurnTeamState& Team : State.Teams)
    {
        for (const FOGTurnSuccessionLane& Lane : Team.Lanes)
        {
            FOGCombatUnitState* ExistingActive = nullptr;

            for (const FOGEntityId& UnitId : Lane.OrderedUnitIds)
            {
                FOGCombatUnitState* Unit = FindMutableUnit(UnitId);
                if (Unit &&
                    Unit->Presence == EOGCombatPresence::Active &&
                    Unit->IsAlive())
                {
                    ExistingActive = Unit;
                    break;
                }
            }

            if (ExistingActive)
            {
                ExistingActive->OccupiedLaneIndex = Lane.LaneIndex;
                continue;
            }

            for (const FOGEntityId& UnitId : Lane.OrderedUnitIds)
            {
                FOGCombatUnitState* Unit = FindMutableUnit(UnitId);
                if (!Unit ||
                    !Unit->IsAlive() ||
                    Unit->Presence == EOGCombatPresence::Defeated ||
                    Unit->Presence == EOGCombatPresence::Removed)
                {
                    continue;
                }

                Unit->Presence = EOGCombatPresence::Active;
                Unit->OccupiedLaneIndex = Lane.LaneIndex;
                Unit->NextActionValue =
                    FMath::Max(
                        Unit->NextActionValue,
                        State.CurrentActionValue +
                            FMath::Max<int64>(
                                1,
                                Unit->DefaultActionDelay));
                break;
            }
        }

        // If an entire preferred lane is exhausted, do not preserve a useless
        // hole while another origin lane still has surviving reserves.
        for (const FOGTurnSuccessionLane& Lane : Team.Lanes)
        {
            if (IsBattlefieldLaneOccupied(
                    Team.TeamIndex,
                    Lane.LaneIndex))
            {
                continue;
            }

            FOGCombatUnitState* Fallback =
                FindFallbackReserve(
                    Team.TeamIndex);

            if (!Fallback)
            {
                continue;
            }

            Fallback->Presence = EOGCombatPresence::Active;
            Fallback->OccupiedLaneIndex = Lane.LaneIndex;
            Fallback->NextActionValue =
                FMath::Max(
                    Fallback->NextActionValue,
                    State.CurrentActionValue +
                        FMath::Max<int64>(
                            1,
                            Fallback->DefaultActionDelay));
        }
    }

    return true;
}

int32 FOGTurnBattle::SelectNextActingUnitIndex() const
{
    if (!OutstandingTriggeredActionSequences.IsEmpty())
    {
        return INDEX_NONE;
    }

    int32 BestIndex = INDEX_NONE;

    for (int32 Index = 0; Index < State.Units.Num(); ++Index)
    {
        const FOGCombatUnitState& Candidate = State.Units[Index];
        if (!Candidate.CanAct())
        {
            continue;
        }

        if (BestIndex == INDEX_NONE)
        {
            BestIndex = Index;
            continue;
        }

        const FOGCombatUnitState& Best = State.Units[BestIndex];

        if (Candidate.NextActionValue < Best.NextActionValue)
        {
            BestIndex = Index;
            continue;
        }

        if (Candidate.NextActionValue == Best.NextActionValue &&
            Candidate.UnitEntityId.ToString().Compare(
                Best.UnitEntityId.ToString(),
                ESearchCase::CaseSensitive) < 0)
        {
            BestIndex = Index;
        }
    }

    return BestIndex;
}

bool FOGTurnBattle::ApplyResolvedAction(
    const FOGResolvedCombatAction& Action,
    FString& OutError)
{
    OutError.Reset();

    if (State.Status != EOGTurnBattleStatus::Running)
    {
        OutError = TEXT("Battle is not running.");
        return false;
    }

    FOGCombatUnitState* Source = FindMutableUnit(Action.SourceUnitId);
    if (!Source || !Source->CanAct())
    {
        OutError = TEXT("Resolved action source is missing, defeated, or not active.");
        return false;
    }

    const bool bInterrupt = Action.InterruptPriority > 0;

    if (!bInterrupt)
    {
        if (!OutstandingTriggeredActionSequences.IsEmpty())
        {
            OutError = TEXT("Triggered actions must resolve before the next ordinary turn action.");
            return false;
        }

        const int32 NextIndex = SelectNextActingUnitIndex();
        if (NextIndex == INDEX_NONE)
        {
            OutError = TEXT("No combat unit can act.");
            return false;
        }

        if (State.Units[NextIndex].UnitEntityId !=
            Action.SourceUnitId)
        {
            OutError = TEXT("Ordinary action was submitted by a unit that is not next on the action timeline.");
            return false;
        }
    }

    FOGCombatLogEvent Declared;
    Declared.Type = EOGCombatLogEventType::ActionDeclared;
    Declared.SourceUnitId = Action.SourceUnitId;
    Declared.SkillId = Action.SkillId;
    Declared.PayloadJson = FString::Printf(
        TEXT("{\"interrupt_priority\":%d}"),
        Action.InterruptPriority);
    Log.Append(MoveTemp(Declared));

    if (!bInterrupt)
    {
        State.CurrentActionValue =
            FMath::Max(
                State.CurrentActionValue,
                Source->NextActionValue);

        Source->NextActionValue =
            State.CurrentActionValue +
            FMath::Max<int64>(
                1,
                Action.ActionDelay);
    }

    FOGCombatLogEvent Resolved;
    Resolved.Type = EOGCombatLogEventType::ActionResolved;
    Resolved.SourceUnitId = Action.SourceUnitId;
    Resolved.SkillId = Action.SkillId;
    Resolved.PayloadJson = Action.ResolutionJson;
    Log.Append(MoveTemp(Resolved));

    FlushTriggerRuntimeQueueToBattleQueue();

    if (OutstandingTriggeredActionSequences.IsEmpty())
    {
        return ResolvePendingReplacements(OutError);
    }

    return true;
}

bool FOGTurnBattle::ApplyDamage(
    const FOGEntityId& SourceUnitId,
    const FOGEntityId& TargetUnitId,
    const FOGLargeNumber& Amount,
    const FOGContentId& SkillId,
    FString& OutError)
{
    OutError.Reset();

    if (FOGLargeNumber::Compare(
            Amount,
            FOGLargeNumber()) < 0)
    {
        OutError = TEXT("Damage amount cannot be negative.");
        return false;
    }

    FOGCombatUnitState* Target =
        FindMutableUnit(TargetUnitId);

    if (!Target || !Target->IsAlive())
    {
        OutError = TEXT("Damage target is missing or already defeated.");
        return false;
    }

    FOGLargeNumber NegativeAmount(
        -Amount.Significand,
        Amount.Exponent10);

    Target->CurrentHp =
        FOGLargeNumber::Add(
            Target->CurrentHp,
            NegativeAmount);

    FOGCombatLogEvent Damage;
    Damage.Type =
        EOGCombatLogEventType::DamageApplied;
    Damage.SourceUnitId = SourceUnitId;
    Damage.TargetUnitId = TargetUnitId;
    Damage.SkillId = SkillId;
    Damage.PayloadJson = FString::Printf(
        TEXT("{\"amount\":\"%s\"}"),
        *Amount.ToDebugString());
    Log.Append(MoveTemp(Damage));

    if (FOGLargeNumber::Compare(
            Target->CurrentHp,
            FOGLargeNumber()) <= 0)
    {
        Target->CurrentHp = FOGLargeNumber();
        Target->Presence =
            EOGCombatPresence::Defeated;

        if (!PendingDefeatedUnitIds.Contains(
                TargetUnitId))
        {
            PendingDefeatedUnitIds.Add(
                TargetUnitId);
        }

        const int64 DefeatSequence =
            Log.GetNextSequence();

        FOGCombatLogEvent Defeat;
        Defeat.Type =
            EOGCombatLogEventType::UnitDefeated;
        Defeat.SourceUnitId = SourceUnitId;
        Defeat.TargetUnitId = TargetUnitId;
        Defeat.SkillId = SkillId;
        Log.Append(MoveTemp(Defeat));

        QueueTriggerEvent(
            OGCombatEventNames::Defeat(),
            SourceUnitId,
            TargetUnitId,
            SkillId,
            DefeatSequence);
    }

    return true;
}

bool FOGTurnBattle::ApplyHealing(
    const FOGEntityId& SourceUnitId,
    const FOGEntityId& TargetUnitId,
    const FOGLargeNumber& Amount,
    const FOGContentId& SkillId,
    FString& OutError)
{
    OutError.Reset();

    if (FOGLargeNumber::Compare(
            Amount,
            FOGLargeNumber()) < 0)
    {
        OutError = TEXT("Healing amount cannot be negative.");
        return false;
    }

    FOGCombatUnitState* Target =
        FindMutableUnit(TargetUnitId);

    if (!Target ||
        Target->Presence ==
            EOGCombatPresence::Removed)
    {
        OutError = TEXT("Healing target is missing or removed.");
        return false;
    }

    Target->CurrentHp =
        FOGLargeNumber::Add(
            Target->CurrentHp,
            Amount);

    FOGCombatLogEvent Healing;
    Healing.Type =
        EOGCombatLogEventType::HealingApplied;
    Healing.SourceUnitId = SourceUnitId;
    Healing.TargetUnitId = TargetUnitId;
    Healing.SkillId = SkillId;
    Healing.PayloadJson = FString::Printf(
        TEXT("{\"amount\":\"%s\"}"),
        *Amount.ToDebugString());
    Log.Append(MoveTemp(Healing));

    return true;
}

bool FOGTurnBattle::ChangePresence(
    const FOGEntityId& UnitId,
    EOGCombatPresence NewPresence,
    FString& OutError)
{
    OutError.Reset();

    FOGCombatUnitState* Unit =
        FindMutableUnit(UnitId);

    if (!Unit)
    {
        OutError = TEXT("Combat unit not found.");
        return false;
    }

    if (NewPresence ==
        EOGCombatPresence::Active)
    {
        if (!Unit->IsAlive())
        {
            OutError = TEXT("Defeated unit cannot become active without first being restored above zero HP.");
            return false;
        }

        if (Unit->OccupiedLaneIndex == INDEX_NONE)
        {
            OutError = TEXT("Activation requires an occupied battlefield lane. Use promotion/replacement mechanics for a reserve.");
            return false;
        }

        if (CountActiveForTeam(
                State.Units,
                Unit->TeamIndex) >=
            FOGTurnTeamState::MaxActiveSize &&
            Unit->Presence !=
                EOGCombatPresence::Active)
        {
            OutError = TEXT("Cannot exceed six active characters on a turn-combat team.");
            return false;
        }
    }

    Unit->Presence = NewPresence;

    if (NewPresence !=
            EOGCombatPresence::Active &&
        NewPresence !=
            EOGCombatPresence::Defeated)
    {
        Unit->OccupiedLaneIndex = INDEX_NONE;
    }

    FOGCombatLogEvent Event;
    Event.Type =
        EOGCombatLogEventType::PresenceChanged;
    Event.SourceUnitId = UnitId;
    Event.PayloadJson = FString::Printf(
        TEXT("{\"presence\":%d}"),
        static_cast<int32>(NewPresence));
    Log.Append(MoveTemp(Event));

    return true;
}

bool FOGTurnBattle::ResolvePendingReplacements(
    FString& OutError)
{
    OutError.Reset();

    if (!OutstandingTriggeredActionSequences.IsEmpty())
    {
        OutError = TEXT("Cannot perform succession before all defeat-triggered actions have resolved.");
        return false;
    }

    TArray<FOGEntityId> Pending =
        MoveTemp(PendingDefeatedUnitIds);
    PendingDefeatedUnitIds.Reset();

    // Pass 1: direct successors. Revival has priority because a unit restored to
    // Active keeps its occupied lane and therefore creates no vacancy.
    for (const FOGEntityId& DefeatedId : Pending)
    {
        FOGCombatUnitState* Defeated =
            FindMutableUnit(DefeatedId);

        if (!Defeated)
        {
            OutError = TEXT("Pending defeated unit no longer exists.");
            return false;
        }

        if (Defeated->Presence ==
                EOGCombatPresence::Active &&
            Defeated->IsAlive())
        {
            continue;
        }

        const int32 VacatedLane =
            Defeated->OccupiedLaneIndex;

        Defeated->OccupiedLaneIndex =
            INDEX_NONE;

        if (VacatedLane == INDEX_NONE ||
            IsBattlefieldLaneOccupied(
                Defeated->TeamIndex,
                VacatedLane))
        {
            continue;
        }

        if (FOGCombatUnitState* Direct =
                FindPreferredDirectSuccessor(
                    *Defeated))
        {
            PromoteIntoLane(
                *Direct,
                VacatedLane,
                TEXT("direct_successor"));
        }
    }

    // Pass 2: global team rebalance. Preferred succession lanes are not sacred
    // empty slots. If a lane is exhausted, surviving reserves from another
    // origin lane may fill it.
    for (const FOGTurnTeamState& Team :
         State.Teams)
    {
        TArray<int32> BattlefieldLanes;
        for (const FOGTurnSuccessionLane& Lane :
             Team.Lanes)
        {
            BattlefieldLanes.Add(
                Lane.LaneIndex);
        }

        BattlefieldLanes.Sort();

        for (const int32 LaneIndex :
             BattlefieldLanes)
        {
            if (IsBattlefieldLaneOccupied(
                    Team.TeamIndex,
                    LaneIndex))
            {
                continue;
            }

            FOGCombatUnitState* Fallback =
                FindFallbackReserve(
                    Team.TeamIndex);

            if (!Fallback)
            {
                continue;
            }

            PromoteIntoLane(
                *Fallback,
                LaneIndex,
                TEXT("cross_lane_rebalance"));
        }
    }

    FlushTriggerRuntimeQueueToBattleQueue();

    if (OutstandingTriggeredActionSequences.IsEmpty())
    {
        EvaluateBattleCompletion();
    }

    return true;
}

TArray<FOGQueuedTriggeredAction>
FOGTurnBattle::DrainTriggeredActions()
{
    TArray<FOGQueuedTriggeredAction> Result =
        MoveTemp(PendingTriggeredActions);
    PendingTriggeredActions.Reset();
    return Result;
}

bool FOGTurnBattle::CompleteTriggeredAction(
    int64 QueueSequence,
    FString& OutError)
{
    OutError.Reset();

    if (!OutstandingTriggeredActionSequences.Contains(
            QueueSequence))
    {
        OutError = TEXT("Triggered action sequence is unknown or already completed.");
        return false;
    }

    OutstandingTriggeredActionSequences.Remove(
        QueueSequence);

    // Triggered effects may themselves have generated new events.
    FlushTriggerRuntimeQueueToBattleQueue();

    if (OutstandingTriggeredActionSequences.IsEmpty())
    {
        return ResolvePendingReplacements(
            OutError);
    }

    return true;
}

FOGCombatUnitState* FOGTurnBattle::FindMutableUnit(
    const FOGEntityId& UnitId)
{
    return State.Units.FindByPredicate(
        [&UnitId](const FOGCombatUnitState& Unit)
        {
            return Unit.UnitEntityId == UnitId;
        });
}

const FOGCombatUnitState* FOGTurnBattle::FindUnit(
    const FOGEntityId& UnitId) const
{
    return State.Units.FindByPredicate(
        [&UnitId](const FOGCombatUnitState& Unit)
        {
            return Unit.UnitEntityId == UnitId;
        });
}

const FOGTurnTeamState* FOGTurnBattle::FindTeam(
    int32 TeamIndex) const
{
    return State.Teams.FindByPredicate(
        [TeamIndex](const FOGTurnTeamState& Team)
        {
            return Team.TeamIndex ==
                   TeamIndex;
        });
}

const FOGTurnSuccessionLane*
FOGTurnBattle::FindOriginLane(
    int32 TeamIndex,
    const FOGEntityId& UnitId,
    int32& OutUnitDepth) const
{
    OutUnitDepth = INDEX_NONE;

    const FOGTurnTeamState* Team =
        FindTeam(TeamIndex);

    if (!Team)
    {
        return nullptr;
    }

    for (const FOGTurnSuccessionLane& Lane :
         Team->Lanes)
    {
        const int32 Index =
            Lane.OrderedUnitIds.IndexOfByPredicate(
                [&UnitId](
                    const FOGEntityId& Candidate)
                {
                    return Candidate ==
                           UnitId;
                });

        if (Index != INDEX_NONE)
        {
            OutUnitDepth = Index;
            return &Lane;
        }
    }

    return nullptr;
}

FOGCombatUnitState*
FOGTurnBattle::FindPreferredDirectSuccessor(
    const FOGCombatUnitState& Defeated)
{
    int32 Depth = INDEX_NONE;
    const FOGTurnSuccessionLane* Lane =
        FindOriginLane(
            Defeated.TeamIndex,
            Defeated.UnitEntityId,
            Depth);

    if (!Lane)
    {
        return nullptr;
    }

    for (int32 Index = Depth + 1;
         Index < Lane->OrderedUnitIds.Num();
         ++Index)
    {
        FOGCombatUnitState* Candidate =
            FindMutableUnit(
                Lane->OrderedUnitIds[Index]);

        if (Candidate &&
            Candidate->Presence ==
                EOGCombatPresence::Reserve &&
            Candidate->IsAlive())
        {
            return Candidate;
        }
    }

    return nullptr;
}

FOGCombatUnitState*
FOGTurnBattle::FindFallbackReserve(
    int32 TeamIndex)
{
    FOGCombatUnitState* Best = nullptr;
    int32 BestDepth = INDEX_NONE;
    int32 BestOriginLane = MAX_int32;
    bool bBestOriginOccupied = false;

    for (FOGCombatUnitState& Candidate :
         State.Units)
    {
        if (Candidate.TeamIndex != TeamIndex ||
            Candidate.Presence !=
                EOGCombatPresence::Reserve ||
            !Candidate.IsAlive())
        {
            continue;
        }

        int32 Depth = INDEX_NONE;
        const FOGTurnSuccessionLane* Origin =
            FindOriginLane(
                TeamIndex,
                Candidate.UnitEntityId,
                Depth);

        if (!Origin)
        {
            continue;
        }

        const bool bOriginOccupied =
            IsBattlefieldLaneOccupied(
                TeamIndex,
                Origin->LaneIndex);

        const bool bBetter =
            Best == nullptr ||
            (bOriginOccupied &&
             !bBestOriginOccupied) ||
            (bOriginOccupied ==
                 bBestOriginOccupied &&
             Depth > BestDepth) ||
            (bOriginOccupied ==
                 bBestOriginOccupied &&
             Depth == BestDepth &&
             Origin->LaneIndex <
                 BestOriginLane) ||
            (bOriginOccupied ==
                 bBestOriginOccupied &&
             Depth == BestDepth &&
             Origin->LaneIndex ==
                 BestOriginLane &&
             Candidate.UnitEntityId.ToString().Compare(
                 Best->UnitEntityId.ToString(),
                 ESearchCase::CaseSensitive) < 0);

        if (bBetter)
        {
            Best = &Candidate;
            BestDepth = Depth;
            BestOriginLane =
                Origin->LaneIndex;
            bBestOriginOccupied =
                bOriginOccupied;
        }
    }

    return Best;
}

bool FOGTurnBattle::IsBattlefieldLaneOccupied(
    int32 TeamIndex,
    int32 BattlefieldLane) const
{
    return State.Units.ContainsByPredicate(
        [TeamIndex, BattlefieldLane](
            const FOGCombatUnitState& Unit)
        {
            return Unit.TeamIndex ==
                       TeamIndex &&
                   Unit.Presence ==
                       EOGCombatPresence::Active &&
                   Unit.IsAlive() &&
                   Unit.OccupiedLaneIndex ==
                       BattlefieldLane;
        });
}

void FOGTurnBattle::PromoteIntoLane(
    FOGCombatUnitState& Unit,
    int32 BattlefieldLane,
    const TCHAR* Reason)
{
    Unit.Presence =
        EOGCombatPresence::Active;
    Unit.OccupiedLaneIndex =
        BattlefieldLane;

    Unit.NextActionValue =
        FMath::Max(
            Unit.NextActionValue,
            State.CurrentActionValue +
                FMath::Max<int64>(
                    1,
                    Unit.DefaultActionDelay));

    const int64 EntrySequence =
        Log.GetNextSequence();

    FOGCombatLogEvent Event;
    Event.Type =
        EOGCombatLogEventType::PresenceChanged;
    Event.SourceUnitId =
        Unit.UnitEntityId;
    Event.PayloadJson =
        FString::Printf(
            TEXT("{\"presence\":%d,\"lane\":%d,\"reason\":\"%s\"}"),
            static_cast<int32>(
                EOGCombatPresence::Active),
            BattlefieldLane,
            Reason);
    Log.Append(MoveTemp(Event));

    QueueTriggerEvent(
        OGCombatEventNames::Entry(),
        Unit.UnitEntityId,
        Unit.UnitEntityId,
        FOGContentId(),
        EntrySequence);
}

void FOGTurnBattle::QueueTriggerEvent(
    FName EventType,
    const FOGEntityId& SourceUnitId,
    const FOGEntityId& TargetUnitId,
    const FOGContentId& SkillId,
    int64 SourceSequence)
{
    FOGCombatTriggerContext Context;
    Context.EventType = EventType;
    Context.SourceUnitId =
        SourceUnitId;
    Context.TargetUnitId =
        TargetUnitId;
    Context.SkillId = SkillId;
    Context.SourceSequence =
        SourceSequence;

    TriggerRuntime.QueueForEvent(
        Context);
}

void FOGTurnBattle::FlushTriggerRuntimeQueueToBattleQueue()
{
    TArray<FOGQueuedTriggeredAction> NewlyQueued =
        TriggerRuntime.DrainQueuedActions();

    for (FOGQueuedTriggeredAction& Queued :
         NewlyQueued)
    {
        OutstandingTriggeredActionSequences.Add(
            Queued.QueueSequence);

        FOGCombatLogEvent LogEvent;
        LogEvent.Type =
            EOGCombatLogEventType::TriggeredActionQueued;
        LogEvent.SourceUnitId =
            Queued.OwnerUnitId;
        LogEvent.SkillId =
            Queued.ActionId;
        LogEvent.PayloadJson =
            FString::Printf(
                TEXT("{\"queue_sequence\":%lld,\"trigger\":\"%s\",\"source_sequence\":%lld}"),
                Queued.QueueSequence,
                *Queued.TriggerId.ToString(),
                Queued.SourceSequence);
        Log.Append(MoveTemp(LogEvent));

        PendingTriggeredActions.Add(
            MoveTemp(Queued));
    }
}

void FOGTurnBattle::EvaluateBattleCompletion()
{
    if (State.Status !=
        EOGTurnBattleStatus::Running)
    {
        return;
    }

    if (!OutstandingTriggeredActionSequences.IsEmpty())
    {
        return;
    }

    int32 TeamsWithLivingUnits = 0;

    for (const FOGTurnTeamState& Team :
         State.Teams)
    {
        const bool bHasLivingUnit =
            State.Units.ContainsByPredicate(
                [&Team](
                    const FOGCombatUnitState& Unit)
                {
                    return Unit.TeamIndex ==
                               Team.TeamIndex &&
                           Unit.Presence !=
                               EOGCombatPresence::Removed &&
                           Unit.IsAlive();
                });

        if (bHasLivingUnit)
        {
            ++TeamsWithLivingUnits;
        }
    }

    if (TeamsWithLivingUnits <= 1)
    {
        State.Status =
            EOGTurnBattleStatus::Completed;

        FOGCombatLogEvent Event;
        Event.Type =
            EOGCombatLogEventType::BattleEnded;
        Log.Append(MoveTemp(Event));
    }
}
