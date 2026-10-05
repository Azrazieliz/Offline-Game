#pragma once

#include "CoreMinimal.h"
#include "Persistence/OGWorldStore.h"

struct sqlite3;

class FOGSQLiteWorldStore final : public IOGWorldStore
{
public:
    FOGSQLiteWorldStore() = default;
    virtual ~FOGSQLiteWorldStore() override;

    virtual bool Open(const FString&, FString&) override;
    virtual void Close() override;
    virtual bool IsOpen() const override { return Database != nullptr; }

    virtual bool BeginTransaction(FString&) override;
    virtual bool CommitTransaction(FString&) override;
    virtual bool RollbackTransaction(FString&) override;
    virtual int32 GetSchemaVersion(FString&) const override;

    static int32 LatestSchemaVersion();
    bool RunApplicationValidation(FString& OutReport, FString& OutError) const;

    // Persistence-internal migration helpers. This concrete header lives under
    // Private; gameplay/UI code must continue to use IOGWorldStore instead.
    bool ExecuteSql(const FString&, FString&) const;

    virtual bool UpsertEntity(const FOGEntityId&, FName, int64, const FString&, FString&) override;
    virtual bool TryReadEntity(const FOGEntityId&, bool&, FName&, FString&, int64&, FString&) const override;

    virtual bool UpsertCharacterManifestation(const FOGCharacterManifestationRecord&, int64, FString&) override;
    virtual bool TryReadCharacterManifestation(const FOGEntityId&, bool&, FOGCharacterManifestationRecord&, FString&) const override;
    virtual bool ListCharacterManifestationsByOwnerAndIdentity(
        const FOGEntityId&, const FOGContentId&, TArray<FOGCharacterManifestationRecord>&, FString&) const override;
    virtual bool ListCharacterManifestationsByOwner(
        const FOGEntityId&, TArray<FOGCharacterManifestationRecord>&, FString&) const override;
    virtual bool SetManifestationAnchor(
        const FOGEntityId&, const FOGEntityId&, int64, FString&) override;
    virtual bool SetManifestationLifecycle(
        const FOGEntityId&, FName, FString&) override;

    bool MigrateLegacyDuplicateManifestations0007(FString& OutError);
    bool ValidateManifestationMigration0007(FString& OutError) const;

    virtual bool UpsertGachaState(const FOGGachaStateRecord&, FString&) override;
    virtual bool TryReadGachaState(const FOGEntityId&, FName, bool&, FOGGachaStateRecord&, FString&) const override;

    virtual bool UpsertLocation(const FOGLocationRecord&, int64, FString&) override;
    virtual bool TryReadLocation(const FOGEntityId&, bool&, FOGLocationRecord&, FString&) const override;
    virtual bool UpsertWorldPresence(const FOGWorldPresenceRecord&, FString&) override;
    virtual bool TryReadWorldPresence(const FOGEntityId&, bool&, FOGWorldPresenceRecord&, FString&) const override;
    virtual bool UpsertKnowledgeFact(const FOGKnowledgeFactRecord&, FString&) override;
    virtual bool TryReadKnowledgeFact(const FOGEntityId&, FName, const FOGEntityId&, bool&, FOGKnowledgeFactRecord&, FString&) const override;

    virtual bool UpsertTerritory(const FOGTerritoryRecord&, int64, FString&) override;
    virtual bool TryReadTerritory(const FOGEntityId&, bool&, FOGTerritoryRecord&, FString&) const override;
    virtual bool UpsertLocationTerritory(const FOGLocationTerritoryRecord&, FString&) override;
    virtual bool UpsertTerritoryClaim(const FOGTerritoryClaimRecord&, int64, FString&) override;
    virtual bool TryReadTerritoryClaim(const FOGEntityId&, bool&, FOGTerritoryClaimRecord&, FString&) const override;
    virtual bool ListTerritoryClaimsByTerritory(
        const FOGEntityId&, TArray<FOGTerritoryClaimRecord>&, FString&) const override;
    virtual bool ListTerritoryClaimsByRuler(
        const FOGEntityId&, TArray<FOGTerritoryClaimRecord>&, FString&) const override;
    virtual bool ListActiveClaimsForLocation(
        const FOGEntityId&, TArray<FOGTerritoryClaimRecord>&, FString&) const override;
    virtual bool UpsertRulerSovereigntyState(const FOGRulerSovereigntyStateRecord&, FString&) override;
    virtual bool TryReadRulerSovereigntyState(
        const FOGEntityId&, bool&, FOGRulerSovereigntyStateRecord&, FString&) const override;
    virtual bool UpsertRulerGachaAccess(const FOGRulerGachaAccessRecord&, FString&) override;
    virtual bool TryReadRulerGachaAccess(
        const FOGEntityId&, bool&, FOGRulerGachaAccessRecord&, FString&) const override;

