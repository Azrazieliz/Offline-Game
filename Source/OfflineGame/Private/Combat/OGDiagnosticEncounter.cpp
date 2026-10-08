#include "Combat/OGDiagnosticEncounter.h"

FOGCombatUnitState OGDiagnosticContent::MakeUnit(const FOGEntityId& Id, int32 Kit, int32 Team)
{
    FOGCombatUnitState Unit;
    Unit.UnitEntityId = Id;
    Unit.IdentityId = FOGContentId(FString::Printf(TEXT("diagnostic:identity.kit_%d"), Kit));
    Unit.TeamIndex = Team;
    Unit.Presence = EOGCombatPresence::Active;
    Unit.Stats.MaxHp = FOGLargeNumber::FromInt64(Kit == 1 ? 160 : 120);
    Unit.Stats.Attack = FOGLargeNumber::FromInt64(Kit == 2 ? 22 : 18);
    Unit.Stats.Defense = FOGLargeNumber::FromInt64(Kit == 1 ? 12 : 5);
    Unit.Stats.Speed = Kit == 2 ? 120 : 100;
    Unit.CurrentHp = Unit.Stats.MaxHp;
    Unit.DefaultActionDelay = Kit == 2 ? 80 : 100;
    Unit.NextActionValue = Unit.DefaultActionDelay;
    Unit.SkillSet.ActiveSkills = {
        FOGContentId(FString::Printf(TEXT("diagnostic:skill.kit_%d.strike"), Kit)),
        FOGContentId(FString::Printf(TEXT("diagnostic:skill.kit_%d.burst"), Kit))};
    Unit.SkillSet.UltimateSkill = FOGContentId(FString::Printf(TEXT("diagnostic:skill.kit_%d.ultimate"), Kit));
    return Unit;
}

bool OGDiagnosticContent::ResolveAction(const FOGCombatUnitState& Unit,
    EOGDiagnosticCommand Command, FOGDiagnosticActionDefinition& Out, FString& Error)
{
    Error.Reset();
    Out = FOGDiagnosticActionDefinition();
    Out.ActionDelay = Unit.DefaultActionDelay;
    int32 Kit = INDEX_NONE;
    for (int32 Index = 0; Index <= 4; ++Index)
        if (Unit.IdentityId == FOGContentId(FString::Printf(TEXT("diagnostic:identity.kit_%d"), Index))) Kit = Index;
    if (Kit == INDEX_NONE) { Error = TEXT("No authored diagnostic kit matches this identity."); return false; }
    if (Command == EOGDiagnosticCommand::Basic)
    {
        Out.SkillId = FOGContentId(TEXT("diagnostic:skill.basic"));
        return true;
    }
    if (Command == EOGDiagnosticCommand::Ultimate)
    {
        Out.SkillId = Unit.SkillSet.UltimateSkill;
        Out.AttackMultiplierBps = Kit == 1 ? 28000 : Kit == 2 ? 36000 : 32000;
        Out.ActionDelay *= 2;
    }
    else
    {
        const int32 Slot = Command == EOGDiagnosticCommand::Skill1 ? 0 : 1;
        if (!Unit.SkillSet.ActiveSkills.IsValidIndex(Slot))
        {
            Error = TEXT("Selected active skill is absent from this unit's resolved kit.");
            return false;
        }
        Out.SkillId = Unit.SkillSet.ActiveSkills[Slot];
        Out.AttackMultiplierBps = Slot == 0 ? (Kit == 1 ? 18000 : Kit == 2 ? 13500 : 15000) :
            (Kit == 1 ? 17500 : Kit == 2 ? 22500 : 20000);
    }
    if (!Out.SkillId.IsValid())
    {
        Error = TEXT("Selected diagnostic skill is invalid.");
        return false;
    }
    // Reject arbitrary installed content: this bounded resolver only knows its fixture.
    const TCHAR* Suffix = Command == EOGDiagnosticCommand::Ultimate ? TEXT("ultimate") :
        Command == EOGDiagnosticCommand::Skill1 ? TEXT("strike") : TEXT("burst");
    if (Out.SkillId != FOGContentId(FString::Printf(TEXT("diagnostic:skill.kit_%d.%s"), Kit, Suffix)))
    {
        Error = TEXT("No diagnostic effect definition exists for the selected skill.");
        return false;
    }
    return true;
}

