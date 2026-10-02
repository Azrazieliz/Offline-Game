#pragma once

#include "CoreMinimal.h"
#include "Persistence/OGWorldStore.h"

/**
 * Small rotating SQLite snapshot policy for long-lived offline saves.
 *
 * Snapshots are full SQLite backups, not SaveGame blobs. The newest KeepCount
 * snapshots are retained; older snapshots are deleted after a successful backup.
 */
class OFFLINEGAME_API FOGSnapshotService
{
public:
    static bool CreateRotatingSnapshot(
        IOGWorldStore& Store,
        const FString& SnapshotDirectory,
        int32 KeepCount,
        FString& OutSnapshotPath,
        FString& OutError);
};
