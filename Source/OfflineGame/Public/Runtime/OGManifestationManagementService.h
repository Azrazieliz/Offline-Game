#pragma once

#include "CoreMinimal.h"
#include "Persistence/OGWorldStore.h"
#include "Runtime/OGPackageReportManagementRecords.h"

class OFFLINEGAME_API FOGManifestationManagementService
{
public:
    explicit FOGManifestationManagementService(IOGWorldStore& InStore)
        : Store(InStore)
    {
    }

    bool SetFlags(
        const FOGEntityId& ManifestationId,
        bool bFavorite,
        bool bProtected,
        bool bLocked,
        int64 WorldTick,
        FString& OutError);

    bool SetBuildLabel(
        const FOGEntityId& ManifestationId,
        const FString& BuildLabel,
        int64 WorldTick,
        FString& OutError);

    bool SetContextSelection(
        const FOGManifestationContextSelectionRecord& Selection,
        FString& OutError);

    bool CanDestructivelyModify(
        const FOGEntityId& ManifestationId,
        bool bExplicitConfirmation,
        FString& OutError) const;

private:
    IOGWorldStore& Store;
};