FOGDamageResolution OGDiagnosticContent::ResolveDamage(
    const FOGDiagnosticActionDefinition& Definition, const FOGCombatUnitState& Source,
    const FOGCombatUnitState& Target, int32 RankMultiplierBps, FOGDeterministicRng& Rng, int32 ContentMultiplierBps)
{
    FOGDamageRequest Request;
    Request.BaseDamage = FOGLargeNumber::ScaleByBasisPoints(Source.Stats.Attack, Definition.AttackMultiplierBps);
    Request.DefenseReference = Source.Stats.Attack;
    Request.RankSuppressionMultiplierBps = RankMultiplierBps;
    Request.AdditionalMultiplierBps.Add(ContentMultiplierBps);
    return FOGCombatMath::ResolveDamage(Request, Source.Stats, Target.Stats, Rng);
}

bool FOGDiagnosticEncounter::Start(const TArray<FOGCombatUnitState>& Players,
    const TArray<FOGCombatUnitState>& Enemies, uint64 Seed,
    FOGRankSuppressionResolver RankResolver, FString& Error,
    const TMap<FOGEntityId, int32>& StartingEnergy)
{
    Error.Reset();
    if (Battle.GetState().Status == EOGTurnBattleStatus::Running || Players.IsEmpty() || Enemies.IsEmpty())
    {
        Error = TEXT("Encounter requires both teams and no running encounter.");
        return false;
    }
    FOGTurnBattleState Initial;
    Initial.BattleId = FOGEntityId::NewId();
    for (int32 TeamIndex = 0; TeamIndex < 2; ++TeamIndex)
    {
        FOGTurnTeamState Team;
        Team.TeamIndex = TeamIndex;
        const TArray<FOGCombatUnitState>& Input = TeamIndex == 0 ? Players : Enemies;
        if (Input.Num() > FOGTurnTeamState::MaxLaneCount)
        {
            Error = TEXT("This diagnostic encounter supports at most six opening units per side.");
            return false;
        }
        for (int32 Index = 0; Index < Input.Num(); ++Index)
        {
            FOGCombatUnitState Unit = FOGActionCombatAdapter::MakeActionSnapshot(Input[Index]);
            Unit.TeamIndex = TeamIndex;
            Unit.Presence = Unit.IsAlive() ? EOGCombatPresence::Active : EOGCombatPresence::Defeated;
            Unit.OccupiedLaneIndex = Index;
            Unit.NextActionValue = Unit.DefaultActionDelay;
            FOGTurnSuccessionLane Lane;
            Lane.LaneIndex = Index;
            Lane.OrderedUnitIds.Add(Unit.UnitEntityId);
            Team.Lanes.Add(Lane);
            Initial.Units.Add(Unit);
        }
        Initial.Teams.Add(Team);
    }
    // This fixture has no authored trigger bindings; it must never drain/drop
    // an installed kit's triggered actions. Production effect integration is separate.
    FOGTurnBattle Candidate;
    if (!Candidate.Initialize(Initial, {}, FOGCombatConditionEvaluator(),
        FOGIdentityExclusivityContext(), MoveTemp(RankResolver), Error)) return false;
    Battle = MoveTemp(Candidate);
    EntryPlayerSnapshots = Players;
    Rng = FOGDeterministicRng(Seed);
    Energy.Reset(); SkillCooldownTurns.Reset(); Exposure.Reset();
    for (const FOGCombatUnitState& Unit : Initial.Units)
    {
        const int32* InitialEnergy = StartingEnergy.Find(Unit.UnitEntityId);
        Energy.Add(Unit.UnitEntityId, InitialEnergy ? FMath::Clamp(*InitialEnergy, 0, 100) : 100);
        SkillCooldownTurns.Add(Unit.UnitEntityId, FIntPoint::ZeroValue);
    }
    return true;
}

