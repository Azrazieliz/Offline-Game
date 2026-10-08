#pragma once

#include "CoreMinimal.h"
#include "Persistence/OGWorldStore.h"
#include "World/OGInventoryEquipmentService.h"
#include "Math/OGLargeNumber.h"
#include "Skills/OGResolvedSkillSet.h"

/** Selection only. Canonical records remain owned by the shared world store. */
struct OFFLINEGAME_API FOGFoundationCharacterContext
{
    FOGEntityId EntityId;
    FOGEntityId ManifestationId;
    FOGEntityId OwnerEntityId;
};

struct OFFLINEGAME_API FOGFoundationEquippedItemProjection
{
    FOGEquipmentBindingRecord Binding;
    FOGItemInstanceRecord Item;
    TArray<FOGItemModifierRecord> Modifiers;
    bool bHasAffinity = false;
    FOGItemOwnerAffinityRecord Affinity;
    bool bBroken = false;
};

struct OFFLINEGAME_API FOGFoundationCharacterProjection
{
    FOGFoundationCharacterContext Context;
    FName EntityKind;
    FString EntityStateJson;
    int64 EntityRevision = 0;
    bool bHasManifestation = false;
    FOGCharacterManifestationRecord Manifestation;
    bool bHasPresence = false;
    FOGWorldPresenceRecord Presence;
    bool bHasRank = false;
    FOGEntityRankStateRecord Rank;
    FOGResolvedRankProjection EffectiveRank;
    TArray<FOGFactorInstanceRecord> Factors;
    TArray<FOGEntityClassRecord> Classes;
    TArray<FOGEntitySkillRecord> Skills;
    TArray<FOGSkillProvenanceRecord> SkillProvenance;
    TArray<FOGManifestationRouteNodeRecord> Routes;
    TArray<FOGManifestationFormRecord> Forms;
    bool bHasReinforcement = false;
    FOGManifestationReinforcementRecord Reinforcement;
    TArray<FOGCharacterConvergenceRecord> Convergences;
    TArray<FOGItemInstanceRecord> Inventory;
    TArray<FOGFoundationEquippedItemProjection> Equipment;
    TArray<FOGEquipmentProficiencyRecord> Proficiencies;
    bool bHasPresentation = false;
    FOGManifestationPresentationStateRecord Presentation;
    bool bHasAdultContext = false;
    FOGCharacterAdultRuntimeStateRecord AdultContext;
    bool bHasNpcPromotion = false;
    FOGNpcPromotionStateRecord NpcPromotion;
    TArray<FOGKnowledgeFactRecord> Knowledge;
    TArray<FOGSemanticMemoryRecord> Memories;
};

/** Content resolves mechanics; no universal affinity/stat multiplier is imposed. */
struct OFFLINEGAME_API FOGFoundationEquipmentFunctions
{
    TMap<FName, FOGLargeNumber> StatContributions;
    TArray<FOGContentId> FunctionIds;
};
using FOGFoundationEquipmentResolver = TFunction<bool(
    const FOGFoundationCharacterProjection&,
    const FOGFoundationEquippedItemProjection&,
    FOGFoundationEquipmentFunctions&, FString&)>;
using FOGFoundationKitResolver = TFunction<bool(
    const FOGFoundationCharacterProjection&, FOGResolvedSkillSet&, FString&)>;

struct OFFLINEGAME_API FOGFoundationCraftCost
{
    FOGContentId ResourceId;
    int64 Amount = 0;
};
/** Authored recipe passed by content; knowledge is checked in the canonical store. */
struct OFFLINEGAME_API FOGFoundationKnownRecipe
{
    FOGContentId RecipeId;
    FName RequiredKnowledgeKey;
    FOGEntityId KnowledgeSubjectId;
    int32 MinimumConfidenceBps = 10000;
    TArray<FOGFoundationCraftCost> Costs;
    FOGContentId OutputDefinitionId;
    FOGContentId OutputRankId;
    FOGContentId OutputQualityId;
};

struct OFFLINEGAME_API FOGFoundationDialogueLine
{
    FOGContentId LineId;
    FString Text;
    FName RequiredMovementContext;
    FName RequiredBeliefState;
    FName RequiredKnowledgeKey;
    FOGEntityId KnowledgeSubjectId;
    int32 MinimumConfidenceBps = 0;
    FName RequiredInjuryState;
    FOGContentId RequiredSimulationTier;
    FOGEntityId RequiredLocationId;
    /** Authored mood/relationship/faction values in existing canonical entity state. */
    TMap<FName, FString> RequiredStateValues;
    FOGContentId RequiredMemoryTypeId;
    FOGEntityId RequiredMemorySubjectId;
    int32 MinimumMemorySalienceBps = 0;
};
struct OFFLINEGAME_API FOGFoundationDialogueProjection
{
    FOGContentId LineId;
    FString Text;
    TArray<FOGKnowledgeFactRecord> RetrievedKnowledge;
    TArray<FOGSemanticMemoryRecord> RetrievedMemories;
    bool bUsedLocalFallback = true;
};

