#include "Combat/OGDiagnosticQteConditions.h"

bool FOGDiagnosticQteConditions::Configure(UOGWorldPartyRuntimeComponent& InParty, FString& Error)
{
    Reset();
    Error.Reset();
    if (InParty.GetSlots().Num() != 3)
    { Error = TEXT("Diagnostic QTE content expects protagonist and two companions."); return false; }
    Party = &InParty;
    for (const FOGWorldPartySlot& Slot : InParty.GetSlots()) ConfiguredUnitIds.Add(Slot.Unit.UnitEntityId);
    FOGCombatTriggerBinding Mark;
    Mark.OwnerUnitId = ConfiguredUnitIds[1];
    Mark.Trigger.TriggerId = FOGContentId(TEXT("diagnostic:trigger.mark_qte"));
    Mark.Trigger.EventType = TEXT("OnHit");
    Mark.Trigger.ActionIds.Add(FOGContentId(TEXT("diagnostic:action.mark_qte_ready")));
    FOGConditionDefinition SkillCondition;
    SkillCondition.Predicate = TEXT("skill_equals");
    SkillCondition.ContentOperand = InParty.GetSlots()[0].Unit.SkillSet.ActiveSkills.IsValidIndex(1)
        ? InParty.GetSlots()[0].Unit.SkillSet.ActiveSkills[1] : FOGContentId();
    if (!SkillCondition.ContentOperand.IsValid())
    { Error = TEXT("Diagnostic protagonist burst skill is missing."); Reset(); return false; }
    Mark.Trigger.Conditions.Add(SkillCondition);
    FOGCombatTriggerBinding Dodge;
    Dodge.OwnerUnitId = ConfiguredUnitIds[2];
    Dodge.Trigger.TriggerId = FOGContentId(TEXT("diagnostic:trigger.dodge_qte"));
    Dodge.Trigger.EventType = TEXT("OnPerfectDodge");
    Dodge.Trigger.ActionIds.Add(FOGContentId(TEXT("diagnostic:action.dodge_qte_ready")));
    Triggers.SetBindings({Mark, Dodge});
    const FOGEntityId Protagonist = ConfiguredUnitIds[0];
    Triggers.SetConditionEvaluator([Protagonist](const FOGCombatTriggerBinding&,
        const FOGConditionDefinition& Condition, const FOGCombatTriggerContext& Context)
    {
        return Context.SourceUnitId == Protagonist &&
            Condition.Predicate == FName(TEXT("skill_equals")) &&
            Context.SkillId == Condition.ContentOperand;
    });
    return true;
}

void FOGDiagnosticQteConditions::ConfirmSkillHit(const FOGEntityId& Source,
    const FOGEntityId& Target, const FOGContentId& Skill, bool bTargetAlive, double Now)
{
    if (!Party.IsValid() || !bTargetAlive || !Target.IsValid()) return;
    FOGCombatTriggerContext Event;
    Event.EventType = TEXT("OnHit");
    Event.SourceUnitId = Source;
    Event.TargetUnitId = Target;
    Event.SkillId = Skill;
    Triggers.QueueForEvent(Event);
    ResolveReadyActions(Now);
}

void FOGDiagnosticQteConditions::ConfirmPerfectDodge(const FOGEntityId& Source, double Now)
{
    // Called only when the player's existing dodge receiver negated an actual
    // telegraphed diagnostic strike. Ordinary dodge button input does not call it.
    if (!Party.IsValid() || !ConfiguredUnitIds.Contains(Source)) return;
    FOGCombatTriggerContext Event;
    Event.EventType = TEXT("OnPerfectDodge");
    Event.SourceUnitId = Source;
    Triggers.QueueForEvent(Event);
    ResolveReadyActions(Now);
}

void FOGDiagnosticQteConditions::ResolveReadyActions(double Now)
{
    if (!Party.IsValid()) return;
    for (const FOGQueuedTriggeredAction& Action : Triggers.DrainQueuedActions())
    {
        for (int32 Slot = 0; Slot < Party->GetSlots().Num(); ++Slot)
        {
            const FOGWorldPartySlot& Candidate = Party->GetSlots()[Slot];
            if (Candidate.Unit.UnitEntityId != Action.OwnerUnitId ||
                !Candidate.bAvailable || Candidate.bDefeated || Slot == Party->GetControlledSlot()) continue;
            if (Action.ActionId == FOGContentId(TEXT("diagnostic:action.mark_qte_ready")))
            { MarkReadyUntil = Now + 4.0; Party->SetQteReady(Slot, true); }
            else if (Action.ActionId == FOGContentId(TEXT("diagnostic:action.dodge_qte_ready")))
            { DodgeReadyUntil = Now + 4.0; Party->SetQteReady(Slot, true); }
        }
    }
}

void FOGDiagnosticQteConditions::Refresh(double Now, bool bMarkedTargetStillAlive)
{
    if (!Party.IsValid()) return;
    if (Party->GetSlots().Num() != ConfiguredUnitIds.Num()) { Reset(); return; }
    for (int32 Slot = 0; Slot < ConfiguredUnitIds.Num(); ++Slot)
        if (Party->GetSlots()[Slot].Unit.UnitEntityId != ConfiguredUnitIds[Slot])
        { Reset(); return; } // Party assignment changed: content must reconfigure.
    if (!bMarkedTargetStillAlive || Now >= MarkReadyUntil) Party->SetQteReady(1, false);
    if (Now >= DodgeReadyUntil) Party->SetQteReady(2, false);
}

void FOGDiagnosticQteConditions::Reset()
{
    // This handler may only clear its original identities, never newly assigned units.
    if (Party.IsValid())
        for (int32 Slot = 1; Slot < ConfiguredUnitIds.Num(); ++Slot)
            if (Party->GetSlots().IsValidIndex(Slot) &&
                Party->GetSlots()[Slot].Unit.UnitEntityId == ConfiguredUnitIds[Slot]) Party->SetQteReady(Slot, false);
    Party = nullptr;
    ConfiguredUnitIds.Reset();
    MarkReadyUntil = DodgeReadyUntil = 0.0;
    Triggers.Reset();
}
