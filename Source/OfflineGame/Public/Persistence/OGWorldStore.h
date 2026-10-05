#pragma once

#include "CoreMinimal.h"
#include "Characters/OGCharacterDefinitions.h"
#include "Content/OGContentId.h"
#include "Core/OGEntityId.h"
#include "Events/OGWorldEvent.h"
#include "Gacha/OGGachaDefinitions.h"
#include "Progression/OGProgressionRecords.h"
#include "Runtime/OGPackageReportManagementRecords.h"
#include "World/OGDispatchFactionWarRecords.h"
#include "World/OGItemKnowledgeCharacterRecords.h"
#include "World/OGRealityTimeRecords.h"
#include "World/OGStrategyExpansionRecords.h"
#include "World/OGTerritoryStateRecords.h"
#include "World/OGWorldStateRecords.h"

class OFFLINEGAME_API IOGWorldStore
{
public:
    virtual ~IOGWorldStore() = default;

    virtual bool Open(const FString& AbsoluteDatabasePath, FString& OutError) = 0;
    virtual void Close() = 0;
    virtual bool IsOpen() const = 0;

    virtual bool BeginTransaction(FString& OutError) = 0;
    virtual bool CommitTransaction(FString& OutError) = 0;
    virtual bool RollbackTransaction(FString& OutError) = 0;
    virtual int32 GetSchemaVersion(FString& OutError) const = 0;

    virtual bool UpsertEntity(const FOGEntityId&, FName, int64, const FString&, FString&) = 0;
    virtual bool TryReadEntity(const FOGEntityId&, bool&, FName&, FString&, int64&, FString&) const = 0;

    virtual bool UpsertCharacterManifestation(const FOGCharacterManifestationRecord&, int64, FString&) = 0;
    virtual bool TryReadCharacterManifestation(const FOGEntityId&, bool&, FOGCharacterManifestationRecord&, FString&) const = 0;
    virtual bool ListCharacterManifestationsByOwnerAndIdentity(
        const FOGEntityId&, const FOGContentId&, TArray<FOGCharacterManifestationRecord>&, FString&) const = 0;
    virtual bool ListCharacterManifestationsByOwner(
        const FOGEntityId&, TArray<FOGCharacterManifestationRecord>&, FString&) const = 0;
    virtual bool SetManifestationAnchor(
        const FOGEntityId&, const FOGEntityId&, int64, FString&) = 0;
    virtual bool SetManifestationLifecycle(
        const FOGEntityId&, FName, FString&) = 0;

    virtual bool UpsertGachaState(const FOGGachaStateRecord&, FString&) = 0;
    virtual bool TryReadGachaState(const FOGEntityId&, FName, bool&, FOGGachaStateRecord&, FString&) const = 0;

    virtual bool UpsertLocation(const FOGLocationRecord&, int64, FString&) = 0;
    virtual bool TryReadLocation(const FOGEntityId&, bool&, FOGLocationRecord&, FString&) const = 0;
    virtual bool UpsertWorldPresence(const FOGWorldPresenceRecord&, FString&) = 0;
    virtual bool TryReadWorldPresence(const FOGEntityId&, bool&, FOGWorldPresenceRecord&, FString&) const = 0;
    virtual bool UpsertKnowledgeFact(const FOGKnowledgeFactRecord&, FString&) = 0;
    virtual bool TryReadKnowledgeFact(const FOGEntityId&, FName, const FOGEntityId&, bool&, FOGKnowledgeFactRecord&, FString&) const = 0;
    virtual bool ListKnowledgeFactsByOwner(
        const FOGEntityId&, TArray<FOGKnowledgeFactRecord>&, FString&) const = 0;