    bool MigrateTerritorySovereignty0008(FString& OutError);
    bool ValidateTerritorySovereigntyMigration0008(FString& OutError) const;

    virtual bool UpsertDomainCore(const FOGDomainCoreRecord&, int64, FString&) override;
    virtual bool TryReadDomainCore(const FOGEntityId&, bool&, FOGDomainCoreRecord&, FString&) const override;
    virtual bool UpsertTerritoryDomainState(
        const FOGTerritoryDomainStateRecord&, FString&) override;
    virtual bool TryReadTerritoryDomainState(
        const FOGEntityId&, bool&, FOGTerritoryDomainStateRecord&, FString&) const override;
    virtual bool UpsertDomainCoreConcept(
        const FOGDomainCoreConceptRecord&, FString&) override;
    virtual bool ListDomainCoreConcepts(
        const FOGEntityId&, TArray<FOGDomainCoreConceptRecord>&, FString&) const override;
    virtual bool UpsertDomainCoreFusion(
        const FOGDomainCoreFusionRecord&, int64, FString&) override;
    virtual bool ListDomainCoreFusionsForResult(
        const FOGEntityId&, TArray<FOGDomainCoreFusionRecord>&, FString&) const override;
    virtual bool UpsertDomainCoreLineage(
        const FOGDomainCoreLineageRecord&, FString&) override;
    virtual bool ListDomainCoreLineage(
        const FOGEntityId&, TArray<FOGDomainCoreLineageRecord>&, FString&) const override;

    bool MigrateDomainHeartAndConcepts0009(FString& OutError);
    bool ValidateDomainHeartMigration0009(FString& OutError) const;

    virtual bool SetResourceBalance(const FOGEntityId&, const FOGContentId&, int64, FString&) override;
    virtual bool TryReadResourceBalance(const FOGEntityId&, const FOGContentId&, bool&, int64&, FString&) const override;
    virtual bool UpsertProject(const FOGProjectRecord&, int64, FString&) override;
    virtual bool TryReadProject(const FOGEntityId&, bool&, FOGProjectRecord&, FString&) const override;

    virtual bool UpsertEntityRankState(const FOGEntityRankStateRecord&, FString&) override;
    virtual bool TryReadEntityRankState(
        const FOGEntityId&, bool&, FOGEntityRankStateRecord&, FString&) const override;

    virtual bool UpsertFactorInstance(const FOGFactorInstanceRecord&, int64, FString&) override;
    virtual bool TryReadFactorInstance(
        const FOGEntityId&, bool&, FOGFactorInstanceRecord&, FString&) const override;
    virtual bool ListFactorInstancesByOwner(
        const FOGEntityId&, TArray<FOGFactorInstanceRecord>&, FString&) const override;
    virtual bool UpsertFactorLineage(const FOGFactorLineageRecord&, FString&) override;
    virtual bool ListFactorLineageForChild(
        const FOGEntityId&, TArray<FOGFactorLineageRecord>&, FString&) const override;

    virtual bool UpsertEntityClass(const FOGEntityClassRecord&, FString&) override;
    virtual bool TryReadEntityClass(
        const FOGEntityId&, const FOGContentId&, bool&, FOGEntityClassRecord&, FString&) const override;
    virtual bool ListEntityClasses(
        const FOGEntityId&, TArray<FOGEntityClassRecord>&, FString&) const override;
    virtual bool UpsertGrandClassSeat(const FOGGrandClassSeatRecord&, FString&) override;
    virtual bool TryReadGrandClassSeat(
        const FOGContentId&, bool&, FOGGrandClassSeatRecord&, FString&) const override;
    virtual bool DeleteGrandClassSeat(const FOGContentId&, FString&) override;

    virtual bool UpsertEntitySkill(const FOGEntitySkillRecord&, FString&) override;
    virtual bool ListEntitySkills(
        const FOGEntityId&, TArray<FOGEntitySkillRecord>&, FString&) const override;
    virtual bool UpsertSkillProvenance(const FOGSkillProvenanceRecord&, FString&) override;
    virtual bool ListSkillProvenance(
        const FOGEntityId&, const FOGContentId&, TArray<FOGSkillProvenanceRecord>&, FString&) const override;

