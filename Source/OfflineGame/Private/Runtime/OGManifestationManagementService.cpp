#include "Runtime/OGManifestationManagementService.h"

bool FOGManifestationManagementService::SetFlags(
    const FOGEntityId& ManifestationId,
    bool bFavorite,
    bool bProtected,
    bool bLocked,
    int64 WorldTick,
    FString& OutError)
{
    FOGManifestationManagementMetadataRecord Metadata;
    Metadata.ManifestationId =
        ManifestationId;
    Metadata.bFavorite =
        bFavorite;
    Metadata.bProtected =
        bProtected;
    Metadata.bLocked =
        bLocked;
    Metadata.UpdatedWorldTick =
        WorldTick;
    return Store.UpsertManifestationManagementMetadata(
        Metadata,
        OutError);
}

bool FOGManifestationManagementService::SetBuildLabel(
    const FOGEntityId& ManifestationId,
    const FString& BuildLabel,
    int64 WorldTick,
    FString& OutError)
{
    bool bFound = false;
    FOGCharacterManifestationRecord Manifestation;
    if (!Store.TryReadCharacterManifestation(
            ManifestationId,
            bFound,
            Manifestation,
            OutError))
    {
        return false;
    }

    if (!bFound)
    {
        OutError =
            TEXT("Cannot label an unknown Manifestation.");
        return false;
    }

    Manifestation.BuildLabel =
        BuildLabel;
    return Store.UpsertCharacterManifestation(
        Manifestation,
        WorldTick,
        OutError);
}

bool FOGManifestationManagementService::SetContextSelection(
    const FOGManifestationContextSelectionRecord& Selection,
    FString& OutError)
{
    bool bFound = false;
    FOGCharacterManifestationRecord Manifestation;
    if (!Store.TryReadCharacterManifestation(
            Selection.ManifestationId,
            bFound,
            Manifestation,
            OutError))
    {
        return false;
    }

    if (!bFound ||
        Manifestation.OwningRulerId !=
            Selection.OwnerEntityId)
    {
        OutError =
            TEXT("Manifestation context selection must reference a Manifestation owned by the selected owner.");
        return false;
    }

    return Store.UpsertManifestationContextSelection(
        Selection,
        OutError);
}

bool FOGManifestationManagementService::CanDestructivelyModify(
    const FOGEntityId& ManifestationId,
    bool bExplicitConfirmation,
    FString& OutError) const
{
    OutError.Reset();

    bool bFound = false;
    FOGManifestationManagementMetadataRecord Metadata;
    if (!Store.TryReadManifestationManagementMetadata(
            ManifestationId,
            bFound,
            Metadata,
            OutError))
    {
        return false;
    }

    if (!bFound)
    {
        return true;
    }

    if ((Metadata.bProtected ||
         Metadata.bLocked) &&
        !bExplicitConfirmation)
    {
        OutError =
            TEXT("Manifestation is Protected/Locked; destructive action requires explicit confirmation.");
        return false;
    }

    return true;
}
