#pragma once

#include "CoreMinimal.h"
#include "Core/OGEntityId.h"
#include "Events/OGWorldEvent.h"

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

    virtual bool AppendWorldEvent(const FOGWorldEvent& Event, FString& OutError) = 0;

    virtual bool BackupTo(const FString& AbsoluteBackupPath, FString& OutError) = 0;
    virtual bool RestoreFrom(const FString& AbsoluteBackupPath, FString& OutError) = 0;
    virtual bool RunIntegrityCheck(FString& OutReport, FString& OutError) const = 0;
    virtual bool Checkpoint(FString& OutError) = 0;

    virtual const FString& GetDatabasePath() const = 0;
};
