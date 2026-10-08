#pragma once

#include "Effects/OGEffectDefinition.h"
#include "UI/OGUiViewModels.h"

// Registered diagnostic CONTENT behavior for one effect only, not a generic
// status service. Both action/turn callers use this handler and shared math.
struct FOGDiagnosticExposeState
{
    FOGEntityId TargetId;
    double ExpiresAtSeconds = 0.0;
    int32 RemainingTargetTurns = 0;
    static FOGEffectDefinition Definition(bool bTurnMode)
    {
        FOGEffectDefinition Value;
        Value.EffectId = FOGContentId(TEXT("diagnostic:effect.exposed"));
        Value.FamilyId = FOGContentId(TEXT("diagnostic:effect_family.combo"));
        Value.MechanicKind = TEXT("diagnostic_exposed");
        Value.MaxStacks = 1;
        Value.Lifetime.Kind = bTurnMode ? EOGEffectLifetimeKind::Turns : EOGEffectLifetimeKind::RealTimeMilliseconds;
        Value.Lifetime.Magnitude = bTurnMode ? 2 : 3000;
        return Value;
    }
    void Apply(const FOGEntityId& Target, double Now)
    { TargetId = Target; ExpiresAtSeconds = Now + 3.0; RemainingTargetTurns = 2; }
    bool IsActive(bool bTurnMode, double Now) const
    { return TargetId.IsValid() && (bTurnMode ? RemainingTargetTurns > 0 : Now < ExpiresAtSeconds); }
    int32 DamageMultiplier(bool bTurnMode, double Now) const { return IsActive(bTurnMode, Now) ? 12500 : 10000; }
    void CompleteTargetTurn() { RemainingTargetTurns = FMath::Max(0, RemainingTargetTurns - 1); }
    void Clear() { TargetId = FOGEntityId(); ExpiresAtSeconds = 0.0; RemainingTargetTurns = 0; }
    FOGHudStatusEffectInput ToHud(bool bTurnMode) const
    {
        FOGHudStatusEffectInput Out;
        Out.EffectId = Definition(bTurnMode).EffectId;
        Out.RemainingTurns = bTurnMode ? RemainingTargetTurns : 0;
        Out.bPreciseKnowledge = true;
        Out.PreciseEffectTextKey = TEXT("Training exposure: incoming damage +25%; one stack.");
        return Out;
    }
};