    virtual bool UpsertTerritory(const FOGTerritoryRecord&, int64, FString&) = 0;
    virtual bool TryReadTerritory(const FOGEntityId&, bool&, FOGTerritoryRecord&, FString&) const = 0;
    virtual bool UpsertLocationTerritory(const FOGLocationTerritoryRecord&, FString&) = 0;
    virtual bool UpsertTerritoryClaim(const FOGTerritoryClaimRecord&, int64, FString&) = 0;
    virtual bool TryReadTerritoryClaim(const FOGEntityId&, bool&, FOGTerritoryClaimRecord&, FString&) const = 0;
    virtual bool ListTerritoryClaimsByTerritory(
        const FOGEntityId&, TArray<FOGTerritoryClaimRecord>&, FString&) const = 0;
    virtual bool ListTerritoryClaimsByRuler(
        const FOGEntityId&, TArray<FOGTerritoryClaimRecord>&, FString&) const = 0;
    virtual bool ListActiveClaimsForLocation(
        const FOGEntityId&, TArray<FOGTerritoryClaimRecord>&, FString&) const = 0;
    virtual bool UpsertRulerSovereigntyState(const FOGRulerSovereigntyStateRecord&, FString&) = 0;
    virtual bool TryReadRulerSovereigntyState(
        const FOGEntityId&, bool&, FOGRulerSovereigntyStateRecord&, FString&) const = 0;
    virtual bool UpsertRulerGachaAccess(const FOGRulerGachaAccessRecord&, FString&) = 0;
    virtual bool TryReadRulerGachaAccess(
        const FOGEntityId&, bool&, FOGRulerGachaAccessRecord&, FString&) const = 0;

    virtual bool UpsertDomainCore(const FOGDomainCoreRecord&, int64, FString&) = 0;
    virtual bool TryReadDomainCore(const FOGEntityId&, bool&, FOGDomainCoreRecord&, FString&) const = 0;
    virtual bool UpsertTerritoryDomainState(
        const FOGTerritoryDomainStateRecord&, FString&) = 0;
    virtual bool TryReadTerritoryDomainState(
        const FOGEntityId&, bool&, FOGTerritoryDomainStateRecord&, FString&) const = 0;
    virtual bool UpsertDomainCoreConcept(
        const FOGDomainCoreConceptRecord&, FString&) = 0;
    virtual bool ListDomainCoreConcepts(
        const FOGEntityId&, TArray<FOGDomainCoreConceptRecord>&, FString&) const = 0;
    virtual bool UpsertDomainCoreFusion(
        const FOGDomainCoreFusionRecord&, int64, FString&) = 0;
    virtual bool ListDomainCoreFusionsForResult(
        const FOGEntityId&, TArray<FOGDomainCoreFusionRecord>&, FString&) const = 0;
    virtual bool UpsertDomainCoreLineage(
        const FOGDomainCoreLineageRecord&, FString&) = 0;
    virtual bool ListDomainCoreLineage(
        const FOGEntityId&, TArray<FOGDomainCoreLineageRecord>&, FString&) const = 0;
    virtual bool SetResourceBalance(const FOGEntityId&, const FOGContentId&, int64, FString&) = 0;
    virtual bool TryReadResourceBalance(const FOGEntityId&, const FOGContentId&, bool&, int64&, FString&) const = 0;
    virtual bool UpsertProject(const FOGProjectRecord&, int64, FString&) = 0;
    virtual bool TryReadProject(const FOGEntityId&, bool&, FOGProjectRecord&, FString&) const = 0;

    virtual bool UpsertEntityRankState(const FOGEntityRankStateRecord&, FString&) = 0;
    virtual bool TryReadEntityRankState(
        const FOGEntityId&, bool&, FOGEntityRankStateRecord&, FString&) const = 0;

    virtual bool UpsertFactorInstance(const FOGFactorInstanceRecord&, int64, FString&) = 0;
    virtual bool TryReadFactorInstance(
        const FOGEntityId&, bool&, FOGFactorInstanceRecord&, FString&) const = 0;
    virtual bool ListFactorInstancesByOwner(
        const FOGEntityId&, TArray<FOGFactorInstanceRecord>&, FString&) const = 0;
    virtual bool UpsertFactorLineage(const FOGFactorLineageRecord&, FString&) = 0;
    virtual bool ListFactorLineageForChild(
        const FOGEntityId&, TArray<FOGFactorLineageRecord>&, FString&) const = 0;

    virtual bool UpsertEntityClass(const FOGEntityClassRecord&, FString&) = 0;
    virtual bool TryReadEntityClass(
        const FOGEntityId&, const FOGContentId&, bool&, FOGEntityClassRecord&, FString&) const = 0;
    virtual bool ListEntityClasses(
        const FOGEntityId&, TArray<FOGEntityClassRecord>&, FString&) const = 0;
    virtual bool UpsertGrandClassSeat(const FOGGrandClassSeatRecord&, FString&) = 0;
    virtual bool TryReadGrandClassSeat(
        const FOGContentId&, bool&, FOGGrandClassSeatRecord&, FString&) const = 0;
    virtual bool DeleteGrandClassSeat(const FOGContentId&, FString&) = 0;

