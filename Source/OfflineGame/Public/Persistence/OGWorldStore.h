#pragma once

#include "CoreMinimal.h"
#include "Events/OGWorldEvent.h"

/**
 * Persistence boundary for authoritative mutable world state.
 *
 * This interface deliberately hides SQLite from gameplay code. Gameplay systems
 * ask for transactions/state operations; the adapter owns SQL, migrations,
 * recovery, WAL/checkpoint policy, and schema details.
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
    virtual bool AppendWorldEvent(const FOGWorldEvent& Event, FString& OutError) = 0;
};
