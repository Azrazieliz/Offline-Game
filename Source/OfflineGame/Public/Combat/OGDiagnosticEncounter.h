#pragma once

#include "Combat/OGActionCombatAdapter.h"
#include "Combat/OGCombatMath.h"
#include "Combat/OGTurnBattle.h"
#include "OGDiagnosticEffectHandler.h"

// Temporary non-shipping authored content using the canonical combat runtime.
enum class EOGDiagnosticCommand : uint8 { Basic, Skill1, Skill2, Ultimate };
enum class EOGDiagnosticOutcome : uint8 { Running, Victory, Defeat };

struct FOGDiagnosticActionDefinition
{
    FOGContentId SkillId;
    int32 AttackMultiplierBps = 10000;
    int64 ActionDelay = 100;
};

namespace OGDiagnosticContent
{
    // IDs refer to diagnostic content only; these are not final character kits.
    FOGCombatUnitState MakeUnit(const FOGEntityId& Id, int32 Kit, int32 Team = 0);
    bool ResolveAction(const FOGCombatUnitState& Unit, EOGDiagnosticCommand Command,
        FOGDiagnosticActionDefinition& Out, FString& Error);
    FOGDamageResolution ResolveDamage(const FOGDiagnosticActionDefinition& Definition,
        const FOGCombatUnitState& Source, const FOGCombatUnitState& Target,
        int32 RankMultiplierBps, FOGDeterministicRng& Rng, int32 ContentMultiplierBps = 10000);
}

// Presentation-owned encounter session; all turn ordering/HP/defeat/succession
// remains in FOGTurnBattle. No second timeline or turn HP array exists here.
class FOGDiagnosticEncounter
{
public:
    bool Start(const TArray<FOGCombatUnitState>& PlayerSnapshots,
        const TArray<FOGCombatUnitState>& EnemySnapshots, uint64 Seed,
        FOGRankSuppressionResolver RankResolver, FString& Error,
        const TMap<FOGEntityId, int32>& StartingEnergy = {});
    bool SubmitPlayerAction(EOGDiagnosticCommand Command,
        const FOGEntityId& TargetId, FString& Error);
    bool StepEnemy(FString& Error); // one action; schedule through existing presentation tick
    const FOGTurnBattleState& GetState() const { return Battle.GetState(); }
    const FOGCombatLog& GetLog() const { return Battle.GetLog(); }
    bool IsWaitingForPlayer() const;
    EOGDiagnosticOutcome GetOutcome() const;
    bool CollectReturnSnapshots(TArray<FOGCombatUnitState>& Out, FString& Error) const;
    int32 GetEnergy(const FOGEntityId& Unit) const { const int32* Value = Energy.Find(Unit); return Value ? *Value : 0; }
    const TMap<FOGEntityId, int32>& GetResources() const { return Energy; }
    const FOGDiagnosticExposeState* GetExposure(const FOGEntityId& Target) const { return Exposure.Find(Target); }
private:
    bool ResolveNextAction(EOGDiagnosticCommand Command,
        const FOGEntityId& TargetId, FString& Error);
    FOGTurnBattle Battle;
    FOGDeterministicRng Rng;
    TArray<FOGCombatUnitState> EntryPlayerSnapshots;
    TMap<FOGEntityId, int32> Energy;
    TMap<FOGEntityId, FIntPoint> SkillCooldownTurns;
    TMap<FOGEntityId, FOGDiagnosticExposeState> Exposure;
};
