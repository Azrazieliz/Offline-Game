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
    State = InitialState;
    Log.Reset();
    OutError.Reset();

    if (!ValidateInitialState(OutError))
    {
        State.Status = EOGTurnBattleStatus::NotStarted;
        return false;
    }

    State.Status = EOGTurnBattleStatus::Running;

    FOGCombatLogEvent Event;
    Event.Type = EOGCombatLogEventType::BattleStarted;
    Event.PayloadJson = FString::Printf(
        TEXT("{\"battle_id\":\"%s\"}"),
        *State.BattleId.ToString());
    Log.Append(MoveTemp(Event));

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

    TSet<FOGEntityId> UnitIds;
    for (const FOGCombatUnitState& Unit : State.Units)
    {
        if (!Unit.UnitEntityId.IsValid())
        {
            OutError = TEXT("Turn battle contains a unit with an invalid entity ID.");
            return false;
        }

        if (UnitIds.Contains(Unit.UnitEntityId))
        {
            OutError = TEXT("Turn battle contains duplicate unit entity IDs.");
            return false;
        }
        UnitIds.Add(Unit.UnitEntityId);
    }

    for (const FOGTurnTeamState& Team : State.Teams)
    {
        if (Team.RosterUnitIds.Num() > FOGTurnTeamState::MaxRosterSize)
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
    }

    return true;
}

int32 FOGTurnBattle::SelectNextActingUnitIndex() const
{
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

    const int32 NextIndex = SelectNextActingUnitIndex();
    if (NextIndex == INDEX_NONE)
    {
        OutError = TEXT("No combat unit can act.");
        return false;
    }

    const bool bInterrupt = Action.InterruptPriority > 0;
    if (!bInterrupt &&
        State.Units[NextIndex].UnitEntityId != Action.SourceUnitId)
    {
        OutError = TEXT("Ordinary action was submitted by a unit that is not next on the action timeline.");
        return false;
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
            FMath::Max(State.CurrentActionValue, Source->NextActionValue);

        Source->NextActionValue =
            State.CurrentActionValue + FMath::Max<int64>(1, Action.ActionDelay);
    }

    FOGCombatLogEvent Resolved;
    Resolved.Type = EOGCombatLogEventType::ActionResolved;
    Resolved.SourceUnitId = Action.SourceUnitId;
    Resolved.SkillId = Action.SkillId;
    Resolved.PayloadJson = Action.ResolutionJson;
    Log.Append(MoveTemp(Resolved));

    EvaluateBattleCompletion();
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

    if (FOGLargeNumber::Compare(Amount, FOGLargeNumber()) < 0)
    {
        OutError = TEXT("Damage amount cannot be negative.");
        return false;
    }

    FOGCombatUnitState* Target = FindMutableUnit(TargetUnitId);
    if (!Target || !Target->IsAlive())
    {
        OutError = TEXT("Damage target is missing or already defeated.");
        return false;
    }

    FOGLargeNumber NegativeAmount(-Amount.Significand, Amount.Exponent10);
    Target->CurrentHp = FOGLargeNumber::Add(Target->CurrentHp, NegativeAmount);

    if (FOGLargeNumber::Compare(Target->CurrentHp, FOGLargeNumber()) <= 0)
    {
        Target->CurrentHp = FOGLargeNumber();
        Target->Presence = EOGCombatPresence::Defeated;
    }

    FOGCombatLogEvent Damage;
    Damage.Type = EOGCombatLogEventType::DamageApplied;
    Damage.SourceUnitId = SourceUnitId;
    Damage.TargetUnitId = TargetUnitId;
    Damage.SkillId = SkillId;
    Damage.PayloadJson = FString::Printf(
        TEXT("{\"amount\":\"%s\"}"),
        *Amount.ToDebugString());
    Log.Append(MoveTemp(Damage));

    if (Target->Presence == EOGCombatPresence::Defeated)
    {
        FOGCombatLogEvent Defeat;
        Defeat.Type = EOGCombatLogEventType::UnitDefeated;
        Defeat.SourceUnitId = SourceUnitId;
        Defeat.TargetUnitId = TargetUnitId;
        Defeat.SkillId = SkillId;
        Log.Append(MoveTemp(Defeat));
    }

    EvaluateBattleCompletion();
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

    if (FOGLargeNumber::Compare(Amount, FOGLargeNumber()) < 0)
    {
        OutError = TEXT("Healing amount cannot be negative.");
        return false;
    }

    FOGCombatUnitState* Target = FindMutableUnit(TargetUnitId);
    if (!Target || Target->Presence == EOGCombatPresence::Removed)
    {
        OutError = TEXT("Healing target is missing or removed.");
        return false;
    }

    Target->CurrentHp = FOGLargeNumber::Add(Target->CurrentHp, Amount);

    // Overheal behavior is mechanic-specific and intentionally not capped here.

    FOGCombatLogEvent Healing;
    Healing.Type = EOGCombatLogEventType::HealingApplied;
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

    FOGCombatUnitState* Unit = FindMutableUnit(UnitId);
    if (!Unit)
    {
        OutError = TEXT("Combat unit not found.");
        return false;
    }

    if (NewPresence == EOGCombatPresence::Active)
    {
        if (!Unit->IsAlive())
        {
            OutError = TEXT("Defeated unit cannot become active without an explicit revival mechanic.");
            return false;
        }

        if (CountActiveForTeam(State.Units, Unit->TeamIndex) >=
            FOGTurnTeamState::MaxActiveSize)
        {
            OutError = TEXT("Cannot exceed six active characters on a turn-combat team.");
            return false;
        }
    }

    Unit->Presence = NewPresence;

    FOGCombatLogEvent Event;
    Event.Type = EOGCombatLogEventType::PresenceChanged;
    Event.SourceUnitId = UnitId;
    Event.PayloadJson = FString::Printf(
        TEXT("{\"presence\":%d}"),
        static_cast<int32>(NewPresence));
    Log.Append(MoveTemp(Event));

    EvaluateBattleCompletion();
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

void FOGTurnBattle::EvaluateBattleCompletion()
{
    if (State.Status != EOGTurnBattleStatus::Running)
    {
        return;
    }

    int32 TeamsWithLivingUnits = 0;

    for (const FOGTurnTeamState& Team : State.Teams)
    {
        const bool bHasLivingUnit = State.Units.ContainsByPredicate(
            [&Team](const FOGCombatUnitState& Unit)
            {
                return Unit.TeamIndex == Team.TeamIndex &&
                       Unit.Presence != EOGCombatPresence::Removed &&
                       Unit.IsAlive();
            });

        if (bHasLivingUnit)
        {
            ++TeamsWithLivingUnits;
        }
    }

    if (TeamsWithLivingUnits <= 1)
    {
        State.Status = EOGTurnBattleStatus::Completed;

        FOGCombatLogEvent Event;
        Event.Type = EOGCombatLogEventType::BattleEnded;
        Log.Append(MoveTemp(Event));
    }
}
