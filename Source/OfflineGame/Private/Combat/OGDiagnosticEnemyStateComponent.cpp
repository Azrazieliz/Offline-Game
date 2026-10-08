#include "Combat/OGDiagnosticEnemyStateComponent.h"

bool UOGDiagnosticEnemyStateComponent::ApplyResolvedDamage(const FOGLargeNumber& Damage, FString& Error)
{
    Error.Reset();
    if (!Snapshot.UnitEntityId.IsValid() || Damage.GetSign() < 0 || !Snapshot.IsAlive())
    { Error = TEXT("Damage requires a living initialized diagnostic enemy and nonnegative amount."); return false; }
    Snapshot.CurrentHp = FOGLargeNumber::Add(Snapshot.CurrentHp,
        FOGLargeNumber::ScaleByBasisPoints(Damage, -10000));
    if (Snapshot.CurrentHp.GetSign() <= 0)
    {
        Snapshot.CurrentHp = FOGLargeNumber();
        Snapshot.Presence = EOGCombatPresence::Defeated;
        OnDefeated.Broadcast();
    }
    return true;
}