bool FOGDiagnosticEncounter::IsWaitingForPlayer() const
{
    if (Battle.GetState().Status != EOGTurnBattleStatus::Running) return false;
    const int32 Index = Battle.SelectNextActingUnitIndex();
    return Battle.GetState().Units.IsValidIndex(Index) && Battle.GetState().Units[Index].TeamIndex == 0;
}

bool FOGDiagnosticEncounter::SubmitPlayerAction(EOGDiagnosticCommand Command,
    const FOGEntityId& TargetId, FString& Error)
{
    if (!IsWaitingForPlayer())
    {
        Error = TEXT("The canonical next actor is not waiting for player input.");
        return false;
    }
    return ResolveNextAction(Command, TargetId, Error);
}

bool FOGDiagnosticEncounter::StepEnemy(FString& Error)
{
    if (Battle.GetState().Status != EOGTurnBattleStatus::Running || IsWaitingForPlayer())
    {
        Error = TEXT("No enemy turn is available.");
        return false;
    }
    const FOGCombatUnitState* Target = Battle.GetState().Units.FindByPredicate(
        [](const FOGCombatUnitState& Unit) { return Unit.TeamIndex == 0 && Unit.CanAct(); });
    if (!Target) { Error = TEXT("No active player target remains."); return false; }
    return ResolveNextAction(EOGDiagnosticCommand::Basic, Target->UnitEntityId, Error);
}