    virtual bool UpsertManifestationRouteNode(
        const FOGManifestationRouteNodeRecord&, FString&) override;
    virtual bool ListManifestationRouteNodes(
        const FOGEntityId&, TArray<FOGManifestationRouteNodeRecord>&, FString&) const override;
    virtual bool UpsertManifestationForm(
        const FOGManifestationFormRecord&, FString&) override;
    virtual bool ListManifestationForms(
        const FOGEntityId&, TArray<FOGManifestationFormRecord>&, FString&) const override;
    virtual bool UpsertManifestationReinforcement(
        const FOGManifestationReinforcementRecord&, FString&) override;
    virtual bool TryReadManifestationReinforcement(
        const FOGEntityId&, bool&, FOGManifestationReinforcementRecord&, FString&) const override;

    virtual bool UpsertEntityTranscendenceState(
        const FOGEntityTranscendenceStateRecord&, FString&) override;
    virtual bool TryReadEntityTranscendenceState(
        const FOGEntityId&, bool&, FOGEntityTranscendenceStateRecord&, FString&) const override;
    virtual bool UpsertManifestationWorldFantasmState(
        const FOGManifestationWorldFantasmStateRecord&, FString&) override;
    virtual bool TryReadManifestationWorldFantasmState(
        const FOGEntityId&, bool&, FOGManifestationWorldFantasmStateRecord&, FString&) const override;
    virtual bool UpsertProtagonistWorldManifestationState(
        const FOGProtagonistWorldManifestationStateRecord&, FString&) override;
    virtual bool TryReadProtagonistWorldManifestationState(
        const FOGEntityId&, bool&, FOGProtagonistWorldManifestationStateRecord&, FString&) const override;

    virtual bool UpsertCharacterConvergence(
        const FOGCharacterConvergenceRecord&, int64, FString&) override;
    virtual bool TryReadCharacterConvergence(
        const FOGEntityId&, bool&, FOGCharacterConvergenceRecord&, FString&) const override;
    virtual bool ListCharacterConvergencesByIdentity(
        const FOGContentId&, TArray<FOGCharacterConvergenceRecord>&, FString&) const override;
    virtual bool UpsertCharacterConvergenceSource(
        const FOGCharacterConvergenceSourceRecord&, FString&) override;
    virtual bool ListCharacterConvergenceSources(
        const FOGEntityId&, TArray<FOGCharacterConvergenceSourceRecord>&, FString&) const override;

    bool MigrateProgression0010(FString& OutError);
    bool ValidateProgressionMigration0010(FString& OutError) const;

    virtual bool UpsertTimeDomain(const FOGTimeDomainRecord&, int64, FString&) override;
    virtual bool TryReadTimeDomain(
        const FOGEntityId&, bool&, FOGTimeDomainRecord&, FString&) const override;
    virtual bool UpsertRealityNode(const FOGRealityNodeRecord&, int64, FString&) override;
    virtual bool TryReadRealityNode(
        const FOGEntityId&, bool&, FOGRealityNodeRecord&, FString&) const override;
    virtual bool ListChildRealityNodes(
        const FOGEntityId&, TArray<FOGRealityNodeRecord>&, FString&) const override;
    virtual bool UpsertJunction(const FOGJunctionRecord&, int64, FString&) override;
    virtual bool TryReadJunction(
        const FOGEntityId&, bool&, FOGJunctionRecord&, FString&) const override;
    virtual bool ListJunctionsFromReality(
        const FOGEntityId&, TArray<FOGJunctionRecord>&, FString&) const override;
    virtual bool UpsertWorldDirectorSchedule(
        const FOGWorldDirectorScheduleRecord&, int64, FString&) override;
    virtual bool TryReadWorldDirectorSchedule(
        const FOGEntityId&, bool&, FOGWorldDirectorScheduleRecord&, FString&) const override;
    virtual bool ListWorldDirectorSchedulesByContent(
        const FOGContentId&, TArray<FOGWorldDirectorScheduleRecord>&, FString&) const override;
    virtual bool UpsertContentUnlockState(
        const FOGContentUnlockStateRecord&, FString&) override;
    virtual bool TryReadContentUnlockState(
        const FOGContentId&, bool&, FOGContentUnlockStateRecord&, FString&) const override;
    virtual bool UpsertOfflineSimulationState(
        const FOGOfflineSimulationStateRecord&, FString&) override;
    virtual bool TryReadOfflineSimulationState(
        const FOGEntityId&, bool&, FOGOfflineSimulationStateRecord&, FString&) const override;