    virtual bool UpsertEntitySkill(const FOGEntitySkillRecord&, FString&) = 0;
    virtual bool ListEntitySkills(
        const FOGEntityId&, TArray<FOGEntitySkillRecord>&, FString&) const = 0;
    virtual bool UpsertSkillProvenance(const FOGSkillProvenanceRecord&, FString&) = 0;
    virtual bool ListSkillProvenance(
        const FOGEntityId&, const FOGContentId&, TArray<FOGSkillProvenanceRecord>&, FString&) const = 0;

    virtual bool UpsertManifestationRouteNode(
        const FOGManifestationRouteNodeRecord&, FString&) = 0;
    virtual bool ListManifestationRouteNodes(
        const FOGEntityId&, TArray<FOGManifestationRouteNodeRecord>&, FString&) const = 0;
    virtual bool UpsertManifestationForm(
        const FOGManifestationFormRecord&, FString&) = 0;
    virtual bool ListManifestationForms(
        const FOGEntityId&, TArray<FOGManifestationFormRecord>&, FString&) const = 0;
    virtual bool UpsertManifestationReinforcement(
        const FOGManifestationReinforcementRecord&, FString&) = 0;
    virtual bool TryReadManifestationReinforcement(
        const FOGEntityId&, bool&, FOGManifestationReinforcementRecord&, FString&) const = 0;

    virtual bool UpsertEntityTranscendenceState(
        const FOGEntityTranscendenceStateRecord&, FString&) = 0;
    virtual bool TryReadEntityTranscendenceState(
        const FOGEntityId&, bool&, FOGEntityTranscendenceStateRecord&, FString&) const = 0;
    virtual bool UpsertManifestationWorldFantasmState(
        const FOGManifestationWorldFantasmStateRecord&, FString&) = 0;
    virtual bool TryReadManifestationWorldFantasmState(
        const FOGEntityId&, bool&, FOGManifestationWorldFantasmStateRecord&, FString&) const = 0;
    virtual bool UpsertProtagonistWorldManifestationState(
        const FOGProtagonistWorldManifestationStateRecord&, FString&) = 0;
    virtual bool TryReadProtagonistWorldManifestationState(
        const FOGEntityId&, bool&, FOGProtagonistWorldManifestationStateRecord&, FString&) const = 0;

    virtual bool UpsertCharacterConvergence(
        const FOGCharacterConvergenceRecord&, int64, FString&) = 0;
    virtual bool TryReadCharacterConvergence(
        const FOGEntityId&, bool&, FOGCharacterConvergenceRecord&, FString&) const = 0;
    virtual bool ListCharacterConvergencesByIdentity(
        const FOGContentId&, TArray<FOGCharacterConvergenceRecord>&, FString&) const = 0;
    virtual bool UpsertCharacterConvergenceSource(
        const FOGCharacterConvergenceSourceRecord&, FString&) = 0;
    virtual bool ListCharacterConvergenceSources(
        const FOGEntityId&, TArray<FOGCharacterConvergenceSourceRecord>&, FString&) const = 0;

    virtual bool UpsertTimeDomain(const FOGTimeDomainRecord&, int64, FString&) = 0;
    virtual bool TryReadTimeDomain(
        const FOGEntityId&, bool&, FOGTimeDomainRecord&, FString&) const = 0;
    virtual bool UpsertRealityNode(const FOGRealityNodeRecord&, int64, FString&) = 0;
    virtual bool TryReadRealityNode(
        const FOGEntityId&, bool&, FOGRealityNodeRecord&, FString&) const = 0;
    virtual bool ListChildRealityNodes(
        const FOGEntityId&, TArray<FOGRealityNodeRecord>&, FString&) const = 0;
    virtual bool UpsertJunction(const FOGJunctionRecord&, int64, FString&) = 0;
    virtual bool TryReadJunction(
        const FOGEntityId&, bool&, FOGJunctionRecord&, FString&) const = 0;
    virtual bool ListJunctionsFromReality(
        const FOGEntityId&, TArray<FOGJunctionRecord>&, FString&) const = 0;
    virtual bool UpsertWorldDirectorSchedule(
        const FOGWorldDirectorScheduleRecord&, int64, FString&) = 0;
    virtual bool TryReadWorldDirectorSchedule(
        const FOGEntityId&, bool&, FOGWorldDirectorScheduleRecord&, FString&) const = 0;
    virtual bool ListWorldDirectorSchedulesByContent(
        const FOGContentId&, TArray<FOGWorldDirectorScheduleRecord>&, FString&) const = 0;
    virtual bool UpsertContentUnlockState(
        const FOGContentUnlockStateRecord&, FString&) = 0;
    virtual bool TryReadContentUnlockState(
        const FOGContentId&, bool&, FOGContentUnlockStateRecord&, FString&) const = 0;
    virtual bool UpsertOfflineSimulationState(
        const FOGOfflineSimulationStateRecord&, FString&) = 0;
    virtual bool TryReadOfflineSimulationState(
        const FOGEntityId&, bool&, FOGOfflineSimulationStateRecord&, FString&) const = 0;