/** Stateless commands/projections for HUD, combat, NPC and mature presentation. */
class OFFLINEGAME_API FOGFoundationCharacterRuntime
{
public:
    explicit FOGFoundationCharacterRuntime(IOGWorldStore& InStore) : Store(InStore) {}

    bool Project(const FOGFoundationCharacterContext&, FOGFoundationCharacterProjection&, FString&) const;
    bool ProjectEquipmentFunctions(const FOGFoundationCharacterContext&,
        const FOGFoundationEquipmentResolver&, FOGFoundationEquipmentFunctions&, FString&) const;
    bool ProjectIntegratedKit(const FOGFoundationCharacterContext&,
        const FOGFoundationKitResolver&, FOGResolvedSkillSet&, FString&) const;
    bool SetRank(const FOGFoundationCharacterContext&, FOGEntityRankStateRecord, int64, FString&);
    bool SetPresence(const FOGFoundationCharacterContext&, FOGWorldPresenceRecord, int64, FString&);
    bool AcquireFactor(const FOGFoundationCharacterContext&, FOGFactorInstanceRecord,
        const TArray<FOGFactorLineageRecord>&, int64, FString&);
    bool SetFactorExpression(const FOGFoundationCharacterContext&, const FOGEntityId&, int32, int64, FString&);
    bool RecognizeClass(const FOGFoundationCharacterContext&, FOGEntityClassRecord, int64, FString&);
    bool LearnSkill(const FOGFoundationCharacterContext&, FOGEntitySkillRecord,
        TArray<FOGSkillProvenanceRecord>, int64, FString&);
    bool SetRouteNode(const FOGFoundationCharacterContext&, FOGManifestationRouteNodeRecord, int64, FString&);
    bool SetForm(const FOGFoundationCharacterContext&, FOGManifestationFormRecord, int64, FString&);
    bool SetReinforcement(const FOGFoundationCharacterContext&, FOGManifestationReinforcementRecord, int64, FString&);
    bool Converge(const FOGFoundationCharacterContext&, FOGCharacterManifestationRecord,
        const TArray<FOGEntityId>&, const TArray<FOGContentId>&, const FOGContentId&,
        int64, const FString&, bool, FOGEntityId&, FString&);
    bool AcquireItem(const FOGFoundationCharacterContext&, const FOGContentId&,
        const FOGContentId&, const FOGContentId&, int64, FOGEntityId&, FString&);
    bool Equip(const FOGFoundationCharacterContext&, const FOGContentId&, const FOGEntityId&,
        const FOGEquipmentSlotValidator&, FString&);
    bool Unequip(const FOGFoundationCharacterContext&, const FOGContentId&, FString&);
    bool SetAffinity(const FOGFoundationCharacterContext&, FOGItemOwnerAffinityRecord, int64, FString&);
    bool SetProficiency(const FOGFoundationCharacterContext&, FOGEquipmentProficiencyRecord, int64, FString&);
    bool DamageEquipment(const FOGFoundationCharacterContext&, const FOGEntityId&, int32,
        const FOGEntityId&, int64, FString&);
    bool RestoreEquipment(const FOGFoundationCharacterContext&, const FOGEntityId&, int32,
        const FOGEntityId&, int64, FString&);
    bool SetPresentation(const FOGFoundationCharacterContext&,
        FOGManifestationPresentationStateRecord, int64, FString&);
    bool SetInjury(const FOGFoundationCharacterContext&, FName, int32,
        const FOGEntityId&, int64, FString&);
    bool RestoreInjury(const FOGFoundationCharacterContext&, const FOGEntityId&, int64, FString&);
    bool CraftKnown(const FOGFoundationCharacterContext&, const FOGFoundationKnownRecipe&,
        int64, FOGEntityId&, FString&);
    bool PromoteNpc(const FOGFoundationCharacterContext&, const FOGContentId&,
        const FOGEntityId&, const FString&, int64, FString&);
    bool RecordKnowledge(const FOGFoundationCharacterContext&, FOGKnowledgeFactRecord, int64, FString&);
    bool RecordMemory(const FOGFoundationCharacterContext&, FOGSemanticMemoryRecord, int64, FString&);
    bool SelectDialogue(const FOGFoundationCharacterContext&, const TArray<FOGFoundationDialogueLine>&,
        uint32, FOGFoundationDialogueProjection&, FString&) const;
    /** Generated output may select an already filtered authored line, never facts or free text. */
    bool SelectGeneratedDialogueLine(const FOGFoundationCharacterContext&,
        const TArray<FOGFoundationDialogueLine>&, const FOGContentId&,
        uint32, FOGFoundationDialogueProjection&, FString&) const;

private:
    bool Resolve(const FOGFoundationCharacterContext&, FOGFoundationCharacterContext&, int64, FString&) const;
    bool RequireManifestation(const FOGFoundationCharacterContext&, int64, FString&) const;
    bool ChangeDurability(const FOGFoundationCharacterContext&, const FOGEntityId&, int32,
        bool, const FOGEntityId&, int64, FString&);
    bool FilterDialogue(const FOGFoundationCharacterContext&, const TArray<FOGFoundationDialogueLine>&,
        TArray<FOGFoundationDialogueLine>&, FOGFoundationDialogueProjection&, FString&) const;
    IOGWorldStore& Store;
};
