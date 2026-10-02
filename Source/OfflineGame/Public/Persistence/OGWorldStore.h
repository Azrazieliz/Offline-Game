#pragma once

#include "CoreMinimal.h"
#include "Characters/OGCharacterDefinitions.h"
#include "Content/OGContentId.h"
#include "Core/OGEntityId.h"
#include "Events/OGWorldEvent.h"
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

    virtual bool UpsertLocation(const FOGLocationRecord&, int64, FString&) = 0;
    virtual bool TryReadLocation(const FOGEntityId&, bool&, FOGLocationRecord&, FString&) const = 0;
    virtual bool UpsertWorldPresence(const FOGWorldPresenceRecord&, FString&) = 0;
    virtual bool TryReadWorldPresence(const FOGEntityId&, bool&, FOGWorldPresenceRecord&, FString&) const = 0;
    virtual bool UpsertKnowledgeFact(const FOGKnowledgeFactRecord&, FString&) = 0;
    virtual bool TryReadKnowledgeFact(const FOGEntityId&, FName, const FOGEntityId&, bool&, FOGKnowledgeFactRecord&, FString&) const = 0;

    virtual bool UpsertTerritory(const FOGTerritoryRecord&, int64, FString&) = 0;
    virtual bool TryReadTerritory(const FOGEntityId&, bool&, FOGTerritoryRecord&, FString&) const = 0;
    virtual bool UpsertDomainCore(const FOGDomainCoreRecord&, int64, FString&) = 0;
    virtual bool TryReadDomainCore(const FOGEntityId&, bool&, FOGDomainCoreRecord&, FString&) const = 0;
    virtual bool SetResourceBalance(const FOGEntityId&, const FOGContentId&, int64, FString&) = 0;
    virtual bool TryReadResourceBalance(const FOGEntityId&, const FOGContentId&, bool&, int64&, FString&) const = 0;
    virtual bool UpsertProject(const FOGProjectRecord&, int64, FString&) = 0;
    virtual bool TryReadProject(const FOGEntityId&, bool&, FOGProjectRecord&, FString&) const = 0;

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