    bool ValidateRealityTimeDirectorMigration0011(FString& OutError) const;

    virtual bool UpsertDispatch(const FOGDispatchRecord&, int64, FString&) override;
    virtual bool TryReadDispatch(const FOGEntityId&, bool&, FOGDispatchRecord&, FString&) const override;

    virtual bool UpsertDispatchObjective(
        const FOGDispatchObjectiveRecord&, FString&) override;
    virtual bool ListDispatchObjectives(
        const FOGEntityId&, TArray<FOGDispatchObjectiveRecord>&, FString&) const override;
    virtual bool UpsertDispatchConstraint(
        const FOGDispatchConstraintRecord&, FString&) override;
    virtual bool ListDispatchConstraints(
        const FOGEntityId&, TArray<FOGDispatchConstraintRecord>&, FString&) const override;

    virtual bool UpsertFaction(const FOGFactionRecord&, int64, FString&) override;
    virtual bool TryReadFaction(const FOGEntityId&, bool&, FOGFactionRecord&, FString&) const override;
    virtual bool UpsertFactionLink(const FOGFactionLinkRecord&, FString&) override;
    virtual bool TryReadFactionLink(const FOGEntityId&, const FOGEntityId&, EOGFactionLinkType, bool&, FOGFactionLinkRecord&, FString&) const override;

    virtual bool UpsertArmy(const FOGArmyRecord&, int64, FString&) override;
    virtual bool TryReadArmy(const FOGEntityId&, bool&, FOGArmyRecord&, FString&) const override;

    virtual bool UpsertArmyCapability(
        const FOGArmyCapabilityRecord&, FString&) override;
    virtual bool ListArmyCapabilities(
        const FOGEntityId&, TArray<FOGArmyCapabilityRecord>&, FString&) const override;

    virtual bool UpsertWar(const FOGWarRecord&, int64, FString&) override;
    virtual bool TryReadWar(const FOGEntityId&, bool&, FOGWarRecord&, FString&) const override;
    virtual bool UpsertWarFront(const FOGWarFrontRecord&, int64, FString&) override;
    virtual bool TryReadWarFront(
        const FOGEntityId&, bool&, FOGWarFrontRecord&, FString&) const override;
    virtual bool ListWarFronts(
        const FOGEntityId&, TArray<FOGWarFrontRecord>&, FString&) const override;
    virtual bool UpsertWarObjective(
        const FOGWarObjectiveRecord&, FString&) override;
    virtual bool ListWarObjectives(
        const FOGEntityId&, TArray<FOGWarObjectiveRecord>&, FString&) const override;
    virtual bool UpsertWarOrder(const FOGWarOrderRecord&, int64, FString&) override;
    virtual bool TryReadWarOrder(
        const FOGEntityId&, bool&, FOGWarOrderRecord&, FString&) const override;
    virtual bool ListWarOrders(
        const FOGEntityId&, TArray<FOGWarOrderRecord>&, FString&) const override;
    virtual bool UpsertWarParticipantHistory(
        const FOGWarParticipantHistoryRecord&, FString&) override;
    virtual bool ListWarParticipantHistory(
        const FOGEntityId&, TArray<FOGWarParticipantHistoryRecord>&, FString&) const override;

    virtual bool UpsertProjectPhase(
        const FOGProjectPhaseRecord&, FString&) override;
    virtual bool ListProjectPhases(
        const FOGEntityId&, TArray<FOGProjectPhaseRecord>&, FString&) const override;
    virtual bool UpsertProjectAssignment(
        const FOGProjectAssignmentRecord&, FString&) override;
    virtual bool ListProjectAssignments(
        const FOGEntityId&, TArray<FOGProjectAssignmentRecord>&, FString&) const override;

    virtual bool UpsertCivilizationState(
        const FOGCivilizationStateRecord&, FString&) override;
    virtual bool TryReadCivilizationState(
        const FOGEntityId&, bool&, FOGCivilizationStateRecord&, FString&) const override;
    virtual bool UpsertCivilizationDimension(
        const FOGCivilizationDimensionRecord&, FString&) override;
    virtual bool ListCivilizationDimensions(
        const FOGEntityId&, TArray<FOGCivilizationDimensionRecord>&, FString&) const override;