    virtual bool UpsertDispatch(const FOGDispatchRecord&, int64, FString&) = 0;
    virtual bool TryReadDispatch(const FOGEntityId&, bool&, FOGDispatchRecord&, FString&) const = 0;

    virtual bool UpsertDispatchObjective(
        const FOGDispatchObjectiveRecord&, FString&) = 0;
    virtual bool ListDispatchObjectives(
        const FOGEntityId&, TArray<FOGDispatchObjectiveRecord>&, FString&) const = 0;
    virtual bool UpsertDispatchConstraint(
        const FOGDispatchConstraintRecord&, FString&) = 0;
    virtual bool ListDispatchConstraints(
        const FOGEntityId&, TArray<FOGDispatchConstraintRecord>&, FString&) const = 0;

    virtual bool UpsertFaction(const FOGFactionRecord&, int64, FString&) = 0;
    virtual bool TryReadFaction(const FOGEntityId&, bool&, FOGFactionRecord&, FString&) const = 0;
    virtual bool UpsertFactionLink(const FOGFactionLinkRecord&, FString&) = 0;
    virtual bool TryReadFactionLink(const FOGEntityId&, const FOGEntityId&, EOGFactionLinkType, bool&, FOGFactionLinkRecord&, FString&) const = 0;

    virtual bool UpsertArmy(const FOGArmyRecord&, int64, FString&) = 0;
    virtual bool TryReadArmy(const FOGEntityId&, bool&, FOGArmyRecord&, FString&) const = 0;

    virtual bool UpsertArmyCapability(
        const FOGArmyCapabilityRecord&, FString&) = 0;
    virtual bool ListArmyCapabilities(
        const FOGEntityId&, TArray<FOGArmyCapabilityRecord>&, FString&) const = 0;

    virtual bool UpsertWar(const FOGWarRecord&, int64, FString&) = 0;
    virtual bool TryReadWar(const FOGEntityId&, bool&, FOGWarRecord&, FString&) const = 0;
    virtual bool UpsertWarFront(const FOGWarFrontRecord&, int64, FString&) = 0;
    virtual bool TryReadWarFront(
        const FOGEntityId&, bool&, FOGWarFrontRecord&, FString&) const = 0;
    virtual bool ListWarFronts(
        const FOGEntityId&, TArray<FOGWarFrontRecord>&, FString&) const = 0;
    virtual bool UpsertWarObjective(
        const FOGWarObjectiveRecord&, FString&) = 0;
    virtual bool ListWarObjectives(
        const FOGEntityId&, TArray<FOGWarObjectiveRecord>&, FString&) const = 0;
    virtual bool UpsertWarOrder(const FOGWarOrderRecord&, int64, FString&) = 0;
    virtual bool TryReadWarOrder(
        const FOGEntityId&, bool&, FOGWarOrderRecord&, FString&) const = 0;
    virtual bool ListWarOrders(
        const FOGEntityId&, TArray<FOGWarOrderRecord>&, FString&) const = 0;
    virtual bool UpsertWarParticipantHistory(
        const FOGWarParticipantHistoryRecord&, FString&) = 0;
    virtual bool ListWarParticipantHistory(
        const FOGEntityId&, TArray<FOGWarParticipantHistoryRecord>&, FString&) const = 0;

    virtual bool UpsertProjectPhase(
        const FOGProjectPhaseRecord&, FString&) = 0;
    virtual bool ListProjectPhases(
        const FOGEntityId&, TArray<FOGProjectPhaseRecord>&, FString&) const = 0;
    virtual bool UpsertProjectAssignment(
        const FOGProjectAssignmentRecord&, FString&) = 0;
    virtual bool ListProjectAssignments(
        const FOGEntityId&, TArray<FOGProjectAssignmentRecord>&, FString&) const = 0;

