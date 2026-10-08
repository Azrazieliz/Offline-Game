#pragma once

#include "CoreMinimal.h"

/**
 * Android Storage Access Framework bridge for uninstall-safe world backup
 * export/import. The platform picker owns the external URI; the canonical
 * world remains internal and is copied only after explicit user selection.
 */
class OFFLINEGAME_API FOGAndroidBackupDocumentBridge
{
public:
    static bool LaunchExport(
        const FString& SourceBackupPath,
        const FString& SuggestedFileName,
        FString& OutError);

    static bool LaunchImport(
        const FString& InternalDestinationPath,
        FString& OutError);

    /**
     * Returns idle / pending_export / pending_import / exported / imported /
     * cancelled / error. OutDetail contains the selected URI or error detail.
     */
    static FName GetTransferState(
        FString& OutDetail);
};