    virtual bool UpsertLogisticsRoute(
        const FOGLogisticsRouteRecord&, int64, FString&) override;
    virtual bool TryReadLogisticsRoute(
        const FOGEntityId&, bool&, FOGLogisticsRouteRecord&, FString&) const override;
    virtual bool ListLogisticsRoutesByOwner(
        const FOGEntityId&, TArray<FOGLogisticsRouteRecord>&, FString&) const override;

    bool MigrateStrategy0012(FString& OutError);
    bool ValidateStrategyMigration0012(FString& OutError) const;

    virtual bool UpsertItemInstance(const FOGItemInstanceRecord&, int64, FString&) override;
    virtual bool TryReadItemInstance(
        const FOGEntityId&, bool&, FOGItemInstanceRecord&, FString&) const override;
    virtual bool ListItemInstancesByOwner(
        const FOGEntityId&, TArray<FOGItemInstanceRecord>&, FString&) const override;
    virtual bool UpsertItemModifier(const FOGItemModifierRecord&, FString&) override;
    virtual bool ListItemModifiers(
        const FOGEntityId&, TArray<FOGItemModifierRecord>&, FString&) const override;
    virtual bool UpsertEquipmentBinding(const FOGEquipmentBindingRecord&, FString&) override;
    virtual bool TryReadEquipmentBinding(
        const FOGEntityId&, const FOGContentId&, bool&, FOGEquipmentBindingRecord&, FString&) const override;
    virtual bool ListEquipmentBindings(
        const FOGEntityId&, TArray<FOGEquipmentBindingRecord>&, FString&) const override;
    virtual bool DeleteEquipmentBindingsForItem(
        const FOGEntityId&, FString&) override;
    virtual bool UpsertInventoryContainer(
        const FOGInventoryContainerRecord&, int64, FString&) override;
    virtual bool TryReadInventoryContainer(
        const FOGEntityId&, bool&, FOGInventoryContainerRecord&, FString&) const override;
    virtual bool ListInventoryContainersByOwner(
        const FOGEntityId&, TArray<FOGInventoryContainerRecord>&, FString&) const override;
    virtual bool UpsertContainerContent(const FOGContainerContentRecord&, FString&) override;
    virtual bool ListContainerContents(
        const FOGEntityId&, TArray<FOGContainerContentRecord>&, FString&) const override;
    virtual bool DeleteContainerContentsForItem(
        const FOGEntityId&, FString&) override;
    virtual bool UpsertItemOwnerAffinity(
        const FOGItemOwnerAffinityRecord&, FString&) override;
    virtual bool TryReadItemOwnerAffinity(
        const FOGEntityId&, const FOGEntityId&, bool&, FOGItemOwnerAffinityRecord&, FString&) const override;
    virtual bool UpsertEquipmentProficiency(
        const FOGEquipmentProficiencyRecord&, FString&) override;
    virtual bool TryReadEquipmentProficiency(
        const FOGEntityId&, const FOGContentId&, bool&, FOGEquipmentProficiencyRecord&, FString&) const override;
    virtual bool UpsertManifestationPresentationState(
        const FOGManifestationPresentationStateRecord&, FString&) override;
    virtual bool TryReadManifestationPresentationState(
        const FOGEntityId&, bool&, FOGManifestationPresentationStateRecord&, FString&) const override;
    virtual bool UpsertOwnedPresentationUnlock(
        const FOGOwnedPresentationUnlockRecord&, FString&) override;
    virtual bool ListOwnedPresentationUnlocks(
        const FOGEntityId&, TArray<FOGOwnedPresentationUnlockRecord>&, FString&) const override;
    virtual bool UpsertEntityLanguage(const FOGEntityLanguageRecord&, FString&) override;
    virtual bool ListEntityLanguages(
        const FOGEntityId&, TArray<FOGEntityLanguageRecord>&, FString&) const override;
    virtual bool UpsertSemanticMemory(
        const FOGSemanticMemoryRecord&, int64, FString&) override;
    virtual bool TryReadSemanticMemory(
        const FOGEntityId&, bool&, FOGSemanticMemoryRecord&, FString&) const override;
    virtual bool ListSemanticMemoriesByOwner(
        const FOGEntityId&, TArray<FOGSemanticMemoryRecord>&, FString&) const override;
    virtual bool UpsertNpcPromotionState(
        const FOGNpcPromotionStateRecord&, FString&) override;
    virtual bool TryReadNpcPromotionState(
        const FOGEntityId&, bool&, FOGNpcPromotionStateRecord&, FString&) const override;
    virtual bool UpsertCharacterAdultRuntimeState(
        const FOGCharacterAdultRuntimeStateRecord&, FString&) override;
    virtual bool TryReadCharacterAdultRuntimeState(
        const FOGEntityId&, bool&, FOGCharacterAdultRuntimeStateRecord&, FString&) const override;
    virtual bool UpsertHeroicRecord(
        const FOGHeroicRecord&, int64, FString&) override;
    virtual bool TryReadHeroicRecord(
        const FOGEntityId&, bool&, FOGHeroicRecord&, FString&) const override;
    virtual bool ListHeroicRecordsByIdentity(
        const FOGContentId&, TArray<FOGHeroicRecord>&, FString&) const override;