    virtual bool UpsertCivilizationState(
        const FOGCivilizationStateRecord&, FString&) = 0;
    virtual bool TryReadCivilizationState(
        const FOGEntityId&, bool&, FOGCivilizationStateRecord&, FString&) const = 0;
    virtual bool UpsertCivilizationDimension(
        const FOGCivilizationDimensionRecord&, FString&) = 0;
    virtual bool ListCivilizationDimensions(
        const FOGEntityId&, TArray<FOGCivilizationDimensionRecord>&, FString&) const = 0;

    virtual bool UpsertLogisticsRoute(
        const FOGLogisticsRouteRecord&, int64, FString&) = 0;
    virtual bool TryReadLogisticsRoute(
        const FOGEntityId&, bool&, FOGLogisticsRouteRecord&, FString&) const = 0;
    virtual bool ListLogisticsRoutesByOwner(
        const FOGEntityId&, TArray<FOGLogisticsRouteRecord>&, FString&) const = 0;

    virtual bool UpsertItemInstance(const FOGItemInstanceRecord&, int64, FString&) = 0;
    virtual bool TryReadItemInstance(
        const FOGEntityId&, bool&, FOGItemInstanceRecord&, FString&) const = 0;
    virtual bool ListItemInstancesByOwner(
        const FOGEntityId&, TArray<FOGItemInstanceRecord>&, FString&) const = 0;
    virtual bool UpsertItemModifier(const FOGItemModifierRecord&, FString&) = 0;
    virtual bool ListItemModifiers(
        const FOGEntityId&, TArray<FOGItemModifierRecord>&, FString&) const = 0;
    virtual bool UpsertEquipmentBinding(const FOGEquipmentBindingRecord&, FString&) = 0;
    virtual bool TryReadEquipmentBinding(
        const FOGEntityId&, const FOGContentId&, bool&, FOGEquipmentBindingRecord&, FString&) const = 0;
    virtual bool ListEquipmentBindings(
        const FOGEntityId&, TArray<FOGEquipmentBindingRecord>&, FString&) const = 0;
    virtual bool DeleteEquipmentBindingsForItem(
        const FOGEntityId&, FString&) = 0;
    virtual bool UpsertInventoryContainer(
        const FOGInventoryContainerRecord&, int64, FString&) = 0;
    virtual bool TryReadInventoryContainer(
        const FOGEntityId&, bool&, FOGInventoryContainerRecord&, FString&) const = 0;
    virtual bool ListInventoryContainersByOwner(
        const FOGEntityId&, TArray<FOGInventoryContainerRecord>&, FString&) const = 0;
    virtual bool UpsertContainerContent(const FOGContainerContentRecord&, FString&) = 0;
    virtual bool ListContainerContents(
        const FOGEntityId&, TArray<FOGContainerContentRecord>&, FString&) const = 0;
    virtual bool DeleteContainerContentsForItem(
        const FOGEntityId&, FString&) = 0;
    virtual bool UpsertItemOwnerAffinity(
        const FOGItemOwnerAffinityRecord&, FString&) = 0;
    virtual bool TryReadItemOwnerAffinity(
        const FOGEntityId&, const FOGEntityId&, bool&, FOGItemOwnerAffinityRecord&, FString&) const = 0;
    virtual bool UpsertEquipmentProficiency(
        const FOGEquipmentProficiencyRecord&, FString&) = 0;
    virtual bool TryReadEquipmentProficiency(
        const FOGEntityId&, const FOGContentId&, bool&, FOGEquipmentProficiencyRecord&, FString&) const = 0;
    virtual bool ListEquipmentProficienciesByOwner(
        const FOGEntityId&, TArray<FOGEquipmentProficiencyRecord>&, FString&) const = 0;
    virtual bool UpsertManifestationPresentationState(
        const FOGManifestationPresentationStateRecord&, FString&) = 0;
    virtual bool TryReadManifestationPresentationState(
        const FOGEntityId&, bool&, FOGManifestationPresentationStateRecord&, FString&) const = 0;
    virtual bool UpsertOwnedPresentationUnlock(
        const FOGOwnedPresentationUnlockRecord&, FString&) = 0;
    virtual bool ListOwnedPresentationUnlocks(
        const FOGEntityId&, TArray<FOGOwnedPresentationUnlockRecord>&, FString&) const = 0;
    virtual bool UpsertEntityLanguage(const FOGEntityLanguageRecord&, FString&) = 0;
    virtual bool ListEntityLanguages(
        const FOGEntityId&, TArray<FOGEntityLanguageRecord>&, FString&) const = 0;
    virtual bool UpsertSemanticMemory(
        const FOGSemanticMemoryRecord&, int64, FString&) = 0;
    virtual bool TryReadSemanticMemory(
        const FOGEntityId&, bool&, FOGSemanticMemoryRecord&, FString&) const = 0;
    virtual bool ListSemanticMemoriesByOwner(
        const FOGEntityId&, TArray<FOGSemanticMemoryRecord>&, FString&) const = 0;
    virtual bool UpsertNpcPromotionState(
        const FOGNpcPromotionStateRecord&, FString&) = 0;
    virtual bool TryReadNpcPromotionState(
        const FOGEntityId&, bool&, FOGNpcPromotionStateRecord&, FString&) const = 0;
    virtual bool UpsertCharacterAdultRuntimeState(
        const FOGCharacterAdultRuntimeStateRecord&, FString&) = 0;
    virtual bool TryReadCharacterAdultRuntimeState(
        const FOGEntityId&, bool&, FOGCharacterAdultRuntimeStateRecord&, FString&) const = 0;
    virtual bool UpsertHeroicRecord(
        const FOGHeroicRecord&, int64, FString&) = 0;
    virtual bool TryReadHeroicRecord(
        const FOGEntityId&, bool&, FOGHeroicRecord&, FString&) const = 0;
    virtual bool ListHeroicRecordsByIdentity(
        const FOGContentId&, TArray<FOGHeroicRecord>&, FString&) const = 0;

