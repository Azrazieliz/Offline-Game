#pragma once

#include "CoreMinimal.h"
#include "Characters/OGCharacterDefinitions.h"
#include "Content/OGContentId.h"
#include "Core/OGEntityId.h"
#include "Events/OGWorldEvent.h"
#include "Gacha/OGGachaDefinitions.h"
#include "Progression/OGProgressionRecords.h"
#include "World/OGDispatchFactionWarRecords.h"
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

    virtual bool UpsertDispatch(const FOGDispatchRecord&, int64, FString&) = 0;
    virtual bool TryReadDispatch(const FOGEntityId&, bool&, FOGDispatchRecord&, FString&) const = 0;

    virtual bool UpsertFaction(const FOGFactionRecord&, int64, FString&) = 0;
    virtual bool TryReadFaction(const FOGEntityId&, bool&, FOGFactionRecord&, FString&) const = 0;
    virtual bool UpsertFactionLink(const FOGFactionLinkRecord&, FString&) = 0;
    virtual bool TryReadFactionLink(const FOGEntityId&, const FOGEntityId&, EOGFactionLinkType, bool&, FOGFactionLinkRecord&, FString&) const = 0;

    virtual bool UpsertArmy(const FOGArmyRecord&, int64, FString&) = 0;
    virtual bool TryReadArmy(const FOGEntityId&, bool&, FOGArmyRecord&, FString&) const = 0;

    virtual bool UpsertWar(const FOGWarRecord&, int64, FString&) = 0;
    virtual bool TryReadWar(const FOGEntityId&, bool&, FOGWarRecord&, FString&) const = 0;

    virtual bool UpsertContentPackage(const FOGContentId&, int32, const FString&, bool, bool, const FString&, FString&) = 0;
    virtual bool SetContentPackageActivated(const FOGContentId&, bool, FString&) = 0;
    virtual bool IsContentPackageActivated(const FOGContentId&, bool&, bool&, FString&) const = 0;

    virtual bool AppendWorldEvent(const FOGWorldEvent&, FString&) = 0;

    virtual bool BackupTo(const FString&, FString&) = 0;
    virtual bool RestoreFrom(const FString&, FString&) = 0;
    virtual bool RunIntegrityCheck(FString&, FString&) const = 0;
    virtual bool Checkpoint(FString&) = 0;

    virtual const FString& GetDatabasePath() const = 0;
};
