#pragma once

#include "Combat/OGCombatTriggerRuntime.h"
#include "Combat/OGWorldActionRuntime.h"

// Bounded diagnostic CONTENT handler over the existing trigger queue + party
// QTE state. No alternate party or general status service is introduced.
class FOGDiagnosticQteConditions
{
public:
    bool Configure(UOGWorldPartyRuntimeComponent& InParty, FString& Error);
    void ConfirmSkillHit(const FOGEntityId& Source, const FOGEntityId& Target,
        const FOGContentId& Skill, bool bTargetAlive, double Now);
    void ConfirmPerfectDodge(const FOGEntityId& Source, double Now);
    void Refresh(double Now, bool bMarkedTargetStillAlive);
    void Reset();
private:
    void ResolveReadyActions(double Now);
    TWeakObjectPtr<UOGWorldPartyRuntimeComponent> Party;
    FOGCombatTriggerRuntime Triggers;
    TArray<FOGEntityId> ConfiguredUnitIds;
    double MarkReadyUntil = 0.0;
    double DodgeReadyUntil = 0.0;
};