    virtual bool UpsertContentPackageRecord(
        const FOGContentPackageRecord&, FString&) = 0;
    virtual bool TryReadContentPackageRecord(
        const FOGContentId&, bool&, FOGContentPackageRecord&, FString&) const = 0;
    virtual bool ListContentPackageRecords(
        TArray<FOGContentPackageRecord>&, FString&) const = 0;
    virtual bool UpsertPackageDependency(
        const FOGPackageDependencyRecord&, FString&) = 0;
    virtual bool ListPackageDependencies(
        const FOGContentId&, TArray<FOGPackageDependencyRecord>&, FString&) const = 0;

    virtual bool UpsertReport(
        const FOGReportRecord&, int64, FString&) = 0;
    virtual bool TryReadReport(
        const FOGEntityId&, bool&, FOGReportRecord&, FString&) const = 0;
    virtual bool ListReportsByOwner(
        const FOGEntityId&, TArray<FOGReportRecord>&, FString&) const = 0;
    virtual bool UpsertReportDelivery(
        const FOGReportDeliveryRecord&, FString&) = 0;
    virtual bool TryReadReportDelivery(
        const FOGEntityId&, FName, bool&, FOGReportDeliveryRecord&, FString&) const = 0;
    virtual bool ListReportDeliveries(
        const FOGEntityId&, TArray<FOGReportDeliveryRecord>&, FString&) const = 0;

    virtual bool UpsertManifestationManagementMetadata(
        const FOGManifestationManagementMetadataRecord&, FString&) = 0;
    virtual bool TryReadManifestationManagementMetadata(
        const FOGEntityId&, bool&, FOGManifestationManagementMetadataRecord&, FString&) const = 0;
    virtual bool UpsertManifestationContextSelection(
        const FOGManifestationContextSelectionRecord&, FString&) = 0;
    virtual bool TryReadManifestationContextSelection(
        const FOGEntityId&, const FOGContentId&, bool&, FOGManifestationContextSelectionRecord&, FString&) const = 0;

    virtual bool UpsertContentPackage(const FOGContentId&, int32, const FString&, bool, bool, const FString&, FString&) = 0;
    virtual bool SetContentPackageActivated(const FOGContentId&, bool, FString&) = 0;
    virtual bool IsContentPackageActivated(const FOGContentId&, bool&, bool&, FString&) const = 0;

    virtual bool AppendWorldEvent(const FOGWorldEvent&, FString&) = 0;
    virtual bool TryReadWorldEvent(
        const FOGEntityId&, bool&, FOGWorldEvent&, FString&) const = 0;
    virtual bool ListWorldEvents(
        const FOGEntityId&, FName, bool, int32,
        TArray<FOGWorldEvent>&, FString&) const = 0;

    virtual bool BackupTo(const FString&, FString&) = 0;
    virtual bool RestoreFrom(const FString&, FString&) = 0;
    virtual bool RunIntegrityCheck(FString&, FString&) const = 0;
    virtual bool Checkpoint(FString&) = 0;

    virtual const FString& GetDatabasePath() const = 0;
};
