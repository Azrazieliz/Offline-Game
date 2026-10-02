#pragma once

#include "CoreMinimal.h"
#include "Characters/OGCharacterDefinitions.h"
#include "Content/OGContentId.h"
#include "Core/OGEntityId.h"
#include "Events/OGWorldEvent.h"
#include "World/OGWorldStateRecords.h"

/**
 * Persistence boundary for authoritative mutable world state.
 *
 * Gameplay systems must not issue SQL directly. The adapter owns SQL,
 * migrations, recovery, WAL/checkpoint policy, and schema details.
 */
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

    virtual bool UpsertEntity(
        const FOGEntityId& EntityId,
        FName Kind,
        int64 CreatedWorldTick,
        const FString& StateJson,
        FString& OutError) = 0;

    virtual bool TryReadEntity(
        const FOGEntityId& EntityId,
        bool& bOutFound,
        FName& OutKind,
        FString& OutStateJson,
        int64& OutRevision,
        FString& OutError) const = 0;

    virtual bool UpsertCharacterManifestation(
        const FOGCharacterManifestationRecord& Manifestation,
        int64 CreatedWorldTick,
        FString& OutError) = 0;

    virtual bool TryReadCharacterManifestation(
        const FOGEntityId& ManifestationId,
        bool& bOutFound,
        FOGCharacterManifestationRecord& OutManifestation,
        FString& OutError) const = 0;

    virtual bool UpsertLocation(
        const FOGLocationRecord& Location,
        int64 CreatedWorldTick,
        FString& OutError) = 0;

    virtual bool TryReadLocation(
        const FOGEntityId& LocationId,
        bool& bOutFound,
        FOGLocationRecord& OutLocation,
        FString& OutError) const = 0;

    virtual bool UpsertWorldPresence(
        const FOGWorldPresenceRecord& Presence,
        FString& OutError) = 0;

    virtual bool TryReadWorldPresence(
        const FOGEntityId& EntityId,
        bool& bOutFound,
        FOGWorldPresenceRecord& OutPresence,
        FString& OutError) const = 0;

    virtual bool UpsertKnowledgeFact(
        const FOGKnowledgeFactRecord& Fact,
        FString& OutError) = 0;

    virtual bool TryReadKnowledgeFact(
        const FOGEntityId& OwnerEntityId,
        FName FactKey,
        const FOGEntityId& SubjectEntityId,
        bool& bOutFound,
        FOGKnowledgeFactRecord& OutFact,
        FString& OutError) const = 0;

    virtual bool UpsertContentPackage(
        const FOGContentId& PackageId,
        int32 Version,
        const FString& ContentHash,
        bool bInstalled,
        bool bValidated,
        const FString& ManifestJson,
        FString& OutError) = 0;

    virtual bool SetContentPackageActivated(
        const FOGContentId& PackageId,
        bool bActivated,
        FString& OutError) = 0;

    virtual bool IsContentPackageActivated(
        const FOGContentId& PackageId,
        bool& bOutKnown,
        bool& bOutActivated,
        FString& OutError) const = 0;

    virtual bool AppendWorldEvent(const FOGWorldEvent& Event, FString& OutError) = 0;

    virtual bool BackupTo(const FString& AbsoluteBackupPath, FString& OutError) = 0;
    virtual bool RestoreFrom(const FString& AbsoluteBackupPath, FString& OutError) = 0;
    virtual bool RunIntegrityCheck(FString& OutReport, FString& OutError) const = 0;
    virtual bool Checkpoint(FString& OutError) = 0;

    virtual const FString& GetDatabasePath() const = 0;
};