    bool MigrateItemsKnowledgeCharacters0013(FString& OutError);
    bool ValidateItemsKnowledgeCharactersMigration0013(FString& OutError) const;

    virtual bool UpsertContentPackageRecord(
        const FOGContentPackageRecord&, FString&) override;
    virtual bool TryReadContentPackageRecord(
        const FOGContentId&, bool&, FOGContentPackageRecord&, FString&) const override;
    virtual bool ListContentPackageRecords(
        TArray<FOGContentPackageRecord>&, FString&) const override;
    virtual bool UpsertPackageDependency(
        const FOGPackageDependencyRecord&, FString&) override;
    virtual bool ListPackageDependencies(
        const FOGContentId&, TArray<FOGPackageDependencyRecord>&, FString&) const override;

    virtual bool UpsertReport(
        const FOGReportRecord&, int64, FString&) override;
    virtual bool TryReadReport(
        const FOGEntityId&, bool&, FOGReportRecord&, FString&) const override;
    virtual bool ListReportsByOwner(
        const FOGEntityId&, TArray<FOGReportRecord>&, FString&) const override;
    virtual bool UpsertReportDelivery(
        const FOGReportDeliveryRecord&, FString&) override;
    virtual bool TryReadReportDelivery(
        const FOGEntityId&, FName, bool&, FOGReportDeliveryRecord&, FString&) const override;
    virtual bool ListReportDeliveries(
        const FOGEntityId&, TArray<FOGReportDeliveryRecord>&, FString&) const override;

    virtual bool UpsertManifestationManagementMetadata(
        const FOGManifestationManagementMetadataRecord&, FString&) override;
    virtual bool TryReadManifestationManagementMetadata(
        const FOGEntityId&, bool&, FOGManifestationManagementMetadataRecord&, FString&) const override;
    virtual bool UpsertManifestationContextSelection(
        const FOGManifestationContextSelectionRecord&, FString&) override;
    virtual bool TryReadManifestationContextSelection(
        const FOGEntityId&, const FOGContentId&, bool&, FOGManifestationContextSelectionRecord&, FString&) const override;

    bool MigratePackagesReportsManagement0014(FString& OutError);
    bool ValidatePackagesReportsManagementMigration0014(FString& OutError) const;

    virtual bool UpsertContentPackage(const FOGContentId&, int32, const FString&, bool, bool, const FString&, FString&) override;
    virtual bool SetContentPackageActivated(const FOGContentId&, bool, FString&) override;
    virtual bool IsContentPackageActivated(const FOGContentId&, bool&, bool&, FString&) const override;

    virtual bool AppendWorldEvent(const FOGWorldEvent&, FString&) override;
    virtual bool TryReadWorldEvent(
        const FOGEntityId&, bool&, FOGWorldEvent&, FString&) const override;
    virtual bool ListWorldEvents(
        const FOGEntityId&, FName, bool, int32,
        TArray<FOGWorldEvent>&, FString&) const override;

    virtual bool BackupTo(const FString&, FString&) override;
    virtual bool RestoreFrom(const FString&, FString&) override;
    virtual bool RunIntegrityCheck(FString&, FString&) const override;
    virtual bool Checkpoint(FString&) override;

    virtual const FString& GetDatabasePath() const override { return DatabasePath; }

private:
    bool EnsureMigrationTable(FString&);
    bool ApplyMigrations(FString&);
    bool RecordMigration(int32, const TCHAR*, FString&);
    FString LastError(const TCHAR*) const;

    sqlite3* Database = nullptr;
    FString DatabasePath;
    bool bTransactionActive = false;
};
