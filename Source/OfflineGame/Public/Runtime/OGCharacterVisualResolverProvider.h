#pragma once

#include "CoreMinimal.h"
#include "Runtime/OGCanonicalCharacterPresentation.h"

/** Read-only authored presentation metadata in the existing package ManifestJson.
 * Identity/Version/Manifestation and all mutable state come only from the projection.
 * No package activation, eligibility policy, or character persistence lives here. */
class OFFLINEGAME_API FOGCharacterVisualResolverProvider
{
public:
    /** Native authoring/diagnostic adapters; caller registers the resulting existing package record. */
    static bool MakeDiagnosticRow(const FOGFoundationCharacterProjection& Projection,
        const FString& AuthoredTemplate, FString& OutRow, FString& OutReason);
    static bool MergeAuthoredMetadata(FOGContentPackageRecord& Package,
        const FString& RowJson, FString& OutReason);
    static bool Resolve(const IOGWorldStore& Store,
        const FOGFoundationCharacterProjection& Projection, bool bPrivacy,
        FOGCharacterVisualBinding& OutBinding, FString& OutReason);
};