bool FOGDiagnosticEncounter::ResolveNextAction(EOGDiagnosticCommand Command,
    const FOGEntityId& TargetId, FString& Error)
{
    const FOGTurnBattleState& State = Battle.GetState();
    const int32 SourceIndex = Battle.SelectNextActingUnitIndex();
    if (State.Status != EOGTurnBattleStatus::Running || !State.Units.IsValidIndex(SourceIndex))
    { Error = TEXT("No canonical turn is available."); return false; }
    const FOGCombatUnitState Source = State.Units[SourceIndex];
    const FOGCombatUnitState* Target = State.Units.FindByPredicate(
        [&TargetId](const FOGCombatUnitState& Unit) { return Unit.UnitEntityId == TargetId; });
    if (!Target || !Target->CanAct() || Target->TeamIndex == Source.TeamIndex)
    { Error = TEXT("Target must be a living active opponent."); return false; }
    FOGDiagnosticActionDefinition Definition;
    if (!OGDiagnosticContent::ResolveAction(Source, Command, Definition, Error)) return false;
    const FIntPoint Cooldown = SkillCooldownTurns.FindRef(Source.UnitEntityId);
    if ((Command == EOGDiagnosticCommand::Skill1 && Cooldown.X > 0) ||
        (Command == EOGDiagnosticCommand::Skill2 && Cooldown.Y > 0))
    { Error = TEXT("This diagnostic skill is still cooling down in turns."); return false; }
    if (Command == EOGDiagnosticCommand::Ultimate && GetEnergy(Source.UnitEntityId) < 100)
    { Error = TEXT("Ultimate requires 100 diagnostic energy."); return false; }
    int32 RankMultiplierBps = 10000;
    if (!Battle.ResolveRankSuppressionMultiplier(Source.UnitEntityId, TargetId,
        OGRankSuppressionChannels::Damage(), RankMultiplierBps, Error)) return false;
    // Resolve on a candidate session so a failed action does not leave HP changed.
    FOGTurnBattle Candidate = Battle;
    FOGDeterministicRng CandidateRng = Rng;
    const FOGDiagnosticExposeState* ExistingEffect = Exposure.Find(TargetId);
    const FOGDamageResolution Damage = OGDiagnosticContent::ResolveDamage(
        Definition, Source, *Target, RankMultiplierBps, CandidateRng,
        ExistingEffect ? ExistingEffect->DamageMultiplier(true, 0.0) : 10000);
    if (FOGLargeNumber::Compare(Damage.TotalDamage, FOGLargeNumber()) > 0 &&
        !Candidate.ApplyDamage(Source.UnitEntityId, TargetId, Damage.TotalDamage,
            Definition.SkillId, Error)) return false;
    FOGResolvedCombatAction Action;
    Action.ActionId = FOGEntityId::NewId();
    Action.SourceUnitId = Source.UnitEntityId;
    Action.TargetUnitIds.Add(TargetId);
    Action.SkillId = Definition.SkillId;
    Action.ActionDelay = Definition.ActionDelay;
    if (!Candidate.ApplyResolvedAction(Action, Error)) return false;
    Battle = MoveTemp(Candidate);
    Rng = CandidateRng;
    FIntPoint& RemainingCooldown = SkillCooldownTurns.FindOrAdd(Source.UnitEntityId);
    RemainingCooldown.X = FMath::Max(0, RemainingCooldown.X - 1);
    RemainingCooldown.Y = FMath::Max(0, RemainingCooldown.Y - 1);
    if (Command == EOGDiagnosticCommand::Skill1) RemainingCooldown.X = 2;
    if (Command == EOGDiagnosticCommand::Skill2) RemainingCooldown.Y = 3;
    int32& CurrentEnergy = Energy.FindOrAdd(Source.UnitEntityId);
    CurrentEnergy = Command == EOGDiagnosticCommand::Ultimate ? 0 :
        FMath::Min(100, CurrentEnergy + (Command == EOGDiagnosticCommand::Basic ? 25 : 15));
    if (FOGDiagnosticExposeState* OwnEffect = Exposure.Find(Source.UnitEntityId)) OwnEffect->CompleteTargetTurn();
    if (Command == EOGDiagnosticCommand::Skill2 && Damage.Hit.ResolvedHitInstances > 0)
    {
        const FOGCombatUnitState* SurvivingTarget = Battle.GetState().Units.FindByPredicate(
            [&TargetId](const FOGCombatUnitState& Unit) { return Unit.UnitEntityId == TargetId; });
        if (SurvivingTarget && SurvivingTarget->IsAlive()) Exposure.FindOrAdd(TargetId).Apply(TargetId, 0.0);
    }
    return true;
}

EOGDiagnosticOutcome FOGDiagnosticEncounter::GetOutcome() const
{
    if (Battle.GetState().Status != EOGTurnBattleStatus::Completed) return EOGDiagnosticOutcome::Running;
    for (const FOGCombatUnitState& Unit : Battle.GetState().Units)
        if (Unit.TeamIndex == 0 && Unit.IsAlive() && Unit.Presence != EOGCombatPresence::Removed)
            return EOGDiagnosticOutcome::Victory;
    return EOGDiagnosticOutcome::Defeat;
}

bool FOGDiagnosticEncounter::CollectReturnSnapshots(TArray<FOGCombatUnitState>& Out, FString& Error) const
{
    Out.Reset();
    if (Battle.GetState().Status != EOGTurnBattleStatus::Completed)
    { Error = TEXT("Resolve victory or defeat before returning to World Mode."); return false; }
    Out = EntryPlayerSnapshots;
    for (FOGCombatUnitState& Snapshot : Out)
    {
        const FOGCombatUnitState* Result = Battle.GetState().Units.FindByPredicate(
            [&Snapshot](const FOGCombatUnitState& Unit) { return Unit.UnitEntityId == Snapshot.UnitEntityId; });
        if (!Result) { Error = TEXT("Player snapshot missing from resolved encounter."); Out.Reset(); return false; }
        Snapshot.CurrentHp = Result->CurrentHp;
        if (!Result->IsAlive()) Snapshot.Presence = EOGCombatPresence::Defeated;
    }
    Error.Reset();
    return true;
}
