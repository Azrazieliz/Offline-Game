#pragma once

#include "CoreMinimal.h"
#include "Runtime/OGFoundationCharacterRuntime.h"

enum class EOGFoundationCharacterDiagnosticAction : uint8
{
    InspectRank, RecordPracticeRank, PracticeFactor, RecognizePracticeClass, LearnPracticeSkill,
    CompletePracticeNode, UnlockPracticeForm, PracticeReinforcement,
    AcquireTool, EquipTool, UnequipTool, DamageTool, RestoreTool,
    LearnKnownRecipe, CraftKnownTool, PracticeAffinity, PracticeProficiency,
    WearOuterLayer, RecordInjury, RestoreInjury,
    RecordNpcObservation, RecordNpcMemory, PromoteNpc, SpeakToNpc
};

struct OFFLINEGAME_API FOGFoundationCharacterDiagnosticMenuEntry
{
    EOGFoundationCharacterDiagnosticAction Action = EOGFoundationCharacterDiagnosticAction::InspectRank;
    FString Label;
    bool bEnabled = true;
    FString DisabledReason;
};

/** Small immutable authored content, independent of character Identity/Version. */
struct OFFLINEGAME_API FOGFoundationCharacterDiagnosticDefinitions
{
    FOGContentId FactorId;
    FOGContentId ClassId;
    FOGContentId SkillId;
    FOGContentId RouteId;
    FOGContentId NodeId;
    FOGContentId FormId;
    FOGContentId ItemId;
    FOGContentId SlotId;
    FOGContentId ProficiencyId;
    FOGContentId ItemFunctionId;
    FOGContentId OuterLayerId;
    FOGContentId NpcTierId;
    FOGCharacterIdentityDefinition NpcIdentity;
    FOGFoundationKnownRecipe Recipe;
};

struct OFFLINEGAME_API FOGFoundationCharacterDiagnosticResult
{
    FString Message;
    FOGFoundationCharacterContext ResolvedContext;
    FOGEntityId ItemId;
    FOGEntityId NpcEntityId;
    FOGFoundationDialogueProjection Dialogue;
};

/** Normal HUD actions using the same reusable canonical adapter as real content. */
class OFFLINEGAME_API FOGFoundationCharacterDiagnostics
{
public:
    explicit FOGFoundationCharacterDiagnostics(IOGWorldStore& InStore) : Store(InStore) {}
    bool EnsureDiagnosticDefinitions(const FOGFoundationCharacterContext&,
        FOGFoundationCharacterDiagnosticDefinitions&, FOGFoundationCharacterContext&, FString&) const;
    bool BuildMenu(const FOGFoundationCharacterContext&,
        TArray<FOGFoundationCharacterDiagnosticMenuEntry>&, FString&) const;
    bool ExecuteDiagnosticAction(const FOGFoundationCharacterContext&,
        EOGFoundationCharacterDiagnosticAction, int64,
        FOGFoundationCharacterDiagnosticResult&, FString&);
    static bool ResolveEquipmentFunctions(const FOGFoundationCharacterProjection&,
        const FOGFoundationEquippedItemProjection&, FOGFoundationEquipmentFunctions&, FString&);
private:
    bool ExecuteInternal(const FOGFoundationCharacterContext&,
        EOGFoundationCharacterDiagnosticAction, int64,
        FOGFoundationCharacterDiagnosticResult&, FString&);
    bool EnsureResident(const FOGFoundationCharacterProjection&,
        const FOGFoundationCharacterDiagnosticDefinitions&, int64,
        FOGFoundationCharacterContext&, FString&);
    IOGWorldStore& Store;
};
